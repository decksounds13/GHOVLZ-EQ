#include "GradientStripEditor.h"
#include "RampColorPickerPanel.h"
#include "RampPresetPicker.h"
#include "ColourRampBank.h"
#include "../ComboBoxLookAndFeel.h"
#include "../PresetOverwriteConfirm.h"
#include "../GraphOverlayButtonLookAndFeel.h"
#include "../MainComponent.h"

namespace
{
    constexpr float kStripH = 24.0f;
    constexpr float kArrowH = 7.0f;
    constexpr float kArrowHalf = 3.2f;
    constexpr int kToolbarRowH = 26;
    constexpr int kToolbarGap = 4;
    constexpr int kToolbarH = kToolbarRowH * 2 + kToolbarGap; // two rows: controls + preset/sample
    constexpr int kStripBlockH = 36;

    bool rampsMatchVisually (const GradientRamp& a, const GradientRamp& b) noexcept
    {
        if (a.stops.size() != b.stops.size() || a.stops.size() < 2)
            return false;

        for (size_t i = 0; i < a.stops.size(); ++i)
        {
            if (std::abs (a.stops[i].position - b.stops[i].position) > 0.03f)
                return false;

            const auto ca = a.stops[i].colour;
            const auto cb = b.stops[i].colour;
            if (std::abs ((int) ca.getRed() - (int) cb.getRed()) > 4
                || std::abs ((int) ca.getGreen() - (int) cb.getGreen()) > 4
                || std::abs ((int) ca.getBlue() - (int) cb.getBlue()) > 4)
                return false;
        }

        return true;
    }

    int textButtonWidth (const juce::String& text, float uiScale, int minW = 44)
    {
        const juce::Font f (juce::FontOptions (12.0f * uiScale));
        const float textW = juce::GlyphArrangement::getStringWidth (f, text);
        return juce::jmax (juce::roundToInt ((float) minW * uiScale),
                           juce::roundToInt (textW + 14.0f * uiScale));
    }
}

GradientStripEditor::DiceButton::DiceButton()
    : juce::Button ({})
{
    setClickingTogglesState (false);
    setTooltip ("Randomize this gradient (Appearance H/S/V limits). Right-click: Ordered / Standard.");
}

void GradientStripEditor::DiceButton::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        if (onPopupMenu != nullptr)
            onPopupMenu();
        return;
    }

    juce::Button::mouseDown (e);
}

void GradientStripEditor::DiceButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto r = getLocalBounds().toFloat().reduced (0.5f);
    auto fill = juce::Colours::black.withAlpha (0.45f);
    GraphOverlayButtonLookAndFeel::paintChromeButton (g, r, fill, highlighted, down);

    const float s = juce::jmin (r.getWidth(), r.getHeight());
    const auto c = r.getCentre();
    const float die = s * 0.52f;
    const float thick = juce::jmax (1.2f, s * 0.08f);
    auto dieR = juce::Rectangle<float> (c.x - die * 0.5f, c.y - die * 0.5f, die, die);
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.92f));
    g.drawRoundedRectangle (dieR, die * 0.12f, thick);
    const float dot = juce::jmax (1.2f, die * 0.12f);
    auto paintDot = [&] (float nx, float ny)
    {
        g.fillEllipse (c.x + nx * die * 0.22f - dot * 0.5f,
                       c.y + ny * die * 0.22f - dot * 0.5f,
                       dot, dot);
    };
    paintDot (-1.0f, -1.0f);
    paintDot (1.0f, -1.0f);
    paintDot (0.0f, 0.0f);
    paintDot (-1.0f, 1.0f);
    paintDot (1.0f, 1.0f);
}

GradientStripEditor::PresetFieldButton::PresetFieldButton()
    : juce::Button ("preset")
{
    setTooltip ("Load a gradient preset");
}

void GradientStripEditor::PresetFieldButton::setDisplay (const GradientRamp* r, juce::String name)
{
    displayName = name.isNotEmpty() ? std::move (name) : juce::String ("Choose preset...");
    hasRamp = r != nullptr && r->stops.size() >= 2;
    if (hasRamp)
        displayRamp = *r;
    repaint();
}

