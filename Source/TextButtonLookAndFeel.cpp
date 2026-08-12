#include "TextButtonLookAndFeel.h"
#include "Menu/Gui/CustomTextButton1.h"

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

    customLabel.setFont(juce::Font("Lato Black", textSize, juce::Font::plain)); // Initialize the custom label
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

    auto* customButton = dynamic_cast<CustomTextButton*>(&button);
    if (customButton) {
        const auto& buttonPath = customButton->getButtonPath();  // Use the path from CustomTextButton

        // Render the shadow using the button path
        //shadow.render(g, buttonPath);

        // Set the gradient fill based on the button's state
        juce::ColourGradient gradient;
        if (button.isMouseButtonDown()) {
            gradient = juce::ColourGradient(gradientColor1.withMultipliedBrightness(1.7f),
                button.getLocalBounds().getCentreX(), button.getLocalBounds().getY(),
                gradientColor2.withMultipliedBrightness(1.7f),
                button.getLocalBounds().getCentreX(), button.getLocalBounds().getBottom(),
                false);
        }
        else if (shouldDrawButtonAsHighlighted) {
            gradient = juce::ColourGradient(gradientColor1.withMultipliedBrightness(1.4f),
                button.getLocalBounds().getCentreX(), button.getLocalBounds().getY(),
                gradientColor2.withMultipliedBrightness(1.35f),
                button.getLocalBounds().getCentreX(), button.getLocalBounds().getBottom(),
                false);
        }
        else {
            gradient = juce::ColourGradient(gradientColor1,
                button.getLocalBounds().getCentreX(), button.getLocalBounds().getY(),
                gradientColor2,
                button.getLocalBounds().getCentreX(), button.getLocalBounds().getBottom(),
                false);
        }

        g.setGradientFill(gradient);
        g.fillPath(buttonPath);
        return;
    }

    // Fallback for plain TextButtons (OptionBox M/S/L/R, Sat, etc.):
    // subtle top→bottom form (slight brighten / darken + desat) for soft 3D.
    auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
    juce::Colour fill = button.getToggleState()
                            ? button.findColour (juce::TextButton::buttonOnColourId)
                            : button.findColour (juce::TextButton::buttonColourId);

    if (shouldDrawButtonAsDown)
        fill = fill.brighter (0.14f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter (0.08f);

    auto top = fill.brighter (0.08f);
    float h = 0.0f, s = 0.0f, v = 0.0f;
    fill.getHSB (h, s, v);
    auto bottom = juce::Colour::fromHSV (h,
                                         juce::jlimit (0.0f, 1.0f, s * 0.90f),
                                         juce::jlimit (0.0f, 1.0f, v * 0.88f),
                                         fill.getFloatAlpha());
    juce::ColourGradient grad (top, bounds.getX(), bounds.getY(),
                               bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (bounds, 3.0f);

    // Prefer LAF outline colour when set (OptionBox themes optionBorder); else soft dark edge.
    auto outline = buttonOutlineColor;
    if (outline.getFloatAlpha() < 0.02f)
        outline = juce::Colours::black.withAlpha (0.55f);
    g.setColour (outline.withAlpha (juce::jmin (1.0f, outline.getFloatAlpha() * 0.95f)));
    g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
}

void TextButtonLookAndFeel::resized(juce::Component& component)
{

 
}

void TextButtonLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
    bool shouldDrawButtonAsHighlighted,
    bool shouldDrawButtonAsDown)
{
    // Save the original button text
    juce::String originalButtonText = button.getButtonText();

    // Use member color for text
    g.setColour(buttonTextColor);

    // Set the font for button text
    g.setFont(juce::Font("Lato Black", buttonTextSize, juce::Font::plain));

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
