#include "OscilloscopeSettingsComponent.h"

#include "../../MainComponent.h"
#include "../../ModuleLookPresets.h"
#include "../AnalyserDefaults.h"
#include "../Menu.h"

namespace
{
    constexpr int kPadX = 16;
    constexpr int kPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kSectionH = 20;
    constexpr int kSliderH = 24;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;
    constexpr int kSectionGap = 10;
}

OscilloscopeSettingsComponent::Content::Content (SharedResources& resources,
                                                 juce::AudioProcessorValueTreeState& state,
                                                 ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::oscilloscope, &ramps.getPresets()),
      displaySection (resources, "osc.display", "Display", false),
      glowSection (resources, "osc.glow", "Glow", false),
      rampSection (resources, "osc.ramp", "Ramp", false)
{
    titleLabel.setText ("Oscilloscope", juce::dontSendNotification);
    titleLabel.setFont (SharedResources::uiFont (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    styleToggle (useRampToggle);
    addAndMakeVisible (useRampToggle);
    useRampAttachment = std::make_unique<ButtonAttachment> (treeState, "OSC_USE_RAMP_ID", useRampToggle);
    useRampToggle.onClick = [this] { syncRampControlsEnabled(); };

    gradientLabel.setText ("Colour Ramp", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::oscilloscope));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::oscilloscope);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<OscilloscopeSettingsComponent>())
            parent->resized();

        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
    addAndMakeVisible (gradientEditor);

    qualityLabel.setText ("Quality", juce::dontSendNotification);
    styleQualityCombo (qualityCombo);
    qualityCombo.addItem ("Draft", 1);
    qualityCombo.addItem ("High", 2);
    addAndMakeVisible (qualityLabel);
    addAndMakeVisible (qualityCombo);
    qualityAttachment = std::make_unique<ComboBoxAttachment> (treeState, "OSC_QUALITY_ID", qualityCombo);

    lineOpacityLabel.setText ("Line Opacity", juce::dontSendNotification);
    styleSlider (lineOpacitySlider);
    lineOpacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (lineOpacityLabel);
    addAndMakeVisible (lineOpacitySlider);
    lineOpacityAttachment = std::make_unique<SliderAttachment> (treeState, "OSC_LINE_OPACITY_ID", lineOpacitySlider);

    auto setupWidth = [this] (juce::Label& label, juce::Slider& slider,
                              std::unique_ptr<SliderAttachment>& attachment,
                              const char* id, const char* text)
    {
        label.setText (text, juce::dontSendNotification);
        styleSlider (slider);
        slider.setTextValueSuffix (" px");
        addAndMakeVisible (label);
        addAndMakeVisible (slider);
        attachment = std::make_unique<SliderAttachment> (treeState, id, slider);
    };

    auto setupGlowGroup = [this] (juce::ToggleButton& toggle,
                                  std::unique_ptr<ButtonAttachment>& toggleAtt,
                                  const char* enableId,
                                  juce::Label& radiusLabel, juce::Slider& radiusSlider,
                                  std::unique_ptr<SliderAttachment>& radiusAtt,
                                  const char* radiusId,
                                  juce::Label& spreadLabel, juce::Slider& spreadSlider,
                                  std::unique_ptr<SliderAttachment>& spreadAtt,
                                  const char* spreadId,
                                  juce::Label& opacityLabel, juce::Slider& opacitySlider,
                                  std::unique_ptr<SliderAttachment>& opacityAtt,
                                  const char* opacityId)
    {
        styleToggle (toggle);
        addAndMakeVisible (toggle);
        toggleAtt = std::make_unique<ButtonAttachment> (treeState, enableId, toggle);

        radiusLabel.setText ("Glow Radius", juce::dontSendNotification);
        spreadLabel.setText ("Glow Spread", juce::dontSendNotification);
        opacityLabel.setText ("Glow Opacity", juce::dontSendNotification);

        styleSlider (radiusSlider);
        radiusSlider.setTextValueSuffix (" px");
        styleSlider (spreadSlider);
        spreadSlider.setTextValueSuffix (" px");
        styleSlider (opacitySlider);
        opacitySlider.setTextValueSuffix (" %");

        addAndMakeVisible (radiusLabel);
        addAndMakeVisible (radiusSlider);
        addAndMakeVisible (spreadLabel);
        addAndMakeVisible (spreadSlider);
        addAndMakeVisible (opacityLabel);
        addAndMakeVisible (opacitySlider);

        radiusAtt = std::make_unique<SliderAttachment> (treeState, radiusId, radiusSlider);
        spreadAtt = std::make_unique<SliderAttachment> (treeState, spreadId, spreadSlider);
        opacityAtt = std::make_unique<SliderAttachment> (treeState, opacityId, opacitySlider);
    };

    compactSectionLabel.setText ("Compact Strip", juce::dontSendNotification);
    styleSectionLabel (compactSectionLabel);
    addAndMakeVisible (compactSectionLabel);

    setupWidth (compactLineWidthLabel, compactLineWidthSlider, compactLineWidthAttachment,
                "OSC_LINE_WIDTH_ID", "Small Window Line Width");
    setupGlowGroup (compactGlowToggle, compactGlowAttachment, "OSC_GLOW_ENABLE_ID",
                    compactGlowRadiusLabel, compactGlowRadiusSlider, compactGlowRadiusAttachment, "OSC_GLOW_RADIUS_ID",
                    compactGlowSpreadLabel, compactGlowSpreadSlider, compactGlowSpreadAttachment, "OSC_GLOW_SPREAD_ID",
                    compactGlowOpacityLabel, compactGlowOpacitySlider, compactGlowOpacityAttachment, "OSC_GLOW_OPACITY_ID");

    expandedSectionLabel.setText ("Expanded / Scope", juce::dontSendNotification);
    styleSectionLabel (expandedSectionLabel);
    addAndMakeVisible (expandedSectionLabel);

    setupWidth (expandedLineWidthLabel, expandedLineWidthSlider, expandedLineWidthAttachment,
                "OSC_EXPANDED_LINE_WIDTH_ID", "Expanded Window Line Width");
    setupGlowGroup (expandedGlowToggle, expandedGlowAttachment, "OSC_EXPANDED_GLOW_ENABLE_ID",
                    expandedGlowRadiusLabel, expandedGlowRadiusSlider, expandedGlowRadiusAttachment, "OSC_EXPANDED_GLOW_RADIUS_ID",
                    expandedGlowSpreadLabel, expandedGlowSpreadSlider, expandedGlowSpreadAttachment, "OSC_EXPANDED_GLOW_SPREAD_ID",
                    expandedGlowOpacityLabel, expandedGlowOpacitySlider, expandedGlowOpacityAttachment, "OSC_EXPANDED_GLOW_OPACITY_ID");

    styleLabel (titleLabel);
    styleLabel (qualityLabel);
    styleLabel (lineOpacityLabel);
    styleLabel (compactLineWidthLabel);
    styleLabel (compactGlowRadiusLabel);
    styleLabel (compactGlowSpreadLabel);
    styleLabel (compactGlowOpacityLabel);
    styleLabel (expandedLineWidthLabel);
    styleLabel (expandedGlowRadiusLabel);
    styleLabel (expandedGlowSpreadLabel);
    styleLabel (expandedGlowOpacityLabel);

    syncRampControlsEnabled();

    wireSection (displaySection);
    wireSection (glowSection);
    wireSection (rampSection);
}

