#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include <cmath>
#include "MatchSettings.h"
#include "../DynamicEq.h"

namespace MatchEq
{
    /**
        Global spectral shape Match: log BP lattice compares detect energy to a
        target curve and reconstructs toward that shape (level-normalized).
    */
    class Processor
    {
    public:
        void prepare (double newSampleRate, int samplesPerBlock) noexcept;
        void reset() noexcept;

        /**
            Process stereo (or mono) buffer in-place toward the working target.
            @param detectL/R  pre-Match tap (may equal buffer channels when detecting in-place
                              is unsafe — caller should pass a copy when needed).
        */
        void process (juce::AudioBuffer<float>& buffer,
                      const float* detectL,
                      const float* detectR,
                      bool enabled,
                      float amount,
                      int speedMode,
                      float smooth01,
                      float hpHz,
                      float lpHz) noexcept;

        void setFactoryTarget (int curveIndex) noexcept;
        /** Copy analyser-style magnitude (display 0..1 or linear) onto the working target. */
        void setCaptureTargetFromSpectrum (const float* magnitudes,
                                           const float* frequenciesHz,
                                           int numBins) noexcept;
        void setWorkingTargetDb (const std::array<float, kNumSlices>& db) noexcept;
        std::array<float, kNumSlices> getWorkingTargetDb() const noexcept;
        std::array<float, kNumSlices> getCenterHz() const noexcept;

        void sampleTargetDb (const float* frequenciesHz, float* destDb, int numPoints) const noexcept;
        /** Graph sum-curve sample: smooth log-f spline through slice GR (not BP lobe sum). */
        void samplePublishedGrDb (const float* frequenciesHz, float* destDb, int numPoints) const noexcept;

        float getPublishedGrDb() const noexcept { return publishedPeakGrDb.load (std::memory_order_relaxed); }
        int getSliceCount() const noexcept { return activeSlices; }

        /** User presets (persisted via ValueTree). */
        juce::ValueTree toUserPresetsTree() const;
        void fromUserPresetsTree (const juce::ValueTree& tree);
        int getNumUserPresets() const noexcept { return numUserPresets; }
        juce::String getUserPresetName (int index) const;
        bool saveUserPreset (const juce::String& name);
        bool loadUserPreset (int index);
        bool removeUserPreset (int index);

    private:
        struct Slice
        {
            juce::dsp::IIR::Filter<float> detect;
            juce::dsp::IIR::Filter<float> applyL;
            juce::dsp::IIR::Filter<float> applyR;
            float centerHz = 1000.0f;
            float q = 1.0f;
            float envDb = kSilenceFloorDb;
            float grLin = 1.0f;
            float grTarget = 1.0f;
            double sumSq = 0.0;
        };

        struct PublishedCurve
        {
            std::array<float, kNumSlices> centerHz {};
            std::array<float, kNumSlices> grDb {};
            int count = 0;
        };

        struct UserPreset
        {
            juce::String name;
            std::array<float, kNumSlices> targetDb {};
        };

        void rebuildLattice() noexcept;
        void fillFactoryTarget (int curveIndex) noexcept;
        void normalizeWorkingTarget() noexcept;
        void publishGrCurve() noexcept;
        void clearPublished() noexcept;
        void applySmoothToLatticeIfNeeded (float smooth01) noexcept;

        double sampleRate = 48000.0;
        int blockSize = 512;
        int activeSlices = 0;
        bool prepared = false;
        bool wasEnabled = false;
        bool settling = false;
        float latticeSmooth01 = kDefaultSmooth;

        std::array<Slice, kNumSlices> slices {};
        std::array<float, kNumSlices> workingTargetDb {};
        std::array<float, kNumSlices> publishedTargetDb {};
        mutable juce::SpinLock targetLock;

        std::array<PublishedCurve, 2> publishedCurves {};
        std::atomic<int> publishedIndex { 0 };
        std::atomic<float> publishedPeakGrDb { 0.0f };

        std::array<UserPreset, kMaxUserPresets> userPresets {};
        int numUserPresets = 0;
    };
}
