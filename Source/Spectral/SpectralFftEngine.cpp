#include "SpectralFftEngine.h"
#include "../DynamicEq.h"
#include <cmath>

namespace
{
    void fillPeriodicHann (float* w, int n) noexcept
    {
        // Periodic Hann: w[i] = 0.5 * (1 - cos(2π i / N)), i = 0..N-1
        const float delta = juce::MathConstants<float>::twoPi / (float) n;
        for (int i = 0; i < n; ++i)
            w[i] = 0.5f * (1.0f - std::cos (delta * (float) i));
    }

    void copyFifoToLinear (const float* fifo, int pos, float* dest, int n) noexcept
    {
        // Oldest sample at dest[0]; pos points one past newest.
        const int first = n - pos;
        if (first > 0)
            std::memcpy (dest, fifo + pos, (size_t) first * sizeof (float));
        if (pos > 0)
            std::memcpy (dest + first, fifo, (size_t) pos * sizeof (float));
    }
}

void SpectralFftEngine::prepare (double newSampleRate, int /*maximumBlockSize*/, int channels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numChannels = juce::jmax (1, channels);
    fillPeriodicHann (window.data(), kFftSize);
    reset();
}

void SpectralFftEngine::reset()
{
    pos = 0;
    hopCount = 0;
    std::fill (inFifoL.begin(), inFifoL.end(), 0.0f);
    std::fill (inFifoR.begin(), inFifoR.end(), 0.0f);
    std::fill (detFifo.begin(), detFifo.end(), 0.0f);
    std::fill (scFifo.begin(), scFifo.end(), 0.0f);
    std::fill (outFifoL.begin(), outFifoL.end(), 0.0f);
    std::fill (outFifoR.begin(), outFifoR.end(), 0.0f);
    for (auto& e : envDbSlot)
        e.fill (DynamicEq::kSilenceFloorDb);
    for (auto& g : grDbSmoothSlice)
        g.fill (0.0f);
    for (auto& c : sliceCenterHz)
        c.fill (0.0f);
    for (auto& g : sliceGrDb)
        g.fill (0.0f);
    sliceCount.fill (0);
    grDbBin.fill (0.0f);
    grLinBin.fill (1.0f);

    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        publishedIndex[(size_t) s].store (0, std::memory_order_relaxed);
        publishedPeakGrDb[(size_t) s].store (0.0f, std::memory_order_relaxed);
        for (auto& buf : publishedGr[(size_t) s])
        {
            buf.count = 0;
            buf.centerHz.fill (0.0f);
            buf.grDb.fill (0.0f);
        }
    }
    publishedArmed.store (0, std::memory_order_relaxed);
    publishedBins.store (0, std::memory_order_relaxed);
    stftWasRunning = false;
}

void SpectralFftEngine::clearBands() noexcept
{
    activeBandCount = 0;
    for (auto& b : bands)
    {
        b.active = false;
        b.settings.enabled = false;
    }
}

void SpectralFftEngine::setBand (int slot, const SpectralDynamics::BandSettings& settings) noexcept
{
    if (slot < 0 || slot >= SpectralDynamics::kNumSlots)
        return;

    auto& band = bands[(size_t) slot];
    if (! settings.enabled || settings.amount < SpectralDynamics::kAmountEpsilon)
    {
        band.active = false;
        band.settings.enabled = false;
        return;
    }

    band.settings = settings;
    band.settings.bandwidthHz = SpectralBinning::clampBandwidthHz (settings.bandwidthHz);
    band.active = true;
    ++activeBandCount;
}

float SpectralFftEngine::safeMaxCenterHz() const noexcept
{
    return juce::jmin (20000.0f, (float) sampleRate * 0.45f);
}

float SpectralFftEngine::binFrequencyHz (int bin) const noexcept
{
    return (float) bin * (float) sampleRate / (float) kFftSize;
}

