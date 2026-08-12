#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <functional>
#include <vector>
#include "Menu/SharedResources.h"
#include "ColourRamp/GradientRamp.h"
#include "MelatoninBlur/melatonin/cached_blur.h"
#include "SpectrogramReassignment.h"

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
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    std::function<void()> onShowContextMenu;
    std::function<void()> onDoubleClick;

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
    /** Independent colour ramp for the 3D heightfield (does not affect 2D Spec). */
    void setCustomColourRamp3D (const GradientRamp* ramp) noexcept;

    /**
        Snapshot history for 3D mesh (message thread). Out sizes are display history W/H;
        column-major dB. Uses the 3D enhanced/classic buffer when it differs from 2D.
    */
    void getHistorySnapshot (std::vector<float>& outColumnMajorDb, int& outW, int& outH,
                             float& outBrightness, float& outMinDb, float& outMaxDb) const;
    /** Monotonic count of appended history columns (for stable 3D scroll). */
    uint64_t getHistoryColumnSerial() const noexcept
    {
        return historyColumnSerial.load (std::memory_order_relaxed);
    }
    /** Refresh 3D colour LUT (message thread) before sampling mesh colours. */
    void refreshColourLutFor3D() { rebuildColourLut3D(); }
    /** Map dB → 2D LUT colour using current Spec brightness / range (message thread). */
    juce::Colour colourFromHistoryDb (float db, float brightness, float minDb, float maxDb) const;
    /** Map dB → 3D LUT colour (independent ramp / scheme fallback). */
    juce::Colour colourFromHistoryDb3D (float db, float brightness, float minDb, float maxDb) const;

    double getDisplaySampleRate() const noexcept
    {
        return sampleRateHz.load (std::memory_order_relaxed);
    }
    bool isLogFrequencyAxis() const { return loadBoolParam ("SPEC_LOG_FREQ_ID", true); }

    static constexpr int kWindowHeightPx = 80;
    /** Preferred compact width relative to a typical oscilloscope strip. */
    static constexpr int kPreferredWidthPx = 160;
    static constexpr float kMinDisplayHz = 20.0f;
    static constexpr float kMaxDisplayHz = 20000.0f;

    static juce::StringArray getColourSchemeNames();
    /** Sample a built-in scheme LUT colour at t in [0,1] (for UI swatches / colourise). */
    static juce::Colour colourForScheme (ColourScheme scheme, float t) noexcept;
    /** 2048 / 4096 / 8192 / 16384 — default index 2 = 8192. */
    static juce::StringArray getFftSizeNames();
    /** Draft / Normal / High / Ultra display pixel density. */
    static juce::StringArray getDisplayResNames();
    /** Enhanced LF multi-res: Off / 2× / 4× — default index 2 = 4×. */
    static juce::StringArray getEnhancedLfDetailNames();

