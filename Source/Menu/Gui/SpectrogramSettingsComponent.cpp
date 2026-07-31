#include "SpectrogramSettingsComponent.h"

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

SpectrogramComponent::ColourScheme ColourSchemeComboLookAndFeel::schemeFromName (const juce::String& name) noexcept
{
    const auto names = SpectrogramComponent::getColourSchemeNames();
    const int idx = names.indexOf (name);
    if (idx < 0)
        return SpectrogramComponent::ColourScheme::heat;
    return static_cast<SpectrogramComponent::ColourScheme> (
        juce::jlimit (0, (int) SpectrogramComponent::ColourScheme::numSchemes - 1, idx));
}

void ColourSchemeComboLookAndFeel::paintSchemeSwatch (juce::Graphics& g,
                                                      juce::Rectangle<float> swatch,
                                                      SpectrogramComponent::ColourScheme scheme)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (swatch, 2.5f);

    juce::ColourGradient grad (SpectrogramComponent::colourForScheme (scheme, 0.0f),
                               swatch.getX(), swatch.getCentreY(),
                               SpectrogramComponent::colourForScheme (scheme, 1.0f),
                               swatch.getRight(), swatch.getCentreY(), false);
    constexpr int kMidStops = 6;
    for (int i = 1; i < kMidStops; ++i)
    {
        const float t = (float) i / (float) kMidStops;
        grad.addColour ((double) t, SpectrogramComponent::colourForScheme (scheme, t));
    }
    g.setGradientFill (grad);
    g.fillRoundedRectangle (swatch.reduced (1.0f), 2.5f);
}

void ColourSchemeComboLookAndFeel::getIdealPopupMenuItemSize (const juce::String& text,
                                                             bool isSeparator,
                                                             int standardMenuItemHeight,
                                                             int& idealWidth,
                                                             int& idealHeight)
{
    if (isSeparator)
    {
        ComboBoxLookAndFeel::getIdealPopupMenuItemSize (text, isSeparator, standardMenuItemHeight,
                                                        idealWidth, idealHeight);
        return;
    }

    idealWidth = 240;
    idealHeight = 28;
}

void ColourSchemeComboLookAndFeel::drawPopupMenuItem (juce::Graphics& g,
                                                      const juce::Rectangle<int>& area,
                                                      bool isSeparator, bool isActive,
                                                      bool isHighlighted, bool isTicked,
                                                      bool hasSubMenu,
                                                      const juce::String& text,
                                                      const juce::String& shortcutKeyText,
                                                      const juce::Drawable* icon,
                                                      const juce::Colour* textColourToUse)
{
    juce::ignoreUnused (hasSubMenu, shortcutKeyText, icon, textColourToUse);

    if (isSeparator)
    {
        ComboBoxLookAndFeel::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted,
                                                isTicked, hasSubMenu, text, shortcutKeyText,
                                                icon, textColourToUse);
        return;
    }

    auto bounds = area.toFloat().reduced (4.0f, 3.0f);
    if (isHighlighted && isActive)
    {
        g.setColour (colors().optionComboHighlight.withAlpha (0.85f));
        g.fillRoundedRectangle (bounds.expanded (2.0f), 3.0f);
    }

    auto swatch = bounds.removeFromLeft (bounds.getWidth() * 0.55f).reduced (0.0f, 2.0f);
    paintSchemeSwatch (g, swatch, schemeFromName (text));

    bounds.removeFromLeft (8.0f);
    g.setColour (isHighlighted ? juce::Colours::black
                               : colors().optionComboText.withAlpha (isActive ? 0.95f : 0.45f));
    g.setFont (juce::FontOptions (12.5f));
    g.drawText (text, bounds.toNearestIntEdges(), juce::Justification::centredLeft, true);

    if (isTicked)
    {
        g.setColour (juce::Colours::goldenrod);
        g.fillEllipse (area.getRight() - 14.0f, area.getCentreY() - 3.0f, 6.0f, 6.0f);
    }
}

void ColourSchemeComboLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height,
                                                bool isButtonDown,
                                                int buttonX, int buttonY, int buttonW, int buttonH,
                                                juce::ComboBox& box)
{
    juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

    const auto& c = colors();
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    g.setColour (c.optionComboBackground);
    g.fillRoundedRectangle (bounds, 2.0f);
    g.setColour (c.optionBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

    auto inner = bounds.reduced (4.0f, 3.0f);
    auto swatch = inner.removeFromLeft (juce::jmin (inner.getWidth() * 0.42f, 72.0f));
    paintSchemeSwatch (g, swatch, schemeFromName (box.getText()));
    inner.removeFromLeft (6.0f);
    inner.removeFromRight (14.0f);

    g.setColour (c.optionComboText.withAlpha (box.isEnabled() ? 0.95f : 0.35f));
    g.setFont (juce::Font ("Lato Black", 11.0f, juce::Font::plain));
    g.drawText (box.getText(), inner.toNearestIntEdges(), juce::Justification::centredLeft, true);

    juce::Rectangle<int> arrowZone (width - 14, 0, 12, height);
    juce::Path path;
    path.startNewSubPath ((float) arrowZone.getX() + 2.0f, (float) arrowZone.getCentreY() - 2.0f);
    path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 2.5f);
    path.lineTo ((float) arrowZone.getRight() - 2.0f, (float) arrowZone.getCentreY() - 2.0f);
    g.setColour (c.optionComboText.withAlpha (box.isEnabled() ? 0.95f : 0.35f));
    g.strokePath (path, juce::PathStrokeType (1.4f));
}

// Hide default combo label — we paint name beside the swatch ourselves.
void ColourSchemeComboLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds ({});
    juce::ignoreUnused (box);
}

