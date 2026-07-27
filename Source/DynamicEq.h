#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

/**
    Per-band dynamic EQ (FabFilter Pro-Q style), v1.

    Detection model
    --------------
    - Sidechain is the **pre-EQ main input** (dry buffer before any band processing).
      Chosen for stability and to avoid serial band interaction in the detector.
    - Detector filter: mono bandpass at the band's frequency / Q (band-local energy).
    - Level: RMS of the bandpass output → dB, then a one-pole envelope follower.
    - Attack / release are per-band (defaults 20 ms / 200 ms).

    Gain law
    --------
    - The band's gain knob is the **maximum** dynamic gain (target).
    - Below threshold: dynamic contribution ≈ 0 dB (band is acoustically flat).
    - Above threshold: effectiveGainDb = targetGainDb * amount, where
        amount = saturate ((envelopeDb - thresholdDb) / kFullRangeDb)
      with kFullRangeDb = 24 dB for full engagement.
    - When dynamic is off, effectiveGainDb = targetGainDb (unchanged static EQ).

    Proportional Q uses effectiveGainDb when dynamic is on.
*/
namespace DynamicEq
{
    /** Envelope defaults (also used when params are missing). */
    constexpr float attackMs = 20.0f;
    constexpr float releaseMs = 200.0f;
    constexpr float kMinAttackMs = 0.5f;
    constexpr float kMaxAttackMs = 100.0f;
    constexpr float kMinReleaseMs = 10.0f;
    constexpr float kMaxReleaseMs = 1000.0f;

    /** dB above threshold for amount → 1.0 */
    constexpr float kFullRangeDb = 24.0f;
    /** Below DynThreshold min (-120) so silence never falsely engages dynamics. */
    constexpr float kSilenceFloorDb = -140.0f;

    /** Skewed toward faster times (more resolution in the musically useful range). */
    inline juce::NormalisableRange<float> attackMsRange()
    {
        return { kMinAttackMs, kMaxAttackMs, 0.1f, 0.4f };
    }

    inline juce::NormalisableRange<float> releaseMsRange()
    {
        return { kMinReleaseMs, kMaxReleaseMs, 1.0f, 0.4f };
    }

    inline float clampAttackMs (float ms) noexcept
    {
        return juce::jlimit (kMinAttackMs, kMaxAttackMs, ms);
    }

    inline float clampReleaseMs (float ms) noexcept
    {
        return juce::jlimit (kMinReleaseMs, kMaxReleaseMs, ms);
    }

    inline float coeffForTimeMs (float timeMs, double sampleRate, int blockSize)
    {
        const float timeSec = juce::jmax (0.001f, timeMs * 0.001f);
        const float n = (float) juce::jmax (1, blockSize);
        return std::exp (-n / (timeSec * (float) sampleRate));
    }

    inline float computeAmount (float envelopeDb, float thresholdDb)
    {
        return juce::jlimit (0.0f, 1.0f, (envelopeDb - thresholdDb) / kFullRangeDb);
    }

    inline float effectiveGainDb (bool dynamicOn, float targetGainDb, float amount)
    {
        if (! dynamicOn)
            return targetGainDb;

        return targetGainDb * amount;
    }

