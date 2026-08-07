#include "MainComponent.h"
#include "ColourRamp/Spec3DRampTimelineWindow.h"
#include "Export/Spec3DExportSandbox.h"
#include "Export/Spec3DExportSettings.h"
#include "Export/Spec3DExportJob.h" // kept; run() is sandboxed when SPEC3D_EXPORT_ENABLED=0
#include <JuceHeader.h>
#include "FrequencyResponseComponent.h"
#include "EqEditor.h"
#include "Visualizer/Analyser.h"
#include "ComboBoxLookAndFeel.h"
#include "EqPresetStore.h"
#include "ModuleLookPresets.h"
#include "Menu/AnalyserDefaults.h"
#include "Menu/Gui/ThemeList.h"
#include "ColourRamp/GradientStripEditor.h"
#include <functional>
#include <algorithm>
#include <cmath>
#include <initializer_list>

// ── True OS fullscreen host for Spec3D (F11) ────────────────────────────────

struct MainComponent::Spec3DOsFullscreenHost final : public juce::Component
{
    MainComponent& owner;

    explicit Spec3DOsFullscreenHost (MainComponent& o) : owner (o)
    {
        setOpaque (true);
        setWantsKeyboardFocus (true);
        setMouseClickGrabsKeyboardFocus (true);
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::black);
    }

    void resized() override
    {
        if (auto* view = findChildWithID ("spec3dFsView"))
            view->setBounds (getLocalBounds());
        else if (getNumChildComponents() > 0)
            if (auto* c = getChildComponent (0))
                if (dynamic_cast<Spectrogram3DComponent*> (c) != nullptr)
                    c->setBounds (getLocalBounds());
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::F11Key || key == juce::KeyPress::escapeKey)
        {
            owner.setSpec3DFullscreen (false, true);
            return true;
        }
        return false;
    }

    void attachView (Spectrogram3DComponent& view)
    {
        // Monitor that contains the plugin editor (multi-monitor safe).
        const auto pluginScreen = owner.localAreaToGlobal (owner.getLocalBounds());
        const auto* display = juce::Desktop::getInstance().getDisplays()
                                  .getDisplayForRect (pluginScreen);
        if (display == nullptr)
            display = &juce::Desktop::getInstance().getDisplays().getMainDisplay();

        // Full physical display — true F11 surface (includes taskbar region; peer FS covers it).
        const auto area = display->totalArea;

        // Borderless top-level: no title bar / no OS chrome flags.
        constexpr int kFlags = juce::ComponentPeer::windowIsTemporary
                             | juce::ComponentPeer::windowHasDropShadow; // drop shadow ignored when FS
        // windowIsTemporary alone → no title bar on Windows
        addToDesktop (juce::ComponentPeer::windowIsTemporary);
        juce::ignoreUnused (kFlags);

        setBounds (area);
        setVisible (true);
        setAlwaysOnTop (true);
        toFront (true);

        if (auto* peer = getPeer())
        {
            peer->setFullScreen (true);
            peer->setAlwaysOnTop (true);
        }

        view.setComponentID ("spec3dFsView");
        addAndMakeVisible (view);
        view.setChromeMode (Spectrogram3DComponent::ChromeMode::docked);
        view.setActive (true);
        view.setVisible (true);
        view.setInterceptsMouseClicks (true, true);
        view.setBounds (getLocalBounds());
        view.toFront (false);

        grabKeyboardFocus();
        view.grabKeyboardFocus();
    }

    void detachView (Spectrogram3DComponent& view, juce::Component& restoreParent)
    {
        view.setComponentID ({});
        restoreParent.addChildComponent (view);
        setVisible (false);
        setAlwaysOnTop (false);
        if (auto* peer = getPeer())
            peer->setFullScreen (false);
        removeFromDesktop();
    }
};

/**
    Always-on-top fullscreen chrome (separate peer so native GL HWND cannot bury it).
    Settings (hamburger) + Exit — lookdev without leaving F11.
*/
struct MainComponent::Spec3DFsExitChrome final : public juce::Component
{
    MainComponent& owner;
    juce::TextButton settingsButton { "Settings" };
    MainComponent::OscToolButton exitButton { MainComponent::OscToolButton::Glyph::Collapse };

    explicit Spec3DFsExitChrome (MainComponent& o) : owner (o)
    {
        setOpaque (false);
        setAlwaysOnTop (true);

        settingsButton.setTooltip ("Open Settings / Look (lookdev while fullscreen)");
        settingsButton.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.55f));
        settingsButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::goldenrod.withAlpha (0.75f));
        settingsButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.95f));
        settingsButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        settingsButton.onClick = [this] { owner.openSettingsMenuFromFullscreen(); };
        addAndMakeVisible (settingsButton);

        exitButton.setTooltip ("Exit fullscreen (F11 / Esc)");
        exitButton.onClick = [this] { owner.setSpec3DFullscreen (false, true); };
        addAndMakeVisible (exitButton);
    }

    void placeOnDisplay (juce::Rectangle<int> displayTotalArea)
    {
        constexpr int kH = 36;
        constexpr int kSettingsW = 88;
        constexpr int kExitW = 36;
        constexpr int kGap = 8;
        constexpr int kMargin = 14;
        const int totalW = kSettingsW + kGap + kExitW;
        const auto b = juce::Rectangle<int> (displayTotalArea.getX() + kMargin,
                                             displayTotalArea.getY() + kMargin,
                                             totalW, kH);
        if (! isOnDesktop())
            addToDesktop (juce::ComponentPeer::windowIsTemporary
                          | juce::ComponentPeer::windowIgnoresKeyPresses);
        setBounds (b);
        setVisible (true);
        setAlwaysOnTop (true);
        toFront (true);
        if (auto* peer = getPeer())
            peer->setAlwaysOnTop (true);
        settingsButton.setBounds (0, 0, kSettingsW, kH);
        exitButton.setBounds (kSettingsW + kGap, 0, kExitW, kH);
    }

    void dismiss()
    {
        setVisible (false);
        setAlwaysOnTop (false);
        removeFromDesktop();
    }

    void resized() override
    {
        constexpr int kH = 36;
        constexpr int kSettingsW = 88;
        constexpr int kExitW = 36;
        constexpr int kGap = 8;
        settingsButton.setBounds (0, 0, kSettingsW, kH);
        exitButton.setBounds (kSettingsW + kGap, 0, kExitW, kH);
    }

    void paint (juce::Graphics& g) override
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (getLocalBounds().toFloat(), 6.0f);
        g.setColour (juce::Colours::goldenrod.withAlpha (0.55f));
        g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 6.0f, 1.0f);
    }
};

namespace
{
    /** Right-click Rename / Duplicate / Delete â€” always parented inside the UI theme panel. */
    class ThemePresetContextPanel final : public juce::Component
    {
    public:
        static void show (juce::Component& host,
                          juce::Point<int> hostLocalPos,
                          bool canEdit,
                          std::function<void (int /*1 rename, 2 dup, 3 del*/)> onAction)
        {
            dismissActive();

            auto* panel = new ThemePresetContextPanel (canEdit, std::move (onAction));
            active = panel;

            constexpr int width = 148;
            const int height = 6 + 3 * 24;
            host.addAndMakeVisible (panel);

            auto bounds = juce::Rectangle<int> (hostLocalPos.x, hostLocalPos.y, width, height);
            bounds = bounds.constrainedWithin (host.getLocalBounds());
            panel->setBounds (bounds);
            panel->toFront (false);
            panel->setWantsKeyboardFocus (false);
            panel->setMouseClickGrabsKeyboardFocus (false);

            juce::Component::SafePointer<ThemePresetContextPanel> safe (panel);
            juce::MessageManager::callAsync ([safe]
            {
                if (safe != nullptr && active == safe.getComponent())
                    safe->armOutsideClickDismiss = true;
            });
        }

        static void dismissActive()
        {
            if (active != nullptr)
            {
                auto* p = active;
                active = nullptr;
                if (auto* parent = p->getParentComponent())
                    parent->removeChildComponent (p);
                delete p;
            }
        }

        static void dismissIfOutside (juce::Component* clicked)
        {
            if (active == nullptr || ! active->armOutsideClickDismiss)
                return;
            if (clicked == active || active->isParentOf (clicked))
                return;
            dismissActive();
        }

        ~ThemePresetContextPanel() override
        {
            if (active == this)
                active = nullptr;
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (PluginMenuTheme::background());
            g.setColour (PluginMenuTheme::highlight().withAlpha (0.55f));
            g.drawRect (getLocalBounds().toFloat(), 1.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (3, 3);
            const int h = area.getHeight() / juce::jmax (1, buttons.size());
            for (auto* b : buttons)
                b->setBounds (area.removeFromTop (h));
        }

    private:
        ThemePresetContextPanel (bool canEdit, std::function<void (int)> onActionIn)
            : onAction (std::move (onActionIn))
        {
            auto addRow = [this] (const juce::String& text, int id, bool enabled)
            {
                auto* b = buttons.add (new juce::TextButton (text));
                b->setEnabled (enabled);
                b->setWantsKeyboardFocus (false);
                b->setMouseClickGrabsKeyboardFocus (false);
                b->setMouseCursor (enabled ? juce::MouseCursor::PointingHandCursor
                                           : juce::MouseCursor::NormalCursor);
                b->setColour (juce::TextButton::buttonColourId, PluginMenuTheme::background());
                b->setColour (juce::TextButton::buttonOnColourId, PluginMenuTheme::highlight());
                b->setColour (juce::TextButton::textColourOffId,
                              enabled ? PluginMenuTheme::text()
                                      : PluginMenuTheme::text().withAlpha (0.35f));
                b->setColour (juce::TextButton::textColourOnId, PluginMenuTheme::textOnHighlight());
                b->onClick = [this, id]
                {
                    auto action = onAction;
                    dismissActive();
                    if (action)
                        action (id);
                };
                addAndMakeVisible (b);
            };

            addRow ("Rename", 1, canEdit);
            addRow ("Duplicate", 2, true);
            addRow ("Delete", 3, canEdit);
        }

        std::function<void (int)> onAction;
        juce::OwnedArray<juce::TextButton> buttons;
        bool armOutsideClickDismiss = false;
        static ThemePresetContextPanel* active;
    };

    ThemePresetContextPanel* ThemePresetContextPanel::active = nullptr;

    /** One UI theme row inside the CallOutBox dropdown (not a PopupMenu item). */
    class UiThemePresetRow final : public juce::Component,
                                   public juce::SettableTooltipClient
    {
    public:
        UiThemePresetRow (juce::String nameIn,
                          bool tickedIn,
                          int themeIndexIn,
                          float uiScaleIn,
                          std::function<void (int)> onApplyIn,
                          std::function<void (int)> onRenameIn,
                          std::function<void (int)> onDuplicateIn,
                          std::function<void (int)> onDeleteIn)
            : name (std::move (nameIn)),
              ticked (tickedIn),
              themeIndex (themeIndexIn),
              uiScale (uiScaleIn),
              onApply (std::move (onApplyIn)),
              onRename (std::move (onRenameIn)),
              onDuplicate (std::move (onDuplicateIn)),
              onDelete (std::move (onDeleteIn))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
            setWantsKeyboardFocus (false);
            setMouseClickGrabsKeyboardFocus (false);
            setTooltip (ticked ? "Active theme" : "Apply this theme");
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat();

            if (hovered)
            {
                g.setColour (PluginMenuTheme::highlight());
                g.fillRect (bounds);
                g.setColour (PluginMenuTheme::textOnHighlight());
            }
            else
            {
                g.setColour (PluginMenuTheme::text());
            }

            g.setFont (juce::FontOptions (14.5f * uiScale));
            auto textArea = getLocalBounds().reduced (juce::roundToInt (10.0f * uiScale), 0);

            if (ticked)
            {
                g.drawText ("*",
                            textArea.removeFromLeft (juce::roundToInt (14.0f * uiScale)),
                            juce::Justification::centredLeft,
                            false);
            }

            g.drawText (name, textArea, juce::Justification::centredLeft, true);
        }

        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                showContextMenu (e);
                return;
            }

            ThemePresetContextPanel::dismissIfOutside (this);
        }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
                return;

            if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()) && onApply)
                onApply (themeIndex);
        }

    private:
        void showContextMenu (const juce::MouseEvent& e)
        {
            // Host inside this dropdown (walk up past viewport/viewed content).
            juce::Component* host = this;
            while (host != nullptr && dynamic_cast<juce::CallOutBox*> (host->getParentComponent()) == nullptr
                   && host->getParentComponent() != nullptr)
                host = host->getParentComponent();

            if (host == nullptr)
                host = getTopLevelComponent();
            if (host == nullptr)
                return;

            const bool canEdit = themeIndex > 0;
            auto ren = onRename;
            auto dup = onDuplicate;
            auto del = onDelete;
            const int idx = themeIndex;

            const auto local = host->getLocalPoint (nullptr, e.getScreenPosition());
            ThemePresetContextPanel::show (
                *host,
                local,
                canEdit,
                [ren, dup, del, idx] (int result)
                {
                    if (result == 1 && ren)
                        ren (idx);
                    else if (result == 2 && dup)
                        dup (idx);
                    else if (result == 3 && del)
                        del (idx);
                });
        }

        juce::String name;
        bool ticked = false;
        bool hovered = false;
        int themeIndex = -1;
        float uiScale = 1.0f;
        std::function<void (int)> onApply;
        std::function<void (int)> onRename;
        std::function<void (int)> onDuplicate;
        std::function<void (int)> onDelete;
    };

    /** One FFT / Spec / Fill row: click to select/deselect sample target; Edit/Hide opens editor. */
    class RampTargetAccordion final : public juce::Component
    {
    public:
        RampTargetAccordion (SharedResources& resources,
                             ColourRampBank& bankIn,
                             ColourRampBank::Target targetIn,
                             GradientStripEditor::ModeFamily family,
                             float uiScaleIn,
                             std::function<void()> onLayoutChangedIn,
                             std::function<void()> onActiveChangedIn)
            : bank (bankIn),
              target (targetIn),
              uiScale (uiScaleIn),
              editor (resources, family, &bankIn.getPresets()),
              onLayoutChanged (std::move (onLayoutChangedIn)),
              onActiveChanged (std::move (onActiveChangedIn))
        {
            setWantsKeyboardFocus (false);

            headerButton.setButtonText (ColourRampBank::targetName (target));
            headerButton.setTooltip ("Select as path-sample target (click again to deselect)");
            headerButton.setColour (juce::TextButton::buttonColourId, PluginMenuTheme::background().brighter (0.04f));
            headerButton.setColour (juce::TextButton::buttonOnColourId, PluginMenuTheme::highlight());
            headerButton.setColour (juce::TextButton::textColourOffId, PluginMenuTheme::text());
            headerButton.setColour (juce::TextButton::textColourOnId, PluginMenuTheme::textOnHighlight());
            headerButton.setClickingTogglesState (true);
            headerButton.onClick = [this]
            {
                // Toggle: radio groups cannot clear selection, so manage it ourselves.
                if (bank.hasActiveTarget() && bank.getActiveTarget() == target)
                    bank.clearActiveTarget();
                else
                    bank.setActiveTarget (target);
                bank.save();
                if (onActiveChanged)
                    onActiveChanged();
            };
            addAndMakeVisible (headerButton);

            editToggle.setButtonText ("Edit");
            editToggle.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
            editToggle.setColour (juce::TextButton::textColourOffId, PluginMenuTheme::text().withAlpha (0.85f));
            editToggle.setTooltip ("Show the gradient editor for this ramp");
            editToggle.onClick = [this]
            {
                expanded = ! expanded;
                editor.setVisible (expanded);
                editToggle.setButtonText (expanded ? "Hide" : "Edit");
                editToggle.setTooltip (expanded ? "Hide the gradient editor"
                                                : "Show the gradient editor for this ramp");
                if (expanded)
                {
                    bank.setActiveTarget (target);
                    bank.save();
                    if (onActiveChanged)
                        onActiveChanged();
                }
                else
                {
                    refreshHeaderLook();
                }
                if (onLayoutChanged)
                    onLayoutChanged();
            };
            addAndMakeVisible (editToggle);

            editor.setUiScale (uiScale);
            editor.setRamp (&bank.get (target));
            editor.setVisible (false);
            editor.onRampChanged = [this]
            {
                bank.notifyEdited();
                refreshHeaderLook();
            };
            editor.onRampPreview = [this]
            {
                bank.notifyPreview();
                refreshHeaderLook();
            };
            editor.onPreferredHeightChanged = [this]
            {
                if (onLayoutChanged)
                    onLayoutChanged();
            };
            addChildComponent (editor);

            refreshHeaderLook();
        }

        int getPreferredHeight() const noexcept
        {
            const int headH = juce::roundToInt (28.0f * uiScale);
            return headH + (expanded ? juce::roundToInt (4.0f * uiScale) + editor.getPreferredHeight() : 0);
        }

        void resized() override
        {
            const int headH = juce::roundToInt (28.0f * uiScale);
            const int editW = juce::roundToInt (40.0f * uiScale);
            auto area = getLocalBounds();
            auto head = area.removeFromTop (headH);
            editToggle.setBounds (head.removeFromRight (editW));
            headerButton.setBounds (head);
            if (expanded)
            {
                area.removeFromTop (juce::roundToInt (4.0f * uiScale));
                editor.setBounds (area);
            }
        }

        void paint (juce::Graphics& g) override
        {
            if (expanded)
            {
                g.setColour (PluginMenuTheme::highlight().withAlpha (0.08f));
                g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), 3.0f);
            }
        }

        void refreshFromBank()
        {
            editor.setRamp (&bank.get (target));
            refreshHeaderLook();
        }

    private:
        void refreshHeaderLook()
        {
            const bool active = bank.hasActiveTarget() && bank.getActiveTarget() == target;
            headerButton.setToggleState (active, juce::dontSendNotification);
            const auto& ramp = bank.get (target);
            auto text = ColourRampBank::targetName (target);
            if (ramp.isUsable())
                text << " - " << GradientRamp::mapModeName (ramp.mapMode);
            headerButton.setButtonText (text);
        }

        ColourRampBank& bank;
        ColourRampBank::Target target;
        float uiScale = 1.0f;
        bool expanded = false;
        juce::TextButton headerButton, editToggle;
        GradientStripEditor editor;
        std::function<void()> onLayoutChanged;
        std::function<void()> onActiveChanged;
    };

    /**
        UI theme dropdown content shown via CallOutBox.
        Colour-ramp accordion + sample live under the theme actions.
    */
    class UiThemeDropdownContent final : public juce::Component
    {
    public:
        static constexpr float kUiScale = 0.7f;

        UiThemeDropdownContent (bool glowDisabled,
                                ThemeList& themes,
                                SharedResources& resources,
                                ColourRampBank& ramps,
                                std::function<void()> onSave,
                                std::function<void()> onToggleGlow,
                                std::function<void (int)> onApply,
                                std::function<void (int)> onRename,
                                std::function<void (int)> onDuplicate,
                                std::function<void (int)> onDelete,
                                std::function<void()> onSamplePath)
            : colourRamps (ramps)
        {
            setWantsKeyboardFocus (false);
            setMouseClickGrabsKeyboardFocus (false);

            auto styleAction = [] (juce::TextButton& b, bool toggled = false)
            {
                b.setWantsKeyboardFocus (false);
                b.setMouseClickGrabsKeyboardFocus (false);
                b.setColour (juce::TextButton::buttonColourId, PluginMenuTheme::background());
                b.setColour (juce::TextButton::buttonOnColourId, PluginMenuTheme::highlight());
                b.setColour (juce::TextButton::textColourOffId, PluginMenuTheme::text());
                b.setColour (juce::TextButton::textColourOnId, PluginMenuTheme::textOnHighlight());
                if (toggled)
                    b.setToggleable (true);
            };

            saveButton.setButtonText ("Save Current UI as Preset");
            saveButton.setTooltip ("Save the current look as a named UI theme preset");
            styleAction (saveButton);
            saveButton.onClick = [onSave]
            {
                ThemePresetContextPanel::dismissActive();
                if (onSave)
                    onSave();
            };
            addAndMakeVisible (saveButton);

            glowButton.setButtonText ("Disable glow/shadow effects");
            glowButton.setTooltip ("Turn off glow and shadow drawing for a flatter UI");
            styleAction (glowButton, true);
            glowButton.setClickingTogglesState (true);
            glowButton.setToggleState (glowDisabled, juce::dontSendNotification);
            glowButton.onClick = [onToggleGlow]
            {
                ThemePresetContextPanel::dismissActive();
                if (onToggleGlow)
                    onToggleGlow();
            };
            addAndMakeVisible (glowButton);

            rampHeader.setText ("Colour ramps", juce::dontSendNotification);
            rampHeader.setFont (juce::FontOptions().withName ("Lato Black").withHeight (13.0f * kUiScale));
            rampHeader.setColour (juce::Label::textColourId, PluginMenuTheme::highlight());
            rampHeader.setJustificationType (juce::Justification::centredLeft);
            addAndMakeVisible (rampHeader);

            sampleButton.setButtonText ("Sample path on UI");
            styleAction (sampleButton);
            sampleButton.setTooltip ("Select a ramp row below, then drag across the plugin UI to fill it");
            sampleButton.onClick = [onSamplePath]
            {
                ThemePresetContextPanel::dismissActive();
                if (onSamplePath)
                    onSamplePath();
            };
            addAndMakeVisible (sampleButton);

            auto layoutCb = [this] { relayout(); };
            auto activeCb = [this]
            {
                if (fftRow != nullptr)     fftRow->refreshFromBank();
                if (specRow != nullptr)    specRow->refreshFromBank();
                if (spec3DRow != nullptr)  spec3DRow->refreshFromBank();
                if (fillRow != nullptr)    fillRow->refreshFromBank();
            };

            fftRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::fftBars,
                GradientStripEditor::ModeFamily::intensity, kUiScale, layoutCb, activeCb);
            specRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::spectrogram,
                GradientStripEditor::ModeFamily::intensity, kUiScale, layoutCb, activeCb);
            spec3DRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::spectrogram3D,
                GradientStripEditor::ModeFamily::intensity, kUiScale, layoutCb, activeCb);
            fillRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::spectrumFill,
                GradientStripEditor::ModeFamily::spatial, kUiScale, layoutCb, activeCb);
            addAndMakeVisible (*fftRow);
            addAndMakeVisible (*specRow);
            addAndMakeVisible (*spec3DRow);
            addAndMakeVisible (*fillRow);

            listContainer.setWantsKeyboardFocus (false);
            listContainer.setMouseClickGrabsKeyboardFocus (false);
            viewport.setViewedComponent (&listContainer, false);
            viewport.setScrollBarsShown (true, false);
            viewport.setWantsKeyboardFocus (false);
            addAndMakeVisible (viewport);

            const int selected = themes.getSelectedRow();
            const int n = themes.getNumRows();
            const int rowH = juce::roundToInt (24.0f * kUiScale);
            const int pad = juce::roundToInt (16.0f * kUiScale);
            // Wider than the old glyph-rail layout so English toolbar labels still fit at 70% scale.
            panelW = juce::roundToInt (360.0f * kUiScale);
            for (int i = 0; i < n; ++i)
            {
                const auto name = themes.getPresetName (i);
                const auto label = name.isNotEmpty() ? name : ("Theme " + juce::String (i + 1));
                auto* row = rows.add (new UiThemePresetRow (label,
                                                            i == selected,
                                                            i,
                                                            kUiScale,
                                                            onApply,
                                                            onRename,
                                                            onDuplicate,
                                                            onDelete));
                listContainer.addAndMakeVisible (row);
            }

            listContainer.setSize (panelW - pad, juce::jmax (rowH, n * rowH));
            for (int i = 0; i < rows.size(); ++i)
                rows[i]->setBounds (0, i * rowH, panelW - pad, rowH);

            themeListH = juce::jmin (7 * rowH, listContainer.getHeight());
            relayout();
        }

        ~UiThemeDropdownContent() override
        {
            ThemePresetContextPanel::dismissActive();
            viewport.setViewedComponent (nullptr, false);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (PluginMenuTheme::background());
            g.setColour (PluginMenuTheme::highlight().withAlpha (0.4f));
            g.drawRect (getLocalBounds().toFloat(), 1.0f);

            if (rows.size() > 0 && viewport.getHeight() > 0)
            {
                const float y = (float) viewport.getY() - 4.0f * kUiScale;
                g.setColour (PluginMenuTheme::highlight().withAlpha (0.22f));
                g.drawLine (12.0f * kUiScale, y, (float) getWidth() - 12.0f * kUiScale, y, 1.0f);
            }
        }

        void resized() override
        {
            const auto px = [] (float v) { return juce::roundToInt (v * kUiScale); };
            auto area = getLocalBounds().reduced (px (8));
            saveButton.setBounds (area.removeFromTop (px (28)));
            area.removeFromTop (px (4));
            glowButton.setBounds (area.removeFromTop (px (28)));
            area.removeFromTop (px (10));

            rampHeader.setBounds (area.removeFromTop (px (18)));
            area.removeFromTop (px (4));
            sampleButton.setBounds (area.removeFromTop (px (28)));
            area.removeFromTop (px (6));

            if (fftRow != nullptr)
            {
                fftRow->setBounds (area.removeFromTop (fftRow->getPreferredHeight()));
                area.removeFromTop (px (4));
            }
            if (specRow != nullptr)
            {
                specRow->setBounds (area.removeFromTop (specRow->getPreferredHeight()));
                area.removeFromTop (px (4));
            }
            if (spec3DRow != nullptr)
            {
                spec3DRow->setBounds (area.removeFromTop (spec3DRow->getPreferredHeight()));
                area.removeFromTop (px (4));
            }
            if (fillRow != nullptr)
            {
                fillRow->setBounds (area.removeFromTop (fillRow->getPreferredHeight()));
                area.removeFromTop (px (4));
            }

            if (rows.size() > 0)
            {
                area.removeFromTop (px (4));
                viewport.setBounds (area.removeFromTop (themeListH));
            }
        }

        void mouseDown (const juce::MouseEvent& e) override
        {
            ThemePresetContextPanel::dismissIfOutside (e.eventComponent);
        }

    private:
        void relayout()
        {
            const auto px = [] (float v) { return juce::roundToInt (v * kUiScale); };
            const int rampH = (fftRow != nullptr ? fftRow->getPreferredHeight() : 0)
                              + (specRow != nullptr ? specRow->getPreferredHeight() : 0)
                              + (spec3DRow != nullptr ? spec3DRow->getPreferredHeight() : 0)
                              + (fillRow != nullptr ? fillRow->getPreferredHeight() : 0)
                              + px (8) + px (8);
            const int h = px (8) + px (28) + px (4) + px (28) + px (10) + px (18) + px (4) + px (28) + px (6)
                          + rampH
                          + (rows.size() > 0 ? px (8) + themeListH : 0) + px (8);
            setSize (panelW, h);
            resized();

            if (auto* box = findParentComponentOfClass<juce::CallOutBox>())
                box->resized();
        }

        ColourRampBank& colourRamps;
        int panelW = juce::roundToInt (360.0f * kUiScale);
        int themeListH = 0;
        juce::TextButton saveButton, glowButton, sampleButton;
        juce::Label rampHeader;
        std::unique_ptr<RampTargetAccordion> fftRow, specRow, spec3DRow, fillRow;
        juce::Viewport viewport;
        juce::Component listContainer;
        juce::OwnedArray<UiThemePresetRow> rows;
    };
}

