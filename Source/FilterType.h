#pragma once

#include <JuceHeader.h>
#include "FilterSlope.h"
#include "EqBand.h"

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
        // PR1 — Pro-Q parity shapes (append only; keep APVTS indices stable)
        tiltShelf,
        flatTilt,
        allPass,
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        // Slightly compact so the OptionBox closed combo can show the full label.
        return { "Bell", "Lo Shelf", "Hi Shelf", "Notch", "Band Pass", "Highpass", "Lowpass",
                 "Tilt Shelf", "Flat Tilt", "All Pass" };
    }

    inline bool usesGain (int type) noexcept
    {
        return type == bell || type == lowShelf || type == highShelf
            || type == tiltShelf || type == flatTilt;
    }

    inline bool isHpLp (int type) noexcept
    {
        return type == highpass || type == lowpass;
    }

    /** Types that need 2+ biquad stages (not HP/LP slope cascades). */
    inline bool isMultiStage (int type) noexcept
    {
        return type == tiltShelf || type == flatTilt;
    }

    /** Ableton-style proportional Q for peaking (bell) bands only. */
    inline float effectiveBellQ (int type, float baseQ, float gainDb, bool proportionalOn)
    {
        if (! proportionalOn || type != bell)
            return baseQ;

        return baseQ * (1.0f + std::abs (gainDb) / 24.0f);
    }

    /**
        Tilt shelf: complementary low + high shelves at the same frequency.
        Positive gain → lows up / highs down; negative reverses.
        Q controls transition sharpness (flatTilt uses a fixed gentle Q).
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeTiltStages (double sampleRate, float frequency, float q, float gainDb, bool flat)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        const float halfDb = 0.5f * gainDb;
        const float gUp = juce::Decibels::decibelsToGain (halfDb);
        const float gDn = juce::Decibels::decibelsToGain (-halfDb);
        // Flat tilt: very wide shelves (gentle, near-constant slope through the pivot).
        // Tilt shelf: user Q, still soft enough to stay musical.
        const float shelfQ = flat ? 0.35f
                                  : juce::jlimit (0.15f, 2.0f, q);

        stages.add (juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, shelfQ, gUp));
        stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, shelfQ, gDn));
        return stages;
    }

    /** Single-biquad types (not cascaded HP/LP slopes or multi-stage tilts). */
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
            case allPass:
                return juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, f, safeQ);
            case tiltShelf:
            case flatTilt:
            {
                // Fallback single stage if a caller only supports one biquad:
                // approximate with a low shelf of half gain (prefer makeStages).
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                    sampleRate, f, type == flatTilt ? 0.35f : juce::jlimit (0.15f, 2.0f, safeQ),
                    juce::Decibels::decibelsToGain (0.5f * gainDb));
            }
            case bell:
            default:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, f, safeQ, gainLin);
        }
    }

    /**
        Full stage list for any non-slope type (single or multi-stage).
        HP/LP with slopes still use FilterSlope::make*Coeffs.
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeStages (int type, double sampleRate, float frequency, float q, float gainDb)
    {
        if (type == tiltShelf)
            return makeTiltStages (sampleRate, frequency, q, gainDb, false);
        if (type == flatTilt)
            return makeTiltStages (sampleRate, frequency, q, gainDb, true);

        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        if (auto c = makeCoefficients (type, sampleRate, frequency, q, gainDb))
            stages.add (c);
        return stages;
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

    /** Global display index (0 = Band 1 … 63 = Band 64). */
    inline juce::String paramIDForGlobal (int globalDisplay)
    {
        return EqBand::typeParamIDForGlobal (globalDisplay);
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
