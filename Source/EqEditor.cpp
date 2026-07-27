#include "JuceHeader.h"
#include "BinaryData.h"
#include "Visualizer/Analyser.h"
#include "EqProcessor.h"
#include "RotaryImageKnobLookAndFeel1.h"
#include "RotaryImageKnobLookAndFeel2.h"
#include "RotaryImageKnobLookAndFeel3.h"
#include "RotaryImageKnobLookAndFeel4.h"
#include "RotaryImageKnob1.h"
#include "RotaryImageKnob2.h"
#include "RotaryImageKnob3.h"
#include "OnOffButton1.h"
#include "EqEditor.h"
#include "KnobBandHighlight.h"
#include "ComboBoxLookAndFeel.h"
#include "FilterType.h"
#include "FilterSlope.h"
#include "EqBand.h"
#include "BandChannel.h"



using namespace ::juce::gl;

using namespace juce;

//juce::Image EqEditor::darkKnob4_StitchedImage;

using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;


EqEditor::EqEditor(EqProcessor& p, juce::AudioProcessorValueTreeState& treeState, Analyser& analyser)
    : AudioProcessorEditor(&p),
    audioProcessor(p),
    onOffButton1(std::make_unique<OnOffButton1>(treeState, "highpassOnOff")),
    onOffButton2(std::make_unique<OnOffButton1>(treeState, "lowpassOnOff")),
    onOffButton3(std::make_unique<OnOffButton1>(treeState, "highShelfOnOff")),
    onOffButton4(std::make_unique<OnOffButton1>(treeState, "lowShelfOnOff")),
    onOffButton5(std::make_unique<OnOffButton1>(treeState, "band1OnOff")),
    onOffButton6(std::make_unique<OnOffButton1>(treeState, "band2OnOff")),
    onOffButton7(std::make_unique<OnOffButton1>(treeState, "band3OnOff")),
    onOffButton8(std::make_unique<OnOffButton1>(treeState, "band4OnOff")),
    phaseModeCombo (treeState, PhaseMode::paramId())

