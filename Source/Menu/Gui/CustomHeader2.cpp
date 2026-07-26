#include "CustomHeader2.h"
#include "../SharedResources.h"
#include <JuceHeader.h> /
#include <juce_core/juce_core.h> 
#include "shadows-main/shadows.h"

CustomHeader2::CustomHeader2(SharedResources& sharedResources)
    : sharedResources(sharedResources),
    customShadow(std::make_unique<shadows::StackShadow>(juce::Colours::black.withAlpha(0.65f), juce::Point<int>(0, 2), 6, 1))
{
    addMouseListener(this, true);
}

CustomHeader2::~CustomHeader2() {
    // Cleanup if needed
}

void CustomHeader2::paint(juce::Graphics& g) {
    // Define the corner size for the rounded header
    float cornerSize = 14.0f;

    // Get the bounds of the parent ListBox
    juce::Rectangle<int> parentBounds = getParentComponent()->getLocalBounds();

    juce::Rectangle<float> headerBounds = parentBounds.expanded(1.0f).toFloat();

    // Create a path for the rounded header based on the parent ListBox size
    juce::Path headerPath;

    headerPath.addRoundedRectangle(headerBounds, cornerSize);

    // Save the current graphics state
    g.saveState();

    // Set the clipping region to the header path
    g.reduceClipRegion(headerPath);


    // Your existing drawing code goes here...
    juce::Colour menuListBoxBackgroundGradientColor1 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor1;
    juce::Colour menuListBoxBackgroundGradientColor2 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor2;
    juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;
    juce::Colour headerLabelTextColor = sharedResources.sharedColors.menuLabelTextColor1;
    juce::Colour color1 = juce::Colour::fromRGB(60, 47, 39);
    juce::Colour color2 = juce::Colour::fromRGB(0, 0, 0);
    juce::Colour listBoxOutlineColor = juce::Colour::fromRGB(25, 30, 25);

    juce::ColourGradient gradient(
        menuListBoxBackgroundGradientColor1,
        juce::Point<float>(static_cast<float>(getWidth()) / 2.0f, static_cast<float>(getHeight() * -1.5)),
        menuListBoxBackgroundGradientColor2,
        juce::Point<float>(static_cast<float>(getWidth()) / 2, static_cast<float>(getHeight() * 1.5)),
        false);

    // Create and draw gradient with padding
    g.setGradientFill(gradient);
    g.fillRect(headerBounds);   

    customShadow->drawInnerShadowForPath(g, headerPath);


    // Set the font for the listbox items
    g.setFont(juce::Font("Lato Black", 14.0f, juce::Font::bold));

    // Draw the labels for each column
    g.setColour(headerLabelTextColor);
    g.drawText("Element Name", 15, 0, getWidth(), getHeight(), juce::Justification::centredLeft);
   
    // Restore the graphics state to clear the clipping region
    g.restoreState();
}


void CustomHeader2::resized() {

    repaint();
}