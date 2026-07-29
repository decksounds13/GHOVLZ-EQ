#include <JuceHeader.h>
#include "RotaryImageKnob6.h"
#include "KnobThemeHelpers.h"

namespace
{
    void showKnobValueText (juce::Slider& slider, bool show, SharedResources* themeColors)
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 20);
        KnobTheme::applyValuePopupColours (slider, show, KnobTheme::colors (themeColors));
    }
}

RotaryImageKnob6::RotaryImageKnob6()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 45, 20);
    setTextBoxIsEditable(true);
    setRange(0.36, 0.80, .01);

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    showKnobValueText (*this, false, themeColors);
}

RotaryImageKnob6::~RotaryImageKnob6()
{
}

void RotaryImageKnob6::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel6.setThemeColors (r);
    refreshValuePopup (isMouseOverOrDragging() || hasKeyboardFocus (true));
    repaint();
}

void RotaryImageKnob6::refreshValuePopup (bool show)
{
    showKnobValueText (*this, show, themeColors);
}

void RotaryImageKnob6::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel6.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel6.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnob6::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnob6::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    refreshValuePopup (true);
}

void RotaryImageKnob6::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    refreshValuePopup (false);
}
