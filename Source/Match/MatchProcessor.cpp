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
        s.sumSq = 0.0f;
    }

    desiredSliceCount = kNumSlices;
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
        s.sumSq = 0.0f;
    }
    clearPublished();
    wasEnabled = false;
    settling = false;
}

float Processor::interpTargetDb (const float* centersHz, const float* db, int n, float fHz) noexcept
{
    if (centersHz == nullptr || db == nullptr || n <= 0)
        return 0.0f;

    const float f = juce::jmax (1.0f, fHz);
    if (n == 1 || f <= centersHz[0])
        return db[0];
    if (f >= centersHz[n - 1])
        return db[n - 1];

    int k = 0;
    while (k + 1 < n && centersHz[k + 1] < f)
        ++k;

    const float c0 = juce::jmax (1.0f, centersHz[k]);
    const float c1 = juce::jmax (c0 * 1.0001f, centersHz[k + 1]);
    const float t = (std::log (f) - std::log (c0)) / juce::jmax (1.0e-6f, std::log (c1) - std::log (c0));
    return db[k] + (db[k + 1] - db[k]) * juce::jlimit (0.0f, 1.0f, t);
}

void Processor::rebuildLattice() noexcept
{
    const float maxHz = juce::jmin (20000.0f, (float) sampleRate * 0.45f);
    const float minHz = 20.0f;
    activeSlices = 0;

    const int count = juce::jlimit (1, kNumSlices, desiredSliceCount);

    if (! (maxHz > minHz * 1.01f) || sampleRate <= 0.0)
        return;

    const double logMin = std::log ((double) minHz);
    const double logMax = std::log ((double) maxHz);

    for (int i = 0; i < count; ++i)
    {
        const double t = (count == 1) ? 0.5
            : (double) i / (double) (count - 1);
        const float f = (float) std::exp (logMin + t * (logMax - logMin));

        // Constant-Q packing so neighbours form a soft partition.
        // Smooth widens Q (lower Q) so bands overlap more and steps soften.
        float q = 1.0f;
        if (count >= 2)
        {
            const double t0 = (double) juce::jmax (0, i - 1) / (double) (count - 1);
            const double t1 = (double) juce::jmin (count - 1, i + 1) / (double) (count - 1);
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

void Processor::ensureLatticeConfig (float smooth01, int resolutionMode) noexcept
{
    const float s = juce::jlimit (kMinSmooth, kMaxSmooth, smooth01);
    const int wantSlices = sliceCountForResolution (resolutionMode);
    const bool smoothChanged = std::abs (s - latticeSmooth01) >= 1.0e-4f;
    const bool resChanged = wantSlices != desiredSliceCount || activeSlices != wantSlices;

    if (! smoothChanged && ! resChanged)
        return;

    std::array<float, kNumSlices> oldCenters {};
    std::array<float, kNumSlices> oldTgt {};
    std::array<float, kNumSlices> env {};
    std::array<float, kNumSlices> grL {};
    std::array<float, kNumSlices> grT {};
    const int oldN = activeSlices;
    {
        const juce::SpinLock::ScopedLockType lock (targetLock);
        oldTgt = publishedTargetDb;
    }
    for (int i = 0; i < oldN; ++i)
    {
        oldCenters[(size_t) i] = slices[(size_t) i].centerHz;
        env[(size_t) i] = slices[(size_t) i].envDb;
        grL[(size_t) i] = slices[(size_t) i].grLin;
        grT[(size_t) i] = slices[(size_t) i].grTarget;
    }

    latticeSmooth01 = s;
    desiredSliceCount = wantSlices;

    if (! prepared)
        return;

    rebuildLattice();

    if (resChanged && oldN > 0 && activeSlices > 0)
    {
        for (int i = 0; i < activeSlices; ++i)
            workingTargetDb[(size_t) i] = interpTargetDb (oldCenters.data(), oldTgt.data(),
                                                          oldN, slices[(size_t) i].centerHz);
        for (int i = activeSlices; i < kNumSlices; ++i)
            workingTargetDb[(size_t) i] = 0.0f;
        normalizeWorkingTarget();
        const juce::SpinLock::ScopedLockType lock (targetLock);
        publishedTargetDb = workingTargetDb;
    }

    // Preserve envelopes / GR where indices still overlap; reset IIR state after coeff swap.
    const int keep = juce::jmin (oldN, activeSlices);
    for (int i = 0; i < keep; ++i)
    {
        slices[(size_t) i].envDb = env[(size_t) i];
        slices[(size_t) i].grLin = grL[(size_t) i];
        slices[(size_t) i].grTarget = grT[(size_t) i];
    }
    for (int i = keep; i < activeSlices; ++i)
    {
        slices[(size_t) i].envDb = kSilenceFloorDb;
        slices[(size_t) i].grLin = 1.0f;
        slices[(size_t) i].grTarget = 1.0f;
    }
    for (int i = 0; i < activeSlices; ++i)
    {
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
                         float lpHz,
                         int resolutionMode,
                         int hpSlope,
                         int lpSlope) noexcept
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

    ensureLatticeConfig (smooth01, resolutionMode);

    const float* detR = detectR != nullptr ? detectR : detectL;
    auto* outL = buffer.getWritePointer (0);
    auto* outR = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    float attackMs = 12.0f, releaseMs = 100.0f, grSmoothMs = 12.0f;
    getBallisticsMs (speedMode, attackMs, releaseMs, grSmoothMs);
    const float atk = DynamicEq::coeffForTimeMs (attackMs, sampleRate, n);
    const float rel = DynamicEq::coeffForTimeMs (releaseMs, sampleRate, n);
    // Block-rate GR smooth (same time constant as former per-sample path).
    const float grSmooth = DynamicEq::coeffForTimeMs (grSmoothMs, sampleRate, n);
    const float amountClamped = juce::jlimit (kMinAmount, kMaxAmount, amount);
    const float spatialSmooth = juce::jlimit (kMinSmooth, kMaxSmooth, smooth01);
    const float bandLo = juce::jlimit (kMinHpLpHz, kMaxFreqHz, juce::jmin (hpHz, lpHz));
    const float bandHi = juce::jlimit (kMinHpLpHz, kMaxFreqHz, juce::jmax (hpHz, lpHz));

    juce::ignoreUnused (bandLo, bandHi);

    // Cache HP/LP stages once per block (not per slice).
    const bool useHp = hpHz > 1.0f;
    const bool useLp = lpHz > 1.0f && lpHz < (float) sampleRate * 0.49f;
    const auto hpStages = useHp
        ? FilterSlope::makeHighpassCoeffs (sampleRate, hpHz, 0.70710678f, hpSlope)
        : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};
    const auto lpStages = useLp
        ? FilterSlope::makeLowpassCoeffs (sampleRate, lpHz, 0.70710678f, lpSlope)
        : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};

    std::array<float, kNumSlices> edgeW {};
    std::array<int, kNumSlices> activeIdx {};
    int numActive = 0;
    for (int b = 0; b < activeSlices; ++b)
    {
        float w = 1.0f;
        if (useHp)
            w *= FilterSlope::cascadeMagnitudeAt (hpStages, (double) slices[(size_t) b].centerHz, sampleRate);
        if (useLp)
            w *= FilterSlope::cascadeMagnitudeAt (lpStages, (double) slices[(size_t) b].centerHz, sampleRate);
        edgeW[(size_t) b] = juce::jlimit (0.0f, 1.0f, w);
        if (edgeW[(size_t) b] > 0.02f)
            activeIdx[(size_t) numActive++] = b;
    }

    std::array<float, kNumSlices> tgt {};
    if (numActive > 0 && (enabled || settling))
    {
        const juce::SpinLock::ScopedLockType lock (targetLock);
        tgt = publishedTargetDb;
    }

    for (int a = 0; a < numActive; ++a)
        slices[(size_t) activeIdx[(size_t) a]].sumSq = 0.0f;

    // Detect pass — only in-band slices.
    for (int i = 0; i < n; ++i)
    {
        const float mid = 0.5f * (detectL[i] + detR[i]);
        for (int a = 0; a < numActive; ++a)
        {
            auto& s = slices[(size_t) activeIdx[(size_t) a]];
            const float d = s.detect.processSample (mid);
            s.sumSq += d * d;
        }
    }

    // Envelopes + shape-error GR (in-band only).
    float measSum = 0.0f;
    float tgtSum = 0.0f;
    std::array<float, kNumSlices> measDb {};
    for (int a = 0; a < numActive; ++a)
    {
        const int b = activeIdx[(size_t) a];
        auto& s = slices[(size_t) b];
        const float rms = std::sqrt (s.sumSq / (float) n);
        const float level = juce::Decibels::gainToDecibels (rms, kSilenceFloorDb);
        const float c = level > s.envDb ? atk : rel;
        s.envDb = c * s.envDb + (1.0f - c) * level;
        measDb[(size_t) b] = s.envDb;
        measSum += s.envDb;
        tgtSum += tgt[(size_t) b];
    }

    const float measMean = numActive > 0 ? measSum / (float) numActive : 0.0f;
    const float tgtMean = numActive > 0 ? tgtSum / (float) numActive : 0.0f;

    for (int b = 0; b < activeSlices; ++b)
    {
        auto& s = slices[(size_t) b];
        float targetGrDb = 0.0f;
        if (enabled && amountClamped > 1.0e-4f
            && numActive > 0
            && edgeW[(size_t) b] > 0.02f
            && measDb[(size_t) b] > kSilenceFloorDb + 1.0f)
        {
            const float err = (measDb[(size_t) b] - measMean) - (tgt[(size_t) b] - tgtMean);
            targetGrDb = juce::jlimit (-kMaxGrDb, kMaxGrDb, -err * amountClamped * edgeW[(size_t) b]);
        }
        s.grTarget = juce::Decibels::decibelsToGain (targetGrDb);
    }

    // Spatial GR blend (active neighbours only; outside window forced to unity after).
    if (spatialSmooth > 1.0e-4f && numActive > 1)
    {
        std::array<float, kNumSlices> rawGr {};
        for (int b = 0; b < activeSlices; ++b)
            rawGr[(size_t) b] = slices[(size_t) b].grTarget;

        for (int a = 0; a < numActive; ++a)
        {
            const int b = activeIdx[(size_t) a];
            const int bL = activeIdx[(size_t) juce::jmax (0, a - 1)];
            const int bR = activeIdx[(size_t) juce::jmin (numActive - 1, a + 1)];
            const float left = rawGr[(size_t) bL];
            const float center = rawGr[(size_t) b];
            const float right = rawGr[(size_t) bR];
            const float neigh = 0.25f * (left + 2.0f * center + right);
            slices[(size_t) b].grTarget = (1.0f - spatialSmooth) * center + spatialSmooth * neigh;
        }
    }

    for (int b = 0; b < activeSlices; ++b)
        if (edgeW[(size_t) b] <= 0.02f)
            slices[(size_t) b].grTarget = 1.0f;

    // Block-rate GR smooth + build apply list (skip unity GR).
    std::array<int, kNumSlices> applyIdx {};
    int numApply = 0;
    bool anyGr = false;
    for (int b = 0; b < activeSlices; ++b)
    {
        auto& s = slices[(size_t) b];
        s.grLin = grSmooth * s.grLin + (1.0f - grSmooth) * s.grTarget;
        if (std::abs (s.grLin - 1.0f) > 1.0e-4f || std::abs (s.grTarget - 1.0f) > 1.0e-4f)
        {
            anyGr = true;
            if (edgeW[(size_t) b] > 0.02f || std::abs (s.grLin - 1.0f) > 1.0e-4f)
                applyIdx[(size_t) numApply++] = b;
        }
    }

    // Apply reconstruct — only slices that still move the signal.
    if (numApply > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            const float inL = outL[i];
            const float inR = outR != nullptr ? outR[i] : inL;
            float l = inL;
            float r = inR;

            for (int a = 0; a < numApply; ++a)
            {
                auto& s = slices[(size_t) applyIdx[(size_t) a]];
                const float g = s.grLin - 1.0f;
                l += s.applyL.processSample (inL) * g;
                if (outR != nullptr)
                    r += s.applyR.processSample (inR) * g;
            }

            outL[i] = l;
            if (outR != nullptr)
                outR[i] = r;
        }
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
