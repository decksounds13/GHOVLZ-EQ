#include "FftComponent.h"

#include "../AnalyserDefaults.h"

namespace
{
    constexpr int kScrollBarWidth = 11;
    constexpr int kContentPadX = 16;
    constexpr int kContentPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kSliderH = 24;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;

    void setFloatParam (juce::AudioProcessorValueTreeState& treeState, const char* id, float value)
    {
        if (auto* param = treeState.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
}

FftComponent::Content::Content (SharedResources& resources, juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      treeState (state)
{
    titleLabel.setText ("FFT", juce::dontSendNotification);
    titleLabel.setFont (juce::Font ("Lato Black", 20.0f, juce::Font::plain));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    styleToggle (showBarsToggle);
    addAndMakeVisible (showBarsToggle);
    showBarsAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_FFT_BINS_ID", showBarsToggle);

    styleToggle (showBinsToggle);
    showBinsToggle.onClick = [this] { applyShowBinsToggle(); };
    addAndMakeVisible (showBinsToggle);
    treeState.addParameterListener ("FFT_RESOLUTION_ID", this);
    syncShowBinsToggleFromParam();

    styleToggle (fullHeightToggle);
    addAndMakeVisible (fullHeightToggle);
    fullHeightAttachment = std::make_unique<ButtonAttachment> (treeState, "FFT_FULL_HEIGHT_ID", fullHeightToggle);

    opacityLabel.setText ("Opacity", juce::dontSendNotification);
    barWidthLabel.setText ("Bar Width", juce::dontSendNotification);
    intensityLabel.setText ("Intensity", juce::dontSendNotification);
    thresholdLabel.setText ("Threshold", juce::dontSendNotification);
    styleToggle (glowToggle);
    addAndMakeVisible (glowToggle);
    glowAttachment = std::make_unique<ButtonAttachment> (treeState, "FFT_GLOW_ENABLE_ID", glowToggle);

    glowRadiusLabel.setText ("Glow Radius", juce::dontSendNotification);
    glowSpreadLabel.setText ("Glow Spread", juce::dontSendNotification);
    glowOpacityLabel.setText ("Glow Opacity", juce::dontSendNotification);
    glowOffsetXLabel.setText ("Glow Offset X", juce::dontSendNotification);
    glowOffsetYLabel.setText ("Glow Offset Y", juce::dontSendNotification);

    styleSlider (opacitySlider);
    opacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (opacitySlider);
    opacityAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_OPACITY_ID", opacitySlider);

    styleSlider (barWidthSlider);
    barWidthSlider.setTextValueSuffix (" %");
    addAndMakeVisible (barWidthSlider);
    barWidthAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_BAR_WIDTH_ID", barWidthSlider);

    styleSlider (intensitySlider);
    intensitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (intensitySlider);
    intensityAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_INTENSITY_ID", intensitySlider);

    styleSlider (thresholdSlider);
    thresholdSlider.setTextValueSuffix (" %");
    addAndMakeVisible (thresholdSlider);
    thresholdAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_THRESHOLD_ID", thresholdSlider);

    styleSlider (glowRadiusSlider);
    glowRadiusSlider.setTextValueSuffix (" px");
    addAndMakeVisible (glowRadiusSlider);
    glowRadiusAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_GLOW_RADIUS_ID", glowRadiusSlider);

    styleSlider (glowSpreadSlider);
    glowSpreadSlider.setTextValueSuffix (" px");
    addAndMakeVisible (glowSpreadSlider);
    glowSpreadAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_GLOW_SPREAD_ID", glowSpreadSlider);

    styleSlider (glowOpacitySlider);
    glowOpacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (glowOpacitySlider);
    glowOpacityAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_GLOW_OPACITY_ID", glowOpacitySlider);

    styleSlider (glowOffsetXSlider);
    glowOffsetXSlider.setTextValueSuffix (" px");
    addAndMakeVisible (glowOffsetXSlider);
    glowOffsetXAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_GLOW_OFFSET_X_ID", glowOffsetXSlider);

    styleSlider (glowOffsetYSlider);
    glowOffsetYSlider.setTextValueSuffix (" px");
    addAndMakeVisible (glowOffsetYSlider);
    glowOffsetYAttachment = std::make_unique<SliderAttachment> (treeState, "FFT_GLOW_OFFSET_Y_ID", glowOffsetYSlider);

    styleLabel (titleLabel);
    styleLabel (opacityLabel);
    styleLabel (barWidthLabel);
    styleLabel (intensityLabel);
    styleLabel (thresholdLabel);
    styleLabel (glowRadiusLabel);
    styleLabel (glowSpreadLabel);
    styleLabel (glowOpacityLabel);
    styleLabel (glowOffsetXLabel);
    styleLabel (glowOffsetYLabel);

    addAndMakeVisible (opacityLabel);
    addAndMakeVisible (barWidthLabel);
    addAndMakeVisible (intensityLabel);
    addAndMakeVisible (thresholdLabel);
    addAndMakeVisible (glowRadiusLabel);
    addAndMakeVisible (glowSpreadLabel);
    addAndMakeVisible (glowOpacityLabel);
    addAndMakeVisible (glowOffsetXLabel);
    addAndMakeVisible (glowOffsetYLabel);
}

FftComponent::Content::~Content()
{
    treeState.removeParameterListener ("FFT_RESOLUTION_ID", this);
}

void FftComponent::Content::syncShowBinsToggleFromParam()
{
    float resolution = 0.0f;
    if (auto* raw = treeState.getRawParameterValue ("FFT_RESOLUTION_ID"))
        resolution = raw->load();

    showBinsToggle.setToggleState (resolution >= 50.0f, juce::dontSendNotification);
}

void FftComponent::Content::applyShowBinsToggle()
{
    // Density only — does not touch SPECTRUM_FFT_BINS_ID (Show Bars).
    setFloatParam (treeState, "FFT_RESOLUTION_ID", showBinsToggle.getToggleState() ? 100.0f : 0.0f);
}

void FftComponent::Content::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "FFT_RESOLUTION_ID")
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Content> (this)]
        {
            if (safe != nullptr)
                safe->syncShowBinsToggleFromParam();
        });
}

