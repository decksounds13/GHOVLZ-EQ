#include "QuadPickerOverlayComponent.h"
#include "HueSelector.h"
#include <JuceHeader.h>

QuadPickerOverlayComponent::QuadPickerOverlayComponent(juce::Component& quadPicker,  juce::Slider& brightnessRangeSlider, juce::Slider& saturationRangeSlider)
    : quadPicker(quadPicker), brightnessRangeSlider(brightnessRangeSlider), saturationRangeSlider(saturationRangeSlider)
{
}

void QuadPickerOverlayComponent::paint(juce::Graphics& g)
{
    auto bounds = quadPicker.getBounds();

    float brightnessLower = brightnessRangeSlider.getMinValue();
    float brightnessUpper = brightnessRangeSlider.getMaxValue();
    float saturationLower = saturationRangeSlider.getMinValue();
    float saturationUpper = saturationRangeSlider.getMaxValue();


    // Calculate the positions of Rectangle 1 and Rectangle 2 based on the constraints

    int brightnessUpperY = bounds.getHeight() - static_cast<int>(brightnessUpper * bounds.getHeight());
    int brightnessLowerY = bounds.getHeight() - static_cast<int>(brightnessLower * bounds.getHeight()) - brightnessUpperY;
    int saturationUpperX = saturationUpper * bounds.getWidth();
    int saturationLowerX = saturationLower * bounds.getWidth();

    int width = bounds.getWidth();
    int height = bounds.getHeight();
 
    g.setColour(juce::Colours::grey.withAlpha(0.25f));

    // Shared corner rectangles (Rectangles 1, 3, 5, 7)
    if (isSaturationRandomizationEnabled || isBrightnessRandomizationEnabled) {
        juce::Rectangle<int> rect1(saturationUpper * width, 0, width, brightnessUpperY);
        juce::Rectangle<int> rect3(0, 0, saturationLowerX, brightnessUpperY);
        juce::Rectangle<int> rect5(saturationUpper * width, brightnessUpperY + brightnessLowerY, width, height - (brightnessUpperY + brightnessLowerY));
        juce::Rectangle<int> rect7(0, brightnessUpperY + brightnessLowerY, saturationLowerX, height - (brightnessUpperY + brightnessLowerY));

        g.fillRect(rect1);
        g.fillRect(rect3);
        g.fillRect(rect5);
        g.fillRect(rect7);
    }

    // Saturation-exclusive rectangles (Rectangles 2, 6)
    if (isBrightnessRandomizationEnabled) {
        juce::Rectangle<int> rect2(saturationLower * width, 0, saturationUpperX - saturationLowerX, brightnessUpperY);
        juce::Rectangle<int> rect6(saturationLower * width, brightnessUpperY + brightnessLowerY, saturationUpperX - saturationLowerX, height - (brightnessUpperY + brightnessLowerY));

        g.fillRect(rect2);
        g.fillRect(rect6);
    }

    // Brightness-exclusive rectangles (Rectangles 4, 8)
    if (isSaturationRandomizationEnabled) {
        juce::Rectangle<int> rect4(saturationUpper * width, brightnessUpperY, width, brightnessLowerY);
        juce::Rectangle<int> rect8(0, brightnessUpperY, saturationLowerX, brightnessLowerY);

        g.fillRect(rect4);
        g.fillRect(rect8);
    }


    // Draw vertical lines across the quadPicker component for hover
    juce::Colour hoverLinesColor = juce::Colours::grey.withAlpha(0.85f); // Custom hover lines color (yellow in this example)

    if (isMouseOverSaturationSlider) {
        if (isMouseOverHorizontalPartOfSaturationSlider) {
            // Draw horizontal line at the appropriate Y position
            g.setColour(hoverLinesColor);
            g.fillRect(0, mouseYPosition - 1, width, 2);
        }
        else {
            // Draw vertical line at the appropriate X position
            g.setColour(hoverLinesColor);
            g.fillRect(mouseXPosition - 1, 0, 2, height);
        }
    }
    else if (isMouseOverBrightnessSlider) {
        if (isMouseOverHorizontalPartOfBrightnessSlider) {
            // Draw horizontal line at the appropriate Y position
            g.setColour(hoverLinesColor);
            g.fillRect(0, mouseYPosition - 1, width, 2);
        }
        else {
            // Draw vertical line at the appropriate X position
            g.setColour(hoverLinesColor);
            g.fillRect(mouseXPosition - 1, 0, 2, height);
        }
    }





    // Range Box / Lines
    // Define the custom color
    juce::Colour rangeLineColor(juce::Colours::whitesmoke.withAlpha(0.85f)); // WhiteSmoke with alpha 0.85

    if (isSaturationRandomizationEnabled && isBrightnessRandomizationEnabled) {
        // Draw within the saturation range if both toggles are enabled
        int saturationLowerX = saturationRangeSlider.getMinValue() * bounds.getWidth();
        int saturationUpperX = saturationRangeSlider.getMaxValue() * bounds.getWidth();

        g.setColour(rangeLineColor);
        g.fillRect(saturationLowerX, brightnessUpperY - 1, saturationUpperX - saturationLowerX, 2);
        g.fillRect(saturationLowerX, brightnessUpperY + brightnessLowerY - 1, saturationUpperX - saturationLowerX, 2);
    }
    else if (isBrightnessRandomizationEnabled) {
        // Span the entire width if only brightness randomization is enabled
        g.setColour(rangeLineColor);
        g.fillRect(0, brightnessUpperY - 1, bounds.getWidth(), 2); // Horizontal line at brightnessUpperY
        g.fillRect(0, brightnessUpperY + brightnessLowerY - 1, bounds.getWidth(), 2); // Horizontal line at brightnessLowerY
    }

    // Vertical lines logic
    if (isSaturationRandomizationEnabled) {
        g.setColour(rangeLineColor); // Custom line color

        if (!isBrightnessRandomizationEnabled) {
            // Span the entire height if brightness randomization is disabled
            g.fillRect(saturationUpperX - 1, 0, 2, bounds.getHeight()); // Vertical line at saturationUpperX
            g.fillRect(saturationLowerX - 1, 0, 2, bounds.getHeight()); // Vertical line at saturationLowerX
        }
        else {
            // Draw within brightness range if brightness randomization is enabled
            g.fillRect(saturationUpperX - 1, brightnessUpperY, 2, brightnessLowerY); // Vertical line at saturationUpperX
            g.fillRect(saturationLowerX - 1, brightnessUpperY, 2, brightnessLowerY); // Vertical line at saturationLowerX
        }
    }




 
 }

void QuadPickerOverlayComponent::resized()
{
    setBounds(quadPicker.getBounds());

}

void QuadPickerOverlayComponent::setSaturationRandomizationEnabled(bool isEnabled) {
    isSaturationRandomizationEnabled = isEnabled;
    repaint();  // Trigger a repaint to reflect the new state
}

void QuadPickerOverlayComponent::setBrightnessRandomizationEnabled(bool isEnabled) {
    isBrightnessRandomizationEnabled = isEnabled;
    repaint();  // Trigger a repaint to reflect the new state
}


void QuadPickerOverlayComponent::mouseMove(const juce::MouseEvent& event) {
    // Convert the global position to a local position within this component
    juce::Point<int> localPosition = event.getEventRelativeTo(this).getPosition();

    // Update mouseXPosition and mouseYPosition with local coordinates
    mouseXPosition = localPosition.getX();
    mouseYPosition = localPosition.getY();

    // Check if the mouse is over the saturation or brightness sliders
    isMouseOverSaturationSlider = saturationRangeSlider.getBounds().contains(localPosition);
    isMouseOverBrightnessSlider = brightnessRangeSlider.getBounds().contains(localPosition);

    // Trigger a repaint to update the hover lines
    repaint();
}