#pragma once

#include <JuceHeader.h>
#include "RotaryImageKnobLookAndFeel5.h"

class RotaryImageKnob5 : public juce::Slider
{
public:
    RotaryImageKnob5();
    ~RotaryImageKnob5();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:

    RotaryImageKnobLookAndFeel5 rotaryImageKnobLookAndFeel5;

    


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob5)
};