int SpectralFftEngine::sliceCountForRes (float bandwidthHz) noexcept
{
    // Same Res curve as lattice budget, remapped to a dramatic visible range:
    // coarse (2.0 Hz) → kMinSlices, fine (0.5 Hz) → kMaxSlices.
    const int budget = SpectralBinning::bandpassBudgetForBandwidth (bandwidthHz);
    const float t = (float) (budget - SpectralBinning::kMinBandpassBudget)
                  / (float) juce::jmax (1, SpectralBinning::kMaxBandpasses
                                           - SpectralBinning::kMinBandpassBudget);
    const float tClamped = juce::jlimit (0.0f, 1.0f, t);
    const int n = (int) std::lround (
        (double) kMinSlices + (double) tClamped * (double) (kMaxSlices - kMinSlices));
    return juce::jlimit (kMinSlices, kMaxSlices, n);
}

void SpectralFftEngine::computeInfluenceRange (const SpectralDynamics::BandSettings& settings,
                                               float& fLo, float& fHi) const noexcept
{
    const float maxHz = safeMaxCenterHz();
    const float fc = juce::jlimit (20.0f, maxHz, settings.frequencyHz);
    const float q = juce::jmax (0.05f, settings.q);
    const float sigma = juce::jmax (0.04f, 0.55f / q);
    const float logHalf = sigma * 2.45f;

    float loMul = std::exp (-logHalf);
    float hiMul = std::exp (+logHalf);
    switch (settings.shape)
    {
        case SpectralDynamics::BandShape::highShelf:
            loMul = std::exp (-0.8f * logHalf);
            hiMul = std::exp (+1.2f * logHalf);
            break;
        case SpectralDynamics::BandShape::lowShelf:
            loMul = std::exp (-1.2f * logHalf);
            hiMul = std::exp (+0.8f * logHalf);
            break;
        case SpectralDynamics::BandShape::bell:
        default:
            break;
    }

    fLo = juce::jlimit (20.0f, maxHz, fc * loMul);
    fHi = juce::jlimit (20.0f, maxHz, fc * hiMul);
    if (fHi < fLo)
        std::swap (fLo, fHi);

    if (fHi <= fLo * 1.001f)
    {
        fLo = juce::jmax (20.0f, fc * 0.97f);
        fHi = juce::jmin (maxHz, fc * 1.03f);
        if (fHi <= fLo)
            fHi = juce::jmin (maxHz, fLo * 1.03f);
    }
}

float SpectralFftEngine::sampleEnvAtHz (const float* envDb, int numBins, float frequencyHz,
                                        double sr, int fftSize) noexcept
{
    if (envDb == nullptr || numBins < 3 || sr <= 0.0 || fftSize <= 0)
        return DynamicEq::kSilenceFloorDb;

    const float binF = frequencyHz * (float) fftSize / (float) sr;
    const int b0 = juce::jlimit (1, numBins - 2, (int) std::floor (binF));
    const int b1 = juce::jmin (numBins - 2, b0 + 1);
    const float frac = juce::jlimit (0.0f, 1.0f, binF - (float) b0);
    return envDb[b0] * (1.0f - frac) + envDb[b1] * frac;
}

float SpectralFftEngine::bandMask (float frequencyHz, const SpectralDynamics::BandSettings& settings) const noexcept
{
    const float fc = juce::jlimit (20.0f, safeMaxCenterHz(), settings.frequencyHz);
    const float f = juce::jmax (1.0f, frequencyHz);
    const float q = juce::jmax (0.05f, settings.q);
    const float sigma = juce::jmax (0.04f, 0.55f / q);
    const float x = std::log (f / fc) / sigma;

    switch (settings.shape)
    {
        case SpectralDynamics::BandShape::highShelf:
        {
            const float step = 0.5f * (1.0f + std::tanh (x));
            const float bell = std::exp (-0.5f * x * x);
            return juce::jmax (step, bell * 0.35f);
        }
        case SpectralDynamics::BandShape::lowShelf:
        {
            const float step = 0.5f * (1.0f + std::tanh (-x));
            const float bell = std::exp (-0.5f * x * x);
            return juce::jmax (step, bell * 0.35f);
        }
        case SpectralDynamics::BandShape::bell:
        default:
            return std::exp (-0.5f * x * x);
    }
}

