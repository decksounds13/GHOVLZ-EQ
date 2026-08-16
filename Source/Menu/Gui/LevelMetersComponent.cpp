#include "LevelMetersComponent.h"

#include "../../MainComponent.h"
#include "../SharedResources.h"
#include "../../ModuleLookPresets.h"
#include "../AnalyserDefaults.h"
#include "../Menu.h"
#include "../../Dyn/DynParams.h"

namespace
{
    constexpr int kMeterPadX = 16;
    constexpr int kMeterPadY = 10;
    constexpr int kMeterLabelH = 18;
    constexpr int kMeterSliderH = 24;
    constexpr int kMeterRowGap = 6;
    constexpr int kMeterLabelGap = 2;
    constexpr int kMeterToggleH = 22;
}

LevelMetersComponent::Content::Content (SharedResources& resources,
                                        juce::AudioProcessorValueTreeState& state,
                                        ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      peakRampEditor (resources, GradientStripEditor::ModeFamily::intensity, &ramps.getPresets()),
      rmsRampEditor (resources, GradientStripEditor::ModeFamily::intensity, &ramps.getPresets()),
      displaySection (resources, "meters.display", "Display", false),
      grSection (resources, "meters.gr", "Gain Reduction", true),
      peakSection (resources, "meters.peak", "Peak", false),
      rmsSection (resources, "meters.rms", "RMS", false)
{
    titleLabel.setText ("Level Meters", juce::dontSendNotification);
    titleLabel.setFont (SharedResources::uiFont (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    modeLabel.setText ("Meter Mode", juce::dontSendNotification);
    styleModeCombo (modeCombo);
    modeCombo.addItem ("Peak", 1);
    modeCombo.addItem ("RMS", 2);
    modeCombo.addItem ("Peak+RMS", 3);
    addAndMakeVisible (modeLabel);
    addAndMakeVisible (modeCombo);
    modeAttachment = std::make_unique<ComboBoxAttachment> (treeState, "METER_MODE_ID", modeCombo);

    channelModeLabel.setText ("Channel Mode", juce::dontSendNotification);
    styleModeCombo (channelModeCombo);
    channelModeCombo.setLookAndFeel (&channelModeLookAndFeel);
    channelModeCombo.addItem ("L/R", 1);
    channelModeCombo.addItem ("M/S", 2);
    addAndMakeVisible (channelModeLabel);
    addAndMakeVisible (channelModeCombo);
    channelModeAttachment = std::make_unique<ComboBoxAttachment> (
        treeState, "METER_CHANNEL_MODE_ID", channelModeCombo);

    readoutIntegrationLabel.setText ("Readout Averaging", juce::dontSendNotification);
    fallLabel.setText ("Meter Fall Time", juce::dontSendNotification);
    peakHoldLabel.setText ("Peak Hold", juce::dontSendNotification);
    clipHoldLabel.setText ("Clip Indicator Hold", juce::dontSendNotification);
    clipThresholdLabel.setText ("Clip Threshold", juce::dontSendNotification);

    styleSlider (readoutIntegrationSlider);
    readoutIntegrationSlider.setTextValueSuffix (" ms");
    addAndMakeVisible (readoutIntegrationSlider);
    readoutIntegrationAttachment = std::make_unique<SliderAttachment> (
        treeState, "METER_READOUT_INTEGRATION_ID", readoutIntegrationSlider);

    styleSlider (fallSlider);
    fallSlider.setTextValueSuffix (" s");
    addAndMakeVisible (fallSlider);
    fallAttachment = std::make_unique<SliderAttachment> (treeState, "METER_FALL_ID", fallSlider);

    styleSlider (peakHoldSlider);
    peakHoldSlider.setTextValueSuffix (" s");
    addAndMakeVisible (peakHoldSlider);
    peakHoldAttachment = std::make_unique<SliderAttachment> (treeState, "METER_PEAK_HOLD_ID", peakHoldSlider);

    styleSlider (clipHoldSlider);
    clipHoldSlider.setTextValueSuffix (" s");
    addAndMakeVisible (clipHoldSlider);
    clipHoldAttachment = std::make_unique<SliderAttachment> (treeState, "METER_CLIP_HOLD_ID", clipHoldSlider);

    styleSlider (clipThresholdSlider);
    clipThresholdSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (clipThresholdSlider);
    clipThresholdAttachment = std::make_unique<SliderAttachment> (
        treeState, "METER_CLIP_THRESHOLD_ID", clipThresholdSlider);

    grAvgLabel.setText ("GR Averaging", juce::dontSendNotification);
    styleLabel (grAvgLabel);
    styleSlider (grAvgSlider);
    grAvgSlider.setTextValueSuffix (" ms");
    addAndMakeVisible (grAvgSlider);
    grAvgAttachment = std::make_unique<SliderAttachment> (
        treeState, DynParams::grAvgMsId(), grAvgSlider);

    grFallLabel.setText ("GR Fall Time", juce::dontSendNotification);
    styleLabel (grFallLabel);
    styleSlider (grFallSlider);
    grFallSlider.setTextValueSuffix (" ms");
    addAndMakeVisible (grFallSlider);
    grFallAttachment = std::make_unique<SliderAttachment> (
        treeState, DynParams::grFallMsId(), grFallSlider);

    // ---- Peak ramp + glow ----
    peakRampSectionLabel.setText ("Peak Colour", juce::dontSendNotification);
    styleSectionLabel (peakRampSectionLabel);
    addAndMakeVisible (peakRampSectionLabel);

    peakRampLabel.setText ("Peak Ramp", juce::dontSendNotification);
    styleLabel (peakRampLabel);
    peakRampLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (peakRampLabel);

    peakRampEditor.setRamp (&colourRamps.get (ColourRampBank::Target::meterPeak));
    peakRampEditor.onRampChanged = [this]
    {
        colourRamps.notifyEdited();
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->applyColourRampsToMeters();
    };
    peakRampEditor.onRampPreview = [this]
    {
        colourRamps.notifyPreview();
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->applyColourRampsToMeters();
    };
    peakRampEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::meterPeak);
    };
    peakRampEditor.onPreferredHeightChanged = [this] { requestParentRelayout(); };
    addAndMakeVisible (peakRampEditor);

    styleToggle (peakGlowToggle);
    peakGlowToggle.setTooltip ("Glow the peak bar when its level is above the threshold.");
    peakGlowToggle.onClick = [this]
    {
        updateGlowVisibility();
        requestParentRelayout();
    };
    addAndMakeVisible (peakGlowToggle);
    peakGlowAttachment = std::make_unique<ButtonAttachment> (
        treeState, "METER_PEAK_GLOW_ENABLE_ID", peakGlowToggle);

    auto setupGlowSlider = [this] (juce::Label& lab, juce::Slider& s,
                                   std::unique_ptr<SliderAttachment>& att,
                                   const char* id, const char* text, const char* suffix)
    {
        lab.setText (text, juce::dontSendNotification);
        styleLabel (lab);
        styleSlider (s);
        if (suffix != nullptr)
            s.setTextValueSuffix (suffix);
        addAndMakeVisible (lab);
        addAndMakeVisible (s);
        att = std::make_unique<SliderAttachment> (treeState, id, s);
    };

    setupGlowSlider (peakGlowThresholdLabel, peakGlowThresholdSlider, peakGlowThresholdAttachment,
                     "METER_PEAK_GLOW_THRESHOLD_ID", "Peak Glow Threshold", " dB");
    setupGlowSlider (peakGlowRadiusLabel, peakGlowRadiusSlider, peakGlowRadiusAttachment,
                     "METER_PEAK_GLOW_RADIUS_ID", "Peak Glow Radius", " px");
    setupGlowSlider (peakGlowSpreadLabel, peakGlowSpreadSlider, peakGlowSpreadAttachment,
                     "METER_PEAK_GLOW_SPREAD_ID", "Peak Glow Spread", " px");
    setupGlowSlider (peakGlowOpacityLabel, peakGlowOpacitySlider, peakGlowOpacityAttachment,
                     "METER_PEAK_GLOW_OPACITY_ID", "Peak Glow Opacity", " %");

    // ---- RMS ramp + glow ----
    rmsRampSectionLabel.setText ("RMS Colour", juce::dontSendNotification);
    styleSectionLabel (rmsRampSectionLabel);
    addAndMakeVisible (rmsRampSectionLabel);

    rmsRampLabel.setText ("RMS Ramp", juce::dontSendNotification);
    styleLabel (rmsRampLabel);
    rmsRampLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (rmsRampLabel);

    rmsRampEditor.setRamp (&colourRamps.get (ColourRampBank::Target::meterRms));
    rmsRampEditor.onRampChanged = [this]
    {
        colourRamps.notifyEdited();
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->applyColourRampsToMeters();
    };
    rmsRampEditor.onRampPreview = [this]
    {
        colourRamps.notifyPreview();
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->applyColourRampsToMeters();
    };
    rmsRampEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::meterRms);
    };
    rmsRampEditor.onPreferredHeightChanged = [this] { requestParentRelayout(); };
    addAndMakeVisible (rmsRampEditor);

    styleToggle (rmsGlowToggle);
    rmsGlowToggle.setTooltip ("Glow the RMS bar when its level is above the threshold.");
    rmsGlowToggle.onClick = [this]
    {
        updateGlowVisibility();
        requestParentRelayout();
    };
    addAndMakeVisible (rmsGlowToggle);
    rmsGlowAttachment = std::make_unique<ButtonAttachment> (
        treeState, "METER_RMS_GLOW_ENABLE_ID", rmsGlowToggle);

    setupGlowSlider (rmsGlowThresholdLabel, rmsGlowThresholdSlider, rmsGlowThresholdAttachment,
                     "METER_RMS_GLOW_THRESHOLD_ID", "RMS Glow Threshold", " dB");
    setupGlowSlider (rmsGlowRadiusLabel, rmsGlowRadiusSlider, rmsGlowRadiusAttachment,
                     "METER_RMS_GLOW_RADIUS_ID", "RMS Glow Radius", " px");
    setupGlowSlider (rmsGlowSpreadLabel, rmsGlowSpreadSlider, rmsGlowSpreadAttachment,
                     "METER_RMS_GLOW_SPREAD_ID", "RMS Glow Spread", " px");
    setupGlowSlider (rmsGlowOpacityLabel, rmsGlowOpacitySlider, rmsGlowOpacityAttachment,
                     "METER_RMS_GLOW_OPACITY_ID", "RMS Glow Opacity", " %");

    styleLabel (titleLabel);
    styleLabel (modeLabel);
    styleLabel (channelModeLabel);
    styleLabel (readoutIntegrationLabel);
    styleLabel (fallLabel);
    styleLabel (peakHoldLabel);
    styleLabel (clipHoldLabel);
    styleLabel (clipThresholdLabel);

    addAndMakeVisible (readoutIntegrationLabel);
    addAndMakeVisible (fallLabel);
    addAndMakeVisible (peakHoldLabel);
    addAndMakeVisible (clipHoldLabel);
    addAndMakeVisible (clipThresholdLabel);
    addAndMakeVisible (grAvgLabel);
    addAndMakeVisible (grFallLabel);

    updateGlowVisibility();

    wireSection (displaySection);
    wireSection (grSection);
    wireSection (peakSection);
    wireSection (rmsSection);
}

