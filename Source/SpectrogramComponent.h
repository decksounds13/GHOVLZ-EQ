#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <vector>
#include "Menu/SharedResources.h"
#include "ColourRamp/GradientRamp.h"
#include "MelatoninBlur/melatonin/cached_blur.h"

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

    /** Expanded overlay: log/linear Hz grid matching the scroll image axis. */
    void paintFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> bounds) const;

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

    /** Optional custom LUT ramp (path-sampled). Null / disabled → scheme combo. */
    void setCustomColourRamp (const GradientRamp* ramp) noexcept;

    static constexpr int kWindowHeightPx = 80;
    /** Preferred compact width relative to a typical oscilloscope strip. */
    static constexpr int kPreferredWidthPx = 160;
    static constexpr float kMinDisplayHz = 20.0f;
    static constexpr float kMaxDisplayHz = 20000.0f;

    static juce::StringArray getColourSchemeNames();
    /** 2048 / 4096 / 8192 / 16384 — default index 2 = 8192. */
    static juce::StringArray getFftSizeNames();
    /** Draft / Normal / High / Ultra display pixel density. */
    static juce::StringArray getDisplayResNames();

private:
    void timerCallback() override;
    void resetDisplay();
    void ensureScratchImage();
    void ensureFft (int order);
    void advanceFromRing();
    void appendColumn (const float* magnitudesDb, int numBins);
    void rebuildColourLut();
    void resolveDisplaySize (int& outW, int& outH) const;
    void rebuildScreenSoftened();
    void softenColumnVertical (std::vector<float>& column, int numRows);
    void ensureHistoryBuffer();
    void colouriseColumnIntoImage (int x, const float* dbRows, float brightness,
                                   float minDb, float maxDb);
    void rerenderScrollFromHistory();
    uint32_t lookFingerprint() const;
    ColourScheme currentScheme() const;
    ChannelMode currentChannelMode() const;
    int currentFftOrder() const;
    float loadFloatParam (const char* id, float fallback) const;
    int loadChoiceIndex (const char* id, int fallback) const;
    bool loadBoolParam (const char* id, bool fallback) const;
    const SharedColors& colors() const noexcept;

    /** Scroll = dump columns; hop is time-based (not FFT-sized). Cap FFTs/tick for CPU. */
    static constexpr int kMaxBufferSeconds = 4;
    static constexpr int kMaxColumnsPerTick = 2;
    static constexpr int kLutSize = 256;

    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;
    const GradientRamp* customColourRamp = nullptr;
    uint32_t customRampRevision = 0;

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
    /** Fractional FFT bin per display row (for interpolated magnitude). */
    std::vector<float> binForRow;

    int internalW = 1280;
    int internalH = 720;

    /** Left-scrolling spectrogram image; display size is independent of component bounds. */
    juce::Image scrollImage;
    /** Component-sized buffer: upscaled scroll + screen-space soften ( Melatonin stack blur ). */
    juce::Image screenImage;
    melatonin::CachedBlur screenBlur { 2 };
    std::vector<float> columnScratch; // vertical soften before colourise
    std::vector<float> columnSoftTmp;
    /** Display-row dB history (column-major, internalW * internalH) for full-strip recolour. */
    std::vector<float> historyDb;
    uint32_t lastLookFingerprint = 0;

    std::array<juce::PixelARGB, kLutSize> colourLut {};
    ColourScheme lutScheme = ColourScheme::numSchemes;
    bool logFreqCached = true;
    int fftSizeCachedForBins = 0;
    double srCachedForBins = 0.0;
    int displayResCached = -1;

    int ringReadPos = 0;
    bool freeze = false;
    bool expanded = false;
    bool imageDirty = false;
    bool screenSoftDirty = true;
    int lastScreenBlurRadius = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramComponent)
};
