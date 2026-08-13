#include "SpectrumComponent.h"

#include "../../EqEditor.h"
#include "../../MainComponent.h"
#include "../../ModuleLookPresets.h"
#include "../../Spectral/SpectralMethod.h"
#include "../../Visualizer/SpectrumAnalysis.h"
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
      gradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      curveGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      preFillGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      preCurveGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      holdFillGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      holdCurveGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      eqCurveGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      eqSumFillGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      eqBandCurveGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      eqBandFillGradientEditor (resources, GradientStripEditor::ModeFamily::spatial, &ramps.getPresets()),
      leftColourRow  (resources, "L",    &SharedColors::spectrumLineL,    &SharedColors::spectrumFillL),
      rightColourRow (resources, "R",    &SharedColors::spectrumLineR,    &SharedColors::spectrumFillR),
      midColourRow   (resources, "Mid",  &SharedColors::spectrumLineMid,  &SharedColors::spectrumFillMid),
      sideColourRow  (resources, "Side", &SharedColors::spectrumLineSide, &SharedColors::spectrumFillSide),
      analysisSection (resources, "spectrum.analysis", "Analysis", false),
      displaySection (resources, "spectrum.display", "Display", false),
      strokeSection (resources, "spectrum.stroke", "Stroke and fill", false),
      glowSection (resources, "spectrum.glow", "Glow", false),
      rampsSection (resources, "spectrum.ramps", "Ramps", false)
{
    titleLabel.setText ("Spectrum", juce::dontSendNotification);
    titleLabel.setFont (SharedResources::uiFont (20.0f));
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

    analysisLabel.setText ("Analysis", juce::dontSendNotification);
    styleSettingsCombo (analysisCombo);
    {
        const auto names = SpectrumAnalysis::channelNames();
        for (int i = 0; i < names.size(); ++i)
            analysisCombo.addItem (names[i], i + 1);
    }
    analysisCombo.setTooltip (
        "Which signal the spectrum analyser shows. Mid + Side and Left + Right "
        "overlay both curves with their own colours. Does not change FFT block size.");
    addAndMakeVisible (analysisLabel);
    addAndMakeVisible (analysisCombo);
    analysisAttachment = std::make_unique<ComboBoxAttachment> (
        treeState, SpectrumAnalysis::channelParamId(), analysisCombo);

    octaveSmoothLabel.setText ("Smoothing", juce::dontSendNotification);
    styleSettingsCombo (octaveSmoothCombo);
    {
        const auto names = SpectrumAnalysis::octaveSmoothNames();
        for (int i = 0; i < names.size(); ++i)
            octaveSmoothCombo.addItem (names[i], i + 1);
    }
    octaveSmoothCombo.setTooltip (
        "Fractional-octave smoothing like Span (1/3 oct is the usual mix setting). "
        "Smoothes the analysed curve so Mid and Side (or L and R) can be compared "
        "without FFT spikes. Does not drop FFT block size.");
    addAndMakeVisible (octaveSmoothLabel);
    addAndMakeVisible (octaveSmoothCombo);
    octaveSmoothAttachment = std::make_unique<ComboBoxAttachment> (
        treeState, SpectrumAnalysis::octaveSmoothParamId(), octaveSmoothCombo);

    channelColoursLabel.setText ("Channel Colours", juce::dontSendNotification);
    addAndMakeVisible (channelColoursLabel);
    addAndMakeVisible (leftColourRow);
    addAndMakeVisible (rightColourRow);
    addAndMakeVisible (midColourRow);
    addAndMakeVisible (sideColourRow);

    curveSmoothLabel.setText ("Display Smooth", juce::dontSendNotification);
    styleSettingsCombo (curveSmoothCombo);
    curveSmoothCombo.addItem ("Off", 1);
    curveSmoothCombo.addItem ("Low", 2);
    curveSmoothCombo.addItem ("Med", 3);
    curveSmoothCombo.addItem ("High", 4);
    curveSmoothCombo.setTooltip (
        "Display-space blur on the drawn path. Cosmetic only. "
        "Use Smoothing (1/3 oct) to tame FFT spikes without lowering block size.");
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

    styleToggle (showEqCurvesToggle);
    showEqCurvesToggle.setTooltip (
        "When off, band and sum EQ fills and curves are hidden so only the analyser remains. "
        "Handles stay visible.");
    addAndMakeVisible (showEqCurvesToggle);
    showEqCurvesAttachment = std::make_unique<ButtonAttachment> (
        treeState, "EQ_SHOW_CURVES_ID", showEqCurvesToggle);

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

    fadeSectionLabel.setText ("Fade Time", juce::dontSendNotification);
    preCurveFadeLabel.setText ("Pre Curve Fade", juce::dontSendNotification);
    preFillFadeLabel.setText ("Pre Fill Fade", juce::dontSendNotification);
    postCurveFadeLabel.setText ("Post Curve Fade", juce::dontSendNotification);
    postFillFadeLabel.setText ("Post Fill Fade", juce::dontSendNotification);
    holdCurveFadeLabel.setText ("Hold Curve Fade", juce::dontSendNotification);
    holdFillFadeLabel.setText ("Hold Fill Fade", juce::dontSendNotification);
    eqCurveFadeLabel.setText ("EQ Curve Fade", juce::dontSendNotification);
    eqFillFadeLabel.setText ("EQ Fill Fade", juce::dontSendNotification);

    auto setupFadeSlider = [this] (juce::Slider& slider, const char* id,
                                   std::unique_ptr<SliderAttachment>& attachment)
    {
        styleSlider (slider);
        slider.setTextValueSuffix (" s");
        slider.setTooltip ("How long this layer eases toward new values. 0 is instant. Max 5 seconds.");
        addAndMakeVisible (slider);
        attachment = std::make_unique<SliderAttachment> (treeState, id, slider);
    };
    setupFadeSlider (preCurveFadeSlider, "SPECTRUM_PRE_CURVE_FADE_ID", preCurveFadeAttachment);
    setupFadeSlider (preFillFadeSlider, "SPECTRUM_PRE_FILL_FADE_ID", preFillFadeAttachment);
    setupFadeSlider (postCurveFadeSlider, "SPECTRUM_POST_CURVE_FADE_ID", postCurveFadeAttachment);
    setupFadeSlider (postFillFadeSlider, "SPECTRUM_POST_FILL_FADE_ID", postFillFadeAttachment);
    setupFadeSlider (holdCurveFadeSlider, "SPECTRUM_HOLD_CURVE_FADE_ID", holdCurveFadeAttachment);
    setupFadeSlider (holdFillFadeSlider, "SPECTRUM_HOLD_FILL_FADE_ID", holdFillFadeAttachment);
    setupFadeSlider (eqCurveFadeSlider, "EQ_CURVE_FADE_ID", eqCurveFadeAttachment);
    setupFadeSlider (eqFillFadeSlider, "EQ_FILL_FADE_ID", eqFillFadeAttachment);

    styleLabel (titleLabel);
    styleLabel (blockSizeLabel);
    styleLabel (spectralMethodLabel);
    styleLabel (refreshLabel);
    styleLabel (avgLabel);
    styleLabel (analysisLabel);
    styleLabel (octaveSmoothLabel);
    styleLabel (channelColoursLabel);
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
    styleLabel (fadeSectionLabel);
    styleLabel (preCurveFadeLabel);
    styleLabel (preFillFadeLabel);
    styleLabel (postCurveFadeLabel);
    styleLabel (postFillFadeLabel);
    styleLabel (holdCurveFadeLabel);
    styleLabel (holdFillFadeLabel);
    styleLabel (eqCurveFadeLabel);
    styleLabel (eqFillFadeLabel);
    fadeSectionLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));

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
    addAndMakeVisible (fadeSectionLabel);
    addAndMakeVisible (preCurveFadeLabel);
    addAndMakeVisible (preFillFadeLabel);
    addAndMakeVisible (postCurveFadeLabel);
    addAndMakeVisible (postFillFadeLabel);
    addAndMakeVisible (holdCurveFadeLabel);
    addAndMakeVisible (holdFillFadeLabel);
    addAndMakeVisible (eqCurveFadeLabel);
    addAndMakeVisible (eqFillFadeLabel);

    auto wireSpatialRamp = [this] (juce::ToggleButton& toggle,
                                   std::unique_ptr<ButtonAttachment>& attachment,
                                   const char* paramId,
                                   juce::Label& label,
                                   const juce::String& labelText,
                                   GradientStripEditor& editor,
                                   ColourRampBank::Target target,
                                   const juce::String& tooltip)
    {
        styleToggle (toggle);
        toggle.setTooltip (tooltip);
        addAndMakeVisible (toggle);
        attachment = std::make_unique<ButtonAttachment> (treeState, paramId, toggle);
        toggle.onClick = [this, &toggle, target]
        {
            auto& ramp = colourRamps.get (target);
            if (toggle.getToggleState() && ramp.stops.size() >= 2 && ! ramp.enabled)
            {
                ramp.enabled = true;
                ++ramp.revision;
                colourRamps.notifyEdited();
            }
            syncRampControlsEnabled();
        };

        label.setText (labelText, juce::dontSendNotification);
        styleLabel (label);
        label.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
        addAndMakeVisible (label);

        editor.setRamp (&colourRamps.get (target));
        editor.onRampChanged = [this] { colourRamps.notifyEdited(); };
        editor.onRampPreview = [this] { colourRamps.notifyPreview(); };
        editor.onSamplePath = [this, target]
        {
            if (auto* main = findParentComponentOfClass<MainComponent>())
                main->beginRampSamplingForTarget (target);
        };
        editor.onPreferredHeightChanged = [this]
        {
            if (auto* parent = findParentComponentOfClass<SpectrumComponent>())
                parent->resized();
            else
                resized();

            if (auto* menu = findParentComponentOfClass<Menu>())
                menu->notifyContentHeightChanged();
        };
        addAndMakeVisible (editor);
    };

    wireSpatialRamp (usePreFillRampToggle, usePreFillRampAttachment, "SPECTRUM_PRE_FILL_RAMP_ID",
                     preFillGradientLabel, "Pre Fill Ramp", preFillGradientEditor,
                     ColourRampBank::Target::spectrumPreFill,
                     "Spatial colour ramp for the analyser pre fill only.");
    wireSpatialRamp (usePreCurveRampToggle, usePreCurveRampAttachment, "SPECTRUM_PRE_CURVE_RAMP_ID",
                     preCurveGradientLabel, "Pre Curve Ramp", preCurveGradientEditor,
                     ColourRampBank::Target::spectrumPreCurve,
                     "Spatial colour ramp for the analyser pre curve only.");
    wireSpatialRamp (useRampToggle, useRampAttachment, "SPECTRUM_USE_RAMP_ID",
                     gradientLabel, "Post Fill Ramp", gradientEditor,
                     ColourRampBank::Target::spectrumFill,
                     "Spatial colour ramp for the analyser post fill only.");
    wireSpatialRamp (useCurveRampToggle, useCurveRampAttachment, "SPECTRUM_CURVE_RAMP_ID",
                     curveGradientLabel, "Post Curve Ramp", curveGradientEditor,
                     ColourRampBank::Target::spectrumCurve,
                     "Spatial colour ramp for the analyser post curve only.");
    wireSpatialRamp (useHoldFillRampToggle, useHoldFillRampAttachment, "SPECTRUM_HOLD_FILL_RAMP_ID",
                     holdFillGradientLabel, "Hold Fill Ramp", holdFillGradientEditor,
                     ColourRampBank::Target::spectrumHoldFill,
                     "Spatial colour ramp for the analyser hold fill only.");
    wireSpatialRamp (useHoldCurveRampToggle, useHoldCurveRampAttachment, "SPECTRUM_HOLD_CURVE_RAMP_ID",
                     holdCurveGradientLabel, "Hold Curve Ramp", holdCurveGradientEditor,
                     ColourRampBank::Target::spectrumHoldCurve,
                     "Spatial colour ramp for the analyser hold curve only.");
    wireSpatialRamp (useEqCurveRampToggle, useEqCurveRampAttachment, "EQ_CURVE_RAMP_ID",
                     eqCurveGradientLabel, "Sum Curve Ramp", eqCurveGradientEditor,
                     ColourRampBank::Target::eqCurve,
                     "Spatial colour ramp for the EQ sum curve only.");
    wireSpatialRamp (useEqSumFillRampToggle, useEqSumFillRampAttachment, "EQ_SUM_FILL_RAMP_ID",
                     eqSumFillGradientLabel, "Sum Fill Ramp", eqSumFillGradientEditor,
                     ColourRampBank::Target::eqSumFill,
                     "Spatial colour ramp for the EQ sum fill only.");
    wireSpatialRamp (useEqBandCurveRampToggle, useEqBandCurveRampAttachment, "EQ_BAND_CURVE_RAMP_ID",
                     eqBandCurveGradientLabel, "Band Curve Ramp", eqBandCurveGradientEditor,
                     ColourRampBank::Target::eqBandCurve,
                     "Spatial colour ramp for individual EQ band curves (tinted per band).");
    wireSpatialRamp (useEqBandFillRampToggle, useEqBandFillRampAttachment, "EQ_BAND_FILL_RAMP_ID",
                     eqBandFillGradientLabel, "Band Fill Ramp", eqBandFillGradientEditor,
                     ColourRampBank::Target::eqBandFill,
                     "Spatial colour ramp for individual EQ band fills (tinted per band).");
    syncRampControlsEnabled();

    wireSection (analysisSection);
    wireSection (displaySection);
    wireSection (strokeSection);
    wireSection (glowSection);
    wireSection (rampsSection);
}

