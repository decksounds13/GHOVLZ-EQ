#include "ModSectionComponent.h"
#include "EqProcessor.h"
#include "EqEditor.h"
#include "ShapeEditorPopup.h"

namespace
{
    void styleSmallButton (juce::TextButton& b, TextButtonLookAndFeel& lf)
    {
        b.setLookAndFeel (&lf);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    }

    void styleImageRotary (RotaryImageKnobForOptionBox& s)
    {
        // Same as OptionBox A/R: square face, no text-box padding that stretches the art.
        s.setCompactNoValueBox (true);
    }

    /** Place a 1:1 knob inside area (centred). Optional cap keeps LFO knobs consistent. */
    juce::Rectangle<int> squareKnobBounds (juce::Rectangle<int> area, int maxSide = 48)
    {
        const int side = juce::jmin (area.getWidth(), area.getHeight(), maxSide);
        return area.withSizeKeepingCentre (juce::jmax (0, side), juce::jmax (0, side));
    }

    void styleAmountSlider (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearHorizontal);
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 34, 14);
        s.setNumDecimalPlacesToDisplay (0);
        s.setTextValueSuffix ({});
        s.textFromValueFunction = [] (double v)
        {
            return juce::String (juce::roundToInt (v)) + "%";
        };
        s.valueFromTextFunction = [] (const juce::String& t)
        {
            return (double) t.upToFirstOccurrenceOf ("%", false, false).trim().getIntValue();
        };
        s.setTooltip ("Modulation amount for this routing");
        s.setColour (juce::Slider::trackColourId, juce::Colour::fromRGB (50, 40, 28));
        s.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGB (20, 14, 10));
        s.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (220, 180, 90));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.8f));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    }

    void styleVerticalThresh (juce::Slider& s)
    {
        s.setSliderStyle (juce::Slider::LinearVertical);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 44, 12);
        s.setTextValueSuffix (" dB");
        s.setTooltip ("Envelope follower threshold in dB. Signal fills from the bottom - processing engages when the level reaches the thumb.");
        // Transparent track so the level meter painted underneath is visible.
        s.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGB (220, 180, 90));
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.85f));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        s.setOpaque (false);
    }

    void paintEnvThresholdMeter (juce::Graphics& g,
                                 juce::Rectangle<float> meterBounds,
                                 float envDb,
                                 float threshDb,
                                 const SharedColors& c)
    {
        constexpr float kFloorDb = -60.0f;
        constexpr float kCeilDb = 0.0f;

        auto r = meterBounds.reduced (2.0f, 1.0f);
        if (r.getWidth() < 2.0f || r.getHeight() < 4.0f)
            return;

        g.setColour (c.modBackground.darker (0.35f).withAlpha (0.95f));
        g.fillRoundedRectangle (r, 2.5f);
        g.setColour (c.modBorder.withAlpha (0.55f));
        g.drawRoundedRectangle (r, 2.5f, 1.0f);

        const float levelNorm = juce::jlimit (0.0f, 1.0f, (envDb - kFloorDb) / (kCeilDb - kFloorDb));
        auto fill = r.removeFromBottom (r.getHeight() * levelNorm);
        const bool triggered = envDb >= threshDb;
        g.setColour (triggered ? c.modAccent.withAlpha (0.92f)
                               : c.modAccent.withAlpha (0.45f));
        g.fillRoundedRectangle (fill, 2.0f);

        // Threshold hairline so the thumb position is easy to read against the meter.
        const float threshNorm = juce::jlimit (0.0f, 1.0f, (threshDb - kFloorDb) / (kCeilDb - kFloorDb));
        const float y = meterBounds.getBottom() - 1.0f - threshNorm * (meterBounds.getHeight() - 2.0f);
        g.setColour (juce::Colours::whitesmoke.withAlpha (triggered ? 0.75f : 0.35f));
        g.drawLine (meterBounds.getX() + 1.0f, y, meterBounds.getRight() - 1.0f, y, 1.0f);
    }

    void setupTinyLabel (juce::Label& lab, const juce::String& text, float fontSize = 9.0f)
    {
        lab.setText (text, juce::dontSendNotification);
        lab.setJustificationType (juce::Justification::centred);
        lab.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.65f));
        lab.setFont (juce::FontOptions (fontSize));
    }
}

