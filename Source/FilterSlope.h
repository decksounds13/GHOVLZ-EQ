#pragma once

#include <JuceHeader.h>
#include <array>
#include "EqBand.h"

/** Shared HP/LP slope helpers: 6 / 12 / 24 / 48 / 96 dB per octave. */
namespace FilterSlope
{
    enum Choice : int
    {
        db6 = 0,
        db12,
        db24,
        db48,
        db96,
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        return { "6 dB/oct", "12 dB/oct", "24 dB/oct", "48 dB/oct", "96 dB/oct" };
    }

    /** Compact labels for narrow OptionBox combo (popup can still use full names via choices). */
    inline juce::StringArray getShortChoiceNames()
    {
        return { "6 dB", "12 dB", "24 dB", "48 dB", "96 dB" };
    }

    /** Per-band slope APVTS IDs (internal band index 0–7). */
    inline juce::String paramIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
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

    inline juce::String paramIDForGlobal (int globalDisplay)
    {
        return EqBand::slopeParamIDForGlobal (globalDisplay);
    }

    inline constexpr int maxBiquadStages = 8;

    /** Butterworth Q factors for cascaded 2nd-order sections (orders 2, 4, 8, 16). */
    inline int getButterworthQs (int slopeChoice, float* outQs, int maxOut)
    {
        switch (slopeChoice)
        {
            case db12:
                if (maxOut < 1) return 0;
                outQs[0] = 0.70710678f;
                return 1;
            case db24:
                if (maxOut < 2) return 0;
                outQs[0] = 0.54119610f;
                outQs[1] = 1.3065630f;
                return 2;
            case db48:
                if (maxOut < 4) return 0;
                outQs[0] = 0.50979558f;
                outQs[1] = 0.60134489f;
                outQs[2] = 0.89997622f;
                outQs[3] = 2.5629154f;
                return 4;
            case db96:
                if (maxOut < 8) return 0;
                outQs[0] = 0.50241929f;
                outQs[1] = 0.52249861f;
                outQs[2] = 0.56694402f;
                outQs[3] = 0.64682178f;
                outQs[4] = 0.78815462f;
                outQs[5] = 1.0606778f;
                outQs[6] = 1.7224471f;
                outQs[7] = 5.1011486f;
                return 8;
            default:
                return 0;
        }
    }

    struct StagePlan
    {
        bool firstOrder = false;
        int numBiquads = 0;
        std::array<float, maxBiquadStages> biquadQs {};
    };

    /** Build stage plan. For 12 dB/oct, userQ is used directly.
        For steeper slopes, Butterworth Qs are used and userQ scales the last stage
        for resonance (relative to Butterworth's 0.707 reference). */
    inline StagePlan makePlan (int slopeChoice, float userQ)
    {
        StagePlan plan;
        const float q = juce::jmax (0.05f, userQ);

        if (slopeChoice == db6)
        {
            plan.firstOrder = true;
            plan.numBiquads = 0;
            return plan;
        }

        if (slopeChoice == db12)
        {
            plan.numBiquads = 1;
            plan.biquadQs[0] = q;
            return plan;
        }

        plan.numBiquads = getButterworthQs (slopeChoice, plan.biquadQs.data(), maxBiquadStages);
        if (plan.numBiquads > 0)
        {
            // Allow the Q knob to add resonance on the highest-Q section.
            const float resonanceScale = q / 0.70710678f;
            plan.biquadQs[(size_t) plan.numBiquads - 1] *= juce::jlimit (0.25f, 8.0f, resonanceScale);
        }
        return plan;
    }

    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeHighpassCoeffs (double sampleRate, float cutoff, float userQ, int slopeChoice)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const float f = juce::jlimit (20.0f, 20000.0f, cutoff);
        const auto plan = makePlan (slopeChoice, userQ);

        if (plan.firstOrder)
        {
            stages.add (juce::dsp::IIR::Coefficients<float>::makeFirstOrderHighPass (sampleRate, f));
            return stages;
        }