void SpectrumComponent::Content::wireSection (SettingsSection& section)
{
    addAndMakeVisible (section);
    section.onChanged = [this]
    {
        resized();
        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
        if (auto* ed = findParentComponentOfClass<EqEditor>())
            ed->requestSaveUiPrefs();
    };
}

SpectrumComponent::Content::~Content()
{
    blockSizeCombo.setLookAndFeel (nullptr);
    spectralMethodCombo.setLookAndFeel (nullptr);
    analysisCombo.setLookAndFeel (nullptr);
    octaveSmoothCombo.setLookAndFeel (nullptr);
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
    label.setFont (SharedResources::uiFont (15.0f));
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
    curveGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumCurve));
    preFillGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumPreFill));
    preCurveGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumPreCurve));
    holdFillGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumHoldFill));
    holdCurveGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrumHoldCurve));
    eqCurveGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::eqCurve));
    eqSumFillGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::eqSumFill));
    eqBandCurveGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::eqBandCurve));
    eqBandFillGradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::eqBandFill));
    gradientEditor.repaint();
    curveGradientEditor.repaint();
    preFillGradientEditor.repaint();
    preCurveGradientEditor.repaint();
    holdFillGradientEditor.repaint();
    holdCurveGradientEditor.repaint();
    eqCurveGradientEditor.repaint();
    eqSumFillGradientEditor.repaint();
    eqBandCurveGradientEditor.repaint();
    eqBandFillGradientEditor.repaint();
    syncRampControlsEnabled();
}