//==============================================================================
ModSectionComponent::LfoColumn::LfoColumn (juce::AudioProcessorValueTreeState& state, int index,
                                            ModSectionComponent& ownerRef)
    : lfoIndex (index),
      treeState (state),
      ownerSection (ownerRef),
      shapeCombo (state, LfoMod::shapeParamId (index))
{
    title.setText ("LFO " + juce::String (index + 1), juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 190, 150));
    title.setFont (SharedResources::uiFont (11.0f, true));
    addAndMakeVisible (title);

    shapeCombo.setTooltip ("LFO waveform shape");
    addAndMakeVisible (shapeCombo);

    styleSmallButton (prevShape, buttonLf);
    styleSmallButton (nextShape, buttonLf);
    prevShape.setTooltip ("Previous waveform shape");
    nextShape.setTooltip ("Next waveform shape");
    prevShape.onClick = [this] { cycleShape (-1); };
    nextShape.onClick = [this] { cycleShape (1); };
    addAndMakeVisible (prevShape);
    addAndMakeVisible (nextShape);

    styleImageRotary (rateSlider);
    styleImageRotary (phaseSlider);
    rateSlider.onPopupMenu = [this] { showRateModeMenu(); };
    rateSlider.setTooltip ("LFO rate. Right-click to switch between Hz and Sync");
    phaseSlider.setTooltip ("Phase offset in degrees");
    addAndMakeVisible (rateSlider);
    addAndMakeVisible (phaseSlider);

    styleSmallButton (retrigButton, buttonLf);
    wireRetrigButton (retrigButton, treeState, LfoMod::retriggerParamId (index));
    addAndMakeVisible (retrigButton);

    phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        treeState, LfoMod::phaseParamId (index), phaseSlider);

    setupTinyLabel (rateLabel, "Rate", 9.0f * 1.2f);
    setupTinyLabel (phaseLabel, "Phase", 9.0f * 1.2f);
    addAndMakeVisible (rateLabel);
    addAndMakeVisible (phaseLabel);

    treeState.addParameterListener (LfoMod::rateSyncParamId (index), this);
    treeState.addParameterListener (LfoMod::retriggerParamId (index), this);
    syncRateModeUi();
    refreshShapeFromParam();
}

ModSectionComponent::LfoColumn::~LfoColumn()
{
    treeState.removeParameterListener (LfoMod::rateSyncParamId (lfoIndex), this);
    treeState.removeParameterListener (LfoMod::retriggerParamId (lfoIndex), this);
    prevShape.setLookAndFeel (nullptr);
    nextShape.setLookAndFeel (nullptr);
    retrigButton.setLookAndFeel (nullptr);
}

void ModSectionComponent::LfoColumn::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == LfoMod::rateSyncParamId (lfoIndex))
        syncRateModeUi();
    else if (parameterID == LfoMod::retriggerParamId (lfoIndex))
        syncRetrigButton (retrigButton, getRetrigModeIndex (treeState, LfoMod::retriggerParamId (lfoIndex)));
}

void ModSectionComponent::LfoColumn::showRetrigModeMenu()
{
    ::showRetrigModeMenu (&retrigButton, treeState, LfoMod::retriggerParamId (lfoIndex));
}

void ModSectionComponent::LfoColumn::showRateModeMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Hz", true, ! rateSyncMode);
    menu.addItem (2, "Sync", true, rateSyncMode);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rateSlider),
                        [safe = juce::Component::SafePointer<LfoColumn> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
                                    safe->treeState.getParameter (LfoMod::rateSyncParamId (safe->lfoIndex))))
                            {
                                const bool wantSync = (result == 2);
                                if (p->get() == wantSync)
                                    return;
                                p->beginChangeGesture();
                                *p = wantSync;
                                p->endChangeGesture();
                            }
                        });
}

void ModSectionComponent::LfoColumn::syncRateModeUi()
{
    rateSyncMode = treeState.getRawParameterValue (LfoMod::rateSyncParamId (lfoIndex)) != nullptr
                   && treeState.getRawParameterValue (LfoMod::rateSyncParamId (lfoIndex))->load() > 0.5f;

    rateAttach.reset();
    rateSlider.textFromValueFunction = nullptr;
    rateSlider.valueFromTextFunction = nullptr;

    if (rateSyncMode)
    {
        rateLabel.setText ("Sync", juce::dontSendNotification);
        rateSlider.setTooltip ("Tempo sync division. Right-click to switch between Hz and Sync");
        rateSlider.setTextValueSuffix ({});
        rateSlider.textFromValueFunction = [] (double v)
        {
            const int i = juce::jlimit (0, LfoMod::numSyncDivs - 1, (int) std::lround (v));
            return LfoMod::getSyncDivNames()[i];
        };
        rateSlider.valueFromTextFunction = [] (const juce::String& t)
        {
            const auto names = LfoMod::getSyncDivNames();
            for (int i = 0; i < names.size(); ++i)
                if (names[i].equalsIgnoreCase (t.trim()))
                    return (double) i;
            return (double) LfoMod::kDefaultSyncDiv;
        };
        rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            treeState, LfoMod::syncDivParamId (lfoIndex), rateSlider);
    }
    else
    {
        rateLabel.setText ("Rate", juce::dontSendNotification);
        rateSlider.setTooltip ("LFO rate in Hz. Right-click to switch between Hz and Sync");
        rateSlider.setTextValueSuffix (" Hz");
        rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            treeState, LfoMod::rateParamId (lfoIndex), rateSlider);
    }
}

void ModSectionComponent::LfoColumn::cycleShape (int delta)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::shapeParamId (lfoIndex))))
    {
        const int n = p->choices.size();
        if (n <= 0)
            return;
        const int next = (p->getIndex() + delta + n * 8) % n;
        p->beginChangeGesture();
        *p = next;
        p->endChangeGesture();
        refreshShapeFromParam();
        repaint();
    }
}

void ModSectionComponent::LfoColumn::refreshShapeFromParam()
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::shapeParamId (lfoIndex))))
        currentShape = juce::jlimit (0, LfoMod::numShapes - 1, p->getIndex());
}

