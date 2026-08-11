#include "EqProcessor.h"
#include "EqEditor.h"
#include "FrequencyResponseComponent.h"
#include "OptionBoxMenu.h"
#include "BinaryData.h"
#include "FilterSlope.h"
#include "FilterType.h"
#include "BandChannel.h"
#include "DynamicEq.h"
#include "Match/MatchSettings.h"
#include "BandSidechain.h"
#include "EqBand.h"
#include "LfoMod.h"
#include "ComboBoxLookAndFeel.h"
#include "MusicNote.h"
#include <JuceHeader.h>

namespace
{
    juce::Colour bandCurveColour (juce::Colour fill) noexcept
    {
        return fill.withAlpha (0.75f);
    }

    /** Expand sparse GR samples (indices 0, step, 2*step, …, last) to full width. */
    void expandSparseDb (const float* sparse, int sparseN, int step, float* dest, int width) noexcept
    {
        if (sparse == nullptr || dest == nullptr || width <= 0 || sparseN <= 0)
            return;
        step = juce::jmax (1, step);
        if (sparseN == 1 || step == 1)
        {
            for (int i = 0; i < width; ++i)
                dest[i] = sparse[juce::jmin (step == 1 ? i : i / step, sparseN - 1)];
            return;
        }
        for (int i = 0; i < width; ++i)
        {
            const int s0 = juce::jmin (i / step, sparseN - 1);
            const int s1 = juce::jmin (s0 + 1, sparseN - 1);
            const int i0 = s0 * step;
            const int i1 = juce::jmin (width - 1, (s0 + 1) * step);
            if (s0 == s1 || i1 <= i0)
            {
                dest[i] = sparse[s0];
                continue;
            }
            const float t = (float) (i - i0) / (float) (i1 - i0);
            dest[i] = sparse[s0] + (sparse[s1] - sparse[s0]) * t;
        }
    }

    bool rawBoolParam (juce::AudioProcessorValueTreeState& state, const char* id) noexcept
    {
        if (auto* v = state.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    }

    /** Map Spectral Amount (0…max) onto the EQ display dB axis (pull down = more cut). */
    float spectralAmountToDisplayDb (float amount, bool expand, float rangeDb) noexcept
    {
        const float t = juce::jlimit (0.0f, 1.0f, amount / SpectralDynamics::kMaxSpectralAmount);
        return expand ? (t * rangeDb) : (-t * rangeDb);
    }

    float displayDbToSpectralAmount (float db, bool expand, float rangeDb) noexcept
    {
        if (rangeDb <= 1.0e-3f)
            return 0.0f;
        const float t = expand ? juce::jlimit (0.0f, 1.0f, db / rangeDb)
                               : juce::jlimit (0.0f, 1.0f, -db / rangeDb);
        return t * SpectralDynamics::kMaxSpectralAmount;
    }

    /**
        Band handle disk + dual rings + centred label.
        When legible text is on: fill is nudged vs graph bg; ink is chosen per-fill
        (so cyan/purple/etc. disks each get readable black or white numbers).
    */
    void paintBandHandleChrome (juce::Graphics& g,
                                float cx, float cy,
                                float scale,
                                juce::Colour bandFill,
                                juce::Colour graphBg,
                                juce::Colour outlineBase,
                                juce::Colour labelBase,
                                const SharedColors& theme,
                                const juce::String& label,
                                float outlineThickness = 1.0f,
                                float outlineThickness2 = 1.0f)
    {
        const float handleSize = 12.0f * scale;
        const float handleOutlineSize = 12.5f * scale;
        const float innerOutlineSize = 11.0f * scale;
        const auto fill = theme.legibleHandleFill (bandFill, graphBg);
        const auto ink = theme.legibleTextOn (labelBase, fill);
        const auto outer = theme.legibleTextOn (outlineBase, graphBg);

        g.setColour (fill);
        g.fillEllipse (cx - handleSize * 0.5f, cy - handleSize * 0.5f, handleSize, handleSize);

        g.setColour (outer);
        g.drawEllipse (cx - handleOutlineSize * 0.5f, cy - handleOutlineSize * 0.5f,
                       handleOutlineSize, handleOutlineSize, outlineThickness);

        g.setColour (ink);
        g.drawEllipse (cx - innerOutlineSize * 0.5f, cy - innerOutlineSize * 0.5f,
                       innerOutlineSize, innerOutlineSize, outlineThickness2);

        if (label.isNotEmpty())
        {
            g.setColour (ink);
            g.setFont (juce::jmax (8.0f, 11.0f * scale));
            if (label.length() <= 1)
            {
                const float textOffset = 3.5f * scale;
                g.drawText (label, cx - textOffset, cy - textOffset,
                            7.0f * scale, 7.0f * scale, juce::Justification::centred, false);
            }
            else
            {
                const float tw = juce::jmax (10.0f, (float) label.length() * 6.0f * scale);
                g.drawText (label, cx - tw * 0.5f, cy - 4.0f * scale, tw, 9.0f * scale,
                            juce::Justification::centred, false);
            }
        }
    }

    /** Dynamic-mode / spectral-amount handle chrome: coloured ring + up/down range arrows. */
    void paintDynamicRangeHandleDecor (juce::Graphics& g,
                                       float cx, float cy,
                                       float handleSize,
                                       juce::Colour bandColour,
                                       float scale)
    {
        const float ringSize = handleSize + 6.0f * scale;
        const float ringRadius = ringSize * 0.5f;
        g.setColour (bandColour.brighter (0.35f).withAlpha (0.95f));
        g.drawEllipse (cx - ringRadius, cy - ringRadius, ringSize, ringSize, 2.0f * scale);

        // 2× prior arrow size; sit just outside the ring with ~2 px padding.
        const float arrowHalf = 7.2f * scale;
        const float arrowH = 10.0f * scale;
        const float gap = 2.0f * scale;

        juce::Path up;
        const float uy = cy - ringRadius - gap;
        up.addTriangle (cx, uy - arrowH,
                        cx - arrowHalf, uy,
                        cx + arrowHalf, uy);
        g.setColour (bandColour.withAlpha (0.95f));
        g.fillPath (up);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (up, juce::PathStrokeType (1.2f * scale));

        juce::Path down;
        const float dy = cy + ringRadius + gap;
        down.addTriangle (cx, dy + arrowH,
                          cx - arrowHalf, dy,
                          cx + arrowHalf, dy);
        g.setColour (bandColour.withAlpha (0.95f));
        g.fillPath (down);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.strokePath (down, juce::PathStrokeType (1.2f * scale));
    }

    int bandIndexFromDynamicParamId (const juce::String& parameterID) noexcept
    {
        if (parameterID == "band1Dynamic") return 0;
        if (parameterID == "band2Dynamic") return 1;
        if (parameterID == "band3Dynamic") return 2;
        if (parameterID == "band4Dynamic") return 3;
        if (parameterID == "highpassDynamic") return 4;
        if (parameterID == "lowpassDynamic") return 5;
        if (parameterID == "highShelfDynamic") return 6;
        if (parameterID == "lowShelfDynamic") return 7;
        // Extended: eqB09Dynamic … eqB64Dynamic
        if (parameterID.startsWith ("eqB") && parameterID.endsWith ("Dynamic"))
        {
            const auto mid = parameterID.substring (3, parameterID.length() - 8);
            const int bandNumber = mid.getIntValue(); // 9..64
            if (bandNumber >= 9 && bandNumber <= EqBand::kMaxBands)
                return bandNumber - 1; // global 8..63
        }
        return -1;
    }
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

    for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
    {
        parameters.addParameterListener (EqBand::frequencyParamIDForGlobal (global), this);
        parameters.addParameterListener (EqBand::gainParamIDForGlobal (global), this);
        parameters.addParameterListener (EqBand::qParamIDForGlobal (global), this);
        parameters.addParameterListener (EqBand::onOffParamIDForGlobal (global), this);
        parameters.addParameterListener (FilterType::paramIDForGlobal (global), this);
        parameters.addParameterListener (EqBand::slopeParamIDForGlobal (global), this);
        parameters.addParameterListener (DynamicEq::dynamicParamIDForGlobal (global), this);
    }

    juce::String optionBoxParamID = "TheActualParamIDForOptionBoxMenu";  // Replace with the actual ID
    optionBoxMenu = std::make_unique<OptionBoxMenu> (parameters, processor);
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
        if (onFaceplateBankJump)
        {
            if (index >= EqBand::kBankSize)
                onFaceplateBankJump (EqBand::bankFromGlobal (index));
            else if (index >= 0)
                onFaceplateBankJump (0);
        }
        repaint(); // grow the newly selected handle
    };
    
    addAndMakeVisible(*optionBoxMenu);
    optionBoxMenu->setVisible(false);
    optionBoxMenu->resized();

    auto styleRangeButton = [this] (juce::TextButton& button)
    {
        button.setClickingTogglesState (false);
        button.setColour (juce::TextButton::buttonColourId, colors().pluginButtonBackground);
        button.setColour (juce::TextButton::buttonOnColourId, colors().pluginButtonAccent);
        button.setColour (juce::TextButton::textColourOffId, colors().pluginButtonText.withAlpha (0.85f));
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };

    styleRangeButton (eqRangeMinusButton);
    styleRangeButton (eqRangePlusButton);
    eqRangeMinusButton.onClick = [this] { adjustEqDisplayRange (-1); };
    eqRangePlusButton.onClick = [this] { adjustEqDisplayRange (1); };
    // Plain ASCII tip: vertical EQ curve scale (not frequency octaves).
    const juce::String eqRangeTip ("EQ display range - vertical scale of the curve (+/-6, +/-12, +/-24, or +/-36 dB)");
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

    styleRangeButton (pianoDisplayButton);
    pianoDisplayButton.setClickingTogglesState (true); // styleRangeButton clears this
    pianoDisplayButton.setToggleState (false, juce::dontSendNotification);
    pianoDisplayButton.onClick = [this]
    {
        setPianoDisplayOn (pianoDisplayButton.getToggleState(), true);
    };
    addAndMakeVisible (pianoDisplayButton);

    styleRangeButton (matchButton);
    matchButton.setClickingTogglesState (true);
    matchButton.setTooltip (
        "Match - pull the signal toward the selected target curve. "
        "Turning on asks whether to disable active EQ bands first.");
    matchButton.onClick = [this]
    {
        if (matchButton.getToggleState())
            requestMatchEnable();
        else
            disableMatch();
    };
    addAndMakeVisible (matchButton);

    // Extras stay hidden until Match is enabled (see syncMatchChrome).
    styleRangeButton (matchCurveButton);
    matchCurveButton.setClickingTogglesState (false);
    matchCurveButton.setTooltip ("Match curve - factory noise slopes, capture, placement, and saved curves");
    matchCurveButton.onClick = [this] { showMatchCurveMenu(); };
    addChildComponent (matchCurveButton);

    auto attachMatchImageKnob = [this] (RotaryImageKnobForOptionBox& knob,
                                        const char* paramId,
                                        const juce::String& tip,
                                        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att,
                                        bool isFreq)
    {
        knob.setCompactNoValueBox (true);
        knob.setTooltip (tip);
        if (isFreq)
        {
            knob.setTextValueSuffix (" Hz");
            knob.setNormalisableRange (juce::NormalisableRange<double> (
                (double) MatchEq::kMinHpLpHz, (double) MatchEq::kMaxFreqHz, 1.0, 0.2));
        }
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (parameters.getParameter (paramId)))
        {
            const auto range = param->getNormalisableRange();
            knob.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start, (double) range.end, (double) range.interval,
                (double) range.skew, range.symmetricSkew));
        }
        if (themeColors != nullptr)
            knob.setThemeColors (themeColors);
        addChildComponent (knob);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            parameters, paramId, knob);
    };
    attachMatchImageKnob (matchAmountKnob, MatchEq::amountParamId(),
        "Match Amount - how strongly to pull toward the target curve", matchAmountAttachment, false);
    attachMatchImageKnob (matchHpKnob, MatchEq::hpHzParamId(),
        "HP - Match highpass (effect starts above this frequency; 0 = fully open). Right-click: slope",
        matchHpAttachment, true);
    attachMatchImageKnob (matchLpKnob, MatchEq::lpHzParamId(),
        "LP - Match lowpass (effect stops below this frequency). Right-click: slope",
        matchLpAttachment, true);
    matchHpKnob.onValueChange = [this] { enforceMatchHpLpGap (&matchHpKnob); };
    matchLpKnob.onValueChange = [this] { enforceMatchHpLpGap (&matchLpKnob); };
    matchHpKnob.onPopupMenu = [this] { showMatchHpLpSlopeMenu (true); };
    matchLpKnob.onPopupMenu = [this] { showMatchHpLpSlopeMenu (false); };

    auto setupMatchRangeLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        // Match Range caption size/weight (eqRangeLabel uses 11 pt).
        label.setFont (juce::Font (11.0f));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, colors().graphAxisText.withAlpha (0.75f));
        label.setInterceptsMouseClicks (false, false);
        addChildComponent (label);
    };
    setupMatchRangeLabel (matchAmountLabel, "AMT");
    setupMatchRangeLabel (matchHpLabel, "HP");
    setupMatchRangeLabel (matchLpLabel, "LP");

    styleRangeButton (matchFreezeButton);
    addChildComponent (matchFreezeButton);
    matchFreezeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        parameters, MatchEq::frozenParamId(), matchFreezeButton);
    matchFreezeButton.addMouseListener (this, false);

    parameters.addParameterListener (MatchEq::enabledParamId(), this);
    parameters.addParameterListener (MatchEq::curveParamId(), this);
    parameters.addParameterListener (MatchEq::frozenParamId(), this);
    processor.syncMatchFactoryTargetFromParam();
    syncMatchChrome();

    styleRangeButton (splitSoloTButton);
    styleRangeButton (splitSoloSButton);
    splitSoloTButton.setClickingTogglesState (true);
    splitSoloSButton.setClickingTogglesState (true);
    splitSoloTButton.setTooltip ("Solo Transient stream (tune Separation)");
    splitSoloSButton.setTooltip ("Solo Sustain stream (tune Separation)");
    splitSoloTButton.onClick = [this]
    {
        if (splitSoloTButton.getToggleState())
            setStructuralSplitSolo (StructuralSplit::Solo::transient);
        else
            setStructuralSplitSolo (StructuralSplit::Solo::off);
    };
    splitSoloSButton.onClick = [this]
    {
        if (splitSoloSButton.getToggleState())
            setStructuralSplitSolo (StructuralSplit::Solo::sustain);
        else
            setStructuralSplitSolo (StructuralSplit::Solo::off);
    };
    addChildComponent (splitSoloTButton);
    addChildComponent (splitSoloSButton);

    splitSeparationKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    splitSeparationKnob.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    splitSeparationKnob.setTooltip ("Separation - how aggressively material is classed as transient vs sustain");
    splitSeparationKnob.setColour (juce::Slider::rotarySliderFillColourId, colors().pluginButtonAccent);
    splitSeparationKnob.setColour (juce::Slider::rotarySliderOutlineColourId, colors().pluginButtonBackground);
    splitSeparationKnob.setColour (juce::Slider::thumbColourId, colors().pluginButtonText);
    addChildComponent (splitSeparationKnob);
    splitSeparationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        parameters, StructuralSplit::separationParamId(), splitSeparationKnob);

    for (int bi = 0; bi < 8; ++bi)
    {
        const auto id = StructuralSplit::splitModeParamIDForBandIndex (bi);
        if (id.isNotEmpty())
            parameters.addParameterListener (id, this);
    }
    parameters.addParameterListener (StructuralSplit::soloParamId(), this);

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
    eqRangeLabel.setColour (juce::Label::textColourId, colors().graphAxisText.withAlpha (0.75f));
    eqRangeLabel.setFont (juce::Font (11.0f));
    // Intercept so the Range caption can show its tooltip (same tip as - / +).
    eqRangeLabel.setInterceptsMouseClicks (true, false);
    eqRangeLabel.setTooltip ("EQ display range - vertical scale of the curve (+/-6, +/-12, +/-24, or +/-36 dB)");
    addAndMakeVisible (eqRangeLabel);
    syncEqRangeControls();

    addChildComponent (outputGainScrubber); // visible only in compact UI
    outputGainScrubber.setVisible (false);

    syncDynamicCurveTimer();
    applyThemeToChildControls();
    repaint();
}

const SharedColors& FrequencyResponseComponent::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void FrequencyResponseComponent::setDisableCumulativeCurve (bool shouldDisable) noexcept
{
    if (disableCumulativeCurve == shouldDisable)
        return;
    disableCumulativeCurve = shouldDisable;
    if (! disableCumulativeCurve)
    {
        needsUpdateCombined = true;
        needsUpdateSpectralAmount = true;
    }
    else
    {
        combinedResponsePath.clear();
    }
    syncDynamicCurveTimer();
    repaint();
}

void FrequencyResponseComponent::setParticleCurveEco (bool particlesActive, int aliveCount) noexcept
{
    aliveCount = juce::jmax (0, aliveCount);
    if (particleCurveEcoActive == particlesActive && particleCurveEcoAlive == aliveCount)
        return;

    const int prevStep = resolveMagnitudeSampleStep();
    particleCurveEcoActive = particlesActive;
    particleCurveEcoAlive = aliveCount;
    const int newStep = resolveMagnitudeSampleStep();

    if (newStep != prevStep || newStep != lastResolvedMagStep)
    {
        lastResolvedMagStep = newStep;
        // Quality changed — rebuild so step-up restores full res, step-down applies immediately.
        needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
        needsUpdateHighpass = needsUpdateLowpass = needsUpdateHighShelf = needsUpdateLowShelf = true;
        needsUpdateExtended = true;
        needsUpdateCombined = true;
        needsUpdateSpectralAmount = true;
    }

    syncDynamicCurveTimer();
}

int FrequencyResponseComponent::resolveMagnitudeSampleStep() const noexcept
{
    // Full res when idle. Particles active → mild; dense → coarser sum/IIR sampling.
    if (! particleCurveEcoActive)
        return 1;
    if (particleCurveEcoAlive >= 50000)
        return 8;
    if (particleCurveEcoAlive >= 20000)
        return 4;
    // Particles on (any count): slight downres to free UI CPU for sim/GL.
    return 2;
}

int FrequencyResponseComponent::resolveDynamicCurveTimerHz() const noexcept
{
    if (shouldSkipCombinedCurveWork() && ! LfoMod::anyActiveRouting (parameters))
        return 0; // timer not needed — only sum was animating
    if (! particleCurveEcoActive)
        return 45;
    if (particleCurveEcoAlive >= 50000)
        return 8;
    if (particleCurveEcoAlive >= 20000)
        return 12;
    return 20;
}

bool FrequencyResponseComponent::shouldSkipCombinedCurveWork() const noexcept
{
    return disableCumulativeCurve;
}

void FrequencyResponseComponent::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    applyThemeToChildControls();
    repaint();
}

