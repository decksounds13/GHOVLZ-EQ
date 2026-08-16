#include <JuceHeader.h>
#include "RotaryImageKnob6.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob6::RotaryImageKnob6()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
    setTextBoxIsEditable(false);
    setPaintingIsUnclipped (true);
    setRange(0.36, 0.80, .01);

    
    setControlDefault (0.36);
float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging())
            repaint();
    };
}

RotaryImageKnob6::~RotaryImageKnob6()
{
}

void RotaryImageKnob6::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel6.setThemeColors (r);
    repaint();
}

void RotaryImageKnob6::refreshValuePopup (bool)
{
}

void RotaryImageKnob6::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel6.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel6.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
    KnobTheme::drawHoverValuePopup (g, *this, themeColors);
}

void RotaryImageKnob6::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnob6::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter (event);
    repaint();
}

void RotaryImageKnob6::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit (event);
    repaint();
}