{
    DBG("EqEditor constructor called");

    PluginMenuTheme::applyColours (getLookAndFeel());

   // initializeSharedImages();
    knob1.repaint(); 
    knob2.repaint();
    knob1.setVisible(true);
    knob2.setVisible(true);

    startTimer(50);

    mainComponent = std::make_unique<MainComponent>(p, analyser, treeState, *this);
  
    frequencyResponseComponent = std::make_unique<FrequencyResponseComponent>(p);

    //treeState.addParameterListener("highpassOnOff", this);




    juce::Colour guiLabelColor = juce::Colours::darkseagreen.withAlpha(1.0f);
    juce::Colour guiLabelColor2 = juce::Colours::ghostwhite.withAlpha(0.8f);
    juce::Colour guiAnalyserBackgroundColor = juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f);
    
    // Highpass
    juce::Colour onOffButtonColor1 = juce::Colours::green.withAlpha(1.0f).darker(0.7);
    // LowShelf
    juce::Colour onOffButtonColor2 = juce::Colours::burlywood.withAlpha(1.0f).darker(0.7);
    // Band 1
    juce::Colour onOffButtonColor3 = juce::Colours::cornflowerblue.withAlpha(1.0f).darker(0.7);
    // Band 2
    juce::Colour onOffButtonColor4 = juce::Colours::purple.withAlpha(1.0f).darker(0.7);
    // Band 3
    juce::Colour onOffButtonColor5 = juce::Colours::cyan.withAlpha(0.7f).darker(0.7);
    // Band 4
    juce::Colour onOffButtonColor6 = juce::Colours::mediumblue.withAlpha(0.9f).darker(0.7);
    // HighShelf
    juce::Colour onOffButtonColor7 = juce::Colours::darkgoldenrod.withAlpha(1.0f).darker(0.7);
    // Lowpass
    juce::Colour onOffButtonColor8 = juce::Colours::red.withAlpha(1.0f).darker(0.7);
    // Orange
    juce::Colour onOffButtonOrange = juce::Colour::fromRGB(100, 50, 12);

    setSize (designWidth, designHeight);

    // Free resize — any aspect ratio; layout adapts in resized().
    resizeConstrainer.setFixedAspectRatio (0.0);
    resizeConstrainer.setSizeLimits (900, 500, 2400, 1600);
    setConstrainer (&resizeConstrainer);
    setResizable (true, true);

    addAndMakeVisible(*mainComponent);

    modSection = std::make_unique<ModSectionComponent> (audioProcessor);
    addChildComponent (*modSection);
    modSection->setVisible (false);

    // Minimize ▲/▼ lives on FrequencyResponseComponent (graph top-left), not the faceplate.


    //frequencyResponseComponent->addMouseListener(this, true);


    // Call setFrequencyResponseComponent to link it with the processor
    //audioProcessor.setFrequencyResponseComponent(*frequencyResponseComponent);



   // addAndMakeVisible(*frequencyResponseComponent);

    // Find the values for Band 1 parameters
    //float band1Frequency = audioProcessor.treeState.getRawParameterValue("band1Frequency")->load();
   // float band1Gain = audioProcessor.treeState.getRawParameterValue("band1Gain")->load();

    // Initialize the slider's values with the parameter values
   // band1FrequencySlider.setValue(band1Frequency);
   // band1GainSlider.setValue(band1Gain);

   
  // addAndMakeVisible(m_controls);
   // m_controls.setMarginInPixels(m_marginInPixels);
   // addAndMakeVisible(m_visualizer);
   // m_visualizer.setMarginInPixels(m_marginInPixels);
  //  m_visualizer.setBackgroundColour(guiAnalyserBackgroundColor);


   // addAndMakeVisible(frequencyResponseComponent);

    // Set the rendering order, bring frequencyResponseComponent to the front
   // m_controls.toFront(false); // Bring other components to the front if needed
   // frequencyResponseComponent.toFront(true);

    // Set the AlwaysOnTop property
  //  frequencyResponseComponent.setAlwaysOnTop(true);
    //m_visualizer.setAlwaysOnTop(false);



    // Band 1 (default HP) — Freq / Gain / Q
    knob1.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible(knob1);
    knob1.setBounds(100,100, 40, 40);
    knob1.addListener(this);
    highpassCutoffAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "highpassCutoff", knob1);

    knobHpGain.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible (knobHpGain);
    knobHpGain.setBounds (100, 100, 40, 40);
    knobHpGain.addListener (this);
    highpassGainAttachment = std::make_unique<SliderAttachment> (audioProcessor.treeState, "highpassGain", knobHpGain);

    knob2.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible(knob2);
    knob2.setBounds(100, 100, 40, 40);
    knob2.addListener(this);
    highpassQAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "highpassQ", knob2);

    // Band 8 (default LP) — Freq / Gain / Q
    knob3.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible(knob3);
    knob3.setBounds(100, 100, 40, 40);
    knob3.addListener(this);
    lowpassCutoffAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "lowpassCutoff", knob3);

    knobLpGain.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible (knobLpGain);
    knobLpGain.setBounds (100, 100, 40, 40);
    knobLpGain.addListener (this);
    lowpassGainAttachment = std::make_unique<SliderAttachment> (audioProcessor.treeState, "lowpassGain", knobLpGain);

    knob4.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    addAndMakeVisible(knob4);
    knob4.setBounds(100, 100, 40, 40);
    knob4.addListener(this);
    lowpassQAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "lowpassQ", knob4);

     
    int yOffset = -15;
   
    // HighShelf
    // Configure highShelfFrequencySlider
    knob5.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob5.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob5);
    knob5.addListener(this);
    highShelfFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "highShelfFrequency", knob5);
    
    // Configure highShelfGainSlider
    knob6.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob6.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob6);
    knob6.addListener(this);
    highShelfGainAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "highShelfGain", knob6);

    // Configure highShelfQSlider
    knob7.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob7.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob7);
    knob7.addListener(this);
    highShelfQAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "highShelfQ", knob7);

    // Configure lowShelfFrequencySlider
    knob8.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob8.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob8);
    knob8.addListener(this);
    lowShelfFrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "lowShelfFrequency", knob8);

    // Configure lowShelfGainSlider
    knob9.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob9.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob9);
    knob9.addListener(this);
    lowShelfGainAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "lowShelfGain", knob9);

    // Configure lowShelfQSlider
    knob10.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob10.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob10);
    knob10.addListener(this);
    lowShelfQAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "lowShelfQ", knob10);

    // Band 1
   // Configure band1FrequencySlider
    knob11.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob11.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob11);
    knob11.addListener(this);
    band1FrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band1Frequency", knob11);

    // Configure band1GainSlider
    knob12.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob12.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob12);
    knob12.addListener(this);
    band1GainAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band1Gain", knob12);

   

    // Configure band1QSlider
    knob13.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob13.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob13);
    knob13.addListener(this);
    band1QAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band1Q", knob13);

    // Band 2
    // Configure band2FrequencySlider
    knob14.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob14.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob14);
    knob14.addListener(this);
    band2FrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band2Frequency", knob14);

    // Configure band2GainSlider
    knob15.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob15.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob15);
    knob15.addListener(this);
    band2GainAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band2Gain", knob15);

    // Configure band2QSlider
    knob16.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob16.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob16);
    knob16.addListener(this);
    band2QAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band2Q", knob16);

    // Band 3
    // Configure band3FrequencySlider
    knob17.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob17.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob17);
    knob17.addListener(this);
    band3FrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band3Frequency", knob17);

    // Configure band3GainSlider
    knob18.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob18.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob18);
    knob18.addListener(this);
    band3GainAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band3Gain", knob18);

    // Configure band3QSlider
    knob19.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob19.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob19);
    knob19.addListener(this);
    band3QAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band3Q", knob19);

    // Band 4
    // Configure band4FrequencySlider
    knob20.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob20.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob20);
    knob20.addListener(this);
    band4FrequencyAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band4Frequency", knob20);

    // Configure band4GainSlider
    knob21.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob21.setBounds(0, 0, 100, 100 - yOffset);
    addAndMakeVisible(knob21);
    knob21.addListener(this);
    band4GainAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band4Gain", knob21);

    // Configure band4QSlider
    knob22.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    knob22.setBounds(0, 0, 100, 100 - yOffset);

    addAndMakeVisible(knob22);
    knob22.addListener(this);
    band4QAttachment = std::make_unique<SliderAttachment>(audioProcessor.treeState, "band4Q", knob22);

    // Faceplate: click opens/focuses OptionBox; wheel cycles HP/LP slope when applicable.
    juce::Slider* faceplateKnobs[] = {
        &knob1, &knobHpGain, &knob2,
        &knob3, &knobLpGain, &knob4,
        &knob5, &knob6, &knob7,
        &knob8, &knob9, &knob10,
        &knob11, &knob12, &knob13,
        &knob14, &knob15, &knob16,
        &knob17, &knob18, &knob19,
        &knob20, &knob21, &knob22
    };
    for (auto* k : faceplateKnobs)
        wireFaceplateKnobInteraction (*k);
    updateFaceplateSlopeWheelMode();

    // Output gain — bottom faceplate trim strip (expanded UI; laid out in resized)
    outputGainKnob.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    outputGainKnob.setBounds (0, 0, 100, 100 - yOffset);
    addAndMakeVisible (outputGainKnob);
    outputGainKnob.addListener (this);
    outputGainAttachment = std::make_unique<SliderAttachment> (audioProcessor.treeState, "outputGain", outputGainKnob);

    autoGainButton.setClickingTogglesState (true);
    autoGainButton.setTooltip ("Auto Gain - match output loudness to pre-EQ level");
    autoGainButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    autoGainButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    autoGainButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    autoGainButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (autoGainButton);
    autoGainAttachment = std::make_unique<ButtonAttachment> (audioProcessor.treeState, "autoGain", autoGainButton);

    sideCheckButton.setClickingTogglesState (true);
    sideCheckButton.setTooltip (
        "Side Check - when Side is louder than Mid in a band, attenuate that side energy (HQ = BP lattice; HQ off = 3-band shelf/bell)");
    sideCheckButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    sideCheckButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    sideCheckButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    sideCheckButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addAndMakeVisible (sideCheckButton);
    sideCheckAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.treeState, SideCheck::enabledParamId(), sideCheckButton);

    sideCheckAmountSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    sideCheckAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    sideCheckAmountSlider.setRange (SideCheck::kMinAmount, SideCheck::kMaxAmount, 0.01);
    sideCheckAmountSlider.setTooltip ("Amount - Side Check strength (how hard Side is tucked when louder than Mid)");
    sideCheckAmountSlider.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGBA (40, 35, 28, 255));
    sideCheckAmountSlider.setColour (juce::Slider::trackColourId, juce::Colour::fromRGBA (180, 150, 55, 220));
    sideCheckAmountSlider.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGBA (220, 200, 120, 255));
    addChildComponent (sideCheckAmountSlider);
    sideCheckAmountAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.treeState, SideCheck::amountParamId(), sideCheckAmountSlider);

    // Fast / Med / Slow ballistics — toggle button shows current state (no ComboBox).
    sideCheckSpeedButton.setClickingTogglesState (false);
    sideCheckSpeedButton.setTooltip (SideCheck::speedTooltipForMode (SideCheck::fast));
    sideCheckSpeedButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    sideCheckSpeedButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    sideCheckSpeedButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    sideCheckSpeedButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    sideCheckSpeedButton.onClick = [this] { toggleSideCheckSpeed(); };
    addChildComponent (sideCheckSpeedButton);
    updateSideCheckSpeedButtonText();

    // HQ: lit = full BP lattice (default); off = 3-band shelf/bell eco path.
    sideCheckHqButton.setClickingTogglesState (true);
    sideCheckHqButton.setTooltip (
        "HQ - on: full bandpass Side Check (default). Off: lighter 3-band mode "
        "(low shelf region below 500 Hz, bell mid, high shelf region above 3 kHz).");
    sideCheckHqButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    sideCheckHqButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    sideCheckHqButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    sideCheckHqButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    addChildComponent (sideCheckHqButton);
    sideCheckHqAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.treeState, SideCheck::hqParamId(), sideCheckHqButton);

    // Compact HP / LP — same OptionBox attach path (copy NormalisableRange skew before SliderAttachment).
    // Range is 0 Hz…20 kHz (SideCheck::kMinHpLpHz / kMaxFreqHz); HP defaults to 0 (fully open).
    auto attachSideCheckFreqKnob = [this] (RotaryImageKnobForOptionBox& knob,
                                           const juce::String& paramID,
                                           const juce::String& tip,
                                           std::unique_ptr<SliderAttachment>& attachment)
    {
        knob.setCompactNoValueBox (true);
        knob.setTooltip (tip);
        knob.setTextValueSuffix (" Hz");
        knob.addListener (this);
        addChildComponent (knob);

        // Always seed the skewed log-ish range before attachment so the knob
        // never keeps the ctor's placeholder 0.36–0.80 range.
        knob.setNormalisableRange (juce::NormalisableRange<double> (
            (double) SideCheck::kMinHpLpHz,
            (double) SideCheck::kMaxFreqHz,
            1.0,
            0.2));

        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (
                audioProcessor.treeState.getParameter (paramID)))
        {
            const auto range = param->getNormalisableRange();
            knob.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start,
                (double) range.end,
                (double) range.interval,
                (double) range.skew,
                range.symmetricSkew));
        }

        attachment = std::make_unique<SliderAttachment> (audioProcessor.treeState, paramID, knob);
    };
    attachSideCheckFreqKnob (sideCheckHpKnob, SideCheck::hpHzParamId(),
        "HP - Side Check highpass (0 Hz–20 kHz; effect starts above this frequency; default 0 = fully open)", sideCheckHpAttachment);
    attachSideCheckFreqKnob (sideCheckLpKnob, SideCheck::lpHzParamId(),
        "LP - Side Check lowpass (effect stops below this frequency)", sideCheckLpAttachment);

    auto setupSideCheckRangeLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::NotificationType::dontSendNotification);
        label.setFont (juce::Font ("Lato Black", 10.0f, juce::Font::plain));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.85f));
        label.setInterceptsMouseClicks (false, false);
        addChildComponent (label);
    };
    setupSideCheckRangeLabel (sideCheckHpLabel, "HP");
    setupSideCheckRangeLabel (sideCheckLpLabel, "LP");

    audioProcessor.treeState.addParameterListener (SideCheck::enabledParamId(), this);
    audioProcessor.treeState.addParameterListener (SideCheck::modeParamId(), this);
    for (const char* typeId : { "highpassType", "lowpassType", "band1Type", "band2Type",
                                "band3Type", "band4Type", "highShelfType", "lowShelfType" })
        audioProcessor.treeState.addParameterListener (typeId, this);
    updateSideCheckAmountVisibility();
    updateBandFaceplateGainVisibility();

    // Bottom chrome: ? toggles tooltips for the whole plugin (default on; prefs next to analyser_defaults).
    helpTooltipsButton.setClickingTogglesState (true);
    helpTooltipsButton.setTooltip ("Tooltips - click to turn help tips on or off");
    helpTooltipsButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    helpTooltipsButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    helpTooltipsButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    helpTooltipsButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    helpTooltipsButton.onClick = [this]
    {
        tooltipsEnabled = helpTooltipsButton.getToggleState();
        applyTooltipsEnabled();
        saveUiPrefs();
    };
    addAndMakeVisible (helpTooltipsButton);

    // Bottom chrome: Phase / Processing Mode (preset-style popup, not ComboBox).
    phaseModeCombo.setTooltip ("Phase - Minimum Phase is zero-latency IIR EQ. Linear Phase matches the EQ magnitude with constant group delay (higher latency).");
    addAndMakeVisible (phaseModeCombo);

    loadUiPrefs();
    applyTooltipsEnabled();
   

    //addAndMakeVisible(testButton);
    //testButton.setButtonText("Test");
    //testButton.setBounds(150, 750, 100, 50);
   /// testButton.addListener(this);



    addAndMakeVisible(*onOffButton1);
    onOffButton1->setBaseColor(onOffButtonOrange);
    onOffButton1->setBounds(0, 0, 100, 100);
    onOffButton1->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton1->addListener(this);

    addAndMakeVisible(*onOffButton2);
    onOffButton2->setBaseColor(onOffButtonOrange);
    onOffButton2->setBounds(0, 0, 100, 100);
    onOffButton2->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton2->addListener(this);

    addAndMakeVisible(*onOffButton3);
    onOffButton3->setBaseColor(onOffButtonOrange);
    onOffButton3->setBounds(0, 0, 100, 100);
    onOffButton3->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton3->addListener(this);

    addAndMakeVisible(*onOffButton4);
    onOffButton4->setBaseColor(onOffButtonOrange);
    onOffButton4->setBounds(0, 0, 100, 100);
    onOffButton4->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton4->addListener(this);

    addAndMakeVisible(*onOffButton5);
    onOffButton5->setBaseColor(onOffButtonOrange);
    onOffButton5->setBounds(0, 0, 100, 100);
    onOffButton5->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton5->addListener(this);

    addAndMakeVisible(*onOffButton6);
    onOffButton6->setBaseColor(onOffButtonOrange);
    onOffButton6->setBounds(0, 0, 100, 100);
    onOffButton6->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton6->addListener(this);

    addAndMakeVisible(*onOffButton7);
    onOffButton7->setBaseColor(onOffButtonOrange);
    onOffButton7->setBounds(0, 0, 100, 100);
    onOffButton7->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton7->addListener(this);

    addAndMakeVisible(*onOffButton8);
    onOffButton8->setBaseColor(onOffButtonOrange);
    onOffButton8->setBounds(0, 0, 100, 100);
    onOffButton8->setToggleState(true, juce::NotificationType::dontSendNotification);
    onOffButton8->addListener(this);


    addAndMakeVisible(border1);
    addAndMakeVisible(border2);
    addAndMakeVisible(border3);
    addAndMakeVisible(border4);
    addAndMakeVisible(border5);
    addAndMakeVisible(border6);
    addAndMakeVisible(border7);
    addAndMakeVisible(border8);

    addAndMakeVisible(border9);
    addAndMakeVisible(border10);
    addAndMakeVisible(border11);
    addAndMakeVisible(border12);
    addAndMakeVisible(border13);
    addAndMakeVisible(border14);
    addAndMakeVisible(border15);
    addAndMakeVisible(border16);

    addAndMakeVisible(border17);
    addAndMakeVisible(border18);
    addAndMakeVisible(border19);
    addAndMakeVisible(border20);
    addAndMakeVisible(border21);
    addAndMakeVisible(border22);
    addAndMakeVisible(borderOutputGain);


    border1.setText("Freq");
    border2.setText("Freq");
    border3.setText("Freq");
    border4.setText("Freq");
    border5.setText("Freq");
    border6.setText("Freq");
    border7.setText("Freq");
    border8.setText("Freq");
  
    border9.setText("Q");
    border10.setText("Q");
    border11.setText("Q");
    border12.setText("Q");
    border13.setText("Q");
    border14.setText("Q");
    border15.setText("Q");
    border16.setText("Q");

    border17.setText("Gain");
    border18.setText("Gain");
    border19.setText("Gain");
    border20.setText("Gain");
    border21.setText("Gain");
    border22.setText("Gain");
    borderOutputGain.setText("Out");
    border17.setTextLabelPosition(juce::Justification::centred);
    // Side label next to the compact trim-strip knob (not above)
    borderOutputGain.setTextLabelPosition(juce::Justification::centredRight);

    border1.setTextLabelPosition(juce::Justification::centred);
    border2.setTextLabelPosition(juce::Justification::centred);
    border3.setTextLabelPosition(juce::Justification::centred);
    border4.setTextLabelPosition(juce::Justification::centred);
    border5.setTextLabelPosition(juce::Justification::centred);
    border6.setTextLabelPosition(juce::Justification::centred);
    border7.setTextLabelPosition(juce::Justification::centred);
    border8.setTextLabelPosition(juce::Justification::centred);
    border9.setTextLabelPosition(juce::Justification::centred);
    border10.setTextLabelPosition(juce::Justification::centred);
    border11.setTextLabelPosition(juce::Justification::centred);
    border12.setTextLabelPosition(juce::Justification::centred);
    border13.setTextLabelPosition(juce::Justification::centred);
    border14.setTextLabelPosition(juce::Justification::centred);
    border15.setTextLabelPosition(juce::Justification::centred);
    border16.setTextLabelPosition(juce::Justification::centred);
    border17.setTextLabelPosition(juce::Justification::centred);
    border18.setTextLabelPosition(juce::Justification::centred);
    border19.setTextLabelPosition(juce::Justification::centred);
    border20.setTextLabelPosition(juce::Justification::centred);
    border21.setTextLabelPosition(juce::Justification::centred);
    border22.setTextLabelPosition(juce::Justification::centred);



    // Define colors for the outline and text
    juce::Colour outlineColor = juce::Colours::white.withAlpha(0.0f);
    juce::Colour textColor(155, 155, 155); // This is the text color

    // Set the colors for border1 (assuming border1 is your GroupComponent)
    border1.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border1.setColour(juce::GroupComponent::textColourId, textColor);
    border2.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border2.setColour(juce::GroupComponent::textColourId, textColor);
    border3.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border3.setColour(juce::GroupComponent::textColourId, textColor);
    border4.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border4.setColour(juce::GroupComponent::textColourId, textColor);
    border5.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border5.setColour(juce::GroupComponent::textColourId, textColor);
    border6.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border6.setColour(juce::GroupComponent::textColourId, textColor);
    border7.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border7.setColour(juce::GroupComponent::textColourId, textColor);
    border8.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border8.setColour(juce::GroupComponent::textColourId, textColor);

    border9.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border9.setColour(juce::GroupComponent::textColourId, textColor);
    border10.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border10.setColour(juce::GroupComponent::textColourId, textColor);
    border11.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border11.setColour(juce::GroupComponent::textColourId, textColor);
    border12.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border12.setColour(juce::GroupComponent::textColourId, textColor);
    border13.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border13.setColour(juce::GroupComponent::textColourId, textColor);
    border14.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border14.setColour(juce::GroupComponent::textColourId, textColor);
    border15.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border15.setColour(juce::GroupComponent::textColourId, textColor);
    border16.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border16.setColour(juce::GroupComponent::textColourId, textColor);
    border17.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border17.setColour(juce::GroupComponent::textColourId, textColor);
    border18.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border18.setColour(juce::GroupComponent::textColourId, textColor);
    border19.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border19.setColour(juce::GroupComponent::textColourId, textColor);
    border20.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border20.setColour(juce::GroupComponent::textColourId, textColor);
    border21.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border21.setColour(juce::GroupComponent::textColourId, textColor);
    border22.setColour(juce::GroupComponent::outlineColourId, outlineColor);
    border22.setColour(juce::GroupComponent::textColourId, textColor);
    // No outline — used as a side text label beside the trim-strip output knob
    borderOutputGain.setColour(juce::GroupComponent::outlineColourId, juce::Colours::transparentBlack);
    borderOutputGain.setColour(juce::GroupComponent::textColourId, textColor);

    // Brand wordmark is parented in layoutBrandWordmark(): under MainComponent in compact
    // mode (beneath Settings/menu), on the editor faceplate trim when expanded.
    addChildComponent (brandWordmark);

    // Default: graph-only compact UI (faceplate hidden). Toggle still expands via ▲/▼.
    applyCompactUi();
}
 
