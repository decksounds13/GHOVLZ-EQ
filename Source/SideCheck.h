#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>
#include "DynamicEq.h"
#include "FilterSlope.h"
#include "Spectral/OverlapAddStft.h"
#include <complex>

/**
    Side Check (S<=M) — global post-EQ Mid/Side balance policeman.

    HQ (default, sideCheckHq): OWN bandpass lattice (complete duplicate of Spectral's
    log-lattice idea, 20 Hz…maxHz). Never touches SpectralDynamicsProcessor state.

    Per HQ slice:
      - Encode L/R → M/S (0.5*(L±R))
      - Measure Mid vs Side through that BP (block RMS → dB → A/R)
      - If Side > Mid: grDb = −excess * amount (clamped)
      - Reconstruct Side: side + Σ bp_i(sideIn) * (grLin_i − 1)
      - Decode M/S → L/R

    HQ off (eco): three bands — low shelf region below ~500 Hz (LP), wide bell
    between ~500–3 kHz (BP), high shelf region above ~3 kHz (HP). Same Mid/Side
    detect + parallel Side reconstruct as HQ (`side += filt(sideIn)*(gr−1)`),
    so lows duck from 500 downward and highs from 3 kHz upward.

    UI: publishes per-slice/band audible GR (double-buffer) for the sum curve.
    Display order (FrequencyResponseComponent): static EQ → spectral GR → SC GR.
    HP/LP (sideCheckHpHz / sideCheckLpHz): only bands whose centre is in [HP, LP]
    receive GR; outside = no duck (sum curve follows published GR).
    Speed (sideCheckMode): Fast / Med / Slow envelope ballistics (no exposed A/R knobs).

    Hard-bypassed when off and settled. DSP order: EQ → Spectral → Side Check → Out.
*/
namespace SideCheck
{
    inline constexpr const char* enabledParamId() noexcept { return "sideCheck"; }
    inline constexpr const char* amountParamId() noexcept { return "sideCheckAmount"; }
    inline constexpr const char* hpHzParamId() noexcept { return "sideCheckHpHz"; }
    inline constexpr const char* lpHzParamId() noexcept { return "sideCheckLpHz"; }
    inline constexpr const char* hpSlopeParamId() noexcept { return "sideCheckHpSlope"; }
    inline constexpr const char* lpSlopeParamId() noexcept { return "sideCheckLpSlope"; }
    inline constexpr const char* modeParamId() noexcept { return "sideCheckMode"; }
    /** HQ on = full BP lattice (default); off = 3-band shelf/bell eco path. */
    inline constexpr const char* hqParamId() noexcept { return "sideCheckHq"; }
    inline constexpr const char* methodParamId() noexcept { return "sideCheckMethod"; }

    /** 0 = IIR bandpass array (zero latency), 1 = FFT. */
    enum Method : int
    {
        lattice = 0,
        fft,
        numMethods
    };

    inline juce::StringArray getMethodChoiceNames()
    {
        return { "Bandpass array", "FFT" };
    }

    /** Envelope ballistics — Fast catches spikes; Med levels; Slow is gentlest. */
    enum SpeedMode : int
    {
        fast = 0,
        med = 1,
        slow = 2,
        numSpeedModes
    };

    inline juce::StringArray getModeChoiceNames()
    {
        return { "Fast", "Med", "Slow" };
    }

