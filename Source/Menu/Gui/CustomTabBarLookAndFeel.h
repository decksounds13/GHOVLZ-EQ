#pragma once

#include <JuceHeader.h>

class CustomTabBarLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomTabBarLookAndFeel();

    int getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth) override;

    void drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                        bool isMouseOver, bool isMouseDown) override;
};
