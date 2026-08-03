#include "HistogramSettingsComponent.h"

#include "../../MainComponent.h"
#include "../Menu.h"

namespace
{
    constexpr int kPadX = 16;
    constexpr int kPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kSliderH = 24;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;
    constexpr int kSectionGap = 10;
}

HistogramSettingsComponent::Content::Content (SharedResources& resources,
                                              juce::AudioProcessorValueTreeState& state,
                                              ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::intensity, &ramps.getPresets())
{
    titleLabel.setText ("Histogram", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    auto setupSlider = [this] (juce::Label& label, juce::Slider& slider,
                               std::unique_ptr<SliderAttachment>& attachment,
                               const char* id, const char* text, const char* suffix)
    {
        label.setText (text, juce::dontSendNotification);
        styleLabel (label);
        styleSlider (slider);
        if (suffix != nullptr)
            slider.setTextValueSuffix (suffix);
        addAndMakeVisible (label);
        addAndMakeVisible (slider);
        attachment = std::make_unique<SliderAttachment> (treeState, id, slider);
    };

    styleToggle (useRampToggle);
    addAndMakeVisible (useRampToggle);
    useRampAttachment = std::make_unique<ButtonAttachment> (treeState, "HISTOGRAM_USE_RAMP_ID", useRampToggle);
    useRampToggle.onClick = [this] { syncRampControlsEnabled(); };

    gradientLabel.setText ("Colour Ramp", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::histogram));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::histogram);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<HistogramSettingsComponent>())
            parent->resized();

        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
    addAndMakeVisible (gradientEditor);

    behaviourSectionLabel.setText ("Behaviour", juce::dontSendNotification);
    styleSectionLabel (behaviourSectionLabel);
    addAndMakeVisible (behaviourSectionLabel);

    setupSlider (speedLabel, speedSlider, speedAttachment,
                 "HISTOGRAM_SPEED_ID", "Scroll Speed", " %");
    setupSlider (lineWidthLabel, lineWidthSlider, lineWidthAttachment,
                 "HISTOGRAM_LINE_WIDTH_ID", "Line Width", " px");
    setupSlider (fillOpacityLabel, fillOpacitySlider, fillOpacityAttachment,
                 "HISTOGRAM_FILL_OPACITY_ID", "Fill Opacity", " %");
    setupSlider (minDbLabel, minDbSlider, minDbAttachment,
                 "HISTOGRAM_MIN_DB_ID", "Min Level", " dB");
    setupSlider (maxDbLabel, maxDbSlider, maxDbAttachment,
                 "HISTOGRAM_MAX_DB_ID", "Max Level", " dB");

    tracesSectionLabel.setText ("Traces", juce::dontSendNotification);
    styleSectionLabel (tracesSectionLabel);
    addAndMakeVisible (tracesSectionLabel);

    styleToggle (showLufsToggle);
    addAndMakeVisible (showLufsToggle);
    showLufsAttachment = std::make_unique<ButtonAttachment> (treeState, "HISTOGRAM_SHOW_LUFS_ID", showLufsToggle);

    styleToggle (showRmsToggle);
    addAndMakeVisible (showRmsToggle);
    showRmsAttachment = std::make_unique<ButtonAttachment> (treeState, "HISTOGRAM_SHOW_RMS_ID", showRmsToggle);

    styleToggle (showTruePeakToggle);
    addAndMakeVisible (showTruePeakToggle);
    showTruePeakAttachment = std::make_unique<ButtonAttachment> (treeState, "HISTOGRAM_SHOW_TRUE_PEAK_ID", showTruePeakToggle);

    styleToggle (freezeToggle);
    addAndMakeVisible (freezeToggle);
    freezeAttachment = std::make_unique<ButtonAttachment> (treeState, "HISTOGRAM_FREEZE_ID", freezeToggle);

    glowSectionLabel.setText ("Glow", juce::dontSendNotification);
    styleSectionLabel (glowSectionLabel);
    addAndMakeVisible (glowSectionLabel);

    styleToggle (glowToggle);
    addAndMakeVisible (glowToggle);
    glowAttachment = std::make_unique<ButtonAttachment> (treeState, "HISTOGRAM_GLOW_ENABLE_ID", glowToggle);

    setupSlider (glowRadiusLabel, glowRadiusSlider, glowRadiusAttachment,
                 "HISTOGRAM_GLOW_RADIUS_ID", "Glow Radius", " px");
    setupSlider (glowSpreadLabel, glowSpreadSlider, glowSpreadAttachment,
                 "HISTOGRAM_GLOW_SPREAD_ID", "Glow Spread", " px");
    setupSlider (glowOpacityLabel, glowOpacitySlider, glowOpacityAttachment,
                 "HISTOGRAM_GLOW_OPACITY_ID", "Glow Opacity", " %");

    styleLabel (titleLabel);
    syncRampControlsEnabled();
}

