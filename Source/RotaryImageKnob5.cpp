#include <JuceHeader.h>
#include "RotaryImageKnob5.h"
#include "KnobThemeHelpers.h"

namespace
{
    void showKnobValueText (juce::Slider& slider, bool show, SharedResources* themeColors)
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 20);
        KnobTheme::applyValuePopupColours (slider, show, KnobTheme::colors (themeColors));
    }
}

RotaryImageKnob5::RotaryImageKnob5()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 45, 20);
    setTextBoxIsEditable(true);
    setRange(5000.0, 20000.0, 1.0);

    
    setControlDefault (5000.0);
float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    showKnobValueText (*this, false, themeColors);
}

RotaryImageKnob5::~RotaryImageKnob5()
{
}

void RotaryImageKnob5::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel5.setThemeColors (r);
    refreshValuePopup (isMouseOverOrDragging() || hasKeyboardFocus (true));
    repaint();
}

void RotaryImageKnob5::refreshValuePopup (bool show)
{
    showKnobValueText (*this, show, themeColors);
}

void RotaryImageKnob5::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel5.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel5.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnob5::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnob5::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    refreshValuePopup (true);
}

void RotaryImageKnob5::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    refreshValuePopup (false);
}
