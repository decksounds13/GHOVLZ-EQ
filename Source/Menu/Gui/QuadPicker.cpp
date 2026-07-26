#include "QuadPicker.h"
#include "../SharedResources.h"
#include <JuceHeader.h>

QuadPicker::QuadPicker(SharedResources& resources)
    : sharedResources(resources)
{
    // DBG("QuadPicker constructor called");
    setHue(0.0f);
    addMouseListener(this, true);


}

void QuadPicker::paint(juce::Graphics& g) {
    // Check if the gradient image needs to be updated
    if (isFirstPaint || hueColor != lastHueColor || gradientImage.isNull()) {
        gradientImage = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
        juce::Graphics imgGraphics(gradientImage);

        for (int y = 0; y < getHeight(); ++y) {
            float lightness = 1.0f - (float)y / (float)getHeight();
            juce::ColourGradient gradient(hueColor.withSaturation(0.0f).withLightness(lightness),
                0, (float)y,
                hueColor.withSaturation(1.0f).withLightness(lightness),
                (float)getWidth(), (float)y,
                false);
            imgGraphics.setGradientFill(gradient);
            imgGraphics.fillRect(0, y, getWidth(), 1);
        }

        lastHueColor = hueColor; // Update the last used color
        isFirstPaint = false; // Update the flag after the first paint
    }

    // Define a rounded rectangle for the clipping region and outline
    float cornerSize = 5.0f;
    juce::Path roundedClipPath;
    roundedClipPath.addRoundedRectangle(getLocalBounds().toFloat(), cornerSize);

    // Save the current graphics state and set the clipping region
    g.saveState();
    g.reduceClipRegion(roundedClipPath);

    // Draw the cached gradient image
    g.drawImageAt(gradientImage, 0, 0);

    // Restore the graphics state to remove clipping
    g.restoreState();

    // Draw an outline around the QuadPicker with the same rounded rectangle (optional)
    // juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;
    // g.setColour(menuThinBorderColor);
    // g.strokePath(roundedClipPath, juce::PathStrokeType(1.0f));

   // Create a path for the shadow2 based on the ellipse's position
    juce::Path shadowPath;
    shadowPath.addEllipse(selectedPosition.x - 5, selectedPosition.y - 5, 10, 10);

    // Render the shadow2 using the path
    //shadow2.render(g, shadowPath);

    // Draw the selected position circle
    g.setColour(juce::Colours::white);
    g.drawEllipse(selectedPosition.x - 5, selectedPosition.y - 5, 10, 10, 2);


}

void QuadPicker::resized() {
    float cornerSize = 5.0f;
    shadowPath.clear();  // Clear the existing path
    shadowPath.addRoundedRectangle(getLocalBounds().toFloat(), cornerSize);

    // ... other resize logic, if any
}
void QuadPicker::mouseDrag(const juce::MouseEvent& event)
{
    setSelectedPosition(event.position);

    if (auto* uiElementsList = findParentComponentOfClass<UIElementsList>()) {
        auto selectedIndices = uiElementsList->getSelectedRows(); // Get selected rows

        if (!selectedIndices.isEmpty()) {
            int firstSelectedIndex = selectedIndices.getFirst(); // Get the first selected index
            uiElementsList->updateColorsForSelectedElements(selectedColor);
        }
    }
}


void QuadPicker::setHue(float newHue)
{
    // Extract the current saturation and brightness
    float currentSaturation = selectedColor.getSaturation();
    float currentBrightness = selectedColor.getBrightness();
    float currentAlpha = selectedColor.getAlpha();

    // Create a new color with the new hue, but keeping the old saturation and brightness
    hueColor = juce::Colour::fromHSV(newHue, currentSaturation, currentBrightness, currentAlpha);

    // Update the selected color
    selectedColor = hueColor;

    // Repaint the component to reflect the changes
    repaint();
}


void QuadPicker::setSelectedPosition(juce::Point<float> newPosition) {
    selectedPosition = newPosition;

    DBG("New Position x: " + juce::String(newPosition.x));
    DBG("QuadPicker Width: " + juce::String(getWidth()));

    float saturation = newPosition.x / (float)getWidth();
    float lightness = 1.0f - newPosition.y / (float)getHeight();

    // Get the current hue of selectedColor
    float currentHue, currentSaturation, currentBrightness;
    selectedColor.getHSB(currentHue, currentSaturation, currentBrightness);

    // Clamp the hue within the specified range
    float limitedHue = std::clamp(currentHue, sharedResources.sharedColors.hueLowerLimit, sharedResources.sharedColors.hueUpperLimit);

    // Create a new color with the limited hue, but keep the old saturation and brightness
    selectedColor = juce::Colour::fromHSV(limitedHue, saturation, lightness, 1.0f);

    DBG("Selected Color: " + selectedColor.toString());

    repaint();

    if (onColorChanged) {
        onColorChanged(selectedColor);
    }
}


void QuadPicker::hueChanged(float newHue)
{
    setHue(newHue);
    repaint();
}

void QuadPicker::setColor(const juce::Colour& newColor)
{
    DBG("QuadPicker::setColor called with color: " + newColor.toString());  // Debug statement

    // Extract the hue, saturation, and brightness of the new color
    float newHue, newSaturation, newBrightness;
    newColor.getHSB(newHue, newSaturation, newBrightness);

    // Extract the hue, saturation, and brightness of the current selected color
    float currentHue, currentSaturation, currentBrightness;
    selectedColor.getHSB(currentHue, currentSaturation, currentBrightness);

    // Check if saturation or brightness has changed
    if (newSaturation != currentSaturation || newBrightness != currentBrightness) {
        // Update the selected color with the new color
        selectedColor = newColor;

        // Update the hueColor for gradient display purposes
        hueColor = juce::Colour::fromHSV(newHue, 1.0f, 0.5f, 1.0f);

        // Update the selectedPosition based on newColor's saturation and brightness
        selectedPosition.x = newSaturation * getWidth();
        selectedPosition.y = (1.0f - newBrightness) * getHeight();

        selectedColor = newColor;

        repaint();
    }
}




juce::Colour QuadPicker::getSelectedColor() const
{
    return selectedColor;
}


//Investigate as cause of mulitple selection only updating last selectedcolor
void QuadPicker::onElementSelected(const juce::String& name, const juce::Colour& color)
{
    // Update the color picker with the new color
    setColor(color);
}

void QuadPicker::updateGradientImage() {
    gradientImage = juce::Image(juce::Image::ARGB, getWidth(), getHeight(), true);
    juce::Graphics g(gradientImage);

    for (int y = 0; y < getHeight(); ++y) {
        float lightness = 1.0f - (float)y / (float)getHeight();
        juce::ColourGradient gradient(hueColor.withSaturation(0.0f).withLightness(lightness),
            0, (float)y,
            hueColor.withSaturation(1.0f).withLightness(lightness),
            (float)getWidth(), (float)y,
            false);
        g.setGradientFill(gradient);
        g.fillRect(0, y, getWidth(), 1);
    }
}