#include "SpectrogramSettingsComponent.h"

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
        GraphOverlayButtonLookAndFeel::fillRoundedGradient (
            g, bounds.expanded (2.0f), colors().optionComboHighlight, 3.0f);
    }

    auto swatch = bounds.removeFromLeft (bounds.getWidth() * 0.55f).reduced (0.0f, 2.0f);
    paintSchemeSwatch (g, swatch, schemeFromName (text));

    bounds.removeFromLeft (8.0f);
    const auto& pal = colors();
    const auto rowInk = isHighlighted
                            ? pal.legibleTextOn (juce::Colours::black, pal.optionComboHighlight)
                            : pal.dropdownTextOn (pal.optionComboText, pal.optionComboBackground);
    g.setColour (rowInk.withAlpha (isActive ? 0.95f : 0.45f));
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
    juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

    const auto& c = colors();
    auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();
    const float corner = GraphOverlayButtonLookAndFeel::cornerRadius();
    const bool hot = box.isMouseOver (true) || isButtonDown || box.isPopupActive();
    auto fill = GraphOverlayButtonLookAndFeel::adjustForInteraction (
        c.optionComboBackground, box.isMouseOver (true), isButtonDown || box.isPopupActive());
    GraphOverlayButtonLookAndFeel::paintChromeFace (g, bounds, fill, corner, hot);
    g.setColour (c.optionBorder);
    g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);

    auto inner = bounds.reduced (4.0f, 3.0f);
    auto swatch = inner.removeFromLeft (juce::jmin (inner.getWidth() * 0.42f, 72.0f));
    paintSchemeSwatch (g, swatch, schemeFromName (box.getText()));
    inner.removeFromLeft (6.0f);
    inner.removeFromRight (14.0f);

    const auto ink = c.dropdownTextOn (c.optionComboText, fill)
                         .withAlpha (box.isEnabled() ? 0.95f : 0.35f);
    g.setColour (ink);
    g.setFont (c.makeUiFont (11.0f));
    g.drawText (box.getText(), inner.toNearestIntEdges(), juce::Justification::centredLeft, true);

    juce::Rectangle<int> arrowZone (width - 14, 0, 12, height);
    juce::Path path;
    path.startNewSubPath ((float) arrowZone.getX() + 2.0f, (float) arrowZone.getCentreY() - 2.0f);
    path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 2.5f);
    path.lineTo ((float) arrowZone.getRight() - 2.0f, (float) arrowZone.getCentreY() - 2.0f);
    g.setColour (ink);
    g.strokePath (path, juce::PathStrokeType (1.4f));
}

// Hide default combo label  -  we paint name beside the swatch ourselves.
void ColourSchemeComboLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds ({});
    juce::ignoreUnused (box);
}

SpectrogramSettingsComponent::Content::Content (SharedResources& resources,
                                                juce::AudioProcessorValueTreeState& state,
                                                ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::intensity, &ramps.getPresets()),
      lookSection (resources, "spec.look", "Look", false),
      behaviourSection (resources, "spec.behaviour", "Behaviour", false),
      rampSection (resources, "spec.ramp", "Ramp", false)
{
    comboLookAndFeel.setThemeColors (&sharedResources);
    colourSchemeLookAndFeel.setThemeColors (&sharedResources);

    titleLabel.setText ("Spectrogram", juce::dontSendNotification);
    titleLabel.setFont (SharedResources::uiFont (20.0f));
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

    gradientLabel.setText ("Custom Gradient (2D)", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrogram));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::spectrogram);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<SpectrogramSettingsComponent>())
            parent->resized();
        else
            resized();

        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
    addAndMakeVisible (gradientEditor);

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
        "2D waterfall: multi-resolution STFT + classical time-frequency reassignment "
        "(Auger-Flandrin IF + group delay) for thin harmonic ridges. "
        "Uses more CPU - tune Strength / LF Detail. Independent of the 3D toggle.");
    addAndMakeVisible (enhancedFreqToggle);
    enhancedFreqAttachment = std::make_unique<ButtonAttachment> (treeState, "SPEC_ENHANCED_FREQ_ID", enhancedFreqToggle);

    enhancedStrengthLabel.setText ("Enhanced Strength", juce::dontSendNotification);
    styleSlider (enhancedStrengthSlider);
    enhancedStrengthSlider.setTextValueSuffix (" %");
    enhancedStrengthSlider.setTooltip (
        "0% = multi-res continuum only (no reassignment FFTs). "
        "100% = full Auger-Flandrin frequency/time reassignment ridges.");
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
        "Off = base FFT only. 2x = longer bass window. 4x = longest bass + mid-band 2x window.");
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

    wireSection (lookSection);
    wireSection (behaviourSection);
    wireSection (rampSection);
}

