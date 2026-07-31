#include "GradientStripEditor.h"
#include "RampColorPickerPanel.h"

namespace
{
    constexpr float kStripH = 24.0f;
    constexpr float kArrowH = 7.0f;
    constexpr float kArrowHalf = 3.2f;
    constexpr int kToolbarH = 24;
    constexpr int kStripBlockH = 36;

    /** Mini gradient preview row for the preset CallOutBox. */
    class RampPresetRow final : public juce::Component
    {
    public:
        RampPresetRow (juce::String nameIn,
                       GradientRamp rampIn,
                       std::function<void()> onApplyIn,
                       std::function<void()> onDeleteIn)
            : name (std::move (nameIn)),
              ramp (std::move (rampIn)),
              onApply (std::move (onApplyIn)),
              onDelete (std::move (onDeleteIn))
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (4.0f, 3.0f);
            if (hovered)
            {
                g.setColour (juce::Colours::goldenrod.withAlpha (0.18f));
                g.fillRoundedRectangle (bounds.expanded (2.0f), 3.0f);
            }

            auto swatch = bounds.removeFromLeft (bounds.getWidth() * 0.55f).reduced (0.0f, 2.0f);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (swatch, 2.5f);

            if (ramp.stops.size() >= 2)
            {
                juce::ColourGradient grad (ramp.colourAt (0.0f), swatch.getX(), swatch.getCentreY(),
                                           ramp.colourAt (1.0f), swatch.getRight(), swatch.getCentreY(), false);
                for (size_t i = 1; i + 1 < ramp.stops.size(); ++i)
                    grad.addColour ((double) ramp.stops[i].position, ramp.stops[i].colour);
                g.setGradientFill (grad);
                g.fillRoundedRectangle (swatch.reduced (1.0f), 2.5f);
            }

            bounds.removeFromLeft (8.0f);
            g.setColour (juce::Colours::whitesmoke.withAlpha (0.9f));
            g.setFont (juce::FontOptions (12.5f));
            g.drawText (name, bounds.toNearestIntEdges(), juce::Justification::centredLeft, true);
        }

        void mouseEnter (const juce::MouseEvent&) override { hovered = true;  repaint(); }
        void mouseExit  (const juce::MouseEvent&) override { hovered = false; repaint(); }

        void mouseUp (const juce::MouseEvent& e) override
        {
            if (! e.mouseWasClicked())
                return;
            if (e.mods.isPopupMenu())
            {
                if (onDelete)
                    onDelete();
                return;
            }
            if (onApply)
                onApply();
        }

    private:
        juce::String name;
        GradientRamp ramp;
        std::function<void()> onApply, onDelete;
        bool hovered = false;
    };

    class RampPresetPicker final : public juce::Component
    {
    public:
        RampPresetPicker (RampPresetStore& storeIn,
                          std::function<void (int)> onPickIn,
                          std::function<void (int)> onDeleteIn)
            : store (storeIn),
              onPick (std::move (onPickIn)),
              onDelete (std::move (onDeleteIn))
        {
            viewport.setViewedComponent (&list, false);
            viewport.setScrollBarsShown (true, false);
            addAndMakeVisible (viewport);

            constexpr int rowH = 28;
            const int n = store.size();
            for (int i = 0; i < n; ++i)
            {
                const auto& p = store.getPresets().getReference (i);
                const bool canDelete = ! p.isFactory;
                auto* row = rows.add (new RampPresetRow (
                    p.isFactory ? (p.name + "  (factory)") : p.name,
                    p.ramp,
                    [this, i]
                    {
                        if (onPick)
                            onPick (i);
                    },
                    [this, i, canDelete]
                    {
                        if (canDelete && onDelete)
                            onDelete (i);
                    }));
                list.addAndMakeVisible (row);
            }

            list.setSize (220, juce::jmax (rowH, n * rowH));
            for (int i = 0; i < rows.size(); ++i)
                rows[i]->setBounds (0, i * rowH, 220, rowH);

            const int listH = juce::jmin (8 * rowH, list.getHeight());
            setSize (236, 8 + (n > 0 ? listH : 36) + 8);

            if (n == 0)
            {
                emptyLabel.setText ("No presets", juce::dontSendNotification);
                emptyLabel.setJustificationType (juce::Justification::centred);
                emptyLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.55f));
                addAndMakeVisible (emptyLabel);
            }
        }

        ~RampPresetPicker() override
        {
            viewport.setViewedComponent (nullptr, false);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colour::fromRGB (28, 28, 26));
            g.setColour (juce::Colours::goldenrod.withAlpha (0.4f));
            g.drawRect (getLocalBounds().toFloat(), 1.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (8);
            if (rows.isEmpty())
                emptyLabel.setBounds (area);
            else
                viewport.setBounds (area);
        }

    private:
        RampPresetStore& store;
        std::function<void (int)> onPick, onDelete;
        juce::Viewport viewport;
        juce::Component list;
        juce::OwnedArray<RampPresetRow> rows;
        juce::Label emptyLabel;
    };
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

    mapCombo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.92f));
    mapCombo.setColour (juce::ComboBox::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
    mapCombo.setColour (juce::ComboBox::outlineColourId, juce::Colours::whitesmoke.withAlpha (0.2f));
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

    presetMenuButton.setButtonText ("Load");
    presetMenuButton.setTooltip ("Load a saved ramp preset");
    styleTiny (presetMenuButton);
    presetMenuButton.onClick = [this] { showPresetMenu(); };
    addAndMakeVisible (presetMenuButton);

    if (presets != nullptr)
        presets->addChangeListener (this);
}

