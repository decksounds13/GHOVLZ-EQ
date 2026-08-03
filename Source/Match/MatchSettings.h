#pragma once

#include <JuceHeader.h>
#include <cmath>

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
    inline constexpr const char* smoothParamId() noexcept { return "matchSmooth"; }
    inline constexpr const char* resolutionParamId() noexcept { return "matchResolution"; }
    inline constexpr const char* frozenParamId() noexcept { return "matchFrozen"; }
    inline constexpr const char* placementParamId() noexcept { return "matchPlacement"; }
    inline constexpr const char* hpHzParamId() noexcept { return "matchHpHz"; }
    inline constexpr const char* lpHzParamId() noexcept { return "matchLpHz"; }
    inline constexpr const char* hpSlopeParamId() noexcept { return "matchHpSlope"; }
    inline constexpr const char* lpSlopeParamId() noexcept { return "matchLpSlope"; }

    constexpr float kMinAmount = 0.0f;
    constexpr float kMaxAmount = 1.0f;
    constexpr float kDefaultAmount = 0.5f;

    /** Frequency-domain lattice smooth (0 = sharp slices, 1 = max neighbour blend + wider Q). */
    constexpr float kMinSmooth = 0.0f;
    constexpr float kMaxSmooth = 1.0f;
    constexpr float kDefaultSmooth = 0.35f;
    /** At Smooth=1, scale packed Q by this (wider bands / more overlap). */
    constexpr float kSmoothMinQScale = 0.55f;
    constexpr int kNumSmoothMenuItems = 5;

    /** HP/LP effect window — only slices with centre in [HP, LP] get Match GR (like Side Check). */
    constexpr float kMaxFreqHz = 20000.0f;
    constexpr float kMinHpLpHz = 0.0f;
    constexpr float kDefaultHpHz = 0.0f;
    constexpr float kDefaultLpHz = 18000.0f;
    constexpr float kMinHpLpGapHz = 50.0f;

    /** Max lattice capacity — High resolution. Never exceed (CPU ceiling). */
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

    /** Lattice density — capped at kNumSlices. */
    enum Resolution : int
    {
        resLow = 0,  // 16
        resMed,      // 24
        resHigh,     // 32
        numResolutions
    };

    enum Placement : int
    {
        beforeEq = 0,
        afterEq,
        numPlacements
    };

    inline int sliceCountForResolution (int resolution) noexcept
    {
        switch (juce::jlimit (0, numResolutions - 1, resolution))
        {
            case resLow:  return 16;
            case resMed:  return 24;
            case resHigh:
            default:      return kNumSlices;
        }
    }

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

    inline juce::StringArray getResolutionChoiceNames()
    {
        return { "Low (16)", "Med (24)", "High (32)" };
    }

    inline juce::StringArray getSmoothMenuNames()
    {
        return { "Off", "Light", "Med", "Strong", "Max" };
    }

    inline float smoothMenuValue (int index) noexcept
    {
        switch (juce::jlimit (0, kNumSmoothMenuItems - 1, index))
        {
            case 0:  return 0.0f;
            case 1:  return 0.20f;
            case 2:  return kDefaultSmooth;
            case 3:  return 0.60f;
            default: return 1.0f;
        }
    }

    /** Nearest Smooth menu index for the current continuous value. */
    inline int nearestSmoothMenuIndex (float smooth01) noexcept
    {
        const float s = juce::jlimit (kMinSmooth, kMaxSmooth, smooth01);
        int best = 0;
        float bestErr = std::abs (s - smoothMenuValue (0));
        for (int i = 1; i < kNumSmoothMenuItems; ++i)
        {
            const float err = std::abs (s - smoothMenuValue (i));
            if (err < bestErr)
            {
                bestErr = err;
                best = i;
            }
        }
        return best;
    }

    /** Q multiply for lattice rebuild: 1 at Smooth=0 → kSmoothMinQScale at Smooth=1. */
    inline float latticeQScaleForSmooth (float smooth01) noexcept
    {
        const float s = juce::jlimit (kMinSmooth, kMaxSmooth, smooth01);
        return 1.0f + s * (kSmoothMinQScale - 1.0f);
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
