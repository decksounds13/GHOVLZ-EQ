#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "../Menu/SharedResources.h"
#include "../TextButtonLookAndFeel.h"
#include "Gui/AppearanceComponent.h"
#include "Gui/CustomScrollBar.h"
#include "../Menu/Gui/CustomTabBarLookAndFeel.h"
#include "../ColourRamp/ColourRampBank.h"
#include "MelatoninBlur/melatonin/shadows.h"

/**
    Floating settings panel: outer frame is freely movable/resizable (any aspect).
    Tab content grows to the active tab's preferred height; one outer Viewport scrolls.
*/
class Menu : public juce::Component,
             public juce::Button::Listener
{
public:
    static constexpr int kContentWidth = 533;          // previous 800 * 2/3 — leaves ~half window for maximized scopes
    static constexpr int kContentHeight = 447;         // ~850 / 1.9
    static constexpr int kDragBarHeight = 24;
    static constexpr int kScrollBarThickness = 11;
    static constexpr int kTabsPerPage = 6;

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
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;

    void updateColors (const juce::Array<juce::Colour>& colors);

    void setAppearanceComponentRef (AppearanceComponent& component);

    AppearanceComponent* getAppearanceComponent() const noexcept { return appearanceComponentRef; }
    ThemeList* getThemeList() const noexcept;
    /** Push Spec3D DOF focus into the settings slider (Ctrl/Cmd+LMB pick). */
    void syncSpec3DDofFocusFromMain();
    /** Push Spec3D debug-sphere pose into the 3D Debug tab (gizmo drag). */
    void syncSpec3DDebugSphereFromMain();
    /** Push full Spec3D look/structure into the 3D Spectrogram settings tab. */
    void syncSpec3DSettingsFromMain();

    /** Fixed design size of the scrolled content (not the outer frame). */
    static juce::Rectangle<int> getContentDesignBounds() noexcept
    {
        return { 0, 0, kContentWidth, kContentHeight };
    }

    /** Call when an active tab's preferred height changes (look toggles, gradients, …). */
    void notifyContentHeightChanged();

    /** Invoked by the title-bar close (X) control. Host should hide the Settings panel. */
    std::function<void()> onCloseRequest;

private:
    /** Chevron pager for tab pages (same idea as faceplate bank arrows). */
    class TabPageArrowButton : public juce::Button
    {
    public:
        explicit TabPageArrowButton (bool pointRightIn)
            : juce::Button (pointRightIn ? "tabNext" : "tabPrev"),
              pointRight (pointRightIn)
        {
            setClickingTogglesState (false);
        }

        void setChromeColours (juce::Colour fill, juce::Colour ink) noexcept
        {
            fillColour = fill;
            inkColour = ink;
            repaint();
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (0.5f);
            auto fill = fillColour;
            auto ink = inkColour;
            if (! isEnabled())
            {
                fill = fill.withMultipliedAlpha (0.45f);
                ink = ink.withMultipliedAlpha (0.4f);
            }
            else if (down)
            {
                fill = fill.brighter (0.15f);
                ink = ink.brighter (0.1f);
            }
            else if (highlighted)
            {
                fill = fill.brighter (0.1f);
                ink = ink.brighter (0.08f);
            }

            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 3.0f);
            g.setColour (ink.withAlpha (0.35f));
            g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float h = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.22f;
            juce::Path chevron;
            if (pointRight)
            {
                chevron.startNewSubPath (cx - h * 0.55f, cy - h);
                chevron.lineTo (cx + h * 0.55f, cy);
                chevron.lineTo (cx - h * 0.55f, cy + h);
            }
            else
            {
                chevron.startNewSubPath (cx + h * 0.55f, cy - h);
                chevron.lineTo (cx - h * 0.55f, cy);
                chevron.lineTo (cx + h * 0.55f, cy + h);
            }
            g.setColour (ink);
            g.strokePath (chevron, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }

    private:
        bool pointRight = false;
        juce::Colour fillColour { juce::Colours::black.withAlpha (0.35f) };
        juce::Colour inkColour { juce::Colours::whitesmoke.withAlpha (0.9f) };
    };

    struct TabEntry
    {
        juce::String name;
        juce::Component* content = nullptr; // owned by ownedTabContents
    };

    bool isInDragBar (juce::Point<int> localPos) const noexcept;
    void syncScrollBarColours();
    void layoutScrollBars();
    void rebuildTabsForCurrentPage();
    void setTabPage (int page);
    int getNumTabPages() const noexcept;
    /** @param preserveScrollPosition keep viewport offset (toggles / resize). Tab changes pass false. */
    void refreshContentPanelSize (bool preserveScrollPosition = false);
    int getActiveTabPreferredContentHeight() const;
    /** Sliders would eat the wheel for value tweaks — prefer scrolling the Settings page. */
    static void disableSliderScrollWheelRecursive (juce::Component& root);

    /** Notifies Menu when the active settings tab changes so contentPanel can resize. */
    class MenuTabbedComponent : public juce::TabbedComponent
    {
    public:
        explicit MenuTabbedComponent (Menu& ownerIn)
            : juce::TabbedComponent (juce::TabbedButtonBar::TabsAtTop),
              owner (ownerIn)
        {
        }

        void currentTabChanged (int newCurrentTabIndex, const juce::String& newTabName) override
        {
            juce::ignoreUnused (newCurrentTabIndex, newTabName);
            // New tab: jump to top (don't preserve prior tab's scroll).
            owner.refreshContentPanelSize (false);
            // Tabs that sync Look toggles from prefs during layout can grow after the
            // first measure — re-measure once the message queue settles so the scrollbar
            // covers the full page without requiring a manual resize.
            juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Menu> (&owner)]
            {
                if (safe != nullptr)
                    safe->refreshContentPanelSize (false);
            });
        }

    private:
        Menu& owner;
    };

    SharedResources& sharedResources;
    TextButtonLookAndFeel& textButtonLookAndFeel;
    CustomTabBarLookAndFeel customTabBarLookAndFeel;

    AppearanceComponent* appearanceComponentRef = nullptr;

    juce::Viewport viewport;
    juce::Component contentPanel;
    MenuTabbedComponent tabBar;
    std::unique_ptr<CustomScrollBar> verticalScrollBar;
    std::unique_ptr<CustomScrollBar> horizontalScrollBar;

    std::vector<std::unique_ptr<juce::Component>> ownedTabContents;
    std::vector<TabEntry> allTabs;
    int tabPageIndex = 0;
    TabPageArrowButton tabPrevButton { false };
    TabPageArrowButton tabNextButton { true };

    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> borderResizer;
    juce::ComponentDragger dragger;
    bool dragging = false;

    juce::TextButton closeButton;
    void layoutCloseButton() noexcept;
    void styleCloseButton() noexcept;

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Menu)
};
