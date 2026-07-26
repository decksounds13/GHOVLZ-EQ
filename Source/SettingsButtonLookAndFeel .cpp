#include "SettingsButtonLookAndFeel.h"

SettingsButtonLookAndFeel::SettingsButtonLookAndFeel()
{
    // Initialization code (if any) can go here.
}

SettingsButtonLookAndFeel::~SettingsButtonLookAndFeel()
{
    // Destructor code (if any) can go here.
}

void SettingsButtonLookAndFeel::drawButtonBackground(juce::Graphics& g,
    juce::Button& button,
    const juce::Colour& backgroundColour,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    // Initialize custom colors
    juce::Colour lightGrey = juce::Colour::fromRGBA(120, 100, 68, 255);
    juce::Colour darkOrangeBrown = juce::Colour::fromRGBA(139, 69, 30, 255);
    juce::Colour mediumBrightOrange = juce::Colour::fromRGBA(160, 165, 30, 255);


    // Calculate reduced bounds for the button (80% of original size, centered)
    auto reducedBounds = button.getLocalBounds().toFloat().reduced(button.getWidth() * 0.1f,
        button.getHeight() * 0.1f);

    // Create a path for the shadow using the reduced bounds
    juce::Path shadowPath;
    float cornerSize = 2.0f; // Use the same corner radius as the button
    shadowPath.addRoundedRectangle(reducedBounds, cornerSize);

    // Render the shadow using the shadowPath
    // Assume shadow is an instance of a class that can render shadows given a path
    shadow.render(g, shadowPath);

    // Draw the button based on its state
    if (button.isMouseButtonDown()) {
        g.setColour(mediumBrightOrange);
    }
    else if (shouldDrawButtonAsHighlighted) {
        g.setColour(darkOrangeBrown);
    }
    else {
        g.setColour(lightGrey);
    }

    // Fill the button within the reduced bounds
    g.fillRoundedRectangle(reducedBounds, cornerSize);

    // Add a 2-pixel outline within the reduced bounds
    g.setColour(buttonBorderColor);
    g.drawRoundedRectangle(reducedBounds.reduced(1), cornerSize, 2.0f);
}

void SettingsButtonLookAndFeel::drawButtonText(juce::Graphics& g,
    juce::TextButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::Colour labelTextColor = juce::Colours::whitesmoke.withAlpha(0.8f);

    g.setColour(labelTextColor);

    int paddingX = button.getWidth() * 0.23;  // 10% horizontal padding
    int paddingY = button.getHeight() * 0.23; // 10% vertical padding
    int lineLength = button.getWidth() - 2 * paddingX;
    int lineHeight = 3;
    int gapBetweenLines = (button.getHeight() - 2 * paddingY - (3 * lineHeight)) / 2;

    for (int i = 0; i < 3; ++i)
    {
        g.fillRect(paddingX, paddingY + i * (lineHeight + gapBetweenLines), lineLength, lineHeight);
    }
}