void GradientStripEditor::PresetFieldButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
{
    auto bounds = getLocalBounds().toFloat().reduced (0.5f);
    GraphOverlayButtonLookAndFeel::paintChromeButton (g, bounds,
                                                      juce::Colours::black.withAlpha (0.45f),
                                                      highlighted, down);

    auto inner = bounds.reduced (4.0f, 3.0f);
    auto arrow = inner.removeFromRight (14.0f);
    auto swatch = inner.removeFromLeft (juce::jmin (72.0f, inner.getWidth() * 0.42f));
    inner.removeFromLeft (6.0f);

    if (hasRamp)
        paintRampSwatch (g, swatch, displayRamp, 2.5f);
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (swatch, 2.5f);
    }

    g.setColour (juce::Colours::whitesmoke.withAlpha (0.92f));
    g.setFont (juce::FontOptions (12.5f));
    g.drawFittedText (displayName, inner.toNearestIntEdges(), juce::Justification::centredLeft, 1);

    juce::Path chev;
    const float cx = arrow.getCentreX();
    const float cy = arrow.getCentreY();
    chev.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.75f));
    g.fillPath (chev);
}

GradientStripEditor::GradientStripEditor (SharedResources& resources,
                                          ModeFamily family,
                                          RampPresetStore* presetStore)
    : sharedResources (resources),
      modeFamily (family),
      presets (presetStore)
{
    enableToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
    enableToggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    enableToggle.setTooltip ("Use this custom ramp on the meter");
    enableToggle.onClick = [this]
    {
        if (ramp == nullptr)
            return;
        ramp->enabled = enableToggle.getToggleState() && ramp->stops.size() >= 2;
        ++ramp->revision;
        notifyChanged();
        repaint();
    };
    addAndMakeVisible (enableToggle);

    mapLabel.setText ("Map", juce::dontSendNotification);
    mapLabel.setFont (juce::FontOptions (12.0f));
    mapLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.7f));
    addAndMakeVisible (mapLabel);

    mapCombo.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    {
        const auto& pal = sharedResources.sharedColors;
        const auto ink = pal.dropdownTextOn (pal.optionComboText, pal.optionComboBackground);
        mapCombo.setColour (juce::ComboBox::textColourId, ink);
        mapCombo.setColour (juce::ComboBox::backgroundColourId, pal.optionComboBackground);
        mapCombo.setColour (juce::ComboBox::outlineColourId, pal.optionBorder);
        mapCombo.setColour (juce::ComboBox::arrowColourId, ink);
    }
    mapCombo.onChange = [this]
    {
        if (ramp == nullptr)
            return;
        const int id = mapCombo.getSelectedId();
        if (id <= 0)
            return;
        ramp->mapMode = static_cast<GradientRamp::MapMode> (id - 1);
        ++ramp->revision;
        notifyChanged();
        repaint();
    };
    addAndMakeVisible (mapCombo);
    rebuildMapCombo();

    auto styleTiny = [] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
        b.setConnectedEdges (0);
    };

    simplifyButton.setButtonText ("Less");
    simplifyButton.setTooltip ("Fewer colour poles (minimum 2)");
    simplifyButton.onClick = [this] { simplifyClicked(); };
    styleTiny (simplifyButton);
    addAndMakeVisible (simplifyButton);

    densifyButton.setButtonText ("More");
    densifyButton.setTooltip ("More colour poles for smoother gradations");
    densifyButton.onClick = [this] { densifyClicked(); };
    styleTiny (densifyButton);
    addAndMakeVisible (densifyButton);

    invertButton.setButtonText ("Invert");
    invertButton.setTooltip ("Reverse the ramp colours (high <-> low)");
    invertButton.onClick = [this] { invertClicked(); };
    styleTiny (invertButton);
    addAndMakeVisible (invertButton);

    stopCountButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    stopCountButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.7f));
    stopCountButton.setTooltip ("Pole count. Click to toggle Hard / Soft colour blend");
    stopCountButton.onClick = [this]
    {
        if (ramp == nullptr)
            return;
        ramp->interpMode = (ramp->interpMode == GradientRamp::InterpMode::soft)
                               ? GradientRamp::InterpMode::hard
                               : GradientRamp::InterpMode::soft;
        ++ramp->revision;
        syncControlsFromRamp();
        notifyChanged();
        repaint();
    };
    addAndMakeVisible (stopCountButton);

    savePresetButton.setButtonText ("Save");
    savePresetButton.setTooltip ("Save this ramp as a reusable preset");
    styleTiny (savePresetButton);
    savePresetButton.onClick = [this] { savePresetClicked(); };
    addAndMakeVisible (savePresetButton);

    presetField.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetField);

    randomizeButton.onClick = [this] { randomizeClicked(); };
    randomizeButton.onPopupMenu = [this] { showRandomizeModeMenu(); };
    addAndMakeVisible (randomizeButton);

    samplePathButton.setButtonText ("Sample Path");
    samplePathButton.setTooltip ("Drag across the plugin UI to sample colours into this gradient");
    styleTiny (samplePathButton);
    samplePathButton.onClick = [this] { samplePathClicked(); };
    addAndMakeVisible (samplePathButton);

    if (presets != nullptr)
        presets->addChangeListener (this);

    syncPresetField();
}