private:
    void timerCallback() override;
    void resetDisplay();
    void ensureScratchImage();
    void ensureFft (int order);
    /** Aux analysis window for Enhanced Frequency (orderBoost 1 = 2×, 2 = 4×). */
    void ensureAuxFft (int mainOrder, int orderBoost,
                       std::unique_ptr<juce::dsp::FFT>& auxFft,
                       std::unique_ptr<juce::dsp::WindowingFunction<float>>& auxWindow,
                       int& auxOrder, int& auxSize,
                       std::vector<float>& auxWork, std::vector<float>& auxWindowed,
                       std::vector<float>& auxColumnDb, std::vector<float>& auxPrevPhase,
                       bool& auxHavePrev, int& auxBinsCached);
    void ensureBinForRowMap();
    void ensureAuxBinForRowMap (int auxFftSize, int& auxBinsCached, std::vector<float>& auxBinForRow);
    void advanceFromRing();
    void appendColumn (const float* magnitudesDb, int numBins);
    /** Deposit a finished display-row dB column (Enhanced Frequency path). */
    void appendDisplayColumn (const float* displayDbRows);
    /** Scroll-only 3D history column (no 2D image update) when 2D/3D enhanced modes differ. */
    void appendHistory3DColumn (const float* displayDbRows);
    void buildClassicDisplayColumn (const float* magnitudesDb, int numBins, std::vector<float>& outRows);
    /** Scatter energy at reassigned IF into log rows; optional previous-column time reassignment.
        Returns true if the previous history column was written. */
    bool depositEnhanced (float* columnRows, float* prevColumnRows,
                          float ifHz, float db, double sr, bool logFreq,
                          float timeOffsetSamples, float hopSamples) const;
    void rebuildColourLut();
    void rebuildColourLut3D();
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
    static constexpr int kMaxAuxFftOrder = 15; // 32768
    static constexpr float kEnhancedMidHiHz = 2000.0f;

    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;
    const GradientRamp* customColourRamp = nullptr;
    uint32_t customRampRevision = 0;
    const GradientRamp* customColourRamp3D = nullptr;
    uint32_t customRampRevision3D = 0;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> window;
    int fftOrder = 0;
    int fftSize = 0;

    /** Enhanced-Frequency LF (up to 4×) and Mid (2×) analysis windows. */
    std::unique_ptr<juce::dsp::FFT> lfFft, midFft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> lfWindow, midWindow;
    int lfFftOrder = 0, midFftOrder = 0;
    int lfFftSize = 0, midFftSize = 0;

    std::vector<float> ringL;
    std::vector<float> ringR;
    std::atomic<int> writePos { 0 };
    std::atomic<int> capacity { 0 };
    std::atomic<bool> enabled { false };
    std::atomic<double> sampleRateHz { 48000.0 };

    std::vector<float> fftWork;
    std::vector<float> windowed;
    std::vector<float> columnDb;
    std::vector<float> lfFftWork, midFftWork;
    std::vector<float> lfWindowed, midWindowed;
    std::vector<float> columnDbLf, columnDbMid;
    /** Fractional FFT bin per display row (for interpolated magnitude). */
    std::vector<float> binForRow;
    std::vector<float> binForRowLf, binForRowMid;
    /** Legacy phase storage (reset on mode change; Auger path does not use hop phase). */
    std::vector<float> prevPhase;
    std::vector<float> prevPhaseLf, prevPhaseMid;
    bool havePrevPhase = false;
    bool havePrevPhaseLf = false, havePrevPhaseMid = false;
    bool lastEnhancedMode = false;
    int lastHopSamples = 0;
    int lfFftSizeCachedForBins = 0, midFftSizeCachedForBins = 0;
    int enhancedLfFrameCounter = 0;

    /** Classical Auger–Flandrin reassignment (shared main / LF / mid sizes). */
    SpectrogramReassignment reassignment;
    std::vector<float> reassignWorkT;
    std::vector<float> reassignWorkD;
    std::vector<float> reassignRaw;

    int internalW = 1280;
    int internalH = 720;

    /** Left-scrolling spectrogram image; display size is independent of component bounds. */
    juce::Image scrollImage;
    /** Component-sized buffer: upscaled scroll + screen-space soften ( Melatonin stack blur ). */
    juce::Image screenImage;
    melatonin::CachedBlur screenBlur { 2 };
    std::vector<float> columnScratch; // vertical soften before colourise
    std::vector<float> columnSoftTmp;
    std::vector<float> columnClassic; // classic STFT rows when dual 2D/3D enhanced modes
    /** Display-row dB history (column-major, internalW * internalH) for full-strip recolour. */
    std::vector<float> historyDb;
    /** Parallel history when 3D enhanced mode differs from 2D. */
    std::vector<float> historyDb3D;
    std::atomic<uint64_t> historyColumnSerial { 0 };
    bool lastEnhanced2D = false;
    bool lastEnhanced3D = false;
    uint32_t lastLookFingerprint = 0;

    std::array<juce::PixelARGB, kLutSize> colourLut {};
    std::array<juce::PixelARGB, kLutSize> colourLut3D {};
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
