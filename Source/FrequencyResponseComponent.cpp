#include "EqProcessor.h"
#include "EqEditor.h"
#include "FrequencyResponseComponent.h"
#include "OptionBoxMenu.h"
#include "BinaryData.h"
#include "FilterSlope.h"
#include "FilterType.h"
#include "BandChannel.h"
#include "LfoMod.h"
#include "ComboBoxLookAndFeel.h"
#include <JuceHeader.h>

namespace
{
    // Per-pixel magnitude sampling. Step>1 with hold-fill creates 2-px stair plateaus that the
    // sum path (one vertex per pixel) draws as visible jaggies; band adaptive downsampling
    // skipped those flats and looked smoother on the same data.
    constexpr int responseSampleStep = 1;
}

FrequencyResponseComponent::FrequencyResponseComponent(EqProcessor& processor)
    : processor(processor), parameters(processor.treeState),
    onOffButton1(std::make_unique<OnOffButton1>(parameters, "highpassOnOff")),
    onOffButton2(std::make_unique<OnOffButton1>(parameters, "lowpassOnOff")),
    onOffButton3(std::make_unique<OnOffButton1>(parameters, "highShelfOnOff")),
    onOffButton4(std::make_unique<OnOffButton1>(parameters, "lowShelfOnOff")),
    onOffButton5(std::make_unique<OnOffButton1>(parameters, "band1OnOff")),
    onOffButton6(std::make_unique<OnOffButton1>(parameters, "band2OnOff")),
    onOffButton7(std::make_unique<OnOffButton1>(parameters, "band3OnOff")),
    onOffButton8(std::make_unique<OnOffButton1>(parameters, "band4OnOff")),
    outputGainScrubber (processor.treeState, &processor.getUndoManager())

{
    // FRC sits transparent over the Visualizer. Without a paint cache, every
    // analyser Graph::repaint() also re-runs this (heavy) EQ paint path.
    // Buffering restores 240-era analyser feel: spectrum ticks without rebuilding curves.
    setBufferedToImage (true);

    auto* highpassOnOffParameter = parameters.getParameter("highpassOnOff");
    if (highpassOnOffParameter != nullptr) {
        highpassOnOff = highpassOnOffParameter->getValue() > 0.5f;
    }

    precomputeLogFrequencies();
    
    logMin = std::log10(20);
    logMax = std::log10(20000);


    addAndMakeVisible(infoLabel);
    infoLabel.setJustificationType(juce::Justification::centred);
    infoLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    infoLabel.setAlwaysOnTop(true);

    parameters.addParameterListener("band1Gain", this);
    parameters.addParameterListener("band1Frequency", this);
    parameters.addParameterListener("band1Q", this);
    parameters.addParameterListener("band1Type", this);
    parameters.addParameterListener("band2Gain", this);
    parameters.addParameterListener("band2Frequency", this);
    parameters.addParameterListener("band2Q", this);
    parameters.addParameterListener("band2Type", this);
    parameters.addParameterListener("band3Gain", this);
    parameters.addParameterListener("band3Frequency", this);
    parameters.addParameterListener("band3Q", this);
    parameters.addParameterListener("band3Type", this);
    parameters.addParameterListener("band4Gain", this);
    parameters.addParameterListener("band4Frequency", this);
    parameters.addParameterListener("band4Q", this);
    parameters.addParameterListener("band4Type", this);
    parameters.addParameterListener("highpassCutoff", this);
    parameters.addParameterListener("highpassQ", this);
    parameters.addParameterListener("highpassGain", this);
    parameters.addParameterListener("highpassType", this);
    parameters.addParameterListener("highpassSlope", this);
    parameters.addParameterListener("lowpassCutoff", this);
    parameters.addParameterListener("lowpassQ", this);
    parameters.addParameterListener("lowpassGain", this);
    parameters.addParameterListener("lowpassType", this);
    parameters.addParameterListener("lowpassSlope", this);
    parameters.addParameterListener("highShelfGain", this);
    parameters.addParameterListener("highShelfFrequency", this);
    parameters.addParameterListener("highShelfQ", this);
    parameters.addParameterListener("highShelfType", this);
    parameters.addParameterListener("highShelfSlope", this);
    parameters.addParameterListener("lowShelfGain", this);
    parameters.addParameterListener("lowShelfFrequency", this);
    parameters.addParameterListener("lowShelfQ", this);
    parameters.addParameterListener("lowShelfType", this);
    parameters.addParameterListener("lowShelfSlope", this);
    parameters.addParameterListener("band1Slope", this);
    parameters.addParameterListener("band2Slope", this);
    parameters.addParameterListener("band3Slope", this);
    parameters.addParameterListener("band4Slope", this);
    parameters.addParameterListener("band1OnOff", this);
    parameters.addParameterListener("band2OnOff", this);
    parameters.addParameterListener("band3OnOff", this);
    parameters.addParameterListener("band4OnOff", this);
    parameters.addParameterListener("highpassOnOff", this);
    parameters.addParameterListener("lowpassOnOff", this);
    parameters.addParameterListener("highShelfOnOff", this);
    parameters.addParameterListener("lowShelfOnOff", this);
    parameters.addParameterListener("EQ_DISPLAY_RANGE_ID", this);
    parameters.addParameterListener("EQ_BAND_PATH_WIDTH_ID", this);
    parameters.addParameterListener("EQ_SUM_PATH_WIDTH_ID", this);
    parameters.addParameterListener("EQ_SUM_GLOW_ENABLE_ID", this);
    parameters.addParameterListener("EQ_SUM_GLOW_RADIUS_ID", this);
    parameters.addParameterListener("EQ_SUM_GLOW_SPREAD_ID", this);
    parameters.addParameterListener("EQ_SUM_GLOW_OPACITY_ID", this);
    parameters.addParameterListener("EQ_MULTICOLOR_BAND_FILL_ID", this);
    parameters.addParameterListener("EQ_SHOW_CROSSHAIR_ID", this);
    parameters.addParameterListener("PROPORTIONAL_Q_ID", this);
    parameters.addParameterListener("band1Dynamic", this);
    parameters.addParameterListener("band2Dynamic", this);
    parameters.addParameterListener("band3Dynamic", this);
    parameters.addParameterListener("band4Dynamic", this);
    parameters.addParameterListener("highShelfDynamic", this);
    parameters.addParameterListener("lowShelfDynamic", this);
    parameters.addParameterListener("band1Sidechain", this);
    parameters.addParameterListener("band2Sidechain", this);
    parameters.addParameterListener("band3Sidechain", this);
    parameters.addParameterListener("band4Sidechain", this);
    parameters.addParameterListener("highShelfSidechain", this);
    parameters.addParameterListener("lowShelfSidechain", this);
    for (int i = 0; i < LfoMod::kNumMatrixSlots; ++i)
    {
        parameters.addParameterListener (LfoMod::slotSourceParamId (i), this);
        parameters.addParameterListener (LfoMod::slotAmountParamId (i), this);
        parameters.addParameterListener (LfoMod::slotDestParamId (i), this);
        parameters.addParameterListener (LfoMod::slotEnabledParamId (i), this);
    }
    parameters.addParameterListener("band1Spectral", this);
    parameters.addParameterListener("band2Spectral", this);
    parameters.addParameterListener("band3Spectral", this);
    parameters.addParameterListener("band4Spectral", this);
    parameters.addParameterListener("highShelfSpectral", this);
    parameters.addParameterListener("lowShelfSpectral", this);
    parameters.addParameterListener("band1SpectralDepth", this);
    parameters.addParameterListener("band2SpectralDepth", this);
    parameters.addParameterListener("band3SpectralDepth", this);
    parameters.addParameterListener("band4SpectralDepth", this);
    parameters.addParameterListener("highShelfSpectralDepth", this);
    parameters.addParameterListener("lowShelfSpectralDepth", this);
    parameters.addParameterListener (SpectralDynamics::spectralResHzParamId(), this);
    parameters.addParameterListener("band1SpectralResHz", this);
    parameters.addParameterListener("band2SpectralResHz", this);
    parameters.addParameterListener("band3SpectralResHz", this);
    parameters.addParameterListener("band4SpectralResHz", this);
    parameters.addParameterListener("highShelfSpectralResHz", this);
    parameters.addParameterListener("lowShelfSpectralResHz", this);
    parameters.addParameterListener("band1SpectralExpand", this);
    parameters.addParameterListener("band2SpectralExpand", this);
    parameters.addParameterListener("band3SpectralExpand", this);
    parameters.addParameterListener("band4SpectralExpand", this);
    parameters.addParameterListener("highShelfSpectralExpand", this);
    parameters.addParameterListener("lowShelfSpectralExpand", this);
    parameters.addParameterListener(SpectralDynamics::spectralPackParamId(), this);
    parameters.addParameterListener(SideCheck::enabledParamId(), this);
    parameters.addParameterListener(SideCheck::amountParamId(), this);
    parameters.addParameterListener(SideCheck::hpHzParamId(), this);
    parameters.addParameterListener(SideCheck::lpHzParamId(), this);
    parameters.addParameterListener(SideCheck::modeParamId(), this);

    juce::String optionBoxParamID = "TheActualParamIDForOptionBoxMenu";  // Replace with the actual ID
    optionBoxMenu = std::make_unique<OptionBoxMenu>(parameters);
    optionBoxMenu->undoManager = &processor.getUndoManager();

    optionBoxMenu->onBandKnobDragHighlight = [this] (int index)
    {
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (index);
    };
    optionBoxMenu->onBandCycled = [this] (int index)
    {
        lastOptionBoxBandIndex = index;
        lastHandlePopupWasOptionBox = true;
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (index);
    };
    
    addAndMakeVisible(*optionBoxMenu);
    optionBoxMenu->setVisible(false);
    optionBoxMenu->resized();

    auto styleRangeButton = [] (juce::TextButton& button)
    {
        button.setClickingTogglesState (false);
        button.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
        button.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
        button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };

    styleRangeButton (eqRangeMinusButton);
    styleRangeButton (eqRangePlusButton);
    eqRangeMinusButton.onClick = [this] { adjustEqDisplayRange (-1); };
    eqRangePlusButton.onClick = [this] { adjustEqDisplayRange (1); };
    // Plain ASCII tip: vertical EQ curve scale (not frequency octaves).
    const juce::String eqRangeTip ("EQ display range - vertical scale of the curve (+/-6, +/-12, or +/-24 dB)");
    eqRangeMinusButton.setTooltip (eqRangeTip);
    eqRangePlusButton.setTooltip (eqRangeTip);
    addAndMakeVisible (eqRangeMinusButton);
    addAndMakeVisible (eqRangePlusButton);

    styleRangeButton (modButton);
    modButton.setClickingTogglesState (true);
    modButton.setTooltip ("LFO / modulation matrix");
    modButton.onClick = [this]
    {
        if (editor != nullptr)
            editor->toggleModPanel();
    };
    addAndMakeVisible (modButton);

    styleRangeButton (uiModeButton);
    uiModeButton.setClickingTogglesState (false);
    uiModeButton.setTooltip ("Collapse to graph-only view");
    uiModeButton.onClick = [this]
    {
        if (editor != nullptr)
            editor->toggleCompactUi();
    };
    addAndMakeVisible (uiModeButton);

    styleRangeButton (proportionalQButton);
    proportionalQButton.setClickingTogglesState (true);
    proportionalQButton.setTooltip ("Proportional Q - tighten peaking Q as |gain| increases");
    addAndMakeVisible (proportionalQButton);
    proportionalQAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        parameters, "PROPORTIONAL_Q_ID", proportionalQButton);

    styleRangeButton (autoGainButton);
    autoGainButton.setClickingTogglesState (true);
    autoGainButton.setTooltip ("Auto Gain - match output loudness to pre-EQ level");
    addChildComponent (autoGainButton); // visible only in compact UI
    autoGainButton.setVisible (false);
    autoGainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        parameters, "autoGain", autoGainButton);

    // Compact name for the +/- display-range control (avoids "A +/-24" reading next to Auto Gain).
    eqRangeLabel.setText ("Range", juce::dontSendNotification);
    eqRangeLabel.setJustificationType (juce::Justification::centredRight);
    eqRangeLabel.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.75f));
    eqRangeLabel.setFont (juce::Font (11.0f));
    // Intercept so the Range caption can show its tooltip (same tip as - / +).
    eqRangeLabel.setInterceptsMouseClicks (true, false);
    eqRangeLabel.setTooltip ("EQ display range - vertical scale of the curve (+/-6, +/-12, or +/-24 dB)");
    addAndMakeVisible (eqRangeLabel);
    syncEqRangeControls();

    addChildComponent (outputGainScrubber); // visible only in compact UI
    outputGainScrubber.setVisible (false);

    syncDynamicCurveTimer();
    repaint();
}

void FrequencyResponseComponent::syncUiModeButton (bool isCompact)
{
    uiModeButton.setButtonText (isCompact ? juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xbc"))  // ▼
                                          : juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb2"))); // ▲
    uiModeButton.setTooltip (isCompact ? "Expand to full UI" : "Collapse to graph-only view");
    outputGainScrubber.setVisible (isCompact);
    autoGainButton.setVisible (isCompact);
    resized();
}

void FrequencyResponseComponent::syncModButton (bool isOpen)
{
    modButton.setToggleState (isOpen, juce::dontSendNotification);
}


FrequencyResponseComponent::~FrequencyResponseComponent()
{
    stopTimer();

    parameters.removeParameterListener("band1Gain", this);
    parameters.removeParameterListener("band1Frequency", this);
    parameters.removeParameterListener("band1Q", this);
    parameters.removeParameterListener("band1Type", this);
    parameters.removeParameterListener("band2Gain", this);
    parameters.removeParameterListener("band2Frequency", this);
    parameters.removeParameterListener("band2Q", this);
    parameters.removeParameterListener("band2Type", this);
    parameters.removeParameterListener("band3Gain", this);
    parameters.removeParameterListener("band3Frequency", this);
    parameters.removeParameterListener("band3Q", this);
    parameters.removeParameterListener("band3Type", this);
    parameters.removeParameterListener("band4Gain", this);
    parameters.removeParameterListener("band4Frequency", this);
    parameters.removeParameterListener("band4Q", this);
    parameters.removeParameterListener("band4Type", this);
    parameters.removeParameterListener("highpassCutoff", this);
    parameters.removeParameterListener("highpassQ", this);
    parameters.removeParameterListener("highpassGain", this);
    parameters.removeParameterListener("highpassType", this);
    parameters.removeParameterListener("highpassSlope", this);
    parameters.removeParameterListener("lowpassCutoff", this);
    parameters.removeParameterListener("lowpassQ", this);
    parameters.removeParameterListener("lowpassGain", this);
    parameters.removeParameterListener("lowpassType", this);
    parameters.removeParameterListener("lowpassSlope", this);
    parameters.removeParameterListener("highShelfGain", this);
    parameters.removeParameterListener("highShelfFrequency", this);
    parameters.removeParameterListener("highShelfQ", this);
    parameters.removeParameterListener("highShelfType", this);
    parameters.removeParameterListener("highShelfSlope", this);
    parameters.removeParameterListener("lowShelfGain", this);
    parameters.removeParameterListener("lowShelfFrequency", this);
    parameters.removeParameterListener("lowShelfQ", this);
    parameters.removeParameterListener("lowShelfType", this);
    parameters.removeParameterListener("lowShelfSlope", this);
    parameters.removeParameterListener("band1Slope", this);
    parameters.removeParameterListener("band2Slope", this);
    parameters.removeParameterListener("band3Slope", this);
    parameters.removeParameterListener("band4Slope", this);
    parameters.removeParameterListener("band1OnOff", this);
    parameters.removeParameterListener("band2OnOff", this);
    parameters.removeParameterListener("band3OnOff", this);
    parameters.removeParameterListener("band4OnOff", this);
    parameters.removeParameterListener("highpassOnOff", this);
    parameters.removeParameterListener("lowpassOnOff", this);
    parameters.removeParameterListener("highShelfOnOff", this);
    parameters.removeParameterListener("lowShelfOnOff", this);
    parameters.removeParameterListener("EQ_DISPLAY_RANGE_ID", this);
    parameters.removeParameterListener("EQ_BAND_PATH_WIDTH_ID", this);
    parameters.removeParameterListener("EQ_SUM_PATH_WIDTH_ID", this);
    parameters.removeParameterListener("EQ_SUM_GLOW_ENABLE_ID", this);
    parameters.removeParameterListener("EQ_SUM_GLOW_RADIUS_ID", this);
    parameters.removeParameterListener("EQ_SUM_GLOW_SPREAD_ID", this);
    parameters.removeParameterListener("EQ_SUM_GLOW_OPACITY_ID", this);
    parameters.removeParameterListener("EQ_MULTICOLOR_BAND_FILL_ID", this);
    parameters.removeParameterListener("EQ_SHOW_CROSSHAIR_ID", this);
    parameters.removeParameterListener("PROPORTIONAL_Q_ID", this);
    parameters.removeParameterListener("band1Dynamic", this);
    parameters.removeParameterListener("band2Dynamic", this);
    parameters.removeParameterListener("band3Dynamic", this);
    parameters.removeParameterListener("band4Dynamic", this);
    parameters.removeParameterListener("highShelfDynamic", this);
    parameters.removeParameterListener("lowShelfDynamic", this);
    parameters.removeParameterListener("band1Sidechain", this);
    parameters.removeParameterListener("band2Sidechain", this);
    parameters.removeParameterListener("band3Sidechain", this);
    parameters.removeParameterListener("band4Sidechain", this);
    parameters.removeParameterListener("highShelfSidechain", this);
    parameters.removeParameterListener("lowShelfSidechain", this);
    for (int i = 0; i < LfoMod::kNumMatrixSlots; ++i)
    {
        parameters.removeParameterListener (LfoMod::slotSourceParamId (i), this);
        parameters.removeParameterListener (LfoMod::slotAmountParamId (i), this);
        parameters.removeParameterListener (LfoMod::slotDestParamId (i), this);
        parameters.removeParameterListener (LfoMod::slotEnabledParamId (i), this);
    }
    parameters.removeParameterListener("band1Spectral", this);
    parameters.removeParameterListener("band2Spectral", this);
    parameters.removeParameterListener("band3Spectral", this);
    parameters.removeParameterListener("band4Spectral", this);
    parameters.removeParameterListener("highShelfSpectral", this);
    parameters.removeParameterListener("lowShelfSpectral", this);
    parameters.removeParameterListener("band1SpectralDepth", this);
    parameters.removeParameterListener("band2SpectralDepth", this);
    parameters.removeParameterListener("band3SpectralDepth", this);
    parameters.removeParameterListener("band4SpectralDepth", this);
    parameters.removeParameterListener("highShelfSpectralDepth", this);
    parameters.removeParameterListener("lowShelfSpectralDepth", this);
    parameters.removeParameterListener (SpectralDynamics::spectralResHzParamId(), this);
    parameters.removeParameterListener("band1SpectralResHz", this);
    parameters.removeParameterListener("band2SpectralResHz", this);
    parameters.removeParameterListener("band3SpectralResHz", this);
    parameters.removeParameterListener("band4SpectralResHz", this);
    parameters.removeParameterListener("highShelfSpectralResHz", this);
    parameters.removeParameterListener("lowShelfSpectralResHz", this);
    parameters.removeParameterListener("band1SpectralExpand", this);
    parameters.removeParameterListener("band2SpectralExpand", this);
    parameters.removeParameterListener("band3SpectralExpand", this);
    parameters.removeParameterListener("band4SpectralExpand", this);
    parameters.removeParameterListener("highShelfSpectralExpand", this);
    parameters.removeParameterListener("lowShelfSpectralExpand", this);
    parameters.removeParameterListener(SpectralDynamics::spectralPackParamId(), this);
    parameters.removeParameterListener(SideCheck::enabledParamId(), this);
    parameters.removeParameterListener(SideCheck::amountParamId(), this);
    parameters.removeParameterListener(SideCheck::hpHzParamId(), this);
    parameters.removeParameterListener(SideCheck::lpHzParamId(), this);
    parameters.removeParameterListener(SideCheck::modeParamId(), this);
}

float FrequencyResponseComponent::getEqDisplayRangeDb() const
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("EQ_DISPLAY_RANGE_ID")))
    {
        switch (choice->getIndex())
        {
            case 0:  return 6.0f;
            case 1:  return 12.0f;
            default: return 24.0f;
        }
    }

    return 24.0f;
}

float FrequencyResponseComponent::dbToY (float db, float height) const
{
    const float r = getEqDisplayRangeDb();
    return juce::jmap (db, -r, r, height, 0.0f);
}

float FrequencyResponseComponent::yToDb (float y, float height) const
{
    const float r = getEqDisplayRangeDb();
    return juce::jmap (y, height, 0.0f, -r, r);
}

float FrequencyResponseComponent::getBandPathWidth() const
{
    if (auto* p = parameters.getRawParameterValue ("EQ_BAND_PATH_WIDTH_ID"))
        return p->load();

    return 3.0f;
}

float FrequencyResponseComponent::getSumPathWidth() const
{
    if (auto* p = parameters.getRawParameterValue ("EQ_SUM_PATH_WIDTH_ID"))
        return p->load();

    return 3.0f;
}

bool FrequencyResponseComponent::isMulticolorBandFill() const
{
    if (auto* p = parameters.getRawParameterValue ("EQ_MULTICOLOR_BAND_FILL_ID"))
        return p->load() >= 0.5f;

    return true;
}

bool FrequencyResponseComponent::isShowCrosshair() const
{
    if (auto* p = parameters.getRawParameterValue ("EQ_SHOW_CROSSHAIR_ID"))
        return p->load() >= 0.5f;

    return true;
}

bool FrequencyResponseComponent::bandGainIsBoost (const char* gainParamId, const char* typeParamId) const
{
    if (typeParamId != nullptr)
    {
        if (auto* typeRaw = parameters.getRawParameterValue (typeParamId))
        {
            const int type = juce::roundToInt (typeRaw->load());
            if (! FilterType::usesGain (type))
                return false; // notch / band-pass are subtractive
        }
    }

    if (gainParamId == nullptr)
        return false; // HP/LP and unknown → cut-style mono fill

    if (auto* gainRaw = parameters.getRawParameterValue (gainParamId))
        return gainRaw->load() >= 0.0f;

    return true;
}

