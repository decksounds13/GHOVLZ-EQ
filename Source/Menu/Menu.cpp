#include "Menu.h"
#include "Gui/AppearanceComponent.h"
#include "Gui/SpectrumComponent.h"
#include "Gui/FftComponent.h"
#include "Gui/OscilloscopeSettingsComponent.h"
#include "Gui/LevelMetersComponent.h"
#include "../Effects/shadows-main/shadows.h" 
#include "../Effects/shadows-main/source/StackShadow.h" 
#include "SharedResources.h"
#include <JuceHeader.h>

Menu::Menu(SharedResources& resources, juce::AudioProcessorValueTreeState& state, TextButtonLookAndFeel& lookAndFeel)
    : sharedResources(resources), quadPicker(resources), textButtonLookAndFeel(lookAndFeel) // This matches the member declaration
{
    juce::Colour menuBorderColor = sharedResources.sharedColors.menuTabBarBorderColor;


    // DBG("Menu constructor called");
    auto* appearance = new AppearanceComponent (resources, state);
    auto* spectrum = new SpectrumComponent (resources, state);
    auto* fft = new FftComponent (resources, state);
    auto* oscilloscope = new OscilloscopeSettingsComponent (resources, state);
    auto* levelMeters = new LevelMetersComponent (resources, state);
    tabBar.setLookAndFeel (&customTabBarLookAndFeel);
    tabBar.addTab ("Spectrum", juce::Colours::transparentBlack, spectrum, true);
    tabBar.addTab ("FFT", juce::Colours::transparentBlack, fft, true);
    tabBar.addTab ("Oscilloscope", juce::Colours::transparentBlack, oscilloscope, true);
    tabBar.addTab ("Level Meters", juce::Colours::transparentBlack, levelMeters, true);
    tabBar.addTab ("Appearance (WIP)", juce::Colours::transparentBlack, appearance, true);
    addAndMakeVisible (tabBar);
    tabBar.setTabBarDepth (35);
    tabBar.setOutline (4.0f);
    tabBar.setColour (juce::TabbedComponent::outlineColourId, menuBorderColor);
    tabBar.setCurrentTabIndex (0);

    appearanceComponentRef = appearance;
}

ThemeList* Menu::getThemeList() const noexcept
{
    return appearanceComponentRef != nullptr ? &appearanceComponentRef->getThemeList() : nullptr;
}

Menu::~Menu()
{
    tabBar.setLookAndFeel(nullptr);
}

void Menu::paint(juce::Graphics& g)
{
    juce::Colour menuBackgroundGradientColor1 = sharedResources.sharedColors.menuBackgroundGradientColor1;
   // DBG("Painting menu with color: " + menuBackgroundGradientColor1.toString());

    juce::Colour menuBackgroundGradientColor2 = sharedResources.sharedColors.menuBackgroundGradientColor2;

    juce::ColourGradient gradient(
        menuBackgroundGradientColor1,
        juce::Point<float>(static_cast<float>(getParentWidth()) / 2, 0.0f).translated(getX(), -1.5 * getY()),
        menuBackgroundGradientColor2,
        juce::Point<float>(static_cast<float>(getParentWidth()) / 2, static_cast<float>(getParentHeight() * 1.5)).translated(getX(), getY()),
        false
    );

    // Assuming 'gradient' is already defined as a ColourGradient object and set up appropriately
    juce::Graphics::ScopedSaveState state(g);  // Save the current state to restore later
    g.setGradientFill(gradient);
    // Define the corner radius for the rounded rectangle
    float cornerRadius = 14.0f; // for example, 10 pixels
    // Draw a rounded rectangle with the gradient fill
    g.fillRoundedRectangle(0, 0, getWidth(), getHeight(), cornerRadius);


}

void Menu::resized()
{
    tabBar.setBounds(0, 0, getWidth(), getHeight());

    juce::Colour menuBorderColor = sharedResources.sharedColors.menuTabBarBorderColor;
    tabBar.setColour(juce::TabbedComponent::outlineColourId, menuBorderColor);
    tabBar.repaint();  // Request that the tabBar is repainted
}

void Menu::buttonClicked(juce::Button* button)
{

}

void Menu::updateColors(const juce::Array<juce::Colour>& colors)
{
    // Ensure the indices match the intended order
    sharedResources.sharedColors.setMenuBackgroundGradientColor1(colors[0]); // Index 0 for "Menu Background"
    sharedResources.sharedColors.setMenuBackgroundGradientColor2(colors[1]); // Index 1 for "Menu Background 2"
    sharedResources.sharedColors.setMenuListBoxBackgroundGradientColor1(colors[2]); // Index 2 for "Menu ListBox Background"
    sharedResources.sharedColors.setMenuListBoxBackgroundGradientColor2(colors[3]); // Index 3 for "Menu ListBox Background 2"
    sharedResources.sharedColors.setMenuTabBarBorderColor(colors[4]);        // Index 4 for "Menu Border"
    sharedResources.sharedColors.setMenuThinBorderColor(colors[5]);          // Index 5 for "Menu Thin Border"
    sharedResources.sharedColors.setMenuButtonGradientColor1(colors[6]); // Index 6 for "Menu Button Gradient Color 1"
    sharedResources.sharedColors.setMenuButtonGradientColor2(colors[7]); // Index 7 for "Menu Button Gradient Color 2"
    sharedResources.sharedColors.setMenuButtonTextColor1(colors[8]); // Index 8 for "Menu Button Text Color 1"
    sharedResources.sharedColors.setMenuLabelTextColor1(colors[9]); // Index 9 for "Menu Label Text Color 1"
    sharedResources.sharedColors.setMenuScrollBarTrackColor1(colors[10]); // Index 10 for "Menu Scroll Bar Track Color 1"
    sharedResources.sharedColors.setMenuScrollBarThumbColor1(colors[11]); // Index 11 for "Menu Scroll Bar Thumb Color 1"
    sharedResources.sharedColors.setMenuScrollBarOutlineColor1(colors[12]); // Index 12 for "Menu Scroll Bar Outline Color 1"
    sharedResources.sharedColors.setMenuListBoxTextColor1(colors[13]); // Index 13 for "Menu ListBox Text Color 1"
    sharedResources.sharedColors.setMenuListBoxSelectionColor1(colors[14]); // Index 14 for "Menu ListBox Selection Color 1"
    sharedResources.sharedColors.setMenuTextBoxTextColor1(colors[15]); // Index 15 for "Menu TextBox Text Color 1"

    // Update the custom LookAndFeel with the new colors
    textButtonLookAndFeel.setButtonOutlineColor(colors[5]);

    textButtonLookAndFeel.setButtonTextColor(colors[9]);

    // Update tabBar colors and request a repaint
    tabBar.setColour(juce::TabbedComponent::outlineColourId, colors[4]);
    tabBar.repaint();

    // Trigger a repaint for the newPresetButton if appearanceComponentRef is valid
    if (appearanceComponentRef) {
        appearanceComponentRef->repaintNewPresetButton();
    }

    // Trigger a repaint for this component
    repaint();
}


