#pragma once

#include <JuceHeader.h>
#include "../Menu/SharedResources.h"
#include "../TextButtonLookAndFeel.h"
#include "Gui/AppearanceComponent.h"
#include "../Menu/Gui/CustomTabBarLookAndFeel.h"
#include "../ColourRamp/ColourRampBank.h"
#include "MelatoninBlur/melatonin/shadows.h"

/**
    Floating settings panel: outer frame is freely movable/resizable (any aspect).
    Tab content stays at a fixed design size; a Viewport clips and scrolls.
*/
class Menu : public juce::Component,
             public juce::Button::Listener
{
public:
    static constexpr int kContentWidth = 800;          // 1200 * 2/3
    static constexpr int kContentHeight = 447;         // ~850 / 1.9
    static constexpr int kDragBarHeight = 24;

    Menu (SharedResources& resources,
          juce::AudioProcessorValueTreeState& state,
          TextButtonLookAndFeel& lookAndFeel,
          ColourRampBank& colourRamps);
    ~Menu() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void buttonClicked (juce::Button* button) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

    void updateColors (const juce::Array<juce::Colour>& colors);

    void setAppearanceComponentRef (AppearanceComponent& component);

    AppearanceComponent* getAppearanceComponent() const noexcept { return appearanceComponentRef; }
    ThemeList* getThemeList() const noexcept;

    /** Fixed design size of the scrolled content (not the outer frame). */
    static juce::Rectangle<int> getContentDesignBounds() noexcept
    {
        return { 0, 0, kContentWidth, kContentHeight };
    }

private:
    bool isInDragBar (juce::Point<int> localPos) const noexcept;

    SharedResources& sharedResources;
    TextButtonLookAndFeel& textButtonLookAndFeel;
    CustomTabBarLookAndFeel customTabBarLookAndFeel;

    AppearanceComponent* appearanceComponentRef = nullptr;

    juce::Viewport viewport;
    juce::Component contentPanel;
    juce::TabbedComponent tabBar { juce::TabbedButtonBar::TabsAtTop };

    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;
    juce::ComponentDragger dragger;
    bool dragging = false;

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Menu)
};
