#include "CustomTextButton1.h"
#include "shadows-main/source/StackShadow.h"
#include "shadows-main/shadows.h"

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
    // Render the shadow based on the button path
   
    // Call the base class paint method to draw the rest of the button
    TextButton::paint(g);

    customShadow->drawInnerShadowForPath(g, buttonPath);

    // innerShadow.render(g, buttonPath);
}

void CustomTextButton::resized() {
    // Calculate the button bounds and update the path
    juce::Rectangle<float> buttonBounds = getLocalBounds().toFloat().reduced(0, 0);
    float cornerSize = 6.0f; // Same as in the LookAndFeel
    buttonPath.clear();
    buttonPath.addRoundedRectangle(buttonBounds.reduced(0, 0), cornerSize);
    buttonPath.createPathWithRoundedCorners(6.0f);

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