GradientStripEditor::~GradientStripEditor()
{
    mapCombo.setLookAndFeel (nullptr);
    if (presets != nullptr)
        presets->removeChangeListener (this);
}

void GradientStripEditor::setPresetStore (RampPresetStore* store)
{
    if (presets != nullptr)
        presets->removeChangeListener (this);
    presets = store;
    if (presets != nullptr)
        presets->addChangeListener (this);
}

void GradientStripEditor::changeListenerCallback (juce::ChangeBroadcaster*)
{
    syncPresetField();
}

void GradientStripEditor::setCompact (bool shouldBeCompact) noexcept
{
    compact = shouldBeCompact;
    resized();
    repaint();
}

void GradientStripEditor::setUiScale (float scale) noexcept
{
    uiScale = juce::jlimit (0.5f, 1.5f, scale);
    const float fs = 11.0f * uiScale;
    mapLabel.setFont (juce::FontOptions (fs));
    stopCountButton.setColour (juce::TextButton::textColourOffId,
                               juce::Colours::whitesmoke.withAlpha (0.7f));
    resized();
    repaint();
}

int GradientStripEditor::getPreferredHeight() const noexcept
{
    int h = juce::roundToInt ((6 + (float) kToolbarH + 4 + kStripBlockH + 4) * uiScale);
    if (polePicker != nullptr)
        h += juce::roundToInt (4.0f * uiScale) + RampColorPickerPanel::kPreferredHeight;
    return h;
}

void GradientStripEditor::notifyHeightChanged()
{
    if (onPreferredHeightChanged != nullptr)
        onPreferredHeightChanged();
}

void GradientStripEditor::rebuildMapCombo()
{
    mapCombo.clear (juce::dontSendNotification);

    if (modeFamily == ModeFamily::intensity)
    {
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::intensityLowToHigh),
                          (int) GradientRamp::MapMode::intensityLowToHigh + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::intensityHighToLow),
                          (int) GradientRamp::MapMode::intensityHighToLow + 1);
    }
    else if (modeFamily == ModeFamily::frequency)
    {
        mapCombo.addItem ("Low to High", (int) GradientRamp::MapMode::intensityLowToHigh + 1);
        mapCombo.addItem ("High to Low", (int) GradientRamp::MapMode::intensityHighToLow + 1);
    }
    else if (modeFamily == ModeFamily::oscilloscope)
    {
        mapCombo.addItem ("Amplitude: Quiet to Loud",
                          (int) GradientRamp::MapMode::intensityLowToHigh + 1);
        mapCombo.addItem ("Amplitude: Loud to Quiet",
                          (int) GradientRamp::MapMode::intensityHighToLow + 1);
        mapCombo.addItem ("Frequency: Low to High",
                          (int) GradientRamp::MapMode::oscFreqLowToHigh + 1);
        mapCombo.addItem ("Frequency: High to Low",
                          (int) GradientRamp::MapMode::oscFreqHighToLow + 1);
    }
    else if (modeFamily == ModeFamily::goniometer)
    {
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::gonLoudness),
                          (int) GradientRamp::MapMode::gonLoudness + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::gonDiversionX),
                          (int) GradientRamp::MapMode::gonDiversionX + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::gonDiversionY),
                          (int) GradientRamp::MapMode::gonDiversionY + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::gonDiversionXY),
                          (int) GradientRamp::MapMode::gonDiversionXY + 1);
    }
    else
    {
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::leftToRight),
                          (int) GradientRamp::MapMode::leftToRight + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::rightToLeft),
                          (int) GradientRamp::MapMode::rightToLeft + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::topToBottom),
                          (int) GradientRamp::MapMode::topToBottom + 1);
        mapCombo.addItem (GradientRamp::mapModeName (GradientRamp::MapMode::bottomToTop),
                          (int) GradientRamp::MapMode::bottomToTop + 1);
    }
}

