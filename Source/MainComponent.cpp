#include "MainComponent.h"
#include <JuceHeader.h>
#include "FrequencyResponseComponent.h"
#include "EqEditor.h"
#include "Visualizer/Analyser.h"
#include "ComboBoxLookAndFeel.h"
#include "EqPresetStore.h"
#include "Menu/Gui/ThemeList.h"
#include "ColourRamp/GradientStripEditor.h"
#include <functional>

namespace
{
    /** Right-click Rename / Duplicate / Delete — always parented inside the UI theme panel. */
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
                if (fftRow != nullptr)  fftRow->refreshFromBank();
                if (specRow != nullptr) specRow->refreshFromBank();
                if (fillRow != nullptr) fillRow->refreshFromBank();
            };

            fftRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::fftBars,
                GradientStripEditor::ModeFamily::intensity, kUiScale, layoutCb, activeCb);
            specRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::spectrogram,
                GradientStripEditor::ModeFamily::intensity, kUiScale, layoutCb, activeCb);
            fillRow = std::make_unique<RampTargetAccordion> (
                resources, colourRamps, ColourRampBank::Target::spectrumFill,
                GradientStripEditor::ModeFamily::spatial, kUiScale, layoutCb, activeCb);
            addAndMakeVisible (*fftRow);
            addAndMakeVisible (*specRow);
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
        std::unique_ptr<RampTargetAccordion> fftRow, specRow, fillRow;
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
    menu (sharedResources, treeState, textButtonLookAndFeel, colourRamps)
{
    colourRamps.addChangeListener (this);
    frequencyResponseComponent.onBandManipulationHighlight = [this] (int bandIndex)
    {
        editor.setBandManipulationHighlight (bandIndex);
    };
    frequencyResponseComponent.setEditor (&editor);

    // Stay on JUCE's native Windows renderer (Direct2D in JUCE 8) instead of OpenGL.
    setOpaque(true);

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

    menuToggleButton.onClick = [this] {
        const bool shouldShowMenu = !menu.isVisible();
        menu.setVisible(shouldShowMenu);

        if (shouldShowMenu)
        {
            syncExpandedOscOverlayStack();
            menu.setInterceptsMouseClicks(true, true);
            frequencyResponseComponent.setInterceptsMouseClicks(false, false);
        }
        else
        {
            // Keep Settings / overlays correct relative to an expanded oscilloscope.
            syncExpandedOscOverlayStack();
            frequencyResponseComponent.setInterceptsMouseClicks(true, true);
        }
    };

    // Top chrome: Bypass (left) + A/B/C/D referencing — same look as graph range buttons / Settings area.
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

    // Preset chrome: ◀ | editable name | ▼ | ▶ | Save — EQ/functionality only (UI themes are separate).
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
    uiRandomizeButton.setTooltip ("Randomize checked UI scopes and colour ramps. Right-click to choose what.");
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

    // Undo / Redo — top right, just left of Settings.
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

    // Eco — same Y as Bypass, X just right of Save. Disables analyser/FFT + all scopes.
    styleChromeButton (ecoButton);
    ecoButton.setClickingTogglesState (true);
    ecoButton.setTooltip ("Eco - disables analyser, spectrum, and all scopes to save CPU. Dynamic (D) and Spectral (S) still work.");
    ecoButton.setAlwaysOnTop (true);
    ecoButton.onClick = [this]
    {
        setEcoMode (ecoButton.getToggleState(), true);
    };
    addAndMakeVisible (ecoButton);

    // OSC — slightly smaller than Eco; reveals waveform strip + zoom / mode / expand buttons.
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
    oscExpandButton.onClick = [this] { setOscExpanded (! oscExpanded); };
    oscZoomInButton.setVisible (false);
    oscZoomOutButton.setVisible (false);
    oscChannelModeButton.setVisible (false);
    oscExpandButton.setVisible (false);
    addChildComponent (oscZoomInButton);
    addChildComponent (oscZoomOutButton);
    addChildComponent (oscChannelModeButton);
    addChildComponent (oscExpandButton);

    // Gon — under OSC; mutually exclusive with spectrum analyser.
    styleChromeButton (gonButton);
    gonButton.setClickingTogglesState (true);
    gonButton.setTooltip ("Goniometer - stereo image + correlation");
    gonButton.setAlwaysOnTop (true);
    gonButton.onClick = [this]
    {
        applyGoniometerActive (gonButton.getToggleState());
    };
    addAndMakeVisible (gonButton);

    gonExpandButton.setTooltip ("Expand goniometer over the full graph");
    gonExpandButton.setAlwaysOnTop (true);
    gonExpandButton.onClick = [this] { setGonExpanded (! gonExpanded); };
    gonExpandButton.setVisible (false);
    addChildComponent (gonExpandButton);

    // Spec — spectrogram strip between UI dice and EQ preset bar.
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
    specExpandButton.setTooltip ("Expand spectrogram over the full graph");
    specSpeedUpButton.setAlwaysOnTop (true);
    specSpeedDownButton.setAlwaysOnTop (true);
    specExpandButton.setAlwaysOnTop (true);
    specSpeedUpButton.onClick = [this] { spectrogram.speedUp(); };
    specSpeedDownButton.onClick = [this] { spectrogram.speedDown(); };
    specExpandButton.onClick = [this] { setSpecExpanded (! specExpanded); };
    specSpeedUpButton.setVisible (false);
    specSpeedDownButton.setVisible (false);
    specExpandButton.setVisible (false);
    addChildComponent (specSpeedUpButton);
    addChildComponent (specSpeedDownButton);
    addChildComponent (specExpandButton);

    oscDimmer.setInterceptsMouseClicks (false, false);
    oscDimmer.setAlwaysOnTop (true);
    oscDimmer.setVisible (false);
    addChildComponent (oscDimmer);

    scopeSplitOverlay.setAlwaysOnTop (true);
    scopeSplitOverlay.setVisible (false);
    addChildComponent (scopeSplitOverlay);

    oscilloscope.setAlwaysOnTop (true);
    oscilloscope.setParameterTree (&processor.treeState);
    addChildComponent (oscilloscope);
    processor.setOscilloscopeTarget (&oscilloscope);

    goniometer.setAlwaysOnTop (true);
    goniometer.setParameterTree (&processor.treeState);
    addChildComponent (goniometer);
    processor.setGoniometerTarget (&goniometer);

    spectrogram.setAlwaysOnTop (true);
    spectrogram.setParameterTree (&processor.treeState);
    addChildComponent (spectrogram);
    processor.setSpectrogramTarget (&spectrogram);
    applyColourRampsToMeters();

    // Default: scope on at load (summed stereo). Gon off. Spec on.
    oscButton.setToggleState (true, juce::dontSendNotification);
    oscilloscope.setEnabled (true);
    oscilloscope.setChannelMode (OscilloscopeComponent::ChannelMode::summedStereo);
    syncOscToolButtons();
    syncGonToolButtons();
    syncSpecToolButtons();
    specButton.setToggleState (true, juce::dontSendNotification);
    spectrogram.setEnabled (true);

    frequencyResponseComponent.onOptionBoxVisibilityChanged = [this]
    {
        syncExpandedOscOverlayStack();
    };

    if (auto* themes = menu.getThemeList())
    {
        themes->setProcessor (&processor);
        themes->addListener (this);
        refreshPresetNameDisplay();
    }

    if (auto* appearance = menu.getAppearanceComponent())
    {
        appearance->onThemeLiveChanged = [this]
        {
            applyThemeToChildComponents();
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

    applyThemeToChildComponents();
}

MainComponent::~MainComponent()
{
    colourRamps.removeChangeListener (this);
    processor.treeState.removeParameterListener ("METER_CHANNEL_MODE_ID", this);

    processor.getUndoManager().removeChangeListener (this);

    presetNameEditor.removeListener (this);

    if (auto* themes = menu.getThemeList())
        themes->removeListener (this);

    frequencyResponseComponent.removeMouseListener (this);
    processor.setFrequencyResponseComponent (nullptr);
    processor.setOscilloscopeTarget (nullptr);
    processor.setGoniometerTarget (nullptr);
    processor.setSpectrogramTarget (nullptr);

    menuToggleButton.setLookAndFeel (nullptr);
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
    uiRandomizeButton.setThemeResources (&sharedResources);

    frequencyResponseComponent.setThemeColors (&sharedResources);
    m_visualizer.setThemeColors (&sharedResources);
    oscilloscope.setThemeColors (&sharedResources);
    goniometer.setThemeColors (&sharedResources);
    spectrogram.setThemeColors (&sharedResources);
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

    oscExpandButton.setGlyph (oscExpanded ? OscToolButton::Glyph::Collapse
                                          : OscToolButton::Glyph::Expand);
    oscExpandButton.setTooltip (oscExpanded
                                    ? "Collapse oscilloscope back to the strip"
                                    : "Expand oscilloscope over the full graph");
    oscExpandButton.setToggleState (oscExpanded, juce::dontSendNotification);
}

void MainComponent::setOscExpanded (bool shouldExpand)
{
    if (oscExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setGonExpanded (false);
        setSpecExpanded (false);
    }

    oscExpanded = shouldExpand;
    // In Scope mode panes always use the rich (expanded) drawing style.
    oscilloscope.setExpanded (scopeModeEnabled || oscExpanded);
    syncOscToolButtons();
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::setGonExpanded (bool shouldExpand)
{
    if (gonExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setOscExpanded (false);
        setSpecExpanded (false);
    }

    gonExpanded = shouldExpand;
    goniometer.setExpanded (scopeModeEnabled || gonExpanded);
    syncGonToolButtons();
    syncScopeChromeButtonOpacity();
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::setSpecExpanded (bool shouldExpand)
{
    if (specExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setOscExpanded (false);
        setGonExpanded (false);
    }

    specExpanded = shouldExpand;
    spectrogram.setExpanded (scopeModeEnabled || specExpanded);
    syncSpecToolButtons();
    syncScopeChromeButtonOpacity();
    resized();
    syncExpandedOscOverlayStack();
}

void MainComponent::syncGonToolButtons()
{
    const bool on = gonButton.getToggleState();
    gonExpandButton.setVisible (on);
    gonExpandButton.setGlyph (gonExpanded ? OscToolButton::Glyph::Collapse
                                          : OscToolButton::Glyph::Expand);
    gonExpandButton.setTooltip (gonExpanded
                                    ? "Collapse goniometer back to the strip"
                                    : "Expand goniometer over the full graph");
    gonExpandButton.setToggleState (gonExpanded, juce::dontSendNotification);
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
    specSpeedUpButton.setVisible (on);
    specSpeedDownButton.setVisible (on);
    specExpandButton.setVisible (on);

    specExpandButton.setGlyph (specExpanded ? OscToolButton::Glyph::Collapse
                                            : OscToolButton::Glyph::Expand);
    specExpandButton.setTooltip (specExpanded
                                     ? "Collapse spectrogram back to the strip"
                                     : "Expand spectrogram over the full graph");
    specExpandButton.setToggleState (specExpanded, juce::dontSendNotification);
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
    const bool oscExp = oscExpanded && oscButton.getToggleState();
    const bool gonExp = gonExpanded && gonButton.getToggleState();
    const bool specExp = specExpanded && specButton.getToggleState();
    const bool expanded = oscExp || gonExp || specExp;
    oscilloscope.setAlwaysOnTop (! oscExp);
    goniometer.setAlwaysOnTop (! gonExp);
    spectrogram.setAlwaysOnTop (! specExp);
    oscDimmer.setAlwaysOnTop (! expanded);

    auto* box = frequencyResponseComponent.getOptionBoxMenu();
    const bool optionOpen = box != nullptr && box->isVisible();

    // Whenever the OptionBox is open, host it here so it can sit above the expanded scope.
    hostOptionBoxAboveExpandedOsc (optionOpen);

    raiseMenuSystemAboveWordmark();
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
        applyColourRampsToMeters();
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

void MainComponent::applyColourRampsToMeters()
{
    const auto* fftRamp = &colourRamps.get (ColourRampBank::Target::fftBars);
    const auto* specRamp = &colourRamps.get (ColourRampBank::Target::spectrogram);
    const auto* fillRamp = &colourRamps.get (ColourRampBank::Target::spectrumFill);

    m_visualizer.setBinOverlayColourRamp (fftRamp->isUsable() ? fftRamp : nullptr);
    m_visualizer.setSpectrumFillRamp (fillRamp->isUsable() ? fillRamp : nullptr);
    spectrogram.setCustomColourRamp (specRamp->isUsable() ? specRamp : nullptr);
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

    // Eco gates analyser/FFT via processor + Analyser flags — SPECTRUM_ANALYSER_ID
    // preference is left untouched so turning Eco off restores the previous setting.
    applyEcoMode (shouldEnable);

    if (notifyPrefs)
        editor.saveUiPrefs();
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

        oscButton.setToggleState (true, juce::dontSendNotification);
        oscilloscope.setEnabled (true);
        applyGoniometerActive (true);
        applySpectrogramActive (true);

        // Richer drawing in large panes (not the fullscreen overlay path).
        oscilloscope.setExpanded (true);
        goniometer.setExpanded (true);
        spectrogram.setExpanded (true);

        frequencyResponseComponent.setVisible (false);
        scopeSplitOverlay.setVisible (true);
    }
    else
    {
        oscilloscope.setExpanded (oscExpanded);
        goniometer.setExpanded (gonExpanded);
        spectrogram.setExpanded (specExpanded);
        frequencyResponseComponent.setVisible (true);
        scopeSplitOverlay.setVisible (false);
    }

    syncOscToolButtons();
    syncGonToolButtons();
    syncSpecToolButtons();
    resized();
    syncExpandedOscOverlayStack();
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

    if (notifyPrefs)
        editor.saveUiPrefs();
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
        editor.saveUiPrefs();
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
    menu.addSubMenu ("Copy settings from snapshot " + slotName + " to…", copyTo);

    juce::PopupMenu swapWith;
    for (int i = 0; i < EqProcessor::abSlotCount; ++i)
    {
        const auto other = static_cast<EqProcessor::AbSlot> (i);
        if (other == slot)
            continue;
        swapWith.addItem (20 + i, "Snapshot " + EqProcessor::abSlotName (other));
    }
    menu.addSubMenu ("Swap with…", swapWith);

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
    // Filled after launchAsynchronously — used to close the dropdown on apply/save/rename.
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
        safeThis->editor.saveUiPrefs();
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
    applyThemeToChildComponents();

    if (auto* appearance = menu.getAppearanceComponent())
    {
        appearance->updateAllComponents();
        appearance->repaintComponents();
        appearance->repaintParentComponent();
    }
}

void MainComponent::randomizeColourRamps()
{
    sharedResources.makeActive();
    const auto& c = sharedResources.sharedColors;
    const bool mask[3] = {
        c.randomizeRampFftBars,
        c.randomizeRampSpectrogram,
        c.randomizeRampSpectrumFill
    };
    colourRamps.randomizeRamps (c, mask);
    applyColourRampsToMeters();
}

void MainComponent::disableCustomColourRamps()
{
    colourRamps.disableAllCustomRamps();
    applyColourRampsToMeters();
}

void MainComponent::runDiceRandomize()
{
    const auto& c = sharedResources.sharedColors;
    const bool anyUi = c.randomizeFaceplateMod || c.randomizeGraphModule || c.randomizeMenuModule;
    const bool anyRamp = c.randomizeRampFftBars || c.randomizeRampSpectrogram || c.randomizeRampSpectrumFill;

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
    menu.addItem (5, "Spectrogram", true, scopes.randomizeRampSpectrogram);
    menu.addItem (6, "Spectrum Fill", true, scopes.randomizeRampSpectrumFill);
    menu.addSeparator();
    menu.addItem (7, "Disable custom ramps (use schemes)");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&uiRandomizeButton),
                        [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            auto& s = safe->sharedResources.sharedColors;
                            if (result == 1)      s.randomizeFaceplateMod = ! s.randomizeFaceplateMod;
                            else if (result == 2) s.randomizeGraphModule = ! s.randomizeGraphModule;
                            else if (result == 3) s.randomizeMenuModule = ! s.randomizeMenuModule;
                            else if (result == 4) s.randomizeRampFftBars = ! s.randomizeRampFftBars;
                            else if (result == 5) s.randomizeRampSpectrogram = ! s.randomizeRampSpectrogram;
                            else if (result == 6) s.randomizeRampSpectrumFill = ! s.randomizeRampSpectrumFill;
                            else if (result == 7) safe->disableCustomColourRamps();

                            if (result >= 1 && result <= 6)
                                safe->editor.saveUiPrefs();
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
}

void MainComponent::onPresetListChanged()
{
    // Appearance UI theme list changed — chrome EQ name is independent.
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
}

void MainComponent::layoutScopeModePanes (float scale)
{
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    auto graph = getLocalBounds();
    scopeSplitOverlay.setBounds (graph);
    scopeSplitOverlay.setVisible (true);
    scopeSplitOverlay.toFront (false);

    const int gap = px (3.0f);
    const int sx = juce::roundToInt ((float) graph.getWidth() * scopeSplitOverlay.getSplitX());
    const int sy = juce::roundToInt ((float) graph.getHeight() * scopeSplitOverlay.getSplitY());

    auto tl = juce::Rectangle<int> (graph.getX(), graph.getY(), sx, sy).reduced (gap);
    auto tr = juce::Rectangle<int> (graph.getX() + sx, graph.getY(), graph.getWidth() - sx, sy).reduced (gap);
    auto bl = juce::Rectangle<int> (graph.getX(), graph.getY() + sy, sx, graph.getHeight() - sy).reduced (gap);
    auto br = juce::Rectangle<int> (graph.getX() + sx, graph.getY() + sy,
                                    graph.getWidth() - sx, graph.getHeight() - sy).reduced (gap);

    const int toolH = px (22.0f);
    const int toolGap = px (2.0f);
    const int toolSize = juce::jmax (14, toolH - 2);

    // Top-left: Goniometer
    {
        auto pane = tl;
        auto tools = pane.removeFromBottom (toolH);
        goniometer.setBounds (pane);
        goniometer.setVisible (true);
        gonExpandButton.setVisible (true);
        gonExpandButton.setBounds (tools.removeFromLeft (toolSize).withSizeKeepingCentre (toolSize, toolSize));
    }

    // Top-right: Spectrum / FFT (visualizer)
    {
        m_visualizer.setBounds (tr);
        m_visualizer.setVisible (true);
    }

    // Bottom-left: Oscilloscope + tools
    {
        auto pane = bl;
        auto tools = pane.removeFromBottom (toolH);
        oscilloscope.setBounds (pane);
        oscilloscope.setVisible (true);

        oscZoomInButton.setVisible (true);
        oscZoomOutButton.setVisible (true);
        oscChannelModeButton.setVisible (true);
        oscExpandButton.setVisible (true);

        auto place = [&] (OscToolButton& b)
        {
            b.setBounds (tools.removeFromLeft (toolSize).withSizeKeepingCentre (toolSize, toolSize));
            tools.removeFromLeft (toolGap);
        };
        place (oscZoomInButton);
        place (oscZoomOutButton);
        place (oscChannelModeButton);
        place (oscExpandButton);
    }

    // Bottom-right: Spectrogram + tools
    {
        auto pane = br;
        auto tools = pane.removeFromBottom (toolH);
        spectrogram.setBounds (pane);
        spectrogram.setVisible (true);

        specSpeedUpButton.setVisible (true);
        specSpeedDownButton.setVisible (true);
        specExpandButton.setVisible (true);

        auto place = [&] (OscToolButton& b)
        {
            b.setBounds (tools.removeFromLeft (toolSize).withSizeKeepingCentre (toolSize, toolSize));
            tools.removeFromLeft (toolGap);
        };
        place (specSpeedUpButton);
        place (specSpeedDownButton);
        place (specExpandButton);
    }

    frequencyResponseComponent.setVisible (false);
    oscDimmer.setVisible (false);

    // Keep tool chrome above panes; divider above panes for grab, under Settings menu.
    goniometer.toFront (false);
    m_visualizer.toFront (false);
    oscilloscope.toFront (false);
    spectrogram.toFront (false);
    gonExpandButton.toFront (false);
    oscZoomInButton.toFront (false);
    oscZoomOutButton.toFront (false);
    oscChannelModeButton.toFront (false);
    oscExpandButton.toFront (false);
    specSpeedUpButton.toFront (false);
    specSpeedDownButton.toFront (false);
    specExpandButton.toFront (false);
    scopeSplitOverlay.toFront (false);
}

void MainComponent::layoutPresetChrome (float scale)
{
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

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

    // Compact: logo is hosted on this component at the top — sit directly under it.
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
    const int meterWidth = static_cast<int>(editorWidth * 0.015);
    const int meterHeight = static_cast<int>(editorHeight * 0.90);
    const int meterSpacing = static_cast<int>(meterWidth * 0.4f);
    const int centerY = (editorHeight - meterHeight) / 2;
    const int totalMeterGroupWidth = 2 * meterWidth + meterSpacing;
    const int padding = static_cast<int>(totalMeterGroupWidth * 0.4);
    const int xLeft = static_cast<int>(padding * 1.5);
    const int xRight = editorWidth - padding - totalMeterGroupWidth;
    const int meterY = centerY + px (20.0f);

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
    menuToggleButton.setBounds (area2.getRight() - settingsW, area2.getY(),
                                settingsW, settingsH);

    // Undo / Redo just left of Settings (hamburger), same size as ?.
    {
        const int historyY = area2.getY() + (settingsH - historySize) / 2;
        int hx = menuToggleButton.getX() - historyGap - historySize;
        redoButton.setBounds (hx, historyY, historySize, historySize);
        hx -= historyGap + historySize;
        undoButton.setBounds (hx, historyY, historySize, historySize);
    }

    // Top-left chrome: Bypass | A | B | C | D | UI | Dice.
    {
        constexpr int minimizeSize = 22;
        constexpr int minimizeMargin = 6;
        const int chromeH = px (28.0f);
        const int chromeY = px ((float) minimizeMargin);
        const int gap = px (4.0f);
        const int bypassW = px (62.0f);
        // Referencing slots — 40% smaller than the previous A/B chrome size.
        const int abW = px (28.0f * 0.6f);
        const int abH = px (28.0f * 0.6f);
        const int abY = chromeY + (chromeH - abH) / 2;
        const int uiW = px (28.0f);
        const int diceW = px (24.0f);
        const int toolsY = chromeY + (chromeH - abH) / 2;
        const int toolsH = abH;

        int x = minimizeMargin + minimizeSize + gap + px (2.0f);
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
        uiThemeButton.setBounds (x, toolsY, uiW, toolsH);
        x += uiW + gap;
        uiRandomizeButton.setBounds (x, toolsY, diceW, toolsH);
    }

    layoutPresetChrome (scale);

    // Eco: just right of Save; same W×H as L/R ↔ M/S meter-mode button.
    // OSC (smaller) sits beside Eco; Gon sits under OSC.
    // Scope strip fills toward Gon (or L/R) when OSC is on; Gon parks on the right.
    {
        const int chromeH = px (28.0f);
        const int chromeY = px (6.0f);
        const int gap = px (6.0f);
        const int ecoY = chromeY + (chromeH - meterModeH) / 2;
        ecoButton.setBounds (presetSaveButton.getRight() + gap, ecoY, meterModeW, meterModeH);

        const int oscW = juce::jmax (28, (meterModeW * 3) / 4);
        const int oscH = juce::jmax (16, (meterModeH * 3) / 4);
        const int oscY = chromeY + (chromeH - oscH) / 2;
        oscButton.setBounds (ecoButton.getRight() + gap, oscY, oscW, oscH);
        gonButton.setBounds (oscButton.getX(), oscButton.getBottom() + px (2.0f), oscW, oscH);
        specButton.setBounds (gonButton.getX(), gonButton.getBottom() + px (2.0f), oscW, oscH);

        if (scopeModeEnabled)
        {
            const bool anyExpanded = oscExpanded || gonExpanded || specExpanded;
            if (! anyExpanded)
            {
                layoutScopeModePanes (scale);
            }
            else
            {
                // One meter maximized over the graph; collapse returns to the quad.
                scopeSplitOverlay.setVisible (false);
                frequencyResponseComponent.setVisible (false);
                m_visualizer.setVisible (false);
                oscilloscope.setVisible (oscExpanded);
                goniometer.setVisible (gonExpanded);
                spectrogram.setVisible (specExpanded);
                oscDimmer.setBounds (getLocalBounds());
                oscDimmer.setVisible (true);
                if (oscExpanded)
                    oscilloscope.setBounds (getLocalBounds());
                if (gonExpanded)
                    goniometer.setBounds (getLocalBounds());
                if (specExpanded)
                    spectrogram.setBounds (getLocalBounds());
                syncOscToolButtons();
                syncGonToolButtons();
                syncSpecToolButtons();
            }
        }
        else
        {
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
        const int btnSize = juce::jmax (14, (scopeH - 3 * btnGap) / 4);
        const int scopeY = chromeY + px (10.0f);
        const int gonExpandSize = juce::jmax (14, btnSize);
        const int specBtnSize = juce::jmax (14, (scopeH - 2 * btnGap) / 3);

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
                oscDimmer.setBounds (getLocalBounds());
                oscDimmer.setVisible (true);
                oscilloscope.setBounds (frequencyResponseComponent.getBounds());
            }
            else
            {
                oscilloscope.setBounds (scopeLeft, scopeY, scopeW, scopeH);
            }

            oscilloscope.setVisible (true);

            oscZoomInButton.setBounds (btnX, scopeY, btnSize, btnSize);
            oscZoomOutButton.setBounds (btnX, scopeY + btnSize + btnGap, btnSize, btnSize);
            oscChannelModeButton.setBounds (btnX, scopeY + 2 * (btnSize + btnGap), btnSize, btnSize);
            oscExpandButton.setBounds (btnX, scopeY + 3 * (btnSize + btnGap), btnSize, btnSize);
        }
        else
        {
            oscilloscope.setVisible (false);
        }

        if (gonOn)
        {
            const int gonW = GoniometerComponent::kCompactWidthPx;
            const int gonX = meterChannelModeButton.getX() - gap - gonExpandSize - px (2.0f) - gonW;
            const int gonExpandX = gonX + gonW + px (2.0f);

            if (gonExpanded)
            {
                oscDimmer.setBounds (getLocalBounds());
                oscDimmer.setVisible (true);
                // Full graph area; correlation sits on the dark square's right edge
                // (raised above post meters in raiseMenuSystemAboveWordmark).
                goniometer.setBounds (frequencyResponseComponent.getBounds());
            }
            else
            {
                goniometer.setBounds (gonX, scopeY, gonW, scopeH);
            }

            goniometer.setVisible (true);
            gonExpandButton.setBounds (gonExpandX, scopeY, gonExpandSize, gonExpandSize);
        }
        else
        {
            goniometer.setVisible (false);
        }

        // Dimmer only while something is expanded.
        if (! oscExpanded && ! gonExpanded && ! specExpanded)
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
            const int toolColW = specBtnSize + px (2.0f);
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
                    oscDimmer.setBounds (getLocalBounds());
                    oscDimmer.setVisible (true);
                    spectrogram.setBounds (frequencyResponseComponent.getBounds());
                }
                else
                {
                    spectrogram.setBounds (blockX, specY, specW, specH);
                }

                spectrogram.setVisible (true);
                specSpeedUpButton.setBounds (btnX, specY, specBtnSize, specBtnSize);
                specSpeedDownButton.setBounds (btnX, specY + specBtnSize + btnGap, specBtnSize, specBtnSize);
                specExpandButton.setBounds (btnX, specY + 2 * (specBtnSize + btnGap), specBtnSize, specBtnSize);
            }
            else
            {
                spectrogram.setVisible (false);
            }
        }
        else
        {
            spectrogram.setVisible (false);
        }
        } // !scopeModeEnabled compact strips
    }

    // Keep menu content at design size and scale uniformly with the plugin.
    // AffineTransform::scale is relative to the parent origin, so compensate with a
    // translation and right-anchor so the visual right edge stays flush on resize.
    constexpr float designHeight = 850.0f;
    constexpr int designMenuW = 800; // 1200 * 2/3
    const int designMenuH = juce::roundToInt (designHeight / 1.9f);
    const int menuY = FrequencyResponseyOffset;
    const int visualMenuW = juce::roundToInt ((float) designMenuW * scale);
    const int menuX = juce::jmax (0, getWidth() - visualMenuW);

    menu.setBounds (menuX, menuY, designMenuW, designMenuH);
    menu.setTransform (juce::AffineTransform::scale (scale)
                           .followedBy (juce::AffineTransform::translation (
                               (float) menuX * (1.0f - scale),
                               (float) menuY * (1.0f - scale))));

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
    //   → OptionBox → Settings menu → Settings button
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

    // Side meters above graph / compact scope.
    verticalGradientMeterL.toFront (false);
    verticalGradientMeterR.toFront (false);
    verticalGradientMeterPostL.toFront (false);
    verticalGradientMeterPostR.toFront (false);

    // Expanded goniometer (incl. correlation strip) sits above the post meters so
    // the right-edge correlation meter is never covered.
    if (gonExp)
        goniometer.toFront (false);
    if (specExp)
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
    specSpeedUpButton.toFront (false);
    specSpeedDownButton.toFront (false);
    specExpandButton.toFront (false);
    if (scopeModeEnabled && ! oscExp && ! gonExp && ! specExp)
        scopeSplitOverlay.toFront (false);
    meterChannelModeButton.toFront (false);
    undoButton.toFront (false);
    redoButton.toFront (false);

    // OptionBox + menu must win over the expanded waveform.
    auto* box = frequencyResponseComponent.getOptionBoxMenu();
    if (box != nullptr && box->isVisible())
    {
        box->setAlwaysOnTop (true);
        box->toFront (false);
    }

    const bool menuOpen = menu.isVisible();
    menu.setAlwaysOnTop (menuOpen);
    if (menuOpen)
        menu.toFront (false);

    menuToggleButton.toFront (false);

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