juce::Colour FrequencyResponseComponent::resolveBandFillColour (juce::Colour multicolorFill, bool isBoostOrPass) const
{
    if (isMulticolorBandFill())
        return multicolorFill;

    // Mono mode: darker / more neutral golden yellow (sum-curve family).
    // Boosts are stronger; cuts stay yellowish but less intense. Semi-transparent
    // so overlapping fills stack (additive-looking) like the multicolor mode.
    if (isBoostOrPass)
        return juce::Colours::darkgoldenrod.darker (0.35f).withAlpha (0.34f);

    return juce::Colours::darkgoldenrod.darker (0.60f).withAlpha (0.18f);
}

void FrequencyResponseComponent::syncEqRangeControls()
{
    const int range = juce::roundToInt (getEqDisplayRangeDb());
    // Keep a stable "Range" caption; current +/-dB is on the graph axis and in the tip.
    eqRangeLabel.setText ("Range", juce::dontSendNotification);
    const juce::String tip ("EQ display range - currently +/-" + juce::String (range)
                            + " dB. Use - / + for +/-6, +/-12, or +/-24.");
    eqRangeMinusButton.setTooltip (tip);
    eqRangePlusButton.setTooltip (tip);
    eqRangeLabel.setTooltip (tip);

    const int index = (range <= 6) ? 0 : (range <= 12) ? 1 : 2;
    eqRangeMinusButton.setEnabled (index > 0);
    eqRangePlusButton.setEnabled (index < 2);
}

void FrequencyResponseComponent::adjustEqDisplayRange (int delta)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("EQ_DISPLAY_RANGE_ID"));
    if (choice == nullptr)
        return;

    const int newIndex = juce::jlimit (0, 2, choice->getIndex() + delta);
    if (newIndex == choice->getIndex())
        return;

    processor.getUndoManager().beginNewTransaction ("EQ display range");
    choice->beginChangeGesture();
    choice->setValueNotifyingHost (choice->convertTo0to1 (static_cast<float> (newIndex)));
    choice->endChangeGesture();
}

void FrequencyResponseComponent::ensureResponseBufferSize (int width)
{
    auto resizeIfNeeded = [width] (std::vector<float>& v)
    {
        if ((int) v.size() != width)
            v.assign ((size_t) width, 0.0f);
    };

    resizeIfNeeded (responseBand1);
    resizeIfNeeded (responseBand2);
    resizeIfNeeded (responseBand3);
    resizeIfNeeded (responseBand4);
    resizeIfNeeded (responseHighpass);
    resizeIfNeeded (responseLowpass);
    resizeIfNeeded (responseHighShelf);
    resizeIfNeeded (responseLowShelf);
    resizeIfNeeded (responseCombined);
}

void FrequencyResponseComponent::fillMagnitudeResponse (const juce::dsp::IIR::Coefficients<float>::Ptr& coeffs,
                                                        const std::vector<float>& frequencies,
                                                        double sr,
                                                        std::vector<float>& outDb,
                                                        int step)
{
    const int w = (int) outDb.size();
    if (coeffs == nullptr || w == 0 || frequencies.size() != (size_t) w)
        return;

    const int stride = juce::jmax (1, step);

    auto evalDb = [&] (int i) -> float
    {
        const float mag = (float) coeffs->getMagnitudeForFrequency ((double) frequencies[(size_t) i], sr);
        return juce::Decibels::gainToDecibels (mag, -100.0f);
    };

    for (int i = 0; i < w; i += stride)
        outDb[(size_t) i] = evalDb (i);

    if ((w - 1) % stride != 0)
        outDb[(size_t) (w - 1)] = evalDb (w - 1);

    // Linear interpolate between evaluated samples (never hold-fill — that creates stair-steps).
    if (stride > 1)
    {
        for (int i = 0; i + stride < w; i += stride)
        {
            const float a = outDb[(size_t) i];
            const float b = outDb[(size_t) (i + stride)];
            for (int j = 1; j < stride; ++j)
            {
                const float t = (float) j / (float) stride;
                outDb[(size_t) (i + j)] = a + (b - a) * t;
            }
        }

        const int lastFull = ((w - 1) / stride) * stride;
        if (lastFull < w - 1)
        {
            const float a = outDb[(size_t) lastFull];
            const float b = outDb[(size_t) (w - 1)];
            const int span = (w - 1) - lastFull;
            for (int j = 1; j < span; ++j)
            {
                const float t = (float) j / (float) span;
                outDb[(size_t) (lastFull + j)] = a + (b - a) * t;
            }
        }
    }
}

void FrequencyResponseComponent::rebuildMagnitudeResponsesIfNeeded (int width)
{
    if (width <= 1)
        return;

    auto raw = [this] (const char* id) -> float
    {
        if (auto* v = parameters.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    };

    // When dynamic EQ / sidechain is on, curve uses published effective gains (timer marks bands dirty each tick).
    const bool dyn1 = raw ("band1Dynamic") > 0.5f || raw ("band1Sidechain") > 0.5f;
    const bool dyn2 = raw ("band2Dynamic") > 0.5f || raw ("band2Sidechain") > 0.5f;
    const bool dyn3 = raw ("band3Dynamic") > 0.5f || raw ("band3Sidechain") > 0.5f;
    const bool dyn4 = raw ("band4Dynamic") > 0.5f || raw ("band4Sidechain") > 0.5f;
    const bool dynHS = raw ("highShelfDynamic") > 0.5f || raw ("highShelfSidechain") > 0.5f;
    const bool dynLS = raw ("lowShelfDynamic") > 0.5f || raw ("lowShelfSidechain") > 0.5f;
    const bool spec1 = raw ("band1Spectral") > 0.5f;
    const bool spec2 = raw ("band2Spectral") > 0.5f;
    const bool spec3 = raw ("band3Spectral") > 0.5f;
    const bool spec4 = raw ("band4Spectral") > 0.5f;
    const bool specHP = raw ("highpassSpectral") > 0.5f;
    const bool specLP = raw ("lowpassSpectral") > 0.5f;
    const bool specHS = raw ("highShelfSpectral") > 0.5f;
    const bool specLS = raw ("lowShelfSpectral") > 0.5f;

    const float eg1 = processor.getBand1EffectiveGainDb();
    const float eg2 = processor.getBand2EffectiveGainDb();
    const float eg3 = processor.getBand3EffectiveGainDb();
    const float eg4 = processor.getBand4EffectiveGainDb();
    const float egHS = processor.getHighShelfEffectiveGainDb();
    const float egLS = processor.getLowShelfEffectiveGainDb();

    // Secondary catch for paints that weren't kicked by the dyn timer (e.g. knob edits).
    // While D is active the timer already forces dirty every tick — no threshold there.
    constexpr float dynCurveEps = 0.05f;
    auto maybeDirtyDyn = [&] (bool dynOn, float effectiveGain, float staticGain, float& lastGain, bool& dirtyFlag)
    {
        const float want = dynOn ? effectiveGain : staticGain;
        if (std::abs (want - lastGain) > dynCurveEps)
        {
            dirtyFlag = true;
            needsUpdateCombined = true;
        }
    };

    maybeDirtyDyn (dyn1, eg1, raw ("band1Gain"), lastDynCurveGain1, needsUpdateBand1);
    maybeDirtyDyn (dyn2, eg2, raw ("band2Gain"), lastDynCurveGain2, needsUpdateBand2);
    maybeDirtyDyn (dyn3, eg3, raw ("band3Gain"), lastDynCurveGain3, needsUpdateBand3);
    maybeDirtyDyn (dyn4, eg4, raw ("band4Gain"), lastDynCurveGain4, needsUpdateBand4);
    maybeDirtyDyn (dynHS, egHS, raw ("highShelfGain"), lastDynCurveGainHS, needsUpdateHighShelf);
    maybeDirtyDyn (dynLS, egLS, raw ("lowShelfGain"), lastDynCurveGainLS, needsUpdateLowShelf);

    const bool anyDirty = needsUpdateBand1 || needsUpdateBand2 || needsUpdateBand3 || needsUpdateBand4
                          || needsUpdateHighpass || needsUpdateLowpass || needsUpdateHighShelf || needsUpdateLowShelf
                          || needsUpdateCombined;

    if (! anyDirty)
        return;

    ensureResponseBufferSize (width);

    if ((int) logFrequencies.size() != width)
        precomputeLogFrequencies();

    sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : sampleRate;

    auto clampFreq = [] (float f) { return juce::jlimit (20.0f, 20000.0f, f); };
    auto clampQ = [] (float q) { return juce::jmax (0.05f, q); };
    const bool proportionalQOn = raw ("PROPORTIONAL_Q_ID") > 0.5f;

    // Live curve: D / SC use published effective gain; LFO uses published post-mod freq/Q/gain.
    // Effective gain is always published each block (static = knob, including LFO on gain).
    auto displayGain = [&] (bool liveGain, float staticGain, float effectiveGain) -> float
    {
        return liveGain ? effectiveGain : staticGain;
    };

    // Any LFO routed → always prefer published F/G/Q so modulation is visible on curves.
    const bool anyLfoRouted = LfoMod::anyActiveRouting (parameters);

    const bool live1 = dyn1 || anyLfoRouted;
    const bool live2 = dyn2 || anyLfoRouted;
    const bool live3 = dyn3 || anyLfoRouted;
    const bool live4 = dyn4 || anyLfoRouted;
    const bool liveHS = dynHS || anyLfoRouted;
    const bool liveLS = dynLS || anyLfoRouted;

    // Display pipeline (DSP untouched):
    //   1) Dense log-f grid (logFrequencies — one sample per pixel)
    //   2) Static IIR magnitudes on that grid → sum into responseCombined
    //   3) Spectral GR evaluated smoothly on the SAME grid, then added
    //   4) Side Check GR on the SAME grid, then added (sum only)
    //   5) Path build/stroke from responseCombined (in paint)
    // Per-band curves stay static IIR only (band gain is makeup when spectral is on).
    auto applySpectralGrToCombined = [&] (int bandIndex, bool spectralOn)
    {
        if (! spectralOn || (int) responseCombined.size() != width || (int) logFrequencies.size() != width)
            return;

        if ((int) spectralGrScratch.size() != width)
            spectralGrScratch.resize ((size_t) width);

        // Dense grid first, then GR — never apply sparse BP-centre GR after path resampling.
        processor.sampleSpectralGrDb (bandIndex, logFrequencies.data(), spectralGrScratch.data(), width);
        for (int i = 0; i < width; ++i)
            responseCombined[(size_t) i] += spectralGrScratch[(size_t) i];
    };

    auto applySideCheckGrToCombined = [&] (bool sideCheckOn)
    {
        if (! sideCheckOn || (int) responseCombined.size() != width || (int) logFrequencies.size() != width)
            return;

        if ((int) sideCheckGrScratch.size() != width)
            sideCheckGrScratch.resize ((size_t) width);

        processor.sampleSideCheckGrDb (logFrequencies.data(), sideCheckGrScratch.data(), width);
        for (int i = 0; i < width; ++i)
            responseCombined[(size_t) i] += sideCheckGrScratch[(size_t) i];
    };

    // Rebuild only dirty bands. needsUpdateCombined alone (e.g. band on/off / spectral GR)
    // just re-sums the cached per-band magnitude buffers below.
    auto fillBandResponse = [&] (int type, float freq, float qBase, float gain,
                                 const juce::String& slopeId, std::vector<float>& dest)
    {
        const float q = FilterType::effectiveBellQ (type, clampQ (qBase), gain, proportionalQOn);
        const float f = clampFreq (freq);
        if (FilterType::isHpLp (type))
        {
            const int slope = BandChannel::readChoiceIndex (parameters, slopeId);
            auto stages = (type == FilterType::highpass)
                ? FilterSlope::makeHighpassCoeffs (sampleRate, f, q, slope)
                : FilterSlope::makeLowpassCoeffs (sampleRate, f, q, slope);
            FilterSlope::fillCascadedMagnitude (stages, logFrequencies, sampleRate, dest, responseSampleStep);
        }
        else
        {
            auto coeffs = FilterType::makeCoefficients (type, sampleRate, f, q, gain);
            fillMagnitudeResponse (coeffs, logFrequencies, sampleRate, dest, responseSampleStep);
        }
    };

    if (needsUpdateBand1)
    {
        const int type = (int) raw ("band1Type");
        const float gain = displayGain (live1 && FilterType::usesGain (type), raw ("band1Gain"), eg1);
        const float freq = live1 ? processor.getPublishedBand1Freq() : raw ("band1Frequency");
        const float qBase = live1 ? processor.getPublishedBand1Q() : raw ("band1Q");
        fillBandResponse (type, freq, qBase, gain, "band1Slope", responseBand1);
        lastDynCurveGain1 = gain;
    }

    if (needsUpdateBand2)
    {
        const int type = (int) raw ("band2Type");
        const float gain = displayGain (live2 && FilterType::usesGain (type), raw ("band2Gain"), eg2);
        const float freq = live2 ? processor.getPublishedBand2Freq() : raw ("band2Frequency");
        const float qBase = live2 ? processor.getPublishedBand2Q() : raw ("band2Q");
        fillBandResponse (type, freq, qBase, gain, "band2Slope", responseBand2);
        lastDynCurveGain2 = gain;
    }

    if (needsUpdateBand3)
    {
        const int type = (int) raw ("band3Type");
        const float gain = displayGain (live3 && FilterType::usesGain (type), raw ("band3Gain"), eg3);
        const float freq = live3 ? processor.getPublishedBand3Freq() : raw ("band3Frequency");
        const float qBase = live3 ? processor.getPublishedBand3Q() : raw ("band3Q");
        fillBandResponse (type, freq, qBase, gain, "band3Slope", responseBand3);
        lastDynCurveGain3 = gain;
    }

    if (needsUpdateBand4)
    {
        const int type = (int) raw ("band4Type");
        const float gain = displayGain (live4 && FilterType::usesGain (type), raw ("band4Gain"), eg4);
        const float freq = live4 ? processor.getPublishedBand4Freq() : raw ("band4Frequency");
        const float qBase = live4 ? processor.getPublishedBand4Q() : raw ("band4Q");
        fillBandResponse (type, freq, qBase, gain, "band4Slope", responseBand4);
        lastDynCurveGain4 = gain;
    }

    if (needsUpdateHighpass)
    {
        const int type = BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass);
        const float freq = clampFreq (raw ("highpassCutoff"));
        const float q = clampQ (raw ("highpassQ"));
        const float gain = FilterType::usesGain (type) ? raw ("highpassGain") : 0.0f;
        fillBandResponse (type, freq, q, gain, "highpassSlope", responseHighpass);
    }

    if (needsUpdateLowpass)
    {
        const int type = BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass);
        const float freq = clampFreq (raw ("lowpassCutoff"));
        const float q = clampQ (raw ("lowpassQ"));
        const float gain = FilterType::usesGain (type) ? raw ("lowpassGain") : 0.0f;
        fillBandResponse (type, freq, q, gain, "lowpassSlope", responseLowpass);
    }

    if (needsUpdateHighShelf)
    {
        const int type = (int) raw ("highShelfType");
        const float gain = displayGain (liveHS && FilterType::usesGain (type), raw ("highShelfGain"), egHS);
        const float freq = liveHS ? processor.getPublishedHighShelfFreq() : raw ("highShelfFrequency");
        const float q = clampQ (liveHS ? processor.getPublishedHighShelfQ() : raw ("highShelfQ"));
        fillBandResponse (type, freq, q, gain, "highShelfSlope", responseHighShelf);
        lastDynCurveGainHS = gain;
    }

    if (needsUpdateLowShelf)
    {
        const int type = (int) raw ("lowShelfType");
        const float gain = displayGain (liveLS && FilterType::usesGain (type), raw ("lowShelfGain"), egLS);
        const float freq = liveLS ? processor.getPublishedLowShelfFreq() : raw ("lowShelfFrequency");
        const float q = clampQ (liveLS ? processor.getPublishedLowShelfQ() : raw ("lowShelfQ"));
        fillBandResponse (type, freq, q, gain, "lowShelfSlope", responseLowShelf);
        lastDynCurveGainLS = gain;
    }

    for (int i = 0; i < width; ++i)
    {
        float sum = 0.0f;
        if (processor.getIsBand1On())     sum += responseBand1[(size_t) i];
        if (processor.getIsBand2On())     sum += responseBand2[(size_t) i];
        if (processor.getIsBand3On())     sum += responseBand3[(size_t) i];
        if (processor.getIsBand4On())     sum += responseBand4[(size_t) i];
        if (processor.getIsHighpassOn())  sum += responseHighpass[(size_t) i];
        if (processor.getIsLowpassOn())   sum += responseLowpass[(size_t) i];
        if (processor.getIsHighShelfOn()) sum += responseHighShelf[(size_t) i];
        if (processor.getIsLowShelfOn())  sum += responseLowShelf[(size_t) i];
        responseCombined[(size_t) i] = sum;
    }

    // Spectral processing shapes only the cumulative display curve.
    // usesGain + current type must match DSP arming (S only on bell / shelves).
    if (processor.getIsBand1On())
        applySpectralGrToCombined (0, spec1 && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "band1Type", FilterType::bell)));
    if (processor.getIsBand2On())
        applySpectralGrToCombined (1, spec2 && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "band2Type", FilterType::bell)));
    if (processor.getIsBand3On())
        applySpectralGrToCombined (2, spec3 && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "band3Type", FilterType::bell)));
    if (processor.getIsBand4On())
        applySpectralGrToCombined (3, spec4 && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "band4Type", FilterType::bell)));
    if (processor.getIsHighpassOn())
        applySpectralGrToCombined (4, specHP && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass)));
    if (processor.getIsLowpassOn())
        applySpectralGrToCombined (5, specLP && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass)));
    if (processor.getIsHighShelfOn())
        applySpectralGrToCombined (6, specHS && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "highShelfType", FilterType::highShelf)));
    if (processor.getIsLowShelfOn())
        applySpectralGrToCombined (7, specLS && FilterType::usesGain (
            BandChannel::readChoiceIndex (parameters, "lowShelfType", FilterType::lowShelf)));

    // Side Check GR after spectral — sum curve only (per-band curves stay static).
    applySideCheckGrToCombined (raw (SideCheck::enabledParamId()) > 0.5f);
}