void GradientStripEditor::syncControlsFromRamp()
{
    if (ramp == nullptr)
        return;

    enableToggle.setToggleState (ramp->enabled, juce::dontSendNotification);
    mapCombo.setSelectedId ((int) ramp->mapMode + 1, juce::dontSendNotification);
    const int n = (int) ramp->stops.size();
    const bool soft = ramp->interpMode == GradientRamp::InterpMode::soft;
    stopCountButton.setButtonText (juce::String (n) + (soft ? "~" : ""));
    stopCountButton.setTooltip (soft
                                    ? "Soft blend (click for Hard). Poles feel less sharp."
                                    : "Hard blend (click for Soft). Linear between poles.");
    simplifyButton.setEnabled (n > 2);
    densifyButton.setEnabled (n >= 2 && n < GradientRamp::kMaxStops);
    invertButton.setEnabled (n >= 2);
    savePresetButton.setEnabled (ramp->stops.size() >= 2);
    samplePathButton.setEnabled (true);
    syncPresetField();
}

int GradientStripEditor::findMatchingPresetIndex() const
{
    if (presets == nullptr || ramp == nullptr || ramp->stops.size() < 2)
        return -1;

    const auto& list = presets->getPresets();
    for (int i = 0; i < list.size(); ++i)
        if (rampsMatchVisually (list.getReference (i).ramp, *ramp))
            return i;

    return -1;
}

void GradientStripEditor::syncPresetField()
{
    selectedPresetIndex = findMatchingPresetIndex();

    if (ramp == nullptr || ramp->stops.size() < 2)
    {
        presetField.setDisplay (nullptr, "Choose preset...");
        return;
    }

    if (selectedPresetIndex >= 0 && presets != nullptr)
    {
        const auto& p = presets->getPresets().getReference (selectedPresetIndex);
        presetField.setDisplay (ramp, p.name);
    }
    else
    {
        presetField.setDisplay (ramp, "Custom");
    }
}

void GradientStripEditor::setRamp (GradientRamp* r)
{
    // Same object (e.g. live colour preview notify) — keep the inline picker open.
    if (r == ramp)
    {
        syncControlsFromRamp();
        repaint();
        return;
    }

    if (polePicker != nullptr)
        closePolePicker (false);

    ramp = r;
    syncControlsFromRamp();
    selectedIndex = -1;
    dragIndex = -1;
    repaint();
}

void GradientStripEditor::simplifyClicked()
{
    if (ramp == nullptr || ramp->stops.size() <= 2)
        return;
    ramp->simplifyOneStep();
    syncControlsFromRamp();
    notifyChanged();
    repaint();
}

void GradientStripEditor::densifyClicked()
{
    if (ramp == nullptr || ramp->stops.size() < 2)
        return;
    ramp->densifyOneStep();
    syncControlsFromRamp();
    notifyChanged();
    repaint();
}

void GradientStripEditor::invertClicked()
{
    if (ramp == nullptr || ramp->stops.size() < 2)
        return;
    ramp->invertStops();
    syncControlsFromRamp();
    notifyChanged();
    repaint();
}

void GradientStripEditor::savePresetClicked()
{
    if (presets == nullptr || ramp == nullptr || ramp->stops.size() < 2)
        return;

    auto* aw = new juce::AlertWindow ("Save Ramp Preset",
                                      "Name for this colour ramp:",
                                      juce::AlertWindow::NoIcon);
    aw->addTextEditor ("name", "Ramp", "Name");
    aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<GradientStripEditor> (this), aw] (int r)
        {
            if (r == 1 && safe != nullptr && safe->presets != nullptr && safe->ramp != nullptr)
            {
                auto name = aw->getTextEditorContents ("name").trim();
                if (name.isEmpty())
                    name = "Ramp";
                const auto resolved = safe->presets->resolvedUserName (name);
                PresetOverwriteConfirm::run (
                    "ramp preset",
                    resolved,
                    safe->presets->containsUserName (resolved),
                    [safe, resolved]
                    {
                        if (safe == nullptr || safe->presets == nullptr || safe->ramp == nullptr)
                            return;
                        safe->presets->savePreset (resolved, *safe->ramp);
                        safe->syncPresetField();
                    },
                    safe.getComponent());
            }
            delete aw;
        }));
}