EqEditor::~EqEditor()
{
    stopTimer();

    audioProcessor.treeState.removeParameterListener (SideCheck::enabledParamId(), this);
    audioProcessor.treeState.removeParameterListener (SideCheck::modeParamId(), this);
    for (const char* typeId : { "highpassType", "lowpassType", "band1Type", "band2Type",
                                "band3Type", "band4Type", "highShelfType", "lowShelfType" })
        audioProcessor.treeState.removeParameterListener (typeId, this);
    sideCheckHpKnob.removeListener (this);
    sideCheckLpKnob.removeListener (this);
    sideCheckHpAttachment.reset();
    sideCheckLpAttachment.reset();
    sideCheckAmountAttachment.reset();
    sideCheckHqAttachment.reset();
    sideCheckAttachment.reset();

    //openGLContext.detach();



    juce::Slider* faceplateKnobs[] = {
        &knob1, &knobHpGain, &knob2,
        &knob3, &knobLpGain, &knob4,
        &knob5, &knob6, &knob7,
        &knob8, &knob9, &knob10,
        &knob11, &knob12, &knob13,
        &knob14, &knob15, &knob16,
        &knob17, &knob18, &knob19,
        &knob20, &knob21, &knob22
    };
    for (auto* k : faceplateKnobs)
    {
        k->removeListener (this);
        k->removeMouseListener (static_cast<juce::Component*> (this));
    }
    outputGainKnob.removeListener(this);



}

void EqEditor::paint(juce::Graphics& g)
{
    if (uiCompact)
    {
        g.fillAll (juce::Colours::black);

        const int titleBarHeight = getTopBandHeight();
        juce::Colour titleBarColor = juce::Colour::fromRGB (110, 90, 80);
        juce::Colour titleBarColor2 = juce::Colour::fromRGB (10, 10, 10);
        juce::ColourGradient gradient2 (titleBarColor,
            juce::Point<float> ((float) getWidth() * 0.5f, 0.0f),
            titleBarColor2,
            juce::Point<float> ((float) getWidth(), (float) getHeight()),
            true);
        g.setGradientFill (gradient2);
        g.fillRect (0, 0, getWidth(), titleBarHeight);
        g.setColour (juce::Colours::black);
        g.drawLine (0.0f, (float) titleBarHeight, (float) getWidth(), (float) titleBarHeight, 4.0f);
        return;
    }

    // Match resized() stack: top band → graph → optional mod → faceplate → trim.
    const int titleBarHeight = getTopBandHeight();
    const int trimH = getBottomTrimHeight();
    const int faceplateStripH = getFaceplateHeightForWidth (getWidth());
    const int modStripH = modPanelOpen ? getModPanelHeightForGraphHeight (0) : 0;
    const int availableBelowTop = juce::jmax (1, getHeight() - titleBarHeight - trimH);
    int graphH = availableBelowTop - faceplateStripH - modStripH;
    int modHForLayout = modStripH;
    if (graphH < 180)
    {
        graphH = 180;
        const int remaining = juce::jmax (1, availableBelowTop - graphH);
        if (modPanelOpen)
        {
            // Preserve mod = 3/4 faceplate ratio when packing.
            const int faceH = juce::jmax (1, (remaining * 4) / 7);
            modHForLayout = juce::jmax (1, remaining - faceH);
        }
        else
        {
            modHForLayout = 0;
        }
    }
    const int titleBarY = titleBarHeight + graphH + modHForLayout;


    // Define custom RGB colors
    juce::Colour color1 = juce::Colour::fromRGB(70, 50, 35);  // Inner color
    juce::Colour color2 = juce::Colour::fromRGB(0, 0, 0); // Outer color

    // Create a ColourGradient object
    juce::ColourGradient gradient(color1,                           // Inner color
        juce::Point<float>(static_cast<float>(getWidth()) / 2.0f, 0.0f),                          // Start position (top-left corner)
        color2,                           // Outer color
        juce::Point<float>(static_cast<float>(getWidth()), static_cast<float>(getHeight())), // End position (bottom-right corner)
        true);                            // Radial gradient


    g.setGradientFill(gradient);
    g.fillRect(0, 0, getWidth(), getHeight());




// Define custom RGB colors
    juce::Colour faceplateColor1 = juce::Colour::fromRGB(20, 15, 10);  // Inner color
    juce::Colour faceplaceColor2 = juce::Colour::fromRGB(0, 0, 0); // Outer color

    // Create a ColourGradient object
    juce::ColourGradient gradient3(faceplateColor1,                           // Inner color
        juce::Point<float>(static_cast<float>(getWidth()) / 2.0f, 0.0f),                          // Start position (top-left corner)
        faceplaceColor2,                           // Outer color
        juce::Point<float>(static_cast<float>(getWidth()), static_cast<float>(getHeight())), // End position (bottom-right corner)
        true);                            // Radial gradient


    g.setGradientFill(gradient);
    g.fillRect(0, 0, getWidth(), getHeight());
    g.fillRect(0, 0, getWidth(), getHeight());
  

    // Title Bar Colors
    juce::Colour titleBarColor = juce::Colour::fromRGB(110, 90, 80);  // Inner color
    juce::Colour titleBarColor2 = juce::Colour::fromRGB(10, 10, 10); // Outer color

    juce::ColourGradient gradient2(titleBarColor,
        juce::Point<float>(static_cast<float>(getWidth()) / 2.0f, 0.0f),
        titleBarColor2,
        juce::Point<float>(static_cast<float>(getWidth()), static_cast<float>(getHeight())),
        true);

    g.setGradientFill(gradient2);
    g.fillRect(0, 0, getWidth(), titleBarHeight);
    g.fillRect(0, titleBarY, getWidth(), titleBarHeight);
    g.fillRect(0, getHeight() - trimH, getWidth(), trimH);

    g.setColour(color2);

    g.drawLine(0, titleBarHeight, getWidth(), titleBarHeight, 4);
    g.drawLine(0, titleBarY + titleBarHeight, getWidth(), titleBarY + titleBarHeight, 2);
}