//=======================================================================================================//
void FrequencyResponseComponent::paint(juce::Graphics& g)
{

    // Get the current component bounds
    juce::Rectangle<int> componentBounds = getLocalBounds();

    // Inset top/right/bottom by 10px; leave the left edge open so dB scale
    // labels at ~5px padding are not clipped.
    componentBounds.removeFromTop (10);
    componentBounds.removeFromBottom (10);
    componentBounds.removeFromRight (10);

    // Set the graphics clip region to the adjusted bounds
    g.reduceClipRegion(componentBounds);

    g.fillAll(juce::Colour(0.0f, 0.0f, 0.0f, 0.0f));

    auto area = getLocalBounds();
    auto w = area.getWidth();
    auto h = area.getHeight();

    // Define your custom colors for the gradient
    juce::Colour color1 = juce::Colour(10, 10, 10); // Custom color 1 (e.g., red)
    juce::Colour color2 = juce::Colour(60, 55, 50); // Custom color 2 (e.g., blue)

    // Create a horizontal linear gradient between two X coordinates
    juce::ColourGradient gradient = juce::ColourGradient::horizontal(color1, 0.0f, color2, static_cast<float>(getWidth()));


    // Fill the component with the gradient
    g.setGradientFill(gradient);
    // g.fillAll();

    rebuildMagnitudeResponsesIfNeeded (w);

    // Local aliases so the rest of paint can keep using the old names
    auto& compositeResponse = responseCombined;
    auto& compositeResponse2 = responseBand1;
    auto& compositeResponse3 = responseBand2;
    auto& compositeResponse4 = responseBand3;
    auto& compositeResponse5 = responseBand4;
    auto& compositeResponse6 = responseHighpass;
    auto& compositeResponse7 = responseLowpass;
    auto& compositeResponse8 = responseHighShelf;
    auto& compositeResponse9 = responseLowShelf;

    //=======================================================================================================//


    // Grid labels in db and hz
    // Constants for logarithmic mapping
    double logMin = std::log10(20);  // 20 Hz
    double logMax = std::log10(20000);  // 20000 Hz

    // Frequencies where you want to place vertical grid lines
    std::vector<float> gridFrequencies = { 50, 100, 200, 400, 800, 1600, 3200, 6400, 12800 };

    // Colors for grid lines
    juce::Colour standardGridLineColor = juce::Colours::whitesmoke.withAlpha(0.1f); // Standard lines
    juce::Colour specialGridLineColor = juce::Colours::whitesmoke.withAlpha(0.2f);  // Special lines

    // Draw vertical grid lines and add labels
    g.setColour(specialGridLineColor);
    juce::Font labelFont(12.0f); // Define the font for labels

    for (float freq : gridFrequencies)
    {
        float gridLineX = (getWidth() - 1) * (std::log10(freq) - logMin) / (logMax - logMin);
        //g.drawLine(gridLineX, 0, gridLineX, getHeight(), 1.0f);

        // Convert frequency to a label (e.g., 50 Hz)
        juce::String label = juce::String(freq) + " Hz";

        // Calculate the position to place the label
        float labelX = gridLineX + 5; // Adjust as needed
        float labelY = getHeight() - 10; // Adjust as needed

        // Set the font and color for the label
        g.setFont(labelFont);
        g.setColour(juce::Colours::darkgrey);

        // Draw the label
        //g.drawText(label, labelX, labelY, 50, 10, juce::Justification::left);
    }

    // Draw horizontal grid lines and dB labels for the current EQ display range.
    // Labels sit at the left edge of the window (~5px padding).
    {
        const int rangeInt = juce::roundToInt (getEqDisplayRangeDb());
        const int step = (rangeInt <= 6) ? 1 : (rangeInt <= 12) ? 2 : 3;

        std::vector<int> specialDbLevels;
        for (int major : { 6, 12, 18, 24 })
            if (major <= rangeInt)
                specialDbLevels.push_back (major);

        constexpr float labelLeft = 5.0f;
        const float labelW = 48.0f;

        g.setFont (labelFont);

        for (int db = -rangeInt; db <= rangeInt; db += step)
        {
            const float y = dbToY (static_cast<float> (db), static_cast<float> (getHeight()));
            const bool isSpecial = (db == 0)
                || std::find (specialDbLevels.begin(), specialDbLevels.end(), std::abs (db)) != specialDbLevels.end();

            g.setColour (isSpecial ? specialGridLineColor : standardGridLineColor);
            //g.drawLine(0, y, getWidth(), y, 1.0f);

            const float labelY = juce::jlimit (0.0f, static_cast<float> (getHeight()) - 14.0f, y - 7.0f);
            g.setColour (juce::Colours::whitesmoke.withAlpha (isSpecial ? 0.82f : 0.58f));
            g.drawText (juce::String (db) + " dB",
                        labelLeft,
                        labelY,
                        labelW,
                        14.0f,
                        juce::Justification::centredLeft,
                        false);
        }
    }





    //=======================================================================================================// 
    // Draw the band1 response

    float band1OnOffParam = processor.treeState.getParameter("band1OnOff")->getValue();
    float band2OnOffParam = processor.treeState.getParameter("band2OnOff")->getValue();
    float band3OnOffParam = processor.treeState.getParameter("band3OnOff")->getValue();
    float band4OnOffParam = processor.treeState.getParameter("band4OnOff")->getValue();
    float highpassOnOffParam = processor.treeState.getParameter("highpassOnOff")->getValue();
    float lowpassOnOffParam = processor.treeState.getParameter("lowpassOnOff")->getValue();
    float highShelfOnOffParam = processor.treeState.getParameter("highShelfOnOff")->getValue();
    float lowShelfOnOffParam = processor.treeState.getParameter("lowShelfOnOff")->getValue();

    if (needsUpdateBand1)
    {
        band1ResponsePath.clear();
        if (processor.getIsBand1On() && ! compositeResponse2.empty())
            band1ResponsePath = intelligentDownsample(band1ResponsePath, compositeResponse2, w, h);
        needsUpdateBand1 = false;
    }

    if (processor.getIsBand1On())
    {
        // Draw the path (this is fast if the path hasn't changed)
        juce::Colour customBandColor1 = resolveBandFillColour (
            juce::Colours::cornflowerblue.withAlpha (0.4f),
            bandGainIsBoost ("band1Gain", "band1Type"));
        g.setColour(customBandColor1);
        g.fillPath(band1ResponsePath);

        juce::Colour band1CurveColor = juce::Colours::cornflowerblue.withAlpha(0.75f);
        g.setColour(band1CurveColor);
        g.strokePath(band1ResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }

    //=======================================================================================================//
    // Draw the band2 response


    if (needsUpdateBand2)
    {
        band2ResponsePath.clear();
        if (processor.getIsBand2On() && ! compositeResponse3.empty())
            band2ResponsePath = intelligentDownsample(band2ResponsePath, compositeResponse3, w, h);
        needsUpdateBand2 = false;
    }

    if (processor.getIsBand2On())
    {
        juce::Colour customBandColor2 = resolveBandFillColour (
            juce::Colours::purple.withAlpha (0.4f),
            bandGainIsBoost ("band2Gain", "band2Type"));
        g.setColour(customBandColor2);
        g.fillPath(band2ResponsePath);

        juce::Colour band2CurveColor = juce::Colours::purple.withAlpha(0.75f);
        g.setColour(band2CurveColor);
        g.strokePath(band2ResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }


    //=======================================================================================================//
    // Draw the band3 response

    if (needsUpdateBand3)
    {
        band3ResponsePath.clear();
        if (processor.getIsBand3On() && ! compositeResponse4.empty())
            band3ResponsePath = intelligentDownsample(band3ResponsePath, compositeResponse4, w, h);
        needsUpdateBand3 = false;
    }

    if (processor.getIsBand3On())
    {
        juce::Colour customBandColor3 = resolveBandFillColour (
            juce::Colours::cyan.withAlpha (0.4f),
            bandGainIsBoost ("band3Gain", "band3Type"));
        g.setColour(customBandColor3);
        g.fillPath(band3ResponsePath);

        juce::Colour band3CurveColor = juce::Colours::cyan.withAlpha(0.75f);
        g.setColour(band3CurveColor);
        g.strokePath(band3ResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }

    //=======================================================================================================//
    // Draw the band4 response

    if (needsUpdateBand4)
    {
        band4ResponsePath.clear();
        if (processor.getIsBand4On() && ! compositeResponse5.empty())
            band4ResponsePath = intelligentDownsample(band4ResponsePath, compositeResponse5, w, h);
        needsUpdateBand4 = false;
    }

    if (processor.getIsBand4On())
    {
        juce::Colour customBandColor4 = resolveBandFillColour (
            juce::Colours::blue.withAlpha (0.35f),
            bandGainIsBoost ("band4Gain", "band4Type"));
        g.setColour(customBandColor4);
        g.fillPath(band4ResponsePath);

        juce::Colour band4CurveColor = juce::Colours::blue.withAlpha(0.75f);
        g.setColour(band4CurveColor);
        g.strokePath(band4ResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }


    //=======================================================================================================//
    // Draw the highpass response

    if (needsUpdateHighpass)
    {
        highpassResponsePath.clear();
        if (processor.getIsHighpassOn() && ! compositeResponse6.empty())
        {
            // Corner-tether paths are only for real HP/LP slopes; bells/shelves use the
            // same mid-line close as the other bands (avoids Q→bottom corner glitches).
            const int hpType = BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass);
            if (hpType == FilterType::highpass)
                highpassResponsePath = intelligentDownsampleHighpass (highpassResponsePath, compositeResponse6, w, h);
            else if (hpType == FilterType::lowpass)
                highpassResponsePath = intelligentDownsampleLowpass (highpassResponsePath, compositeResponse6, w, h);
            else
                highpassResponsePath = intelligentDownsample (highpassResponsePath, compositeResponse6, w, h);
        }
        needsUpdateHighpass = false;
    }

    if (processor.getIsHighpassOn())
    {
        juce::Colour customHighpassColor = resolveBandFillColour (
            juce::Colours::green.withAlpha (0.35f),
            bandGainIsBoost ("highpassGain", "highpassType"));
        g.setColour(customHighpassColor);
        g.fillPath(highpassResponsePath);

        juce::Colour highpassCurveColor = juce::Colours::green.withAlpha(0.75f);
        g.setColour(highpassCurveColor);
        g.strokePath(highpassResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }


    //=======================================================================================================//
    // Draw the lowpass response

    if (needsUpdateLowpass)
    {
        lowpassResponsePath.clear();
        if (processor.getIsLowpassOn() && ! compositeResponse7.empty())
        {
            const int lpType = BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass);
            if (lpType == FilterType::lowpass)
                lowpassResponsePath = intelligentDownsampleLowpass (lowpassResponsePath, compositeResponse7, w, h);
            else if (lpType == FilterType::highpass)
                lowpassResponsePath = intelligentDownsampleHighpass (lowpassResponsePath, compositeResponse7, w, h);
            else
                lowpassResponsePath = intelligentDownsample (lowpassResponsePath, compositeResponse7, w, h);
        }
        needsUpdateLowpass = false;
    }

    if (processor.getIsLowpassOn())
    {
        juce::Colour customLowpassColor = resolveBandFillColour (
            juce::Colours::red.withAlpha (0.45f),
            bandGainIsBoost ("lowpassGain", "lowpassType"));
        g.setColour(customLowpassColor);
        g.fillPath(lowpassResponsePath);

        juce::Colour lowpassCurveColor = juce::Colours::red.withAlpha(0.75f);
        g.setColour(lowpassCurveColor);
        g.strokePath(lowpassResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }


    //=======================================================================================================//
    // Draw the highShelf response


    if (needsUpdateHighShelf)
    {
        highShelfResponsePath.clear();
        if (processor.getIsHighShelfOn() && ! compositeResponse8.empty())
            highShelfResponsePath = simpleDownsample(highShelfResponsePath, compositeResponse8, w, h, 8);
        needsUpdateHighShelf = false;
    }

    if (processor.getIsHighShelfOn())
    {
        juce::Colour customHighShelfColor = resolveBandFillColour (
            juce::Colours::burlywood.withAlpha (0.5f),
            bandGainIsBoost ("highShelfGain", "highShelfType"));
        g.setColour(customHighShelfColor);
        g.fillPath(highShelfResponsePath);

        juce::Colour highShelfCurveColor = juce::Colours::burlywood.withAlpha(0.75f);
        g.setColour(highShelfCurveColor);
        g.strokePath(highShelfResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }


    //======================================================================================================//
    // Draw the lowShelf response
  
    if (needsUpdateLowShelf)
    {
        lowShelfResponsePath.clear();
        if (processor.getIsLowShelfOn() && ! compositeResponse9.empty())
            lowShelfResponsePath = simpleDownsample(lowShelfResponsePath, compositeResponse9, w, h, 8);
        needsUpdateLowShelf = false;
    }

    if (processor.getIsLowShelfOn())
    {
        juce::Colour customLowShelfColor = resolveBandFillColour (
            juce::Colours::indianred.withAlpha (0.5f),
            bandGainIsBoost ("lowShelfGain", "lowShelfType"));
        g.setColour(customLowShelfColor);
        g.fillPath(lowShelfResponsePath);

        juce::Colour lowShelfCurveColor = juce::Colours::indianred.withAlpha(0.75f);
        g.setColour(lowShelfCurveColor);
        g.strokePath(lowShelfResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }



    //=======================================================================================================//
   // Draw the combined frequency response



      // Update combinedResponsePath only if it's marked as dirty
    if (needsUpdateCombined) {

        //  DBG("needsUpdateCombined True");

          // Clear the existing path
        combinedResponsePath.clear();

        // Create the new path using intelligent downsampling
        if (! compositeResponse.empty())
            combinedResponsePath = intelligentDownsampleToBottom(combinedResponsePath, compositeResponse, w, h);
        needsUpdateCombined = false;

    }

    // Draw the path (this is fast if the path hasn't changed)
    // Create a gradient from the top to the bottom
    juce::Colour topColor = juce::Colour::fromRGBA(255, 130, 30, 180);
    juce::Colour bottomColor = juce::Colours::darkgoldenrod.darker(0.7f).withAlpha(0.4f);
    juce::ColourGradient gradient12(topColor, 0, 0, bottomColor, 0, h, false);

    // Fill and stroke the path with the gradient
    g.setGradientFill(gradient12);
    g.fillPath(combinedResponsePath);

    juce::Colour combinedCurveColor = juce::Colours::goldenrod.withAlpha(0.8f).darker(0.0f);
    // Light corner rounding + curved/rounded stroke matches band AA look on dense polylines.
    const auto combinedStrokePath = combinedResponsePath.createPathWithRoundedCorners (4.0f);
    const float sumWidth = getSumPathWidth();
    const auto sumStroke = juce::PathStrokeType (sumWidth,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded);

    // Optional Melatonin glow for the cumulative sum curve (off by default — Post Glow is the main one).
    {
        bool glowEnabled = false;
        if (auto* p = parameters.getRawParameterValue ("EQ_SUM_GLOW_ENABLE_ID"))
            glowEnabled = p->load() > 0.5f;

        if (glowEnabled)
        {
            float glowOpacity = 70.0f;
            float glowRadius = 14.0f;
            float glowSpread = 2.0f;
            if (auto* p = parameters.getRawParameterValue ("EQ_SUM_GLOW_OPACITY_ID"))
                glowOpacity = p->load();
            if (auto* p = parameters.getRawParameterValue ("EQ_SUM_GLOW_RADIUS_ID"))
                glowRadius = p->load();
            if (auto* p = parameters.getRawParameterValue ("EQ_SUM_GLOW_SPREAD_ID"))
                glowSpread = p->load();

            const float glowAlpha = juce::jlimit (0.0f, 1.0f, glowOpacity * 0.01f);
            if (glowAlpha > 0.05f && glowRadius > 0.5f)
            {
                const auto bloom = juce::Colour::fromRGBA (255, 180, 60, 130).withAlpha (glowAlpha * 0.45f);
                const auto core = juce::Colour::fromRGBA (255, 220, 120, 180).withAlpha (glowAlpha * 0.75f);

                sumCurveGlow.setRadius ((double) juce::jlimit (0.0f, 80.0f, glowRadius), 0);
                sumCurveGlow.setSpread ((double) juce::jlimit (0.0f, 40.0f, glowSpread), 0);
                sumCurveGlow.setOffset (0, 0, 0);
                sumCurveGlow.setColor (bloom, 0);

                sumCurveGlow.setRadius (juce::jmax (2.0, (double) glowRadius * 0.35), 1);
                sumCurveGlow.setSpread (0.0, 1);
                sumCurveGlow.setOffset (0, 0, 1);
                sumCurveGlow.setColor (core, 1);

                sumCurveGlow.render (g, combinedStrokePath,
                                     juce::PathStrokeType (juce::jmax (1.0f, sumWidth + 0.5f),
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded),
                                     true);
            }
        }
    }

    g.setColour(combinedCurveColor);
    g.strokePath (combinedStrokePath, sumStroke);



    //=======================================================================================================//
    juce::Colour outlineColor = juce::Colour::fromRGB(1, 1, 1);  // Set your custom color here
    juce::Colour outlineColor2 = juce::Colour::fromRGB(211, 167, 143);  // Set your custom color here
    juce::Colour handleColor1 = juce::Colours::cornflowerblue.withAlpha(1.0f);
    juce::Colour handleColor2 = juce::Colours::purple.withAlpha(1.0f);
    juce::Colour handleColor3 = juce::Colours::cyan.withAlpha(0.7f);
    juce::Colour handleColor4 = juce::Colours::mediumblue.withAlpha(0.9f);
   
    juce::Colour handleColor5 = juce::Colours::green.withAlpha(1.0f);
    juce::Colour handleColor6 = juce::Colours::red.withAlpha(1.0f);
  
    juce::Colour handleColor7 = juce::Colours::darkgoldenrod.withAlpha(1.0f);
    juce::Colour handleColor8 = juce::Colours::burlywood.withAlpha(1.0f);

    float outlineThickness2 = 1.0f;  // Half of prior 2.0f (handles at 1/2 size)
    float outlineThickness = 1.0f;  // Half of prior 2.0f (handles at 1/2 size)




    // Band 1 //



    if (processor.getIsBand1On())
    {
        // Unified scale factor
        float scaleFactor = (isMouseHoveringOverHandle1 ? 1.25f : 1.0f)
                            * processor.getModHandlePulseScale (0);

        // Get the values from the tree state
        float band1Frequency = processor.treeState.getRawParameterValue("band1Frequency")->load();
        const int band1Type = (int) processor.treeState.getRawParameterValue ("band1Type")->load();
        float band1Gain = FilterType::usesGain (band1Type)
                            ? processor.treeState.getRawParameterValue("band1Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band1X = (getWidth() - 1) * (std::log10(band1Frequency) - logMin) / (logMax - logMin);
        float band1Y = dbToY(band1Gain, static_cast<float>(getHeight()));

        // Apply scale factor to handle and outlines
        float handleSize = 12.0f * scaleFactor;

        // Limit handle position by the edge of the window, taking full diameter into account
        band1X = std::min(std::max(band1X, handleSize), static_cast<float>(getWidth()) - handleSize);
        band1Y = std::min(std::max(band1Y, handleSize), static_cast<float>(getHeight()) - handleSize);

        // Update handle positions to reflect these constraints
        handleX = band1X;
        handleY = band1Y;

        // More scale factor applications
        float handleOutlineSize = 12.5f * scaleFactor;
        float innerOutlineSize = 11.0f * scaleFactor;

        // Draw the handle with your existing color
        g.setColour(handleColor1);
        g.fillEllipse(band1X - handleSize / 2.0f, band1Y - handleSize / 2.0f, handleSize, handleSize);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(band1X - handleOutlineSize / 2.0f, band1Y - handleOutlineSize / 2.0f, handleOutlineSize, handleOutlineSize, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(band1X - innerOutlineSize / 2.0f, band1Y - innerOutlineSize / 2.0f, innerOutlineSize, innerOutlineSize, outlineThickness2);

        // Draw the label for Band 1
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor);  // Apply scale f   actor to font size
        float textOffset = 3.5f * scaleFactor;  // Apply scale factor to text position
        g.drawText("1", band1X - textOffset, band1Y - textOffset, 7.0f * scaleFactor, 7.0f * scaleFactor, juce::Justification::centred, false);
    }
    else
    {
        handleX = -1000.0f;
        handleY = -1000.0f;
    }


    //=======================================================================================================//
    // Band 2 //


    if (processor.getIsBand2On())
    {
        // Unified scale factor for Band 2
        float scaleFactor2 = (isMouseHoveringOverHandle2 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (1);

        // Get the values from the tree state
        float band2Frequency = processor.treeState.getRawParameterValue("band2Frequency")->load();
        const int band2Type = (int) processor.treeState.getRawParameterValue ("band2Type")->load();
        float band2Gain = FilterType::usesGain (band2Type)
                            ? processor.treeState.getRawParameterValue("band2Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band2X = (getWidth() - 1) * (std::log10(band2Frequency) - logMin) / (logMax - logMin);
        float band2Y = dbToY(band2Gain, static_cast<float>(getHeight()));

        // Apply scale factor to handle and outlines
        float handleSize2 = 12.0f * scaleFactor2;

        // Limit handle position by the edge of the window, taking full diameter into account
        band2X = std::min(std::max(band2X, handleSize2), static_cast<float>(getWidth()) - handleSize2);
        band2Y = std::min(std::max(band2Y, handleSize2), static_cast<float>(getHeight()) - handleSize2);

        // Update handle positions to align with the graphical representation
        handleX2 = band2X;
        handleY2 = band2Y;

        // More graphical attributes
        float handleOutlineSize2 = 12.5f * scaleFactor2;
        float innerOutlineSize2 = 11.0f * scaleFactor2;

        // Draw the handle with your existing color
        g.setColour(handleColor2);
        g.fillEllipse(band2X - handleSize2 / 2.0f, band2Y - handleSize2 / 2.0f, handleSize2, handleSize2);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(band2X - handleOutlineSize2 / 2.0f, band2Y - handleOutlineSize2 / 2.0f, handleOutlineSize2, handleOutlineSize2, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(band2X - innerOutlineSize2 / 2.0f, band2Y - innerOutlineSize2 / 2.0f, innerOutlineSize2, innerOutlineSize2, outlineThickness2);

        // Draw the label for Band 2
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor2);  // Apply scale factor to font size
        float textOffset2 = 3.5f * scaleFactor2;  // Apply scale factor to text position
        g.drawText("2", band2X - textOffset2, band2Y - textOffset2, 7.0f * scaleFactor2, 7.0f * scaleFactor2, juce::Justification::centred, false);
    }
    else
    {
        handleX2 = -1000.0f;
        handleY2 = -1000.0f;
    }


    //=======================================================================================================//
    // Band 3 //


    if (processor.getIsBand3On())
    {
        // Unified scale factor for Band 3
        float scaleFactor3 = (isMouseHoveringOverHandle3 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (2);

        // Get the values from the tree state
        float band3Frequency = processor.treeState.getRawParameterValue("band3Frequency")->load();
        const int band3Type = (int) processor.treeState.getRawParameterValue ("band3Type")->load();
        float band3Gain = FilterType::usesGain (band3Type)
                            ? processor.treeState.getRawParameterValue("band3Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band3X = (getWidth() - 1) * (std::log10(band3Frequency) - logMin) / (logMax - logMin);
        float band3Y = dbToY(band3Gain, static_cast<float>(getHeight()));

        // Apply scale factor to handle and outlines
        float handleSize3 = 12.0f * scaleFactor3;

        // Limit handle position by the edge of the window, taking full diameter into account
        band3X = std::min(std::max(band3X, handleSize3), static_cast<float>(getWidth()) - handleSize3);
        band3Y = std::min(std::max(band3Y, handleSize3), static_cast<float>(getHeight()) - handleSize3);

        // Update handle positions to align with the graphical representation
        handleX3 = band3X;
        handleY3 = band3Y;


        // More graphical attributes
        float handleOutlineSize3 = 12.5f * scaleFactor3;
        float innerOutlineSize3 = 11.0f * scaleFactor3;

        // Draw the handle with your existing color
        g.setColour(handleColor3);
        g.fillEllipse(band3X - handleSize3 / 2.0f, band3Y - handleSize3 / 2.0f, handleSize3, handleSize3);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(band3X - handleOutlineSize3 / 2.0f, band3Y - handleOutlineSize3 / 2.0f, handleOutlineSize3, handleOutlineSize3, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(band3X - innerOutlineSize3 / 2.0f, band3Y - innerOutlineSize3 / 2.0f, innerOutlineSize3, innerOutlineSize3, outlineThickness2);

        // Draw the label for Band 3
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor3);  // Apply scale factor to font size
        float textOffset3 = 3.5f * scaleFactor3;  // Apply scale factor to text position
        g.drawText("3", band3X - textOffset3, band3Y - textOffset3, 7.0f * scaleFactor3, 7.0f * scaleFactor3, juce::Justification::centred, false);
    }
    else
    {
        handleX3 = -1000.0f;
        handleY3 = -1000.0f;
    }

    //=======================================================================================================//
    // Band 4 //


    if (processor.getIsBand4On())
    {
        // Unified scale factor for Band 4
        float scaleFactor4 = (isMouseHoveringOverHandle4 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (3);

        // Get the values from the tree state
        float band4Frequency = processor.treeState.getRawParameterValue("band4Frequency")->load();
        const int band4Type = (int) processor.treeState.getRawParameterValue ("band4Type")->load();
        float band4Gain = FilterType::usesGain (band4Type)
                            ? processor.treeState.getRawParameterValue("band4Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band4X = (getWidth() - 1) * (std::log10(band4Frequency) - logMin) / (logMax - logMin);
        float band4Y = dbToY(band4Gain, static_cast<float>(getHeight()));

        // Apply scale factor to handle and outlines
        float handleSize4 = 12.0f * scaleFactor4;

        // Limit handle position by the edge of the window, taking full diameter into account
        band4X = std::min(std::max(band4X, handleSize4), static_cast<float>(getWidth()) - handleSize4);
        band4Y = std::min(std::max(band4Y, handleSize4), static_cast<float>(getHeight()) - handleSize4);

        // Update handle positions to align with the graphical representation
        handleX4 = band4X;
        handleY4 = band4Y;


        // More graphical attributes
        float handleOutlineSize4 = 12.5f * scaleFactor4;
        float innerOutlineSize4 = 11.0f * scaleFactor4;

        // Draw the handle with your existing color
        g.setColour(handleColor4);
        g.fillEllipse(band4X - handleSize4 / 2.0f, band4Y - handleSize4 / 2.0f, handleSize4, handleSize4);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(band4X - handleOutlineSize4 / 2.0f, band4Y - handleOutlineSize4 / 2.0f, handleOutlineSize4, handleOutlineSize4, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(band4X - innerOutlineSize4 / 2.0f, band4Y - innerOutlineSize4 / 2.0f, innerOutlineSize4, innerOutlineSize4, outlineThickness2);

        // Draw the label for Band 4
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor4);  // Apply scale factor to font size
        float textOffset4 = 3.5f * scaleFactor4;  // Apply scale factor to text position
        g.drawText("4", band4X - textOffset4, band4Y - textOffset4, 7.0f * scaleFactor4, 7.0f * scaleFactor4, juce::Justification::centred, false);
    }
    else
    {
        handleX4 = -1000.0f;
        handleY4 = -1000.0f;
    }


    //=======================================================================================================//
    // Highpass //


    if (processor.getIsHighpassOn())
    {
        // Unified scale factor for Highpass
        float scaleFactor5 = (isMouseHoveringOverHandle5 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (4);

        float highpassCutoff = processor.treeState.getRawParameterValue("highpassCutoff")->load();
        const int hpType = BandChannel::readChoiceIndex (processor.treeState, "highpassType", FilterType::highpass);
        const float hpGain = FilterType::usesGain (hpType)
                               ? processor.treeState.getRawParameterValue ("highpassGain")->load()
                               : 0.0f;

        float highpassX = (getWidth() - 1) * (std::log10(highpassCutoff) - logMin) / (logMax - logMin);
        float highpassY = dbToY (hpGain, static_cast<float> (getHeight()));

        float handleSize5 = 12.0f * scaleFactor5;

        highpassX = std::min(std::max(highpassX, handleSize5), static_cast<float>(getWidth()) - handleSize5);
        highpassY = std::min(std::max(highpassY, handleSize5), static_cast<float>(getHeight()) - handleSize5);

        handleX5 = highpassX;
        handleY5 = highpassY;

        // More graphical attributes
        float handleOutlineSize5 = 12.5f * scaleFactor5;
        float innerOutlineSize5 = 11.0f * scaleFactor5;

        // Draw the handle with your existing color
        g.setColour(handleColor5);
        g.fillEllipse(highpassX - handleSize5 / 2.0f, handleY5 - handleSize5 / 2.0f, handleSize5, handleSize5);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(highpassX - handleOutlineSize5 / 2.0f, handleY5 - handleOutlineSize5 / 2.0f, handleOutlineSize5, handleOutlineSize5, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(highpassX - innerOutlineSize5 / 2.0f, handleY5 - innerOutlineSize5 / 2.0f, innerOutlineSize5, innerOutlineSize5, outlineThickness2);

        // Draw the label for Highpass
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor5);  // Apply scale factor to font size
        float textOffset5 = 3.5f * scaleFactor5;  // Apply scale factor to text position
        const char* hpLabel = (hpType == FilterType::highpass) ? "HP"
                            : (hpType == FilterType::lowpass)  ? "LP" : "1";
        g.drawText(hpLabel, highpassX - textOffset5, handleY5 - textOffset5, 7.0f * scaleFactor5, 7.0f * scaleFactor5, juce::Justification::centred, false);
    }
    else
    {
        handleX5 = -1000.0f;
        handleY5 = -1000.0f;
    }


    //=======================================================================================================//
    // Lowpass //


    if (processor.getIsLowpassOn())
    {
        // Unified scale factor for Lowpass
        float scaleFactor6 = (isMouseHoveringOverHandle6 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (5);

        float lowpassCutoff = processor.treeState.getRawParameterValue("lowpassCutoff")->load();
        const int lpType = BandChannel::readChoiceIndex (processor.treeState, "lowpassType", FilterType::lowpass);
        const float lpGain = FilterType::usesGain (lpType)
                               ? processor.treeState.getRawParameterValue ("lowpassGain")->load()
                               : 0.0f;

        float lowpassX = (getWidth() - 1) * (std::log10(lowpassCutoff) - logMin) / (logMax - logMin);
        float lowpassY = dbToY (lpGain, static_cast<float> (getHeight()));

        float handleSize6 = 12.0f * scaleFactor6;

        lowpassX = std::min(std::max(lowpassX, handleSize6), static_cast<float>(getWidth()) - handleSize6);
        lowpassY = std::min(std::max(lowpassY, handleSize6), static_cast<float>(getHeight()) - handleSize6);

        handleX6 = lowpassX;
        handleY6 = lowpassY;

        // More graphical attributes
        float handleOutlineSize6 = 12.5f * scaleFactor6;
        float innerOutlineSize6 = 11.0f * scaleFactor6;

        // Draw the handle with your existing color
        g.setColour(handleColor6);
        g.fillEllipse(lowpassX - handleSize6 / 2.0f, handleY6 - handleSize6 / 2.0f, handleSize6, handleSize6);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(lowpassX - handleOutlineSize6 / 2.0f, handleY6 - handleOutlineSize6 / 2.0f, handleOutlineSize6, handleOutlineSize6, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(lowpassX - innerOutlineSize6 / 2.0f, handleY6 - innerOutlineSize6 / 2.0f, innerOutlineSize6, innerOutlineSize6, outlineThickness2);

        // Draw the label for Lowpass
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor6);  // Apply scale factor to font size
        float textOffset6 = 3.5f * scaleFactor6;  // Apply scale factor to text position
        const char* lpLabel = (lpType == FilterType::lowpass)  ? "LP"
                            : (lpType == FilterType::highpass) ? "HP" : "8";
        g.drawText(lpLabel, lowpassX - textOffset6, handleY6 - textOffset6, 7.0f * scaleFactor6, 7.0f * scaleFactor6, juce::Justification::centred, false);
    }
    else
    {
        handleX6 = -1000.0f;
        handleY6 = -1000.0f;
    }

    //=======================================================================================================//
    // High Shelf //


    if (processor.getIsHighShelfOn())
    {
        // Unified scale factor
        float scaleFactor7 = (isMouseHoveringOverHandle7 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (6);

        // Get the values from the tree state
        float highShelfFrequency = processor.treeState.getRawParameterValue("highShelfFrequency")->load();
        const int highShelfType = (int) processor.treeState.getRawParameterValue ("highShelfType")->load();
        float highShelfGain = FilterType::usesGain (highShelfType)
                                ? processor.treeState.getRawParameterValue("highShelfGain")->load()
                                : 0.0f;

        // Calculate the x and y coordinates for the circle
        float highShelfX = (getWidth() - 1) * (std::log10(highShelfFrequency) - logMin) / (logMax - logMin);
        float highShelfY = dbToY(highShelfGain, static_cast<float>(getHeight()));

        // Apply scale factor
        float handleSize7 = 12.0f * scaleFactor7;

        // Limit handle positions by the edge of the window
        highShelfX = std::min(std::max(highShelfX, handleSize7), static_cast<float>(getWidth()) - handleSize7);
        highShelfY = std::min(std::max(highShelfY, handleSize7), static_cast<float>(getHeight()) - handleSize7);


        // Update handle positions
        handleX7 = highShelfX;
        handleY7 = highShelfY;

        // More graphical attributes
        float handleOutlineSize7 = 12.5f * scaleFactor7;
        float innerOutlineSize7 = 11.0f * scaleFactor7;

        // Draw the handle with your existing color
        g.setColour(handleColor7);
        g.fillEllipse(highShelfX - handleSize7 / 2.0f, highShelfY - handleSize7 / 2.0f, handleSize7, handleSize7);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(highShelfX - handleOutlineSize7 / 2.0f, highShelfY - handleOutlineSize7 / 2.0f, handleOutlineSize7, handleOutlineSize7, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(highShelfX - innerOutlineSize7 / 2.0f, highShelfY - innerOutlineSize7 / 2.0f, innerOutlineSize7, innerOutlineSize7, outlineThickness2);

        // Draw the label for High Shelf
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor7);  // Apply scale factor to font size
        float textOffset7 = 3.5f * scaleFactor7;  // Apply scale factor to text position
        g.drawText("HS", highShelfX - textOffset7, highShelfY - textOffset7, 7.0f * scaleFactor7, 7.0f * scaleFactor7, juce::Justification::centred, false);
    }
    else
    {
        handleX7 = -1000.0f;
        handleY7 = -1000.0f;
    }

    //=======================================================================================================//
    // Low Shelf //


    if (processor.getIsLowShelfOn())
    {
        // Unified scale factor for LowShelf
        float scaleFactor8 = (isMouseHoveringOverHandle8 ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (7);

        // Get the values from the tree state
        float lowShelfFrequency = processor.treeState.getRawParameterValue("lowShelfFrequency")->load();
        const int lowShelfType = (int) processor.treeState.getRawParameterValue ("lowShelfType")->load();
        float lowShelfGain = FilterType::usesGain (lowShelfType)
                               ? processor.treeState.getRawParameterValue("lowShelfGain")->load()
                               : 0.0f;

        // Calculate the x and y coordinates for the circle
        float lowShelfX = (getWidth() - 1) * (std::log10(lowShelfFrequency) - logMin) / (logMax - logMin);
        float lowShelfY = dbToY(lowShelfGain, static_cast<float>(getHeight()));

        // Apply scale factor to handle
        float handleSize8 = 12.0f * scaleFactor8;

        // Limit handle position by the edge of the window, taking full diameter into account
        lowShelfX = std::min(std::max(lowShelfX, handleSize8), static_cast<float>(getWidth()) - handleSize8);
        lowShelfY = std::min(std::max(lowShelfY, handleSize8), static_cast<float>(getHeight()) - handleSize8);

        // Update handle positions to reflect these constraints
        handleX8 = lowShelfX;
        handleY8 = lowShelfY;


        // Apply scale factor
        float handleOutlineSize8 = 12.5f * scaleFactor8;
        float innerOutlineSize8 = 11.0f * scaleFactor8;

        // Draw the handle with your existing color
        g.setColour(handleColor8);
        g.fillEllipse(lowShelfX - handleSize8 / 2.0f, lowShelfY - handleSize8 / 2.0f, handleSize8, handleSize8);

        // Draw the first outline with your existing color
        g.setColour(outlineColor);
        g.drawEllipse(lowShelfX - handleOutlineSize8 / 2.0f, lowShelfY - handleOutlineSize8 / 2.0f, handleOutlineSize8, handleOutlineSize8, outlineThickness);

        // Draw the second outline with your existing color
        g.setColour(outlineColor2);
        g.drawEllipse(lowShelfX - innerOutlineSize8 / 2.0f, lowShelfY - innerOutlineSize8 / 2.0f, innerOutlineSize8, innerOutlineSize8, outlineThickness2);

        // Draw the label for Low Shelf
        g.setColour(juce::Colours::black);
        g.setFont(11.0f * scaleFactor8);  // Apply scale factor to font size
        float textOffset8 = 3.5f * scaleFactor8;  // Apply scale factor to text position
        g.drawText("LS", lowShelfX - textOffset8, lowShelfY - textOffset8, 7.0f * scaleFactor8, 7.0f * scaleFactor8, juce::Justification::centred, false);
    }
    else
    {
        handleX8 = -1000.0f;
        handleY8 = -1000.0f;
    }



    //=======================================================================================================//
    // Crosshairs //


// Crosshairs
    if (isShowCrosshair() && mouseInside && !isAnyHandleMouseOver && !optionBoxMenu->isVisible()) {
        g.setColour(juce::Colour(229, 189, 128));  // Medium/Dark grey
        g.drawLine(cursorX, 0, cursorX, getHeight(), 1.0f);
        g.drawLine(0, cursorY, getWidth(), cursorY, 1.0f);
    }


    // Calculate frequency and dB from mouse position
    float cursorFreq = static_cast<float>(std::pow(10.0, static_cast<double>(juce::jmap(static_cast<float>(cursorX), 0.0f, static_cast<float>(getWidth()), static_cast<float>(logMin), static_cast<float>(logMax)))));
    float cursorDB = yToDb(cursorY, static_cast<float>(getHeight()));

    // Create the numerical readout strings for cursor position
    juce::String cursorReadoutDb = juce::String(cursorDB, 2) + " dB";
    juce::String cursorReadoutHz = juce::String(cursorFreq, 2) + " Hz";

    // Initialize the display variables with cursor values as defaults
    float displayGain = cursorDB;
    float displayFreq = cursorFreq;
    juce::String displayGainString = juce::String(cursorDB, 2) + " dB"; // New string for gain display
    juce::String displayQ = "N/A";  // Initialize displayQ as "N/A"

    // Initialize activeBand to -1 (no band is active)
    int activeBand = -1;

    // Loop through all bands to see which one is active
    for (int i = 0; i < 8; ++i) {
        //DBG("Checking handle: " << i);
        //DBG("isMouseHovering: " << static_cast<int>(isMouseHoveringOverHandle[i]));
        //DBG("isDragging: " << static_cast<int>(isHandleDragging[i]));

        if (isMouseHoveringOverHandle[i] || isHandleDragging[i]) {
            displayGain = arrayCurrentBandGain[i];
            displayFreq = arrayCurrentBandFrequency[i];
            displayQ = juce::String(arrayCurrentBandQ[i], 2);  // Update displayQ when a handle is hovered or dragged

            if (i == 4 || i == 5 || i == 6 || i == 7) {  // Include indices for High Shelf and Low Shelf
                displayGainString = "N/A";  // Gain is N/A for these types
            }
            else {
                displayGainString = juce::String(displayGain, 2) + " dB";  // Update the gain string
            }

            activeBand = i;  // Update the active band

        }
    }



    // Create the numerical readout strings based on the active band or cursor position
    juce::String readoutDb = isAnyHandleMouseOver ? juce::String(displayGain, 2) + " dB" : cursorReadoutDb;
    juce::String readoutHz = isAnyHandleMouseOver ? juce::String(displayFreq, 2) + " Hz" : cursorReadoutHz;
    juce::String readoutQ = "Q: " + (isAnyHandleMouseOver ? juce::String(displayQ) : "N/A");

    // Hover / cursor readout — band hover and free-cursor both +40% vs prior 5 / 7.5
    float labelY = cursorY - 20;
    float labelX = cursorX + 30;
    const float fontSize = isAnyHandleMouseOver ? 9.8f : 10.5f;
    juce::Font readoutFont ("Lato Black", fontSize, juce::Font::plain);
    g.setFont (readoutFont);
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.8f));

    const juce::String readoutBandName = (activeBand >= 0 && activeBand < 8)
        ? juce::String (arrayBandName[activeBand])
        : juce::String();

    // Size box to the widest line so drawText never ellipsizes to "..."
    auto measureLine = [&readoutFont] (const juce::String& text) -> float
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (readoutFont, text, 0.0f, 0.0f);
        return ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth();
    };
    float labelWidth = juce::jmax (juce::jmax (measureLine (readoutDb), measureLine (readoutHz)),
                                   juce::jmax (measureLine (readoutQ), measureLine (readoutBandName)));
    labelWidth = juce::jmax (labelWidth + 2.0f, 24.0f);

    const float verticalOffset = fontSize * 0.1f;
    const float lineHeight = fontSize + verticalOffset;
    const float totalLabelHeight = 4 * lineHeight;
    const float defaultLabelX = cursorX + 30;
    const float leftSideOffset = labelWidth + 15.0f;

    const bool isNearRightEdge = (getWidth() - cursorX) < labelWidth;
    const bool isNearLeftEdge = cursorX < labelWidth;

    if (isNearRightEdge || isNearLeftEdge)
    {
        if (cursorX < getWidth() / 2)
            labelX = cursorX + 15;
        else
            labelX = cursorX - leftSideOffset;
    }
    else
    {
        labelX = defaultLabelX;
    }

    labelX = std::min (std::max (labelX, 10.0f), static_cast<float> (getWidth()) - labelWidth - 10);
    labelY = std::min (std::max (labelY, 10.0f), static_cast<float> (getHeight()) - totalLabelHeight - 10);

    // Draw the rounded rectangle box only if a handle is hovered over, being dragged, or optionBoxMenu is not visible
    if (!optionBoxMenu->isVisible() && (isAnyHandleMouseOver || anyHandleDragging)) {
        // New code for rounded rectangle background
        juce::Colour customColor = juce::Colour::fromRGBA(65, 60, 55, 160); // RGBA with alpha 0.4
        juce::Colour borderColor = juce::Colour::fromRGBA(20, 10, 5, 150); // A bit darker and more opaque
        float borderRadius = 2.0f; // Rounded corner radius
        float borderWidth = 2.0f; // Border width
        float padding = 5.0f; // Padding around text

        // Calculate the bounding box for the rounded rectangle
        float boxX = labelX - padding;
        float boxY = labelY - padding;
        float boxWidth = labelWidth + 2 * padding;
        float boxHeight = totalLabelHeight + 2 * padding;

        // Limit box by the edge of the window
        boxX = std::min(std::max(boxX, 10.0f), static_cast<float>(getWidth()) - boxWidth - 10);
        boxY = std::min(std::max(boxY, 10.0f), static_cast<float>(getHeight()) - boxHeight - 10);

        // Update label positions based on box limits
        labelX = boxX + padding;
        labelY = boxY + padding;

        // Draw the background
        g.setColour(customColor);
        g.fillRoundedRectangle(boxX, boxY, boxWidth, boxHeight, borderRadius);

        // Create a Path object for the main border outline
        juce::Path borderPath;
        float offset = borderWidth / 2.0f; // Center the border on the edge
        borderPath.addRoundedRectangle(boxX + offset, boxY + offset, boxWidth - borderWidth, boxHeight - borderWidth, borderRadius - offset);

        // Draw the border using the path
        g.setColour(borderColor);
        g.strokePath(borderPath, juce::PathStrokeType(borderWidth));
    }


    g.setColour (juce::Colours::whitesmoke.withAlpha (0.8f));

    // Draw band name + dB / Hz / Q without ellipses (width already measured to fit)
    if (! optionBoxMenu->isVisible() && (isAnyHandleMouseOver || anyHandleDragging) && readoutBandName.isNotEmpty())
        g.drawText (readoutBandName, labelX, labelY, labelWidth, lineHeight, juce::Justification::bottomLeft, false);

    if (! optionBoxMenu->isVisible())
    {
        g.drawText (readoutDb, labelX, labelY + lineHeight, labelWidth, lineHeight, juce::Justification::bottomLeft, false);
        g.drawText (readoutHz, labelX, labelY + 2 * lineHeight, labelWidth, lineHeight, juce::Justification::bottomLeft, false);
        g.drawText (readoutQ, labelX, labelY + 3 * lineHeight, labelWidth, lineHeight, juce::Justification::bottomLeft, false);
    }

}


void FrequencyResponseComponent::mouseMove(const juce::MouseEvent& event)
{

    float distanceToHandle1 = std::hypot(event.position.x - handleX, event.position.y - handleY);
    float distanceToHandle2 = std::hypot(event.position.x - handleX2, event.position.y - handleY2);
    float distanceToHandle3 = std::hypot(event.position.x - handleX3, event.position.y - handleY3);
    float distanceToHandle4 = std::hypot(event.position.x - handleX4, event.position.y - handleY4);
    float distanceToHandle5 = std::hypot(event.position.x - handleX5, event.position.y - handleY5);
    float distanceToHandle6 = std::hypot(event.position.x - handleX6, event.position.y - handleY6);
    float distanceToHandle7 = std::hypot(event.position.x - handleX7, event.position.y - handleY7);
    float distanceToHandle8 = std::hypot(event.position.x - handleX8, event.position.y - handleY8);
   
   

    float mouseOverThreshold = 10.0f;  // Half of prior 20 (proportional to 1/2 handle size)

    cursorX = static_cast<float>(event.x);
    cursorY = static_cast<float>(event.y);
    isAnyHandleMouseOver = false;
    mouseInside = true;
    repaint();



    // Check if mouse is over the handle

    // Mouse over logic for handle 1
    float currentBand1Gain = 0.0f;
    float currentBand1Frequency = 0.0f;

    if (distanceToHandle1 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = true;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[0];
        activeBand = 0;

        // Get the current band1Gain value (normalized)
        float normalizedBand1Gain = processor.treeState.getParameter("band1Gain")->getValue();

        // Convert the normalized gain to full-scale (assuming the range is -24 to +24 dB)
        currentBand1Gain = juce::jmap(normalizedBand1Gain, 0.0f, 1.0f, -24.0f, 24.0f);

        // Update the array with the full-scale value
        arrayCurrentBandGain[0] = currentBand1Gain;

        // Get the current band1Frequency value (normalized)
        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band1Frequency"));
        if (paramFrequency) {
            currentBand1Frequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[0] = currentBand1Frequency;
        }

        float currentBand1Q = processor.treeState.getParameter("band1Q")->getValue();
        float normalizedBand1Q = juce::jmap(currentBand1Q, 0.0f, 1.0f, 0.15f, 10.0f);  // Declare the variable here
        arrayCurrentBandQ[0] = normalizedBand1Q;

    }
    else
    {
        isMouseHoveringOverHandle1 = false;
    }


    // Mouse over logic for handle 2
    float currentBand2Gain = 0.0f;
    float currentBand2Frequency = 0.0f;

    if (distanceToHandle2 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = true;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[1];
        activeBand = 1;

        // Get the current band2Gain value (normalized)
        float normalizedBand2Gain = processor.treeState.getParameter("band2Gain")->getValue();

        // Convert the normalized gain to full-scale (assuming the range is -24 to +24 dB)
        currentBand2Gain = juce::jmap(normalizedBand2Gain, 0.0f, 1.0f, -24.0f, 24.0f);

        // Update the array with the full-scale value
        arrayCurrentBandGain[1] = currentBand2Gain;

        // Get the current band2Frequency value (normalized)
        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band2Frequency"));
        if (paramFrequency) {
            currentBand2Frequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[1] = currentBand2Frequency;
        }

        float currentBand2Q = processor.treeState.getParameter("band2Q")->getValue();
        float normalizedBand2Q = juce::jmap(currentBand2Q, 0.0f, 1.0f, 0.15f, 10.0f);  // Declare the variable here
        arrayCurrentBandQ[1] = normalizedBand2Q;
    }
    else
    {
        isMouseHoveringOverHandle2 = false;
    }

    // Mouse over logic for handle 2
    float currentBand3Gain = 0.0f;
    float currentBand3Frequency = 0.0f;


    if (distanceToHandle3 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = true;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[2];
        activeBand = 2;

        // You know the drill, get the normalized gain
        float normalizedBand3Gain = processor.treeState.getParameter("band3Gain")->getValue();
        currentBand3Gain = juce::jmap(normalizedBand3Gain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[2] = currentBand3Gain;

        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band3Frequency"));
        if (paramFrequency) {
            currentBand3Frequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[2] = currentBand3Frequency;
        }

        float currentBand3Q = processor.treeState.getParameter("band3Q")->getValue();
        float normalizedBand3Q = juce::jmap(currentBand3Q, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[2] = normalizedBand3Q;
    }
    else
    {
        isMouseHoveringOverHandle3 = false;
    }



    // Mouse over logic for handle 4
    float currentBand4Gain = 0.0f;
    float currentBand4Frequency = 0.0f;

    if (distanceToHandle4 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = true;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[3];
        activeBand = 3;

        // Get the normalized gain for band 4
        float normalizedBand4Gain = processor.treeState.getParameter("band4Gain")->getValue();
        currentBand4Gain = juce::jmap(normalizedBand4Gain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[3] = currentBand4Gain;

        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band4Frequency"));
        if (paramFrequency) {
            currentBand4Frequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[3] = currentBand4Frequency;
        }

        float currentBand4Q = processor.treeState.getParameter("band4Q")->getValue();
        float normalizedBand4Q = juce::jmap(currentBand4Q, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[3] = normalizedBand4Q;
    }
    else
    {
        isMouseHoveringOverHandle4 = false;
    }


    // Mouse over logic for handle 5 (Highpass)
    float currentHighpassGain = 0.0f;
    float currentHighpassFrequency = 0.0f;

    if (distanceToHandle5 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = true;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[4];  // Highpass is at index 4
        activeBand = 4;

        {
            const int hpType = BandChannel::readChoiceIndex (processor.treeState, "highpassType", FilterType::highpass);
            arrayCurrentBandGain[4] = FilterType::usesGain (hpType)
                                         ? processor.treeState.getRawParameterValue ("highpassGain")->load()
                                         : 0.0f;
        }

        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("highpassCutoff"));
        if (paramFrequency) {
            currentHighpassFrequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[4] = currentHighpassFrequency;
        }

        float currentHighpassQ = processor.treeState.getParameter("highpassQ")->getValue();
        float normalizedHighpassQ = juce::jmap(currentHighpassQ, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[4] = normalizedHighpassQ;
    }


    // Mouse over logic for handle 6 (Lowpass)
    float currentLowpassGain = 0.0f;
    float currentLowpassFrequency = 0.0f;

    if (distanceToHandle6 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = true;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[5];  // Lowpass is at index 5
        activeBand = 5;

        {
            const int lpType = BandChannel::readChoiceIndex (processor.treeState, "lowpassType", FilterType::lowpass);
            arrayCurrentBandGain[5] = FilterType::usesGain (lpType)
                                         ? processor.treeState.getRawParameterValue ("lowpassGain")->load()
                                         : 0.0f;
            currentLowpassGain = arrayCurrentBandGain[5];
        }

        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("lowpassCutoff"));
        if (paramFrequency) {
            currentLowpassFrequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[5] = currentLowpassFrequency;
        }

        float currentLowpassQ = processor.treeState.getParameter("lowpassQ")->getValue();
        float normalizedLowpassQ = juce::jmap(currentLowpassQ, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[5] = normalizedLowpassQ;
    }


    // Mouse over logic for handle 7 (HighShelf)
    float currentHighShelfGain = 0.0f;
    float currentHighShelfFrequency = 0.0f;

    if (distanceToHandle7 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = true;
        isMouseHoveringOverHandle8 = false;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[6];  // HighShelf is at index 6
        activeBand = 6;

        float normalizedHighShelfGain = processor.treeState.getParameter("highShelfGain")->getValue();
        currentHighShelfGain = juce::jmap(normalizedHighShelfGain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[6] = currentHighShelfGain;

        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("highShelfFrequency"));
        if (paramFrequency) {
            currentHighShelfFrequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[6] = currentHighShelfFrequency;
        }

        float currentHighShelfQ = processor.treeState.getParameter("highShelfQ")->getValue();
        float normalizedHighShelfQ = juce::jmap(currentHighShelfQ, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[6] = normalizedHighShelfQ;
    }


    // Mouse over logic for handle 8 (LowShelf)
    float currentLowShelfGain = 0.0f;
    float currentLowShelfFrequency = 0.0f;

    if (distanceToHandle8 <= mouseOverThreshold)
    {
        isMouseHoveringOverHandle1 = false;
        isMouseHoveringOverHandle2 = false;
        isMouseHoveringOverHandle3 = false;
        isMouseHoveringOverHandle4 = false;
        isMouseHoveringOverHandle5 = false;
        isMouseHoveringOverHandle6 = false;
        isMouseHoveringOverHandle7 = false;
        isMouseHoveringOverHandle8 = true;
        isAnyHandleMouseOver = true;
        currentBandName = arrayBandName[7];  // LowShelf is at index 7
        activeBand = 7;

        float normalizedLowShelfGain = processor.treeState.getParameter("lowShelfGain")->getValue();
        currentLowShelfGain = juce::jmap(normalizedLowShelfGain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[7] = currentLowShelfGain;

        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("lowShelfFrequency"));
        if (paramFrequency) {
            currentLowShelfFrequency = paramFrequency->convertFrom0to1(paramFrequency->getValue());
            arrayCurrentBandFrequency[7] = currentLowShelfFrequency;
        }

        float currentLowShelfQ = processor.treeState.getParameter("lowShelfQ")->getValue();
        float normalizedLowShelfQ = juce::jmap(currentLowShelfQ, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[7] = normalizedLowShelfQ;
    }



    // Aggregating individual handle hover states into an array for easier access.
    isMouseHoveringOverHandle[0] = isMouseHoveringOverHandle1;
    isMouseHoveringOverHandle[1] = isMouseHoveringOverHandle2;
    isMouseHoveringOverHandle[2] = isMouseHoveringOverHandle3;
    isMouseHoveringOverHandle[3] = isMouseHoveringOverHandle4;
    isMouseHoveringOverHandle[4] = isMouseHoveringOverHandle5;
    isMouseHoveringOverHandle[5] = isMouseHoveringOverHandle6;
    isMouseHoveringOverHandle[6] = isMouseHoveringOverHandle7;
    isMouseHoveringOverHandle[7] = isMouseHoveringOverHandle8;


    // Trigger a repaint to reflect the change
    // repaint();
}





    //=======================================================================================================//
void FrequencyResponseComponent::setOptionBoxVisible (bool shouldBeVisible)
{
    if (optionBoxMenu == nullptr)
        return;

    const bool wasVisible = optionBoxMenu->isVisible();
    optionBoxMenu->setVisible (shouldBeVisible);

    // Always notify when visible so the host can repair z-order (setInitialPosition may
    // have already called setVisible(true) before we get here).
    if (onOptionBoxVisibilityChanged != nullptr
        && (shouldBeVisible || wasVisible != shouldBeVisible))
        onOptionBoxVisibilityChanged();
}

void FrequencyResponseComponent::showOptionBoxForHandle (int bandIndex, float handlePosX, float handlePosY)
{
    if (optionBoxMenu == nullptr)
        return;

    const bool wasVisible = optionBoxMenu->isVisible();
    const int previousBand = optionBoxMenu->getCurrentBandIndex();

    optionBoxMenu->setCurrentBandIndex (bandIndex, arrayBandName);

    // Snap beside the handle only on first open or when switching bands.
    // While the same band stays selected, keep the user's placed position
    // even if the handle moves (freq/gain drag).
    if (! wasVisible || previousBand != bandIndex)
        optionBoxMenu->setInitialPosition (juce::roundToInt (handlePosX), juce::roundToInt (handlePosY));

    setOptionBoxVisible (true);
    optionBoxMenu->setDraggable (true);
    optionBoxMenu->setInteractionFaded (false);
    lastOptionBoxBandIndex = bandIndex;
    lastHandlePopupWasOptionBox = true;
}

void FrequencyResponseComponent::showOptionBoxForBand (int bandIndex)
{
    if (bandIndex < 0 || bandIndex > 7)
        return;

    float hx = (float) getWidth() * 0.5f;
    float hy = (float) getHeight() * 0.4f;

    switch (bandIndex)
    {
        case 0: hx = handleX;  hy = handleY;  break;
        case 1: hx = handleX2; hy = handleY2; break;
        case 2: hx = handleX3; hy = handleY3; break;
        case 3: hx = handleX4; hy = handleY4; break;
        case 4: hx = handleX5; hy = handleY5; break;
        case 5: hx = handleX6; hy = handleY6; break;
        case 6: hx = handleX7; hy = handleY7; break;
        case 7: hx = handleX8; hy = handleY8; break;
        default: break;
    }

    if (hx < 0.0f || hy < 0.0f)
    {
        hx = (float) getWidth() * 0.5f;
        hy = (float) getHeight() * 0.4f;
    }

    showOptionBoxForHandle (bandIndex, hx, hy);
}

//=======================================================================================================//
void FrequencyResponseComponent::showHandleModMenu (int bandIndex)
{
    struct SlotRef
    {
        int slot = 0;
        int source = LfoMod::srcOff;
        int dest = LfoMod::destOff;
        bool enabled = true;
    };

    std::vector<SlotRef> targeting;
    targeting.reserve (8);

    for (int s = 0; s < LfoMod::kNumMatrixSlots; ++s)
    {
        int src = LfoMod::srcOff, dest = LfoMod::destOff;
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (LfoMod::slotSourceParamId (s))))
            src = p->getIndex();
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (LfoMod::slotDestParamId (s))))
            dest = p->getIndex();

        if (src <= LfoMod::srcOff || dest <= LfoMod::destOff)
            continue;
        if (! LfoMod::destinationTargetsBand (dest, bandIndex))
            continue;

        bool on = true;
        if (auto* v = parameters.getRawParameterValue (LfoMod::slotEnabledParamId (s)))
            on = v->load() > 0.5f;

        targeting.push_back ({ s, src, dest, on });
    }

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const auto sourceNames = LfoMod::getSourceNames();

    if (targeting.empty())
    {
        menu.addItem (-1, "(no mod sources on this band)", false, false);
    }
    else
    {
        for (const auto& t : targeting)
        {
            const juce::String label = sourceNames[juce::jlimit (0, sourceNames.size() - 1, t.source)]
                                       + " → " + LfoMod::shortDestinationLabel (t.dest);
            // Checkable: active = enabled. Toggle on select.
            menu.addItem (1000 + t.slot, label, true, t.enabled);
        }

        menu.addSeparator();
        menu.addItem (1, "Deactivate all (testing)");
        menu.addItem (2, "Activate all");
        menu.addSeparator();

        for (const auto& t : targeting)
        {
            const juce::String label = "Remove "
                                       + sourceNames[juce::jlimit (0, sourceNames.size() - 1, t.source)]
                                       + " → " + LfoMod::shortDestinationLabel (t.dest);
            menu.addItem (2000 + t.slot, label);
        }

        menu.addItem (3, "Remove all mod sources");
    }

    lastHandlePopupWasOptionBox = false;

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<FrequencyResponseComponent> (this),
                         targeting] (int result)
                        {
                            if (safe == nullptr || result == 0)
                                return;

                            auto& state = safe->parameters;

                            auto setBool = [&state] (const juce::String& id, bool value)
                            {
                                if (auto* p = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (id)))
                                {
                                    if (p->get() == value)
                                        return;
                                    p->beginChangeGesture();
                                    *p = value;
                                    p->endChangeGesture();
                                }
                            };

                            auto clearSlot = [&state] (int slot)
                            {
                                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                                        state.getParameter (LfoMod::slotSourceParamId (slot))))
                                {
                                    p->beginChangeGesture();
                                    *p = LfoMod::srcOff;
                                    p->endChangeGesture();
                                }
                                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                                        state.getParameter (LfoMod::slotDestParamId (slot))))
                                {
                                    p->beginChangeGesture();
                                    *p = LfoMod::destOff;
                                    p->endChangeGesture();
                                }
                                if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
                                        state.getParameter (LfoMod::slotEnabledParamId (slot))))
                                {
                                    if (! p->get())
                                    {
                                        p->beginChangeGesture();
                                        *p = true;
                                        p->endChangeGesture();
                                    }
                                }
                            };

                            if (result == 1) // deactivate all
                            {
                                for (const auto& t : targeting)
                                    setBool (LfoMod::slotEnabledParamId (t.slot), false);
                                return;
                            }
                            if (result == 2) // activate all
                            {
                                for (const auto& t : targeting)
                                    setBool (LfoMod::slotEnabledParamId (t.slot), true);
                                return;
                            }
                            if (result == 3) // remove all
                            {
                                for (const auto& t : targeting)
                                    clearSlot (t.slot);
                                return;
                            }
                            if (result >= 2000 && result < 2000 + LfoMod::kNumMatrixSlots)
                            {
                                clearSlot (result - 2000);
                                return;
                            }
                            if (result >= 1000 && result < 1000 + LfoMod::kNumMatrixSlots)
                            {
                                const int slot = result - 1000;
                                bool currentlyOn = true;
                                if (auto* v = state.getRawParameterValue (LfoMod::slotEnabledParamId (slot)))
                                    currentlyOn = v->load() > 0.5f;
                                setBool (LfoMod::slotEnabledParamId (slot), ! currentlyOn);
                            }
                        });
}

//=======================================================================================================//
void FrequencyResponseComponent::setOptionBoxInteractionFaded (bool shouldFade)
{
    if (optionBoxMenu != nullptr && optionBoxMenu->isVisible())
        optionBoxMenu->setInteractionFaded (shouldFade);
}

//=======================================================================================================//
void FrequencyResponseComponent::resetBandToDefaultsAndDeactivate (int bandIndex)
{
    auto setFloatParam = [this] (const juce::String& paramID, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter (paramID)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    auto setBoolParam = [this] (const juce::String& paramID, bool value)
    {
        if (auto* param = processor.treeState.getParameter (paramID))
            param->setValueNotifyingHost (value ? 1.0f : 0.0f);
    };

    switch (bandIndex)
    {
        case 0:
            setFloatParam ("band1Frequency", 300.0f);
            setFloatParam ("band1Gain", 0.0f);
            setFloatParam ("band1Q", 0.67f);
            setBoolParam ("band1OnOff", false);
            setBoolParam ("band1Dynamic", false);
            setBoolParam ("band1Spectral", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band1Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band1Channel")))
                *p = BandChannel::stereo;
            break;
        case 1:
            setFloatParam ("band2Frequency", 1000.0f);
            setFloatParam ("band2Gain", 0.0f);
            setFloatParam ("band2Q", 0.67f);
            setBoolParam ("band2OnOff", false);
            setBoolParam ("band2Dynamic", false);
            setBoolParam ("band2Spectral", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band2Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band2Channel")))
                *p = BandChannel::stereo;
            break;
        case 2:
            setFloatParam ("band3Frequency", 4000.0f);
            setFloatParam ("band3Gain", 0.0f);
            setFloatParam ("band3Q", 0.67f);
            setBoolParam ("band3OnOff", false);
            setBoolParam ("band3Dynamic", false);
            setBoolParam ("band3Spectral", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band3Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band3Channel")))
                *p = BandChannel::stereo;
            break;
        case 3:
            setFloatParam ("band4Frequency", 8000.0f);
            setFloatParam ("band4Gain", 0.0f);
            setFloatParam ("band4Q", 0.67f);
            setBoolParam ("band4OnOff", false);
            setBoolParam ("band4Dynamic", false);
            setBoolParam ("band4Spectral", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band4Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band4Channel")))
                *p = BandChannel::stereo;
            break;
        case 4:
            setFloatParam ("highpassCutoff", 20.0f);
            setFloatParam ("highpassQ", 0.5f);
            setBoolParam ("highpassOnOff", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("highpassSlope")))
                *p = FilterSlope::db12;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("highpassChannel")))
                *p = BandChannel::stereo;
            break;
        case 5:
            setFloatParam ("lowpassCutoff", 20000.0f);
            setFloatParam ("lowpassQ", 0.5f);
            setBoolParam ("lowpassOnOff", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("lowpassSlope")))
                *p = FilterSlope::db12;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("lowpassChannel")))
                *p = BandChannel::stereo;
            break;
        case 6:
            setFloatParam ("highShelfFrequency", 5500.0f);
            setFloatParam ("highShelfGain", 0.0f);
            setFloatParam ("highShelfQ", 0.5f);
            setBoolParam ("highShelfOnOff", false);
            setBoolParam ("highShelfDynamic", false);
            setBoolParam ("highShelfSpectral", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("highShelfType")))
                *p = FilterType::highShelf;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("highShelfChannel")))
                *p = BandChannel::stereo;
            break;
        case 7:
            setFloatParam ("lowShelfFrequency", 100.0f);
            setFloatParam ("lowShelfGain", 0.0f);
            setFloatParam ("lowShelfQ", 0.5f);
            setBoolParam ("lowShelfOnOff", false);
            setBoolParam ("lowShelfDynamic", false);
            setBoolParam ("lowShelfSpectral", false);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("lowShelfType")))
                *p = FilterType::lowShelf;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("lowShelfChannel")))
                *p = BandChannel::stereo;
            break;
        default:
            break;
    }

    setOptionBoxVisible (false);

    needsUpdateCombined = true;
    repaint();
}

