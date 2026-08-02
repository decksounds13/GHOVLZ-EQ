#pragma once

#include <JuceHeader.h>
#include <functional>
#include "VerticalGradientMeter.h"
#include "Menu/SharedResources.h"

class EqProcessor;

/** Stereo level-meter pair for Scope modules (Input or Output tap). */
class ScopeLevelMeterModule : public juce::Component,
                              private juce::Timer
{
public:
    enum class Tap { input = 0, output };

    ScopeLevelMeterModule (EqProcessor& processor,
                           juce::AudioProcessorValueTreeState& state,
                           Tap defaultTap,
                           const juce::String& title);
    ~ScopeLevelMeterModule() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    void setTap (Tap t) noexcept;
    Tap getTap() const noexcept { return tap; }
    void setThemeColors (SharedResources* r) noexcept;

    /** Tiled vs strip: same left-readout / full-height meter layout; sizes scale with bounds. */
    void setTiledPresentation (bool shouldUseTiled) noexcept;

    std::function<void()> onShowContextMenu;

private:
    void showTapMenu();
    void timerCallback() override;
    float readPeakDb (int channel) const;
    float readRmsDb (int channel) const;
    float readTruePeakDb (int channel) const;
    void updateTruePeakBallistics();
    int computeReadoutWidth (int availableW) const;

    EqProcessor& processor;
    juce::AudioProcessorValueTreeState& treeState;
    Tap tap = Tap::input;
    juce::String title;
    MeterClipState clipState;
    VerticalGradientMeter meterL;
    VerticalGradientMeter meterR;
    SharedResources* theme = nullptr;
    bool tiledPresentation = false;

    float displayedTruePeakL = -100.0f;
    float displayedTruePeakR = -100.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopeLevelMeterModule)
};