void EqEditor::resized()
{
    // Keep horizontal spacing proportional to the 1200 design width; height is free.
    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    if (uiCompact)
    {
        int mainH = getHeight();
        int modH = 0;
        if (modPanelOpen && modSection != nullptr)
        {
            modH = getModPanelHeightForGraphHeight (0);
            mainH = juce::jmax (120, getHeight() - modH);
        }

        if (mainComponent != nullptr)
            mainComponent->setBounds (0, 0, getWidth(), mainH);

        if (modSection != nullptr)
        {
            modSection->setVisible (modPanelOpen);
            if (modPanelOpen)
            {
                // Use the fixed mod height (not leftover), so resize can't stretch the LFO panel.
                modSection->setBounds (0, mainH, getWidth(), modH);
                // Above faceplate leftovers / chrome siblings so matrix combos stay clickable.
                modSection->toFront (false);
            }
        }

        // Graph-only: hide + park faceplate controls so none can paint over the spectrum.
        setFaceplateVisible (false);
        juce::Slider* faceplateKnobs[] = {
            &knob1, &knobHpGain, &knob2,
            &knob3, &knobLpGain, &knob4,
            &knob5, &knob6, &knob7,
            &knob8, &knob9, &knob10,
            &knob11, &knob12, &knob13,
            &knob14, &knob15, &knob16,
            &knob17, &knob18, &knob19,
            &knob20, &knob21, &knob22,
            &outputGainKnob
        };
        for (auto* k : faceplateKnobs)
            k->setBounds ({});

        layoutBrandWordmark (-1);
        layoutHelpTooltipsButton();
        layoutPhaseModeCombo();
        layoutSideCheckButton();
        return;
    }

    // Padding between groups and rows
    const int padding = px (10.0f);

    // Stack: graph (fills leftover) → mod (optional, 3/4 faceplate) → faceplate → bottom trim.
    const int graphTop = getTopBandHeight();
    const int trimH = getBottomTrimHeight();
    const int faceplateTopTrimH = getTopBandHeight(); // painted trim strip above knobs
    const int faceplateStripH = getFaceplateHeightForWidth (getWidth());
    const int modStripH = modPanelOpen ? getModPanelHeightForGraphHeight (0) : 0;
    const int availableBelowTop = juce::jmax (1, getHeight() - graphTop - trimH);
    int graphH = availableBelowTop - faceplateStripH - modStripH;
    int faceplateH = faceplateStripH;
    int modHForLayout = modStripH;
    if (graphH < 180)
    {
        graphH = 180;
        const int remaining = juce::jmax (1, availableBelowTop - graphH);
        if (modPanelOpen)
        {
            faceplateH = juce::jmax (1, (remaining * 4) / 7);
            modHForLayout = juce::jmax (1, remaining - faceplateH);
        }
        else
        {
            faceplateH = remaining;
            modHForLayout = 0;
        }
    }
    const int faceplateMinTop = graphTop + graphH + modHForLayout;
    const int bottomLimit = getHeight() - trimH - 2;
    faceplateH = juce::jmax (1, juce::jmin (faceplateH, bottomLimit - faceplateMinTop));
    const int faceplateBottom = faceplateMinTop + faceplateH;

    const int totalWidth = getWidth();
    // Integer groupWidth leaves a remainder that used to sit entirely on the right
    // (colX1 = 0), so the faceplate looked left-heavy / Band 1 clipped. Reserve a
    // small side margin, then centre the used block so left/right padding match.
    const int sideMargin = px (5.0f);
    int availableWidth = juce::jmax (1, totalWidth - (7 * padding) - 2 * sideMargin);
    int groupWidth = availableWidth / 8;
    const int usedWidth = 8 * groupWidth + 7 * padding;
    int faceplateOriginX = juce::jmax (sideMargin, (totalWidth - usedWidth) / 2);

    // Power buttons sit 10px below the faceplate top trim; knobs 5px below power.
    const int onOffButtonSize = juce::jmax (12, groupWidth / 3);
    const int powerBelowTrim = px (10.0f);
    const int knobsBelowPower = px (5.0f);
    const int onOffY = faceplateMinTop + faceplateTopTrimH + powerBelowTrim;
    const int topKnobY = onOffY + onOffButtonSize + knobsBelowPower;

    // Label spacing under knobs (must match placeLabelUnderKnob below).
    const int labelGap = juce::jmax (2, juce::roundToInt (4.0f * scale));
    const int labelGapReserve = labelGap;
    const int belowLabelPad = px (2.5f); // under Gain label / under HP-LP Q label
    int contentBelowTopKnob = juce::jmax (1, faceplateBottom - topKnobY);
    int rowHeight = juce::jmax (1, contentBelowTopKnob / 2);

    // Faceplate image knobs must stay 1:1 — never stretch W/H independently.
    int largeSide = juce::jmin ((int) (groupWidth * 0.7f), (int) (rowHeight * 0.85f));
    int smallSide = juce::jmin (groupWidth / 2, (int) (rowHeight * 0.55f));
    int gainSide = juce::jmax (1, juce::roundToInt ((float) largeSide * 1.2f)); // gain knobs +20%

    // Visual centre sits a bit left; shift the whole cluster right by ~½ Freq/Q knob.
    faceplateOriginX += juce::jlimit (5, 10, smallSide / 2);

    // Shelf/band Freq+Q: under Gain label + 2.5px.
    // HP/LP stack centres on the same gain column as the middle bands.
    int bandLargeXOff = px (23.0f);
    int gainLabelH = juce::jmax (10, juce::roundToInt ((float) gainSide * 0.45f));
    int qLabelH = juce::jmax (10, juce::roundToInt ((float) largeSide * 0.45f));
    // Middle 6 bands: Freq/Q under Gain label + 2.5px, then nudged up 2.5px.
    int bandSmallY = topKnobY + gainSide + labelGap + gainLabelH + belowLabelPad - px (2.5f);
    int hpLpBottomY = topKnobY + largeSide + labelGap + qLabelH + belowLabelPad;

    const int labelHReserve = juce::jmax (10, juce::roundToInt ((float) smallSide * 0.45f));

    auto contentBottomAt = [&]()
    {
        return juce::jmax (hpLpBottomY + largeSide,
                           bandSmallY + smallSide)
               + labelGapReserve + labelHReserve;
    };

    // Pack into the faceplate strip if anything would spill into the trim.
    if (contentBottomAt() > faceplateBottom)
    {
        const int blockTop = topKnobY;
        const int needed = juce::jmax (1, contentBottomAt() - blockTop);
        const int available = juce::jmax (1, faceplateBottom - blockTop);

        if (available < needed)
        {
            const float fit = (float) available / (float) needed;
            auto scaleI = [fit] (int v) { return juce::jmax (1, juce::roundToInt ((float) v * fit)); };

            largeSide = scaleI (largeSide);
            smallSide = scaleI (smallSide);
            gainSide = juce::jmax (1, juce::roundToInt ((float) largeSide * 1.2f));
            gainLabelH = juce::jmax (8, juce::roundToInt ((float) gainSide * 0.45f));
            qLabelH = juce::jmax (8, juce::roundToInt ((float) largeSide * 0.45f));
            bandSmallY = topKnobY + gainSide + labelGap + gainLabelH + belowLabelPad - px (2.5f);
            hpLpBottomY = topKnobY + largeSide + labelGap + qLabelH + belowLabelPad;
        }
    }

    auto typeIsHpLp = [this] (const char* typeId, int fallbackType) -> bool
    {
        const int t = [&]() -> int
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    audioProcessor.treeState.getParameter (typeId)))
                return choice->getIndex();
            return fallbackType;
        }();
        return FilterType::isHpLp (t);
    };

    const bool band1HpLp = typeIsHpLp ("highpassType", FilterType::highpass);
    const bool band2HpLp = typeIsHpLp ("lowShelfType", FilterType::lowShelf);
    const bool band3HpLp = typeIsHpLp ("band1Type", FilterType::bell);
    const bool band4HpLp = typeIsHpLp ("band2Type", FilterType::bell);
    const bool band5HpLp = typeIsHpLp ("band3Type", FilterType::bell);
    const bool band6HpLp = typeIsHpLp ("band4Type", FilterType::bell);
    const bool band7HpLp = typeIsHpLp ("highShelfType", FilterType::highShelf);
    const bool band8HpLp = typeIsHpLp ("lowpassType", FilterType::lowpass);

    // Place shelf/band Freq (left) and Q (right) equally spaced about the gain centre.
    // Does not advance startX — caller controls column X (keeps bands 2–7 fixed).
    auto placeBandFreqQAroundGainAt = [&] (int colX,
                                           juce::Component& freqKnob,
                                           juce::Component& qKnob,
                                           juce::Component& gainKnob)
    {
        const int gainX = colX + bandLargeXOff;
        gainKnob.setBounds (gainX, topKnobY, gainSide, gainSide);

        const int gainCentreX = gainX + gainSide / 2;
        const int preferredHalfPitch = gainSide / 2 + smallSide / 2 + px (4.0f);
        const int maxLeft = gainCentreX - (colX + smallSide / 2);
        const int maxRight = (colX + groupWidth - smallSide / 2) - gainCentreX;
        const int halfPitch = juce::jmax (1, juce::jmin (preferredHalfPitch, maxLeft, maxRight));

        freqKnob.setBounds (gainCentreX - halfPitch - smallSide / 2, bandSmallY, smallSide, smallSide);
        qKnob.setBounds (gainCentreX + halfPitch - smallSide / 2, bandSmallY, smallSide, smallSide);
    };

    // Classic HP/LP: large Q on top, large Freq under Q label — same size/orientation as before.
    // Column X uses the same gain-centre alignment as the middle bands (brings 1 & 8 in a bit).
    auto placeHpLpVerticalAt = [&] (int colX,
                                    juce::Component& freqKnob,
                                    juce::Component& qKnob,
                                    juce::Component& gainKnob)
    {
        gainKnob.setBounds ({}); // parked — not used in 2-knob mode
        const int stackX = colX + bandLargeXOff
                           + (gainSide - largeSide) / 2; // centre stack on gain column
        qKnob.setBounds (stackX, topKnobY, largeSide, largeSide);
        freqKnob.setBounds (stackX, hpLpBottomY, largeSide, largeSide);
    };

    auto placeBandColumn = [&] (int colX, bool hpLp,
                                juce::Component& freqKnob,
                                juce::Component& qKnob,
                                juce::Component& gainKnob)
    {
        if (hpLp)
            placeHpLpVerticalAt (colX, freqKnob, qKnob, gainKnob);
        else
            placeBandFreqQAroundGainAt (colX, freqKnob, qKnob, gainKnob);
    };

    const int colPitch = groupWidth + padding;
    // Even pitch grid, centred in the editor (see faceplateOriginX above).
    const int colX1 = faceplateOriginX;
    const int colX2 = faceplateOriginX + colPitch;
    const int colX3 = faceplateOriginX + colPitch * 2;
    const int colX4 = faceplateOriginX + colPitch * 3;
    const int colX5 = faceplateOriginX + colPitch * 4;
    const int colX6 = faceplateOriginX + colPitch * 5;
    const int colX7 = faceplateOriginX + colPitch * 6;
    const int colX8 = faceplateOriginX + colPitch * 7;

    // All 8 columns: HP/LP → vertical 2-knob; otherwise 3-knob with gain.
    placeBandColumn (colX1, band1HpLp, knob1,  knob2,  knobHpGain);
    placeBandColumn (colX2, band2HpLp, knob8,  knob10, knob9);
    placeBandColumn (colX3, band3HpLp, knob11, knob13, knob12);
    placeBandColumn (colX4, band4HpLp, knob14, knob16, knob15);
    placeBandColumn (colX5, band5HpLp, knob17, knob19, knob18);
    placeBandColumn (colX6, band6HpLp, knob20, knob22, knob21);
    placeBandColumn (colX7, band7HpLp, knob5,  knob7,  knob6);
    placeBandColumn (colX8, band8HpLp, knob3,  knob4,  knobLpGain);

    updateBandFaceplateGainVisibility();

    // Output gain — small knob centered in the lighter bottom faceplate trim strip
    int outClusterLeftX = getWidth();
    {
        const int outTrimH = trimH;
        const int trimTop = getHeight() - outTrimH;

        // Previous bottom-right size/center (keep same horizontal region, ~1/3 scale)
        const int prevSize = juce::jmax (px (52.0f), (int) (groupWidth * 0.55f));
        const int prevX = getWidth() - prevSize - px (18.0f);
        const int centerX = prevX + prevSize / 2;

        // ~1/3 prior size, capped a few px under trim height so it sits inside the band
        const int outSize = juce::jlimit (1, outTrimH - 4, prevSize / 3);
        const int outX = centerX - outSize / 2;
        const int outY = trimTop + (outTrimH - outSize) / 2;
        outputGainKnob.setBounds (outX, outY, outSize, outSize);

        // Layout: [A] [Out] [knob] — button a few px under trim height
        const int btnSize = juce::jlimit (16, outTrimH - 6, outSize);
        const int labelW = px (28.0f);
        const int labelH = juce::jmin (px (18.0f), outTrimH - 2);
        const int gap = px (2.0f);

        const int labelX = outX - labelW - gap;
        const int labelY = trimTop + (outTrimH - labelH) / 2;
        borderOutputGain.setBounds (labelX, labelY, labelW, labelH);

        const int btnX = labelX - btnSize - gap;
        const int btnY = trimTop + (outTrimH - btnSize) / 2;
        autoGainButton.setBounds (btnX, btnY, btnSize, btnSize);

        outClusterLeftX = btnX;
    }

    layoutBrandWordmark (outClusterLeftX);
    layoutHelpTooltipsButton();
    layoutPhaseModeCombo();
    layoutSideCheckButton();

    int parentWidth = getWidth();
    int componentWidth = parentWidth;
    // Use the same stack metrics computed above (graph fills leftover height).
    const int componentHeight = graphH;
    const int modH = modHForLayout;
    const int xPos = (parentWidth - componentWidth) / 2;

    mainComponent->setBounds (xPos, graphTop, componentWidth, componentHeight);

    if (modSection != nullptr)
    {
        modSection->setVisible (modPanelOpen);
        if (modPanelOpen)
        {
            modSection->setBounds (xPos, graphTop + componentHeight, componentWidth, modH);
            // Stay above faceplate knobs/borders that can overlap the matrix hit-test area.
            modSection->toFront (false);
        }
    }
    //mainComponent->setBounds(getLocalBounds());
   
    //frequencyResponseComponent->setBounds(0, 0, getWidth(), getHeight() / 2);
 
    //m_visualizer.setBounds(0, getHeight() / 2, getWidth(), getHeight() / 2);


    // Labels sit just under their knobs (design spacing), clamped above the bottom trim.
    // Width can be wider than the knob (centered) so longer words like "Freq" don't become "...".
    const int labelBottomLimit = bottomLimit;
    auto placeLabelUnderKnob = [labelBottomLimit, labelGap, totalWidth] (juce::GroupComponent& label,
                                                                        const juce::Component& knob,
                                                                        float heightFracOfKnob,
                                                                        int minWidth)
    {
        const auto kb = knob.getBounds();
        if (kb.isEmpty())
            return;

        int h = juce::jmax (10, juce::roundToInt ((float) kb.getHeight() * heightFracOfKnob));
        int y = kb.getBottom() + labelGap;

        // Keep fully above the trim; shrink height if the window is very tight.
        if (y + h > labelBottomLimit)
        {
            h = juce::jmax (8, labelBottomLimit - y);
            if (h < 8)
            {
                h = 8;
                y = juce::jmax (0, labelBottomLimit - h);
            }
        }

        const int w = juce::jmax (kb.getWidth(), minWidth);
        int x = kb.getCentreX() - w / 2;
        x = juce::jlimit (0, juce::jmax (0, totalWidth - w), x);
        label.setBounds (x, y, w, h);
    };

    // Freq needs a bit more width than the small knobs provide when scaled down.
    const int freqLabelMinW = juce::jmax (groupWidth / 2, px (44.0f));
    const int gainLabelMinW = juce::jmax (groupWidth, px (48.0f));
    const int qLabelMinW = juce::jmax (groupWidth / 2, px (28.0f));

    // Freq labels
    placeLabelUnderKnob (border1, knob1, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border2, knob8, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border3, knob11, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border4, knob14, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border5, knob17, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border6, knob20, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border7, knob5, 0.45f, freqLabelMinW);
    placeLabelUnderKnob (border8, knob3, 0.45f, freqLabelMinW);

    // Q labels
    placeLabelUnderKnob (border9, knob2, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border10, knob10, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border11, knob13, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border12, knob16, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border13, knob19, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border14, knob22, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border15, knob7, 0.45f, qLabelMinW);
    placeLabelUnderKnob (border16, knob4, 0.45f, qLabelMinW);

    // Gain labels — only when that column is in 3-knob (gain) mode.
    auto placeGainLabel = [&] (juce::GroupComponent& label, juce::Component& gainKnob, bool hpLp)
    {
        if (hpLp)
            label.setBounds ({});
        else
            placeLabelUnderKnob (label, gainKnob, 0.45f, gainLabelMinW);
    };
    placeGainLabel (border17, knob9, band2HpLp);
    placeGainLabel (border18, knob12, band3HpLp);
    placeGainLabel (border19, knob15, band4HpLp);
    placeGainLabel (border20, knob18, band5HpLp);
    placeGainLabel (border21, knob21, band6HpLp);
    placeGainLabel (border22, knob6, band7HpLp);

    // Power buttons: evenly spaced on the same centres as Gain (3-knob) or top Q (HP/LP stack).
    auto placeOnOffCentered = [onOffY, onOffButtonSize] (juce::Component& btn, const juce::Component& anchor)
    {
        btn.setBounds (anchor.getBounds().getCentreX() - onOffButtonSize / 2,
                       onOffY,
                       onOffButtonSize,
                       onOffButtonSize);
    };

    placeOnOffCentered (*onOffButton1, band1HpLp ? static_cast<juce::Component&> (knob2)  : knobHpGain);
    placeOnOffCentered (*onOffButton4, band2HpLp ? static_cast<juce::Component&> (knob10) : knob9);
    placeOnOffCentered (*onOffButton5, band3HpLp ? static_cast<juce::Component&> (knob13) : knob12);
    placeOnOffCentered (*onOffButton6, band4HpLp ? static_cast<juce::Component&> (knob16) : knob15);
    placeOnOffCentered (*onOffButton7, band5HpLp ? static_cast<juce::Component&> (knob19) : knob18);
    placeOnOffCentered (*onOffButton8, band6HpLp ? static_cast<juce::Component&> (knob22) : knob21);
    placeOnOffCentered (*onOffButton3, band7HpLp ? static_cast<juce::Component&> (knob7)  : knob6);
    placeOnOffCentered (*onOffButton2, band8HpLp ? static_cast<juce::Component&> (knob4)  : knobLpGain);
}