void FrequencyResponseComponent::applyThemeToChildControls()
{
    const auto& c = colors();

    auto styleRangeButton = [&c] (juce::TextButton& button)
    {
        button.setColour (juce::TextButton::buttonColourId, c.pluginButtonBackground);
        button.setColour (juce::TextButton::buttonOnColourId, c.pluginButtonAccent);
        button.setColour (juce::TextButton::textColourOffId, c.pluginButtonText.withAlpha (0.85f));
        button.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };

    styleRangeButton (eqRangeMinusButton);
    styleRangeButton (eqRangePlusButton);
    styleRangeButton (modButton);
    styleRangeButton (uiModeButton);
    styleRangeButton (proportionalQButton);
    styleRangeButton (pianoDisplayButton);
    styleRangeButton (autoGainButton);
    styleRangeButton (splitSoloTButton);
    styleRangeButton (splitSoloSButton);

    splitSeparationKnob.setColour (juce::Slider::rotarySliderFillColourId, c.pluginButtonAccent);
    splitSeparationKnob.setColour (juce::Slider::rotarySliderOutlineColourId, c.pluginButtonBackground);
    splitSeparationKnob.setColour (juce::Slider::thumbColourId, c.pluginButtonText);
    matchAmountKnob.setThemeColors (themeColors);
    matchHpKnob.setThemeColors (themeColors);
    matchLpKnob.setThemeColors (themeColors);
    matchAmountLabel.setColour (juce::Label::textColourId, c.graphAxisText.withAlpha (0.75f));
    matchHpLabel.setColour (juce::Label::textColourId, c.graphAxisText.withAlpha (0.75f));
    matchLpLabel.setColour (juce::Label::textColourId, c.graphAxisText.withAlpha (0.75f));
    styleRangeButton (matchButton);
    styleRangeButton (matchCurveButton);
    styleRangeButton (matchFreezeButton);

    eqRangeLabel.setColour (juce::Label::textColourId, c.graphAxisText.withAlpha (0.75f));

    if (optionBoxMenu != nullptr)
        optionBoxMenu->setThemeColors (themeColors);
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
    for (int bi = 0; bi < 8; ++bi)
    {
        const auto id = StructuralSplit::splitModeParamIDForBandIndex (bi);
        if (id.isNotEmpty())
            parameters.removeParameterListener (id, this);
    }
    parameters.removeParameterListener (StructuralSplit::soloParamId(), this);
    parameters.removeParameterListener (MatchEq::enabledParamId(), this);
    parameters.removeParameterListener (MatchEq::curveParamId(), this);
    parameters.removeParameterListener (MatchEq::frozenParamId(), this);
    matchFreezeButton.removeMouseListener (this);
    matchAmountAttachment.reset();
    matchHpAttachment.reset();
    matchLpAttachment.reset();
    matchFreezeAttachment.reset();
    splitSeparationAttachment.reset();
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

    for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
    {
        parameters.removeParameterListener (EqBand::frequencyParamIDForGlobal (global), this);
        parameters.removeParameterListener (EqBand::gainParamIDForGlobal (global), this);
        parameters.removeParameterListener (EqBand::qParamIDForGlobal (global), this);
        parameters.removeParameterListener (EqBand::onOffParamIDForGlobal (global), this);
        parameters.removeParameterListener (FilterType::paramIDForGlobal (global), this);
        parameters.removeParameterListener (EqBand::slopeParamIDForGlobal (global), this);
        parameters.removeParameterListener (DynamicEq::dynamicParamIDForGlobal (global), this);
    }
}

float FrequencyResponseComponent::getEqDisplayRangeDb() const
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("EQ_DISPLAY_RANGE_ID")))
    {
        switch (choice->getIndex())
        {
            case 0:  return 6.0f;
            case 1:  return 12.0f;
            case 2:  return 24.0f;
            case 3:  return 36.0f;
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
    return bandGainIsBoost (juce::String (gainParamId != nullptr ? gainParamId : ""),
                            juce::String (typeParamId != nullptr ? typeParamId : ""));
}

bool FrequencyResponseComponent::bandGainIsBoost (const juce::String& gainParamId, const juce::String& typeParamId) const
{
    if (typeParamId.isNotEmpty())
    {
        if (auto* typeRaw = parameters.getRawParameterValue (typeParamId))
        {
            const int type = juce::roundToInt (typeRaw->load());
            if (! FilterType::usesGain (type))
                return false; // notch / band-pass are subtractive
        }
    }

    if (gainParamId.isEmpty())
        return false; // HP/LP and unknown → cut-style mono fill

    if (auto* gainRaw = parameters.getRawParameterValue (gainParamId))
        return gainRaw->load() >= 0.0f;

    return true;
}

juce::Colour FrequencyResponseComponent::resolveBandFillColour (juce::Colour multicolorFill, bool isBoostOrPass) const
{
    if (isMulticolorBandFill())
        return multicolorFill;

    const auto& c = colors();
    if (isBoostOrPass)
        return c.graphSumFillTop.withAlpha (0.34f);

    return c.graphSumFillBottom.withAlpha (0.18f);
}

void FrequencyResponseComponent::syncEqRangeControls()
{
    const int range = juce::roundToInt (getEqDisplayRangeDb());
    // Keep a stable "Range" caption; current +/-dB is on the graph axis and in the tip.
    eqRangeLabel.setText ("Range", juce::dontSendNotification);
    const juce::String tip ("EQ display range - currently +/-" + juce::String (range)
                            + " dB. Use - / + for +/-6, +/-12, +/-24, or +/-36.");
    eqRangeMinusButton.setTooltip (tip);
    eqRangePlusButton.setTooltip (tip);
    eqRangeLabel.setTooltip (tip);

    const int index = (range <= 6) ? 0 : (range <= 12) ? 1 : (range <= 24) ? 2 : 3;
    eqRangeMinusButton.setEnabled (index > 0);
    eqRangePlusButton.setEnabled (index < 3);
}

void FrequencyResponseComponent::adjustEqDisplayRange (int delta)
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter ("EQ_DISPLAY_RANGE_ID"));
    if (choice == nullptr)
        return;

    const int newIndex = juce::jlimit (0, 3, choice->getIndex() + delta);
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
    resizeIfNeeded (responseDynSumScratch);
    for (auto& ext : responseExtended)
        resizeIfNeeded (ext);
    for (auto& spec : responseSpectralAmount)
        resizeIfNeeded (spec);
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

    const double fMinEval = 2.0;
    const double fMaxEval = (sr > 0.0) ? sr * 0.499 : 20000.0;

    auto evalDb = [&] (int i) -> float
    {
        const double freq = juce::jlimit (fMinEval, fMaxEval, (double) frequencies[(size_t) i]);
        const float mag = (float) coeffs->getMagnitudeForFrequency (freq, sr);
        if (! std::isfinite (mag) || mag <= 0.0f)
            return -100.0f;
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

    auto raw = [this] (const juce::String& id) -> float
    {
        if (auto* v = parameters.getRawParameterValue (id))
            return v->load();
        return 0.0f;
    };

    // D / SC: per-band curve stays on target/range (handle); cumulative uses effective gain.
    const bool dyn1 = raw ("band1Dynamic") > 0.5f || raw ("band1Sidechain") > 0.5f;
    const bool dyn2 = raw ("band2Dynamic") > 0.5f || raw ("band2Sidechain") > 0.5f;
    const bool dyn3 = raw ("band3Dynamic") > 0.5f || raw ("band3Sidechain") > 0.5f;
    const bool dyn4 = raw ("band4Dynamic") > 0.5f || raw ("band4Sidechain") > 0.5f;
    const bool dynHP = raw ("highpassDynamic") > 0.5f || raw ("highpassSidechain") > 0.5f;
    const bool dynLP = raw ("lowpassDynamic") > 0.5f || raw ("lowpassSidechain") > 0.5f;
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

    // Any LFO routed → prefer published F/G/Q so modulation is visible on curves.
    const bool anyLfoRouted = LfoMod::anyActiveRouting (parameters);
    const bool skipCombined = shouldSkipCombinedCurveWork();
    const int magStep = resolveMagnitudeSampleStep();
    lastResolvedMagStep = magStep;

    const float eg1 = processor.getBand1EffectiveGainDb();
    const float eg2 = processor.getBand2EffectiveGainDb();
    const float eg3 = processor.getBand3EffectiveGainDb();
    const float eg4 = processor.getBand4EffectiveGainDb();
    const float egHP = processor.getPublishedEffectiveGainDb (4);
    const float egLP = processor.getPublishedEffectiveGainDb (5);
    const float egHS = processor.getHighShelfEffectiveGainDb();
    const float egLS = processor.getLowShelfEffectiveGainDb();

    // Secondary catch for paints that weren't kicked by the animation timer.
    // LFO dirties per-band paths; D/SC sum animation is driven by the 45 Hz timer.
    constexpr float dynCurveEps = 0.05f;
    auto maybeDirtyLive = [&] (bool trackEffective, float effectiveGain, float staticGain,
                               float& lastGain, bool& dirtyFlag)
    {
        const float want = trackEffective ? effectiveGain : staticGain;
        if (std::abs (want - lastGain) > dynCurveEps)
        {
            dirtyFlag = true;
            if (! skipCombined)
                needsUpdateCombined = true;
        }
    };

    maybeDirtyLive (anyLfoRouted, eg1, raw ("band1Gain"), lastDynCurveGain1, needsUpdateBand1);
    maybeDirtyLive (anyLfoRouted, eg2, raw ("band2Gain"), lastDynCurveGain2, needsUpdateBand2);
    maybeDirtyLive (anyLfoRouted, eg3, raw ("band3Gain"), lastDynCurveGain3, needsUpdateBand3);
    maybeDirtyLive (anyLfoRouted, eg4, raw ("band4Gain"), lastDynCurveGain4, needsUpdateBand4);
    maybeDirtyLive (anyLfoRouted, egHS, raw ("highShelfGain"), lastDynCurveGainHS, needsUpdateHighShelf);
    maybeDirtyLive (anyLfoRouted, egLS, raw ("lowShelfGain"), lastDynCurveGainLS, needsUpdateLowShelf);

    // When the sum is disabled, ignore combined-only dirties (spectral GR / D sum ticks).
    const bool combinedDirty = needsUpdateCombined && ! skipCombined;

    const bool anyDirty = needsUpdateBand1 || needsUpdateBand2 || needsUpdateBand3 || needsUpdateBand4
                          || needsUpdateHighpass || needsUpdateLowpass || needsUpdateHighShelf || needsUpdateLowShelf
                          || needsUpdateExtended || combinedDirty || needsUpdateSpectralAmount;

    if (! anyDirty)
    {
        if (skipCombined)
            needsUpdateCombined = false;
        return;
    }

    ensureResponseBufferSize (width);

    if ((int) logFrequencies.size() != width)
        precomputeLogFrequencies();

    sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : sampleRate;

    auto clampFreq = [] (float f) { return juce::jlimit (20.0f, 20000.0f, f); };
    auto clampQ = [] (float q) { return juce::jmax (0.05f, q); };
    const bool proportionalQOn = raw ("PROPORTIONAL_Q_ID") > 0.5f;

    // Per-band curves: LFO uses published post-mod F/G/Q; D/SC keep target/range (handle).
    // Cumulative curve: D/SC/LFO use published effective gain (rebuilt below when needed).
    auto displayGain = [&] (bool liveGain, float staticGain, float effectiveGain) -> float
    {
        return liveGain ? effectiveGain : staticGain;
    };

    const bool live1 = anyLfoRouted;
    const bool live2 = anyLfoRouted;
    const bool live3 = anyLfoRouted;
    const bool live4 = anyLfoRouted;
    const bool liveHS = anyLfoRouted;
    const bool liveLS = anyLfoRouted;

    // Display pipeline (DSP untouched):
    //   1) Dense log-f grid (logFrequencies — one sample per pixel)
    //   2) Static IIR magnitudes on that grid → sum into responseCombined
    //   3) Spectral GR evaluated on the SAME grid → UI temporal smooth → add
    //   4) Side Check GR on the SAME grid, then added (sum only)
    //   5) Path build/stroke from responseCombined (in paint)
    // Per-band curves stay static IIR only (band gain is makeup when spectral is on).
    // Under particle eco, magStep > 1 downsamples IIR + GR eval (linear fill between).
    auto ensureSpectralGrBuffers = [&]()
    {
        if ((int) spectralGrScratch.size() != width)
            spectralGrScratch.resize ((size_t) width);
        if ((int) spectralGrTarget.size() != width)
            spectralGrTarget.assign ((size_t) width, 0.0f);
        else
            std::fill (spectralGrTarget.begin(), spectralGrTarget.end(), 0.0f);
        if ((int) spectralGrSmoothed.size() != width)
        {
            spectralGrSmoothed.assign ((size_t) width, 0.0f);
            spectralGrSmoothedValid = false;
        }
    };

    // Sparse GR sample into full-width scratch (step>1), then accumulate.
    auto sampleGrOntoScratch = [&] (auto&& sampleFn)
    {
        if (magStep <= 1)
        {
            sampleFn (logFrequencies.data(), spectralGrScratch.data(), width);
            return;
        }
        // One sample every magStep px, always including the last column.
        const int sparseN = (width + magStep - 1) / magStep;
        thread_local std::vector<float> sparseFreq, sparseDb;
        sparseFreq.resize ((size_t) sparseN);
        sparseDb.resize ((size_t) sparseN);
        for (int s = 0; s < sparseN; ++s)
        {
            const int i = juce::jmin (width - 1, s * magStep);
            sparseFreq[(size_t) s] = logFrequencies[(size_t) i];
        }
        sampleFn (sparseFreq.data(), sparseDb.data(), sparseN);
        expandSparseDb (sparseDb.data(), sparseN, magStep, spectralGrScratch.data(), width);
    };

    auto accumulateSpectralGr = [&] (int bandIndex, bool spectralOn) -> bool
    {
        if (! spectralOn || (int) responseCombined.size() != width || (int) logFrequencies.size() != width)
            return false;

        sampleGrOntoScratch ([&] (const float* freqs, float* dest, int n)
        {
            processor.sampleSpectralGrDb (bandIndex, freqs, dest, n);
        });
        for (int i = 0; i < width; ++i)
            spectralGrTarget[(size_t) i] += spectralGrScratch[(size_t) i];
        return true;
    };

    auto commitSmoothedSpectralGr = [&] (bool anySpectralSampled)
    {
        if ((int) responseCombined.size() != width)
            return;

        if (! anySpectralSampled)
        {
            // Snap idle so the sum does not lag after S disarms.
            if (spectralGrSmoothedValid)
            {
                std::fill (spectralGrSmoothed.begin(), spectralGrSmoothed.end(), 0.0f);
                spectralGrSmoothedValid = false;
            }
            return;
        }

        // ~30 ms toward publish at 45 Hz (~0.35/tick). Audio already has 8 ms GR smooth.
        constexpr float kSpectralUiSmoothAlpha = 0.35f;

        if (! spectralGrSmoothedValid)
        {
            spectralGrSmoothed = spectralGrTarget;
            spectralGrSmoothedValid = true;
        }
        else
        {
            for (int i = 0; i < width; ++i)
            {
                const float t = spectralGrTarget[(size_t) i];
                const float s = spectralGrSmoothed[(size_t) i];
                spectralGrSmoothed[(size_t) i] = s + (t - s) * kSpectralUiSmoothAlpha;
            }
        }

        for (int i = 0; i < width; ++i)
            responseCombined[(size_t) i] += spectralGrSmoothed[(size_t) i];
    };

    auto applySideCheckGrToCombined = [&] (bool sideCheckOn)
    {
        if (! sideCheckOn || (int) responseCombined.size() != width || (int) logFrequencies.size() != width)
            return;

        if ((int) sideCheckGrScratch.size() != width)
            sideCheckGrScratch.resize ((size_t) width);

        if (magStep <= 1)
        {
            processor.sampleSideCheckGrDb (logFrequencies.data(), sideCheckGrScratch.data(), width);
        }
        else
        {
            sampleGrOntoScratch ([&] (const float* freqs, float* dest, int n)
            {
                processor.sampleSideCheckGrDb (freqs, dest, n);
            });
            sideCheckGrScratch = spectralGrScratch;
        }
        for (int i = 0; i < width; ++i)
            responseCombined[(size_t) i] += sideCheckGrScratch[(size_t) i];
    };

    auto applyMatchGrToCombined = [&] (bool matchOn)
    {
        if (! matchOn || (int) responseCombined.size() != width || (int) logFrequencies.size() != width)
            return;

        if ((int) matchGrScratch.size() != width)
            matchGrScratch.resize ((size_t) width);

        if (magStep <= 1)
        {
            processor.sampleMatchGrDb (logFrequencies.data(), matchGrScratch.data(), width);
        }
        else
        {
            sampleGrOntoScratch ([&] (const float* freqs, float* dest, int n)
            {
                processor.sampleMatchGrDb (freqs, dest, n);
            });
            matchGrScratch = spectralGrScratch;
        }
        for (int i = 0; i < width; ++i)
            responseCombined[(size_t) i] += matchGrScratch[(size_t) i];
    };

    // Rebuild only dirty bands. needsUpdateCombined alone (e.g. band on/off / spectral GR)
    // just re-sums the cached per-band magnitude buffers below.
    auto fillBandResponse = [&] (int type, float freq, float qBase, float gain,
                                 const juce::String& slopeId, std::vector<float>& dest)
    {
        const float q = FilterType::effectiveBellQ (type, clampQ (qBase), gain, proportionalQOn);
        const float f = clampFreq (freq);
        if (FilterType::isBrickwall (type))
        {
            auto stages = FilterType::makeBrickwallStages (
                sampleRate, f, q, FilterType::isHighpassFamily (type));
            FilterSlope::fillCascadedMagnitude (stages, logFrequencies, sampleRate, dest, magStep);
        }
        else if (FilterType::isHpLp (type))
        {
            const int slope = BandChannel::readChoiceIndex (parameters, slopeId);
            auto stages = FilterType::isHighpassFamily (type)
                ? FilterSlope::makeHighpassCoeffs (sampleRate, f, q, slope)
                : FilterSlope::makeLowpassCoeffs (sampleRate, f, q, slope);
            FilterSlope::fillCascadedMagnitude (stages, logFrequencies, sampleRate, dest, magStep);
        }
        else if (FilterType::isMultiStage (type))
        {
            auto stages = FilterType::makeStages (type, sampleRate, f, q, gain);
            FilterSlope::fillCascadedMagnitude (stages, logFrequencies, sampleRate, dest, magStep);
        }
        else
        {
            auto coeffs = FilterType::makeCoefficients (type, sampleRate, f, q, gain);
            fillMagnitudeResponse (coeffs, logFrequencies, sampleRate, dest, magStep);
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

    // Extended banks 2–8: cache per-band magnitudes for individual curves + sum.
    // Paths are rebuilt in paint while needsUpdateExtended is still true.
    if (needsUpdateExtended)
    {
        for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
        {
            const int ei = global - EqBand::kBankSize;
            auto& dest = responseExtended[(size_t) ei];

            if (! processor.isGlobalBandOn (global))
            {
                if ((int) dest.size() == width)
                    std::fill (dest.begin(), dest.end(), 0.0f);
                continue;
            }

            const int type = BandChannel::readChoiceIndex (parameters, FilterType::paramIDForGlobal (global), FilterType::bell);
            const float freq = clampFreq (raw (EqBand::frequencyParamIDForGlobal (global)));
            const float qBase = clampQ (raw (EqBand::qParamIDForGlobal (global)));
            const float gain = FilterType::usesGain (type) ? raw (EqBand::gainParamIDForGlobal (global)) : 0.0f;
            fillBandResponse (type, freq, qBase, gain, EqBand::slopeParamIDForGlobal (global), dest);
        }
    }

    // Sum: reuse per-band buffers unless D/SC is live without LFO (buffers are at target/range).
    // Skipped entirely when Spec3D "Disable Cumulative Curve" is on.
    auto addBandToCombined = [&] (bool bandOn, bool dynLive, bool lfoLive,
                                  int type, float freq, float qBase, float effectiveGain,
                                  const juce::String& slopeId, const std::vector<float>& bandBuf)
    {
        if (! bandOn || (int) bandBuf.size() != width)
            return;

        const bool sumNeedsEffective = dynLive && ! lfoLive && FilterType::usesGain (type);
        if (sumNeedsEffective)
        {
            fillBandResponse (type, freq, qBase, effectiveGain, slopeId, responseDynSumScratch);
            for (int i = 0; i < width; ++i)
                responseCombined[(size_t) i] += responseDynSumScratch[(size_t) i];
        }
        else
        {
            for (int i = 0; i < width; ++i)
                responseCombined[(size_t) i] += bandBuf[(size_t) i];
        }
    };

    if (! skipCombined)
    {
        std::fill (responseCombined.begin(), responseCombined.end(), 0.0f);

        {
            const int type = (int) raw ("band1Type");
            addBandToCombined (processor.getIsBand1On(), dyn1, anyLfoRouted, type,
                               live1 ? processor.getPublishedBand1Freq() : raw ("band1Frequency"),
                               live1 ? processor.getPublishedBand1Q() : raw ("band1Q"),
                               eg1, "band1Slope", responseBand1);
        }
        {
            const int type = (int) raw ("band2Type");
            addBandToCombined (processor.getIsBand2On(), dyn2, anyLfoRouted, type,
                               live2 ? processor.getPublishedBand2Freq() : raw ("band2Frequency"),
                               live2 ? processor.getPublishedBand2Q() : raw ("band2Q"),
                               eg2, "band2Slope", responseBand2);
        }
        {
            const int type = (int) raw ("band3Type");
            addBandToCombined (processor.getIsBand3On(), dyn3, anyLfoRouted, type,
                               live3 ? processor.getPublishedBand3Freq() : raw ("band3Frequency"),
                               live3 ? processor.getPublishedBand3Q() : raw ("band3Q"),
                               eg3, "band3Slope", responseBand3);
        }
        {
            const int type = (int) raw ("band4Type");
            addBandToCombined (processor.getIsBand4On(), dyn4, anyLfoRouted, type,
                               live4 ? processor.getPublishedBand4Freq() : raw ("band4Frequency"),
                               live4 ? processor.getPublishedBand4Q() : raw ("band4Q"),
                               eg4, "band4Slope", responseBand4);
        }
        {
            const int type = BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass);
            addBandToCombined (processor.getIsHighpassOn(), dynHP, false, type,
                               raw ("highpassCutoff"), raw ("highpassQ"),
                               egHP, "highpassSlope", responseHighpass);
        }
        {
            const int type = BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass);
            addBandToCombined (processor.getIsLowpassOn(), dynLP, false, type,
                               raw ("lowpassCutoff"), raw ("lowpassQ"),
                               egLP, "lowpassSlope", responseLowpass);
        }
        {
            const int type = (int) raw ("highShelfType");
            addBandToCombined (processor.getIsHighShelfOn(), dynHS, anyLfoRouted, type,
                               liveHS ? processor.getPublishedHighShelfFreq() : raw ("highShelfFrequency"),
                               liveHS ? processor.getPublishedHighShelfQ() : raw ("highShelfQ"),
                               egHS, "highShelfSlope", responseHighShelf);
        }
        {
            const int type = (int) raw ("lowShelfType");
            addBandToCombined (processor.getIsLowShelfOn(), dynLS, anyLfoRouted, type,
                               liveLS ? processor.getPublishedLowShelfFreq() : raw ("lowShelfFrequency"),
                               liveLS ? processor.getPublishedLowShelfQ() : raw ("lowShelfQ"),
                               egLS, "lowShelfSlope", responseLowShelf);
        }

        for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
        {
            if (! processor.isGlobalBandOn (global))
                continue;

            const auto& dest = responseExtended[(size_t) (global - EqBand::kBankSize)];
            if ((int) dest.size() != width)
                continue;

            const int type = BandChannel::readChoiceIndex (parameters, FilterType::paramIDForGlobal (global), FilterType::bell);
            const auto dynId = DynamicEq::dynamicParamIDForGlobal (global);
            const auto scId = BandSidechain::sidechainParamIDForGlobal (global);
            const bool dynLive = (raw (dynId) > 0.5f) || (raw (scId) > 0.5f);
            addBandToCombined (true, dynLive, false, type,
                               raw (EqBand::frequencyParamIDForGlobal (global)),
                               raw (EqBand::qParamIDForGlobal (global)),
                               processor.getPublishedEffectiveGainDb (global),
                               EqBand::slopeParamIDForGlobal (global),
                               dest);
        }
    }
    else
    {
        needsUpdateCombined = false;
    }

    // Spectral Amount curves (second handle): same shape as the band at Amount→dB.
    // Makeup stays on the normal band curve; Amount is visual only (GR still shapes the sum).
    if (needsUpdateSpectralAmount || needsUpdateBand1 || needsUpdateBand2 || needsUpdateBand3 || needsUpdateBand4
        || needsUpdateHighpass || needsUpdateLowpass || needsUpdateHighShelf || needsUpdateLowShelf
        || (needsUpdateCombined && ! skipCombined))
    {
        const float rangeDb = getEqDisplayRangeDb();
        auto fillAmount = [&] (int slot, bool bandOn, bool spectralOn, int type,
                               float freq, float qBase, const juce::String& slopeId,
                               const juce::String& amountId, const juce::String& expandId)
        {
            auto& dest = responseSpectralAmount[(size_t) slot];
            if ((int) dest.size() != width)
                dest.assign ((size_t) width, 0.0f);

            if (! bandOn || ! spectralOn || ! FilterType::usesGain (type))
            {
                std::fill (dest.begin(), dest.end(), 0.0f);
                return;
            }

            const float amount = raw (amountId);
            const bool expand = raw (expandId) > 0.5f;
            const float gainDb = spectralAmountToDisplayDb (amount, expand, rangeDb);
            fillBandResponse (type, freq, qBase, gainDb, slopeId, dest);
        };

        fillAmount (0, processor.getIsBand1On(), spec1, (int) raw ("band1Type"),
                    live1 ? processor.getPublishedBand1Freq() : raw ("band1Frequency"),
                    live1 ? processor.getPublishedBand1Q() : raw ("band1Q"),
                    "band1Slope", "band1SpectralDepth", "band1SpectralExpand");
        fillAmount (1, processor.getIsBand2On(), spec2, (int) raw ("band2Type"),
                    live2 ? processor.getPublishedBand2Freq() : raw ("band2Frequency"),
                    live2 ? processor.getPublishedBand2Q() : raw ("band2Q"),
                    "band2Slope", "band2SpectralDepth", "band2SpectralExpand");
        fillAmount (2, processor.getIsBand3On(), spec3, (int) raw ("band3Type"),
                    live3 ? processor.getPublishedBand3Freq() : raw ("band3Frequency"),
                    live3 ? processor.getPublishedBand3Q() : raw ("band3Q"),
                    "band3Slope", "band3SpectralDepth", "band3SpectralExpand");
        fillAmount (3, processor.getIsBand4On(), spec4, (int) raw ("band4Type"),
                    live4 ? processor.getPublishedBand4Freq() : raw ("band4Frequency"),
                    live4 ? processor.getPublishedBand4Q() : raw ("band4Q"),
                    "band4Slope", "band4SpectralDepth", "band4SpectralExpand");
        fillAmount (4, processor.getIsHighpassOn(), specHP,
                    BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass),
                    raw ("highpassCutoff"), raw ("highpassQ"),
                    "highpassSlope", "highpassSpectralDepth", "highpassSpectralExpand");
        fillAmount (5, processor.getIsLowpassOn(), specLP,
                    BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass),
                    raw ("lowpassCutoff"), raw ("lowpassQ"),
                    "lowpassSlope", "lowpassSpectralDepth", "lowpassSpectralExpand");
        fillAmount (6, processor.getIsHighShelfOn(), specHS, (int) raw ("highShelfType"),
                    liveHS ? processor.getPublishedHighShelfFreq() : raw ("highShelfFrequency"),
                    liveHS ? processor.getPublishedHighShelfQ() : raw ("highShelfQ"),
                    "highShelfSlope", "highShelfSpectralDepth", "highShelfSpectralExpand");
        fillAmount (7, processor.getIsLowShelfOn(), specLS, (int) raw ("lowShelfType"),
                    liveLS ? processor.getPublishedLowShelfFreq() : raw ("lowShelfFrequency"),
                    liveLS ? processor.getPublishedLowShelfQ() : raw ("lowShelfQ"),
                    "lowShelfSlope", "lowShelfSpectralDepth", "lowShelfSpectralExpand");

        needsUpdateSpectralAmount = true; // tell paint to rebuild amount paths
    }

    // Spectral / Side Check / Match GR shape the cumulative sum only — skip when disabled.
    if (! skipCombined)
    {
        // usesGain + current type must match DSP arming (S only on bell / shelves).
        ensureSpectralGrBuffers();
        bool anySpectralSampled = false;
        if (processor.getIsBand1On())
            anySpectralSampled |= accumulateSpectralGr (0, spec1 && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "band1Type", FilterType::bell)));
        if (processor.getIsBand2On())
            anySpectralSampled |= accumulateSpectralGr (1, spec2 && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "band2Type", FilterType::bell)));
        if (processor.getIsBand3On())
            anySpectralSampled |= accumulateSpectralGr (2, spec3 && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "band3Type", FilterType::bell)));
        if (processor.getIsBand4On())
            anySpectralSampled |= accumulateSpectralGr (3, spec4 && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "band4Type", FilterType::bell)));
        if (processor.getIsHighpassOn())
            anySpectralSampled |= accumulateSpectralGr (4, specHP && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass)));
        if (processor.getIsLowpassOn())
            anySpectralSampled |= accumulateSpectralGr (5, specLP && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass)));
        if (processor.getIsHighShelfOn())
            anySpectralSampled |= accumulateSpectralGr (6, specHS && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "highShelfType", FilterType::highShelf)));
        if (processor.getIsLowShelfOn())
            anySpectralSampled |= accumulateSpectralGr (7, specLS && FilterType::usesGain (
                BandChannel::readChoiceIndex (parameters, "lowShelfType", FilterType::lowShelf)));
        commitSmoothedSpectralGr (anySpectralSampled);

        applySideCheckGrToCombined (raw (SideCheck::enabledParamId()) > 0.5f);
        applyMatchGrToCombined (raw (MatchEq::enabledParamId()) > 0.5f);
    }
}

