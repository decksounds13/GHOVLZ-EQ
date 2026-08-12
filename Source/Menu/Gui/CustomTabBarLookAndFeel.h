#pragma once

#include <JuceHeader.h>

class CustomTabBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomTabBarLookAndFeel();

    void setTabColours (juce::Colour labelInk, juce::Colour activeFill) noexcept
    {
        ink = labelInk;
        activePill = activeFill;
    }

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override;

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override;

private:
    juce::Font tabFont() const
    {
        return juce::Font (juce::FontOptions ("Lato Black", 13.0f, juce::Font::plain));
    }

    juce::Colour ink { juce::Colours::whitesmoke };
    juce::Colour activePill { juce::Colours::whitesmoke.withAlpha (0.12f) };
};
