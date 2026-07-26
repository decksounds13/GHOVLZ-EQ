#pragma once
#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"

// Forward declaration
class SharedColors;

class TextButtonLookAndFeel2 : public juce::LookAndFeel_V4
{
public:
    explicit TextButtonLookAndFeel2(float textSize = 16.0f);
    virtual ~TextButtonLookAndFeel2();

    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
        const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;

    void applyThemeColors(const juce::Colour& color1, const juce::Colour& color2, const juce::Colour& outlineColor, const juce::Colour& textColor);

    void setTextSize(float textSize);

    void resized(juce::Component& component);


    // Setters
    void setButtonBackgroundColor(const juce::Colour& newColour);
    void setGradientColor1(const juce::Colour& newColour);
    void setGradientColor2(const juce::Colour& newColour);
    void setButtonTextColor(const juce::Colour& newColour);
    void setButtonOutlineColor(const juce::Colour& newColour);

private:
    juce::Colour buttonBackgroundColor;
    juce::Colour buttonTextColor;
    juce::Colour buttonOutlineColor;
    float buttonTextSize; // Store the text size
    juce::Label customLabel; // Declare CustomLabel as a member

    juce::Colour gradientColor1;
    juce::Colour gradientColor2;

    // juce::Path buttonPath;


     // This array holds the buttons that will use this custom look and feel
    juce::Array<juce::Button*> buttonsUsingCustomLookAndFeel2;
    melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha(0.05f), 5, { 0, 0 }, 0 } };
    melatonin::DropShadow buttonGlowShadow = { { juce::Colours::black.withAlpha(0.05f), 5, { 0, 0 }, 0 } };
    melatonin::InnerShadow innerShadow = { { juce::Colour::fromRGBA(0, 0, 0, 255), 18, {2, 5}} };


};
