#include <JuceHeader.h>
#include "RotaryImageKnob3.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob3::RotaryImageKnob3()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 56, 18);
    setTextBoxIsEditable(true);
    setRange(-24, 24.0, 0.01);

    
    setControlDefault (-24);
float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging() || hasKeyboardFocus (true))
            refreshValuePopup (true);
    };

    KnobTheme::showValueTextBox (*this, false, themeColors);
}

RotaryImageKnob3::~RotaryImageKnob3()
{
}

void RotaryImageKnob3::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel3.setThemeColors (r);
    refreshValuePopup (isMouseOverOrDragging() || hasKeyboardFocus (true));
    repaint();
}

void RotaryImageKnob3::refreshValuePopup (bool show)
{
    KnobTheme::showValueTextBox (*this, show, themeColors);
}

void RotaryImageKnob3::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel3.setThemeColors (themeColors);
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

    setControlDefault (newMin);
}

void RotaryImageKnob3::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    refreshValuePopup (true);
}

void RotaryImageKnob3::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    refreshValuePopup (false);
}
