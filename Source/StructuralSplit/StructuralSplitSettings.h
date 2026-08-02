#pragma once

#include <JuceHeader.h>

/**
    Per-band Transient / Sustain targeting (SplitEQ-style) + global separator.

    Mode Off  = band processes the full signal (current behaviour).
    Transient = band's EQ delta is applied only to the transient stream.
    Sustain   = band's EQ delta is applied only to the sustain stream.
*/
namespace StructuralSplit
{
    enum class Mode : int
    {
        off = 0,
        transient,
        sustain
    };

    enum class Solo : int
    {
        off = 0,
        transient,
        sustain
    };

    inline juce::StringArray modeChoiceNames()
    {
        return { "Off", "Transient", "Sustain" };
    }

    inline juce::StringArray soloChoiceNames()
    {
        return { "Off", "Transient", "Sustain" };
    }

    /** Bank-1 internal index 0–7 → APVTS id (same map as spectral). */
    inline juce::String splitModeParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SplitMode";
            case 1: return "band2SplitMode";
            case 2: return "band3SplitMode";
            case 3: return "band4SplitMode";
            case 4: return "highpassSplitMode";
            case 5: return "lowpassSplitMode";
            case 6: return "highShelfSplitMode";
            case 7: return "lowShelfSplitMode";
            default: return {};
        }
    }

    inline constexpr const char* separationParamId() noexcept { return "structuralSplitSeparation"; }
    inline constexpr const char* soloParamId() noexcept { return "structuralSplitSolo"; }

    constexpr float kDefaultSeparation = 50.0f;
    constexpr float kMinSeparation = 0.0f;
    constexpr float kMaxSeparation = 100.0f;

    inline Mode clampMode (int v) noexcept
    {
        return static_cast<Mode> (juce::jlimit (0, (int) Mode::sustain, v));
    }

    inline Solo clampSolo (int v) noexcept
    {
        return static_cast<Solo> (juce::jlimit (0, (int) Solo::sustain, v));
    }
}
