#include "MainComponent.h"
#include <JuceHeader.h>
#include "FrequencyResponseComponent.h"
#include "EqEditor.h"
#include "Visualizer/Analyser.h"
#include "ComboBoxLookAndFeel.h"

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
    menu(sharedResources, treeState, textButtonLookAndFeel)
{
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
    menuToggleButton.setColour(juce::TextButton::textColourOnId, juce::Colours::lightgrey.withAlpha(0.8f));
    menuToggleButton.setColour(juce::TextButton::textColourOffId, juce::Colours::lightgrey.withAlpha(0.8f));

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

    // Preset chrome: ◀ | editable name | ▼ | ▶ | Save — full EQ+appearance state.
    auto setupPresetNavButton = [this] (juce::TextButton& button, int delta, const juce::String& tip)
    {
        styleChromeButton (button);
        button.setTooltip (tip);
        button.setAlwaysOnTop (true);
        button.onClick = [this, delta] { cycleAppearancePreset (delta); };
        addAndMakeVisible (button);
    };

    setupPresetNavButton (presetPrevButton, -1, "Previous preset");
    setupPresetNavButton (presetNextButton, 1, "Next preset");

    stylePresetNameEditor();
    presetNameEditor.setText ("Default", juce::dontSendNotification);
    presetNameEditor.setTooltip ("Click to type a preset name. Enter or click away to rename.");
    presetNameEditor.setAlwaysOnTop (true);
    presetNameEditor.addListener (this);
    addAndMakeVisible (presetNameEditor);

    styleChromeButton (presetMenuButton);
    presetMenuButton.setTooltip ("Browse presets");
    presetMenuButton.setAlwaysOnTop (true);
    presetMenuButton.onClick = [this] { showPresetPopupMenu(); };
    addAndMakeVisible (presetMenuButton);

    styleChromeButton (presetSaveButton);
    presetSaveButton.setTooltip ("Save full EQ + appearance under the typed name");
    presetSaveButton.setAlwaysOnTop (true);
    presetSaveButton.onClick = [this] { saveCurrentAppearancePreset(); };
    addAndMakeVisible (presetSaveButton);

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

    // Eco — same Y as Bypass, X just right of Save. Disables analyser/FFT visuals only.
    styleChromeButton (ecoButton);
    ecoButton.setClickingTogglesState (true);
    ecoButton.setTooltip ("Eco - disables analyser and spectrum drawing to save CPU. Dynamic (D) and Spectral (S) still work.");
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

    oscDimmer.setInterceptsMouseClicks (false, false);
    oscDimmer.setAlwaysOnTop (true);
    oscDimmer.setVisible (false);
    addChildComponent (oscDimmer);

    oscilloscope.setAlwaysOnTop (true);
    oscilloscope.setParameterTree (&processor.treeState);
    addChildComponent (oscilloscope);
    processor.setOscilloscopeTarget (&oscilloscope);

    // Default: scope on at load (summed stereo).
    oscButton.setToggleState (true, juce::dontSendNotification);
    oscilloscope.setEnabled (true);
    oscilloscope.setChannelMode (OscilloscopeComponent::ChannelMode::summedStereo);
    syncOscToolButtons();

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

    addAndMakeVisible(menu);
    menu.setVisible(false);
    raiseMenuSystemAboveWordmark();
    menu.setInterceptsMouseClicks(true, true);

    frequencyResponseComponent.setInterceptsMouseClicks(true, true);

    addAndMakeVisible(verticalGradientMeterPostL);
    addAndMakeVisible(verticalGradientMeterPostR);

    styleChromeButton (meterChannelModeButton);
    meterChannelModeButton.setClickingTogglesState (false);
    meterChannelModeButton.setAlwaysOnTop (true);
    meterChannelModeButton.setTooltip ("Meter channel mode - L/R or Mid/Side (M/S)");
    meterChannelModeButton.onClick = [this] { toggleMeterChannelMode(); };
    addAndMakeVisible (meterChannelModeButton);
    syncMeterChannelModeButton();

    processor.treeState.addParameterListener ("METER_CHANNEL_MODE_ID", this);
}

