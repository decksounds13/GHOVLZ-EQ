#pragma once

#include <JuceHeader.h>
#include "RotaryImageKnobLookAndFeel6.h"

class RotaryImageKnob6 : public juce::Slider
{
public:
    RotaryImageKnob6();
    ~RotaryImageKnob6();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:

    RotaryImageKnobLookAndFeel6 rotaryImageKnobLookAndFeel6;

    


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob6)
};

