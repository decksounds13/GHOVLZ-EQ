#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include "Menu/SharedResources.h"

/** Compact scrolling spectrogram strip for the top chrome (dice ↔ preset bar). */
class SpectrogramComponent : public juce::Component,
                             private juce::Timer
{
public:
    enum class ChannelMode
    {
        summedStereo = 0,
        left,
        right
    };

    enum class ColourScheme
    {
        classic = 0,
        inferno,
        magma,
        viridis,
        ice,
        greyscale,
        heat,
        numSchemes
    };

    SpectrogramComponent();
    ~SpectrogramComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Audio-thread safe: push latest block when enabled. */
    void pushSamples (const float* left, const float* right, int numSamples) noexcept;

    void prepare (double sampleRate);
    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setEnabled (bool shouldEnable) noexcept;
    bool isSpectrogramEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    /** Full-graph overlay mode (clicks pass through), same convention as osc/gon. */
    void setExpanded (bool shouldExpand) noexcept;
    bool isExpanded() const noexcept { return expanded; }

    /** Nudge SPEC_SPEED_ID — Plus = faster scroll, Minus = slower. */
    void speedUp();
    void speedDown();
    float getSpeed() const;

    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

    static constexpr int kWindowHeightPx = 80;
    /** Preferred compact width relative to a typical oscilloscope strip. */
    static constexpr int kPreferredWidthPx = 160;
    static constexpr float kMinDisplayHz = 20.0f;
    static constexpr float kMaxDisplayHz = 20000.0f;

    static juce::StringArray getColourSchemeNames();

private:
    void timerCallback() override;
    void resetDisplay();
    void ensureDisplaySize (int widthPx, int heightPx);
    void ensureFft (int order);
    void advanceFromRing();
    void writeColumn (const float* magnitudesDb, int numBins);
    juce::Colour mapMagnitude (float norm01) const;
    ColourScheme currentScheme() const;
    ChannelMode currentChannelMode() const;
    int currentFftOrder() const;
    float loadFloatParam (const char* id, float fallback) const;
    int loadChoiceIndex (const char* id, int fallback) const;
    bool loadBoolParam (const char* id, bool fallback) const;
    const SharedColors& colors() const noexcept;

    static constexpr int kMaxBufferSeconds = 4;

    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    int fftOrder = 0;
    int fftSize = 0;

    std::vector<float> ringL;
    std::vector<float> ringR;
    std::atomic<int> writePos { 0 };
    std::atomic<int> capacity { 0 };
    std::atomic<bool> enabled { false };
    std::atomic<double> sampleRateHz { 48000.0 };

    std::vector<float> fftWork;
    std::vector<float> windowed;
    std::vector<float> columnDb;

    /** Row-major: column-major storage [col * height + y], values in 0..1. */
    std::vector<float> history;
    int historyW = 0;
    int historyH = 0;
    int writeCol = 0;
    int ringReadPos = 0;
    int samplesUntilHop = 0;
    bool freeze = false;
    bool expanded = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramComponent)
};