void SpectrumComponent::Content::syncRampControlsEnabled()
{
    auto syncOne = [] (bool on, juce::Label& label, GradientStripEditor& editor)
    {
        label.setEnabled (on);
        editor.setEnabled (on);
        editor.setAlpha (on ? 1.0f : 0.45f);
    };
    syncOne (useRampToggle.getToggleState(), gradientLabel, gradientEditor);
    syncOne (useCurveRampToggle.getToggleState(), curveGradientLabel, curveGradientEditor);
    syncOne (usePreFillRampToggle.getToggleState(), preFillGradientLabel, preFillGradientEditor);
    syncOne (usePreCurveRampToggle.getToggleState(), preCurveGradientLabel, preCurveGradientEditor);
    syncOne (useHoldFillRampToggle.getToggleState(), holdFillGradientLabel, holdFillGradientEditor);
    syncOne (useHoldCurveRampToggle.getToggleState(), holdCurveGradientLabel, holdCurveGradientEditor);
    syncOne (useEqCurveRampToggle.getToggleState(), eqCurveGradientLabel, eqCurveGradientEditor);
    syncOne (useEqSumFillRampToggle.getToggleState(), eqSumFillGradientLabel, eqSumFillGradientEditor);
    syncOne (useEqBandCurveRampToggle.getToggleState(), eqBandCurveGradientLabel, eqBandCurveGradientEditor);
    syncOne (useEqBandFillRampToggle.getToggleState(), eqBandFillGradientLabel, eqBandFillGradientEditor);
}