void SpectrogramSettingsComponent::Content::wireSection (SettingsSection& section)
{
    addAndMakeVisible (section);
    section.onChanged = [this]
    {
        resized();
        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
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
    label.setFont (SharedResources::uiFont (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void SpectrogramSettingsComponent::Content::styleSectionLabel (juce::Label& label)
{
    label.setFont (SharedResources::uiFont (16.0f));
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
    const bool ok = ModuleLookPresets::saveDefaultFromApvts (ModuleLookPresets::Kind::spectrogram, treeState);
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

void SpectrogramSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrogram));
    gradientEditor.repaint();
}

int SpectrogramSettingsComponent::Content::getPreferredHeight() const
{
    const int row = kLabelH + kLabelGap + kSliderH + kRowGap;
    const int tog = 22 + 6;
    return kPadY * 2 + 24 + 8
           + lookSection.heightFor (row * 2)
           + behaviourSection.heightFor (row * 3 + row * 5 + tog * 3 + row * 3)
           + rampSection.heightFor (kLabelH + kLabelGap + gradientEditor.getPreferredHeight());
}

void SpectrogramSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    lookSectionLabel.setVisible (false);
    behaviourSectionLabel.setVisible (false);

    lookSection.applyVisible ({
        &colourSchemeLabel, &colourSchemeCombo, &brightnessLabel, &brightnessSlider });
    lookSection.placeHeader (area);
    if (lookSection.isOpen())
    {
        layoutComboRow (area, colourSchemeLabel, colourSchemeCombo);
        layoutSliderRow (area, brightnessLabel, brightnessSlider);
    }

    behaviourSection.applyVisible ({
        &fftSizeLabel, &fftSizeCombo, &displayResLabel, &displayResCombo,
        &channelLabel, &channelCombo, &speedLabel, &speedSlider,
        &minDbLabel, &minDbSlider, &maxDbLabel, &maxDbSlider,
        &smoothLabel, &smoothSlider, &softenLabel, &softenSlider,
        &logFreqToggle, &enhancedFreqToggle,
        &enhancedStrengthLabel, &enhancedStrengthSlider,
        &enhancedLfDetailLabel, &enhancedLfDetailCombo,
        &enhancedCrossoverLabel, &enhancedCrossoverSlider, &freezeToggle });
    behaviourSection.placeHeader (area);
    if (behaviourSection.isOpen())
    {
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

    rampSection.applyVisible ({ &gradientLabel, &gradientEditor });
    rampSection.placeHeader (area);
    if (rampSection.isOpen())
    {
        gradientLabel.setBounds (area.removeFromTop (kLabelH));
        area.removeFromTop (kLabelGap);
        gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                      .removeFromLeft (juce::jmin (520, area.getWidth())));
    }
}

SpectrogramSettingsComponent::SpectrogramSettingsComponent (SharedResources& resources,
                                                            juce::AudioProcessorValueTreeState& state,
                                                            ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

SpectrogramSettingsComponent::~SpectrogramSettingsComponent()
{
    colourRamps.removeChangeListener (this);
}

void SpectrogramSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void SpectrogramSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void SpectrogramSettingsComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