//=======================================================================================================//
void FrequencyResponseComponent::paintGraphChromeShadows (juce::Graphics& g)
{
    if (! SharedResources::glowShadowEffectsEnabled())
        return;

    auto shadowRounded = [this, &g] (juce::Component& c, float corner = 3.0f)
    {
        if (! c.isVisible() || c.getWidth() <= 1 || c.getHeight() <= 1)
            return;
        juce::Path p;
        p.addRoundedRectangle (c.getBounds().toFloat(), corner);
        chromeDropShadow.render (g, p);
    };
    auto shadowEllipse = [this, &g] (juce::Component& c)
    {
        if (! c.isVisible() || c.getWidth() <= 1 || c.getHeight() <= 1)
            return;
        juce::Path p;
        p.addEllipse (c.getBounds().toFloat().reduced (1.0f));
        chromeDropShadow.render (g, p);
    };

    shadowRounded (uiModeButton);
    shadowRounded (proportionalQButton);
    shadowRounded (pianoDisplayButton);
    shadowRounded (modButton);
    shadowRounded (eqRangeMinusButton);
    shadowRounded (eqRangePlusButton);
    shadowRounded (autoGainButton);
    shadowRounded (outputGainScrubber, 4.0f);

    shadowRounded (matchButton);
    shadowRounded (matchCurveButton);
    shadowRounded (matchFreezeButton);
    // Match AMT/HP/LP image knobs draw their own Melatonin disc shadow.

    shadowRounded (splitSoloTButton);
    shadowRounded (splitSoloSButton);
    shadowEllipse (splitSeparationKnob);
}