MainComponent::MainComponent(EqProcessor& p, Analyser& analyser, juce::AudioProcessorValueTreeState& treeState, EqEditor& editorRef)
    : frequencyResponseComponent(p),
    m_visualizer(treeState, analyser),
    verticalGradientMeterL([&]()
                            {
                                return processor.isMeterMsMode()
                                           ? processor.getInputMsPeakValue (0)
                                           : processor.getInputPeakValue (0);
                            },
                           [&]()
                            {
                                return processor.isMeterMsMode()
                                           ? processor.getInputMsRmsValue (0)
                                           : processor.getInputRmsValue (0);
                            },
                           p.treeState,
                           inputMeterClip,
                           0),
    verticalGradientMeterR([&]()
                            {
                                return processor.isMeterMsMode()
                                           ? processor.getInputMsPeakValue (1)
                                           : processor.getInputPeakValue (1);
                            },
                           [&]()
                            {
                                return processor.isMeterMsMode()
                                           ? processor.getInputMsRmsValue (1)
                                           : processor.getInputRmsValue (1);
                            },
                           p.treeState,
                           inputMeterClip,
                           1),
    verticalGradientMeterPostL([&]()
                                {
                                    return processor.isMeterMsMode()
                                               ? processor.getPostProcessingMsPeakValue (0)
                                               : processor.getPostProcessingPeakValue (0);
                                },
                               [&]()
                                {
                                    return processor.isMeterMsMode()
                                               ? processor.getPostProcessingMsRmsValue (0)
                                               : processor.getPostProcessingRmsValue (0);
                                },
                               p.treeState,
                               outputMeterClip,
                               0),
    verticalGradientMeterPostR([&]()
                                {
                                    return processor.isMeterMsMode()
                                               ? processor.getPostProcessingMsPeakValue (1)
                                               : processor.getPostProcessingPeakValue (1);
                                },
                               [&]()
                                {
                                    return processor.isMeterMsMode()
                                               ? processor.getPostProcessingMsRmsValue (1)
                                               : processor.getPostProcessingRmsValue (1);
                                },
                               p.treeState,
                               outputMeterClip,
                               1),
    m_controls(treeState),
    processor(p),
    editor(editorRef),
    menu (sharedResources, treeState, textButtonLookAndFeel, colourRamps),
    levelMeterIn (p, treeState, ScopeLevelMeterModule::Tap::input, "Level Meter 1"),
    levelMeterOut (p, treeState, ScopeLevelMeterModule::Tap::output, "Level Meter 2")
{
    // Host may have already called setStateInformation (Ableton often does before the editor).
    restoreSessionUiThemeIfAny();
    colourRamps.addChangeListener (this);
    frequencyResponseComponent.onBandManipulationHighlight = [this] (int bandIndex)
    {
        editor.setBandManipulationHighlight (bandIndex);
    };
    frequencyResponseComponent.setEditor (&editor);

    // Stay on JUCE's native Windows renderer (Direct2D in JUCE 8) instead of OpenGL.
    // OpenGL is scoped only to Spectrogram3DComponent when 3D Spec is active.
    setOpaque(true);
    setWantsKeyboardFocus (true);

    m_controls.setMarginInPixels(m_marginInPixels);
    m_visualizer.setMarginInPixels(m_marginInPixels);
    m_visualizer.setOpaque(true);
    addAndMakeVisible(m_visualizer);

    frequencyResponseComponent.addMouseListener(this, true);
    processor.setFrequencyResponseComponent (&frequencyResponseComponent);
    addAndMakeVisible(frequencyResponseComponent);

    addAndMakeVisible(menuToggleButton);
    menuToggleButton.setButtonText("Settings");
    menuToggleButton.setAlwaysOnTop(true);

    menuToggleButton.setLookAndFeel(&customLookAndFeel);
    customLookAndFeel.setThemeColors (&sharedResources);

    menu.addComponentListener (this);
    menu.onCloseRequest = [this] { closeSettingsMenu(); };
    menuToggleButton.onClick = [this] {
        const bool shouldShowMenu = ! menu.isVisible();
        if (shouldShowMenu)
        {
            menu.setVisible (true);
            menuDismissCatcher.setVisible (true);
            menuDismissCatcher.setBounds (getLocalBounds());
            menuDismissCatcher.toFront (false);
            // Menu above the Settings button (hamburger is covered / unusable under the panel).
            menuToggleButton.toFront (false);
            menu.toFront (false);
            layoutSettingsMenu();
            resized();
            syncExpandedOscOverlayStack();
            menu.setInterceptsMouseClicks (true, true);
            frequencyResponseComponent.setInterceptsMouseClicks (false, false);
        }
        else
        {
            closeSettingsMenu();
        }
    };

    menuDismissCatcher.setVisible (false);
    addChildComponent (menuDismissCatcher);

    // Top chrome: Bypass (left) + A/B/C/D referencing â€” same look as graph range buttons / Settings area.
    styleChromeButton (bypassButton);
    bypassButton.setClickingTogglesState (true);
    bypassButton.setTooltip ("Bypass - pass audio through unaffected");
    bypassButton.setAlwaysOnTop (true);
    addAndMakeVisible (bypassButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        treeState, "bypass", bypassButton);

    auto setupAbButton = [this] (AbSlotButton& button, EqProcessor::AbSlot slot)
    {
        const auto name = EqProcessor::abSlotName (slot);
        styleChromeButton (button);
        button.setClickingTogglesState (false);
        button.setAlwaysOnTop (true);
        button.setTooltip (name + " snapshot - click to recall. Right-click for options.");
        button.onClick = [this, slot]
        {
            processor.getUndoManager().beginNewTransaction ("A/B/C/D recall");
            processor.switchToAbSlot (slot);
            syncAbButtons();
        };
        button.onPopupMenu = [this, slot] { showAbMenu (slot); };
        addAndMakeVisible (button);
    };

    setupAbButton (slotAButton, EqProcessor::AbSlot::A);
    setupAbButton (slotBButton, EqProcessor::AbSlot::B);
    setupAbButton (slotCButton, EqProcessor::AbSlot::C);
    setupAbButton (slotDButton, EqProcessor::AbSlot::D);
    syncAbButtons();

    // Preset chrome: â—€ | editable name | â–¼ | â–¶ | Save â€” EQ/functionality only (UI themes are separate).
    auto setupPresetNavButton = [this] (juce::TextButton& button, int delta, const juce::String& tip)
    {
        styleChromeButton (button);
        button.setTooltip (tip);
        button.setAlwaysOnTop (true);
        button.onClick = [this, delta] { cycleEqPreset (delta); };
        addAndMakeVisible (button);
    };

    setupPresetNavButton (presetPrevButton, -1, "Previous EQ preset");
    setupPresetNavButton (presetNextButton, 1, "Next EQ preset");

    stylePresetNameEditor();
    presetNameEditor.setText ("Default", juce::dontSendNotification);
    presetNameEditor.setTooltip ("EQ preset name. Enter or click away to rename.");
    presetNameEditor.setAlwaysOnTop (true);
    presetNameEditor.addListener (this);
    addAndMakeVisible (presetNameEditor);

    styleChromeButton (presetMenuButton);
    presetMenuButton.setTooltip ("Browse EQ presets");
    presetMenuButton.setAlwaysOnTop (true);
    presetMenuButton.onClick = [this] { showPresetPopupMenu(); };
    addAndMakeVisible (presetMenuButton);

    styleChromeButton (presetSaveButton);
    presetSaveButton.setTooltip ("Save current EQ settings under the typed name");
    presetSaveButton.setAlwaysOnTop (true);
    presetSaveButton.onClick = [this] { saveCurrentEqPreset(); };
    addAndMakeVisible (presetSaveButton);

    styleChromeButton (uiThemeButton);
    uiThemeButton.setTooltip ("UI themes, glow, and colour ramps (path sample)");
    uiThemeButton.setAlwaysOnTop (true);
    uiThemeButton.onClick = [this] { showUiThemePopupMenu(); };
    addAndMakeVisible (uiThemeButton);

    uiRandomizeButton.setThemeResources (&sharedResources);
    uiRandomizeButton.setTooltip (
        "Randomize checked UI scopes and colour ramps. Right-click: scopes + Ordered/Standard ramp mode.");
    uiRandomizeButton.setAlwaysOnTop (true);
    uiRandomizeButton.onClick = [this] { runDiceRandomize(); };
    uiRandomizeButton.onPopupMenu = [this] { showRandomizeDiceMenu(); };
    addAndMakeVisible (uiRandomizeButton);

    rampSampleOverlay.onSampled = [this] (GradientRamp ramp)
    {
        if (colourRamps.hasActiveTarget())
            colourRamps.setRamp (colourRamps.getActiveTarget(), std::move (ramp));
    };
    addChildComponent (rampSampleOverlay);

    eqPresets = std::make_unique<EqPresetStore> (processor);
    eqPresets->onChanged = [this]
    {
        refreshPresetNameDisplay();
        syncAbButtons();
    };
    refreshPresetNameDisplay();

    // Undo / Redo â€” top right, just left of Settings.
    styleChromeButton (undoButton);
    undoButton.setTooltip ("Undo");
    undoButton.setAlwaysOnTop (true);
    undoButton.onClick = [this]
    {
        if (processor.getUndoManager().canUndo())
            processor.getUndoManager().undo();
        syncUndoRedoButtons();
    };
    addAndMakeVisible (undoButton);

    styleChromeButton (redoButton);
    redoButton.setTooltip ("Redo");
    redoButton.setAlwaysOnTop (true);
    redoButton.onClick = [this]
    {
        if (processor.getUndoManager().canRedo())
            processor.getUndoManager().redo();
        syncUndoRedoButtons();
    };
    addAndMakeVisible (redoButton);
    processor.getUndoManager().addChangeListener (this);
    syncUndoRedoButtons();

    // Eco â€” same Y as Bypass, X just right of Save. Disables analyser/FFT + all scopes.
    styleChromeButton (ecoButton);
    ecoButton.setClickingTogglesState (true);
    ecoButton.setTooltip ("Eco - disables analyser, spectrum, and all scopes to save CPU. Dynamic (D) and Spectral (S) still work.");
    ecoButton.setAlwaysOnTop (true);
    ecoButton.onClick = [this]
    {
        setEcoMode (ecoButton.getToggleState(), true);
    };
    addAndMakeVisible (ecoButton);

    // OSC â€” slightly smaller than Eco; reveals waveform strip + zoom / mode / expand buttons.
    styleChromeButton (oscButton);
    oscButton.setClickingTogglesState (true);
    oscButton.setTooltip ("Enables oscilloscope");
    oscButton.setAlwaysOnTop (true);
    oscButton.onClick = [this]
    {
        const bool on = oscButton.getToggleState();
        if (! on)
            setOscExpanded (false);
        oscilloscope.setEnabled (on);
        if (on)
            oscilloscope.setZoomIndex (0); // most zoomed in: 1 beat
        syncOscToolButtons();
        resized();
        if (on)
            oscilloscope.setZoomIndex (0); // again after layout so samples/column match width
    };
    addAndMakeVisible (oscButton);

    oscZoomInButton.setTooltip ("Zoom in - show less time (down to 1 beat)");
    oscZoomOutButton.setTooltip ("Zoom out - show more time (up to 32 beats)");
    oscChannelModeButton.setTooltip ("Summed stereo (ST) or split L/R waveforms");
    oscExpandButton.setTooltip ("Expand oscilloscope over the full graph");
    oscZoomInButton.setAlwaysOnTop (true);
    oscZoomOutButton.setAlwaysOnTop (true);
    oscChannelModeButton.setAlwaysOnTop (true);
    oscExpandButton.setAlwaysOnTop (true);
    oscZoomInButton.onClick = [this] { oscilloscope.zoomIn(); };
    oscZoomOutButton.onClick = [this] { oscilloscope.zoomOut(); };
    oscChannelModeButton.onClick = [this]
    {
        oscilloscope.toggleChannelMode();
        syncOscToolButtons();
    };
    oscExpandButton.onClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::oscilloscope);
        else
            setOscExpanded (! oscExpanded);
    };
    oscZoomInButton.setVisible (false);
    oscZoomOutButton.setVisible (false);
    oscChannelModeButton.setVisible (false);
    oscExpandButton.setVisible (false);
    addChildComponent (oscZoomInButton);
    addChildComponent (oscZoomOutButton);
    addChildComponent (oscChannelModeButton);
    addChildComponent (oscExpandButton);

    // Gon â€” under OSC; mutually exclusive with spectrum analyser.
    styleChromeButton (gonButton);
    gonButton.setClickingTogglesState (true);
    gonButton.setTooltip ("Goniometer - stereo image + correlation");
    gonButton.setAlwaysOnTop (true);
    gonButton.onClick = [this]
    {
        applyGoniometerActive (gonButton.getToggleState());
    };
    addAndMakeVisible (gonButton);

    gonExpandButton.setTooltip ("Open goniometer in a framed floating window");
    gonExpandButton.setAlwaysOnTop (true);
    gonExpandButton.onClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::goniometer);
        else
            setGonExpanded (! gonExpanded);
    };
    gonExpandButton.setVisible (false);
    addChildComponent (gonExpandButton);

    // Spec â€” spectrogram strip between UI dice and EQ preset bar.
    styleChromeButton (specButton);
    specButton.setClickingTogglesState (true);
    specButton.setTooltip ("Spectrogram - scrolling frequency waterfall");
    specButton.setAlwaysOnTop (true);
    specButton.onClick = [this]
    {
        applySpectrogramActive (specButton.getToggleState());
    };
    addAndMakeVisible (specButton);

    specSpeedUpButton.setTooltip ("Speed up spectrogram scroll");
    specSpeedDownButton.setTooltip ("Slow down spectrogram scroll");
    specExpandButton.setTooltip ("Open spectrogram in a framed floating window");
    spec3DButton.setTooltip ("3D spectrogram - orbit (drag), ground pan (Shift/MMB), screen pan (RMB), zoom (wheel). Expanded Spec only.");
    specSpeedUpButton.setAlwaysOnTop (true);
    specSpeedDownButton.setAlwaysOnTop (true);
    specExpandButton.setAlwaysOnTop (true);
    spec3DButton.setAlwaysOnTop (true);
    specSpeedUpButton.onClick = [this] { spectrogram.speedUp(); };
    specSpeedDownButton.onClick = [this] { spectrogram.speedDown(); };
    specExpandButton.onClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::spectrogram);
        else
            setSpecExpanded (! specExpanded);
    };
    spec3DButton.setClickingTogglesState (true);
    spec3DButton.onClick = [this]
    {
        // Outside Scope: expanded 3D overlay. Inside Scope: independent Spectrogram 3D module.
        setSpec3DMode (spec3DButton.getToggleState(), true);
    };
    spec3DButton.onPopupMenu = [this]
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
        const auto q = spectrogram3D.getMeshQuality();
        menu.addItem (1, "Mesh: Low", true, q == Spectrogram3DComponent::MeshQuality::low);
        menu.addItem (2, "Mesh: Medium", true, q == Spectrogram3DComponent::MeshQuality::medium);
        menu.addItem (3, "Mesh: High", true, q == Spectrogram3DComponent::MeshQuality::high);
        menu.addItem (4, "Mesh: Ultra", true, q == Spectrogram3DComponent::MeshQuality::ultra
                                              || q == Spectrogram3DComponent::MeshQuality::overkill);
        menu.addSeparator();
        menu.addItem (5, "Reset camera");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&spec3DButton),
            [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
            {
                if (safe == nullptr || result <= 0)
                    return;
                if (result == 5)
                {
                    safe->spectrogram3D.resetCamera();
                    return;
                }
                safe->setSpec3DMeshQuality (
                    result == 1 ? Spectrogram3DComponent::MeshQuality::low
                                : (result == 3 ? Spectrogram3DComponent::MeshQuality::high
                                               : (result == 4 ? Spectrogram3DComponent::MeshQuality::ultra
                                                              : Spectrogram3DComponent::MeshQuality::medium)),
                    true);
            });
    };
    specSpeedUpButton.setVisible (false);
    specSpeedDownButton.setVisible (false);
    specExpandButton.setVisible (false);
    spec3DButton.setVisible (false);
    addChildComponent (specSpeedUpButton);
    addChildComponent (specSpeedDownButton);
    addChildComponent (specExpandButton);
    addChildComponent (spec3DButton);

    oscDimmer.setInterceptsMouseClicks (false, false);
    oscDimmer.setAlwaysOnTop (true);
    oscDimmer.setVisible (false);
    addChildComponent (oscDimmer);

    scopeSplitOverlay.setAlwaysOnTop (true);
    scopeSplitOverlay.setVisible (false);
    addChildComponent (scopeSplitOverlay);

    scopeArrangeOverlay.setAlwaysOnTop (true);
    scopeArrangeOverlay.setVisible (false);
    addChildComponent (scopeArrangeOverlay);

    arrangeButton.setThemeResources (&sharedResources);
    arrangeButton.setClickingTogglesState (true);
    arrangeButton.setTooltip ("Arrange - Square = tiled grid; line = strip. Drag pane tops to reorder; drag strip bottom to resize.");
    arrangeButton.setAlwaysOnTop (true);
    arrangeButton.onClick = [this]
    {
        setScopeStripLayout (arrangeButton.getToggleState(), true);
        arrangeButton.setGlyph (scopeStripLayout ? OscToolButton::Glyph::StripLayout
                                                  : OscToolButton::Glyph::GridLayout);
    };
    arrangeButton.setVisible (false);
    addChildComponent (arrangeButton);

    oscilloscope.setAlwaysOnTop (true);
    oscilloscope.setParameterTree (&processor.treeState);
    addChildComponent (oscilloscope);
    processor.setOscilloscopeTarget (&oscilloscope);
    oscilloscope.onDoubleClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::oscilloscope);
        else if (oscExpanded)
            toggleOscFullGraph();
    };

    goniometer.setAlwaysOnTop (true);
    goniometer.setParameterTree (&processor.treeState);
    addChildComponent (goniometer);
    processor.setGoniometerTarget (&goniometer);
    goniometer.onDoubleClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::goniometer);
        else if (gonExpanded)
            toggleGonFullGraph();
    };

    spectrogram.setAlwaysOnTop (true);
    spectrogram.setParameterTree (&processor.treeState);
    addChildComponent (spectrogram);
    processor.setSpectrogramTarget (&spectrogram);
    spectrogram.onDoubleClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::spectrogram);
        else if (specExpanded)
            toggleSpecFullGraph();
    };

    auto setupFrame = [this] (FramedFloatingScopeWindow& frame)
    {
        frame.setThemeColors (&sharedResources);
        frame.setSoftFill (true);
        frame.setChromeMode (FramedFloatingScopeWindow::ChromeMode::floating);
        frame.setAlwaysOnTop (true);
        frame.onEscape = [this] { collapseAnyExpandedScope(); };
        addChildComponent (frame);
    };
    setupFrame (oscFrame);
    setupFrame (gonFrame);
    setupFrame (specFrame);
    oscFrame.onDoubleClick = [this] { toggleOscFullGraph(); };
    gonFrame.onDoubleClick = [this] { toggleGonFullGraph(); };
    specFrame.onDoubleClick = [this] { toggleSpecFullGraph(); };
    oscFrame.onUserMoved = oscFrame.onUserResized = [this]
    {
        syncOscFramedTools();
        rememberFrameBounds (oscFrame, oscFrameBoundsCustom,
                             oscFramePreferredW, oscFramePreferredH,
                             oscFramePreferredX, oscFramePreferredY);
    };
    gonFrame.onUserMoved = gonFrame.onUserResized = [this]
    {
        syncGonFramedTools();
        rememberFrameBounds (gonFrame, gonFrameBoundsCustom,
                             gonFramePreferredW, gonFramePreferredH,
                             gonFramePreferredX, gonFramePreferredY);
    };
    specFrame.onUserMoved = specFrame.onUserResized = [this]
    {
        syncSpecFramedTools();
        rememberFrameBounds (specFrame, specFrameBoundsCustom,
                             specFramePreferredW, specFramePreferredH,
                             specFramePreferredX, specFramePreferredY);
    };

    spectrogram3D.setDataSource (&spectrogram);
    spectrogram3D.setAlwaysOnTop (false);
    spectrogram3D.setColourRampBank (&colourRamps);
    spectrogram3D.setAudioLevelProvider ([this] { return processor.getSpec3DVisualLevel01(); });
    spectrogram3D.onEscape = [this]
    {
        if (spec3DFullscreen)
            setSpec3DFullscreen (false, true);
        else
            collapseAnyExpandedScope();
    };
    spectrogram3D.onToggleFullscreen = [this] { toggleSpec3DFullscreen (true); };
    spectrogram3D.isFullscreenQuery = [this] { return isSpec3DFullscreen(); };
    spectrogram3D.onDefaultViewChanged = [this] { editor.requestSaveUiPrefs(); };
    spectrogram3D.onAugmentContextMenu = [this] (juce::PopupMenu& menu)
    {
        appendModuleLookMenuItems (menu, ModuleLookPresets::Kind::spectrogram3D, 1000);
    };
    spectrogram3D.onContextMenuResult = [this] (int result)
    {
        return handleModuleLookMenuResult (ModuleLookPresets::Kind::spectrogram3D, result, 1000);
    };
    spectrogram3D.onAutoRotateSettingsChanged = [this] { editor.requestSaveUiPrefs(); };
    spectrogram3D.onRampSequenceChanged = [this] { editor.requestSaveUiPrefs(); };
    spectrogram3D.onRequestRampTimelineExpand = [this] { showRampTimelineWindow(); };
    spectrogram3D.onExportRegionOffline =
        [this] (const Spec3DExportSettings& s) { startSpec3DRegionExport (s); };
    spectrogram3D.onDofFocusChanged = [this]
    {
        editor.requestSaveUiPrefs();
        menu.syncSpec3DDofFocusFromMain();
    };
    spectrogram3D.onDebugSphereChanged = [this]
    {
        editor.requestSaveUiPrefs();
        menu.syncSpec3DDebugSphereFromMain();
    };
    spectrogram3D.onDoubleClick = [this]
    {
        if (scopeModeEnabled)
            toggleScopePaneFullscreen (ScopeModuleId::spectrogram3D);
        else if (specExpanded)
            toggleSpecFullGraph();
    };
    spectrogram3D.onUserResized = [this]
    {
        if (spec3DFullscreen)
            return; // do not clobber floating prefs while edge-to-edge
        syncSpec3DFramedTools();
        spec3DPreferredW = spectrogram3D.getWidth();
        spec3DPreferredH = spectrogram3D.getHeight();
        spec3DPreferredX = spectrogram3D.getX();
        spec3DPreferredY = spectrogram3D.getY();
        spec3DBoundsCustom = true;
    };
    spectrogram3D.onUserMoved = [this]
    {
        if (spec3DFullscreen)
            return;
        syncSpec3DFramedTools();
        spec3DPreferredX = spectrogram3D.getX();
        spec3DPreferredY = spectrogram3D.getY();
        spec3DPreferredW = spectrogram3D.getWidth();
        spec3DPreferredH = spectrogram3D.getHeight();
        spec3DBoundsCustom = true;
    };
    addChildComponent (spectrogram3D);

    addChildComponent (levelMeterIn);
    addChildComponent (levelMeterOut);
    addChildComponent (loudnessMeter);
    addChildComponent (stereogram);
    addChildComponent (histogram);
    loudnessMeter.setParameterTree (&processor.treeState);
    stereogram.setParameterTree (&processor.treeState);
    histogram.setParameterTree (&processor.treeState);
    processor.setLoudnessTarget (&loudnessMeter);
    processor.setStereogramTarget (&stereogram);
    processor.setHistogramTarget (&histogram);
    applyColourRampsToMeters();

    auto wireScopeMenu = [this] (auto& component, ScopeModuleId id)
    {
        component.onShowContextMenu = [this, id, &component]
        {
            showScopeModuleContextMenu (id, &component);
        };
    };
    auto wireScopeFullscreenDblClick = [this] (auto& component, ScopeModuleId id)
    {
        component.onDoubleClick = [this, id]
        {
            if (scopeModeEnabled)
                toggleScopePaneFullscreen (id);
        };
    };
    wireScopeMenu (levelMeterIn, ScopeModuleId::levelIn);
    wireScopeMenu (levelMeterOut, ScopeModuleId::levelOut);
    wireScopeMenu (loudnessMeter, ScopeModuleId::loudness);
    wireScopeMenu (stereogram, ScopeModuleId::stereogram);
    wireScopeMenu (histogram, ScopeModuleId::histogram);
    wireScopeMenu (oscilloscope, ScopeModuleId::oscilloscope);
    wireScopeMenu (goniometer, ScopeModuleId::goniometer);
    wireScopeMenu (spectrogram, ScopeModuleId::spectrogram);
    wireScopeMenu (m_visualizer, ScopeModuleId::spectrum);
    // Osc/Gon/Spec/3D keep richer onDoubleClick handlers (Scope fullscreen + non-Scope full-graph).
    wireScopeFullscreenDblClick (levelMeterIn, ScopeModuleId::levelIn);
    wireScopeFullscreenDblClick (levelMeterOut, ScopeModuleId::levelOut);
    wireScopeFullscreenDblClick (loudnessMeter, ScopeModuleId::loudness);
    wireScopeFullscreenDblClick (stereogram, ScopeModuleId::stereogram);
    wireScopeFullscreenDblClick (histogram, ScopeModuleId::histogram);
    wireScopeFullscreenDblClick (m_visualizer, ScopeModuleId::spectrum);

    // Default at load: mini oscilloscope only (summed stereo). Gon/Spec off; nothing maximized.
    oscButton.setToggleState (true, juce::dontSendNotification);
    oscilloscope.setEnabled (true);
    oscilloscope.setChannelMode (OscilloscopeComponent::ChannelMode::summedStereo);
    syncOscToolButtons();
    applyGoniometerActive (false);
    syncGonToolButtons();
    specButton.setToggleState (false, juce::dontSendNotification);
    spectrogram.setEnabled (false);
    syncSpecToolButtons();

    frequencyResponseComponent.onOptionBoxVisibilityChanged = [this]
    {
        syncExpandedOscOverlayStack();
    };

    if (auto* themes = menu.getThemeList())
    {
        themes->setProcessor (&processor);
        themes->setGlobalUiCapture ([this] { return captureGlobalUiModules(); });
        themes->setGlobalUiApply ([this] (const juce::ValueTree& g) { applyGlobalUiModules (g); });
        themes->addListener (this);
        refreshPresetNameDisplay();
    }

    if (auto* appearance = menu.getAppearanceComponent())
    {
        appearance->onThemeLiveChanged = [this]
        {
            applyThemeToChildComponents();
            persistSessionUiTheme();
        };
    }

    addAndMakeVisible(menu);
    menu.setVisible(false);
    raiseMenuSystemAboveWordmark();
    menu.setInterceptsMouseClicks(true, true);

    frequencyResponseComponent.setInterceptsMouseClicks(true, true);

    addAndMakeVisible (verticalGradientMeterL);
    addAndMakeVisible (verticalGradientMeterR);
    addAndMakeVisible (verticalGradientMeterPostL);
    addAndMakeVisible (verticalGradientMeterPostR);

    styleChromeButton (meterChannelModeButton);
    meterChannelModeButton.setClickingTogglesState (false);
    meterChannelModeButton.setAlwaysOnTop (true);
    meterChannelModeButton.setTooltip ("Meter channel mode - L/R or Mid/Side (M/S)");
    meterChannelModeButton.onClick = [this] { toggleMeterChannelMode(); };
    addAndMakeVisible (meterChannelModeButton);
    syncMeterChannelModeButton();

    processor.treeState.addParameterListener ("METER_CHANNEL_MODE_ID", this);
    processor.treeState.addParameterListener ("SPEC_COLOUR_SCHEME_ID", this);

    // Re-apply after full construction (ThemeList / Appearance widgets exist).
    // Host session, else last_ui_theme.xml on disk (dice prefs already use that folder).
    if (processor.hasSessionUiTheme() || processor.hasSessionUiState())
        reapplySessionUiThemeFromProcessor();
    else if (editor.loadLastUiThemeFromDisk (&sharedResources))
        reapplySessionUiThemeFromProcessor();
    else
        applyThemeToChildComponents();
}

MainComponent::~MainComponent()
{
    // Reparent 3D view out of the OS fullscreen host before teardown.
    if (spec3DFullscreen)
        exitSpec3DOsFullscreen();

    persistSessionUiTheme();
    colourRamps.removeChangeListener (this);
    processor.treeState.removeParameterListener ("METER_CHANNEL_MODE_ID", this);
    processor.treeState.removeParameterListener ("SPEC_COLOUR_SCHEME_ID", this);

    processor.getUndoManager().removeChangeListener (this);

    presetNameEditor.removeListener (this);

    if (auto* themes = menu.getThemeList())
        themes->removeListener (this);

    menu.removeComponentListener (this);

    frequencyResponseComponent.removeMouseListener (this);
    processor.setFrequencyResponseComponent (nullptr);
    processor.setOscilloscopeTarget (nullptr);
    processor.setGoniometerTarget (nullptr);
    processor.setSpectrogramTarget (nullptr);
    processor.setLoudnessTarget (nullptr);
    processor.setStereogramTarget (nullptr);
    processor.setHistogramTarget (nullptr);

    menuToggleButton.setLookAndFeel (nullptr);
    auto clearChromeLf = [] (juce::Button& b) { b.setLookAndFeel (nullptr); };
    clearChromeLf (bypassButton);
    clearChromeLf (slotAButton);
    clearChromeLf (slotBButton);
    clearChromeLf (slotCButton);
    clearChromeLf (slotDButton);
    clearChromeLf (presetPrevButton);
    clearChromeLf (presetMenuButton);
    clearChromeLf (presetNextButton);
    clearChromeLf (presetSaveButton);
    clearChromeLf (uiThemeButton);
    clearChromeLf (undoButton);
    clearChromeLf (redoButton);
    clearChromeLf (ecoButton);
    clearChromeLf (oscButton);
    clearChromeLf (gonButton);
    clearChromeLf (specButton);
    clearChromeLf (meterChannelModeButton);
    bypassAttachment.reset();

    if (m_controls.getParentComponent() != nullptr)
        m_controls.getParentComponent()->removeChildComponent (&m_controls);

    if (m_visualizer.getParentComponent() != nullptr)
        m_visualizer.getParentComponent()->removeChildComponent (&m_visualizer);
}

