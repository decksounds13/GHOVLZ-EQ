#include "TextButtonLookAndFeel.h"
#include "Menu/Gui/CustomTextButton1.h"
#include "GraphOverlayButtonLookAndFeel.h"
#include "Menu/SharedResources.h"

TextButtonLookAndFeel::TextButtonLookAndFeel(float textSize)
    : buttonTextSize(textSize),
    customLabel(juce::String()),
    shadow({ {juce::Colours::black.withAlpha(0.75f), 8, {0, 2}, 1} }),
    innerShadow({ { juce::Colours::black, 3, { 0, 0 } } })
{
    // You can set default colors here
    buttonBackgroundColor = juce::Colour::fromRGBA(120, 100, 68, 255);
    buttonTextColor = juce::Colours::whitesmoke.withAlpha(0.8f);
    buttonOutlineColor = juce::Colours::black;

    customLabel.setFont (SharedResources::uiFont (textSize));
    customLabel.setColour(juce::Label::textColourId, buttonTextColor);
    customLabel.setJustificationType(juce::Justification::centred);

}

TextButtonLookAndFeel::~TextButtonLookAndFeel()
{
    // Destructor code (if any) can go here.
}

void TextButtonLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
    const juce::Colour& backgroundColour,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::Colour mediumBrightOrange = juce::Colour::fromRGBA(255, 210, 50, 200.0f);
    juce::ignoreUnused (mediumBrightOrange, backgroundColour);

    const float corner = GraphOverlayButtonLookAndFeel::cornerRadius();

    auto* customButton = dynamic_cast<CustomTextButton*>(&button);
    if (customButton) {
        const auto& buttonPath = customButton->getButtonPath();
        auto g1 = GraphOverlayButtonLookAndFeel::adjustForInteraction (
            gradientColor1, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown || button.isMouseButtonDown());
        auto g2 = GraphOverlayButtonLookAndFeel::adjustForInteraction (
            gradientColor2, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown || button.isMouseButtonDown());

        const bool hot = shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown
                         || button.isMouseButtonDown();
        if (GraphOverlayButtonLookAndFeel::shouldShowButtonGlow (hot))
            GraphOverlayButtonLookAndFeel::renderOutlineBlur (
                g, button.getLocalBounds().toFloat().reduced (0.5f), corner, g1.brighter (0.25f));

        juce::ColourGradient gradient (g1,
            button.getLocalBounds().getCentreX(), (float) button.getLocalBounds().getY(),
            g2,
            button.getLocalBounds().getCentreX(), (float) button.getLocalBounds().getBottom(),
            false);
        g.setGradientFill (gradient);
        g.fillPath (buttonPath);
        return;
    }

    // Fallback for plain TextButtons (OptionBox M/S/L/R, Sat, etc.).
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour fill = button.getToggleState()
                            ? button.findColour (juce::TextButton::buttonOnColourId)
                            : button.findColour (juce::TextButton::buttonColourId);

    fill = GraphOverlayButtonLookAndFeel::adjustForInteraction (fill,
                                                                shouldDrawButtonAsHighlighted,
                                                                shouldDrawButtonAsDown);
    GraphOverlayButtonLookAndFeel::paintChromeFace (
        g, bounds, fill, corner,
        shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown);

    // Prefer LAF outline colour when set (OptionBox themes optionBorder).
    auto outline = buttonOutlineColor;
    if (outline.getFloatAlpha() >= 0.02f)
    {
        g.setColour (outline.withAlpha (juce::jmin (1.0f, outline.getFloatAlpha() * 0.95f)));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }
}

void TextButtonLookAndFeel::resized(juce::Component& component)
{

 
}

void TextButtonLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
    juce::String originalButtonText = button.getButtonText();

    g.setColour(buttonTextColor);
    g.setFont (SharedResources::uiFont (buttonTextSize));

    // Never ellipsize chrome captions to "..." — layout must fit the full plain string.
    g.drawText (originalButtonText, button.getLocalBounds().reduced (2, 0),
                juce::Justification::centred, false);
}



void TextButtonLookAndFeel::setButtonBackgroundColor(const juce::Colour& newColour)
{
    buttonBackgroundColor = newColour;
}



void TextButtonLookAndFeel::setTextSize(float textSize)
{
    buttonTextSize = textSize;
    // Assuming you also want to update the font size of the custom label, if it exists
    customLabel.setFont(juce::Font(buttonTextSize)); // Update the font size for the custom label
    // Trigger a repaint for the label if needed
    customLabel.repaint();
}



void TextButtonLookAndFeel::setGradientColor1(const juce::Colour& newColour)
{
    gradientColor1 = newColour;
}

void TextButtonLookAndFeel::setGradientColor2(const juce::Colour& newColour)
{
    gradientColor2 = newColour;
}

void TextButtonLookAndFeel::setButtonOutlineColor(const juce::Colour& newColour)
{
    buttonOutlineColor = newColour;
}

void TextButtonLookAndFeel::setButtonTextColor(const juce::Colour& newColour)
{   
    buttonTextColor = newColour;
}

void TextButtonLookAndFeel::applyThemeColors(const juce::Colour& color1, const juce::Colour& color2, const juce::Colour& outlineColor, const juce::Colour& textColor)
{
    setGradientColor1(color1);
    setGradientColor2(color2);
    setButtonOutlineColor(outlineColor);
    setButtonTextColor(textColor);

    // Now trigger a repaint on all buttons using this look and feel
    for (auto* button : buttonsUsingCustomLookAndFeel) {
        button->setLookAndFeel(this); // Ensure the button is using the updated LookAndFeel
        button->repaint(); // Force the button to repaint with the new colors
    }
}