    inline int readModeIndex (juce::AudioProcessorValueTreeState& treeState,
                              int fallback = fast) noexcept
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (modeParamId())))
            return juce::jlimit (0, numSpeedModes - 1, choice->getIndex());

        if (auto* raw = treeState.getRawParameterValue (modeParamId()))
            return juce::jlimit (0, numSpeedModes - 1, (int) std::lround (raw->load()));

        return fallback;
    }

    constexpr float kMinAmount = 0.0f;
    constexpr float kMaxAmount = 1.0f;
    /** Musical default — audible tuck without crushing width. */
    constexpr float kDefaultAmount = 0.65f;

    /** Lattice / slice geometry floor (log placement). */
    constexpr float kMinFreqHz = 20.0f;   // same floor as other freq knobs (highpassCutoff, bands)
    constexpr float kMaxFreqHz = 20000.0f;
    /** HP/LP control floor — 0 = effect open to the bottom. */
    constexpr float kMinHpLpHz = 0.0f;
    /** Default HP — fully open (no low cut on the Side Check effect band). */
    constexpr float kDefaultHpHz = 0.0f;
    constexpr float kDefaultLpHz = 18000.0f;
    /** Minimum UI gap so HP stays strictly below LP. */
    constexpr float kMinHpLpGapHz = 50.0f;
    constexpr float kMaxGrDb = 24.0f;
    constexpr float kSilenceFloorDb = -90.0f;
    /** Engage as soon as Side exceeds Mid by this much (dB). */
    constexpr float kExcessEpsilonDb = 0.15f;

    /** Fast: snappy tuck for harsh Side spikes (default). */
    constexpr float kFastAttackMs = 8.0f;
    constexpr float kFastReleaseMs = 80.0f;
    constexpr float kFastGrSmoothMs = 8.0f;
    /** Med: former Slow — balanced leveling between Fast and Slow. */
    constexpr float kMedAttackMs = 40.0f;
    constexpr float kMedReleaseMs = 300.0f;
    constexpr float kMedGrSmoothMs = 28.0f;
    /** Slow: gentlest leveling — less pumping on sustained width. */
    constexpr float kSlowAttackMs = 120.0f;
    constexpr float kSlowReleaseMs = 900.0f;
    constexpr float kSlowGrSmoothMs = 70.0f;

    /** Default quality — full bandpass lattice. */
    constexpr bool kDefaultHq = true;

    /** Eco (HQ off): fixed 3-band shelf / bell geometry. */
    constexpr int kNumEcoBands = 3;
    constexpr float kEcoLowShelfHz = 500.0f;
    constexpr float kEcoHighShelfHz = 3000.0f;
    /** Geometric mean of the shelf corners — sits between them. */
    constexpr float kEcoBellHz = 1224.7449f; // sqrt (500 * 3000)
    /** Wide enough that −3 dB points sit near the shelf corners. */
    constexpr float kEcoBellQ = 0.5f;

    inline void getBallisticsMs (int mode, float& attackMs, float& releaseMs, float& grSmoothMs) noexcept
    {
        if (mode == slow)
        {
            attackMs = kSlowAttackMs;
            releaseMs = kSlowReleaseMs;
            grSmoothMs = kSlowGrSmoothMs;
        }
        else if (mode == med)
        {
            attackMs = kMedAttackMs;
            releaseMs = kMedReleaseMs;
            grSmoothMs = kMedGrSmoothMs;
        }
        else
        {
            attackMs = kFastAttackMs;
            releaseMs = kFastReleaseMs;
            grSmoothMs = kFastGrSmoothMs;
        }
    }

    /** Tooltip helper — mode name plus A/R times for the Side Check speed button. */
    inline juce::String speedTooltipForMode (int mode) noexcept
    {
        float attackMs = kFastAttackMs, releaseMs = kFastReleaseMs, grSmoothMs = kFastGrSmoothMs;
        getBallisticsMs (mode, attackMs, releaseMs, grSmoothMs);
        juce::ignoreUnused (grSmoothMs);

        const auto names = getModeChoiceNames();
        const juce::String name = names[juce::jlimit (0, names.size() - 1, mode)];

        auto fmtMs = [] (float ms) -> juce::String
        {
            if (ms >= 100.0f)
                return juce::String (juce::roundToInt (ms)) + " ms";
            if (ms >= 10.0f)
                return juce::String (ms, 0) + " ms";
            return juce::String (ms, 1) + " ms";
        };

        return "Speed - Side Check ballistics (" + name + "). "
               "Attack " + fmtMs (attackMs) + ", Release " + fmtMs (releaseMs)
               + ". Click to cycle Fast / Med / Slow.";
    }
    /**
        Own slice count — independent of SpectralBinning::kMinBandpassBudget.
        Coarser than Spectral's finest lattice so SC stays cheap and the
        reconstruct sum stays closer to a partition of unity.
    */
    constexpr int kNumSlices = 24;

    class Processor
    {
    public:
        void prepare (double newSampleRate, int samplesPerBlock) noexcept
        {
            sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
            blockSize = juce::jmax (1, samplesPerBlock);

            juce::dsp::ProcessSpec monoSpec;
            monoSpec.sampleRate = sampleRate;
            monoSpec.maximumBlockSize = (juce::uint32) blockSize;
            monoSpec.numChannels = 1;

            for (auto& s : slices)
            {
                s.detectM.prepare (monoSpec);
                s.detectS.prepare (monoSpec);
                s.applyS.prepare (monoSpec);
                s.detectM.reset();
                s.detectS.reset();
                s.applyS.reset();
                s.envMDb = kSilenceFloorDb;
                s.envSDb = kSilenceFloorDb;
                s.grLin = 1.0f;
                s.grTarget = 1.0f;
                s.sumSqM = 0.0;
                s.sumSqS = 0.0;
            }

            for (auto& b : ecoBands)
            {
                b.detectM.prepare (monoSpec);
                b.detectS.prepare (monoSpec);
                b.applyS.prepare (monoSpec);
                b.detectM.reset();
                b.detectS.reset();
                b.applyS.reset();
                b.envMDb = kSilenceFloorDb;
                b.envSDb = kSilenceFloorDb;
                b.grLin = 1.0f;
                b.grTarget = 1.0f;
                b.sumSqM = 0.0;
                b.sumSqS = 0.0;
            }

            rebuildLattice();
            rebuildEco();
            stft.prepare (sampleRate);
            fftEnvM.fill (kSilenceFloorDb);
            fftEnvS.fill (kSilenceFloorDb);
            fftGrDb.fill (0.0f);
            clearPublished();
            wasEnabled = false;
            settling = false;
            lastHq = kDefaultHq;
            useFft = false;
            prepared = true;
        }

        void reset() noexcept
        {
            resetHqState();
            resetEcoState();
            stft.reset();
            fftEnvM.fill (kSilenceFloorDb);
            fftEnvS.fill (kSilenceFloorDb);
            fftGrDb.fill (0.0f);
            clearPublished();
            wasEnabled = false;
            settling = false;
        }

        /**
            Process stereo buffer in-place.
            No-op when disabled and GR has settled (zero filter cost).
            Mono buffers are a no-op (needs L/R for M/S).
            @param amount 0…1 scales GR strength (APVTS sideCheckAmount).
            @param hpHz / lpHz  effect band — only slices/bands with centre in [min,max] get GR.
            @param speedMode  SideCheck::fast / med / slow (APVTS sideCheckMode).
            @param hq  true = BP lattice (default); false = 3-band shelf/bell eco path.
        */
        int getLatencySamples() const noexcept { return useFft ? stft.latencySamples() : 0; }

        void process (juce::AudioBuffer<float>& buffer, bool enabled, float amount,
                      float hpHz, float lpHz, int speedMode = fast, bool hq = kDefaultHq,
                      int hpSlope = FilterSlope::db12, int lpSlope = FilterSlope::db12,
                      int method = lattice) noexcept
        {
            const int numCh = buffer.getNumChannels();
            const int n = buffer.getNumSamples();
            if (numCh < 2 || n <= 0)
            {
                clearPublished();
                return;
            }

            // Recover if prepare was skipped / lattice failed.
            if (! prepared || activeSlices <= 0 || ! ecoReady)
            {
                prepare (sampleRate > 0.0 ? sampleRate : 48000.0, juce::jmax (blockSize, n));
                if (activeSlices <= 0 || ! ecoReady)
                {
                    clearPublished();
                    return;
                }
            }

            const bool wantFft = (method == Method::fft);
            if (wantFft != useFft)
            {
                useFft = wantFft;
                if (useFft)
                {
                    stft.prepare (sampleRate);
                    stft.reset();
                    fftEnvM.fill (kSilenceFloorDb);
                    fftEnvS.fill (kSilenceFloorDb);
                    fftGrDb.fill (0.0f);
                }
            }

            if (useFft)
            {
                processFft (buffer, enabled, amount, hpHz, lpHz, speedMode, hpSlope, lpSlope);
                return;
            }

            if (hq != lastHq)
            {
                resetHqState();
                resetEcoState();
                lastHq = hq;
            }

            // Hard bypass when off and already transparent.
            if (! enabled && ! wasEnabled && ! settling)
            {
                clearPublished();
                return;
            }

            if (enabled && ! wasEnabled)
            {
                if (hq)
                    resetHqState();
                else
                    resetEcoState();
            }

            wasEnabled = enabled;
            settling = ! enabled; // fade out while off until GR ≈ 0

            if (hq)
                processHq (buffer, enabled, amount, hpHz, lpHz, speedMode, hpSlope, lpSlope);
            else
                processEco (buffer, enabled, amount, hpHz, lpHz, speedMode, hpSlope, lpSlope);
        }

        /** Most-negative slice GR target this block (dB; 0 when idle). */
        float getPublishedGrDb() const noexcept { return publishedGrDb.load (std::memory_order_relaxed); }

        int getActiveSliceCount() const noexcept
        {
            return lastHq ? activeSlices : (ecoReady ? kNumEcoBands : 0);
        }

        /**
            Sample published Side Check GR onto a dense frequency grid (typically 1/px log-f).
            Mirrors SpectralDynamicsProcessor::samplePublishedGrDb — Hann lobes in log-f.
            Zeros dest when idle / empty. UI adds this to the sum curve only (not per-band).
        */
        void samplePublishedGrDb (const float* frequenciesHz, float* destDb, int numPoints) const noexcept
        {
            if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
                return;

            const int idx = publishedIndex.load (std::memory_order_acquire);
            const auto& src = publishedCurves[(size_t) juce::jlimit (0, 1, idx)];
            if (src.count <= 0)
            {
                juce::FloatVectorOperations::clear (destDb, numPoints);
                return;
            }

            // Eco: evaluate parallel LP/BP/HP reconstruct across the full spectrum
            // (Hann lobes at 500/1.2k/3k wrongly confined the curve to the mid gap).
            if (src.eco)
            {
                samplePublishedGrDbEco (src, frequenciesHz, destDb, numPoints);
                return;
            }

            const float nyquist = (float) sampleRate * 0.5f;
            const int n = src.count;

            std::array<float, kNumSlices> logC {};
            std::array<float, kNumSlices> invHalfLog {};
            std::array<float, kNumSlices> gLinMinus1 {};

            for (int k = 0; k < n; ++k)
            {
                const float c = juce::jmax (1.0f, src.centerHz[(size_t) k]);
                logC[(size_t) k] = std::log (c);

                float halfLog = 0.15f;
                if (n >= 2)
                {
                    if (k == 0)
                    {
                        const float c1 = juce::jmax (c * 1.0001f, src.centerHz[1]);
                        halfLog = juce::jmax (1.0e-4f, std::log (c1 / c));
                    }
                    else if (k == n - 1)
                    {
                        const float c0 = juce::jmax (1.0f, src.centerHz[(size_t) (k - 1)]);
                        halfLog = juce::jmax (1.0e-4f, std::log (c / c0));
                    }
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

                while (kFirst < n
                       && (logF - logC[(size_t) kFirst]) * invHalfLog[(size_t) kFirst] > 1.0f)
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
                    h += w * gLinMinus1[(size_t) k];
                }

                destDb[i] = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, h), -100.0f);
            }
        }

    private:
        enum class EcoKind : int
        {
            lowShelf = 0,
            bell = 1,
            highShelf = 2
        };

        struct Slice
        {
            juce::dsp::IIR::Filter<float> detectM;
            juce::dsp::IIR::Filter<float> detectS;
            juce::dsp::IIR::Filter<float> applyS;
            float centerHz = 1000.0f;
            float q = 1.0f;
            float envMDb = kSilenceFloorDb;
            float envSDb = kSilenceFloorDb;
            float grLin = 1.0f;
            float grTarget = 1.0f;
            double sumSqM = 0.0;
            double sumSqS = 0.0;
        };

        struct EcoBand
        {
            juce::dsp::IIR::Filter<float> detectM;
            juce::dsp::IIR::Filter<float> detectS;
            juce::dsp::IIR::Filter<float> applyS;
            EcoKind kind = EcoKind::bell;
            float centerHz = 1000.0f;
            float envMDb = kSilenceFloorDb;
            float envSDb = kSilenceFloorDb;
            float grLin = 1.0f;
            float grTarget = 1.0f;
            double sumSqM = 0.0;
            double sumSqS = 0.0;
        };

        struct PublishedCurve
        {
            std::array<float, kNumSlices> centerHz {};
            std::array<float, kNumSlices> grDb {};
            int count = 0;
            bool eco = false;
        };

        /** True when this eco band's region intersects the HP/LP effect window. */
        static bool ecoBandIntersectsEffectRange (EcoKind kind, float bandLo, float bandHi) noexcept
        {
            switch (kind)
            {
                case EcoKind::lowShelf:
                    // Acts from the bottom up to the shelf corner.
                    return bandLo <= kEcoLowShelfHz;
                case EcoKind::highShelf:
                    // Acts from the shelf corner up to the top.
                    return bandHi >= kEcoHighShelfHz;
                case EcoKind::bell:
                default:
                    return bandHi > kEcoLowShelfHz && bandLo < kEcoHighShelfHz;
            }
        }

        void samplePublishedGrDbEco (const PublishedCurve& src,
                                     const float* frequenciesHz,
                                     float* destDb,
                                     int numPoints) const noexcept
        {
            const float nyquist = (float) sampleRate * 0.5f;
            if (src.count < kNumEcoBands || ! (sampleRate > 0.0))
            {
                juce::FloatVectorOperations::clear (destDb, numPoints);
                return;
            }

            auto lp = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, kEcoLowShelfHz);
            auto bp = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, kEcoBellHz, kEcoBellQ);
            auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, kEcoHighShelfHz);
            if (lp == nullptr || bp == nullptr || hp == nullptr)
            {
                juce::FloatVectorOperations::clear (destDb, numPoints);
                return;
            }

            const float g0 = juce::Decibels::decibelsToGain (src.grDb[0]);
            const float g1 = juce::Decibels::decibelsToGain (src.grDb[1]);
            const float g2 = juce::Decibels::decibelsToGain (src.grDb[2]);

            for (int i = 0; i < numPoints; ++i)
            {
                const float f = juce::jlimit (1.0f, nyquist, frequenciesHz[i]);
                const float m0 = (float) lp->getMagnitudeForFrequency ((double) f, sampleRate);
                const float m1 = (float) bp->getMagnitudeForFrequency ((double) f, sampleRate);
                const float m2 = (float) hp->getMagnitudeForFrequency ((double) f, sampleRate);
                // Parallel reconstruct: side += Σ filt_i (sideIn) * (gr_i − 1)
                const float h = 1.0f + (g0 - 1.0f) * m0 + (g1 - 1.0f) * m1 + (g2 - 1.0f) * m2;
                destDb[i] = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, h), -100.0f);
            }
        }

        void resetHqState() noexcept
        {
            for (int i = 0; i < activeSlices; ++i)
            {
                auto& s = slices[(size_t) i];
                s.detectM.reset();
                s.detectS.reset();
                s.applyS.reset();
                s.envMDb = kSilenceFloorDb;
                s.envSDb = kSilenceFloorDb;
                s.grLin = 1.0f;
                s.grTarget = 1.0f;
                s.sumSqM = 0.0;
                s.sumSqS = 0.0;
            }
        }

        void resetEcoState() noexcept
        {
            for (auto& b : ecoBands)
            {
                b.detectM.reset();
                b.detectS.reset();
                b.applyS.reset();
                b.envMDb = kSilenceFloorDb;
                b.envSDb = kSilenceFloorDb;
                b.grLin = 1.0f;
                b.grTarget = 1.0f;
                b.sumSqM = 0.0;
                b.sumSqS = 0.0;
            }
        }

        void clearPublished() noexcept
        {
            publishedGrDb.store (0.0f, std::memory_order_relaxed);
            const int writeIdx = 1 - publishedIndex.load (std::memory_order_relaxed);
            auto& dest = publishedCurves[(size_t) writeIdx];
            dest.count = 0;
            dest.eco = false;
            publishedIndex.store (writeIdx, std::memory_order_release);
        }

        void publishGrCurveHq() noexcept
        {
            const int writeIdx = 1 - publishedIndex.load (std::memory_order_relaxed);
            auto& dest = publishedCurves[(size_t) writeIdx];
            dest.count = 0;
            dest.eco = false;

            for (int b = 0; b < activeSlices; ++b)
            {
                const auto& s = slices[(size_t) b];
                dest.centerHz[(size_t) dest.count] = s.centerHz;
                // Audible GR (smoothed grLin) so the UI matches the mix.
                dest.grDb[(size_t) dest.count] = juce::Decibels::gainToDecibels (
                    juce::jmax (1.0e-6f, s.grLin), -100.0f);
                ++dest.count;
            }

            publishedIndex.store (writeIdx, std::memory_order_release);
        }

        void publishGrCurveEco() noexcept
        {
            const int writeIdx = 1 - publishedIndex.load (std::memory_order_relaxed);
            auto& dest = publishedCurves[(size_t) writeIdx];
            dest.count = 0;
            dest.eco = true;

            for (int b = 0; b < kNumEcoBands; ++b)
            {
                const auto& band = ecoBands[(size_t) b];
                dest.centerHz[(size_t) dest.count] = band.centerHz;
                dest.grDb[(size_t) dest.count] = juce::Decibels::gainToDecibels (
                    juce::jmax (1.0e-6f, band.grLin), -100.0f);
                ++dest.count;
            }

            publishedIndex.store (writeIdx, std::memory_order_release);
        }

        void processHq (juce::AudioBuffer<float>& buffer, bool enabled, float amount,
                        float hpHz, float lpHz, int speedMode,
                        int hpSlope, int lpSlope) noexcept
        {
            const int n = buffer.getNumSamples();
            const float amountClamped = juce::jlimit (kMinAmount, kMaxAmount, amount);
            const bool useHp = hpHz > 1.0f;
            const bool useLp = lpHz > 1.0f && lpHz < (float) sampleRate * 0.49f;
            const auto hpStages = useHp
                ? FilterSlope::makeHighpassCoeffs (sampleRate, hpHz, 0.70710678f, hpSlope)
                : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};
            const auto lpStages = useLp
                ? FilterSlope::makeLowpassCoeffs (sampleRate, lpHz, 0.70710678f, lpSlope)
                : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            float attackMs = kFastAttackMs, releaseMs = kFastReleaseMs, grSmoothMs = kFastGrSmoothMs;
            getBallisticsMs (speedMode, attackMs, releaseMs, grSmoothMs);

            const float atk = DynamicEq::coeffForTimeMs (attackMs, sampleRate, n);
            const float rel = DynamicEq::coeffForTimeMs (releaseMs, sampleRate, n);
            const float grCoeff = 1.0f - std::exp (-1.0f / juce::jmax (1.0f,
                (float) sampleRate * grSmoothMs * 0.001f));

            for (int i = 0; i < activeSlices; ++i)
            {
                slices[(size_t) i].sumSqM = 0.0;
                slices[(size_t) i].sumSqS = 0.0;
            }

            for (int i = 0; i < n; ++i)
            {
                const float mid = 0.5f * (left[i] + right[i]);
                const float sideIn = 0.5f * (left[i] - right[i]);
                float side = sideIn;

                for (int b = 0; b < activeSlices; ++b)
                {
                    auto& s = slices[(size_t) b];

                    const float dm = s.detectM.processSample (mid);
                    const float ds = s.detectS.processSample (sideIn);
                    s.sumSqM += (double) dm * (double) dm;
                    s.sumSqS += (double) ds * (double) ds;

                    s.grLin += (s.grTarget - s.grLin) * grCoeff;

                    const float bp = s.applyS.processSample (sideIn);
                    side += bp * (s.grLin - 1.0f);
                }

                left[i] = mid + side;
                right[i] = mid - side;
            }

            float peakGrDb = 0.0f;
            bool anyActiveGr = false;

            for (int b = 0; b < activeSlices; ++b)
            {
                auto& s = slices[(size_t) b];
                const float rmsM = std::sqrt ((float) (s.sumSqM / (double) n));
                const float rmsS = std::sqrt ((float) (s.sumSqS / (double) n));
                const float levelM = juce::Decibels::gainToDecibels (rmsM, kSilenceFloorDb);
                const float levelS = juce::Decibels::gainToDecibels (rmsS, kSilenceFloorDb);

                const float cM = levelM > s.envMDb ? atk : rel;
                const float cS = levelS > s.envSDb ? atk : rel;
                s.envMDb = cM * s.envMDb + (1.0f - cM) * levelM;
                s.envSDb = cS * s.envSDb + (1.0f - cS) * levelS;

                float targetGrDb = 0.0f;
                float edgeW = 1.0f;
                if (useHp)
                    edgeW *= FilterSlope::cascadeMagnitudeAt (hpStages, (double) s.centerHz, sampleRate);
                if (useLp)
                    edgeW *= FilterSlope::cascadeMagnitudeAt (lpStages, (double) s.centerHz, sampleRate);
                edgeW = juce::jlimit (0.0f, 1.0f, edgeW);
                if (enabled && amountClamped > 1.0e-4f && edgeW > 0.02f)
                {
                    const float excess = s.envSDb - s.envMDb;
                    if (excess > kExcessEpsilonDb
                        && (s.envMDb > kSilenceFloorDb + 1.0f || s.envSDb > kSilenceFloorDb + 1.0f))
                    {
                        targetGrDb = -juce::jmin (kMaxGrDb, excess * amountClamped * edgeW);
                    }
                }

                s.grTarget = juce::Decibels::decibelsToGain (targetGrDb);
                peakGrDb = juce::jmin (peakGrDb, targetGrDb);

                if (std::abs (s.grLin - 1.0f) > 1.0e-4f || std::abs (s.grTarget - 1.0f) > 1.0e-4f)
                    anyActiveGr = true;
            }

            publishedGrDb.store (peakGrDb, std::memory_order_relaxed);
            publishGrCurveHq();

            if (! enabled && ! anyActiveGr)
                settling = false;
        }

        void processEco (juce::AudioBuffer<float>& buffer, bool enabled, float amount,
                         float hpHz, float lpHz, int speedMode,
                         int hpSlope, int lpSlope) noexcept
        {
            const int n = buffer.getNumSamples();
            const float amountClamped = juce::jlimit (kMinAmount, kMaxAmount, amount);
            const bool useHp = hpHz > 1.0f;
            const bool useLp = lpHz > 1.0f && lpHz < (float) sampleRate * 0.49f;
            const auto hpStages = useHp
                ? FilterSlope::makeHighpassCoeffs (sampleRate, hpHz, 0.70710678f, hpSlope)
                : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};
            const auto lpStages = useLp
                ? FilterSlope::makeLowpassCoeffs (sampleRate, lpHz, 0.70710678f, lpSlope)
                : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};

            auto* left = buffer.getWritePointer (0);
            auto* right = buffer.getWritePointer (1);

            float attackMs = kFastAttackMs, releaseMs = kFastReleaseMs, grSmoothMs = kFastGrSmoothMs;
            getBallisticsMs (speedMode, attackMs, releaseMs, grSmoothMs);

            const float atk = DynamicEq::coeffForTimeMs (attackMs, sampleRate, n);
            const float rel = DynamicEq::coeffForTimeMs (releaseMs, sampleRate, n);
            const float grCoeff = 1.0f - std::exp (-1.0f / juce::jmax (1.0f,
                (float) sampleRate * grSmoothMs * 0.001f));

            for (auto& band : ecoBands)
            {
                band.sumSqM = 0.0;
                band.sumSqS = 0.0;
            }

            for (int i = 0; i < n; ++i)
            {
                const float mid = 0.5f * (left[i] + right[i]);
                const float sideIn = 0.5f * (left[i] - right[i]);
                float side = sideIn;

                for (auto& band : ecoBands)
                {
                    const float dm = band.detectM.processSample (mid);
                    const float ds = band.detectS.processSample (sideIn);
                    band.sumSqM += (double) dm * (double) dm;
                    band.sumSqS += (double) ds * (double) ds;

                    band.grLin += (band.grTarget - band.grLin) * grCoeff;

                    // Parallel reconstruct (same model as HQ): LP ducks ≤500,
                    // BP the gap, HP ≥3 kHz — not a mid-only cascade.
                    const float filt = band.applyS.processSample (sideIn);
                    side += filt * (band.grLin - 1.0f);
                }

                left[i] = mid + side;
                right[i] = mid - side;
            }

            float peakGrDb = 0.0f;
            bool anyActiveGr = false;

            for (auto& band : ecoBands)
            {
                const float rmsM = std::sqrt ((float) (band.sumSqM / (double) n));
                const float rmsS = std::sqrt ((float) (band.sumSqS / (double) n));
                const float levelM = juce::Decibels::gainToDecibels (rmsM, kSilenceFloorDb);
                const float levelS = juce::Decibels::gainToDecibels (rmsS, kSilenceFloorDb);

                const float cM = levelM > band.envMDb ? atk : rel;
                const float cS = levelS > band.envSDb ? atk : rel;
                band.envMDb = cM * band.envMDb + (1.0f - cM) * levelM;
                band.envSDb = cS * band.envSDb + (1.0f - cS) * levelS;

                float targetGrDb = 0.0f;
                float edgeW = 1.0f;
                if (useHp)
                    edgeW *= FilterSlope::cascadeMagnitudeAt (hpStages, (double) band.centerHz, sampleRate);
                if (useLp)
                    edgeW *= FilterSlope::cascadeMagnitudeAt (lpStages, (double) band.centerHz, sampleRate);
                edgeW = juce::jlimit (0.0f, 1.0f, edgeW);
                if (enabled && amountClamped > 1.0e-4f && edgeW > 0.02f)
                {
                    const float excess = band.envSDb - band.envMDb;
                    if (excess > kExcessEpsilonDb
                        && (band.envMDb > kSilenceFloorDb + 1.0f || band.envSDb > kSilenceFloorDb + 1.0f))
                    {
                        targetGrDb = -juce::jmin (kMaxGrDb, excess * amountClamped * edgeW);
                    }
                }

                band.grTarget = juce::Decibels::decibelsToGain (targetGrDb);
                peakGrDb = juce::jmin (peakGrDb, targetGrDb);

                if (std::abs (band.grLin - 1.0f) > 1.0e-4f || std::abs (band.grTarget - 1.0f) > 1.0e-4f)
                    anyActiveGr = true;
            }

            publishedGrDb.store (peakGrDb, std::memory_order_relaxed);
            publishGrCurveEco();

            if (! enabled && ! anyActiveGr)
                settling = false;
        }

        void processFft (juce::AudioBuffer<float>& buffer, bool enabled, float amount,
                         float hpHz, float lpHz, int speedMode,
                         int hpSlope, int lpSlope) noexcept
        {
            if (! prepared)
                prepare (sampleRate > 0.0 ? sampleRate : 48000.0, juce::jmax (blockSize, buffer.getNumSamples()));

            if (! enabled && ! wasEnabled && ! settling)
            {
                clearPublished();
                return;
            }

            if (enabled && ! wasEnabled)
            {
                stft.reset();
                fftEnvM.fill (kSilenceFloorDb);
                fftEnvS.fill (kSilenceFloorDb);
                fftGrDb.fill (0.0f);
            }

            wasEnabled = enabled;
            settling = ! enabled;

            float attackMs = kFastAttackMs, releaseMs = kFastReleaseMs, grSmoothMs = kFastGrSmoothMs;
            getBallisticsMs (speedMode, attackMs, releaseMs, grSmoothMs);
            const float atk = DynamicEq::coeffForTimeMs (attackMs, sampleRate, OverlapAddStft::kHop);
            const float rel = DynamicEq::coeffForTimeMs (releaseMs, sampleRate, OverlapAddStft::kHop);
            const float grSmooth = DynamicEq::coeffForTimeMs (grSmoothMs, sampleRate, OverlapAddStft::kHop);
            const float amountClamped = juce::jlimit (kMinAmount, kMaxAmount, amount);
            const bool useHp = hpHz > 1.0f;
            const bool useLp = lpHz > 1.0f && lpHz < (float) sampleRate * 0.49f;
            const auto hpStages = useHp
                ? FilterSlope::makeHighpassCoeffs (sampleRate, hpHz, 0.70710678f, hpSlope)
                : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};
            const auto lpStages = useLp
                ? FilterSlope::makeLowpassCoeffs (sampleRate, lpHz, 0.70710678f, lpSlope)
                : juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> {};

            stft.process (buffer, buffer.getReadPointer (0),
                         buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : buffer.getReadPointer (0),
                [&] (float* workL, float* workR, const float*, int numBins)
                {
                    auto* cL = reinterpret_cast<std::complex<float>*> (workL);
                    auto* cR = reinterpret_cast<std::complex<float>*> (workR);
                    float peak = 0.0f;

                    for (int k = 1; k < numBins - 1; ++k)
                    {
                        const float f = stft.binHz (k);
                        float edgeW = 1.0f;
                        if (useHp)
                            edgeW *= FilterSlope::cascadeMagnitudeAt (hpStages, (double) f, sampleRate);
                        if (useLp)
                            edgeW *= FilterSlope::cascadeMagnitudeAt (lpStages, (double) f, sampleRate);
                        edgeW = juce::jlimit (0.0f, 1.0f, edgeW);

                        const std::complex<float> mid = 0.5f * (cL[k] + cR[k]);
                        const std::complex<float> side = 0.5f * (cL[k] - cR[k]);
                        const float magM = std::abs (mid) * (2.0f / (float) OverlapAddStft::kSize);
                        const float magS = std::abs (side) * (2.0f / (float) OverlapAddStft::kSize);
                        const float lvM = juce::Decibels::gainToDecibels (magM, kSilenceFloorDb);
                        const float lvS = juce::Decibels::gainToDecibels (magS, kSilenceFloorDb);
                        float& eM = fftEnvM[(size_t) k];
                        float& eS = fftEnvS[(size_t) k];
                        eM = (lvM > eM ? atk : rel) * eM + (1.0f - (lvM > eM ? atk : rel)) * lvM;
                        eS = (lvS > eS ? atk : rel) * eS + (1.0f - (lvS > eS ? atk : rel)) * lvS;

                        float targetGrDb = 0.0f;
                        if (enabled && amountClamped > 1.0e-4f && edgeW > 0.02f)
                        {
                            const float excess = eS - eM;
                            if (excess > kExcessEpsilonDb
                                && (eM > kSilenceFloorDb + 1.0f || eS > kSilenceFloorDb + 1.0f))
                                targetGrDb = -juce::jmin (kMaxGrDb, excess * amountClamped * edgeW);
                        }

                        float& g = fftGrDb[(size_t) k];
                        g = grSmooth * g + (1.0f - grSmooth) * targetGrDb;
                        const float gLin = juce::Decibels::decibelsToGain (g);
                        const std::complex<float> sideOut = side * gLin;
                        cL[k] = mid + sideOut;
                        cR[k] = mid - sideOut;
                        if (g < peak)
                            peak = g;
                    }

                    publishedGrDb.store (peak, std::memory_order_relaxed);

                    const int writeIdx = 1 - publishedIndex.load (std::memory_order_relaxed);
                    auto& dest = publishedCurves[(size_t) writeIdx];
                    dest.count = 0;
                    const int nPub = juce::jmax (1, activeSlices);
                    for (int i = 0; i < nPub && dest.count < kNumSlices; ++i)
                    {
                        const float f = juce::jmax (20.0f, slices[(size_t) i].centerHz);
                        const int bin = juce::jlimit (1, numBins - 2,
                                                      (int) std::lround (f * (float) OverlapAddStft::kSize
                                                                          / (float) sampleRate));
                        dest.centerHz[(size_t) dest.count] = f;
                        dest.grDb[(size_t) dest.count] = fftGrDb[(size_t) bin];
                        ++dest.count;
                    }
                    publishedIndex.store (writeIdx, std::memory_order_release);
                });

            if (! enabled)
            {
                bool any = false;
                for (float g : fftGrDb)
                    if (std::abs (g) > 0.05f) { any = true; break; }
                if (! any)
                    settling = false;
            }
        }

        void rebuildLattice() noexcept
        {
            activeSlices = 0;
            const float maxHz = juce::jmin (kMaxFreqHz, 0.45f * (float) sampleRate);
            const float placeLo = kMinFreqHz;
            const float placeHi = maxHz;
            if (! (placeHi > placeLo) || ! (sampleRate > 0.0))
                return;

            const float logLo = std::log (placeLo);
            const float logHi = std::log (placeHi);
            const float logSpan = logHi - logLo;
            const int count = kNumSlices;

            for (int k = 0; k < count; ++k)
            {
                const float t0 = (float) k / (float) count;
                const float t1 = (float) (k + 1) / (float) count;
                const float f0 = std::exp (logLo + logSpan * t0);
                const float f1 = std::exp (logLo + logSpan * t1);
                const float center = std::sqrt (f0 * f1);
                if (! (center >= kMinFreqHz && center <= maxHz)
                    || ! (center < (float) sampleRate * 0.499f))
                    continue;

                // Near-partition bandwidth (no 1.15 inflate) → cleaner reconstruct.
                const float bw = juce::jmax (center * (1.0f / 200.0f), (f1 - f0));
                const float q = juce::jlimit (0.5f, 200.0f, center / bw);
                if (! (q > 0.05f) || ! std::isfinite (center) || ! std::isfinite (q))
                    continue;

                auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, center, q);
                if (coeffs == nullptr)
                    continue;

                auto& s = slices[(size_t) activeSlices];
                // Share one Coefficients object (read-only geometry); each filter
                // keeps independent delay state.
                s.detectM.coefficients = coeffs;
                s.detectS.coefficients = coeffs;
                s.applyS.coefficients = coeffs;
                s.detectM.reset();
                s.detectS.reset();
                s.applyS.reset();
                s.centerHz = center;
                s.q = q;
                ++activeSlices;
            }
        }

        void rebuildEco() noexcept
        {
            ecoReady = false;
            if (! (sampleRate > 0.0))
                return;

            const float nyquistGuard = 0.45f * (float) sampleRate;
            if (kEcoLowShelfHz >= nyquistGuard || kEcoHighShelfHz >= nyquistGuard
                || kEcoBellHz >= nyquistGuard)
                return;

            // Detect + apply share LP / wide BP / HP geometry (HQ-style parallel reconstruct).
            // Low = shelf region below 500; bell fills 500–3k; high = shelf region above 3k.
            struct Spec
            {
                EcoKind kind;
                float centerHz;
            };

            const Spec specs[kNumEcoBands] = {
                { EcoKind::lowShelf,  kEcoLowShelfHz },
                { EcoKind::bell,      kEcoBellHz },
                { EcoKind::highShelf, kEcoHighShelfHz }
            };

            for (int i = 0; i < kNumEcoBands; ++i)
            {
                auto& band = ecoBands[(size_t) i];
                band.kind = specs[i].kind;
                band.centerHz = specs[i].centerHz;

                juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
                switch (band.kind)
                {
                    case EcoKind::lowShelf:
                        coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass (
                            sampleRate, kEcoLowShelfHz);
                        break;
                    case EcoKind::highShelf:
                        coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass (
                            sampleRate, kEcoHighShelfHz);
                        break;
                    case EcoKind::bell:
                    default:
                        coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (
                            sampleRate, kEcoBellHz, kEcoBellQ);
                        break;
                }

                if (coeffs == nullptr)
                    return;

                // Shared read-only coeffs; each filter keeps independent delay state.
                band.detectM.coefficients = coeffs;
                band.detectS.coefficients = coeffs;
                band.applyS.coefficients = coeffs;
                band.detectM.reset();
                band.detectS.reset();
                band.applyS.reset();
                band.envMDb = kSilenceFloorDb;
                band.envSDb = kSilenceFloorDb;
                band.grLin = 1.0f;
                band.grTarget = 1.0f;
                band.sumSqM = 0.0;
                band.sumSqS = 0.0;
            }

            ecoReady = true;
        }

        double sampleRate = 48000.0;
        int blockSize = 512;
        int activeSlices = 0;
        bool wasEnabled = false;
        bool settling = false;
        bool prepared = false;
        bool ecoReady = false;
        bool lastHq = kDefaultHq;
        bool useFft = false;
        OverlapAddStft stft;
        std::array<float, OverlapAddStft::kBins> fftEnvM {};
        std::array<float, OverlapAddStft::kBins> fftEnvS {};
        std::array<float, OverlapAddStft::kBins> fftGrDb {};

        std::array<Slice, kNumSlices> slices {};
        std::array<EcoBand, kNumEcoBands> ecoBands {};
        std::atomic<float> publishedGrDb { 0.0f };
        std::array<PublishedCurve, 2> publishedCurves {};
        std::atomic<int> publishedIndex { 0 };
    };
}