void MainComponent::styleChromeButton (juce::TextButton& button)
{
    const auto& c = sharedResources.sharedColors;
    button.setColour (juce::TextButton::buttonColourId, c.pluginButtonBackground);
    button.setColour (juce::TextButton::buttonOnColourId, c.pluginButtonAccent);
    button.setColour (juce::TextButton::textColourOffId, c.pluginButtonText.withAlpha (0.85f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    button.setLookAndFeel (&chromeButtonLookAndFeel);
    // Melatonin drop extends past local bounds; without this the blur is clipped away.
    button.setPaintingIsUnclipped (true);
}

void MainComponent::applyThemeToChildComponents()
{
    sharedResources.makeActive();

    styleChromeButton (bypassButton);
    styleChromeButton (slotAButton);
    styleChromeButton (slotBButton);
    styleChromeButton (slotCButton);
    styleChromeButton (slotDButton);
    styleChromeButton (presetPrevButton);
    styleChromeButton (presetMenuButton);
    styleChromeButton (presetNextButton);
    styleChromeButton (presetSaveButton);
    styleChromeButton (uiThemeButton);
    styleChromeButton (undoButton);
    styleChromeButton (redoButton);
    styleChromeButton (ecoButton);
    styleChromeButton (oscButton);
    styleChromeButton (gonButton);
    styleChromeButton (specButton);
    styleChromeButton (meterChannelModeButton);

    customLookAndFeel.setThemeColors (&sharedResources);
    ComboBoxLookAndFeel::sharedForPopupMenus().setThemeColors (&sharedResources);
    menuToggleButton.repaint();

    stylePresetNameEditor();

    oscZoomInButton.setThemeResources (&sharedResources);
    oscZoomOutButton.setThemeResources (&sharedResources);
    oscChannelModeButton.setThemeResources (&sharedResources);
    oscExpandButton.setThemeResources (&sharedResources);
    gonExpandButton.setThemeResources (&sharedResources);
    specSpeedUpButton.setThemeResources (&sharedResources);
    specSpeedDownButton.setThemeResources (&sharedResources);
    specExpandButton.setThemeResources (&sharedResources);
    spec3DButton.setThemeResources (&sharedResources);
    uiRandomizeButton.setThemeResources (&sharedResources);
    arrangeButton.setThemeResources (&sharedResources);

    frequencyResponseComponent.setThemeColors (&sharedResources);
    m_visualizer.setThemeColors (&sharedResources);
    oscilloscope.setThemeColors (&sharedResources);
    goniometer.setThemeColors (&sharedResources);
    spectrogram.setThemeColors (&sharedResources);
    spectrogram3D.setThemeColors (&sharedResources);
    oscFrame.setThemeColors (&sharedResources);
    gonFrame.setThemeColors (&sharedResources);
    specFrame.setThemeColors (&sharedResources);
    levelMeterIn.setThemeColors (&sharedResources);
    levelMeterOut.setThemeColors (&sharedResources);
    loudnessMeter.setThemeColors (&sharedResources);
    stereogram.setThemeColors (&sharedResources);
    histogram.setThemeColors (&sharedResources);
    verticalGradientMeterL.setThemeColors (&sharedResources);
    verticalGradientMeterR.setThemeColors (&sharedResources);
    verticalGradientMeterPostL.setThemeColors (&sharedResources);
    verticalGradientMeterPostR.setThemeColors (&sharedResources);
    editor.setThemeColors (&sharedResources);

    repaint();
}

void MainComponent::syncScopeChromeButtonOpacity()
{
    // When Gon is maximized, keep OSC/Gon clickable on top but see-through so the
    // correlation strip (+1) under them stays readable.
    const float a = (gonExpanded && gonButton.getToggleState()) ? 0.5f : 1.0f;
    oscButton.setAlpha (a);
    gonButton.setAlpha (a);
}

void MainComponent::syncOscToolButtons()
{
    const bool on = oscButton.getToggleState();
    oscZoomInButton.setVisible (on);
    oscZoomOutButton.setVisible (on);
    oscChannelModeButton.setVisible (on);
    oscExpandButton.setVisible (on);

    const bool split = oscilloscope.getChannelMode() == OscilloscopeComponent::ChannelMode::splitStereo;
    oscChannelModeButton.setGlyph (split ? OscToolButton::Glyph::SplitStereo
                                         : OscToolButton::Glyph::SummedStereo);
    oscChannelModeButton.setTooltip (split
                                         ? "L/R - left top, right bottom (click for summed stereo)"
                                         : "ST - summed stereo (click for split L/R)");
    oscChannelModeButton.setToggleState (split, juce::dontSendNotification);

    const bool maximized = scopeModeEnabled ? isScopeModuleFullscreen (ScopeModuleId::oscilloscope)
                                            : oscExpanded;
    oscExpandButton.setGlyph (maximized ? OscToolButton::Glyph::Collapse
                                        : OscToolButton::Glyph::Expand);
    oscExpandButton.setTooltip (maximized
                                    ? (scopeModeEnabled ? "Collapse back to Scope arrange"
                                                        : "Collapse oscilloscope back to the strip")
                                    : (scopeModeEnabled ? "Fullscreen this Scope pane"
                                                        : "Open oscilloscope in a framed floating window"));
    oscExpandButton.setToggleState (maximized, juce::dontSendNotification);
}

void MainComponent::setOscExpanded (bool shouldExpand, bool notifyPrefs)
{
    if (oscExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setGonExpanded (false, false);
        setSpecExpanded (false, false);
        oscFullGraph = false;
    }
    else
    {
        oscFullGraph = false;
    }

    oscExpanded = shouldExpand;
    oscilloscope.setExpanded (scopeModeEnabled || oscExpanded);
    syncOscToolButtons();
    resized();
    syncExpandedOscOverlayStack();
    if (shouldExpand)
        grabKeyboardFocus();
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::setGonExpanded (bool shouldExpand, bool notifyPrefs)
{
    if (gonExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setOscExpanded (false, false);
        setSpecExpanded (false, false);
        gonFullGraph = false;
    }
    else
    {
        gonFullGraph = false;
    }

    gonExpanded = shouldExpand;
    goniometer.setExpanded (scopeModeEnabled || gonExpanded);
    syncGonToolButtons();
    syncScopeChromeButtonOpacity();
    resized();
    syncExpandedOscOverlayStack();
    if (shouldExpand)
        grabKeyboardFocus();
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::setSpecExpanded (bool shouldExpand, bool notifyPrefs)
{
    if (specExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setOscExpanded (false, false);
        setGonExpanded (false, false);
        specFullGraph = false;
    }
    else
    {
        specFullGraph = false;
    }

    specExpanded = shouldExpand;
    spectrogram.setExpanded (scopeModeEnabled || specExpanded);
    syncSpecToolButtons();
    syncScopeChromeButtonOpacity();
    resized();
    syncExpandedOscOverlayStack();
    if (shouldExpand)
        grabKeyboardFocus();
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::toggleOscFullGraph()
{
    if (! oscExpanded)
        return;
    oscFullGraph = ! oscFullGraph;
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::toggleGonFullGraph()
{
    if (! gonExpanded)
        return;
    gonFullGraph = ! gonFullGraph;
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::toggleSpecFullGraph()
{
    if (! specExpanded)
        return;
    specFullGraph = ! specFullGraph;
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::closeSettingsMenu()
{
    if (! menu.isVisible() && ! menuDismissCatcher.isVisible())
        return;
    menu.setVisible (false);
    menuDismissCatcher.setVisible (false);
    // Restore parenting if Settings was floated for Spec3D OS fullscreen.
    if (menu.isOnDesktop())
    {
        menu.removeFromDesktop();
        addChildComponent (menu);
        menu.setAlwaysOnTop (false);
    }
    resized();
    syncExpandedOscOverlayStack();
    frequencyResponseComponent.setInterceptsMouseClicks (true, true);
}

void MainComponent::openSettingsMenuFromFullscreen()
{
    // Keep Spec3D fullscreen; float Settings as its own always-on-top desktop peer
    // so lookdev remains available over the OS F11 surface.
    if (! menu.isVisible())
    {
        menu.setVisible (true);
        menuDismissCatcher.setVisible (false); // FS has no editor-local catcher
        menu.setInterceptsMouseClicks (true, true);
    }

    if (! menu.isOnDesktop())
        menu.addToDesktop (juce::ComponentPeer::windowIsTemporary
                           | juce::ComponentPeer::windowIsResizable
                           | juce::ComponentPeer::windowHasDropShadow);

    menu.setAlwaysOnTop (true);
    menu.toFront (true);
    if (auto* peer = menu.getPeer())
        peer->setAlwaysOnTop (true);

    // Place beside the FS chrome (top-left of the display that hosts Spec3D).
    juce::Rectangle<int> displayArea;
    if (spec3DOsFullscreenHost != nullptr)
        displayArea = spec3DOsFullscreenHost->getBounds();
    else
    {
        const auto& d = juce::Desktop::getInstance().getDisplays().getMainDisplay();
        displayArea = d.totalArea;
    }

    constexpr int kMargin = 14;
    constexpr int kChromeH = 36 + kMargin; // below Settings/Exit strip
    const int w = juce::jlimit (280, displayArea.getWidth() - kMargin * 2, Menu::kContentWidth + 40);
    const int h = juce::jlimit (320, displayArea.getHeight() - kChromeH - kMargin,
                                displayArea.getHeight() - kChromeH - kMargin);
    menu.setBounds (displayArea.getX() + kMargin,
                    displayArea.getY() + kChromeH,
                    w, h);
    menu.setVisible (true);
    menu.toFront (true);
    if (spec3DFsExitChrome != nullptr)
        spec3DFsExitChrome->toFront (true);
}

bool MainComponent::isPointOverSettingsDismissExempt (int catcherX, int catcherY,
                                                      const juce::Component& catcher) const noexcept
{
    const auto local = getLocalPoint (&catcher, juce::Point<int> (catcherX, catcherY));

    auto over = [&] (const juce::Component& c) -> bool
    {
        return c.isShowing() && c.getBounds().contains (local);
    };

    // Floating / docked 3D spectrogram â€” keep Settings open for lookdev while orbiting.
    if (over (spectrogram3D))
        return true;

    // Framed expanded Osc / Gon / Spec windows.
    if (over (oscFrame) || over (gonFrame) || over (specFrame))
        return true;

    // Full-graph or Scope-mode analyser surfaces (not always-on-top).
    if (over (oscilloscope) || over (goniometer) || over (spectrogram))
        return true;

    // Tool columns that sit beside those windows.
    const juce::Component* tools[] = {
        &oscZoomInButton, &oscZoomOutButton, &oscChannelModeButton, &oscExpandButton,
        &gonExpandButton,
        &specSpeedUpButton, &specSpeedDownButton, &specExpandButton, &spec3DButton,
        &arrangeButton
    };
    for (auto* t : tools)
        if (t != nullptr && over (*t))
            return true;

    return false;
}

void MainComponent::rememberFrameBounds (FramedFloatingScopeWindow& frame,
                                         bool& boundsCustom,
                                         int& prefW, int& prefH, int& prefX, int& prefY)
{
    prefW = frame.getWidth();
    prefH = frame.getHeight();
    prefX = frame.getX();
    prefY = frame.getY();
    boundsCustom = true;
}

void MainComponent::deactivateAnalyserFrames()
{
    oscFrame.setContent (nullptr);
    gonFrame.setContent (nullptr);
    specFrame.setContent (nullptr);
    oscFrame.setFrameActive (false);
    gonFrame.setFrameActive (false);
    specFrame.setFrameActive (false);
}

juce::Rectangle<int> MainComponent::getFramedScopeAvailableArea() const
{
    auto area = getExpandedScopeContentBounds();
    if (area.isEmpty())
        area = getLocalBounds();

    area.setTop (juce::jmax (area.getY(), getTopChromeClearY()));

    const int bottomChrome = frequencyResponseComponent.getBottomGraphChromeHeight();
    if (bottomChrome > 0 && area.getHeight() > bottomChrome + 40)
        area.removeFromBottom (bottomChrome);

    if (verticalGradientMeterR.isVisible())
        area.setLeft (juce::jmax (area.getX(), verticalGradientMeterR.getRight() + 6));
    if (verticalGradientMeterPostL.isVisible())
        area.setRight (juce::jmin (area.getRight(), verticalGradientMeterPostL.getX() - 6));

    if (menu.isVisible())
        area.setRight (juce::jmin (area.getRight(), menu.getX() - 10));

    return area;
}

int MainComponent::getFramedToolButtonSize() const noexcept
{
    constexpr int btnGap = 2;
    return juce::jmax (14, (OscilloscopeComponent::kWindowHeightPx - 3 * btnGap) / 4);
}

int MainComponent::getFramedToolColumnWidth() const noexcept
{
    return getFramedToolButtonSize() + 8;
}

void MainComponent::clampComponentWithToolColumn (juce::Component& frame, int toolColW)
{
    auto area = getFramedScopeAvailableArea();
    if (area.getWidth() < 80 || area.getHeight() < 80)
        return;

    auto b = frame.getBounds();
    const int maxW = juce::jmax (180, area.getWidth() - toolColW);
    const int maxH = juce::jmax (120, area.getHeight());
    b.setWidth (juce::jlimit (180, maxW, b.getWidth()));
    b.setHeight (juce::jlimit (120, maxH, b.getHeight()));
    b.setX (juce::jlimit (area.getX(), area.getRight() - b.getWidth() - toolColW, b.getX()));
    b.setY (juce::jlimit (area.getY(), area.getBottom() - b.getHeight(), b.getY()));
    frame.setBounds (b);
}

void MainComponent::placeToolColumnBesideFrame (juce::Rectangle<int> frameBounds,
                                                int btnSize, int btnGap,
                                                const std::initializer_list<OscToolButton*>& buttons)
{
    const int pad = 14; // match floating shadow pad for top alignment
    const int toolX = frameBounds.getRight() + 2;
    int y = frameBounds.getY() + pad + 4;
    for (auto* b : buttons)
    {
        if (b == nullptr)
            continue;
        b->setVisible (true);
        b->setBounds (toolX, y, btnSize, btnSize);
        b->toFront (false);
        y += btnSize + btnGap;
    }
}

void MainComponent::syncOscFramedTools()
{
    if (! oscExpanded || oscFullGraph || ! oscFrame.isFrameActive())
        return;
    const int toolW = getFramedToolColumnWidth();
    clampComponentWithToolColumn (oscFrame, toolW);
    placeToolColumnBesideFrame (oscFrame.getBounds(), getFramedToolButtonSize(), 2,
                                { &oscZoomInButton, &oscZoomOutButton,
                                  &oscChannelModeButton, &oscExpandButton });
}

void MainComponent::syncGonFramedTools()
{
    if (! gonExpanded || gonFullGraph || ! gonFrame.isFrameActive())
        return;
    const int toolW = getFramedToolColumnWidth();
    clampComponentWithToolColumn (gonFrame, toolW);
    placeToolColumnBesideFrame (gonFrame.getBounds(), getFramedToolButtonSize(), 2,
                                { &gonExpandButton });
}

void MainComponent::syncSpecFramedTools()
{
    if (! specExpanded || specFullGraph || ! specFrame.isFrameActive())
        return;
    if (spec3DEnabled && specButton.getToggleState())
        return;
    const int toolW = getFramedToolColumnWidth();
    clampComponentWithToolColumn (specFrame, toolW);
    placeToolColumnBesideFrame (specFrame.getBounds(), getFramedToolButtonSize(), 2,
                                { &specSpeedUpButton, &specSpeedDownButton,
                                  &specExpandButton, &spec3DButton });
}

void MainComponent::syncSpec3DFramedTools()
{
    if (scopeModeEnabled || ! specExpanded || ! spectrogram3D.isActive())
        return;
    const int toolW = getFramedToolColumnWidth();
    if (! specFullGraph)
        clampComponentWithToolColumn (spectrogram3D, toolW);
    placeToolColumnBesideFrame (spectrogram3D.getBounds(), getFramedToolButtonSize(), 2,
                                { &specSpeedUpButton, &specSpeedDownButton,
                                  &specExpandButton, &spec3DButton });
    raiseSpecToolButtons();
}

void MainComponent::layoutFramedScopeWindow (FramedFloatingScopeWindow& frame,
                                             bool& boundsCustom,
                                             int& prefW, int& prefH, int& prefX, int& prefY,
                                             int defaultW, int defaultH,
                                             bool gonSquareShape)
{
    auto area = getFramedScopeAvailableArea();
    const int toolW = getFramedToolColumnWidth();
    if (area.getWidth() < 80 + toolW || area.getHeight() < 80)
    {
        frame.setBounds (area);
        return;
    }

    frame.setThemeColors (&sharedResources);
    frame.setChromeMode (FramedFloatingScopeWindow::ChromeMode::floating);
    frame.setResizeLimits (juce::jmax (180, area.getWidth() - toolW), area.getHeight());
    frame.setMovementBounds (area);

    auto centeredXY = [&] (int fw, int fh) -> juce::Point<int>
    {
        return { area.getX() + juce::jmax (0, (area.getWidth() - toolW - fw) / 2),
                 area.getY() + juce::jmax (0, (area.getHeight() - fh) / 3) };
    };

    int w = defaultW;
    int h = defaultH;
    auto c0 = centeredXY (w, h);
    int frameX = c0.x;
    int frameY = c0.y;

    if (boundsCustom && prefW > 0 && prefH > 0)
    {
        w = prefW;
        h = prefH;
        frameX = prefX;
        frameY = prefY;
    }

    if (gonSquareShape)
    {
        const int pad = frame.getShadowPad() * 2;
        const int corr = GoniometerComponent::kCorrelationWidthPx + 4 + FramedFloatingScopeWindow::kContentInset * 2;
        const int side = juce::jlimit (140, juce::jmin (area.getWidth() - toolW - pad, area.getHeight() - pad),
                                       h - pad);
        w = side + corr + pad;
        h = side + pad;
    }

    const int maxW = juce::jmax (180, area.getWidth() - toolW);
    w = juce::jlimit (180, maxW, w);
    h = juce::jlimit (120, area.getHeight(), h);
    const int minX = area.getX();
    const int maxX = area.getRight() - w - toolW;
    const int minY = area.getY();
    const int maxY = area.getBottom() - h;
    frameX = juce::jlimit (minX, maxX, frameX);
    frameY = juce::jlimit (minY, maxY, frameY);
    // Reject corner pins from stale prefs / first-layout 0,0.
    if (! boundsCustom || prefW <= 0 || prefH <= 0
        || (frameX <= minX + 8 && (frameY <= minY + 8 || frameY >= maxY - 8))
        || (frameY >= maxY - 8 && frameX <= minX + 24))
    {
        const auto c = centeredXY (w, h);
        frameX = c.x;
        frameY = c.y;
    }

    frame.setBounds (frameX, frameY, w, h);
    clampComponentWithToolColumn (frame, toolW);
    frame.setFrameActive (true);
    frame.toFront (false);
}

void MainComponent::restoreExpandedScope (bool osc, bool gon, bool spec)
{
    if (spec)
    {
        applySpectrogramActive (true);
        setSpecExpanded (true, false);
    }
    else if (gon)
    {
        applyGoniometerActive (true);
        setGonExpanded (true, false);
    }
    else if (osc)
    {
        if (! oscButton.getToggleState())
        {
            oscButton.setToggleState (true, juce::dontSendNotification);
            oscilloscope.setEnabled (true);
            syncOscToolButtons();
        }
        setOscExpanded (true, false);
    }
}

void MainComponent::syncGonToolButtons()
{
    const bool on = gonButton.getToggleState();
    gonExpandButton.setVisible (on);
    const bool maximized = scopeModeEnabled ? isScopeModuleFullscreen (ScopeModuleId::goniometer)
                                            : gonExpanded;
    gonExpandButton.setGlyph (maximized ? OscToolButton::Glyph::Collapse
                                        : OscToolButton::Glyph::Expand);
    gonExpandButton.setTooltip (maximized
                                    ? (scopeModeEnabled ? "Collapse back to Scope arrange"
                                                        : "Collapse goniometer back to the strip")
                                    : (scopeModeEnabled ? "Fullscreen this Scope pane"
                                                        : "Open goniometer in a framed floating window"));
    gonExpandButton.setToggleState (maximized, juce::dontSendNotification);
}

void MainComponent::applyGoniometerActive (bool shouldEnable)
{
    if (! shouldEnable)
        setGonExpanded (false);

    gonButton.setToggleState (shouldEnable, juce::dontSendNotification);
    goniometer.setEnabled (shouldEnable);
    syncGonToolButtons();
    syncScopeChromeButtonOpacity();
    resized();
}

void MainComponent::syncSpecToolButtons()
{
    const bool on = specButton.getToggleState();
    // Scope panes place their own speed/expand overlays; never show the 2D/3D cube there.
    if (! scopeModeEnabled)
    {
        specSpeedUpButton.setVisible (on);
        specSpeedDownButton.setVisible (on);
        specExpandButton.setVisible (on);
        spec3DButton.setVisible (on);
    }
    else
    {
        spec3DButton.setVisible (false);
    }
    spec3DButton.setToggleState (spec3DEnabled, juce::dontSendNotification);
    spec3DButton.setThemeResources (&sharedResources);
    spec3DButton.setTooltip (
        specExpanded
            ? "3D spectrogram - orbit (drag), ground pan (Shift/MMB), screen pan (RMB), zoom (wheel). Ctrl+RMB: view menu."
            : "3D spectrogram (applies when expanded). Right-click cube: mesh quality.");

    const bool maximized = scopeModeEnabled ? isScopeModuleFullscreen (ScopeModuleId::spectrogram)
                                            : specExpanded;
    specExpandButton.setGlyph (maximized ? OscToolButton::Glyph::Collapse
                                         : OscToolButton::Glyph::Expand);
    specExpandButton.setTooltip (maximized
                                     ? (scopeModeEnabled ? "Collapse back to Scope arrange"
                                                         : "Collapse spectrogram back to the strip")
                                     : (scopeModeEnabled ? "Fullscreen this Scope pane"
                                                         : "Open spectrogram in a framed floating window"));
    specExpandButton.setToggleState (maximized, juce::dontSendNotification);
    syncSpec3DPresentation();
}

void MainComponent::raiseSpecToolButtons()
{
    specSpeedUpButton.toFront (false);
    specSpeedDownButton.toFront (false);
    specExpandButton.toFront (false);
    spec3DButton.toFront (false);
}

juce::Rectangle<int> MainComponent::getExpandedScopeContentBounds() const
{
    auto b = frequencyResponseComponent.getBounds();
    const int pianoH = frequencyResponseComponent.getPianoStripHeight();
    if (pianoH > 0)
        b = b.withTrimmedBottom (pianoH);
    return b;
}

int MainComponent::getTopChromeClearY() const
{
    int bottom = 0;
    auto consider = [&bottom] (const juce::Component& c)
    {
        if (c.isVisible() && c.getWidth() > 0 && c.getHeight() > 0)
            bottom = juce::jmax (bottom, c.getBottom());
    };
    consider (bypassButton);
    consider (slotAButton);
    consider (presetPrevButton);
    consider (presetNameEditor);
    consider (menuToggleButton);
    consider (undoButton);
    consider (redoButton);
    consider (uiThemeButton);
    consider (uiRandomizeButton);
    consider (ecoButton);
    consider (oscButton);
    consider (specButton);
    return bottom + 6;
}

void MainComponent::layoutExpandedSpectrogramWithTools (int btnSize, int btnGap)
{
    // OpenGL peer is inset inside spectrogram3D's framed window â€” keep tools / Settings
    // outside the GL host rect (native HWND ignores JUCE z-order).
    auto area = getExpandedScopeContentBounds();
    if (area.isEmpty())
        area = getLocalBounds();

    area.setTop (juce::jmax (area.getY(), getTopChromeClearY()));

    // Match / Mod / P / Help / SideCheck sit above the piano â€” keep GL clear of that row.
    const int bottomChrome = frequencyResponseComponent.getBottomGraphChromeHeight();
    if (bottomChrome > 0 && area.getHeight() > bottomChrome + 40)
        area.removeFromBottom (bottomChrome);

    if (verticalGradientMeterR.isVisible())
        area.setLeft (juce::jmax (area.getX(), verticalGradientMeterR.getRight() + 6));
    if (verticalGradientMeterPostL.isVisible())
        area.setRight (juce::jmin (area.getRight(), verticalGradientMeterPostL.getX() - 6));

    // Settings open: pin left, shrink from the right so the 3D window clears the menu.
    if (menu.isVisible())
        area.setRight (juce::jmin (area.getRight(), menu.getX() - 10));

    const int toolW = getFramedToolColumnWidth();
    if (area.getWidth() <= toolW + 80 || area.getHeight() <= btnSize + 40)
    {
        spectrogram.setBounds (area);
        spectrogram3D.setBounds (area);
        return;
    }

    const int maxFrameW = juce::jmax (220, area.getWidth() - toolW);

    // Default: centered floating window (same idea as osc/gon framed scopes).
    auto centeredXY = [&] (int fw, int fh) -> juce::Point<int>
    {
        return { area.getX() + juce::jmax (0, (area.getWidth() - toolW - fw) / 2),
                 area.getY() + juce::jmax (0, (area.getHeight() - fh) / 3) };
    };

    int w = juce::jlimit (220, maxFrameW, juce::jmin (maxFrameW, juce::jmax (420, area.getWidth() * 2 / 3)));
    int h = juce::jlimit (160, area.getHeight(), juce::jmin (area.getHeight(), juce::jmax (280, area.getHeight() * 2 / 3)));
    auto c0 = centeredXY (w, h);
    int frameX = c0.x;
    int frameY = c0.y;

    if (spec3DBoundsCustom && spec3DPreferredW > 0 && spec3DPreferredH > 0)
    {
        w = juce::jlimit (220, maxFrameW, spec3DPreferredW);
        h = juce::jlimit (160, area.getHeight(), spec3DPreferredH);
        const int minX = area.getX();
        const int maxX = area.getRight() - w - toolW;
        const int minY = area.getY();
        const int maxY = area.getBottom() - h;
        // 0,0 / missing = never placed. Also reject corner-clamped restores (lower/upper
        // left) which read as "stuck in the corner" after session load.
        const bool hasSavedPos = spec3DPreferredX > minX + 4 || spec3DPreferredY > minY + 4;
        if (hasSavedPos)
        {
            frameX = juce::jlimit (minX, maxX, spec3DPreferredX);
            frameY = juce::jlimit (minY, maxY, spec3DPreferredY);
            const bool pinnedCorner = (frameX <= minX + 8 && (frameY <= minY + 8 || frameY >= maxY - 8))
                                   || (frameY >= maxY - 8 && frameX <= minX + 24);
            if (pinnedCorner)
            {
                const auto c = centeredXY (w, h);
                frameX = c.x;
                frameY = c.y;
            }
        }
        else
        {
            const auto c = centeredXY (w, h);
            frameX = c.x;
            frameY = c.y;
        }
    }

    const auto placed = juce::Rectangle<int> (frameX, frameY, w, h);
    const bool show3D = spec3DEnabled && specButton.getToggleState();
    // Scope panes stay docked (hard GL HWND). Soft FBO compositing is floating-only.
    spectrogram3D.setChromeMode (scopeModeEnabled
                                     ? Spectrogram3DComponent::ChromeMode::docked
                                     : Spectrogram3DComponent::ChromeMode::floating);
    spectrogram3D.setResizeLimits (maxFrameW, area.getHeight());
    spectrogram3D.setMovementBounds (area);
    spectrogram3D.setBounds (placed);
    if (! scopeModeEnabled)
        clampComponentWithToolColumn (spectrogram3D, toolW);

    // 2D fills the window; when 3D is on it tracks the inner frame under the GL host.
    const int pad = show3D ? spectrogram3D.getShadowPad() : 0;
    spectrogram.setBounds (pad > 0 ? spectrogram3D.getBounds().reduced (pad) : spectrogram3D.getBounds());

    juce::ignoreUnused (btnGap);
    syncSpec3DFramedTools();
}

bool MainComponent::collapseAnyExpandedScope()
{
    bool collapsed = false;
    if (spec3DFullscreen)
    {
        setSpec3DFullscreen (false, true);
        collapsed = true;
    }
    if (scopeModeEnabled && scopeFullscreenModule.has_value())
    {
        setScopeFullscreenModule (std::nullopt);
        collapsed = true;
    }
    if (specExpanded) { setSpecExpanded (false); collapsed = true; }
    if (oscExpanded)  { setOscExpanded (false);  collapsed = true; }
    if (gonExpanded)  { setGonExpanded (false);  collapsed = true; }
    return collapsed;
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::F11Key)
    {
        // F11 toggles Spec3D fullscreen when 3D is available / already fullscreen.
        const bool canFs = spec3DFullscreen
            || spectrogram3D.isActive()
            || (scopeModeEnabled && isScopeModuleEnabled (ScopeModuleId::spectrogram3D))
            || (! scopeModeEnabled && spec3DEnabled);
        if (canFs)
        {
            toggleSpec3DFullscreen (true);
            return true;
        }
    }
    if (key == juce::KeyPress::escapeKey && collapseAnyExpandedScope())
        return true;
    return false;
}

void MainComponent::syncSpec3DPresentation()
{
    const bool has3DModule = scopeModeEnabled && isScopeModuleEnabled (ScopeModuleId::spectrogram3D);
    const bool hasSpecModule = scopeModeEnabled && isScopeModuleEnabled (ScopeModuleId::spectrogram);
    // Scope: 3D only via its own module. Expanded (non-Scope): cube flag on Spec expand.
    // Fullscreen forces presentation even if intermediate chrome is mid-toggle.
    const bool show3D = spec3DFullscreen
                        || (scopeModeEnabled && has3DModule)
                        || (! scopeModeEnabled && specExpanded && spec3DEnabled
                            && specButton.getToggleState());

    spectrogram3D.setAlwaysOnTop (false);
    spectrogram3D.setThemeColors (&sharedResources);
    spectrogram3D.setChromeMode ((spec3DFullscreen || (scopeModeEnabled && show3D))
                                     ? Spectrogram3DComponent::ChromeMode::docked
                                     : Spectrogram3DComponent::ChromeMode::floating);
    spectrogram3D.setActive (show3D);
    spectrogram3D.setVisible (show3D);

    // Mesh history comes from the 2D spectrogram feeder — keep analysing whenever 3D is up.
    if (show3D)
        spectrogram.setEnabled (true);

    const bool show2D = (! scopeModeEnabled && specButton.getToggleState() && ! show3D)
                        || (scopeModeEnabled && hasSpecModule)
                        || (! scopeModeEnabled && specButton.getToggleState() && specExpanded && ! spec3DEnabled);
    // Expanded non-Scope with 3D on: hide 2D under the floating 3D frame.
    const bool show2DFinal = ! spec3DFullscreen
                             && (scopeModeEnabled
                                     ? hasSpecModule
                                     : (specButton.getToggleState() && ! (specExpanded && spec3DEnabled)));

    spectrogram.setVisible (show2DFinal);
    spectrogram.setInterceptsMouseClicks (show2DFinal && (specExpanded || scopeModeEnabled), true);
    juce::ignoreUnused (show2D);

    if (spec3DFullscreen)
    {
        applySpec3DFullscreenLayout();
        return;
    }

    if (show3D)
    {
        spectrogram3D.setInterceptsMouseClicks (true, true);
        spectrogram3D.toFront (false);
        raiseSpecToolButtons();
        menuToggleButton.toFront (false);
        if (menu.isVisible())
            menu.toFront (false);
        bypassButton.toFront (false);
        undoButton.toFront (false);
        redoButton.toFront (false);
    }
    else if (show2DFinal)
    {
        raiseSpecToolButtons();
    }
}

void MainComponent::requestUiPrefsSave() noexcept
{
    editor.requestSaveUiPrefs();
}

void MainComponent::setSpec3DMode (bool shouldEnable, bool notifyPrefs)
{
    if (scopeModeEnabled)
    {
        const bool moduleOn = isScopeModuleEnabled (ScopeModuleId::spectrogram3D);
        if (shouldEnable == moduleOn && spec3DEnabled == shouldEnable)
            return;
    }
    else if (spec3DEnabled == shouldEnable)
    {
        return;
    }

    spec3DEnabled = shouldEnable;

    // Settings / cube outside Scope: expanded overlay preference.
    // Inside Scope: also toggle the independent Spectrogram 3D module.
    if (scopeModeEnabled)
        setScopeModuleEnabled (ScopeModuleId::spectrogram3D, shouldEnable, false);

    if (! shouldEnable && spec3DFullscreen)
        spec3DFullscreen = false;

    syncSpecToolButtons();
    resized();
    syncExpandedOscOverlayStack();
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::setSpec3DFullscreen (bool shouldEnable, bool notifyPrefs)
{
    if (spec3DFullscreen == shouldEnable)
        return;

    if (shouldEnable)
    {
        // Ensure 3D is live so the mesh keeps feeding while OS-fullscreen.
        if (scopeModeEnabled)
        {
            if (! isScopeModuleEnabled (ScopeModuleId::spectrogram3D))
                setScopeModuleEnabled (ScopeModuleId::spectrogram3D, true, false);
            setScopeFullscreenModule (std::nullopt);
        }
        else
        {
            if (! spec3DEnabled)
                setSpec3DMode (true, false);
            if (! specExpanded)
                setSpecExpanded (true, false);
            if (! specButton.getToggleState())
                specButton.setToggleState (true, juce::dontSendNotification);
        }
        if (oscExpanded) setOscExpanded (false, false);
        if (gonExpanded) setGonExpanded (false, false);

        // Close settings panel — it lives on the editor, not the OS FS surface.
        if (menu.isVisible())
            closeSettingsMenu();
    }

    spec3DFullscreen = shouldEnable;

    if (shouldEnable)
        enterSpec3DOsFullscreen();
    else
        exitSpec3DOsFullscreen();

    resized();
    syncExpandedOscOverlayStack();
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::toggleSpec3DFullscreen (bool notifyPrefs)
{
    setSpec3DFullscreen (! spec3DFullscreen, notifyPrefs);
}

void MainComponent::enterSpec3DOsFullscreen()
{
    // Keep spectrogram analysis feeding the mesh while the view is reparented.
    spectrogram.setEnabled (true);

    spectrogram3D.setThemeColors (&sharedResources);
    spectrogram3D.setChromeMode (Spectrogram3DComponent::ChromeMode::docked);
    spectrogram3D.setActive (true);
    spectrogram3D.setVisible (true);

    if (spec3DOsFullscreenHost == nullptr)
        spec3DOsFullscreenHost = std::make_unique<Spec3DOsFullscreenHost> (*this);

    if (spectrogram3D.getParentComponent() != spec3DOsFullscreenHost.get())
        spec3DOsFullscreenHost->attachView (spectrogram3D);
    else
    {
        // Already attached — just re-assert bounds / FS peer.
        const auto pluginScreen = localAreaToGlobal (getLocalBounds());
        const auto* display = juce::Desktop::getInstance().getDisplays()
                                  .getDisplayForRect (pluginScreen);
        if (display == nullptr)
            display = &juce::Desktop::getInstance().getDisplays().getMainDisplay();
        spec3DOsFullscreenHost->setBounds (display->totalArea);
        if (auto* peer = spec3DOsFullscreenHost->getPeer())
            peer->setFullScreen (true);
        spectrogram3D.setBounds (spec3DOsFullscreenHost->getLocalBounds());
    }

    // Exit control on its own always-on-top peer (above native GL HWND).
    if (spec3DFsExitChrome == nullptr)
        spec3DFsExitChrome = std::make_unique<Spec3DFsExitChrome> (*this);
    {
        const auto hostBounds = spec3DOsFullscreenHost->getBounds();
        spec3DFsExitChrome->placeOnDisplay (hostBounds);
    }

    spec3DOsFullscreenHost->grabKeyboardFocus();
    spectrogram3D.grabKeyboardFocus();
}

void MainComponent::exitSpec3DOsFullscreen()
{
    // Bring Settings back into the editor if it was floated for lookdev.
    if (menu.isOnDesktop())
    {
        menu.setVisible (false);
        menu.removeFromDesktop();
        addChildComponent (menu);
        menu.setAlwaysOnTop (false);
    }

    if (spec3DFsExitChrome != nullptr)
    {
        spec3DFsExitChrome->dismiss();
        spec3DFsExitChrome.reset();
    }

    if (spec3DOsFullscreenHost != nullptr)
    {
        if (spectrogram3D.getParentComponent() == spec3DOsFullscreenHost.get())
            spec3DOsFullscreenHost->detachView (spectrogram3D, *this);
        spec3DOsFullscreenHost.reset();
    }

    // Restore in-editor presentation.
    spectrogram3D.setThemeColors (&sharedResources);
    addChildComponent (spectrogram3D);
    syncSpec3DPresentation();
}

void MainComponent::applySpec3DFullscreenLayout()
{
    if (! spec3DFullscreen)
        return;

    // Plugin editor resized while OS FS is up — keep the desktop host on the right display.
    if (spec3DOsFullscreenHost != nullptr)
    {
        const auto pluginScreen = localAreaToGlobal (getLocalBounds());
        const auto* display = juce::Desktop::getInstance().getDisplays()
                                  .getDisplayForRect (pluginScreen);
        if (display == nullptr)
            display = &juce::Desktop::getInstance().getDisplays().getMainDisplay();
        if (spec3DOsFullscreenHost->getBounds() != display->totalArea)
            spec3DOsFullscreenHost->setBounds (display->totalArea);
        spectrogram3D.setBounds (spec3DOsFullscreenHost->getLocalBounds());
        if (spec3DFsExitChrome != nullptr)
            spec3DFsExitChrome->placeOnDisplay (display->totalArea);
    }
}

void MainComponent::setSpec3DMeshQuality (Spectrogram3DComponent::MeshQuality q, bool notifyPrefs)
{
    spectrogram3D.setMeshQuality (q);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

Spectrogram3DComponent::MeshQuality MainComponent::getSpec3DMeshQuality() const noexcept
{
    return spectrogram3D.getMeshQuality();
}

void MainComponent::setSpec3DFreqMeshBias (float amount01, bool notifyPrefs)
{
    spectrogram3D.setFreqMeshBias (amount01);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

float MainComponent::getSpec3DFreqMeshBias() const noexcept
{
    return spectrogram3D.getFreqMeshBias();
}

void MainComponent::setSpec3DFreqMeshBiasPivot (float pivot01, bool notifyPrefs)
{
    spectrogram3D.setFreqMeshBiasPivot (pivot01);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

float MainComponent::getSpec3DFreqMeshBiasPivot() const noexcept
{
    return spectrogram3D.getFreqMeshBiasPivot();
}

void MainComponent::setSpec3DMsaaLevel (Spectrogram3DComponent::MsaaLevel level, bool notifyPrefs)
{
    spectrogram3D.setMsaaLevel (level);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

Spectrogram3DComponent::MsaaLevel MainComponent::getSpec3DMsaaLevel() const noexcept
{
    return spectrogram3D.getMsaaLevel();
}

bool MainComponent::isSpec3DMultisampling() const noexcept
{
    return spectrogram3D.isMultisamplingEnabled();
}

void MainComponent::setSpec3DTransparentBackground (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setTransparentBackground (shouldEnable);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

bool MainComponent::isSpec3DTransparentBackground() const noexcept
{
    return spectrogram3D.isTransparentBackground();
}

void MainComponent::setSpec3DReverseFrequencyAxis (bool shouldReverse, bool notifyPrefs)
{
    spectrogram3D.setReverseFrequencyAxis (shouldReverse);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

bool MainComponent::isSpec3DReverseFrequencyAxis() const noexcept
{
    return spectrogram3D.isReverseFrequencyAxis();
}

void MainComponent::setSpec3DMeshHeight (float heightWorld, bool notifyPrefs)
{
    spectrogram3D.setMeshHeight (heightWorld);
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

float MainComponent::getSpec3DMeshHeight() const noexcept
{
    return spectrogram3D.getMeshHeight();
}

void MainComponent::setSpec3DClosedMeshEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setClosedMeshEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DClosedMeshEnabled() const noexcept
{
    return spectrogram3D.isClosedMeshEnabled();
}

void MainComponent::setSpec3DAutoRotateEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setAutoRotateEnabled (shouldEnable, notifyPrefs);
}
bool MainComponent::isSpec3DAutoRotateEnabled() const noexcept
{
    return spectrogram3D.isAutoRotateEnabled();
}

Spec3DRampSequence MainComponent::getSpec3DRampSequence() const noexcept
{
    return spectrogram3D.getRampSequence();
}

void MainComponent::setSpec3DRampSequence (const Spec3DRampSequence& seq, bool notifyPrefs)
{
    spectrogram3D.setColourRampBank (&colourRamps);
    spectrogram3D.setRampSequence (seq);
    if (rampTimelineWindow != nullptr)
        rampTimelineWindow->getTimeline().setPlayheadSec (spectrogram3D.getRampTimelinePlayheadSec());
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::showRampTimelineWindow()
{
    if (rampTimelineWindow == nullptr)
    {
        rampTimelineWindow = std::make_unique<Spec3DRampTimelineWindow> (
            sharedResources, colourRamps, spectrogram3D.getRampSequence());
        rampTimelineWindow->setThemeColors (&sharedResources);
        rampTimelineWindow->getTimeline().playheadProvider =
            [this] { return spectrogram3D.getRampTimelinePlayheadSec(); };
        rampTimelineWindow->getTimeline().onExportRegionOffline =
            [this] (const Spec3DExportSettings& s) { startSpec3DRegionExport (s); };
        rampTimelineWindow->onSequenceChanged = [this] { editor.requestSaveUiPrefs(); };
        rampTimelineWindow->onEnabledChanged = [this]
        {
            if (! spectrogram3D.getRampSequence().enabled)
                spectrogram3D.clearMorphRamp();
            editor.requestSaveUiPrefs();
        };
        rampTimelineWindow->onClose = [this] {};
        addAndMakeVisible (*rampTimelineWindow);
        rampTimelineWindow->setBounds ((getWidth() - 560) / 2, (getHeight() - 200) / 2, 560, 190);
    }

    rampTimelineWindow->setVisible (true);
    rampTimelineWindow->toFront (true);
    rampTimelineWindow->setPlayheadSec (spectrogram3D.getRampTimelinePlayheadSec());
}

void MainComponent::startSpec3DRegionExport (const Spec3DExportSettings& settings)
{
#if ! SPEC3D_EXPORT_ENABLED
    juce::ignoreUnused (settings);
    juce::AlertWindow::showMessageBoxAsync (
        juce::AlertWindow::InfoIcon,
        "Export region offline",
        "Offline export is sandboxed for now (kept for a future standalone / re-enable).\n\n"
        "To restore: set SPEC3D_EXPORT_ENABLED to 1 in Source/Export/Spec3DExportSandbox.h "
        "and rebuild.");
    return;
#else
    if (activeSpec3DExport != nullptr)
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Export region offline",
                                                "An export is already running.");
        return;
    }

    if (! spectrogram3D.isActive())
    {
        juce::AlertWindow::showMessageBoxAsync (juce::AlertWindow::WarningIcon,
                                                "Export region offline",
                                                "Enable Spectrogram 3D before exporting.");
        return;
    }

    activeSpec3DExport = std::make_unique<Spec3DExportJob> (
        spectrogram3D, processor.getExportAudioRing(), settings);
    activeSpec3DExport->onFinished =
        [this] (bool ok, juce::String msg)
        {
            juce::MessageManager::callAsync (
                [this, ok, msg]
                {
                    activeSpec3DExport.reset();
                    juce::AlertWindow::showMessageBoxAsync (
                        ok ? juce::AlertWindow::InfoIcon : juce::AlertWindow::WarningIcon,
                        "Export region offline",
                        msg);
                });
        };
    activeSpec3DExport->launchThread();
#endif
}

void MainComponent::setSpec3DAutoRotatePeriodSec (float secondsPerRevolution, bool notifyPrefs)
{
    spectrogram3D.setAutoRotatePeriodSec (secondsPerRevolution, notifyPrefs);
}
float MainComponent::getSpec3DAutoRotatePeriodSec() const noexcept
{
    return spectrogram3D.getAutoRotatePeriodSec();
}

void MainComponent::setSpec3DZoomOscillateEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setZoomOscillateEnabled (shouldEnable, notifyPrefs);
}
bool MainComponent::isSpec3DZoomOscillateEnabled() const noexcept
{
    return spectrogram3D.isZoomOscillateEnabled();
}

void MainComponent::setSpec3DZoomOscillateDepth (float amount01, bool notifyPrefs)
{
    spectrogram3D.setZoomOscillateDepth (amount01, notifyPrefs);
}
float MainComponent::getSpec3DZoomOscillateDepth() const noexcept
{
    return spectrogram3D.getZoomOscillateDepth();
}

void MainComponent::setSpec3DZoomOscillatePeriodSec (float secondsPerCycle, bool notifyPrefs)
{
    spectrogram3D.setZoomOscillatePeriodSec (secondsPerCycle, notifyPrefs);
}
float MainComponent::getSpec3DZoomOscillatePeriodSec() const noexcept
{
    return spectrogram3D.getZoomOscillatePeriodSec();
}

void MainComponent::syncSpec3DAudioSidechainToProcessor() noexcept
{
    float atk = 8.0f, rel = 80.0f;
    Spectrogram3DComponent::audioLevelBallisticsMs (spectrogram3D.getAudioLevelSpeed(), atk, rel);
    processor.configureSpec3DVisualSidechain (spectrogram3D.isAudioLevelModEnabled(),
                                              spectrogram3D.getAudioLevelHpHz(),
                                              spectrogram3D.getAudioLevelLpHz(),
                                              spectrogram3D.getAudioLevelThresholdDb(),
                                              atk, rel);
}

void MainComponent::setSpec3DAudioLevelModEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelModEnabled (shouldEnable);
    syncSpec3DAudioSidechainToProcessor();
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DAudioLevelModEnabled() const noexcept
{
    return spectrogram3D.isAudioLevelModEnabled();
}

void MainComponent::setSpec3DAudioLevelTarget (Spectrogram3DComponent::AudioLevelTarget target,
                                              bool notifyPrefs)
{
    spectrogram3D.setAudioLevelTarget (target);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::AudioLevelTarget MainComponent::getSpec3DAudioLevelTarget() const noexcept
{
    return spectrogram3D.getAudioLevelTarget();
}

void MainComponent::setSpec3DAudioLevelMinPercent (float pct, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelMinPercent (pct);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DAudioLevelMinPercent() const noexcept
{
    return spectrogram3D.getAudioLevelMinPercent();
}

void MainComponent::setSpec3DAudioLevelMaxPercent (float pct, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelMaxPercent (pct);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DAudioLevelMaxPercent() const noexcept
{
    return spectrogram3D.getAudioLevelMaxPercent();
}

void MainComponent::setSpec3DAudioLevelHpHz (float hz, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelHpHz (hz);
    syncSpec3DAudioSidechainToProcessor();
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DAudioLevelHpHz() const noexcept
{
    return spectrogram3D.getAudioLevelHpHz();
}

void MainComponent::setSpec3DAudioLevelLpHz (float hz, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelLpHz (hz);
    syncSpec3DAudioSidechainToProcessor();
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DAudioLevelLpHz() const noexcept
{
    return spectrogram3D.getAudioLevelLpHz();
}

void MainComponent::setSpec3DAudioLevelThresholdDb (float thresholdDb, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelThresholdDb (thresholdDb);
    syncSpec3DAudioSidechainToProcessor();
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DAudioLevelThresholdDb() const noexcept
{
    return spectrogram3D.getAudioLevelThresholdDb();
}

void MainComponent::setSpec3DAudioLevelSpeed (Spectrogram3DComponent::AudioLevelSpeed speed,
                                             bool notifyPrefs)
{
    spectrogram3D.setAudioLevelSpeed (speed);
    syncSpec3DAudioSidechainToProcessor();
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::AudioLevelSpeed MainComponent::getSpec3DAudioLevelSpeed() const noexcept
{
    return spectrogram3D.getAudioLevelSpeed();
}

void MainComponent::setSpec3DAudioLevelAffectPlayhead (bool shouldAffect, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelAffectPlayhead (shouldAffect);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::getSpec3DAudioLevelAffectPlayhead() const noexcept
{
    return spectrogram3D.getAudioLevelAffectPlayhead();
}

void MainComponent::setSpec3DAudioLevelAffectAntiPlayhead (bool shouldAffect, bool notifyPrefs)
{
    spectrogram3D.setAudioLevelAffectAntiPlayhead (shouldAffect);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::getSpec3DAudioLevelAffectAntiPlayhead() const noexcept
{
    return spectrogram3D.getAudioLevelAffectAntiPlayhead();
}

void MainComponent::setSpec3DNormalCuspAngleDeg (float deg, bool notifyPrefs)
{
    spectrogram3D.setNormalCuspAngleDeg (deg);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DNormalCuspAngleDeg() const noexcept
{
    return spectrogram3D.getNormalCuspAngleDeg();
}

void MainComponent::setSpec3DNormalWeighting (Spectrogram3DComponent::NormalWeighting method,
                                             bool notifyPrefs)
{
    spectrogram3D.setNormalWeighting (method);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::NormalWeighting MainComponent::getSpec3DNormalWeighting() const noexcept
{
    return spectrogram3D.getNormalWeighting();
}

void MainComponent::setSpec3DLightingEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setLightingEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DLightingEnabled() const noexcept { return spectrogram3D.isLightingEnabled(); }

void MainComponent::setSpec3DLightingAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setLightingAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DLightingAmount() const noexcept { return spectrogram3D.getLightingAmount(); }

void MainComponent::setSpec3DLightAzimuthDeg (float deg, bool notifyPrefs)
{
    spectrogram3D.setLightAzimuthDeg (deg);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DLightAzimuthDeg() const noexcept { return spectrogram3D.getLightAzimuthDeg(); }

void MainComponent::setSpec3DLightElevationDeg (float deg, bool notifyPrefs)
{
    spectrogram3D.setLightElevationDeg (deg);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DLightElevationDeg() const noexcept { return spectrogram3D.getLightElevationDeg(); }

void MainComponent::setSpec3DSpecularAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSpecularAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSpecularAmount() const noexcept { return spectrogram3D.getSpecularAmount(); }

void MainComponent::setSpec3DRoughnessAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setRoughnessAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DRoughnessAmount() const noexcept { return spectrogram3D.getRoughnessAmount(); }

void MainComponent::setSpec3DMetalnessAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setMetalnessAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DMetalnessAmount() const noexcept { return spectrogram3D.getMetalnessAmount(); }

void MainComponent::setSpec3DRimAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setRimAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DRimAmount() const noexcept { return spectrogram3D.getRimAmount(); }

void MainComponent::setSpec3DLightColour (juce::Colour c, bool notifyPrefs)
{
    spectrogram3D.setLightColour (c);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Colour MainComponent::getSpec3DLightColour() const noexcept { return spectrogram3D.getLightColour(); }

void MainComponent::setSpec3DRimColour (juce::Colour c, bool notifyPrefs)
{
    spectrogram3D.setRimColour (c);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Colour MainComponent::getSpec3DRimColour() const noexcept { return spectrogram3D.getRimColour(); }

void MainComponent::setSpec3DDomeFillEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setDomeFillEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DDomeFillEnabled() const noexcept { return spectrogram3D.isDomeFillEnabled(); }

void MainComponent::setSpec3DDomeFillStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDomeFillStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDomeFillStrength() const noexcept { return spectrogram3D.getDomeFillStrength(); }

void MainComponent::setSpec3DDomeSkyColour (juce::Colour c, bool notifyPrefs)
{
    spectrogram3D.setDomeSkyColour (c);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Colour MainComponent::getSpec3DDomeSkyColour() const noexcept { return spectrogram3D.getDomeSkyColour(); }

void MainComponent::setSpec3DDomeGroundColour (juce::Colour c, bool notifyPrefs)
{
    spectrogram3D.setDomeGroundColour (c);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Colour MainComponent::getSpec3DDomeGroundColour() const noexcept { return spectrogram3D.getDomeGroundColour(); }

void MainComponent::setSpec3DDomeTextureEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setDomeTextureEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DDomeTextureEnabled() const noexcept { return spectrogram3D.isDomeTextureEnabled(); }

void MainComponent::setSpec3DDomeTextureSource (Spectrogram3DComponent::DomeTextureSource source,
                                                bool notifyPrefs)
{
    spectrogram3D.setDomeTextureSource (source);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::DomeTextureSource MainComponent::getSpec3DDomeTextureSource() const noexcept
{
    return spectrogram3D.getDomeTextureSource();
}

void MainComponent::setSpec3DDomeTextureCustomPath (const juce::String& absolutePath, bool notifyPrefs)
{
    spectrogram3D.setDomeTextureCustomPath (absolutePath);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::String MainComponent::getSpec3DDomeTextureCustomPath() const noexcept
{
    return spectrogram3D.getDomeTextureCustomPath();
}

void MainComponent::setSpec3DSsgiEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsgiEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsgiEnabled() const noexcept { return spectrogram3D.isSsgiEnabled(); }

void MainComponent::setSpec3DSsgiStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsgiStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsgiStrength() const noexcept { return spectrogram3D.getSsgiStrength(); }

void MainComponent::setSpec3DSsgiRadius (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsgiRadius (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsgiRadius() const noexcept { return spectrogram3D.getSsgiRadius(); }

void MainComponent::setSpec3DSsgiQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setSsgiQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DSsgiQuality() const noexcept
{
    return spectrogram3D.getSsgiQuality();
}

void MainComponent::setSpec3DSsgiTemporalEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsgiTemporalEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsgiTemporalEnabled() const noexcept { return spectrogram3D.isSsgiTemporalEnabled(); }

void MainComponent::setSpec3DSsgiTemporalAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsgiTemporalAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsgiTemporalAmount() const noexcept { return spectrogram3D.getSsgiTemporalAmount(); }

void MainComponent::setSpec3DSsgiDenoiseEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsgiDenoiseEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsgiDenoiseEnabled() const noexcept { return spectrogram3D.isSsgiDenoiseEnabled(); }

void MainComponent::setSpec3DSsgiDenoiseAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsgiDenoiseAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsgiDenoiseAmount() const noexcept { return spectrogram3D.getSsgiDenoiseAmount(); }

void MainComponent::setSpec3DSsgiDenoiseMode (Spectrogram3DComponent::SsgiDenoiseMode mode, bool notifyPrefs)
{
    spectrogram3D.setSsgiDenoiseMode (mode);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::SsgiDenoiseMode MainComponent::getSpec3DSsgiDenoiseMode() const noexcept
{
    return spectrogram3D.getSsgiDenoiseMode();
}

void MainComponent::setSpec3DSsgiAtrousQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setSsgiAtrousQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DSsgiAtrousQuality() const noexcept
{
    return spectrogram3D.getSsgiAtrousQuality();
}

void MainComponent::setSpec3DSsgiHalfResEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsgiHalfResEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsgiHalfResEnabled() const noexcept { return spectrogram3D.isSsgiHalfResEnabled(); }

void MainComponent::setSpec3DSsgiMeshNormalsEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsgiMeshNormalsEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsgiMeshNormalsEnabled() const noexcept { return spectrogram3D.isSsgiMeshNormalsEnabled(); }

void MainComponent::setSpec3DSsrEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsrEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsrEnabled() const noexcept { return spectrogram3D.isSsrEnabled(); }

void MainComponent::setSpec3DSsrStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrStrength() const noexcept { return spectrogram3D.getSsrStrength(); }

void MainComponent::setSpec3DSsrDistance (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrDistance (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrDistance() const noexcept { return spectrogram3D.getSsrDistance(); }

void MainComponent::setSpec3DSsrThickness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrThickness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrThickness() const noexcept { return spectrogram3D.getSsrThickness(); }

void MainComponent::setSpec3DSsrQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setSsrQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DSsrQuality() const noexcept
{
    return spectrogram3D.getSsrQuality();
}

void MainComponent::setSpec3DSsrFresnel (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrFresnel (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrFresnel() const noexcept { return spectrogram3D.getSsrFresnel(); }

void MainComponent::setSpec3DSsrRoughnessInfluence (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrRoughnessInfluence (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrRoughnessInfluence() const noexcept
{
    return spectrogram3D.getSsrRoughnessInfluence();
}

void MainComponent::setSpec3DSsrIntensity (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrIntensity (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrIntensity() const noexcept { return spectrogram3D.getSsrIntensity(); }

void MainComponent::setSpec3DSsrEdgeFade (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrEdgeFade (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrEdgeFade() const noexcept { return spectrogram3D.getSsrEdgeFade(); }

void MainComponent::setSpec3DSsrMetallicBias (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrMetallicBias (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrMetallicBias() const noexcept { return spectrogram3D.getSsrMetallicBias(); }

void MainComponent::setSpec3DSsrDomeFallback (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsrDomeFallback (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsrDomeFallback() const noexcept { return spectrogram3D.getSsrDomeFallback(); }

void MainComponent::setSpec3DEnergyConservingEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setEnergyConservingEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DEnergyConservingEnabled() const noexcept { return spectrogram3D.isEnergyConservingEnabled(); }

void MainComponent::setSpec3DTonemapEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setTonemapEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DTonemapEnabled() const noexcept { return spectrogram3D.isTonemapEnabled(); }

void MainComponent::setSpec3DTonemapExposureStops (float stops, bool notifyPrefs)
{
    spectrogram3D.setTonemapExposureStops (stops);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DTonemapExposureStops() const noexcept { return spectrogram3D.getTonemapExposureStops(); }

void MainComponent::setSpec3DColorGrade (Spectrogram3DComponent::ColorGrade grade, bool notifyPrefs)
{
    spectrogram3D.setColorGrade (grade);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ColorGrade MainComponent::getSpec3DColorGrade() const noexcept
{
    return spectrogram3D.getColorGrade();
}

void MainComponent::setSpec3DContactShadowEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setContactShadowEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DContactShadowEnabled() const noexcept { return spectrogram3D.isContactShadowEnabled(); }

void MainComponent::setSpec3DContactShadowStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setContactShadowStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DContactShadowStrength() const noexcept { return spectrogram3D.getContactShadowStrength(); }

void MainComponent::setSpec3DSelfShadowEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSelfShadowEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSelfShadowEnabled() const noexcept { return spectrogram3D.isSelfShadowEnabled(); }

void MainComponent::setSpec3DSelfShadowStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSelfShadowStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSelfShadowStrength() const noexcept { return spectrogram3D.getSelfShadowStrength(); }

void MainComponent::setSpec3DSelfShadowBias (float bias01, bool notifyPrefs)
{
    spectrogram3D.setSelfShadowBias (bias01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSelfShadowBias() const noexcept { return spectrogram3D.getSelfShadowBias(); }

void MainComponent::setSpec3DSelfShadowSoftness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSelfShadowSoftness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSelfShadowSoftness() const noexcept { return spectrogram3D.getSelfShadowSoftness(); }

void MainComponent::setSpec3DSelfShadowQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setSelfShadowQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DSelfShadowQuality() const noexcept
{
    return spectrogram3D.getSelfShadowQuality();
}

void MainComponent::setSpec3DCastShadowsEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setCastShadowsEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DCastShadowsEnabled() const noexcept { return spectrogram3D.isCastShadowsEnabled(); }

void MainComponent::setSpec3DShadowMapResolution (Spectrogram3DComponent::ShadowMapResolution res,
                                                  bool notifyPrefs)
{
    spectrogram3D.setShadowMapResolution (res);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowMapResolution MainComponent::getSpec3DShadowMapResolution() const noexcept
{
    return spectrogram3D.getShadowMapResolution();
}

void MainComponent::setSpec3DShadowCascadeCount (int count, bool notifyPrefs)
{
    spectrogram3D.setShadowCascadeCount (count);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
int MainComponent::getSpec3DShadowCascadeCount() const noexcept { return spectrogram3D.getShadowCascadeCount(); }

void MainComponent::setSpec3DShadowCascadeDistributionExponent (float exponent, bool notifyPrefs)
{
    spectrogram3D.setShadowCascadeDistributionExponent (exponent);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DShadowCascadeDistributionExponent() const noexcept
{
    return spectrogram3D.getShadowCascadeDistributionExponent();
}

void MainComponent::setSpec3DShadowCascadeTransitionFraction (float amount01, bool notifyPrefs)
{
    spectrogram3D.setShadowCascadeTransitionFraction (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DShadowCascadeTransitionFraction() const noexcept
{
    return spectrogram3D.getShadowCascadeTransitionFraction();
}

void MainComponent::setSpec3DDebugSphereEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setDebugSphereEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DDebugSphereEnabled() const noexcept { return spectrogram3D.isDebugSphereEnabled(); }

void MainComponent::setSpec3DDebugSphereDiameter (float metres, bool notifyPrefs)
{
    spectrogram3D.setDebugSphereDiameter (metres);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDebugSphereDiameter() const noexcept
{
    return spectrogram3D.getDebugSphereDiameter();
}

void MainComponent::setSpec3DDebugSpherePosition (juce::Vector3D<float> worldPos, bool notifyPrefs)
{
    spectrogram3D.setDebugSpherePosition (worldPos);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Vector3D<float> MainComponent::getSpec3DDebugSpherePosition() const noexcept
{
    return spectrogram3D.getDebugSpherePosition();
}

void MainComponent::setSpec3DDebugSphereAlbedo (juce::Colour c, bool notifyPrefs)
{
    spectrogram3D.setDebugSphereAlbedo (c);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Colour MainComponent::getSpec3DDebugSphereAlbedo() const noexcept
{
    return spectrogram3D.getDebugSphereAlbedo();
}

void MainComponent::setSpec3DDebugSphereRoughness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDebugSphereRoughness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDebugSphereRoughness() const noexcept
{
    return spectrogram3D.getDebugSphereRoughness();
}

void MainComponent::setSpec3DDebugSphereMetalness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDebugSphereMetalness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDebugSphereMetalness() const noexcept
{
    return spectrogram3D.getDebugSphereMetalness();
}

void MainComponent::setSpec3DDebugSphereSpecular (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDebugSphereSpecular (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDebugSphereSpecular() const noexcept
{
    return spectrogram3D.getDebugSphereSpecular();
}

void MainComponent::setSpec3DSsaoEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSsaoEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSsaoEnabled() const noexcept { return spectrogram3D.isSsaoEnabled(); }

void MainComponent::setSpec3DSsaoStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSsaoStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsaoStrength() const noexcept { return spectrogram3D.getSsaoStrength(); }

void MainComponent::setSpec3DSsaoRadius (float radius, bool notifyPrefs)
{
    spectrogram3D.setSsaoRadius (radius);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSsaoRadius() const noexcept { return spectrogram3D.getSsaoRadius(); }

void MainComponent::setSpec3DBloomEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setBloomEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DBloomEnabled() const noexcept { return spectrogram3D.isBloomEnabled(); }

void MainComponent::setSpec3DBloomStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setBloomStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DBloomStrength() const noexcept { return spectrogram3D.getBloomStrength(); }

void MainComponent::setSpec3DBloomThreshold (float amount01, bool notifyPrefs)
{
    spectrogram3D.setBloomThreshold (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DBloomThreshold() const noexcept { return spectrogram3D.getBloomThreshold(); }

void MainComponent::setSpec3DMotionBlurEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setMotionBlurEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DMotionBlurEnabled() const noexcept { return spectrogram3D.isMotionBlurEnabled(); }

void MainComponent::setSpec3DMotionBlurAmount (float amount01, bool notifyPrefs)
{
    spectrogram3D.setMotionBlurAmount (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DMotionBlurAmount() const noexcept { return spectrogram3D.getMotionBlurAmount(); }

void MainComponent::setSpec3DMotionBlurMax (float maxPixels, bool notifyPrefs)
{
    spectrogram3D.setMotionBlurMax (maxPixels);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DMotionBlurMax() const noexcept { return spectrogram3D.getMotionBlurMax(); }

void MainComponent::setSpec3DMotionBlurQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setMotionBlurQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DMotionBlurQuality() const noexcept
{
    return spectrogram3D.getMotionBlurQuality();
}

void MainComponent::setSpec3DDofEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setDofEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DDofEnabled() const noexcept { return spectrogram3D.isDofEnabled(); }

void MainComponent::setSpec3DDofFocusDistance (float distance, bool notifyPrefs)
{
    spectrogram3D.setDofFocusDistance (distance);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofFocusDistance() const noexcept { return spectrogram3D.getDofFocusDistance(); }

void MainComponent::setSpec3DDofFStop (float fStop, bool notifyPrefs)
{
    spectrogram3D.setDofFStop (fStop);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofFStop() const noexcept { return spectrogram3D.getDofFStop(); }

void MainComponent::setSpec3DDofFocalLengthMm (float mm, bool notifyPrefs)
{
    spectrogram3D.setDofFocalLengthMm (mm);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofFocalLengthMm() const noexcept
{
    return spectrogram3D.getDofFocalLengthMm();
}

void MainComponent::setSpec3DDofAperture (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDofAperture (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofAperture() const noexcept { return spectrogram3D.getDofAperture(); }

void MainComponent::setSpec3DDofAmount (float amount01, bool notifyPrefs)
{
    setSpec3DDofAperture (amount01, notifyPrefs);
}
float MainComponent::getSpec3DDofAmount() const noexcept { return getSpec3DDofAperture(); }

void MainComponent::setSpec3DDofQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setDofQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DDofQuality() const noexcept
{
    return spectrogram3D.getDofQuality();
}

void MainComponent::setSpec3DDofBlurScale (float scale, bool notifyPrefs)
{
    spectrogram3D.setDofBlurScale (scale);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofBlurScale() const noexcept { return spectrogram3D.getDofBlurScale(); }

void MainComponent::setSpec3DDofCocDilate (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDofCocDilate (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofCocDilate() const noexcept { return spectrogram3D.getDofCocDilate(); }

void MainComponent::setSpec3DDofEdgeSpill (float amount01, bool notifyPrefs)
{
    spectrogram3D.setDofEdgeSpill (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DDofEdgeSpill() const noexcept { return spectrogram3D.getDofEdgeSpill(); }

void MainComponent::setSpec3DSssEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setSssEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DSssEnabled() const noexcept
{
    return spectrogram3D.isSssEnabled();
}

void MainComponent::setSpec3DParticleModeEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setParticleModeEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DParticleModeEnabled() const noexcept
{
    return spectrogram3D.isParticleModeEnabled();
}
void MainComponent::setSpec3DParticleEmitMode (int mode, bool notifyPrefs)
{
    spectrogram3D.setParticleEmitMode (
        mode == 1 ? Spectrogram3DComponent::ParticleEmitMode::continuous
                  : Spectrogram3DComponent::ParticleEmitMode::slice);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
int MainComponent::getSpec3DParticleEmitMode() const noexcept
{
    return (int) spectrogram3D.getParticleEmitMode();
}
void MainComponent::setSpec3DParticleBindingMode (int mode, bool notifyPrefs)
{
    spectrogram3D.setParticleBindingMode (
        mode == 1 ? ParticleBindingMode::freeVisualizer
                  : ParticleBindingMode::spectrogramTrail);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
int MainComponent::getSpec3DParticleBindingMode() const noexcept
{
    return (int) spectrogram3D.getParticleBindingMode();
}
void MainComponent::setSpec3DParticleEmission (float amount, bool notifyPrefs)
{
    spectrogram3D.setParticleEmission (amount);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleEmission() const noexcept
{
    return spectrogram3D.getParticleEmission();
}
void MainComponent::setSpec3DParticleSpawnJitter (float amount, bool notifyPrefs)
{
    spectrogram3D.setParticleSpawnJitter (amount);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleSpawnJitter() const noexcept
{
    return spectrogram3D.getParticleSpawnJitter();
}
void MainComponent::setSpec3DParticleModSlot (int index, const ParticleModSlot& slot, bool notifyPrefs)
{
    spectrogram3D.setParticleModSlot (index, slot);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
ParticleModSlot MainComponent::getSpec3DParticleModSlot (int index) const noexcept
{
    return spectrogram3D.getParticleModSlot (index);
}
void MainComponent::setSpec3DParticleRandomSource (int index, const ParticleRandomSource& src, bool notifyPrefs)
{
    spectrogram3D.setParticleRandomSource (index, src);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
ParticleRandomSource MainComponent::getSpec3DParticleRandomSource (int index) const noexcept
{
    return spectrogram3D.getParticleRandomSource (index);
}
void MainComponent::setSpec3DParticleForcesEnabled (bool e, bool notifyPrefs)
{
    spectrogram3D.setParticleForcesEnabled (e);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DParticleForcesEnabled() const noexcept { return spectrogram3D.isParticleForcesEnabled(); }
void MainComponent::setSpec3DParticleWaterfallLock (bool e, bool notifyPrefs)
{
    spectrogram3D.setParticleWaterfallLock (e);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DParticleWaterfallLock() const noexcept { return spectrogram3D.isParticleWaterfallLock(); }
void MainComponent::setSpec3DParticleForceStack (const std::vector<ParticleForceModule>& stack, bool notifyPrefs)
{
    spectrogram3D.setParticleForceStack (stack);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
std::vector<ParticleForceModule> MainComponent::getSpec3DParticleForceStack() const
{
    std::vector<ParticleForceModule> out;
    const int n = spectrogram3D.getParticleForceModuleCount();
    out.reserve ((size_t) n);
    for (int i = 0; i < n; ++i)
        out.push_back (spectrogram3D.getParticleForceModule (i));
    return out;
}
void MainComponent::setSpec3DParticleMeshShape (int shape, bool notifyPrefs)
{
    spectrogram3D.setParticleMeshShape ((ParticleMeshShape) juce::jlimit (0, 2, shape));
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
int MainComponent::getSpec3DParticleMeshShape() const noexcept
{
    return (int) spectrogram3D.getParticleMeshShape();
}
void MainComponent::setSpec3DParticleInitRotX (float deg, bool notifyPrefs)
{
    spectrogram3D.setParticleInitRotX (deg);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleInitRotX() const noexcept { return spectrogram3D.getParticleInitRotX(); }
void MainComponent::setSpec3DParticleInitRotY (float deg, bool notifyPrefs)
{
    spectrogram3D.setParticleInitRotY (deg);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleInitRotY() const noexcept { return spectrogram3D.getParticleInitRotY(); }
void MainComponent::setSpec3DParticleInitRotZ (float deg, bool notifyPrefs)
{
    spectrogram3D.setParticleInitRotZ (deg);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleInitRotZ() const noexcept { return spectrogram3D.getParticleInitRotZ(); }
void MainComponent::setSpec3DParticleInitRotRandom (float amount, bool notifyPrefs)
{
    spectrogram3D.setParticleInitRotRandom (amount);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleInitRotRandom() const noexcept { return spectrogram3D.getParticleInitRotRandom(); }
void MainComponent::setSpec3DParticleInitVelX (float unitsPerSec, bool notifyPrefs)
{
    spectrogram3D.setParticleInitVelX (unitsPerSec);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
void MainComponent::setSpec3DParticleInitVelY (float unitsPerSec, bool notifyPrefs)
{
    spectrogram3D.setParticleInitVelY (unitsPerSec);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
void MainComponent::setSpec3DParticleInitVelZ (float unitsPerSec, bool notifyPrefs)
{
    spectrogram3D.setParticleInitVelZ (unitsPerSec);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleInitVelX() const noexcept { return spectrogram3D.getParticleInitVelX(); }
float MainComponent::getSpec3DParticleInitVelY() const noexcept { return spectrogram3D.getParticleInitVelY(); }
float MainComponent::getSpec3DParticleInitVelZ() const noexcept { return spectrogram3D.getParticleInitVelZ(); }
void MainComponent::setSpec3DParticleRiseSpeed (float unitsPerSec, bool notifyPrefs)
{
    setSpec3DParticleInitVelY (unitsPerSec, notifyPrefs);
}
float MainComponent::getSpec3DParticleRiseSpeed() const noexcept
{
    return getSpec3DParticleInitVelY();
}
void MainComponent::setSpec3DParticleVelRandom (float amount01, bool notifyPrefs)
{
    spectrogram3D.setParticleVelRandom (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleVelRandom() const noexcept
{
    return spectrogram3D.getParticleVelRandom();
}
void MainComponent::setSpec3DParticleLifespan (float seconds, bool notifyPrefs)
{
    spectrogram3D.setParticleLifespan (seconds);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleLifespan() const noexcept
{
    return spectrogram3D.getParticleLifespan();
}
void MainComponent::setSpec3DParticleLifespanRandom (float amount01, bool notifyPrefs)
{
    spectrogram3D.setParticleLifespanRandom (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleLifespanRandom() const noexcept
{
    return spectrogram3D.getParticleLifespanRandom();
}
void MainComponent::setSpec3DParticleSize (float worldSize, bool notifyPrefs)
{
    spectrogram3D.setParticleSize (worldSize);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleSize() const noexcept
{
    return spectrogram3D.getParticleSize();
}
void MainComponent::setSpec3DParticleEmissiveEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setParticleEmissiveEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DParticleEmissiveEnabled() const noexcept
{
    return spectrogram3D.isParticleEmissiveEnabled();
}
void MainComponent::setSpec3DParticleEmissiveStrength (float amount, bool notifyPrefs)
{
    spectrogram3D.setParticleEmissiveStrength (amount);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleEmissiveStrength() const noexcept
{
    return spectrogram3D.getParticleEmissiveStrength();
}
void MainComponent::setSpec3DParticleRoughness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setParticleRoughness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleRoughness() const noexcept
{
    return spectrogram3D.getParticleRoughness();
}
void MainComponent::setSpec3DParticleMetalness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setParticleMetalness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleMetalness() const noexcept
{
    return spectrogram3D.getParticleMetalness();
}
void MainComponent::setSpec3DParticleSpecular (float amount01, bool notifyPrefs)
{
    spectrogram3D.setParticleSpecular (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DParticleSpecular() const noexcept
{
    return spectrogram3D.getParticleSpecular();
}
void MainComponent::setSpec3DParticleGpuSimEnabled (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setParticleGpuSimEnabled (shouldEnable);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DParticleGpuSimEnabled() const noexcept
{
    return spectrogram3D.isParticleGpuSimEnabled();
}
bool MainComponent::isSpec3DParticleGpuSimAvailable() const noexcept
{
    return spectrogram3D.isParticleGpuSimAvailable();
}
void MainComponent::setSpec3DParticleMaxAlive (int maxAlive, bool notifyPrefs)
{
    spectrogram3D.setParticleMaxAlive (maxAlive);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
int MainComponent::getSpec3DParticleMaxAlive() const noexcept
{
    return spectrogram3D.getParticleMaxAlive();
}
int MainComponent::getSpec3DParticleAliveCount() const noexcept
{
    return spectrogram3D.getParticleAliveCount();
}
void MainComponent::clearSpec3DParticles (bool /*notifyPrefs*/)
{
    spectrogram3D.clearParticles();
}
void MainComponent::setSpec3DParticleDebugOverlayEnabled (bool shouldShow, bool notifyPrefs)
{
    spectrogram3D.setParticleDebugOverlayEnabled (shouldShow);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
bool MainComponent::isSpec3DParticleDebugOverlayEnabled() const noexcept
{
    return spectrogram3D.isParticleDebugOverlayEnabled();
}

void MainComponent::setSpec3DSssStrength (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssStrength (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssStrength() const noexcept { return spectrogram3D.getSssStrength(); }

void MainComponent::setSpec3DSssWrap (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssWrap (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssWrap() const noexcept { return spectrogram3D.getSssWrap(); }

void MainComponent::setSpec3DSssTransmission (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssTransmission (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssTransmission() const noexcept { return spectrogram3D.getSssTransmission(); }

void MainComponent::setSpec3DSssTint (juce::Colour c, bool notifyPrefs)
{
    spectrogram3D.setSssTint (c);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
juce::Colour MainComponent::getSpec3DSssTint() const noexcept { return spectrogram3D.getSssTint(); }

void MainComponent::setSpec3DSssRadius (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssRadius (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssRadius() const noexcept { return spectrogram3D.getSssRadius(); }

void MainComponent::setSpec3DSssContrast (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssContrast (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssContrast() const noexcept { return spectrogram3D.getSssContrast(); }

void MainComponent::setSpec3DSssQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs)
{
    spectrogram3D.setSssQuality (q);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
Spectrogram3DComponent::ShadowQuality MainComponent::getSpec3DSssQuality() const noexcept
{
    return spectrogram3D.getSssQuality();
}

void MainComponent::setSpec3DSssThicknessScale (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssThicknessScale (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssThicknessScale() const noexcept { return spectrogram3D.getSssThicknessScale(); }

void MainComponent::setSpec3DSssMaxThickness (float amount01, bool notifyPrefs)
{
    spectrogram3D.setSssMaxThickness (amount01);
    if (notifyPrefs) editor.requestSaveUiPrefs();
}
float MainComponent::getSpec3DSssMaxThickness() const noexcept { return spectrogram3D.getSssMaxThickness(); }

void MainComponent::placeSpectrogram3DPane (juce::Rectangle<int> view, juce::Rectangle<int> overlayTools,
                                            int toolH, int toolSize, int toolGap)
{
    juce::Rectangle<int> toolRow = overlayTools;
    auto placeArea = view;
    if (toolH > 0)
    {
        placeArea = view.withTrimmedBottom (toolH + 2);
        toolRow = juce::Rectangle<int> (view.getX() + 2,
                                        placeArea.getBottom() + 1,
                                        view.getWidth() - 4,
                                        toolH);
    }

    // Docked Scope uses Soft FBO compositing (nested GL HWND is unreliable under Direct2D).
    spectrogram3D.setChromeMode (Spectrogram3DComponent::ChromeMode::docked);
    spectrogram3D.setResizeLimits (placeArea.getWidth(), placeArea.getHeight());
    spectrogram3D.setBounds (placeArea);
    spectrogram3D.setVisible (true);

    // Keep a hidden 2D feeder under the docked frame for mesh history.
    const int pad = spectrogram3D.getShadowPad();
    if (! spectrogram.isVisible())
        spectrogram.setBounds (placeArea.reduced (pad));

    if (toolH > 0)
    {
        auto row = toolRow;
        auto placeOverlayTool = [&] (OscToolButton& b)
        {
            b.setVisible (true);
            b.setBounds (row.removeFromLeft (toolSize).withSizeKeepingCentre (toolSize, toolSize));
            row.removeFromLeft (toolGap);
            b.toFront (false);
        };
        placeOverlayTool (specSpeedUpButton);
        placeOverlayTool (specSpeedDownButton);
        placeOverlayTool (specExpandButton);
        // 2D/3D cube toggle is for expanded Spec only â€” Scope has separate modules.
    }
}

void MainComponent::setSpec3DDefaultCamera (const Spectrogram3DComponent::CameraState& state, bool) noexcept
{
    spectrogram3D.setDefaultCameraState (state);
}

Spectrogram3DComponent::CameraState MainComponent::getSpec3DDefaultCamera() const noexcept
{
    return spectrogram3D.getDefaultCameraState();
}

void MainComponent::applySpectrogramActive (bool shouldEnable)
{
    if (! shouldEnable)
        setSpecExpanded (false);

    specButton.setToggleState (shouldEnable, juce::dontSendNotification);
    spectrogram.setEnabled (shouldEnable);
    syncSpecToolButtons();
    resized();
}

void MainComponent::hostOptionBoxAboveExpandedOsc (bool shouldHost)
{
    auto* box = frequencyResponseComponent.getOptionBoxMenu();
    if (box == nullptr)
        return;

    if (shouldHost && box->isVisible())
    {
        if (box->getParentComponent() != this)
        {
            // Keep visual position while moving under MainComponent for z-order.
            const auto boundsInMain = getLocalArea (&frequencyResponseComponent, box->getBounds());
            addAndMakeVisible (box);
            box->setBounds (boundsInMain);
        }

        box->setAlwaysOnTop (true);
        return;
    }

    if (box->getParentComponent() == this)
    {
        const bool keepVisible = box->isVisible();
        const auto boundsInFrc = frequencyResponseComponent.getLocalArea (this, box->getBounds());
        box->setAlwaysOnTop (false);
        frequencyResponseComponent.addAndMakeVisible (box);
        box->setBounds (boundsInFrc);
        box->setVisible (keepVisible);
    }
    else
    {
        box->setAlwaysOnTop (false);
    }
}

void MainComponent::syncExpandedOscOverlayStack()
{
    // Expanded scope/gon/spec: above graph, below OptionBox + Settings menu.
    // Compact strip: stay in the always-on-top chrome layer.
    // Scope quad: never always-on-top â€” post meters must stay above the BR spectrogram.
    const bool oscExp = oscExpanded && oscButton.getToggleState();
    const bool gonExp = gonExpanded && gonButton.getToggleState();
    const bool specExp = specExpanded && specButton.getToggleState();
    const bool expanded = oscExp || gonExp || specExp;
    const bool compactChrome = ! scopeModeEnabled;
    const bool oscFramed = oscExp && ! oscFullGraph;
    const bool gonFramed = gonExp && ! gonFullGraph;
    const bool specFramed = specExp && ! specFullGraph && ! (spec3DEnabled && specButton.getToggleState());
    oscilloscope.setAlwaysOnTop (compactChrome && ! oscExp);
    goniometer.setAlwaysOnTop (compactChrome && ! gonExp);
    spectrogram.setAlwaysOnTop (compactChrome && ! specExp);
    oscFrame.setAlwaysOnTop (oscFramed);
    gonFrame.setAlwaysOnTop (gonFramed);
    specFrame.setAlwaysOnTop (specFramed);
    oscDimmer.setAlwaysOnTop (! expanded);

    auto* box = frequencyResponseComponent.getOptionBoxMenu();
    const bool optionOpen = box != nullptr && box->isVisible();

    // Whenever the OptionBox is open, host it here so it can sit above the expanded scope.
    hostOptionBoxAboveExpandedOsc (optionOpen);

    raiseMenuSystemAboveWordmark();

    if (menu.isVisible())
    {
        menuDismissCatcher.setBounds (getLocalBounds());
        menuDismissCatcher.setVisible (true);
        menuDismissCatcher.toFront (false);
        menuToggleButton.toFront (false);
        menu.toFront (false);
    }
}

void MainComponent::stylePresetNameEditor()
{
    const auto& c = sharedResources.sharedColors;
    presetNameEditor.setMultiLine (false);
    presetNameEditor.setReturnKeyStartsNewLine (false);
    presetNameEditor.setScrollbarsShown (false);
    presetNameEditor.setCaretVisible (true);
    presetNameEditor.setPopupMenuEnabled (true);
    presetNameEditor.setJustification (juce::Justification::centred);
    presetNameEditor.setFont (juce::FontOptions().withHeight (13.0f));
    presetNameEditor.setIndents (6, 2);
    presetNameEditor.setColour (juce::TextEditor::backgroundColourId, c.pluginPresetBackground.withAlpha (210.0f / 255.0f));
    presetNameEditor.setColour (juce::TextEditor::textColourId, c.pluginPresetText);
    presetNameEditor.setColour (juce::TextEditor::outlineColourId, c.pluginButtonAccent.withAlpha (200.0f / 255.0f));
    presetNameEditor.setColour (juce::TextEditor::focusedOutlineColourId, c.pluginButtonAccent.withAlpha (230.0f / 255.0f));
    presetNameEditor.setColour (juce::TextEditor::highlightColourId, c.pluginButtonAccent.withAlpha (160.0f / 255.0f));
    presetNameEditor.setColour (juce::CaretComponent::caretColourId, c.pluginButtonText);
}

void MainComponent::syncAbButtons()
{
    const auto active = processor.getActiveAbSlot();
    slotAButton.setToggleState (active == EqProcessor::AbSlot::A, juce::dontSendNotification);
    slotBButton.setToggleState (active == EqProcessor::AbSlot::B, juce::dontSendNotification);
    slotCButton.setToggleState (active == EqProcessor::AbSlot::C, juce::dontSendNotification);
    slotDButton.setToggleState (active == EqProcessor::AbSlot::D, juce::dontSendNotification);
}

void MainComponent::syncUndoRedoButtons()
{
    auto& um = processor.getUndoManager();
    undoButton.setEnabled (um.canUndo());
    redoButton.setEnabled (um.canRedo());
}

void MainComponent::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &processor.getUndoManager())
        syncUndoRedoButtons();

    if (source == &colourRamps)
    {
        applyColourRampsToMeters();
        persistSessionUiTheme();
    }
}

void MainComponent::beginRampSampling()
{
    if (! colourRamps.hasActiveTarget())
    {
        juce::AlertWindow::showMessageBoxAsync (
            juce::AlertWindow::InfoIcon,
            "Colour ramp",
            "Select FFT Bars, Spectrogram, or Spectrum Fill first (yellow highlight), then sample a path.");
        return;
    }

    rampSampleOverlay.beginSession (*this);
}

void MainComponent::beginRampSamplingForTarget (ColourRampBank::Target target)
{
    colourRamps.setActiveTarget (target);
    colourRamps.save();

    if (menu.isVisible())
        menu.setVisible (false);

    beginRampSampling();
}

void MainComponent::applyColourRampsToMeters()
{
    const auto* fftRamp = &colourRamps.get (ColourRampBank::Target::fftBars);
    const auto* specRamp = &colourRamps.get (ColourRampBank::Target::spectrogram);
    const auto* spec3DRamp = &colourRamps.get (ColourRampBank::Target::spectrogram3D);
    const auto* fillRamp = &colourRamps.get (ColourRampBank::Target::spectrumFill);
    const auto* oscRamp = &colourRamps.get (ColourRampBank::Target::oscilloscope);
    const auto* gonRamp = &colourRamps.get (ColourRampBank::Target::goniometer);
    const auto* stereoRamp = &colourRamps.get (ColourRampBank::Target::stereogram);
    const auto* histRamp = &colourRamps.get (ColourRampBank::Target::histogram);

    m_visualizer.setBinOverlayColourRamp (fftRamp->isUsable() ? fftRamp : nullptr);
    m_visualizer.setSpectrumFillRamp (fillRamp->isUsable() ? fillRamp : nullptr);
    spectrogram.setCustomColourRamp (specRamp->isUsable() ? specRamp : nullptr);
    spectrogram.setCustomColourRamp3D (spec3DRamp->isUsable() ? spec3DRamp : nullptr);
    spectrogram3D.recolourMesh();

    if (oscRamp->isUsable())
        oscilloscope.setColourRamp (*oscRamp);
    else
        oscilloscope.clearColourRamp();

    if (gonRamp->isUsable())
        goniometer.setColourRamp (*gonRamp);
    else
        goniometer.clearColourRamp();

    if (stereoRamp->isUsable())
        stereogram.setColourRamp (*stereoRamp);
    else
        stereogram.clearColourRamp();

    if (histRamp->isUsable())
        histogram.setColourRamp (*histRamp);
    else
        histogram.clearColourRamp();
}

void MainComponent::disableAllScopes()
{
    setOscExpanded (false);
    setGonExpanded (false);
    setSpecExpanded (false);

    oscButton.setToggleState (false, juce::dontSendNotification);
    oscilloscope.setEnabled (false);
    applyGoniometerActive (false);
    applySpectrogramActive (false);
    loudnessMeter.setEnabled (false);
    stereogram.setEnabled (false);
    histogram.setEnabled (false);
    syncOscToolButtons();
    syncGonToolButtons();
    syncSpecToolButtons();
}

void MainComponent::applyEcoMode (bool shouldEnable)
{
    processor.setEcoMode (shouldEnable);
    ecoEnabled = shouldEnable;
    ecoButton.setToggleState (shouldEnable, juce::dontSendNotification);

    if (shouldEnable)
    {
        if (scopeModeEnabled)
            setScopeMode (false, false);

        scopesBeforeEcoOsc = oscButton.getToggleState();
        scopesBeforeEcoGon = gonButton.getToggleState();
        scopesBeforeEcoSpec = specButton.getToggleState();
        disableAllScopes();
        oscButton.setEnabled (false);
        gonButton.setEnabled (false);
        specButton.setEnabled (false);
    }
    else
    {
        oscButton.setEnabled (true);
        gonButton.setEnabled (true);
        specButton.setEnabled (true);

        if (scopesBeforeEcoOsc)
        {
            oscButton.setToggleState (true, juce::dontSendNotification);
            oscilloscope.setEnabled (true);
        }
        if (scopesBeforeEcoGon)
            applyGoniometerActive (true);
        if (scopesBeforeEcoSpec)
            applySpectrogramActive (true);

        syncOscToolButtons();
        syncGonToolButtons();
        syncSpecToolButtons();
    }

    // Force Graph layers to refresh visibility immediately.
    m_visualizer.repaint();
    resized();
}

void MainComponent::setEcoMode (bool shouldEnable, bool notifyPrefs)
{
    if (shouldEnable == ecoEnabled)
    {
        ecoButton.setToggleState (ecoEnabled, juce::dontSendNotification);
        return;
    }

    // Eco gates analyser/FFT via processor + Analyser flags â€” SPECTRUM_ANALYSER_ID
    // preference is left untouched so turning Eco off restores the previous setting.
    applyEcoMode (shouldEnable);

    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::applyScopeMode (bool shouldEnable)
{
    scopeModeEnabled = shouldEnable;
    processor.setScopeMode (shouldEnable);

    if (shouldEnable)
    {
        if (ecoEnabled)
            setEcoMode (false, false);

        setOscExpanded (false);
        setGonExpanded (false);
        setSpecExpanded (false);
        scopeFullscreenModule.reset();

        oscButton.setToggleState (true, juce::dontSendNotification);
        oscilloscope.setEnabled (true);
        applyGoniometerActive (true);
        applySpectrogramActive (true);

        // Richer drawing in large panes (not the fullscreen overlay path).
        oscilloscope.setExpanded (true);
        goniometer.setExpanded (true);
        spectrogram.setExpanded (true);

        syncScopeModuleEnabledStates();

        frequencyResponseComponent.setOptionBoxVisible (false);
        // Scope mode never shows the EQ band graph (quad or strip).
        frequencyResponseComponent.setVisible (false);
        scopeSplitOverlay.setVisible (! scopeStripLayout);
        scopeArrangeOverlay.setVisible (true);
        arrangeButton.setToggleState (scopeStripLayout, juce::dontSendNotification);
        arrangeButton.setGlyph (scopeStripLayout ? OscToolButton::Glyph::StripLayout
                                                 : OscToolButton::Glyph::GridLayout);
    }
    else
    {
        scopeFullscreenModule.reset();
        oscilloscope.setExpanded (oscExpanded);
        goniometer.setExpanded (gonExpanded);
        spectrogram.setExpanded (specExpanded);
        loudnessMeter.setEnabled (false);
        stereogram.setEnabled (false);
        histogram.setEnabled (false);
        levelMeterIn.setVisible (false);
        levelMeterOut.setVisible (false);
        loudnessMeter.setVisible (false);
        stereogram.setVisible (false);
        histogram.setVisible (false);
        frequencyResponseComponent.setVisible (true);
        scopeSplitOverlay.setVisible (false);
        scopeArrangeOverlay.setVisible (false);
        arrangeButton.setVisible (false);
    }

    syncOscToolButtons();
    syncGonToolButtons();
    syncSpecToolButtons();
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::setScopeStripLayout (bool shouldUseStrip, bool notifyPrefs)
{
    if (scopeStripLayout == shouldUseStrip)
    {
        arrangeButton.setToggleState (scopeStripLayout, juce::dontSendNotification);
        arrangeButton.setGlyph (scopeStripLayout ? OscToolButton::Glyph::StripLayout
                                                 : OscToolButton::Glyph::GridLayout);
        return;
    }

    scopeStripLayout = shouldUseStrip;
    arrangeButton.setToggleState (scopeStripLayout, juce::dontSendNotification);
    arrangeButton.setGlyph (scopeStripLayout ? OscToolButton::Glyph::StripLayout
                                             : OscToolButton::Glyph::GridLayout);

    if (scopeModeEnabled)
    {
        frequencyResponseComponent.setVisible (false);
        scopeSplitOverlay.setVisible (! scopeStripLayout);
        editor.syncScopeModeLayout();
        resized();
    }

    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

ScopeLayoutPreset MainComponent::captureScopeLayoutPreset (const juce::String& name) const
{
    ScopeLayoutPreset p;
    p.name = name;
    p.strip = scopeStripLayout;
    p.modules = scopeEnabledOrder;
    p.stripFractions = scopeStripFractions;
    p.stripHeightPx = scopeStripHeightPx;
    p.splitX = scopeSplitOverlay.getSplitX();
    p.splitY = scopeSplitOverlay.getSplitY();
    // Inline colour snapshot only â€” never writes RampPresetStore / UI theme presets.
    p.colourRamps = colourRamps.toValueTree();
    return p;
}

void MainComponent::applyScopeLayoutPreset (const ScopeLayoutPreset& preset, bool notifyPrefs)
{
    scopeEnabledOrder = preset.modules.empty() ? ScopeModules::defaultEnabledOrder() : preset.modules;
    scopeStripFractions = preset.stripFractions;
    ensureScopeStripFractions();
    scopeStripHeightPx = juce::jmax (kScopeStripHeightMinPx, preset.stripHeightPx);
    scopeSplitOverlay.setSplitNorm (preset.splitX, preset.splitY);
    setScopeStripLayout (preset.strip, false);
    syncScopeModuleEnabledStates();

    if (preset.colourRamps.isValid() && preset.colourRamps.hasType ("ColourRamps"))
    {
        // Apply snapshot to live bank (session); do not create named ramp presets.
        colourRamps.applyFromValueTree (preset.colourRamps, false);
        colourRamps.notifyPreview();
        applyColourRampsToMeters();
    }

    editor.syncScopeModeLayout();
    resized();
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::setScopeStripHeightPx (int heightDesignPx, bool notifyPrefs)
{
    const int clamped = juce::jmax (kScopeStripHeightMinPx, heightDesignPx);
    if (scopeStripHeightPx == clamped)
        return;

    scopeStripHeightPx = clamped;

    if (scopeModeEnabled && scopeStripLayout)
    {
        editor.syncScopeModeLayout();
        resized();
    }

    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::syncStripHeightFromWindow (int windowW, int windowH) noexcept
{
    constexpr float designWidth = 1200.0f;
    const float scale = juce::jmax (0.001f, (float) juce::jmax (1, windowW) / designWidth);
    scopeStripHeightPx = juce::jmax (kScopeStripHeightMinPx,
                                     juce::roundToInt ((float) juce::jmax (1, windowH) / scale));
}

void MainComponent::ensureScopeStripFractions()
{
    const int n = (int) scopeEnabledOrder.size();
    if (n <= 0)
    {
        scopeStripFractions.clear();
        return;
    }

    if ((int) scopeStripFractions.size() != n)
    {
        scopeStripFractions.assign ((size_t) n, 1.0f / (float) n);
        return;
    }

    float sum = 0.0f;
    for (float f : scopeStripFractions)
        sum += f;
    if (sum < 0.001f)
    {
        scopeStripFractions.assign ((size_t) n, 1.0f / (float) n);
        return;
    }
    for (float& f : scopeStripFractions)
        f /= sum;
}

void MainComponent::setScopeStripColumnFraction (int leftSlot, float leftFrac)
{
    ensureScopeStripFractions();
    const int n = (int) scopeStripFractions.size();
    if (leftSlot < 0 || leftSlot + 1 >= n)
        return;

    const float pair = scopeStripFractions[(size_t) leftSlot] + scopeStripFractions[(size_t) leftSlot + 1];
    const float minF = juce::jmin (kScopeStripMinFrac, pair * 0.45f);
    const float maxF = pair - minF;
    leftFrac = juce::jlimit (minF, maxF, leftFrac);
    scopeStripFractions[(size_t) leftSlot] = leftFrac;
    scopeStripFractions[(size_t) leftSlot + 1] = pair - leftFrac;
    resized();
}

void MainComponent::setScopeEnabledOrder (const std::vector<ScopeModuleId>& order, bool notifyPrefs)
{
    scopeEnabledOrder = order.empty() ? ScopeModules::defaultEnabledOrder() : order;
    ensureScopeStripFractions();
    if (scopeModeEnabled)
    {
        syncScopeModuleEnabledStates();
        resized();
    }
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

bool MainComponent::isScopeModuleEnabled (ScopeModuleId id) const noexcept
{
    return std::find (scopeEnabledOrder.begin(), scopeEnabledOrder.end(), id) != scopeEnabledOrder.end();
}

std::optional<ModuleLookPresets::Kind> MainComponent::moduleLookKindForScope (ScopeModuleId id) noexcept
{
    using K = ModuleLookPresets::Kind;
    switch (id)
    {
        case ScopeModuleId::oscilloscope:  return K::oscilloscope;
        case ScopeModuleId::goniometer:    return K::goniometer;
        case ScopeModuleId::spectrogram:   return K::spectrogram;
        case ScopeModuleId::spectrogram3D: return K::spectrogram3D;
        case ScopeModuleId::spectrum:      return K::spectrum;
        case ScopeModuleId::histogram:     return K::histogram;
        case ScopeModuleId::stereogram:    return K::stereogram;
        case ScopeModuleId::loudness:      return K::loudness;
        case ScopeModuleId::levelIn:
        case ScopeModuleId::levelOut:      return K::levelMeters;
        default:                           return std::nullopt;
    }
}

namespace
{
    constexpr int kLookSaveDefault = 0;
    constexpr int kLookSavePreset  = 1;
    constexpr int kLookLoadDefault = 2;
    constexpr int kLookLoadBase    = 10;
    constexpr int kLookDeleteBase  = 200;

    int meshQToInt (Spectrogram3DComponent::MeshQuality q) noexcept
    {
        switch (q)
        {
            case Spectrogram3DComponent::MeshQuality::low: return 0;
            case Spectrogram3DComponent::MeshQuality::high: return 2;
            case Spectrogram3DComponent::MeshQuality::ultra: return 3;
            case Spectrogram3DComponent::MeshQuality::overkill: return 4;
            default: return 1;
        }
    }
    Spectrogram3DComponent::MeshQuality meshQFromInt (int v) noexcept
    {
        if (v <= 0) return Spectrogram3DComponent::MeshQuality::low;
        if (v == 2) return Spectrogram3DComponent::MeshQuality::high;
        if (v >= 3) return Spectrogram3DComponent::MeshQuality::ultra;
        return Spectrogram3DComponent::MeshQuality::medium;
    }
    int msaaToInt (Spectrogram3DComponent::MsaaLevel l) noexcept
    {
        switch (l)
        {
            case Spectrogram3DComponent::MsaaLevel::off: return 0;
            case Spectrogram3DComponent::MsaaLevel::x8: return 8;
            case Spectrogram3DComponent::MsaaLevel::x16: return 16;
            default: return 4;
        }
    }
    Spectrogram3DComponent::MsaaLevel msaaFromInt (int v) noexcept
    {
        if (v <= 0) return Spectrogram3DComponent::MsaaLevel::off;
        if (v >= 16) return Spectrogram3DComponent::MsaaLevel::x16;
        if (v >= 8) return Spectrogram3DComponent::MsaaLevel::x8;
        return Spectrogram3DComponent::MsaaLevel::x4;
    }
    int shQToInt (Spectrogram3DComponent::ShadowQuality q) noexcept
    {
        switch (q)
        {
            case Spectrogram3DComponent::ShadowQuality::low: return 0;
            case Spectrogram3DComponent::ShadowQuality::high: return 2;
            case Spectrogram3DComponent::ShadowQuality::ultra: return 3;
            default: return 1;
        }
    }
    Spectrogram3DComponent::ShadowQuality shQFromInt (int v) noexcept
    {
        if (v <= 0) return Spectrogram3DComponent::ShadowQuality::low;
        if (v >= 3) return Spectrogram3DComponent::ShadowQuality::ultra;
        if (v == 2) return Spectrogram3DComponent::ShadowQuality::high;
        return Spectrogram3DComponent::ShadowQuality::medium;
    }
}

juce::ValueTree MainComponent::captureModuleLook (ModuleLookPresets::Kind kind)
{
    auto root = ModuleLookPresets::captureFromApvts (kind, processor.treeState);
    if (kind != ModuleLookPresets::Kind::spectrogram3D)
        return root;

    juce::ValueTree look ("Spec3DLook");
    auto setB = [&] (const char* n, bool v) { look.setProperty (n, v, nullptr); };
    auto setF = [&] (const char* n, float v) { look.setProperty (n, (double) v, nullptr); };
    auto setI = [&] (const char* n, int v) { look.setProperty (n, v, nullptr); };
    auto setC = [&] (const char* n, juce::Colour c) { look.setProperty (n, (int) c.getARGB(), nullptr); };

    setI ("meshQuality", meshQToInt (getSpec3DMeshQuality()));
    setF ("freqMeshBias", getSpec3DFreqMeshBias());
    setF ("freqMeshBiasPivot", getSpec3DFreqMeshBiasPivot());
    setI ("msaaLevel", msaaToInt (getSpec3DMsaaLevel()));
    setB ("transparentBg", isSpec3DTransparentBackground());
    setB ("reverseFreq", isSpec3DReverseFrequencyAxis());
    setF ("meshHeight", getSpec3DMeshHeight());
    setB ("closedMesh", isSpec3DClosedMeshEnabled());
    setF ("normalCusp", getSpec3DNormalCuspAngleDeg());

    setB ("audioLevel", isSpec3DAudioLevelModEnabled());
    setI ("audioTarget", (int) getSpec3DAudioLevelTarget());
    setF ("audioMinPct", getSpec3DAudioLevelMinPercent());
    setF ("audioMaxPct", getSpec3DAudioLevelMaxPercent());
    setF ("audioHp", getSpec3DAudioLevelHpHz());
    setF ("audioLp", getSpec3DAudioLevelLpHz());
    setF ("audioThresh", getSpec3DAudioLevelThresholdDb());
    setI ("audioSpeed", (int) getSpec3DAudioLevelSpeed());
    setB ("audioPlayhead", getSpec3DAudioLevelAffectPlayhead());
    setB ("audioAntiPlayhead", getSpec3DAudioLevelAffectAntiPlayhead());

    setB ("lighting", isSpec3DLightingEnabled());
    setF ("lightingAmt", getSpec3DLightingAmount());
    setF ("lightAz", getSpec3DLightAzimuthDeg());
    setF ("lightEl", getSpec3DLightElevationDeg());
    setF ("specular", getSpec3DSpecularAmount());
    setF ("roughness", getSpec3DRoughnessAmount());
    setF ("metalness", getSpec3DMetalnessAmount());
    setF ("rim", getSpec3DRimAmount());
    setC ("lightCol", getSpec3DLightColour());
    setC ("rimCol", getSpec3DRimColour());
    setB ("dome", isSpec3DDomeFillEnabled());
    setF ("domeStr", getSpec3DDomeFillStrength());
    setC ("domeSky", getSpec3DDomeSkyColour());
    setC ("domeGround", getSpec3DDomeGroundColour());
    setB ("domeTex", isSpec3DDomeTextureEnabled());
    setI ("domeTexSrc", (int) getSpec3DDomeTextureSource());
    look.setProperty ("domeTexPath", getSpec3DDomeTextureCustomPath(), nullptr);

    setB ("ssgi", isSpec3DSsgiEnabled());
    setF ("ssgiStr", getSpec3DSsgiStrength());
    setF ("ssgiRad", getSpec3DSsgiRadius());
    setI ("ssgiQuality", shQToInt (getSpec3DSsgiQuality()));
    setB ("ssr", isSpec3DSsrEnabled());
    setF ("ssrStr", getSpec3DSsrStrength());
    setF ("ssrDist", getSpec3DSsrDistance());
    setF ("ssrThick", getSpec3DSsrThickness());
    setI ("ssrQuality", shQToInt (getSpec3DSsrQuality()));
    setF ("ssrFresnel", getSpec3DSsrFresnel());
    setF ("ssrRoughInf", getSpec3DSsrRoughnessInfluence());
    setF ("ssrIntensity", getSpec3DSsrIntensity());
    setF ("ssrEdgeFade", getSpec3DSsrEdgeFade());
    setF ("ssrMetalBias", getSpec3DSsrMetallicBias());
    setF ("ssrDomeFb", getSpec3DSsrDomeFallback());
    setB ("energyConserve", isSpec3DEnergyConservingEnabled());
    setB ("tonemap", isSpec3DTonemapEnabled());
    setF ("exposure", getSpec3DTonemapExposureStops());
    setI ("grade", (int) getSpec3DColorGrade());
    setB ("contactShadow", isSpec3DContactShadowEnabled());
    setF ("contactShadowStr", getSpec3DContactShadowStrength());
    setB ("selfShadow", isSpec3DSelfShadowEnabled());
    setF ("selfShadowStr", getSpec3DSelfShadowStrength());
    setF ("selfShadowBias", getSpec3DSelfShadowBias());
    setF ("selfShadowSoft", getSpec3DSelfShadowSoftness());
    setI ("selfShadowQuality", shQToInt (getSpec3DSelfShadowQuality()));
    setB ("castShadows", isSpec3DCastShadowsEnabled());
    setB ("ssao", isSpec3DSsaoEnabled());
    setF ("ssaoStr", getSpec3DSsaoStrength());
    setF ("ssaoRad", getSpec3DSsaoRadius());
    setB ("bloom", isSpec3DBloomEnabled());
    setF ("bloomStr", getSpec3DBloomStrength());
    setF ("bloomThr", getSpec3DBloomThreshold());
    setB ("dof", isSpec3DDofEnabled());
    setF ("dofFocus", getSpec3DDofFocusDistance());
    setF ("dofFStop", getSpec3DDofFStop());
    setF ("dofFocalMm", getSpec3DDofFocalLengthMm());
    setI ("dofQuality", shQToInt (getSpec3DDofQuality()));
    setF ("dofCocDilate", getSpec3DDofCocDilate());
    setF ("dofEdgeSpill", getSpec3DDofEdgeSpill());
    setB ("sss", isSpec3DSssEnabled());
    setF ("sssStr", getSpec3DSssStrength());
    setF ("sssWrap", getSpec3DSssWrap());
    setF ("sssTrans", getSpec3DSssTransmission());
    setC ("sssTint", getSpec3DSssTint());
    setF ("sssRad", getSpec3DSssRadius());
    setF ("sssContrast", getSpec3DSssContrast());
    setI ("sssQuality", shQToInt (getSpec3DSssQuality()));
    setF ("sssThickScale", getSpec3DSssThicknessScale());
    setF ("sssMaxThick", getSpec3DSssMaxThickness());
    setB ("autoRotate", isSpec3DAutoRotateEnabled());
    setF ("autoRotatePeriod", getSpec3DAutoRotatePeriodSec());
    setB ("zoomOsc", isSpec3DZoomOscillateEnabled());
    setF ("zoomOscDepth", getSpec3DZoomOscillateDepth());
    setF ("zoomOscPeriod", getSpec3DZoomOscillatePeriodSec());

    root.appendChild (std::move (look), nullptr);
    return root;
}

void MainComponent::applyModuleLook (ModuleLookPresets::Kind kind, const juce::ValueTree& look, bool notifyPrefs)
{
    if (! look.isValid())
        return;

    ModuleLookPresets::applyToApvts (kind, processor.treeState, look);

    if (kind == ModuleLookPresets::Kind::spectrogram3D)
    {
        auto s = look.getChildWithName ("Spec3DLook");
        if (! s.isValid() && look.hasType ("Spec3DLook"))
            s = look;
        if (s.isValid())
        {
            constexpr bool kSave = false;
            auto getB = [&] (const char* n, bool d) { return (bool) s.getProperty (n, d); };
            auto getF = [&] (const char* n, float d) { return (float) (double) s.getProperty (n, (double) d); };
            auto getI = [&] (const char* n, int d) { return (int) s.getProperty (n, d); };
            auto getC = [&] (const char* n, juce::Colour d)
            {
                return s.hasProperty (n) ? juce::Colour ((juce::uint32) (int) s.getProperty (n)) : d;
            };

            setSpec3DMeshQuality (meshQFromInt (getI ("meshQuality", 1)), kSave);
            setSpec3DFreqMeshBias (getF ("freqMeshBias", 0.0f), kSave);
            setSpec3DFreqMeshBiasPivot (getF ("freqMeshBiasPivot", 0.5f), kSave);
            setSpec3DMsaaLevel (msaaFromInt (getI ("msaaLevel", 4)), kSave);
            setSpec3DTransparentBackground (getB ("transparentBg", true), kSave);
            setSpec3DReverseFrequencyAxis (getB ("reverseFreq", true), kSave);
            setSpec3DMeshHeight (getF ("meshHeight", Spectrogram3DComponent::kDefaultMeshHeight), kSave);
            setSpec3DClosedMeshEnabled (getB ("closedMesh", false), kSave);
            setSpec3DNormalCuspAngleDeg (getF ("normalCusp", Spectrogram3DComponent::kNormalCuspDefaultDeg), kSave);

            setSpec3DAudioLevelModEnabled (getB ("audioLevel", false), kSave);
            setSpec3DAudioLevelTarget (static_cast<Spectrogram3DComponent::AudioLevelTarget> (getI ("audioTarget", 0)), kSave);
            setSpec3DAudioLevelMinPercent (getF ("audioMinPct", Spectrogram3DComponent::kAudioLevelMinPercentDefault), kSave);
            setSpec3DAudioLevelMaxPercent (getF ("audioMaxPct", Spectrogram3DComponent::kAudioLevelMaxPercentDefault), kSave);
            setSpec3DAudioLevelHpHz (getF ("audioHp", Spectrogram3DComponent::kAudioLevelHpDefaultHz), kSave);
            setSpec3DAudioLevelLpHz (getF ("audioLp", Spectrogram3DComponent::kAudioLevelLpDefaultHz), kSave);
            setSpec3DAudioLevelThresholdDb (getF ("audioThresh", Spectrogram3DComponent::kAudioLevelThresholdDefaultDb), kSave);
            setSpec3DAudioLevelSpeed (static_cast<Spectrogram3DComponent::AudioLevelSpeed> (getI ("audioSpeed", 0)), kSave);
            setSpec3DAudioLevelAffectPlayhead (getB ("audioPlayhead", false), kSave);
            setSpec3DAudioLevelAffectAntiPlayhead (getB ("audioAntiPlayhead", false), kSave);

            setSpec3DLightingEnabled (getB ("lighting", false), kSave);
            setSpec3DLightingAmount (getF ("lightingAmt", 0.70f), kSave);
            setSpec3DLightAzimuthDeg (getF ("lightAz", -40.0f), kSave);
            setSpec3DLightElevationDeg (getF ("lightEl", 55.0f), kSave);
            setSpec3DSpecularAmount (getF ("specular", 0.35f), kSave);
            setSpec3DRoughnessAmount (getF ("roughness", 0.45f), kSave);
            setSpec3DMetalnessAmount (getF ("metalness", 0.0f), kSave);
            setSpec3DRimAmount (getF ("rim", 0.22f), kSave);
            setSpec3DLightColour (getC ("lightCol", juce::Colours::white), kSave);
            setSpec3DRimColour (getC ("rimCol", juce::Colours::white), kSave);
            setSpec3DDomeFillEnabled (getB ("dome", false), kSave);
            setSpec3DDomeFillStrength (getF ("domeStr", 0.35f), kSave);
            setSpec3DDomeSkyColour (getC ("domeSky", juce::Colour (0xff7390bf)), kSave);
            setSpec3DDomeGroundColour (getC ("domeGround", juce::Colour (0xff403328)), kSave);
            setSpec3DDomeTextureCustomPath (s.getProperty ("domeTexPath").toString(), kSave);
            setSpec3DDomeTextureSource (getI ("domeTexSrc", 0) == 1
                                            ? Spectrogram3DComponent::DomeTextureSource::custom
                                            : Spectrogram3DComponent::DomeTextureSource::veniceSunset, kSave);
            setSpec3DDomeTextureEnabled (getB ("domeTex", false), kSave);

            setSpec3DSsgiEnabled (getB ("ssgi", false), kSave);
            setSpec3DSsgiStrength (getF ("ssgiStr", 0.40f), kSave);
            setSpec3DSsgiRadius (getF ("ssgiRad", 0.45f), kSave);
            setSpec3DSsgiQuality (shQFromInt (getI ("ssgiQuality", 1)), kSave);
            setSpec3DSsrEnabled (getB ("ssr", true), kSave);
            setSpec3DSsrStrength (getF ("ssrStr", 0.55f), kSave);
            setSpec3DSsrDistance (getF ("ssrDist", 0.55f), kSave);
            setSpec3DSsrThickness (getF ("ssrThick", 0.40f), kSave);
            setSpec3DSsrQuality (shQFromInt (getI ("ssrQuality", 1)), kSave);
            setSpec3DSsrFresnel (getF ("ssrFresnel", 0.75f), kSave);
            setSpec3DSsrRoughnessInfluence (getF ("ssrRoughInf", 0.85f), kSave);
            setSpec3DSsrIntensity (getF ("ssrIntensity", 1.0f), kSave);
            setSpec3DSsrEdgeFade (getF ("ssrEdgeFade", 0.15f), kSave);
            setSpec3DSsrMetallicBias (getF ("ssrMetalBias", 0.35f), kSave);
            setSpec3DSsrDomeFallback (getF ("ssrDomeFb", 0.65f), kSave);
            setSpec3DEnergyConservingEnabled (getB ("energyConserve", false), kSave);
            setSpec3DTonemapEnabled (getB ("tonemap", false), kSave);
            setSpec3DTonemapExposureStops (getF ("exposure", -0.3f), kSave);
            setSpec3DColorGrade (static_cast<Spectrogram3DComponent::ColorGrade> (juce::jlimit (0, 5, getI ("grade", 2))), kSave);
            setSpec3DContactShadowEnabled (getB ("contactShadow", false), kSave);
            setSpec3DContactShadowStrength (getF ("contactShadowStr", 0.45f), kSave);
            setSpec3DSelfShadowEnabled (getB ("selfShadow", false), kSave);
            setSpec3DSelfShadowStrength (getF ("selfShadowStr", 0.85f), kSave);
            setSpec3DSelfShadowBias (getF ("selfShadowBias", 0.35f), kSave);
            setSpec3DSelfShadowSoftness (getF ("selfShadowSoft", 0.85f), kSave);
            setSpec3DSelfShadowQuality (shQFromInt (getI ("selfShadowQuality", 1)), kSave);
            setSpec3DCastShadowsEnabled (getB ("castShadows", false), kSave);
            setSpec3DSsaoEnabled (getB ("ssao", false), kSave);
            setSpec3DSsaoStrength (getF ("ssaoStr", 0.55f), kSave);
            setSpec3DSsaoRadius (getF ("ssaoRad", 1.0f), kSave);
            setSpec3DBloomEnabled (getB ("bloom", false), kSave);
            setSpec3DBloomStrength (getF ("bloomStr", 0.45f), kSave);
            setSpec3DBloomThreshold (getF ("bloomThr", 0.62f), kSave);
            setSpec3DDofEnabled (getB ("dof", false), kSave);
            setSpec3DDofFocusDistance (getF ("dofFocus", Spectrogram3DComponent::kDofFocusDefault), kSave);
            setSpec3DDofFStop (getF ("dofFStop", Spectrogram3DComponent::kDofFStopDefault), kSave);
            setSpec3DDofFocalLengthMm (getF ("dofFocalMm", Spectrogram3DComponent::kDofFocalLengthDefaultMm), kSave);
            setSpec3DDofQuality (shQFromInt (getI ("dofQuality", 1)), kSave);
            setSpec3DDofCocDilate (getF ("dofCocDilate", Spectrogram3DComponent::kDofCocDilateDefault), kSave);
            setSpec3DDofEdgeSpill (getF ("dofEdgeSpill", Spectrogram3DComponent::kDofEdgeSpillDefault), kSave);
            setSpec3DSssEnabled (getB ("sss", false), kSave);
            setSpec3DSssStrength (getF ("sssStr", 0.45f), kSave);
            setSpec3DSssWrap (getF ("sssWrap", 0.55f), kSave);
            setSpec3DSssTransmission (getF ("sssTrans", 0.65f), kSave);
            setSpec3DSssTint (getC ("sssTint", juce::Colour (0xffe8b090)), kSave);
            setSpec3DSssRadius (getF ("sssRad", 0.40f), kSave);
            setSpec3DSssContrast (getF ("sssContrast", 0.50f), kSave);
            setSpec3DSssQuality (shQFromInt (getI ("sssQuality", 1)), kSave);
            setSpec3DSssThicknessScale (getF ("sssThickScale", 0.50f), kSave);
            setSpec3DSssMaxThickness (getF ("sssMaxThick", 0.70f), kSave);
            setSpec3DAutoRotateEnabled (getB ("autoRotate", false), kSave);
            setSpec3DAutoRotatePeriodSec (getF ("autoRotatePeriod", Spectrogram3DComponent::kAutoRotatePeriodDefaultSec), kSave);
            setSpec3DZoomOscillateEnabled (getB ("zoomOsc", false), kSave);
            setSpec3DZoomOscillateDepth (getF ("zoomOscDepth", Spectrogram3DComponent::kZoomOscillateDepthDefault), kSave);
            setSpec3DZoomOscillatePeriodSec (getF ("zoomOscPeriod", Spectrogram3DComponent::kZoomOscillatePeriodDefaultSec), kSave);

            menu.syncSpec3DSettingsFromMain();
            menu.notifyContentHeightChanged();
        }
    }

    if (notifyPrefs)
        requestUiPrefsSave();
}

bool MainComponent::saveModuleLookDefault (ModuleLookPresets::Kind kind)
{
    const auto look = captureModuleLook (kind);
    const bool ok = ModuleLookPresets::saveDefault (kind, look);
    AnalyserDefaults::mergeIdsFrom (processor.treeState, ModuleLookPresets::parameterIdsForKind (kind));
    return ok;
}

bool MainComponent::saveModuleLookNamed (ModuleLookPresets::Kind kind, const juce::String& name)
{
    return ModuleLookPresets::saveNamed (kind, name, captureModuleLook (kind));
}

void MainComponent::promptSaveModuleLookPreset (ModuleLookPresets::Kind kind)
{
    auto* aw = new juce::AlertWindow (
        "Save look preset",
        juce::String ("Name this ") + ModuleLookPresets::kindDisplayName (kind) + " look preset:",
        juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", juce::String (ModuleLookPresets::kindDisplayName (kind)) + " Look", "Name");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
    juce::Component::SafePointer<juce::AlertWindow> awSafe (aw);
    juce::Component::SafePointer<MainComponent> safe (this);
    aw->enterModalState (true,
        juce::ModalCallbackFunction::create (
            [safe, awSafe, kind] (int r)
            {
                if (safe == nullptr || r != 1 || awSafe == nullptr)
                    return;
                const auto name = awSafe->getTextEditorContents ("name").trim();
                if (name.isNotEmpty())
                    safe->saveModuleLookNamed (kind, name);
            }),
        true);
}

void MainComponent::appendModuleLookMenuItems (juce::PopupMenu& menu, ModuleLookPresets::Kind kind, int baseId)
{
    menu.addSectionHeader (juce::String (ModuleLookPresets::kindDisplayName (kind)) + " Look");
    menu.addItem (baseId + kLookSaveDefault, "Save current look as default");
    menu.addItem (baseId + kLookSavePreset, "Save current look as preset...");
    menu.addItem (baseId + kLookLoadDefault, "Load look default",
                  ModuleLookPresets::loadDefault (kind).isValid());

    const auto names = ModuleLookPresets::listNames (kind);
    juce::PopupMenu loadSub;
    if (names.empty())
        loadSub.addItem (-1, "(no saved presets)", false, false);
    else
        for (int i = 0; i < (int) names.size(); ++i)
            loadSub.addItem (baseId + kLookLoadBase + i, names[(size_t) i]);
    menu.addSubMenu ("Load look preset", loadSub);

    if (! names.empty())
    {
        juce::PopupMenu delSub;
        for (int i = 0; i < (int) names.size(); ++i)
            delSub.addItem (baseId + kLookDeleteBase + i, names[(size_t) i]);
        menu.addSubMenu ("Delete look preset", delSub);
    }
}

bool MainComponent::handleModuleLookMenuResult (ModuleLookPresets::Kind kind, int result, int baseId)
{
    if (result < baseId)
        return false;
    const int rel = result - baseId;
    if (rel == kLookSaveDefault)
    {
        saveModuleLookDefault (kind);
        return true;
    }
    if (rel == kLookSavePreset)
    {
        promptSaveModuleLookPreset (kind);
        return true;
    }
    if (rel == kLookLoadDefault)
    {
        auto def = ModuleLookPresets::loadDefault (kind);
        if (def.isValid())
            applyModuleLook (kind, def, true);
        return true;
    }
    if (rel >= kLookLoadBase && rel < kLookDeleteBase)
    {
        const auto names = ModuleLookPresets::listNames (kind);
        const int idx = rel - kLookLoadBase;
        if (idx >= 0 && idx < (int) names.size())
        {
            auto p = ModuleLookPresets::loadNamed (kind, names[(size_t) idx]);
            if (p.isValid())
                applyModuleLook (kind, p, true);
        }
        return true;
    }
    if (rel >= kLookDeleteBase)
    {
        const auto names = ModuleLookPresets::listNames (kind);
        const int idx = rel - kLookDeleteBase;
        if (idx >= 0 && idx < (int) names.size())
            ModuleLookPresets::deleteNamed (kind, names[(size_t) idx]);
        return true;
    }
    return false;
}

juce::ValueTree MainComponent::captureGlobalUiModules()
{
    juce::ValueTree root ("GlobalUi");

    auto ramps = colourRamps.toValueTree();
    if (ramps.isValid())
        root.appendChild (std::move (ramps), nullptr);

    {
        auto layout = captureScopeLayoutPreset ("_global");
        juce::ValueTree layoutTree ("ScopeLayout");
        layoutTree.setProperty ("strip", layout.strip, nullptr);
        layoutTree.setProperty ("modules", ScopeModules::orderToString (layout.modules), nullptr);
        layoutTree.setProperty ("fractions", ScopeLayoutPresets::encodeFractions (layout.stripFractions), nullptr);
        layoutTree.setProperty ("stripHeightPx", layout.stripHeightPx, nullptr);
        layoutTree.setProperty ("splitX", (double) layout.splitX, nullptr);
        layoutTree.setProperty ("splitY", (double) layout.splitY, nullptr);
        root.appendChild (std::move (layoutTree), nullptr);
    }

    {
        juce::ValueTree looks ("ModuleLooks");
        for (int i = 0; i < (int) ModuleLookPresets::Kind::numKinds; ++i)
        {
            auto look = captureModuleLook (static_cast<ModuleLookPresets::Kind> (i));
            if (look.isValid())
                looks.appendChild (std::move (look), nullptr);
        }
        if (looks.getNumChildren() > 0)
            root.appendChild (std::move (looks), nullptr);
    }

    return root;
}

void MainComponent::applyGlobalUiModules (const juce::ValueTree& globalUi)
{
    if (! globalUi.isValid() || ! globalUi.hasType ("GlobalUi"))
        return;

    if (auto ramps = globalUi.getChildWithName ("ColourRamps"); ramps.isValid())
    {
        colourRamps.applyFromValueTree (ramps, false);
        colourRamps.notifyPreview();
        applyColourRampsToMeters();
    }

    if (auto layoutTree = globalUi.getChildWithName ("ScopeLayout"); layoutTree.isValid())
    {
        ScopeLayoutPreset layout;
        layout.name = "_global";
        layout.strip = (bool) layoutTree.getProperty ("strip", false);
        layout.modules = ScopeModules::orderFromString (
            layoutTree.getProperty ("modules",
                                    ScopeModules::orderToString (ScopeModules::defaultEnabledOrder())).toString());
        layout.stripHeightPx = (int) layoutTree.getProperty ("stripHeightPx", 200);
        layout.splitX = (float) (double) layoutTree.getProperty ("splitX", 0.5);
        layout.splitY = (float) (double) layoutTree.getProperty ("splitY", 0.5);
        layout.stripFractions = ScopeLayoutPresets::decodeFractions (
            layoutTree.getProperty ("fractions").toString(), (int) layout.modules.size());
        applyScopeLayoutPreset (layout, false);
    }

    if (auto looks = globalUi.getChildWithName ("ModuleLooks"); looks.isValid())
    {
        for (int i = 0; i < looks.getNumChildren(); ++i)
        {
            auto look = looks.getChild (i);
            if (! look.isValid() || ! look.hasType ("ModuleLook"))
                continue;
            const auto kindName = look.getProperty ("kind").toString();
            for (int k = 0; k < (int) ModuleLookPresets::Kind::numKinds; ++k)
            {
                const auto kind = static_cast<ModuleLookPresets::Kind> (k);
                if (kindName == ModuleLookPresets::kindFolder (kind))
                {
                    applyModuleLook (kind, look, false);
                    break;
                }
            }
        }
        requestUiPrefsSave();
    }

    persistSessionUiTheme();
}

void MainComponent::showScopeModuleContextMenu (ScopeModuleId id, juce::Component* anchor)
{
    if (anchor == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    const int removeId = 900;
    const int resetIntegId = 10;
    const int tapInId = 20;
    const int tapOutId = 21;
    const int oscRedrawId = 30;
    const int lookBaseId = 1000;
    bool hasExtras = false;

    const auto lookKind = moduleLookKindForScope (id);
    if (lookKind.has_value())
    {
        appendModuleLookMenuItems (menu, *lookKind, lookBaseId);
        hasExtras = true;
    }

    switch (id)
    {
        case ScopeModuleId::loudness:
        case ScopeModuleId::histogram:
            if (hasExtras)
                menu.addSeparator();
            menu.addItem (resetIntegId, "Reset Integrated");
            hasExtras = true;
            break;

        case ScopeModuleId::levelIn:
        case ScopeModuleId::levelOut:
        {
            if (hasExtras)
                menu.addSeparator();
            auto& meter = (id == ScopeModuleId::levelOut) ? levelMeterOut : levelMeterIn;
            menu.addSectionHeader ("Level Meter Tap");
            menu.addItem (tapInId, "Input", true, meter.getTap() == ScopeLevelMeterModule::Tap::input);
            menu.addItem (tapOutId, "Output", true, meter.getTap() == ScopeLevelMeterModule::Tap::output);
            hasExtras = true;
            break;
        }

        case ScopeModuleId::oscilloscope:
            if (hasExtras)
                menu.addSeparator();
            menu.addItem (oscRedrawId, "Redraw in place", true, ! oscilloscope.isScrollMode());
            hasExtras = true;
            break;

        default:
            break;
    }

    if (scopeModeEnabled)
    {
        if (hasExtras)
            menu.addSeparator();
        menu.addItem (removeId, "Remove Module", scopeEnabledOrder.size() > 1);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
                        [safe = juce::Component::SafePointer<MainComponent> (this), id, lookKind,
                         removeId, resetIntegId, tapInId, tapOutId, oscRedrawId, lookBaseId] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            if (lookKind.has_value()
                                && safe->handleModuleLookMenuResult (*lookKind, result, lookBaseId))
                                return;

                            if (result == removeId)
                            {
                                if (safe->scopeModeEnabled)
                                    safe->setScopeModuleEnabled (id, false, true);
                                return;
                            }

                            if (result == resetIntegId)
                            {
                                if (id == ScopeModuleId::histogram)
                                    safe->histogram.resetIntegrated();
                                else
                                    safe->loudnessMeter.resetIntegrated();
                                return;
                            }

                            if (result == tapInId || result == tapOutId)
                            {
                                auto& meter = (id == ScopeModuleId::levelOut) ? safe->levelMeterOut
                                                                              : safe->levelMeterIn;
                                meter.setTap (result == tapOutId ? ScopeLevelMeterModule::Tap::output
                                                                 : ScopeLevelMeterModule::Tap::input);
                                return;
                            }

                            if (result == oscRedrawId)
                                safe->oscilloscope.setScrollMode (! safe->oscilloscope.isScrollMode());
                        });
}

void MainComponent::setScopeModuleEnabled (ScopeModuleId id, bool enabled, bool notifyPrefs)
{
    auto order = scopeEnabledOrder;
    const auto it = std::find (order.begin(), order.end(), id);

    if (enabled)
    {
        if (it != order.end())
            return;

        const auto defaults = ScopeModules::defaultEnabledOrder();
        const auto defIt = std::find (defaults.begin(), defaults.end(), id);
        const int defIdx = defIt != defaults.end() ? (int) (defIt - defaults.begin()) : (int) defaults.size();

        int insertAt = (int) order.size();
        for (int i = 0; i < (int) order.size(); ++i)
        {
            const auto otherDefIt = std::find (defaults.begin(), defaults.end(), order[(size_t) i]);
            const int otherDefIdx = otherDefIt != defaults.end() ? (int) (otherDefIt - defaults.begin()) : (int) defaults.size();
            if (otherDefIdx > defIdx)
            {
                insertAt = i;
                break;
            }
        }

        order.insert (order.begin() + insertAt, id);
    }
    else
    {
        if (it == order.end())
            return;
        order.erase (it);
    }

    setScopeEnabledOrder (order, notifyPrefs);
}

void MainComponent::syncScopeModuleEnabledStates()
{
    if (! scopeModeEnabled)
        return;

    const auto enabled = [this] (ScopeModuleId id) { return isScopeModuleEnabled (id); };

    oscilloscope.setEnabled (enabled (ScopeModuleId::oscilloscope));
    applyGoniometerActive (enabled (ScopeModuleId::goniometer));
    applySpectrogramActive (enabled (ScopeModuleId::spectrogram)
                            || enabled (ScopeModuleId::spectrogram3D));
    loudnessMeter.setEnabled (enabled (ScopeModuleId::loudness));
    stereogram.setEnabled (enabled (ScopeModuleId::stereogram));
    histogram.setEnabled (enabled (ScopeModuleId::histogram));
}

void MainComponent::applyScopePaneReorder (int fromSlot, int toSlot, bool insertBefore)
{
    const int n = (int) scopeEnabledOrder.size();
    if (fromSlot < 0 || fromSlot >= n || toSlot < 0 || toSlot > n)
        return;

    ensureScopeStripFractions();

    if (! scopeStripLayout || ! insertBefore)
    {
        if (toSlot >= n || fromSlot == toSlot)
            return;
        std::swap (scopeEnabledOrder[(size_t) fromSlot], scopeEnabledOrder[(size_t) toSlot]);
        if ((int) scopeStripFractions.size() == n)
            std::swap (scopeStripFractions[(size_t) fromSlot], scopeStripFractions[(size_t) toSlot]);
    }
    else
    {
        int insertAt = juce::jlimit (0, n, toSlot);
        if (insertAt == fromSlot || insertAt == fromSlot + 1)
            return;

        const auto id = scopeEnabledOrder[(size_t) fromSlot];
        auto order = scopeEnabledOrder;
        auto fracs = scopeStripFractions;
        const float movedFrac = (fromSlot < (int) fracs.size()) ? fracs[(size_t) fromSlot] : (1.0f / (float) n);
        order.erase (order.begin() + fromSlot);
        if (fromSlot < (int) fracs.size())
            fracs.erase (fracs.begin() + fromSlot);
        if (insertAt > fromSlot)
            --insertAt;
        insertAt = juce::jlimit (0, (int) order.size(), insertAt);
        order.insert (order.begin() + insertAt, id);
        if ((int) fracs.size() == (int) order.size() - 1)
            fracs.insert (fracs.begin() + insertAt, movedFrac);
        scopeEnabledOrder = std::move (order);
        scopeStripFractions = std::move (fracs);
        ensureScopeStripFractions();
    }

    editor.requestSaveUiPrefs();
    resized();
}

void MainComponent::setScopeMode (bool shouldEnable, bool notifyPrefs)
{
    if (shouldEnable == scopeModeEnabled)
    {
        editor.syncScopeModeButton();
        return;
    }

    applyScopeMode (shouldEnable);
    editor.syncScopeModeButton();
    editor.syncScopeModeLayout();

    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::setScopeTapPost (bool shouldTapPost, bool notifyPrefs)
{
    if (shouldTapPost == scopeTapPost)
    {
        editor.syncScopeModeButton();
        return;
    }

    scopeTapPost = shouldTapPost;
    processor.setScopeTapPost (shouldTapPost);
    editor.syncScopeModeButton();
    editor.syncScopeModeLayout();
    resized();

    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

void MainComponent::setDisableGlowShadowEffects (bool shouldDisable, bool notifyPrefs)
{
    if (sharedResources.disableGlowShadowEffects == shouldDisable)
        return;

    sharedResources.disableGlowShadowEffects = shouldDisable;
    menuToggleButton.repaint();
    m_visualizer.repaint();
    oscilloscope.repaint();
    goniometer.repaint();
    frequencyResponseComponent.repaint();

    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

bool MainComponent::areGlowShadowEffectsDisabled() const noexcept
{
    return sharedResources.disableGlowShadowEffects;
}

void MainComponent::showAbMenu (EqProcessor::AbSlot slot)
{
    const auto slotName = EqProcessor::abSlotName (slot);
    AbSlotButton* target = &slotAButton;
    switch (slot)
    {
        case EqProcessor::AbSlot::A: target = &slotAButton; break;
        case EqProcessor::AbSlot::B: target = &slotBButton; break;
        case EqProcessor::AbSlot::C: target = &slotCButton; break;
        case EqProcessor::AbSlot::D: target = &slotDButton; break;
    }

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Save current settings to snapshot " + slotName);

    juce::PopupMenu copyTo;
    for (int i = 0; i < EqProcessor::abSlotCount; ++i)
    {
        const auto dest = static_cast<EqProcessor::AbSlot> (i);
        if (dest == slot)
            continue;
        copyTo.addItem (10 + i, "Snapshot " + EqProcessor::abSlotName (dest));
    }
    menu.addSubMenu ("Copy settings from snapshot " + slotName + " toâ€¦", copyTo);

    juce::PopupMenu swapWith;
    for (int i = 0; i < EqProcessor::abSlotCount; ++i)
    {
        const auto other = static_cast<EqProcessor::AbSlot> (i);
        if (other == slot)
            continue;
        swapWith.addItem (20 + i, "Snapshot " + EqProcessor::abSlotName (other));
    }
    menu.addSubMenu ("Swap withâ€¦", swapWith);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                        [this, slot] (int result)
                        {
                            if (result <= 0)
                                return;

                            processor.getUndoManager().beginNewTransaction ("A/B/C/D snapshot");

                            if (result == 1)
                            {
                                processor.saveCurrentToAbSlot (slot);
                            }
                            else if (result >= 10 && result < 20)
                            {
                                processor.copyAbSlot (slot, static_cast<EqProcessor::AbSlot> (result - 10));
                            }
                            else if (result >= 20 && result < 30)
                            {
                                processor.swapAbSlots (slot, static_cast<EqProcessor::AbSlot> (result - 20));
                            }
                            else
                            {
                                return;
                            }

                            syncAbButtons();
                        });
}

void MainComponent::refreshPresetNameDisplay()
{
    if (presetNameEditor.hasKeyboardFocus (true))
        return;

    juce::String name = "Default";
    if (eqPresets != nullptr)
    {
        const auto selected = eqPresets->getSelectedName();
        name = selected.isNotEmpty() ? selected : "Default";
    }

    refreshingPresetName = true;
    presetNameEditor.setText (name, juce::dontSendNotification);
    refreshingPresetName = false;
}

void MainComponent::commitPresetNameEdit()
{
    if (refreshingPresetName || eqPresets == nullptr)
        return;

    const auto typed = presetNameEditor.getText().trim();
    if (typed.isEmpty())
    {
        refreshPresetNameDisplay();
        return;
    }

    // Rename current user EQ preset; for Default the typed name is pending for Save.
    if (eqPresets->getSelectedIndex() > 0)
        eqPresets->renameSelected (typed);
}

void MainComponent::textEditorReturnKeyPressed (juce::TextEditor& editor)
{
    if (&editor == &presetNameEditor)
    {
        commitPresetNameEdit();
        presetNameEditor.giveAwayKeyboardFocus();
    }
}

void MainComponent::textEditorFocusLost (juce::TextEditor& editor)
{
    if (&editor == &presetNameEditor)
        commitPresetNameEdit();
}

void MainComponent::showUiThemePopupMenu()
{
    auto* themes = menu.getThemeList();
    if (themes == nullptr)
        return;

    juce::Component::SafePointer<MainComponent> safeThis (this);
    // Filled after launchAsynchronously â€” used to close the dropdown on apply/save/rename.
    auto activeCallout = std::make_shared<juce::Component::SafePointer<juce::CallOutBox>>();

    auto dismissUiCallout = [activeCallout]
    {
        if (*activeCallout != nullptr)
            (*activeCallout)->dismiss();
    };

    auto renameTheme = [safeThis, dismissUiCallout] (int index)
    {
        dismissUiCallout();
        if (safeThis == nullptr || index <= 0)
            return;

        auto* list = safeThis->menu.getThemeList();
        if (list == nullptr)
            return;

        const auto currentName = list->getPresetName (index);
        auto* aw = new juce::AlertWindow ("Rename Theme",
                                          "New name for this UI theme:",
                                          juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", currentName.isNotEmpty() ? currentName : "UI Theme", "Name");
        aw->addButton ("Rename", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [safeThis, aw, index] (int r)
            {
                if (r == 1 && safeThis != nullptr)
                {
                    if (auto* themes = safeThis->menu.getThemeList())
                    {
                        auto name = aw->getTextEditorContents ("name").trim();
                        if (name.isNotEmpty())
                            themes->renamePreset (index, name);
                    }
                }
                delete aw;
            }));
    };

    auto duplicateTheme = [safeThis, dismissUiCallout] (int index)
    {
        dismissUiCallout();
        if (safeThis == nullptr)
            return;
        if (auto* list = safeThis->menu.getThemeList())
            list->duplicatePreset (index);
    };

    auto deleteTheme = [safeThis, dismissUiCallout] (int index)
    {
        dismissUiCallout();
        if (safeThis == nullptr)
            return;
        if (auto* list = safeThis->menu.getThemeList())
            list->deletePreset (index);
    };

    auto applyTheme = [safeThis, dismissUiCallout] (int index)
    {
        dismissUiCallout();
        if (safeThis == nullptr)
            return;
        if (auto* list = safeThis->menu.getThemeList())
            list->applyPreset (index, false);
    };

    auto saveTheme = [safeThis, dismissUiCallout]
    {
        dismissUiCallout();
        if (safeThis == nullptr)
            return;

        auto* aw = new juce::AlertWindow ("Save UI Preset",
                                          "Name for the current UI colours:",
                                          juce::AlertWindow::NoIcon);
        aw->addTextEditor ("name", "UI Theme", "Name");
        aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true, juce::ModalCallbackFunction::create (
            [safeThis, aw] (int r)
            {
                if (r == 1 && safeThis != nullptr)
                {
                    if (auto* themes = safeThis->menu.getThemeList())
                    {
                        auto name = aw->getTextEditorContents ("name").trim();
                        if (name.isEmpty())
                            name = "UI Theme";
                        themes->saveOrUpdateWithName (name);
                    }
                }
                delete aw;
            }));
    };

    auto toggleGlow = [safeThis]
    {
        if (safeThis == nullptr)
            return;

        safeThis->sharedResources.disableGlowShadowEffects = ! safeThis->sharedResources.disableGlowShadowEffects;
        safeThis->editor.requestSaveUiPrefs();
        safeThis->applyThemeToChildComponents();
        safeThis->m_visualizer.repaint();
        safeThis->oscilloscope.repaint();
        safeThis->goniometer.repaint();
        safeThis->frequencyResponseComponent.repaint();
    };

    auto samplePath = [safeThis, dismissUiCallout]
    {
        dismissUiCallout();
        if (safeThis != nullptr)
            safeThis->beginRampSampling();
    };

    auto content = std::make_unique<UiThemeDropdownContent> (
        sharedResources.disableGlowShadowEffects,
        *themes,
        sharedResources,
        colourRamps,
        saveTheme,
        toggleGlow,
        applyTheme,
        renameTheme,
        duplicateTheme,
        deleteTheme,
        samplePath);

    auto& box = juce::CallOutBox::launchAsynchronously (std::move (content),
                                                        uiThemeButton.getScreenBounds(),
                                                        nullptr);
    *activeCallout = &box;
    box.setDismissalMouseClicksAreAlwaysConsumed (true);
}

void MainComponent::randomizeUiTheme()
{
    sharedResources.makeActive();
    sharedResources.sharedColors.randomizeColors();
    // Accessibility: only after colours are randomized (never inside randomizeColors).
    if (sharedResources.sharedColors.enforceLegibleText)
    {
        sharedResources.sharedColors.enforceLegibleTextContrast();
        if (sharedResources.sharedColors.randomizeFaceplateMod)
            sharedResources.sharedColors.syncFaceplateModScheme();
    }
    applyThemeToChildComponents();

    if (auto* appearance = menu.getAppearanceComponent())
    {
        // refreshAfterRandomize updates Menu Slider Fill + glow (updateAllComponents alone does not).
        appearance->refreshAfterRandomize();
    }

    persistSessionUiTheme();
}

void MainComponent::setOrderedRampGradation (bool shouldEnable, bool notifyPrefs)
{
    if (sharedResources.sharedColors.orderedRampGradation == shouldEnable)
        return;
    sharedResources.sharedColors.orderedRampGradation = shouldEnable;
    if (notifyPrefs)
        editor.requestSaveUiPrefs();
}

bool MainComponent::isOrderedRampGradation() const noexcept
{
    return sharedResources.sharedColors.orderedRampGradation;
}

void MainComponent::randomizeColourRamps()
{
    sharedResources.makeActive();
    const auto& c = sharedResources.sharedColors;
    bool mask[(int) ColourRampBank::Target::numTargets] {};
    mask[(int) ColourRampBank::Target::fftBars] = c.randomizeRampFftBars;
    mask[(int) ColourRampBank::Target::spectrogram] = c.randomizeRampSpectrogram;
    mask[(int) ColourRampBank::Target::spectrogram3D] = c.randomizeRampSpectrogram3D;
    mask[(int) ColourRampBank::Target::spectrumFill] = c.randomizeRampSpectrumFill;
    colourRamps.randomizeRamps (c, mask, (int) ColourRampBank::Target::numTargets);
    applyColourRampsToMeters();
    persistSessionUiTheme();
}

void MainComponent::disableCustomColourRamps()
{
    colourRamps.disableAllCustomRamps();
    applyColourRampsToMeters();
    persistSessionUiTheme();
}

void MainComponent::persistSessionUiTheme()
{
    processor.storeSessionUiTheme (sharedResources.sharedColors, colourRamps.toValueTree());
    // Disk write is what actually survives Ableton (ui_prefs dice flags already use this folder).
    editor.saveLastUiThemeToDisk();
    colourRamps.save();
    editor.requestSaveUiPrefs();
}

void MainComponent::restoreSessionUiThemeIfAny()
{
    juce::ValueTree rampTree;
    if (! processor.tryRestoreSessionUiTheme (sharedResources.sharedColors, rampTree))
    {
        // No host UI yet — load last-used palette from disk (same reliability as dice prefs).
        // Pass our SharedResources: editor.mainComponent is still null during our construction.
        if (! editor.loadLastUiThemeFromDisk (&sharedResources))
            return;
        // Ramps already come from colour_ramps.xml via ColourRampBank::load().
    }
    else if (rampTree.isValid())
    {
        colourRamps.applyFromValueTree (rampTree, false);
    }

    sharedResources.makeActive();

    // Menu/Appearance were constructed before restore; refresh cached colour widgets.
    if (auto* appearance = menu.getAppearanceComponent())
        appearance->refreshAfterRandomize();
}

void MainComponent::reapplySessionUiThemeFromProcessor()
{
    restoreSessionUiThemeIfAny();
    applyThemeToChildComponents();
    if (auto* appearance = menu.getAppearanceComponent())
        appearance->refreshAfterRandomize();
}

void MainComponent::runDiceRandomize()
{
    const auto& c = sharedResources.sharedColors;
    const bool anyUi = c.randomizeFaceplateMod || c.randomizeGraphModule || c.randomizeMenuModule;
    const bool anyRamp = c.randomizeRampFftBars || c.randomizeRampSpectrogram
                         || c.randomizeRampSpectrogram3D || c.randomizeRampSpectrumFill;

    if (anyUi)
        randomizeUiTheme();
    if (anyRamp)
        randomizeColourRamps();
}

void MainComponent::showRandomizeDiceMenu()
{
    auto& scopes = sharedResources.sharedColors;
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    menu.addSectionHeader ("UI colours");
    menu.addItem (1, "Faceplate/Mod", true, scopes.randomizeFaceplateMod);
    menu.addItem (2, "Graph", true, scopes.randomizeGraphModule);
    menu.addItem (3, "Menu", true, scopes.randomizeMenuModule);
    menu.addSeparator();
    menu.addSectionHeader ("Colour ramps");
    menu.addItem (4, "FFT Bars", true, scopes.randomizeRampFftBars);
    menu.addItem (5, "Spectrogram (2D)", true, scopes.randomizeRampSpectrogram);
    menu.addItem (6, "Spectrogram 3D", true, scopes.randomizeRampSpectrogram3D);
    menu.addItem (7, "Spectrum Fill", true, scopes.randomizeRampSpectrumFill);
    menu.addSeparator();
    menu.addSectionHeader ("Ramp randomize mode");
    menu.addItem (9, "Ordered gradation", true, scopes.orderedRampGradation);
    menu.addItem (10, "Standard (independent stops)", true, ! scopes.orderedRampGradation);
    menu.addSeparator();
    menu.addItem (8, "Disable custom ramps (use schemes)");

    // JUCE PopupMenu always dismisses on click — re-open after toggles so multi-select
    // scopes stay convenient (checkmarks update, dismiss with click-away / Esc).
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&uiRandomizeButton),
                        [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            auto& s = safe->sharedResources.sharedColors;
                            bool keepOpen = false;

                            if (result == 1)      { s.randomizeFaceplateMod = ! s.randomizeFaceplateMod; keepOpen = true; }
                            else if (result == 2) { s.randomizeGraphModule = ! s.randomizeGraphModule; keepOpen = true; }
                            else if (result == 3) { s.randomizeMenuModule = ! s.randomizeMenuModule; keepOpen = true; }
                            else if (result == 4) { s.randomizeRampFftBars = ! s.randomizeRampFftBars; keepOpen = true; }
                            else if (result == 5) { s.randomizeRampSpectrogram = ! s.randomizeRampSpectrogram; keepOpen = true; }
                            else if (result == 6) { s.randomizeRampSpectrogram3D = ! s.randomizeRampSpectrogram3D; keepOpen = true; }
                            else if (result == 7) { s.randomizeRampSpectrumFill = ! s.randomizeRampSpectrumFill; keepOpen = true; }
                            else if (result == 8) safe->disableCustomColourRamps();
                            else if (result == 9)  { safe->setOrderedRampGradation (true, true); keepOpen = true; }
                            else if (result == 10) { safe->setOrderedRampGradation (false, true); keepOpen = true; }

                            if (result >= 1 && result <= 7)
                                safe->editor.requestSaveUiPrefs();

                            if (keepOpen)
                            {
                                juce::MessageManager::callAsync ([safe]
                                {
                                    if (safe != nullptr)
                                        safe->showRandomizeDiceMenu();
                                });
                            }
                        });
}

void MainComponent::showPresetPopupMenu()
{
    if (eqPresets == nullptr || eqPresets->getNumPresets() <= 0)
        return;

    juce::PopupMenu popup;
    popup.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const int selected = eqPresets->getSelectedIndex();

    for (int i = 0; i < eqPresets->getNumPresets(); ++i)
    {
        const auto name = eqPresets->getName (i);
        popup.addItem (i + 1, name.isNotEmpty() ? name : ("Preset " + juce::String (i + 1)),
                       true, i == selected);
    }

    popup.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetMenuButton),
                         [this] (int result)
                         {
                             if (result <= 0 || eqPresets == nullptr)
                                 return;
                             eqPresets->apply (result - 1);
                         });
}

void MainComponent::cycleEqPreset (int delta)
{
    if (eqPresets != nullptr)
        eqPresets->cycle (delta);
}

void MainComponent::saveCurrentEqPreset()
{
    if (eqPresets == nullptr)
        return;

    eqPresets->saveOrUpdateWithName (presetNameEditor.getText());
    refreshPresetNameDisplay();
}

void MainComponent::onPresetApplied (const Theme&)
{
    applyThemeToChildComponents();
    persistSessionUiTheme();
}

void MainComponent::onPresetListChanged()
{
    // Appearance UI theme list changed â€” chrome EQ name is independent.
}

void MainComponent::ScopeSplitOverlay::setSplitNorm (float xNorm, float yNorm) noexcept
{
    splitX = juce::jlimit (0.18f, 0.82f, xNorm);
    splitY = juce::jlimit (0.18f, 0.82f, yNorm);
}

int MainComponent::ScopeSplitOverlay::splitXPx() const noexcept
{
    return juce::roundToInt ((float) getWidth() * splitX);
}

int MainComponent::ScopeSplitOverlay::splitYPx() const noexcept
{
    return juce::roundToInt ((float) getHeight() * splitY);
}

MainComponent::ScopeSplitOverlay::Drag MainComponent::ScopeSplitOverlay::hitZone (juce::Point<int> p) const noexcept
{
    const int sx = splitXPx();
    const int sy = splitYPx();
    const bool nearV = std::abs (p.x - sx) <= kHitPad;
    const bool nearH = std::abs (p.y - sy) <= kHitPad;
    if (nearV && nearH) return Drag::both;
    if (nearV) return Drag::vertical;
    if (nearH) return Drag::horizontal;
    return Drag::none;
}

bool MainComponent::ScopeSplitOverlay::hitTest (int x, int y)
{
    return hitZone ({ x, y }) != Drag::none;
}

void MainComponent::ScopeSplitOverlay::paint (juce::Graphics& g)
{
    const float sx = (float) splitXPx();
    const float sy = (float) splitYPx();
    g.setColour (juce::Colour::fromRGBA (180, 150, 55, 160));
    g.drawLine (sx, 0.0f, sx, (float) getHeight(), 1.5f);
    g.drawLine (0.0f, sy, (float) getWidth(), sy, 1.5f);

    auto hub = juce::Rectangle<float> (sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
    g.setColour (juce::Colour::fromRGBA (220, 200, 120, 200));
    g.fillEllipse (hub);
}

void MainComponent::ScopeSplitOverlay::mouseDown (const juce::MouseEvent& e)
{
    drag = hitZone (e.getPosition());
    if (drag != Drag::none)
        setMouseCursor (drag == Drag::vertical ? juce::MouseCursor::LeftRightResizeCursor
                      : drag == Drag::horizontal ? juce::MouseCursor::UpDownResizeCursor
                                                 : juce::MouseCursor::UpDownLeftRightResizeCursor);
}

void MainComponent::ScopeSplitOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (drag == Drag::none || getWidth() <= 0 || getHeight() <= 0)
        return;

    if (drag == Drag::vertical || drag == Drag::both)
        splitX = juce::jlimit (0.18f, 0.82f, (float) e.x / (float) getWidth());
    if (drag == Drag::horizontal || drag == Drag::both)
        splitY = juce::jlimit (0.18f, 0.82f, (float) e.y / (float) getHeight());

    main.resized();
}

void MainComponent::ScopeSplitOverlay::mouseUp (const juce::MouseEvent&)
{
    drag = Drag::none;
    setMouseCursor (juce::MouseCursor::NormalCursor);
    main.editor.requestSaveUiPrefs();
}

void MainComponent::setScopeSplitNorm (float xNorm, float yNorm) noexcept
{
    scopeSplitOverlay.setSplitNorm (xNorm, yNorm);
}

void MainComponent::setScopeStripFractions (const std::vector<float>& fracs)
{
    scopeStripFractions = fracs;
    ensureScopeStripFractions();
}

int MainComponent::ScopeArrangeOverlay::hitDragHandle (juce::Point<int> p) const noexcept
{
    for (int i = 0; i < (int) slotBounds.size(); ++i)
    {
        const auto& b = slotBounds[(size_t) i];
        if (b.isEmpty())
            continue;
        auto handle = b.withHeight (kHandleH);
        if (handle.contains (p))
            return i;
    }
    return -1;
}

bool MainComponent::ScopeArrangeOverlay::hitResizeEdge (juce::Point<int> p) const noexcept
{
    if (! main.scopeStripLayout || stripBounds.isEmpty())
        return false;

    if (p.x < stripBounds.getX() || p.x > stripBounds.getRight())
        return false;

    return std::abs (p.y - stripBounds.getBottom()) <= kResizeHitPad;
}

int MainComponent::ScopeArrangeOverlay::hitColumnDivider (juce::Point<int> p) const noexcept
{
    if (! main.scopeStripLayout || slotBounds.size() < 2)
        return -1;

    for (int i = 0; i < (int) slotBounds.size() - 1; ++i)
    {
        const auto& a = slotBounds[(size_t) i];
        const auto& b = slotBounds[(size_t) i + 1];
        if (a.isEmpty() || b.isEmpty())
            continue;
        const int seamX = (a.getRight() + b.getX()) / 2;
        if (std::abs (p.x - seamX) <= kResizeHitPad
            && p.y >= juce::jmin (a.getY(), b.getY())
            && p.y <= juce::jmax (a.getBottom(), b.getBottom()))
            return i;
    }
    return -1;
}

int MainComponent::ScopeArrangeOverlay::hitPaneEdgeForHover (juce::Point<int> p) const noexcept
{
    for (int i = 0; i < (int) slotBounds.size(); ++i)
    {
        const auto& b = slotBounds[(size_t) i];
        if (b.isEmpty())
            continue;

        const bool nearL = std::abs (p.x - b.getX()) <= kResizeHitPad;
        const bool nearR = std::abs (p.x - b.getRight()) <= kResizeHitPad;
        const bool nearT = std::abs (p.y - b.getY()) <= kResizeHitPad;
        const bool nearB = std::abs (p.y - b.getBottom()) <= kResizeHitPad;
        const bool inY = p.y >= b.getY() - kResizeHitPad && p.y <= b.getBottom() + kResizeHitPad;
        const bool inX = p.x >= b.getX() - kResizeHitPad && p.x <= b.getRight() + kResizeHitPad;

        if ((nearL || nearR) && inY)
            return i;
        if ((nearT || nearB) && inX)
            return i;
    }
    return -1;
}

void MainComponent::ScopeArrangeOverlay::updateHoverCursor (juce::Point<int> p)
{
    auto setHoverVisuals = [this] (bool stripBottom, int colDiv, int paneOutline)
    {
        bool dirty = false;
        if (hoverResize != stripBottom) { hoverResize = stripBottom; dirty = true; }
        if (hoverColumnDivider != colDiv) { hoverColumnDivider = colDiv; dirty = true; }
        if (hoverPaneOutline != paneOutline) { hoverPaneOutline = paneOutline; dirty = true; }
        if (dirty)
            repaint();
    };

    if (resizingStrip)
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        return;
    }

    if (resizingColumn >= 0)
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        return;
    }

    if (dragFromSlot >= 0)
    {
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        return;
    }

    if (hitResizeEdge (p))
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        setHoverVisuals (true, -1, -1);
        return;
    }

    const int colDiv = hitColumnDivider (p);
    if (colDiv >= 0)
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        setHoverVisuals (false, colDiv, -1);
        return;
    }

    const int paneEdge = hitPaneEdgeForHover (p);
    if (paneEdge >= 0)
    {
        setMouseCursor (hitDragHandle (p) >= 0 ? juce::MouseCursor::DraggingHandCursor
                                               : juce::MouseCursor::NormalCursor);
        setHoverVisuals (false, -1, paneEdge);
        return;
    }

    setHoverVisuals (false, -1, -1);
    setMouseCursor (hitDragHandle (p) >= 0 ? juce::MouseCursor::DraggingHandCursor
                                           : juce::MouseCursor::NormalCursor);
}

void MainComponent::ScopeArrangeOverlay::updateDropTarget (juce::Point<int> p) noexcept
{
    dropSlot = -1;
    dropInsertBefore = false;

    if (! main.scopeStripLayout)
    {
        for (int i = 0; i < (int) slotBounds.size(); ++i)
        {
            if (i == dragFromSlot)
                continue;
            if (slotBounds[(size_t) i].contains (p))
            {
                dropSlot = i;
                return;
            }
        }
        return;
    }

    // Strip: insertion index 0â€¦N from pointer X (midpoints between pane centres).
    dropInsertBefore = true;
    dropSlot = (int) slotBounds.size();
    for (int i = 0; i < (int) slotBounds.size(); ++i)
    {
        const auto& b = slotBounds[(size_t) i];
        if (b.isEmpty())
            continue;
        if (p.x < b.getCentreX())
        {
            dropSlot = i;
            break;
        }
    }
}

bool MainComponent::ScopeArrangeOverlay::hitTest (int x, int y)
{
    if (dragFromSlot >= 0 || resizingStrip || resizingColumn >= 0)
        return true;
    const juce::Point<int> p { x, y };
    return hitResizeEdge (p) || hitColumnDivider (p) >= 0 || hitDragHandle (p) >= 0
           || hitPaneEdgeForHover (p) >= 0;
}

void MainComponent::ScopeArrangeOverlay::mouseMove (const juce::MouseEvent& e)
{
    updateHoverCursor (e.getPosition());
}

void MainComponent::ScopeArrangeOverlay::mouseExit (const juce::MouseEvent&)
{
    if (dragFromSlot < 0 && ! resizingStrip && resizingColumn < 0)
    {
        if (hoverResize || hoverColumnDivider >= 0 || hoverPaneOutline >= 0)
        {
            hoverResize = false;
            hoverColumnDivider = -1;
            hoverPaneOutline = -1;
            repaint();
        }
        setMouseCursor (juce::MouseCursor::NormalCursor);
    }
}

void MainComponent::ScopeArrangeOverlay::mouseDown (const juce::MouseEvent& e)
{
    dropSlot = -1;
    dropInsertBefore = false;
    dragPos = e.getPosition();
    resizingStrip = false;
    resizingColumn = -1;
    dragFromSlot = -1;
    dragGrabOffset = {};

    if (e.mods.isPopupMenu())
    {
        for (int i = 0; i < (int) slotBounds.size(); ++i)
        {
            if (slotBounds[(size_t) i].contains (e.getPosition())
                && i < (int) main.scopeEnabledOrder.size())
            {
                main.showScopeModuleContextMenu (main.scopeEnabledOrder[(size_t) i], &main);
                return;
            }
        }
    }

    if (hitResizeEdge (e.getPosition()))
    {
        resizingStrip = true;
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        repaint();
        return;
    }

    resizingColumn = hitColumnDivider (e.getPosition());
    if (resizingColumn >= 0)
    {
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
        repaint();
        return;
    }

    dragFromSlot = hitDragHandle (e.getPosition());
    if (dragFromSlot >= 0)
    {
        const auto& b = slotBounds[(size_t) dragFromSlot];
        dragGrabOffset = { (float) e.x - (float) b.getX(), (float) e.y - (float) b.getY() };
        setMouseCursor (juce::MouseCursor::DraggingHandCursor);
        repaint();
    }
}

void MainComponent::ScopeArrangeOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (resizingStrip)
    {
        constexpr float designWidth = 1200.0f;
        const float scale = juce::jmax (0.001f, (float) main.getWidth() / designWidth);
        const int stripTop = stripBounds.getY();
        // Dragging the strip bottom edge resizes the editor (strip fills the window).
        const int screenH = juce::jmax (juce::roundToInt ((float) MainComponent::kScopeStripHeightMinPx * scale),
                                        e.y - stripTop);
        const int designH = juce::jmax (MainComponent::kScopeStripHeightMinPx,
                                        juce::roundToInt ((float) screenH / scale));
        main.setScopeStripHeightPx (designH, false);
        return;
    }

    if (resizingColumn >= 0 && ! stripBounds.isEmpty())
    {
        main.ensureScopeStripFractions();
        const int n = (int) main.scopeStripFractions.size();
        if (resizingColumn + 1 < n)
        {
            const float relX = (float) (e.x - stripBounds.getX()) / (float) juce::jmax (1, stripBounds.getWidth());
            float leftEdge = 0.0f;
            for (int i = 0; i < resizingColumn; ++i)
                leftEdge += main.scopeStripFractions[(size_t) i];
            main.setScopeStripColumnFraction (resizingColumn, relX - leftEdge);
        }
        return;
    }

    if (dragFromSlot < 0)
        return;
    dragPos = e.getPosition();
    updateDropTarget (dragPos);
    repaint();
}

void MainComponent::ScopeArrangeOverlay::mouseUp (const juce::MouseEvent& e)
{
    if (resizingStrip)
    {
        resizingStrip = false;
        main.editor.requestSaveUiPrefs();
        updateHoverCursor (e.getPosition());
        repaint();
        return;
    }

    if (resizingColumn >= 0)
    {
        resizingColumn = -1;
        main.editor.requestSaveUiPrefs();
        updateHoverCursor (e.getPosition());
        repaint();
        return;
    }

    if (dragFromSlot >= 0)
    {
        updateDropTarget (e.getPosition());
        if (dropSlot >= 0 && dropSlot != dragFromSlot)
            main.applyScopePaneReorder (dragFromSlot, dropSlot, dropInsertBefore);
    }

    dragFromSlot = -1;
    dropSlot = -1;
    dropInsertBefore = false;
    dragGrabOffset = {};
    updateHoverCursor (e.getPosition());
    repaint();
}

void MainComponent::ScopeArrangeOverlay::paint (juce::Graphics& g)
{
    const auto outline = main.sharedResources.sharedColors.scopeDropOutline;

    // Module titles: fixed padding from the window/pane top (not a % of pane height).
    constexpr int kTitlePadTop = 6;
    constexpr int kTitleH = 14;
    for (int i = 0; i < (int) slotBounds.size() && i < (int) main.scopeEnabledOrder.size(); ++i)
    {
        const auto r = slotBounds[(size_t) i];
        if (r.isEmpty())
            continue;

        // Strip: pin to window top. Tiled: fixed pad inside each pane.
        const int titleY = main.scopeStripLayout ? kTitlePadTop : (r.getY() + kTitlePadTop);
        auto titleArea = juce::Rectangle<int> (r.getX() + 4, titleY, r.getWidth() - 8, kTitleH);
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.8f));
        g.setFont (juce::Font (juce::FontOptions (11.0f)));
        g.drawText (ScopeModules::idToLabel (main.scopeEnabledOrder[(size_t) i]).toUpperCase(),
                    titleArea, juce::Justification::centred, false);
    }

    // Hover / active edges â€” same outline colour in tiled and strip.
    if (main.scopeStripLayout && ! stripBounds.isEmpty() && (hoverResize || resizingStrip))
    {
        const float y = (float) stripBounds.getBottom();
        g.setColour (outline.withAlpha (resizingStrip ? 0.90f : 0.65f));
        g.drawLine ((float) stripBounds.getX(), y, (float) stripBounds.getRight(), y, 1.5f);
    }

    const int activeCol = resizingColumn >= 0 ? resizingColumn : hoverColumnDivider;
    if (main.scopeStripLayout && activeCol >= 0 && activeCol + 1 < (int) slotBounds.size())
    {
        const auto& a = slotBounds[(size_t) activeCol];
        const float x = (float) a.getRight();
        g.setColour (outline.withAlpha (resizingColumn >= 0 ? 0.95f : 0.70f));
        g.drawLine (x, (float) a.getY(), x, (float) a.getBottom(), 2.0f);
    }

    if (hoverPaneOutline >= 0 && hoverPaneOutline < (int) slotBounds.size()
        && dragFromSlot < 0)
    {
        auto r = slotBounds[(size_t) hoverPaneOutline].toFloat().reduced (0.5f);
        g.setColour (outline.withAlpha (0.75f));
        g.drawRoundedRectangle (r, 3.0f, 1.75f);
    }

    if (dragFromSlot < 0)
        return;

    // Detached ghost keeps the grab point (corner/handle), not recentered on the mouse.
    if (dragFromSlot >= 0 && dragFromSlot < (int) slotBounds.size())
    {
        auto ghost = slotBounds[(size_t) dragFromSlot].toFloat();
        ghost.setPosition ((float) dragPos.x - dragGrabOffset.x,
                           (float) dragPos.y - dragGrabOffset.y);
        g.setColour (outline.withAlpha (0.20f));
        g.fillRoundedRectangle (ghost, 4.0f);
        g.setColour (outline.withAlpha (0.85f));
        g.drawRoundedRectangle (ghost, 4.0f, 2.0f);
    }

    if (dropSlot < 0)
        return;

    if (main.scopeStripLayout && dropInsertBefore)
    {
        // Insertion caret before dropSlot (or after last when dropSlot == N).
        float x = 0.0f;
        float y = 0.0f;
        float h = 0.0f;
        if (dropSlot >= (int) slotBounds.size())
        {
            const auto& b = slotBounds.back();
            x = (float) b.getRight();
            y = (float) b.getY();
            h = (float) b.getHeight();
        }
        else
        {
            const auto& b = slotBounds[(size_t) dropSlot];
            x = (float) b.getX();
            y = (float) b.getY();
            h = (float) b.getHeight();
        }
        g.setColour (outline);
        g.fillRect (x - 2.0f, y, 4.0f, h);
    }
    else if (dropSlot < (int) slotBounds.size())
    {
        auto r = slotBounds[(size_t) dropSlot].toFloat().reduced (1.0f);
        g.setColour (outline.withAlpha (0.35f));
        g.fillRoundedRectangle (r, 4.0f);
        g.setColour (outline);
        g.drawRoundedRectangle (r, 4.0f, 2.5f);
    }
}

void MainComponent::hideAllScopePanes()
{
    goniometer.setVisible (false);
    m_visualizer.setVisible (false);
    oscilloscope.setVisible (false);
    spectrogram.setVisible (false);
    levelMeterIn.setVisible (false);
    levelMeterOut.setVisible (false);
    loudnessMeter.setVisible (false);
    stereogram.setVisible (false);
    histogram.setVisible (false);
    gonExpandButton.setVisible (false);
    oscZoomInButton.setVisible (false);
    oscZoomOutButton.setVisible (false);
    oscChannelModeButton.setVisible (false);
    oscExpandButton.setVisible (false);
    specSpeedUpButton.setVisible (false);
    specSpeedDownButton.setVisible (false);
    specExpandButton.setVisible (false);
    spec3DButton.setVisible (false);
    spectrogram3D.setVisible (false);
}

void MainComponent::setScopeFullscreenModule (std::optional<ScopeModuleId> id)
{
    if (scopeFullscreenModule == id)
        return;
    scopeFullscreenModule = id;
    resized();
    grabKeyboardFocus();
}

void MainComponent::toggleScopePaneFullscreen (ScopeModuleId id)
{
    if (! scopeModeEnabled)
        return;
    if (scopeFullscreenModule.has_value() && *scopeFullscreenModule == id)
        setScopeFullscreenModule (std::nullopt);
    else
        setScopeFullscreenModule (id);
}

void MainComponent::placeScopePane (ScopeModuleId moduleId, juce::Rectangle<int> pane,
                                    int toolH, int toolSize, int toolGap)
{
    // Tool buttons overlay the pane bottom â€” module keeps the full tile height.
    const auto overlayTools = (toolH > 0)
                                  ? juce::Rectangle<int> (pane.getX() + 2,
                                                          pane.getBottom() - toolH - 1,
                                                          pane.getWidth() - 4,
                                                          toolH)
                                  : juce::Rectangle<int>{};

    auto placeOverlayTool = [&] (OscToolButton& b, juce::Rectangle<int>& row)
    {
        b.setVisible (true);
        b.setBounds (row.removeFromLeft (toolSize).withSizeKeepingCentre (toolSize, toolSize));
        row.removeFromLeft (toolGap);
        b.toFront (false);
    };

    switch (moduleId)
    {
        case ScopeModuleId::levelIn:
            levelMeterIn.setTiledPresentation (! scopeStripLayout);
            levelMeterIn.setBounds (pane);
            levelMeterIn.setVisible (true);
            break;

        case ScopeModuleId::levelOut:
            levelMeterOut.setTiledPresentation (! scopeStripLayout);
            levelMeterOut.setBounds (pane);
            levelMeterOut.setVisible (true);
            break;

        case ScopeModuleId::loudness:
            loudnessMeter.setBounds (pane);
            loudnessMeter.setVisible (true);
            loudnessMeter.setEnabled (true);
            break;

        case ScopeModuleId::stereogram:
            stereogram.setBounds (pane);
            stereogram.setVisible (true);
            stereogram.setEnabled (true);
            break;

        case ScopeModuleId::histogram:
            histogram.setBounds (pane);
            histogram.setVisible (true);
            histogram.setEnabled (true);
            break;

        case ScopeModuleId::goniometer:
            goniometer.setBounds (pane);
            goniometer.setVisible (true);
            // Scope panes need right-click menus (expanded overlay otherwise passes clicks through).
            goniometer.setInterceptsMouseClicks (true, true);
            if (toolH > 0)
            {
                auto row = overlayTools;
                placeOverlayTool (gonExpandButton, row);
            }
            break;

        case ScopeModuleId::spectrum:
            m_visualizer.setBounds (pane);
            m_visualizer.setVisible (true);
            break;

        case ScopeModuleId::oscilloscope:
        {
            oscilloscope.setBounds (pane);
            oscilloscope.setVisible (true);
            oscilloscope.setInterceptsMouseClicks (true, true);
            if (toolH > 0)
            {
                auto row = overlayTools;
                placeOverlayTool (oscZoomInButton, row);
                placeOverlayTool (oscZoomOutButton, row);
                placeOverlayTool (oscChannelModeButton, row);
                placeOverlayTool (oscExpandButton, row);
            }
            break;
        }

        case ScopeModuleId::spectrogram:
        {
            // Always 2D â€” Spectrogram 3D is a separate selectable Scope module.
            auto view = pane;
            if (menu.isVisible())
                view.setRight (juce::jmin (view.getRight(), menu.getX() - 8));

            spectrogram.setVisible (true);
            spectrogram.setInterceptsMouseClicks (true, true);
            spectrogram.setBounds (view);

            if (toolH > 0)
            {
                auto row = overlayTools;
                placeOverlayTool (specSpeedUpButton, row);
                placeOverlayTool (specSpeedDownButton, row);
                placeOverlayTool (specExpandButton, row);
                // No 2D/3D cube toggle in Scope â€” Spec and Spec 3D are separate modules.
            }
            syncSpec3DPresentation();
            break;
        }

        case ScopeModuleId::spectrogram3D:
        {
            auto view = pane;
            if (menu.isVisible())
                view.setRight (juce::jmin (view.getRight(), menu.getX() - 8));
            placeSpectrogram3DPane (view, overlayTools, toolH, toolSize, toolGap);
            syncSpec3DPresentation();
            break;
        }

        default:
            break;
    }
}

void MainComponent::layoutScopeModePanes (float scale)
{
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    const int n = (int) scopeEnabledOrder.size();
    const int gap = px (3.0f);
    // Compact tool overlay for both strip and tiled (cube / speed / expand on Spec panes).
    const int toolH = px (16.0f);
    const int toolGap = px (1.0f);
    const int toolSize = toolH > 0 ? juce::jmax (12, toolH - 2) : 12;

    hideAllScopePanes();

    std::vector<juce::Rectangle<int>> slots ((size_t) juce::jmax (0, n));
    juce::Rectangle<int> stripForOverlay {};

    if (n == 0)
    {
        scopeArrangeOverlay.setBounds (getLocalBounds());
        scopeArrangeOverlay.setSlotBounds (slots);
        scopeArrangeOverlay.setStripBounds (stripForOverlay);
        return;
    }

    if (scopeStripLayout)
    {
        scopeSplitOverlay.setVisible (false);
        frequencyResponseComponent.setVisible (false);
        frequencyResponseComponent.setBounds ({});

        ensureScopeStripFractions();
        // Edge-to-edge strip â€” Scope / Settings / Arrange overlay the panes.
        const int stripTop = 0;
        const int stripH = juce::jmax (px ((float) kScopeStripHeightMinPx), getHeight() - stripTop);
        auto strip = juce::Rectangle<int> (0, stripTop, getWidth(), stripH).reduced (gap, 0);
        stripForOverlay = strip;

        int x = strip.getX();
        const int totalW = strip.getWidth();
        for (int slot = 0; slot < n; ++slot)
        {
            int cellW = (slot == n - 1)
                            ? (strip.getRight() - x)
                            : juce::jmax (1, juce::roundToInt ((float) totalW
                                                               * scopeStripFractions[(size_t) slot]));
            auto cell = juce::Rectangle<int> (x, strip.getY(), cellW, strip.getHeight())
                            .reduced (gap / 2, gap);
            slots[(size_t) slot] = cell;
            placeScopePane (scopeEnabledOrder[(size_t) slot], cell, toolH, toolSize, toolGap);
            x += cellW;
        }
    }
    else if (n == 4)
    {
        auto graph = getLocalBounds();
        scopeSplitOverlay.setBounds (graph);
        scopeSplitOverlay.setVisible (true);

        const int sx = juce::roundToInt ((float) graph.getWidth() * scopeSplitOverlay.getSplitX());
        const int sy = juce::roundToInt ((float) graph.getHeight() * scopeSplitOverlay.getSplitY());

        slots[0] = juce::Rectangle<int> (graph.getX(), graph.getY(), sx, sy).reduced (gap);
        slots[1] = juce::Rectangle<int> (graph.getX() + sx, graph.getY(), graph.getWidth() - sx, sy).reduced (gap);
        slots[2] = juce::Rectangle<int> (graph.getX(), graph.getY() + sy, sx, graph.getHeight() - sy).reduced (gap);
        slots[3] = juce::Rectangle<int> (graph.getX() + sx, graph.getY() + sy,
                                           graph.getWidth() - sx, graph.getHeight() - sy).reduced (gap);

        frequencyResponseComponent.setVisible (false);

        for (int slot = 0; slot < n; ++slot)
            placeScopePane (scopeEnabledOrder[(size_t) slot], slots[(size_t) slot], toolH, toolSize, toolGap);
    }
    else
    {
        scopeSplitOverlay.setVisible (false);
        frequencyResponseComponent.setVisible (false);

        auto graph = getLocalBounds().reduced (gap);
        const int cols = juce::jmax (1, (int) std::ceil (std::sqrt ((float) n)));
        const int rows = (n + cols - 1) / cols;
        const int cellW = juce::jmax (1, graph.getWidth() / cols);
        const int cellH = juce::jmax (1, graph.getHeight() / rows);

        for (int slot = 0; slot < n; ++slot)
        {
            const int row = slot / cols;
            const int col = slot % cols;
            auto cell = juce::Rectangle<int> (graph.getX() + col * cellW,
                                              graph.getY() + row * cellH,
                                              cellW, cellH).reduced (gap / 2);
            slots[(size_t) slot] = cell;
            placeScopePane (scopeEnabledOrder[(size_t) slot], cell, toolH, toolSize, toolGap);
        }
    }

    oscDimmer.setVisible (false);

    scopeArrangeOverlay.setBounds (getLocalBounds());
    scopeArrangeOverlay.setSlotBounds (slots);
    scopeArrangeOverlay.setStripBounds (stripForOverlay);
    scopeArrangeOverlay.setVisible (true);

    goniometer.toFront (false);
    m_visualizer.toFront (false);
    oscilloscope.toFront (false);
    spectrogram.toFront (false);
    spectrogram3D.toFront (false);
    levelMeterIn.toFront (false);
    levelMeterOut.toFront (false);
    loudnessMeter.toFront (false);
    stereogram.toFront (false);
    gonExpandButton.toFront (false);
    oscZoomInButton.toFront (false);
    oscZoomOutButton.toFront (false);
    oscChannelModeButton.toFront (false);
    oscExpandButton.toFront (false);
    specSpeedUpButton.toFront (false);
    specSpeedDownButton.toFront (false);
    specExpandButton.toFront (false);
    scopeArrangeOverlay.toFront (false);
    if (! scopeStripLayout && n == 4)
        scopeSplitOverlay.toFront (false);

    if (scopeStripLayout)
    {
        menuToggleButton.toFront (false);
        if (menu.isVisible())
            menu.toFront (false);
    }

    verticalGradientMeterL.toFront (false);
    verticalGradientMeterR.toFront (false);
    verticalGradientMeterPostL.toFront (false);
    verticalGradientMeterPostR.toFront (false);
    meterChannelModeButton.toFront (false);
    arrangeButton.toFront (false);
}

void MainComponent::layoutPresetChrome (float scale)
{
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    // Scope mode: clear the center for meters. DSP chrome (Bypass / Aâ€“D / Eco) follows Pre/Post.
    if (scopeModeEnabled)
    {
        presetPrevButton.setVisible (false);
        presetNameEditor.setVisible (false);
        presetMenuButton.setVisible (false);
        presetNextButton.setVisible (false);
        presetSaveButton.setVisible (false);
        presetPrevButton.setBounds ({});
        presetNameEditor.setBounds ({});
        presetMenuButton.setBounds ({});
        presetNextButton.setBounds ({});
        presetSaveButton.setBounds ({});
        return;
    }

    presetPrevButton.setVisible (true);
    presetNameEditor.setVisible (true);
    presetMenuButton.setVisible (true);
    presetNextButton.setVisible (true);
    presetSaveButton.setVisible (true);

    const int chromeH = px (28.0f);
    const int chromeY = px (6.0f);
    const int gap = px (4.0f);
    const int navW = px (22.0f);
    const int menuW = px (22.0f);
    const int nameW = px (138.0f);
    const int saveW = px (52.0f);
    const int barH = px (24.0f);
    const int totalW = navW + gap + nameW + menuW + gap + navW + gap + saveW;

    int barY = chromeY + chromeH + px (2.0f);

    // Compact: logo is hosted on this component at the top â€” sit directly under it.
    if (hostedWordmark != nullptr
        && hostedWordmark->getParentComponent() == this
        && hostedWordmark->isVisible()
        && hostedWordmark->getY() < getHeight() / 2)
    {
        barY = hostedWordmark->getBottom() + px (2.0f);
    }

    int x = (getWidth() - totalW) / 2;
    presetPrevButton.setBounds (x, barY, navW, barH);
    x += navW + gap;
    presetNameEditor.setBounds (x, barY, nameW, barH);
    x += nameW;
    presetMenuButton.setBounds (x, barY, menuW, barH);
    x += menuW + gap;
    presetNextButton.setBounds (x, barY, navW, barH);
    x += navW + gap;
    presetSaveButton.setBounds (x, barY, saveW, barH);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll (sharedResources.sharedColors.pluginBackground);
}

void MainComponent::resized()
{
    // Match EqEditor design width so fixed offsets stay proportional when the window scales.
    constexpr float designWidth = 1200.0f;
    const float scale = (float) getWidth() / designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    const int FrequencyResponseyOffset = 0;
    const int parentWidth = getWidth();
    const int parentHeight = getHeight();
    const int componentWidth = parentWidth;
    const int componentHeight = parentHeight;
    const int xPos = (parentWidth - componentWidth) / 2;

    frequencyResponseComponent.setBounds(xPos, FrequencyResponseyOffset, componentWidth, componentHeight);

    auto area = getLocalBounds().reduced (px ((float) m_marginInPixels));

    if (! scopeModeEnabled)
        m_visualizer.setBounds(xPos, FrequencyResponseyOffset, componentWidth, componentHeight);

    const int controlsWidth = px (200.0f);
    const int controlsHeight = px (180.0f);
    const int xPositionForControls = 0;
    const int yPositionForControls = area.getHeight() - controlsHeight + px (10.0f);
    m_controls.setBounds(xPositionForControls, yPositionForControls, controlsWidth, controlsHeight);

    const int editorWidth = getWidth();
    const int editorHeight = getHeight();
    // Keep meters in the plot area â€” never grow into / over the piano strip.
    const int pianoH = frequencyResponseComponent.getPianoStripHeight();
    const int layoutH = juce::jmax (1, editorHeight - pianoH);
    const int meterWidth = static_cast<int>(editorWidth * 0.015);
    const int meterHeight = static_cast<int>(layoutH * 0.90);
    const int meterSpacing = static_cast<int>(meterWidth * 0.4f);
    const int centerY = (layoutH - meterHeight) / 2;
    const int totalMeterGroupWidth = 2 * meterWidth + meterSpacing;
    const int padding = static_cast<int>(totalMeterGroupWidth * 0.4);
    const int xLeft = static_cast<int>(padding * 1.5);
    const int xRight = editorWidth - padding - totalMeterGroupWidth;
    const int meterY = centerY + px (20.0f);

    // Scope mode uses Level Meter modules only â€” hide the default edge meters.
    const bool hideEdgeMeters = scopeModeEnabled;
    verticalGradientMeterL.setVisible (! hideEdgeMeters);
    verticalGradientMeterR.setVisible (! hideEdgeMeters);
    verticalGradientMeterPostL.setVisible (! hideEdgeMeters);
    verticalGradientMeterPostR.setVisible (! hideEdgeMeters);
    meterChannelModeButton.setVisible (! hideEdgeMeters);

    verticalGradientMeterL.setBounds(xLeft, meterY, meterWidth, meterHeight);
    verticalGradientMeterR.setBounds(xLeft + meterWidth + meterSpacing, meterY, meterWidth, meterHeight);
    verticalGradientMeterPostL.setBounds(xRight, meterY, meterWidth, meterHeight);
    verticalGradientMeterPostR.setBounds(xRight + meterWidth + meterSpacing, meterY, meterWidth, meterHeight);

    // Channel-mode toggle sits just left of the post (output) meter pair.
    // Eco (top chrome) uses these same dimensions so the two chrome toggles match.
    const int meterModeW = juce::jmax (px (36.0f), meterWidth * 2 + meterSpacing);
    const int meterModeH = px (22.0f);
    {
        const int modeX = juce::jmax (0, xRight - meterModeW - px (6.0f));
        const int modeY = meterY + px (4.0f);
        meterChannelModeButton.setBounds (modeX, modeY, meterModeW, meterModeH);
    }

    auto area2 = getLocalBounds().reduced (px (10.0f));
    constexpr float buttonScaleFactor = 1.2f;
    const int settingsW = px (40.0f * buttonScaleFactor);
    const int settingsH = px (30.0f * buttonScaleFactor);
    // Match EqEditor helpTooltipsButton (?) size: px(20), clamped 16..22 compact / 16..24 expanded.
    constexpr int helpTrimH = 30;
    const int historySize = juce::jlimit (16, editor.isUiCompact() ? 22 : (helpTrimH - 6), px (20.0f));
    const int historyGap = px (4.0f);
    const bool scopeStripMinimal = scopeModeEnabled && scopeStripLayout;

    if (scopeStripMinimal)
    {
        // Strip: Settings stays, 50% size, overlaid top-right. Undo/Redo hidden.
        undoButton.setVisible (false);
        redoButton.setVisible (false);
        undoButton.setBounds ({});
        redoButton.setBounds ({});
        const int sw = juce::jmax (14, settingsW / 2);
        const int sh = juce::jmax (12, settingsH / 2);
        menuToggleButton.setVisible (true);
        menuToggleButton.setIdleAlpha (0.5f);
        menuToggleButton.setBounds (area2.getRight() - sw, area2.getY(), sw, sh);
        menuToggleButton.toFront (false);
        if (menu.isVisible())
            menu.toFront (false);
    }
    else
    {
        menuToggleButton.setVisible (true);
        menuToggleButton.setIdleAlpha (1.0f);
        undoButton.setVisible (true);
        redoButton.setVisible (true);
        menuToggleButton.setBounds (area2.getRight() - settingsW, area2.getY(),
                                    settingsW, settingsH);

        // Undo / Redo just left of Settings (hamburger), same size as ?.
        const int historyY = area2.getY() + (settingsH - historySize) / 2;
        int hx = menuToggleButton.getX() - historyGap - historySize;
        redoButton.setBounds (hx, historyY, historySize, historySize);
        hx -= historyGap + historySize;
        undoButton.setBounds (hx, historyY, historySize, historySize);
    }

    // Top-left chrome: Bypass | A | B | C | D | UI | Dice.
    // Scope strip: Bypass / Aâ€“D / Eco hidden; UI + Dice overlay panes (no stripTop).
    {
        constexpr int minimizeSize = 22;
        constexpr int minimizeMargin = 6;
        const int chromeH = px (28.0f);
        const int chromeY = px ((float) minimizeMargin);
        const int gap = px (4.0f);
        const int bypassW = px (62.0f);
        // Referencing slots â€” 40% smaller than the previous A/B chrome size.
        const int abW = px (28.0f * 0.6f);
        const int abH = px (28.0f * 0.6f);
        const int abY = chromeY + (chromeH - abH) / 2;
        const int uiW = px (28.0f);
        const int diceW = px (24.0f);
        const int toolsY = chromeY + (chromeH - abH) / 2;
        const int toolsH = abH;
        const bool analyzerOnly = isScopeAnalyzerOnly();
        const float stripIdle = scopeStripMinimal ? 0.5f : 1.0f;

        if (scopeStripMinimal)
        {
            bypassButton.setVisible (false);
            slotAButton.setVisible (false);
            slotBButton.setVisible (false);
            slotCButton.setVisible (false);
            slotDButton.setVisible (false);
            bypassButton.setBounds ({});
            slotAButton.setBounds ({});
            slotBButton.setBounds ({});
            slotCButton.setBounds ({});
            slotDButton.setBounds ({});

            // Square UI / Dice / Arrange â€” same size & pad as Scope button (bottom-left).
            const int pad = px (8.0f);
            const int btn = juce::jlimit (16, 22, px (20.0f));
            const int btnGap = px (6.0f);
            int x = pad;
            const int y = pad;
            uiThemeButton.setVisible (true);
            uiRandomizeButton.setVisible (true);
            uiThemeButton.setIdleAlpha (stripIdle);
            uiRandomizeButton.setIdleAlpha (stripIdle);
            uiThemeButton.setBounds (x, y, btn, btn);
            x += btn + btnGap;
            uiRandomizeButton.setBounds (x, y, btn, btn);
            x += btn + btnGap;
            arrangeButton.setIdleAlpha (stripIdle);
            arrangeButton.setVisible (true);
            arrangeButton.setBounds (x, y, btn, btn);
            arrangeButton.setToggleState (scopeStripLayout, juce::dontSendNotification);
            arrangeButton.setGlyph (OscToolButton::Glyph::StripLayout);
            arrangeButton.refreshAlpha();
            uiThemeButton.toFront (false);
            uiRandomizeButton.toFront (false);
            arrangeButton.toFront (false);
        }
        else
        {
        int x = minimizeMargin + minimizeSize + gap + px (2.0f);
        if (analyzerOnly)
        {
            bypassButton.setVisible (false);
            slotAButton.setVisible (false);
            slotBButton.setVisible (false);
            slotCButton.setVisible (false);
            slotDButton.setVisible (false);
            bypassButton.setBounds ({});
            slotAButton.setBounds ({});
            slotBButton.setBounds ({});
            slotCButton.setBounds ({});
            slotDButton.setBounds ({});
        }
        else
        {
            bypassButton.setVisible (true);
            slotAButton.setVisible (true);
            slotBButton.setVisible (true);
            slotCButton.setVisible (true);
            slotDButton.setVisible (true);
            bypassButton.setBounds (x, chromeY, bypassW, chromeH);
            x += bypassW + gap;
            slotAButton.setBounds (x, abY, abW, abH);
            x += abW + gap;
            slotBButton.setBounds (x, abY, abW, abH);
            x += abW + gap;
            slotCButton.setBounds (x, abY, abW, abH);
            x += abW + gap;
            slotDButton.setBounds (x, abY, abW, abH);
            x += abW + gap + px (2.0f);
        }

        uiThemeButton.setVisible (true);
        uiRandomizeButton.setVisible (true);
        uiThemeButton.setIdleAlpha (1.0f);
        uiRandomizeButton.setIdleAlpha (1.0f);
        uiThemeButton.setBounds (x, toolsY, uiW, toolsH);
        x += uiW + gap;
        uiRandomizeButton.setBounds (x, toolsY, diceW, toolsH);
        }
    }

    layoutPresetChrome (scale);

    // Eco / Arrange chrome.
    // Strip: Eco hidden; Arrange stays (line icon â†’ click for grid). Tiled: Arrange beside Eco.
    {
        const int chromeH = px (28.0f);
        const int chromeY = px (6.0f);
        const int gap = px (6.0f);
        const int ecoY = chromeY + (chromeH - meterModeH) / 2;
        const bool analyzerOnly = isScopeAnalyzerOnly();

        if (analyzerOnly || scopeStripMinimal)
        {
            ecoButton.setVisible (false);
            ecoButton.setBounds ({});
        }
        else
        {
            ecoButton.setVisible (true);
            const int ecoX = scopeModeEnabled ? (uiRandomizeButton.getRight() + gap)
                                              : (presetSaveButton.getRight() + gap);
            ecoButton.setBounds (ecoX, ecoY, meterModeW, meterModeH);
        }

        // Arrange: tiled places it in the top chrome; strip already laid it out with UI/Dice.
        if (scopeModeEnabled && ! scopeStripMinimal)
        {
            const int arrangeSize = meterModeH;
            const int arrangeX = analyzerOnly ? (uiRandomizeButton.getRight() + gap)
                                              : (ecoButton.getRight() + gap);
            arrangeButton.setIdleAlpha (1.0f);
            arrangeButton.setVisible (true);
            arrangeButton.setBounds (arrangeX, ecoY, arrangeSize, arrangeSize);
            arrangeButton.setToggleState (scopeStripLayout, juce::dontSendNotification);
            arrangeButton.setGlyph (OscToolButton::Glyph::GridLayout);
            arrangeButton.refreshAlpha();
            arrangeButton.toFront (false);
        }
        else if (! scopeModeEnabled)
        {
            arrangeButton.setVisible (false);
            arrangeButton.setBounds ({});
            arrangeButton.setIdleAlpha (1.0f);
        }

        const int oscW = juce::jmax (28, (meterModeW * 3) / 4);
        const int oscH = juce::jmax (16, (meterModeH * 3) / 4);
        const int oscY = chromeY + (chromeH - oscH) / 2;

        if (scopeModeEnabled)
        {
            oscButton.setVisible (false);
            gonButton.setVisible (false);
            specButton.setVisible (false);
            oscButton.setBounds ({});
            gonButton.setBounds ({});
            specButton.setBounds ({});

            if (! scopeFullscreenModule.has_value())
            {
                layoutScopeModePanes (scale);
            }
            else
            {
                // One Scope pane fullscreen; collapse restores strip or tiled arrange.
                hideAllScopePanes();
                scopeSplitOverlay.setVisible (false);
                scopeArrangeOverlay.setVisible (false);
                frequencyResponseComponent.setVisible (false);
                oscDimmer.setBounds (getLocalBounds());
                oscDimmer.setVisible (true);
                const auto expandBounds = getExpandedScopeContentBounds().isEmpty()
                                              ? getLocalBounds()
                                              : getExpandedScopeContentBounds();
                const int toolH = px (16.0f);
                const int toolGap = px (1.0f);
                const int toolSize = juce::jmax (12, toolH - 2);
                placeScopePane (*scopeFullscreenModule, expandBounds, toolH, toolSize, toolGap);
                // Don't call sync*ToolButtons here â€” they key off chrome toggles and would
                // re-show Osc/Gon/Spec tools over unrelated fullscreen panes.
                if (*scopeFullscreenModule == ScopeModuleId::oscilloscope)
                {
                    oscExpandButton.setGlyph (OscToolButton::Glyph::Collapse);
                    oscExpandButton.setTooltip ("Collapse back to Scope arrange");
                    oscExpandButton.setToggleState (true, juce::dontSendNotification);
                    oscZoomInButton.toFront (false);
                    oscZoomOutButton.toFront (false);
                    oscChannelModeButton.toFront (false);
                    oscExpandButton.toFront (false);
                }
                else if (*scopeFullscreenModule == ScopeModuleId::goniometer)
                {
                    gonExpandButton.setGlyph (OscToolButton::Glyph::Collapse);
                    gonExpandButton.setTooltip ("Collapse back to Scope arrange");
                    gonExpandButton.setToggleState (true, juce::dontSendNotification);
                    gonExpandButton.toFront (false);
                }
                else if (*scopeFullscreenModule == ScopeModuleId::spectrogram)
                {
                    specExpandButton.setGlyph (OscToolButton::Glyph::Collapse);
                    specExpandButton.setTooltip ("Collapse back to Scope arrange");
                    specExpandButton.setToggleState (true, juce::dontSendNotification);
                    raiseSpecToolButtons();
                }
            }
        }
        else
        {
        scopeArrangeOverlay.setVisible (false);
        oscButton.setVisible (true);
        gonButton.setVisible (true);
        specButton.setVisible (true);
        oscButton.setBounds (ecoButton.getRight() + gap, oscY, oscW, oscH);
        gonButton.setBounds (oscButton.getX(), oscButton.getBottom() + px (2.0f), oscW, oscH);
        specButton.setBounds (gonButton.getX(), gonButton.getBottom() + px (2.0f), oscW, oscH);

        scopeSplitOverlay.setVisible (false);
        if (! frequencyResponseComponent.isVisible())
            frequencyResponseComponent.setVisible (true);
        m_visualizer.setVisible (true);

        const bool oscOn = oscButton.getToggleState();
        const bool gonOn = gonButton.getToggleState();
        const bool specOn = specButton.getToggleState();
        syncOscToolButtons();
        syncGonToolButtons();
        syncSpecToolButtons();

        const int scopeH = OscilloscopeComponent::kWindowHeightPx;
        const int btnGap = px (2.0f);
        // Shared tool size for Osc / Gon / Spec compact chrome (was oversized for Spec).
        const int btnSize = juce::jmax (14, (scopeH - 3 * btnGap) / 4);
        const int scopeY = chromeY + px (10.0f);
        const int gonExpandSize = juce::jmax (14, btnSize);

        // Reserve right-side room for Gon square + correlation + expand when Gon is on.
        int rightLimit = meterChannelModeButton.getX() - gap;
        if (gonOn)
        {
            const int gonBlockW = GoniometerComponent::kCompactWidthPx + px (2.0f) + gonExpandSize;
            rightLimit = juce::jmax (oscButton.getRight() + gap + 40,
                                     meterChannelModeButton.getX() - gap - gonBlockW);
        }

        if (oscOn)
        {
            const int scopeLeft = oscButton.getRight() + gap;
            const int scopeRightLimit = rightLimit - btnSize - px (4.0f);
            const int scopeW = juce::jmax (40, scopeRightLimit - scopeLeft);
            const int btnX = scopeLeft + scopeW + px (2.0f);

            if (oscExpanded)
            {
                if (oscFullGraph)
                {
                    oscFrame.setContent (nullptr);
                    oscFrame.setFrameActive (false);
                    if (oscilloscope.getParentComponent() != this)
                        addAndMakeVisible (oscilloscope);
                    oscDimmer.setBounds (getLocalBounds());
                    oscDimmer.setVisible (true);
                    oscilloscope.setBounds (getExpandedScopeContentBounds());
                    oscZoomInButton.setBounds (btnX, scopeY, btnSize, btnSize);
                    oscZoomOutButton.setBounds (btnX, scopeY + btnSize + btnGap, btnSize, btnSize);
                    oscChannelModeButton.setBounds (btnX, scopeY + 2 * (btnSize + btnGap), btnSize, btnSize);
                    oscExpandButton.setBounds (btnX, scopeY + 3 * (btnSize + btnGap), btnSize, btnSize);
                }
                else
                {
                    oscFrame.setContent (&oscilloscope);
                    layoutFramedScopeWindow (oscFrame, oscFrameBoundsCustom,
                                            oscFramePreferredW, oscFramePreferredH,
                                            oscFramePreferredX, oscFramePreferredY,
                                            juce::jmin (520, getFramedScopeAvailableArea().getWidth()
                                                             - getFramedToolColumnWidth()),
                                            juce::jmin (220, getFramedScopeAvailableArea().getHeight()),
                                            false);
                    // Framed mode needs clicks for double-click toggle (expanded usually click-through).
                    oscilloscope.setInterceptsMouseClicks (true, true);
                    syncOscFramedTools();
                }
            }
            else
            {
                oscFrame.setContent (nullptr);
                oscFrame.setFrameActive (false);
                if (oscilloscope.getParentComponent() != this)
                    addAndMakeVisible (oscilloscope);
                oscilloscope.setBounds (scopeLeft, scopeY, scopeW, scopeH);
                oscZoomInButton.setBounds (btnX, scopeY, btnSize, btnSize);
                oscZoomOutButton.setBounds (btnX, scopeY + btnSize + btnGap, btnSize, btnSize);
                oscChannelModeButton.setBounds (btnX, scopeY + 2 * (btnSize + btnGap), btnSize, btnSize);
                oscExpandButton.setBounds (btnX, scopeY + 3 * (btnSize + btnGap), btnSize, btnSize);
            }

            oscilloscope.setVisible (true);
        }
        else
        {
            oscFrame.setContent (nullptr);
            oscFrame.setFrameActive (false);
            oscilloscope.setVisible (false);
        }

        if (gonOn)
        {
            const int gonW = GoniometerComponent::kCompactWidthPx;
            const int gonX = meterChannelModeButton.getX() - gap - gonExpandSize - px (2.0f) - gonW;
            const int gonExpandX = gonX + gonW + px (2.0f);

            if (gonExpanded)
            {
                if (gonFullGraph)
                {
                    gonFrame.setContent (nullptr);
                    gonFrame.setFrameActive (false);
                    if (goniometer.getParentComponent() != this)
                        addAndMakeVisible (goniometer);
                    oscDimmer.setBounds (getLocalBounds());
                    oscDimmer.setVisible (true);
                    goniometer.setBounds (getExpandedScopeContentBounds());
                    gonExpandButton.setBounds (gonExpandX, scopeY, gonExpandSize, gonExpandSize);
                }
                else
                {
                    gonFrame.setContent (&goniometer);
                    layoutFramedScopeWindow (gonFrame, gonFrameBoundsCustom,
                                            gonFramePreferredW, gonFramePreferredH,
                                            gonFramePreferredX, gonFramePreferredY,
                                            280, 280, true);
                    goniometer.setInterceptsMouseClicks (true, true);
                    syncGonFramedTools();
                }
            }
            else
            {
                gonFrame.setContent (nullptr);
                gonFrame.setFrameActive (false);
                if (goniometer.getParentComponent() != this)
                    addAndMakeVisible (goniometer);
                goniometer.setBounds (gonX, scopeY, gonW, scopeH);
                gonExpandButton.setBounds (gonExpandX, scopeY, gonExpandSize, gonExpandSize);
            }

            goniometer.setVisible (true);
        }
        else
        {
            gonFrame.setContent (nullptr);
            gonFrame.setFrameActive (false);
            goniometer.setVisible (false);
        }

        // Dimmer only for full-graph overlays (framed windows are soft-composited).
        if (! ((oscExpanded && oscFullGraph) || (gonExpanded && gonFullGraph)
               || (specExpanded && specFullGraph)))
            oscDimmer.setVisible (false);

        // Spectrogram: between UI dice (left) and EQ preset bar (right), osc height, a bit wider.
        // Tool column (+ / - / expand) sits on the right edge of the strip, like OSC.
        if (specOn)
        {
            const int pad = px (8.0f);
            const int left = uiRandomizeButton.getRight() + pad;
            const int right = presetPrevButton.getX() - pad;
            const int specH = SpectrogramComponent::kWindowHeightPx;
            const int preferredW = juce::jmax (SpectrogramComponent::kPreferredWidthPx,
                                               OscilloscopeComponent::kWindowHeightPx + px (80.0f));
            const int toolColW = btnSize + px (2.0f);
            const int availW = right - left - toolColW;
            if (availW >= 48)
            {
                const int specW = juce::jmin (availW, juce::jmax (preferredW, (availW * 3) / 4));
                const int blockW = specW + toolColW;
                const int blockX = left + juce::jmax (0, (right - left - blockW) / 2);
                const int specY = juce::jmax (chromeY + px (10.0f),
                                              presetPrevButton.getY() + (presetPrevButton.getHeight() - specH) / 2);
                const int btnX = blockX + specW + px (2.0f);

                if (specExpanded)
                {
                    const bool use3D = spec3DEnabled && specButton.getToggleState();
                    if (use3D || specFullGraph)
                    {
                        specFrame.setContent (nullptr);
                        specFrame.setFrameActive (false);
                        if (spectrogram.getParentComponent() != this)
                            addAndMakeVisible (spectrogram);
                        if (specFullGraph && ! use3D)
                        {
                            oscDimmer.setBounds (getLocalBounds());
                            oscDimmer.setVisible (true);
                        }
                        layoutExpandedSpectrogramWithTools (btnSize, btnGap);
                        if (use3D && specFullGraph)
                        {
                            // Fill available area (ignore custom floating size).
                            auto area = getFramedScopeAvailableArea();
                            const int toolW = getFramedToolColumnWidth();
                            if (area.getWidth() > toolW + 80)
                                area.removeFromRight (toolW);
                            spectrogram3D.setBounds (area);
                            syncSpec3DFramedTools();
                        }
                    }
                    else
                    {
                        specFrame.setContent (&spectrogram);
                        layoutFramedScopeWindow (specFrame, specFrameBoundsCustom,
                                                specFramePreferredW, specFramePreferredH,
                                                specFramePreferredX, specFramePreferredY,
                                                juce::jmin (560, getFramedScopeAvailableArea().getWidth()
                                                                 - getFramedToolColumnWidth()),
                                                juce::jmin (260, getFramedScopeAvailableArea().getHeight()),
                                                false);
                        spectrogram.setOpaque (false);
                        spectrogram.setInterceptsMouseClicks (true, true);
                        syncSpecFramedTools();
                    }
                }
                else
                {
                    specFrame.setContent (nullptr);
                    specFrame.setFrameActive (false);
                    if (spectrogram.getParentComponent() != this)
                        addAndMakeVisible (spectrogram);
                    spectrogram.setBounds (blockX, specY, specW, specH);
                    spectrogram3D.setBounds (spectrogram.getBounds());
                    specSpeedUpButton.setBounds (btnX, specY, btnSize, btnSize);
                    specSpeedDownButton.setBounds (btnX, specY + btnSize + btnGap, btnSize, btnSize);
                    specExpandButton.setBounds (btnX, specY + 2 * (btnSize + btnGap), btnSize, btnSize);
                    spec3DButton.setBounds (btnX, specY + 3 * (btnSize + btnGap), btnSize, btnSize);
                    raiseSpecToolButtons();
                }

                spectrogram.setVisible (true);
                syncSpec3DPresentation();
            }
            else
            {
                spectrogram.setVisible (false);
            }
        }
        else
        {
            specFrame.setContent (nullptr);
            specFrame.setFrameActive (false);
            spectrogram.setVisible (false);
        }
        } // !scopeModeEnabled compact strips
    }

    // Settings panel: freely movable/resizable; content stays at design size (viewport scrolls).
    layoutSettingsMenu();

    // Spec3D OS fullscreen: keep desktop host sized if display/editor moved.
    if (spec3DFullscreen)
        applySpec3DFullscreenLayout();

    // Wordmark may be re-hosted from EqEditor::resized; keep menu chrome above it.
    syncExpandedOscOverlayStack();
}

void MainComponent::mouseDrag(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
    // FrequencyResponseComponent already repaints on drag; avoid full-tree repaints.
}

void MainComponent::mouseDown(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);
}

void MainComponent::layoutSettingsMenu()
{
    menu.setTransform ({});

    const int contentW = Menu::kContentWidth;
    const int parentW = getWidth();
    const int parentH = getHeight();
    if (parentW <= 0 || parentH <= 0)
        return;

    // Same floor expanded Osc / Gon / Spec3D use: graph pane, just above the
    // Match / Mod / Help / â€¦ button row (and above the piano strip).
    auto contentArea = getExpandedScopeContentBounds();
    if (contentArea.isEmpty())
        contentArea = getLocalBounds();
    const int bottomChrome = frequencyResponseComponent.getBottomGraphChromeHeight();
    constexpr int kGapAboveBottomChrome = 4;
    constexpr int topY = 0;
    const int maxBottom = juce::jlimit (140, parentH,
                                        contentArea.getBottom()
                                            - juce::jmax (0, bottomChrome)
                                            - kGapAboveBottomChrome);

    if (settingsMenuBounds.isEmpty() || ! settingsMenuBoundsFromUser)
    {
        // Default: right-anchored, full height down to the shared bottom chrome limit.
        const int w = juce::jmin (contentW, parentW);
        const int h = juce::jmax (140, maxBottom - topY);
        const int x = juce::jmax (0, parentW - w);
        settingsMenuBounds = { x, topY, w, h };
    }

    auto b = settingsMenuBounds;
    // Clamp size first, then re-pin so the *entire* frame stays on-screen.
    // (Previously a wide menu could keep a large X and push the right edge off.)
    b.setWidth (juce::jlimit (200, parentW, b.getWidth()));
    b.setHeight (juce::jlimit (140, parentH, b.getHeight()));
    if (b.getRight() > parentW)
        b.setX (juce::jmax (0, parentW - b.getWidth()));
    if (b.getX() < 0)
        b.setX (0);
    if (b.getRight() > parentW)
        b.setWidth (parentW - b.getX());
    b.setY (juce::jlimit (0, juce::jmax (0, parentH - b.getHeight()), b.getY()));

    // Keep the panel above the shared bottom-chrome line (even after user drag/resize).
    if (b.getBottom() > maxBottom)
    {
        if (b.getHeight() > maxBottom - topY)
        {
            b.setY (topY);
            b.setHeight (juce::jmax (140, maxBottom - topY));
        }
        else
        {
            b.setY (juce::jmax (topY, maxBottom - b.getHeight()));
        }
    }

    updatingSettingsMenuBounds = true;
    menu.setBounds (b);
    updatingSettingsMenuBounds = false;
    settingsMenuBounds = menu.getBounds();

    // Constrain border-resize so left-edge drags cannot push the right edge off-screen.
    if (auto* parent = menu.getParentComponent())
        menu.setResizeLimitsWithinParent (parent->getLocalBounds());
}

void MainComponent::componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized)
{
    juce::ignoreUnused (wasMoved, wasResized);
    if (updatingSettingsMenuBounds || &component != &menu)
        return;

    settingsMenuBounds = menu.getBounds();
    settingsMenuBoundsFromUser = true;

    // If a border-resize shoved any edge past the editor, clamp immediately
    // (left-edge drag used to grow width and park the right edge off-screen).
    {
        const int parentW = getWidth();
        const int parentH = getHeight();
        auto b = settingsMenuBounds;
        bool fixed = false;
        if (b.getWidth() > parentW)
        {
            b.setWidth (parentW);
            fixed = true;
        }
        if (b.getHeight() > parentH)
        {
            b.setHeight (parentH);
            fixed = true;
        }
        if (b.getRight() > parentW)
        {
            b.setX (juce::jmax (0, parentW - b.getWidth()));
            fixed = true;
        }
        if (b.getX() < 0)
        {
            b.setX (0);
            fixed = true;
        }
        if (b.getBottom() > parentH)
        {
            b.setY (juce::jmax (0, parentH - b.getHeight()));
            fixed = true;
        }
        if (b.getY() < 0)
        {
            b.setY (0);
            fixed = true;
        }
        if (fixed)
        {
            updatingSettingsMenuBounds = true;
            menu.setBounds (b);
            updatingSettingsMenuBounds = false;
            settingsMenuBounds = menu.getBounds();
        }
        menu.setResizeLimitsWithinParent (getLocalBounds());
    }

    // Keep expanded 3D parked left of Settings as the menu is dragged/resized.
    if (menu.isVisible() && spectrogram3D.isActive() && specExpanded)
    {
        constexpr float designWidth = 1200.0f;
        const float scale = (float) getWidth() / designWidth;
        const int btnGap = juce::roundToInt (2.0f * scale);
        const int btnSize = juce::jmax (14, (OscilloscopeComponent::kWindowHeightPx - 3 * btnGap) / 4);
        layoutExpandedSpectrogramWithTools (btnSize, btnGap);
        syncSpec3DPresentation();
        if (menu.isVisible())
            menu.toFront (false);
    }
}

void MainComponent::hostBrandWordmark (juce::Component& wordmark)
{
    hostedWordmark = &wordmark;
    addAndMakeVisible (wordmark);
    syncExpandedOscOverlayStack();
}

void MainComponent::relayoutPresetChrome()
{
    constexpr float designWidth = 1200.0f;
    layoutPresetChrome ((float) getWidth() / designWidth);
    syncExpandedOscOverlayStack();
}

void MainComponent::raiseMenuSystemAboveWordmark()
{
    // Z-order (bottom → top):
    //   graph → expanded osc/gon/dimmer → brand wordmark → chrome / meters / zoom
    //   → OptionBox → Settings button → Settings menu (panel above hamburger when open)
    const bool oscExp = oscExpanded && oscButton.getToggleState() && oscilloscope.isVisible();
    const bool gonExp = gonExpanded && gonButton.getToggleState() && goniometer.isVisible();
    const bool specExp = specExpanded && specButton.getToggleState() && spectrogram.isVisible();
    const bool expanded = oscExp || gonExp || specExp;

    // Graph peers first (under an expanded scope).
    m_visualizer.toFront (false);
    frequencyResponseComponent.toFront (false);

    if (expanded)
    {
        oscDimmer.toFront (false);
        if (oscExp)
            oscilloscope.toFront (false);
        if (gonExp)
            goniometer.toFront (false);
        if (specExp)
            spectrogram.toFront (false);
    }

    // Compact logo sits above the opaque graph, below Bypass / presets / Settings.
    if (hostedWordmark != nullptr && hostedWordmark->getParentComponent() == this)
        hostedWordmark->toFront (false);

    presetPrevButton.toFront (false);
    presetNameEditor.toFront (false);
    presetMenuButton.toFront (false);
    presetNextButton.toFront (false);
    presetSaveButton.toFront (false);
    uiThemeButton.toFront (false);
    uiRandomizeButton.toFront (false);
    if (! specExp)
        spectrogram.toFront (false);
    bypassButton.toFront (false);
    slotAButton.toFront (false);
    slotBButton.toFront (false);
    slotCButton.toFront (false);
    slotDButton.toFront (false);
    ecoButton.toFront (false);
    oscButton.toFront (false);
    gonButton.toFront (false);
    specButton.toFront (false);

    if (! expanded)
    {
        oscDimmer.toFront (false);
        oscilloscope.toFront (false);
        goniometer.toFront (false);
        spectrogram.toFront (false);
    }
    else
    {
        // Compact peer stays above dimmer when the other view is expanded.
        if (! oscExp)
            oscilloscope.toFront (false);
        if (! gonExp)
            goniometer.toFront (false);
        if (! specExp)
            spectrogram.toFront (false);
    }

    // Side meters above graph / compact scope / Scope-mode spectrogram pane.
    verticalGradientMeterL.toFront (false);
    verticalGradientMeterR.toFront (false);
    verticalGradientMeterPostL.toFront (false);
    verticalGradientMeterPostR.toFront (false);

    // Expanded goniometer (incl. correlation strip) sits above the post meters so
    // the right-edge correlation meter is never covered. In Scope mode keep meters
    // on top of the spectrogram pane (including when a pane is maximized).
    if (gonExp)
        goniometer.toFront (false);
    if (specExp && ! scopeModeEnabled)
        spectrogram.toFront (false);

    // OSC / Gon / Spec stay above the expanded goniometer (semi-transparent) so they
    // remain clickable while the correlation +1 region stays readable underneath.
    oscButton.toFront (false);
    gonButton.toFront (false);
    specButton.toFront (false);

    oscZoomInButton.toFront (false);
    oscZoomOutButton.toFront (false);
    oscChannelModeButton.toFront (false);
    oscExpandButton.toFront (false);
    gonExpandButton.toFront (false);

    // GL last among peers that share its inset bounds â€” chrome / tools stay outside those
    // bounds (native HWND ignores JUCE z-order on overlap).
    if (spectrogram3D.isActive())
        spectrogram3D.toFront (false);
    raiseSpecToolButtons();

    // Re-raise top chrome after GL so lightweight hits win in any residual overlap.
    bypassButton.toFront (false);
    presetPrevButton.toFront (false);
    presetNameEditor.toFront (false);
    presetMenuButton.toFront (false);
    presetNextButton.toFront (false);
    presetSaveButton.toFront (false);
    undoButton.toFront (false);
    redoButton.toFront (false);
    uiThemeButton.toFront (false);
    uiRandomizeButton.toFront (false);
    ecoButton.toFront (false);
    verticalGradientMeterL.toFront (false);
    verticalGradientMeterR.toFront (false);
    verticalGradientMeterPostL.toFront (false);
    verticalGradientMeterPostR.toFront (false);

    if (scopeModeEnabled && ! oscExp && ! gonExp && ! specExp)
        scopeSplitOverlay.toFront (false);
    meterChannelModeButton.toFront (false);

    // OptionBox + menu must win over the expanded waveform.
    auto* box = frequencyResponseComponent.getOptionBoxMenu();
    if (box != nullptr && box->isVisible())
    {
        box->setAlwaysOnTop (true);
        box->toFront (false);
    }

    const bool menuOpen = menu.isVisible();
    menu.setAlwaysOnTop (menuOpen);
    // Settings chrome under the floating panel when open (close via panel X).
    menuToggleButton.toFront (false);
    if (menuOpen)
        menu.toFront (false);

    // Strip overlays must sit above pane modules (same stack as Settings).
    if (scopeModeEnabled && scopeStripLayout)
    {
        uiThemeButton.toFront (false);
        uiRandomizeButton.toFront (false);
        if (menuOpen)
            menu.toFront (false);
        else
            menuToggleButton.toFront (false);
    }

    // Spec3D OS fullscreen lives on its own desktop peers (host + exit chrome).
    if (spec3DFullscreen)
    {
        if (spec3DOsFullscreenHost != nullptr)
            spec3DOsFullscreenHost->toFront (true);
        if (spec3DFsExitChrome != nullptr)
            spec3DFsExitChrome->toFront (true);
    }

    if (rampSampleOverlay.isVisible())
    {
        rampSampleOverlay.setBounds (getLocalBounds());
        rampSampleOverlay.toFront (true);
    }
}

void MainComponent::setOptionBoxInteractionFaded (bool shouldFade)
{
    frequencyResponseComponent.setOptionBoxInteractionFaded (shouldFade);
}

void MainComponent::setBandManipulationHighlight (int bandIndex)
{
    editor.setBandManipulationHighlight (bandIndex);
}

void MainComponent::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == "METER_CHANNEL_MODE_ID")
    {
        syncMeterChannelModeButton();
        return;
    }

    // Built-in schemes must win over a leftover custom Use toggle (2D only;
    // Spectrogram 3D keeps its own independent custom ramp).
    if (parameterID == "SPEC_COLOUR_SCHEME_ID")
    {
        colourRamps.disableCustomRamp (ColourRampBank::Target::spectrogram);
        spectrogram.refreshColourLutFor3D();
        spectrogram3D.recolourMesh();
        return;
    }

    frequencyResponseComponent.repaint();
}

void MainComponent::syncMeterChannelModeButton()
{
    const bool ms = processor.isMeterMsMode();
    meterChannelModeButton.setButtonText (ms ? "M/S" : "L/R");
    meterChannelModeButton.setToggleState (ms, juce::dontSendNotification);
}

void MainComponent::toggleMeterChannelMode()
{
    auto* param = processor.treeState.getParameter ("METER_CHANNEL_MODE_ID");
    if (param == nullptr)
        return;

    const bool toMs = ! processor.isMeterMsMode();
    param->beginChangeGesture();
    param->setValueNotifyingHost (toMs ? 1.0f : 0.0f);
    param->endChangeGesture();
    syncMeterChannelModeButton();
}