void SpectralFftEngine::process (juce::dsp::AudioBlock<float>& block,
                                 const float* detectL,
                                 const float* detectR,
                                 const float* scDetectL,
                                 const float* scDetectR)
{
    const int n = (int) block.getNumSamples();
    const int ch = (int) block.getNumChannels();
    if (n <= 0 || ch <= 0)
        return;

    float* wetL = block.getChannelPointer (0);
    float* wetR = ch > 1 ? block.getChannelPointer (1) : wetL;

    // No armed S bands: pure delay of getLatencySamples() so host PDC matches
    // even when Spectral is off (avoids undelayed dry under reported latency).
    if (activeBandCount <= 0)
    {
        if (stftWasRunning)
        {
            std::fill (outFifoL.begin(), outFifoL.end(), 0.0f);
            std::fill (outFifoR.begin(), outFifoR.end(), 0.0f);
            hopCount = 0;
            stftWasRunning = false;
        }

        for (int i = 0; i < n; ++i)
        {
            const float inL = wetL[i];
            const float inR = wetR[i];
            wetL[i] = outFifoL[(size_t) pos];
            wetR[i] = outFifoR[(size_t) pos];
            outFifoL[(size_t) pos] = inL;
            outFifoR[(size_t) pos] = inR;
            pos += 1;
            if (pos >= kFftSize)
                pos = 0;
        }
        return;
    }

    stftWasRunning = true;

    for (int i = 0; i < n; ++i)
    {
        const float dL = detectL != nullptr ? detectL[i] : wetL[i];
        const float dR = detectR != nullptr ? detectR[i] : (ch > 1 ? wetR[i] : dL);
        const float mid = 0.5f * (dL + dR);

        const float sL = scDetectL != nullptr ? scDetectL[i] : 0.0f;
        const float sR = scDetectR != nullptr ? scDetectR[i] : sL;
        const float scMid = 0.5f * (sL + sR);

        processSample (wetL[i], wetR[i], mid, scMid);
    }
}

void SpectralFftEngine::processSample (float& wetL, float& wetR,
                                       float detectMid, float scMid) noexcept
{
    inFifoL[(size_t) pos] = wetL;
    inFifoR[(size_t) pos] = wetR;
    detFifo[(size_t) pos] = detectMid;
    scFifo[(size_t) pos] = scMid;

    float outL = outFifoL[(size_t) pos];
    float outR = outFifoR[(size_t) pos];
    outFifoL[(size_t) pos] = 0.0f;
    outFifoR[(size_t) pos] = 0.0f;

    pos += 1;
    if (pos >= kFftSize)
        pos = 0;

    hopCount += 1;
    if (hopCount >= kHopSize)
    {
        hopCount = 0;
        processFrame();
    }

    wetL = outL;
    wetR = outR;
}

