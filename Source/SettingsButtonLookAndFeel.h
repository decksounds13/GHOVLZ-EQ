#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"

class SettingsButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SettingsButtonLookAndFeel();
    ~SettingsButtonLookAndFeel();

    void drawButtonBackground(juce::Graphics& g,
        juce::Button& button,
        const juce::Colour& backgroundColour,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;

    void drawButtonText(juce::Graphics& g,
        juce::TextButton& button,
        bool shouldDrawButtonAsHighlighted,
        bool shouldDrawButtonAsDown) override;
 
    const juce::Path& getShadowPath() const {
        return shadowPath;
    }
   
    juce::Path shadowPath;
    juce::Colour buttonBorderColor = juce::Colour::fromRGBA(15, 10, 10, 255.0f);

    melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha(0.75f), 12, { 0, 2 }, 0 } };
};