float FrequencyResponseComponent::xToFrequency (float x) const
{
    const int w = juce::jmax (2, getWidth());
    const float clampedX = juce::jlimit (0.0f, static_cast<float> (w - 1), x);
    const double logF = static_cast<double> (logMin)
                        + (static_cast<double> (logMax) - static_cast<double> (logMin))
                          * static_cast<double> (clampedX) / static_cast<double> (w - 1);
    return juce::jlimit (20.0f, 20000.0f, static_cast<float> (std::pow (10.0, logF)));
}

int FrequencyResponseComponent::bandIndexForFrequencyZone (float frequencyHz) const
{
    // Zones: HP <50 | LS 50–150 | 4 bells (log) 150–8k | HS 8–12k | LP 12–20k
    // Internal indices: 4=HP(Band1), 7=LS(Band2), 0–3=Bells(Band3–6), 6=HS(Band7), 5=LP(Band8)
    constexpr float kHpMaxHz = 50.0f;
    constexpr float kLsMaxHz = 150.0f;
    constexpr float kPeakMinHz = 150.0f;
    constexpr float kPeakMaxHz = 8000.0f;
    constexpr float kHsMaxHz = 12000.0f;

    const float f = juce::jlimit (20.0f, 20000.0f, frequencyHz);

    if (f < kHpMaxHz)
        return 4; // Band 1 — Highpass

    if (f < kLsMaxHz)
        return 7; // Band 2 — Low shelf

    if (f < kPeakMaxHz)
    {
        const float logMinPeak = std::log10 (kPeakMinHz);
        const float logMaxPeak = std::log10 (kPeakMaxHz);
        const float t = (std::log10 (f) - logMinPeak) / (logMaxPeak - logMinPeak);
        return juce::jlimit (0, 3, static_cast<int> (std::floor (t * 4.0f)));
    }

    if (f < kHsMaxHz)
        return 6; // Band 7 — High shelf

    return 5; // Band 8 — Lowpass
}

