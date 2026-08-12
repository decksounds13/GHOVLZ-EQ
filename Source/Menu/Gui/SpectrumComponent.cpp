#include "SpectrumComponent.h"

#include "../../EqEditor.h"
#include "../../MainComponent.h"
#include "../../ModuleLookPresets.h"
#include "../../Spectral/SpectralMethod.h"
#include "../AnalyserDefaults.h"
#include "../Menu.h"

namespace
{
    constexpr int kSpectrumPadX = 16;
    constexpr int kSpectrumPadY = 10;
    constexpr int kSpectrumLabelH = 18;
    constexpr int kSpectrumSliderH = 24;
    constexpr int kSpectrumRowGap = 6;
    constexpr int kSpectrumLabelGap = 2;
    constexpr int kSpectrumToggleH = 20;

    void setFloatParam (juce::AudioProcessorValueTreeState& treeState, const char* id, float value)
    {
        if (auto* param = treeState.getParameter (id))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    }
}

SpectrumComponent::Content::Content (SharedResources& resources,
                                     juce::AudioProcessorValueTreeState& state,
                                     ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets())
{
    titleLabel.setText ("Spectrum", juce::dontSendNotification);
    titleLabel.setFont (juce::Font ("Lato Black", 20.0f, juce::Font::plain));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    styleToggle (showBinsToggle);
    showBinsToggle.onClick = [this] { applyShowBinsToggle(); };
    addAndMakeVisible (showBinsToggle);
    treeState.addParameterListener ("SPECTRUM_RESOLUTION_ID", this);
    syncShowBinsToggleFromParam();

    enableToggle.setClickingTogglesState (true);
    styleToggle (enableToggle);
    addAndMakeVisible (enableToggle);
    enableAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_ANALYSER_ID", enableToggle);

    blockSizeLabel.setText ("FFT Block Size", juce::dontSendNotification);
    styleSettingsCombo (blockSizeCombo);
    {
        const auto names = AnalyserDefaults::getBlockSizeNames();
        for (int i = 0; i < names.size(); ++i)
            blockSizeCombo.addItem (names[i], i + 1);
    }
    addAndMakeVisible (blockSizeLabel);
    addAndMakeVisible (blockSizeCombo);
    blockSizeAttachment = std::make_unique<ComboBoxAttachment> (treeState, "BLOCK_ID", blockSizeCombo);

    spectralMethodLabel.setText ("Spectral Method", juce::dontSendNotification);
    styleSettingsCombo (spectralMethodCombo);
    {
        const auto names = SpectralMethod::choiceNames();
        for (int i = 0; i < names.size(); ++i)
            spectralMethodCombo.addItem (names[i], i + 1);
    }
    spectralMethodCombo.setTooltip (
        "Lattice: zero-latency IIR bandpass bank (default). "
        "FFT: STFT magnitude GR with reported latency (~2048 samples). "
        "Does not change Match or Side Check.");
    addAndMakeVisible (spectralMethodLabel);
    addAndMakeVisible (spectralMethodCombo);
    spectralMethodAttachment = std::make_unique<ComboBoxAttachment> (
        treeState, SpectralMethod::paramId(), spectralMethodCombo);

    refreshLabel.setText ("Refresh", juce::dontSendNotification);
    styleSlider (refreshSlider);
    refreshSlider.setTextValueSuffix (" ms");
    refreshSlider.setTooltip ("How often analysis runs on the latest Block window. Faster = smoother & more CPU.");
    addAndMakeVisible (refreshLabel);
    addAndMakeVisible (refreshSlider);
    refreshAttachment = std::make_unique<SliderAttachment> (treeState, "REFRESH_ID", refreshSlider);

    avgLabel.setText ("Average", juce::dontSendNotification);
    styleSlider (avgSlider);
    avgSlider.setTooltip ("Average this many analysed blocks for a smoother display.");
    addAndMakeVisible (avgLabel);
    addAndMakeVisible (avgSlider);
    avgAttachment = std::make_unique<SliderAttachment> (treeState, "AVG_ID", avgSlider);

    curveSmoothLabel.setText ("Curve Smoothness", juce::dontSendNotification);
    styleSettingsCombo (curveSmoothCombo);
    curveSmoothCombo.addItem ("Off", 1);
    curveSmoothCombo.addItem ("Low", 2);
    curveSmoothCombo.addItem ("Med", 3);
    curveSmoothCombo.addItem ("High", 4);
    addAndMakeVisible (curveSmoothLabel);
    addAndMakeVisible (curveSmoothCombo);
    curveSmoothAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPECTRUM_CURVE_RES_ID", curveSmoothCombo);

    multicolorBandFillToggle.setClickingTogglesState (true);
    styleToggle (multicolorBandFillToggle);
    multicolorBandFillToggle.setTooltip (
        "When on, EQ band fills and handles use Graph Band 1-8 colours. "
        "When off, bands use neutral golden boost/cut fills.");
    addAndMakeVisible (multicolorBandFillToggle);
    multicolorBandFillAttachment = std::make_unique<ButtonAttachment> (
        treeState, "EQ_MULTICOLOR_BAND_FILL_ID", multicolorBandFillToggle);

    bandChromeMatchHandlesToggle.setClickingTogglesState (true);
    styleToggle (bandChromeMatchHandlesToggle);
    bandChromeMatchHandlesToggle.setTooltip (
        "When on, faceplate power rings and knob glow arcs use the same multicolour "
        "as that band's graph handle. When off, they share the theme Knob Arc colour.");
    addAndMakeVisible (bandChromeMatchHandlesToggle);
    bandChromeMatchHandlesAttachment = std::make_unique<ButtonAttachment> (
        treeState, "EQ_BAND_CHROME_MATCH_HANDLES_ID", bandChromeMatchHandlesToggle);

    bandMinSatEnableToggle.setClickingTogglesState (true);
    styleToggle (bandMinSatEnableToggle);
    bandMinSatEnableToggle.setTooltip (
        "When on, Graph Band colours (and matching faceplate power rings / knob glows) "
        "never drop below Band min sat. Default on at 25%.");
    bandMinSatEnableToggle.onClick = [this] { applyBandMinSatFromControls(); };
    addAndMakeVisible (bandMinSatEnableToggle);

    styleLabel (bandMinSatLabel);
    bandMinSatLabel.setText ("Min sat", juce::dontSendNotification);
    bandMinSatLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (bandMinSatLabel);

    styleSlider (bandMinSatSlider);
    bandMinSatSlider.setRange (0.0, 1.0, 0.01);
    bandMinSatSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    bandMinSatSlider.setTooltip (
        "Minimum saturation for Graph Band 1-8 when randomizing, and for faceplate "
        "power/glow when Match power/glow is on. Default 25%.");
    bandMinSatSlider.onValueChange = [this] { applyBandMinSatFromControls(); };
    addAndMakeVisible (bandMinSatSlider);

    bandMinSatPercentLabel.setJustificationType (juce::Justification::centredRight);
    bandMinSatPercentLabel.setMinimumHorizontalScale (1.0f);
    bandMinSatPercentLabel.setInterceptsMouseClicks (false, false);
    styleLabel (bandMinSatPercentLabel);
    addAndMakeVisible (bandMinSatPercentLabel);

    syncBandMinSatControlsFromShared();

    showCrosshairToggle.setClickingTogglesState (true);
    styleToggle (showCrosshairToggle);
    addAndMakeVisible (showCrosshairToggle);
    showCrosshairAttachment = std::make_unique<ButtonAttachment> (
        treeState, "EQ_SHOW_CROSSHAIR_ID", showCrosshairToggle);

    layersLabel.setText ("Layers", juce::dontSendNotification);

    styleToggle (preCurveToggle);
    styleToggle (preFillToggle);
    styleToggle (postCurveToggle);
    styleToggle (postFillToggle);
    styleToggle (holdCurveToggle);
    styleToggle (holdFillToggle);
    addAndMakeVisible (preCurveToggle);
    addAndMakeVisible (preFillToggle);
    addAndMakeVisible (postCurveToggle);
    addAndMakeVisible (postFillToggle);
    addAndMakeVisible (holdCurveToggle);
    addAndMakeVisible (holdFillToggle);
    preCurveAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_PRE_CURVE_ID", preCurveToggle);
    preFillAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_PRE_FILL_ID", preFillToggle);
    postCurveAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_POST_CURVE_ID", postCurveToggle);
    postFillAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_POST_FILL_ID", postFillToggle);
    holdCurveAttachment = std::make_unique<ButtonAttachment> (treeState, "MAX_ID", holdCurveToggle);
    holdFillAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_HOLD_FILL_ID", holdFillToggle);

    scaleLabel.setText ("Frequency Scale", juce::dontSendNotification);
    opacityLabel.setText ("Opacity", juce::dontSendNotification);
    fillOpacityLabel.setText ("Fill Opacity", juce::dontSendNotification);
    pathWidthLabel.setText ("Path Width", juce::dontSendNotification);
    bandLineWidthLabel.setText ("Band Line Width", juce::dontSendNotification);
    sumLineWidthLabel.setText ("Sum Line Width", juce::dontSendNotification);
    sumGlowToggle.setClickingTogglesState (true);
    styleToggle (sumGlowToggle);
    addAndMakeVisible (sumGlowToggle);
    sumGlowAttachment = std::make_unique<ButtonAttachment> (treeState, "EQ_SUM_GLOW_ENABLE_ID", sumGlowToggle);

    sumGlowRadiusLabel.setText ("Sum Glow Radius", juce::dontSendNotification);
    sumGlowSpreadLabel.setText ("Sum Glow Spread", juce::dontSendNotification);
    sumGlowOpacityLabel.setText ("Sum Glow Opacity", juce::dontSendNotification);

    postGlowToggle.setClickingTogglesState (true);
    styleToggle (postGlowToggle);
    addAndMakeVisible (postGlowToggle);
    postGlowAttachment = std::make_unique<ButtonAttachment> (treeState, "SPECTRUM_GLOW_ENABLE_ID", postGlowToggle);

    spectrumGlowRadiusLabel.setText ("Post Glow Radius", juce::dontSendNotification);
    spectrumGlowSpreadLabel.setText ("Post Glow Spread", juce::dontSendNotification);
    spectrumGlowOpacityLabel.setText ("Post Glow Opacity", juce::dontSendNotification);
    holdTimeLabel.setText ("Fade Time (Max Hold)", juce::dontSendNotification);

    styleScaleButton (linButton);
    styleScaleButton (logButton);
    styleScaleButton (stButton);
    linButton.setRadioGroupId (scaleRadioGroup);
    logButton.setRadioGroupId (scaleRadioGroup);
    stButton.setRadioGroupId (scaleRadioGroup);
    addAndMakeVisible (linButton);
    addAndMakeVisible (logButton);
    addAndMakeVisible (stButton);
    linAttachment = std::make_unique<ButtonAttachment> (treeState, "LIN_ID", linButton);
    logAttachment = std::make_unique<ButtonAttachment> (treeState, "LOG_ID", logButton);
    stAttachment = std::make_unique<ButtonAttachment> (treeState, "ST_ID", stButton);

    styleSlider (opacitySlider);
    opacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (opacitySlider);
    opacityAttachment = std::make_unique<SliderAttachment> (treeState, "SPECTRUM_OPACITY_ID", opacitySlider);

    styleSlider (fillOpacitySlider);
    fillOpacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (fillOpacitySlider);
    fillOpacityAttachment = std::make_unique<SliderAttachment> (treeState, "SPECTRUM_FILL_OPACITY_ID", fillOpacitySlider);

    styleSlider (pathWidthSlider);
    pathWidthSlider.setTextValueSuffix (" px");
    addAndMakeVisible (pathWidthSlider);
    pathWidthAttachment = std::make_unique<SliderAttachment> (treeState, "SPECTRUM_PATH_WIDTH_ID", pathWidthSlider);

    styleSlider (bandLineWidthSlider);
    bandLineWidthSlider.setTextValueSuffix (" px");
    addAndMakeVisible (bandLineWidthSlider);
    bandLineWidthAttachment = std::make_unique<SliderAttachment> (treeState, "EQ_BAND_PATH_WIDTH_ID", bandLineWidthSlider);

    styleSlider (sumLineWidthSlider);
    sumLineWidthSlider.setTextValueSuffix (" px");
    addAndMakeVisible (sumLineWidthSlider);
    sumLineWidthAttachment = std::make_unique<SliderAttachment> (treeState, "EQ_SUM_PATH_WIDTH_ID", sumLineWidthSlider);

    styleSlider (sumGlowRadiusSlider);
    sumGlowRadiusSlider.setTextValueSuffix (" px");
    addAndMakeVisible (sumGlowRadiusSlider);
    sumGlowRadiusAttachment = std::make_unique<SliderAttachment> (treeState, "EQ_SUM_GLOW_RADIUS_ID", sumGlowRadiusSlider);

    styleSlider (sumGlowSpreadSlider);
    sumGlowSpreadSlider.setTextValueSuffix (" px");
    addAndMakeVisible (sumGlowSpreadSlider);
    sumGlowSpreadAttachment = std::make_unique<SliderAttachment> (treeState, "EQ_SUM_GLOW_SPREAD_ID", sumGlowSpreadSlider);

    styleSlider (sumGlowOpacitySlider);
    sumGlowOpacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (sumGlowOpacitySlider);
    sumGlowOpacityAttachment = std::make_unique<SliderAttachment> (treeState, "EQ_SUM_GLOW_OPACITY_ID", sumGlowOpacitySlider);

    styleSlider (spectrumGlowRadiusSlider);
    spectrumGlowRadiusSlider.setTextValueSuffix (" px");
    addAndMakeVisible (spectrumGlowRadiusSlider);
    spectrumGlowRadiusAttachment = std::make_unique<SliderAttachment> (treeState, "SPECTRUM_GLOW_RADIUS_ID", spectrumGlowRadiusSlider);

    styleSlider (spectrumGlowSpreadSlider);
    spectrumGlowSpreadSlider.setTextValueSuffix (" px");
    addAndMakeVisible (spectrumGlowSpreadSlider);
    spectrumGlowSpreadAttachment = std::make_unique<SliderAttachment> (treeState, "SPECTRUM_GLOW_SPREAD_ID", spectrumGlowSpreadSlider);

    styleSlider (spectrumGlowOpacitySlider);
    spectrumGlowOpacitySlider.setTextValueSuffix (" %");
    addAndMakeVisible (spectrumGlowOpacitySlider);
    spectrumGlowOpacityAttachment = std::make_unique<SliderAttachment> (treeState, "SPECTRUM_GLOW_OPACITY_ID", spectrumGlowOpacitySlider);

    styleSlider (holdTimeSlider);
    holdTimeSlider.setTextValueSuffix (" s");
    addAndMakeVisible (holdTimeSlider);
    holdTimeAttachment = std::make_unique<SliderAttachment> (treeState, "MAX_HOLD_ID", holdTimeSlider);

    styleLabel (titleLabel);
    styleLabel (blockSizeLabel);
    styleLabel (spectralMethodLabel);
    styleLabel (refreshLabel);
    styleLabel (avgLabel);
    styleLabel (curveSmoothLabel);
    styleLabel (layersLabel);
    styleLabel (scaleLabel);
    styleLabel (opacityLabel);
    styleLabel (fillOpacityLabel);
    styleLabel (pathWidthLabel);
    styleLabel (bandLineWidthLabel);
    styleLabel (sumLineWidthLabel);
    styleLabel (sumGlowRadiusLabel);
    styleLabel (sumGlowSpreadLabel);
    styleLabel (sumGlowOpacityLabel);
    styleLabel (spectrumGlowRadiusLabel);
    styleLabel (spectrumGlowSpreadLabel);
    styleLabel (spectrumGlowOpacityLabel);
    styleLabel (holdTimeLabel);

    addAndMakeVisible (layersLabel);
    addAndMakeVisible (scaleLabel);
    addAndMakeVisible (opacityLabel);
    addAndMakeVisible (fillOpacityLabel);
    addAndMakeVisible (pathWidthLabel);
    addAndMakeVisible (bandLineWidthLabel);
    addAndMakeVisible (sumLineWidthLabel);
    addAndMakeVisible (sumGlowRadiusLabel);
    addAndMakeVisible (sumGlowSpreadLabel);
    addAndMakeVisible (sumGlowOpacityLabel);
    addAndMakeVisible (spectrumGlowRadiusLabel);
    addAndMakeVisible (spectrumGlowSpreadLabel);
    addAndMakeVisible (spectrumGlowOpacityLabel);
    addAndMakeVisible (holdTimeLabel);

    gradientLabel.setText ("Fill Gradient", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumFill));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::spectrumFill);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<SpectrumComponent>())
            parent->resized();
        else
            resized();

        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
    addAndMakeVisible (gradientEditor);
}

