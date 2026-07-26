#pragma once

#include <JuceHeader.h>
#include "Gui/QuadPicker.h"
#include "../Menu/SharedResources.h"
#include "../TextButtonLookAndFeel.h"
#include "Gui/AppearanceComponent.h"
#include "../Menu/Gui/CustomTabBarLookAndFeel.h" 

class Menu : public juce::Component,
             public juce::Button::Listener
{
public:
    // Fixed constructor declaration
    Menu(SharedResources& resources, juce::AudioProcessorValueTreeState& state, TextButtonLookAndFeel& lookAndFeel);
    ~Menu() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void buttonClicked(juce::Button* button) override;

    // Update the function signature to accept an array of colors
    void updateColors(const juce::Array<juce::Colour>& colors);

    void setAppearanceComponentRef(AppearanceComponent& component);

    AppearanceComponent* getAppearanceComponent() const noexcept { return appearanceComponentRef; }
    ThemeList* getThemeList() const noexcept;

private:
    juce::TextButton appearanceButton{ "Appearance (WIP)" };
    juce::TextButton behaviorButton{ "Behavior" };
    juce::TextButton aboutButton{ "About" };

    QuadPicker quadPicker;

    SharedResources& sharedResources;

    // Fixed member variable name to match the previous usage
    TextButtonLookAndFeel& textButtonLookAndFeel;
    CustomTabBarLookAndFeel customTabBarLookAndFeel;

    AppearanceComponent* appearanceComponentRef = nullptr;

    juce::TabbedComponent tabBar{ juce::TabbedButtonBar::TabsAtTop };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Menu)
};
