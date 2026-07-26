#include "CustomHeader.h"
#include "../SharedResources.h"
#include <JuceHeader.h> /
#include <juce_core/juce_core.h> 

CustomHeader::CustomHeader(ThemeList* owner, SharedResources* sharedResources)
    : themeList(owner), sharedResources(sharedResources),
    customShadow(std::make_unique<shadows::StackShadow>(juce::Colours::black.withAlpha(0.65f), juce::Point<int>(0, 2), 6, 1))
{
    addMouseListener(this, true);
}

CustomHeader::~CustomHeader() {
    // Cleanup if needed
}

void CustomHeader::paint(juce::Graphics& g) {
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
    juce::Colour menuListBoxBackgroundGradientColor1 = sharedResources->sharedColors.menuListBoxBackgroundGradientColor1;
    juce::Colour menuListBoxBackgroundGradientColor2 = sharedResources->sharedColors.menuListBoxBackgroundGradientColor2;
    juce::Colour menuThinBorderColor = sharedResources->sharedColors.menuThinBorderColor;
    juce::Colour headerLabelTextColor = sharedResources->sharedColors.menuLabelTextColor1;
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
    g.drawText("Name", 0, 0, col1Width, getHeight(), juce::Justification::centred);
    g.drawText("Created", col1Width, 0, col2Width, getHeight(), juce::Justification::centred);
    g.drawText("Modified", col1Width + col2Width, 0, col3Width, getHeight(), juce::Justification::centred);

    // Draw separators
    g.setColour(juce::Colours::darkgrey);
    g.drawLine(col1Width, 0, col1Width, getHeight());
    g.drawLine(col1Width + col2Width, 0, col1Width + col2Width, getHeight());

    // Restore the graphics state to clear the clipping region
    g.restoreState();
}



void CustomHeader::mouseDrag(const juce::MouseEvent& event) {
    const int minSeparatorDistance = 15; // Minimum distance between separators
    const int edgePadding = 15; // Minimum distance from window edges

    int mouseX = event.getDistanceFromDragStartX() + event.getMouseDownX();
    int totalWidth = getWidth();

    // Separator positions
    int firstSeparatorPosition = col1Width;
    int secondSeparatorPosition = col1Width + col2Width;

    if (!isDraggingFirstSeparator && !isDraggingSecondSeparator) {
        // Determine which separator is being dragged
        if (std::abs(mouseX - firstSeparatorPosition) <= 5) {
            isDraggingFirstSeparator = true;
        }
        else if (std::abs(mouseX - secondSeparatorPosition) <= 5) {
            isDraggingSecondSeparator = true;
        }
    }

    if (isDraggingFirstSeparator) {
        // Ensure first separator respects the padding from left edge and second separator
        int newFirstSeparatorPosition = std::clamp(mouseX, edgePadding, secondSeparatorPosition - minSeparatorDistance);
        col1Width = newFirstSeparatorPosition;
        col2Width = secondSeparatorPosition - newFirstSeparatorPosition;
    }
    else if (isDraggingSecondSeparator) {
        // Ensure second separator respects the padding from right edge and first separator
        int newSecondSeparatorPosition = std::clamp(mouseX, firstSeparatorPosition + minSeparatorDistance, totalWidth - edgePadding);
        col2Width = newSecondSeparatorPosition - firstSeparatorPosition;
        col3Width = totalWidth - newSecondSeparatorPosition;
    }

    repaint();
}



void CustomHeader::mouseUp(const juce::MouseEvent& event) {
    // Reset dragging flags
    isDraggingFirstSeparator = false;
    isDraggingSecondSeparator = false;
}





void CustomHeader::mouseMove(const juce::MouseEvent& event) {
    // Define a margin within which the cursor will change to the resize cursor
    const int resizeMargin = 5; // pixels

    // Get the current mouse x position
    int mouseX = event.x;

    bool nearSeparator = (std::abs(mouseX - col1Width) <= resizeMargin) || (std::abs(mouseX - (col1Width + col2Width)) <= resizeMargin);
    //DBG("Mouse Move - X: " << mouseX << ", Near Separator: " << (nearSeparator ? "true" : "false"));

    // Check if the mouse is near the first separator
    if (std::abs(mouseX - col1Width) <= resizeMargin ||
        std::abs(mouseX - (col1Width + col2Width)) <= resizeMargin) {
        // Mouse is near a separator, show the resize cursor
        setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
    }
    else {
        // Mouse is not near a separator, show the normal cursor
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }


}


void CustomHeader::setColumnWidths(int col1, int col2, int col3) {
    //this->col1Width = col1; // Update member variable with parameter value
    //this->col2Width = col2; // Update member variable with parameter value
    //this->col3Width = col3; // Update member variable with parameter value
    //repaint(); // Repaint to update the header with new column widths
}

juce::Array<int> CustomHeader::getColumnWidths() const {
    return { col1Width, col2Width, col3Width };
}

void CustomHeader::resized() {
    int parentWidth = getWidth();

    // Set column widths based on percentages of the parent's width
    col1Width = static_cast<int>(parentWidth * 0.3); // 40% of parent width
    col2Width = static_cast<int>(parentWidth * 0.35); // 30% of parent width
    col3Width = parentWidth - col1Width - col2Width; // Remaining width

    // Now that the widths are updated, call repaint to reflect changes
    repaint();
}