MainComponent::~MainComponent()
{
    processor.treeState.removeParameterListener ("METER_CHANNEL_MODE_ID", this);

    processor.getUndoManager().removeChangeListener (this);

    presetNameEditor.removeListener (this);

    if (auto* themes = menu.getThemeList())
        themes->removeListener (this);

    frequencyResponseComponent.removeMouseListener (this);
    processor.setFrequencyResponseComponent (nullptr);
    processor.setOscilloscopeTarget (nullptr);

    menuToggleButton.setLookAndFeel (nullptr);
    bypassAttachment.reset();

    if (m_controls.getParentComponent() != nullptr)
        m_controls.getParentComponent()->removeChildComponent (&m_controls);

    if (m_visualizer.getParentComponent() != nullptr)
        m_visualizer.getParentComponent()->removeChildComponent (&m_visualizer);
}

void MainComponent::styleChromeButton (juce::TextButton& button)
{
    button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
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

    oscExpanded = shouldExpand;
    oscilloscope.setExpanded (oscExpanded);
    syncOscToolButtons();
    resized();
    syncExpandedOscOverlayStack();
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
    // Expanded scope: above graph, below OptionBox + Settings menu.
    // Compact strip: stay in the always-on-top chrome layer.
    const bool expanded = oscExpanded && oscButton.getToggleState();
    oscilloscope.setAlwaysOnTop (! expanded);
    oscDimmer.setAlwaysOnTop (! expanded);

    auto* box = frequencyResponseComponent.getOptionBoxMenu();
    const bool optionOpen = box != nullptr && box->isVisible();

    // Whenever the OptionBox is open, host it here so it can sit above the expanded scope.
    hostOptionBoxAboveExpandedOsc (optionOpen);

    raiseMenuSystemAboveWordmark();
}

void MainComponent::stylePresetNameEditor()
{
    presetNameEditor.setMultiLine (false);
    presetNameEditor.setReturnKeyStartsNewLine (false);
    presetNameEditor.setScrollbarsShown (false);
    presetNameEditor.setCaretVisible (true);
    presetNameEditor.setPopupMenuEnabled (true);
    presetNameEditor.setJustification (juce::Justification::centred);
    presetNameEditor.setFont (juce::FontOptions().withHeight (13.0f));
    presetNameEditor.setIndents (6, 2);
    presetNameEditor.setColour (juce::TextEditor::backgroundColourId, juce::Colour::fromRGBA (20, 18, 14, 210));
    presetNameEditor.setColour (juce::TextEditor::textColourId, juce::Colours::whitesmoke.withAlpha (0.88f));
    presetNameEditor.setColour (juce::TextEditor::outlineColourId, juce::Colour::fromRGBA (90, 75, 50, 200));
    presetNameEditor.setColour (juce::TextEditor::focusedOutlineColourId, juce::Colour::fromRGBA (140, 115, 70, 230));
    presetNameEditor.setColour (juce::TextEditor::highlightColourId, juce::Colour::fromRGBA (180, 150, 55, 160));
    presetNameEditor.setColour (juce::CaretComponent::caretColourId, juce::Colours::whitesmoke);
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
}

void MainComponent::applyEcoMode (bool shouldEnable)
{
    processor.setEcoMode (shouldEnable);
    ecoEnabled = shouldEnable;
    ecoButton.setToggleState (shouldEnable, juce::dontSendNotification);

    // Force Graph layers to refresh visibility immediately.
    m_visualizer.repaint();
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

    juce::String name = "No Preset";

    if (auto* themes = menu.getThemeList())
    {
        if (themes->getNumRows() <= 0)
            name = "No Preset";
        else
        {
            const auto selected = themes->getSelectedPresetName();
            name = selected.isNotEmpty() ? selected : "Default";
        }
    }

    refreshingPresetName = true;
    presetNameEditor.setText (name, juce::dontSendNotification);
    refreshingPresetName = false;
}

void MainComponent::commitPresetNameEdit()
{
    if (refreshingPresetName)
        return;

    auto* themes = menu.getThemeList();
    if (themes == nullptr)
        return;

    const auto typed = presetNameEditor.getText().trim();
    if (typed.isEmpty())
    {
        refreshPresetNameDisplay();
        return;
    }

    const int selected = themes->getSelectedRow();
    // Rename current user preset; for Default / none the typed name is pending for Save.
    if (selected > 0)
        themes->renamePreset (selected, typed);
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

void MainComponent::showPresetPopupMenu()
{
    auto* themes = menu.getThemeList();
    if (themes == nullptr || themes->getNumRows() <= 0)
        return;

    juce::PopupMenu popup;
    popup.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const int selected = themes->getSelectedRow();

    for (int i = 0; i < themes->getNumRows(); ++i)
    {
        const auto name = themes->getPresetName (i);
        popup.addItem (i + 1, name.isNotEmpty() ? name : ("Preset " + juce::String (i + 1)),
                       true, i == selected);
    }

    popup.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&presetMenuButton),
                         [this] (int result)
                         {
                             if (result <= 0)
                                 return;

                             if (auto* list = menu.getThemeList())
                                 list->applyPreset (result - 1);
                         });
}

