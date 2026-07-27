#pragma once

#include <JuceHeader.h>

/**
    Canonical Band 1–8 identity for UI and helpers.

    Internal DSP/UI indices (legacy layout in FrequencyResponseComponent handles) stay:
      0–3 peaking params (band1–4*), 4 highpass*, 5 lowpass*, 6 highShelf*, 7 lowShelf*

    Display / faceplate order (left → right):
      Band 1 = HP slot (4), Band 2 = low shelf (7), Band 3–6 = peaking (0–3),
      Band 7 = high shelf (6), Band 8 = LP slot (5)
*/
namespace EqBand
{
    constexpr int kCount = 8;

    /** Faceplate / "Band N" order → internal index. */
    inline constexpr int internalFromDisplay (int displayZeroBased) noexcept
    {
        constexpr int map[kCount] = { 4, 7, 0, 1, 2, 3, 6, 5 };
        return (displayZeroBased >= 0 && displayZeroBased < kCount) ? map[displayZeroBased] : 0;
    }

    /** Internal index → 0-based display order (Band N = display + 1). */
    inline constexpr int displayFromInternal (int internal) noexcept
    {
        switch (internal)
        {
            case 4: return 0; // Band 1
            case 7: return 1; // Band 2
            case 0: return 2; // Band 3
            case 1: return 3; // Band 4
            case 2: return 4; // Band 5
            case 3: return 5; // Band 6
            case 6: return 6; // Band 7
            case 5: return 7; // Band 8
            default: return 0;
        }
    }

    inline juce::String displayNameForInternal (int internal)
    {
        return "Band " + juce::String (displayFromInternal (internal) + 1);
    }

    inline juce::String displayNameForDisplay (int displayZeroBased)
    {
        return "Band " + juce::String (juce::jlimit (0, kCount - 1, displayZeroBased) + 1);
    }

    /** Default centre frequencies when a band is enabled (zone-aligned). */
    inline float defaultFrequencyHz (int internal) noexcept
    {
        switch (internal)
        {
            case 4: return 20.0f;     // Band 1 HP
            case 7: return 100.0f;    // Band 2 LS
            case 0: return 300.0f;    // Band 3
            case 1: return 800.0f;    // Band 4
            case 2: return 2000.0f;   // Band 5
            case 3: return 5000.0f;   // Band 6
            case 6: return 10000.0f;  // Band 7 HS
            case 5: return 20000.0f;  // Band 8 LP
            default: return 1000.0f;
        }
    }

    inline juce::String frequencyParamID (int internal)
    {
        switch (internal)
        {
            case 0: return "band1Frequency";
            case 1: return "band2Frequency";
            case 2: return "band3Frequency";
            case 3: return "band4Frequency";
            case 4: return "highpassCutoff";
            case 5: return "lowpassCutoff";
            case 6: return "highShelfFrequency";
            case 7: return "lowShelfFrequency";
            default: return {};
        }
    }

    inline juce::String gainParamID (int internal)
    {
        switch (internal)
        {
            case 0: return "band1Gain";
            case 1: return "band2Gain";
            case 2: return "band3Gain";
            case 3: return "band4Gain";
            case 4: return "highpassGain";
            case 5: return "lowpassGain";
            case 6: return "highShelfGain";
            case 7: return "lowShelfGain";
            default: return {};
        }
    }

    inline juce::String qParamID (int internal)
    {
        switch (internal)
        {
            case 0: return "band1Q";
            case 1: return "band2Q";
            case 2: return "band3Q";
            case 3: return "band4Q";
            case 4: return "highpassQ";
            case 5: return "lowpassQ";
            case 6: return "highShelfQ";
            case 7: return "lowShelfQ";
            default: return {};
        }
    }

    inline juce::String onOffParamID (int internal)
    {
        switch (internal)
        {
            case 0: return "band1OnOff";
            case 1: return "band2OnOff";
            case 2: return "band3OnOff";
            case 3: return "band4OnOff";
            case 4: return "highpassOnOff";
            case 5: return "lowpassOnOff";
            case 6: return "highShelfOnOff";
            case 7: return "lowShelfOnOff";
            default: return {};
        }
    }

    inline juce::String slopeParamID (int internal)
    {
        switch (internal)
        {
            case 0: return "band1Slope";
            case 1: return "band2Slope";
            case 2: return "band3Slope";
            case 3: return "band4Slope";
            case 4: return "highpassSlope";
            case 5: return "lowpassSlope";
            case 6: return "highShelfSlope";
            case 7: return "lowShelfSlope";
            default: return {};
        }
    }
}
