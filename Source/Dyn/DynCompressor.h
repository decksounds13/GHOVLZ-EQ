#pragma once

#include <JuceHeader.h>
#include <array>
#include <atomic>
#include "DynParams.h"

/** Linkwitz-Riley (min-phase) or complementary FIR (linear-phase) splits + compressor. */
class DynCompressor
{
public:
    void prepare (double sampleRate, int samplesPerBlock);
    void reset() noexcept;
    void process (juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& state,
                  bool isOfflineRender);

    int getLatencySamples() const noexcept { return reportedLatency; }

    float getGainReductionDb (int band) const noexcept
    {
        if (band < 0 || band >= DynParams::kMaxBands)
            return 0.0f;
        return grDb[(size_t) band].load (std::memory_order_relaxed);
    }

    float getUpwardGrDb (int band) const noexcept
    {
        if (band < 0 || band >= DynParams::kMaxBands)
            return 0.0f;
        return upGrDb[(size_t) band].load (std::memory_order_relaxed);
    }

    /** Detector envelope (dB) for the transfer-plot traveling ball. */
    float getInputEnvelopeDb (int band) const noexcept
    {
        if (band < 0 || band >= DynParams::kMaxBands)
            return -140.0f;
        return inDb[(size_t) band].load (std::memory_order_relaxed);
    }

    float getClipDb (int band) const noexcept
    {
        if (band < 0 || band >= DynParams::kMaxBands)
            return 0.0f;
        return clipDb[(size_t) band].load (std::memory_order_relaxed);
    }

    int getBandCount (juce::AudioProcessorValueTreeState& state) const;

private:
    void ensureRatePath (int osLog2, bool linearPhase, int maxN, int chans);
    void rebuildFirCoeffs (double processSr, const float* splits, int numSplits);
    void splitMinPhase (int count, int n, int chans, juce::AudioProcessorValueTreeState& state);
    void splitLinearPhase (int count, int n, int chans, const float* splits);
    void processDynamics (int count, int n, int chans, double processSr,
                          juce::AudioProcessorValueTreeState& state,
                          juce::AudioBuffer<float>& dryBuf,
                          juce::AudioBuffer<float>& outBuf);
    struct Band
    {
        juce::dsp::LinkwitzRileyFilter<float> lp;
        juce::dsp::LinkwitzRileyFilter<float> hp;
        float envDb = -140.0f;
        float rmsLin = 0.0f;
        float autoMakeupDb = 0.0f;
        juce::AudioBuffer<float> work;
        std::array<std::vector<float>, 2> lookBuf {};
        int lookWrite = 0;
    };

    double sr = 44100.0;
    std::array<Band, DynParams::kMaxBands> bands;
    std::array<std::atomic<float>, DynParams::kMaxBands> grDb {};
    std::array<std::atomic<float>, DynParams::kMaxBands> upGrDb {};
    std::array<std::atomic<float>, DynParams::kMaxBands> inDb {};
    std::array<std::atomic<float>, DynParams::kMaxBands> clipDb {};
    juce::AudioBuffer<float> dry;
    juce::AudioBuffer<float> delayedIn;
    juce::AudioBuffer<float> osWork;
    juce::AudioBuffer<float> lpWork[DynParams::kMaxBands];
    float globalAutoMakeupDb = 0.0f;

    static constexpr int kFirTaps = 1023;
    static constexpr int kFirDelay = (kFirTaps - 1) / 2;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    int preparedOsLog2 = -1;
    int maxBlock = 512;
    int reportedLatency = 0;
    bool preparedLinPhase = false;
    double processSr = 44100.0;

    using Fir = juce::dsp::FIR::Filter<float>;
    std::array<std::array<Fir, 2>, DynParams::kMaxBands> firLp {};
    std::array<float, DynParams::kMaxBands> lastSplitHz {};
    std::array<std::vector<float>, 2> inDelay {};
    int inDelayWrite = 0;
    std::array<std::vector<float>, 2> dryLook {};
    int dryLookWrite = 0;
};