void GradientStripEditor::samplePathClicked()
{
    if (onSamplePath != nullptr)
        onSamplePath();
}

void GradientStripEditor::randomizeClicked()
{
    if (ramp == nullptr)
        return;

    if (polePicker != nullptr)
        closePolePicker (false);

    const bool varyAlpha = (modeFamily == ModeFamily::spatial);
    ColourRampBank::randomizeRamp (*ramp, sharedResources.sharedColors, varyAlpha);
    selectedIndex = -1;
    dragIndex = -1;
    syncControlsFromRamp();
    notifyChanged();
    repaint();
}

void GradientStripEditor::showRandomizeModeMenu()
{
    auto& c = sharedResources.sharedColors;
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addSectionHeader ("Ramp randomize mode");
    menu.addItem (1, "Ordered gradation", true, c.orderedRampGradation);
    menu.addItem (2, "Standard (independent stops)", true, ! c.orderedRampGradation);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&randomizeButton),
                        [safe = juce::Component::SafePointer<GradientStripEditor> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            const bool ordered = (result == 1);
                            if (auto* main = safe->findParentComponentOfClass<MainComponent>())
                                main->setOrderedRampGradation (ordered, true);
                            else
                                safe->sharedResources.sharedColors.orderedRampGradation = ordered;

                            safe->randomizeButton.setTooltip (
                                safe->sharedResources.sharedColors.orderedRampGradation
                                    ? "Randomize this gradient (ordered H/S/V spans). Right-click: mode."
                                    : "Randomize this gradient (independent stops). Right-click: mode.");
                        });
}

void GradientStripEditor::showPresetMenu()
{
    if (presets == nullptr)
        return;

    showRampPresetPickerCallOut (
        *presets,
        &presetField,
        [safe = juce::Component::SafePointer<GradientStripEditor> (this)] (int index)
        {
            if (safe == nullptr || safe->ramp == nullptr || safe->presets == nullptr)
                return;

            safe->presets->applyPreset (index, *safe->ramp);

            if (safe->modeFamily == ModeFamily::goniometer)
            {
                if (! safe->ramp->isGoniometerMap())
                    safe->ramp->mapMode = GradientRamp::MapMode::gonLoudness;
            }
            else if (safe->modeFamily == ModeFamily::oscilloscope)
            {
                if (! safe->ramp->isOscilloscopeMap())
                    safe->ramp->mapMode = GradientRamp::MapMode::intensityLowToHigh;
            }
            else if (safe->modeFamily == ModeFamily::intensity
                     || safe->modeFamily == ModeFamily::frequency)
            {
                if (! safe->ramp->isIntensityMap())
                    safe->ramp->mapMode = GradientRamp::MapMode::intensityLowToHigh;
            }
            else if (! safe->ramp->isSpatialMap())
            {
                safe->ramp->mapMode = GradientRamp::MapMode::bottomToTop;
            }

            safe->ramp->enabled = true;
            ++safe->ramp->revision;
            safe->selectedPresetIndex = index;
            safe->syncControlsFromRamp();
            safe->notifyChanged();
            safe->repaint();
        },
        [safe = juce::Component::SafePointer<GradientStripEditor> (this)] (int index)
        {
            if (safe == nullptr || safe->presets == nullptr)
                return;
            safe->presets->deletePreset (index);
            safe->syncPresetField();
        });
}

juce::Rectangle<float> GradientStripEditor::stripBounds() const
{
    const float s = uiScale;
    auto area = getLocalBounds().toFloat().reduced (4.0f * s, 2.0f * s);
    area.removeFromTop ((float) kToolbarH * s + 4.0f * s);
    return area.removeFromTop (kStripH * s);
}