void FrequencyResponseComponent::paint(juce::Graphics& g)
{
    // Graph content uses an inset clip; chrome drop shadows paint after (full bounds)
    // so they sit under the buttons/knobs without being cropped.
    {
    juce::Graphics::ScopedSaveState clipScope (g);

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
    // Exclude piano strip so enabling piano expands the window without rescaling the EQ.
    auto h = getPlotHeight();

    // Define your custom colors for the gradient
    const auto& theme = colors();
    juce::Colour color1 = theme.graphBackground;
    juce::Colour color2 = theme.graphBackground2;

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
    juce::Colour standardGridLineColor = theme.graphGrid;
    juce::Colour specialGridLineColor = theme.graphGrid.withAlpha (0.2f);

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
        float labelY = (float) getPlotHeight() - 10; // Adjust as needed

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
        const int step = (rangeInt <= 6) ? 1 : (rangeInt <= 12) ? 2 : (rangeInt <= 24) ? 3 : 6;
        const float plotH = (float) getPlotHeight();

        std::vector<int> specialDbLevels;
        for (int major : { 6, 12, 18, 24, 30, 36 })
            if (major <= rangeInt)
                specialDbLevels.push_back (major);

        constexpr float labelLeft = 5.0f;
        const float labelW = 48.0f;

        g.setFont (labelFont);

        for (int db = -rangeInt; db <= rangeInt; db += step)
        {
            const float y = dbToY (static_cast<float> (db), plotH);
            const bool isSpecial = (db == 0)
                || std::find (specialDbLevels.begin(), specialDbLevels.end(), std::abs (db)) != specialDbLevels.end();

            g.setColour (isSpecial ? specialGridLineColor : standardGridLineColor);
            //g.drawLine(0, y, getWidth(), y, 1.0f);

            const float labelY = juce::jlimit (0.0f, plotH - 14.0f, y - 7.0f);
            g.setColour (theme.graphAxisText.withAlpha (isSpecial ? 0.82f : 0.58f));
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
        juce::Colour customBandColor1 = resolveBandFillColour (theme.graphBand1,
            bandGainIsBoost ("band1Gain", "band1Type"));
        g.setColour(customBandColor1);
        g.fillPath (closeShelfFillPath (band1ResponsePath, (float) h));

        juce::Colour band1CurveColor = bandCurveColour (theme.graphBand1);
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
        juce::Colour customBandColor2 = resolveBandFillColour (theme.graphBand2,
            bandGainIsBoost ("band2Gain", "band2Type"));
        g.setColour(customBandColor2);
        g.fillPath (closeShelfFillPath (band2ResponsePath, (float) h));

        juce::Colour band2CurveColor = bandCurveColour (theme.graphBand2);
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
        juce::Colour customBandColor3 = resolveBandFillColour (theme.graphBand3,
            bandGainIsBoost ("band3Gain", "band3Type"));
        g.setColour(customBandColor3);
        g.fillPath (closeShelfFillPath (band3ResponsePath, (float) h));

        juce::Colour band3CurveColor = bandCurveColour (theme.graphBand3);
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
        juce::Colour customBandColor4 = resolveBandFillColour (theme.graphBand4,
            bandGainIsBoost ("band4Gain", "band4Type"));
        g.setColour(customBandColor4);
        g.fillPath (closeShelfFillPath (band4ResponsePath, (float) h));

        juce::Colour band4CurveColor = bandCurveColour (theme.graphBand4);
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
        juce::Colour customHighpassColor = resolveBandFillColour (theme.graphBand5,
            bandGainIsBoost ("highpassGain", "highpassType"));
        g.setColour(customHighpassColor);
        const int hpType = BandChannel::readChoiceIndex (parameters, "highpassType", FilterType::highpass);
        if (FilterType::isHpLp (hpType))
            g.fillPath (highpassResponsePath);
        else
            g.fillPath (closeShelfFillPath (highpassResponsePath, (float) h));

        juce::Colour highpassCurveColor = bandCurveColour (theme.graphBand5);
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
        juce::Colour customLowpassColor = resolveBandFillColour (theme.graphBand6,
            bandGainIsBoost ("lowpassGain", "lowpassType"));
        g.setColour(customLowpassColor);
        const int lpType = BandChannel::readChoiceIndex (parameters, "lowpassType", FilterType::lowpass);
        if (FilterType::isHpLp (lpType))
            g.fillPath (lowpassResponsePath);
        else
            g.fillPath (closeShelfFillPath (lowpassResponsePath, (float) h));

        juce::Colour lowpassCurveColor = bandCurveColour (theme.graphBand6);
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
        juce::Colour customHighShelfColor = resolveBandFillColour (theme.graphBand7,
            bandGainIsBoost ("highShelfGain", "highShelfType"));
        g.setColour(customHighShelfColor);
        // Fill closes to 0 dB; stroke keeps open edge asymptotes (no drop to centre).
        g.fillPath (closeShelfFillPath (highShelfResponsePath, (float) h));

        juce::Colour highShelfCurveColor = bandCurveColour (theme.graphBand7);
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
        juce::Colour customLowShelfColor = resolveBandFillColour (theme.graphBand8,
            bandGainIsBoost ("lowShelfGain", "lowShelfType"));
        g.setColour(customLowShelfColor);
        g.fillPath (closeShelfFillPath (lowShelfResponsePath, (float) h));

        juce::Colour lowShelfCurveColor = bandCurveColour (theme.graphBand8);
        g.setColour(lowShelfCurveColor);
        g.strokePath(lowShelfResponsePath, juce::PathStrokeType(getBandPathWidth()));
    }

    //=======================================================================================================//
    // Spectral Amount curves (second curve when S is on — Amount as display-dB shape)
    {
        const juce::Colour specCols[8] = {
            theme.graphBand1, theme.graphBand2, theme.graphBand3, theme.graphBand4,
            theme.graphBand5, theme.graphBand6, theme.graphBand7, theme.graphBand8
        };
        const bool bandOn[8] = {
            processor.getIsBand1On(), processor.getIsBand2On(), processor.getIsBand3On(), processor.getIsBand4On(),
            processor.getIsHighpassOn(), processor.getIsLowpassOn(),
            processor.getIsHighShelfOn(), processor.getIsLowShelfOn()
        };
        const char* specIds[8] = {
            "band1Spectral", "band2Spectral", "band3Spectral", "band4Spectral",
            "highpassSpectral", "lowpassSpectral", "highShelfSpectral", "lowShelfSpectral"
        };
        const char* expandIds[8] = {
            "band1SpectralExpand", "band2SpectralExpand", "band3SpectralExpand", "band4SpectralExpand",
            "highpassSpectralExpand", "lowpassSpectralExpand",
            "highShelfSpectralExpand", "lowShelfSpectralExpand"
        };

        if (needsUpdateSpectralAmount)
        {
            for (int slot = 0; slot < kNumSpectralSlots; ++slot)
            {
                auto& path = spectralAmountPaths[(size_t) slot];
                path.clear();
                const auto& mag = responseSpectralAmount[(size_t) slot];
                if (! bandOn[slot] || ! rawBoolParam (parameters, specIds[slot]) || mag.empty())
                    continue;
                path = intelligentDownsample (path, mag, w, h);
            }
            needsUpdateSpectralAmount = false;
        }

        for (int slot = 0; slot < kNumSpectralSlots; ++slot)
        {
            if (! bandOn[slot] || ! rawBoolParam (parameters, specIds[slot]))
                continue;

            const auto& path = spectralAmountPaths[(size_t) slot];
            if (path.isEmpty())
                continue;

            const bool expand = rawBoolParam (parameters, expandIds[slot]);
            const auto fillCol = resolveBandFillColour (specCols[slot], expand);
            g.setColour (fillCol.withMultipliedAlpha (0.85f));
            g.fillPath (closeShelfFillPath (path, (float) h));
            g.setColour (bandCurveColour (specCols[slot]));
            g.strokePath (path, juce::PathStrokeType (getBandPathWidth()));
        }
    }

    //=======================================================================================================//
    // Extended bands (Band 9–64) — individual fill + stroke (colour wraps Bank 1 palette)
    {
        const juce::Colour extColours[8] = {
            theme.graphBand1, theme.graphBand2, theme.graphBand3, theme.graphBand4,
            theme.graphBand5, theme.graphBand6, theme.graphBand7, theme.graphBand8
        };

        if (needsUpdateExtended)
        {
            for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
            {
                const int ei = global - EqBand::kBankSize;
                auto& path = extendedResponsePaths[(size_t) ei];
                path.clear();

                if (! processor.isGlobalBandOn (global))
                    continue;

                const auto& mag = responseExtended[(size_t) ei];
                if (mag.empty())
                    continue;

                const int type = BandChannel::readChoiceIndex (
                    parameters, FilterType::paramIDForGlobal (global), FilterType::bell);

                if (type == FilterType::highpass)
                    path = intelligentDownsampleHighpass (path, mag, w, h);
                else if (type == FilterType::lowpass)
                    path = intelligentDownsampleLowpass (path, mag, w, h);
                else
                    path = intelligentDownsample (path, mag, w, h);
            }

            needsUpdateExtended = false;
        }

        for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
        {
            if (! processor.isGlobalBandOn (global))
                continue;

            const int ei = global - EqBand::kBankSize;
            const auto& path = extendedResponsePaths[(size_t) ei];
            if (path.isEmpty())
                continue;

            const auto gainId = EqBand::gainParamIDForGlobal (global);
            const auto typeId = FilterType::paramIDForGlobal (global);
            const int type = BandChannel::readChoiceIndex (parameters, typeId, FilterType::bell);
            const auto fillCol = resolveBandFillColour (
                extColours[EqBand::colourSlotFromGlobal (global)],
                bandGainIsBoost (gainId, typeId));

            g.setColour (fillCol);
            if (FilterType::isHpLp (type))
                g.fillPath (path);
            else
                g.fillPath (closeShelfFillPath (path, (float) h));

            g.setColour (bandCurveColour (extColours[EqBand::colourSlotFromGlobal (global)]));
            g.strokePath (path, juce::PathStrokeType (getBandPathWidth()));
        }
    }

    //=======================================================================================================//
   // Draw the combined frequency response (skipped when Spec3D disables cumulative curve)

    if (! shouldSkipCombinedCurveWork())
    {
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
    juce::Colour topColor = theme.graphSumFillTop;
    juce::Colour bottomColor = theme.graphSumFillBottom;
    juce::ColourGradient gradient12(topColor, 0, 0, bottomColor, 0, h, false);

    // Fill and stroke the path with the gradient
    g.setGradientFill(gradient12);
    g.fillPath(combinedResponsePath);

    juce::Colour combinedCurveColor = theme.graphSumCurve;
    // Light corner rounding + curved/rounded stroke matches band AA look on dense polylines.
    const auto combinedStrokePath = combinedResponsePath.createPathWithRoundedCorners (4.0f);
    const float sumWidth = getSumPathWidth();
    const auto sumStroke = juce::PathStrokeType (sumWidth,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded);

    // Optional Melatonin glow for the cumulative sum curve (off by default — Post Glow is the main one).
    {
        bool glowEnabled = false;
        if (SharedResources::glowShadowEffectsEnabled())
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
                const auto bloom = theme.graphSumGlow.withAlpha (glowAlpha * 0.45f);
                const auto core = theme.graphSumGlow.brighter (0.15f).withAlpha (glowAlpha * 0.75f);

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
    } // ! shouldSkipCombinedCurveWork
    else
    {
        needsUpdateCombined = false;
        combinedResponsePath.clear();
    }

    //=======================================================================================================//
    // Handle fills are multi-colour (graphBand1–8). Legible text resolves ink per disk.
    const juce::Colour graphBgForHandles = theme.graphBackground.interpolatedWith (theme.graphBackground2, 0.5f);
    juce::Colour handleColor1 = theme.graphBand1.withAlpha (1.0f);
    juce::Colour handleColor2 = theme.graphBand2.withAlpha (1.0f);
    juce::Colour handleColor3 = theme.graphBand3.withAlpha (1.0f);
    juce::Colour handleColor4 = theme.graphBand4.withAlpha (1.0f);
    juce::Colour handleColor5 = theme.graphBand5.withAlpha (1.0f);
    juce::Colour handleColor6 = theme.graphBand6.withAlpha (1.0f);
    juce::Colour handleColor7 = theme.graphBand7.withAlpha (1.0f);
    juce::Colour handleColor8 = theme.graphBand8.withAlpha (1.0f);

    auto paintHandle = [&] (float cx, float cy, float scale, juce::Colour bandFill, const juce::String& label)
    {
        paintBandHandleChrome (g, cx, cy, scale, bandFill, graphBgForHandles,
                               theme.graphHandleOutline, theme.graphHandleText, theme, label);
    };

    // Band 1 //

    if (processor.getIsBand1On())
    {
        // Unified scale factor
        float scaleFactor = ((isMouseHoveringOverHandle1 || isOptionBoxSelectingBand (0)) ? 1.25f : 1.0f)
                            * processor.getModHandlePulseScale (0);

        // Get the values from the tree state
        float band1Frequency = processor.treeState.getRawParameterValue("band1Frequency")->load();
        const int band1Type = (int) processor.treeState.getRawParameterValue ("band1Type")->load();
        float band1Gain = FilterType::usesGain (band1Type)
                            ? processor.treeState.getRawParameterValue("band1Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band1X = (getWidth() - 1) * (std::log10(band1Frequency) - logMin) / (logMax - logMin);
        float band1Y = dbToY(band1Gain, (float) getPlotHeight());

        // Apply scale factor to handle and outlines
        float handleSize = 12.0f * scaleFactor;

        // Limit handle position by the edge of the window, taking full diameter into account
        band1X = std::min(std::max(band1X, handleSize), static_cast<float>(getWidth()) - handleSize);
        band1Y = std::min(std::max(band1Y, handleSize), (float) getPlotHeight() - handleSize);

        // Update handle positions to reflect these constraints
        handleX = band1X;
        handleY = band1Y;

        paintHandle (band1X, band1Y, scaleFactor, handleColor1, "3");

        if (rawBoolParam (parameters, "band1Dynamic"))
            paintDynamicRangeHandleDecor (g, band1X, band1Y, handleSize, handleColor1, scaleFactor);
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
        float scaleFactor2 = ((isMouseHoveringOverHandle2 || isOptionBoxSelectingBand (1)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (1);

        // Get the values from the tree state
        float band2Frequency = processor.treeState.getRawParameterValue("band2Frequency")->load();
        const int band2Type = (int) processor.treeState.getRawParameterValue ("band2Type")->load();
        float band2Gain = FilterType::usesGain (band2Type)
                            ? processor.treeState.getRawParameterValue("band2Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band2X = (getWidth() - 1) * (std::log10(band2Frequency) - logMin) / (logMax - logMin);
        float band2Y = dbToY(band2Gain, (float) getPlotHeight());

        // Apply scale factor to handle and outlines
        float handleSize2 = 12.0f * scaleFactor2;

        // Limit handle position by the edge of the window, taking full diameter into account
        band2X = std::min(std::max(band2X, handleSize2), static_cast<float>(getWidth()) - handleSize2);
        band2Y = std::min(std::max(band2Y, handleSize2), (float) getPlotHeight() - handleSize2);

        // Update handle positions to align with the graphical representation
        handleX2 = band2X;
        handleY2 = band2Y;

        paintHandle (band2X, band2Y, scaleFactor2, handleColor2, "4");

        if (rawBoolParam (parameters, "band2Dynamic"))
            paintDynamicRangeHandleDecor (g, band2X, band2Y, handleSize2, handleColor2, scaleFactor2);
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
        float scaleFactor3 = ((isMouseHoveringOverHandle3 || isOptionBoxSelectingBand (2)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (2);

        // Get the values from the tree state
        float band3Frequency = processor.treeState.getRawParameterValue("band3Frequency")->load();
        const int band3Type = (int) processor.treeState.getRawParameterValue ("band3Type")->load();
        float band3Gain = FilterType::usesGain (band3Type)
                            ? processor.treeState.getRawParameterValue("band3Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band3X = (getWidth() - 1) * (std::log10(band3Frequency) - logMin) / (logMax - logMin);
        float band3Y = dbToY(band3Gain, (float) getPlotHeight());

        // Apply scale factor to handle and outlines
        float handleSize3 = 12.0f * scaleFactor3;

        // Limit handle position by the edge of the window, taking full diameter into account
        band3X = std::min(std::max(band3X, handleSize3), static_cast<float>(getWidth()) - handleSize3);
        band3Y = std::min(std::max(band3Y, handleSize3), (float) getPlotHeight() - handleSize3);

        // Update handle positions to align with the graphical representation
        handleX3 = band3X;
        handleY3 = band3Y;

        paintHandle (band3X, band3Y, scaleFactor3, handleColor3, "5");

        if (rawBoolParam (parameters, "band3Dynamic"))
            paintDynamicRangeHandleDecor (g, band3X, band3Y, handleSize3, handleColor3, scaleFactor3);
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
        float scaleFactor4 = ((isMouseHoveringOverHandle4 || isOptionBoxSelectingBand (3)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (3);

        // Get the values from the tree state
        float band4Frequency = processor.treeState.getRawParameterValue("band4Frequency")->load();
        const int band4Type = (int) processor.treeState.getRawParameterValue ("band4Type")->load();
        float band4Gain = FilterType::usesGain (band4Type)
                            ? processor.treeState.getRawParameterValue("band4Gain")->load()
                            : 0.0f;

        // Calculate the x and y coordinates for the circle
        float band4X = (getWidth() - 1) * (std::log10(band4Frequency) - logMin) / (logMax - logMin);
        float band4Y = dbToY(band4Gain, (float) getPlotHeight());

        // Apply scale factor to handle and outlines
        float handleSize4 = 12.0f * scaleFactor4;

        // Limit handle position by the edge of the window, taking full diameter into account
        band4X = std::min(std::max(band4X, handleSize4), static_cast<float>(getWidth()) - handleSize4);
        band4Y = std::min(std::max(band4Y, handleSize4), (float) getPlotHeight() - handleSize4);

        // Update handle positions to align with the graphical representation
        handleX4 = band4X;
        handleY4 = band4Y;

        paintHandle (band4X, band4Y, scaleFactor4, handleColor4, "6");

        if (rawBoolParam (parameters, "band4Dynamic"))
            paintDynamicRangeHandleDecor (g, band4X, band4Y, handleSize4, handleColor4, scaleFactor4);
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
        float scaleFactor5 = ((isMouseHoveringOverHandle5 || isOptionBoxSelectingBand (4)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (4);

        float highpassCutoff = processor.treeState.getRawParameterValue("highpassCutoff")->load();
        const int hpType = BandChannel::readChoiceIndex (processor.treeState, "highpassType", FilterType::highpass);
        const float hpGain = FilterType::usesGain (hpType)
                               ? processor.treeState.getRawParameterValue ("highpassGain")->load()
                               : 0.0f;

        float highpassX = (getWidth() - 1) * (std::log10(highpassCutoff) - logMin) / (logMax - logMin);
        float highpassY = dbToY (hpGain, (float) getPlotHeight());

        float handleSize5 = 12.0f * scaleFactor5;

        highpassX = std::min(std::max(highpassX, handleSize5), static_cast<float>(getWidth()) - handleSize5);
        highpassY = std::min(std::max(highpassY, handleSize5), (float) getPlotHeight() - handleSize5);

        handleX5 = highpassX;
        handleY5 = highpassY;

        paintHandle (highpassX, handleY5, scaleFactor5, handleColor5, "1");
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
        float scaleFactor6 = ((isMouseHoveringOverHandle6 || isOptionBoxSelectingBand (5)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (5);

        float lowpassCutoff = processor.treeState.getRawParameterValue("lowpassCutoff")->load();
        const int lpType = BandChannel::readChoiceIndex (processor.treeState, "lowpassType", FilterType::lowpass);
        const float lpGain = FilterType::usesGain (lpType)
                               ? processor.treeState.getRawParameterValue ("lowpassGain")->load()
                               : 0.0f;

        float lowpassX = (getWidth() - 1) * (std::log10(lowpassCutoff) - logMin) / (logMax - logMin);
        float lowpassY = dbToY (lpGain, (float) getPlotHeight());

        float handleSize6 = 12.0f * scaleFactor6;

        lowpassX = std::min(std::max(lowpassX, handleSize6), static_cast<float>(getWidth()) - handleSize6);
        lowpassY = std::min(std::max(lowpassY, handleSize6), (float) getPlotHeight() - handleSize6);

        handleX6 = lowpassX;
        handleY6 = lowpassY;

        paintHandle (lowpassX, handleY6, scaleFactor6, handleColor6, "8");
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
        float scaleFactor7 = ((isMouseHoveringOverHandle7 || isOptionBoxSelectingBand (6)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (6);

        // Get the values from the tree state
        float highShelfFrequency = processor.treeState.getRawParameterValue("highShelfFrequency")->load();
        const int highShelfType = (int) processor.treeState.getRawParameterValue ("highShelfType")->load();
        float highShelfGain = FilterType::usesGain (highShelfType)
                                ? processor.treeState.getRawParameterValue("highShelfGain")->load()
                                : 0.0f;

        // Calculate the x and y coordinates for the circle
        float highShelfX = (getWidth() - 1) * (std::log10(highShelfFrequency) - logMin) / (logMax - logMin);
        float highShelfY = dbToY(highShelfGain, (float) getPlotHeight());

        // Apply scale factor
        float handleSize7 = 12.0f * scaleFactor7;

        // Limit handle positions by the edge of the window
        highShelfX = std::min(std::max(highShelfX, handleSize7), static_cast<float>(getWidth()) - handleSize7);
        highShelfY = std::min(std::max(highShelfY, handleSize7), (float) getPlotHeight() - handleSize7);


        // Update handle positions
        handleX7 = highShelfX;
        handleY7 = highShelfY;

        paintHandle (highShelfX, highShelfY, scaleFactor7, handleColor7, "7");

        if (rawBoolParam (parameters, "highShelfDynamic"))
            paintDynamicRangeHandleDecor (g, highShelfX, highShelfY, handleSize7, handleColor7, scaleFactor7);
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
        float scaleFactor8 = ((isMouseHoveringOverHandle8 || isOptionBoxSelectingBand (7)) ? 1.25f : 1.0f)
                             * processor.getModHandlePulseScale (7);

        // Get the values from the tree state
        float lowShelfFrequency = processor.treeState.getRawParameterValue("lowShelfFrequency")->load();
        const int lowShelfType = (int) processor.treeState.getRawParameterValue ("lowShelfType")->load();
        float lowShelfGain = FilterType::usesGain (lowShelfType)
                               ? processor.treeState.getRawParameterValue("lowShelfGain")->load()
                               : 0.0f;

        // Calculate the x and y coordinates for the circle
        float lowShelfX = (getWidth() - 1) * (std::log10(lowShelfFrequency) - logMin) / (logMax - logMin);
        float lowShelfY = dbToY(lowShelfGain, (float) getPlotHeight());

        // Apply scale factor to handle
        float handleSize8 = 12.0f * scaleFactor8;

        // Limit handle position by the edge of the window, taking full diameter into account
        lowShelfX = std::min(std::max(lowShelfX, handleSize8), static_cast<float>(getWidth()) - handleSize8);
        lowShelfY = std::min(std::max(lowShelfY, handleSize8), (float) getPlotHeight() - handleSize8);

        // Update handle positions to reflect these constraints
        handleX8 = lowShelfX;
        handleY8 = lowShelfY;

        paintHandle (lowShelfX, lowShelfY, scaleFactor8, handleColor8, "2");

        if (rawBoolParam (parameters, "lowShelfDynamic"))
            paintDynamicRangeHandleDecor (g, lowShelfX, lowShelfY, handleSize8, handleColor8, scaleFactor8);
    }
    else
    {
        handleX8 = -1000.0f;
        handleY8 = -1000.0f;
    }

    //=======================================================================================================//
    // Spectral Amount handles (dynamic-style chrome; Y = Amount on the display dB axis)
    {
        const juce::Colour specHandleCols[8] = {
            handleColor1, handleColor2, handleColor3, handleColor4,
            handleColor5, handleColor6, handleColor7, handleColor8
        };
        const bool bandOn[8] = {
            processor.getIsBand1On(), processor.getIsBand2On(), processor.getIsBand3On(), processor.getIsBand4On(),
            processor.getIsHighpassOn(), processor.getIsLowpassOn(),
            processor.getIsHighShelfOn(), processor.getIsLowShelfOn()
        };
        const char* freqIds[8] = {
            "band1Frequency", "band2Frequency", "band3Frequency", "band4Frequency",
            "highpassCutoff", "lowpassCutoff", "highShelfFrequency", "lowShelfFrequency"
        };
        const char* typeIds[8] = {
            "band1Type", "band2Type", "band3Type", "band4Type",
            "highpassType", "lowpassType", "highShelfType", "lowShelfType"
        };
        const char* specIds[8] = {
            "band1Spectral", "band2Spectral", "band3Spectral", "band4Spectral",
            "highpassSpectral", "lowpassSpectral", "highShelfSpectral", "lowShelfSpectral"
        };
        const char* amountIds[8] = {
            "band1SpectralDepth", "band2SpectralDepth", "band3SpectralDepth", "band4SpectralDepth",
            "highpassSpectralDepth", "lowpassSpectralDepth",
            "highShelfSpectralDepth", "lowShelfSpectralDepth"
        };
        const char* expandIds[8] = {
            "band1SpectralExpand", "band2SpectralExpand", "band3SpectralExpand", "band4SpectralExpand",
            "highpassSpectralExpand", "lowpassSpectralExpand",
            "highShelfSpectralExpand", "lowShelfSpectralExpand"
        };
        const float rangeDb = getEqDisplayRangeDb();

        for (int slot = 0; slot < kNumSpectralSlots; ++slot)
        {
            auto& hs = spectralAmountHandles[(size_t) slot];
            const int type = BandChannel::readChoiceIndex (parameters, typeIds[slot], FilterType::bell);

            if (! bandOn[slot] || ! rawBoolParam (parameters, specIds[slot]) || ! FilterType::usesGain (type))
            {
                hs.x = hs.y = -1000.0f;
                continue;
            }

            const float fHz = juce::jmax (20.0f, processor.treeState.getRawParameterValue (freqIds[slot])->load());
            const float amount = processor.treeState.getRawParameterValue (amountIds[slot])->load();
            const bool expand = rawBoolParam (parameters, expandIds[slot]);
            const float amountDb = spectralAmountToDisplayDb (amount, expand, rangeDb);

            const float scale = (hs.hovering || hs.dragging || activeSpectralAmountSlot == slot) ? 1.25f : 1.0f;
            float hx = (getWidth() - 1) * (std::log10 (fHz) - logMin) / (logMax - logMin);
            float hy = dbToY (amountDb, (float) getPlotHeight());
            const float handleSize = 12.0f * scale;
            hx = std::min (std::max (hx, handleSize), static_cast<float> (getWidth()) - handleSize);
            hy = std::min (std::max (hy, handleSize), (float) getPlotHeight() - handleSize);
            hs.x = hx;
            hs.y = hy;

            const auto col = specHandleCols[slot];
            // No number on spectral amount handles — still use legible fill/rings.
            paintBandHandleChrome (g, hx, hy, scale, col, graphBgForHandles,
                                   theme.graphHandleOutline, theme.graphHandleText, theme, {});
            paintDynamicRangeHandleDecor (g, hx, hy, handleSize, col, scale);
        }
    }

    //=======================================================================================================//
    // Extended bands (Band 9–64) — all on slots across banks
    {
        const juce::Colour extColours[8] = {
            theme.graphBand1, theme.graphBand2, theme.graphBand3, theme.graphBand4,
            theme.graphBand5, theme.graphBand6, theme.graphBand7, theme.graphBand8
        };

        for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
        {
            const int ei = global - EqBand::kBankSize;
            auto& hs = extendedHandles[(size_t) ei];

            if (! processor.isGlobalBandOn (global))
            {
                hs.x = hs.y = -1000.0f;
                continue;
            }

            const float fHz = processor.treeState.getRawParameterValue (EqBand::frequencyParamIDForGlobal (global))->load();
            const int type = BandChannel::readChoiceIndex (processor.treeState, FilterType::paramIDForGlobal (global), FilterType::bell);
            const float gainDb = FilterType::usesGain (type)
                                     ? processor.treeState.getRawParameterValue (EqBand::gainParamIDForGlobal (global))->load()
                                     : 0.0f;

            const float scale = (hs.hovering || hs.dragging || activeExtendedGlobal == global
                                 || isOptionBoxSelectingBand (global)) ? 1.25f : 1.0f;
            float hx = (getWidth() - 1) * (std::log10 (juce::jmax (20.0f, fHz)) - logMin) / (logMax - logMin);
            float hy = dbToY (gainDb, (float) getPlotHeight());
            const float handleSize = 12.0f * scale;
            hx = std::min (std::max (hx, handleSize), static_cast<float> (getWidth()) - handleSize);
            hy = std::min (std::max (hy, handleSize), (float) getPlotHeight() - handleSize);
            hs.x = hx;
            hs.y = hy;

            const auto col = extColours[EqBand::colourSlotFromGlobal (global)].withAlpha (1.0f);
            paintHandle (hx, hy, scale, col, juce::String (global + 1));

            {
                const auto dynId = DynamicEq::dynamicParamIDForGlobal (global);
                if (auto* v = parameters.getRawParameterValue (dynId); v != nullptr && v->load() > 0.5f)
                    paintDynamicRangeHandleDecor (g, hx, hy, handleSize, col, scale);
            }
        }
    }

    //=======================================================================================================//
    // Match target curve overlay — only while Match is enabled.
    {
        const bool matchOn = parameters.getRawParameterValue (MatchEq::enabledParamId()) != nullptr
                             && parameters.getRawParameterValue (MatchEq::enabledParamId())->load() > 0.5f;
        if (matchOn && (int) logFrequencies.size() == w)
        {
            if ((int) matchTargetScratch.size() != w)
                matchTargetScratch.assign ((size_t) w, 0.0f);
            processor.sampleMatchTargetDb (logFrequencies.data(), matchTargetScratch.data(), w);

            matchTargetPath.clear();
            bool started = false;
            for (int i = 0; i < w; ++i)
            {
                const float db = matchTargetScratch[(size_t) i];
                const float y = dbToY (db, (float) h);
                const float x = (float) i;
                if (! started)
                {
                    matchTargetPath.startNewSubPath (x, y);
                    started = true;
                }
                else
                {
                    matchTargetPath.lineTo (x, y);
                }
            }
            if (! matchTargetPath.isEmpty())
            {
                constexpr juce::uint32 kMatchGrey = 0xffb0b0b0;
                g.setColour (juce::Colour (kMatchGrey).withAlpha (0.85f));
                g.strokePath (matchTargetPath, juce::PathStrokeType (1.5f));
            }
        }
    }

    //=======================================================================================================//
    // Alt+drag bandpass audition: dim outside the passband; hardwired light-middle-grey BP curve.
    if (processor.isAuditionBandpassActive())
    {
        const float freq = processor.getAuditionBandpassFreqHz();
        const float q = juce::jmax (0.05f, processor.getAuditionBandpassQ());
        const float halfBw = 0.5f * freq / q;
        const float fLo = juce::jmax (20.0f, freq - halfBw);
        const float fHi = juce::jmin (20000.0f, freq + halfBw);

        auto freqToX = [w, logMin, logMax] (float fHz) -> float
        {
            const float f = juce::jlimit (20.0f, 20000.0f, fHz);
            return (float) (w - 1) * (float) ((std::log10 (f) - logMin) / (logMax - logMin));
        };

        const float xLo = freqToX (fLo);
        const float xHi = freqToX (fHi);
        constexpr float kOutsideDimAlpha = 0.38f;
        g.setColour (juce::Colours::black.withAlpha (kOutsideDimAlpha));
        if (xLo > 0.5f)
            g.fillRect (0.0f, 0.0f, xLo, (float) h);
        if (xHi < (float) w - 0.5f)
            g.fillRect (xHi, 0.0f, (float) w - xHi, (float) h);

        if ((int) logFrequencies.size() == w)
        {
            if ((int) responseAuditionBp.size() != w)
                responseAuditionBp.assign ((size_t) w, 0.0f);

            const double sr = processor.getSampleRate() > 0.0 ? processor.getSampleRate() : sampleRate;
            if (auto coeffs = FilterType::makeCoefficients (FilterType::bandPass, sr, freq, q, 0.0f))
            {
                fillMagnitudeResponse (coeffs, logFrequencies, sr, responseAuditionBp,
                                       resolveMagnitudeSampleStep());
                auditionBandpassPath.clear();
                auditionBandpassPath = intelligentDownsample (auditionBandpassPath, responseAuditionBp, w, h);

                // Hardwired light-middle grey isolation curve (not theme-driven).
                constexpr juce::uint32 kAuditionBpGrey = 0xffb0b0b0;
                g.setColour (juce::Colour (kAuditionBpGrey).withAlpha (0.35f));
                g.fillPath (closeShelfFillPath (auditionBandpassPath, (float) h));
                g.setColour (juce::Colour (kAuditionBpGrey));
                g.strokePath (auditionBandpassPath, juce::PathStrokeType (getBandPathWidth()));
            }
        }
    }

    //=======================================================================================================//
    // Crosshairs //


// Crosshairs
    if (isShowCrosshair() && mouseInside && !isAnyHandleMouseOver && !optionBoxMenu->isVisible()) {
        g.setColour (theme.graphHandleText);  // Medium/Dark grey
        g.drawLine(cursorX, 0, cursorX, (float) getPlotHeight(), 1.0f);
        g.drawLine(0, cursorY, getWidth(), cursorY, 1.0f);
    }


    // Calculate frequency and dB from mouse position
    float cursorFreq = static_cast<float>(std::pow(10.0, static_cast<double>(juce::jmap(static_cast<float>(cursorX), 0.0f, static_cast<float>(getWidth()), static_cast<float>(logMin), static_cast<float>(logMax)))));
    float cursorDB = yToDb(cursorY, (float) getPlotHeight());

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
    g.setColour (theme.graphAxisText.withAlpha (0.8f));

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
        juce::Colour customColor = theme.graphOverlayBackground;
        juce::Colour borderColor = theme.graphOverlayBorder;
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


    g.setColour (theme.graphAxisText.withAlpha (0.8f));

    // Draw band name + dB / Hz / Q without ellipses (width already measured to fit)
    if (! optionBoxMenu->isVisible() && (isAnyHandleMouseOver || anyHandleDragging) && readoutBandName.isNotEmpty())
        g.drawText (readoutBandName, labelX, labelY, labelWidth, lineHeight, juce::Justification::bottomLeft, false);

    if (! optionBoxMenu->isVisible())
    {
        g.drawText (readoutDb, labelX, labelY + lineHeight, labelWidth, lineHeight, juce::Justification::bottomLeft, false);
        g.drawText (readoutHz, labelX, labelY + 2 * lineHeight, labelWidth, lineHeight, juce::Justification::bottomLeft, false);
        g.drawText (readoutQ, labelX, labelY + 3 * lineHeight, labelWidth, lineHeight, juce::Justification::bottomLeft, false);
    }

    paintPianoStrip (g);
    } // clipScope — restore full-component clip for chrome shadows

    paintGraphChromeShadows (g);
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

    for (int slot = 0; slot < kNumSpectralSlots; ++slot)
    {
        auto& hs = spectralAmountHandles[(size_t) slot];
        hs.hovering = false;
        if (hs.x < 0.0f)
            continue;
        if (std::hypot (event.position.x - hs.x, event.position.y - hs.y) <= mouseOverThreshold)
        {
            hs.hovering = true;
            isAnyHandleMouseOver = true;
            activeBand = slot;
        }
    }

    for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
    {
        auto& hs = extendedHandles[(size_t) (global - EqBand::kBankSize)];
        hs.hovering = false;
        if (hs.x < 0.0f)
            continue;
        if (std::hypot (event.position.x - hs.x, event.position.y - hs.y) <= mouseOverThreshold)
        {
            hs.hovering = true;
            isAnyHandleMouseOver = true;
            activeExtendedGlobal = global;
            currentBandName = EqBand::displayNameForGlobal (global).toStdString();
        }
    }
}





    //=======================================================================================================//
void FrequencyResponseComponent::setOptionBoxVisible (bool shouldBeVisible)
{
    if (optionBoxMenu == nullptr)
        return;

    if (shouldBeVisible && editor != nullptr && editor->isScopeModeActive())
        shouldBeVisible = false;

    const bool wasVisible = optionBoxMenu->isVisible();
    optionBoxMenu->setVisible (shouldBeVisible);

    // Band headphones monitor is OptionBox-scoped — clear when the box closes.
    if (! shouldBeVisible && wasVisible)
        optionBoxMenu->setBandListening (false);

    // Always notify when visible so the host can repair z-order (setInitialPosition may
    // have already called setVisible(true) before we get here).
    if (onOptionBoxVisibilityChanged != nullptr
        && (shouldBeVisible || wasVisible != shouldBeVisible))
        onOptionBoxVisibilityChanged();

    if (wasVisible != shouldBeVisible)
        repaint(); // grow / shrink selected handle with OptionBox visibility
}

void FrequencyResponseComponent::showOptionBoxForHandle (int bandIndex, float handlePosX, float handlePosY)
{
    if (optionBoxMenu == nullptr)
        return;

    if (editor != nullptr && editor->isScopeModeActive())
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
    repaint(); // grow the selected band handle like hover
}

bool FrequencyResponseComponent::isOptionBoxSelectingBand (int bandIndex) const noexcept
{
    return optionBoxMenu != nullptr
           && optionBoxMenu->isVisible()
           && optionBoxMenu->getCurrentBandIndex() == bandIndex;
}

void FrequencyResponseComponent::showOptionBoxForBand (int bandIndex)
{
    if (bandIndex < 0 || bandIndex >= EqBand::kMaxBands)
        return;

    float hx = (float) getWidth() * 0.5f;
    float hy = (float) getHeight() * 0.4f;

    if (bandIndex < EqBand::kBankSize)
    {
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
    }
    else
    {
        const auto& hs = extendedHandles[(size_t) (bandIndex - EqBand::kBankSize)];
        hx = hs.x;
        hy = hs.y;
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
            setFloatParam ("band1DynThreshold", -24.0f);
            setFloatParam ("band1AttackMs", DynamicEq::attackMs);
            setFloatParam ("band1ReleaseMs", DynamicEq::releaseMs);
            setBoolParam ("band1OnOff", false);
            setBoolParam ("band1Dynamic", false);
            setBoolParam ("band1Spectral", false);
            processor.clearDynamicModeGainMemory (0);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band1Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band1Channel")))
                *p = BandChannel::stereo;
            break;
        case 1:
            setFloatParam ("band2Frequency", 1000.0f);
            setFloatParam ("band2Gain", 0.0f);
            setFloatParam ("band2Q", 0.67f);
            setFloatParam ("band2DynThreshold", -24.0f);
            setFloatParam ("band2AttackMs", DynamicEq::attackMs);
            setFloatParam ("band2ReleaseMs", DynamicEq::releaseMs);
            setBoolParam ("band2OnOff", false);
            setBoolParam ("band2Dynamic", false);
            setBoolParam ("band2Spectral", false);
            processor.clearDynamicModeGainMemory (1);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band2Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band2Channel")))
                *p = BandChannel::stereo;
            break;
        case 2:
            setFloatParam ("band3Frequency", 4000.0f);
            setFloatParam ("band3Gain", 0.0f);
            setFloatParam ("band3Q", 0.67f);
            setFloatParam ("band3DynThreshold", -24.0f);
            setFloatParam ("band3AttackMs", DynamicEq::attackMs);
            setFloatParam ("band3ReleaseMs", DynamicEq::releaseMs);
            setBoolParam ("band3OnOff", false);
            setBoolParam ("band3Dynamic", false);
            setBoolParam ("band3Spectral", false);
            processor.clearDynamicModeGainMemory (2);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band3Type")))
                *p = FilterType::bell;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("band3Channel")))
                *p = BandChannel::stereo;
            break;
        case 3:
            setFloatParam ("band4Frequency", 8000.0f);
            setFloatParam ("band4Gain", 0.0f);
            setFloatParam ("band4Q", 0.67f);
            setFloatParam ("band4DynThreshold", -24.0f);
            setFloatParam ("band4AttackMs", DynamicEq::attackMs);
            setFloatParam ("band4ReleaseMs", DynamicEq::releaseMs);
            setBoolParam ("band4OnOff", false);
            setBoolParam ("band4Dynamic", false);
            setBoolParam ("band4Spectral", false);
            processor.clearDynamicModeGainMemory (3);
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
            setFloatParam ("highShelfDynThreshold", -24.0f);
            setFloatParam ("highShelfAttackMs", DynamicEq::attackMs);
            setFloatParam ("highShelfReleaseMs", DynamicEq::releaseMs);
            setBoolParam ("highShelfOnOff", false);
            setBoolParam ("highShelfDynamic", false);
            setBoolParam ("highShelfSpectral", false);
            processor.clearDynamicModeGainMemory (6);
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("highShelfType")))
                *p = FilterType::highShelf;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter ("highShelfChannel")))
                *p = BandChannel::stereo;
            break;
        case 7:
            setFloatParam ("lowShelfFrequency", 100.0f);
            setFloatParam ("lowShelfGain", 0.0f);
            setFloatParam ("lowShelfQ", 0.5f);
            setFloatParam ("lowShelfDynThreshold", -24.0f);
            setFloatParam ("lowShelfAttackMs", DynamicEq::attackMs);
            setFloatParam ("lowShelfReleaseMs", DynamicEq::releaseMs);
            setBoolParam ("lowShelfOnOff", false);
            setBoolParam ("lowShelfDynamic", false);
            setBoolParam ("lowShelfSpectral", false);
            processor.clearDynamicModeGainMemory (7);
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

void FrequencyResponseComponent::updateAuditionBandpassFromMouse (const juce::MouseEvent& event)
{
    const float freq = xToFrequency (event.position.x);
    const float h = juce::jmax (1.0f, (float) getPlotHeight());
    // Top of graph = tight Q, bottom = wide (avoids whistling via processor clamp 0.55–8).
    const float t = 1.0f - juce::jlimit (0.0f, 1.0f, event.position.y / h);
    const float q = juce::jmap (t, 0.55f, 8.0f);
    auditionBandpassDragging = true;
    processor.setAuditionBandpass (true, freq, q);
    repaint();
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

void FrequencyResponseComponent::activateOrSelectBandAtFrequency (float frequencyHz,
                                                                  int typeOverride,
                                                                  bool preferPeaking)
{
    const float freq = juce::jlimit (20.0f, 20000.0f, frequencyHz);
    int preferredBand = bandIndexForFrequencyZone (frequencyHz);

    // Harmonic stacking: aim for peaking slots first so we don't steal HP/LP/shelves.
    if (preferPeaking)
    {
        constexpr float kPeakMinHz = 150.0f;
        constexpr float kPeakMaxHz = 8000.0f;
        preferredBand = bandIndexForFrequencyZone (
            juce::jlimit (kPeakMinHz, kPeakMaxHz, freq));
        preferredBand = juce::jlimit (0, 3, preferredBand);
    }

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

    // Choice params: set by index via convertTo0to1 (same path as OptionBox).
    auto setTypeParamById = [this] (const juce::String& paramID, int typeIndex)
    {
        if (paramID.isEmpty())
            return;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                processor.treeState.getParameter (paramID)))
        {
            const int clamped = juce::jlimit (0, juce::jmax (0, choice->choices.size() - 1), typeIndex);
            choice->setValueNotifyingHost (choice->convertTo0to1 ((float) clamped));
        }
    };

    auto resolveCreateType = [&] (int bank1InternalOrNeg1, bool isExtended) -> int
    {
        if (typeOverride >= 0)
            return typeOverride;
        if (isExtended)
            return FilterType::typeForFrequencyZone (freq);
        return FilterType::defaultTypeForBandIndex (bank1InternalOrNeg1);
    };

    auto activateGlobalBand = [&] (int globalDisplay)
    {
        // Bank 1 slots keep channel-strip defaults (HP/LS/Bell/HS/LP).
        // Extended slots use frequency-zone type (never Bax — see typeForFrequencyZone).
        const bool isBank1 = globalDisplay < EqBand::kBankSize;
        const int createType = isBank1
            ? resolveCreateType (EqBand::internalFromDisplay (globalDisplay), false)
            : resolveCreateType (-1, true);
        const int bank = EqBand::bankFromGlobal (globalDisplay);
        processor.ensureBankAvailable (bank);

        setFloatParam (EqBand::frequencyParamIDForGlobal (globalDisplay), freq);
        setFloatParam (EqBand::gainParamIDForGlobal (globalDisplay), 0.0f);
        setTypeParamById (FilterType::paramIDForGlobal (globalDisplay), createType);
        setBoolParam (EqBand::onOffParamIDForGlobal (globalDisplay), true);

        if (isBank1)
        {
            // Bank 1: keep legacy dirty flags / handle selection via internal index.
            const int internal = EqBand::internalFromDisplay (globalDisplay);
            switch (internal)
            {
                case 0: needsUpdateBand1 = true; break;
                case 1: needsUpdateBand2 = true; break;
                case 2: needsUpdateBand3 = true; break;
                case 3: needsUpdateBand4 = true; break;
                case 4: needsUpdateHighpass = true; break;
                case 5: needsUpdateLowpass = true; break;
                case 6: needsUpdateHighShelf = true; break;
                case 7: needsUpdateLowShelf = true; break;
                default: break;
            }
            this->activeBand = internal;
            isMouseHoveringOverHandle1 = internal == 0;
            isMouseHoveringOverHandle2 = internal == 1;
            isMouseHoveringOverHandle3 = internal == 2;
            isMouseHoveringOverHandle4 = internal == 3;
            isMouseHoveringOverHandle5 = internal == 4;
            isMouseHoveringOverHandle6 = internal == 5;
            isMouseHoveringOverHandle7 = internal == 6;
            isMouseHoveringOverHandle8 = internal == 7;
            isAnyHandleMouseOver = true;
            activeExtendedGlobal = -1;
            if (onBandManipulationHighlight)
                onBandManipulationHighlight (internal);
            if (optionBoxMenu != nullptr)
                optionBoxMenu->setCurrentBandIndex (internal, arrayBandName);
        }
        else
        {
            needsUpdateExtended = true;
            activeExtendedGlobal = globalDisplay;
            this->activeBand = -1;
            if (onBandManipulationHighlight)
                onBandManipulationHighlight (globalDisplay);
            if (optionBoxMenu != nullptr)
                optionBoxMenu->setCurrentBandIndex (globalDisplay, arrayBandName);
        }

        needsUpdateCombined = true;
        if (onFaceplateBankJump)
            onFaceplateBankJump (bank);
        repaint();
    };

    // Faceplate banks 2+: prefer a free slot in that bank (then any / grow).
    if (preferredCreateBank > 0)
    {
        const int freeGlobal = processor.findFreeGlobalBand (preferredCreateBank);
        if (freeGlobal < 0)
        {
            if (onBandsFullSoftMax)
                onBandsFullSoftMax();
            return;
        }
        activateGlobalBand (freeGlobal);
        return;
    }

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

    auto findFreePeakingNear = [&] (int peakAnchor) -> int
    {
        for (int offset = 0; offset < 4; ++offset)
        {
            const int candidates[] = { peakAnchor - offset, peakAnchor + offset };
            for (int candidate : candidates)
            {
                if (candidate < 0 || candidate > 3)
                    continue;
                if (offset == 0 && candidate != peakAnchor)
                    continue;
                if (! isBandOn (candidate))
                    return candidate;
            }
        }
        return -1;
    };

    // Prefer the frequency-zone band when it is off. If that zone already has an
    // active handle (or preferPeaking for harmonic stack), allocate a free slot.
    int bandIndex = preferredBand;

    const bool preferredBusy = isBandOn (preferredBand);
    const bool needAlternateSlot = preferredBusy || preferPeaking;

    if (preferPeaking && ! preferredBusy && preferredBand >= 0 && preferredBand <= 3)
    {
        // Harmonic path: preferred peaking slot is free — take it.
        bandIndex = preferredBand;
    }
    else if (needAlternateSlot)
    {
        int peakAnchor = preferredBand;
        if (preferredBand < 0 || preferredBand > 3)
        {
            // HP / LP / shelf already on → borrow a free peaking slot near the click.
            constexpr float kPeakMinHz = 300.0f;
            constexpr float kPeakMaxHz = 11999.0f;
            peakAnchor = bandIndexForFrequencyZone (juce::jlimit (kPeakMinHz, kPeakMaxHz, freq));
        }

        bandIndex = findFreePeakingNear (peakAnchor);

        // Non-harmonic: fall back to shelf / HP / LP. Harmonic: skip to extended.
        if (bandIndex < 0 && ! preferPeaking)
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

        // Bank 1 full (or harmonic with no free peaks) — extended slot.
        if (bandIndex < 0)
        {
            const int freeGlobal = processor.findFreeGlobalBand (preferredCreateBank);
            if (freeGlobal < 0)
            {
                selectBandOnly (juce::jlimit (0, 7, preferredBand));
                if (onBandsFullSoftMax)
                    onBandsFullSoftMax();
                return;
            }

            activateGlobalBand (freeGlobal);
            return;
        }
    }

    // Activate unused Bank 1 band at click Hz / 0 dB.
    // Type follows the slot's channel-strip default (peaks → Bell, LS → Lo Shelf, …),
    // unless typeOverride is set (e.g. Shift+double harmonic → always Bell).
    const int createType = resolveCreateType (bandIndex, false);

    switch (bandIndex)
    {
        case 0:
            setFloatParam ("band1Frequency", freq);
            setFloatParam ("band1Gain", 0.0f);
            setTypeParamById ("band1Type", createType);
            setBoolParam ("band1OnOff", true);
            needsUpdateBand1 = true;
            break;
        case 1:
            setFloatParam ("band2Frequency", freq);
            setFloatParam ("band2Gain", 0.0f);
            setTypeParamById ("band2Type", createType);
            setBoolParam ("band2OnOff", true);
            needsUpdateBand2 = true;
            break;
        case 2:
            setFloatParam ("band3Frequency", freq);
            setFloatParam ("band3Gain", 0.0f);
            setTypeParamById ("band3Type", createType);
            setBoolParam ("band3OnOff", true);
            needsUpdateBand3 = true;
            break;
        case 3:
            setFloatParam ("band4Frequency", freq);
            setFloatParam ("band4Gain", 0.0f);
            setTypeParamById ("band4Type", createType);
            setBoolParam ("band4OnOff", true);
            needsUpdateBand4 = true;
            break;
        case 4:
            setFloatParam ("highpassCutoff", freq);
            setFloatParam ("highpassGain", 0.0f);
            setTypeParamById ("highpassType", createType);
            setBoolParam ("highpassOnOff", true);
            needsUpdateHighpass = true;
            break;
        case 5:
            setFloatParam ("lowpassCutoff", freq);
            setFloatParam ("lowpassGain", 0.0f);
            setTypeParamById ("lowpassType", createType);
            setBoolParam ("lowpassOnOff", true);
            needsUpdateLowpass = true;
            break;
        case 6:
            setFloatParam ("highShelfFrequency", freq);
            setFloatParam ("highShelfGain", 0.0f);
            setTypeParamById ("highShelfType", createType);
            setBoolParam ("highShelfOnOff", true);
            needsUpdateHighShelf = true;
            break;
        case 7:
            setFloatParam ("lowShelfFrequency", freq);
            setFloatParam ("lowShelfGain", 0.0f);
            setTypeParamById ("lowShelfType", createType);
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
    if (handlePianoMouseDown (event))
        return;

    if (event.eventComponent == &matchFreezeButton && event.mods.isPopupMenu())
    {
        if (auto* f = dynamic_cast<juce::AudioParameterBool*> (
                parameters.getParameter (MatchEq::frozenParamId())))
            *f = true;

        juce::PopupMenu menu;
        menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
        menu.addItem (1, "Save frozen curve...");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&matchFreezeButton),
            [this] (int result)
            {
                if (result != 1)
                    return;
                auto* aw = new juce::AlertWindow (
                    "Save match curve",
                    "Name for this frozen target curve:",
                    juce::AlertWindow::NoIcon,
                    this);
                aw->addTextEditor ("name", "Match curve " + juce::String (processor.getMatchEngine().getNumUserPresets() + 1));
                aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create (
                    [safe = juce::Component::SafePointer<FrequencyResponseComponent> (this), aw] (int r)
                    {
                        if (safe == nullptr || r != 1)
                            return;
                        safe->processor.getMatchEngine().saveUserPreset (aw->getTextEditorContents ("name"));
                    }), true);
            });
        return;
    }

    // Alt+drag: temporary dry bandpass isolate (freq = X, Q = Y). Takes priority over handles.
    if (event.mods.isAltDown() && ! event.mods.isPopupMenu())
    {
        updateAuditionBandpassFromMouse (event);
        return;
    }

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

    int clickedSpectralSlot = -1;
    float bestSpectralDist = clickThreshold;
    for (int slot = 0; slot < kNumSpectralSlots; ++slot)
    {
        const auto& hs = spectralAmountHandles[(size_t) slot];
        if (hs.x < 0.0f)
            continue;
        const float d = std::hypot (event.position.x - hs.x, event.position.y - hs.y);
        if (d <= bestSpectralDist)
        {
            bestSpectralDist = d;
            clickedSpectralSlot = slot;
        }
    }

    float bestNormalDist = clickThreshold + 1.0f;
    auto considerNormal = [&] (bool clicked, float dist)
    {
        if (clicked)
            bestNormalDist = juce::jmin (bestNormalDist, dist);
    };
    considerNormal (clickedHandle1, distanceToHandle1);
    considerNormal (clickedHandle2, distanceToHandle2);
    considerNormal (clickedHandle3, distanceToHandle3);
    considerNormal (clickedHandle4, distanceToHandle4);
    considerNormal (clickedHandle5, distanceToHandle5);
    considerNormal (clickedHandle6, distanceToHandle6);
    considerNormal (clickedHandle7, distanceToHandle7);
    considerNormal (clickedHandle8, distanceToHandle8);

    const bool preferSpectralAmount = clickedSpectralSlot >= 0
                                      && bestSpectralDist <= bestNormalDist;

    int clickedExtendedGlobal = -1;
    for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
    {
        const auto& hs = extendedHandles[(size_t) (global - EqBand::kBankSize)];
        if (hs.x < 0.0f)
            continue;
        if (std::hypot (event.position.x - hs.x, event.position.y - hs.y) <= clickThreshold)
        {
            clickedExtendedGlobal = global;
            break;
        }
    }

    const bool clickedAnyHandle = (! preferSpectralAmount
                                   && (clickedHandle1 || clickedHandle2 || clickedHandle3 || clickedHandle4
                                       || clickedHandle5 || clickedHandle6 || clickedHandle7 || clickedHandle8))
                                  || preferSpectralAmount
                                  || clickedExtendedGlobal >= 0;

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

    if (preferSpectralAmount && ! anyHandleDragging)
    {
        static const char* kFreqIds[8] = {
            "band1Frequency", "band2Frequency", "band3Frequency", "band4Frequency",
            "highpassCutoff", "lowpassCutoff", "highShelfFrequency", "lowShelfFrequency"
        };
        static const char* kAmountIds[8] = {
            "band1SpectralDepth", "band2SpectralDepth", "band3SpectralDepth", "band4SpectralDepth",
            "highpassSpectralDepth", "lowpassSpectralDepth",
            "highShelfSpectralDepth", "lowShelfSpectralDepth"
        };

        auto& hs = spectralAmountHandles[(size_t) clickedSpectralSlot];
        showOptionBoxForHandle (clickedSpectralSlot, hs.x, hs.y);
        hs.dragging = true;
        anyHandleDragging = true;
        activeSpectralAmountSlot = clickedSpectralSlot;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (clickedSpectralSlot);
        hs.dragOffsetX = event.position.x - hs.x;
        hs.dragOffsetY = event.position.y - hs.y;
        if (auto* p = processor.treeState.getParameter (kFreqIds[clickedSpectralSlot]))
            p->beginChangeGesture();
        if (auto* p = processor.treeState.getParameter (kAmountIds[clickedSpectralSlot]))
            p->beginChangeGesture();
        return;
    }

    if (clickedExtendedGlobal >= 0 && ! anyHandleDragging)
    {
        const int ei = clickedExtendedGlobal - EqBand::kBankSize;
        auto& hs = extendedHandles[(size_t) ei];
        showOptionBoxForHandle (clickedExtendedGlobal, hs.x, hs.y);
        hs.dragging = true;
        anyHandleDragging = true;
        activeExtendedGlobal = clickedExtendedGlobal;
        setOptionBoxInteractionFaded (true);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (clickedExtendedGlobal);
        if (onFaceplateBankJump)
            onFaceplateBankJump (EqBand::bankFromGlobal (clickedExtendedGlobal));
        hs.dragOffsetX = event.position.x - hs.x;
        hs.dragOffsetY = event.position.y - hs.y;
        processor.treeState.getParameter (EqBand::frequencyParamIDForGlobal (clickedExtendedGlobal))->beginChangeGesture();
        processor.treeState.getParameter (EqBand::gainParamIDForGlobal (clickedExtendedGlobal))->beginChangeGesture();
        return;
    }

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

    auto hitBank1Internal = [&]() -> int
    {
        const float x = event.position.x;
        const float y = event.position.y;
        if (std::hypot (x - handleX,  y - handleY)  <= clickThreshold) return 0;
        if (std::hypot (x - handleX2, y - handleY2) <= clickThreshold) return 1;
        if (std::hypot (x - handleX3, y - handleY3) <= clickThreshold) return 2;
        if (std::hypot (x - handleX4, y - handleY4) <= clickThreshold) return 3;
        if (std::hypot (x - handleX5, y - handleY5) <= clickThreshold) return 4;
        if (std::hypot (x - handleX6, y - handleY6) <= clickThreshold) return 5;
        if (std::hypot (x - handleX7, y - handleY7) <= clickThreshold) return 6;
        if (std::hypot (x - handleX8, y - handleY8) <= clickThreshold) return 7;
        return -1;
    };

    auto hitExtendedGlobal = [&]() -> int
    {
        for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
        {
            const auto& hs = extendedHandles[(size_t) (global - EqBand::kBankSize)];
            if (hs.x < 0.0f)
                continue;
            if (std::hypot (event.position.x - hs.x, event.position.y - hs.y) <= clickThreshold)
                return global;
        }
        return -1;
    };

    auto readBandFrequencyHz = [this] (int bank1Internal, int extendedGlobal) -> float
    {
        const juce::String id = (extendedGlobal >= 0)
            ? EqBand::frequencyParamIDForGlobal (extendedGlobal)
            : EqBand::frequencyParamID (bank1Internal);
        if (auto* v = parameters.getRawParameterValue (id))
            return juce::jlimit (20.0f, 20000.0f, v->load());
        return 1000.0f;
    };

    const int bank1Hit = hitBank1Internal();
    const int extendedHit = (bank1Hit < 0) ? hitExtendedGlobal() : -1;
    const bool hitHandle = (bank1Hit >= 0 || extendedHit >= 0);

    // Shift + double-click handle → spawn empty Bell at 2× frequency (harmonic stack).
    if (hitHandle && event.mods.isShiftDown())
    {
        const float srcHz = readBandFrequencyHz (bank1Hit, extendedHit);
        const float harmonicHz = juce::jlimit (20.0f, 20000.0f, srcHz * 2.0f);
        processor.getUndoManager().beginNewTransaction ("Harmonic band");
        activateOrSelectBandAtFrequency (harmonicHz, FilterType::bell, true);
        return;
    }

    // Double-click on an existing (active) handle → reset/deactivate that band.
    if (bank1Hit >= 0)
    {
        resetBandToDefaultsAndDeactivate (bank1Hit);
        return;
    }

    if (extendedHit >= 0)
    {
        // Deactivate extended band and clear dynamic dual-mode memory.
        auto setF = [this] (const juce::String& id, float v)
        {
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter (id)))
                param->setValueNotifyingHost (param->convertTo0to1 (v));
        };
        auto setB = [this] (const juce::String& id, bool v)
        {
            if (auto* param = processor.treeState.getParameter (id))
                param->setValueNotifyingHost (v ? 1.0f : 0.0f);
        };

        setB (EqBand::onOffParamIDForGlobal (extendedHit), false);
        setB (DynamicEq::dynamicParamIDForGlobal (extendedHit), false);
        setF (EqBand::gainParamIDForGlobal (extendedHit), 0.0f);
        setF (DynamicEq::thresholdParamIDForGlobal (extendedHit), -24.0f);
        setF (DynamicEq::attackMsParamIDForGlobal (extendedHit), DynamicEq::attackMs);
        setF (DynamicEq::releaseMsParamIDForGlobal (extendedHit), DynamicEq::releaseMs);
        processor.clearDynamicModeGainMemory (extendedHit);
        needsUpdateExtended = true;
        needsUpdateCombined = true;
        repaint();
        return;
    }

    // Empty graph double-click → enable a free band at click frequency (0 dB).
    activateOrSelectBandAtFrequency (xToFrequency (event.position.x));
}

//=======================================================================================================//
void FrequencyResponseComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (handlePianoMouseDrag (event))
        return;

    if (auditionBandpassDragging)
    {
        updateAuditionBandpassFromMouse (event);
        return;
    }

    auto area = getLocalBounds();
    auto w = area.getWidth();
    auto h = getPlotHeight();

    float newBand1Gain = 0.0f;
    float newBand1Frequency = 0.0f;
    float newBand1Q = 0.0f;



    if (anyHandleDragging)
    {
        cursorX = static_cast<float>(event.x);
        cursorY = static_cast<float>(event.y);

    }

    // Spectral Amount handle drag (Y → Amount, X → shared band frequency)
    if (activeSpectralAmountSlot >= 0
        && activeSpectralAmountSlot < kNumSpectralSlots
        && spectralAmountHandles[(size_t) activeSpectralAmountSlot].dragging)
    {
        static const char* kFreqIds[8] = {
            "band1Frequency", "band2Frequency", "band3Frequency", "band4Frequency",
            "highpassCutoff", "lowpassCutoff", "highShelfFrequency", "lowShelfFrequency"
        };
        static const char* kAmountIds[8] = {
            "band1SpectralDepth", "band2SpectralDepth", "band3SpectralDepth", "band4SpectralDepth",
            "highpassSpectralDepth", "lowpassSpectralDepth",
            "highShelfSpectralDepth", "lowShelfSpectralDepth"
        };
        static const char* kExpandIds[8] = {
            "band1SpectralExpand", "band2SpectralExpand", "band3SpectralExpand", "band4SpectralExpand",
            "highpassSpectralExpand", "lowpassSpectralExpand",
            "highShelfSpectralExpand", "lowShelfSpectralExpand"
        };

        auto& hs = spectralAmountHandles[(size_t) activeSpectralAmountSlot];
        float newX = juce::jlimit (0.0f, (float) (w - 1), event.position.x - hs.dragOffsetX);
        float newY = juce::jlimit (0.0f, (float) (h - 1), event.position.y - hs.dragOffsetY);
        hs.x = newX;
        hs.y = newY;

        const double logMinHz = std::log10 (20.0);
        const double logMaxHz = std::log10 (20000.0);
        const float newFreq = std::pow (10.0f, (float) (logMinHz + (logMaxHz - logMinHz) * newX / (float) (w - 1)));
        const bool expand = rawBoolParam (parameters, kExpandIds[activeSpectralAmountSlot]);
        const float amountDb = yToDb (newY, (float) h);
        const float newAmount = displayDbToSpectralAmount (amountDb, expand, getEqDisplayRangeDb());

        if (auto* paramF = dynamic_cast<juce::RangedAudioParameter*> (
                processor.treeState.getParameter (kFreqIds[activeSpectralAmountSlot])))
            paramF->setValueNotifyingHost (paramF->convertTo0to1 (juce::jlimit (20.0f, 20000.0f, newFreq)));

        if (auto* paramA = dynamic_cast<juce::RangedAudioParameter*> (
                processor.treeState.getParameter (kAmountIds[activeSpectralAmountSlot])))
            paramA->setValueNotifyingHost (paramA->convertTo0to1 (
                juce::jlimit (SpectralDynamics::kMinSpectralAmount,
                              SpectralDynamics::kMaxSpectralAmount,
                              newAmount)));

        needsUpdateSpectralAmount = true;
        needsUpdateCombined = true;
        // Frequency move also shifts the makeup band curve.
        switch (activeSpectralAmountSlot)
        {
            case 0: needsUpdateBand1 = true; break;
            case 1: needsUpdateBand2 = true; break;
            case 2: needsUpdateBand3 = true; break;
            case 3: needsUpdateBand4 = true; break;
            case 4: needsUpdateHighpass = true; break;
            case 5: needsUpdateLowpass = true; break;
            case 6: needsUpdateHighShelf = true; break;
            case 7: needsUpdateLowShelf = true; break;
            default: break;
        }
        repaint();
        return;
    }

    // Extended-band handle drag (Band 9–64)
    for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
    {
        auto& hs = extendedHandles[(size_t) (global - EqBand::kBankSize)];
        if (! hs.dragging)
            continue;

        float newX = juce::jlimit (0.0f, (float) (w - 1), event.position.x - hs.dragOffsetX);
        float newY = juce::jlimit (0.0f, (float) (h - 1), event.position.y - hs.dragOffsetY);
        hs.x = newX;
        hs.y = newY;

        const double logMin = std::log10 (20.0);
        const double logMax = std::log10 (20000.0);
        const float newFreq = std::pow (10.0f, (float) (logMin + (logMax - logMin) * newX / (float) (w - 1)));
        const int type = BandChannel::readChoiceIndex (processor.treeState, FilterType::paramIDForGlobal (global), FilterType::bell);

        if (auto* paramF = dynamic_cast<juce::RangedAudioParameter*> (
                processor.treeState.getParameter (EqBand::frequencyParamIDForGlobal (global))))
            paramF->setValueNotifyingHost (paramF->convertTo0to1 (juce::jlimit (20.0f, 20000.0f, newFreq)));

        if (FilterType::usesGain (type))
        {
            const float gainDb = juce::jlimit (-24.0f, 24.0f, yToDb (newY, (float) h));
            if (auto* paramG = dynamic_cast<juce::RangedAudioParameter*> (
                    processor.treeState.getParameter (EqBand::gainParamIDForGlobal (global))))
                paramG->setValueNotifyingHost (paramG->convertTo0to1 (gainDb));
        }

        needsUpdateExtended = true;
        needsUpdateCombined = true;
        activeExtendedGlobal = global;
        repaint();
        return;
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
    if (handlePianoMouseUp (event))
        return;

    juce::ignoreUnused(event);

    if (auditionBandpassDragging || processor.isAuditionBandpassActive())
    {
        auditionBandpassDragging = false;
        processor.setAuditionBandpass (false,
                                       processor.getAuditionBandpassFreqHz(),
                                       processor.getAuditionBandpassQ());
        repaint();
        return;
    }

    if (activeSpectralAmountSlot >= 0
        && activeSpectralAmountSlot < kNumSpectralSlots
        && spectralAmountHandles[(size_t) activeSpectralAmountSlot].dragging)
    {
        static const char* kFreqIds[8] = {
            "band1Frequency", "band2Frequency", "band3Frequency", "band4Frequency",
            "highpassCutoff", "lowpassCutoff", "highShelfFrequency", "lowShelfFrequency"
        };
        static const char* kAmountIds[8] = {
            "band1SpectralDepth", "band2SpectralDepth", "band3SpectralDepth", "band4SpectralDepth",
            "highpassSpectralDepth", "lowpassSpectralDepth",
            "highShelfSpectralDepth", "lowShelfSpectralDepth"
        };

        if (auto* p = processor.treeState.getParameter (kFreqIds[activeSpectralAmountSlot]))
            p->endChangeGesture();
        if (auto* p = processor.treeState.getParameter (kAmountIds[activeSpectralAmountSlot]))
            p->endChangeGesture();

        spectralAmountHandles[(size_t) activeSpectralAmountSlot].dragging = false;
        activeSpectralAmountSlot = -1;
        anyHandleDragging = false;
        setOptionBoxInteractionFaded (false);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (-1);
        needsUpdateSpectralAmount = true;
        needsUpdateCombined = true;
        repaint();
        return;
    }

    for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
    {
        auto& hs = extendedHandles[(size_t) (global - EqBand::kBankSize)];
        if (! hs.dragging)
            continue;

        processor.treeState.getParameter (EqBand::frequencyParamIDForGlobal (global))->endChangeGesture();
        processor.treeState.getParameter (EqBand::gainParamIDForGlobal (global))->endChangeGesture();
        hs.dragging = false;
        anyHandleDragging = false;
        setOptionBoxInteractionFaded (false);
        if (onBandManipulationHighlight)
            onBandManipulationHighlight (-1);
        return;
    }

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

            if (FilterType::showsFilterSlope (type))
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

        // Extended bands (Band 9–64)
        for (int global = EqBand::kBankSize; global < EqBand::kMaxBands; ++global)
        {
            const auto& hs = extendedHandles[(size_t) (global - EqBand::kBankSize)];
            if (hs.x < 0.0f)
                continue;
            if (std::hypot (event.position.x - hs.x, event.position.y - hs.y) > clickThreshold)
                continue;

            const auto typeId = FilterType::paramIDForGlobal (global);
            const int type = BandChannel::readChoiceIndex (parameters, typeId, FilterType::bell);
            needsUpdateExtended = true;
            needsUpdateCombined = true;

            if (FilterType::showsFilterSlope (type))
            {
                const auto slopeId = FilterSlope::paramIDForGlobal (global);
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
            }
            else if (auto* qParam = processor.treeState.getParameter (EqBand::qParamIDForGlobal (global)))
            {
                constexpr float sensitivity = 0.1f;
                const float newQ = juce::jlimit (0.0f, 1.0f, qParam->getValue() + wheel.deltaY * sensitivity);
                qParam->setValueNotifyingHost (newQ);
            }

            repaint();
            break;
        }
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
    needsUpdateExtended = true;
    needsUpdateCombined = true;

    // Keep handle caches (and a placed OptionBox) proportional to graph size.
    // Do not snap OptionBox back to the handle — user placement sticks until another band is selected.
    if (previousGraphWidth > 0 && previousGraphHeight > 0 && getWidth() > 0 && getPlotHeight() > 0)
    {
        const float sx = (float) getWidth() / (float) previousGraphWidth;
        const float sy = (float) getPlotHeight() / (float) previousGraphHeight;

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
    previousGraphHeight = getPlotHeight();

    if (optionBoxMenu != nullptr)
        optionBoxMenu->updateUiScaleFromParent();

    // Minimize / expand — top-left on the graph so it stays usable when the faceplate is hidden.
    {
        constexpr int btn = 22;
        constexpr int margin = 6;
        uiModeButton.setBounds (margin, margin, btn, btn);
        uiModeButton.toFront (false);
    }

    // Proportional Q — bottom-left, above piano strip.
    {
        constexpr int btnW = 22;
        constexpr int btnH = 18;
        constexpr int marginLeft = 18;
        constexpr int marginBottomBase = 18;
        const int marginBottom = marginBottomBase + getPianoStripHeight();
        auto area = getLocalBounds();
        area.removeFromLeft (marginLeft);
        area.removeFromBottom (marginBottom);
        auto row = area.removeFromBottom (btnH);
        proportionalQButton.setBounds (row.removeFromLeft (btnW));
        proportionalQButton.toFront (false);
    }

    // Transient / Sustain strip — bottom center between P (left) and Mod/Range (right).
    syncStructuralSplitChrome();
    layoutStructuralSplitChrome();

    // Bottom-right: Mod / Range / piano icon / AutoGain / Output (piano left of A + Out).
    {
        constexpr int btnW = 22;
        constexpr int btnH = 18;
        constexpr int gap = 4;
        constexpr int marginRight = 18;
        constexpr int marginBottomBase = 18;
        const int marginBottom = marginBottomBase + getPianoStripHeight();
        constexpr int outGainW = 58;
        constexpr int autoGainW = 22;
        constexpr int pianoW = 22;
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

        pianoDisplayButton.setBounds (row.removeFromRight (pianoW));
        row.removeFromRight (gap);
        pianoDisplayButton.toFront (false);

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

    // Match — graph bottom, left of Scope (EqEditor refreshes anchor after Scope layout).
    layoutMatchChrome();
    syncMatchChrome();

    repaint();
}

bool FrequencyResponseComponent::anyStructuralSplitArmed() const
{
    // Fast reject when every split mode is Off (common case).
    bool anyMode = false;
    for (int bi = 0; bi < 8; ++bi)
    {
        const auto id = StructuralSplit::splitModeParamIDForBandIndex (bi);
        if (auto* v = parameters.getRawParameterValue (id))
        {
            if (v->load() > 0.001f)
            {
                anyMode = true;
                break;
            }
        }
    }
    if (! anyMode)
        return false;

    auto on = [this] (const char* id) -> bool
    {
        if (auto* v = parameters.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };
    auto modeArmed = [this] (int bandIndex) -> bool
    {
        const auto id = StructuralSplit::splitModeParamIDForBandIndex (bandIndex);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (id)))
            return StructuralSplit::clampMode (choice->getIndex()) != StructuralSplit::Mode::off;
        return false;
    };
    auto usesGainType = [this] (const char* typeId, int fallbackType) -> bool
    {
        return FilterType::usesGain (BandChannel::readChoiceIndex (parameters, typeId, fallbackType));
    };

    return (on ("band1OnOff") && usesGainType ("band1Type", FilterType::bell) && modeArmed (0))
        || (on ("band2OnOff") && usesGainType ("band2Type", FilterType::bell) && modeArmed (1))
        || (on ("band3OnOff") && usesGainType ("band3Type", FilterType::bell) && modeArmed (2))
        || (on ("band4OnOff") && usesGainType ("band4Type", FilterType::bell) && modeArmed (3))
        || (on ("highpassOnOff") && usesGainType ("highpassType", FilterType::highpass) && modeArmed (4))
        || (on ("lowpassOnOff") && usesGainType ("lowpassType", FilterType::lowpass) && modeArmed (5))
        || (on ("highShelfOnOff") && usesGainType ("highShelfType", FilterType::highShelf) && modeArmed (6))
        || (on ("lowShelfOnOff") && usesGainType ("lowShelfType", FilterType::lowShelf) && modeArmed (7));
}

void FrequencyResponseComponent::setStructuralSplitSolo (StructuralSplit::Solo solo)
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            parameters.getParameter (StructuralSplit::soloParamId())))
        *choice = (int) solo;

    const bool t = solo == StructuralSplit::Solo::transient;
    const bool s = solo == StructuralSplit::Solo::sustain;
    splitSoloTButton.setToggleState (t, juce::dontSendNotification);
    splitSoloSButton.setToggleState (s, juce::dontSendNotification);
}

void FrequencyResponseComponent::layoutStructuralSplitChrome()
{
    if (! splitSoloTButton.isVisible())
        return;

    constexpr int btnW = 22;
    constexpr int btnH = 18;
    constexpr int knob = 28;
    constexpr int gap = 4;
    constexpr int marginBottom = 14;
    const int stripW = btnW + gap + knob + gap + btnW;
    const int x = (getWidth() - stripW) / 2;
    const int y = getHeight() - (marginBottom + getPianoStripHeight()) - juce::jmax (btnH, knob);
    splitSoloTButton.setBounds (x, y + (knob - btnH) / 2, btnW, btnH);
    splitSeparationKnob.setBounds (x + btnW + gap, y, knob, knob);
    splitSoloSButton.setBounds (x + btnW + gap + knob + gap, y + (knob - btnH) / 2, btnW, btnH);
    splitSoloTButton.toFront (false);
    splitSeparationKnob.toFront (false);
    splitSoloSButton.toFront (false);
}

void FrequencyResponseComponent::syncStructuralSplitChrome()
{
    const bool show = anyStructuralSplitArmed();
    splitSoloTButton.setVisible (show);
    splitSoloSButton.setVisible (show);
    splitSeparationKnob.setVisible (show);

    if (! show)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                parameters.getParameter (StructuralSplit::soloParamId())))
        {
            if (choice->getIndex() != (int) StructuralSplit::Solo::off)
                *choice = (int) StructuralSplit::Solo::off;
        }
        splitSoloTButton.setToggleState (false, juce::dontSendNotification);
        splitSoloSButton.setToggleState (false, juce::dontSendNotification);
        return;
    }

    StructuralSplit::Solo solo = StructuralSplit::Solo::off;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            parameters.getParameter (StructuralSplit::soloParamId())))
        solo = StructuralSplit::clampSolo (choice->getIndex());

    splitSoloTButton.setToggleState (solo == StructuralSplit::Solo::transient, juce::dontSendNotification);
    splitSoloSButton.setToggleState (solo == StructuralSplit::Solo::sustain, juce::dontSendNotification);
}