int SpectrumComponent::Content::getPreferredHeight() const
{
    const int row = kSpectrumLabelH + kSpectrumLabelGap + kSpectrumSliderH + kSpectrumRowGap;
    const int tog = 22 + 6;
    auto rampH = [] (const GradientStripEditor& e)
    {
        return tog + kSpectrumLabelH + kSpectrumLabelGap + e.getPreferredHeight() + kSpectrumRowGap;
    };

    const int analysisBody = tog + tog + 4 * row
        + kSpectrumLabelH + kSpectrumLabelGap + (4 * (24 + 4)) + kSpectrumRowGap
        + 2 * row;
    const int displayBody = tog + tog + tog + tog + tog
        + kSpectrumLabelH + kSpectrumLabelGap + (3 * (kSpectrumToggleH + 4)) + kSpectrumRowGap
        + kSpectrumLabelH + kSpectrumLabelGap + 24 + kSpectrumRowGap;
    const int strokeBody = 5 * row + 9 * row; // widths + hold + 8 fades
    const int glowBody = tog + 3 * row + tog + 3 * row;
    const int rampsBody = rampH (preFillGradientEditor) + rampH (preCurveGradientEditor)
        + rampH (gradientEditor) + rampH (curveGradientEditor)
        + rampH (holdFillGradientEditor) + rampH (holdCurveGradientEditor)
        + rampH (eqCurveGradientEditor) + rampH (eqSumFillGradientEditor)
        + rampH (eqBandCurveGradientEditor) + rampH (eqBandFillGradientEditor);

    return kSpectrumPadY * 2
           + 24 + 8
           + analysisSection.heightFor (analysisBody)
           + displaySection.heightFor (displayBody)
           + strokeSection.heightFor (strokeBody)
           + glowSection.heightFor (glowBody)
           + rampsSection.heightFor (rampsBody);
}

void SpectrumComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kSpectrumPadX, kSpectrumPadY);

    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    auto layoutRampBlock = [] (juce::Rectangle<int>& dest,
                               juce::ToggleButton& toggle, juce::Label& label, GradientStripEditor& editor)
    {
        const juce::Font tfont (juce::FontOptions (14.0f));
        const int tw = juce::jmax (160, (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (tfont, toggle.getButtonText())) + 36);
        toggle.setBounds (dest.removeFromTop (22).removeFromLeft (juce::jmin (tw, dest.getWidth())));
        dest.removeFromTop (kSpectrumRowGap);

        label.setBounds (dest.removeFromTop (kSpectrumLabelH));
        dest.removeFromTop (kSpectrumLabelGap);
        editor.setBounds (dest.removeFromTop (editor.getPreferredHeight())
                              .removeFromLeft (juce::jmin (520, dest.getWidth())));
        dest.removeFromTop (kSpectrumRowGap);
    };

    analysisSection.applyVisible ({
        &showBinsToggle, &enableToggle, &blockSizeLabel, &blockSizeCombo,
        &spectralMethodLabel, &spectralMethodCombo, &analysisLabel, &analysisCombo,
        &octaveSmoothLabel, &octaveSmoothCombo, &channelColoursLabel,
        &leftColourRow, &rightColourRow, &midColourRow, &sideColourRow,
        &refreshLabel, &refreshSlider, &avgLabel, &avgSlider,
        &curveSmoothLabel, &curveSmoothCombo });
    analysisSection.placeHeader (area);
    if (analysisSection.isOpen())
    {
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

    {
        analysisLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
        area.removeFromTop (kSpectrumLabelGap);
        analysisCombo.setBounds (
            area.removeFromTop (kSpectrumSliderH).removeFromLeft (juce::jmin (280, area.getWidth())));
        area.removeFromTop (kSpectrumRowGap);
    }

    {
        octaveSmoothLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
        area.removeFromTop (kSpectrumLabelGap);
        octaveSmoothCombo.setBounds (
            area.removeFromTop (kSpectrumSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
        area.removeFromTop (kSpectrumRowGap);
    }

    channelColoursLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    {
        const int rowW = juce::jmin (360, area.getWidth());
        leftColourRow.setBounds (area.removeFromTop (24).removeFromLeft (rowW));
        area.removeFromTop (4);
        rightColourRow.setBounds (area.removeFromTop (24).removeFromLeft (rowW));
        area.removeFromTop (4);
        midColourRow.setBounds (area.removeFromTop (24).removeFromLeft (rowW));
        area.removeFromTop (4);
        sideColourRow.setBounds (area.removeFromTop (24).removeFromLeft (rowW));
        area.removeFromTop (kSpectrumRowGap);
    }

    layoutSliderRow (area, refreshLabel, refreshSlider);
    layoutSliderRow (area, avgLabel, avgSlider);
    layoutComboRow (area, curveSmoothLabel, curveSmoothCombo);
    }

    displaySection.applyVisible ({
        &multicolorBandFillToggle, &bandChromeMatchHandlesToggle,
        &bandMinSatEnableToggle, &bandMinSatLabel, &bandMinSatSlider, &bandMinSatPercentLabel,
        &showCrosshairToggle, &showEqCurvesToggle, &layersLabel,
        &preCurveToggle, &preFillToggle, &postCurveToggle, &postFillToggle,
        &holdCurveToggle, &holdFillToggle, &scaleLabel, &linButton, &logButton, &stButton });
    displaySection.placeHeader (area);
    if (displaySection.isOpen())
    {
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
    area.removeFromTop (6);

    {
        const juce::Font tfont (juce::FontOptions (14.0f));
        const int tw = juce::jmax (160, (int) std::ceil (
            juce::GlyphArrangement::getStringWidth (tfont, showEqCurvesToggle.getButtonText())) + 36);
        showEqCurvesToggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (tw, area.getWidth())));
    }
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
    }

    strokeSection.applyVisible ({
        &opacityLabel, &opacitySlider, &fillOpacityLabel, &fillOpacitySlider,
        &pathWidthLabel, &pathWidthSlider, &bandLineWidthLabel, &bandLineWidthSlider,
        &sumLineWidthLabel, &sumLineWidthSlider, &holdTimeLabel, &holdTimeSlider,
        &fadeSectionLabel,
        &preCurveFadeLabel, &preCurveFadeSlider, &preFillFadeLabel, &preFillFadeSlider,
        &postCurveFadeLabel, &postCurveFadeSlider, &postFillFadeLabel, &postFillFadeSlider,
        &holdCurveFadeLabel, &holdCurveFadeSlider, &holdFillFadeLabel, &holdFillFadeSlider,
        &eqCurveFadeLabel, &eqCurveFadeSlider, &eqFillFadeLabel, &eqFillFadeSlider });
    strokeSection.placeHeader (area);
    if (strokeSection.isOpen())
    {
    layoutSliderRow (area, opacityLabel, opacitySlider);
    layoutSliderRow (area, fillOpacityLabel, fillOpacitySlider);
    layoutSliderRow (area, pathWidthLabel, pathWidthSlider);
    layoutSliderRow (area, bandLineWidthLabel, bandLineWidthSlider);
    layoutSliderRow (area, sumLineWidthLabel, sumLineWidthSlider);
    layoutSliderRow (area, holdTimeLabel, holdTimeSlider);

    fadeSectionLabel.setBounds (area.removeFromTop (kSpectrumLabelH));
    area.removeFromTop (kSpectrumLabelGap);
    layoutSliderRow (area, preCurveFadeLabel, preCurveFadeSlider);
    layoutSliderRow (area, preFillFadeLabel, preFillFadeSlider);
    layoutSliderRow (area, postCurveFadeLabel, postCurveFadeSlider);
    layoutSliderRow (area, postFillFadeLabel, postFillFadeSlider);
    layoutSliderRow (area, holdCurveFadeLabel, holdCurveFadeSlider);
    layoutSliderRow (area, holdFillFadeLabel, holdFillFadeSlider);
    layoutSliderRow (area, eqCurveFadeLabel, eqCurveFadeSlider);
    layoutSliderRow (area, eqFillFadeLabel, eqFillFadeSlider);
    }

    glowSection.applyVisible ({
        &sumGlowToggle, &sumGlowRadiusLabel, &sumGlowRadiusSlider,
        &sumGlowSpreadLabel, &sumGlowSpreadSlider, &sumGlowOpacityLabel, &sumGlowOpacitySlider,
        &postGlowToggle, &spectrumGlowRadiusLabel, &spectrumGlowRadiusSlider,
        &spectrumGlowSpreadLabel, &spectrumGlowSpreadSlider,
        &spectrumGlowOpacityLabel, &spectrumGlowOpacitySlider });
    glowSection.placeHeader (area);
    if (glowSection.isOpen())
    {
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
    }

    rampsSection.applyVisible ({
        &usePreFillRampToggle, &preFillGradientLabel, &preFillGradientEditor,
        &usePreCurveRampToggle, &preCurveGradientLabel, &preCurveGradientEditor,
        &useRampToggle, &gradientLabel, &gradientEditor,
        &useCurveRampToggle, &curveGradientLabel, &curveGradientEditor,
        &useHoldFillRampToggle, &holdFillGradientLabel, &holdFillGradientEditor,
        &useHoldCurveRampToggle, &holdCurveGradientLabel, &holdCurveGradientEditor,
        &useEqCurveRampToggle, &eqCurveGradientLabel, &eqCurveGradientEditor,
        &useEqSumFillRampToggle, &eqSumFillGradientLabel, &eqSumFillGradientEditor,
        &useEqBandCurveRampToggle, &eqBandCurveGradientLabel, &eqBandCurveGradientEditor,
        &useEqBandFillRampToggle, &eqBandFillGradientLabel, &eqBandFillGradientEditor });
    rampsSection.placeHeader (area);
    if (rampsSection.isOpen())
    {
        layoutRampBlock (area, usePreFillRampToggle, preFillGradientLabel, preFillGradientEditor);
        layoutRampBlock (area, usePreCurveRampToggle, preCurveGradientLabel, preCurveGradientEditor);
        layoutRampBlock (area, useRampToggle, gradientLabel, gradientEditor);
        layoutRampBlock (area, useCurveRampToggle, curveGradientLabel, curveGradientEditor);
        layoutRampBlock (area, useHoldFillRampToggle, holdFillGradientLabel, holdFillGradientEditor);
        layoutRampBlock (area, useHoldCurveRampToggle, holdCurveGradientLabel, holdCurveGradientEditor);
        layoutRampBlock (area, useEqCurveRampToggle, eqCurveGradientLabel, eqCurveGradientEditor);
        layoutRampBlock (area, useEqSumFillRampToggle, eqSumFillGradientLabel, eqSumFillGradientEditor);
        layoutRampBlock (area, useEqBandCurveRampToggle, eqBandCurveGradientLabel, eqBandCurveGradientEditor);
        layoutRampBlock (area, useEqBandFillRampToggle, eqBandFillGradientLabel, eqBandFillGradientEditor);
    }
}