void ModSectionComponent::LfoColumn::setPlayhead (float phase01, bool active)
{
    playheadPhase = phase01 - std::floor (phase01);
    playheadActive = active;
}

void ModSectionComponent::LfoColumn::paint (juce::Graphics& g)
{
    const auto& c = ownerSection.getSharedColors();
    auto bounds = getLocalBounds().toFloat();
    g.setColour (c.modBackground.withAlpha (220.0f / 255.0f));
    g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);
    g.setColour (c.modBorder.withAlpha (180.0f / 255.0f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);

    auto preview = getLocalBounds();
    preview.removeFromTop (20);
    preview.removeFromTop (20);
    preview.removeFromBottom (78);
    preview = preview.reduced (6, 3);

    g.setColour (c.modBackground.darker (0.35f));
    g.fillRoundedRectangle (preview.toFloat(), 3.0f);
    g.setColour (c.modBorder.withAlpha (160.0f / 255.0f));
    g.drawRoundedRectangle (preview.toFloat(), 3.0f, 1.0f);

    if (preview.getWidth() < 8 || preview.getHeight() < 8)
        return;

    juce::Path wave;
    const float midY = (float) preview.getCentreY();
    const float amp = (float) preview.getHeight() * 0.38f;
    const int steps = juce::jmax (32, preview.getWidth());
    for (int i = 0; i <= steps; ++i)
    {
        const float t = (float) i / (float) steps;
        const float y = midY - LfoMod::shapeAt (currentShape, t) * amp;
        const float x = (float) preview.getX() + t * (float) preview.getWidth();
        if (i == 0)
            wave.startNewSubPath (x, y);
        else
            wave.lineTo (x, y);
    }

    g.setColour (c.modAccent);
    g.strokePath (wave, juce::PathStrokeType (1.5f));

    // Playhead dot only when this LFO is assigned in the mod matrix.
    if (playheadActive)
    {
        const float t = playheadPhase;
        const float x = (float) preview.getX() + t * (float) preview.getWidth();
        const float y = midY - LfoMod::shapeAt (currentShape, t) * amp;

        g.setColour (c.modAccent.brighter (0.35f));
        g.fillEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.drawEllipse (x - 3.5f, y - 3.5f, 7.0f, 7.0f, 1.0f);
    }
}

void ModSectionComponent::LfoColumn::resized()
{
    auto r = getLocalBounds().reduced (3);
    title.setBounds (r.removeFromTop (16));
    r.removeFromTop (2);

    auto shapeRow = r.removeFromTop (18);
    prevShape.setBounds (shapeRow.removeFromLeft (16));
    shapeRow.removeFromLeft (2);
    nextShape.setBounds (shapeRow.removeFromRight (16));
    shapeRow.removeFromRight (2);
    shapeCombo.setBounds (shapeRow);

    auto knobs = r.removeFromBottom (74);
    const int colW = knobs.getWidth() / 2;
    auto k0 = knobs.removeFromLeft (colW);
    auto k1 = knobs;

    auto rateHeader = k0.removeFromTop (14);
    rateLabel.setBounds (rateHeader.removeFromLeft (juce::jmax (28, rateHeader.getWidth() - 18)));
    retrigButton.setBounds (rateHeader);
    phaseLabel.setBounds (k1.removeFromTop (14));
    rateSlider.setBounds (squareKnobBounds (k0, 40));
    phaseSlider.setBounds (squareKnobBounds (k1, 40));
}

//==============================================================================
ModSectionComponent::ShapeColumn::ShapeColumn (EqProcessor& proc, ModSectionComponent& ownerRef)
    : processor (proc),
      owner (ownerRef),
      treeState (proc.treeState),
      editor (proc.getShapeEngine())
{
    title.setText ("Shape", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, juce::Colour::fromRGB (210, 190, 150));
    title.setFont (SharedResources::uiFont (11.0f, true));
    title.setTooltip ("Custom shape modulator. Use as a mod matrix source");
    addAndMakeVisible (title);

    styleSmallButton (expandButton, buttonLf);
    expandButton.setTooltip ("Open larger floating Shape editor");
    expandButton.onClick = [this] { openExpandedEditor(); };
    addAndMakeVisible (expandButton);

    editor.onCurveChanged = [this] { syncSmoothToCurve(); };
    addAndMakeVisible (editor);

    styleImageRotary (rateSlider);
    styleImageRotary (phaseSlider);
    styleImageRotary (smoothSlider);
    rateSlider.onPopupMenu = [this] { showRateModeMenu(); };
    rateSlider.setTooltip ("Shape rate. Right-click to switch between Hz and Sync");
    phaseSlider.setTooltip ("Phase offset in degrees");
    smoothSlider.setTooltip ("Smooth the whole shape curve");
    smoothSlider.setTextValueSuffix (" %");
    addAndMakeVisible (rateSlider);
    addAndMakeVisible (phaseSlider);
    addAndMakeVisible (smoothSlider);

    styleSmallButton (retrigButton, buttonLf);
    wireRetrigButton (retrigButton, treeState, ShapeMod::retriggerParamId());
    addAndMakeVisible (retrigButton);

    phaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        treeState, ShapeMod::phaseParamId(), phaseSlider);
    smoothAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        treeState, ShapeMod::smoothParamId(), smoothSlider);

    setupTinyLabel (rateLabel, "Rate", 9.0f * 1.2f);
    setupTinyLabel (phaseLabel, "Phase", 9.0f * 1.2f);
    setupTinyLabel (smoothLabel, "Smooth", 9.0f * 1.2f);
    addAndMakeVisible (rateLabel);
    addAndMakeVisible (phaseLabel);
    addAndMakeVisible (smoothLabel);

    treeState.addParameterListener (ShapeMod::rateSyncParamId(), this);
    treeState.addParameterListener (ShapeMod::smoothParamId(), this);
    treeState.addParameterListener (ShapeMod::retriggerParamId(), this);
    syncRateModeUi();
    syncSmoothToCurve();
}