void FftComponent::Content::styleSlider (juce::Slider& slider)
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

void FftComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::Font ("Lato Black", 15.0f, juce::Font::plain));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void FftComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void FftComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void FftComponent::Content::saveAnalyserDefaults()
{
    const bool ok = AnalyserDefaults::saveFrom (treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void FftComponent::Content::layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    slider.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kRowGap);
}

int FftComponent::Content::getPreferredHeight() const
{
    // title + show bars + show bins + full height + 4 sliders + glow toggle + 5 glow sliders
    return kContentPadY * 2
           + 24 + 8
           + 22 + 6
           + 22 + 6
           + 22 + 8
           + (4 * (kLabelH + kLabelGap + kSliderH + kRowGap))
           + 22 + 6
           + (5 * (kLabelH + kLabelGap + kSliderH + kRowGap));
}

void FftComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kContentPadX, kContentPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    showBarsToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (260, area.getWidth())));
    area.removeFromTop (6);

    showBinsToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (260, area.getWidth())));
    area.removeFromTop (6);

    fullHeightToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (260, area.getWidth())));
    area.removeFromTop (8);

    layoutSliderRow (area, opacityLabel, opacitySlider);
    layoutSliderRow (area, barWidthLabel, barWidthSlider);
    layoutSliderRow (area, intensityLabel, intensitySlider);
    layoutSliderRow (area, thresholdLabel, thresholdSlider);

    glowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);
    layoutSliderRow (area, glowRadiusLabel, glowRadiusSlider);
    layoutSliderRow (area, glowSpreadLabel, glowSpreadSlider);
    layoutSliderRow (area, glowOpacityLabel, glowOpacitySlider);
    layoutSliderRow (area, glowOffsetXLabel, glowOffsetXSlider);
    layoutSliderRow (area, glowOffsetYLabel, glowOffsetYSlider);
}

FftComponent::FftComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state)
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

FftComponent::~FftComponent()
{
    viewport.setViewedComponent (nullptr, false);
}

void FftComponent::syncScrollBarColours()
{
    if (customScrollBar == nullptr)
        return;

    customScrollBar->setTrackBackgroundColour (sharedResources.sharedColors.menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour (sharedResources.sharedColors.menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour (sharedResources.sharedColors.menuScrollBarOutlineColor1);
    customScrollBar->repaint();
}

void FftComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void FftComponent::resized()
{
    syncScrollBarColours();

    auto bounds = getLocalBounds();

    // Always reserve the gutter so content width / right margin stay stable.
    if (customScrollBar != nullptr)
    {
        customScrollBar->setVisible (true);
        customScrollBar->setBounds (bounds.removeFromRight (kScrollBarWidth));
    }

    viewport.setBounds (bounds);

    content.setSize (viewport.getWidth(), juce::jmax (viewport.getHeight(), content.getPreferredHeight()));
    content.resized();

    // Force the viewport scrollbar range to refresh before syncing the thumb.
    viewport.setViewPosition (viewport.getViewPosition());

    if (customScrollBar != nullptr)
        customScrollBar->updateThumbPosition();
}
