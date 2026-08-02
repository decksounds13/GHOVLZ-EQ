#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <vector>
#include "Menu/SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "ColourRamp/GradientRamp.h"

/**
 * Stereo goniometer (Lissajous / vectorscope) with a vertical correlation meter.
 *
 * Display is rotated 45° so mono (L=R) is vertical, out-of-phase (L=-R) is
 * horizontal, and L-only / R-only fall on the diagonals. Correlation runs from
 * +1 (top) through 0 (centre) to -1 (bottom).
 */
class GoniometerComponent : public juce::Component,
                            private juce::Timer
{
public:
    GoniometerComponent();
    ~GoniometerComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Audio-thread safe: push latest stereo block when enabled. */
    void pushSamples (const float* left, const float* right, int numSamples) noexcept;

    void prepare (double sampleRate);
    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setEnabled (bool shouldEnable) noexcept;
    bool isGoniometerEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    /** Full-graph overlay: clicks pass through to EQ handles underneath. */
    void setExpanded (bool shouldExpand) noexcept;
    bool isExpanded() const noexcept { return expanded; }

    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

    void setColourRamp (const GradientRamp& ramp);
    void clearColourRamp();

    void mouseDown (const juce::MouseEvent& e) override;
    std::function<void()> onShowContextMenu;

    /** Same compact chrome height as OscilloscopeComponent. */
    static constexpr int kWindowHeightPx = 80;
    /** Slim correlation strip beside the square plot. */
    static constexpr int kCorrelationWidthPx = 16;
    /** Compact footprint: square plot + gap + correlation strip. */
    static constexpr int kCompactWidthPx = kWindowHeightPx + 4 + kCorrelationWidthPx;

private:
    void timerCallback() override;
    void resetDisplay();
    void ensureTrailImage (int plotSize);
    void fadeAndPlotTrail();
    void updateCorrelationFromRing();
    /** Filled ellipses at sample points (Melatonin blurs fills; stroked Lissajous paths do not). */
    void buildGlowPath (juce::Path& outPath, float pointRadius) const;
    float loadFloatParam (const char* id, float fallback) const;
    bool isHighQuality() const;

    const SharedColors& colors() const noexcept;

    juce::Rectangle<int> getPlotBounds() const noexcept;
    juce::Rectangle<int> getCorrelationBounds() const noexcept;

    static constexpr int kMaxBufferSeconds = 2;
    static constexpr int kMaxPlotSamplesPerTick = 2048;

    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;

    melatonin::DropShadow plotGlow {
        {
            { juce::Colour::fromRGBA (220, 190, 120, 90), 14, { 0, 0 }, 2 },
            { juce::Colour::fromRGBA (255, 230, 160, 140), 5, { 0, 0 }, 0 }
        }
    };

    // Audio-thread rings.
    std::vector<float> ringL;
    std::vector<float> ringR;
    std::atomic<int> writePos { 0 };
    std::atomic<int> capacity { 0 };
    std::atomic<bool> enabled { false };
    std::atomic<double> sampleRateHz { 48000.0 };

    // Message-thread correlation accumulators (smoothed).
    double corrSumLR = 0.0;
    double corrSumLL = 0.0;
    double corrSumRR = 0.0;
    float correlation = 0.0f;
    float correlationDisplay = 0.0f;

    int ringReadPos = 0;
    bool expanded = false;

    juce::Image trailImage;
    juce::Path lastGlowPath;
    GradientRamp colourRamp;
    bool hasCustomRamp = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoniometerComponent)
};
