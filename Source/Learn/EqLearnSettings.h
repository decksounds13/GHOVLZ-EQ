#pragma once

#include <JuceHeader.h>
#include <array>

/**
    Learn EQ — one-shot parametric band fit toward a target spectrum.
    Isolated from continuous Spectral Match (MatchEq).
*/
namespace EqLearn
{
    inline constexpr float kMinFreqHz = 20.0f;
    inline constexpr float kMaxFreqHz = 20000.0f;
    inline constexpr float kRefHz = 1000.0f;

    /** Default capture window before fit (seconds). Longer = more stable bass/note avg. */
    inline constexpr float kDefaultCaptureSec = 2.5f;
    inline constexpr float kMinCaptureSec = 0.8f;
    inline constexpr float kMaxCaptureSec = 6.0f;

    /**
        Silence gate: peak bin level below this dB skips a frame / aborts Learn.
        Uses peak (not mean) so sparse HF bins don't drag the average into the floor.
    */
    inline constexpr float kSilenceFloorDb = -55.0f;

    /** Only average frames within this many dB of the session peak (ignore quiet gaps). */
    inline constexpr float kCaptureDynamicGateDb = 18.0f;

    /** Minimum accepted frames before a Learn bake is allowed. */
    inline constexpr int kMinCaptureFrames = 12;

    /**
        Shape-match energy mask: bins more than this below source peak are excluded
        from mean-normalization (stops silent HF from dominating bass captures).
    */
    inline constexpr float kShapeMaskBelowPeakDb = 36.0f;

    /**
        Soft activity: weight goes 0→1 from (peak − this) up to (peak − half).
        Below full silence we still allow gentle correction; hard-zero was too aggressive
        and produced permanent "No change".
    */
    inline constexpr float kCorrectionActiveBelowPeakDb = 40.0f;

    /** Frequency range used for tonal Learn shape match. */
    inline constexpr float kShapeMatchMinHz = 30.0f;
    inline constexpr float kShapeMatchMaxHz = 16000.0f;

    /**
        Soft rolloff of C(f) above source 90% energy frequency × this factor.
        Floor at kMinCorrectBandwidthHz so bass still gets low/mid correction room.
    */
    inline constexpr float kActiveBandwidthMargin = 2.5f;
    inline constexpr float kMinCorrectBandwidthHz = 4500.0f;

    inline constexpr float kDefaultStrength = 1.0f;
    inline constexpr float kMinStrength = 0.1f;
    inline constexpr float kMaxStrength = 1.0f;

    inline constexpr int kDefaultMaxBands = 6;
    inline constexpr int kMinMaxBands = 1;
    /** Hard product cap: bake at most 10 editable bands. */
    inline constexpr int kMaxMaxBands = 10;

    /** Clamp per-band gain proposals (dB) — Match-aligned ceiling. */
    inline constexpr float kDefaultMaxGainDb = 12.0f;
    inline constexpr float kMinProposalGainDb = 0.35f;

    /** Log-frequency work grid for fitting. */
    inline constexpr int kFitGridPoints = 128;

    /** Minimum spacing between peaking proposals (octaves). */
    inline constexpr float kMinPeakSpacingOct = 0.55f;

    /** Octave smoothing applied to the correction curve (Match-like global shape). */
    inline constexpr float kErrorSmoothOctaves = 0.35f;

