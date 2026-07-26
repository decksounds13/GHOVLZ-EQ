#pragma once

#include <JuceHeader.h>

/** Filter models for the six tunable bands (not HP/LP). */
namespace FilterType
{
    enum Choice : int
    {
        bell = 0,
        lowShelf,
        highShelf,
        notch,
        bandPass,
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        return { "Bell", "Low Shelf", "High Shelf", "Notch", "Band Pass" };
    }

    inline bool usesGain (int type)
    {
        return type == bell || type == lowShelf || type == highShelf;
    }

    /** Ableton-style proportional Q for peaking (bell) bands only.
     *  Stored Q is the base Q (independent of gain). When proportional mode is on:
     *    Qeff = baseQ * (1 + |gainDb| / 24)
     *  → 0 dB: ≈1×; ±12 dB: 1.5×; ±24 dB: 2× (tighter as |gain| rises).
     *  Non-bell types always return baseQ unchanged.
     */
    inline float effectiveBellQ (int type, float baseQ, float gainDb, bool proportionalOn)
    {
        if (! proportionalOn || type != bell)
            return baseQ;

        return baseQ * (1.0f + std::abs (gainDb) / 24.0f);
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (int type,
                                                                      double sampleRate,
                                                                      float frequency,
                                                                      float q,
                                                                      float gainDb)
    {
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        const float safeQ = juce::jmax (0.05f, q);
        const float gainLin = juce::Decibels::decibelsToGain (gainDb);

        switch (type)
        {
            case lowShelf:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, safeQ, gainLin);
            case highShelf:
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, safeQ, gainLin);
            case notch:
                return juce::dsp::IIR::Coefficients<float>::makeNotch (sampleRate, f, safeQ);
            case bandPass:
                return juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, f, safeQ);
            case bell:
            default:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, f, safeQ, gainLin);
        }
    }

    inline juce::String paramIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1Type";
            case 1: return "band2Type";
            case 2: return "band3Type";
            case 3: return "band4Type";
            case 6: return "highShelfType";
            case 7: return "lowShelfType";
            default: return {};
        }
    }

    /** Defaults match current plugin behavior: peaks are Bell; shelves stay shelves. */
    inline int defaultTypeForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 6: return highShelf;
            case 7: return lowShelf;
            default: return bell;
        }
    }
}
