#include "MatchProcessor.h"

namespace MatchEq
{

void Processor::prepare (double newSampleRate, int samplesPerBlock) noexcept
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    blockSize = juce::jmax (1, samplesPerBlock);

    juce::dsp::ProcessSpec monoSpec;
    monoSpec.sampleRate = sampleRate;
    monoSpec.maximumBlockSize = (juce::uint32) blockSize;
    monoSpec.numChannels = 1;

    for (auto& s : slices)
    {
        s.detect.prepare (monoSpec);
        s.applyL.prepare (monoSpec);
        s.applyR.prepare (monoSpec);
        s.detect.reset();
        s.applyL.reset();
        s.applyR.reset();
        s.envDb = kSilenceFloorDb;
        s.grLin = 1.0f;
        s.grTarget = 1.0f;
        s.sumSq = 0.0;
    }

    rebuildLattice();
    fillFactoryTarget (pink);
    clearPublished();
    wasEnabled = false;
    settling = false;
    prepared = true;
}

void Processor::reset() noexcept
{
    for (int i = 0; i < activeSlices; ++i)
    {
        auto& s = slices[(size_t) i];
        s.detect.reset();
        s.applyL.reset();
        s.applyR.reset();
        s.envDb = kSilenceFloorDb;
        s.grLin = 1.0f;
        s.grTarget = 1.0f;
        s.sumSq = 0.0;
    }
    clearPublished();
    wasEnabled = false;
    settling = false;
}

void Processor::rebuildLattice() noexcept
{
    const float maxHz = juce::jmin (20000.0f, (float) sampleRate * 0.45f);
    const float minHz = 20.0f;
    activeSlices = 0;

    if (! (maxHz > minHz * 1.01f) || sampleRate <= 0.0)
        return;

    const double logMin = std::log ((double) minHz);
    const double logMax = std::log ((double) maxHz);

    for (int i = 0; i < kNumSlices; ++i)
    {
        const double t = (kNumSlices == 1) ? 0.5
            : (double) i / (double) (kNumSlices - 1);
        const float f = (float) std::exp (logMin + t * (logMax - logMin));

        // Constant-Q packing so neighbours form a soft partition.
        // Smooth widens Q (lower Q) so bands overlap more and steps soften.
        float q = 1.0f;
        if (kNumSlices >= 2)
        {
            const double t0 = (double) juce::jmax (0, i - 1) / (double) (kNumSlices - 1);
            const double t1 = (double) juce::jmin (kNumSlices - 1, i + 1) / (double) (kNumSlices - 1);
            const float f0 = (float) std::exp (logMin + t0 * (logMax - logMin));
            const float f1 = (float) std::exp (logMin + t1 * (logMax - logMin));
            const float ratio = juce::jmax (1.001f, f1 / juce::jmax (1.0f, f0));
            q = juce::jlimit (0.35f, 8.0f, 1.0f / std::log (ratio));
            q *= latticeQScaleForSmooth (latticeSmooth01);
            q = juce::jlimit (0.25f, 8.0f, q);
        }

        auto& s = slices[(size_t) i];
        s.centerHz = f;
        s.q = q;

        if (auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, f, q))
        {
            *s.detect.coefficients = *coeffs;
            *s.applyL.coefficients = *coeffs;
            *s.applyR.coefficients = *coeffs;
            ++activeSlices;
        }
    }
}

void Processor::normalizeWorkingTarget() noexcept
{
    if (activeSlices <= 0)
        return;

    double sum = 0.0;
    for (int i = 0; i < activeSlices; ++i)
        sum += (double) workingTargetDb[(size_t) i];
    const float mean = (float) (sum / (double) activeSlices);
    for (int i = 0; i < activeSlices; ++i)
        workingTargetDb[(size_t) i] -= mean;
}

void Processor::fillFactoryTarget (int curveIndex) noexcept
{
    const float slope = slopeDbPerOctave (curveIndex);
    for (int i = 0; i < activeSlices; ++i)
    {
        const float f = juce::jmax (1.0f, slices[(size_t) i].centerHz);
        workingTargetDb[(size_t) i] = slope * std::log2 (f / kRefHz);
    }
    normalizeWorkingTarget();

    const juce::SpinLock::ScopedLockType lock (targetLock);
    publishedTargetDb = workingTargetDb;
}

void Processor::setFactoryTarget (int curveIndex) noexcept
{
    if (curveIndex == capture)
        return;
    fillFactoryTarget (juce::jlimit (0, numFactoryCurves - 1, curveIndex));
}

