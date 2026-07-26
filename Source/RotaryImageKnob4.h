#pragma once

#include <JuceHeader.h>
#include "RotaryImageKnobLookAndFeel4.h"

class RotaryImageKnob4 : public juce::Slider
{
public:
    RotaryImageKnob4();
    ~RotaryImageKnob4();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

private:

    RotaryImageKnobLookAndFeel4 rotaryImageKnobLookAndFeel4;

    


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob4)
};

