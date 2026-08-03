#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <functional>
#include <memory>
#include <vector>
#include "Menu/SharedResources.h"
#include "ColourRamp/GradientRamp.h"
#include "MelatoninBlur/melatonin/shadows.h"

/**
    Stereogram: Y = log frequency (low at bottom), X = signed L/R balance.
    Main + longer LF aux FFT (spectrogram-style) for low-frequency detail.
*/
class StereogramComponent : public juce::Component,
                            private juce::Timer
{
public:
    StereogramComponent();
    ~StereogramComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    void pushSamples (const float* left, const float* right, int numSamples) noexcept;
    void prepare (double sampleRate);
    void setEnabled (bool shouldEnable) noexcept;
    bool isScopeEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }

    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }
    void setColourRamp (const GradientRamp& ramp);
    void clearColourRamp();

    std::function<void()> onShowContextMenu;
    std::function<void()> onDoubleClick;

    static constexpr float kMinDisplayHz = 20.0f;
    static constexpr float kMaxDisplayHz = 20000.0f;

private:
    void timerCallback() override;
    void ensureRing (int minCapacity);
    void ensureLfFft();
    void processFromRing();
    void runChannelFft (const float* srcRing, int cap, int endPos, int size,
                        juce::dsp::FFT& fftEng,
                        juce::dsp::WindowingFunction<float>& win,
                        std::vector<float>& work,
                        std::vector<float>& magOut);
    void ensureTrailImage (int width, int height);
    void fadeAndPlotTrail();
    void buildGlowPath (juce::Path& outPath) const;
    void paintFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> plot) const;
    juce::Rectangle<int> getPlotBounds() const noexcept;
    float loadFloatParam (const char* id, float fallback) const;
    bool loadBoolParam (const char* id, bool fallback) const;
    float freqForDisplayT (float t01) const noexcept;
    float displayTForFreq (float hz) const noexcept;
    void sampleBalanceEnergyAtFreq (float hz, float& balanceOut, float& energyOut) const noexcept;

    static constexpr int kMainOrder = 11; // 2048
    static constexpr int kMainSize = 1 << kMainOrder;
    static constexpr int kLfBoost = 2; // 4× → 8192
    static constexpr int kTimerHz = 30;
    static constexpr float kCrossoverHz = 350.0f;

    std::atomic<bool> enabled { false };
    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;

    // Audio ring (holds longest analysis window).
    std::vector<float> ringL, ringR;
    std::atomic<int> writePos { 0 };
    std::atomic<int> capacity { 0 };

    juce::dsp::FFT mainFft { kMainOrder };
    juce::dsp::WindowingFunction<float> mainWindow { (size_t) kMainSize,
                                                     juce::dsp::WindowingFunction<float>::hann };
    std::vector<float> mainWorkL, mainWorkR;
    std::vector<float> mainMagL, mainMagR;

    std::unique_ptr<juce::dsp::FFT> lfFft;
    std::unique_ptr<juce::dsp::WindowingFunction<float>> lfWindow;
    int lfOrder = 0;
    int lfSize = 0;
    std::vector<float> lfWorkL, lfWorkR;
    std::vector<float> lfMagL, lfMagR;

    /** Display-resolution maps (main bin count) after LF/main blend. */
    std::vector<float> balanceByBin;
    std::vector<float> energyByBin;
    /** Raw LF-resolution maps for sampling below crossover. */
    std::vector<float> balanceLf;
    std::vector<float> energyLf;

    juce::CriticalSection lock;
    GradientRamp colourRamp;
    bool hasCustomRamp = false;
    int lfFrameCounter = 0;

    juce::Image trailImage;
    juce::Path lastGlowPath;
    melatonin::DropShadow plotGlow {
        {
            { juce::Colour::fromRGBA (120, 200, 255, 90), 14, { 0, 0 }, 2 },
            { juce::Colour::fromRGBA (180, 230, 255, 140), 5, { 0, 0 }, 0 }
        }
    };
    double sampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereogramComponent)
};