SpectrumComponent::Content::~Content()
{
    blockSizeCombo.setLookAndFeel (nullptr);
    spectralMethodCombo.setLookAndFeel (nullptr);
    curveSmoothCombo.setLookAndFeel (nullptr);
    treeState.removeParameterListener ("SPECTRUM_RESOLUTION_ID", this);
}

void SpectrumComponent::Content::syncShowBinsToggleFromParam()
{
    float value = 100.0f;

    if (auto* raw = treeState.getRawParameterValue ("SPECTRUM_RESOLUTION_ID"))
        value = raw->load();

    showBinsToggle.setToggleState (value >= 50.0f, juce::dontSendNotification);
}

void SpectrumComponent::Content::applyShowBinsToggle()
{
    setFloatParam (treeState, "SPECTRUM_RESOLUTION_ID", showBinsToggle.getToggleState() ? 100.0f : 0.0f);
}

void SpectrumComponent::Content::parameterChanged (const juce::String& parameterID, float)
{
    if (parameterID == "SPECTRUM_RESOLUTION_ID")
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Content> (this)]
        {
            if (safe != nullptr)
                safe->syncShowBinsToggleFromParam();
        });
}

void SpectrumComponent::Content::styleSlider (juce::Slider& slider)
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

void SpectrumComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::Font ("Lato Black", 15.0f, juce::Font::plain));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void SpectrumComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void SpectrumComponent::Content::styleScaleButton (juce::TextButton& button)
{
    button.setClickingTogglesState (true);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void SpectrumComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void SpectrumComponent::Content::styleSettingsCombo (juce::ComboBox& combo)
{
    comboLookAndFeel.setThemeColors (&sharedResources);
    combo.setLookAndFeel (&comboLookAndFeel);
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
}

void SpectrumComponent::Content::saveAnalyserDefaults()
{
    const bool ok = ModuleLookPresets::saveDefaultFromApvts (ModuleLookPresets::Kind::spectrum, treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void SpectrumComponent::Content::syncBandMinSatControlsFromShared()
{
    auto& c = sharedResources.sharedColors;
    bandMinSatEnableToggle.setToggleState (c.graphBandRandomMinSatEnabled, juce::dontSendNotification);
    bandMinSatSlider.setValue (c.graphBandRandomMinSaturation, juce::dontSendNotification);
    const int pct = juce::roundToInt (c.graphBandRandomMinSaturation * 100.0f);
    bandMinSatPercentLabel.setText (juce::String (pct) + "%", juce::dontSendNotification);
    const bool en = c.graphBandRandomMinSatEnabled;
    bandMinSatLabel.setEnabled (en);
    bandMinSatSlider.setEnabled (en);
    bandMinSatPercentLabel.setEnabled (en);
}

void SpectrumComponent::Content::applyBandMinSatFromControls()
{
    auto& c = sharedResources.sharedColors;
    c.graphBandRandomMinSatEnabled = bandMinSatEnableToggle.getToggleState();
    c.graphBandRandomMinSaturation = juce::jlimit (0.0f, 1.0f, (float) bandMinSatSlider.getValue());
    const int pct = juce::roundToInt (c.graphBandRandomMinSaturation * 100.0f);
    bandMinSatPercentLabel.setText (juce::String (pct) + "%", juce::dontSendNotification);
    const bool en = c.graphBandRandomMinSatEnabled;
    bandMinSatLabel.setEnabled (en);
    bandMinSatSlider.setEnabled (en);
    bandMinSatPercentLabel.setEnabled (en);

    // Refresh faceplate power/glow and graph handles/fills that use the floor.
    if (auto* ed = findParentComponentOfClass<EqEditor>())
        ed->applyFaceplateBandChrome();
    if (auto* main = findParentComponentOfClass<MainComponent>())
    {
        main->getSharedResources().makeActive();
        main->getFrequencyResponseComponent().repaint();
    }

    notifyHostSaveUiPrefs();
}

void SpectrumComponent::Content::notifyHostSaveUiPrefs()
{
    if (auto* ed = findParentComponentOfClass<EqEditor>())
        ed->requestSaveUiPrefs();
}

void SpectrumComponent::Content::layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    slider.setBounds (area.removeFromTop (kSpectrumSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kSpectrumRowGap);
}

void SpectrumComponent::Content::layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo)
{
    label.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    combo.setBounds (area.removeFromTop (kSpectrumSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kSpectrumRowGap);
}

void SpectrumComponent::Content::layoutTogglePair (juce::Rectangle<int>& area, juce::ToggleButton& left, juce::ToggleButton& right)
{
    auto row = area.removeFromTop (kSpectrumToggleH).removeFromLeft (juce::jmin (420, area.getWidth()));
    const int gap = 10;
    const int half = (row.getWidth() - gap) / 2;
    left.setBounds (row.removeFromLeft (half));
    row.removeFromLeft (gap);
    right.setBounds (row);
    area.removeFromTop (4);
}

void SpectrumComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumFill));
    gradientEditor.repaint();
}

