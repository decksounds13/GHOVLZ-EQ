#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <functional>
#include "Menu/SharedResources.h"

/**
    ITU-R BS.1770-style loudness meter (K-weighting).
    Shows Momentary / Short-term / Integrated LUFS with bold readouts
    and a three-bar meter to the right.
*/
class LoudnessComponent : public juce::Component,
                          private juce::Timer
{
public:
    LoudnessComponent();
    ~LoudnessComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    void pushSamples (const float* left, const float* right, int numSamples) noexcept;
    void prepare (double sampleRate);
    void setEnabled (bool shouldEnable) noexcept;
    bool isScopeEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

    void resetIntegrated() noexcept;

    std::function<void()> onShowContextMenu;

private:
    void timerCallback() override;
    void showContextMenu();
    float loadFloatParam (const char* id, float fallback) const;
    int loadIntParam (const char* id, int fallback) const;

    struct KWeightState
    {
        float z1a = 0.0f, z2a = 0.0f;
        float z1b = 0.0f, z2b = 0.0f;
    };

    float processKWeight (float x, KWeightState& s) const noexcept;
    void updateKCoeffs (double sr);
    static float lufsToMeterNorm (float lufs, float minLufs, float maxLufs) noexcept;

    std::atomic<bool> enabled { false };
    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;

    double sampleRate = 48000.0;
    float b0a = 1, b1a = 0, b2a = 0, a1a = 0, a2a = 0;
    float b0b = 1, b1b = 0, b2b = 0, a1b = 0, a2b = 0;
    KWeightState stateL, stateR;

    double momSum = 0.0, shortSum = 0.0, integSum = 0.0;
    int momCount = 0, shortCount = 0, integCount = 0;
    int momWindow = 19200;
    int shortWindow = 144000;

    std::atomic<float> momentaryLufs { -70.0f };
    std::atomic<float> shortTermLufs { -70.0f };
    std::atomic<float> integratedLufs { -70.0f };

    juce::CriticalSection lock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessComponent)
};
