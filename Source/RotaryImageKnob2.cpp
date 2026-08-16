#include <JuceHeader.h>
#include "RotaryImageKnob2.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob2::RotaryImageKnob2()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
    setTextBoxIsEditable(false);
    setPaintingIsUnclipped (true);
    setRange(0.15, 10.0, 0.01);

    
    setControlDefault (0.15);
float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging())
            repaint();
    };
}

RotaryImageKnob2::~RotaryImageKnob2()
{
}

void RotaryImageKnob2::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel2.setThemeColors (r);
    repaint();
}

void RotaryImageKnob2::refreshValuePopup (bool)
{
}

void RotaryImageKnob2::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel2.setThemeColors (themeColors);
    const int side = juce::jmin (getWidth(), getHeight());
    const int x = (getWidth() - side) / 2;
    const int y = (getHeight() - side) / 2;
    rotaryImageKnobLookAndFeel2.drawRotarySlider (g, x, y, side, side,
        static_cast<float> (getValue()), 0.0f, 1.0f, *this);
    KnobTheme::drawHoverValuePopup (g, *this, themeColors);
}

void RotaryImageKnob2::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnob2::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter (event);
    repaint();
}

void RotaryImageKnob2::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit (event);
    repaint();
}
