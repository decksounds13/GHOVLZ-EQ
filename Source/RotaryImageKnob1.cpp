#include <JuceHeader.h>
#include "RotaryImageKnob1.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob1::RotaryImageKnob1()
{
    setLookAndFeel(&rotaryImageKnobLookAndFeel1);

    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 56, 18);
    setTextBoxIsEditable(true);
    setRange(0.36, 0.80, .01);
    setControlDefault (0.36); // low end of factory range

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging() || hasKeyboardFocus (true))
            refreshValuePopup (true);
    };

    KnobTheme::showValueTextBox (*this, false, themeColors);
    repaint();
}

RotaryImageKnob1::~RotaryImageKnob1()
{
    setLookAndFeel(nullptr);
}

void RotaryImageKnob1::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel1.setThemeColors (r);
    refreshValuePopup (isMouseOverOrDragging() || hasKeyboardFocus (true));
    repaint();
}

void RotaryImageKnob1::refreshValuePopup (bool show)
{
    KnobTheme::showValueTextBox (*this, show, themeColors);
}

void RotaryImageKnob1::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel1.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel1.setMinValue ((float) getMinimum());
    rotaryImageKnobLookAndFeel1.setMaxValue ((float) getMaximum());

    const int side = juce::jmin (getWidth(), getHeight());
    const int x = (getWidth() - side) / 2;
    const int y = (getHeight() - side) / 2;
    rotaryImageKnobLookAndFeel1.drawRotarySlider (g, x, y, side, side,
        static_cast<float> (getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnob1::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnob1::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    refreshValuePopup (true);
}

void RotaryImageKnob1::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    refreshValuePopup (false);
}

bool RotaryImageKnob1::isImageValid() const {
    return rotaryImageKnobLookAndFeel1.isImageValid();
}
