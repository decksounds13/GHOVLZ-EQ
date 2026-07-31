#include "Menu.h"
#include "Gui/AppearanceComponent.h"
#include "Gui/SpectrumComponent.h"
#include "Gui/FftComponent.h"
#include "Gui/OscilloscopeSettingsComponent.h"
#include "Gui/GoniometerSettingsComponent.h"
#include "Gui/SpectrogramSettingsComponent.h"
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
    auto* goniometer = new GoniometerSettingsComponent (resources, state);
    auto* spectrogram = new SpectrogramSettingsComponent (resources, state);
    auto* levelMeters = new LevelMetersComponent (resources, state);
    tabBar.setLookAndFeel (&customTabBarLookAndFeel);
    tabBar.addTab ("Spectrum", juce::Colours::transparentBlack, spectrum, true);
    tabBar.addTab ("FFT", juce::Colours::transparentBlack, fft, true);
    tabBar.addTab ("Oscilloscope", juce::Colours::transparentBlack, oscilloscope, true);
    tabBar.addTab ("Goniometer", juce::Colours::transparentBlack, goniometer, true);
    tabBar.addTab ("Spectrogram", juce::Colours::transparentBlack, spectrogram, true);
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
    juce::ignoreUnused (colors);

    // Colours are already written into sharedResources; refresh LookAndFeel bindings.
    textButtonLookAndFeel.setButtonOutlineColor(sharedResources.sharedColors.menuThinBorderColor);
    textButtonLookAndFeel.setButtonTextColor(sharedResources.sharedColors.menuButtonTextColor1);
    textButtonLookAndFeel.setGradientColor1(sharedResources.sharedColors.menuButtonGradientColor1);
    textButtonLookAndFeel.setGradientColor2(sharedResources.sharedColors.menuButtonGradientColor2);

    tabBar.setColour(juce::TabbedComponent::outlineColourId, sharedResources.sharedColors.menuTabBarBorderColor);
    tabBar.repaint();

    if (appearanceComponentRef) {
        appearanceComponentRef->repaintNewPresetButton();
    }

    repaint();
}


