#include "QuadPickerOverlayComponent.h"
#include "HueSelector.h"
#include <JuceHeader.h>

QuadPickerOverlayComponent::QuadPickerOverlayComponent (juce::Component& quadPickerIn,
                                                        juce::Slider& brightnessRangeSliderIn,
                                                        juce::Slider& saturationRangeSliderIn)
    : quadPicker (quadPickerIn),
      brightnessRangeSlider (brightnessRangeSliderIn),
      saturationRangeSlider (saturationRangeSliderIn)
{
    setInterceptsMouseClicks (false, false);
}

void QuadPickerOverlayComponent::paint (juce::Graphics& g)
{
    // Overlay is laid out to match the pad — always paint in *local* coords.
    const int width = getWidth();
    const int height = getHeight();
    if (width <= 0 || height <= 0)
        return;

    // Nothing to draw when randomize range guides are off and not hovering range sliders.
    const bool showRangeGuides = isSaturationRandomizationEnabled || isBrightnessRandomizationEnabled;
    const bool showHover = isMouseOverSaturationSlider || isMouseOverBrightnessSlider;
    if (! showRangeGuides && ! showHover)
        return;

    const float brightnessLower = (float) brightnessRangeSlider.getMinValue();
    const float brightnessUpper = (float) brightnessRangeSlider.getMaxValue();
    const float saturationLower = (float) saturationRangeSlider.getMinValue();
    const float saturationUpper = (float) saturationRangeSlider.getMaxValue();

    const int brightnessUpperY = height - (int) (brightnessUpper * (float) height);
    const int brightnessLowerY = height - (int) (brightnessLower * (float) height) - brightnessUpperY;
    const int saturationUpperX = (int) (saturationUpper * (float) width);
    const int saturationLowerX = (int) (saturationLower * (float) width);

    g.setColour (juce::Colours::grey.withAlpha (0.25f));

    if (isSaturationRandomizationEnabled || isBrightnessRandomizationEnabled)
    {
        g.fillRect (saturationUpperX, 0, juce::jmax (0, width - saturationUpperX), brightnessUpperY);
        g.fillRect (0, 0, juce::jmax (0, saturationLowerX), brightnessUpperY);
        g.fillRect (saturationUpperX, brightnessUpperY + brightnessLowerY,
                    juce::jmax (0, width - saturationUpperX),
                    juce::jmax (0, height - (brightnessUpperY + brightnessLowerY)));
        g.fillRect (0, brightnessUpperY + brightnessLowerY,
                    juce::jmax (0, saturationLowerX),
                    juce::jmax (0, height - (brightnessUpperY + brightnessLowerY)));
    }

    if (isBrightnessRandomizationEnabled)
    {
        g.fillRect (saturationLowerX, 0,
                    juce::jmax (0, saturationUpperX - saturationLowerX), brightnessUpperY);
        g.fillRect (saturationLowerX, brightnessUpperY + brightnessLowerY,
                    juce::jmax (0, saturationUpperX - saturationLowerX),
                    juce::jmax (0, height - (brightnessUpperY + brightnessLowerY)));
    }

    if (isSaturationRandomizationEnabled)
    {
        g.fillRect (saturationUpperX, brightnessUpperY,
                    juce::jmax (0, width - saturationUpperX), juce::jmax (0, brightnessLowerY));
        g.fillRect (0, brightnessUpperY, juce::jmax (0, saturationLowerX), juce::jmax (0, brightnessLowerY));
    }

    // Hover guides only when the mouse is actually over a range slider (parent space).
    const juce::Colour hoverLinesColor = juce::Colours::grey.withAlpha (0.85f);
    if (isMouseOverSaturationSlider || isMouseOverBrightnessSlider)
    {
        g.setColour (hoverLinesColor);
        if (isMouseOverHorizontalPartOfSaturationSlider || isMouseOverHorizontalPartOfBrightnessSlider)
            g.fillRect (0, mouseYPosition - 1, width, 2);
        else
            g.fillRect (mouseXPosition - 1, 0, 2, height);
    }

    // Range outline lines — only while the matching randomize chip is on.
    const juce::Colour rangeLineColor (juce::Colours::whitesmoke.withAlpha (0.85f));
    g.setColour (rangeLineColor);

    if (isSaturationRandomizationEnabled && isBrightnessRandomizationEnabled)
    {
        g.fillRect (saturationLowerX, brightnessUpperY - 1,
                    juce::jmax (0, saturationUpperX - saturationLowerX), 2);
        g.fillRect (saturationLowerX, brightnessUpperY + brightnessLowerY - 1,
                    juce::jmax (0, saturationUpperX - saturationLowerX), 2);
        g.fillRect (saturationUpperX - 1, brightnessUpperY, 2, juce::jmax (0, brightnessLowerY));
        g.fillRect (saturationLowerX - 1, brightnessUpperY, 2, juce::jmax (0, brightnessLowerY));
    }
    else if (isBrightnessRandomizationEnabled)
    {
        g.fillRect (0, brightnessUpperY - 1, width, 2);
        g.fillRect (0, brightnessUpperY + brightnessLowerY - 1, width, 2);
    }
    else if (isSaturationRandomizationEnabled)
    {
        g.fillRect (saturationUpperX - 1, 0, 2, height);
        g.fillRect (saturationLowerX - 1, 0, 2, height);
    }
}

void QuadPickerOverlayComponent::resized()
{
    // Parent owns setBounds.
}

void QuadPickerOverlayComponent::setSaturationRandomizationEnabled (bool isEnabled)
{
    isSaturationRandomizationEnabled = isEnabled;
    repaint();
}

void QuadPickerOverlayComponent::setBrightnessRandomizationEnabled (bool isEnabled)
{
    isBrightnessRandomizationEnabled = isEnabled;
    repaint();
}

void QuadPickerOverlayComponent::mouseMove (const juce::MouseEvent& event)
{
    // Hit-test range sliders in parent space (they are siblings of the pad, not children).
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    const auto parentPos = event.getEventRelativeTo (parent).getPosition();
    const auto localPos = event.getEventRelativeTo (this).getPosition();
    mouseXPosition = localPos.x;
    mouseYPosition = localPos.y;

    isMouseOverSaturationSlider = saturationRangeSlider.getBounds().contains (parentPos);
    isMouseOverBrightnessSlider = brightnessRangeSlider.getBounds().contains (parentPos);

    // TwoValueHorizontal: thumbs are vertical strips near the ends; treat as vertical hover.
    isMouseOverHorizontalPartOfSaturationSlider = false;
    isMouseOverHorizontalPartOfBrightnessSlider = false;

    repaint();
}

void QuadPickerOverlayComponent::mouseExit (const juce::MouseEvent&)
{
    isMouseOverSaturationSlider = false;
    isMouseOverBrightnessSlider = false;
    isMouseOverHorizontalPartOfSaturationSlider = false;
    isMouseOverHorizontalPartOfBrightnessSlider = false;
    repaint();
}
