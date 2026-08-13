#pragma once

#include <JuceHeader.h>
#include "SpectrumAnalysis.h"

using ValueTree = juce::AudioProcessorValueTreeState;

// ****************************************************************************
// ANALYSER CLASS
// Ableton-style: audio thread fills a ring; analysis thread FFTs the latest
// Block samples every Refresh ms (overlapping windows).
// ****************************************************************************
class Analyser : public juce::AudioProcessorValueTreeState::Listener,
                 private juce::Thread
{
public:
    using Channels = SpectrumAnalysis::Channel;

    Analyser (juce::AudioProcessorValueTreeState& audioProcessorValueTreeState);
    ~Analyser() override;

    // ========================================================================
    void resetScopeMaximumsData();

    /** Eco mode: skip FFT / spectrum CPU. Dynamic (D) / Spectral (S) DSP stay available. */
    void setEcoMode (bool shouldEnable) noexcept { ecoMode.store (shouldEnable); }
    bool isEcoMode() const noexcept { return ecoMode.load(); }

    // ========================================================================
    /** True when a new analysed frame is ready for the UI (post and/or pre). */
    void setNextFFTBlockStatus (bool);
    bool getNextFFTBlockStatus();

    bool getNextPreFFTBlockStatus();
    void setNextPreFFTBlockStatus (bool);

    // ========================================================================
    size_t getScopeSize();
    size_t getFftSize() const;
    float getScopeData (size_t);
    float getScopePreData (size_t);
    float getScopeMaximumsData (size_t);
    float getScopeSecondaryData (size_t);
    float getScopeSecondaryPreData (size_t);
    float getScopeSecondaryMaximumsData (size_t);
    Channels getActiveChannel() const noexcept { return m_activeChannels.load(); }
    bool isOverlayChannel() const noexcept { return SpectrumAnalysis::isOverlay (m_activeChannels.load()); }
    float getOffset();
    void setSampleRate (double sampleRate);
    double getSampleRate() const;

    /** Display-range min/max used to map scope 0..1 ↔ dB (same as spectrum paint). */
    void getVolumeRangeInDecibels (float& minDb, float& maxDb) noexcept;
    /** Scope bin as absolute level in dB (post-EQ analyser path). */
    float getScopeDataDb (size_t index);
    /** Scope bin as absolute level in dB (pre-EQ analyser path). */
    float getScopePreDataDb (size_t index);
    /**
        Copy scope levels in dB into dest[0..n) via display scope (message-thread safe).
        Uses only m_scopeLock — never the analysis-thread lock (avoids host freezes).
        @param pre  true = pre-EQ, false = post-EQ
        @return number of bins written (0 if empty)
    */
    int copyScopeDataDb (bool pre, float* destDb, int maxBins);

    /**
        Peak bin level in dB for silence / activity checks (display scope).
        Returns a very low value if no data.
    */
    float getScopePeakDb (bool pre) noexcept;

    /** BLOCK_ID menu index: 0=2048, 1=4096, 2=8192, 3=16384. */
    void setFFTBlockSize (int);

    /** Display axis matches EQ FrequencyResponseComponent: 20 Hz … min(20 kHz, Nyquist). */
    static constexpr float kMinDisplayFrequencyHz = 20.0f;
    static constexpr float kMaxDisplayFrequencyHz = 20000.0f;

    float getBinFrequencyHz (size_t bin) const;
    float getDisplayMaxFrequencyHz() const;
    /** Bin → x in [0,1] on the display axis. Values outside (0,1) are off-screen. */
    float getBinDisplayXNorm (int binIndex, bool logarithmic) const;
    /** Display x in [0,1] → Hz on the same axis GraphLine / EQ use. */
    float getFrequencyForDisplayXNorm (float xNorm, bool logarithmic) const;
    /** Continuous bin index for a frequency (fftSize * freq / sampleRate). */
    float getFractionalBinForFrequencyHz (float freqHz) const;
    /** Highest scope bin whose centre frequency is still on-axis (≤ display max). */
    int getHighestDisplayBinIndex() const;

    // ========================================================================
    void pushSamplesIntoFifo (float, float) noexcept;
    void pushSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept;
    void pushPreSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept;

    /** Message-thread: apply dynamic range host updates after a new frame, then clear ready flags. */
    void calculateNextFrameOfSpectrum();

    void processFFT();
    void processPreFFT();
    void calculateCurrentVolumes();
    void calculateDynamicVolumes();
    void calculateMaximumVolumes();
    void calculateCurrentAverageVolume();
    void calculatePreScopeFromFft();

    float getModifiedScopeData (size_t index);

private:
    static constexpr size_t kRingCapacity = 16384; // max BLOCK_ID
    static const size_t m_avgFifoMaximumSize { 16 };

    float adaptData (float volume, float minDb, float maxDb) const;

    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void run() override;

    void setActiveChannel (Channels);
    void setAvg (size_t);
    void setVolumeScaleModeAsDynamic (bool);
    void setVolumeRangeInDecibels (float, float);
    void setRefreshMs (int);

    void analyseLatestWindow();
    void pushIntoStereoRings (std::array<float, kRingCapacity>& ringL,
                              std::array<float, kRingCapacity>& ringR,
                              std::atomic<size_t>& writePos,
                              std::atomic<size_t>& filled,
                              const float* left,
                              const float* right,
                              int numSamples) noexcept;
    void mixTimeDomain (const float* left,
                        const float* right,
                        float* dest,
                        size_t numSamples,
                        Channels mix) const noexcept;
    void performFrequencyTransform (float* data);
    void applyOctaveSmoothing (std::vector<float>& db);
    void calculateVolumesFromFft (const std::vector<float>& fftData,
                                  std::array<std::vector<float>, m_avgFifoMaximumSize>& avgData);
    void calculateAveragedVolumes (const std::array<std::vector<float>, m_avgFifoMaximumSize>& avgData,
                                   std::vector<float>& destDb);
    void calculateHoldVolumes (const std::vector<float>& dynamicDb,
                               std::vector<float>& holdDb,
                               std::atomic<float>* peakOut);
    void publishAdaptedPair (const std::vector<float>& srcDb, std::vector<float>& destNorm,
                             float minDb, float maxDb);
    Channels secondaryMixFor (Channels channel) const noexcept;
    Channels primaryMixFor (Channels channel) const noexcept;
    void applyChannelFromChoiceIndex (int index);
    void readChannelParamsFromTree();
    void readOctaveSmoothFromTree();
    bool copyLatestFromRing (const std::array<float, kRingCapacity>& ring,
                             size_t writePos,
                             size_t filled,
                             float* dest,
                             size_t fftSize) const;
    void publishAdaptedScopes();
    void applyDynamicRangeToHost();
    bool isSpectrumAnalyserParamOn() const;

    juce::AudioProcessorValueTreeState& mr_valueTree;

    juce::dsp::FFT m_forwardFFT_9;
    juce::dsp::FFT m_forwardFFT_10;
    juce::dsp::FFT m_forwardFFT_11;
    juce::dsp::FFT m_forwardFFT_12;
    juce::dsp::FFT m_forwardFFT_13;
    juce::dsp::FFT m_forwardFFT_14;

    juce::dsp::WindowingFunction<float> m_window_512;
    juce::dsp::WindowingFunction<float> m_window_1024;
    juce::dsp::WindowingFunction<float> m_window_2048;
    juce::dsp::WindowingFunction<float> m_window_4096;
    juce::dsp::WindowingFunction<float> m_window_8192;
    juce::dsp::WindowingFunction<float> m_window_16384;

    std::atomic<size_t> m_fftOrder { 11 };   // 2048
    std::atomic<size_t> m_fftSize { 2048 };
    std::atomic<size_t> m_scopeSize { 1024 };

    std::atomic<Channels> m_activeChannels { Channels::both };
    std::atomic<float> m_octaveSmoothOctaves { 0.0f };
    std::atomic<bool> m_syncingChannelParams { false };

    std::atomic<bool> m_blockSizeDefined { false };
    std::atomic<bool> m_nextFFTBlockReady { false };
    std::atomic<bool> m_preNextFFTBlockReady { false };
    std::atomic<bool> m_volumeRangeIsDynamamic { false };
    std::atomic<bool> m_frequencyIsLogarithmic { true };
    std::atomic<bool> ecoMode { false };
    std::atomic<bool> m_dynamicRangeNeedsHostUpdate { false };
    std::atomic<int> m_refreshMs { 33 }; // ~30 Hz

    std::array<float, kRingCapacity> m_ringL {};
    std::array<float, kRingCapacity> m_ringR {};
    std::atomic<size_t> m_ringWritePos { 0 };
    std::atomic<size_t> m_ringFilled { 0 };

    std::array<float, kRingCapacity> m_preRingL {};
    std::array<float, kRingCapacity> m_preRingR {};
    std::atomic<size_t> m_preRingWritePos { 0 };
    std::atomic<size_t> m_preRingFilled { 0 };

    std::vector<float> m_timeL;
    std::vector<float> m_timeR;
    std::vector<float> m_fftData;
    std::vector<float> m_fftDataB;
    std::vector<float> m_preFftData;
    std::vector<float> m_preFftDataB;
    std::vector<float> m_volumsDynamicData;
    std::vector<float> m_volumsDynamicDataB;
    std::vector<float> m_volumsMaximumData;
    std::vector<float> m_volumsMaximumDataB;
    std::vector<float> m_volumsPreData;
    std::vector<float> m_volumsPreDataB;
    std::vector<float> m_scopeDynamicData;
    std::vector<float> m_scopeDynamicDataB;
    std::vector<float> m_scopeMaximumData;
    std::vector<float> m_scopeMaximumDataB;
    std::vector<float> m_scopePreData;
    std::vector<float> m_scopePreDataB;
    std::vector<float> m_octaveSmoothScratch;
    std::vector<double> m_octavePrefix;

    std::atomic<size_t> m_avgFactor { 1 };
    std::atomic<size_t> m_avgFifoSize { 0 };
    std::atomic<size_t> m_avgFifoWritePointer { 0 };
    size_t              m_avgFifoReadPointer { 0 };

    std::array<std::vector<float>, m_avgFifoMaximumSize> m_avgData;
    std::array<std::vector<float>, m_avgFifoMaximumSize> m_avgDataB;

    juce::CriticalSection m_volumeRangeChange;
    juce::CriticalSection m_analysisLock;
    juce::CriticalSection m_scopeLock;

    float m_maximumVolumeInDecibels { 12.0f };
    float m_minimumVolumeInDecibels { -120.0f };

    std::atomic<float> m_offset { 1.0f };
    std::atomic<double> m_sampleRate { 48000.0 };
    std::atomic<float> m_currentMaximumVolumeInDecibels;
    std::atomic<float> m_currentAverageVolumeInDecibels;

    double m_lastMaxHoldUpdateMs { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Analyser)
};
