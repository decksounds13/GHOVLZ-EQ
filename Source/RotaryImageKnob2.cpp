#include <JuceHeader.h>
#include "RotaryImageKnob2.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob2::RotaryImageKnob2()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 56, 18);
    setTextBoxIsEditable(true);
    setRange(0.15, 10.0, 0.01);

    
    setControlDefault (0.15);
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

RotaryImageKnob2::~RotaryImageKnob2()
{
}

void RotaryImageKnob2::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel2.setThemeColors (r);
    refreshValuePopup (isMouseOverOrDragging() || hasKeyboardFocus (true));
    repaint();
}

void RotaryImageKnob2::refreshValuePopup (bool show)
{
    KnobTheme::showValueTextBox (*this, show, themeColors);
}

void RotaryImageKnob2::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel2.setThemeColors (themeColors);
    const int side = juce::jmin (getWidth(), getHeight());
    const int x = (getWidth() - side) / 2;
    const int y = (getHeight() - side) / 2;
    rotaryImageKnobLookAndFeel2.drawRotarySlider (g, x, y, side, side,
        static_cast<float> (getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnob2::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnob2::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    refreshValuePopup (true);
}

void RotaryImageKnob2::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    refreshValuePopup (false);
}