void EqEditor::updateBandFaceplateGainVisibility()
{
    // Compact: setFaceplateVisible(false) already hid everything — don't fight it here.
    if (uiCompact)
    {
        updateFaceplateSlopeWheelMode();
        return;
    }

    auto typeUsesGain = [this] (const char* typeId, int fallbackType) -> bool
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                audioProcessor.treeState.getParameter (typeId)))
            return FilterType::usesGain (choice->getIndex());
        return FilterType::usesGain (fallbackType);
    };

    // Same rule for all 8 columns: show gain only when the filter type uses gain.
    knobHpGain.setVisible (typeUsesGain ("highpassType", FilterType::highpass));
    knob9.setVisible (typeUsesGain ("lowShelfType", FilterType::lowShelf));
    knob12.setVisible (typeUsesGain ("band1Type", FilterType::bell));
    knob15.setVisible (typeUsesGain ("band2Type", FilterType::bell));
    knob18.setVisible (typeUsesGain ("band3Type", FilterType::bell));
    knob21.setVisible (typeUsesGain ("band4Type", FilterType::bell));
    knob6.setVisible (typeUsesGain ("highShelfType", FilterType::highShelf));
    knobLpGain.setVisible (typeUsesGain ("lowpassType", FilterType::lowpass));

    updateFaceplateSlopeWheelMode();
}


void EqEditor::layoutBrandWordmark (int outClusterLeftX)
{
    brandWordmark.setVisible (true);
    brandWordmark.setCompactLook (uiCompact);

    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    if (uiCompact)
    {
        // Compact: host on MainComponent so Settings / menu paint above the wordmark.
        if (mainComponent != nullptr)
            mainComponent->hostBrandWordmark (brandWordmark);
        else
            addAndMakeVisible (brandWordmark);

        // Top-center of the graph UI — clear of minimize/Bypass/A–D (left) and Settings (right).
        // Extra width/height for 2× "with SideCheck™" + beta beside "GHOVLZ! EQ".
        const int logoH = juce::jmax (22, getTopBandHeight() - 2);
        const int logoW = juce::jmin (px (640.0f), juce::jmax (220, (getWidth() * 7) / 10));
        const int logoX = (getWidth() - logoW) / 2;
        const int logoY = 9; // was 4; +5px down
        brandWordmark.setBounds (logoX, logoY, logoW, logoH);

        if (mainComponent != nullptr)
            mainComponent->relayoutPresetChrome();
        return;
    }

    // Expanded: bottom faceplate trim lives on the editor, outside MainComponent.
    addAndMakeVisible (brandWordmark);

    constexpr int trimH = 30;
    const int trimTop = getHeight() - trimH;
    const int clearance = px (8.0f);
    const int openRight = juce::jmax (px (120.0f), outClusterLeftX - clearance);

    // Prefer true window center; clamp so the wordmark stays left of the A/Out cluster
    // and right of the help (?) button on the trim.
    // Extra width for 2× "with SideCheck™" + beta beside "GHOVLZ! EQ".
    const int maxLogoW = juce::jmin (px (660.0f), juce::jmax (80, openRight - px (16.0f)));
    const int logoH = juce::jmax (20, trimH - 4);
    const int logoW = maxLogoW;

    int centerX = getWidth() / 2;
    const int halfW = logoW / 2;
    if (centerX + halfW > openRight)
        centerX = openRight - halfW;
    const int helpLeftClearance = px (8.0f) + juce::jlimit (16, trimH - 6, px (20.0f)) + px (8.0f);
    if (centerX - halfW < helpLeftClearance)
        centerX = halfW + helpLeftClearance;

    const int logoX = centerX - halfW;
    // Vertically center in the bottom trim (same math as Out knob / A).
    const int logoY = trimTop + (trimH - logoH) / 2;
    brandWordmark.setBounds (logoX, logoY, logoW, logoH);

    // Expanded: logo is in the bottom trim; preset bar stays top-center on MainComponent.
    if (mainComponent != nullptr)
        mainComponent->relayoutPresetChrome();
}