void SpectralFftEngine::processFrame() noexcept
{
    float* work = fftWork.data();
    copyFifoToLinear (detFifo.data(), pos, work, kFftSize);

    bool anySc = false;
    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
        if (bands[(size_t) s].active && bands[(size_t) s].settings.detectFromSidechain)
            anySc = true;

    std::array<float, kNumBins> detMag {};
    std::array<float, kNumBins> scMag {};

    for (int i = 0; i < kFftSize; ++i)
        work[i] *= window[(size_t) i];
    fft.performRealOnlyForwardTransform (work, true);
    {
        auto* cdata = reinterpret_cast<std::complex<float>*> (work);
        for (int k = 0; k < kNumBins; ++k)
            detMag[(size_t) k] = std::abs (cdata[k]);
    }

    if (anySc)
    {
        copyFifoToLinear (scFifo.data(), pos, work, kFftSize);
        for (int i = 0; i < kFftSize; ++i)
            work[i] *= window[(size_t) i];
        fft.performRealOnlyForwardTransform (work, true);
        auto* cdata = reinterpret_cast<std::complex<float>*> (work);
        for (int k = 0; k < kNumBins; ++k)
            scMag[(size_t) k] = std::abs (cdata[k]);
    }

    const int hopSamples = kHopSize;
    // Stack GR in linear domain: H = 1 + Σ (g_i - 1) * lobe_i  (matches UI lobe sum).
    std::array<float, kNumBins> hLin {};
    hLin.fill (1.0f);

    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        auto& nSlices = sliceCount[(size_t) s];
        auto& centres = sliceCenterHz[(size_t) s];
        auto& grs = sliceGrDb[(size_t) s];
        auto& smooth = grDbSmoothSlice[(size_t) s];

        if (! bands[(size_t) s].active)
        {
            nSlices = 0;
            for (int i = 0; i < kMaxSlices; ++i)
                smooth[(size_t) i] *= 0.5f;
            continue;
        }

        const auto& settings = bands[(size_t) s].settings;
        const float* magSrc = (settings.detectFromSidechain && anySc) ? scMag.data() : detMag.data();

        const float atkMs = DynamicEq::clampAttackMs (settings.attackMs);
        const float relMs = DynamicEq::clampReleaseMs (settings.releaseMs);
        const float atk = DynamicEq::coeffForTimeMs (atkMs, sampleRate, hopSamples);
        const float rel = DynamicEq::coeffForTimeMs (relMs, sampleRate, hopSamples);

        auto& slotEnv = envDbSlot[(size_t) s];

        // Full-bin detect envelope (A/R).
        for (int k = 1; k < kNumBins - 1; ++k)
        {
            const float mag = magSrc[k] * (2.0f / (float) kFftSize);
            const float levelDb = juce::Decibels::gainToDecibels (mag, DynamicEq::kSilenceFloorDb);
            float& e = slotEnv[(size_t) k];
            const float coeff = levelDb > e ? atk : rel;
            e = coeff * e + (1.0f - coeff) * levelDb;
        }

        // Res → how many GR centres inside the Q footprint (graph density).
        const int n = sliceCountForRes (settings.bandwidthHz);
        nSlices = n;

        float fLo = 20.0f, fHi = safeMaxCenterHz();
        computeInfluenceRange (settings, fLo, fHi);
        const float logLo = std::log (fLo);
        const float logHi = std::log (fHi);
        const float logSpan = juce::jmax (1.0e-4f, logHi - logLo);
        // Neighbour gap = one lattice step (prominence scale tracks Res).
        const float dLog = logSpan / (float) juce::jmax (1, n);
        const float halfLog = juce::jmax (1.0e-4f, 0.5f * dLog);
        const float invHalfLog = 1.0f / halfLog;
        const float expStep = std::exp (dLog);

        const float signedMax = (settings.expand ? 1.0f : -1.0f)
                              * settings.amount * SpectralDynamics::kMaxCutDb;

        for (int i = 0; i < n; ++i)
        {
            // Log lattice inside Q; Pack warps placement (same as lattice).
            const float u = ((float) i + 0.5f) / (float) n;
            const float t = SpectralBinning::warpLatticeT (u, settings.pack);
            const float f = std::exp (logLo + t * logSpan);
            centres[(size_t) i] = f;

            float engage = 0.0f;
            const float env = sampleEnvAtHz (slotEnv.data(), kNumBins, f, sampleRate, kFftSize);
            if (env >= SpectralDynamics::kDetectFloorDb)
            {
                const float left = sampleEnvAtHz (slotEnv.data(), kNumBins, f / expStep,
                                                  sampleRate, kFftSize);
                const float right = sampleEnvAtHz (slotEnv.data(), kNumBins, f * expStep,
                                                   sampleRate, kFftSize);
                const float baseline = 0.5f * (left + right);
                const float prominence = env - baseline;
                engage = juce::jlimit (
                    0.0f, 1.0f, prominence / SpectralDynamics::kResonanceFullRangeDb);
            }

            const float mask = bandMask (f, settings);
            const float targetGr = signedMax * engage * mask;

            float& sm = smooth[(size_t) i];
            const bool engaging = std::abs (targetGr) > std::abs (sm);
            const float grCoeff = engaging ? atk : rel;
            sm = grCoeff * sm + (1.0f - grCoeff) * targetGr;
            grs[(size_t) i] = sm;

            // Paint Hann lobe into bin response (audio = graph reconstruction).
            if (std::abs (sm) < 0.01f)
                continue;

            const float gLinMinus1 = juce::Decibels::decibelsToGain (sm) - 1.0f;
            const float logC = std::log (juce::jmax (1.0f, f));
            // Bin range covered by this lobe.
            const float fMinL = juce::jmax (1.0f, f * std::exp (-halfLog));
            const float fMaxL = f * std::exp (+halfLog);
            const int k0 = juce::jlimit (1, kNumBins - 2,
                                         (int) std::floor (fMinL * (float) kFftSize / (float) sampleRate));
            const int k1 = juce::jlimit (1, kNumBins - 2,
                                         (int) std::ceil (fMaxL * (float) kFftSize / (float) sampleRate));

            for (int k = k0; k <= k1; ++k)
            {
                const float logF = std::log (juce::jmax (1.0f, binFrequencyHz (k)));
                const float x = (logF - logC) * invHalfLog;
                if (x < -1.0f || x > 1.0f)
                    continue;
                const float w = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * x));
                hLin[(size_t) k] += gLinMinus1 * w * w;
            }
        }

        // Clear unused smooth slots when Res drops.
        for (int i = n; i < kMaxSlices; ++i)
            smooth[(size_t) i] = 0.0f;
    }

    for (int k = 0; k < kNumBins; ++k)
    {
        const float h = juce::jmax (1.0e-6f, hLin[(size_t) k]);
        grDbBin[(size_t) k] = juce::jlimit (SpectralDynamics::kGrDbFloor,
                                            SpectralDynamics::kGrDbCeiling,
                                            juce::Decibels::gainToDecibels (h, -100.0f));
        grLinBin[(size_t) k] = juce::Decibels::decibelsToGain (grDbBin[(size_t) k]);
    }
    grLinBin[0] = 1.0f;
    grLinBin[(size_t) (kNumBins - 1)] = 1.0f;
    grDbBin[0] = 0.0f;
    grDbBin[(size_t) (kNumBins - 1)] = 0.0f;

    publishGrCurves();

    auto processChannel = [this] (float* inFifo, float* outFifo, float* workBuf)
    {
        copyFifoToLinear (inFifo, pos, workBuf, kFftSize);
        for (int i = 0; i < kFftSize; ++i)
            workBuf[i] *= window[(size_t) i];

        fft.performRealOnlyForwardTransform (workBuf, true);
        {
            auto* cdata = reinterpret_cast<std::complex<float>*> (workBuf);
            for (int k = 0; k < kNumBins; ++k)
            {
                const float mag = std::abs (cdata[k]);
                const float phase = std::arg (cdata[k]);
                cdata[k] = std::polar (mag * grLinBin[(size_t) k], phase);
            }
        }
        fft.performRealOnlyInverseTransform (workBuf);

        for (int i = 0; i < kFftSize; ++i)
            workBuf[i] *= window[(size_t) i] * windowCorrection;

        for (int i = 0; i < pos; ++i)
            outFifo[i] += workBuf[i + kFftSize - pos];
        for (int i = 0; i < kFftSize - pos; ++i)
            outFifo[i + pos] += workBuf[i];
    };

    processChannel (inFifoL.data(), outFifoL.data(), fftWork.data());
    processChannel (inFifoR.data(), outFifoR.data(), fftWorkR.data());
}