SpectrumComponent::Content::ColourSwatch::ColourSwatch (SharedResources& resources,
                                                       juce::Colour SharedColors::* member,
                                                       juce::String captionIn)
    : sharedResources (resources),
      colourMember (member),
      caption (std::move (captionIn))
{
    setMouseCursor (juce::MouseCursor::PointingHandCursor);
}

void SpectrumComponent::Content::ColourSwatch::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    const int chip = juce::jmin (bounds.getHeight() - 2, 18);
    auto swatch = bounds.removeFromLeft (chip).toFloat().reduced (1.0f);
    const auto colour = sharedResources.sharedColors.*colourMember;

    g.setColour (colour);
    g.fillRoundedRectangle (swatch, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawRoundedRectangle (swatch, 3.0f, 1.0f);

    bounds.removeFromLeft (6);
    g.setColour (sharedResources.sharedColors.menuLabelTextColor1);
    g.setFont (SharedResources::uiFont (13.0f));
    g.drawText (caption, bounds, juce::Justification::centredLeft, false);
}

void SpectrumComponent::Content::ColourSwatch::mouseUp (const juce::MouseEvent& e)
{
    if (e.mouseWasClicked())
        launchSelector();
}

void SpectrumComponent::Content::ColourSwatch::launchSelector()
{
    auto* editor = findParentComponentOfClass<EqEditor>();

    class SelectorPanel : public juce::Component,
                          private juce::ChangeListener
    {
    public:
        SelectorPanel (SharedResources& resources,
                       juce::Colour SharedColors::* member,
                       juce::Component* swatchToRepaint,
                       EqEditor* editorIn)
            : sharedResources (resources),
              colourMember (member),
              swatch (swatchToRepaint),
              editor (editorIn),
              selector (juce::ColourSelector::showColourAtTop
                        | juce::ColourSelector::showColourspace
                        | juce::ColourSelector::showAlphaChannel)
        {
            selector.setCurrentColour (sharedResources.sharedColors.*colourMember, juce::dontSendNotification);
            selector.addChangeListener (this);
            addAndMakeVisible (selector);
            setSize (280, 320);
        }

        ~SelectorPanel() override
        {
            selector.removeChangeListener (this);
        }

        void resized() override
        {
            selector.setBounds (getLocalBounds().reduced (6));
        }

    private:
        void changeListenerCallback (juce::ChangeBroadcaster*) override
        {
            sharedResources.sharedColors.*colourMember = selector.getCurrentColour();

            if (swatch != nullptr)
                swatch->repaint();

            if (editor != nullptr)
                editor->requestSaveUiPrefs();
        }

        SharedResources& sharedResources;
        juce::Colour SharedColors::* colourMember;
        juce::Component* swatch = nullptr;
        EqEditor* editor = nullptr;
        juce::ColourSelector selector;
    };

    auto* panel = new SelectorPanel (sharedResources, colourMember, this, editor);
    juce::CallOutBox::launchAsynchronously (std::unique_ptr<juce::Component> (panel),
                                            getScreenBounds(),
                                            nullptr);
}

