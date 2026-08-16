#include <JuceHeader.h>
#include "RotaryImageKnobForOptionBox.h"
#include "KnobThemeHelpers.h"

RotaryImageKnobForOptionBox::RotaryImageKnobForOptionBox()
{
    setSliderStyle(Slider::SliderStyle::RotaryHorizontalVerticalDrag);
    setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
    setTextBoxIsEditable(false);
    setRange(0.36, 0.80, .01);
    
    setControlDefault (0.36);
// Melatonin disc drop extends past local bounds; without this the blur is clipped away.
    setPaintingIsUnclipped (true);

    float startAngleDegrees = 40.0;
    float endAngleDegrees = 320.0;
    setRotaryParameters(juce::degreesToRadians(startAngleDegrees), juce::degreesToRadians(endAngleDegrees), true);

    onValueChange = [this]
    {
        if (compactNoValueBox)
            return;
        if (isMouseOverOrDragging())
            repaint();
    };
}

RotaryImageKnobForOptionBox::~RotaryImageKnobForOptionBox()
{
}

void RotaryImageKnobForOptionBox::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rotaryImageKnobLookAndFeel1.setThemeColors (r);
    repaint();
}

void RotaryImageKnobForOptionBox::refreshValuePopup (bool)
{
}

void RotaryImageKnobForOptionBox::paint(juce::Graphics& g)
{
    rotaryImageKnobLookAndFeel1.setThemeColors (themeColors);
    rotaryImageKnobLookAndFeel1.setMinValue ((float) getMinimum());
    rotaryImageKnobLookAndFeel1.setMaxValue ((float) getMaximum());

    const int textH = compactNoValueBox ? 0 : getTextBoxHeight();
    const int drawW = getWidth();
    const int drawH = juce::jmax (1, getHeight() - textH);
    const int side = juce::jmin (drawW, drawH);
    const int x = (drawW - side) / 2;
    const int y = (drawH - side) / 2;

    if (SharedResources::glowShadowEffectsEnabled() && side > 2)
    {
        juce::Path disc;
        disc.addEllipse ((float) x + 1.0f, (float) y + 1.0f,
                         (float) side - 2.0f, (float) side - 2.0f);
        knobDropShadow.render (g, disc);
    }

    rotaryImageKnobLookAndFeel1.drawRotarySlider (g, x, y, side, side,
        static_cast<float> (getValue()), 0.0f, 1.0f, *this);

    if (! compactNoValueBox)
        KnobTheme::drawHoverValuePopup (g, *this, themeColors);
}

void RotaryImageKnobForOptionBox::setCustomRange(double newMin, double newMax, double newInterval)
{
    setRange(newMin, newMax, newInterval);

    setControlDefault (newMin);
}

void RotaryImageKnobForOptionBox::setCompactNoValueBox (bool shouldBeCompact)
{
    compactNoValueBox = shouldBeCompact;

    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    repaint();
}

void RotaryImageKnobForOptionBox::mouseDown (const juce::MouseEvent& event)
{
    if (event.mods.isPopupMenu() && onPopupMenu != nullptr)
    {
        onPopupMenu();
        return;
    }

    juce::Slider::mouseDown (event);
}

void RotaryImageKnobForOptionBox::mouseEnter(const juce::MouseEvent& event)
{
    juce::Slider::mouseEnter (event);
    if (! compactNoValueBox)
        repaint();
}

void RotaryImageKnobForOptionBox::mouseExit(const juce::MouseEvent& event)
{
    juce::Slider::mouseExit (event);
    if (! compactNoValueBox)
        repaint();
}
