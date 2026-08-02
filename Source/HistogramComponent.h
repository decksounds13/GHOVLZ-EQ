#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <vector>
#include <functional>
#include "ColourRamp/GradientRamp.h"
#include "Menu/SharedResources.h"

/**
    Scope loudness histogram (Youlean-style averages):
    Integrated LUFS, long-window RMS, and True Peak over time.
*/
class HistogramComponent : public juce::Component,
                           private juce::Timer
{
public:
    HistogramComponent();
    ~HistogramComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    void pushSamples (const float* left, const float* right, int numSamples) noexcept;
    void prepare (double sampleRate);
    void setEnabled (bool shouldEnable) noexcept;
    bool isScopeEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

    void setColourRamp (const GradientRamp& ramp);
    void clearColourRamp();
    void resetIntegrated() noexcept;

    std::function<void()> onShowContextMenu;

private:
    void timerCallback() override;
    void showContextMenu();
    float loadFloatParam (const char* id, float fallback) const;
    bool loadBoolParam (const char* id, bool fallback) const;

    void updateKCoeffs (double sr);
    float processKWeight (float x, float& z1a, float& z2a, float& z1b, float& z2b) const noexcept;
    void appendHistory (float lufs, float rmsDb, float truePeakDb);
    void ensureHistorySize (int width);
    float levelToY (float levelDb, juce::Rectangle<float> plot) const noexcept;
    juce::Colour seriesColour (int series, float levelNorm) const;

    void paintSeries (juce::Graphics& g,
                      juce::Rectangle<float> plot,
                      const std::vector<float>& history,
                      juce::Colour lineCol,
                      float lineWidth,
                      float fillOpacity,
                      bool glowEnabled,
                      float glowOpacity,
                      float glowRadius,
                      float glowSpread) const;

    void paintScale (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour textCol) const;

    std::atomic<bool> enabled { false };
    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;
    GradientRamp colourRamp;
    bool hasCustomRamp = false;

    double sampleRate = 48000.0;

    // K-weight → Integrated LUFS (program average) + long-window RMS
    float b0a = 1, b1a = 0, b2a = 0, a1a = 0, a2a = 0;
    float b0b = 1, b1b = 0, b2b = 0, a1b = 0, a2b = 0;
    float z1aL = 0, z2aL = 0, z1bL = 0, z2bL = 0;
    float z1aR = 0, z2aR = 0, z1bR = 0, z2bR = 0;

    double integSum = 0.0;
    int integCount = 0;
    std::atomic<float> integratedLufs { -70.0f };

    /** ~3 s exponential RMS (track-loudness timescale, not block peaks). */
    double rmsMeanSquare = 0.0;
    double rmsTauSec = 3.0;
    std::atomic<float> averageRmsDb { -100.0f };

    /** Max true-peak within the current history column interval. */
    std::atomic<float> columnTruePeakDb { -100.0f };
    float tpHistL[3] { 0, 0, 0 };
    float tpHistR[3] { 0, 0, 0 };
    float columnTpMax = -100.0f;

    struct TpOverMarker
    {
        int column = 0;   // index into history (0 = oldest / left)
        float overDb = 0.0f;
    };

    juce::CriticalSection historyLock;
    std::vector<float> histLufs;
    std::vector<float> histRms;
    std::vector<float> histTruePeak;
    std::vector<TpOverMarker> tpOverMarkers;
    double sampleAccum = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistogramComponent)
};