    // ---- Residual parametric matcher (curve → bands) ----
    inline constexpr float kShelfQFixed = 0.707f;
    inline constexpr float kMinPeakQ = 0.30f;
    inline constexpr float kMaxPeakQ = 4.0f;
    inline constexpr float kLowShelfFcMin = 40.0f;
    inline constexpr float kLowShelfFcMax = 400.0f;
    inline constexpr float kHighShelfFcMin = 2000.0f;
    inline constexpr float kHighShelfFcMax = 12000.0f;
    /** Min |low/mid or high/mid| tilt before proposing a shelf. */
    inline constexpr float kShelfMinTiltDb = 0.45f;
    /** Stop adding peaks when residual loss falls below this. */
    inline constexpr float kResidualStopMaeDb = 0.12f;
    /** Peak Fc may only walk this many octaves from its seed during optimise. */
    inline constexpr float kPeakFcMaxWalkOct = 0.45f;
    /** Min |mean residual| in an octave window to place a band. */
    inline constexpr float kRegionMinAbsDb = 0.40f;
    /** Always place at least one band if residual peak exceeds this (dB). */
    inline constexpr float kForceBandMinAbsDb = 0.50f;
    /** Peaking Q never wider than this (musical floor). */
    inline constexpr float kMinMusicalPeakQ = 0.85f;

    /**
        Bake optimizer (AutoEq / AcoustiQ inspired):
        - Energy-weighted RMSE prioritises large acoustic errors over HF floor wiggle.
        - Above this Hz only total energy matters (AutoEq 10 kHz collapse).
        - Extra joint-refine passes after greedy placement.
    */
    inline constexpr float kBakeTrebleCollapseHz = 10000.0f;
    inline constexpr int kJointRefinePasses = 4;
    /** Blend: 0 = pure dB RMSE, 1 = fully energy-weighted (AcoustiQ-like). */
    inline constexpr float kEnergyWeightBlend = 0.65f;

    /**
        Minimum confidence to accept Auto-detect (0..1).
        Below this, Learn falls back to Pink and reports Unknown.
    */
    inline constexpr float kMinDetectConfidence = 0.38f;

    // -------------------------------------------------------------------------
    // Source classes (rule-based detection + templates)
    // -------------------------------------------------------------------------

    enum class SourceClass : int
    {
        unknown = 0,
        bass,
        guitar,
        vocals,
        synth,
        drums,
        mix,
        numClasses
    };

    inline juce::String sourceClassName (SourceClass c) noexcept
    {
        switch (c)
        {
            case SourceClass::bass:   return "Bass";
            case SourceClass::guitar: return "Guitar";
            case SourceClass::vocals: return "Vocals";
            case SourceClass::synth:  return "Synth";
            case SourceClass::drums:  return "Drums";
            case SourceClass::mix:    return "Mix";
            case SourceClass::unknown:
            default:                 return "Unknown";
        }
    }

    inline juce::StringArray getManualSourceNames()
    {
        // Manual overrides only (no Unknown)
        return { "Bass", "Guitar", "Vocals", "Synth", "Drums", "Mix" };
    }

    inline SourceClass sourceClassFromManualIndex (int index) noexcept
    {
        // 0..5 → bass..mix
        switch (juce::jlimit (0, 5, index))
        {
            case 0:  return SourceClass::bass;
            case 1:  return SourceClass::guitar;
            case 2:  return SourceClass::vocals;
            case 3:  return SourceClass::synth;
            case 4:  return SourceClass::drums;
            default: return SourceClass::mix;
        }
    }

    enum class Target : int
    {
        pink = 0,       // music-friendly -3 dB/oct
        flat,           // technical flat spectrum
        matchCurve,     // current Match engine target
        autoSource,     // classify → source template (or pink if low conf)
        sourceBass,
        sourceGuitar,
        sourceVocals,
        sourceSynth,
        sourceDrums,
        sourceMix,
        numTargets
    };

    inline bool isSourceTemplateTarget (Target t) noexcept
    {
        return t == Target::autoSource
            || t == Target::sourceBass
            || t == Target::sourceGuitar
            || t == Target::sourceVocals
            || t == Target::sourceSynth
            || t == Target::sourceDrums
            || t == Target::sourceMix;
    }

    inline SourceClass sourceClassForTarget (Target t) noexcept
    {
        switch (t)
        {
            case Target::sourceBass:   return SourceClass::bass;
            case Target::sourceGuitar: return SourceClass::guitar;
            case Target::sourceVocals: return SourceClass::vocals;
            case Target::sourceSynth:  return SourceClass::synth;
            case Target::sourceDrums:  return SourceClass::drums;
            case Target::sourceMix:    return SourceClass::mix;
            default:                   return SourceClass::unknown;
        }
    }

