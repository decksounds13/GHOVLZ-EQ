#pragma once

#include <JuceHeader.h>
#include "FilterSlope.h"
#include "EqBand.h"
#include <cmath>

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
        // PR3 — brickwall cuts + vintage (Pultec-style) shelves
        brickwallHighpass,
        brickwallLowpass,
        vintageLowShelf,
        vintageHighShelf,
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        // Slightly compact so the OptionBox closed combo can show the full label.
        return { "Bell", "Lo Shelf", "Hi Shelf", "Notch", "Band Pass", "Highpass", "Lowpass",
                 "Tilt Shelf", "Flat Tilt", "All Pass",
                 "Band Shelf", "Bax Bass", "Bax Treble",
                 "Brick HP", "Brick LP", "Vintage LS", "Vintage HS" };
    }

    inline bool usesGain (int type) noexcept
    {
        return type == bell || type == lowShelf || type == highShelf
            || type == tiltShelf || type == flatTilt
            || type == bandShelf || type == baxandallBass || type == baxandallTreble
            || type == vintageLowShelf || type == vintageHighShelf;
    }

    inline bool isBrickwall (int type) noexcept
    {
        return type == brickwallHighpass || type == brickwallLowpass;
    }

    inline bool isHighpassFamily (int type) noexcept
    {
        return type == highpass || type == brickwallHighpass;
    }

    inline bool isLowpassFamily (int type) noexcept
    {
        return type == lowpass || type == brickwallLowpass;
    }

    /** HP/LP family including brickwall (no gain; cascade path). */
    inline bool isHpLp (int type) noexcept
    {
        return isHighpassFamily (type) || isLowpassFamily (type);
    }

    /** Types that need 2+ biquad stages (not HP/LP slope cascades). */
    inline bool isMultiStage (int type) noexcept
    {
        return type == tiltShelf || type == flatTilt || type == bandShelf
            || type == vintageLowShelf || type == vintageHighShelf;
    }

    /** Fixed-Q gentle shelves (Baxandall / flat tilt) — hide Q in UI. */
    inline bool hidesQ (int type) noexcept
    {
        return type == flatTilt || type == baxandallBass || type == baxandallTreble;
    }

    /** Slope menu only for regular (non-brickwall) HP/LP. */
    inline bool showsFilterSlope (int type) noexcept
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

    /**
        Vintage / Pultec-style shelf: main shelf + complementary dip near the boost.
        Positive gain: shelf boost with a nearby cut (classic “boost with dip” curve).
        Negative gain: shelf cut with a smaller complementary lift.
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeVintageShelfStages (double sampleRate, float frequency, float q, float gainDb, bool highShelf)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        const float shelfQ = juce::jlimit (0.2f, 1.5f, q);
        const float gMain = juce::Decibels::decibelsToGain (gainDb);
        // Complementary dip ~40% of main move, opposite sign; wider for cuts.
        const float dipRatio = gainDb >= 0.0f ? 0.40f : 0.25f;
        const float gDip = juce::Decibels::decibelsToGain (-gainDb * dipRatio);
        const float dipQ = juce::jlimit (0.4f, 2.5f, shelfQ * 1.4f);
        // Dip sits ~1.6 octaves into the pass of the shelf.
        const float dipF = highShelf
            ? juce::jlimit (20.0f, 20000.0f, f / 2.8f)
            : juce::jlimit (20.0f, 20000.0f, f * 2.8f);

        if (highShelf)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, shelfQ, gMain));
        else
            stages.add (juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, shelfQ, gMain));

        stages.add (juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, dipF, dipQ, gDip));
        return stages;
    }

    /**
        Brickwall cut: steepest stable cascade (16th-order Butterworth = 96 dB/oct).
        Fixed “wall” slope — Q only adds knee resonance (same as steep HP/LP).
        Distinct from regular Highpass/Lowpass, which expose 6–96 dB slope choices.
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeBrickwallStages (double sampleRate, float frequency, float q, bool highpass)
    {
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        const float safeQ = juce::jmax (0.15f, q);
        return highpass
            ? FilterSlope::makeHighpassCoeffs (sampleRate, f, safeQ, FilterSlope::db96)
            : FilterSlope::makeLowpassCoeffs (sampleRate, f, safeQ, FilterSlope::db96);
    }

    /** Single-biquad types (not cascaded HP/LP slopes or multi-stage shapes). */
    inline juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (int type,
                                                                      double sampleRate,
                                                                      float frequency,
                                                                      float q,
                                                                      float gainDb)
    {
        const float f = juce::jlimit (20.0f, 20000.0f, frequency);
        const float safeQ = juce::jmax (0.05f, q);
        const float gainLin = juce::Decibels::decibelsToGain (gainDb);
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
            case vintageLowShelf:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (sampleRate, f, safeQ, gainLin);
            case vintageHighShelf:
                return juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, f, safeQ, gainLin);
            case notch:
                return juce::dsp::IIR::Coefficients<float>::makeNotch (sampleRate, f, safeQ);
            case bandPass:
                return juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, f, safeQ);
            case highpass:
            case brickwallHighpass:
                return juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, safeQ);
            case lowpass:
            case brickwallLowpass:
                return juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, f, safeQ);
            case allPass:
                return juce::dsp::IIR::Coefficients<float>::makeAllPass (sampleRate, f, safeQ);
            case tiltShelf:
            case flatTilt:
                return juce::dsp::IIR::Coefficients<float>::makeLowShelf (
                    sampleRate, f, type == flatTilt ? 0.35f : juce::jlimit (0.15f, 2.0f, safeQ),
                    juce::Decibels::decibelsToGain (0.5f * gainDb));
            case bandShelf:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (
                    sampleRate, f, juce::jlimit (0.15f, 1.0f, safeQ * 0.35f), gainLin);
            case bell:
            default:
                return juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, f, safeQ, gainLin);
        }
    }

    /**
        Full stage list for any non-slope type (single or multi-stage).
        Regular HP/LP with slopes still use FilterSlope::make*Coeffs.
        Brickwall uses makeBrickwallStages.
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
        if (type == vintageLowShelf)
            return makeVintageShelfStages (sampleRate, frequency, q, gainDb, false);
        if (type == vintageHighShelf)
            return makeVintageShelfStages (sampleRate, frequency, q, gainDb, true);
        if (type == brickwallHighpass)
            return makeBrickwallStages (sampleRate, frequency, q, true);
        if (type == brickwallLowpass)
            return makeBrickwallStages (sampleRate, frequency, q, false);

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
        if (f < 50.0f)    return highpass;
        if (f < 120.0f)   return baxandallBass;
        if (f < 8000.0f)  return bell;
        if (f < 14000.0f) return baxandallTreble;
        return lowpass;
    }
}
