#include "TextButtonLookAndFeel2.h"


TextButtonLookAndFeel2::TextButtonLookAndFeel2(float textSize)
    : buttonTextSize(textSize),
    customLabel(juce::String()),
    shadow({ {juce::Colours::black.withAlpha(0.75f), 8, {0, 0}, 3} }),
    innerShadow({ { juce::Colours::black, 3, { 0, 0 } } })
{
    // You can set default colors here
    buttonBackgroundColor = juce::Colour::fromRGBA(120, 100, 68, 255);
    buttonTextColor = juce::Colours::whitesmoke.withAlpha(0.8f);
    buttonOutlineColor = juce::Colours::black;

    customLabel.setFont(juce::Font("Lato Black", textSize, juce::Font::plain)); // Initialize the custom label
    customLabel.setColour(juce::Label::textColourId, buttonTextColor);
    customLabel.setJustificationType(juce::Justification::centred);
}

TextButtonLookAndFeel2::~TextButtonLookAndFeel2()
{
}

void TextButtonLookAndFeel2::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour& backgroundColour,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{

}

void TextButtonLookAndFeel2::resized(juce::Component& component)
{
}

void TextButtonLookAndFeel2::drawButtonText(juce::Graphics& g, juce::TextButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::String originalButtonText = button.getButtonText();

    // Original button bounds
    juce::Rectangle<int> buttonBounds = button.getLocalBounds().toFloat().toNearestInt();

    // Adjust the text size for different states
    float textSize = buttonTextSize;
    if (shouldDrawButtonAsHighlighted) {
        textSize *= 1.2f; // Slightly larger text when hovered
    }

    // Check if the button is toggled
    if (button.getToggleState()) {
        textSize *= 1.4f; // Larger and brighter text when toggled on
    }
    else if (shouldDrawButtonAsDown) {
        textSize *= 1.4f; // Even larger text when pressed (non-toggled state)
    }

    // Calculate the size difference
    float sizeDifference = textSize - buttonTextSize;

    // Adjust the text brightness for different states
    float brightnessMultiplier = button.getToggleState() ? 1.8f : (shouldDrawButtonAsHighlighted ? 1.4f : 1.0f);
    juce::Colour currentTextColor = buttonTextColor.withMultipliedBrightness(brightnessMultiplier);

    g.setColour(currentTextColor);
    g.setFont(juce::Font("Lato Black", textSize, juce::Font::plain));

    // Calculate the centered position for the text
    juce::Rectangle<float> textBounds(buttonBounds.getX(), buttonBounds.getY() + (sizeDifference / 2),
        buttonBounds.getWidth(), buttonBounds.getHeight() - sizeDifference);

    g.drawText(originalButtonText, textBounds.toNearestInt(), juce::Justification::centred, true);
}


void TextButtonLookAndFeel2::setButtonBackgroundColor(const juce::Colour& newColour)
{
    buttonBackgroundColor = newColour;
}

void TextButtonLookAndFeel2::setTextSize(float textSize)
{
    buttonTextSize = textSize;
    // Assuming you also want to update the font size of the custom label, if it exists
    customLabel.setFont(juce::Font(buttonTextSize)); // Update the font size for the custom label
    // Trigger a repaint for the label if needed
    customLabel.repaint();
}

void TextButtonLookAndFeel2::setGradientColor1(const juce::Colour& newColour)
{
    gradientColor1 = newColour;
}

void TextButtonLookAndFeel2::setGradientColor2(const juce::Colour& newColour)
{
    gradientColor2 = newColour;
}

void TextButtonLookAndFeel2::setButtonOutlineColor(const juce::Colour& newColour)
{
    buttonOutlineColor = newColour;
}

void TextButtonLookAndFeel2::setButtonTextColor(const juce::Colour& newColour)
{
    buttonTextColor = newColour;
}

void TextButtonLookAndFeel2::applyThemeColors(const juce::Colour& color1, const juce::Colour& color2, const juce::Colour& outlineColor, const juce::Colour& textColor)
{
    setGradientColor1(color1);
    setGradientColor2(color2);
    setButtonOutlineColor(outlineColor);
    setButtonTextColor(textColor);

    // Now trigger a repaint on all buttons using this look and feel
    for (auto* button : buttonsUsingCustomLookAndFeel2) {
        button->setLookAndFeel(this); // Ensure the button is using the updated LookAndFeel
        button->repaint(); // Force the button to repaint with the new colors
    }
}
