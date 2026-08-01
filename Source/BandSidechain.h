#pragma once

#include <JuceHeader.h>
#include "DynamicEq.h"
#include "EqBand.h"

/**
    Per-band external sidechain ducking (audio bus or MIDI gate).

    When enabled, band gain is driven toward the gain knob by the sidechain
    envelope (same A/R as Dynamic/Spectral). MIDI mode: any note = full engage.
    Audio mode: bandpass detect on the Sidechain input bus (like Dynamic EQ).
*/
namespace BandSidechain
{
    inline juce::String sidechainParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1Sidechain";
            case 1: return "band2Sidechain";
            case 2: return "band3Sidechain";
            case 3: return "band4Sidechain";
            case 4: return "highpassSidechain";
            case 5: return "lowpassSidechain";
            case 6: return "highShelfSidechain";
            case 7: return "lowShelfSidechain";
            default: return {};
        }
    }

    inline juce::String midiParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SidechainMidi";
            case 1: return "band2SidechainMidi";
            case 2: return "band3SidechainMidi";
            case 3: return "band4SidechainMidi";
            case 4: return "highpassSidechainMidi";
            case 5: return "lowpassSidechainMidi";
            case 6: return "highShelfSidechainMidi";
            case 7: return "lowShelfSidechainMidi";
            default: return {};
        }
    }

    inline bool supportsSidechain (int bandIndex)
    {
        return DynamicEq::supportsDynamic (bandIndex);
    }

    inline juce::String sidechainParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < EqBand::kBankSize)
            return sidechainParamIDForBandIndex (EqBand::internalFromDisplay (globalDisplay));
        return EqBand::extendedParamID (globalDisplay, "Sidechain");
    }

    inline juce::String midiParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < EqBand::kBankSize)
            return midiParamIDForBandIndex (EqBand::internalFromDisplay (globalDisplay));
        return EqBand::extendedParamID (globalDisplay, "SidechainMidi");
    }

    /** MIDI / audio envelope state (0..1 amount). */
    struct GateState
    {
        float amount = 0.0f;
        float attackCoeff = 0.0f;
        float releaseCoeff = 0.0f;

        void prepare (double sampleRate, int blockSize)
        {
            updateCoeffs (DynamicEq::attackMs, DynamicEq::releaseMs, sampleRate, blockSize);
            amount = 0.0f;
        }

        void reset() noexcept { amount = 0.0f; }

        void updateCoeffs (float attackMs, float releaseMs, double sampleRate, int blockSize)
        {
            attackCoeff = DynamicEq::coeffForTimeMs (DynamicEq::clampAttackMs (attackMs), sampleRate, blockSize);
            releaseCoeff = DynamicEq::coeffForTimeMs (DynamicEq::clampReleaseMs (releaseMs), sampleRate, blockSize);
        }

        /** Step toward targetAmount (0 or 1 for MIDI; audio path sets target each block). */
        float stepToward (float targetAmount) noexcept
        {
            const float t = juce::jlimit (0.0f, 1.0f, targetAmount);
            const float coeff = t > amount ? attackCoeff : releaseCoeff;
            amount = coeff * amount + (1.0f - coeff) * t;
            return amount;
        }
    };
}