SpectrogramSettingsComponent::Content::Content (SharedResources& resources,
                                                juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      treeState (state)
{
    comboLookAndFeel.setThemeColors (&sharedResources);
    colourSchemeLookAndFeel.setThemeColors (&sharedResources);

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
    colourSchemeCombo.setLookAndFeel (&colourSchemeLookAndFeel);
    colourSchemeCombo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
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
    {
        const auto names = SpectrogramComponent::getFftSizeNames();
        for (int i = 0; i < names.size(); ++i)
            fftSizeCombo.addItem (names[i], i + 1);
    }
    addAndMakeVisible (fftSizeLabel);
    addAndMakeVisible (fftSizeCombo);
    fftSizeAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPEC_FFT_SIZE_ID", fftSizeCombo);

    displayResLabel.setText ("Display Resolution", juce::dontSendNotification);
    styleCombo (displayResCombo);
    {
        const auto names = SpectrogramComponent::getDisplayResNames();
        for (int i = 0; i < names.size(); ++i)
            displayResCombo.addItem (names[i], i + 1);
    }
    addAndMakeVisible (displayResLabel);
    addAndMakeVisible (displayResCombo);
    displayResAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPEC_DISPLAY_RES_ID", displayResCombo);

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

    softenLabel.setText ("Soften (screen blur)", juce::dontSendNotification);
    styleSlider (softenSlider);
    softenSlider.setTextValueSuffix (" %");
    addAndMakeVisible (softenLabel);
    addAndMakeVisible (softenSlider);
    softenAttachment = std::make_unique<SliderAttachment> (treeState, "SPEC_SOFTEN_ID", softenSlider);

    styleToggle (logFreqToggle);
    addAndMakeVisible (logFreqToggle);
    logFreqAttachment = std::make_unique<ButtonAttachment> (treeState, "SPEC_LOG_FREQ_ID", logFreqToggle);

    styleToggle (enhancedFreqToggle);
    enhancedFreqToggle.setTooltip (
        "Multi-resolution analysis + instantaneous-frequency reassignment: "
        "thinner tonal ridges (especially bass). Uses more CPU — tune Strength / LF Detail below.");
    addAndMakeVisible (enhancedFreqToggle);
    enhancedFreqAttachment = std::make_unique<ButtonAttachment> (treeState, "SPEC_ENHANCED_FREQ_ID", enhancedFreqToggle);

    enhancedStrengthLabel.setText ("Enhanced Strength", juce::dontSendNotification);
    styleSlider (enhancedStrengthSlider);
    enhancedStrengthSlider.setTextValueSuffix (" %");
    enhancedStrengthSlider.setTooltip ("0% = multi-res continuum only; 100% = full frequency/time reassignment.");
    addAndMakeVisible (enhancedStrengthLabel);
    addAndMakeVisible (enhancedStrengthSlider);
    enhancedStrengthAttachment = std::make_unique<SliderAttachment> (
        treeState, "SPEC_ENHANCED_STRENGTH_ID", enhancedStrengthSlider);

    enhancedLfDetailLabel.setText ("Enhanced LF Detail", juce::dontSendNotification);
    styleCombo (enhancedLfDetailCombo);
    {
        const auto names = SpectrogramComponent::getEnhancedLfDetailNames();
        for (int i = 0; i < names.size(); ++i)
            enhancedLfDetailCombo.addItem (names[i], i + 1);
    }
    enhancedLfDetailCombo.setTooltip (
        "Off = base FFT only. 2× = longer bass window. 4× = longest bass + mid-band 2× window.");
    addAndMakeVisible (enhancedLfDetailLabel);
    addAndMakeVisible (enhancedLfDetailCombo);
    enhancedLfDetailAttachment = std::make_unique<ComboBoxAttachment> (
        treeState, "SPEC_ENHANCED_LF_DETAIL_ID", enhancedLfDetailCombo);

    enhancedCrossoverLabel.setText ("Enhanced Crossover", juce::dontSendNotification);
    styleSlider (enhancedCrossoverSlider);
    enhancedCrossoverSlider.setTextValueSuffix (" Hz");
    enhancedCrossoverSlider.setTooltip ("LF / mid multi-res split (soft blend around this frequency).");
    addAndMakeVisible (enhancedCrossoverLabel);
    addAndMakeVisible (enhancedCrossoverSlider);
    enhancedCrossoverAttachment = std::make_unique<SliderAttachment> (
        treeState, "SPEC_ENHANCED_CROSSOVER_ID", enhancedCrossoverSlider);

    styleToggle (freezeToggle);
    addAndMakeVisible (freezeToggle);
    freezeAttachment = std::make_unique<ButtonAttachment> (treeState, "SPEC_FREEZE_ID", freezeToggle);

    styleLabel (titleLabel);
    styleLabel (colourSchemeLabel);
    styleLabel (brightnessLabel);
    styleLabel (fftSizeLabel);
    styleLabel (displayResLabel);
    styleLabel (channelLabel);
    styleLabel (speedLabel);
    styleLabel (minDbLabel);
    styleLabel (maxDbLabel);
    styleLabel (smoothLabel);
    styleLabel (softenLabel);
    styleLabel (enhancedStrengthLabel);
    styleLabel (enhancedLfDetailLabel);
    styleLabel (enhancedCrossoverLabel);
}

SpectrogramSettingsComponent::Content::~Content()
{
    colourSchemeCombo.setLookAndFeel (nullptr);
    fftSizeCombo.setLookAndFeel (nullptr);
    displayResCombo.setLookAndFeel (nullptr);
    channelCombo.setLookAndFeel (nullptr);
    enhancedLfDetailCombo.setLookAndFeel (nullptr);
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
    comboLookAndFeel.setThemeColors (&sharedResources);
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
    const int comboRows = 5;   // colour, fft, display, channel, enhanced LF detail
    const int sliderRows = 8;  // brightness, speed, min/max, smooth, soften, strength, crossover
    const int toggles = 3;     // log, enhanced, freeze

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
    layoutComboRow (area, displayResLabel, displayResCombo);
    layoutComboRow (area, channelLabel, channelCombo);
    layoutSliderRow (area, speedLabel, speedSlider);
    layoutSliderRow (area, minDbLabel, minDbSlider);
    layoutSliderRow (area, maxDbLabel, maxDbSlider);
    layoutSliderRow (area, smoothLabel, smoothSlider);
    layoutSliderRow (area, softenLabel, softenSlider);

    logFreqToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);
    enhancedFreqToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (260, area.getWidth())));
    area.removeFromTop (6);
    layoutSliderRow (area, enhancedStrengthLabel, enhancedStrengthSlider);
    layoutComboRow (area, enhancedLfDetailLabel, enhancedLfDetailCombo);
    layoutSliderRow (area, enhancedCrossoverLabel, enhancedCrossoverSlider);
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