void Processor::setWorkingTargetDb (const std::array<float, kNumSlices>& db) noexcept
{
    workingTargetDb = db;
    normalizeWorkingTarget();
    const juce::SpinLock::ScopedLockType lock (targetLock);
    publishedTargetDb = workingTargetDb;
}

std::array<float, kNumSlices> Processor::getWorkingTargetDb() const noexcept
{
    const juce::SpinLock::ScopedLockType lock (targetLock);
    return publishedTargetDb;
}

std::array<float, kNumSlices> Processor::getCenterHz() const noexcept
{
    std::array<float, kNumSlices> out {};
    for (int i = 0; i < activeSlices; ++i)
        out[(size_t) i] = slices[(size_t) i].centerHz;
    return out;
}

void Processor::setCaptureTargetFromSpectrum (const float* magnitudes,
                                              const float* frequenciesHz,
                                              int numBins) noexcept
{
    if (magnitudes == nullptr || frequenciesHz == nullptr || numBins <= 0 || activeSlices <= 0)
        return;

    for (int i = 0; i < activeSlices; ++i)
    {
        const float f = slices[(size_t) i].centerHz;
        // Nearest bin in log-f.
        int best = 0;
        float bestDist = 1.0e9f;
        for (int b = 0; b < numBins; ++b)
        {
            const float fb = juce::jmax (1.0f, frequenciesHz[b]);
            const float d = std::abs (std::log (fb) - std::log (juce::jmax (1.0f, f)));
            if (d < bestDist)
            {
                bestDist = d;
                best = b;
            }
        }

        const float mag = juce::jmax (1.0e-6f, magnitudes[best]);
        // Scope data is typically 0..1 display; treat as linear magnitude proxy.
        workingTargetDb[(size_t) i] = juce::Decibels::gainToDecibels (mag, kSilenceFloorDb);
    }

    normalizeWorkingTarget();
    const juce::SpinLock::ScopedLockType lock (targetLock);
    publishedTargetDb = workingTargetDb;
}

void Processor::sampleTargetDb (const float* frequenciesHz, float* destDb, int numPoints) const noexcept
{
    if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
        return;

    std::array<float, kNumSlices> tgt {};
    std::array<float, kNumSlices> centers {};
    int n = 0;
    {
        const juce::SpinLock::ScopedLockType lock (targetLock);
        tgt = publishedTargetDb;
        n = activeSlices;
        for (int i = 0; i < n; ++i)
            centers[(size_t) i] = slices[(size_t) i].centerHz;
    }

    if (n <= 0)
    {
        juce::FloatVectorOperations::clear (destDb, numPoints);
        return;
    }

    for (int i = 0; i < numPoints; ++i)
    {
        const float f = juce::jmax (1.0f, frequenciesHz[i]);
        const float logF = std::log (f);

        // Piecewise-linear in log-f between slice centres.
        if (f <= centers[0])
        {
            destDb[i] = tgt[0];
            continue;
        }
        if (f >= centers[(size_t) (n - 1)])
        {
            destDb[i] = tgt[(size_t) (n - 1)];
            continue;
        }

        int k = 0;
        while (k + 1 < n && centers[(size_t) (k + 1)] < f)
            ++k;

        const float c0 = centers[(size_t) k];
        const float c1 = centers[(size_t) (k + 1)];
        const float t = (logF - std::log (c0)) / juce::jmax (1.0e-6f, std::log (c1) - std::log (c0));
        destDb[i] = tgt[(size_t) k] + (tgt[(size_t) (k + 1)] - tgt[(size_t) k]) * juce::jlimit (0.0f, 1.0f, t);
    }
}

void Processor::clearPublished() noexcept
{
    publishedPeakGrDb.store (0.0f, std::memory_order_relaxed);
    const int writeIdx = 1 - publishedIndex.load (std::memory_order_relaxed);
    publishedCurves[(size_t) writeIdx].count = 0;
    publishedIndex.store (writeIdx, std::memory_order_release);
}

void Processor::publishGrCurve() noexcept
{
    const int writeIdx = 1 - publishedIndex.load (std::memory_order_relaxed);
    auto& dest = publishedCurves[(size_t) writeIdx];
    dest.count = 0;
    float peak = 0.0f;

    for (int b = 0; b < activeSlices; ++b)
    {
        const auto& s = slices[(size_t) b];
        dest.centerHz[(size_t) dest.count] = s.centerHz;
        const float grDb = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, s.grLin), -100.0f);
        dest.grDb[(size_t) dest.count] = grDb;
        peak = juce::jmax (peak, std::abs (grDb));
        ++dest.count;
    }

    publishedPeakGrDb.store (peak, std::memory_order_relaxed);
    publishedIndex.store (writeIdx, std::memory_order_release);
}

