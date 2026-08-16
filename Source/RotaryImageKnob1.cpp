#include <JuceHeader.h>
#include "RotaryImageKnob1.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob1::RotaryImageKnob1()
{
    setLookAndFeel(&rotaryImageKnobLookAndFeel1);

    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
    setTextBoxIsEditable(false);
    setPaintingIsUnclipped (true);
    setRange(0.36, 0.80, .01);
    setControlDefault (0.36); // low end of factory range

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging())
            repaint();
    };
}

RotaryImageKnob1::~RotaryImageKnob1()
{
    setLookAndFeel(nullptr);
}

void RotaryImageKnob1::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel1.setThemeColors (r);
    repaint();
}

void RotaryImageKnob1::refreshValuePopup (bool)
{
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
    KnobTheme::drawHoverValuePopup (g, *this, themeColors);
}

void RotaryImageKnob1::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnob1::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter (event);
    repaint();
}

void RotaryImageKnob1::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit (event);
    repaint();
}

bool RotaryImageKnob1::isImageValid() const {
    return rotaryImageKnobLookAndFeel1.isImageValid();
}