    inline juce::StringArray getTargetNames()
    {
        return {
            "Pink (-3 dB/oct)",
            "Flat (0 dB/oct)",
            "Match curve",
            "Auto-detect source",
            "Bass template",
            "Guitar template",
            "Vocals template",
            "Synth template",
            "Drums template",
            "Mix template"
        };
    }

    inline juce::String targetShortName (Target t) noexcept
    {
        switch (t)
        {
            case Target::flat:         return "Flat";
            case Target::matchCurve:   return "Match";
            case Target::autoSource:   return "Auto";
            case Target::sourceBass:   return "Bass";
            case Target::sourceGuitar: return "Guitar";
            case Target::sourceVocals: return "Vocals";
            case Target::sourceSynth:  return "Synth";
            case Target::sourceDrums:  return "Drums";
            case Target::sourceMix:    return "Mix";
            case Target::pink:
            default:                   return "Pink";
        }
    }

    struct SpectralFeatures
    {
        float centroidHz = 1000.0f;
        float rolloffHz = 5000.0f;   // 85% energy
        float flatness = 0.5f;       // 0 = tonal, 1 = noise-like
        float energySub = 0.0f;      // fraction of total linear power
        float energyBass = 0.0f;
        float energyLowMid = 0.0f;
        float energyMid = 0.0f;
        float energyPresence = 0.0f;
        float energyAir = 0.0f;
        float crestDb = 12.0f;       // peak − RMS
        float meanDb = -60.0f;
        /**
            Mean StructuralSplit transient gain 0..1 (1 = transient, 0 = sustain).
            Only valid when Learn was capturing T/S stats (or Split DSP was armed).
        */
        float transientRatio = 0.3f;
        bool hasTransientStats = false;
    };

    struct Classification
    {
        SourceClass label = SourceClass::unknown;
        float confidence = 0.0f; // 0..1
        SpectralFeatures features;
        /** Per-class scores before pick (size numClasses). */
        std::array<float, (size_t) SourceClass::numClasses> scores {};
        juce::String summary; // e.g. "likely Vocals (68%)"
    };

    struct Settings
    {
        /** Default Auto so Learn uses detection → class template, not pink-vs-bass HF mess. */
        Target target = Target::autoSource;
        float strength = kDefaultStrength;
        int maxBands = kDefaultMaxBands;
        bool usePreEq = true;
        float captureSec = kDefaultCaptureSec;
        float maxGainDb = kDefaultMaxGainDb;
        /** When true, overwrite Bank 1 peaking (and shelves if used) even if already on. */
        bool replaceMidBells = true;
        /** Auto-detect confidence floor (also used for reporting). */
        float minDetectConfidence = kMinDetectConfidence;
    };

    struct BandProposal
    {
        int globalDisplay = -1; // assigned slot (EqBand global)
        float frequencyHz = 1000.0f;
        float gainDb = 0.0f;
        float q = 1.0f;
        int filterType = 0; // FilterType::bell / lowShelf / highShelf
        bool isShelf = false;
    };

    struct Result
    {
        bool ok = false;
        juce::String message;
        int bandsApplied = 0;
        Target target = Target::pink;
        float strength = 1.0f;
        juce::Array<BandProposal> proposals;

        /** Fit quality: mean |H_cascade − C| on the Learn grid (dB). */
        float fitMaeDb = 0.0f;
        float residualMaeDb = 0.0f;

        /** Detection outcome (filled when target is Auto or always when classified). */
        SourceClass detectedClass = SourceClass::unknown;
        float detectConfidence = 0.0f;
        SourceClass appliedSourceClass = SourceClass::unknown; // template actually used
        bool usedDetection = false;
        SpectralFeatures features {};

        /** Monotonic; UI only rebuilds the EQ curve when this increases after apply. */
        int applySerial = 0;
    };
}