void Processor::samplePublishedGrDb (const float* frequenciesHz, float* destDb, int numPoints) const noexcept
{
    // Display path: log-f Catmull-Rom through slice GR centres (same spirit as the
    // target overlay). Raised-cosine BP lobes were accurate to the lattice partition
    // but drew scallops/stairs even when the underlying shape was a smooth slope.
    // Match Smooth still affects DSP Q + neighbour GR blend; this only changes the curve draw.
    if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
        return;

    const int idx = publishedIndex.load (std::memory_order_acquire);
    const auto& src = publishedCurves[(size_t) juce::jlimit (0, 1, idx)];
    if (src.count <= 0)
    {
        juce::FloatVectorOperations::clear (destDb, numPoints);
        return;
    }

    const float nyquist = (float) sampleRate * 0.5f;
    const int n = src.count;
    std::array<float, kNumSlices> logC {};
    std::array<float, kNumSlices> gr {};
    for (int k = 0; k < n; ++k)
    {
        logC[(size_t) k] = std::log (juce::jmax (1.0f, src.centerHz[(size_t) k]));
        gr[(size_t) k] = src.grDb[(size_t) k];
    }

    auto catmull = [] (float p0, float p1, float p2, float p3, float t) noexcept
    {
        const float t2 = t * t;
        const float t3 = t2 * t;
        return 0.5f * ((2.0f * p1)
                       + (-p0 + p2) * t
                       + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                       + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
    };

    int seg = 0;
    for (int i = 0; i < numPoints; ++i)
    {
        const float f = juce::jlimit (1.0f, nyquist, frequenciesHz[i]);
        const float logF = std::log (f);

        if (n == 1 || logF <= logC[0])
        {
            destDb[i] = gr[0];
            continue;
        }
        if (logF >= logC[(size_t) (n - 1)])
        {
            destDb[i] = gr[(size_t) (n - 1)];
            continue;
        }

        while (seg + 1 < n && logC[(size_t) (seg + 1)] < logF)
            ++seg;

        const int i1 = seg;
        const int i2 = juce::jmin (n - 1, i1 + 1);
        const int i0 = juce::jmax (0, i1 - 1);
        const int i3 = juce::jmin (n - 1, i2 + 1);

        const float denom = juce::jmax (1.0e-6f, logC[(size_t) i2] - logC[(size_t) i1]);
        const float t = juce::jlimit (0.0f, 1.0f, (logF - logC[(size_t) i1]) / denom);
        destDb[i] = catmull (gr[(size_t) i0], gr[(size_t) i1],
                             gr[(size_t) i2], gr[(size_t) i3], t);
    }
}

void Processor::applySmoothToLatticeIfNeeded (float smooth01) noexcept
{
    const float s = juce::jlimit (kMinSmooth, kMaxSmooth, smooth01);
    if (std::abs (s - latticeSmooth01) < 1.0e-4f)
        return;

    latticeSmooth01 = s;
    if (! prepared)
        return;

    // Preserve envelopes / GR while swapping BP coeffs for the new Q packing.
    std::array<float, kNumSlices> env {};
    std::array<float, kNumSlices> grL {};
    std::array<float, kNumSlices> grT {};
    for (int i = 0; i < activeSlices; ++i)
    {
        env[(size_t) i] = slices[(size_t) i].envDb;
        grL[(size_t) i] = slices[(size_t) i].grLin;
        grT[(size_t) i] = slices[(size_t) i].grTarget;
    }

    rebuildLattice();

    for (int i = 0; i < activeSlices; ++i)
    {
        slices[(size_t) i].envDb = env[(size_t) i];
        slices[(size_t) i].grLin = grL[(size_t) i];
        slices[(size_t) i].grTarget = grT[(size_t) i];
        slices[(size_t) i].detect.reset();
        slices[(size_t) i].applyL.reset();
        slices[(size_t) i].applyR.reset();
    }
}

void Processor::process (juce::AudioBuffer<float>& buffer,
                         const float* detectL,
                         const float* detectR,
                         bool enabled,
                         float amount,
                         int speedMode,
                         float smooth01,
                         float hpHz,
                         float lpHz) noexcept
{
    const int numCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (numCh < 1 || n <= 0 || detectL == nullptr)
    {
        clearPublished();
        return;
    }

    if (! prepared || activeSlices <= 0)
    {
        prepare (sampleRate > 0.0 ? sampleRate : 48000.0, juce::jmax (blockSize, n));
        if (activeSlices <= 0)
        {
            clearPublished();
            return;
        }
    }

    if (! enabled && ! wasEnabled && ! settling)
    {
        clearPublished();
        return;
    }

    if (enabled && ! wasEnabled)
        reset();

    wasEnabled = enabled;
    settling = ! enabled;

    applySmoothToLatticeIfNeeded (smooth01);

    const float* detR = detectR != nullptr ? detectR : detectL;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float attackMs = 12.0f, releaseMs = 100.0f, grSmoothMs = 12.0f;
    getBallisticsMs (speedMode, attackMs, releaseMs, grSmoothMs);
    const float atk = DynamicEq::coeffForTimeMs (attackMs, sampleRate, n);
    const float rel = DynamicEq::coeffForTimeMs (releaseMs, sampleRate, n);
    const float grCoeff = 1.0f - std::exp (-1.0f / juce::jmax (1.0f,
        (float) sampleRate * grSmoothMs * 0.001f));
    const float amountClamped = juce::jlimit (kMinAmount, kMaxAmount, amount);
    const float spatialSmooth = juce::jlimit (kMinSmooth, kMaxSmooth, smooth01);
    const float bandLo = juce::jlimit (kMinHpLpHz, kMaxFreqHz, juce::jmin (hpHz, lpHz));
    const float bandHi = juce::jlimit (kMinHpLpHz, kMaxFreqHz, juce::jmax (hpHz, lpHz));

    std::array<float, kNumSlices> tgt {};
    {
        const juce::SpinLock::ScopedLockType lock (targetLock);
        tgt = publishedTargetDb;
    }

    for (int i = 0; i < activeSlices; ++i)
        slices[(size_t) i].sumSq = 0.0;

    // Detect pass (mono mid).
    for (int i = 0; i < n; ++i)
    {
        const float mid = 0.5f * (detectL[i] + detR[i]);
        for (int b = 0; b < activeSlices; ++b)
        {
            auto& s = slices[(size_t) b];
            const float d = s.detect.processSample (mid);
            s.sumSq += (double) d * (double) d;
        }
    }

    // Update envelopes + GR targets (level-normalized shape error inside HP/LP window).
    double measSum = 0.0;
    double tgtSum = 0.0;
    int inBandCount = 0;
    std::array<float, kNumSlices> measDb {};
    std::array<bool, kNumSlices> inBand {};
    for (int b = 0; b < activeSlices; ++b)
    {
        auto& s = slices[(size_t) b];
        const float rms = std::sqrt ((float) (s.sumSq / (double) n));
        const float level = juce::Decibels::gainToDecibels (rms, kSilenceFloorDb);
        const float c = level > s.envDb ? atk : rel;
        s.envDb = c * s.envDb + (1.0f - c) * level;
        measDb[(size_t) b] = s.envDb;
        inBand[(size_t) b] = s.centerHz >= bandLo && s.centerHz <= bandHi;
        if (inBand[(size_t) b])
        {
            measSum += (double) s.envDb;
            tgtSum += (double) tgt[(size_t) b];
            ++inBandCount;
        }
    }

    const float measMean = (float) (measSum / (double) juce::jmax (1, inBandCount));
    const float tgtMean = (float) (tgtSum / (double) juce::jmax (1, inBandCount));

    for (int b = 0; b < activeSlices; ++b)
    {
        auto& s = slices[(size_t) b];
        float targetGrDb = 0.0f;
        if (enabled && amountClamped > 1.0e-4f
            && inBandCount > 0
            && inBand[(size_t) b]
            && measDb[(size_t) b] > kSilenceFloorDb + 1.0f)
        {
            // Positive err = too loud vs target shape -> cut; negative -> boost.
            const float err = (measDb[(size_t) b] - measMean) - (tgt[(size_t) b] - tgtMean);
            targetGrDb = juce::jlimit (-kMaxGrDb, kMaxGrDb, -err * amountClamped);
        }
        else if (! enabled)
        {
            targetGrDb = 0.0f;
        }

        s.grTarget = juce::Decibels::decibelsToGain (targetGrDb);
    }

    // Spatial GR blend across neighbours (0 = current stepped lattice, 1 = soft).
    if (spatialSmooth > 1.0e-4f && activeSlices > 1)
    {
        std::array<float, kNumSlices> rawGr {};
        for (int b = 0; b < activeSlices; ++b)
            rawGr[(size_t) b] = slices[(size_t) b].grTarget;

        for (int b = 0; b < activeSlices; ++b)
        {
            const float left = rawGr[(size_t) juce::jmax (0, b - 1)];
            const float center = rawGr[(size_t) b];
            const float right = rawGr[(size_t) juce::jmin (activeSlices - 1, b + 1)];
            const float neigh = 0.25f * (left + 2.0f * center + right);
            slices[(size_t) b].grTarget = (1.0f - spatialSmooth) * center + spatialSmooth * neigh;
        }
    }

    // Keep Match GR strictly inside the HP/LP window after Smooth blend.
    for (int b = 0; b < activeSlices; ++b)
        if (! inBand[(size_t) b])
            slices[(size_t) b].grTarget = 1.0f;

    // Apply reconstruct on wet buffer (BP fed from pre-apply sample, like Side Check).
    bool anyGr = false;
    for (int i = 0; i < n; ++i)
    {
        const float inL = outL[i];
        const float inR = outR != nullptr ? outR[i] : inL;
        float l = inL;
        float r = inR;

        for (int b = 0; b < activeSlices; ++b)
        {
            auto& s = slices[(size_t) b];
            s.grLin += (s.grTarget - s.grLin) * grCoeff;
            if (std::abs (s.grLin - 1.0f) > 1.0e-4f)
                anyGr = true;

            const float bpL = s.applyL.processSample (inL);
            l += bpL * (s.grLin - 1.0f);
            if (outR != nullptr)
            {
                const float bpR = s.applyR.processSample (inR);
                r += bpR * (s.grLin - 1.0f);
            }
        }

        outL[i] = l;
        if (outR != nullptr)
            outR[i] = r;
    }

    if (! enabled && ! anyGr)
    {
        settling = false;
        clearPublished();
        return;
    }

    publishGrCurve();
}

juce::ValueTree Processor::toUserPresetsTree() const
{
    juce::ValueTree root ("matchUserCurves");
    for (int i = 0; i < numUserPresets; ++i)
    {
        juce::ValueTree p ("preset");
        p.setProperty ("name", userPresets[(size_t) i].name, nullptr);
        juce::String csv;
        for (int s = 0; s < kNumSlices; ++s)
        {
            if (s > 0)
                csv << ",";
            csv << juce::String (userPresets[(size_t) i].targetDb[(size_t) s], 4);
        }
        p.setProperty ("db", csv, nullptr);
        root.appendChild (p, nullptr);
    }
    return root;
}

void Processor::fromUserPresetsTree (const juce::ValueTree& tree)
{
    numUserPresets = 0;
    if (! tree.isValid() || ! tree.hasType ("matchUserCurves"))
        return;

    for (int i = 0; i < tree.getNumChildren() && numUserPresets < kMaxUserPresets; ++i)
    {
        const auto child = tree.getChild (i);
        if (! child.hasType ("preset"))
            continue;

        auto& slot = userPresets[(size_t) numUserPresets];
        slot.name = child.getProperty ("name", "Match curve").toString();
        slot.targetDb.fill (0.0f);

        const auto csv = child.getProperty ("db").toString();
        juce::StringArray parts;
        parts.addTokens (csv, ",", {});
        for (int s = 0; s < juce::jmin (kNumSlices, parts.size()); ++s)
            slot.targetDb[(size_t) s] = parts[s].getFloatValue();

        ++numUserPresets;
    }
}

juce::String Processor::getUserPresetName (int index) const
{
    if (index < 0 || index >= numUserPresets)
        return {};
    return userPresets[(size_t) index].name;
}

bool Processor::saveUserPreset (const juce::String& name)
{
    if (numUserPresets >= kMaxUserPresets)
        return false;

    auto& slot = userPresets[(size_t) numUserPresets];
    slot.name = name.isNotEmpty() ? name : ("Match curve " + juce::String (numUserPresets + 1));
    {
        const juce::SpinLock::ScopedLockType lock (targetLock);
        slot.targetDb = publishedTargetDb;
    }
    ++numUserPresets;
    return true;
}

bool Processor::loadUserPreset (int index)
{
    if (index < 0 || index >= numUserPresets)
        return false;
    setWorkingTargetDb (userPresets[(size_t) index].targetDb);
    return true;
}

bool Processor::removeUserPreset (int index)
{
    if (index < 0 || index >= numUserPresets)
        return false;
    for (int i = index; i < numUserPresets - 1; ++i)
        userPresets[(size_t) i] = userPresets[(size_t) (i + 1)];
    --numUserPresets;
    return true;
}

} // namespace MatchEq
