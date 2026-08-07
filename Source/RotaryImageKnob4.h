#pragma once

#include <JuceHeader.h>
#include "ControlReset.h"
#include "RotaryImageKnobLookAndFeel4.h"
#include "Menu/SharedResources.h"

class RotaryImageKnob4 : public ResettableSlider
{
public:
    RotaryImageKnob4();
    ~RotaryImageKnob4();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setThemeColors (SharedResources* r) noexcept;

private:
    void refreshValuePopup (bool show);

    SharedResources* themeColors = nullptr;
    RotaryImageKnobLookAndFeel4 rotaryImageKnobLookAndFeel4;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob4)
};