void MainComponent::cycleAppearancePreset (int delta)
{
    auto* themes = menu.getThemeList();
    if (themes == nullptr)
        return;

    const int count = themes->getNumRows();
    if (count <= 0 || delta == 0)
        return;

    int index = themes->getSelectedRow();
    if (index < 0)
        index = 0;
    else
        index = (index + delta + count) % count;

    themes->applyPreset (index);
}

void MainComponent::saveCurrentAppearancePreset()
{
    auto* themes = menu.getThemeList();
    if (themes == nullptr)
        return;

    themes->saveOrUpdateWithName (presetNameEditor.getText());
    refreshPresetNameDisplay();
}

void MainComponent::onPresetApplied (const Theme&)
{
    refreshPresetNameDisplay();
    syncAbButtons();
}

void MainComponent::onPresetListChanged()
{
    refreshPresetNameDisplay();
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
    g.fillAll(juce::Colour(10, 10, 10));
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

    // Top-left chrome: clear of FRC minimize (6,6 / 22px), then Bypass | A | B | C | D.
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
    }

    layoutPresetChrome (scale);

    // Eco: just right of Save; same W×H as L/R ↔ M/S meter-mode button.
    // OSC (smaller) sits beside Eco. Scope window fills toward the L/R button when on.
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

        const bool oscOn = oscButton.getToggleState();
        syncOscToolButtons();

        if (oscOn)
        {
            const int scopeH = OscilloscopeComponent::kWindowHeightPx;
            const int btnGap = px (2.0f);
            // Four square buttons share the compact scope height: + − ST/L/R expand.
            const int btnSize = juce::jmax (14, (scopeH - 3 * btnGap) / 4);
            // Compact strip + buttons sit 10px below chrome top.
            const int scopeY = chromeY + px (10.0f);
            const int scopeLeft = oscButton.getRight() + gap;
            const int scopeRightLimit = meterChannelModeButton.getX() - gap - btnSize - px (4.0f);
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
                oscDimmer.setVisible (false);
                oscilloscope.setBounds (scopeLeft, scopeY, scopeW, scopeH);
            }

            oscilloscope.setVisible (true);

            // Keep tool buttons at the compact strip position so they stay clickable.
            oscZoomInButton.setBounds (btnX, scopeY, btnSize, btnSize);
            oscZoomOutButton.setBounds (btnX, scopeY + btnSize + btnGap, btnSize, btnSize);
            oscChannelModeButton.setBounds (btnX, scopeY + 2 * (btnSize + btnGap), btnSize, btnSize);
            oscExpandButton.setBounds (btnX, scopeY + 3 * (btnSize + btnGap), btnSize, btnSize);
        }
        else
        {
            oscDimmer.setVisible (false);
            oscilloscope.setVisible (false);
        }
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
    //   graph → expanded osc/dimmer (not always-on-top) → chrome / zoom buttons
    //   → OptionBox → Settings menu → Settings button
    const bool expanded = oscExpanded && oscButton.getToggleState() && oscilloscope.isVisible();

    if (hostedWordmark != nullptr && hostedWordmark->getParentComponent() == this)
        hostedWordmark->toFront (false);

    // Graph peers first (under an expanded scope).
    m_visualizer.toFront (false);
    frequencyResponseComponent.toFront (false);

    if (expanded)
    {
        oscDimmer.toFront (false);
        oscilloscope.toFront (false);
    }

    presetPrevButton.toFront (false);
    presetNameEditor.toFront (false);
    presetMenuButton.toFront (false);
    presetNextButton.toFront (false);
    presetSaveButton.toFront (false);
    bypassButton.toFront (false);
    slotAButton.toFront (false);
    slotBButton.toFront (false);
    slotCButton.toFront (false);
    slotDButton.toFront (false);
    ecoButton.toFront (false);
    oscButton.toFront (false);

    if (! expanded)
    {
        oscDimmer.toFront (false);
        oscilloscope.toFront (false);
    }

    oscZoomInButton.toFront (false);
    oscZoomOutButton.toFront (false);
    oscChannelModeButton.toFront (false);
    oscExpandButton.toFront (false);
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
