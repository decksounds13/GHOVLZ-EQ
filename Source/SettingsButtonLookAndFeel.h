#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"
#include "Menu/SharedResources.h"

class SettingsButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SettingsButtonLookAndFeel();
    ~SettingsButtonLookAndFeel() override;

    void setThemeColors (SharedResources* r) noexcept
    {
        themeColors = r;
    }

    void drawButtonBackground (juce::Graphics& g,
                               juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics& g,
                         juce::TextButton& button,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    const juce::Path& getShadowPath() const { return shadowPath; }

    juce::Path shadowPath;
    melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha (0.75f), 12, { 0, 2 }, 0 } };

private:
    const SharedColors& palette() const noexcept
    {
        if (themeColors != nullptr)
            return themeColors->sharedColors;
        if (auto* active = SharedResources::getActive())
            return active->sharedColors;
        static const SharedColors defaults;
        return defaults;
    }

    SharedResources* themeColors = nullptr;
};
