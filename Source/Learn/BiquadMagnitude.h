#pragma once

#include "EqLearnSettings.h"
#include <vector>

namespace EqLearn
{
    /**
        Offline magnitude response for Learn bake (bell / low shelf / high shelf).
        Uses the same JUCE IIR designs as FilterType::makeCoefficients so baked
        bands match what the DSP will sound like.
    */
    namespace BiquadMagnitude
    {
        /** Nominal rate for coefficient design (response shape is fs-stable for our band range). */
        inline constexpr double kDesignSampleRate = 48000.0;

        /** Single-band magnitude in dB on a frequency grid. */
        void evaluateBandDb (int filterType,
                             float frequencyHz,
                             float gainDb,
                             float q,
                             const float* gridHz,
                             float* outDb,
                             int n,
                             double sampleRate = kDesignSampleRate) noexcept;

        /** Sum of band magnitudes in dB (cascade ≈ sum in dB for second-order stages). */
        void evaluateCascadeDb (const BandProposal* bands,
                                int numBands,
                                const float* gridHz,
                                float* outDb,
                                int n,
                                double sampleRate = kDesignSampleRate) noexcept;

        /** mean |a − b| over n points. */
        float meanAbsError (const float* a, const float* b, int n) noexcept;
    }
}
