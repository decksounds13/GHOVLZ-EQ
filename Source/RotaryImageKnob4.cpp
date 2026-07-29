#include <JuceHeader.h>
#include "RotaryImageKnob4.h"
#include "KnobThemeHelpers.h"

namespace
{
    void showKnobValueText (juce::Slider& slider, bool show, SharedResources* themeColors)
    {
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 45, 20);
        KnobTheme::applyValuePopupColours (slider, show, KnobTheme::colors (themeColors));
    }
}

RotaryImageKnob4::RotaryImageKnob4()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 45, 20);
    setTextBoxIsEditable(true);
    setRange(20.0, 250.0, 1.0);

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    showKnobValueText (*this, false, themeColors);
}

RotaryImageKnob4::~RotaryImageKnob4()
{
}

void RotaryImageKnob4::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel4.setThemeColors (r);
    refreshValuePopup (isMouseOverOrDragging() || hasKeyboardFocus (true));
    repaint();
}

void RotaryImageKnob4::refreshValuePopup (bool show)
{
    showKnobValueText (*this, show, themeColors);
}

void RotaryImageKnob4::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel4.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel4.drawRotarySlider(g, 0, 0, getWidth(), getHeight(),
        static_cast<float>(getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnob4::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnob4::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    refreshValuePopup (true);
}

void RotaryImageKnob4::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (hasKeyboardFocus (true))
        return;
    refreshValuePopup (false);
}
