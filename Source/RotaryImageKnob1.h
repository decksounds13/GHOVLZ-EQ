#pragma once

#include <JuceHeader.h>
#include "RotaryImageKnobLookAndFeel1.h"

class RotaryImageKnob1 : public juce::Slider
{
public:
    RotaryImageKnob1();
    ~RotaryImageKnob1();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;

    bool isImageValid() const;

private:

 

    RotaryImageKnobLookAndFeel1 rotaryImageKnobLookAndFeel1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnob1)
};
