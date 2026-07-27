#pragma once

#include <JuceHeader.h>
#include "FilterSlope.h"

/** Filter models for all eight Band 1–8 slots (HP/LP inclusive). */
namespace FilterType
{
    enum Choice : int
    {
        bell = 0,
        lowShelf,
        highShelf,
        notch,
        bandPass,
        highpass,
        lowpass,
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        // Slightly compact so the OptionBox closed combo can show the full label.
        return { "Bell", "Lo Shelf", "Hi Shelf", "Notch", "Band Pass", "Highpass", "Lowpass" };
    }

    inline bool usesGain (int type)
    {
        return type == bell || type == lowShelf || type == highShelf;
    }

    inline bool isHpLp (int type) noexcept
    {
        return type == highpass || type == lowpass;
    }

    /** Ableton-style proportional Q for peaking (bell) bands only. */
    inline float effectiveBellQ (int type, float baseQ, float gainDb, bool proportionalOn)
    {
        if (! proportionalOn || type != bell)
            return baseQ;

        return baseQ * (1.0f + std::abs (gainDb) / 24.0f);
    }

    /** Single-biquad types (not cascaded HP/LP slopes). */
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
            case highpass:
                // 12 dB single-biquad fallback; cascaded slopes use FilterSlope.
                return juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, safeQ);
            case lowpass:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, f, safeQ);
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
            case 4: return "highpassType";
            case 5: return "lowpassType";
            case 6: return "highShelfType";
            case 7: return "lowShelfType";
            default: return {};
        }
    }

    /**
        Defaults: Band1 HP, Band2 LS, Band3–6 Bell, Band7 HS, Band8 LP
        (internal indices 4,7,0–3,6,5).
    */
    inline int defaultTypeForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 4: return highpass;
            case 7: return lowShelf;
            case 6: return highShelf;
            case 5: return lowpass;
            default: return bell;
        }
    }

    /** Preferred type when creating a band by graph-click frequency zone. */
    inline int typeForFrequencyZone (float frequencyHz) noexcept
    {
        const float f = juce::jlimit (20.0f, 20000.0f, frequencyHz);
        if (f < 50.0f)   return highpass;
        if (f < 150.0f)  return lowShelf;
        if (f < 8000.0f) return bell;
        if (f < 12000.0f) return highShelf;
        return lowpass;
    }
}