juce::Rectangle<int> FrequencyResponseComponent::layoutMatchChromeAt (juce::Point<int> leftTopInLocal,
                                                                     int btnH, int matchBtnW)
{
    matchChromeLeftTop = leftTopInLocal;
    matchChromeBtnH = juce::jmax (16, btnH);
    matchChromeMatchW = juce::jmax (matchChromeBtnH, matchBtnW);
    matchChromeHasAnchor = true;
    layoutMatchChrome();
    return matchChromeBounds;
}

int FrequencyResponseComponent::getPianoStripHeight() const noexcept
{
    return pianoDisplayOn ? kPianoStripHeightPx : 0;
}

int FrequencyResponseComponent::getBottomGraphChromeHeight() const noexcept
{
    // Keep in sync with layoutMatchChrome marginBottomBase + btnH (above piano).
    constexpr int marginBottomBase = 18;
    return marginBottomBase + juce::jmax (18, matchChromeBtnH);
}

int FrequencyResponseComponent::getPlotHeight() const noexcept
{
    return juce::jmax (1, getHeight() - getPianoStripHeight());
}

void FrequencyResponseComponent::setPianoDisplayOn (bool shouldShow, bool savePrefs)
{
    if (pianoDisplayOn == shouldShow)
        return;
    pianoDisplayOn = shouldShow;
    pianoDisplayButton.setToggleState (pianoDisplayOn, juce::dontSendNotification);

    // Expand/shrink the editor window downward; graph plot height stays the same via getPlotHeight().
    if (editor != nullptr)
        editor->applyPianoStripWindowHeight (pianoDisplayOn);
    else
        resized();

    if (editor != nullptr && savePrefs)
        editor->saveUiPrefs();
}