SpectrumComponent::Content::ChannelColourRow::ChannelColourRow (SharedResources& resources,
                                                                const juce::String& name,
                                                                juce::Colour SharedColors::* lineMember,
                                                                juce::Colour SharedColors::* fillMember)
    : lineSwatch (resources, lineMember, "Line"),
      fillSwatch (resources, fillMember, "Fill")
{
    nameLabel.setText (name, juce::dontSendNotification);
    nameLabel.setFont (SharedResources::uiFont (13.0f));
    nameLabel.setJustificationType (juce::Justification::centredLeft);
    nameLabel.setMinimumHorizontalScale (1.0f);
    nameLabel.setColour (juce::Label::textColourId, resources.sharedColors.menuLabelTextColor1);
    addAndMakeVisible (nameLabel);
    addAndMakeVisible (lineSwatch);
    addAndMakeVisible (fillSwatch);
}

void SpectrumComponent::Content::ChannelColourRow::resized()
{
    auto area = getLocalBounds();
    const juce::Font font (SharedResources::uiFont (13.0f));
    const int nameW = juce::jmax (36, (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, nameLabel.getText())) + 8);
    nameLabel.setBounds (area.removeFromLeft (nameW));
    area.removeFromLeft (8);
    const int swatchW = juce::jmax (70, (int) std::ceil (juce::GlyphArrangement::getStringWidth (font, "Fill")) + 28);
    lineSwatch.setBounds (area.removeFromLeft (swatchW));
    area.removeFromLeft (10);
    fillSwatch.setBounds (area.removeFromLeft (swatchW));
}

void SpectrumComponent::Content::ChannelColourRow::refresh()
{
    lineSwatch.refresh();
    fillSwatch.refresh();
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