ModSectionComponent::ShapeColumn::~ShapeColumn()
{
    treeState.removeParameterListener (ShapeMod::rateSyncParamId(), this);
    treeState.removeParameterListener (ShapeMod::smoothParamId(), this);
    treeState.removeParameterListener (ShapeMod::retriggerParamId(), this);
    retrigButton.setLookAndFeel (nullptr);
    expandButton.setLookAndFeel (nullptr);
}

void ModSectionComponent::ShapeColumn::openExpandedEditor()
{
    owner.showShapeEditorPopup();
}

void ModSectionComponent::ShapeColumn::showRetrigModeMenu()
{
    ::showRetrigModeMenu (&retrigButton, treeState, ShapeMod::retriggerParamId());
}

void ModSectionComponent::ShapeColumn::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == ShapeMod::rateSyncParamId())
        syncRateModeUi();
    else if (parameterID == ShapeMod::smoothParamId())
        syncSmoothToCurve();
    else if (parameterID == ShapeMod::retriggerParamId())
        syncRetrigButton (retrigButton, getRetrigModeIndex (treeState, ShapeMod::retriggerParamId()));
}

void ModSectionComponent::ShapeColumn::syncSmoothToCurve()
{
    float sm = 0.0f;
    if (auto* v = treeState.getRawParameterValue (ShapeMod::smoothParamId()))
        sm = v->load();
    processor.getShapeEngine().uiCurve.setSmoothPercent (sm);
    processor.getShapeEngine().publishFromUi();
    editor.repaint();
}

void ModSectionComponent::ShapeColumn::showRateModeMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Hz", true, ! rateSyncMode);
    menu.addItem (2, "Sync", true, rateSyncMode);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&rateSlider),
                        [safe = juce::Component::SafePointer<ShapeColumn> (this)] (int result)
                        {
                            if (safe == nullptr || result <= 0)
                                return;

                            if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
                                    safe->treeState.getParameter (ShapeMod::rateSyncParamId())))
                            {
                                const bool wantSync = (result == 2);
                                if (p->get() == wantSync)
                                    return;
                                p->beginChangeGesture();
                                *p = wantSync;
                                p->endChangeGesture();
                            }
                        });
}

void ModSectionComponent::ShapeColumn::syncRateModeUi()
{
    rateSyncMode = treeState.getRawParameterValue (ShapeMod::rateSyncParamId()) != nullptr
                   && treeState.getRawParameterValue (ShapeMod::rateSyncParamId())->load() > 0.5f;

    rateAttach.reset();
    rateSlider.textFromValueFunction = nullptr;
    rateSlider.valueFromTextFunction = nullptr;

    if (rateSyncMode)
    {
        rateLabel.setText ("Sync", juce::dontSendNotification);
        rateSlider.setTooltip ("Tempo sync division. Right-click to switch between Hz and Sync");
        rateSlider.setTextValueSuffix ({});
        rateSlider.textFromValueFunction = [] (double v)
        {
            const int i = juce::jlimit (0, LfoMod::numSyncDivs - 1, (int) std::lround (v));
            return LfoMod::getSyncDivNames()[i];
        };
        rateSlider.valueFromTextFunction = [] (const juce::String& t)
        {
            const auto names = LfoMod::getSyncDivNames();
            for (int i = 0; i < names.size(); ++i)
                if (names[i].equalsIgnoreCase (t.trim()))
                    return (double) i;
            return (double) LfoMod::kDefaultSyncDiv;
        };
        rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            treeState, ShapeMod::syncDivParamId(), rateSlider);
    }
    else
    {
        rateLabel.setText ("Rate", juce::dontSendNotification);
        rateSlider.setTooltip ("Shape rate in Hz. Right-click to switch between Hz and Sync");
        rateSlider.setTextValueSuffix (" Hz");
        rateAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            treeState, ShapeMod::rateParamId(), rateSlider);
    }
}

void ModSectionComponent::ShapeColumn::refreshPlayhead()
{
    const bool active = LfoMod::isSourceAssignedInMatrix (treeState, LfoMod::srcShape);
    editor.setPlayheadActive (active);
    editor.setPlayheadPhase (processor.getPublishedShapePhase());
    editor.repaint();
}