void LevelMetersComponent::Content::wireSection (SettingsSection& section)
{
    addAndMakeVisible (section);
    section.onChanged = [this]
    {
        resized();
        requestParentRelayout();
    };
}

LevelMetersComponent::Content::~Content()
{
    modeCombo.setLookAndFeel (nullptr);
    channelModeCombo.setLookAndFeel (nullptr);
}

void LevelMetersComponent::Content::updateGlowVisibility()
{
    const bool peakOn = peakGlowToggle.getToggleState();
    peakGlowThresholdLabel.setVisible (peakOn);
    peakGlowThresholdSlider.setVisible (peakOn);
    peakGlowRadiusLabel.setVisible (peakOn);
    peakGlowRadiusSlider.setVisible (peakOn);
    peakGlowSpreadLabel.setVisible (peakOn);
    peakGlowSpreadSlider.setVisible (peakOn);
    peakGlowOpacityLabel.setVisible (peakOn);
    peakGlowOpacitySlider.setVisible (peakOn);

    const bool rmsOn = rmsGlowToggle.getToggleState();
    rmsGlowThresholdLabel.setVisible (rmsOn);
    rmsGlowThresholdSlider.setVisible (rmsOn);
    rmsGlowRadiusLabel.setVisible (rmsOn);
    rmsGlowRadiusSlider.setVisible (rmsOn);
    rmsGlowSpreadLabel.setVisible (rmsOn);
    rmsGlowSpreadSlider.setVisible (rmsOn);
    rmsGlowOpacityLabel.setVisible (rmsOn);
    rmsGlowOpacitySlider.setVisible (rmsOn);
}