void GradientStripEditor::resized()
{
    const float s = uiScale;
    auto px = [s] (float v) { return juce::roundToInt (v * s); };

    auto area = getLocalBounds().reduced (px (4), px (2));
    auto bar1 = area.removeFromTop (px ((float) kToolbarRowH));
    area.removeFromTop (px ((float) kToolbarGap));
    auto bar2 = area.removeFromTop (px ((float) kToolbarRowH));

    const int lessW = textButtonWidth ("Less", s);
    const int moreW = textButtonWidth ("More", s);
    const int invertW = textButtonWidth ("Invert", s);
    const int saveW = textButtonWidth ("Save", s);
    const int countW = juce::jmax (px (32), textButtonWidth (stopCountButton.getButtonText(), s, 28));
    const int useW = juce::jmax (px (52), textButtonWidth ("Use", s, 52));
    const int sampleW = textButtonWidth ("Sample Path", s, 96);

    enableToggle.setBounds (bar1.removeFromLeft (useW));
    bar1.removeFromLeft (px (4));
    mapLabel.setBounds (bar1.removeFromLeft (px (30)));
    const int trailing = lessW + countW + moreW + invertW + saveW + px (18);
    mapCombo.setBounds (bar1.removeFromLeft (juce::jmax (px (100), bar1.getWidth() - trailing)));
    bar1.removeFromLeft (px (4));
    simplifyButton.setBounds (bar1.removeFromLeft (lessW));
    stopCountButton.setBounds (bar1.removeFromLeft (countW));
    densifyButton.setBounds (bar1.removeFromLeft (moreW));
    bar1.removeFromLeft (px (4));
    invertButton.setBounds (bar1.removeFromLeft (invertW));
    bar1.removeFromLeft (px (4));
    savePresetButton.setBounds (bar1.removeFromLeft (saveW));

    samplePathButton.setBounds (bar2.removeFromRight (sampleW));
    bar2.removeFromRight (px (4));
    const int diceSize = juce::jmin (bar2.getHeight(), px (24));
    randomizeButton.setBounds (bar2.removeFromRight (diceSize).withSizeKeepingCentre (diceSize, diceSize));
    bar2.removeFromRight (px (6));
    presetField.setBounds (bar2);

    if (polePicker != nullptr)
    {
        area.removeFromTop (px ((float) kStripBlockH) + px (4));
        polePicker->setBounds (area.removeFromTop (RampColorPickerPanel::kPreferredHeight));
    }
}

void GradientStripEditor::paint (juce::Graphics& g)
{
    const auto strip = stripBounds();
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (strip, 3.0f);

    if (ramp == nullptr || ramp->stops.size() < 2)
    {
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.35f));
        g.setFont (juce::FontOptions (12.0f * uiScale));
        g.drawFittedText ("Sample a path to fill this ramp",
                          strip.toNearestIntEdges(), juce::Justification::centred, 1);
        return;
    }

    juce::ColourGradient grad (ramp->colourAt (0.0f), strip.getX(), strip.getCentreY(),
                               ramp->colourAt (1.0f), strip.getRight(), strip.getCentreY(), false);
    for (size_t i = 1; i + 1 < ramp->stops.size(); ++i)
        grad.addColour ((double) ramp->stops[i].position, ramp->stops[i].colour);

    g.setGradientFill (grad);
    g.fillRoundedRectangle (strip.reduced (1.0f), 3.0f);
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.28f));
    g.drawRoundedRectangle (strip, 3.0f, 1.0f);

    const float s = uiScale;
    const float arrowH = kArrowH * s;
    const float arrowHalf = kArrowHalf * s;

    for (int i = 0; i < (int) ramp->stops.size(); ++i)
    {
        const float x = strip.getX() + ramp->stops[(size_t) i].position * strip.getWidth();
        const float y = strip.getBottom() + 1.0f * s;
        juce::Path arrow;
        arrow.addTriangle (x, y,
                           x - arrowHalf, y + arrowH,
                           x + arrowHalf, y + arrowH);

        g.setColour (ramp->stops[(size_t) i].colour);
        g.fillPath (arrow);
        g.setColour (i == selectedIndex ? juce::Colours::goldenrod
                                        : juce::Colours::whitesmoke.withAlpha (0.7f));
        g.strokePath (arrow, juce::PathStrokeType (0.9f * s));
    }
}

int GradientStripEditor::hitTestStop (juce::Point<float> p) const
{
    if (ramp == nullptr)
        return -1;

    const float s = uiScale;
    const auto strip = stripBounds();
    for (int i = 0; i < (int) ramp->stops.size(); ++i)
    {
        const float x = strip.getX() + ramp->stops[(size_t) i].position * strip.getWidth();
        const float y = strip.getBottom() + 1.0f * s;
        juce::Rectangle<float> hit (x - 6.0f * s, y - 1.0f * s, 12.0f * s, kArrowH * s + 5.0f * s);
        if (hit.contains (p))
            return i;
    }
    return -1;
}

