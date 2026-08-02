#include "StereogramSettingsComponent.h"

#include "../../MainComponent.h"

namespace
{
    constexpr int kScrollBarWidth = 11;
    constexpr int kPadX = 16;
    constexpr int kPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kSliderH = 24;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;
    constexpr int kSectionGap = 10;
}

StereogramSettingsComponent::Content::Content (SharedResources& resources,
                                               juce::AudioProcessorValueTreeState& state,
                                               ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::frequency, &ramps.getPresets())
{
    titleLabel.setText ("Stereogram", juce::dontSendNotification);
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

    setupSlider (dotSizeLabel, dotSizeSlider, dotSizeAttachment,
                 "STEREOGRAM_DOT_SIZE_ID", "Dot Size", " px");
    setupSlider (densityLabel, densitySlider, densityAttachment,
                 "STEREOGRAM_DOT_DENSITY_ID", "Dot Density", " %");
    setupSlider (fadeLabel, fadeSlider, fadeAttachment,
                 "STEREOGRAM_FADE_MS_ID", "Fade Time", " ms");

    glowSectionLabel.setText ("Glow", juce::dontSendNotification);
    styleSectionLabel (glowSectionLabel);
    addAndMakeVisible (glowSectionLabel);

    styleToggle (glowToggle);
    addAndMakeVisible (glowToggle);
    glowAttachment = std::make_unique<ButtonAttachment> (treeState, "STEREOGRAM_GLOW_ENABLE_ID", glowToggle);

    setupSlider (glowRadiusLabel, glowRadiusSlider, glowRadiusAttachment,
                 "STEREOGRAM_GLOW_RADIUS_ID", "Glow Radius", " px");
    setupSlider (glowSpreadLabel, glowSpreadSlider, glowSpreadAttachment,
                 "STEREOGRAM_GLOW_SPREAD_ID", "Glow Spread", " px");
    setupSlider (glowOpacityLabel, glowOpacitySlider, glowOpacityAttachment,
                 "STEREOGRAM_GLOW_OPACITY_ID", "Glow Opacity", " %");

    styleToggle (useRampToggle);
    addAndMakeVisible (useRampToggle);
    useRampAttachment = std::make_unique<ButtonAttachment> (treeState, "STEREOGRAM_USE_RAMP_ID", useRampToggle);
    useRampToggle.onClick = [this] { syncRampControlsEnabled(); };

    gradientLabel.setText ("Colour Ramp", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::stereogram));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::stereogram);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<StereogramSettingsComponent>())
            parent->resized();
    };
    addAndMakeVisible (gradientEditor);

    styleLabel (titleLabel);
    syncRampControlsEnabled();
}

StereogramSettingsComponent::Content::~Content() = default;

void StereogramSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void StereogramSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (16.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
}

void StereogramSettingsComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void StereogramSettingsComponent::Content::styleSlider (juce::Slider& slider)
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

void StereogramSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::stereogram));
    gradientEditor.repaint();
    syncRampControlsEnabled();
}

void StereogramSettingsComponent::Content::syncRampControlsEnabled()
{
    const bool rampOn = useRampToggle.getToggleState();
    gradientLabel.setEnabled (rampOn);
    gradientEditor.setEnabled (rampOn);
    gradientEditor.setAlpha (rampOn ? 1.0f : 0.45f);
}

int StereogramSettingsComponent::Content::getPreferredHeight() const
{
    const int sliderBlock = kLabelH + kLabelGap + kSliderH + kRowGap;
    return kPadY * 2 + 24 + 8
         + 22 + kRowGap                    // use ramp
         + kLabelH + kLabelGap + gradientEditor.getPreferredHeight()
         + kSectionGap
         + sliderBlock * 3                 // size, density, fade
         + kSectionGap + 20 + 8 + 22 + kRowGap // glow section + toggle
         + sliderBlock * 3;                // glow radius/spread/opacity
}

void StereogramSettingsComponent::Content::resized()
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

    // Ramp first so it is visible without scrolling.
    useRampToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    gradientLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                  .removeFromLeft (controlW));

    area.removeFromTop (kSectionGap);
    placeSlider (dotSizeLabel, dotSizeSlider);
    placeSlider (densityLabel, densitySlider);
    placeSlider (fadeLabel, fadeSlider);

    area.removeFromTop (kSectionGap);
    glowSectionLabel.setBounds (area.removeFromTop (20).removeFromLeft (controlW));
    area.removeFromTop (8);
    glowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (240, area.getWidth())));
    area.removeFromTop (kRowGap);
    placeSlider (glowRadiusLabel, glowRadiusSlider);
    placeSlider (glowSpreadLabel, glowSpreadSlider);
    placeSlider (glowOpacityLabel, glowOpacitySlider);
}

StereogramSettingsComponent::StereogramSettingsComponent (SharedResources& resources,
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

StereogramSettingsComponent::~StereogramSettingsComponent()
{
    colourRamps.removeChangeListener (this);
    viewport.setViewedComponent (nullptr, false);
}

void StereogramSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void StereogramSettingsComponent::syncScrollBarColours()
{
    if (customScrollBar == nullptr)
        return;

    customScrollBar->setTrackBackgroundColour (sharedResources.sharedColors.menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour (sharedResources.sharedColors.menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour (sharedResources.sharedColors.menuScrollBarOutlineColor1);
    customScrollBar->repaint();
}

void StereogramSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void StereogramSettingsComponent::resized()
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