void FrequencyResponseComponent::showMatchHpLpSlopeMenu (bool forHp)
{
    const char* paramId = forHp ? MatchEq::hpSlopeParamId() : MatchEq::lpSlopeParamId();
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (parameters.getParameter (paramId));
    if (choice == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const auto names = FilterSlope::getChoiceNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (1 + i, names[i], true, choice->getIndex() == i);

    auto* target = forHp ? &matchHpKnob : &matchLpKnob;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
        [safe = juce::Component::SafePointer<FrequencyResponseComponent> (this), paramId] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    safe->parameters.getParameter (paramId)))
                *p = result - 1;
        });
}

float FrequencyResponseComponent::frequencyToX (float freqHz) const
{
    const int w = getWidth();
    if (w <= 1)
        return 0.0f;
    const double logMin = std::log10 (20.0);
    const double logMax = std::log10 (20000.0);
    const float f = juce::jlimit (20.0f, 20000.0f, freqHz);
    return (float) (w - 1) * (float) ((std::log10 ((double) f) - logMin) / (logMax - logMin));
}

void FrequencyResponseComponent::paintPianoStrip (juce::Graphics& g)
{
    if (! pianoDisplayOn)
        return;

    const int h = kPianoStripHeightPx;
    const int y0 = getHeight() - h;
    auto strip = juce::Rectangle<int> (0, y0, getWidth(), h);
    g.setColour (juce::Colour (0xff1a1a1a));
    g.fillRect (strip);
    g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawHorizontalLine (y0, 0.0f, (float) getWidth());

    // FabFilter Pro-Q: highlighted 88-key grand A0..C8 (~27.5 Hz .. 4186 Hz).
    // Each semitone is one column on the log-f axis. White-note columns are full-height
    // white keys; black-note columns are a light key-bed with a shorter black key on top
    // (never leave black columns as empty dark gaps — those looked like "black white keys").
    const float keyTop = (float) y0 + 2.0f;
    const float keyH = (float) h - 4.0f;
    const float blackH = keyH * 0.58f;
    const float xLo = frequencyToX (MusicNote::midiToHz (MusicNote::kPianoLowestMidi));
    const float xHi = frequencyToX (MusicNote::midiToHz (MusicNote::kPianoHighestMidi + 1));

    auto semitoneX0 = [this] (int midi) { return frequencyToX (MusicNote::midiToHz (midi)); };
    auto semitoneX1 = [this] (int midi) { return frequencyToX (MusicNote::midiToHz (midi + 1)); };
    auto semitoneCentreX = [&] (int midi)
    {
        return 0.5f * (semitoneX0 (midi) + semitoneX1 (midi));
    };

    // White key bed across the whole A0..C8 span, then carve black-key columns.
    if (xHi > xLo)
    {
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.94f));
        g.fillRect (xLo, keyTop, xHi - xLo, keyH);
    }

    for (int midi = MusicNote::kPianoLowestMidi; midi <= MusicNote::kPianoHighestMidi; ++midi)
    {
        const float x0 = semitoneX0 (midi);
        const float x1 = semitoneX1 (midi);
        const float w = juce::jmax (1.0f, x1 - x0);

        if (MusicNote::isBlackKey (midi))
        {
            // Key bed under the black key stays white (already filled); black key on top.
            g.setColour (juce::Colours::black.withAlpha (0.92f));
            g.fillRect (x0, keyTop, w, blackH);
        }
        else
        {
            // Subtle separators between white keys (left edge of each white column).
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.drawVerticalLine (juce::roundToInt (x0), keyTop, keyTop + keyH);
        }
    }

    // Right edge of C8.
    g.setColour (juce::Colours::black.withAlpha (0.28f));
    g.drawVerticalLine (juce::roundToInt (xHi), keyTop, keyTop + keyH);

    // Band dots centered in the snapped note's semitone column (not on the left border).
    auto drawDot = [&] (float freqHz, juce::Colour c)
    {
        const int midi = MusicNote::hzToNearestMidi (freqHz);
        if (midi < MusicNote::kPianoLowestMidi || midi > MusicNote::kPianoHighestMidi)
            return;
        const float x = semitoneCentreX (midi);
        const float dotY = MusicNote::isBlackKey (midi)
            ? keyTop + blackH * 0.55f
            : keyTop + keyH * 0.72f;
        g.setColour (c);
        g.fillEllipse (x - 4.0f, dotY - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colours::white.withAlpha (0.7f));
        g.drawEllipse (x - 4.0f, dotY - 4.0f, 8.0f, 8.0f, 1.0f);
    };

    const juce::Colour dots[] = {
        juce::Colour (0xffe07a3a), juce::Colour (0xff3aa0e0), juce::Colour (0xff7ae03a),
        juce::Colour (0xffe03a9a), juce::Colour (0xffe0c03a), juce::Colour (0xff3ae0c0),
        juce::Colour (0xffa03ae0), juce::Colour (0xffe06060)
    };
    for (int gi = 0; gi < EqBand::kMaxBands; ++gi)
    {
        if (auto* on = parameters.getRawParameterValue (EqBand::onOffParamIDForGlobal (gi)))
            if (on->load() <= 0.5f)
                continue;
        float f = 1000.0f;
        if (auto* v = parameters.getRawParameterValue (EqBand::frequencyParamIDForGlobal (gi)))
            f = v->load();
        drawDot (f, dots[gi % 8]);
    }
}

