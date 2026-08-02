#pragma once

#include <JuceHeader.h>

/**
    Spectral Match — global shape-match toward a target curve (noise slopes / capture).
    Isolated from per-band Spectral (S) and Side Check.
*/
namespace MatchEq
{
    inline constexpr const char* enabledParamId() noexcept { return "matchEnable"; }
    inline constexpr const char* amountParamId() noexcept { return "matchAmount"; }
    inline constexpr const char* curveParamId() noexcept { return "matchCurve"; }
    inline constexpr const char* speedParamId() noexcept { return "matchSpeed"; }
    inline constexpr const char* frozenParamId() noexcept { return "matchFrozen"; }
    inline constexpr const char* placementParamId() noexcept { return "matchPlacement"; }

    constexpr float kMinAmount = 0.0f;
    constexpr float kMaxAmount = 1.0f;
    constexpr float kDefaultAmount = 0.5f;

    constexpr int kNumSlices = 32;
    constexpr int kMaxUserPresets = 12;
    constexpr float kRefHz = 1000.0f;
    constexpr float kMaxGrDb = 12.0f;
    constexpr float kSilenceFloorDb = -90.0f;

    enum Curve : int
    {
        flat = 0,
        white,
        pink,
        brown,
        blue,
        violet,
        capture,
        numFactoryCurves
    };

    enum Speed : int
    {
        fast = 0,
        med,
        slow,
        numSpeeds
    };

    enum Placement : int
    {
        beforeEq = 0,
        afterEq,
        numPlacements
    };

    inline juce::StringArray getCurveChoiceNames()
    {
        return {
            "Flat (0 dB/oct)",
            "White noise (0 dB/oct)",
            "Pink noise (-3 dB/oct)",
            "Brown noise (-6 dB/oct)",
            "Blue noise (+3 dB/oct)",
            "Violet noise (+6 dB/oct)",
            "Capture spectrum"
        };
    }

    inline juce::StringArray getSpeedChoiceNames()
    {
        return { "Fast", "Med", "Slow" };
    }

    inline juce::StringArray getPlacementChoiceNames()
    {
        return { "Before EQ", "After EQ" };
    }

    /** dB/octave slope for factory curves (Capture ignored). */
    inline float slopeDbPerOctave (int curve) noexcept
    {
        switch (curve)
        {
            case pink:   return -3.0f;
            case brown:  return -6.0f;
            case blue:   return  3.0f;
            case violet: return  6.0f;
            case flat:
            case white:
            default:     return  0.0f;
        }
    }

    inline void getBallisticsMs (int speed, float& attackMs, float& releaseMs, float& grSmoothMs) noexcept
    {
        if (speed == slow)
        {
            attackMs = 120.0f;
            releaseMs = 900.0f;
            grSmoothMs = 70.0f;
        }
        else if (speed == med)
        {
            attackMs = 40.0f;
            releaseMs = 300.0f;
            grSmoothMs = 28.0f;
        }
        else
        {
            attackMs = 12.0f;
            releaseMs = 100.0f;
            grSmoothMs = 12.0f;
        }
    }

    inline int readChoiceIndex (juce::AudioProcessorValueTreeState& state,
                                const char* id,
                                int fallback,
                                int maxIndex) noexcept
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (id)))
            return juce::jlimit (0, maxIndex, choice->getIndex());
        if (auto* raw = state.getRawParameterValue (id))
            return juce::jlimit (0, maxIndex, (int) std::lround (raw->load()));
        return fallback;
    }
}