GradientStripEditor::~GradientStripEditor()
{
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
    // Preset list changed — nothing to redraw on the strip itself.
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
    int h = juce::roundToInt ((6 + kToolbarH + 4 + kStripBlockH + 4) * uiScale);
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

    const auto add = [this] (GradientRamp::MapMode m)
    {
        mapCombo.addItem (GradientRamp::mapModeName (m), (int) m + 1);
    };

    if (modeFamily == ModeFamily::intensity)
    {
        add (GradientRamp::MapMode::intensityLowToHigh);
        add (GradientRamp::MapMode::intensityHighToLow);
    }
    else
    {
        add (GradientRamp::MapMode::leftToRight);
        add (GradientRamp::MapMode::rightToLeft);
        add (GradientRamp::MapMode::topToBottom);
        add (GradientRamp::MapMode::bottomToTop);
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
    savePresetButton.setEnabled (ramp->stops.size() >= 2);
}

void GradientStripEditor::setRamp (GradientRamp* r)
{
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
                safe->presets->savePreset (name, *safe->ramp);
            }
            delete aw;
        }));
}

void GradientStripEditor::showPresetMenu()
{
    if (presets == nullptr)
        return;

    auto pickerBox = std::make_shared<juce::Component::SafePointer<juce::CallOutBox>>();

    auto* picker = new RampPresetPicker (
        *presets,
        [safe = juce::Component::SafePointer<GradientStripEditor> (this), pickerBox] (int index)
        {
            if (safe == nullptr || safe->ramp == nullptr || safe->presets == nullptr)
                return;

            const bool wantIntensity = (safe->modeFamily == ModeFamily::intensity);
            safe->presets->applyPreset (index, *safe->ramp);

            if (wantIntensity && ! safe->ramp->isIntensityMap())
                safe->ramp->mapMode = GradientRamp::MapMode::intensityLowToHigh;
            else if (! wantIntensity && ! safe->ramp->isSpatialMap())
                safe->ramp->mapMode = GradientRamp::MapMode::bottomToTop;

            safe->ramp->enabled = true;
            ++safe->ramp->revision;
            safe->syncControlsFromRamp();
            safe->notifyChanged();
            safe->repaint();

            if (*pickerBox != nullptr)
                (*pickerBox)->dismiss();
        },
        [safe = juce::Component::SafePointer<GradientStripEditor> (this), pickerBox] (int index)
        {
            if (safe == nullptr || safe->presets == nullptr)
                return;
            safe->presets->deletePreset (index);
            if (*pickerBox != nullptr)
                (*pickerBox)->dismiss();
        });

    auto& box = juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (picker),
                                                        presetMenuButton.getScreenBounds(),
                                                        nullptr);
    *pickerBox = &box;
    box.setDismissalMouseClicksAreAlwaysConsumed (true);
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
    auto bar = area.removeFromTop (px ((float) kToolbarH));

    enableToggle.setBounds (bar.removeFromLeft (px (40)));
    bar.removeFromLeft (px (2));
    mapLabel.setBounds (bar.removeFromLeft (px (28)));
    mapCombo.setBounds (bar.removeFromLeft (juce::jmin (px (96), bar.getWidth() - px (150))));
    bar.removeFromLeft (px (3));
    simplifyButton.setBounds (bar.removeFromLeft (px (36)));
    stopCountButton.setBounds (bar.removeFromLeft (px (28)));
    densifyButton.setBounds (bar.removeFromLeft (px (36)));
    bar.removeFromLeft (px (4));
    savePresetButton.setBounds (bar.removeFromLeft (px (36)));
    bar.removeFromLeft (px (2));
    presetMenuButton.setBounds (bar.removeFromLeft (px (36)));

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
