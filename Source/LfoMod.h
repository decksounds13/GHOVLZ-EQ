#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>

/**
    Three LFOs + a 20-slot source→destination mod matrix.

    Modulation is applied in the audio thread as offsets to smoothed base
    parameters — APVTS knobs stay at the unmodulated values.
*/
namespace LfoMod
{
    constexpr int kNumLfos = 3;
    constexpr int kNumMatrixSlots = 20;

    enum Shape : int
    {
        sine = 0,
        triangle,
        square,
        sawUp,
        sawDown,
        numShapes
    };

    /** Matrix sources — Off + LFO 1..3 + Envelope Follower + Shape. */
    enum Source : int
    {
        srcOff = 0,
        srcLfo1,
        srcLfo2,
        srcLfo3,
        srcEnvFollower,
        srcShape,
        numSources
    };

    /** Matrix destinations — Off + Freq/Gain/Q for each gain-capable band. */
    enum Destination : int
    {
        destOff = 0,
        destBand1Freq, destBand1Gain, destBand1Q,
        destBand2Freq, destBand2Gain, destBand2Q,
        destBand3Freq, destBand3Gain, destBand3Q,
        destBand4Freq, destBand4Gain, destBand4Q,
        destHighShelfFreq, destHighShelfGain, destHighShelfQ,
        destLowShelfFreq, destLowShelfGain, destLowShelfQ,
        numDestinations
    };

    inline juce::StringArray getShapeNames()
    {
        return { "Sine", "Triangle", "Square", "Saw Up", "Saw Down" };
    }

    inline juce::StringArray getSourceNames()
    {
        return { "Off", "LFO 1", "LFO 2", "LFO 3", "Envelope Follower", "Shape" };
    }

    inline juce::StringArray getDestinationNames()
    {
        return {
            "Off",
            "Band 1 Freq", "Band 1 Gain", "Band 1 Q",
            "Band 2 Freq", "Band 2 Gain", "Band 2 Q",
            "Band 3 Freq", "Band 3 Gain", "Band 3 Q",
            "Band 4 Freq", "Band 4 Gain", "Band 4 Q",
            "High Shelf Freq", "High Shelf Gain", "High Shelf Q",
            "Low Shelf Freq", "Low Shelf Gain", "Low Shelf Q"
        };
    }

    /** Handle / OptionBox band index 0..7, or -1 if dest is Off / unknown. */
    inline int bandIndexForDestination (int dest) noexcept
    {
        switch (juce::jlimit (0, numDestinations - 1, dest))
        {
            case destBand1Freq: case destBand1Gain: case destBand1Q: return 0;
            case destBand2Freq: case destBand2Gain: case destBand2Q: return 1;
            case destBand3Freq: case destBand3Gain: case destBand3Q: return 2;
            case destBand4Freq: case destBand4Gain: case destBand4Q: return 3;
            case destHighShelfFreq: case destHighShelfGain: case destHighShelfQ: return 6;
            case destLowShelfFreq:  case destLowShelfGain:  case destLowShelfQ:  return 7;
            default: return -1;
        }
    }

    inline bool destinationTargetsBand (int dest, int bandIndex) noexcept
    {
        return bandIndexForDestination (dest) == bandIndex;
    }

    /** Short label for menus, e.g. "Gain" / "Freq" / "Q". */
    inline juce::String shortDestinationLabel (int dest)
    {
        switch (juce::jlimit (0, numDestinations - 1, dest))
        {
            case destBand1Freq: case destBand2Freq: case destBand3Freq:
            case destBand4Freq: case destHighShelfFreq: case destLowShelfFreq:
                return "Freq";
            case destBand1Gain: case destBand2Gain: case destBand3Gain:
            case destBand4Gain: case destHighShelfGain: case destLowShelfGain:
                return "Gain";
            case destBand1Q: case destBand2Q: case destBand3Q:
            case destBand4Q: case destHighShelfQ: case destLowShelfQ:
                return "Q";
            default:
                return "Off";
        }
    }

