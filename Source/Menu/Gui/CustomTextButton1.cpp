#include "CustomTextButton1.h"
#include "shadows-main/source/StackShadow.h"
#include "shadows-main/shadows.h"
#include "../../GraphOverlayButtonLookAndFeel.h"

CustomTextButton::CustomTextButton(const juce::String& buttonText)
    : juce::TextButton(buttonText),
    customShadow(std::make_unique<shadows::StackShadow>(juce::Colours::black.withAlpha(0.65f), juce::Point<int>(0, 2), 6, 1)) 
{
    setLookAndFeel(&customLookAndFeel);
}

CustomTextButton::~CustomTextButton() {
    setLookAndFeel(nullptr);
}

void CustomTextButton::paint(juce::Graphics& g) {
    // Keep path corner radius in sync with Appearance (live slider).
    rebuildButtonPath();

    // Call the base class paint method to draw the rest of the button
    TextButton::paint(g);

    customShadow->drawInnerShadowForPath(g, buttonPath);
}

void CustomTextButton::rebuildButtonPath()
{
    juce::Rectangle<float> buttonBounds = getLocalBounds().toFloat();
    const float cornerSize = GraphOverlayButtonLookAndFeel::cornerRadius();
    buttonPath.clear();
    buttonPath.addRoundedRectangle (buttonBounds, cornerSize);
}

void CustomTextButton::resized() {
    rebuildButtonPath();
}

void CustomTextButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onPopupMenu != nullptr)
            onPopupMenu();
        return;
    }

    juce::TextButton::mouseDown (e);
}

