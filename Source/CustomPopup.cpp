#include "CustomPopup.h"
#include "Menu/Gui/ThemeList.h"
#include "Menu/Gui/AppearanceComponent.h"
#include "Menu/SharedResources.h"

CustomPopup::CustomPopup()
    : currentMode(Normal),
    backgroundColor(juce::Colours::white),
    textColor(juce::Colours::black),
    displayDurationMs(500), // 1 second before fade out
    fadeOutDurationMs(500), // 2 seconds fade out
    alpha(1.0f),
    fadingOut(false) {
    setOpaque(false);
}

void CustomPopup::setMessage(const juce::String& newMessage) {
    message = newMessage;
    alpha = 1.0f;
    fadingOut = false;
    updateBackgroundBlur(); // Update the blur when showing the message
    repaint();
    startTimer(displayDurationMs); // Start timer for display duration
}

void CustomPopup::setMode(Mode mode) {
    currentMode = mode;
    repaint();
}

void CustomPopup::setColors(const juce::Colour& bg, const juce::Colour& /* outline */, const juce::Colour& text) {
    backgroundColor = bg;
    textColor = text;
    repaint();
}

void CustomPopup::setDisplayDuration(int durationMs) {
    displayDurationMs = durationMs;
}

void CustomPopup::paint(juce::Graphics& g) {
    // Define the radius for the rounded corners
    float cornerSize = 14.0f;

    // Create a path for the rounded rectangle (clipping region)
    juce::Path roundedRectPath;
    roundedRectPath.addRoundedRectangle(getLocalBounds().toFloat().reduced(0, 0), cornerSize);

    // Save the current graphics state
    g.saveState();

    // Set the clipping region to the path
    g.reduceClipRegion(roundedRectPath);

    if (alpha > 0) {
        // Draw the blurred background
        juce::Image blurredImage = blur.render();
        g.setOpacity(alpha);
        g.drawImageAt(blurredImage, 0, 0);

        // Overlay a semi-transparent black rectangle to darken the image
        g.setColour(juce::Colours::black.withAlpha(0.2f * alpha)); // Adjust alpha as needed
        g.fillRect(getLocalBounds());
    }

    // Restore the graphics state
    g.restoreState();

    // Draw the text
    g.setColour(textColor.withAlpha(alpha));
    g.setFont(SharedResources::uiFont (16.0f, true));
    g.drawText(message, getLocalBounds(), juce::Justification::centred, true);
}


void CustomPopup::timerCallback() {
    if (!fadingOut) {
        // Start fading out
        fadingOut = true;
        startTimer(500 / 30); // Approx. 30 fps for smooth fade out
    }
    else {
        // Fade out logic
        alpha -= 1.0f / (fadeOutDurationMs / (1000.0f / 30)); // Decrease alpha
        if (alpha <= 0) {
            alpha = 0;
            stopTimer();
            setVisible(false);
            fadingOut = false;
        }
        repaint();
    }
}



void CustomPopup::updateBackgroundBlur() {
    if (auto* appearanceComp = dynamic_cast<AppearanceComponent*>(getParentComponent())) {
        auto themesListBounds = appearanceComp->getThemeListBounds();
        blur.update(appearanceComp->createComponentSnapshot(themesListBounds));
    }
}