void ModSectionComponent::ShapeColumn::paint (juce::Graphics& g)
{
    const auto& c = owner.getSharedColors();
    auto bounds = getLocalBounds().toFloat();
    g.setColour (c.modBackground.withAlpha (220.0f / 255.0f));
    g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);
    g.setColour (c.modBorder.withAlpha (180.0f / 255.0f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);
}

void ModSectionComponent::ShapeColumn::resized()
{
    auto r = getLocalBounds().reduced (3);
    auto titleRow = r.removeFromTop (16);
    expandButton.setBounds (titleRow.removeFromRight (16));
    titleRow.removeFromRight (2);
    title.setBounds (titleRow);
    r.removeFromTop (2);

    auto knobs = r.removeFromBottom (70);
    const int colW = knobs.getWidth() / 3;
    auto k0 = knobs.removeFromLeft (colW);
    auto k1 = knobs.removeFromLeft (colW);
    auto k2 = knobs;

    auto rateHeader = k0.removeFromTop (14);
    rateLabel.setBounds (rateHeader.removeFromLeft (juce::jmax (24, rateHeader.getWidth() - 16)));
    retrigButton.setBounds (rateHeader);
    phaseLabel.setBounds (k1.removeFromTop (14));
    smoothLabel.setBounds (k2.removeFromTop (14));
    rateSlider.setBounds (squareKnobBounds (k0, 36));
    phaseSlider.setBounds (squareKnobBounds (k1, 36));
    smoothSlider.setBounds (squareKnobBounds (k2, 36));

    editor.setBounds (r);
}

//==============================================================================
ModSectionComponent::EnvFollowerColumn::EnvFollowerColumn (EqProcessor& proc,
                                                           ModSectionComponent& ownerRef)
    : processor (proc),
      ownerSection (ownerRef)
{
    title.setText ("Env", juce::dontSendNotification);
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, ownerSection.getSharedColors().modText);
    title.setFont (SharedResources::uiFont (11.0f, true));
    title.setTooltip ("Envelope follower. Use as a mod matrix source");
    addAndMakeVisible (title);

    styleVerticalThresh (thresholdSlider);
    // Match LFO Rate/Phase: square image face, no text-box that shrinks the knob art.
    styleImageRotary (attackSlider);
    styleImageRotary (releaseSlider);
    attackSlider.setTooltip ("Envelope follower attack time in milliseconds");
    releaseSlider.setTooltip ("Envelope follower release time in milliseconds");
    addAndMakeVisible (thresholdSlider);
    addAndMakeVisible (attackSlider);
    addAndMakeVisible (releaseSlider);

    setupTinyLabel (threshLabel, "Thresh");
    setupTinyLabel (attackLabel, "Attack", 9.0f * 1.2f);
    setupTinyLabel (releaseLabel, "Release", 9.0f * 1.2f);
    addAndMakeVisible (threshLabel);
    addAndMakeVisible (attackLabel);
    addAndMakeVisible (releaseLabel);

    auto& state = processor.treeState;
    threshAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, LfoMod::envThresholdParamId(), thresholdSlider);
    attackAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, LfoMod::envAttackParamId(), attackSlider);
    releaseAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, LfoMod::envReleaseParamId(), releaseSlider);
}

void ModSectionComponent::EnvFollowerColumn::refreshMeter()
{
    displayedEnvDb = processor.getPublishedEnvDb();
    attackSlider.setTooltip ("Attack: " + juce::String (attackSlider.getValue(), 1) + " ms");
    releaseSlider.setTooltip ("Release: " + juce::String (releaseSlider.getValue(), 1) + " ms");
    repaint();
}

void ModSectionComponent::EnvFollowerColumn::paint (juce::Graphics& g)
{
    const auto& c = ownerSection.getSharedColors();
    auto bounds = getLocalBounds().toFloat();
    g.setColour (c.modBackground.withAlpha (220.0f / 255.0f));
    g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);
    g.setColour (c.modBorder.withAlpha (180.0f / 255.0f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 1.0f);

    // Level meter behind the vertical threshold slider (track is transparent).
    auto meter = thresholdSlider.getBounds().toFloat();
    meter.removeFromBottom (14.0f); // TextBoxBelow
    paintEnvThresholdMeter (g, meter, displayedEnvDb, (float) thresholdSlider.getValue(), c);
}

void ModSectionComponent::EnvFollowerColumn::resized()
{
    auto r = getLocalBounds().reduced (3);
    title.setBounds (r.removeFromTop (16));
    r.removeFromTop (2);

    // Same bottom knob row proportions as LFO Rate/Phase.
    auto knobs = r.removeFromBottom (74);
    const int colW = knobs.getWidth() / 2;
    auto k0 = knobs.removeFromLeft (colW);
    auto k1 = knobs;
    attackLabel.setBounds (k0.removeFromTop (14));
    releaseLabel.setBounds (k1.removeFromTop (14));
    attackSlider.setBounds (squareKnobBounds (k0, 40));
    releaseSlider.setBounds (squareKnobBounds (k1, 40));

    threshLabel.setBounds (r.removeFromTop (12));
    thresholdSlider.setBounds (r.withTrimmedBottom (2));
}

