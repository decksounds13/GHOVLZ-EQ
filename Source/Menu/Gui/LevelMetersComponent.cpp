#include "LevelMetersComponent.h"

#include "../AnalyserDefaults.h"

namespace
{
    constexpr int kMeterScrollBarWidth = 11;
    constexpr int kMeterPadX = 16;
    constexpr int kMeterPadY = 10;
    constexpr int kMeterLabelH = 18;
    constexpr int kMeterSliderH = 24;
    constexpr int kMeterRowGap = 6;
    constexpr int kMeterLabelGap = 2;
}

LevelMetersComponent::Content::Content (SharedResources& resources, juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      treeState (state)
{
    titleLabel.setText ("Level Meters", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions ("Lato Black", 20.0f, juce::Font::plain));
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
}

LevelMetersComponent::Content::~Content()
{
    modeCombo.setLookAndFeel (nullptr);
    channelModeCombo.setLookAndFeel (nullptr);
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
    label.setFont (juce::FontOptions ("Lato Black", 15.0f, juce::Font::plain));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
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
    combo.setLookAndFeel (&modeLookAndFeel);
    combo.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.8f));
}

void LevelMetersComponent::Content::saveAnalyserDefaults()
{
    const bool ok = AnalyserDefaults::saveFrom (treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void LevelMetersComponent::Content::layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
{
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

int LevelMetersComponent::Content::getPreferredHeight() const
{
    return kMeterPadY * 2
           + 24 + 8
           + (kMeterLabelH + kMeterLabelGap + kMeterSliderH + kMeterRowGap) // mode
           + (kMeterLabelH + kMeterLabelGap + kMeterSliderH + kMeterRowGap) // channel mode
           + (5 * (kMeterLabelH + kMeterLabelGap + kMeterSliderH + kMeterRowGap));
}

void LevelMetersComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kMeterPadX, kMeterPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    layoutComboRow (area, modeLabel, modeCombo);
    layoutComboRow (area, channelModeLabel, channelModeCombo);
    layoutSliderRow (area, readoutIntegrationLabel, readoutIntegrationSlider);
    layoutSliderRow (area, fallLabel, fallSlider);
    layoutSliderRow (area, peakHoldLabel, peakHoldSlider);
    layoutSliderRow (area, clipHoldLabel, clipHoldSlider);
    layoutSliderRow (area, clipThresholdLabel, clipThresholdSlider);
}

LevelMetersComponent::LevelMetersComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      content (resources, state)
{
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

LevelMetersComponent::~LevelMetersComponent()
{
    viewport.setViewedComponent (nullptr, false);
}

void LevelMetersComponent::syncScrollBarColours()
{
    if (customScrollBar == nullptr)
        return;

    customScrollBar->setTrackBackgroundColour (sharedResources.sharedColors.menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour (sharedResources.sharedColors.menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour (sharedResources.sharedColors.menuScrollBarOutlineColor1);
    customScrollBar->repaint();
}

void LevelMetersComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void LevelMetersComponent::resized()
{
    syncScrollBarColours();

    auto bounds = getLocalBounds();

    if (customScrollBar != nullptr)
    {
        customScrollBar->setVisible (true);
        customScrollBar->setBounds (bounds.removeFromRight (kMeterScrollBarWidth));
    }

    viewport.setBounds (bounds);
    content.setSize (viewport.getWidth(), juce::jmax (viewport.getHeight(), content.getPreferredHeight()));
    content.resized();

    viewport.setViewPosition (viewport.getViewPosition());

    if (customScrollBar != nullptr)
        customScrollBar->updateThumbPosition();
}
