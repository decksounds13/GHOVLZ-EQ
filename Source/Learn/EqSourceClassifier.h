#pragma once

#include "EqLearnSettings.h"

namespace EqLearn
{
    /**
        Rule-based source classifier from averaged spectrum (dB) + crest + optional T/S.
        Message-thread only. No ML / no network.
    */
    class SourceClassifier
    {
    public:
        /**
            @param spectrumDb   absolute level per bin (dB)
            @param frequenciesHz bin centre frequencies
            @param numBins
            @param crestDb      peak − RMS in dB (from input meters)
            @param transientRatio  StructuralSplit mean T-gain 0..1 (ignored if hasTransientStats false)
            @param hasTransientStats  true when ratio was measured during Learn/Split
        */
        static SpectralFeatures extractFeatures (const float* spectrumDb,
                                                 const float* frequenciesHz,
                                                 int numBins,
                                                 float crestDb,
                                                 float transientRatio = 0.3f,
                                                 bool hasTransientStats = false) noexcept;

        static Classification classify (const SpectralFeatures& features,
                                        float minConfidence = kMinDetectConfidence) noexcept;

        static Classification classify (const float* spectrumDb,
                                        const float* frequenciesHz,
                                        int numBins,
                                        float crestDb,
                                        float minConfidence = kMinDetectConfidence,
                                        float transientRatio = 0.3f,
                                        bool hasTransientStats = false) noexcept;
    };
}