void GradientStripEditor::mouseDown (const juce::MouseEvent& e)
{
    // Right-click → delete pole (keep at least 2).
    if (e.mods.isPopupMenu())
    {
        if (ramp == nullptr)
            return;

        const int idx = hitTestStop (e.position);
        if (idx < 0 || ramp->stops.size() <= 2)
            return;

        if (polePicker != nullptr && polePickerIndex == idx)
            closePolePicker (false);

        ramp->stops.erase (ramp->stops.begin() + idx);
        ++ramp->revision;
        selectedIndex = -1;
        dragIndex = -1;
        if (polePickerIndex > idx)
            --polePickerIndex;
        syncControlsFromRamp();
        notifyChanged();
        repaint();
        return;
    }

    dragIndex = hitTestStop (e.position);
    selectedIndex = dragIndex;
    mouseDownPos = e.position;
    didDrag = false;
    repaint();
}

void GradientStripEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (ramp == nullptr || dragIndex < 0 || dragIndex >= (int) ramp->stops.size())
        return;

    if (e.position.getDistanceFrom (mouseDownPos) > 3.0f)
        didDrag = true;

    if (! didDrag)
        return;

    const auto strip = stripBounds();
    if (strip.getWidth() <= 1.0f)
        return;

    const auto colour = ramp->stops[(size_t) dragIndex].colour;
    const float pos = juce::jlimit (0.0f, 1.0f, (e.position.x - strip.getX()) / strip.getWidth());
    ramp->stops[(size_t) dragIndex].position = pos;
    ramp->sortAndClamp();

    selectedIndex = 0;
    float best = 1.0e9f;
    for (int i = 0; i < (int) ramp->stops.size(); ++i)
    {
        const float err = std::abs (ramp->stops[(size_t) i].position - pos);
        if (err < best && ramp->stops[(size_t) i].colour == colour)
        {
            best = err;
            selectedIndex = i;
        }
    }
    dragIndex = selectedIndex;
    // Repaint only while dragging — commit on mouseUp.
    repaint();
}

void GradientStripEditor::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);

    if (didDrag && ramp != nullptr)
    {
        ++ramp->revision;
        notifyChanged();
    }

    dragIndex = -1;
    didDrag = false;
}

void GradientStripEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    // Double-click → inline colour picker (never a nested CallOutBox).
    const int idx = hitTestStop (e.position);
    if (idx < 0)
        return;

    if (polePicker != nullptr && polePickerIndex == idx)
    {
        closePolePicker (true);
        return;
    }

    openPickerForStop (idx);
}

void GradientStripEditor::closePolePicker (bool persist)
{
    if (polePicker == nullptr)
        return;

    polePicker.reset();
    polePickerIndex = -1;
    resized();
    notifyHeightChanged();
    repaint();

    if (persist && onRampChanged != nullptr)
        onRampChanged();
}

void GradientStripEditor::openPickerForStop (int index)
{
    if (ramp == nullptr || index < 0 || index >= (int) ramp->stops.size())
        return;

    const auto initial = ramp->stops[(size_t) index].colour;
    polePickerIndex = index;
    selectedIndex = index;

    polePicker = std::make_unique<RampColorPickerPanel> (initial);
    polePicker->onColourChanged = [this] (juce::Colour c)
    {
        if (ramp == nullptr || polePickerIndex < 0
            || polePickerIndex >= (int) ramp->stops.size())
            return;

        ramp->stops[(size_t) polePickerIndex].colour = c;
        ++ramp->revision;
        if (onRampPreview != nullptr)
            onRampPreview();
        else if (onRampChanged != nullptr)
            onRampChanged();
        repaint();
    };
    polePicker->onDone = [this] { closePolePicker (true); };
    addAndMakeVisible (*polePicker);

    resized();
    notifyHeightChanged();
    repaint();
}

void GradientStripEditor::notifyChanged()
{
    if (ramp != nullptr)
        ramp->enabled = enableToggle.getToggleState() && ramp->stops.size() >= 2;

    syncControlsFromRamp();

    if (onRampChanged != nullptr)
        onRampChanged();
}