void FrequencyResponseComponent::activateOrSelectBandAtFrequency (float frequencyHz)
{
    const int preferredBand = bandIndexForFrequencyZone (frequencyHz);
    const float freq = juce::jlimit (20.0f, 20000.0f, frequencyHz);

    auto setFloatParam = [this] (const juce::String& paramID, float value)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter (paramID)))
            param->setValueNotifyingHost (param->convertTo0to1 (value));
    };

    auto setBoolParam = [this] (const juce::String& paramID, bool value)
    {
        if (auto* param = processor.treeState.getParameter (paramID))
            param->setValueNotifyingHost (value ? 1.0f : 0.0f);
    };

    auto isBandOn = [this] (int index) -> bool
    {
        switch (index)
        {
            case 0: return processor.getIsBand1On();
            case 1: return processor.getIsBand2On();
            case 2: return processor.getIsBand3On();
            case 3: return processor.getIsBand4On();
            case 4: return processor.getIsHighpassOn();
            case 5: return processor.getIsLowpassOn();
            case 6: return processor.getIsHighShelfOn();
            case 7: return processor.getIsLowShelfOn();
            default: return false;
        }
    };

    auto selectBandOnly = [this] (int bandIndex)
    {
        this->activeBand = bandIndex;
        isMouseHoveringOverHandle1 = bandIndex == 0;
        isMouseHoveringOverHandle2 = bandIndex == 1;
        isMouseHoveringOverHandle3 = bandIndex == 2;
        isMouseHoveringOverHandle4 = bandIndex == 3;
        isMouseHoveringOverHandle5 = bandIndex == 4;
        isMouseHoveringOverHandle6 = bandIndex == 5;
        isMouseHoveringOverHandle7 = bandIndex == 6;
        isMouseHoveringOverHandle8 = bandIndex == 7;
        isAnyHandleMouseOver = true;

        if (onBandManipulationHighlight)
            onBandManipulationHighlight (bandIndex);

        needsUpdateCombined = true;
        repaint();
    };

    // Prefer the frequency-zone band when it is off. If that zone already has an
    // active handle, allocate a free neighboring / unused band instead of moving it.
    int bandIndex = preferredBand;

    if (isBandOn (preferredBand))
    {
        int peakAnchor = preferredBand;

        if (preferredBand < 0 || preferredBand > 3)
        {
            // HP / LP / shelf already on → borrow a free peaking slot near the click.
            constexpr float kPeakMinHz = 300.0f;
            constexpr float kPeakMaxHz = 11999.0f;
            peakAnchor = bandIndexForFrequencyZone (juce::jlimit (kPeakMinHz, kPeakMaxHz, freq));
        }

        bandIndex = -1;

        for (int offset = 0; offset < 4 && bandIndex < 0; ++offset)
        {
            const int candidates[] = { peakAnchor - offset, peakAnchor + offset };

            for (int candidate : candidates)
            {
                if (candidate < 0 || candidate > 3)
                    continue;

                if (offset == 0 && candidate != peakAnchor)
                    continue;

                if (! isBandOn (candidate))
                {
                    bandIndex = candidate;
                    break;
                }
            }
        }

        // Last resort: any other unused filter band (shelf / HP / LP).
        if (bandIndex < 0)
        {
            constexpr int kFallbackOrder[] = { 7, 6, 4, 5 };

            for (int candidate : kFallbackOrder)
            {
                if (! isBandOn (candidate))
                {
                    bandIndex = candidate;
                    break;
                }
            }
        }

        // Every band is already active — select the zone band, do not relocate it.
        if (bandIndex < 0)
        {
            selectBandOnly (preferredBand);
            return;
        }
    }

    // Activate an unused band at click Hz / 0 dB; set type from frequency zone.
    const int zoneType = FilterType::typeForFrequencyZone (freq);

    auto setTypeParam = [this] (const juce::String& paramID, int typeIndex)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                processor.treeState.getParameter (paramID)))
            *choice = typeIndex;
    };

    switch (bandIndex)
    {
        case 0:
            setFloatParam ("band1Frequency", freq);
            setFloatParam ("band1Gain", 0.0f);
            setTypeParam ("band1Type", zoneType);
            setBoolParam ("band1OnOff", true);
            needsUpdateBand1 = true;
            break;
        case 1:
            setFloatParam ("band2Frequency", freq);
            setFloatParam ("band2Gain", 0.0f);
            setTypeParam ("band2Type", zoneType);
            setBoolParam ("band2OnOff", true);
            needsUpdateBand2 = true;
            break;
        case 2:
            setFloatParam ("band3Frequency", freq);
            setFloatParam ("band3Gain", 0.0f);
            setTypeParam ("band3Type", zoneType);
            setBoolParam ("band3OnOff", true);
            needsUpdateBand3 = true;
            break;
        case 3:
            setFloatParam ("band4Frequency", freq);
            setFloatParam ("band4Gain", 0.0f);
            setTypeParam ("band4Type", zoneType);
            setBoolParam ("band4OnOff", true);
            needsUpdateBand4 = true;
            break;
        case 4:
            setFloatParam ("highpassCutoff", freq);
            setFloatParam ("highpassGain", 0.0f);
            setTypeParam ("highpassType", zoneType);
            setBoolParam ("highpassOnOff", true);
            needsUpdateHighpass = true;
            break;
        case 5:
            setFloatParam ("lowpassCutoff", freq);
            setFloatParam ("lowpassGain", 0.0f);
            setTypeParam ("lowpassType", zoneType);
            setBoolParam ("lowpassOnOff", true);
            needsUpdateLowpass = true;
            break;
        case 6:
            setFloatParam ("highShelfFrequency", freq);
            setFloatParam ("highShelfGain", 0.0f);
            setTypeParam ("highShelfType", zoneType);
            setBoolParam ("highShelfOnOff", true);
            needsUpdateHighShelf = true;
            break;
        case 7:
            setFloatParam ("lowShelfFrequency", freq);
            setFloatParam ("lowShelfGain", 0.0f);
            setTypeParam ("lowShelfType", zoneType);
            setBoolParam ("lowShelfOnOff", true);
            needsUpdateLowShelf = true;
            break;
        default:
            break;
    }

    selectBandOnly (bandIndex);
}

