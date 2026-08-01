#pragma once

#include <JuceHeader.h>

/**
    Canonical Band identity for UI and helpers.

    Bank 1 (display Band 1–8) keeps legacy internal DSP indices and APVTS IDs:
      Internal: 0–3 peaking (band1–4*), 4 highpass*, 5 lowpass*, 6 highShelf*, 7 lowShelf*
      Display L→R: Band1=HP(4), Band2=LS(7), Band3–6=peaking(0–3), Band7=HS(6), Band8=LP(5)

    Banks 2–8 (display Band 9–64) use uniform IDs: eqB09Frequency, eqB09Gain, …
    Every slot is type-agnostic after create; zone only picks the initial type.
*/
namespace EqBand
{
    constexpr int kBankSize = 8;
    constexpr int kMaxBanks = 8;
    constexpr int kMaxBands = kBankSize * kMaxBanks; // 64
    /** @deprecated Prefer kBankSize — Bank 1 slot count / faceplate columns. */
    constexpr int kCount = kBankSize;

    /** Faceplate / "Band N" order → internal index (Bank 1 only). */
    inline constexpr int internalFromDisplay (int displayZeroBased) noexcept
    {
        constexpr int map[kBankSize] = { 4, 7, 0, 1, 2, 3, 6, 5 };
        return (displayZeroBased >= 0 && displayZeroBased < kBankSize) ? map[displayZeroBased] : 0;
    }

    /** Internal index → 0-based display order within Bank 1 (Band N = display + 1). */
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

    inline constexpr int bankFromGlobal (int globalDisplay) noexcept
    {
        return globalDisplay / kBankSize;
    }

    inline constexpr int slotInBankFromGlobal (int globalDisplay) noexcept
    {
        return globalDisplay % kBankSize;
    }

    /** Global display index for faceplate bank + column (0-based). */
    inline constexpr int globalFromBankSlot (int bank, int slotInBank) noexcept
    {
        return bank * kBankSize + slotInBank;
    }

    /** Theme Graph Band 1–8 colour index (wraps across banks). */
    inline constexpr int colourSlotFromGlobal (int globalDisplay) noexcept
    {
        return ((globalDisplay % kBankSize) + kBankSize) % kBankSize;
    }

    inline juce::String displayNameForGlobal (int globalDisplay)
    {
        return "Band " + juce::String (juce::jlimit (0, kMaxBands - 1, globalDisplay) + 1);
    }

    inline juce::String displayNameForInternal (int internal)
    {
        return displayNameForGlobal (displayFromInternal (internal));
    }

    inline juce::String displayNameForDisplay (int displayZeroBased)
    {
        return displayNameForGlobal (displayZeroBased);
    }

    /** eqB09 … eqB64 prefix for banks 2+ (1-based band number, zero-padded). */
    inline juce::String extendedPrefix (int globalDisplay)
    {
        const int bandNumber = juce::jlimit (kBankSize + 1, kMaxBands, globalDisplay + 1);
        return "eqB" + juce::String (bandNumber).paddedLeft ('0', 2);
    }

    inline juce::String extendedParamID (int globalDisplay, const char* suffix)
    {
        return extendedPrefix (globalDisplay) + suffix;
    }

    /** Default centre frequencies when a Bank 1 band is enabled (zone-aligned). */
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

    inline float defaultFrequencyHzForGlobal (int globalDisplay) noexcept
    {
        if (globalDisplay >= 0 && globalDisplay < kBankSize)
            return defaultFrequencyHz (internalFromDisplay (globalDisplay));
        return 1000.0f;
    }

    // ----- Bank 1 legacy IDs (internal index 0–7) -----

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

    // ----- Unified global-display IDs (0 = Band 1 … 63 = Band 64) -----

    inline juce::String frequencyParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
            return frequencyParamID (internalFromDisplay (globalDisplay));
        return extendedParamID (globalDisplay, "Frequency");
    }

    inline juce::String gainParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
            return gainParamID (internalFromDisplay (globalDisplay));
        return extendedParamID (globalDisplay, "Gain");
    }

    inline juce::String qParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
            return qParamID (internalFromDisplay (globalDisplay));
        return extendedParamID (globalDisplay, "Q");
    }

    inline juce::String onOffParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
            return onOffParamID (internalFromDisplay (globalDisplay));
        return extendedParamID (globalDisplay, "OnOff");
    }

    inline juce::String slopeParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
            return slopeParamID (internalFromDisplay (globalDisplay));
        return extendedParamID (globalDisplay, "Slope");
    }

    inline juce::String typeParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
        {
            switch (internalFromDisplay (globalDisplay))
            {
                case 0: return "band1Type";
                case 1: return "band2Type";
                case 2: return "band3Type";
                case 3: return "band4Type";
                case 4: return "highpassType";
                case 5: return "lowpassType";
                case 6: return "highShelfType";
                case 7: return "lowShelfType";
                default: return {};
            }
        }
        return extendedParamID (globalDisplay, "Type");
    }

    inline juce::String channelParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < kBankSize)
        {
            switch (internalFromDisplay (globalDisplay))
            {
                case 0: return "band1Channel";
                case 1: return "band2Channel";
                case 2: return "band3Channel";
                case 3: return "band4Channel";
                case 4: return "highpassChannel";
                case 5: return "lowpassChannel";
                case 6: return "highShelfChannel";
                case 7: return "lowShelfChannel";
                default: return {};
            }
        }
        return extendedParamID (globalDisplay, "Channel");
    }
}
