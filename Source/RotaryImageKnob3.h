#pragma once

#include <JuceHeader.h>
#include "RotaryImageKnobLookAndFeel3.h"
#include "Menu/SharedResources.h"

class RotaryImageKnob3 : public juce::Slider
{
public:
    RotaryImageKnob3();
    ~RotaryImageKnob3();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setThemeColors (SharedResources* r) noexcept;

private:
    void refreshValuePopup (bool show);

    SharedResources* themeColors = nullptr;
    RotaryImageKnobLookAndFeel3 rotaryImageKnobLookAndFeel3;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob3)
};