//==============================================================================
ModSectionComponent::MatrixRow::MatrixRow (juce::AudioProcessorValueTreeState& state,
                                           int slotIndex,
                                           TextButtonLookAndFeel& buttonLf)
    : slot (slotIndex),
      treeState (state),
      sourceCombo (state, LfoMod::slotSourceParamId (slotIndex)),
      destCombo (state, LfoMod::slotDestParamId (slotIndex))
{
    indexLabel.setText (juce::String (slotIndex + 1).paddedLeft ('0', 2), juce::dontSendNotification);
    indexLabel.setJustificationType (juce::Justification::centred);
    indexLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.55f));
    indexLabel.setFont (SharedResources::uiFont (10.0f));
    indexLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (indexLabel);

    sourceCombo.setTooltip ("Modulation source");
    addAndMakeVisible (sourceCombo);

    styleSmallButton (polarButton, buttonLf);
    polarButton.setClickingTogglesState (true);
    polarButton.setTooltip ("Toggle bipolar or polar modulation. Bipolar moves both ways around the base value. Polar moves in one direction only");
    addAndMakeVisible (polarButton);
    polarAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, LfoMod::slotPolarParamId (slotIndex), polarButton);

    styleAmountSlider (amountSlider);
    addAndMakeVisible (amountSlider);
    amountAttach = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        state, LfoMod::slotAmountParamId (slotIndex), amountSlider);

    destCombo.setTooltip ("Modulation destination");
    addAndMakeVisible (destCombo);

    enabledToggle.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke.withAlpha (0.75f));
    enabledToggle.setColour (juce::ToggleButton::tickColourId, juce::Colour::fromRGB (220, 180, 90));
    enabledToggle.setTooltip ("Enable or temporarily bypass this routing");
    addAndMakeVisible (enabledToggle);
    enabledAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        state, LfoMod::slotEnabledParamId (slotIndex), enabledToggle);

    treeState.addParameterListener (LfoMod::slotPolarParamId (slotIndex), this);
    syncPolarButton();
}

ModSectionComponent::MatrixRow::~MatrixRow()
{
    treeState.removeParameterListener (LfoMod::slotPolarParamId (slot), this);
    polarButton.setLookAndFeel (nullptr);
}

void ModSectionComponent::MatrixRow::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == LfoMod::slotPolarParamId (slot))
        syncPolarButton();
}

void ModSectionComponent::MatrixRow::syncPolarButton()
{
    const bool polar = treeState.getRawParameterValue (LfoMod::slotPolarParamId (slot)) != nullptr
                       && treeState.getRawParameterValue (LfoMod::slotPolarParamId (slot))->load() > 0.5f;
    polarButton.setButtonText (polar ? "<|" : "<>");
}

void ModSectionComponent::MatrixRow::resized()
{
    auto r = getLocalBounds();
    indexLabel.setBounds (r.removeFromLeft (18));
    r.removeFromLeft (2);
    enabledToggle.setBounds (r.removeFromRight (36));
    r.removeFromRight (2);

    // Fixed mins so source/dest stay usable; amount takes the flexible middle.
    const int sourceW = juce::jmax (64, r.getWidth() * 22 / 100);
    const int polarW = 26;
    const int destW = juce::jmax (72, r.getWidth() * 28 / 100);

    sourceCombo.setBounds (r.removeFromLeft (sourceW).reduced (0, 1));
    r.removeFromLeft (2);
    polarButton.setBounds (r.removeFromLeft (polarW).reduced (0, 1));
    r.removeFromLeft (2);
    destCombo.setBounds (r.removeFromRight (destW).reduced (0, 1));
    r.removeFromRight (2);
    amountSlider.setBounds (r.reduced (0, 2));

    // Keep combos above amount/polar for hit-testing if layout is tight.
    sourceCombo.toFront (false);
    destCombo.toFront (false);
}

ModSectionComponent::MatrixContent::MatrixContent (juce::AudioProcessorValueTreeState& state,
                                                   TextButtonLookAndFeel& buttonLf)
{
    for (int i = 0; i < LfoMod::kNumMatrixSlots; ++i)
    {
        rows[(size_t) i] = std::make_unique<MatrixRow> (state, i, buttonLf);
        addAndMakeVisible (*rows[(size_t) i]);
    }
    setSize (300, LfoMod::kNumMatrixSlots * (kRowH + kRowGap));
}

void ModSectionComponent::MatrixContent::resized()
{
    auto area = getLocalBounds();
    for (int i = 0; i < LfoMod::kNumMatrixSlots; ++i)
    {
        if (rows[(size_t) i] != nullptr)
            rows[(size_t) i]->setBounds (area.removeFromTop (kRowH));
        area.removeFromTop (kRowGap);
    }
}