    inline int sourceToLfoIndex (int source) noexcept
    {
        switch (source)
        {
            case srcLfo1: return 0;
            case srcLfo2: return 1;
            case srcLfo3: return 2;
            default:      return -1;
        }
    }

    inline bool sourceIsEnvFollower (int source) noexcept
    {
        return source == srcEnvFollower;
    }

    inline bool sourceIsShape (int source) noexcept
    {
        return source == srcShape;
    }

    inline juce::String shapeParamId (int lfoIndex)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "Shape";
    }

    inline juce::String rateParamId (int lfoIndex)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "Rate";
    }

    /** false = Hz, true = host-tempo sync. */
    inline juce::String rateSyncParamId (int lfoIndex)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "RateSync";
    }

    /** Musical division index when RateSync is on. */
    inline juce::String syncDivParamId (int lfoIndex)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "SyncDiv";
    }

    inline juce::String phaseParamId (int lfoIndex)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "Phase";
    }

    /** Retrigger mode: Off / MIDI (Serum FX–style plugin MIDI input). */
    inline juce::String retriggerParamId (int lfoIndex)
    {
        return "lfo" + juce::String (lfoIndex + 1) + "Retrig";
    }

    enum RetrigMode
    {
        retrigOff = 0,
        retrigMidi,
        numRetrigModes
    };

    inline juce::StringArray getRetrigModeNames()
    {
        return { "Off", "MIDI" };
    }

    inline bool shouldRetrigger (int mode, bool midiNoteOn) noexcept
    {
        return mode == retrigMidi && midiNoteOn;
    }

    inline juce::String slotSourceParamId (int slot)
    {
        return "modSlot" + juce::String (slot + 1) + "Source";
    }

    /** Per-slot modulation amount 0–100% (applies when this routing is used). */
    inline juce::String slotAmountParamId (int slot)
    {
        return "modSlot" + juce::String (slot + 1) + "Amount";
    }

    inline juce::String slotDestParamId (int slot)
    {
        return "modSlot" + juce::String (slot + 1) + "Dest";
    }

    /** When false, routing is kept but modulation is bypassed (for testing). */
    inline juce::String slotEnabledParamId (int slot)
    {
        return "modSlot" + juce::String (slot + 1) + "On";
    }

    /**
        false = bipolar <> (−1..1 around base).
        true  = polar <| (0..1, one direction only).
    */
    inline juce::String slotPolarParamId (int slot)
    {
        return "modSlot" + juce::String (slot + 1) + "Polar";
    }

    inline juce::String envThresholdParamId() { return "modEnvThreshold"; }
    inline juce::String envAttackParamId()    { return "modEnvAttack"; }
    inline juce::String envReleaseParamId()   { return "modEnvRelease"; }

    inline juce::NormalisableRange<float> rateHzRange()
    {
        return { 0.05f, 20.0f, 0.01f, 0.35f };
    }

    /** Amount / depth 0–100% in whole percent steps. */
    inline juce::NormalisableRange<float> amountRange()
    {
        return { 0.0f, 100.0f, 1.0f };
    }

    /** Phase 0–360 deg. */
    inline juce::NormalisableRange<float> phaseRange()
    {
        return { 0.0f, 360.0f, 0.1f };
    }

    inline juce::NormalisableRange<float> envThresholdRange()
    {
        return { -60.0f, 0.0f, 0.1f };
    }

    inline juce::NormalisableRange<float> envAttackRange()
    {
        return { 0.5f, 100.0f, 0.1f, 0.4f };
    }

    inline juce::NormalisableRange<float> envReleaseRange()
    {
        return { 10.0f, 1000.0f, 1.0f, 0.4f };
    }

    /** Map bipolar −1..1 → polar 0..1 when polar mode is on. */
    inline float toModDrive (float bipolar, bool polar) noexcept
    {
        if (polar)
            return (bipolar + 1.0f) * 0.5f;
        return bipolar;
    }

    /** Env follower amount 0..1 → bipolar −1..1 for the shared drive path. */
    inline float envAmountToBipolar (float amount01) noexcept
    {
        return juce::jlimit (0.0f, 1.0f, amount01) * 2.0f - 1.0f;
    }

    /**
        Sync divisions (beats per LFO cycle; 1.0 = one quarter note).
        Default index = 1/4.
    */
    enum SyncDiv : int
    {
        sync8Bars = 0,
        sync4Bars,
        sync2Bars,
        sync1Bar,
        sync1_2,
        sync1_2D,
        sync1_2T,
        sync1_4,
        sync1_4D,
        sync1_4T,
        sync1_8,
        sync1_8D,
        sync1_8T,
        sync1_16,
        sync1_16D,
        sync1_16T,
        sync1_32,
        numSyncDivs
    };

    inline constexpr int kDefaultSyncDiv = sync1_4;

    inline juce::StringArray getSyncDivNames()
    {
        return {
            "8 bars", "4 bars", "2 bars", "1 bar",
            "1/2", "1/2 D", "1/2 T",
            "1/4", "1/4 D", "1/4 T",
            "1/8", "1/8 D", "1/8 T",
            "1/16", "1/16 D", "1/16 T",
            "1/32"
        };
    }

    /** Beats (quarter notes) per one LFO cycle. */
    inline float beatsPerCycleForSyncDiv (int divIndex) noexcept
    {
        switch (juce::jlimit (0, numSyncDivs - 1, divIndex))
        {
            case sync8Bars:  return 32.0f;
            case sync4Bars:  return 16.0f;
            case sync2Bars:  return 8.0f;
            case sync1Bar:   return 4.0f;
            case sync1_2:    return 2.0f;
            case sync1_2D:   return 3.0f;
            case sync1_2T:   return 4.0f / 3.0f;
            case sync1_4:    return 1.0f;
            case sync1_4D:   return 1.5f;
            case sync1_4T:   return 2.0f / 3.0f;
            case sync1_8:    return 0.5f;
            case sync1_8D:   return 0.75f;
            case sync1_8T:   return 1.0f / 3.0f;
            case sync1_16:   return 0.25f;
            case sync1_16D:  return 0.375f;
            case sync1_16T:  return 1.0f / 6.0f;
            case sync1_32:   return 0.125f;
            default:         return 1.0f;
        }
    }

    /** Convert sync division + BPM → free-running Hz. Falls back to 120 BPM. */
    inline float syncDivToHz (int divIndex, double bpm) noexcept
    {
        const double safeBpm = (bpm > 1.0 && bpm < 999.0) ? bpm : 120.0;
        const float beats = juce::jmax (1.0e-4f, beatsPerCycleForSyncDiv (divIndex));
        return (float) (safeBpm / (60.0 * (double) beats));
    }

    /** Resolve effective LFO rate in Hz from APVTS (Hz or Sync). */
    inline float resolveRateHz (juce::AudioProcessorValueTreeState& treeState,
                                int lfoIndex,
                                double bpm) noexcept
    {
        const bool sync = treeState.getRawParameterValue (rateSyncParamId (lfoIndex)) != nullptr
                          && treeState.getRawParameterValue (rateSyncParamId (lfoIndex))->load() > 0.5f;
        if (sync)
        {
            int div = kDefaultSyncDiv;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    treeState.getParameter (syncDivParamId (lfoIndex))))
                div = p->getIndex();
            return syncDivToHz (div, bpm);
        }

        if (auto* v = treeState.getRawParameterValue (rateParamId (lfoIndex)))
            return juce::jmax (0.05f, v->load());
        return 1.0f;
    }

    /** True if any enabled slot has a real source and destination. */
    inline bool anyActiveRouting (juce::AudioProcessorValueTreeState& treeState) noexcept
    {
        for (int s = 0; s < kNumMatrixSlots; ++s)
        {
            if (auto* onV = treeState.getRawParameterValue (slotEnabledParamId (s)))
                if (onV->load() <= 0.5f)
                    continue;

            int src = srcOff, dest = destOff;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (slotSourceParamId (s))))
                src = p->getIndex();
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (slotDestParamId (s))))
                dest = p->getIndex();

            if (src > srcOff && dest > destOff)
                return true;
        }
        return false;
    }

    /** True if this matrix source is used by any enabled slot with a real destination. */
    inline bool isSourceAssignedInMatrix (juce::AudioProcessorValueTreeState& treeState, int source) noexcept
    {
        if (source <= srcOff)
            return false;

        for (int s = 0; s < kNumMatrixSlots; ++s)
        {
            if (auto* onV = treeState.getRawParameterValue (slotEnabledParamId (s)))
                if (onV->load() <= 0.5f)
                    continue;

            int src = srcOff, dest = destOff;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (slotSourceParamId (s))))
                src = p->getIndex();
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (slotDestParamId (s))))
                dest = p->getIndex();

            if (src == source && dest > destOff)
                return true;
        }
        return false;
    }

    /** Sample waveform at phase 0..1 → bipolar −1..1. */
    inline float shapeAt (int shape, float phase01) noexcept
    {
        const float p = phase01 - std::floor (phase01);

        switch (shape)
        {
            case triangle:
                return p < 0.5f ? (4.0f * p - 1.0f) : (3.0f - 4.0f * p);
            case square:
                return p < 0.5f ? 1.0f : -1.0f;
            case sawUp:
                return 2.0f * p - 1.0f;
            case sawDown:
                return 1.0f - 2.0f * p;
            case sine:
            default:
                return std::sin (p * juce::MathConstants<float>::twoPi);
        }
    }

    struct Voice
    {
        double phase = 0.0; // 0..1

        void reset() noexcept { phase = 0.0; }

        /** Advance by numSamples at rateHz; return phase 0..1 at block centre. */
        float processBlock (double sampleRate, int numSamples, float rateHz, float phaseOffsetDeg) noexcept
        {
            if (sampleRate <= 0.0 || numSamples <= 0)
                return 0.0f;

            const double inc = (double) juce::jmax (0.05f, rateHz) / sampleRate;
            const double start = phase + (double) phaseOffsetDeg / 360.0;
            phase += inc * (double) numSamples;
            phase -= std::floor (phase);

            const double mid = start + inc * (0.5 * (double) numSamples);
            return (float) (mid - std::floor (mid));
        }
    };

    /** Broadband envelope follower for mod matrix (main-bus level → 0..1 above threshold). */
    struct EnvFollower
    {
        float envelopeDb = -140.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;

        void reset() noexcept { envelopeDb = -140.0f; }

        void updateCoeffs (float attackMs, float releaseMs, double sampleRate, int blockSize) noexcept
        {
            auto coeff = [sampleRate, blockSize] (float timeMs) -> float
            {
                const float timeSec = juce::jmax (0.001f, timeMs * 0.001f);
                const float n = (float) juce::jmax (1, blockSize);
                return std::exp (-n / (timeSec * (float) sampleRate));
            };
            attackCoeff = coeff (attackMs);
            releaseCoeff = coeff (releaseMs);
        }

        /** Returns amount 0..1 from stereo (or mono) peak level vs threshold. */
        float process (const float* left, const float* right, int numSamples, float thresholdDb) noexcept
        {
            if (numSamples <= 0 || left == nullptr)
                return 0.0f;

            float peak = 0.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                float s = std::abs (left[i]);
                if (right != nullptr)
                    s = juce::jmax (s, std::abs (right[i]));
                peak = juce::jmax (peak, s);
            }

            const float levelDb = juce::Decibels::gainToDecibels (juce::jmax (peak, 1.0e-8f), -140.0f);
            const float coeff = levelDb > envelopeDb ? attackCoeff : releaseCoeff;
            envelopeDb = coeff * envelopeDb + (1.0f - coeff) * levelDb;
            return juce::jlimit (0.0f, 1.0f, (envelopeDb - thresholdDb) / 24.0f);
        }
    };

    /** Apply depth% bipolar/polar drive to a destination value. */
    inline void applyToFreq (float& freqHz, float drive, float depthPercent) noexcept
    {
        const float depth = juce::jlimit (0.0f, 1.0f, depthPercent * 0.01f);
        const float oct = drive * depth * 2.0f;
        freqHz = juce::jlimit (20.0f, 20000.0f, freqHz * std::pow (2.0f, oct));
    }

    inline void applyToGain (float& gainDb, float drive, float depthPercent) noexcept
    {
        const float depth = juce::jlimit (0.0f, 1.0f, depthPercent * 0.01f);
        gainDb = juce::jlimit (-24.0f, 24.0f, gainDb + drive * depth * 24.0f);
    }

    inline void applyToQ (float& q, float drive, float depthPercent) noexcept
    {
        const float depth = juce::jlimit (0.0f, 1.0f, depthPercent * 0.01f);
        q = juce::jlimit (0.15f, 10.0f, q * std::pow (2.0f, drive * depth));
    }

    inline void applyDestination (int dest,
                                  float drive,
                                  float depth,
                                  float& b1F, float& b1G, float& b1Q,
                                  float& b2F, float& b2G, float& b2Q,
                                  float& b3F, float& b3G, float& b3Q,
                                  float& b4F, float& b4G, float& b4Q,
                                  float& hsF, float& hsG, float& hsQ,
                                  float& lsF, float& lsG, float& lsQ) noexcept
    {
        switch (dest)
        {
            case destBand1Freq: applyToFreq (b1F, drive, depth); break;
            case destBand1Gain: applyToGain (b1G, drive, depth); break;
            case destBand1Q:    applyToQ (b1Q, drive, depth); break;
            case destBand2Freq: applyToFreq (b2F, drive, depth); break;
            case destBand2Gain: applyToGain (b2G, drive, depth); break;
            case destBand2Q:    applyToQ (b2Q, drive, depth); break;
            case destBand3Freq: applyToFreq (b3F, drive, depth); break;
            case destBand3Gain: applyToGain (b3G, drive, depth); break;
            case destBand3Q:    applyToQ (b3Q, drive, depth); break;
            case destBand4Freq: applyToFreq (b4F, drive, depth); break;
            case destBand4Gain: applyToGain (b4G, drive, depth); break;
            case destBand4Q:    applyToQ (b4Q, drive, depth); break;
            case destHighShelfFreq: applyToFreq (hsF, drive, depth); break;
            case destHighShelfGain: applyToGain (hsG, drive, depth); break;
            case destHighShelfQ:    applyToQ (hsQ, drive, depth); break;
            case destLowShelfFreq:  applyToFreq (lsF, drive, depth); break;
            case destLowShelfGain:  applyToGain (lsG, drive, depth); break;
            case destLowShelfQ:     applyToQ (lsQ, drive, depth); break;
            default: break;
        }
    }

    /**
        Advance LFOs (optional MIDI retrigger), then apply each enabled matrix slot.
        envBipolar / shapeBipolar are −1..1 drives for Envelope Follower and Shape.
        slotPolar[i] true = unipolar / polar (<|).
    */
    inline void applyMatrix (std::array<Voice, kNumLfos>& voices,
                             double sampleRate,
                             int numSamples,
                             const int* shapes,
                             const float* ratesHz,
                             const float* phasesDeg,
                             const int* retrigModes,
                             bool midiNoteOn,
                             float envBipolar,
                             float shapeBipolar,
                             const int* slotSources,
                             const int* slotDests,
                             const float* slotAmountsPct,
                             const bool* slotEnabled,
                             const bool* slotPolar,
                             float* outBipolar,
                             float* outPhase01,
                             float& b1F, float& b1G, float& b1Q,
                             float& b2F, float& b2G, float& b2Q,
                             float& b3F, float& b3G, float& b3Q,
                             float& b4F, float& b4G, float& b4Q,
                             float& hsF, float& hsG, float& hsQ,
                             float& lsF, float& lsG, float& lsQ) noexcept
    {
        float bipolar[kNumLfos] {};
        for (int i = 0; i < kNumLfos; ++i)
        {
            const int mode = retrigModes != nullptr ? retrigModes[i] : retrigOff;
            if (shouldRetrigger (mode, midiNoteOn))
                voices[(size_t) i].reset();

            const float phase01 = voices[(size_t) i].processBlock (sampleRate, numSamples,
                                                                   ratesHz[i], phasesDeg[i]);
            bipolar[i] = shapeAt (shapes[i], phase01);
            if (outBipolar != nullptr)
                outBipolar[i] = bipolar[i];
            if (outPhase01 != nullptr)
                outPhase01[i] = phase01;
        }

        for (int s = 0; s < kNumMatrixSlots; ++s)
        {
            if (! slotEnabled[s])
                continue;

            const int src = slotSources[s];
            const int dest = slotDests[s];
            if (src <= srcOff || dest <= destOff || dest >= numDestinations)
                continue;

            float rawBipolar = 0.0f;
            if (sourceIsEnvFollower (src))
            {
                rawBipolar = envBipolar;
            }
            else if (sourceIsShape (src))
            {
                rawBipolar = shapeBipolar;
            }
            else
            {
                const int lfo = sourceToLfoIndex (src);
                if (lfo < 0)
                    continue;
                rawBipolar = bipolar[lfo];
            }

            const bool polar = slotPolar != nullptr && slotPolar[s];
            const float drive = toModDrive (rawBipolar, polar);

            applyDestination (dest, drive, slotAmountsPct[s],
                              b1F, b1G, b1Q, b2F, b2G, b2Q, b3F, b3G, b3Q,
                              b4F, b4G, b4Q, hsF, hsG, hsQ, lsF, lsG, lsQ);
        }
    }

    /** Append all LFO + env + matrix parameters to an APVTS layout vector. */
    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
    {
        for (int i = 0; i < kNumLfos; ++i)
        {
            const float defaultRate = (i == 0 ? 1.0f : (i == 1 ? 0.5f : 2.0f));
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                shapeParamId (i), "LFO" + juce::String (i + 1) + " Shape",
                getShapeNames(), sine));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                rateParamId (i), "LFO" + juce::String (i + 1) + " Rate",
                rateHzRange(), defaultRate));
            params.push_back (std::make_unique<juce::AudioParameterBool> (
                rateSyncParamId (i), "LFO" + juce::String (i + 1) + " Rate Sync", false));
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                syncDivParamId (i), "LFO" + juce::String (i + 1) + " Sync Div",
                getSyncDivNames(), kDefaultSyncDiv));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                phaseParamId (i), "LFO" + juce::String (i + 1) + " Phase",
                phaseRange(), 0.0f));
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                retriggerParamId (i), "LFO" + juce::String (i + 1) + " Retrigger",
                getRetrigModeNames(), retrigOff));
        }

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            envThresholdParamId(), "Mod Env Threshold",
            envThresholdRange(), -24.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            envAttackParamId(), "Mod Env Attack",
            envAttackRange(), 20.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            envReleaseParamId(), "Mod Env Release",
            envReleaseRange(), 200.0f));

        for (int s = 0; s < kNumMatrixSlots; ++s)
        {
            const juce::String n = juce::String (s + 1);
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                slotSourceParamId (s), "Mod Slot " + n + " Source",
                getSourceNames(), srcOff));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                slotAmountParamId (s), "Mod Slot " + n + " Amount",
                amountRange(), 50.0f));
            params.push_back (std::make_unique<juce::AudioParameterBool> (
                slotPolarParamId (s), "Mod Slot " + n + " Polar", false));
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                slotDestParamId (s), "Mod Slot " + n + " Dest",
                getDestinationNames(), destOff));
            params.push_back (std::make_unique<juce::AudioParameterBool> (
                slotEnabledParamId (s), "Mod Slot " + n + " On", true));
        }
    }
}