bool FrequencyResponseComponent::handlePianoMouseDown (const juce::MouseEvent& event)
{
    if (! pianoDisplayOn || event.mods.isPopupMenu())
        return false;
    const int y0 = getHeight() - kPianoStripHeightPx;
    if (event.y < y0)
        return false;

    const float freq = MusicNote::snapHzToNearestNote (xToFrequency ((float) event.x));
    // Prefer dragging an existing On band nearest in log-f; else create/select.
    int best = -1;
    float bestDist = 1.0e9f;
    for (int g = 0; g < EqBand::kMaxBands; ++g)
    {
        if (auto* on = parameters.getRawParameterValue (EqBand::onOffParamIDForGlobal (g)))
            if (on->load() <= 0.5f)
                continue;
        float f = 1000.0f;
        if (auto* v = parameters.getRawParameterValue (EqBand::frequencyParamIDForGlobal (g)))
            f = v->load();
        const float d = std::abs (std::log (juce::jmax (1.0f, f)) - std::log (freq));
        if (d < bestDist)
        {
            bestDist = d;
            best = g;
        }
    }

    if (best >= 0 && bestDist < 0.35f) // ~half octave
    {
        pianoDragBandIndex = best;
        setBandFrequencySnappedToNote (best, freq);
        return true;
    }

    activateOrSelectBandAtFrequency (freq);
    return true;
}

bool FrequencyResponseComponent::handlePianoMouseDrag (const juce::MouseEvent& event)
{
    if (pianoDragBandIndex < 0 || ! pianoDisplayOn)
        return false;
    const float freq = MusicNote::snapHzToNearestNote (xToFrequency ((float) event.x));
    setBandFrequencySnappedToNote (pianoDragBandIndex, freq);
    return true;
}

bool FrequencyResponseComponent::handlePianoMouseUp (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    if (pianoDragBandIndex < 0)
        return false;
    pianoDragBandIndex = -1;
    return true;
}

void FrequencyResponseComponent::setBandFrequencySnappedToNote (int bandIndex, float freqHz)
{
    const float f = juce::jlimit (20.0f, 20000.0f, freqHz);
    juce::String id;
    id = EqBand::frequencyParamIDForGlobal (bandIndex);
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (parameters.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (f));
        p->endChangeGesture();
    }
    needsUpdateCombined = true;
    repaint();
}

void FrequencyResponseComponent::enforceMatchHpLpGap (juce::Slider* changed)
{
    if (matchHpLpGapGuard || changed == nullptr)
        return;
    if (changed != &matchHpKnob && changed != &matchLpKnob)
        return;

    float hp = (float) matchHpKnob.getValue();
    float lp = (float) matchLpKnob.getValue();
    if (hp + MatchEq::kMinHpLpGapHz <= lp)
        return;

    matchHpLpGapGuard = true;
    if (changed == &matchHpKnob)
    {
        const float newLp = juce::jmin (MatchEq::kMaxFreqHz, hp + MatchEq::kMinHpLpGapHz);
        if (std::abs ((float) matchLpKnob.getValue() - newLp) > 0.5f)
            matchLpKnob.setValue ((double) newLp, juce::sendNotificationSync);
    }
    else
    {
        const float newHp = juce::jmax (MatchEq::kMinHpLpHz, lp - MatchEq::kMinHpLpGapHz);
        if (std::abs ((float) matchHpKnob.getValue() - newHp) > 0.5f)
            matchHpKnob.setValue ((double) newHp, juce::sendNotificationSync);
    }
    matchHpLpGapGuard = false;
}

void FrequencyResponseComponent::layoutMatchChrome()
{
    // Match X from EqEditor anchor; Y always matches Mod / Proportional Q (above piano).
    const int btnH = matchChromeBtnH;
    const int matchW = matchChromeMatchW;
    constexpr int curveW = 22;
    constexpr int freezeW = 22;
    // Same size as SideCheck AMT/HP/LP image knobs (chrome button height).
    const int knob = btnH;
    constexpr int labelW = 20; // HP / LP — same 11 pt as Range
    constexpr int gap = 4;
    constexpr int marginBottomBase = 18;
    constexpr int marginSide = 8;

    const bool extrasOn = parameters.getRawParameterValue (MatchEq::enabledParamId()) != nullptr
                          && parameters.getRawParameterValue (MatchEq::enabledParamId())->load() > 0.5f;

    constexpr int amtLabelW = 28;
    const int stripW = extrasOn
        ? (matchW + gap + curveW + gap + amtLabelW + knob
           + gap + labelW + knob + gap + labelW + knob
           + gap + freezeW)
        : matchW;

    int rightLimit = getWidth() - marginSide;
    if (modButton.isVisible() && modButton.getWidth() > 0)
        rightLimit = juce::jmin (rightLimit, modButton.getX() - gap);

    // Same bottom chrome row as Mod / P — never follow faceplate-trim anchors for Y.
    const int marginBottom = marginBottomBase + getPianoStripHeight();
    auto chromeArea = getLocalBounds();
    chromeArea.removeFromBottom (marginBottom);
    const int btnY = chromeArea.removeFromBottom (btnH).getY();

    int x0 = 0;
    if (matchChromeHasAnchor)
        x0 = matchChromeLeftTop.x;
    else
        x0 = juce::jmax (marginSide, (rightLimit - stripW) / 2);

    x0 = juce::jlimit (marginSide, juce::jmax (marginSide, rightLimit - stripW), x0);

    int x = x0;
    const int knobY = btnY + (btnH - knob) / 2;

    matchButton.setBounds (x, btnY, matchW, btnH);
    x += matchW + gap;

    if (extrasOn)
    {
        matchCurveButton.setBounds (x, btnY, curveW, btnH);
        x += curveW + gap;

        matchAmountLabel.setBounds (x, btnY, amtLabelW, btnH);
        x += amtLabelW;
        matchAmountKnob.setBounds (x, knobY, knob, knob);
        x += knob + gap;

        matchHpLabel.setBounds (x, btnY, labelW, btnH);
        x += labelW;
        matchHpKnob.setBounds (x, knobY, knob, knob);
        x += knob + gap;

        matchLpLabel.setBounds (x, btnY, labelW, btnH);
        x += labelW;
        matchLpKnob.setBounds (x, knobY, knob, knob);
        x += knob + gap;

        matchFreezeButton.setBounds (x, btnY, freezeW, btnH);
        x += freezeW;
    }
    else
    {
        matchCurveButton.setBounds ({});
        matchAmountLabel.setBounds ({});
        matchAmountKnob.setBounds ({});
        matchHpLabel.setBounds ({});
        matchHpKnob.setBounds ({});
        matchLpLabel.setBounds ({});
        matchLpKnob.setBounds ({});
        matchFreezeButton.setBounds ({});
    }

    matchChromeBounds = juce::Rectangle<int> (x0, btnY, juce::jmax (matchW, x - x0), btnH);

    matchButton.toFront (false);
    if (extrasOn)
    {
        matchCurveButton.toFront (false);
        matchAmountLabel.toFront (false);
        matchAmountKnob.toFront (false);
        matchHpLabel.toFront (false);
        matchHpKnob.toFront (false);
        matchLpLabel.toFront (false);
        matchLpKnob.toFront (false);
        matchFreezeButton.toFront (false);
    }
}