//=======================================================================================================//
void FrequencyResponseComponent::mouseDown(const juce::MouseEvent& event)
{
    processor.getUndoManager().beginNewTransaction ("EQ edit");

    float distanceToHandle1 = std::hypot(event.position.x - handleX, event.position.y - handleY);
    float distanceToHandle2 = std::hypot(event.position.x - handleX2, event.position.y - handleY2);
    float distanceToHandle3 = std::hypot(event.position.x - handleX3, event.position.y - handleY3);
    float distanceToHandle4 = std::hypot(event.position.x - handleX4, event.position.y - handleY4);
    float distanceToHandle5 = std::hypot(event.position.x - handleX5, event.position.y - handleY5);
    float distanceToHandle6 = std::hypot(event.position.x - handleX6, event.position.y - handleY6);
    float distanceToHandle7 = std::hypot(event.position.x - handleX7, event.position.y - handleY7);
    float distanceToHandle8 = std::hypot(event.position.x - handleX8, event.position.y - handleY8);
    float clickThreshold = 10.0f;

    const bool clickedHandle1 = distanceToHandle1 <= clickThreshold;
    const bool clickedHandle2 = distanceToHandle2 <= clickThreshold;
    const bool clickedHandle3 = distanceToHandle3 <= clickThreshold;
    const bool clickedHandle4 = distanceToHandle4 <= clickThreshold;
    const bool clickedHandle5 = distanceToHandle5 <= clickThreshold;
    const bool clickedHandle6 = distanceToHandle6 <= clickThreshold;
    const bool clickedHandle7 = distanceToHandle7 <= clickThreshold;
    const bool clickedHandle8 = distanceToHandle8 <= clickThreshold;
    const bool clickedAnyHandle = clickedHandle1 || clickedHandle2 || clickedHandle3 || clickedHandle4
                                  || clickedHandle5 || clickedHandle6 || clickedHandle7 || clickedHandle8;

    // Hide popup when clicking empty graph space (not when selecting a handle or interacting with the popup).
    // Use screen bounds so this still works if MainComponent is hosting the OptionBox above the expanded scope.
    const bool clickInsideOptionBox = optionBoxMenu != nullptr
                                      && optionBoxMenu->isVisible()
                                      && optionBoxMenu->getScreenBounds().contains (event.getScreenPosition());

    if (! clickedAnyHandle
        && optionBoxMenu != nullptr
        && optionBoxMenu->isVisible()
        && ! clickInsideOptionBox)
    {
        setOptionBoxVisible (false);
        lastOptionBoxBandIndex = -1;
        lastHandlePopupWasOptionBox = false;
    }

    // Clicks inside the option box are for its controls — don't start handle drags.
    if (clickInsideOptionBox)
        return;

    // Right-click a handle: first opens OptionBox; second (same handle) opens mod source menu.
    if (event.mods.isPopupMenu() && clickedAnyHandle && ! anyHandleDragging)
    {
        int band = -1;
        if (clickedHandle1)      band = 0;
        else if (clickedHandle2) band = 1;
        else if (clickedHandle3) band = 2;
        else if (clickedHandle4) band = 3;
        else if (clickedHandle5) band = 4;
        else if (clickedHandle6) band = 5;
        else if (clickedHandle7) band = 6;
        else if (clickedHandle8) band = 7;

        if (band >= 0)
        {
            const bool optionVisible = optionBoxMenu != nullptr && optionBoxMenu->isVisible();
            const bool sameBandAsOption = optionVisible
                                          && optionBoxMenu->getCurrentBandIndex() == band
                                          && lastOptionBoxBandIndex == band
                                          && lastHandlePopupWasOptionBox;

            if (sameBandAsOption)
            {
                showHandleModMenu (band);
            }
            else
            {
                float hx = handleX, hy = handleY;
                if (band == 1) { hx = handleX2; hy = handleY2; }
                else if (band == 2) { hx = handleX3; hy = handleY3; }
                else if (band == 3) { hx = handleX4; hy = handleY4; }
                else if (band == 4) { hx = handleX5; hy = handleY5; }
                else if (band == 5) { hx = handleX6; hy = handleY6; }
                else if (band == 6) { hx = handleX7; hy = handleY7; }
                else if (band == 7) { hx = handleX8; hy = handleY8; }
                showOptionBoxForHandle (band, hx, hy);
            }
        }
        return;
    }

    // Double-click is handled in mouseDoubleClick (deactivate + reset).
    if (event.getNumberOfClicks() >= 2)
        return;

    if (clickedHandle1 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (0, handleX, handleY);

        isHandle1Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (0);
        dragOffsetXHandle1 = event.position.x - handleX;
        dragOffsetYHandle1 = event.position.y - handleY;

        processor.treeState.getParameter("band1Frequency")->beginChangeGesture();
        processor.treeState.getParameter("band1Gain")->beginChangeGesture();
        processor.treeState.getParameter("band1Q")->beginChangeGesture();
    }
    else if (clickedHandle2 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (1, handleX2, handleY2);

        isHandle2Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (1);
        dragOffsetXHandle2 = event.position.x - handleX2;
        dragOffsetYHandle2 = event.position.y - handleY2;

        processor.treeState.getParameter("band2Frequency")->beginChangeGesture();
        processor.treeState.getParameter("band2Gain")->beginChangeGesture();
        processor.treeState.getParameter("band2Q")->beginChangeGesture();
    }
    else if (clickedHandle3 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (2, handleX3, handleY3);

        isHandle3Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (2);
        dragOffsetXHandle3 = event.position.x - handleX3;
        dragOffsetYHandle3 = event.position.y - handleY3;

        processor.treeState.getParameter("band3Frequency")->beginChangeGesture();
        processor.treeState.getParameter("band3Gain")->beginChangeGesture();
        processor.treeState.getParameter("band3Q")->beginChangeGesture();
    }
    else if (clickedHandle4 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (3, handleX4, handleY4);

        isHandle4Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (3);
        dragOffsetXHandle4 = event.position.x - handleX4;
        dragOffsetYHandle4 = event.position.y - handleY4;

        processor.treeState.getParameter("band4Frequency")->beginChangeGesture();
        processor.treeState.getParameter("band4Gain")->beginChangeGesture();
        processor.treeState.getParameter("band4Q")->beginChangeGesture();
    }
    else if (clickedHandle5 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (4, handleX5, handleY5);

        isHandle5Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (4);
        dragOffsetXHandle5 = event.position.x - handleX5;
        dragOffsetYHandle5 = event.position.y - handleY5;

        processor.treeState.getParameter("highpassCutoff")->beginChangeGesture();
        processor.treeState.getParameter("highpassQ")->beginChangeGesture();
    }
    else if (clickedHandle6 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (5, handleX6, handleY6);

        isHandle6Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (5);
        dragOffsetXHandle6 = event.position.x - handleX6;
        dragOffsetYHandle6 = event.position.y - handleY6;

        processor.treeState.getParameter("lowpassCutoff")->beginChangeGesture();
        processor.treeState.getParameter("lowpassQ")->beginChangeGesture();
    }
    else if (clickedHandle7 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (6, handleX7, handleY7);

        isHandle7Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (6);
        dragOffsetXHandle7 = event.position.x - handleX7;
        dragOffsetYHandle7 = event.position.y - handleY7;

        processor.treeState.getParameter("highShelfFrequency")->beginChangeGesture();
        processor.treeState.getParameter("highShelfGain")->beginChangeGesture();
        processor.treeState.getParameter("highShelfQ")->beginChangeGesture();
    }
    else if (clickedHandle8 && ! anyHandleDragging)
    {
        showOptionBoxForHandle (7, handleX8, handleY8);

        isHandle8Dragging = true;
        anyHandleDragging = true;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (7);
        dragOffsetXHandle8 = event.position.x - handleX8;
        dragOffsetYHandle8 = event.position.y - handleY8;

        processor.treeState.getParameter("lowShelfFrequency")->beginChangeGesture();
        processor.treeState.getParameter("lowShelfGain")->beginChangeGesture();
        processor.treeState.getParameter("lowShelfQ")->beginChangeGesture();
    }
}