void SpectralFftEngine::publishGrCurves() noexcept
{
    int armed = 0;
    int pts = 0;

    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        const int writeIdx = 1 - publishedIndex[(size_t) s].load (std::memory_order_relaxed);
        auto& dest = publishedGr[(size_t) s][(size_t) writeIdx];
        dest.count = 0;
        float peak = 0.0f;

        if (bands[(size_t) s].active)
        {
            ++armed;
            const int n = juce::jlimit (0, kMaxSlices, sliceCount[(size_t) s]);
            for (int i = 0; i < n; ++i)
            {
                dest.centerHz[(size_t) i] = sliceCenterHz[(size_t) s][(size_t) i];
                dest.grDb[(size_t) i] = sliceGrDb[(size_t) s][(size_t) i];
                if (std::abs (dest.grDb[(size_t) i]) > std::abs (peak))
                    peak = dest.grDb[(size_t) i];
            }
            dest.count = n;
            pts = juce::jmax (pts, n);
        }

        publishedIndex[(size_t) s].store (writeIdx, std::memory_order_release);
        publishedPeakGrDb[(size_t) s].store (peak, std::memory_order_relaxed);
    }

    publishedArmed.store (armed, std::memory_order_relaxed);
    publishedBins.store (pts, std::memory_order_relaxed);
}