void FrequencyResponseComponent::syncMatchChrome()
{
    const bool on = parameters.getRawParameterValue (MatchEq::enabledParamId()) != nullptr
                    && parameters.getRawParameterValue (MatchEq::enabledParamId())->load() > 0.5f;
    matchButton.setToggleState (on, juce::dontSendNotification);
    matchButton.setVisible (true);

    matchCurveButton.setVisible (on);
    matchAmountLabel.setVisible (on);
    matchAmountKnob.setVisible (on);
    matchHpKnob.setVisible (on);
    matchLpKnob.setVisible (on);
    matchHpLabel.setVisible (on);
    matchLpLabel.setVisible (on);
    matchFreezeButton.setVisible (on);

    const bool frozen = parameters.getRawParameterValue (MatchEq::frozenParamId()) != nullptr
                        && parameters.getRawParameterValue (MatchEq::frozenParamId())->load() > 0.5f;
    matchFreezeButton.setToggleState (frozen, juce::dontSendNotification);

    layoutMatchChrome();
}

void FrequencyResponseComponent::requestMatchEnable()
{
    if (matchEnableDialogOpen)
    {
        matchButton.setToggleState (false, juce::dontSendNotification);
        return;
    }

    matchEnableDialogOpen = true;
    matchButton.setToggleState (false, juce::dontSendNotification);

    auto* aw = new juce::AlertWindow (
        "Spectral Match",
        "Disable all active EQ bands for matching?\n\n"
        "Recommended so Match hears the dry source cleanly. "
        "You can turn bands back on afterward (Match before EQ by default).",
        juce::AlertWindow::QuestionIcon,
        this);

    aw->addButton ("Yes", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("No", 2);
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<FrequencyResponseComponent> (this)] (int result)
        {
            if (safe == nullptr)
                return;

            safe->matchEnableDialogOpen = false;
            if (result == 0)
            {
                safe->matchButton.setToggleState (false, juce::dontSendNotification);
                return;
            }

            safe->processor.getUndoManager().beginNewTransaction ("Match on");

            if (result == 1)
                safe->processor.applyMatchBandDisable();

            if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
                    safe->parameters.getParameter (MatchEq::enabledParamId())))
                *p = true;

            safe->syncMatchChrome();
            safe->needsUpdateCombined = true;
            safe->repaint();
        }), true);
}

void FrequencyResponseComponent::disableMatch()
{
    processor.getUndoManager().beginNewTransaction ("Match off");

    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
            parameters.getParameter (MatchEq::enabledParamId())))
        *p = false;

    processor.restoreMatchBandDisable();
    syncMatchChrome(); // hides curve / amount / freeze; leaves Match button only
    needsUpdateCombined = true;
    repaint();
}

void FrequencyResponseComponent::showMatchCurveMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    const int curve = MatchEq::readChoiceIndex (
        parameters, MatchEq::curveParamId(), MatchEq::pink, MatchEq::numFactoryCurves - 1);
    const auto names = MatchEq::getCurveChoiceNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (10 + i, names[i], true, curve == i);

    const bool eco = processor.getAnalyser().isEcoMode();
    menu.addItem (50, "Capture spectrum", ! eco, curve == MatchEq::capture);

    menu.addSeparator();
    juce::PopupMenu placeMenu;
    const int place = MatchEq::readChoiceIndex (
        parameters, MatchEq::placementParamId(), MatchEq::beforeEq, MatchEq::numPlacements - 1);
    const auto placeNames = MatchEq::getPlacementChoiceNames();
    for (int i = 0; i < placeNames.size(); ++i)
        placeMenu.addItem (60 + i, placeNames[i], true, place == i);
    menu.addSubMenu ("Match placement", placeMenu);

    juce::PopupMenu speedMenu;
    const int speed = MatchEq::readChoiceIndex (
        parameters, MatchEq::speedParamId(), MatchEq::med, MatchEq::numSpeeds - 1);
    const auto speedNames = MatchEq::getSpeedChoiceNames();
    for (int i = 0; i < speedNames.size(); ++i)
        speedMenu.addItem (70 + i, speedNames[i], true, speed == i);
    menu.addSubMenu ("Match speed", speedMenu);

    juce::PopupMenu smoothMenu;
    const float smoothVal = parameters.getRawParameterValue (MatchEq::smoothParamId()) != nullptr
                                ? parameters.getRawParameterValue (MatchEq::smoothParamId())->load()
                                : MatchEq::kDefaultSmooth;
    const int smoothIdx = MatchEq::nearestSmoothMenuIndex (smoothVal);
    const auto smoothNames = MatchEq::getSmoothMenuNames();
    for (int i = 0; i < smoothNames.size(); ++i)
        smoothMenu.addItem (90 + i, smoothNames[i], true, smoothIdx == i);
    menu.addSubMenu ("Match smooth", smoothMenu);

    juce::PopupMenu resMenu;
    const int res = MatchEq::readChoiceIndex (
        parameters, MatchEq::resolutionParamId(), MatchEq::resHigh, MatchEq::numResolutions - 1);
    const auto resNames = MatchEq::getResolutionChoiceNames();
    for (int i = 0; i < resNames.size(); ++i)
        resMenu.addItem (120 + i, resNames[i], true, res == i);
    menu.addSubMenu ("Match resolution", resMenu);

    menu.addSeparator();
    menu.addItem (80, "Save frozen curve...");
    auto& matchEng = processor.getMatchEngine();
    const int nUser = matchEng.getNumUserPresets();
    if (nUser > 0)
    {
        juce::PopupMenu userMenu;
        for (int i = 0; i < nUser; ++i)
            userMenu.addItem (100 + i, matchEng.getUserPresetName (i));
        menu.addSubMenu ("Saved curves", userMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&matchCurveButton),
        [this] (int result)
        {
            if (result <= 0)
                return;

            processor.getUndoManager().beginNewTransaction ("Match curve");

            if (result >= 10 && result < 10 + MatchEq::numFactoryCurves)
            {
                const int idx = result - 10;
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                        parameters.getParameter (MatchEq::curveParamId())))
                    *p = idx;
                if (idx != MatchEq::capture)
                    processor.syncMatchFactoryTargetFromParam();
                else
                    processor.captureMatchSpectrumFromAnalyser();
            }
            else if (result == 50)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                        parameters.getParameter (MatchEq::curveParamId())))
                    *p = MatchEq::capture;
                if (auto* f = dynamic_cast<juce::AudioParameterBool*> (
                        parameters.getParameter (MatchEq::frozenParamId())))
                    *f = false;
                processor.captureMatchSpectrumFromAnalyser();
            }
            else if (result >= 60 && result < 60 + MatchEq::numPlacements)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                        parameters.getParameter (MatchEq::placementParamId())))
                    *p = result - 60;
            }
            else if (result >= 70 && result < 70 + MatchEq::numSpeeds)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                        parameters.getParameter (MatchEq::speedParamId())))
                    *p = result - 70;
            }
            else if (result >= 90 && result < 90 + MatchEq::kNumSmoothMenuItems)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (
                        parameters.getParameter (MatchEq::smoothParamId())))
                    *p = MatchEq::smoothMenuValue (result - 90);
            }
            else if (result >= 120 && result < 120 + MatchEq::numResolutions)
            {
                if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                        parameters.getParameter (MatchEq::resolutionParamId())))
                    *p = result - 120;
            }
            else if (result == 80)
            {
                if (auto* f = dynamic_cast<juce::AudioParameterBool*> (
                        parameters.getParameter (MatchEq::frozenParamId())))
                    *f = true;

                auto* aw = new juce::AlertWindow (
                    "Save match curve",
                    "Name for this frozen target curve:",
                    juce::AlertWindow::NoIcon,
                    this);
                aw->addTextEditor ("name", "Match curve " + juce::String (processor.getMatchEngine().getNumUserPresets() + 1));
                aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                aw->enterModalState (true, juce::ModalCallbackFunction::create (
                    [safe = juce::Component::SafePointer<FrequencyResponseComponent> (this), aw] (int r)
                    {
                        if (safe == nullptr || r != 1)
                            return;
                        const auto name = aw->getTextEditorContents ("name");
                        safe->processor.getMatchEngine().saveUserPreset (name);
                    }), true);
            }
            else if (result >= 100 && result < 100 + MatchEq::kMaxUserPresets)
            {
                processor.getMatchEngine().loadUserPreset (result - 100);
                if (auto* f = dynamic_cast<juce::AudioParameterBool*> (
                        parameters.getParameter (MatchEq::frozenParamId())))
                    *f = true;
            }

            syncMatchChrome();
            needsUpdateCombined = true;
            repaint();
        });
}

void FrequencyResponseComponent::updateLiveMatchCaptureIfNeeded()
{
    const int curve = MatchEq::readChoiceIndex (
        parameters, MatchEq::curveParamId(), MatchEq::pink, MatchEq::numFactoryCurves - 1);
    if (curve != MatchEq::capture)
        return;

    const bool frozen = parameters.getRawParameterValue (MatchEq::frozenParamId()) != nullptr
                        && parameters.getRawParameterValue (MatchEq::frozenParamId())->load() > 0.5f;
    if (frozen)
        return;

    if (processor.getAnalyser().isEcoMode())
        return;

    processor.captureMatchSpectrumFromAnalyser();
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

    // D/SC: per-band curves stay on the handle; cumulative uses effective gain each tick.
    const bool anyDyn = (on ("band1OnOff") && on ("band1Dynamic"))
        || (on ("band2OnOff") && on ("band2Dynamic"))
        || (on ("band3OnOff") && on ("band3Dynamic"))
        || (on ("band4OnOff") && on ("band4Dynamic"))
        || (on ("highpassOnOff") && on ("highpassDynamic"))
        || (on ("lowpassOnOff") && on ("lowpassDynamic"))
        || (on ("highShelfOnOff") && on ("highShelfDynamic"))
        || (on ("lowShelfOnOff") && on ("lowShelfDynamic"));

    const bool anySc = (on ("band1OnOff") && on ("band1Sidechain"))
        || (on ("band2OnOff") && on ("band2Sidechain"))
        || (on ("band3OnOff") && on ("band3Sidechain"))
        || (on ("band4OnOff") && on ("band4Sidechain"))
        || (on ("highpassOnOff") && on ("highpassSidechain"))
        || (on ("lowpassOnOff") && on ("lowpassSidechain"))
        || (on ("highShelfOnOff") && on ("highShelfSidechain"))
        || (on ("lowShelfOnOff") && on ("lowShelfSidechain"));

    const bool anySpec = (on ("band1OnOff") && on ("band1Spectral"))
        || (on ("band2OnOff") && on ("band2Spectral"))
        || (on ("band3OnOff") && on ("band3Spectral"))
        || (on ("band4OnOff") && on ("band4Spectral"))
        || (on ("highShelfOnOff") && on ("highShelfSpectral"))
        || (on ("lowShelfOnOff") && on ("lowShelfSpectral"));

    const bool sideCheckOn = on (SideCheck::enabledParamId());
    const bool matchOn = on (MatchEq::enabledParamId());
    const bool matchLiveCapture = MatchEq::readChoiceIndex (
                                      parameters, MatchEq::curveParamId(), MatchEq::pink,
                                      MatchEq::numFactoryCurves - 1) == MatchEq::capture
                                  && ! on (MatchEq::frozenParamId());

    const bool anyLfo = LfoMod::anyActiveRouting (parameters);

    return anyDyn || anySc || anySpec || sideCheckOn || matchOn || matchLiveCapture || anyLfo;
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

    // LFO changes per-band IIR shape (published F/G/Q).
    // D/SC only rebuild the cumulative curve from published effective gains.
    if (anyLfo && on ("band1OnOff"))
        needsUpdateBand1 = true;
    if (anyLfo && on ("band2OnOff"))
        needsUpdateBand2 = true;
    if (anyLfo && on ("band3OnOff"))
        needsUpdateBand3 = true;
    if (anyLfo && on ("band4OnOff"))
        needsUpdateBand4 = true;
    if (anyLfo && on ("highShelfOnOff"))
        needsUpdateHighShelf = true;
    if (anyLfo && on ("lowShelfOnOff"))
        needsUpdateLowShelf = true;

    if (! shouldSkipCombinedCurveWork())
        needsUpdateCombined = true;
}

void FrequencyResponseComponent::syncDynamicCurveTimer()
{
    if (anyActiveDynamicEq())
    {
        const int hz = resolveDynamicCurveTimerHz();
        if (hz <= 0)
        {
            // Cumulative disabled and no LFO — nothing on the graph needs 45 Hz rebuilds.
            if (isTimerRunning())
                stopTimer();
            setBufferedToImage (true);
            return;
        }

        // Buffered paint can blit a stale image even after repaint() while D is moving
        // the curve every block — disable caching for the duration of dynamic animation.
        setBufferedToImage (false);

        if (! isTimerRunning() || getTimerInterval() != juce::jmax (1, 1000 / hz))
            startTimerHz (hz);

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
    updateLiveMatchCaptureIfNeeded();

    if (! anyActiveDynamicEq())
    {
        stopTimer();
        setBufferedToImage (true);
        lastDynCurveGain1 = lastDynCurveGain2 = lastDynCurveGain3 = lastDynCurveGain4 = 1.0e9f;
        lastDynCurveGainHS = lastDynCurveGainLS = 1.0e9f;
        return;
    }

    const int hz = resolveDynamicCurveTimerHz();
    if (hz <= 0)
    {
        stopTimer();
        setBufferedToImage (true);
        return;
    }

    // Keep interval in sync if particle density changed while the timer was running.
    if (getTimerInterval() != juce::jmax (1, 1000 / hz))
        startTimerHz (hz);

    // Every tick while D+On: force magnitude rebuild from published effective gains, then repaint.
    // No gain-delta threshold — small GR changes must still redraw.
    markActiveDynamicBandsDirty();
    repaint();
}

void FrequencyResponseComponent::parameterChanged(const juce::String& parameterID, float newValue)
{
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
    const bool isMatchToggle = parameterID == MatchEq::enabledParamId()
                               || parameterID == MatchEq::curveParamId()
                               || parameterID == MatchEq::frozenParamId();
    if (isDynToggle || isBandOnOff || isSideCheckToggle || isMatchToggle)
        syncDynamicCurveTimer();

    if (parameterID == MatchEq::enabledParamId()
        || parameterID == MatchEq::curveParamId()
        || parameterID == MatchEq::frozenParamId()
        || parameterID == MatchEq::amountParamId()
        || parameterID == MatchEq::smoothParamId()
        || parameterID == MatchEq::resolutionParamId()
        || parameterID == MatchEq::hpHzParamId()
        || parameterID == MatchEq::lpHzParamId())
    {
        if (parameterID == MatchEq::curveParamId())
            processor.syncMatchFactoryTargetFromParam();
        syncMatchChrome();
        needsUpdateCombined = true;
        repaint();
    }

    // Static ↔ dynamic-range gain memory when the user toggles D (not Spectral*Dynamic).
    if (parameterID.endsWith ("Dynamic") && ! parameterID.contains ("Spectral"))
    {
        const int bandIdx = bandIndexFromDynamicParamId (parameterID);
        if (bandIdx >= 0)
            processor.applyDynamicModeGainSwap (bandIdx, newValue > 0.5f);
    }

    if (parameterID.endsWith ("Spectral")
        || parameterID.contains ("SpectralDepth")
        || parameterID.endsWith ("SpectralExpand"))
        needsUpdateSpectralAmount = true;

    if (anyHandleDragging)
        return;

    if (parameterID == "EQ_DISPLAY_RANGE_ID")
    {
        needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
        needsUpdateHighpass = needsUpdateLowpass = needsUpdateHighShelf = needsUpdateLowShelf = true;
        needsUpdateExtended = true;
        needsUpdateCombined = true;
        syncEqRangeControls();
        repaint();
        return;
    }

    if (parameterID == "PROPORTIONAL_Q_ID")
    {
        // Peaking shape depends on effective Q when P is on — rebuild all tunable bands.
        needsUpdateBand1 = needsUpdateBand2 = needsUpdateBand3 = needsUpdateBand4 = true;
        needsUpdateExtended = true;
        needsUpdateCombined = true;
        repaint();
        return;
    }

    // Split chrome only — never call full resized() here (state restore / OnOff storms were lagging the UI).
    if (parameterID.endsWith ("SplitMode")
        || parameterID == StructuralSplit::soloParamId()
        || parameterID == StructuralSplit::separationParamId()
        || parameterID == "band1OnOff" || parameterID == "band2OnOff"
        || parameterID == "band3OnOff" || parameterID == "band4OnOff"
        || parameterID == "highpassOnOff" || parameterID == "lowpassOnOff"
        || parameterID == "highShelfOnOff" || parameterID == "lowShelfOnOff"
        || parameterID == "band1Type" || parameterID == "band2Type"
        || parameterID == "band3Type" || parameterID == "band4Type"
        || parameterID == "highpassType" || parameterID == "lowpassType"
        || parameterID == "highShelfType" || parameterID == "lowShelfType")
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<FrequencyResponseComponent> (this)]
        {
            if (safe == nullptr)
                return;
            safe->syncStructuralSplitChrome();
            safe->layoutStructuralSplitChrome();
        });
        if (parameterID.endsWith ("SplitMode")
            || parameterID == StructuralSplit::soloParamId()
            || parameterID == StructuralSplit::separationParamId())
            return;
    }

    if (parameterID.startsWith ("eqB"))
    {
        needsUpdateExtended = true;
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
    juce::ignoreUnused (originalPath);

    juce::Path simplifiedPath;
    if (compositeResponse.empty() || w <= 0)
        return simplifiedPath;

    const int numPoints = juce::jmin (w, (int) compositeResponse.size());
    if (numPoints <= 0)
        return simplifiedPath;

    // Open curve at real edge magnitudes. Starting at h/2 and skipping flat
    // regions made shelves draw a diagonal from 0 dB to the transition.
    simplifiedPath.startNewSubPath (0.0f, dbToY (compositeResponse[0], (float) h));

    float movingAverageY = dbToY (compositeResponse[0], (float) h);
    constexpr float adaptiveThreshold = 0.2f;

    // Dense vertices in the outer ~8% so band-shelf asymptotes / residual
    // return don't get adaptive-skipped into a diagonal "pop" at the plot edges.
    const int edgeDense = juce::jmax (4, numPoints / 12);

    for (int i = 1; i < numPoints; ++i)
    {
        const float newY = dbToY (compositeResponse[(size_t) i], (float) h);
        movingAverageY = 0.8f * movingAverageY + 0.2f * newY;
        const float error = std::abs (movingAverageY - newY);

        const bool nearEdge = (i < edgeDense) || (i >= numPoints - edgeDense);
        // Keep occasional flat-region samples so shelf asymptotes stay attached
        // to the graph edges instead of being optimized away.
        if (nearEdge || error > adaptiveThreshold || (i % 24) == 0)
        {
            simplifiedPath.lineTo ((float) i, newY);
            movingAverageY = newY;
        }
    }

    const int last = numPoints - 1;
    simplifiedPath.lineTo ((float) last, dbToY (compositeResponse[(size_t) last], (float) h));

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
    juce::ignoreUnused (originalPath);

    juce::Path simplifiedPath;

    if (compositeResponse.empty() || downsampleFactor <= 0 || w <= 0)
        return simplifiedPath;

    const int numPoints = juce::jmin (w, (int) compositeResponse.size());
    if (numPoints <= 0)
        return simplifiedPath;

    // Open curve at real edge magnitudes so low-shelf left / high-shelf right
    // asymptotes meet the graph sides (fill is closed separately via 0 dB).
    simplifiedPath.startNewSubPath (0.0f, dbToY (compositeResponse[0], (float) h));

    for (int i = downsampleFactor; i < numPoints; i += downsampleFactor)
        simplifiedPath.lineTo ((float) i, dbToY (compositeResponse[(size_t) i], (float) h));

    const int last = numPoints - 1;
    if (last > 0)
        simplifiedPath.lineTo ((float) last, dbToY (compositeResponse[(size_t) last], (float) h));

    return simplifiedPath;
}

juce::Path FrequencyResponseComponent::closeShelfFillPath (const juce::Path& curvePath, float height) const
{
    if (curvePath.isEmpty())
        return {};

    // Close to the 0 dB centreline with vertical drops at both ends.
    // (Previously only the right end dropped vertically; a non-zero left
    // asymptote then closed with a diagonal that read as an edge "pop".)
    juce::Path fillPath (curvePath);
    const auto end = fillPath.getCurrentPosition();
    const float midY = height * 0.5f;
    const float endX = end.x;
    fillPath.lineTo (endX, midY);
    fillPath.lineTo (0.0f, midY);
    fillPath.closeSubPath();
    return fillPath;
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

