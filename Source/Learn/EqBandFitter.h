#pragma once

#include "EqLearnSettings.h"
#include <vector>

namespace EqLearn
{
    /**
        Offline band fitter façade:
        source spectrum S(f) + target T(f) → Match-style correction C(f)
        → residual parametric bake (shelves + peaking, ≤ maxBands ≤ 10).

        Message-thread only. Does not touch APVTS.
    */
    class BandFitter
    {
    public:
        /** Build mean-normalized factory target on the fit grid (pink / flat). */
        static void makeFactoryTargetDb (Target target,
                                         const float* frequenciesHz,
                                         float* destDb,
                                         int numPoints) noexcept;

        /**
            Fit Match-style error C = strength * clamp(T_n − S_n) with residual bake.
            @param sourceDb / sourceHz  analyser bins (absolute dB)
            @param targetDb / targetHz  target shape (absolute or relative; mean-normalized with source)
            @param settings             strength, max bands, gain clamp
            @param out                  proposals (unassigned slots); also writes MAE into outMae if non-null
        */
        static void fit (const float* sourceDb,
                         const float* sourceHz,
                         int sourceN,
                         const float* targetDb,
                         const float* targetHz,
                         int targetN,
                         const Settings& settings,
                         juce::Array<BandProposal>& out,
                         float* outMaeDb = nullptr,
                         float* outResidualMaeDb = nullptr);

    private:
        static void resampleLog (const float* srcDb, const float* srcHz, int srcN,
                                 const float* gridHz, float* destDb, int gridN) noexcept;
        static void meanNormalize (float* db, int n) noexcept;
        static void smoothOctaves (const float* gridHz, float* db, int n, float octaves) noexcept;
    };
}