void SpectralFftEngine::samplePublishedGrDb (int bandIndex, const float* frequenciesHz,
                                             float* destDb, int numPoints) const
{
    if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
        return;

    const int slot = SpectralDynamics::slotForBandIndex (bandIndex);
    if (slot < 0)
    {
        juce::FloatVectorOperations::clear (destDb, numPoints);
        return;
    }

    const int idx = publishedIndex[(size_t) slot].load (std::memory_order_acquire);
    const auto& src = publishedGr[(size_t) slot][(size_t) juce::jlimit (0, 1, idx)];
    if (src.count <= 0)
    {
        juce::FloatVectorOperations::clear (destDb, numPoints);
        return;
    }

    const float nyquist = (float) sampleRate * 0.5f;
    const int n = src.count;

    std::array<float, kMaxPublishPoints> logC {};
    std::array<float, kMaxPublishPoints> invHalfLog {};
    std::array<float, kMaxPublishPoints> gLinMinus1 {};

    for (int k = 0; k < n; ++k)
    {
        const float c = juce::jmax (1.0f, src.centerHz[(size_t) k]);
        logC[(size_t) k] = std::log (c);
        float halfLog = 0.15f;
        if (n >= 2)
        {
            if (k == 0)
                halfLog = juce::jmax (1.0e-4f, std::log (juce::jmax (c * 1.0001f, src.centerHz[1]) / c));
            else if (k == n - 1)
                halfLog = juce::jmax (1.0e-4f, std::log (c / juce::jmax (1.0f, src.centerHz[(size_t) (k - 1)])));
            else
            {
                const float c0 = juce::jmax (1.0f, src.centerHz[(size_t) (k - 1)]);
                const float c1 = juce::jmax (c * 1.0001f, src.centerHz[(size_t) (k + 1)]);
                halfLog = juce::jmax (1.0e-4f, 0.5f * std::log (c1 / c0));
            }
        }
        invHalfLog[(size_t) k] = 1.0f / halfLog;
        gLinMinus1[(size_t) k] = juce::Decibels::decibelsToGain (src.grDb[(size_t) k]) - 1.0f;
    }

    int kFirst = 0;
    for (int i = 0; i < numPoints; ++i)
    {
        const float f = juce::jlimit (1.0f, nyquist, frequenciesHz[i]);
        const float logF = std::log (f);

        while (kFirst < n && (logF - logC[(size_t) kFirst]) * invHalfLog[(size_t) kFirst] > 1.0f)
            ++kFirst;

        float h = 1.0f;
        for (int k = kFirst; k < n; ++k)
        {
            const float x = (logF - logC[(size_t) k]) * invHalfLog[(size_t) k];
            if (x < -1.0f)
                break;
            if (x > 1.0f)
                continue;
            const float w = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * x));
            h += gLinMinus1[(size_t) k] * w * w;
        }
        destDb[i] = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, h), -100.0f);
    }
}

bool SpectralFftEngine::hasActiveGr (int bandIndex) const noexcept
{
    const int slot = SpectralDynamics::slotForBandIndex (bandIndex);
    if (slot < 0)
        return false;
    return std::abs (publishedPeakGrDb[(size_t) slot].load (std::memory_order_relaxed)) > 0.05f;
}

float SpectralFftEngine::getPublishedPeakGrDb (int bandIndex) const noexcept
{
    const int slot = SpectralDynamics::slotForBandIndex (bandIndex);
    if (slot < 0)
        return 0.0f;
    return publishedPeakGrDb[(size_t) slot].load (std::memory_order_relaxed);
}

SpectralFftEngine::RuntimeStats SpectralFftEngine::getRuntimeStats() const noexcept
{
    RuntimeStats s;
    s.armedSlots = publishedArmed.load (std::memory_order_relaxed);
    s.bankedBandpasses = publishedBins.load (std::memory_order_relaxed);
    s.gatedBandpasses = s.bankedBandpasses;
    s.processingBandpasses = s.bankedBandpasses;
    return s;
}