//==============================================================================
ModSectionComponent::ModSectionComponent (EqProcessor& proc)
    : processor (proc),
      treeState (proc.treeState)
{
    for (int i = 0; i < LfoMod::kNumLfos; ++i)
    {
        columns[(size_t) i] = std::make_unique<LfoColumn> (treeState, i, *this);
        addAndMakeVisible (*columns[(size_t) i]);
    }

    shapeColumn = std::make_unique<ShapeColumn> (processor, *this);
    addAndMakeVisible (*shapeColumn);

    envColumn = std::make_unique<EnvFollowerColumn> (processor, *this);
    addAndMakeVisible (*envColumn);

    matrixTitle.setText ("Mod Matrix", juce::dontSendNotification);
    matrixTitle.setJustificationType (juce::Justification::centredLeft);
    matrixTitle.setColour (juce::Label::textColourId, colors().modText);
    matrixTitle.setFont (SharedResources::uiFont (13.0f, true));
    addAndMakeVisible (matrixTitle);

    matrixContent = std::make_unique<MatrixContent> (treeState, sharedButtonLf);
    matrixViewport.setViewedComponent (matrixContent.get(), false);
    matrixViewport.setScrollBarsShown (true, false);
    {
        auto& bar = matrixViewport.getVerticalScrollBar();
        bar.setColour (juce::ScrollBar::backgroundColourId, juce::Colour::fromRGB (20, 14, 10));
        bar.setColour (juce::ScrollBar::thumbColourId, juce::Colour::fromRGB (120, 95, 55));
        bar.setColour (juce::ScrollBar::trackColourId, juce::Colour::fromRGB (40, 30, 22));
    }
    addAndMakeVisible (matrixViewport);

    applyThemeToChildControls();
    startTimerHz (24);
}

ModSectionComponent::~ModSectionComponent()
{
    stopTimer();
    shapePopup.reset();
    matrixViewport.setViewedComponent (nullptr, false);
}

void ModSectionComponent::showShapeEditorPopup()
{
    auto* host = findParentComponentOfClass<EqEditor>();
    if (host == nullptr)
        host = dynamic_cast<EqEditor*> (getTopLevelComponent());

    if (host == nullptr)
        return;

    if (shapePopup == nullptr)
        shapePopup = std::make_unique<ShapeEditorPopup> (processor);

    shapePopup->setThemeColors (themeColors);

    shapePopup->onClosed = [this]
    {
        // Keep instance around for quick reopen; just hide.
    };

    shapePopup->showCenteredIn (*host);
}

void ModSectionComponent::timerCallback()
{
    for (auto& col : columns)
        if (col != nullptr)
        {
            col->refreshShapeFromParam();
            const int src = LfoMod::srcLfo1 + col->lfoIndex;
            const bool active = LfoMod::isSourceAssignedInMatrix (treeState, src);
            col->setPlayhead (processor.getPublishedLfoPhase (col->lfoIndex), active);
            col->repaint();
        }

    if (shapeColumn != nullptr)
        shapeColumn->refreshPlayhead();

    if (envColumn != nullptr)
        envColumn->refreshMeter();
}

void ModSectionComponent::paint (juce::Graphics& g)
{
    const auto& c = colors();
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient grad (c.modBackground,
                               bounds.getTopLeft(),
                               c.modBackground.darker (0.55f),
                               bounds.getBottomRight(),
                               false);
    g.setGradientFill (grad);
    g.fillRect (bounds);

    g.setColour (c.modBorder.withAlpha (200.0f / 255.0f));
    g.drawLine (0.0f, 0.5f, bounds.getWidth(), 0.5f, 1.5f);
    g.drawLine (0.0f, bounds.getHeight() - 0.5f, bounds.getWidth(), bounds.getHeight() - 0.5f, 1.5f);

    const float sepX = bounds.getWidth() * 0.55f;
    g.setColour (c.modBorder.withAlpha (160.0f / 255.0f));
    g.drawLine (sepX, 8.0f, sepX, bounds.getHeight() - 8.0f, 1.2f);
}

const SharedColors& ModSectionComponent::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void ModSectionComponent::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    applyThemeToChildControls();
    repaint();
}