//=======================================================================================================//
void FrequencyResponseComponent::mouseDoubleClick (const juce::MouseEvent& event)
{
    constexpr float clickThreshold = 10.0f;

    // Double-click on an existing (active) handle → reset/deactivate that band.
    if (std::hypot (event.position.x - handleX, event.position.y - handleY) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (0);
    else if (std::hypot (event.position.x - handleX2, event.position.y - handleY2) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (1);
    else if (std::hypot (event.position.x - handleX3, event.position.y - handleY3) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (2);
    else if (std::hypot (event.position.x - handleX4, event.position.y - handleY4) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (3);
    else if (std::hypot (event.position.x - handleX5, event.position.y - handleY5) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (4);
    else if (std::hypot (event.position.x - handleX6, event.position.y - handleY6) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (5);
    else if (std::hypot (event.position.x - handleX7, event.position.y - handleY7) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (6);
    else if (std::hypot (event.position.x - handleX8, event.position.y - handleY8) <= clickThreshold)
        resetBandToDefaultsAndDeactivate (7);
    else
    {
        // Empty graph double-click → enable a free band at click frequency (0 dB).
        // If the zone band is already on, allocate a neighboring unused band instead of moving it.
        activateOrSelectBandAtFrequency (xToFrequency (event.position.x));
    }
}

//=======================================================================================================//
void FrequencyResponseComponent::mouseDrag(const juce::MouseEvent& event)
{
    auto area = getLocalBounds();
    auto w = area.getWidth();
    auto h = area.getHeight();

    float newBand1Gain = 0.0f;
    float newBand1Frequency = 0.0f;
    float newBand1Q = 0.0f;



    if (anyHandleDragging)
    {
        cursorX = static_cast<float>(event.x);
        cursorY = static_cast<float>(event.y);

    }

    if (isHandle1Dragging)
    {
        isHandle2Dragging = false;
        isHandle3Dragging = false;
        isHandle4Dragging = false;
        isHandle5Dragging = false;
        isHandle6Dragging = false;
        isHandle7Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[0];
        activeBand = 0;

        // Calculate the new position of handle 1
        float newXValue = juce::jlimit(0.0f, static_cast<float>(w), event.position.x - dragOffsetXHandle1);
        float newYValue = juce::jlimit(0.0f, static_cast<float>(h), event.position.y - dragOffsetYHandle1);

        // Update handleX and handleY with the new position for handle 1
        handleX = newXValue;
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band1Type")->load()))
            handleY = newYValue;
        else
            handleY = dbToY (0.0f, static_cast<float> (h));

        // Calculate newBand1Gain based on the Y position (display-range aware)
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter ("band1Gain")))
                newBand1Gain = paramGain->convertTo0to1 (gainDb);
            else
                newBand1Gain = juce::jmap (gainDb, -24.0f, 24.0f, 0.0f, 1.0f);
        }

        // Constants for logarithmic mapping
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20000 Hz

        // Calculate newBand1Frequency based on the X position
        newBand1Frequency = std::pow(10.0f, logMin + (logMax - logMin) * handleX / static_cast<float>(w - 1));

        // Set the new band1Gain value (ignored for notch / band-pass)
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band1Type")->load()))
            processor.treeState.getParameter("band1Gain")->setValueNotifyingHost(newBand1Gain);

        float fullScaleBand1Gain = juce::jmap(newBand1Gain, 0.0f, 1.0f, -24.0f, 24.0f);

        // Update the array with the full-scale value
        arrayCurrentBandGain[0] = fullScaleBand1Gain;

        // Set the new band1Frequency value
        auto* paramFrequency = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band1Frequency"));
        if (paramFrequency)
        {
            float currentBand1Frequency = 0.0f;
            float normalizedBand1Frequency = paramFrequency->convertTo0to1(newBand1Frequency);
            processor.treeState.getParameter("band1Frequency")->setValueNotifyingHost(normalizedBand1Frequency);
            arrayCurrentBandFrequency[0] = newBand1Frequency;

            float currentBand1Q = processor.treeState.getParameter("band1Q")->getValue();
            float fullScaleBand1Q = juce::jmap(currentBand1Q, 0.0f, 1.0f, 0.15f, 10.0f);
            arrayCurrentBandQ[0] = fullScaleBand1Q;
        }

        float currentBand1Q = processor.treeState.getParameter("band1Q")->getValue();
        float fullScaleBand1Q = juce::jmap(currentBand1Q, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[0] = fullScaleBand1Q;

        needsUpdateBand1 = true;
        needsUpdateCombined = true;

    }    
        //==================================================================================================================//
    else if (isHandle2Dragging) // Check if handle 3 is dragging
    {
        isHandle1Dragging = false;
        isHandle3Dragging = false;
        isHandle4Dragging = false;
        isHandle5Dragging = false;
        isHandle6Dragging = false;
        isHandle7Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateBand2 = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[1];
        activeBand = 1;

        // Calculate the new position of handle 2
        float newXValue2 = juce::jlimit(0.0f, static_cast<float>(w), event.position.x - dragOffsetXHandle2);
        float newYValue2 = juce::jlimit(0.0f, static_cast<float>(h), event.position.y - dragOffsetYHandle2);

        // Update handleX2 and handleY2 with the new position for handle 2
        handleX2 = newXValue2;
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band2Type")->load()))
            handleY2 = newYValue2;
        else
            handleY2 = dbToY (0.0f, static_cast<float> (h));

        // Calculate newBand2Gain based on the Y position (display-range aware)
        float newBand2Gain = 0.0f;
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue2, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter ("band2Gain")))
                newBand2Gain = paramGain->convertTo0to1 (gainDb);
            else
                newBand2Gain = juce::jmap (gainDb, -24.0f, 24.0f, 0.0f, 1.0f);
        }

        // Constants for logarithmic mapping
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20000 Hz

        // Calculate newBand2Frequency based on the X position
        float newBand2Frequency = std::pow(10.0f, logMin + (logMax - logMin) * handleX2 / static_cast<float>(w - 1));

        // Set the new band2Gain value (ignored for notch / band-pass)
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band2Type")->load()))
            processor.treeState.getParameter("band2Gain")->setValueNotifyingHost(newBand2Gain);

        float fullScaleBand2Gain = juce::jmap(newBand2Gain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[1] = fullScaleBand2Gain;

        // Set the new band2Frequency value
        auto* paramFrequency2 = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band2Frequency"));
        if (paramFrequency2)
        {
            float normalizedBand2Frequency = paramFrequency2->convertTo0to1(newBand2Frequency);
            processor.treeState.getParameter("band2Frequency")->setValueNotifyingHost(normalizedBand2Frequency);
            arrayCurrentBandFrequency[1] = newBand2Frequency;
        }

        float currentBand2Q = processor.treeState.getParameter("band2Q")->getValue();
        float fullScaleBand2Q = juce::jmap(currentBand2Q, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[1] = fullScaleBand2Q;

        needsUpdateBand2 = true;
        needsUpdateCombined = true;
    }

    else if (isHandle3Dragging) // Check if handle 3 is dragging
    {
        isHandle2Dragging = false;
        isHandle1Dragging = false;
        isHandle4Dragging = false;
        isHandle5Dragging = false;
        isHandle6Dragging = false;
        isHandle7Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateBand3 = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[2];
        activeBand = 2;

        // Calculate the new position of handle 3
        float newXValue3 = juce::jlimit(0.0f, static_cast<float>(w), event.position.x - dragOffsetXHandle3);
        float newYValue3 = juce::jlimit(0.0f, static_cast<float>(h), event.position.y - dragOffsetYHandle3);

        // Update handleX3 and handleY3 with the new position for handle 3
        handleX3 = newXValue3;
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band3Type")->load()))
            handleY3 = newYValue3;
        else
            handleY3 = dbToY (0.0f, static_cast<float> (h));

        // Calculate newBand3Gain based on the Y position (display-range aware)
        float newBand3Gain = 0.0f;
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue3, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter ("band3Gain")))
                newBand3Gain = paramGain->convertTo0to1 (gainDb);
            else
                newBand3Gain = juce::jmap (gainDb, -24.0f, 24.0f, 0.0f, 1.0f);
        }

        // Constants for logarithmic mapping
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20000 Hz

        // Calculate newBand3Frequency based on the X position
        float newBand3Frequency = std::pow(10.0f, logMin + (logMax - logMin) * handleX3 / static_cast<float>(w - 1));

        // Set the new band3Gain value (ignored for notch / band-pass)
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band3Type")->load()))
            processor.treeState.getParameter("band3Gain")->setValueNotifyingHost(newBand3Gain);

        float fullScaleBand3Gain = juce::jmap(newBand3Gain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[2] = fullScaleBand3Gain;

        // Set the new band3Frequency value
        auto* paramFrequency3 = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band3Frequency"));
        if (paramFrequency3)
        {
            float normalizedBand3Frequency = paramFrequency3->convertTo0to1(newBand3Frequency);
            processor.treeState.getParameter("band3Frequency")->setValueNotifyingHost(normalizedBand3Frequency);
            arrayCurrentBandFrequency[2] = newBand3Frequency;
        }

        float currentBand3Q = processor.treeState.getParameter("band3Q")->getValue();
        float fullScaleBand3Q = juce::jmap(currentBand3Q, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[2] = fullScaleBand3Q;

        needsUpdateBand3 = true;
        needsUpdateCombined = true;



    }



    else if (isHandle4Dragging) // Check if handle 4 is dragging
    {
        isHandle1Dragging = false;
        isHandle2Dragging = false;
        isHandle3Dragging = false;
        isHandle5Dragging = false;
        isHandle6Dragging = false;
        isHandle7Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateBand4 = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[3];
        activeBand = 3;

        // Calculate the new position of handle 4
        float newXValue4 = juce::jlimit(0.0f, static_cast<float>(w), event.position.x - dragOffsetXHandle4);
        float newYValue4 = juce::jlimit(0.0f, static_cast<float>(h), event.position.y - dragOffsetYHandle4);

        // Update handleX4 and handleY4 with the new position for handle 4
        handleX4 = newXValue4;
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band4Type")->load()))
            handleY4 = newYValue4;
        else
            handleY4 = dbToY (0.0f, static_cast<float> (h));

        // Calculate newBand4Gain based on the Y position (display-range aware)
        float newBand4Gain = 0.0f;
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue4, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter ("band4Gain")))
                newBand4Gain = paramGain->convertTo0to1 (gainDb);
            else
                newBand4Gain = juce::jmap (gainDb, -24.0f, 24.0f, 0.0f, 1.0f);
        }

        // Constants for logarithmic mapping
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20000 Hz

        // Calculate newBand4Frequency based on the X position
        float newBand4Frequency = std::pow(10.0f, logMin + (logMax - logMin) * handleX4 / static_cast<float>(w - 1));

        // Set the new band4Gain value (ignored for notch / band-pass)
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("band4Type")->load()))
            processor.treeState.getParameter("band4Gain")->setValueNotifyingHost(newBand4Gain);

        float fullScaleBand4Gain = juce::jmap(newBand4Gain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[3] = fullScaleBand4Gain;

        // Set the new band4Frequency value
        auto* paramFrequency4 = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("band4Frequency"));
        if (paramFrequency4)
        {
            float normalizedBand4Frequency = paramFrequency4->convertTo0to1(newBand4Frequency);
            processor.treeState.getParameter("band4Frequency")->setValueNotifyingHost(normalizedBand4Frequency);
            arrayCurrentBandFrequency[3] = newBand4Frequency;
        }

        float currentBand4Q = processor.treeState.getParameter("band4Q")->getValue();
        float fullScaleBand4Q = juce::jmap(currentBand4Q, 0.0f, 1.0f, 0.15f, 10.0f);
        arrayCurrentBandQ[3] = fullScaleBand4Q;

        needsUpdateBand4 = true;
        needsUpdateCombined = true;
    }

    else if (isHandle5Dragging)  // Check if handle 5 is dragging
    {
        isHandle1Dragging = false;
        isHandle2Dragging = false;
        isHandle3Dragging = false;
        isHandle4Dragging = false;
        isHandle6Dragging = false;
        isHandle7Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateHighpass = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[4];
        activeBand = 4;

        float newXValue5 = juce::jlimit (0.0f, static_cast<float> (w), event.position.x - dragOffsetXHandle5);
        float newYValue5 = juce::jlimit (0.0f, static_cast<float> (h), event.position.y - dragOffsetYHandle5);
        handleX5 = newXValue5;

        const int hpType = BandChannel::readChoiceIndex (processor.treeState, "highpassType", FilterType::highpass);
        if (FilterType::usesGain (hpType))
            handleY5 = newYValue5;
        else
            handleY5 = dbToY (0.0f, static_cast<float> (h));

        double logMin = std::log10 (20);
        double logMax = std::log10 (20000);
        float newHighpassCutoff = std::pow (10.0f, logMin + (logMax - logMin) * handleX5 / static_cast<float> (w - 1));

        if (auto* paramFrequency5 = dynamic_cast<juce::RangedAudioParameter*> (
                processor.treeState.getParameter ("highpassCutoff")))
        {
            processor.treeState.getParameter ("highpassCutoff")->setValueNotifyingHost (
                paramFrequency5->convertTo0to1 (newHighpassCutoff));
            arrayCurrentBandFrequency[4] = newHighpassCutoff;
        }

        if (FilterType::usesGain (hpType))
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue5, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (
                    processor.treeState.getParameter ("highpassGain")))
                processor.treeState.getParameter ("highpassGain")->setValueNotifyingHost (
                    paramGain->convertTo0to1 (gainDb));
            arrayCurrentBandGain[4] = gainDb;
        }
        else
        {
            arrayCurrentBandGain[4] = 0.0f;
        }

        float currentHighpassQ = processor.treeState.getParameter ("highpassQ")->getValue();
        arrayCurrentBandQ[4] = juce::jmap (currentHighpassQ, 0.0f, 1.0f, 0.15f, 10.0f);

        needsUpdateHighpass = true;
        needsUpdateCombined = true;
    }



    else if (isHandle6Dragging)  // Check if handle 6 is dragging
    {
        isHandle1Dragging = false;
        isHandle2Dragging = false;
        isHandle3Dragging = false;
        isHandle4Dragging = false;
        isHandle5Dragging = false;
        isHandle7Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateLowpass = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[5];
        activeBand = 5;

        float newXValue6 = juce::jlimit (0.0f, static_cast<float> (w), event.position.x - dragOffsetXHandle6);
        float newYValue6 = juce::jlimit (0.0f, static_cast<float> (h), event.position.y - dragOffsetYHandle6);
        handleX6 = newXValue6;

        const int lpType = BandChannel::readChoiceIndex (processor.treeState, "lowpassType", FilterType::lowpass);
        if (FilterType::usesGain (lpType))
            handleY6 = newYValue6;
        else
            handleY6 = dbToY (0.0f, static_cast<float> (h));

        double logMin = std::log10 (20);
        double logMax = std::log10 (20000);
        float newLowpassCutoff = std::pow (10.0f, logMin + (logMax - logMin) * handleX6 / static_cast<float> (w - 1));

        if (auto* paramFrequency6 = dynamic_cast<juce::RangedAudioParameter*> (
                processor.treeState.getParameter ("lowpassCutoff")))
        {
            processor.treeState.getParameter ("lowpassCutoff")->setValueNotifyingHost (
                paramFrequency6->convertTo0to1 (newLowpassCutoff));
            arrayCurrentBandFrequency[5] = newLowpassCutoff;
        }

        if (FilterType::usesGain (lpType))
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue6, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (
                    processor.treeState.getParameter ("lowpassGain")))
                processor.treeState.getParameter ("lowpassGain")->setValueNotifyingHost (
                    paramGain->convertTo0to1 (gainDb));
            arrayCurrentBandGain[5] = gainDb;
        }
        else
        {
            arrayCurrentBandGain[5] = 0.0f;
        }

        float currentLowpassQ = processor.treeState.getParameter("lowpassQ")->getValue();
        float fullScaleLowpassQ = juce::jmap(currentLowpassQ, 0.0f, 1.0f, 0.15f, 10.0f);  // The full-scale range

        // Update the array with the full-scale Q value
        arrayCurrentBandQ[5] = fullScaleLowpassQ;

        needsUpdateLowpass = true;
        needsUpdateCombined = true;
    }



    else if (isHandle7Dragging) // Check if handle 7 is dragging
    {
        // Reset the dragging states for all other handles
        isHandle1Dragging = false;
        isHandle2Dragging = false;
        isHandle3Dragging = false;
        isHandle4Dragging = false;
        isHandle5Dragging = false;
        isHandle6Dragging = false;
        isHandle8Dragging = false;
        anyHandleDragging = true;
        needsUpdateHighShelf = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[6];
        activeBand = 6;

        // Calculate the new position of handle 7
        float newXValue7 = juce::jlimit(0.0f, static_cast<float>(w), event.position.x - dragOffsetXHandle7);
        float newYValue7 = juce::jlimit(0.0f, static_cast<float>(h), event.position.y - dragOffsetYHandle7);

        // Update handleX7 and handleY7
        handleX7 = newXValue7;
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("highShelfType")->load()))
            handleY7 = newYValue7;
        else
            handleY7 = dbToY (0.0f, static_cast<float> (h));

        // Calculate newHighShelfGain based on the Y position (display-range aware)
        float newHighShelfGain = 0.0f;
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue7, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter ("highShelfGain")))
                newHighShelfGain = paramGain->convertTo0to1 (gainDb);
            else
                newHighShelfGain = juce::jmap (gainDb, -24.0f, 24.0f, 0.0f, 1.0f);
        }

        // Constants for logarithmic mapping
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20,000 Hz

        // Calculate newHighShelfFrequency based on the X position
        float newHighShelfFrequency = std::pow(10.0f, logMin + (logMax - logMin) * handleX7 / static_cast<float>(w - 1));

        // Set the new highShelfGain value (ignored for notch / band-pass)
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("highShelfType")->load()))
            processor.treeState.getParameter("highShelfGain")->setValueNotifyingHost(newHighShelfGain);

        float fullScaleHighShelfGain = juce::jmap(newHighShelfGain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[6] = fullScaleHighShelfGain;

        // Set the new highShelfFrequency value
        auto* paramFrequency7 = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("highShelfFrequency"));
        if (paramFrequency7)
        {
            float normalizedHighShelfFrequency = paramFrequency7->convertTo0to1(newHighShelfFrequency);
            processor.treeState.getParameter("highShelfFrequency")->setValueNotifyingHost(normalizedHighShelfFrequency);

            arrayCurrentBandFrequency[6] = newHighShelfFrequency;
        }

        float currentHighShelfQ = processor.treeState.getParameter("highShelfQ")->getValue();
        float fullScaleHighShelfQ = juce::jmap(currentHighShelfQ, 0.0f, 1.0f, 0.36f, 0.8f);  // The full-scale range

        // Update the array with the full-scale Q value
        arrayCurrentBandQ[6] = fullScaleHighShelfQ;

        needsUpdateHighShelf = true;
        needsUpdateCombined = true;
    }


    else if (isHandle8Dragging) // Check if handle 8 is dragging
    {
        // Reset the dragging states for all other handles
        isHandle1Dragging = false;
        isHandle2Dragging = false;
        isHandle3Dragging = false;
        isHandle4Dragging = false;
        isHandle5Dragging = false;
        isHandle6Dragging = false;
        isHandle7Dragging = false;
        anyHandleDragging = true;
        needsUpdateLowShelf = true;
        needsUpdateCombined = true;
        currentBandName = arrayBandName[7];
        activeBand = 7;

        // Calculate the new position of handle 8
        float newXValue8 = juce::jlimit(0.0f, static_cast<float>(w), event.position.x - dragOffsetXHandle8);
        float newYValue8 = juce::jlimit(0.0f, static_cast<float>(h), event.position.y - dragOffsetYHandle8);

        // Update handleX8 and handleY8
        handleX8 = newXValue8;
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("lowShelfType")->load()))
            handleY8 = newYValue8;
        else
            handleY8 = dbToY (0.0f, static_cast<float> (h));

        // Calculate newLowShelfGain based on the Y position (display-range aware)
        float newLowShelfGain = 0.0f;
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newYValue8, static_cast<float> (h)));
            if (auto* paramGain = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter ("lowShelfGain")))
                newLowShelfGain = paramGain->convertTo0to1 (gainDb);
            else
                newLowShelfGain = juce::jmap (gainDb, -24.0f, 24.0f, 0.0f, 1.0f);
        }

        // Constants for logarithmic mapping
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20,000 Hz

        // Calculate newLowShelfFrequency based on the X position
        float newLowShelfFrequency = std::pow(10.0f, logMin + (logMax - logMin) * handleX8 / static_cast<float>(w - 1));

        // Set the new lowShelfGain value (ignored for notch / band-pass)
        if (FilterType::usesGain ((int) processor.treeState.getRawParameterValue ("lowShelfType")->load()))
            processor.treeState.getParameter("lowShelfGain")->setValueNotifyingHost(newLowShelfGain);

        float fullScaleLowShelfGain = juce::jmap(newLowShelfGain, 0.0f, 1.0f, -24.0f, 24.0f);
        arrayCurrentBandGain[7] = fullScaleLowShelfGain;

        // Set the new lowShelfFrequency value
        auto* paramFrequency8 = dynamic_cast<juce::RangedAudioParameter*>(processor.treeState.getParameter("lowShelfFrequency"));
        if (paramFrequency8)
        {
            float normalizedLowShelfFrequency = paramFrequency8->convertTo0to1(newLowShelfFrequency);
            processor.treeState.getParameter("lowShelfFrequency")->setValueNotifyingHost(normalizedLowShelfFrequency);

            arrayCurrentBandFrequency[7] = newLowShelfFrequency;
        }

        float currentLowShelfQ = processor.treeState.getParameter("lowShelfQ")->getValue();
        float fullScaleLowShelfQ = juce::jmap(currentLowShelfQ, 0.0f, 1.0f, 0.36f, 0.8f);  // The full-scale range

        // Update the array with the full-scale Q value
        arrayCurrentBandQ[7] = fullScaleLowShelfQ;

        needsUpdateLowShelf = true;
        needsUpdateCombined = true;
    }

    repaint();
}



    //=======================================================================================================//
void FrequencyResponseComponent::mouseUp(const juce::MouseEvent& event)
{
    juce::ignoreUnused(event);

    if (isHandle1Dragging)
    {
        processor.treeState.getParameter("band1Frequency")->endChangeGesture();
        processor.treeState.getParameter("band1Gain")->endChangeGesture();
        processor.treeState.getParameter("band1Q")->endChangeGesture();
    }
    else if (isHandle2Dragging)
    {
        processor.treeState.getParameter("band2Frequency")->endChangeGesture();
        processor.treeState.getParameter("band2Gain")->endChangeGesture();
        processor.treeState.getParameter("band2Q")->endChangeGesture();
    }
    else if (isHandle3Dragging)
    {
        processor.treeState.getParameter("band3Frequency")->endChangeGesture();
        processor.treeState.getParameter("band3Gain")->endChangeGesture();
        processor.treeState.getParameter("band3Q")->endChangeGesture();
    }
    else if (isHandle4Dragging)
    {
        processor.treeState.getParameter("band4Frequency")->endChangeGesture();
        processor.treeState.getParameter("band4Gain")->endChangeGesture();
        processor.treeState.getParameter("band4Q")->endChangeGesture();
    }
    else if (isHandle5Dragging)
    {
        processor.treeState.getParameter("highpassCutoff")->endChangeGesture();
        processor.treeState.getParameter("highpassQ")->endChangeGesture();
    }
    else if (isHandle6Dragging)
    {
        processor.treeState.getParameter("lowpassCutoff")->endChangeGesture();
        processor.treeState.getParameter("lowpassQ")->endChangeGesture();
    }
    else if (isHandle7Dragging)
    {
        processor.treeState.getParameter("highShelfFrequency")->endChangeGesture();
        processor.treeState.getParameter("highShelfGain")->endChangeGesture();
        processor.treeState.getParameter("highShelfQ")->endChangeGesture();
    }
    else if (isHandle8Dragging)
    {
        processor.treeState.getParameter("lowShelfFrequency")->endChangeGesture();
        processor.treeState.getParameter("lowShelfGain")->endChangeGesture();
        processor.treeState.getParameter("lowShelfQ")->endChangeGesture();
    }

    const bool wasDragging = anyHandleDragging
        || isHandle1Dragging || isHandle2Dragging || isHandle3Dragging || isHandle4Dragging
        || isHandle5Dragging || isHandle6Dragging || isHandle7Dragging || isHandle8Dragging;

    isHandle1Dragging = false;
    isHandle2Dragging = false;
    isHandle3Dragging = false;
    isHandle4Dragging = false;
    isHandle5Dragging = false;
    isHandle6Dragging = false;
    isHandle7Dragging = false;
    isHandle8Dragging = false;
    anyHandleDragging = false;

    if (wasDragging)
    {
        setOptionBoxInteractionFaded (false);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (-1);
    }

    if (! wasDragging)
        return;

    // Final snap to released parameter values, then freeze — no further curve animation.
    needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
    needsUpdateHighpass = needsUpdateLowpass = needsUpdateHighShelf = needsUpdateLowShelf = true;
    needsUpdateCombined = true;
    repaint();
}
  
    
    void FrequencyResponseComponent::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
    {
        // Calculate the distance to the handle
        float distanceToHandle = std::hypot(event.position.x - handleX, event.position.y - handleY);
        float distanceToHandle2 = std::hypot(event.position.x - handleX2, event.position.y - handleY2);
        float distanceToHandle3 = std::hypot(event.position.x - handleX3, event.position.y - handleY3);
        float distanceToHandle4 = std::hypot(event.position.x - handleX4, event.position.y - handleY4);
        float distanceToHandle5 = std::hypot(event.position.x - handleX5, event.position.y - handleY5);
        float distanceToHandle6 = std::hypot(event.position.x - handleX6, event.position.y - handleY6);
        float distanceToHandle7 = std::hypot(event.position.x - handleX7, event.position.y - handleY7);
        float distanceToHandle8 = std::hypot(event.position.x - handleX8, event.position.y - handleY8);


        // Define a threshold for the distance check
        float clickThreshold = 10.0f;
        if (wheel.deltaY == 0.0f)
            return;

        // Any band typed HP/LP: wheel cycles that band's slope. Otherwise adjust Q.
        auto wheelOnBand = [&] (bool overHandle, int bandIndex, const juce::String& typeId,
                                int typeFallback, const juce::String& qId, int qArrayIndex,
                                bool& needsUpdateFlag)
        {
            if (! overHandle)
                return;

            const int type = BandChannel::readChoiceIndex (parameters, typeId, typeFallback);
            needsUpdateFlag = true;
            needsUpdateCombined = true;

            if (FilterType::isHpLp (type))
            {
                const auto slopeId = FilterSlope::paramIDForBandIndex (bandIndex);
                if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                        parameters.getParameter (slopeId)))
                {
                    const int delta = (wheel.deltaY > 0.0f) ? 1 : -1;
                    const int newIndex = juce::jlimit (0, FilterSlope::numChoices - 1,
                                                       choice->getIndex() + delta);
                    if (newIndex != choice->getIndex())
                    {
                        choice->beginChangeGesture();
                        choice->setValueNotifyingHost (choice->convertTo0to1 ((float) newIndex));
                        choice->endChangeGesture();
                    }
                }
                return;
            }

            if (auto* qParam = processor.treeState.getParameter (qId))
            {
                const float sensitivity = 0.1f;
                const float newQ = juce::NormalisableRange<float> (0.15f, 10.0f, 0.01f, 0.25f)
                                       .snapToLegalValue (qParam->getValue() + wheel.deltaY * sensitivity);
                qParam->setValueNotifyingHost (newQ);
                arrayCurrentBandQ[(size_t) qArrayIndex] = newQ;
            }
        };

        wheelOnBand (distanceToHandle  <= clickThreshold, 0, "band1Type",     FilterType::bell,      "band1Q",     0, needsUpdateBand1);
        wheelOnBand (distanceToHandle2 <= clickThreshold, 1, "band2Type",     FilterType::bell,      "band2Q",     1, needsUpdateBand2);
        wheelOnBand (distanceToHandle3 <= clickThreshold, 2, "band3Type",     FilterType::bell,      "band3Q",     2, needsUpdateBand3);
        wheelOnBand (distanceToHandle4 <= clickThreshold, 3, "band4Type",     FilterType::bell,      "band4Q",     3, needsUpdateBand4);
        wheelOnBand (distanceToHandle5 <= clickThreshold, 4, "highpassType",  FilterType::highpass,  "highpassQ",  4, needsUpdateHighpass);
        wheelOnBand (distanceToHandle6 <= clickThreshold, 5, "lowpassType",   FilterType::lowpass,   "lowpassQ",   5, needsUpdateLowpass);
        wheelOnBand (distanceToHandle7 <= clickThreshold, 6, "highShelfType", FilterType::highShelf, "highShelfQ", 6, needsUpdateHighShelf);
        wheelOnBand (distanceToHandle8 <= clickThreshold, 7, "lowShelfType",  FilterType::lowShelf,  "lowShelfQ",  7, needsUpdateLowShelf);
    }





    void FrequencyResponseComponent::updateBand1()
    {
        shouldRedrawBand1 = true;
        repaint();
    }

    void FrequencyResponseComponent::updateBand2()
    {
        shouldRedrawBand2 = true;
        repaint();
    }

    void FrequencyResponseComponent::updateBand3()
    {
        shouldRedrawBand3 = true;
        repaint();
    }

    void FrequencyResponseComponent::updateBand4()
    {
        shouldRedrawBand4 = true;
        repaint();
    }

    void FrequencyResponseComponent::updateHighpass()
    {
        shouldRedrawHighpass = true;
        repaint();
    }

    void FrequencyResponseComponent::updateLowpass()
    {
        shouldRedrawLowpass = true;
        repaint();
    }

    void FrequencyResponseComponent::updateHighShelf()
    {
        shouldRedrawHighShelf = true;
        repaint();
    }

    void FrequencyResponseComponent::updateLowShelf()
    {
        shouldRedrawLowShelf = true;
        repaint();
    }


