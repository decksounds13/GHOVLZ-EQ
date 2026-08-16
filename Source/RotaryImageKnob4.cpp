#include <JuceHeader.h>
#include "RotaryImageKnob4.h"
#include "KnobThemeHelpers.h"

RotaryImageKnob4::RotaryImageKnob4()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
    setTextBoxIsEditable(false);
    setPaintingIsUnclipped (true);
    setRange(20.0, 250.0, 1.0);

    
    setControlDefault (20.0);
float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (isMouseOverOrDragging())
            repaint();
    };
}

RotaryImageKnob4::~RotaryImageKnob4()
{
}

void RotaryImageKnob4::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel4.setThemeColors (r);
    repaint();
}

void RotaryImageKnob4::refreshValuePopup (bool)
{
}

void RotaryImageKnob4::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel4.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel4.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
    KnobTheme::drawHoverValuePopup (g, *this, themeColors);
}

void RotaryImageKnob4::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnob4::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter (event);
    repaint();
}

void RotaryImageKnob4::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit (event);
    repaint();
}
