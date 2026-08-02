#include "OscilloscopeSettingsComponent.h"

#include "../../MainComponent.h"
#include "../AnalyserDefaults.h"

namespace
{
    constexpr int kScrollBarWidth = 11;
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
      gradientEditor (resources, GradientStripEditor::ModeFamily::oscilloscope, &ramps.getPresets())
{
    titleLabel.setText ("Oscilloscope", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (20.0f));
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
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void OscilloscopeSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (16.0f));
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
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
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
    const bool ok = AnalyserDefaults::saveFrom (treeState);
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

void OscilloscopeSettingsComponent::Content::resized()
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

OscilloscopeSettingsComponent::OscilloscopeSettingsComponent (SharedResources& resources,
                                                              juce::AudioProcessorValueTreeState& state,
                                                              ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, false);
    viewport.setScrollOnDragMode (juce::Viewport::ScrollOnDragMode::never);
    viewport.setScrollBarThickness (0);
    addAndMakeVisible (viewport);

    customScrollBar = std::make_unique<CustomScrollBar> (viewport.getVerticalScrollBar());
    addAndMakeVisible (*customScrollBar);
    customScrollBar->toFront (false);
    syncScrollBarColours();
}

OscilloscopeSettingsComponent::~OscilloscopeSettingsComponent()
{
    colourRamps.removeChangeListener (this);
    viewport.setViewedComponent (nullptr, false);
}

void OscilloscopeSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void OscilloscopeSettingsComponent::syncScrollBarColours()
{
    if (customScrollBar == nullptr)
        return;

    customScrollBar->setTrackBackgroundColour (sharedResources.sharedColors.menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour (sharedResources.sharedColors.menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour (sharedResources.sharedColors.menuScrollBarOutlineColor1);
    customScrollBar->repaint();
}

void OscilloscopeSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void OscilloscopeSettingsComponent::resized()
{
    syncScrollBarColours();

    auto bounds = getLocalBounds();

    if (customScrollBar != nullptr)
    {
        customScrollBar->setVisible (true);
        customScrollBar->setBounds (bounds.removeFromRight (kScrollBarWidth));
    }

    viewport.setBounds (bounds);
    content.setSize (viewport.getWidth(), juce::jmax (viewport.getHeight(), content.getPreferredHeight()));
    content.resized();
    viewport.setViewPosition (viewport.getViewPosition());

    if (customScrollBar != nullptr)
        customScrollBar->updateThumbPosition();
}