void FrequencyResponseComponent::resized()
{
    precomputeLogFrequencies();

    needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
    needsUpdateHighpass = needsUpdateLowpass = needsUpdateHighShelf = needsUpdateLowShelf = true;
    needsUpdateCombined = true;

    // Keep handle caches (and a placed OptionBox) proportional to graph size.
    // Do not snap OptionBox back to the handle — user placement sticks until another band is selected.
    if (previousGraphWidth > 0 && previousGraphHeight > 0 && getWidth() > 0 && getHeight() > 0)
    {
        const float sx = (float) getWidth() / (float) previousGraphWidth;
        const float sy = (float) getHeight() / (float) previousGraphHeight;

        auto scaleHandle = [sx, sy] (float& x, float& y)
        {
            x *= sx;
            y *= sy;
        };

        scaleHandle (handleX, handleY);
        scaleHandle (handleX2, handleY2);
        scaleHandle (handleX3, handleY3);
        scaleHandle (handleX4, handleY4);
        scaleHandle (handleX5, handleY5);
        scaleHandle (handleX6, handleY6);
        scaleHandle (handleX7, handleY7);
        scaleHandle (handleX8, handleY8);

        if (optionBoxMenu != nullptr && optionBoxMenu->isVisible())
            optionBoxMenu->setTopLeftPosition (juce::roundToInt ((float) optionBoxMenu->getX() * sx),
                                               juce::roundToInt ((float) optionBoxMenu->getY() * sy));
    }

    previousGraphWidth = getWidth();
    previousGraphHeight = getHeight();

    if (optionBoxMenu != nullptr)
        optionBoxMenu->updateUiScaleFromParent();

    // Minimize / expand — top-left on the graph so it stays usable when the faceplate is hidden.
    {
        constexpr int btn = 22;
        constexpr int margin = 6;
        uiModeButton.setBounds (margin, margin, btn, btn);
        uiModeButton.toFront (false);
    }

    // Proportional Q — bottom-left (same size vibe as Range +/- buttons).
    {
        constexpr int btnW = 22;
        constexpr int btnH = 18;
        constexpr int marginLeft = 18;
        constexpr int marginBottom = 18;
        auto area = getLocalBounds();
        area.removeFromLeft (marginLeft);
        area.removeFromBottom (marginBottom);
        proportionalQButton.setBounds (area.removeFromBottom (btnH).removeFromLeft (btnW));
        proportionalQButton.toFront (false);
    }

    // Vertical scale controls — bottom-right (intercept mouse so +/- work over the graph)
    {
        constexpr int btnW = 22;
        constexpr int btnH = 18;
        constexpr int gap = 4;
        constexpr int marginRight = 18;
        constexpr int marginBottom = 18;
        constexpr int outGainW = 58;
        constexpr int autoGainW = 22;
        constexpr int rangeLabelW = 40; // "Range" caption
        constexpr int modBtnW = 34;
        auto area = getLocalBounds();
        area.removeFromRight (marginRight);
        area.removeFromBottom (marginBottom);
        auto row = area.removeFromBottom (btnH);

        if (outputGainScrubber.isVisible())
        {
            outputGainScrubber.setBounds (row.removeFromRight (outGainW));
            row.removeFromRight (gap);
            outputGainScrubber.toFront (false);
        }

        if (autoGainButton.isVisible())
        {
            autoGainButton.setBounds (row.removeFromRight (autoGainW));
            row.removeFromRight (gap);
            autoGainButton.toFront (false);
        }

        eqRangePlusButton.setBounds (row.removeFromRight (btnW));
        row.removeFromRight (gap);
        eqRangeMinusButton.setBounds (row.removeFromRight (btnW));
        row.removeFromRight (gap);
        eqRangeLabel.setBounds (row.removeFromRight (rangeLabelW));
        row.removeFromRight (gap);
        modButton.setBounds (row.removeFromRight (modBtnW));
        eqRangePlusButton.toFront (false);
        eqRangeMinusButton.toFront (false);
        eqRangeLabel.toFront (false);
        modButton.toFront (false);
    }

    repaint();
}

bool FrequencyResponseComponent::anyActiveDynamicEq() const
{
    // Read APVTS directly — processor on/off flags only update in processBlock.
    auto on = [this] (const char* id) -> bool
    {
        if (auto* v = parameters.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };

    const bool anyDyn = (on ("band1OnOff") && on ("band1Dynamic"))
        || (on ("band2OnOff") && on ("band2Dynamic"))
        || (on ("band3OnOff") && on ("band3Dynamic"))
        || (on ("band4OnOff") && on ("band4Dynamic"))
        || (on ("highShelfOnOff") && on ("highShelfDynamic"))
        || (on ("lowShelfOnOff") && on ("lowShelfDynamic"));

    const bool anySc = (on ("band1OnOff") && on ("band1Sidechain"))
        || (on ("band2OnOff") && on ("band2Sidechain"))
        || (on ("band3OnOff") && on ("band3Sidechain"))
        || (on ("band4OnOff") && on ("band4Sidechain"))
        || (on ("highShelfOnOff") && on ("highShelfSidechain"))
        || (on ("lowShelfOnOff") && on ("lowShelfSidechain"));

    const bool anySpec = (on ("band1OnOff") && on ("band1Spectral"))
        || (on ("band2OnOff") && on ("band2Spectral"))
        || (on ("band3OnOff") && on ("band3Spectral"))
        || (on ("band4OnOff") && on ("band4Spectral"))
        || (on ("highShelfOnOff") && on ("highShelfSpectral"))
        || (on ("lowShelfOnOff") && on ("lowShelfSpectral"));

    const bool sideCheckOn = on (SideCheck::enabledParamId());

    const bool anyLfo = LfoMod::anyActiveRouting (parameters);

    return anyDyn || anySc || anySpec || sideCheckOn || anyLfo;
}

void FrequencyResponseComponent::markActiveDynamicBandsDirty()
{
    auto on = [this] (const char* id) -> bool
    {
        if (auto* v = parameters.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };

    const bool anyLfo = LfoMod::anyActiveRouting (parameters);

    // Dynamic / sidechain change the per-band IIR shape (effective gain + LFO F/Q).
    // Spectral GR is sum-only, so spectral animation only needs needsUpdateCombined.
    auto liveBand = [&] (const char* onId, const char* dynId, const char* scId) -> bool
    {
        return on (onId) && (on (dynId) || on (scId) || anyLfo);
    };

    if (liveBand ("band1OnOff", "band1Dynamic", "band1Sidechain"))
        needsUpdateBand1 = true;
    if (liveBand ("band2OnOff", "band2Dynamic", "band2Sidechain"))
        needsUpdateBand2 = true;
    if (liveBand ("band3OnOff", "band3Dynamic", "band3Sidechain"))
        needsUpdateBand3 = true;
    if (liveBand ("band4OnOff", "band4Dynamic", "band4Sidechain"))
        needsUpdateBand4 = true;
    if (liveBand ("highShelfOnOff", "highShelfDynamic", "highShelfSidechain"))
        needsUpdateHighShelf = true;
    if (liveBand ("lowShelfOnOff", "lowShelfDynamic", "lowShelfSidechain"))
        needsUpdateLowShelf = true;

    needsUpdateCombined = true;
}

void FrequencyResponseComponent::syncDynamicCurveTimer()
{
    if (anyActiveDynamicEq())
    {
        // Buffered paint can blit a stale image even after repaint() while D is moving
        // the curve every block — disable caching for the duration of dynamic animation.
        setBufferedToImage (false);

        if (! isTimerRunning())
            startTimerHz (45);

        // Immediate kick so enabling D doesn't wait a timer period.
        markActiveDynamicBandsDirty();
        repaint();
    }
    else
    {
        if (isTimerRunning())
            stopTimer();

        setBufferedToImage (true);

        // Force a fresh comparison the next time D is enabled.
        lastDynCurveGain1 = lastDynCurveGain2 = lastDynCurveGain3 = lastDynCurveGain4 = 1.0e9f;
        lastDynCurveGainHS = lastDynCurveGainLS = 1.0e9f;
    }
}

void FrequencyResponseComponent::timerCallback()
{
    if (! anyActiveDynamicEq())
    {
        stopTimer();
        setBufferedToImage (true);
        lastDynCurveGain1 = lastDynCurveGain2 = lastDynCurveGain3 = lastDynCurveGain4 = 1.0e9f;
        lastDynCurveGainHS = lastDynCurveGainLS = 1.0e9f;
        return;
    }

    // Every tick while D+On: force magnitude rebuild from published effective gains, then repaint.
    // No gain-delta threshold — small GR changes must still redraw.
    markActiveDynamicBandsDirty();
    repaint();
}

void FrequencyResponseComponent::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused(newValue);

    // While a handle is being dragged, mouseDrag owns curve updates.
    // Ignoring these callbacks prevents post-release attachment/smooth echoes from re-animating the curve.
    // Dynamic/spectral on/off / band on/off / Side Check still need the animation timer synced.
    const bool isDynToggle = parameterID.endsWith ("Dynamic")
                          || parameterID.endsWith ("Spectral")
                          || parameterID.endsWith ("SpectralExpand")
                          || parameterID.endsWith ("Sidechain")
                          || parameterID.startsWith ("lfo")
                          || parameterID.startsWith ("modSlot")
                          || parameterID.startsWith ("modEnv");
    const bool isBandOnOff = parameterID.endsWith ("OnOff");
    const bool isSideCheckToggle = parameterID == SideCheck::enabledParamId();
    if (isDynToggle || isBandOnOff || isSideCheckToggle)
        syncDynamicCurveTimer();

    if (anyHandleDragging)
        return;

    if (parameterID == "EQ_DISPLAY_RANGE_ID")
    {
        needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
        needsUpdateHighpass = needsUpdateLowpass = needsUpdateHighShelf = needsUpdateLowShelf = true;
        needsUpdateCombined = true;
        syncEqRangeControls();
        repaint();
        return;
    }

    if (parameterID == "PROPORTIONAL_Q_ID")
    {
        // Peaking shape depends on effective Q when P is on — rebuild all tunable bands.
        needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
        needsUpdateCombined = true;
        repaint();
        return;
    }

    if (parameterID == "EQ_BAND_PATH_WIDTH_ID" || parameterID == "EQ_SUM_PATH_WIDTH_ID"
        || parameterID == "EQ_SUM_GLOW_ENABLE_ID"
        || parameterID == "EQ_SUM_GLOW_RADIUS_ID" || parameterID == "EQ_SUM_GLOW_SPREAD_ID"
        || parameterID == "EQ_SUM_GLOW_OPACITY_ID"
        || parameterID == "EQ_MULTICOLOR_BAND_FILL_ID"
        || parameterID == "EQ_SHOW_CROSSHAIR_ID")
    {
        repaint();
        return;
    }

    // Spectral / Side Check GR affects only the cumulative curve; do not dirty per-band IIR shapes.
    const bool isSpectralDisplayParam = parameterID.endsWith ("Spectral")
                                     || parameterID.endsWith ("SpectralDepth")
                                     || parameterID.endsWith ("SpectralResHz")
                                     || parameterID.endsWith ("SpectralExpand")
                                     || parameterID == SpectralDynamics::spectralResHzParamId()
                                     || parameterID == SpectralDynamics::spectralPackParamId();
    const bool isSideCheckDisplayParam = parameterID == SideCheck::enabledParamId()
                                      || parameterID == SideCheck::amountParamId()
                                      || parameterID == SideCheck::hpHzParamId()
                                      || parameterID == SideCheck::lpHzParamId()
                                      || parameterID == SideCheck::modeParamId();

    if (parameterID == "band1Gain" || parameterID == "band1Frequency" || parameterID == "band1Q"
        || parameterID == "band1OnOff" || parameterID == "band1Type" || parameterID == "band1Slope"
        || parameterID == "band1Dynamic" || parameterID == "band1DynThreshold")
        needsUpdateBand1 = true;
    else if (parameterID == "band2Gain" || parameterID == "band2Frequency" || parameterID == "band2Q"
             || parameterID == "band2OnOff" || parameterID == "band2Type" || parameterID == "band2Slope"
             || parameterID == "band2Dynamic" || parameterID == "band2DynThreshold")
        needsUpdateBand2 = true;
    else if (parameterID == "band3Gain" || parameterID == "band3Frequency" || parameterID == "band3Q"
             || parameterID == "band3OnOff" || parameterID == "band3Type" || parameterID == "band3Slope"
             || parameterID == "band3Dynamic" || parameterID == "band3DynThreshold")
        needsUpdateBand3 = true;
    else if (parameterID == "band4Gain" || parameterID == "band4Frequency" || parameterID == "band4Q"
             || parameterID == "band4OnOff" || parameterID == "band4Type" || parameterID == "band4Slope"
             || parameterID == "band4Dynamic" || parameterID == "band4DynThreshold")
        needsUpdateBand4 = true;
    else if (parameterID == "highpassCutoff" || parameterID == "highpassQ" || parameterID == "highpassGain"
             || parameterID == "highpassOnOff" || parameterID == "highpassType" || parameterID == "highpassSlope")
        needsUpdateHighpass = true;
    else if (parameterID == "lowpassCutoff" || parameterID == "lowpassQ" || parameterID == "lowpassGain"
             || parameterID == "lowpassOnOff" || parameterID == "lowpassType" || parameterID == "lowpassSlope")
        needsUpdateLowpass = true;
    else if (parameterID == "highShelfGain" || parameterID == "highShelfFrequency" || parameterID == "highShelfQ"
             || parameterID == "highShelfOnOff" || parameterID == "highShelfType" || parameterID == "highShelfSlope"
             || parameterID == "highShelfDynamic" || parameterID == "highShelfDynThreshold")
        needsUpdateHighShelf = true;
    else if (parameterID == "lowShelfGain" || parameterID == "lowShelfFrequency" || parameterID == "lowShelfQ"
             || parameterID == "lowShelfOnOff" || parameterID == "lowShelfType" || parameterID == "lowShelfSlope"
             || parameterID == "lowShelfDynamic" || parameterID == "lowShelfDynThreshold")
        needsUpdateLowShelf = true;
    else if (! isSpectralDisplayParam && ! isSideCheckDisplayParam)
        return;

    needsUpdateCombined = true;
    repaint();
}



juce::Path FrequencyResponseComponent::intelligentDownsample(
    const juce::Path& originalPath,
    const std::vector<float>& compositeResponse,
    int w, int h)
{
    // Hardcoded adaptiveThreshold and decimationFactor
    float adaptiveThreshold = 0.2f;
    int decimationFactor = 1;

    juce::Path simplifiedPath;
    if (compositeResponse.empty()) {
        return simplifiedPath;
    }

    simplifiedPath.startNewSubPath(0, h / 2);

    float lastX = 0;
    float lastY = dbToY(compositeResponse[0], float(h));

    float movingAverageY = lastY;

    for (int i = 5; i < w; i += decimationFactor) {
        float dB = compositeResponse[i];
        float newY = dbToY(dB, float(h));

        // Use a simple moving average for this example
        movingAverageY = 0.8f * movingAverageY + 0.2f * newY;

        float error = std::abs(movingAverageY - newY);

        if (error > adaptiveThreshold) {
            simplifiedPath.lineTo(i, newY);
            lastX = i;
            lastY = newY;

            // Reset moving average
            movingAverageY = newY;
        }
    }

    // Complete the path to the right edge
    simplifiedPath.lineTo(w, h / 2);


    return simplifiedPath;
}



juce::Path FrequencyResponseComponent::intelligentDownsampleToBottom(
    const juce::Path& originalPath,
    const std::vector<float>& compositeResponse,
    int w, int h)
{
    juce::ignoreUnused (originalPath);

    juce::Path simplifiedPath;
    if (compositeResponse.empty() || w <= 0)
        return simplifiedPath;

    // One float vertex per display pixel — same x-grid as magnitude buffers.
    // Earlier adaptive path (decimationFactor=3 + flat-sample skip) made the sum jaggy;
    // hold-filled magnitude (step=2) made per-pixel tracing look stair-stepped on slopes.
    simplifiedPath.startNewSubPath (0.0f, (float) h);

    const int numPoints = juce::jmin (w, (int) compositeResponse.size());
    for (int i = 0; i < numPoints; ++i)
        simplifiedPath.lineTo ((float) i, dbToY (compositeResponse[(size_t) i], (float) h));

    const float lastX = (float) juce::jmax (0, numPoints - 1);
    simplifiedPath.lineTo (lastX, (float) h);
    simplifiedPath.closeSubPath();

    return simplifiedPath;
}




juce::Path FrequencyResponseComponent::intelligentDownsampleLowpass(
    const juce::Path& originalPath,
    const std::vector<float>& compositeResponse,
    int w, int h)
{
    // Hardcoded adaptiveThreshold and decimationFactor
    float adaptiveThreshold = 0.2f;
    int decimationFactor = 1;

    juce::Path simplifiedPath;
    if (compositeResponse.empty()) {
        return simplifiedPath;
    }

    simplifiedPath.startNewSubPath(0, h / 2);

    float lastX = 0;
    float lastY = dbToY(compositeResponse[0], float(h));

    float movingAverageY = lastY;

    for (int i = 5; i < w; i += decimationFactor) {
        float dB = compositeResponse[i];
        float newY = dbToY(dB, float(h));

        // Use a simple moving average for this example
        movingAverageY = 0.8f * movingAverageY + 0.2f * newY;

        float error = std::abs(movingAverageY - newY);

        if (error > adaptiveThreshold) {
            simplifiedPath.lineTo(i, newY);
            lastX = i;
            lastY = newY;

            // Reset moving average
            movingAverageY = newY;
        }
    }

    simplifiedPath.lineTo(w, h);

    // Complete the path to the right edge
    simplifiedPath.lineTo(w, h / 2);


    return simplifiedPath;
}


juce::Path FrequencyResponseComponent::intelligentDownsampleHighpass(
    const juce::Path& originalPath,
    const std::vector<float>& compositeResponse,
    int w, int h)
{
    // Hardcoded adaptiveThreshold and decimationFactor
    float adaptiveThreshold = 0.2f;
    int decimationFactor = 1;

    juce::Path simplifiedPath;
    if (compositeResponse.empty()) {
        return simplifiedPath;
    }

    simplifiedPath.startNewSubPath(w, h / 2);  // Starting point at (w, h/2)

    float lastX = w;
    float lastY = dbToY(compositeResponse[w - 1], float(h));

    float movingAverageY = lastY;

    // Iterate from right to left, effectively reversing the curve
    for (int i = w - decimationFactor; i >= 5; i -= decimationFactor) {
        float dB = compositeResponse[i];
        float newY = dbToY(dB, float(h));

        // Use a simple moving average for this example
        movingAverageY = 0.8f * movingAverageY + 0.2f * newY;

        float error = std::abs(movingAverageY - newY);

        if (error > adaptiveThreshold) {
            simplifiedPath.lineTo(i, newY);
            lastX = i;
            lastY = newY;

            // Reset moving average
            movingAverageY = newY;
        }
    }

    simplifiedPath.lineTo(0, h);  // Move to (0, h)

    // Complete the path to the left edge
    simplifiedPath.lineTo(0, h / 2);  // Move to (0, h/2)

    return simplifiedPath;
}




juce::Path FrequencyResponseComponent::simpleDownsample(
    const juce::Path& originalPath,
    const std::vector<float>& compositeResponse,
    int w, int h,
    int downsampleFactor
)
{
    juce::Path simplifiedPath;

    if (compositeResponse.empty() || downsampleFactor <= 0) {
        return simplifiedPath;
    }

    // Start the path at (0, h / 2)
    simplifiedPath.startNewSubPath(0, h / 2);

    // Add points to the path
    for (int i = 0; i < w; i += downsampleFactor) {
        if (i < compositeResponse.size()) {
            float dataPoint = compositeResponse[i];
            float newY = dbToY(dataPoint, float(h));
            simplifiedPath.lineTo(i, newY);
        }
    }

    // Close the path by linking the last point to (w, h / 2) and then to the start point (0, h / 2)
    simplifiedPath.lineTo(w, h / 2);
    simplifiedPath.lineTo(0, h / 2);
    simplifiedPath.closeSubPath();

    return simplifiedPath;
}

//==============================================================================
OutputGainScrubber::OutputGainScrubber (juce::AudioProcessorValueTreeState& state, juce::UndoManager* undoMgr)
    : treeState (state), undoManager (undoMgr)
{
    setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    treeState.addParameterListener ("outputGain", this);
    refreshText();
}

OutputGainScrubber::~OutputGainScrubber()
{
    treeState.removeParameterListener ("outputGain", this);
    endGesture();
}

void OutputGainScrubber::refreshText()
{
    float db = 0.0f;
    if (auto* v = treeState.getRawParameterValue ("outputGain"))
        db = v->load();

    displayText = juce::String (db, 1) + " dB";
    repaint();
}

void OutputGainScrubber::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (parameterID, newValue);
    refreshText();
}

void OutputGainScrubber::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colour::fromRGBA (60, 50, 35, hovered || gestureActive ? 220 : 180));
    g.fillRoundedRectangle (bounds, 3.0f);
    g.setColour (juce::Colours::whitesmoke.withAlpha (hovered || gestureActive ? 0.95f : 0.8f));
    g.setFont (juce::Font (11.0f));
    g.drawText (displayText, getLocalBounds(), juce::Justification::centred, false);
}

void OutputGainScrubber::beginGesture()
{
    if (gestureActive)
        return;
    if (auto* p = treeState.getParameter ("outputGain"))
    {
        p->beginChangeGesture();
        gestureActive = true;
    }
}

void OutputGainScrubber::endGesture()
{
    if (! gestureActive)
        return;
    if (auto* p = treeState.getParameter ("outputGain"))
        p->endChangeGesture();
    gestureActive = false;
}

void OutputGainScrubber::mouseDown (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Output gain");
    dragStartDb = 0.0f;
    if (auto* v = treeState.getRawParameterValue ("outputGain"))
        dragStartDb = v->load();
    beginGesture();
}

void OutputGainScrubber::mouseDrag (const juce::MouseEvent& e)
{
    auto* param = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter ("outputGain"));
    if (param == nullptr)
        return;

    // Match JUCE Slider fine-drag: Shift slows by 10x. Also accept Alt (requested).
    const bool fine = e.mods.isShiftDown() || e.mods.isAltDown();
    const float pixelsPerDb = fine ? 40.0f : 4.0f; // 10x finer with Shift/Alt
    const float newDb = juce::jlimit (-24.0f, 24.0f,
                                      dragStartDb - (float) e.getDistanceFromDragStartY() / pixelsPerDb);

    param->setValueNotifyingHost (param->convertTo0to1 (newDb));
    refreshText();
}

void OutputGainScrubber::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    endGesture();
}

void OutputGainScrubber::mouseEnter (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    hovered = true;
    repaint();
}

void OutputGainScrubber::mouseExit (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    hovered = false;
    repaint();
}