void EqEditor::layoutHelpTooltipsButton()
{
    helpTooltipsButton.setVisible (true);
    addAndMakeVisible (helpTooltipsButton);

    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    constexpr int trimH = 30;
    const int btnSize = juce::jlimit (16, uiCompact ? 22 : (trimH - 6), px (20.0f));
    const int margin = px (8.0f);

    if (uiCompact)
    {
        // Sit just right of Proportional Q at the bottom of the spectrum graph
        // (mainComponent), not the editor — so Mod panel doesn't push these down.
        constexpr int pLeft = 18;
        constexpr int pW = 22;
        constexpr int gap = 4;
        constexpr int bottomMargin = 18;
        const int graphBottom = (mainComponent != nullptr) ? mainComponent->getBottom() : getHeight();
        const int x = pLeft + pW + gap;
        const int y = graphBottom - btnSize - bottomMargin;
        helpTooltipsButton.setBounds (x, y, btnSize, btnSize);
    }
    else
    {
        // Prefer bottom of spectrum graph when Mod is open; otherwise faceplate trim.
        if (modPanelOpen && mainComponent != nullptr)
        {
            constexpr int bottomMargin = 18;
            const int x = margin;
            const int y = mainComponent->getBottom() - btnSize - bottomMargin;
            helpTooltipsButton.setBounds (x, y, btnSize, btnSize);
        }
        else
        {
            const int trimTop = getHeight() - trimH;
            const int x = margin;
            const int y = trimTop + (trimH - btnSize) / 2;
            helpTooltipsButton.setBounds (x, y, btnSize, btnSize);
        }
    }

    helpTooltipsButton.toFront (false);
}

void EqEditor::layoutPhaseModeCombo()
{
    phaseModeCombo.setVisible (true);
    addAndMakeVisible (phaseModeCombo);

    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    constexpr int trimH = 30;
    const int comboH = juce::jlimit (16, uiCompact ? 22 : (trimH - 6), px (20.0f));
    const int comboW = px (132.0f);
    const int gap = px (6.0f);

    // Sit just right of the ? help button in both layouts.
    const int x = helpTooltipsButton.getRight() + gap;
    const int y = helpTooltipsButton.getY() + (helpTooltipsButton.getHeight() - comboH) / 2;
    phaseModeCombo.setBounds (x, y, comboW, comboH);
    phaseModeCombo.toFront (false);
}


void EqEditor::layoutSideCheckButton()
{
    sideCheckButton.setVisible (true);
    addAndMakeVisible (sideCheckButton);

    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    constexpr int trimH = 30;
    const int btnSize = juce::jlimit (16, uiCompact ? 22 : (trimH - 6), px (20.0f));
    const int btnW = juce::jmax (btnSize, px (uiCompact ? 64.0f : 72.0f));
    const int gap = px (6.0f);

    // Sit just right of Phase mode (which sits right of ?) in both layouts.
    const int x = phaseModeCombo.getRight() + gap;
    const int y = phaseModeCombo.getY() + (phaseModeCombo.getHeight() - btnSize) / 2;
    sideCheckButton.setBounds (x, y, btnW, btnSize);
    sideCheckButton.toFront (false);

    // Amount + Speed + HQ + HP/LP appear beside SideCheck only while enabled.
    const bool scOn = sideCheckButton.getToggleState()
        || (audioProcessor.treeState.getRawParameterValue (SideCheck::enabledParamId()) != nullptr
            && audioProcessor.treeState.getRawParameterValue (SideCheck::enabledParamId())->load() > 0.5f);
    sideCheckAmountSlider.setVisible (scOn);
    sideCheckSpeedButton.setVisible (scOn);
    sideCheckHqButton.setVisible (scOn);
    sideCheckHpKnob.setVisible (scOn);
    sideCheckLpKnob.setVisible (scOn);
    sideCheckHpLabel.setVisible (scOn);
    sideCheckLpLabel.setVisible (scOn);
    if (scOn)
    {
        const int sliderW = juce::jmax (px (44.0f), btnSize * 2);
        sideCheckAmountSlider.setBounds (sideCheckButton.getRight() + gap, y, sliderW, btnSize);
        sideCheckAmountSlider.toFront (false);

        // Compact Fast/Med/Slow toggle — short enough for both compact and expanded chrome.
        const int speedW = juce::jmax (px (48.0f), uiCompact ? 52 : 58);
        sideCheckSpeedButton.setBounds (sideCheckAmountSlider.getRight() + gap, y, speedW, btnSize);
        sideCheckSpeedButton.toFront (false);

        const int hqW = juce::jmax (btnSize, px (28.0f));
        sideCheckHqButton.setBounds (sideCheckSpeedButton.getRight() + gap, y, hqW, btnSize);
        sideCheckHqButton.toFront (false);

        // Compact rotary knobs (OptionBox A/R style), labels left of each knob.
        const int knobSize = btnSize;
        const int labelW = juce::jmax (px (16.0f), 14);
        const int labelGap = px (2.0f);
        const int pairGap = gap;

        int cx = sideCheckHqButton.getRight() + gap;
        sideCheckHpLabel.setBounds (cx, y, labelW, btnSize);
        cx += labelW + labelGap;
        sideCheckHpKnob.setBounds (cx, y, knobSize, knobSize);
        cx += knobSize + pairGap;
        sideCheckLpLabel.setBounds (cx, y, labelW, btnSize);
        cx += labelW + labelGap;
        sideCheckLpKnob.setBounds (cx, y, knobSize, knobSize);

        sideCheckHpLabel.toFront (false);
        sideCheckHpKnob.toFront (false);
        sideCheckLpLabel.toFront (false);
        sideCheckLpKnob.toFront (false);
    }
    else
    {
        // Park off-layout so a stale visible flag can never flash knobs at (0,0).
        sideCheckAmountSlider.setBounds ({});
        sideCheckSpeedButton.setBounds ({});
        sideCheckHqButton.setBounds ({});
        sideCheckHpKnob.setBounds ({});
        sideCheckLpKnob.setBounds ({});
        sideCheckHpLabel.setBounds ({});
        sideCheckLpLabel.setBounds ({});
    }
}

void EqEditor::updateSideCheckAmountVisibility()
{
    const bool scOn = sideCheckButton.getToggleState()
        || (audioProcessor.treeState.getRawParameterValue (SideCheck::enabledParamId()) != nullptr
            && audioProcessor.treeState.getRawParameterValue (SideCheck::enabledParamId())->load() > 0.5f);
    sideCheckAmountSlider.setVisible (scOn);
    sideCheckSpeedButton.setVisible (scOn);
    sideCheckHqButton.setVisible (scOn);
    sideCheckHpKnob.setVisible (scOn);
    sideCheckLpKnob.setVisible (scOn);
    sideCheckHpLabel.setVisible (scOn);
    sideCheckLpLabel.setVisible (scOn);
    if (scOn && getWidth() > 0)
        layoutSideCheckButton();
}

void EqEditor::updateSideCheckSpeedButtonText()
{
    const int mode = SideCheck::readModeIndex (audioProcessor.treeState, SideCheck::fast);
    const auto names = SideCheck::getModeChoiceNames();
    const juce::String text = names[juce::jlimit (0, names.size() - 1, mode)];
    sideCheckSpeedButton.setButtonText (text);
    sideCheckSpeedButton.setTooltip (SideCheck::speedTooltipForMode (mode));
}

void EqEditor::toggleSideCheckSpeed()
{
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
        audioProcessor.treeState.getParameter (SideCheck::modeParamId()));
    if (choice == nullptr)
        return;

    const int next = (choice->getIndex() + 1) % SideCheck::numSpeedModes;
    choice->beginChangeGesture();
    choice->setValueNotifyingHost (choice->convertTo0to1 ((float) next));
    choice->endChangeGesture();
    updateSideCheckSpeedButtonText();
}

void EqEditor::enforceSideCheckHpLpOrder (juce::Slider* changed)
{
    if (changed != &sideCheckHpKnob && changed != &sideCheckLpKnob)
        return;

    float hp = (float) sideCheckHpKnob.getValue();
    float lp = (float) sideCheckLpKnob.getValue();
    if (hp + SideCheck::kMinHpLpGapHz <= lp)
        return;

    if (changed == &sideCheckHpKnob)
    {
        const float newLp = juce::jmin (SideCheck::kMaxFreqHz, hp + SideCheck::kMinHpLpGapHz);
        if (std::abs (newLp - lp) > 0.5f)
            sideCheckLpKnob.setValue ((double) newLp, juce::sendNotificationSync);
    }
    else
    {
        const float newHp = juce::jmax (SideCheck::kMinHpLpHz, lp - SideCheck::kMinHpLpGapHz);
        if (std::abs (newHp - hp) > 0.5f)
            sideCheckHpKnob.setValue ((double) newHp, juce::sendNotificationSync);
    }
}
void EqEditor::applyTooltipsEnabled()
{
    helpTooltipsButton.setToggleState (tooltipsEnabled, juce::dontSendNotification);
    // Huge delay effectively disables tip display while leaving setTooltip() intact on controls.
    tooltipWindow.setMillisecondsBeforeTipAppears (tooltipsEnabled ? 500 : 0x7fffffff);
    if (! tooltipsEnabled)
        tooltipWindow.hideTip();
}

juce::File EqEditor::getUiPrefsFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("ParametricEq")
        .getChildFile ("ui_prefs.xml");
}

void EqEditor::loadUiPrefs()
{
    tooltipsEnabled = true;
    bool ecoEnabled = false;

    const auto file = getUiPrefsFile();
    if (file.existsAsFile())
    {
        if (auto xml = juce::parseXML (file))
        {
            if (xml->hasTagName ("UiPrefs"))
            {
                tooltipsEnabled = xml->getBoolAttribute ("tooltipsEnabled", true);
                ecoEnabled = xml->getBoolAttribute ("ecoEnabled", false);
            }
        }
    }

    if (mainComponent != nullptr)
        mainComponent->setEcoMode (ecoEnabled, false);
}

void EqEditor::saveUiPrefs() const
{
    auto xml = std::make_unique<juce::XmlElement> ("UiPrefs");
    xml->setAttribute ("tooltipsEnabled", tooltipsEnabled);
    xml->setAttribute ("ecoEnabled", mainComponent != nullptr && mainComponent->isEcoMode());
    xml->setAttribute ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));

    auto file = getUiPrefsFile();
    file.getParentDirectory().createDirectory();
    if (file.getParentDirectory().isDirectory())
        xml->writeTo (file);
}

void EqEditor::wireFaceplateKnobInteraction (juce::Slider& knob)
{
    // Component already is a MouseListener; avoid ambiguous EqEditor::MouseListener*.
    knob.addMouseListener (static_cast<juce::Component*> (this), false);
}