HistogramSettingsComponent::Content::~Content() = default;

void HistogramSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void HistogramSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (16.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
}

void HistogramSettingsComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void HistogramSettingsComponent::Content::styleSlider (juce::Slider& slider)
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

void HistogramSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::histogram));
    gradientEditor.repaint();
    syncRampControlsEnabled();
}

void HistogramSettingsComponent::Content::syncRampControlsEnabled()
{
    const bool rampOn = useRampToggle.getToggleState();
    gradientLabel.setEnabled (rampOn);
    gradientEditor.setEnabled (rampOn);
    gradientEditor.setAlpha (rampOn ? 1.0f : 0.45f);
}

int HistogramSettingsComponent::Content::getPreferredHeight() const
{
    const int sliderBlock = kLabelH + kLabelGap + kSliderH + kRowGap;
    return kPadY * 2 + 24 + 8
         + 22 + kRowGap
         + kLabelH + kLabelGap + gradientEditor.getPreferredHeight()
         + kSectionGap + 20 + 8
         + sliderBlock * 5
         + kSectionGap + 20 + 8
         + 22 * 4 + kRowGap * 4
         + kSectionGap + 20 + 8 + 22 + kRowGap
         + sliderBlock * 3;
}

void HistogramSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);
    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    const int controlW = juce::jmin (520, area.getWidth());

    auto placeSlider = [&] (juce::Label& label, juce::Slider& slider)
    {
        label.setBounds (area.removeFromTop (kLabelH).removeFromLeft (controlW));
        area.removeFromTop (kLabelGap);
        slider.setBounds (area.removeFromTop (kSliderH).removeFromLeft (controlW));
        area.removeFromTop (kRowGap);
    };

    useRampToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    gradientLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                  .removeFromLeft (controlW));

    area.removeFromTop (kSectionGap);
    behaviourSectionLabel.setBounds (area.removeFromTop (20).removeFromLeft (controlW));
    area.removeFromTop (8);
    placeSlider (speedLabel, speedSlider);
    placeSlider (lineWidthLabel, lineWidthSlider);
    placeSlider (fillOpacityLabel, fillOpacitySlider);
    placeSlider (minDbLabel, minDbSlider);
    placeSlider (maxDbLabel, maxDbSlider);

    area.removeFromTop (kSectionGap);
    tracesSectionLabel.setBounds (area.removeFromTop (20).removeFromLeft (controlW));
    area.removeFromTop (8);
    showLufsToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    showRmsToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    showTruePeakToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    freezeToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));

    area.removeFromTop (kSectionGap);
    glowSectionLabel.setBounds (area.removeFromTop (20).removeFromLeft (controlW));
    area.removeFromTop (8);
    glowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    placeSlider (glowRadiusLabel, glowRadiusSlider);
    placeSlider (glowSpreadLabel, glowSpreadSlider);
    placeSlider (glowOpacityLabel, glowOpacitySlider);
}

HistogramSettingsComponent::HistogramSettingsComponent (SharedResources& resources,
                                                        juce::AudioProcessorValueTreeState& state,
                                                        ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

HistogramSettingsComponent::~HistogramSettingsComponent()
{
    colourRamps.removeChangeListener (this);
}

void HistogramSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void HistogramSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void HistogramSettingsComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
