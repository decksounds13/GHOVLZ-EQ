#include "GoniometerSettingsComponent.h"

#include "../../MainComponent.h"
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

GoniometerSettingsComponent::Content::Content (SharedResources& resources,
                                               juce::AudioProcessorValueTreeState& state,
                                               ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::goniometer, &ramps.getPresets())
{
    titleLabel.setText ("Goniometer", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    styleToggle (useRampToggle);
    addAndMakeVisible (useRampToggle);
    useRampAttachment = std::make_unique<ButtonAttachment> (treeState, "GON_USE_RAMP_ID", useRampToggle);
    useRampToggle.onClick = [this] { syncRampControlsEnabled(); };

    gradientLabel.setText ("Colour Ramp", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::goniometer));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::goniometer);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<GoniometerSettingsComponent>())
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
    qualityAttachment = std::make_unique<ComboBoxAttachment> (treeState, "GON_QUALITY_ID", qualityCombo);

    lineOpacityLabel.setText ("Line Opacity", juce::dontSendNotification);
    styleSlider (lineOpacitySlider);
    lineOpacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (lineOpacityLabel);
    addAndMakeVisible (lineOpacitySlider);
    lineOpacityAttachment = std::make_unique<SliderAttachment> (treeState, "GON_LINE_OPACITY_ID", lineOpacitySlider);

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

    compactSectionLabel.setText ("Compact Window", juce::dontSendNotification);
    styleSectionLabel (compactSectionLabel);
    addAndMakeVisible (compactSectionLabel);

    setupWidth (compactLineWidthLabel, compactLineWidthSlider, compactLineWidthAttachment,
                "GON_LINE_WIDTH_ID", "Small Window Line Width");
    setupGlowGroup (compactGlowToggle, compactGlowAttachment, "GON_GLOW_ENABLE_ID",
                    compactGlowRadiusLabel, compactGlowRadiusSlider, compactGlowRadiusAttachment, "GON_GLOW_RADIUS_ID",
                    compactGlowSpreadLabel, compactGlowSpreadSlider, compactGlowSpreadAttachment, "GON_GLOW_SPREAD_ID",
                    compactGlowOpacityLabel, compactGlowOpacitySlider, compactGlowOpacityAttachment, "GON_GLOW_OPACITY_ID");

    expandedSectionLabel.setText ("Expanded / Scope", juce::dontSendNotification);
    styleSectionLabel (expandedSectionLabel);
    addAndMakeVisible (expandedSectionLabel);

    setupWidth (expandedLineWidthLabel, expandedLineWidthSlider, expandedLineWidthAttachment,
                "GON_EXPANDED_LINE_WIDTH_ID", "Expanded Window Line Width");
    setupGlowGroup (expandedGlowToggle, expandedGlowAttachment, "GON_EXPANDED_GLOW_ENABLE_ID",
                    expandedGlowRadiusLabel, expandedGlowRadiusSlider, expandedGlowRadiusAttachment, "GON_EXPANDED_GLOW_RADIUS_ID",
                    expandedGlowSpreadLabel, expandedGlowSpreadSlider, expandedGlowSpreadAttachment, "GON_EXPANDED_GLOW_SPREAD_ID",
                    expandedGlowOpacityLabel, expandedGlowOpacitySlider, expandedGlowOpacityAttachment, "GON_EXPANDED_GLOW_OPACITY_ID");

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
}

GoniometerSettingsComponent::Content::~Content()
{
    qualityCombo.setLookAndFeel (nullptr);
}

void GoniometerSettingsComponent::Content::styleSlider (juce::Slider& slider)
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

void GoniometerSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void GoniometerSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (16.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
}

void GoniometerSettingsComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void GoniometerSettingsComponent::Content::styleQualityCombo (juce::ComboBox& combo)
{
    qualityLookAndFeel.setThemeColors (&sharedResources);
    combo.setLookAndFeel (&qualityLookAndFeel);
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
}

void GoniometerSettingsComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void GoniometerSettingsComponent::Content::saveAnalyserDefaults()
{
    const bool ok = AnalyserDefaults::saveFrom (treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void GoniometerSettingsComponent::Content::layoutSliderRow (juce::Rectangle<int>& area,
                                                            juce::Label& label,
                                                            juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    slider.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void GoniometerSettingsComponent::Content::layoutComboRow (juce::Rectangle<int>& area,
                                                           juce::Label& label,
                                                           juce::ComboBox& combo)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    combo.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void GoniometerSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::goniometer));
    gradientEditor.repaint();
    syncRampControlsEnabled();
}

void GoniometerSettingsComponent::Content::syncRampControlsEnabled()
{
    const bool rampOn = useRampToggle.getToggleState();
    gradientLabel.setEnabled (rampOn);
    gradientEditor.setEnabled (rampOn);
    gradientEditor.setAlpha (rampOn ? 1.0f : 0.45f);
}

int GoniometerSettingsComponent::Content::getPreferredHeight() const
{
    const int sharedRows = 2; // quality, line opacity
    const int perModeRows = 4; // line width + 3 glow sliders
    const int perModeExtras = kSectionH + kSectionGap + 22 + 6; // section + glow toggle

    return kPadY * 2
           + 24 + 8
           + 22 + kRowGap // use ramp
           + kLabelH + kLabelGap + gradientEditor.getPreferredHeight()
           + kSectionGap
           + sharedRows * (kLabelH + kLabelGap + kSliderH + kRowGap)
           + 2 * (perModeExtras + perModeRows * (kLabelH + kLabelGap + kSliderH + kRowGap));
}

void GoniometerSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    const int controlW = juce::jmin (520, area.getWidth());

    // Ramp first so it is visible without scrolling.
    useRampToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    gradientLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                  .removeFromLeft (controlW));
    area.removeFromTop (kSectionGap);

    layoutComboRow (area, qualityLabel, qualityCombo);
    layoutSliderRow (area, lineOpacityLabel, lineOpacitySlider);

    auto layoutModeBlock = [this] (juce::Rectangle<int>& areaIn,
                                   juce::Label& section,
                                   juce::Label& widthLabel, juce::Slider& widthSlider,
                                   juce::ToggleButton& glowToggle,
                                   juce::Label& radiusLabel, juce::Slider& radiusSlider,
                                   juce::Label& spreadLabel, juce::Slider& spreadSlider,
                                   juce::Label& opacityLabel, juce::Slider& opacitySlider)
    {
        section.setBounds (areaIn.removeFromTop (kSectionH));
        areaIn.removeFromTop (kLabelGap);
        layoutSliderRow (areaIn, widthLabel, widthSlider);
        glowToggle.setBounds (areaIn.removeFromTop (22).removeFromLeft (juce::jmin (220, areaIn.getWidth())));
        areaIn.removeFromTop (6);
        layoutSliderRow (areaIn, radiusLabel, radiusSlider);
        layoutSliderRow (areaIn, spreadLabel, spreadSlider);
        layoutSliderRow (areaIn, opacityLabel, opacitySlider);
        areaIn.removeFromTop (kSectionGap);
    };

    layoutModeBlock (area, compactSectionLabel,
                     compactLineWidthLabel, compactLineWidthSlider,
                     compactGlowToggle,
                     compactGlowRadiusLabel, compactGlowRadiusSlider,
                     compactGlowSpreadLabel, compactGlowSpreadSlider,
                     compactGlowOpacityLabel, compactGlowOpacitySlider);

    layoutModeBlock (area, expandedSectionLabel,
                     expandedLineWidthLabel, expandedLineWidthSlider,
                     expandedGlowToggle,
                     expandedGlowRadiusLabel, expandedGlowRadiusSlider,
                     expandedGlowSpreadLabel, expandedGlowSpreadSlider,
                     expandedGlowOpacityLabel, expandedGlowOpacitySlider);
}

GoniometerSettingsComponent::GoniometerSettingsComponent (SharedResources& resources,
                                                          juce::AudioProcessorValueTreeState& state,
                                                          ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

GoniometerSettingsComponent::~GoniometerSettingsComponent()
{
    colourRamps.removeChangeListener (this);
}

void GoniometerSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void GoniometerSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void GoniometerSettingsComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
