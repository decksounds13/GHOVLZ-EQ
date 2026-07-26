#pragma once

#include <JuceHeader.h>

using ValueTree = juce::AudioProcessorValueTreeState;

// ****************************************************************************
// ANALYSER CLASS
// ****************************************************************************
class Analyser : public juce::AudioProcessorValueTreeState::Listener
{
public:
    enum class Channels { left, right, both };

    // Constructor
    Analyser(juce::AudioProcessorValueTreeState& audioProcessorValueTreeState);
    ~Analyser();

    // ========================================================================
    void resetScopeMaximumsData();


    /** Eco mode: skip FFT / spectrum CPU. Dynamic (D) / Spectral (S) DSP stay available. */
    void setEcoMode (bool shouldEnable) noexcept { ecoMode.store (shouldEnable); }
    bool isEcoMode() const noexcept { return ecoMode.load(); }

    // ========================================================================
    void setNextFFTBlockStatus(const bool);
    bool getNextFFTBlockStatus();


    // ========================================================================
    size_t getScopeSize();
    size_t getFftSize() const;
    float getScopeData(size_t);
    float getScopePreData (size_t);
    float getScopeMaximumsData(size_t);
    float getOffset();
    void setSampleRate (double sampleRate);
    double getSampleRate() const;

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
    void pushSamplesIntoFifo(const float, const float) noexcept;
    void pushSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept;
    void pushPreSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept;
    void calculateNextFrameOfSpectrum();
    bool getNextPreFFTBlockStatus();
    void setNextPreFFTBlockStatus (bool);

    void processFFT();
    void processPreFFT();
    void calculateCurrentVolumes();
    void calculateDynamicVolumes();
    void calculateMaximumVolumes();
    void calculateCurrentAverageVolume();
    void calculatePreScopeFromFft();

    float getModifiedScopeData(size_t index);
private:
    // ========================================================================

    float adaptData (float volume, float minDb, float maxDb) const;


    // ========================================================================
    void parameterChanged(const juce::String& parameterID, float newValue) override;



    // ========================================================================
    void setFFTBlockSize(int);
    void setActiveChannel(Channels);
    void setAvg(size_t);
    void setVolumeScaleModeAsDynamic(bool);
    void setVolumeRangeInDecibels(float, float);


    // ========================================================================
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
    std::atomic<size_t> m_fifoIndex { 0 };

    std::atomic<Channels> m_activeChannels{ Channels::both };

    std::atomic<bool> m_blockSizeDefined{ false };
    std::atomic<bool> m_nextFFTBlockReady{ false };
    std::atomic<bool> m_preNextFFTBlockReady { false };
    std::atomic<bool> m_volumeRangeIsDynamamic{ false };
    std::atomic<bool> m_frequencyIsLogarithmic{ true };
    std::atomic<bool> ecoMode { false };

    std::vector<float> m_fifo;
    std::vector<float> m_fftData;
    std::atomic<size_t> m_preFifoIndex { 0 };
    std::vector<float> m_preFifo;
    std::vector<float> m_preFftData;
    std::vector<float> m_volumsDynamicData;
    std::vector<float> m_volumsMaximumData;
    std::vector<float> m_volumsPreData;
    std::vector<float> m_scopeDynamicData;
    std::vector<float> m_scopeMaximumData;
    std::vector<float> m_scopePreData;

    std::atomic<size_t> m_avgFactor{ 1 };
    std::atomic<size_t> m_avgFifoSize{ 0 };
    std::atomic<size_t> m_avgFifoWritePointer{ 0 };
    size_t              m_avgFifoReadPointer;
    static const size_t m_avgFifoMaximumSize{ 16 };

    std::array<std::vector<float>, m_avgFifoMaximumSize> m_avgData;

    juce::CriticalSection m_volumeRangeChange;
    float m_maximumVolumeInDecibels{ 12.0f };
    float m_minimumVolumeInDecibels{ -120.0f };

    std::atomic<float> m_offset { 1.0f };
    std::atomic<double> m_sampleRate { 48000.0 };
    std::atomic<float> m_currentMaximumVolumeInDecibels;
    std::atomic<float> m_currentAverageVolumeInDecibels;

    double m_lastMaxHoldUpdateMs { 0.0 };

    // ========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Analyser)
};