void OscilloscopeSettingsComponent::Content::wireSection (SettingsSection& section)
{
    addAndMakeVisible (section);
    section.onChanged = [this]
    {
        resized();
        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
}

OscilloscopeSettingsComponent::Content::~Content()
{
    qualityCombo.setLookAndFeel (nullptr);
}

void OscilloscopeSettingsComponent::Content::styleSlider (juce::Slider& slider)
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

void OscilloscopeSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (SharedResources::uiFont (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void OscilloscopeSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (SharedResources::uiFont (16.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
}

void OscilloscopeSettingsComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void OscilloscopeSettingsComponent::Content::styleQualityCombo (juce::ComboBox& combo)
{
    qualityLookAndFeel.setThemeColors (&sharedResources);
    combo.setLookAndFeel (&qualityLookAndFeel);
}

void OscilloscopeSettingsComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void OscilloscopeSettingsComponent::Content::saveAnalyserDefaults()
{
    const bool ok = ModuleLookPresets::saveDefaultFromApvts (ModuleLookPresets::Kind::oscilloscope, treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void OscilloscopeSettingsComponent::Content::layoutSliderRow (juce::Rectangle<int>& area,
                                                              juce::Label& label,
                                                              juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    slider.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void OscilloscopeSettingsComponent::Content::layoutComboRow (juce::Rectangle<int>& area,
                                                             juce::Label& label,
                                                             juce::ComboBox& combo)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    combo.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void OscilloscopeSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::oscilloscope));
    gradientEditor.repaint();
    syncRampControlsEnabled();
}

void OscilloscopeSettingsComponent::Content::syncRampControlsEnabled()
{
    const bool rampOn = useRampToggle.getToggleState();
    gradientLabel.setEnabled (rampOn);
    gradientEditor.setEnabled (rampOn);
    gradientEditor.setAlpha (rampOn ? 1.0f : 0.45f);
}

int OscilloscopeSettingsComponent::Content::getPreferredHeight() const
{
    const int row = kLabelH + kLabelGap + kSliderH + kRowGap;
    const int tog = 22 + 6;
    return kPadY * 2 + 24 + 8
           + displaySection.heightFor (2 * row + 2 * (kSectionH + kLabelGap + row))
           + glowSection.heightFor (2 * (tog + 3 * row + kSectionGap))
           + rampSection.heightFor (tog + kLabelH + kLabelGap + gradientEditor.getPreferredHeight());
}

void OscilloscopeSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    const int controlW = juce::jmin (520, area.getWidth());

    displaySection.applyVisible ({
        &qualityLabel, &qualityCombo, &lineOpacityLabel, &lineOpacitySlider,
        &compactSectionLabel, &compactLineWidthLabel, &compactLineWidthSlider,
        &expandedSectionLabel, &expandedLineWidthLabel, &expandedLineWidthSlider });
    displaySection.placeHeader (area);
    if (displaySection.isOpen())
    {
        layoutComboRow (area, qualityLabel, qualityCombo);
        layoutSliderRow (area, lineOpacityLabel, lineOpacitySlider);
        compactSectionLabel.setBounds (area.removeFromTop (kSectionH));
        area.removeFromTop (kLabelGap);
        layoutSliderRow (area, compactLineWidthLabel, compactLineWidthSlider);
        expandedSectionLabel.setBounds (area.removeFromTop (kSectionH));
        area.removeFromTop (kLabelGap);
        layoutSliderRow (area, expandedLineWidthLabel, expandedLineWidthSlider);
    }

    glowSection.applyVisible ({
        &compactGlowToggle, &compactGlowRadiusLabel, &compactGlowRadiusSlider,
        &compactGlowSpreadLabel, &compactGlowSpreadSlider,
        &compactGlowOpacityLabel, &compactGlowOpacitySlider,
        &expandedGlowToggle, &expandedGlowRadiusLabel, &expandedGlowRadiusSlider,
        &expandedGlowSpreadLabel, &expandedGlowSpreadSlider,
        &expandedGlowOpacityLabel, &expandedGlowOpacitySlider });
    glowSection.placeHeader (area);
    if (glowSection.isOpen())
    {
        compactGlowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
        area.removeFromTop (6);
        layoutSliderRow (area, compactGlowRadiusLabel, compactGlowRadiusSlider);
        layoutSliderRow (area, compactGlowSpreadLabel, compactGlowSpreadSlider);
        layoutSliderRow (area, compactGlowOpacityLabel, compactGlowOpacitySlider);
        expandedGlowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
        area.removeFromTop (6);
        layoutSliderRow (area, expandedGlowRadiusLabel, expandedGlowRadiusSlider);
        layoutSliderRow (area, expandedGlowSpreadLabel, expandedGlowSpreadSlider);
        layoutSliderRow (area, expandedGlowOpacityLabel, expandedGlowOpacitySlider);
    }

    rampSection.applyVisible ({ &useRampToggle, &gradientLabel, &gradientEditor });
    rampSection.placeHeader (area);
    if (rampSection.isOpen())
    {
        useRampToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
        area.removeFromTop (kRowGap);
        gradientLabel.setBounds (area.removeFromTop (kLabelH));
        area.removeFromTop (kLabelGap);
        gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                      .removeFromLeft (controlW));
    }
}

OscilloscopeSettingsComponent::OscilloscopeSettingsComponent (SharedResources& resources,
                                                              juce::AudioProcessorValueTreeState& state,
                                                              ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

OscilloscopeSettingsComponent::~OscilloscopeSettingsComponent()
{
    colourRamps.removeChangeListener (this);
}

void OscilloscopeSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void OscilloscopeSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void OscilloscopeSettingsComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