void ModSectionComponent::applyThemeToChildControls()
{
    const auto& c = colors();
    const auto panelBg = c.modBackground;
    const auto buttonBg = c.modBackground.brighter (0.15f);
    // Per-surface ink: Legible text resolves against actual panel / button fills.
    const auto primaryInk = c.legibleTextOn (c.modText, panelBg);
    const auto secondaryInk = c.enforceLegibleText
                                  ? c.legibleTextOn (c.modText.withAlpha (0.90f), panelBg)
                                  : c.modText.withAlpha (0.65f);
    const auto buttonInk = c.legibleTextOn (c.modText, buttonBg);
    const auto sliderTextInk = c.legibleTextOn (c.modText, panelBg.darker (0.35f));

    ComboBoxLookAndFeel::sharedForPopupMenus().setThemeColors (themeColors);
    matrixTitle.setColour (juce::Label::textColourId, primaryInk);
    matrixTitle.setMinimumHorizontalScale (1.0f);

    auto styleModButton = [&] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, buttonBg);
        b.setColour (juce::TextButton::buttonOnColourId, c.modAccent);
        b.setColour (juce::TextButton::textColourOffId, buttonInk);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };

    auto styleTinyLabel = [&] (juce::Label& lab, bool secondary)
    {
        lab.setColour (juce::Label::textColourId, secondary ? secondaryInk : primaryInk);
        lab.setMinimumHorizontalScale (1.0f);
    };

    auto styleAmount = [&] (juce::Slider& s)
    {
        s.setColour (juce::Slider::trackColourId, c.modBackground.darker (0.25f));
        s.setColour (juce::Slider::backgroundColourId, c.modBackground.darker (0.45f));
        s.setColour (juce::Slider::thumbColourId, c.modAccent);
        s.setColour (juce::Slider::textBoxTextColourId, sliderTextInk.withAlpha (0.92f));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    };

    auto styleThresh = [&] (juce::Slider& s)
    {
        s.setColour (juce::Slider::trackColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::thumbColourId, c.modAccent);
        s.setColour (juce::Slider::textBoxTextColourId, sliderTextInk.withAlpha (0.92f));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    };

    for (auto& col : columns)
        if (col != nullptr)
        {
            styleTinyLabel (col->title, false);
            styleTinyLabel (col->rateLabel, true);
            styleTinyLabel (col->phaseLabel, true);
            styleModButton (col->prevShape);
            styleModButton (col->nextShape);
            styleModButton (col->retrigButton);
            col->rateSlider.setThemeColors (themeColors);
            col->phaseSlider.setThemeColors (themeColors);
            col->repaint();
        }

    if (shapeColumn != nullptr)
    {
        styleTinyLabel (shapeColumn->title, false);
        styleTinyLabel (shapeColumn->rateLabel, true);
        styleTinyLabel (shapeColumn->phaseLabel, true);
        styleTinyLabel (shapeColumn->smoothLabel, true);
        styleModButton (shapeColumn->expandButton);
        styleModButton (shapeColumn->retrigButton);
        shapeColumn->rateSlider.setThemeColors (themeColors);
        shapeColumn->phaseSlider.setThemeColors (themeColors);
        shapeColumn->smoothSlider.setThemeColors (themeColors);
        shapeColumn->editor.setThemeColors (themeColors);
        shapeColumn->repaint();
    }

    if (shapePopup != nullptr)
        shapePopup->setThemeColors (themeColors);

    if (envColumn != nullptr)
    {
        styleTinyLabel (envColumn->title, false);
        styleTinyLabel (envColumn->threshLabel, true);
        styleTinyLabel (envColumn->attackLabel, true);
        styleTinyLabel (envColumn->releaseLabel, true);
        styleThresh (envColumn->thresholdSlider);
        envColumn->attackSlider.setThemeColors (themeColors);
        envColumn->releaseSlider.setThemeColors (themeColors);
        envColumn->repaint();
    }

    // Matrix rows: index / On / amount / polar used hardcoded whitesmoke — theme them.
    if (matrixContent != nullptr)
    {
        for (auto& row : matrixContent->rows)
        {
            if (row == nullptr)
                continue;
            styleTinyLabel (row->indexLabel, true);
            styleModButton (row->polarButton);
            styleAmount (row->amountSlider);
            row->enabledToggle.setColour (juce::ToggleButton::textColourId, secondaryInk);
            row->enabledToggle.setColour (juce::ToggleButton::tickColourId, c.modAccent);
            row->repaint();
        }
    }

    {
        auto& bar = matrixViewport.getVerticalScrollBar();
        bar.setColour (juce::ScrollBar::backgroundColourId, c.modBackground.darker (0.45f));
        bar.setColour (juce::ScrollBar::thumbColourId, c.modAccent.withMultipliedBrightness (0.75f));
        bar.setColour (juce::ScrollBar::trackColourId, c.modBackground.darker (0.25f));
    }
}

void ModSectionComponent::resized()
{
    auto area = getLocalBounds().reduced (6, 4);
    const int matrixW = juce::jmax (280, (int) (area.getWidth() * 0.45f));
    auto matrix = area.removeFromRight (matrixW);
    area.removeFromRight (8);

    // Left: narrower LFOs + Shape (wider for graph) + compact Env.
    const int colGap = 3;
    const int envW = juce::jmax (88, area.getWidth() * 18 / 100);
    const int shapeW = juce::jmax (110, area.getWidth() * 22 / 100);
    auto envArea = area.removeFromRight (envW);
    area.removeFromRight (colGap);
    auto shapeArea = area.removeFromRight (shapeW);
    area.removeFromRight (colGap);

    const int colW = (area.getWidth() - colGap * (LfoMod::kNumLfos - 1)) / LfoMod::kNumLfos;
    for (int i = 0; i < LfoMod::kNumLfos; ++i)
    {
        if (columns[(size_t) i] != nullptr)
            columns[(size_t) i]->setBounds (area.removeFromLeft (colW));
        if (i + 1 < LfoMod::kNumLfos)
            area.removeFromLeft (colGap);
    }

    if (shapeColumn != nullptr)
        shapeColumn->setBounds (shapeArea);

    if (envColumn != nullptr)
        envColumn->setBounds (envArea);

    matrixTitle.setBounds (matrix.removeFromTop (20));
    matrix.removeFromTop (2);
    matrixViewport.setBounds (matrix);

    if (matrixContent != nullptr)
    {
        const int contentH = LfoMod::kNumMatrixSlots * (MatrixContent::kRowH + MatrixContent::kRowGap);
        matrixContent->setSize (juce::jmax (280, matrixViewport.getWidth() - 14), contentH);
        matrixContent->resized();
    }
}
