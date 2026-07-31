#include "SpectrogramSettingsComponent.h"

#include "../../SpectrogramComponent.h"
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

SpectrogramSettingsComponent::Content::Content (SharedResources& resources,
                                                juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      treeState (state)
{
    titleLabel.setText ("Spectrogram", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    lookSectionLabel.setText ("Look", juce::dontSendNotification);
    styleSectionLabel (lookSectionLabel);
    addAndMakeVisible (lookSectionLabel);

    colourSchemeLabel.setText ("Colour Scheme", juce::dontSendNotification);
    styleCombo (colourSchemeCombo);
    {
        const auto names = SpectrogramComponent::getColourSchemeNames();
        for (int i = 0; i < names.size(); ++i)
            colourSchemeCombo.addItem (names[i], i + 1);
    }
    addAndMakeVisible (colourSchemeLabel);
    addAndMakeVisible (colourSchemeCombo);
    colourSchemeAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPEC_COLOUR_SCHEME_ID", colourSchemeCombo);

    brightnessLabel.setText ("Brightness", juce::dontSendNotification);
    styleSlider (brightnessSlider);
    brightnessSlider.setTextValueSuffix (" %");
    addAndMakeVisible (brightnessLabel);
    addAndMakeVisible (brightnessSlider);
    brightnessAttachment = std::make_unique<SliderAttachment> (treeState, "SPEC_BRIGHTNESS_ID", brightnessSlider);

    behaviourSectionLabel.setText ("Behaviour", juce::dontSendNotification);
    styleSectionLabel (behaviourSectionLabel);
    addAndMakeVisible (behaviourSectionLabel);

    fftSizeLabel.setText ("FFT Size", juce::dontSendNotification);
    styleCombo (fftSizeCombo);
    fftSizeCombo.addItem ("512", 1);
    fftSizeCombo.addItem ("1024", 2);
    fftSizeCombo.addItem ("2048", 3);
    fftSizeCombo.addItem ("4096", 4);
    addAndMakeVisible (fftSizeLabel);
    addAndMakeVisible (fftSizeCombo);
    fftSizeAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPEC_FFT_SIZE_ID", fftSizeCombo);

    channelLabel.setText ("Channel", juce::dontSendNotification);
    styleCombo (channelCombo);
    channelCombo.addItem ("Sum", 1);
    channelCombo.addItem ("Left", 2);
    channelCombo.addItem ("Right", 3);
    addAndMakeVisible (channelLabel);
    addAndMakeVisible (channelCombo);
    channelAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPEC_CHANNEL_ID", channelCombo);

    speedLabel.setText ("Scroll Speed", juce::dontSendNotification);
    styleSlider (speedSlider);
    addAndMakeVisible (speedLabel);
    addAndMakeVisible (speedSlider);
    speedAttachment = std::make_unique<SliderAttachment> (treeState, "SPEC_SPEED_ID", speedSlider);

    minDbLabel.setText ("Floor", juce::dontSendNotification);
    styleSlider (minDbSlider);
    minDbSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (minDbLabel);
    addAndMakeVisible (minDbSlider);
    minDbAttachment = std::make_unique<SliderAttachment> (treeState, "SPEC_MIN_DB_ID", minDbSlider);

    maxDbLabel.setText ("Ceiling", juce::dontSendNotification);
    styleSlider (maxDbSlider);
    maxDbSlider.setTextValueSuffix (" dB");
    addAndMakeVisible (maxDbLabel);
    addAndMakeVisible (maxDbSlider);
    maxDbAttachment = std::make_unique<SliderAttachment> (treeState, "SPEC_MAX_DB_ID", maxDbSlider);

    smoothLabel.setText ("Smoothing", juce::dontSendNotification);
    styleSlider (smoothSlider);
    smoothSlider.setTextValueSuffix (" %");
    addAndMakeVisible (smoothLabel);
    addAndMakeVisible (smoothSlider);
    smoothAttachment = std::make_unique<SliderAttachment> (treeState, "SPEC_SMOOTH_ID", smoothSlider);

    styleToggle (logFreqToggle);
    addAndMakeVisible (logFreqToggle);
    logFreqAttachment = std::make_unique<ButtonAttachment> (treeState, "SPEC_LOG_FREQ_ID", logFreqToggle);

    styleToggle (freezeToggle);
    addAndMakeVisible (freezeToggle);
    freezeAttachment = std::make_unique<ButtonAttachment> (treeState, "SPEC_FREEZE_ID", freezeToggle);

    styleLabel (titleLabel);
    styleLabel (colourSchemeLabel);
    styleLabel (brightnessLabel);
    styleLabel (fftSizeLabel);
    styleLabel (channelLabel);
    styleLabel (speedLabel);
    styleLabel (minDbLabel);
    styleLabel (maxDbLabel);
    styleLabel (smoothLabel);
}

SpectrogramSettingsComponent::Content::~Content()
{
    colourSchemeCombo.setLookAndFeel (nullptr);
    fftSizeCombo.setLookAndFeel (nullptr);
    channelCombo.setLookAndFeel (nullptr);
}

void SpectrogramSettingsComponent::Content::styleSlider (juce::Slider& slider)
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

void SpectrogramSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void SpectrogramSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (16.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
}

void SpectrogramSettingsComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void SpectrogramSettingsComponent::Content::styleCombo (juce::ComboBox& combo)
{
    combo.setLookAndFeel (&comboLookAndFeel);
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
}

void SpectrogramSettingsComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void SpectrogramSettingsComponent::Content::saveAnalyserDefaults()
{
    const bool ok = AnalyserDefaults::saveFrom (treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void SpectrogramSettingsComponent::Content::layoutSliderRow (juce::Rectangle<int>& area,
                                                             juce::Label& label,
                                                             juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    slider.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void SpectrogramSettingsComponent::Content::layoutComboRow (juce::Rectangle<int>& area,
                                                            juce::Label& label,
                                                            juce::ComboBox& combo)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    combo.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kRowGap);
}

int SpectrogramSettingsComponent::Content::getPreferredHeight() const
{
    const int comboRows = 3;
    const int sliderRows = 5;
    const int toggles = 2;

    return kPadY * 2
           + 24 + 8
           + 2 * (kSectionH + kLabelGap + kSectionGap)
           + comboRows * (kLabelH + kLabelGap + kSliderH + kRowGap)
           + sliderRows * (kLabelH + kLabelGap + kSliderH + kRowGap)
           + toggles * (22 + 6);
}

void SpectrogramSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    lookSectionLabel.setBounds (area.removeFromTop (kSectionH));
    area.removeFromTop (kLabelGap);
    layoutComboRow (area, colourSchemeLabel, colourSchemeCombo);
    layoutSliderRow (area, brightnessLabel, brightnessSlider);
    area.removeFromTop (kSectionGap);

    behaviourSectionLabel.setBounds (area.removeFromTop (kSectionH));
    area.removeFromTop (kLabelGap);
    layoutComboRow (area, fftSizeLabel, fftSizeCombo);
    layoutComboRow (area, channelLabel, channelCombo);
    layoutSliderRow (area, speedLabel, speedSlider);
    layoutSliderRow (area, minDbLabel, minDbSlider);
    layoutSliderRow (area, maxDbLabel, maxDbSlider);
    layoutSliderRow (area, smoothLabel, smoothSlider);

    logFreqToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);
    freezeToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
}

SpectrogramSettingsComponent::SpectrogramSettingsComponent (SharedResources& resources,
                                                            juce::AudioProcessorValueTreeState& state)
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

SpectrogramSettingsComponent::~SpectrogramSettingsComponent()
{
    viewport.setViewedComponent (nullptr, false);
}

void SpectrogramSettingsComponent::syncScrollBarColours()
{
    if (customScrollBar == nullptr)
        return;

    customScrollBar->setTrackBackgroundColour (sharedResources.sharedColors.menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour (sharedResources.sharedColors.menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour (sharedResources.sharedColors.menuScrollBarOutlineColor1);
    customScrollBar->repaint();
}

void SpectrogramSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void SpectrogramSettingsComponent::resized()
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