int EqEditor::faceplateBandIndexForSlider (const juce::Slider* slider) const noexcept
{
    if (slider == &knob1 || slider == &knobHpGain || slider == &knob2) return 4; // Band 1
    if (slider == &knob3 || slider == &knobLpGain || slider == &knob4) return 5; // Band 8
    if (slider == &knob5 || slider == &knob6 || slider == &knob7) return 6;     // Band 7
    if (slider == &knob8 || slider == &knob9 || slider == &knob10) return 7;    // Band 2
    if (slider == &knob11 || slider == &knob12 || slider == &knob13) return 0;  // Band 3
    if (slider == &knob14 || slider == &knob15 || slider == &knob16) return 1;  // Band 4
    if (slider == &knob17 || slider == &knob18 || slider == &knob19) return 2;  // Band 5
    if (slider == &knob20 || slider == &knob21 || slider == &knob22) return 3;  // Band 6
    return -1;
}

void EqEditor::openOptionBoxForFaceplateBand (int bandIndex)
{
    if (bandIndex < 0 || mainComponent == nullptr)
        return;
    mainComponent->getFrequencyResponseComponent().showOptionBoxForBand (bandIndex);
}

bool EqEditor::cycleFilterSlopeForBand (int bandIndex, int delta)
{
    const auto slopeID = FilterSlope::paramIDForBandIndex (bandIndex);
    if (slopeID.isEmpty())
        return false;

    const auto typeID = FilterType::paramIDForBandIndex (bandIndex);
    const int type = BandChannel::readChoiceIndex (
        audioProcessor.treeState, typeID, FilterType::defaultTypeForBandIndex (bandIndex));
    if (! FilterType::isHpLp (type))
        return false;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            audioProcessor.treeState.getParameter (slopeID)))
    {
        const int newIndex = juce::jlimit (0, FilterSlope::numChoices - 1,
                                           choice->getIndex() + delta);
        if (newIndex == choice->getIndex())
            return true;

        choice->beginChangeGesture();
        choice->setValueNotifyingHost (choice->convertTo0to1 ((float) newIndex));
        choice->endChangeGesture();
        return true;
    }
    return false;
}

void EqEditor::updateFaceplateSlopeWheelMode()
{
    auto typeIsHpLp = [this] (const juce::String& typeId, int fallback) -> bool
    {
        return FilterType::isHpLp (
            BandChannel::readChoiceIndex (audioProcessor.treeState, typeId, fallback));
    };

    auto setWheel = [] (juce::Slider& s, bool enable) { s.setScrollWheelEnabled (enable); };

    // When HP/LP, disable knob-value wheel so mouseWheelMove can cycle slope instead.
    const bool b1 = typeIsHpLp ("highpassType", FilterType::highpass);
    const bool b8 = typeIsHpLp ("lowpassType", FilterType::lowpass);
    const bool hs = typeIsHpLp ("highShelfType", FilterType::highShelf);
    const bool ls = typeIsHpLp ("lowShelfType", FilterType::lowShelf);
    const bool p1 = typeIsHpLp ("band1Type", FilterType::bell);
    const bool p2 = typeIsHpLp ("band2Type", FilterType::bell);
    const bool p3 = typeIsHpLp ("band3Type", FilterType::bell);
    const bool p4 = typeIsHpLp ("band4Type", FilterType::bell);

    setWheel (knob1, ! b1); setWheel (knobHpGain, ! b1); setWheel (knob2, ! b1);
    setWheel (knob3, ! b8); setWheel (knobLpGain, ! b8); setWheel (knob4, ! b8);
    setWheel (knob5, ! hs); setWheel (knob6, ! hs); setWheel (knob7, ! hs);
    setWheel (knob8, ! ls); setWheel (knob9, ! ls); setWheel (knob10, ! ls);
    setWheel (knob11, ! p1); setWheel (knob12, ! p1); setWheel (knob13, ! p1);
    setWheel (knob14, ! p2); setWheel (knob15, ! p2); setWheel (knob16, ! p2);
    setWheel (knob17, ! p3); setWheel (knob18, ! p3); setWheel (knob19, ! p3);
    setWheel (knob20, ! p4); setWheel (knob21, ! p4); setWheel (knob22, ! p4);
}

void EqEditor::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    auto* slider = dynamic_cast<juce::Slider*> (event.eventComponent);
    const int band = faceplateBandIndexForSlider (slider);
    if (band < 0 || wheel.deltaY == 0.0f)
        return;

    const int delta = (wheel.deltaY > 0.0f) ? 1 : -1;
    if (cycleFilterSlopeForBand (band, delta))
        return;
}

void EqEditor::sliderDragStarted (juce::Slider* slider)
{
    audioProcessor.getUndoManager().beginNewTransaction ("Parameter change");

    const int band = faceplateBandIndexForSlider (slider);
    if (band >= 0)
        openOptionBoxForFaceplateBand (band);

    if (mainComponent != nullptr)
        mainComponent->setOptionBoxInteractionFaded (true);
}

void EqEditor::sliderDragEnded (juce::Slider* slider)
{
    juce::ignoreUnused (slider);

    if (mainComponent != nullptr)
        mainComponent->setOptionBoxInteractionFaded (false);
}

void EqEditor::setBandManipulationHighlight (int bandIndex)
{
    auto setHighlight = [] (juce::Slider& slider, bool on)
    {
        KnobBandHighlight::setActive (slider, on);
    };

    // Clear all first
    setHighlight (knob1, false);
    setHighlight (knob2, false);
    setHighlight (knob3, false);
    setHighlight (knob4, false);
    setHighlight (knob5, false);
    setHighlight (knob6, false);
    setHighlight (knob7, false);
    setHighlight (knob8, false);
    setHighlight (knob9, false);
    setHighlight (knob10, false);
    setHighlight (knob11, false);
    setHighlight (knob12, false);
    setHighlight (knob13, false);
    setHighlight (knob14, false);
    setHighlight (knob15, false);
    setHighlight (knob16, false);
    setHighlight (knob17, false);
    setHighlight (knob18, false);
    setHighlight (knob19, false);
    setHighlight (knob20, false);
    setHighlight (knob21, false);
    setHighlight (knob22, false);

    switch (bandIndex)
    {
        case 0: // Band 1
            setHighlight (knob11, true);
            setHighlight (knob12, true);
            setHighlight (knob13, true);
            break;
        case 1: // Band 2
            setHighlight (knob14, true);
            setHighlight (knob15, true);
            setHighlight (knob16, true);
            break;
        case 2: // Band 3
            setHighlight (knob17, true);
            setHighlight (knob18, true);
            setHighlight (knob19, true);
            break;
        case 3: // Band 4
            setHighlight (knob20, true);
            setHighlight (knob21, true);
            setHighlight (knob22, true);
            break;
        case 4: // Highpass
            setHighlight (knob1, true);
            setHighlight (knob2, true);
            break;
        case 5: // Lowpass
            setHighlight (knob3, true);
            setHighlight (knob4, true);
            break;
        case 6: // High shelf
            setHighlight (knob5, true);
            setHighlight (knob6, true);
            setHighlight (knob7, true);
            break;
        case 7: // Low shelf
            setHighlight (knob8, true);
            setHighlight (knob9, true);
            setHighlight (knob10, true);
            break;
        default:
            break;
    }
}

void EqEditor::sliderValueChanged(juce::Slider* slider)
{
    //DBG("Slider value changed"); // Add this line for debugging

    if (slider == &sideCheckHpKnob || slider == &sideCheckLpKnob)
        enforceSideCheckHpLpOrder (slider);

    //Highpass
    if (slider == &knob1)
    {
        float cutoff = knob1.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("highpassQ");
        const int slope = (int) audioProcessor.treeState.getRawParameterValue ("highpassSlope")->load();
       audioProcessor.updateHighpass(cutoff, q, slope);
    }

    if (slider == &knob2)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("highpassCutoff");
        float q = knob2.getValue();
        const int slope = (int) audioProcessor.treeState.getRawParameterValue ("highpassSlope")->load();
        audioProcessor.updateHighpass(cutoff, q, slope);
    }



    // Lowpass
    if (slider == &knob3)
    {
        float cutoff = knob3.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("lowpassQ");
        const int slope = (int) audioProcessor.treeState.getRawParameterValue ("lowpassSlope")->load();
        audioProcessor.updateLowpass(cutoff, q, slope);
    }

    if (slider == &knob4)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("lowpassCutoff");
        float q = knob4.getValue();
        const int slope = (int) audioProcessor.treeState.getRawParameterValue ("lowpassSlope")->load();
        audioProcessor.updateLowpass(cutoff, q, slope);
    }



    // High Shelf
    if (slider == &knob5)
    {
        float cutoff = knob5.getValue();
        float gain = *audioProcessor.treeState.getRawParameterValue("highShelfGain");
        float q = *audioProcessor.treeState.getRawParameterValue("highShelfQ");
        audioProcessor.updateHighShelf(cutoff, q, gain);
    }

    if (slider == &knob6)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("highShelfFrequency");
        float gain = knob6.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("highShelfQ");
        audioProcessor.updateHighShelf(cutoff, q, gain);
    }

    if (slider == &knob7)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("highShelfFrequency");
        float gain = *audioProcessor.treeState.getRawParameterValue("highShelfGain");
        float q = knob7.getValue();
        audioProcessor.updateHighShelf(cutoff, q, gain);
    }




   // Low Shelf
    if (slider == &knob8)
    {
        float cutoff = knob8.getValue();
        float gain = *audioProcessor.treeState.getRawParameterValue("lowShelfGain");
        float q = *audioProcessor.treeState.getRawParameterValue("lowShelfQ");
        audioProcessor.updateLowShelf(cutoff, q, gain);
    }

    if (slider == &knob9)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("lowShelfFrequency");
        float gain = knob9.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("lowShelfQ");
        audioProcessor.updateLowShelf(cutoff, q, gain);
    }

    if (slider == &knob10)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("lowShelfFrequency");
        float gain = *audioProcessor.treeState.getRawParameterValue("lowShelfGain");
        float q = knob10.getValue();
        audioProcessor.updateLowShelf(cutoff, q, gain);
    }



    // Band 1
    if (slider == &knob11)
    {
        float cutoff = knob11.getValue();
        float gain = *audioProcessor.treeState.getRawParameterValue("band1Gain");
        float q = *audioProcessor.treeState.getRawParameterValue("band1Q");
        audioProcessor.updateBand1(cutoff, q, gain);

        juce::dsp::IIR::Coefficients<float> updatedCoefficients = audioProcessor.getBand1Coefficients();
        double sampleRate = audioProcessor.getSampleRate();
        frequencyResponseComponent->setCoefficients(updatedCoefficients, sampleRate);
    }

    if (slider == &knob12)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band1Frequency");
        float gain = knob12.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("band1Q");
        audioProcessor.updateBand1(cutoff, q, gain);

        juce::dsp::IIR::Coefficients<float> updatedCoefficients = audioProcessor.getBand1Coefficients();
        double sampleRate = audioProcessor.getSampleRate();
        frequencyResponseComponent->setCoefficients(updatedCoefficients, sampleRate);
    }

    if (slider == &knob13)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band1Frequency");
        float gain = *audioProcessor.treeState.getRawParameterValue("band1Gain");
        float q = knob13.getValue();
        audioProcessor.updateBand1(cutoff, q, gain);

        juce::dsp::IIR::Coefficients<float> updatedCoefficients = audioProcessor.getBand1Coefficients();
        double sampleRate = audioProcessor.getSampleRate();
        frequencyResponseComponent->setCoefficients(updatedCoefficients, sampleRate);
    }



    // Band 2
    if (slider == &knob14)
    {
        float cutoff = knob14.getValue();
        float gain = *audioProcessor.treeState.getRawParameterValue("band2Gain");
        float q = *audioProcessor.treeState.getRawParameterValue("band2Q");
        audioProcessor.updateBand2(cutoff, q, gain);
    }

    if (slider == &knob15)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band2Frequency");
        float gain = knob15.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("band2Q");
        audioProcessor.updateBand2(cutoff, q, gain);
    }

    if (slider == &knob16)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band2Frequency");
        float gain = *audioProcessor.treeState.getRawParameterValue("band2Gain");
        float q = knob16.getValue();
        audioProcessor.updateBand2(cutoff, q, gain);
    }



    // Band 3
    if (slider == &knob17)
    {
        float cutoff = knob17.getValue();
        float gain = *audioProcessor.treeState.getRawParameterValue("band3Gain");
        float q = *audioProcessor.treeState.getRawParameterValue("band3Q");
        audioProcessor.updateBand3(cutoff, q, gain);
    }

    if (slider == &knob18)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band3Frequency");
        float gain = knob18.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("band3Q");
        audioProcessor.updateBand3(cutoff, q, gain);
    }

    if (slider == &knob19)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band3Frequency");
        float gain = *audioProcessor.treeState.getRawParameterValue("band3Gain");
        float q = knob19.getValue();
        audioProcessor.updateBand3(cutoff, q, gain);
    }


    // Band 4
    if (slider == &knob20)
    {
        float cutoff = knob20.getValue();
        float gain = *audioProcessor.treeState.getRawParameterValue("band4Gain");
        float q = *audioProcessor.treeState.getRawParameterValue("band4Q");
        audioProcessor.updateBand4(cutoff, q, gain);
    }

    if (slider == &knob21)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band4Frequency");
        float gain = knob21.getValue();
        float q = *audioProcessor.treeState.getRawParameterValue("band4Q");
        audioProcessor.updateBand4(cutoff, q, gain);
    }

    if (slider == &knob22)
    {
        float cutoff = *audioProcessor.treeState.getRawParameterValue("band4Frequency");
        float gain = *audioProcessor.treeState.getRawParameterValue("band4Gain");
        float q = knob22.getValue();
        audioProcessor.updateBand4(cutoff, q, gain);
    }

}




