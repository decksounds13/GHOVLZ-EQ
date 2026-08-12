#include "QuadPicker.h"
#include "../SharedResources.h"
#include <JuceHeader.h>

QuadPicker::QuadPicker (SharedResources& resources)
    : sharedResources (resources)
{
    setHue (0.0f);
}

void QuadPicker::paint (juce::Graphics& g)
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    // Rebuild when hue changes or the component was resized (menu drag-resize).
    if (isFirstPaint || hueColor != lastHueColor || gradientImage.isNull()
        || gradientImage.getWidth() != w || gradientImage.getHeight() != h)
    {
        updateGradientImage();
        lastHueColor = hueColor;
        isFirstPaint = false;
    }

    constexpr float cornerSize = 5.0f;
    juce::Path roundedClipPath;
    roundedClipPath.addRoundedRectangle (getLocalBounds().toFloat(), cornerSize);

    g.saveState();
    g.reduceClipRegion (roundedClipPath);
    g.drawImageAt (gradientImage, 0, 0);
    g.restoreState();

    // Selection ring — follows mouse via setSelectedPosition.
    g.setColour (juce::Colours::white);
    g.drawEllipse (selectedPosition.x - 5.0f, selectedPosition.y - 5.0f, 10.0f, 10.0f, 2.0f);
}

void QuadPicker::resized()
{
    constexpr float cornerSize = 5.0f;
    shadowPath.clear();
    shadowPath.addRoundedRectangle (getLocalBounds().toFloat(), cornerSize);

    if (getWidth() > 0 && getHeight() > 0)
    {
        updateGradientImage();
        lastHueColor = hueColor;
        isFirstPaint = false;
        // Keep the ring on the same colour after layout / menu resize.
        syncPositionFromSelectedColor();
    }
}

void QuadPicker::applyPointerPosition (juce::Point<float> localPos)
{
    const float w = (float) juce::jmax (1, getWidth());
    const float h = (float) juce::jmax (1, getHeight());
    localPos.x = juce::jlimit (0.0f, w - 1.0f, localPos.x);
    localPos.y = juce::jlimit (0.0f, h - 1.0f, localPos.y);
    setSelectedPosition (localPos);
}

void QuadPicker::mouseDown (const juce::MouseEvent& event)
{
    draggingColour = true;
    applyPointerPosition (event.position);
}

void QuadPicker::mouseDrag (const juce::MouseEvent& event)
{
    if (! draggingColour)
        draggingColour = true;
    applyPointerPosition (event.position);
}

void QuadPicker::mouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (! draggingColour)
        return;

    draggingColour = false;
    // Full commit path (theme / chrome) after a light live-preview scrub.
    if (onColorChanged)
        onColorChanged (selectedColor);
}

void QuadPicker::setHue (float newHue)
{
    float currentSaturation = selectedColor.getSaturation();
    float currentBrightness = selectedColor.getBrightness();
    float currentAlpha = selectedColor.getFloatAlpha();

    // Full-sat mid-bright sample for the SV pad base colour.
    hueColor = juce::Colour::fromHSV (newHue, 1.0f, 0.5f, 1.0f);
    selectedColor = juce::Colour::fromHSV (newHue, currentSaturation, currentBrightness, currentAlpha);
    syncPositionFromSelectedColor();
    updateGradientImage();
    lastHueColor = hueColor;
    repaint();
}

void QuadPicker::syncPositionFromSelectedColor()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    float h = 0.0f, s = 0.0f, b = 0.0f;
    selectedColor.getHSB (h, s, b);
    juce::ignoreUnused (h);
    selectedPosition.x = s * (float) getWidth();
    selectedPosition.y = (1.0f - b) * (float) getHeight();
}

void QuadPicker::setSelectedPosition (juce::Point<float> newPosition)
{
    const float w = (float) juce::jmax (1, getWidth());
    const float h = (float) juce::jmax (1, getHeight());

    selectedPosition.x = juce::jlimit (0.0f, w - 1.0f, newPosition.x);
    selectedPosition.y = juce::jlimit (0.0f, h - 1.0f, newPosition.y);

    const float saturation = juce::jlimit (0.0f, 1.0f, selectedPosition.x / w);
    const float brightness = juce::jlimit (0.0f, 1.0f, 1.0f - selectedPosition.y / h);

    float currentHue = 0.0f, currentSaturation = 0.0f, currentBrightness = 0.0f;
    selectedColor.getHSB (currentHue, currentSaturation, currentBrightness);
    juce::ignoreUnused (currentSaturation, currentBrightness);
    const float keepAlpha = selectedColor.getFloatAlpha();

    const float limitedHue = juce::jlimit (sharedResources.sharedColors.hueLowerLimit,
                                           sharedResources.sharedColors.hueUpperLimit,
                                           currentHue);

    selectedColor = juce::Colour::fromHSV (limitedHue, saturation, brightness, keepAlpha);

    // Full pad repaint (gradient is cached) — partial dirty rects left edge artifacts.
    repaint();

    if (onColorChanged)
        onColorChanged (selectedColor);
}

void QuadPicker::hueChanged (float newHue)
{
    setHue (newHue);
}

void QuadPicker::setColor (const juce::Colour& newColor)
{
    // While the user is scrubbing the pad, ignore external setColor (feedback from
    // directColorUpdate / theme notify) so the ring tracks the mouse 1:1.
    if (draggingColour)
        return;

    float newHue = 0.0f, newSaturation = 0.0f, newBrightness = 0.0f;
    newColor.getHSB (newHue, newSaturation, newBrightness);

    selectedColor = newColor;
    hueColor = juce::Colour::fromHSV (newHue, 1.0f, 0.5f, 1.0f);

    if (hueColor != lastHueColor)
    {
        updateGradientImage();
        lastHueColor = hueColor;
    }

    syncPositionFromSelectedColor();
    repaint();
}

juce::Colour QuadPicker::getSelectedColor() const
{
    return selectedColor;
}

void QuadPicker::onElementSelected (const juce::String& name, const juce::Colour& color)
{
    juce::ignoreUnused (name);
    setColor (color);
}

void QuadPicker::updateGradientImage()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
    {
        gradientImage = {};
        return;
    }

    gradientImage = juce::Image (juce::Image::ARGB, w, h, true);
    juce::Graphics g (gradientImage);

    for (int y = 0; y < h; ++y)
    {
        // Match setSelectedPosition: top = bright, bottom = dark (HSB brightness).
        const float brightness = 1.0f - (float) y / (float) juce::jmax (1, h - 1);
        juce::ColourGradient gradient (hueColor.withSaturation (0.0f).withBrightness (brightness),
                                       0.0f, (float) y,
                                       hueColor.withSaturation (1.0f).withBrightness (brightness),
                                       (float) w, (float) y,
                                       false);
        g.setGradientFill (gradient);
        g.fillRect (0, y, w, 1);
    }
}
