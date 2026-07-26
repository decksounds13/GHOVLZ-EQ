#include <JuceHeader.h>
#include "RotaryImageKnob3.h"

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

RotaryImageKnob3::RotaryImageKnob3()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 45, 20);
    setTextBoxIsEditable(true);
    setRange(-24, 24.0, 0.01);

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    showKnobValueText (*this, false);
}

RotaryImageKnob3::~RotaryImageKnob3()
{
}

void RotaryImageKnob3::paint(juce::Graphics& g)
{
    constexpr float startAngleRadians = juce::degreesToRadians (-40.0);
    constexpr float endAngleRadians = juce::degreesToRadians (-320.0);
    const int side = juce::jmin (getWidth(), getHeight());
    const int x = (getWidth() - side) / 2;
    const int y = (getHeight() - side) / 2;
    rotaryImageKnobLookAndFeel3.drawRotarySlider (g, x, y, side, side,
        static_cast<float> (getValue()), startAngleRadians, endAngleRadians, *this);
}

void RotaryImageKnob3::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnob3::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    showKnobValueText (*this, true);
}

void RotaryImageKnob3::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    showKnobValueText (*this, false);
}