void LevelMetersComponent::Content::requestParentRelayout()
{
    if (auto* parent = findParentComponentOfClass<LevelMetersComponent>())
        parent->resized();
    if (auto* menu = findParentComponentOfClass<Menu>())
        menu->notifyContentHeightChanged();
    else
        resized();
}

void LevelMetersComponent::Content::styleSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha (0.55f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::goldenrod);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::whitesmoke.withAlpha (0.2f));
}

void LevelMetersComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (SharedResources::uiFont (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    label.setMinimumHorizontalScale (1.0f);
}

void LevelMetersComponent::Content::styleSectionLabel (juce::Label& label)
{
    styleLabel (label);
    label.setFont (SharedResources::uiFont (16.0f));
    label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.9f));
}

void LevelMetersComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
}

void LevelMetersComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void LevelMetersComponent::Content::styleModeCombo (juce::ComboBox& combo)
{
    modeLookAndFeel.setThemeColors (&sharedResources);
    channelModeLookAndFeel.setThemeColors (&sharedResources);
    combo.setLookAndFeel (&modeLookAndFeel);
    combo.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.8f));
}

void LevelMetersComponent::Content::saveAnalyserDefaults()
{
    const bool ok = ModuleLookPresets::saveDefaultFromApvts (ModuleLookPresets::Kind::levelMeters, treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void LevelMetersComponent::Content::layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
{
    if (! label.isVisible())
        return;
    label.setBounds (area.removeFromTop (kMeterLabelH));
    area.removeFromTop (kMeterLabelGap);
    slider.setBounds (area.removeFromTop (kMeterSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kMeterRowGap);
}

void LevelMetersComponent::Content::layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo)
{
    label.setBounds (area.removeFromTop (kMeterLabelH));
    area.removeFromTop (kMeterLabelGap);
    combo.setBounds (area.removeFromTop (kMeterSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kMeterRowGap);
}

void LevelMetersComponent::Content::layoutToggle (juce::Rectangle<int>& area, juce::ToggleButton& toggle)
{
    if (! toggle.isVisible())
        return;
    const juce::Font tfont (juce::FontOptions (14.0f));
    const int tw = juce::jmax (120, (int) std::ceil (
        juce::GlyphArrangement::getStringWidth (tfont, toggle.getButtonText())) + 36);
    toggle.setBounds (area.removeFromTop (kMeterToggleH).removeFromLeft (juce::jmin (tw, area.getWidth())));
    area.removeFromTop (6);
}

void LevelMetersComponent::Content::syncGradientFromBank()
{
    peakRampEditor.setRamp (&colourRamps.get (ColourRampBank::Target::meterPeak));
    rmsRampEditor.setRamp (&colourRamps.get (ColourRampBank::Target::meterRms));
    peakRampEditor.repaint();
    rmsRampEditor.repaint();
}

int LevelMetersComponent::Content::getPreferredHeight() const
{
    const int sliderRow = kMeterLabelH + kMeterLabelGap + kMeterSliderH + kMeterRowGap;
    const int toggleRow = kMeterToggleH + 6;
    const int peakGlowRows = peakGlowToggle.getToggleState() ? 4 : 0;
    const int rmsGlowRows = rmsGlowToggle.getToggleState() ? 4 : 0;
    const int rampBlock = kMeterLabelH + kMeterLabelGap;

    return kMeterPadY * 2
           + 24 + 8
           + displaySection.heightFor (7 * sliderRow)
           + grSection.heightFor (2 * sliderRow)
           + peakSection.heightFor (rampBlock + peakRampEditor.getPreferredHeight()
                                    + kMeterRowGap + toggleRow + peakGlowRows * sliderRow)
           + rmsSection.heightFor (rampBlock + rmsRampEditor.getPreferredHeight()
                                   + kMeterRowGap + toggleRow + rmsGlowRows * sliderRow);
}

void LevelMetersComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kMeterPadX, kMeterPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    peakRampSectionLabel.setVisible (false);
    rmsRampSectionLabel.setVisible (false);

    displaySection.applyVisible ({
        &modeLabel, &modeCombo, &channelModeLabel, &channelModeCombo,
        &readoutIntegrationLabel, &readoutIntegrationSlider,
        &fallLabel, &fallSlider, &peakHoldLabel, &peakHoldSlider,
        &clipHoldLabel, &clipHoldSlider, &clipThresholdLabel, &clipThresholdSlider });
    displaySection.placeHeader (area);
    if (displaySection.isOpen())
    {
        layoutComboRow (area, modeLabel, modeCombo);
        layoutComboRow (area, channelModeLabel, channelModeCombo);
        layoutSliderRow (area, readoutIntegrationLabel, readoutIntegrationSlider);
        layoutSliderRow (area, fallLabel, fallSlider);
        layoutSliderRow (area, peakHoldLabel, peakHoldSlider);
        layoutSliderRow (area, clipHoldLabel, clipHoldSlider);
        layoutSliderRow (area, clipThresholdLabel, clipThresholdSlider);
    }

    grSection.applyVisible ({ &grAvgLabel, &grAvgSlider, &grFallLabel, &grFallSlider });
    grSection.placeHeader (area);
    if (grSection.isOpen())
    {
        layoutSliderRow (area, grAvgLabel, grAvgSlider);
        layoutSliderRow (area, grFallLabel, grFallSlider);
    }

    peakSection.applyVisible ({
        &peakRampLabel, &peakRampEditor, &peakGlowToggle,
        &peakGlowThresholdLabel, &peakGlowThresholdSlider,
        &peakGlowRadiusLabel, &peakGlowRadiusSlider,
        &peakGlowSpreadLabel, &peakGlowSpreadSlider,
        &peakGlowOpacityLabel, &peakGlowOpacitySlider });
    peakSection.placeHeader (area);
    if (peakSection.isOpen())
    {
        peakRampLabel.setBounds (area.removeFromTop (kMeterLabelH));
        area.removeFromTop (kMeterLabelGap);
        peakRampEditor.setBounds (area.removeFromTop (peakRampEditor.getPreferredHeight())
                                      .removeFromLeft (juce::jmin (520, area.getWidth())));
        area.removeFromTop (kMeterRowGap);

        layoutToggle (area, peakGlowToggle);
        updateGlowVisibility();
        layoutSliderRow (area, peakGlowThresholdLabel, peakGlowThresholdSlider);
        layoutSliderRow (area, peakGlowRadiusLabel, peakGlowRadiusSlider);
        layoutSliderRow (area, peakGlowSpreadLabel, peakGlowSpreadSlider);
        layoutSliderRow (area, peakGlowOpacityLabel, peakGlowOpacitySlider);
    }

    rmsSection.applyVisible ({
        &rmsRampLabel, &rmsRampEditor, &rmsGlowToggle,
        &rmsGlowThresholdLabel, &rmsGlowThresholdSlider,
        &rmsGlowRadiusLabel, &rmsGlowRadiusSlider,
        &rmsGlowSpreadLabel, &rmsGlowSpreadSlider,
        &rmsGlowOpacityLabel, &rmsGlowOpacitySlider });
    rmsSection.placeHeader (area);
    if (rmsSection.isOpen())
    {
        rmsRampLabel.setBounds (area.removeFromTop (kMeterLabelH));
        area.removeFromTop (kMeterLabelGap);
        rmsRampEditor.setBounds (area.removeFromTop (rmsRampEditor.getPreferredHeight())
                                     .removeFromLeft (juce::jmin (520, area.getWidth())));
        area.removeFromTop (kMeterRowGap);

        layoutToggle (area, rmsGlowToggle);
        updateGlowVisibility();
        layoutSliderRow (area, rmsGlowThresholdLabel, rmsGlowThresholdSlider);
        layoutSliderRow (area, rmsGlowRadiusLabel, rmsGlowRadiusSlider);
        layoutSliderRow (area, rmsGlowSpreadLabel, rmsGlowSpreadSlider);
        layoutSliderRow (area, rmsGlowOpacityLabel, rmsGlowOpacitySlider);
    }

    if (! peakSection.isOpen())
        peakSection.applyVisible ({
            &peakRampLabel, &peakRampEditor, &peakGlowToggle,
            &peakGlowThresholdLabel, &peakGlowThresholdSlider,
            &peakGlowRadiusLabel, &peakGlowRadiusSlider,
            &peakGlowSpreadLabel, &peakGlowSpreadSlider,
            &peakGlowOpacityLabel, &peakGlowOpacitySlider });
    if (! rmsSection.isOpen())
        rmsSection.applyVisible ({
            &rmsRampLabel, &rmsRampEditor, &rmsGlowToggle,
            &rmsGlowThresholdLabel, &rmsGlowThresholdSlider,
            &rmsGlowRadiusLabel, &rmsGlowRadiusSlider,
            &rmsGlowSpreadLabel, &rmsGlowSpreadSlider,
            &rmsGlowOpacityLabel, &rmsGlowOpacitySlider });
}

LevelMetersComponent::LevelMetersComponent (SharedResources& resources,
                                            juce::AudioProcessorValueTreeState& state,
                                            ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

LevelMetersComponent::~LevelMetersComponent()
{
    colourRamps.removeChangeListener (this);
}

void LevelMetersComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void LevelMetersComponent::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::transparentBlack);
}

void LevelMetersComponent::resized()
{
    content.setBounds (getLocalBounds());
}