void EqEditor::parameterChanged(const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == SideCheck::enabledParamId())
        updateSideCheckAmountVisibility();
    else if (parameterID == SideCheck::modeParamId())
        updateSideCheckSpeedButtonText();
    else if (parameterID.endsWithIgnoreCase ("Type"))
        resized(); // swap Band 1/8 between vertical HP/LP and 3-knob layouts
}

void EqEditor::initializeSharedImages()
{/*
    DBG("initializeSharedImages called");

    // Attempt to load the image from binary data
    darkKnob4_StitchedImage = juce::ImageCache::getFromMemory(BinaryData::DarkKnob4_Stitched_png, BinaryData::DarkKnob4_Stitched_pngSize);

   // DBG("InitializeSharedImages Called");

    // Check if the image loaded successfully   
    if (!darkKnob4_StitchedImage.isValid())
    {
        // Handle the error gracefully or provide a fallback image
        DBG("Failed to load DarkKnob4_Stitched_png");
        // You can set a fallback image here if needed.
    }
 
    else
    {
        DBG("DarkKnob4_Stitched_png loaded successfully");
    }

    repaint();
    */
}

void EqEditor::buttonClicked(juce::Button* button)
{
    // On/off buttons update their own APVTS parameter in OnOffButton1::clicked().
    // Here we only need to refresh the editor after a toggle.
    if (button == onOffButton1.get()
        || button == onOffButton2.get()
        || button == onOffButton3.get()
        || button == onOffButton4.get()
        || button == onOffButton5.get()
        || button == onOffButton6.get()
        || button == onOffButton7.get()
        || button == onOffButton8.get())
    {
        repaint();
    }
}
void EqEditor::timerCallback()
{
    if (!hasForcedRepaint)
    {
        hasForcedRepaint = true;
        stopTimer();  // Stop the timer
        repaint();    // Force a repaint
    }
}

void EqEditor::toggleCompactUi()
{
    uiCompact = ! uiCompact;
    applyCompactUi();
}

void EqEditor::toggleModPanel()
{
    modPanelOpen = ! modPanelOpen;
    applyCompactUi();
    if (mainComponent != nullptr)
        mainComponent->getFrequencyResponseComponent().syncModButton (modPanelOpen);
}

void EqEditor::syncModButton (bool isOpen)
{
    modPanelOpen = isOpen;
}

int EqEditor::getTopBandHeight() const
{
    const float scale = (float) getWidth() / (float) designWidth;
    return juce::jmax (22, juce::roundToInt (30.0f * scale));
}

int EqEditor::getGraphHeightForWidth (int width) const
{
    const float scale = (float) width / (float) designWidth;
    const int fullH = juce::roundToInt ((float) designHeight * scale);
    return juce::jmax (180, juce::roundToInt ((float) fullH / 1.9f));
}

int EqEditor::getModPanelHeightForGraphHeight (int graphHeight) const
{
    // 3/4 of the faceplate strip height — shortens expanded UI with mod open.
    const int gh = graphHeight > 0 ? graphHeight : getGraphHeightForWidth (getWidth());
    const int full = juce::jmax (190, juce::roundToInt ((float) gh * 0.62f));
    return juce::jmax (1, juce::roundToInt ((float) full * 0.75f));
}

int EqEditor::getFaceplateHeightForWidth (int width) const
{
    const int gh = getGraphHeightForWidth (width);
    return juce::jmax (190, juce::roundToInt ((float) gh * 0.62f));
}

int EqEditor::getBottomTrimHeight() const
{
    const float scale = (float) getWidth() / (float) designWidth;
    return juce::jmax (22, juce::roundToInt (30.0f * scale));
}

int EqEditor::getExpandedEditorHeight (int width, bool includeModPanel) const
{
    const float scale = (float) width / (float) designWidth;
    const int graphTop = juce::jmax (22, juce::roundToInt (30.0f * scale));
    const int graphH = getGraphHeightForWidth (width);
    const int stripH = getFaceplateHeightForWidth (width);
    const int modH = includeModPanel ? getModPanelHeightForGraphHeight (graphH) : 0;
    const int trimH = juce::jmax (22, juce::roundToInt (30.0f * scale));
    return graphTop + graphH + modH + stripH + trimH;
}

void EqEditor::setFaceplateVisible (bool shouldShow)
{
    auto setVis = [shouldShow] (juce::Component& c) { c.setVisible (shouldShow); };

    setVis (knob1);  setVis (knob2);  setVis (knob3);  setVis (knob4);
    setVis (knobHpGain); setVis (knobLpGain); // were missing — leaked onto graph in compact UI
    setVis (knob5);  setVis (knob6);  setVis (knob7);  setVis (knob8);
    setVis (knob9);  setVis (knob10); setVis (knob11); setVis (knob12);
    setVis (knob13); setVis (knob14); setVis (knob15); setVis (knob16);
    setVis (knob17); setVis (knob18); setVis (knob19); setVis (knob20);
    setVis (knob21); setVis (knob22);
    setVis (outputGainKnob);
    setVis (autoGainButton);

    setVis (border1);  setVis (border2);  setVis (border3);  setVis (border4);
    setVis (border5);  setVis (border6);  setVis (border7);  setVis (border8);
    setVis (border9);  setVis (border10); setVis (border11); setVis (border12);
    setVis (border13); setVis (border14); setVis (border15); setVis (border16);
    setVis (border17); setVis (border18); setVis (border19); setVis (border20);
    setVis (border21); setVis (border22);
    setVis (borderOutputGain);

    setVis (bandNumberLabel1); setVis (bandNumberLabel2); setVis (bandNumberLabel3);
    setVis (bandNumberLabel4); setVis (bandNumberLabel5); setVis (bandNumberLabel6);
    setVis (bandNumberLabel7); setVis (bandNumberLabel8);

    if (onOffButton1) setVis (*onOffButton1);
    if (onOffButton2) setVis (*onOffButton2);
    if (onOffButton3) setVis (*onOffButton3);
    if (onOffButton4) setVis (*onOffButton4);
    if (onOffButton5) setVis (*onOffButton5);
    if (onOffButton6) setVis (*onOffButton6);
    if (onOffButton7) setVis (*onOffButton7);
    if (onOffButton8) setVis (*onOffButton8);

    // Brand wordmark stays visible in both compact and expanded modes.
    // Parent / z-order are applied in layoutBrandWordmark() from resized()/applyCompactUi().
    brandWordmark.setVisible (true);
    brandWordmark.setCompactLook (! shouldShow);

    // Help (?), Phase, and Side Check stay visible in both compact and expanded.
    helpTooltipsButton.setVisible (true);
    phaseModeCombo.setVisible (true);
    sideCheckButton.setVisible (true);
    updateSideCheckAmountVisibility();
}

void EqEditor::applyCompactUi()
{
    setFaceplateVisible (! uiCompact);
    if (! uiCompact)
        updateBandFaceplateGainVisibility(); // restore per-type gain knobs after unhide

    if (mainComponent != nullptr)
        mainComponent->getFrequencyResponseComponent().syncUiModeButton (uiCompact);

    if (uiCompact)
    {
        savedEditorWidth = getWidth();
        savedEditorHeight = getHeight();

        const int w = juce::jmax (600, savedEditorWidth);
        const int graphH = getGraphHeightForWidth (w);
        const int modH = modPanelOpen ? getModPanelHeightForGraphHeight (graphH) : 0;
        const int h = graphH + modH;

        setConstrainer (nullptr);
        setSize (w, h);
    }
    else
    {
        const int w = savedEditorWidth > 0 ? savedEditorWidth : designWidth;
        const int h = savedEditorHeight > 0 ? savedEditorHeight
                                            : getExpandedEditorHeight (w, modPanelOpen);

        // Free aspect — any ratio within size limits.
        resizeConstrainer.setFixedAspectRatio (0.0);
        resizeConstrainer.setSizeLimits (900, 500, 2400, 1600);
        setConstrainer (&resizeConstrainer);
        setSize (w, juce::jmax (h, getExpandedEditorHeight (w, modPanelOpen)));
    }

    if (modSection != nullptr)
        modSection->setVisible (modPanelOpen);

    resized();
    repaint();
}
