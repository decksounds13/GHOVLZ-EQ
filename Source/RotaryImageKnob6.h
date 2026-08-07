#pragma once

#include <JuceHeader.h>
#include "ControlReset.h"
#include "RotaryImageKnobLookAndFeel6.h"
#include "Menu/SharedResources.h"

class RotaryImageKnob6 : public ResettableSlider
{
public:
    RotaryImageKnob6();
    ~RotaryImageKnob6();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setThemeColors (SharedResources* r) noexcept;

private:
    void refreshValuePopup (bool show);

    SharedResources* themeColors = nullptr;
    RotaryImageKnobLookAndFeel6 rotaryImageKnobLookAndFeel6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob6)
};
