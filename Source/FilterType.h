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
        // PR2 — Ozone-style shapes
        bandShelf,
        baxandallBass,
        baxandallTreble,
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        // Slightly compact so the OptionBox closed combo can show the full label.
        return { "Bell", "Lo Shelf", "Hi Shelf", "Notch", "Band Pass", "Highpass", "Lowpass",
                 "Tilt Shelf", "Flat Tilt", "All Pass",
                 "Band Shelf", "Bax Bass", "Bax Treble" };
    }

    inline bool usesGain (int type) noexcept
    {
        return type == bell || type == lowShelf || type == highShelf
            || type == tiltShelf || type == flatTilt
            || type == bandShelf || type == baxandallBass || type == baxandallTreble;
    }

    inline bool isHpLp (int type) noexcept
    {
        return type == highpass || type == lowpass;
    }

    /** Types that need 2+ biquad stages (not HP/LP slope cascades). */
    inline bool isMultiStage (int type) noexcept
    {
        return type == tiltShelf || type == flatTilt || type == bandShelf;
    }

    /** Fixed-Q gentle shelves (Baxandall / flat tilt) — hide Q in UI. */
    inline bool hidesQ (int type) noexcept
    {
        return type == flatTilt || type == baxandallBass || type == baxandallTreble;
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

    /**
        Band shelf (flat-top plateau): high-shelf +G at f_lo, high-shelf −G at f_hi.
        Q maps to bandwidth in octaves (higher Q → narrower plateau).
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeBandShelfStages (double sampleRate, float frequency, float q, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        // ~0.5–6 octaves of plateau width from Q 10…0.15
        const float bwOct = juce::jlimit (0.5f, 6.0f, 2.0f / juce::jmax (0.15f, q));
        const float halfRatio = std::pow (2.0f, 0.5f * bwOct);
        const float fLo = juce::jlimit (20.0f, 20000.0f, f / halfRatio);
        const float fHi = juce::jlimit (20.0f, 20000.0f, f * halfRatio);
        const float g = juce::Decibels::decibelsToGain (gainDb);
        const float gInv = juce::Decibels::decibelsToGain (-gainDb);
        constexpr float sQ = 0.70710678f;
        stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, fLo, sQ, g));
        stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, fHi, sQ, gInv));
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
        // Baxandall: very wide, gentle shelves (classic tone-stack feel).
        constexpr float kBaxQ = 0.30f;

        switch (type)
        {
            case lowShelf:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, safeQ, gainLin);
            case highShelf:
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, safeQ, gainLin);
            case baxandallBass:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, kBaxQ, gainLin);
            case baxandallTreble:
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, kBaxQ, gainLin);
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
            case bandShelf:
            {
                // Single-stage fallback: wide peak (prefer makeStages for true plateau).
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    sampleRate, f, juce::jlimit (0.15f, 1.0f, safeQ * 0.35f), gainLin);
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
        if (type == bandShelf)
            return makeBandShelfStages (sampleRate, frequency, q, gainDb);

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
        // Ozone-like defaults: gentle Bax at extremes when not cutting.
        if (f < 50.0f)    return highpass;
        if (f < 120.0f)   return baxandallBass;
        if (f < 8000.0f)  return bell;
        if (f < 14000.0f) return baxandallTreble;
        return lowpass;
    }
}
