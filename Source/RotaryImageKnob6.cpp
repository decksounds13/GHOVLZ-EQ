#include <JuceHeader.h>
#include "RotaryImageKnob6.h"

namespace
{
    void showKnobValueText (juce::Slider& slider, bool show)
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 20);

        const auto textColour = show ? juce::Colours::whitesmoke.withAlpha (0.9f)
                                     : juce::Colours::transparentBlack;
        const auto bgColour = show ? juce::Colours::black.withAlpha (0.35f)
                                   : juce::Colours::transparentBlack;
        const auto outlineColour = show ? juce::Colours::whitesmoke.withAlpha (0.25f)
                                        : juce::Colours::transparentBlack;

        slider.setColour (juce::Slider::textBoxTextColourId, textColour);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, bgColour);
        slider.setColour (juce::Slider::textBoxOutlineColourId, outlineColour);
    }
}

RotaryImageKnob6::RotaryImageKnob6()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 45, 20);
    setTextBoxIsEditable(true);
    setRange(0.36, 0.80, .01);

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    showKnobValueText (*this, false);
}

RotaryImageKnob6::~RotaryImageKnob6()
{
}

void RotaryImageKnob6::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel6.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnob6::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnob6::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    showKnobValueText (*this, true);
}

void RotaryImageKnob6::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    showKnobValueText (*this, false);
}