    inline juce::String dynamicParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1Dynamic";
            case 1: return "band2Dynamic";
            case 2: return "band3Dynamic";
            case 3: return "band4Dynamic";
            case 4: return "highpassDynamic";
            case 5: return "lowpassDynamic";
            case 6: return "highShelfDynamic";
            case 7: return "lowShelfDynamic";
            default: return {};
        }
    }

    inline juce::String thresholdParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1DynThreshold";
            case 1: return "band2DynThreshold";
            case 2: return "band3DynThreshold";
            case 3: return "band4DynThreshold";
            case 4: return "highpassDynThreshold";
            case 5: return "lowpassDynThreshold";
            case 6: return "highShelfDynThreshold";
            case 7: return "lowShelfDynThreshold";
            default: return {};
        }
    }

    inline juce::String attackMsParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1AttackMs";
            case 1: return "band2AttackMs";
            case 2: return "band3AttackMs";
            case 3: return "band4AttackMs";
            case 4: return "highpassAttackMs";
            case 5: return "lowpassAttackMs";
            case 6: return "highShelfAttackMs";
            case 7: return "lowShelfAttackMs";
            default: return {};
        }
    }

    inline juce::String releaseMsParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1ReleaseMs";
            case 1: return "band2ReleaseMs";
            case 2: return "band3ReleaseMs";
            case 3: return "band4ReleaseMs";
            case 4: return "highpassReleaseMs";
            case 5: return "lowpassReleaseMs";
            case 6: return "highShelfReleaseMs";
            case 7: return "lowShelfReleaseMs";
            default: return {};
        }
    }

    /** All eight Band 1–8 slots (D only engages when filter type uses gain). */
    inline bool supportsDynamic (int bandIndex)
    {
        return bandIndex >= 0 && bandIndex <= 7;
    }

    struct BandState
    {
        juce::dsp::IIR::Filter<float> detector;
        float envelopeDb = kSilenceFloorDb;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;
        float lastDetectorFreq = -1.0f;
        float lastDetectorQ = -1.0f;
        std::atomic<float> publishedEffectiveGainDb { 0.0f };

        void prepare (const juce::dsp::ProcessSpec& spec, int blockSize)
        {
            detector.prepare (spec);
            detector.reset();
            updateEnvelopeCoeffs (attackMs, releaseMs, spec.sampleRate, blockSize);
            envelopeDb = kSilenceFloorDb;
            lastDetectorFreq = -1.0f;
            lastDetectorQ = -1.0f;
            publishedEffectiveGainDb.store (0.0f);
        }

        void reset()
        {
            detector.reset();
            envelopeDb = kSilenceFloorDb;
        }

        void updateEnvelopeCoeffs (float attackTimeMs, float releaseTimeMs,
                                   double sampleRate, int blockSize)
        {
            attackCoeff = coeffForTimeMs (clampAttackMs (attackTimeMs), sampleRate, blockSize);
            releaseCoeff = coeffForTimeMs (clampReleaseMs (releaseTimeMs), sampleRate, blockSize);
        }

        void updateDetectorCoeffs (double sampleRate, float frequency, float q)
        {
            const float f = juce::jlimit (20.0f, 20000.0f, frequency);
            const float safeQ = juce::jmax (0.05f, q);

            constexpr float eps = 1.0e-4f;
            if (std::abs (lastDetectorFreq - f) < eps && std::abs (lastDetectorQ - safeQ) < eps)
                return;

            *detector.coefficients = *juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, f, safeQ);
            lastDetectorFreq = f;
            lastDetectorQ = safeQ;
        }

        /** Bandpass-filter pre-EQ mono, update envelope; returns amount 0..1. */
        float detectAmount (const float* left, const float* right, int numSamples,
                            double sampleRate, float frequency, float q, float thresholdDb)
        {
            if (left == nullptr || numSamples <= 0)
                return 0.0f;

            updateDetectorCoeffs (sampleRate, frequency, q);

            double sumSq = 0.0;
            for (int i = 0; i < numSamples; ++i)
            {
                const float mono = right != nullptr ? 0.5f * (left[i] + right[i]) : left[i];
                const float filtered = detector.processSample (mono);
                sumSq += (double) filtered * (double) filtered;
            }

            const float rms = (float) std::sqrt (sumSq / (double) numSamples);
            const float levelDb = juce::Decibels::gainToDecibels (rms, kSilenceFloorDb);

            const float coeff = levelDb > envelopeDb ? attackCoeff : releaseCoeff;
            envelopeDb = coeff * envelopeDb + (1.0f - coeff) * levelDb;

            return computeAmount (envelopeDb, thresholdDb);
        }

        void publishEffectiveGain (float gainDb)
        {
            publishedEffectiveGainDb.store (gainDb, std::memory_order_relaxed);
        }

        float getPublishedEffectiveGain() const
        {
            return publishedEffectiveGainDb.load (std::memory_order_relaxed);
        }
    };
}