int SpectrumComponent::Content::getPreferredHeight() const
{
    // title + show bins + enable + block + spectral method + refresh/avg + curve smooth
    // + multicolor + band chrome match + crosshair + layers + scale
    // + opacity/fill/path/band/sum + sumGlow toggle + 3 sum glow + postGlow toggle + 3 post glow + hold + gradient
    return kSpectrumPadY * 2
           + 24 + 8
           + 22 + 6
           + 22 + 6
           + (5 * (kSpectrumLabelH + kSpectrumLabelGap + kSpectrumSliderH + kSpectrumRowGap))
           + 22 + 6
           + 22 + 6   // multicolor
           + 22 + 6   // match power/glow to band colours
           + 22 + 6   // band min sat enable + slider row
           + 22 + 8
           + kSpectrumLabelH + kSpectrumLabelGap
           + (3 * (kSpectrumToggleH + 4)) + kSpectrumRowGap
           + kSpectrumLabelH + kSpectrumLabelGap + 24 + kSpectrumRowGap
           + (5 * (kSpectrumLabelH + kSpectrumLabelGap + kSpectrumSliderH + kSpectrumRowGap))
           + 22 + 6
           + (3 * (kSpectrumLabelH + kSpectrumLabelGap + kSpectrumSliderH + kSpectrumRowGap))
           + 22 + 6
           + (3 * (kSpectrumLabelH + kSpectrumLabelGap + kSpectrumSliderH + kSpectrumRowGap))
           + (kSpectrumLabelH + kSpectrumLabelGap + kSpectrumSliderH + kSpectrumRowGap)
           + kSpectrumLabelH + kSpectrumLabelGap + gradientEditor.getPreferredHeight() + kSpectrumRowGap;
}

void SpectrumComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kSpectrumPadX, kSpectrumPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    gradientLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                  .removeFromLeft (juce::jmin (520, area.getWidth())));
    area.removeFromTop (kSpectrumRowGap);

    showBinsToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);

    enableToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);

    layoutComboRow (area, blockSizeLabel, blockSizeCombo);

    // Wider than default combo row so "Lattice (zero latency)" never ellipsizes.
    {
        spectralMethodLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
        area.removeFromTop (kSpectrumLabelGap);
        spectralMethodCombo.setBounds (
            area.removeFromTop (kSpectrumSliderH).removeFromLeft (juce::jmin (320, area.getWidth())));
        area.removeFromTop (kSpectrumRowGap);
    }

    layoutSliderRow (area, refreshLabel, refreshSlider);
    layoutSliderRow (area, avgLabel, avgSlider);
    layoutComboRow (area, curveSmoothLabel, curveSmoothCombo);

    multicolorBandFillToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (320, area.getWidth())));
    area.removeFromTop (6);

    // Full plain caption — measure width so it never ships as "..."
    {
        const juce::Font tfont (juce::FontOptions (14.0f));
        const int tw = juce::jmax (220, (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (tfont, bandChromeMatchHandlesToggle.getButtonText())) + 36);
        bandChromeMatchHandlesToggle.setBounds (
            area.removeFromTop (22).removeFromLeft (juce::jmin (tw, area.getWidth())));
    }
    area.removeFromTop (6);

    {
        auto row = area.removeFromTop (22);
        const juce::Font tfont (juce::FontOptions (14.0f));
        const int enW = juce::jmax (100, (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (tfont, "Band min sat")) + 36);
        bandMinSatEnableToggle.setBounds (row.removeFromLeft (juce::jmin (enW, row.getWidth())));
        row.removeFromLeft (8);
        const int pctW = juce::jmax (36, (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (tfont, "100%")) + 6);
        const int labW = juce::jmax (52, (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (tfont, "Min sat")) + 8);
        bandMinSatPercentLabel.setBounds (row.removeFromRight (pctW));
        row.removeFromRight (4);
        bandMinSatLabel.setBounds (row.removeFromLeft (labW));
        row.removeFromLeft (4);
        bandMinSatSlider.setBounds (row);
        const bool en = bandMinSatEnableToggle.getToggleState();
        bandMinSatLabel.setEnabled (en);
        bandMinSatSlider.setEnabled (en);
        bandMinSatPercentLabel.setEnabled (en);
    }
    area.removeFromTop (6);

    showCrosshairToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (260, area.getWidth())));
    area.removeFromTop (8);

    layersLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    layoutTogglePair (area, preCurveToggle, preFillToggle);
    layoutTogglePair (area, postCurveToggle, postFillToggle);
    layoutTogglePair (area, holdCurveToggle, holdFillToggle);
    area.removeFromTop (kSpectrumRowGap);

    scaleLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    {
        auto row = area.removeFromTop (24).removeFromLeft (juce::jmin (420, area.getWidth()));
        const int gap = 6;
        const int buttonW = (row.getWidth() - gap * 2) / 3;
        linButton.setBounds (row.removeFromLeft (buttonW));
        row.removeFromLeft (gap);
        logButton.setBounds (row.removeFromLeft (buttonW));
        row.removeFromLeft (gap);
        stButton.setBounds (row);
    }
    area.removeFromTop (kSpectrumRowGap);

    layoutSliderRow (area, opacityLabel, opacitySlider);
    layoutSliderRow (area, fillOpacityLabel, fillOpacitySlider);
    layoutSliderRow (area, pathWidthLabel, pathWidthSlider);
    layoutSliderRow (area, bandLineWidthLabel, bandLineWidthSlider);
    layoutSliderRow (area, sumLineWidthLabel, sumLineWidthSlider);

    sumGlowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);
    layoutSliderRow (area, sumGlowRadiusLabel, sumGlowRadiusSlider);
    layoutSliderRow (area, sumGlowSpreadLabel, sumGlowSpreadSlider);
    layoutSliderRow (area, sumGlowOpacityLabel, sumGlowOpacitySlider);

    postGlowToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (6);
    layoutSliderRow (area, spectrumGlowRadiusLabel, spectrumGlowRadiusSlider);
    layoutSliderRow (area, spectrumGlowSpreadLabel, spectrumGlowSpreadSlider);
    layoutSliderRow (area, spectrumGlowOpacityLabel, spectrumGlowOpacitySlider);

    layoutSliderRow (area, holdTimeLabel, holdTimeSlider);
}

SpectrumComponent::SpectrumComponent (SharedResources& resources,
                                      juce::AudioProcessorValueTreeState& state,
                                      ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

SpectrumComponent::~SpectrumComponent()
{
    colourRamps.removeChangeListener (this);
}

void SpectrumComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void SpectrumComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void SpectrumComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
