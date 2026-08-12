#pragma once

#include "EqLearnSettings.h"
#include <vector>

namespace EqLearn
{
    /**
        Offline residual → parametric bake (≤ maxBands, hard cap 10).

        Projects C(f) onto octave centres with musical Q (≈0.9–1.6), plus optional
        low/high shelves for broadband tilt. Avoids a single ultra-wide mid boost
        (which was the previous failure mode).
    */
    class ParametricMatcher
    {
    public:
        struct FitResult
        {
            juce::Array<BandProposal> bands;
            float maeDb = 0.0f;       // |H − C| after fit
            float residualMaeDb = 0.0f; // leftover residual energy
            int iterations = 0;
        };

        /**
            @param correctionDb  desired EQ curve C(f) on grid (already strength/clamped)
            @param gridHz        log-spaced frequencies
            @param n             grid length
            @param settings      maxBands, maxGainDb, etc.
        */
        static FitResult match (const float* correctionDb,
                                const float* gridHz,
                                int n,
                                const Settings& settings);
    };
}
