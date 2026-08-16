#include <JuceHeader.h>
#include "RotaryImageKnob5.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob5::RotaryImageKnob5()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
    setTextBoxIsEditable(false);
    setPaintingIsUnclipped (true);
    setRange(5000.0, 20000.0, 1.0);

    
    setControlDefault (5000.0);
float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging())
            repaint();
    };
}

RotaryImageKnob5::~RotaryImageKnob5()
{
}

void RotaryImageKnob5::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel5.setThemeColors (r);
    repaint();
}

void RotaryImageKnob5::refreshValuePopup (bool)
{
}

void RotaryImageKnob5::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel5.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel5.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
    KnobTheme::drawHoverValuePopup (g, *this, themeColors);
}

void RotaryImageKnob5::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnob5::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter (event);
    repaint();
}

void RotaryImageKnob5::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit (event);
    repaint();
}
