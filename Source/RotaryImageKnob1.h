#pragma once

#include <JuceHeader.h>
#include "RotaryImageKnobLookAndFeel1.h"
#include "Menu/SharedResources.h"
#include "ControlReset.h"

class RotaryImageKnob1 : public ResettableSlider
{
public:
    RotaryImageKnob1();
    ~RotaryImageKnob1();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    void setThemeColors (SharedResources* r) noexcept;

    bool isImageValid() const;

private:
    void refreshValuePopup (bool show);

    SharedResources* themeColors = nullptr;
    RotaryImageKnobLookAndFeel1 rotaryImageKnobLookAndFeel1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob1)
};
