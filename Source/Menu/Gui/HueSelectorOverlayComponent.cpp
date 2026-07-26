#include "HueSelectorOverlayComponent.h"
#include "HueSelector.h"

HueSelectorOverlayComponent::HueSelectorOverlayComponent(juce::Component& hueSelector, juce::Slider& hueRangeSlider)
    : hueSelector(hueSelector), hueRangeSlider(hueRangeSlider)
{
}

void HueSelectorOverlayComponent::paint(juce::Graphics& g)
{
    drawHueSelectorOverlay(g);
}

void HueSelectorOverlayComponent::resized()
{
    // Match the bounds of the HueSelector component
    setBounds(hueSelector.getBounds());
}

void HueSelectorOverlayComponent::drawHueSelectorOverlay(juce::Graphics& g)
{
    // Safely cast hueSelector to HueSelector type and retrieve the gradient bounds
    auto* hueSelectorSpecific = dynamic_cast<HueSelector*>(&hueSelector);
    if (hueSelectorSpecific) {
        juce::Rectangle<int> gradientRect = hueSelectorSpecific->getGradientBounds();

        // Calculate hueMax and hueMin based on the slider values and gradient rectangle
        float hueMax = static_cast<int>(hueRangeSlider.getMaxValue() * gradientRect.getHeight());
        float hueMin = static_cast<int>(hueRangeSlider.getMinValue() * gradientRect.getHeight());

        // Calculate the x-coordinate for the rectangles based on the gradientRect's position
        int rectX = gradientRect.getX();

        // Top rectangle's bottom edge is controlled by hueMax, anchored at the top of the gradient
        int upperRectY = gradientRect.getY();
        int upperRectHeight = gradientRect.getHeight() - static_cast<int>(hueMax);

        // Bottom rectangle's top edge is controlled by hueMin, anchored at the bottom of the gradient
        // Flipping the relationship for hueMin
        int bottomRectY = gradientRect.getBottom() - static_cast<int>(hueMin);
        int bottomRectHeight = gradientRect.getBottom() - bottomRectY;

        // Calculate the widths of the rectangles to match the gradient width
        int rectWidth = gradientRect.getWidth();

        // Create the upper and bottom rectangles
        juce::Rectangle<int> upperRect(rectX, upperRectY, rectWidth, upperRectHeight);
        juce::Rectangle<int> bottomRect(rectX, bottomRectY, rectWidth, bottomRectHeight);

        // Draw the upper and bottom rectangles
        g.setColour(juce::Colours::grey.withAlpha(0.5f)); // Semi-transparent grey
        g.fillRect(upperRect);
        g.fillRect(bottomRect);

        // Draw horizontal lines matching the gradient width
        g.setColour(juce::Colours::grey); // Solid grey for the lines
        g.drawLine(upperRect.getX(), upperRect.getBottom(), upperRect.getRight(), upperRect.getBottom(), 2.0f); // Line at the bottom of upperRect
        g.drawLine(bottomRect.getX(), bottomRect.getY(), bottomRect.getRight(), bottomRect.getY(), 2.0f); // Line at the top of bottomRect
    }
}

