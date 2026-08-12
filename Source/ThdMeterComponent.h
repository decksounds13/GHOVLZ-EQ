#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <functional>
#include <vector>
#include "Menu/SharedResources.h"

/**
    Scope THD meter:
    - Broadband: spectral THD from locked fundamental + H2..Hn bars
    - Multiband: log-band residual map (non-peak energy / total) — useful on
      program; not classic lab THD per band
*/
class ThdMeterComponent : public juce::Component,
                          private juce::Timer
{
public:
    static constexpr int kMaxHarmonics = 8;
    static constexpr int kNumBands = 6;
    static constexpr int kFftOrder = 12; // 4096
    static constexpr int kFftSize = 1 << kFftOrder;

    enum class DisplayMode : int
    {
        broadband = 0,
        multiband = 1
    };

    ThdMeterComponent();
    ~ThdMeterComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    void pushSamples (const float* left, const float* right, int numSamples) noexcept;
    void prepare (double sampleRate);
    void setEnabled (bool shouldEnable) noexcept;
    bool isScopeEnabled() const noexcept { return enabled.load (std::memory_order_relaxed); }
    void setUiTimerRunning (bool shouldRun) noexcept;

    void setParameterTree (juce::AudioProcessorValueTreeState* tree) noexcept { valueTree = tree; }
    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

    std::function<void()> onShowContextMenu;
    std::function<void()> onDoubleClick;

private:
    void timerCallback() override;
    void showContextMenu();
    void analyseIfReady();
    void paintBroadband (juce::Graphics& g, juce::Rectangle<int> area,
                         juce::Colour text, juce::Colour muted,
                         juce::Colour accent, juce::Colour accentCool, bool locked);
    void paintMultiband (juce::Graphics& g, juce::Rectangle<int> area,
                         juce::Colour text, juce::Colour muted,
                         juce::Colour accent, juce::Colour accentCool, bool locked);
    float loadFloatParam (const char* id, float fallback) const;
    int loadIntParam (const char* id, int fallback) const;
    bool loadBoolParam (const char* id, bool fallback) const;
    DisplayMode currentMode() const;
    void setMode (DisplayMode mode);
    static const char* bandLabel (int index) noexcept;
    static void bandFreqRange (int index, float& loHz, float& hiHz) noexcept;

    std::atomic<bool> enabled { false };
    juce::AudioProcessorValueTreeState* valueTree = nullptr;
    SharedResources* themeColors = nullptr;

    double sampleRate = 48000.0;

    juce::dsp::FFT fft { kFftOrder };
    juce::dsp::WindowingFunction<float> window {
        (size_t) kFftSize, juce::dsp::WindowingFunction<float>::hann, true
    };

    juce::AbstractFifo fifo { kFftSize * 4 };
    std::vector<float> ring;

    std::vector<float> workTime;
    std::vector<float> workFft;
    std::vector<float> mag;

    std::atomic<float> thdPercent { 0.0f };
    std::atomic<float> thdDb { -120.0f };
    std::atomic<float> fundamentalHz { 0.0f };
    std::atomic<float> lockQuality { 0.0f };
    std::array<std::atomic<float>, kMaxHarmonics> harmonicNorm {};
    /** Per-band residual 0..1 (non-peak energy / total energy in band). */
    std::array<std::atomic<float>, kNumBands> bandResidual {};
    /** Peak level in band (linear mag), for relative bar height. */
    std::array<std::atomic<float>, kNumBands> bandPeakNorm {};

    float smoothThdPct = 0.0f;
    float smoothThdDb = -120.0f;
    float smoothF0 = 0.0f;
    float smoothLock = 0.0f;
    std::array<float, kMaxHarmonics> smoothHarm {};
    std::array<float, kNumBands> smoothBandRes {};
    std::array<float, kNumBands> smoothBandPeak {};

    juce::Rectangle<int> broadbandChip, multibandChip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ThdMeterComponent)
};