        for (int i = 0; i < plan.numBiquads; ++i)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f, plan.biquadQs[(size_t) i]));

        return stages;
    }

    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeLowpassCoeffs (double sampleRate, float cutoff, float userQ, int slopeChoice)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const float f = juce::jlimit (20.0f, 20000.0f, cutoff);
        const auto plan = makePlan (slopeChoice, userQ);

        if (plan.firstOrder)
        {
            stages.add (juce::dsp::IIR::Coefficients<float>::makeFirstOrderLowPass (sampleRate, f));
            return stages;
        }

        for (int i = 0; i < plan.numBiquads; ++i)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, f, plan.biquadQs[(size_t) i]));

        return stages;
    }

    /** Linear magnitude of cascaded stages at one frequency (1 = unity). */
    inline float cascadeMagnitudeAt (
        const juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& stages,
        double frequencyHz,
        double sampleRate) noexcept
    {
        if (stages.isEmpty() || sampleRate <= 0.0)
            return 1.0f;
        // Product in double — float product of many stages can underflow to 0.
        double mag = 1.0;
        for (auto* coeffs : stages)
            if (coeffs != nullptr)
                mag *= coeffs->getMagnitudeForFrequency (frequencyHz, sampleRate);
        if (! std::isfinite (mag) || mag < 0.0)
            return 0.0f;
        return (float) mag;
    }

    /**
        HP×LP window weight at f for Match / Side Check range limiting.
        HP fully open (≤0 Hz) or LP at/above Nyquist → that side contributes 1.
    */
    inline float hpLpWindowWeight (double sampleRate, float freqHz,
                                   float hpHz, float lpHz,
                                   int hpSlope, int lpSlope) noexcept
    {
        const float f = juce::jmax (1.0f, freqHz);
        float w = 1.0f;
        if (hpHz > 1.0f)
        {
            const auto stages = makeHighpassCoeffs (sampleRate, hpHz, 0.70710678f, hpSlope);
            w *= cascadeMagnitudeAt (stages, (double) f, sampleRate);
        }
        if (lpHz > 1.0f && lpHz < (float) sampleRate * 0.49f)
        {
            const auto stages = makeLowpassCoeffs (sampleRate, lpHz, 0.70710678f, lpSlope);
            w *= cascadeMagnitudeAt (stages, (double) f, sampleRate);
        }
        return juce::jlimit (0.0f, 1.0f, w);
    }

    inline void fillCascadedMagnitude (const juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& stages,
                                       const std::vector<float>& frequencies,
                                       double sampleRate,
                                       std::vector<float>& outDb,
                                       int step)
    {
        const int w = (int) outDb.size();
        if (w == 0 || frequencies.size() != (size_t) w)
            return;

        // Empty cascade → flat 0 dB (never leave a stale previous curve).
        if (stages.isEmpty())
        {
            std::fill (outDb.begin(), outDb.end(), 0.0f);
            return;
        }

        const int stride = juce::jmax (1, step);

        auto evalDb = [&] (int i) -> float
        {
            // Double product — matches DSP cascade; float product of 6–8 stages
            // can underflow and paint a vanished band on the graph.
            double mag = 1.0;
            const double freq = (double) frequencies[(size_t) i];

            for (auto* coeffs : stages)
                if (coeffs != nullptr)
                    mag *= coeffs->getMagnitudeForFrequency (freq, sampleRate);

            if (! std::isfinite (mag) || mag <= 0.0)
                return -100.0f;

            return juce::Decibels::gainToDecibels ((float) mag, -100.0f);
        };

        for (int i = 0; i < w; i += stride)
            outDb[(size_t) i] = evalDb (i);

        if ((w - 1) % stride != 0)
            outDb[(size_t) (w - 1)] = evalDb (w - 1);

        // Linear interpolate gaps — hold-fill made steep HP/LP slopes stair-step on the sum path.
        if (stride > 1)
        {
            for (int i = 0; i + stride < w; i += stride)
            {
                const float a = outDb[(size_t) i];
                const float b = outDb[(size_t) (i + stride)];
                for (int j = 1; j < stride; ++j)
                {
                    const float t = (float) j / (float) stride;
                    outDb[(size_t) (i + j)] = a + (b - a) * t;
                }
            }

            const int lastFull = ((w - 1) / stride) * stride;
            if (lastFull < w - 1)
            {
                const float a = outDb[(size_t) lastFull];
                const float b = outDb[(size_t) (w - 1)];
                const int span = (w - 1) - lastFull;
                for (int j = 1; j < span; ++j)
                {
                    const float t = (float) j / (float) span;
                    outDb[(size_t) (lastFull + j)] = a + (b - a) * t;
                }
            }
        }
    }
}
