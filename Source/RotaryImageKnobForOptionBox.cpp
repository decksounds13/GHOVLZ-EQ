#include <JuceHeader.h>
#include "RotaryImageKnobForOptionBox.h"
#include "RotaryImageKnobLookAndFeel1.h"
#include "RotaryImageKnobLookAndFeel2.h"
#include "RotaryImageKnobLookAndFeel3.h"
#include "RotaryImageKnobLookAndFeel4.h"
#include "RotaryImageKnobLookAndFeel5.h"
#include "RotaryImageKnobLookAndFeel6.h"

namespace
{
    void showKnobValueText (juce::Slider& slider, bool show)
    {
        // Keep TextBoxBelow always so layout stays stable (avoids flicker from recreating the text box).
        // Tiny A/R knobs get a shorter readout so the face stays usable.
        const int boxW = juce::jmax (28, juce::jmin (45, slider.getWidth() > 0 ? slider.getWidth() : 45));
        const int boxH = (slider.getWidth() > 0 && slider.getWidth() < 40) ? 14 : 20;
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, boxW, boxH);

        const auto textColour = show ? juce::Colours::whitesmoke.withAlpha (0.9f)
                                     : juce::Colours::transparentBlack;
        const auto bgColour = show ? juce::Colours::black.withAlpha (0.35f)
                                   : juce::Colours::transparentBlack;
        const auto outlineColour = show ? juce::Colours::whitesmoke.withAlpha (0.25f)
                                        : juce::Colours::transparentBlack;

        slider.setColour (juce::Slider::textBoxTextColourId, textColour);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, bgColour);
        slider.setColour (juce::Slider::textBoxOutlineColourId, outlineColour);
    }
}

RotaryImageKnobForOptionBox::RotaryImageKnobForOptionBox()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::TextBoxBelow, false, 45, 20);
    setTextBoxIsEditable(true);
    setRange(0.36, 0.80, .01);

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    showKnobValueText (*this, false);
    repaint();
}

RotaryImageKnobForOptionBox::~RotaryImageKnobForOptionBox()
{
}

void RotaryImageKnobForOptionBox::paint(juce::Graphics& g)
{
    // Keep stitched-frame mapping in sync with this slider's range (freq / gain / Q / A / R).
    rotaryImageKnobLookAndFeel1.setMinValue ((float) getMinimum());
    rotaryImageKnobLookAndFeel1.setMaxValue ((float) getMaximum());

    // Draw a 1:1 face above the value text box so enabling readouts never stretches the art.
    const int textH = compactNoValueBox ? 0 : getTextBoxHeight();
    const int drawW = getWidth();
    const int drawH = juce::jmax (1, getHeight() - textH);
    const int side = juce::jmin (drawW, drawH);
    const int x = (drawW - side) / 2;
    const int y = (drawH - side) / 2;

    rotaryImageKnobLookAndFeel1.drawRotarySlider (g, x, y, side, side,
        static_cast<float> (getValue()), 0.0f, 1.0f, *this);
}

void RotaryImageKnobForOptionBox::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);
}

void RotaryImageKnobForOptionBox::setCompactNoValueBox (bool shouldBeCompact)
{
    compactNoValueBox = shouldBeCompact;

    if (compactNoValueBox)
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    else
        showKnobValueText (*this, false);
}

void RotaryImageKnobForOptionBox::mouseEnter(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    if (compactNoValueBox)
        return;

    showKnobValueText (*this, true);
}

void RotaryImageKnobForOptionBox::mouseExit(const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);

    if (compactNoValueBox)
        return;

    // Keep values visible while the text box is being edited.
    if (hasKeyboardFocus (true))
        return;

    showKnobValueText (*this, false);
}
