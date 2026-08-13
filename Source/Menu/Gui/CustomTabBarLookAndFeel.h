#pragma once

#include <JuceHeader.h>
#include "../SharedResources.h"

/**
    Settings tab strip: theme-driven ink/active pill, widths sized to full labels
    (never ellipsis), active underline for the selected tab.
*/
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

    /** Full-label width used by the Settings pager (never ellipsis). */
    static int bestWidthForLabel (const juce::String& text) noexcept;

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override;

private:
    juce::Font tabFont() const
    {
        if (auto* active = SharedResources::getActive())
            return active->sharedColors.makeUiFont (13.0f);
        return SharedResources::uiFont (13.0f);
    }

    juce::Colour ink { juce::Colours::whitesmoke };
    juce::Colour activePill { juce::Colours::whitesmoke.withAlpha (0.12f) };
};
