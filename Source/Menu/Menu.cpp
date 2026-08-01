#include "Menu.h"
#include "Gui/AppearanceComponent.h"
#include "Gui/SpectrumComponent.h"
#include "Gui/FftComponent.h"
#include "Gui/OscilloscopeSettingsComponent.h"
#include "Gui/GoniometerSettingsComponent.h"
#include "Gui/SpectrogramSettingsComponent.h"
#include "Gui/LevelMetersComponent.h"
#include "SharedResources.h"
#include <JuceHeader.h>

Menu::Menu (SharedResources& resources,
            juce::AudioProcessorValueTreeState& state,
            TextButtonLookAndFeel& lookAndFeel,
            ColourRampBank& colourRamps)
    : sharedResources (resources),
      textButtonLookAndFeel (lookAndFeel)
{
    juce::Colour menuBorderColor = sharedResources.sharedColors.menuTabBarBorderColor;

    auto* appearance = new AppearanceComponent (resources, state);
    auto* spectrum = new SpectrumComponent (resources, state, colourRamps);
    auto* fft = new FftComponent (resources, state, colourRamps);
    auto* oscilloscope = new OscilloscopeSettingsComponent (resources, state);
    auto* goniometer = new GoniometerSettingsComponent (resources, state);
    auto* spectrogram = new SpectrogramSettingsComponent (resources, state, colourRamps);
    auto* levelMeters = new LevelMetersComponent (resources, state);

    tabBar.setLookAndFeel (&customTabBarLookAndFeel);
    tabBar.addTab ("Spectrum", juce::Colours::transparentBlack, spectrum, true);
    tabBar.addTab ("FFT", juce::Colours::transparentBlack, fft, true);
    tabBar.addTab ("Oscilloscope", juce::Colours::transparentBlack, oscilloscope, true);
    tabBar.addTab ("Goniometer", juce::Colours::transparentBlack, goniometer, true);
    tabBar.addTab ("Spectrogram", juce::Colours::transparentBlack, spectrogram, true);
    tabBar.addTab ("Level Meters", juce::Colours::transparentBlack, levelMeters, true);
    tabBar.addTab ("Appearance (WIP)", juce::Colours::transparentBlack, appearance, true);
    tabBar.setTabBarDepth (35);
    tabBar.setOutline (4.0f);
    tabBar.setColour (juce::TabbedComponent::outlineColourId, menuBorderColor);
    tabBar.setCurrentTabIndex (0);

    contentPanel.setSize (kContentWidth, kContentHeight);
    contentPanel.addAndMakeVisible (tabBar);
    tabBar.setBounds (contentPanel.getLocalBounds());

    viewport.setViewedComponent (&contentPanel, false);
    viewport.setScrollBarsShown (true, true);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    addAndMakeVisible (viewport);

    constrainer.setMinimumSize (200, 140);
    constrainer.setMaximumSize (4000, 4000);
    constrainer.setMinimumOnscreenAmounts (kDragBarHeight, 40, 40, 40);
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
    addAndMakeVisible (*resizer);

    appearanceComponentRef = appearance;

    setPaintingIsUnclipped (true);
    setSize (kContentWidth, kContentHeight + kDragBarHeight);
}

ThemeList* Menu::getThemeList() const noexcept
{
    return appearanceComponentRef != nullptr ? &appearanceComponentRef->getThemeList() : nullptr;
}

Menu::~Menu()
{
    tabBar.setLookAndFeel (nullptr);
}

bool Menu::isInDragBar (juce::Point<int> localPos) const noexcept
{
    return localPos.y >= 0 && localPos.y < kDragBarHeight;
}

void Menu::mouseDown (const juce::MouseEvent& e)
{
    dragging = isInDragBar (e.getPosition()) && ! e.mods.isPopupMenu();
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void Menu::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        dragger.dragComponent (this, e, &constrainer);
}

void Menu::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

void Menu::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isInDragBar (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                   : juce::MouseCursor::NormalCursor);
}

void Menu::paint (juce::Graphics& g)
{
    const auto& c1 = sharedResources.sharedColors.menuBackgroundGradientColor1;
    const auto& c2 = sharedResources.sharedColors.menuBackgroundGradientColor2;

    juce::Path panelPath;
    panelPath.addRoundedRectangle (0.0f, 0.0f, (float) getWidth(), (float) getHeight(), 14.0f);
    if (SharedResources::glowShadowEffectsEnabled())
        panelShadow.render (g, panelPath);

    juce::ColourGradient gradient (
        c1,
        juce::Point<float> ((float) getWidth() * 0.5f, 0.0f),
        c2,
        juce::Point<float> ((float) getWidth() * 0.5f, (float) getHeight()),
        false);

    g.setGradientFill (gradient);
    g.fillPath (panelPath);

    // Drag bar
    auto bar = getLocalBounds().removeFromTop (kDragBarHeight).toFloat();
    g.setColour (c1.brighter (0.08f).withAlpha (0.9f));
    g.fillRoundedRectangle (bar.getX() + 1.0f, bar.getY() + 1.0f,
                            bar.getWidth() - 2.0f, bar.getHeight() + 8.0f, 12.0f);

    g.setColour (sharedResources.sharedColors.menuLabelTextColor1.withAlpha (0.9f));
    g.setFont (juce::FontOptions().withHeight (13.0f).withStyle ("Bold"));
    g.drawText ("Settings",
                getLocalBounds().removeFromTop (kDragBarHeight).reduced (10, 0),
                juce::Justification::centredLeft,
                false);

    g.setColour (sharedResources.sharedColors.menuTabBarBorderColor.withAlpha (0.55f));
    g.drawRoundedRectangle (0.5f, 0.5f, (float) getWidth() - 1.0f, (float) getHeight() - 1.0f, 14.0f, 1.5f);
}

void Menu::resized()
{
    auto r = getLocalBounds();
    r.removeFromTop (kDragBarHeight);
    viewport.setBounds (r);

    // Content stays at design size — window resize only clips via the viewport.
    contentPanel.setSize (kContentWidth, kContentHeight);
    tabBar.setBounds (contentPanel.getLocalBounds());

    tabBar.setColour (juce::TabbedComponent::outlineColourId,
                      sharedResources.sharedColors.menuTabBarBorderColor);

    if (resizer != nullptr)
    {
        constexpr int grip = 18;
        resizer->setBounds (getWidth() - grip, getHeight() - grip, grip, grip);
        resizer->toFront (false);
    }
}

void Menu::buttonClicked (juce::Button* button)
{
    juce::ignoreUnused (button);
}

void Menu::updateColors (const juce::Array<juce::Colour>& colors)
{
    juce::ignoreUnused (colors);

    textButtonLookAndFeel.setButtonOutlineColor (sharedResources.sharedColors.menuThinBorderColor);
    textButtonLookAndFeel.setButtonTextColor (sharedResources.sharedColors.menuButtonTextColor1);
    textButtonLookAndFeel.setGradientColor1 (sharedResources.sharedColors.menuButtonGradientColor1);
    textButtonLookAndFeel.setGradientColor2 (sharedResources.sharedColors.menuButtonGradientColor2);

    tabBar.setColour (juce::TabbedComponent::outlineColourId, sharedResources.sharedColors.menuTabBarBorderColor);
    tabBar.repaint();

    if (appearanceComponentRef != nullptr)
        appearanceComponentRef->repaintNewPresetButton();

    repaint();
}

void Menu::setAppearanceComponentRef (AppearanceComponent& component)
{
    appearanceComponentRef = &component;
}
