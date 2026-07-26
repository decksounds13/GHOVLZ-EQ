#pragma once
#include <JuceHeader.h>

class Spectrogram : public juce::Component, private juce::Timer
{
public:
    Spectrogram();
    ~Spectrogram();

    void prepareToPlay(int samplesPerBlockExpected, double newSampleRate);
    void releaseResources();
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill);
    void paint(juce::Graphics& g);
    void timerCallback();

private:
    void pushNextSampleIntoFifo(float sample) noexcept;
    void drawNextLineOfSpectrogramWithStreaking();

    enum
    {
        fftOrder = 10,
        fftSize = 1 << fftOrder
    };

    juce::dsp::FFT forwardFFT;
    juce::Image spectrogramImage;
    float fifo[fftSize];
    float fftData[2 * fftSize];
    int fifoIndex = 0;
    bool nextFFTBlockReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Spectrogram)
};
