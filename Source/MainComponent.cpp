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
#include <algorithm>
#include <cmath>

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
    menuToggleButton.onClick = [this] {
        const bool shouldShowMenu = !menu.isVisible();
        menu.setVisible(shouldShowMenu);

        if (shouldShowMenu)
        {
            layoutSettingsMenu();
            // Relayout so expanded 3D parks left of Settings (pin left, shrink from right).
            resized();
            syncExpandedOscOverlayStack();
            menu.setInterceptsMouseClicks(true, true);
            frequencyResponseComponent.setInterceptsMouseClicks(false, false);
        }
        else
        {
            resized();
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
    spec3DButton.setTooltip ("3D spectrogram - orbit (drag), pan (Shift+drag), zoom (wheel). Double-click resets. Expanded/Scope only.");
    specSpeedUpButton.setAlwaysOnTop (true);
    specSpeedDownButton.setAlwaysOnTop (true);
    specExpandButton.setAlwaysOnTop (true);
    spec3DButton.setAlwaysOnTop (true);
    specSpeedUpButton.onClick = [this] { spectrogram.speedUp(); };
    specSpeedDownButton.onClick = [this] { spectrogram.speedDown(); };
    specExpandButton.onClick = [this] { setSpecExpanded (! specExpanded); };
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
        menu.addSeparator();
        menu.addItem (4, "Reset camera");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&spec3DButton),
            [safe = juce::Component::SafePointer<MainComponent> (this)] (int result)
            {
                if (safe == nullptr || result <= 0)
                    return;
                if (result == 4)
                {
                    safe->spectrogram3D.resetCamera();
                    return;
                }
                safe->setSpec3DMeshQuality (
                    result == 1 ? Spectrogram3DComponent::MeshQuality::low
                                : (result == 3 ? Spectrogram3DComponent::MeshQuality::high
                                               : Spectrogram3DComponent::MeshQuality::medium),
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

    goniometer.setAlwaysOnTop (true);
    goniometer.setParameterTree (&processor.treeState);
    addChildComponent (goniometer);
    processor.setGoniometerTarget (&goniometer);

    spectrogram.setAlwaysOnTop (true);
    spectrogram.setParameterTree (&processor.treeState);
    addChildComponent (spectrogram);
    processor.setSpectrogramTarget (&spectrogram);

    spectrogram3D.setDataSource (&spectrogram);
    spectrogram3D.setAlwaysOnTop (false);
    spectrogram3D.onEscape = [this] { collapseAnyExpandedScope(); };
    spectrogram3D.onDefaultViewChanged = [this] { editor.saveUiPrefs(); };
    spectrogram3D.onUserResized = [this]
    {
        spec3DPreferredW = spectrogram3D.getWidth();
        spec3DPreferredH = spectrogram3D.getHeight();
        spec3DPreferredX = spectrogram3D.getX();
        spec3DPreferredY = spectrogram3D.getY();
        spec3DBoundsCustom = true;
    };
    spectrogram3D.onUserMoved = [this]
    {
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
    wireScopeMenu (levelMeterIn, ScopeModuleId::levelIn);
    wireScopeMenu (levelMeterOut, ScopeModuleId::levelOut);
    wireScopeMenu (loudnessMeter, ScopeModuleId::loudness);
    wireScopeMenu (stereogram, ScopeModuleId::stereogram);
    wireScopeMenu (histogram, ScopeModuleId::histogram);
    wireScopeMenu (oscilloscope, ScopeModuleId::oscilloscope);
    wireScopeMenu (goniometer, ScopeModuleId::goniometer);
    wireScopeMenu (spectrogram, ScopeModuleId::spectrogram);
    wireScopeMenu (m_visualizer, ScopeModuleId::spectrum);

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

    applyThemeToChildComponents();
}

MainComponent::~MainComponent()
{
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

    oscExpandButton.setGlyph (oscExpanded ? OscToolButton::Glyph::Collapse
                                          : OscToolButton::Glyph::Expand);
    oscExpandButton.setTooltip (oscExpanded
                                    ? "Collapse oscilloscope back to the strip"
                                    : "Expand oscilloscope over the full graph");
    oscExpandButton.setToggleState (oscExpanded, juce::dontSendNotification);
}

void MainComponent::setOscExpanded (bool shouldExpand, bool notifyPrefs)
{
    if (oscExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setGonExpanded (false, false);
        setSpecExpanded (false, false);
    }

    oscExpanded = shouldExpand;
    // In Scope mode panes always use the rich (expanded) drawing style.
    oscilloscope.setExpanded (scopeModeEnabled || oscExpanded);
    syncOscToolButtons();
    resized();
    syncExpandedOscOverlayStack();
    if (shouldExpand)
        grabKeyboardFocus();
    if (notifyPrefs)
        editor.saveUiPrefs();
}

void MainComponent::setGonExpanded (bool shouldExpand, bool notifyPrefs)
{
    if (gonExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setOscExpanded (false, false);
        setSpecExpanded (false, false);
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
        editor.saveUiPrefs();
}

void MainComponent::setSpecExpanded (bool shouldExpand, bool notifyPrefs)
{
    if (specExpanded == shouldExpand)
        return;

    if (shouldExpand)
    {
        setOscExpanded (false, false);
        setGonExpanded (false, false);
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
        editor.saveUiPrefs();
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
    // 3D toggle stays visible whenever Spec is on (preference applies on expand / Scope).
    specSpeedUpButton.setVisible (on);
    specSpeedDownButton.setVisible (on);
    specExpandButton.setVisible (on);
    spec3DButton.setVisible (on);
    spec3DButton.setToggleState (
        scopeModeEnabled ? isScopeModuleEnabled (ScopeModuleId::spectrogram3D) : spec3DEnabled,
        juce::dontSendNotification);
    spec3DButton.setThemeResources (&sharedResources);
    spec3DButton.setTooltip (
        (specExpanded || scopeModeEnabled)
            ? "3D spectrogram - orbit (drag), pan (Shift+drag), zoom (wheel). Double-click resets. Right-click: mesh quality."
            : "3D spectrogram (applies when expanded or in Scope). Right-click: mesh quality.");

    specExpandButton.setGlyph (specExpanded ? OscToolButton::Glyph::Collapse
                                            : OscToolButton::Glyph::Expand);
    specExpandButton.setTooltip (specExpanded
                                     ? "Collapse spectrogram back to the strip"
                                     : "Expand spectrogram over the full graph");
    specExpandButton.setToggleState (specExpanded, juce::dontSendNotification);
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
    // OpenGL peer is inset inside spectrogram3D's framed window — keep tools / Settings
    // outside the GL host rect (native HWND ignores JUCE z-order).
    auto area = getExpandedScopeContentBounds();
    if (area.isEmpty())
        area = getLocalBounds();

    area.setTop (juce::jmax (area.getY(), getTopChromeClearY()));

    // Match / Mod / P / Help / SideCheck sit above the piano — keep GL clear of that row.
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

    const int toolW = btnSize + 8;
    if (area.getWidth() <= toolW + 80 || area.getHeight() <= btnSize + 40)
    {
        spectrogram.setBounds (area);
        spectrogram3D.setBounds (area);
        return;
    }

    auto toolCol = area.removeFromRight (toolW);

    // Restore the user's preferred size/position when it fits; otherwise fill.
    int w = area.getWidth();
    int h = area.getHeight();
    int frameX = area.getX();
    int frameY = area.getY();
    if (spec3DBoundsCustom && spec3DPreferredW > 0 && spec3DPreferredH > 0)
    {
        w = juce::jlimit (220, area.getWidth(), spec3DPreferredW);
        h = juce::jlimit (160, area.getHeight(), spec3DPreferredH);
        frameX = juce::jlimit (area.getX(), area.getRight() - w, spec3DPreferredX);
        frameY = juce::jlimit (area.getY(), area.getBottom() - h, spec3DPreferredY);
    }

    const auto placed = juce::Rectangle<int> (frameX, frameY, w, h);
    const bool show3D = spec3DEnabled && specButton.getToggleState();
    spectrogram3D.setChromeMode (Spectrogram3DComponent::ChromeMode::floating);
    spectrogram3D.setResizeLimits (area.getWidth(), area.getHeight());
    spectrogram3D.setMovementBounds (area);
    spectrogram3D.setBounds (placed);

    // 2D fills the window; when 3D is on it tracks the inner frame under the GL host.
    const int pad = show3D ? spectrogram3D.getShadowPad() : 0;
    spectrogram.setBounds (pad > 0 ? placed.reduced (pad) : placed);

    // Tools sit just to the right of the framed window (not over GL).
    const int toolX = placed.getRight() + 2;
    int y = placed.getY() + juce::jmax (4, pad) + 4;
    const int x = toolX + juce::jmax (0, (toolCol.getRight() - toolX - btnSize) / 2);
    auto placeTool = [&] (OscToolButton& b)
    {
        b.setVisible (true);
        b.setBounds (x, y, btnSize, btnSize);
        y += btnSize + btnGap;
        b.toFront (false);
    };
    placeTool (specSpeedUpButton);
    placeTool (specSpeedDownButton);
    placeTool (specExpandButton);
    placeTool (spec3DButton);
    raiseSpecToolButtons();
}

bool MainComponent::collapseAnyExpandedScope()
{
    bool collapsed = false;
    if (specExpanded) { setSpecExpanded (false); collapsed = true; }
    if (oscExpanded)  { setOscExpanded (false);  collapsed = true; }
    if (gonExpanded)  { setGonExpanded (false);  collapsed = true; }
    return collapsed;
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && collapseAnyExpandedScope())
        return true;
    return false;
}

void MainComponent::syncSpec3DPresentation()
{
    const bool has3DModule = scopeModeEnabled && isScopeModuleEnabled (ScopeModuleId::spectrogram3D);
    const bool hasSpecModule = scopeModeEnabled && isScopeModuleEnabled (ScopeModuleId::spectrogram);
    // Scope: 3D only via its own module. Expanded (non-Scope): cube flag on Spec expand.
    const bool show3D = (scopeModeEnabled && has3DModule)
                        || (! scopeModeEnabled && specExpanded && spec3DEnabled
                            && specButton.getToggleState());

    spectrogram3D.setAlwaysOnTop (false);
    spectrogram3D.setThemeColors (&sharedResources);
    spectrogram3D.setChromeMode (scopeModeEnabled && show3D
                                     ? Spectrogram3DComponent::ChromeMode::docked
                                     : Spectrogram3DComponent::ChromeMode::floating);
    spectrogram3D.setActive (show3D);

    // Mesh history comes from the 2D spectrogram feeder — keep analysing whenever 3D is up.
    if (show3D)
        spectrogram.setEnabled (true);

    const bool show2D = (! scopeModeEnabled && specButton.getToggleState() && ! show3D)
                        || (scopeModeEnabled && hasSpecModule)
                        || (! scopeModeEnabled && specButton.getToggleState() && specExpanded && ! spec3DEnabled);
    // Expanded non-Scope with 3D on: hide 2D under the floating 3D frame.
    const bool show2DFinal = scopeModeEnabled
                                 ? hasSpecModule
                                 : (specButton.getToggleState() && ! (specExpanded && spec3DEnabled));

    spectrogram.setVisible (show2DFinal);
    spectrogram.setInterceptsMouseClicks (show2DFinal && (specExpanded || scopeModeEnabled), true);
    juce::ignoreUnused (show2D);

    if (show3D)
    {
        spectrogram3D.setInterceptsMouseClicks (true, true);
        spectrogram3D.toFront (false);
        raiseSpecToolButtons();
        if (menu.isVisible())
            menu.toFront (false);
        menuToggleButton.toFront (false);
        bypassButton.toFront (false);
        undoButton.toFront (false);
        redoButton.toFront (false);
    }
    else if (show2DFinal)
    {
        raiseSpecToolButtons();
    }
}

void MainComponent::setSpec3DMode (bool shouldEnable, bool notifyPrefs)
{
    if (spec3DEnabled == shouldEnable && ! scopeModeEnabled)
    {
        syncSpec3DPresentation();
        return;
    }

    spec3DEnabled = shouldEnable;

    // Settings / cube outside Scope: expanded overlay preference.
    // Inside Scope: also toggle the independent Spectrogram 3D module.
    if (scopeModeEnabled)
        setScopeModuleEnabled (ScopeModuleId::spectrogram3D, shouldEnable, false);

    syncSpecToolButtons();
    resized();
    syncExpandedOscOverlayStack();
    if (notifyPrefs)
        editor.saveUiPrefs();
}

void MainComponent::setSpec3DMeshQuality (Spectrogram3DComponent::MeshQuality q, bool notifyPrefs)
{
    spectrogram3D.setMeshQuality (q);
    if (notifyPrefs)
        editor.saveUiPrefs();
}

Spectrogram3DComponent::MeshQuality MainComponent::getSpec3DMeshQuality() const noexcept
{
    return spectrogram3D.getMeshQuality();
}

void MainComponent::setSpec3DMultisampling (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setMultisamplingEnabled (shouldEnable);
    if (notifyPrefs)
        editor.saveUiPrefs();
}

bool MainComponent::isSpec3DMultisampling() const noexcept
{
    return spectrogram3D.isMultisamplingEnabled();
}

void MainComponent::setSpec3DTransparentBackground (bool shouldEnable, bool notifyPrefs)
{
    spectrogram3D.setTransparentBackground (shouldEnable);
    if (notifyPrefs)
        editor.saveUiPrefs();
}

bool MainComponent::isSpec3DTransparentBackground() const noexcept
{
    return spectrogram3D.isTransparentBackground();
}

void MainComponent::setSpec3DMeshHeight (float heightWorld, bool notifyPrefs)
{
    spectrogram3D.setMeshHeight (heightWorld);
    if (notifyPrefs)
        editor.saveUiPrefs();
}

float MainComponent::getSpec3DMeshHeight() const noexcept
{
    return spectrogram3D.getMeshHeight();
}

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

    spectrogram3D.setChromeMode (Spectrogram3DComponent::ChromeMode::docked);
    spectrogram3D.setResizeLimits (placeArea.getWidth(), placeArea.getHeight());
    spectrogram3D.setBounds (placeArea);

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
        placeOverlayTool (spec3DButton);
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
    // Scope quad: never always-on-top — post meters must stay above the BR spectrogram.
    const bool oscExp = oscExpanded && oscButton.getToggleState();
    const bool gonExp = gonExpanded && gonButton.getToggleState();
    const bool specExp = specExpanded && specButton.getToggleState();
    const bool expanded = oscExp || gonExp || specExp;
    const bool compactChrome = ! scopeModeEnabled;
    oscilloscope.setAlwaysOnTop (compactChrome && ! oscExp);
    goniometer.setAlwaysOnTop (compactChrome && ! gonExp);
    spectrogram.setAlwaysOnTop (compactChrome && ! specExp);
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
        editor.saveUiPrefs();
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
    // Inline colour snapshot only — never writes RampPresetStore / UI theme presets.
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
        editor.saveUiPrefs();
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
        editor.saveUiPrefs();
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
        editor.saveUiPrefs();
}

bool MainComponent::isScopeModuleEnabled (ScopeModuleId id) const noexcept
{
    return std::find (scopeEnabledOrder.begin(), scopeEnabledOrder.end(), id) != scopeEnabledOrder.end();
}

void MainComponent::showScopeModuleContextMenu (ScopeModuleId id, juce::Component* anchor)
{
    if (! scopeModeEnabled || anchor == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    const int removeId = 900;
    const int resetIntegId = 10;
    const int tapInId = 20;
    const int tapOutId = 21;
    const int oscRedrawId = 30;
    bool hasExtras = false;

    switch (id)
    {
        case ScopeModuleId::loudness:
        case ScopeModuleId::histogram:
            menu.addItem (resetIntegId, "Reset Integrated");
            hasExtras = true;
            break;

        case ScopeModuleId::levelIn:
        case ScopeModuleId::levelOut:
        {
            auto& meter = (id == ScopeModuleId::levelOut) ? levelMeterOut : levelMeterIn;
            menu.addSectionHeader ("Level Meter Tap");
            menu.addItem (tapInId, "Input", true, meter.getTap() == ScopeLevelMeterModule::Tap::input);
            menu.addItem (tapOutId, "Output", true, meter.getTap() == ScopeLevelMeterModule::Tap::output);
            hasExtras = true;
            break;
        }

        case ScopeModuleId::oscilloscope:
            menu.addItem (oscRedrawId, "Redraw in place", true, ! oscilloscope.isScrollMode());
            hasExtras = true;
            break;

        default:
            break;
    }

    if (hasExtras)
        menu.addSeparator();

    const bool canRemove = scopeEnabledOrder.size() > 1;
    menu.addItem (removeId, "Remove Module", canRemove);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (anchor),
                        [safe = juce::Component::SafePointer<MainComponent> (this), id,
                         removeId, resetIntegId, tapInId, tapOutId, oscRedrawId] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            if (result == removeId)
                            {
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

    editor.saveUiPrefs();
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
        editor.saveUiPrefs();
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
        // refreshAfterRandomize updates Menu Slider Fill + glow (updateAllComponents alone does not).
        appearance->refreshAfterRandomize();
    }

    persistSessionUiTheme();
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
}

void MainComponent::restoreSessionUiThemeIfAny()
{
    juce::ValueTree rampTree;
    if (! processor.tryRestoreSessionUiTheme (sharedResources.sharedColors, rampTree))
        return;

    colourRamps.applyFromValueTree (rampTree, false);
    sharedResources.makeActive();

    // Menu/Appearance were constructed before restore; refresh cached colour widgets.
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
    menu.addItem (8, "Disable custom ramps (use schemes)");

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
                            else if (result == 6) s.randomizeRampSpectrogram3D = ! s.randomizeRampSpectrogram3D;
                            else if (result == 7) s.randomizeRampSpectrumFill = ! s.randomizeRampSpectrumFill;
                            else if (result == 8) safe->disableCustomColourRamps();

                            if (result >= 1 && result <= 7)
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
    persistSessionUiTheme();
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
    main.editor.saveUiPrefs();
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

    // Strip: insertion index 0…N from pointer X (midpoints between pane centres).
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
        main.editor.saveUiPrefs();
        updateHoverCursor (e.getPosition());
        repaint();
        return;
    }

    if (resizingColumn >= 0)
    {
        resizingColumn = -1;
        main.editor.saveUiPrefs();
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

    // Hover / active edges — same outline colour in tiled and strip.
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

void MainComponent::placeScopePane (ScopeModuleId moduleId, juce::Rectangle<int> pane,
                                    int toolH, int toolSize, int toolGap)
{
    // Tool buttons overlay the pane bottom — module keeps the full tile height.
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
            // Always 2D — Spectrogram 3D is a separate selectable Scope module.
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
                // Cube lives on the 3D module pane when that module is enabled.
                if (! isScopeModuleEnabled (ScopeModuleId::spectrogram3D))
                    placeOverlayTool (spec3DButton, row);
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
        // Edge-to-edge strip — Scope / Settings / Arrange overlay the panes.
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
        menuToggleButton.toFront (false);

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

    // Scope mode: clear the center for meters. DSP chrome (Bypass / A–D / Eco) follows Pre/Post.
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
    // Keep meters in the plot area — never grow into / over the piano strip.
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

    // Scope mode uses Level Meter modules only — hide the default edge meters.
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
    // Scope strip: Bypass / A–D / Eco hidden; UI + Dice overlay panes (no stripTop).
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

            // Square UI / Dice / Arrange — same size & pad as Scope button (bottom-left).
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
    // Strip: Eco hidden; Arrange stays (line icon → click for grid). Tiled: Arrange beside Eco.
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

            const bool anyExpanded = oscExpanded || gonExpanded || specExpanded;
            if (! anyExpanded)
            {
                layoutScopeModePanes (scale);
            }
            else
            {
                // One meter maximized over the graph; collapse returns to the quad.
                scopeSplitOverlay.setVisible (false);
                scopeArrangeOverlay.setVisible (false);
                frequencyResponseComponent.setVisible (false);
                m_visualizer.setVisible (false);
                oscilloscope.setVisible (oscExpanded);
                goniometer.setVisible (gonExpanded);
                spectrogram.setVisible (specExpanded);
                oscDimmer.setBounds (getLocalBounds());
                oscDimmer.setVisible (true);
                const auto expandBounds = getExpandedScopeContentBounds().isEmpty()
                                              ? getLocalBounds()
                                              : getExpandedScopeContentBounds();
                if (oscExpanded)
                    oscilloscope.setBounds (expandBounds);
                if (gonExpanded)
                    goniometer.setBounds (expandBounds);
                syncOscToolButtons();
                syncGonToolButtons();
                syncSpecToolButtons();
                if (specExpanded)
                {
                    const int btnGap = px (2.0f);
                    const int btnSize = juce::jmax (14, (OscilloscopeComponent::kWindowHeightPx - 3 * btnGap) / 4);
                    layoutExpandedSpectrogramWithTools (btnSize, btnGap);
                    syncSpec3DPresentation();
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
                oscDimmer.setBounds (getLocalBounds());
                oscDimmer.setVisible (true);
                oscilloscope.setBounds (getExpandedScopeContentBounds());
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
                // Plot area only (above piano); correlation on the dark square's right edge.
                goniometer.setBounds (getExpandedScopeContentBounds());
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
                    oscDimmer.setBounds (getLocalBounds());
                    oscDimmer.setVisible (true);
                    layoutExpandedSpectrogramWithTools (btnSize, btnGap);
                }
                else
                {
                    spectrogram.setBounds (blockX, specY, specW, specH);
                    spectrogram3D.setBounds (spectrogram.getBounds());
                    // Compact strip: tools sit beside the view (never over GL).
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
            spectrogram.setVisible (false);
        }
        } // !scopeModeEnabled compact strips
    }

    // Settings panel: freely movable/resizable; content stays at design size (viewport scrolls).
    layoutSettingsMenu();

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
    const int contentH = Menu::kContentHeight + Menu::kDragBarHeight;
    const int parentW = getWidth();
    const int parentH = getHeight();
    if (parentW <= 0 || parentH <= 0)
        return;

    if (settingsMenuBounds.isEmpty() || ! settingsMenuBoundsFromUser)
    {
        // Default: right-anchored, design content size (clipped if the host is smaller).
        constexpr int topY = 0;
        const int w = juce::jmin (contentW, parentW);
        const int h = juce::jmin (contentH, juce::jmax (140, parentH - topY));
        const int x = juce::jmax (0, parentW - w);
        const int y = juce::jlimit (0, juce::jmax (0, parentH - h), topY);
        settingsMenuBounds = { x, y, w, h };
    }

    auto b = settingsMenuBounds;
    b.setWidth (juce::jlimit (200, parentW, b.getWidth()));
    b.setHeight (juce::jlimit (140, parentH, b.getHeight()));
    b.setX (juce::jlimit (0, juce::jmax (0, parentW - b.getWidth()), b.getX()));
    b.setY (juce::jlimit (0, juce::jmax (0, parentH - b.getHeight()), b.getY()));

    updatingSettingsMenuBounds = true;
    menu.setBounds (b);
    updatingSettingsMenuBounds = false;
    settingsMenuBounds = menu.getBounds();
}

void MainComponent::componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized)
{
    juce::ignoreUnused (wasMoved, wasResized);
    if (updatingSettingsMenuBounds || &component != &menu)
        return;

    settingsMenuBounds = menu.getBounds();
    settingsMenuBoundsFromUser = true;

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

    // GL last among peers that share its inset bounds — chrome / tools stay outside those
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
    if (menuOpen)
        menu.toFront (false);

    menuToggleButton.toFront (false);

    // Strip overlays must sit above pane modules (same stack as Settings).
    if (scopeModeEnabled && scopeStripLayout)
    {
        uiThemeButton.toFront (false);
        uiRandomizeButton.toFront (false);
        menuToggleButton.toFront (false);
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
