#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <vector>
#include "Menu/SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "ColourRamp/GradientRamp.h"

/** Compact beat-synced waveform strip for the graph chrome. */
class OscilloscopeComponent : public juce::Component,
                              private juce::Timer
{
public:
    enum class ChannelMode
    {
        summedStereo = 0, // ST — (L+R)/2
        splitStereo       // L/R — left top half, right bottom half
    };

    OscilloscopeComponent();
    ~OscilloscopeComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    /** Audio-thread safe: push latest block when enabled. */
    void pushSamples (const float* left, const float* right, int numSamples) noexcept;

    void prepare (double sampleRate);
    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setHostBpm (double bpm) noexcept;
    void setEnabled (bool shouldEnable) noexcept;
    bool isScopeEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    /** Zoom: 0 = 1 beat (most zoomed in) … larger = more beats on screen. */
    void zoomIn();
    void zoomOut();
    void setZoomIndex (int index) noexcept;
    int getZoomIndex() const noexcept { return zoomIndex.load (std::memory_order_relaxed); }
    juce::String getZoomLabel() const;

    void setChannelMode (ChannelMode mode) noexcept;
    ChannelMode getChannelMode() const noexcept { return channelMode; }
    void toggleChannelMode() noexcept;

    /** When false, new columns overwrite in place (no left scroll). */
    void setScrollMode (bool shouldScroll) noexcept;
    bool isScrollMode() const noexcept { return scrollMode; }

    /** Full-graph overlay mode: waveform only, clicks pass through. */
    void setExpanded (bool shouldExpand) noexcept;
    bool isExpanded() const noexcept { return expanded; }

    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

    void setColourRamp (const GradientRamp& ramp);
    void clearColourRamp();

    std::function<void()> onShowContextMenu;

    static constexpr int kWindowHeightPx = 80; // 60 * 4/3
    static constexpr int kWavePadPx = 2;

private:
    struct Column
    {
        float minSum = 0.0f, maxSum = 0.0f;
        float minL = 0.0f, maxL = 0.0f;
        float minR = 0.0f, maxR = 0.0f;
        /** Peak |sample| in column (0..1) for amplitude ramp mapping. */
        float peakAmp = 0.0f;
        /** Normalised zero-crossing rate (0..1) for frequency ramp mapping. */
        float freqNorm = 0.0f;
        bool valid = false;
    };

    void timerCallback() override;
    int getWindowLengthInSamples() const noexcept;
    float loadFloatParam (const char* id, float fallback) const;
    bool isHighQuality() const;
    void resetDisplay();
    void ensureDisplayWidth (int widthPx);
    void updateSamplesPerColumn();
    void advanceDisplayFromRing();
    void showContextMenu();
    void appendColumnStub (juce::Path& path, float px, float yMax, float yMin) const;

    /** High-quality continuous min/max envelopes (+ soft fill) for one plot lane. */
    void paintEnvelopeLane (juce::Graphics& g,
                            juce::Rectangle<float> plot,
                            float pathWidth,
                            float lineOpacity,
                            bool highQuality,
                            bool glowEnabled,
                            float glowOpacity,
                            float glowRadius,
                            float glowSpread,
                            bool useLeft,
                            bool useRight);

    void strokeWaveform (juce::Graphics& g, const juce::Path& waveform,
                         float pathWidth, float lineOpacity, bool highQuality,
                         bool glowEnabled, float glowOpacity, float glowRadius, float glowSpread);

    const SharedColors& colors() const noexcept;

    static constexpr int kMaxZoomIndex = 5; // 1, 2, 4, 8, 16, 32 beats
    static constexpr int kMaxBufferSeconds = 16;

    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;

    melatonin::DropShadow waveGlow {
        {
            { juce::Colour::fromRGBA (220, 190, 120, 90), 14, { 0, 0 }, 2 },
            { juce::Colour::fromRGBA (255, 230, 160, 140), 5, { 0, 0 }, 0 }
        }
    };

    // Audio-thread rings (L / R).
    std::vector<float> ringL;
    std::vector<float> ringR;
    std::atomic<int> writePos { 0 };
    std::atomic<int> capacity { 0 };
    std::atomic<bool> enabled { false };
    std::atomic<double> sampleRateHz { 48000.0 };
    std::atomic<double> hostBpm { 120.0 };
    std::atomic<int> zoomIndex { 0 }; // default: 1 beat

    // Message-thread display history.
    std::vector<Column> columns;
    int ringReadPos = 0;
    int samplesInColumn = 0;
    int samplesPerColumn = 1;
    float colMinSum = 1.0f, colMaxSum = -1.0f;
    float colMinL = 1.0f, colMaxL = -1.0f;
    float colMinR = 1.0f, colMaxR = -1.0f;
    float colPrevSum = 0.0f;
    bool colHavePrev = false;
    int colZeroCrossings = 0;
    int lastZoomIndex = -1;
    int lastWindowSamples = -1;
    int lastColumnCount = -1;
    int writeColumn = 0;
    bool scrollMode = true;
    bool expanded = false;
    ChannelMode channelMode = ChannelMode::summedStereo;
    GradientRamp colourRamp;
    bool hasCustomRamp = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OscilloscopeComponent)
};
