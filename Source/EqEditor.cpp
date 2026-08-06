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
#include "BandNumberButton.h"
#include "EqEditor.h"
#include "KnobBandHighlight.h"
#include "ComboBoxLookAndFeel.h"
#include "FilterType.h"
#include "FilterSlope.h"
#include "EqBand.h"
#include "BandChannel.h"
#include "Menu/Theme.h"
#include "ColourRamp/Spec3DRampSequence.h"



using namespace ::juce::gl;

using namespace juce;

//juce::Image EqEditor::darkKnob4_StitchedImage;

using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;


EqEditor::EqEditor(EqProcessor& p, juce::AudioProcessorValueTreeState& treeState, Analyser& analyser)
    : AudioProcessorEditor(&p),
    audioProcessor(p),
    onOffButton1(std::make_unique<BandNumberButton>(treeState, "highpassOnOff")),
    onOffButton2(std::make_unique<BandNumberButton>(treeState, "lowpassOnOff")),
    onOffButton3(std::make_unique<BandNumberButton>(treeState, "highShelfOnOff")),
    onOffButton4(std::make_unique<BandNumberButton>(treeState, "lowShelfOnOff")),
    onOffButton5(std::make_unique<BandNumberButton>(treeState, "band1OnOff")),
    onOffButton6(std::make_unique<BandNumberButton>(treeState, "band2OnOff")),
    onOffButton7(std::make_unique<BandNumberButton>(treeState, "band3OnOff")),
    onOffButton8(std::make_unique<BandNumberButton>(treeState, "band4OnOff")),
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
    sideCheckButton.setLookAndFeel (&graphOverlayButtonLookAndFeel);
    sideCheckButton.setPaintingIsUnclipped (true);
    addAndMakeVisible (sideCheckButton);
    sideCheckAttachment = std::make_unique<ButtonAttachment> (
        audioProcessor.treeState, SideCheck::enabledParamId(), sideCheckButton);

    scopeModeButton.setClickingTogglesState (true);
    scopeModeButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    scopeModeButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    scopeModeButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    scopeModeButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    scopeModeButton.onClick = [this]
    {
        if (mainComponent != nullptr)
            mainComponent->setScopeMode (scopeModeButton.getToggleState(), true);
    };
    scopeModeButton.onPopupMenu = [this] { showScopeTapMenu(); };
    scopeModeButton.setLookAndFeel (&graphOverlayButtonLookAndFeel);
    scopeModeButton.setPaintingIsUnclipped (true);
    addAndMakeVisible (scopeModeButton);
    syncScopeModeButton();

    sideCheckAmountKnob.setCompactNoValueBox (true);
    sideCheckAmountKnob.setTooltip ("Amount - Side Check strength (how hard Side is tucked when louder than Mid)");
    sideCheckAmountKnob.setRange (SideCheck::kMinAmount, SideCheck::kMaxAmount, 0.01);
    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (
            audioProcessor.treeState.getParameter (SideCheck::amountParamId())))
    {
        const auto range = param->getNormalisableRange();
        sideCheckAmountKnob.setNormalisableRange (juce::NormalisableRange<double> (
            (double) range.start, (double) range.end, (double) range.interval,
            (double) range.skew, range.symmetricSkew));
    }
    addChildComponent (sideCheckAmountKnob);
    sideCheckAmountAttachment = std::make_unique<SliderAttachment> (
        audioProcessor.treeState, SideCheck::amountParamId(), sideCheckAmountKnob);

    // Fast / Med / Slow ballistics — toggle button shows current state (no ComboBox).
    sideCheckSpeedButton.setClickingTogglesState (false);
    sideCheckSpeedButton.setTooltip (SideCheck::speedTooltipForMode (SideCheck::fast));
    sideCheckSpeedButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    sideCheckSpeedButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    sideCheckSpeedButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    sideCheckSpeedButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    sideCheckSpeedButton.onClick = [this] { toggleSideCheckSpeed(); };
    sideCheckSpeedButton.setLookAndFeel (&graphOverlayButtonLookAndFeel);
    sideCheckSpeedButton.setPaintingIsUnclipped (true);
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
    sideCheckHqButton.setLookAndFeel (&graphOverlayButtonLookAndFeel);
    sideCheckHqButton.setPaintingIsUnclipped (true);
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
        "HP - Side Check highpass (0 Hz–20 kHz; effect starts above this frequency; default 0 = fully open). Right-click: slope", sideCheckHpAttachment);
    attachSideCheckFreqKnob (sideCheckLpKnob, SideCheck::lpHzParamId(),
        "LP - Side Check lowpass (effect stops below this frequency). Right-click: slope", sideCheckLpAttachment);
    sideCheckHpKnob.onPopupMenu = [this] { showSideCheckHpLpSlopeMenu (true); };
    sideCheckLpKnob.onPopupMenu = [this] { showSideCheckHpLpSlopeMenu (false); };

    auto setupSideCheckRangeLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::NotificationType::dontSendNotification);
        // Same caption style as Match AMT / HP / LP (11 pt, centred).
        label.setFont (juce::Font (11.0f));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.85f));
        label.setInterceptsMouseClicks (false, false);
        addChildComponent (label);
    };
    setupSideCheckRangeLabel (sideCheckAmountLabel, "AMT");
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
    helpTooltipsButton.setLookAndFeel (&graphOverlayButtonLookAndFeel);
    helpTooltipsButton.setPaintingIsUnclipped (true);
    addAndMakeVisible (helpTooltipsButton);

    // Bottom chrome: Phase / Processing Mode (preset-style popup, not ComboBox).
    phaseModeCombo.setTooltip ("Phase - Minimum Phase is zero-latency IIR EQ. Linear Phase matches the EQ magnitude with constant group delay (higher latency).");
    addAndMakeVisible (phaseModeCombo);

    loadUiPrefs();
    // Host setState often runs before the editor; if it already stored UiSession, loadUiPrefs
    // restored it. If setState runs later, EqProcessor will call loadUiPrefs again.
    applyTooltipsEnabled();

    // Faceplate bank pager: chevron left of band 1, right of band 8 (no readout).
    faceplateBankPrevButton.setTooltip ("Previous band bank");
    faceplateBankNextButton.setTooltip ("Next band bank");
    faceplateBankPrevButton.onClick = [this] { setFaceplateBank (faceplateBank - 1); };
    faceplateBankNextButton.onClick = [this]
    {
        const int maxBank = audioProcessor.getFaceplateBankCount() - 1;
        if (faceplateBank >= maxBank && faceplateBank + 1 < EqBand::kMaxBanks)
            audioProcessor.ensureBankAvailable (faceplateBank + 1);
        setFaceplateBank (faceplateBank + 1);
    };
    addAndMakeVisible (faceplateBankPrevButton);
    addAndMakeVisible (faceplateBankNextButton);

    bandsFullToastLabel.setJustificationType (juce::Justification::centred);
    bandsFullToastLabel.setColour (juce::Label::textColourId, juce::Colour (218, 165, 32));
    bandsFullToastLabel.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.65f));
    bandsFullToastLabel.setText ("Max 64 bands", juce::dontSendNotification);
    bandsFullToastLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (bandsFullToastLabel);

    if (mainComponent != nullptr)
    {
        auto& frc = mainComponent->getFrequencyResponseComponent();
        frc.onFaceplateBankJump = [this] (int bank) { setFaceplateBank (bank, true); };
        frc.onBandsFullSoftMax = [this] { showBandsFullSoftMaxFeedback(); };
        frc.setPreferredCreateBank (faceplateBank);
    }

    rebindFaceplateAttachments();
    updateFaceplateBankChrome();

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
    saveUiPrefs();

    helpTooltipsButton.setLookAndFeel (nullptr);
    sideCheckButton.setLookAndFeel (nullptr);
    scopeModeButton.setLookAndFeel (nullptr);
    sideCheckSpeedButton.setLookAndFeel (nullptr);
    sideCheckHqButton.setLookAndFeel (nullptr);

    audioProcessor.treeState.removeParameterListener (SideCheck::enabledParamId(), this);
    audioProcessor.treeState.removeParameterListener (SideCheck::modeParamId(), this);
    for (const char* typeId : { "highpassType", "lowpassType", "band1Type", "band2Type",
                                "band3Type", "band4Type", "highShelfType", "lowShelfType" })
        audioProcessor.treeState.removeParameterListener (typeId, this);
    sideCheckHpKnob.removeListener (this);
    sideCheckLpKnob.removeListener (this);
    sideCheckHpKnob.onPopupMenu = nullptr;
    sideCheckLpKnob.onPopupMenu = nullptr;
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
    const auto& pal = themePalette();

    if (uiCompact)
    {
        g.fillAll (pal.pluginBackground);

        const int titleBarHeight = getTopBandHeight();
        juce::ColourGradient gradient2 (pal.pluginButtonBackground,
            juce::Point<float> ((float) getWidth() * 0.5f, 0.0f),
            pal.pluginBackground,
            juce::Point<float> ((float) getWidth(), (float) getHeight()),
            true);
        g.setGradientFill (gradient2);
        g.fillRect (0, 0, getWidth(), titleBarHeight);
        g.setColour (pal.pluginBackground);
        g.drawLine (0.0f, (float) titleBarHeight, (float) getWidth(), (float) titleBarHeight, 4.0f);
        return;
    }

    // Match resized() stack: top band → graph → optional mod → faceplate → trim.
    const int titleBarHeight = getTopBandHeight();
    const int trimH = getBottomTrimHeight();
    const int faceplateStripH = isFaceplateSuppressed() ? 0 : getFaceplateHeightForWidth (getWidth());
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

    juce::ColourGradient bodyGrad (pal.pluginBackground2,
        juce::Point<float> ((float) getWidth() * 0.5f, 0.0f),
        pal.pluginBackground,
        juce::Point<float> ((float) getWidth(), (float) getHeight()),
        true);
    g.setGradientFill (bodyGrad);
    g.fillRect (0, 0, getWidth(), getHeight());

    juce::ColourGradient chromeGrad (pal.pluginButtonBackground,
        juce::Point<float> ((float) getWidth() * 0.5f, 0.0f),
        pal.pluginBackground,
        juce::Point<float> ((float) getWidth(), (float) getHeight()),
        true);
    g.setGradientFill (chromeGrad);
    g.fillRect (0, 0, getWidth(), titleBarHeight);
    g.fillRect (0, titleBarY, getWidth(), titleBarHeight);
    g.fillRect (0, getHeight() - trimH, getWidth(), trimH);

    g.setColour (pal.pluginBackground);
    g.drawLine (0.0f, (float) titleBarHeight, (float) getWidth(), (float) titleBarHeight, 4.0f);
    g.drawLine (0.0f, (float) (titleBarY + titleBarHeight), (float) getWidth(), (float) (titleBarY + titleBarHeight), 2.0f);
}



void EqEditor::resized()
{
    // Keep horizontal spacing proportional to the 1200 design width; height is free.
    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    const bool scopeOn = isScopeModeActive();
    const bool scopeStrip = scopeOn && mainComponent != nullptr
                            && mainComponent->isScopeStripLayout();

    // Track Scope window size continuously so leave/re-enter restores host or edge resizes.
    if (holdingSizeBeforeScope && scopeOn)
    {
        appliedScopeStrip = scopeStrip;
        captureCurrentScopeWindowSize();
    }

    // Compact UI, or Scope strip: MainComponent owns the full editor (no faceplate stack / black void).
    if (uiCompact || scopeStrip)
    {
        int mainH = getHeight();
        int modH = 0;
        if (! scopeStrip && modPanelOpen && modSection != nullptr)
        {
            modH = getModPanelHeightForGraphHeight (0);
            mainH = juce::jmax (120, getHeight() - modH);
        }

        if (mainComponent != nullptr)
            mainComponent->setBounds (0, 0, getWidth(), mainH);

        if (modSection != nullptr)
        {
            // Mod panel stays off the strip Scope window.
            const bool showMod = modPanelOpen && ! scopeStrip;
            modSection->setVisible (showMod);
            if (showMod)
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
        layoutScopeModeButton();
        return;
    }

    // Padding between groups and rows
    const int padding = px (10.0f);

    // Stack: graph (fills leftover) → mod (optional, 3/4 faceplate) → faceplate → bottom trim.
    // Scope mode suppresses the faceplate strip (knobs parked); bottom chrome stays.
    const int graphTop = getTopBandHeight();
    const int trimH = getBottomTrimHeight();
    const int faceplateTopTrimH = getTopBandHeight(); // painted trim strip above knobs
    const bool suppressFaceplate = isFaceplateSuppressed();
    const int faceplateStripH = suppressFaceplate ? 0 : getFaceplateHeightForWidth (getWidth());
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
            faceplateH = suppressFaceplate ? 0 : juce::jmax (1, (remaining * 4) / 7);
            modHForLayout = juce::jmax (1, remaining - faceplateH);
        }
        else
        {
            faceplateH = suppressFaceplate ? 0 : remaining;
            modHForLayout = 0;
        }
    }
    const int faceplateMinTop = graphTop + graphH + modHForLayout;
    const int bottomLimit = getHeight() - trimH - 2;
    if (suppressFaceplate)
        faceplateH = 0;
    else
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

    auto typeIsHpLpForGlobal = [this] (int globalDisplay, int fallbackType) -> bool
    {
        const auto typeId = FilterType::paramIDForGlobal (globalDisplay);
        const int t = BandChannel::readChoiceIndex (audioProcessor.treeState, typeId, fallbackType);
        return FilterType::isHpLp (t);
    };

    const int g0 = EqBand::globalFromBankSlot (faceplateBank, 0);
    const int g1 = EqBand::globalFromBankSlot (faceplateBank, 1);
    const int g2 = EqBand::globalFromBankSlot (faceplateBank, 2);
    const int g3 = EqBand::globalFromBankSlot (faceplateBank, 3);
    const int g4 = EqBand::globalFromBankSlot (faceplateBank, 4);
    const int g5 = EqBand::globalFromBankSlot (faceplateBank, 5);
    const int g6 = EqBand::globalFromBankSlot (faceplateBank, 6);
    const int g7 = EqBand::globalFromBankSlot (faceplateBank, 7);

    const bool band1HpLp = typeIsHpLpForGlobal (g0, FilterType::highpass);
    const bool band2HpLp = typeIsHpLpForGlobal (g1, FilterType::lowShelf);
    const bool band3HpLp = typeIsHpLpForGlobal (g2, FilterType::bell);
    const bool band4HpLp = typeIsHpLpForGlobal (g3, FilterType::bell);
    const bool band5HpLp = typeIsHpLpForGlobal (g4, FilterType::bell);
    const bool band6HpLp = typeIsHpLpForGlobal (g5, FilterType::bell);
    const bool band7HpLp = typeIsHpLpForGlobal (g6, FilterType::highShelf);
    const bool band8HpLp = typeIsHpLpForGlobal (g7, FilterType::lowpass);

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
    layoutScopeModeButton();

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

    // Bank pager: centre each arrow in the gap between window edge and end power buttons.
    {
        const int pagerW = juce::jmax (18, px (20.0f));
        const int pagerH = juce::jmax (18, juce::jmin (onOffButtonSize, px (22.0f)));
        const int pagerY = onOffY + (onOffButtonSize - pagerH) / 2;
        const int edgePad = 2;

        if (onOffButton1 != nullptr)
        {
            const int gapStart = edgePad;
            const int gapEnd = onOffButton1->getX();
            const int gap = juce::jmax (pagerW, gapEnd - gapStart);
            const int prevX = gapStart + (gap - pagerW) / 2;
            faceplateBankPrevButton.setBounds (prevX, pagerY, pagerW, pagerH);
        }
        if (onOffButton2 != nullptr)
        {
            const int gapStart = onOffButton2->getRight();
            const int gapEnd = getWidth() - edgePad;
            const int gap = juce::jmax (pagerW, gapEnd - gapStart);
            const int nextX = gapStart + (gap - pagerW) / 2;
            faceplateBankNextButton.setBounds (nextX, pagerY, pagerW, pagerH);
        }

        faceplateBankPrevButton.setVisible (true);
        faceplateBankNextButton.setVisible (true);
        updateFaceplateBankChrome();
    }

    // Scope under band 6 once power buttons have final bounds (avoids logo overlap).
    layoutScopeModeButton();

    if (suppressFaceplate)
    {
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
    }

    if (bandsFullToastLabel.isVisible())
    {
        const int tw = juce::jmax (120, px (140.0f));
        const int th = juce::jmax (22, px (24.0f));
        bandsFullToastLabel.setBounds ((getWidth() - tw) / 2, graphTop + px (8.0f), tw, th);
        bandsFullToastLabel.toFront (false);
    }
}

void EqEditor::updateBandFaceplateGainVisibility()
{
    // Compact / Scope: faceplate is suppressed — don't fight visibility here.
    if (isFaceplateSuppressed())
    {
        updateFaceplateSlopeWheelMode();
        return;
    }

    // Same rule for all 8 columns: show gain only when the filter type uses gain.
    auto typeUsesGainGlobal = [this] (int col, int fallback) -> bool
    {
        const int g = EqBand::globalFromBankSlot (faceplateBank, col);
        return FilterType::usesGain (
            BandChannel::readChoiceIndex (audioProcessor.treeState,
                                          FilterType::paramIDForGlobal (g), fallback));
    };

    knobHpGain.setVisible (typeUsesGainGlobal (0, FilterType::highpass));
    knob9.setVisible (typeUsesGainGlobal (1, FilterType::lowShelf));
    knob12.setVisible (typeUsesGainGlobal (2, FilterType::bell));
    knob15.setVisible (typeUsesGainGlobal (3, FilterType::bell));
    knob18.setVisible (typeUsesGainGlobal (4, FilterType::bell));
    knob21.setVisible (typeUsesGainGlobal (5, FilterType::bell));
    knob6.setVisible (typeUsesGainGlobal (6, FilterType::highShelf));
    knobLpGain.setVisible (typeUsesGainGlobal (7, FilterType::lowpass));

    updateFaceplateSlopeWheelMode();
}


void EqEditor::layoutBrandWordmark (int outClusterLeftX)
{
    const bool scopeOn = mainComponent != nullptr && mainComponent->isScopeMode();
    brandWordmark.setVisible (! scopeOn);
    if (scopeOn)
    {
        brandWordmark.setBounds ({});
        return;
    }

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
    // Strip Scope: hide ? — Scope / Settings / UI / Dice overlays remain.
    if (mainComponent != nullptr && mainComponent->isScopeMode() && mainComponent->isScopeStripLayout())
    {
        helpTooltipsButton.setVisible (false);
        helpTooltipsButton.setBounds ({});
        return;
    }

    helpTooltipsButton.setVisible (true);
    addAndMakeVisible (helpTooltipsButton);

    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    constexpr int trimH = 30;
    const int btnSize = juce::jlimit (16, uiCompact ? 22 : (trimH - 6), px (20.0f));
    const int margin = px (8.0f);

    const int pianoH = (mainComponent != nullptr)
        ? mainComponent->getFrequencyResponseComponent().getPianoStripHeight() : 0;

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
        const int y = graphBottom - btnSize - bottomMargin - pianoH;
        helpTooltipsButton.setBounds (x, y, btnSize, btnSize);
    }
    else
    {
        // Prefer bottom of spectrum graph when Mod is open; otherwise faceplate trim.
        if (modPanelOpen && mainComponent != nullptr)
        {
            constexpr int bottomMargin = 18;
            const int x = margin;
            const int y = mainComponent->getBottom() - btnSize - bottomMargin - pianoH;
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
    // Scope Pre / strip: no DSP phase control.
    if (mainComponent != nullptr && mainComponent->shouldHideScopeDspChrome())
    {
        phaseModeCombo.setVisible (false);
        phaseModeCombo.setBounds ({});
        return;
    }

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


int EqEditor::getPianoWindowExtra() const noexcept
{
    // Keep in sync with FrequencyResponseComponent::kPianoStripHeightPx.
    constexpr int kStrip = 50;
    if (mainComponent != nullptr
        && mainComponent->getFrequencyResponseComponent().isPianoDisplayOn())
        return kStrip;
    return 0;
}

void EqEditor::applyPianoStripWindowHeight (bool pianoOn)
{
    constexpr int kStrip = 50; // keep in sync with FrequencyResponseComponent::kPianoStripHeightPx
    if (pianoOn == pianoStripWindowApplied)
    {
        resized();
        return;
    }

    const int minH = (getConstrainer() != nullptr) ? getConstrainer()->getMinimumHeight() : 500;
    const int newH = pianoOn ? (getHeight() + kStrip)
                             : juce::jmax (minH, getHeight() - kStrip);
    pianoStripWindowApplied = pianoOn;
    setSize (getWidth(), newH);
}

void EqEditor::layoutScopeModeButton()
{
    scopeModeButton.setVisible (true);
    addAndMakeVisible (scopeModeButton);

    const float scale = (float) getWidth() / (float) designWidth;
    auto px = [scale] (float value) { return juce::roundToInt (value * scale); };

    constexpr int trimH = 30;
    const int btnSize = juce::jlimit (16, uiCompact ? 22 : (trimH - 6), px (20.0f));
    const int btnW = juce::jmax (btnSize, px (uiCompact ? 52.0f : 58.0f));
    const int matchBtnW = juce::jmax (btnSize, px (uiCompact ? 52.0f : 58.0f));
    const int gap = px (6.0f);
    const int pianoH = (mainComponent != nullptr)
        ? mainComponent->getFrequencyResponseComponent().getPianoStripHeight() : 0;

    const bool stripOverlay = mainComponent != nullptr && mainComponent->isScopeMode()
                              && mainComponent->isScopeStripLayout();

    int x = 0;
    int y = 0;
    if (stripOverlay)
    {
        // Square control, bottom-left above piano strip — keep Scope on this Y (not Match row centre).
        x = px (8.0f);
        y = juce::jmax (0, getHeight() - btnSize - px (8.0f) - pianoH);
        const int scopeY = y;
        if (mainComponent != nullptr)
        {
            auto& frc = mainComponent->getFrequencyResponseComponent();
            const auto matchBounds = frc.layoutMatchChromeAt (
                frc.getLocalPoint (this, juce::Point<int> (x, y)), btnSize, matchBtnW);
            const auto matchEditor = getLocalArea (&frc, matchBounds);
            x = matchEditor.getRight() + gap;
        }
        scopeModeButton.setBounds (x, scopeY, btnSize, btnSize);
        scopeModeButton.setToggleState (mainComponent != nullptr && mainComponent->isScopeMode(),
                                        juce::dontSendNotification);
        scopeModeButton.setIdleAlpha (0.5f);
        scopeModeButton.toFront (false);
        return;
    }

    // Horizontal cluster follows Help / Phase / SideCheck; Match/Scope Y always uses
    // the graph chrome row (Mod/P), never the faceplate trim when Help is parked there.
    y = helpTooltipsButton.getY() + (helpTooltipsButton.getHeight() - btnSize) / 2;
    if (sideCheckButton.isVisible() && sideCheckButton.getHeight() > 0)
        y = sideCheckButton.getY() + (sideCheckButton.getHeight() - btnSize) / 2;
    else if (phaseModeCombo.isVisible() && phaseModeCombo.getHeight() > 0)
        y = phaseModeCombo.getY() + (phaseModeCombo.getHeight() - btnSize) / 2;

    x = helpTooltipsButton.getRight() + gap;
    if (sideCheckButton.isVisible() && sideCheckButton.getWidth() > 0)
        x = sideCheckButton.getRight() + gap;
    else if (phaseModeCombo.isVisible() && phaseModeCombo.getWidth() > 0)
        x = phaseModeCombo.getRight() + gap;

    if (sideCheckAmountKnob.isVisible())
        x = juce::jmax (x, sideCheckLpKnob.getRight() + gap);

    const int chromeY = y;

    // Keep Scope beside Match (same place as when not in Scope mode). Do not park it
    // on the far right when OSC/Gon/Spec toggles are hidden.
    int scopeX = x;
    int scopeY = chromeY;
    if (mainComponent != nullptr)
    {
        auto& frc = mainComponent->getFrequencyResponseComponent();
        const auto matchBounds = frc.layoutMatchChromeAt (
            frc.getLocalPoint (this, juce::Point<int> (x, chromeY)), btnSize, matchBtnW);
        const auto matchEditor = getLocalArea (&frc, matchBounds);
        scopeX = matchEditor.getRight() + gap;
        scopeY = matchEditor.getY() + (matchEditor.getHeight() - btnSize) / 2;

        if (! mainComponent->isScopeMode())
        {
            const auto col = mainComponent->getAnalyserToggleColumnBounds();
            if (! col.isEmpty())
            {
                const auto colInEditor = getLocalArea (mainComponent.get(), col);
                scopeX = colInEditor.getX() + (colInEditor.getWidth() - btnW) / 2;
            }
        }
    }

    scopeModeButton.setBounds (scopeX, scopeY, btnW, btnSize);
    scopeModeButton.setToggleState (mainComponent != nullptr && mainComponent->isScopeMode(),
                                    juce::dontSendNotification);
    scopeModeButton.setIdleAlpha (1.0f);
    scopeModeButton.toFront (false);
}

void EqEditor::layoutSideCheckButton()
{
    // Scope Pre / strip: hide Side Check and its DSP controls.
    if (mainComponent != nullptr && mainComponent->shouldHideScopeDspChrome())
    {
        sideCheckButton.setVisible (false);
        sideCheckAmountKnob.setVisible (false);
        sideCheckAmountLabel.setVisible (false);
        sideCheckSpeedButton.setVisible (false);
        sideCheckHqButton.setVisible (false);
        sideCheckHpKnob.setVisible (false);
        sideCheckLpKnob.setVisible (false);
        sideCheckHpLabel.setVisible (false);
        sideCheckLpLabel.setVisible (false);
        sideCheckButton.setBounds ({});
        sideCheckAmountKnob.setBounds ({});
        sideCheckAmountLabel.setBounds ({});
        sideCheckSpeedButton.setBounds ({});
        sideCheckHqButton.setBounds ({});
        sideCheckHpKnob.setBounds ({});
        sideCheckLpKnob.setBounds ({});
        sideCheckHpLabel.setBounds ({});
        sideCheckLpLabel.setBounds ({});
        layoutScopeModeButton();
        return;
    }

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
    sideCheckAmountKnob.setVisible (scOn);
    sideCheckAmountLabel.setVisible (scOn);
    sideCheckSpeedButton.setVisible (scOn);
    sideCheckHqButton.setVisible (scOn);
    sideCheckHpKnob.setVisible (scOn);
    sideCheckLpKnob.setVisible (scOn);
    sideCheckHpLabel.setVisible (scOn);
    sideCheckLpLabel.setVisible (scOn);
    if (scOn)
    {
        // Match-style cluster: AMT | knob | Speed | HQ | HP | knob | LP | knob
        const int knobSize = btnSize;
        const int labelW = juce::jmax (px (22.0f), 18);
        const int labelGap = px (1.0f);
        const int pairGap = px (2.0f);

        int cx = sideCheckButton.getRight() + gap;
        sideCheckAmountLabel.setBounds (cx, y, labelW, btnSize);
        cx += labelW + labelGap;
        sideCheckAmountKnob.setBounds (cx, y, knobSize, knobSize);
        cx += knobSize + gap;

        // Compact Fast/Med/Slow toggle — short enough for both compact and expanded chrome.
        const int speedW = juce::jmax (px (48.0f), uiCompact ? 52 : 58);
        sideCheckSpeedButton.setBounds (cx, y, speedW, btnSize);
        cx = sideCheckSpeedButton.getRight() + gap;

        const int hqW = juce::jmax (btnSize, px (28.0f));
        sideCheckHqButton.setBounds (cx, y, hqW, btnSize);
        cx = sideCheckHqButton.getRight() + px (4.0f);

        const int hpLpLabelW = juce::jmax (px (14.0f), 12);
        sideCheckHpLabel.setBounds (cx, y, hpLpLabelW, btnSize);
        cx += hpLpLabelW + labelGap;
        sideCheckHpKnob.setBounds (cx, y, knobSize, knobSize);
        cx += knobSize + pairGap;
        sideCheckLpLabel.setBounds (cx, y, hpLpLabelW, btnSize);
        cx += hpLpLabelW + labelGap;
        sideCheckLpKnob.setBounds (cx, y, knobSize, knobSize);

        sideCheckAmountLabel.toFront (false);
        sideCheckAmountKnob.toFront (false);
        sideCheckSpeedButton.toFront (false);
        sideCheckHqButton.toFront (false);
        sideCheckHpLabel.toFront (false);
        sideCheckHpKnob.toFront (false);
        sideCheckLpLabel.toFront (false);
        sideCheckLpKnob.toFront (false);
    }
    else
    {
        // Park off-layout so a stale visible flag can never flash knobs at (0,0).
        sideCheckAmountKnob.setBounds ({});
        sideCheckAmountLabel.setBounds ({});
        sideCheckSpeedButton.setBounds ({});
        sideCheckHqButton.setBounds ({});
        sideCheckHpKnob.setBounds ({});
        sideCheckLpKnob.setBounds ({});
        sideCheckHpLabel.setBounds ({});
        sideCheckLpLabel.setBounds ({});
    }

    layoutScopeModeButton();
}

void EqEditor::updateSideCheckAmountVisibility()
{
    if (mainComponent != nullptr && mainComponent->shouldHideScopeDspChrome())
    {
        sideCheckAmountKnob.setVisible (false);
        sideCheckAmountLabel.setVisible (false);
        sideCheckSpeedButton.setVisible (false);
        sideCheckHqButton.setVisible (false);
        sideCheckHpKnob.setVisible (false);
        sideCheckLpKnob.setVisible (false);
        sideCheckHpLabel.setVisible (false);
        sideCheckLpLabel.setVisible (false);
        return;
    }

    const bool scOn = sideCheckButton.getToggleState()
        || (audioProcessor.treeState.getRawParameterValue (SideCheck::enabledParamId()) != nullptr
            && audioProcessor.treeState.getRawParameterValue (SideCheck::enabledParamId())->load() > 0.5f);
    sideCheckAmountKnob.setVisible (scOn);
    sideCheckAmountLabel.setVisible (scOn);
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

void EqEditor::showSideCheckHpLpSlopeMenu (bool forHp)
{
    const char* paramId = forHp ? SideCheck::hpSlopeParamId() : SideCheck::lpSlopeParamId();
    auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
        audioProcessor.treeState.getParameter (paramId));
    if (choice == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const auto names = FilterSlope::getChoiceNames();
    for (int i = 0; i < names.size(); ++i)
        menu.addItem (1 + i, names[i], true, choice->getIndex() == i);

    auto* target = forHp ? (juce::Component*) &sideCheckHpKnob : (juce::Component*) &sideCheckLpKnob;
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
        [this, paramId] (int result)
        {
            if (result <= 0)
                return;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    audioProcessor.treeState.getParameter (paramId)))
                *p = result - 1;
        });
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

juce::File EqEditor::getLastUiThemeFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("ParametricEq")
        .getChildFile ("last_ui_theme.xml");
}

void EqEditor::saveLastUiThemeToDisk() const
{
    if (mainComponent == nullptr)
        return;

    // Same reliability path as dice flags in ui_prefs.xml (host chunk has been unreliable).
    Theme theme (mainComponent->getSharedResources().sharedColors);
    theme.setModified (juce::Time::getCurrentTime());
    if (auto xml = std::unique_ptr<juce::XmlElement> (theme.toXml()))
    {
        // Colours only — strip any accidental DSP / GlobalUi payload.
        while (auto* child = xml->getFirstChildElement())
            xml->removeChildElement (child, true);

        auto file = getLastUiThemeFile();
        file.getParentDirectory().createDirectory();
        if (file.getParentDirectory().isDirectory())
            xml->writeTo (file);
    }
}

bool EqEditor::loadLastUiThemeFromDisk (SharedResources* into)
{
    SharedResources* resources = into;
    if (resources == nullptr && mainComponent != nullptr)
        resources = &mainComponent->getSharedResources();
    if (resources == nullptr)
        return false;

    const auto file = getLastUiThemeFile();
    if (! file.existsAsFile())
        return false;

    auto xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("Theme"))
        return false;

    Theme theme;
    theme.fromXml (*xml);

    auto& live = resources->sharedColors;
    // Preserve dice / accessibility scopes (loaded from ui_prefs separately).
    const bool keepFace = live.randomizeFaceplateMod;
    const bool keepGraph = live.randomizeGraphModule;
    const bool keepMenu = live.randomizeMenuModule;
    const bool keepRampFft = live.randomizeRampFftBars;
    const bool keepRampSpec = live.randomizeRampSpectrogram;
    const bool keepRampSpec3D = live.randomizeRampSpectrogram3D;
    const bool keepRampFill = live.randomizeRampSpectrumFill;
    const bool keepOrdered = live.orderedRampGradation;
    const bool keepLegible = live.enforceLegibleText;
    const float keepContrast = live.textContrastAmount;
    const bool keepH = live.randomizeHue, keepS = live.randomizeSaturation;
    const bool keepB = live.randomizeBrightness, keepA = live.randomizeAlpha;
    const float hueL = live.hueLowerLimit, hueU = live.hueUpperLimit;
    const float satL = live.saturationLowerLimit, satU = live.saturationUpperLimit;
    const float briL = live.brightnessLowerLimit, briU = live.brightnessUpperLimit;

    live = theme.getColors();

    live.randomizeFaceplateMod = keepFace;
    live.randomizeGraphModule = keepGraph;
    live.randomizeMenuModule = keepMenu;
    live.randomizeRampFftBars = keepRampFft;
    live.randomizeRampSpectrogram = keepRampSpec;
    live.randomizeRampSpectrogram3D = keepRampSpec3D;
    live.randomizeRampSpectrumFill = keepRampFill;
    live.orderedRampGradation = keepOrdered;
    live.enforceLegibleText = keepLegible;
    live.textContrastAmount = keepContrast;
    live.randomizeHue = keepH;
    live.randomizeSaturation = keepS;
    live.randomizeBrightness = keepB;
    live.randomizeAlpha = keepA;
    live.hueLowerLimit = hueL;
    live.hueUpperLimit = hueU;
    live.saturationLowerLimit = satL;
    live.saturationUpperLimit = satU;
    live.brightnessLowerLimit = briL;
    live.brightnessUpperLimit = briU;

    // Seed processor so host getState / reapply paths also see these colours.
    audioProcessor.storeSessionUiTheme (live, {});
    return true;
}

void EqEditor::syncScopeModeButton()
{
    const bool scopeOn = mainComponent != nullptr && mainComponent->isScopeMode();
    const bool tapPost = mainComponent != nullptr && mainComponent->isScopeTapPost();
    scopeModeButton.setToggleState (scopeOn, juce::dontSendNotification);

    juce::String tip = "Scope - quad metering view (Gon / Spectrum / Oscilloscope / Spectrogram).";
    tip << " Right-click for Pre/Post tap.";
    tip << (tapPost ? " Scope · Post (EQ DSP on)." : " Scope · Pre (analyzer, DSP off).");
    scopeModeButton.setTooltip (tip);
}

bool EqEditor::isScopeModeActive() const noexcept
{
    return mainComponent != nullptr && mainComponent->isScopeMode();
}

bool EqEditor::isFaceplateSuppressed() const noexcept
{
    return uiCompact || isScopeModeActive();
}

int EqEditor::getScopeStripWindowHeight (int width) const
{
    const float scale = (float) juce::jmax (1, width) / (float) designWidth;
    // Edge-to-edge strip window — overlays sit on the panes.
    const int stripDesign = mainComponent != nullptr ? mainComponent->getScopeStripHeightPx() : 200;
    return juce::jmax (1, juce::roundToInt ((float) stripDesign * scale));
}

void EqEditor::captureCurrentScopeWindowSize()
{
    const int w = getWidth();
    const int h = getHeight();
    if (w <= 0 || h <= 0)
        return;

    lastScopeWidth = w;
    lastScopeHeight = h;
    lastScopeWasStrip = appliedScopeStrip;
    if (appliedScopeStrip)
    {
        lastStripScopeWidth = w;
        lastStripScopeHeight = h;
        if (mainComponent != nullptr)
            mainComponent->syncStripHeightFromWindow (w, h);
    }
    else
    {
        lastTiledScopeWidth = w;
        lastTiledScopeHeight = h;
    }
}

void EqEditor::applyStripScopeWindowSize()
{
    resizeConstrainer.setFixedAspectRatio (0.0);
    resizeConstrainer.setSizeLimits (600, 100, 2400, 2000);
    setConstrainer (&resizeConstrainer);

    if (lastStripScopeWidth >= 600 && lastStripScopeHeight >= 100)
    {
        if (mainComponent != nullptr)
            mainComponent->syncStripHeightFromWindow (lastStripScopeWidth, lastStripScopeHeight);
        setSize (juce::jlimit (600, 2400, lastStripScopeWidth),
                 juce::jlimit (100, 2000, lastStripScopeHeight));
        return;
    }

    const int stripW = juce::jlimit (600, 2400, getWidth() > 0 ? getWidth() : designWidth);
    setSize (stripW, getScopeStripWindowHeight (stripW));
}

void EqEditor::applyTiledScopeWindowSize()
{
    resizeConstrainer.setFixedAspectRatio (0.0);
    resizeConstrainer.setSizeLimits (900, 400, 2400, 1600);
    setConstrainer (&resizeConstrainer);

    if (lastTiledScopeWidth >= 900 && lastTiledScopeHeight >= 400)
    {
        setSize (juce::jlimit (900, 2400, lastTiledScopeWidth),
                 juce::jlimit (400, 1600, lastTiledScopeHeight));
        return;
    }

    const int w = getWidth() > 0 ? getWidth() : designWidth;
    if (uiCompact)
    {
        const int graphH = getGraphHeightForWidth (w);
        const int modH = modPanelOpen ? getModPanelHeightForGraphHeight (graphH) : 0;
        setSize (w, graphH + modH + getPianoWindowExtra());
    }
    else
    {
        setSize (w, getExpandedEditorHeight (w, modPanelOpen));
    }
    pianoStripWindowApplied = (getPianoWindowExtra() > 0);
}

void EqEditor::syncScopeModeLayout()
{
    if (getWidth() <= 0 || getHeight() <= 0)
        return;

    // Scope mode hides the faceplate (same as compact) and collapses its strip height.
    const bool showFaceplate = ! isFaceplateSuppressed();
    setFaceplateVisible (showFaceplate);
    if (showFaceplate)
        updateBandFaceplateGainVisibility();

    const int w = getWidth() > 0 ? getWidth() : designWidth;
    const bool scopeOn = isScopeModeActive();
    const bool stripOn = scopeOn && mainComponent != nullptr && mainComponent->isScopeStripLayout();

    if (scopeOn)
    {
        const bool enteringScope = ! holdingSizeBeforeScope;
        const bool switchingArrange = holdingSizeBeforeScope && (appliedScopeStrip != stripOn);

        if (enteringScope)
        {
            // Piano-free height so leave-scope restore + getExpandedEditorHeight don't double-count.
            savedEditorWidth = getWidth();
            savedEditorHeight = juce::jmax (1, getHeight() - getPianoWindowExtra());
            holdingSizeBeforeScope = true;
        }
        else if (switchingArrange)
        {
            // Persist the arrange mode we're leaving before applying the other.
            captureCurrentScopeWindowSize();
        }

        if (stripOn)
        {
            if (enteringScope || switchingArrange)
            {
                applyStripScopeWindowSize();
            }
            else
            {
                // Already in strip (e.g. edge-drag height change): keep width, apply design height.
                resizeConstrainer.setFixedAspectRatio (0.0);
                resizeConstrainer.setSizeLimits (600, 100, 2400, 2000);
                setConstrainer (&resizeConstrainer);
                const int stripW = juce::jlimit (600, 2400, getWidth());
                setSize (stripW, getScopeStripWindowHeight (stripW));
            }
        }
        else if (enteringScope || switchingArrange)
        {
            applyTiledScopeWindowSize();
        }

        appliedScopeStrip = stripOn;
    }
    else if (holdingSizeBeforeScope)
    {
        captureCurrentScopeWindowSize();
        holdingSizeBeforeScope = false;
        appliedScopeStrip = false;
        const int rw = savedEditorWidth > 0 ? savedEditorWidth : designWidth;
        const int pianoExtra = getPianoWindowExtra();
        const int rh = savedEditorHeight > 0
            ? (savedEditorHeight + pianoExtra)
            : (uiCompact ? (getGraphHeightForWidth (rw) + pianoExtra)
                         : getExpandedEditorHeight (rw, modPanelOpen));
        if (uiCompact)
        {
            setConstrainer (nullptr);
            setSize (rw, rh);
        }
        else
        {
            resizeConstrainer.setFixedAspectRatio (0.0);
            resizeConstrainer.setSizeLimits (900, 500, 2400, 1600);
            setConstrainer (&resizeConstrainer);
            setSize (rw, juce::jmax (rh, getExpandedEditorHeight (rw, modPanelOpen)));
        }
        pianoStripWindowApplied = (pianoExtra > 0);
    }
    else if (! uiCompact)
    {
        setSize (w, getExpandedEditorHeight (w, modPanelOpen));
        pianoStripWindowApplied = (getPianoWindowExtra() > 0);
    }

    layoutBrandWordmark (-1);
    layoutHelpTooltipsButton();
    layoutPhaseModeCombo();
    layoutSideCheckButton();
    layoutScopeModeButton();
}

void EqEditor::showScopeTapMenu()
{
    if (mainComponent == nullptr)
        return;

    const bool tapPost = mainComponent->isScopeTapPost();
    const bool stripOn = mainComponent->isScopeStripLayout();
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addSectionHeader ("Scope tap");
    menu.addItem (1, "Pre (analyzer / DSP off)", true, ! tapPost);
    menu.addItem (2, "Post (EQ DSP on)", true, tapPost);
    menu.addSeparator();
    menu.addSectionHeader ("Arrange");
    menu.addItem (3, "Tiled (quad / grid)", true, ! stripOn);
    menu.addItem (4, "Strip (side-by-side)", true, stripOn);
    menu.addSeparator();
    menu.addSectionHeader (stripOn ? "Strip layout presets" : "Tiled layout presets");
    menu.addItem (5, "Save scope layout...");
    auto modePresets = ScopeLayoutPresets::loadForMode (stripOn);
    std::vector<juce::String> presetNames;
    int presetItemId = 100;
    if (modePresets.empty())
    {
        menu.addItem (-1, "(no saved presets)", false, false);
    }
    else
    {
        for (const auto& p : modePresets)
        {
            presetNames.push_back (p.name);
            menu.addItem (presetItemId++, p.name);
        }
    }
    menu.addSeparator();
    menu.addSectionHeader ("Modules");

    const auto defaults = ScopeModules::defaultEnabledOrder();
    int itemId = 10;
    std::vector<ScopeModuleId> menuIds;
    for (int mi = 0; mi < ScopeModules::kNumModules; ++mi)
    {
        const auto id = static_cast<ScopeModuleId> (mi);
        menuIds.push_back (id);
        const bool checked = mainComponent->isScopeModuleEnabled (id);
        const bool isDefault = std::find (defaults.begin(), defaults.end(), id) != defaults.end();
        juce::String label = ScopeModules::idToLabel (id);
        if (! isDefault)
            label += " (optional)";
        menu.addItem (itemId++, label, true, checked);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&scopeModeButton),
                        [safe = juce::Component::SafePointer<EqEditor> (this), menuIds, presetNames, stripOn] (int result)
                        {
                            if (safe == nullptr || safe->mainComponent == nullptr || result <= 0)
                                return;

                            if (result == 1)
                                safe->mainComponent->setScopeTapPost (false, true);
                            else if (result == 2)
                                safe->mainComponent->setScopeTapPost (true, true);
                            else if (result == 3)
                                safe->mainComponent->setScopeStripLayout (false, true);
                            else if (result == 4)
                                safe->mainComponent->setScopeStripLayout (true, true);
                            else if (result == 5)
                            {
                                auto* aw = new juce::AlertWindow ("Save scope layout",
                                                                  stripOn ? "Name this Strip layout preset:"
                                                                          : "Name this Tiled layout preset:",
                                                                  juce::AlertWindow::NoIcon);
                                aw->addTextEditor ("name", "My Layout", "Name");
                                aw->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
                                aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                                juce::Component::SafePointer<juce::AlertWindow> awSafe (aw);
                                aw->enterModalState (true,
                                    juce::ModalCallbackFunction::create (
                                        [safe, awSafe] (int r)
                                        {
                                            if (safe == nullptr || safe->mainComponent == nullptr || r != 1
                                                || awSafe == nullptr)
                                                return;
                                            const auto name = awSafe->getTextEditorContents ("name").trim();
                                            if (name.isEmpty())
                                                return;
                                            auto preset = safe->mainComponent->captureScopeLayoutPreset (name);
                                            ScopeLayoutPresets::savePreset (preset);
                                        }),
                                    true);
                            }
                            else if (result >= 100)
                            {
                                const int idx = result - 100;
                                if (idx >= 0 && idx < (int) presetNames.size())
                                {
                                    for (const auto& p : ScopeLayoutPresets::loadForMode (stripOn))
                                    {
                                        if (p.name == presetNames[(size_t) idx])
                                        {
                                            safe->mainComponent->applyScopeLayoutPreset (p, true);
                                            break;
                                        }
                                    }
                                }
                            }
                            else if (result >= 10 && result < 100)
                            {
                                const int idx = result - 10;
                                if (idx >= 0 && idx < (int) menuIds.size())
                                {
                                    const auto id = menuIds[(size_t) idx];
                                    const bool nowEnabled = safe->mainComponent->isScopeModuleEnabled (id);
                                    safe->mainComponent->setScopeModuleEnabled (id, ! nowEnabled, true);
                                }
                            }
                        });
}

void EqEditor::loadUiPrefs()
{
    tooltipsEnabled = true;
    bool ecoEnabled = false;
    bool disableGlow = false;
    bool scopeTapPost = false; // default Pre (analyzer)
    int scopeStripH = 200;
    bool scopeStrip = false;
    float scopeSplitX = 0.5f;
    float scopeSplitY = 0.5f;
    juce::String scopeFractionsStr;
    std::vector<ScopeModuleId> scopeModules = ScopeModules::defaultEnabledOrder();
    bool randFaceplate = true, randGraph = true, randMenu = true;
    bool randRampFft = true, randRampSpec = true, randRampSpec3D = true, randRampFill = true;
    bool orderedRampGradation = true;
    bool pianoDisplayOnLoad = false;
    bool spec3DOnLoad = false;
    bool spec3DFrameCustom = false;
    int spec3DFrameX = 0, spec3DFrameY = 0, spec3DFrameW = 0, spec3DFrameH = 0;
    int spec3DMeshQuality = 1; // medium
    float spec3DFreqMeshBias = 0.0f;
    float spec3DFreqMeshBiasPivot = Spectrogram3DComponent::kFreqMeshBiasPivotDefault;
    int spec3DMsaaLevel = 4; // 4x default
    bool spec3DTransparentBg = true;
    bool spec3DReverseFreq = true;
    float spec3DMeshHeight = Spectrogram3DComponent::kDefaultMeshHeight;
    bool spec3DLighting = false;
    float spec3DLightingAmt = 0.70f;
    float spec3DLightAz = -40.0f;
    float spec3DLightEl = 55.0f;
    float spec3DSpecular = 0.35f;
    float spec3DRoughness = 0.45f;
    float spec3DMetalness = 0.0f;
    float spec3DRim = 0.22f;
    juce::uint32 spec3DLightCol = 0xffffffff;
    juce::uint32 spec3DRimCol = 0xffffffff;
    bool spec3DDome = false;
    float spec3DDomeStr = 0.35f;
    juce::uint32 spec3DDomeSky = 0xff7390bf;
    juce::uint32 spec3DDomeGround = 0xff403328;
    bool spec3DDomeTex = false;
    int spec3DDomeTexSrc = 0; // Venice Sunset
    juce::String spec3DDomeTexPath;
    bool spec3DSsgi = false;
    float spec3DSsgiStr = 0.40f;
    float spec3DSsgiRad = 0.45f;
    int spec3DSsgiQuality = 1; // medium
    bool spec3DSsr = true;
    float spec3DSsrStr = 0.55f;
    float spec3DSsrDist = 0.55f;
    float spec3DSsrThick = 0.40f;
    int spec3DSsrQuality = 1; // medium
    float spec3DSsrFresnel = 0.75f;
    float spec3DSsrRoughInf = 0.85f;
    float spec3DSsrIntensity = 1.0f;
    float spec3DSsrEdgeFade = 0.15f;
    float spec3DSsrMetalBias = 0.35f;
    float spec3DSsrDomeFb = 0.65f;
    bool spec3DEnergyConserve = false;
    bool spec3DTonemap = false;
    float spec3DExposure = -0.3f;
    int spec3DGrade = 2; // Warm Cinema
    bool spec3DContactShadow = false;
    float spec3DContactShadowStr = 0.45f;
    bool spec3DSelfShadow = false;
    float spec3DSelfShadowStr = 0.85f;
    float spec3DSelfShadowBias = 0.35f;
    float spec3DSelfShadowSoft = 0.85f;
    int spec3DSelfShadowQuality = 1; // medium
    bool spec3DCastShadows = false;
    int spec3DShadowMapRes = 1024;
    int spec3DShadowCascades = 1;
    float spec3DShadowCascadeDist = 3.0f;
    float spec3DShadowCascadeTrans = 0.10f;
    bool spec3DDebugSphere = false;
    float spec3DDebugSphereDiam = Spectrogram3DComponent::kDebugSphereDefaultDiameter;
    float spec3DDebugSphereX = 0.6f;
    float spec3DDebugSphereY = Spectrogram3DComponent::kDebugSphereDefaultDiameter * 0.5f;
    float spec3DDebugSphereZ = 0.6f;
    int spec3DDebugSphereAlbedo = (int) juce::Colours::white.getARGB();
    float spec3DDebugSphereRough = 0.70f;
    float spec3DDebugSphereMetal = 0.0f;
    float spec3DDebugSphereSpec = 1.0f;
    bool spec3DSsao = false;
    float spec3DSsaoStr = 0.55f;
    float spec3DSsaoRad = 1.0f;
    bool spec3DBloom = false;
    float spec3DBloomStr = 0.45f;
    float spec3DBloomThr = 0.62f;
    bool spec3DDof = false;
    float spec3DDofFocus = Spectrogram3DComponent::kDofFocusDefault;
    float spec3DDofFStop = Spectrogram3DComponent::kDofFStopDefault;
    float spec3DDofFocalMm = Spectrogram3DComponent::kDofFocalLengthDefaultMm;
    int spec3DDofQuality = 1; // medium
    float spec3DDofBlurScale = Spectrogram3DComponent::kDofBlurScaleDefault;
    float spec3DDofCocDilate = Spectrogram3DComponent::kDofCocDilateDefault;
    float spec3DDofEdgeSpill = Spectrogram3DComponent::kDofEdgeSpillDefault;
    bool spec3DClosedMesh = false;
    bool spec3DAutoRotate = false;
    float spec3DAutoRotatePeriod = Spectrogram3DComponent::kAutoRotatePeriodDefaultSec;
    bool spec3DZoomOsc = false;
    float spec3DZoomOscDepth = Spectrogram3DComponent::kZoomOscillateDepthDefault;
    float spec3DZoomOscPeriod = Spectrogram3DComponent::kZoomOscillatePeriodDefaultSec;
    bool spec3DAudioLevel = false;
    int spec3DAudioTarget = 0;
    float spec3DAudioMinPct = Spectrogram3DComponent::kAudioLevelMinPercentDefault;
    float spec3DAudioMaxPct = Spectrogram3DComponent::kAudioLevelMaxPercentDefault;
    float spec3DAudioHp = Spectrogram3DComponent::kAudioLevelHpDefaultHz;
    float spec3DAudioLp = Spectrogram3DComponent::kAudioLevelLpDefaultHz;
    float spec3DAudioThresh = Spectrogram3DComponent::kAudioLevelThresholdDefaultDb;
    int spec3DAudioSpeed = (int) Spectrogram3DComponent::AudioLevelSpeed::fast;
    bool spec3DAudioPlayhead = false;
    bool spec3DAudioAntiPlayhead = false;
    float spec3DNormalCusp = Spectrogram3DComponent::kNormalCuspDefaultDeg;
    int spec3DNormalWeight = (int) Spectrogram3DComponent::NormalWeighting::angleAndArea;
    bool spec3DSss = false;
    float spec3DSssStr = 0.45f;
    float spec3DSssWrap = 0.55f;
    float spec3DSssTrans = 0.65f;
    juce::uint32 spec3DSssTint = 0xffe8b090;
    float spec3DSssRad = 0.40f;
    float spec3DSssContrast = 0.50f;
    int spec3DSssQuality = 1; // medium
    float spec3DSssThickScale = 0.50f;
    float spec3DSssMaxThick = 0.70f;
    bool spec3DParticle = false;
    int spec3DParticleEmitMode = 0; // 0 = slice, 1 = continuous
    float spec3DParticleEmission = 0.5f;
    float spec3DParticleSpeed = 1.0f;
    float spec3DParticleVelRandom = 0.0f;
    float spec3DParticleLifespan = 0.0f;
    float spec3DParticleLifespanRandom = 0.0f;
    float spec3DParticleSize = 0.008f;
    bool spec3DParticleEmissive = true;
    float spec3DParticleEmissiveStr = 1.0f;
    float spec3DParticleRough = 0.45f;
    float spec3DParticleMetal = 0.0f;
    float spec3DParticleSpec = 0.35f;
    std::array<ParticleModSlot, kParticleModSlotCount> spec3DParticleMods {};
    bool oscExpandedOnLoad = false;
    bool gonExpandedOnLoad = false;
    bool specExpandedOnLoad = false;
    bool scopeModeOnLoad = false;
    bool uiCompactOnLoad = true; // factory default matches ctor (compact)
    bool modPanelOnLoad = false;
    bool spec3DCamCustom = false;
    auto spec3DCam = Spectrogram3DComponent::getFactoryCameraState();
    juce::ValueTree sessionGlobalUi;
    bool haveSessionTheme = false;

    // Prefer host-session blob (project reopen) over machine-wide ui_prefs.xml.
    std::unique_ptr<juce::XmlElement> sessionPrefsXml;
    juce::ValueTree sessionUi;
    if (audioProcessor.tryGetSessionUiState (sessionUi))
    {
        if (auto prefsTree = sessionUi.getChildWithName ("UiPrefs"); prefsTree.isValid())
            sessionPrefsXml = prefsTree.createXml();
        sessionGlobalUi = sessionUi.getChildWithName ("GlobalUi");
        haveSessionTheme = sessionUi.getChildWithName ("Theme").isValid()
                           || sessionUi.getChildWithName ("GlobalUi").isValid();
        scopeModeOnLoad = (bool) sessionUi.getProperty ("scopeMode", false);
        uiCompactOnLoad = (bool) sessionUi.getProperty ("uiCompact", true);
        modPanelOnLoad = (bool) sessionUi.getProperty ("modPanelOpen", false);
    }

    const auto file = getUiPrefsFile();
    std::unique_ptr<juce::XmlElement> fileXml;
    if (sessionPrefsXml == nullptr && file.existsAsFile())
        fileXml = juce::parseXML (file);

    juce::XmlElement* xml = sessionPrefsXml != nullptr ? sessionPrefsXml.get()
                                                       : fileXml.get();
    if (xml != nullptr && xml->hasTagName ("UiPrefs"))
    {
                tooltipsEnabled = xml->getBoolAttribute ("tooltipsEnabled", true);
                ecoEnabled = xml->getBoolAttribute ("ecoEnabled", false);
                disableGlow = xml->getBoolAttribute ("disableGlowShadowEffects", false);
                scopeTapPost = xml->getBoolAttribute ("scopeTapPost", false);
                scopeStripH = xml->getIntAttribute ("scopeStripHeightPx", 200);
                scopeStrip = xml->getBoolAttribute ("scopeStripLayout", false);
                scopeSplitX = (float) xml->getDoubleAttribute ("scopeSplitX", 0.5);
                scopeSplitY = (float) xml->getDoubleAttribute ("scopeSplitY", 0.5);
                scopeFractionsStr = xml->getStringAttribute ("scopeStripFractions", {});
                lastScopeWidth = xml->getIntAttribute ("lastScopeWidth", 0);
                lastScopeHeight = xml->getIntAttribute ("lastScopeHeight", 0);
                lastScopeWasStrip = xml->getBoolAttribute ("lastScopeWasStrip", false);
                lastStripScopeWidth = xml->getIntAttribute ("lastStripScopeWidth", 0);
                lastStripScopeHeight = xml->getIntAttribute ("lastStripScopeHeight", 0);
                lastTiledScopeWidth = xml->getIntAttribute ("lastTiledScopeWidth", 0);
                lastTiledScopeHeight = xml->getIntAttribute ("lastTiledScopeHeight", 0);
                // Migrate older single-size prefs into the per-arrange slots.
                if (lastStripScopeWidth <= 0 && lastStripScopeHeight <= 0
                    && lastScopeWasStrip && lastScopeWidth > 0 && lastScopeHeight > 0)
                {
                    lastStripScopeWidth = lastScopeWidth;
                    lastStripScopeHeight = lastScopeHeight;
                }
                if (lastTiledScopeWidth <= 0 && lastTiledScopeHeight <= 0
                    && ! lastScopeWasStrip && lastScopeWidth > 0 && lastScopeHeight > 0)
                {
                    lastTiledScopeWidth = lastScopeWidth;
                    lastTiledScopeHeight = lastScopeHeight;
                }
                if (xml->hasAttribute ("scopeModules"))
                {
                    scopeModules = ScopeModules::orderFromString (xml->getStringAttribute ("scopeModules"));
                }
                else
                {
                    const auto orderStr = xml->getStringAttribute ("scopePaneOrder", "0,1,2,3");
                    auto parts = juce::StringArray::fromTokens (orderStr, ",", {});
                    if (parts.size() == 4)
                    {
                        static const ScopeModuleId legacyMap[4] = {
                            ScopeModuleId::goniometer,
                            ScopeModuleId::spectrum,
                            ScopeModuleId::oscilloscope,
                            ScopeModuleId::spectrogram
                        };
                        scopeModules.clear();
                        std::array<bool, ScopeModules::kNumModules> seen {};
                        for (int i = 0; i < 4; ++i)
                        {
                            const int v = parts[i].getIntValue();
                            if (v < 0 || v > 3 || seen[(size_t) legacyMap[v]])
                                continue;
                            seen[(size_t) legacyMap[v]] = true;
                            scopeModules.push_back (legacyMap[v]);
                        }
                        if (scopeModules.empty())
                            scopeModules = ScopeModules::defaultEnabledOrder();
                    }
                }
                randFaceplate = xml->getBoolAttribute ("randFaceplateMod", true);
                randGraph = xml->getBoolAttribute ("randGraph", true);
                randMenu = xml->getBoolAttribute ("randMenu", true);
                randRampFft = xml->getBoolAttribute ("randRampFft", true);
                randRampSpec = xml->getBoolAttribute ("randRampSpec", true);
                randRampSpec3D = xml->getBoolAttribute ("randRampSpec3D", true);
                randRampFill = xml->getBoolAttribute ("randRampFill", true);
                orderedRampGradation = xml->getBoolAttribute ("orderedRampGradation", true);
                faceplateBank = juce::jlimit (0, EqBand::kMaxBanks - 1,
                                              xml->getIntAttribute ("faceplateBank", 0));
                pianoDisplayOnLoad = xml->getBoolAttribute ("pianoDisplay", false);
                spec3DOnLoad = xml->getBoolAttribute ("spec3d", false);
                spec3DFrameCustom = xml->getBoolAttribute ("spec3dFrameCustom", false);
                spec3DFrameX = xml->getIntAttribute ("spec3dFrameX", 0);
                spec3DFrameY = xml->getIntAttribute ("spec3dFrameY", 0);
                spec3DFrameW = xml->getIntAttribute ("spec3dFrameW", 0);
                spec3DFrameH = xml->getIntAttribute ("spec3dFrameH", 0);
                spec3DMeshQuality = xml->getIntAttribute ("spec3dMeshQuality", 1);
                spec3DFreqMeshBias = (float) xml->getDoubleAttribute ("spec3dFreqMeshBias", 0.0);
                spec3DFreqMeshBiasPivot = (float) xml->getDoubleAttribute (
                    "spec3dFreqMeshBiasPivot",
                    (double) Spectrogram3DComponent::kFreqMeshBiasPivotDefault);
                if (xml->hasAttribute ("spec3dMsaaLevel"))
                {
                    const int level = xml->getIntAttribute ("spec3dMsaaLevel", 4);
                    spec3DMsaaLevel = (level == 0 || level == 4 || level == 8 || level == 16) ? level : 4;
                }
                else
                {
                    // Migrate legacy bool: on → 4x, off stays off.
                    spec3DMsaaLevel = xml->getBoolAttribute ("spec3dMsaa", true) ? 4 : 0;
                }
                spec3DTransparentBg = xml->getBoolAttribute ("spec3dTransparentBg", true);
                spec3DReverseFreq = xml->getBoolAttribute ("spec3dReverseFreq", true);
                spec3DMeshHeight = (float) xml->getDoubleAttribute (
                    "spec3dMeshHeight", Spectrogram3DComponent::kDefaultMeshHeight);
                spec3DLighting = xml->getBoolAttribute ("spec3dLighting", false);
                spec3DLightingAmt = (float) xml->getDoubleAttribute ("spec3dLightingAmt", 0.70);
                spec3DLightAz = (float) xml->getDoubleAttribute ("spec3dLightAz", -40.0);
                spec3DLightEl = (float) xml->getDoubleAttribute ("spec3dLightEl", 55.0);
                spec3DSpecular = (float) xml->getDoubleAttribute ("spec3dSpecular", 0.35);
                spec3DRoughness = (float) xml->getDoubleAttribute ("spec3dRoughness", 0.45);
                spec3DMetalness = (float) xml->getDoubleAttribute ("spec3dMetalness", 0.0);
                spec3DRim = (float) xml->getDoubleAttribute ("spec3dRim", 0.22);
                spec3DLightCol = (juce::uint32) xml->getIntAttribute ("spec3dLightCol", (int) 0xffffffff);
                spec3DRimCol = (juce::uint32) xml->getIntAttribute ("spec3dRimCol", (int) 0xffffffff);
                spec3DDome = xml->getBoolAttribute ("spec3dDome", false);
                spec3DDomeStr = (float) xml->getDoubleAttribute ("spec3dDomeStr", 0.35);
                spec3DDomeSky = (juce::uint32) xml->getIntAttribute ("spec3dDomeSky", (int) 0xff7390bf);
                spec3DDomeGround = (juce::uint32) xml->getIntAttribute ("spec3dDomeGround", (int) 0xff403328);
                spec3DDomeTex = xml->getBoolAttribute ("spec3dDomeTex", false);
                spec3DDomeTexSrc = xml->getIntAttribute ("spec3dDomeTexSrc", 0);
                spec3DDomeTexPath = xml->getStringAttribute ("spec3dDomeTexPath");
                spec3DSsgi = xml->getBoolAttribute ("spec3dSsgi", false);
                spec3DSsgiStr = (float) xml->getDoubleAttribute ("spec3dSsgiStr", 0.40);
                spec3DSsgiRad = (float) xml->getDoubleAttribute ("spec3dSsgiRad", 0.45);
                spec3DSsgiQuality = xml->getIntAttribute ("spec3dSsgiQuality", 1);
                spec3DSsr = xml->getBoolAttribute ("spec3dSsr", true);
                spec3DSsrStr = (float) xml->getDoubleAttribute ("spec3dSsrStr", 0.55);
                spec3DSsrDist = (float) xml->getDoubleAttribute ("spec3dSsrDist", 0.55);
                spec3DSsrThick = (float) xml->getDoubleAttribute ("spec3dSsrThick", 0.40);
                spec3DSsrQuality = xml->getIntAttribute ("spec3dSsrQuality", 1);
                spec3DSsrFresnel = (float) xml->getDoubleAttribute ("spec3dSsrFresnel", 0.75);
                spec3DSsrRoughInf = (float) xml->getDoubleAttribute ("spec3dSsrRoughInf", 0.85);
                spec3DSsrIntensity = (float) xml->getDoubleAttribute ("spec3dSsrIntensity", 1.0);
                spec3DSsrEdgeFade = (float) xml->getDoubleAttribute ("spec3dSsrEdgeFade", 0.15);
                spec3DSsrMetalBias = (float) xml->getDoubleAttribute ("spec3dSsrMetalBias", 0.35);
                spec3DSsrDomeFb = (float) xml->getDoubleAttribute ("spec3dSsrDomeFb", 0.65);
                spec3DEnergyConserve = xml->getBoolAttribute ("spec3dEnergyConserve", false);
                spec3DTonemap = xml->getBoolAttribute ("spec3dTonemap", false);
                spec3DExposure = (float) xml->getDoubleAttribute ("spec3dExposure", -0.3);
                spec3DGrade = xml->getIntAttribute ("spec3dGrade", 2); // Warm Cinema
                spec3DContactShadow = xml->getBoolAttribute ("spec3dContactShadow", false);
                spec3DContactShadowStr = (float) xml->getDoubleAttribute ("spec3dContactShadowStr", 0.45);
                spec3DSelfShadow = xml->getBoolAttribute ("spec3dSelfShadow", false);
                spec3DSelfShadowStr = (float) xml->getDoubleAttribute ("spec3dSelfShadowStr", 0.85);
                spec3DSelfShadowBias = (float) xml->getDoubleAttribute ("spec3dSelfShadowBias", 0.35);
                spec3DSelfShadowSoft = (float) xml->getDoubleAttribute ("spec3dSelfShadowSoft", 0.85);
                spec3DSelfShadowQuality = xml->getIntAttribute ("spec3dSelfShadowQuality", 1);
                spec3DCastShadows = xml->getBoolAttribute ("spec3dCastShadows", false);
                spec3DShadowMapRes = xml->getIntAttribute ("spec3dShadowMapRes", 1024);
                spec3DShadowCascades = xml->getIntAttribute ("spec3dShadowCascades", 1);
                spec3DShadowCascadeDist = (float) xml->getDoubleAttribute ("spec3dShadowCascadeDist", 3.0);
                spec3DShadowCascadeTrans = (float) xml->getDoubleAttribute ("spec3dShadowCascadeTrans", 0.10);
                spec3DDebugSphere = xml->getBoolAttribute ("spec3dDebugSphere", false);
                spec3DDebugSphereDiam = (float) xml->getDoubleAttribute (
                    "spec3dDebugSphereDiam", Spectrogram3DComponent::kDebugSphereDefaultDiameter);
                spec3DDebugSphereX = (float) xml->getDoubleAttribute ("spec3dDebugSphereX", 0.6);
                spec3DDebugSphereY = (float) xml->getDoubleAttribute (
                    "spec3dDebugSphereY", Spectrogram3DComponent::kDebugSphereDefaultDiameter * 0.5);
                spec3DDebugSphereZ = (float) xml->getDoubleAttribute ("spec3dDebugSphereZ", 0.6);
                spec3DDebugSphereAlbedo = xml->getIntAttribute (
                    "spec3dDebugSphereAlbedo", (int) juce::Colours::white.getARGB());
                spec3DDebugSphereRough = (float) xml->getDoubleAttribute ("spec3dDebugSphereRough", 0.70);
                spec3DDebugSphereMetal = (float) xml->getDoubleAttribute ("spec3dDebugSphereMetal", 0.0);
                spec3DDebugSphereSpec = (float) xml->getDoubleAttribute ("spec3dDebugSphereSpec", 1.0);
                spec3DSsao = xml->getBoolAttribute ("spec3dSsao", false);
                spec3DSsaoStr = (float) xml->getDoubleAttribute ("spec3dSsaoStr", 0.55);
                spec3DSsaoRad = (float) xml->getDoubleAttribute ("spec3dSsaoRad", 1.0);
                spec3DBloom = xml->getBoolAttribute ("spec3dBloom", false);
                spec3DBloomStr = (float) xml->getDoubleAttribute ("spec3dBloomStr", 0.45);
                spec3DBloomThr = (float) xml->getDoubleAttribute ("spec3dBloomThr", 0.62);
                spec3DDof = xml->getBoolAttribute ("spec3dDof", false);
                spec3DDofFocus = (float) xml->getDoubleAttribute (
                    "spec3dDofFocus", Spectrogram3DComponent::kDofFocusDefault);
                // Prefer F-Stop / focal length; migrate legacy aperture/amount → F-Stop.
                if (xml->hasAttribute ("spec3dDofFStop"))
                {
                    spec3DDofFStop = (float) xml->getDoubleAttribute (
                        "spec3dDofFStop", Spectrogram3DComponent::kDofFStopDefault);
                }
                else
                {
                    float legacyAperture = Spectrogram3DComponent::kDofApertureDefault;
                    if (xml->hasAttribute ("spec3dDofAperture"))
                        legacyAperture = (float) xml->getDoubleAttribute (
                            "spec3dDofAperture", Spectrogram3DComponent::kDofApertureDefault);
                    else if (xml->hasAttribute ("spec3dDofAmount"))
                        legacyAperture = (float) xml->getDoubleAttribute (
                            "spec3dDofAmount", Spectrogram3DComponent::kDofApertureDefault);
                    const float t = juce::jlimit (0.0f, 1.0f,
                                                  legacyAperture / Spectrogram3DComponent::kDofApertureMax);
                    spec3DDofFStop = juce::jmap (t,
                                                 Spectrogram3DComponent::kDofFStopMax,
                                                 Spectrogram3DComponent::kDofFStopMin);
                }
                spec3DDofFocalMm = (float) xml->getDoubleAttribute (
                    "spec3dDofFocalMm", Spectrogram3DComponent::kDofFocalLengthDefaultMm);
                spec3DDofQuality = xml->getIntAttribute ("spec3dDofQuality", 1);
                spec3DDofBlurScale = (float) xml->getDoubleAttribute (
                    "spec3dDofBlurScale", Spectrogram3DComponent::kDofBlurScaleDefault);
                spec3DDofCocDilate = (float) xml->getDoubleAttribute (
                    "spec3dDofCocDilate", Spectrogram3DComponent::kDofCocDilateDefault);
                spec3DDofEdgeSpill = (float) xml->getDoubleAttribute (
                    "spec3dDofEdgeSpill", Spectrogram3DComponent::kDofEdgeSpillDefault);
                // Migrate legacy SSS mode combo (0=off, 1=heightfield, 2=closed).
                const int legacySssMode = xml->getIntAttribute ("spec3dSssMode", -1);
                if (xml->hasAttribute ("spec3dClosedMesh"))
                    spec3DClosedMesh = xml->getBoolAttribute ("spec3dClosedMesh", false);
                else
                    spec3DClosedMesh = (legacySssMode >= 2);
                spec3DAutoRotate = xml->getBoolAttribute ("spec3dAutoRotate", false);
                spec3DAutoRotatePeriod = (float) xml->getDoubleAttribute (
                    "spec3dAutoRotatePeriod",
                    Spectrogram3DComponent::kAutoRotatePeriodDefaultSec);
                spec3DZoomOsc = xml->getBoolAttribute ("spec3dZoomOsc", false);
                spec3DZoomOscDepth = (float) xml->getDoubleAttribute (
                    "spec3dZoomOscDepth", Spectrogram3DComponent::kZoomOscillateDepthDefault);
                spec3DZoomOscPeriod = (float) xml->getDoubleAttribute (
                    "spec3dZoomOscPeriod", Spectrogram3DComponent::kZoomOscillatePeriodDefaultSec);
                if (mainComponent != nullptr && xml->hasAttribute ("spec3dRampSequence"))
                {
                    const auto seqStr = xml->getStringAttribute ("spec3dRampSequence");
                    if (seqStr.isNotEmpty())
                        if (auto seqXml = juce::parseXML (seqStr))
                            if (auto seqTree = juce::ValueTree::fromXml (*seqXml); seqTree.isValid())
                                mainComponent->setSpec3DRampSequence (
                                    Spec3DRampSequence::fromValueTree (seqTree), false);
                }
                spec3DAudioLevel = xml->getBoolAttribute ("spec3dAudioLevel", false);
                spec3DAudioTarget = xml->getIntAttribute ("spec3dAudioTarget", 0);
                spec3DAudioMinPct = (float) xml->getDoubleAttribute (
                    "spec3dAudioMinPct", Spectrogram3DComponent::kAudioLevelMinPercentDefault);
                spec3DAudioMaxPct = (float) xml->getDoubleAttribute (
                    "spec3dAudioMaxPct", Spectrogram3DComponent::kAudioLevelMaxPercentDefault);
                spec3DAudioHp = (float) xml->getDoubleAttribute (
                    "spec3dAudioHp", Spectrogram3DComponent::kAudioLevelHpDefaultHz);
                spec3DAudioLp = (float) xml->getDoubleAttribute (
                    "spec3dAudioLp", Spectrogram3DComponent::kAudioLevelLpDefaultHz);
                spec3DAudioThresh = (float) xml->getDoubleAttribute (
                    "spec3dAudioThresh", Spectrogram3DComponent::kAudioLevelThresholdDefaultDb);
                spec3DAudioSpeed = xml->getIntAttribute (
                    "spec3dAudioSpeed", (int) Spectrogram3DComponent::AudioLevelSpeed::fast);
                spec3DAudioPlayhead = xml->getBoolAttribute ("spec3dAudioPlayhead", false);
                spec3DAudioAntiPlayhead = xml->getBoolAttribute ("spec3dAudioAntiPlayhead", false);
                spec3DNormalCusp = (float) xml->getDoubleAttribute (
                    "spec3dNormalCusp", Spectrogram3DComponent::kNormalCuspDefaultDeg);
                spec3DNormalWeight = xml->getIntAttribute (
                    "spec3dNormalWeight",
                    (int) Spectrogram3DComponent::NormalWeighting::angleAndArea);
                if (xml->hasAttribute ("spec3dSss"))
                    spec3DSss = xml->getBoolAttribute ("spec3dSss", false);
                else
                    spec3DSss = (legacySssMode >= 1);
                spec3DSssStr = (float) xml->getDoubleAttribute ("spec3dSssStr", 0.45);
                spec3DSssWrap = (float) xml->getDoubleAttribute ("spec3dSssWrap", 0.55);
                spec3DSssTrans = (float) xml->getDoubleAttribute ("spec3dSssTrans", 0.65);
                spec3DSssTint = (juce::uint32) xml->getIntAttribute ("spec3dSssTint", (int) 0xffe8b090);
                spec3DSssRad = (float) xml->getDoubleAttribute ("spec3dSssRad", 0.40);
                spec3DSssContrast = (float) xml->getDoubleAttribute ("spec3dSssContrast", 0.50);
                spec3DSssQuality = xml->getIntAttribute ("spec3dSssQuality", 1);
                spec3DSssThickScale = (float) xml->getDoubleAttribute ("spec3dSssThickScale", 0.50);
                spec3DSssMaxThick = (float) xml->getDoubleAttribute ("spec3dSssMaxThick", 0.70);
                spec3DParticle = xml->getBoolAttribute ("spec3dParticleMode", false);
                spec3DParticleEmitMode = xml->getIntAttribute ("spec3dParticleEmitMode", 0);
                spec3DParticleEmission = (float) xml->getDoubleAttribute ("spec3dParticleEmission", 0.5);
                // Migrate old 0..1 emission into 0..5 range (v1 max felt sparse).
                if (spec3DParticleEmission > 0.0f && spec3DParticleEmission <= 1.0f
                    && ! xml->hasAttribute ("spec3dParticleEmissionV2"))
                    spec3DParticleEmission = juce::jmin (5.0f, spec3DParticleEmission * 1.5f);
                spec3DParticleSpeed = (float) xml->getDoubleAttribute ("spec3dParticleSpeed", 1.0);
                spec3DParticleVelRandom = (float) xml->getDoubleAttribute ("spec3dParticleVelRandom", 0.0);
                spec3DParticleLifespan = (float) xml->getDoubleAttribute ("spec3dParticleLifespan", 0.0);
                spec3DParticleLifespanRandom = (float) xml->getDoubleAttribute ("spec3dParticleLifespanRandom", 0.0);
                spec3DParticleSize = (float) xml->getDoubleAttribute ("spec3dParticleSize", 0.008);
                spec3DParticleEmissive = xml->getBoolAttribute ("spec3dParticleEmissive", true);
                spec3DParticleEmissiveStr = (float) xml->getDoubleAttribute ("spec3dParticleEmissiveStr", 1.0);
                spec3DParticleRough = (float) xml->getDoubleAttribute ("spec3dParticleRough", 0.45);
                spec3DParticleMetal = (float) xml->getDoubleAttribute ("spec3dParticleMetal", 0.0);
                spec3DParticleSpec = (float) xml->getDoubleAttribute ("spec3dParticleSpec", 0.35);
                for (int i = 0; i < kParticleModSlotCount; ++i)
                {
                    const auto key = "spec3dPMod" + juce::String (i);
                    const auto s = xml->getStringAttribute (key, {});
                    if (s.isEmpty())
                        continue;
                    // en,src,dst,op,amt,k[,curve,thrEn,thr,atk,rel]
                    juce::StringArray parts;
                    parts.addTokens (s, ",", "");
                    if (parts.size() < 4)
                        continue;
                    auto& slot = spec3DParticleMods[(size_t) i];
                    slot.enabled = parts[0].getIntValue() != 0;
                    slot.source = (ParticleModSource) juce::jlimit (0, 6, parts[1].getIntValue());
                    slot.dest = (ParticleModDest) juce::jlimit (0, 7, parts[2].getIntValue());
                    slot.op = (ParticleModOp) juce::jlimit (0, 2, parts[3].getIntValue());
                    if (parts.size() > 4)
                        slot.amount = (float) parts[4].getDoubleValue();
                    if (parts.size() > 5)
                        slot.constant = (float) parts[5].getDoubleValue();
                    if (parts.size() > 6)
                        slot.curveShape = (float) parts[6].getDoubleValue();
                    if (parts.size() > 7)
                        slot.thresholdEnabled = parts[7].getIntValue() != 0;
                    if (parts.size() > 8)
                        slot.threshold = (float) parts[8].getDoubleValue();
                    if (parts.size() > 9)
                        slot.attackMs = (float) parts[9].getDoubleValue();
                    if (parts.size() > 10)
                        slot.releaseMs = (float) parts[10].getDoubleValue();
                }
                oscExpandedOnLoad = xml->getBoolAttribute ("oscExpanded", false);
                gonExpandedOnLoad = xml->getBoolAttribute ("gonExpanded", false);
                specExpandedOnLoad = xml->getBoolAttribute ("specExpanded", false);
                spec3DCamCustom = xml->getBoolAttribute ("spec3dCamCustom", false);
                if (spec3DCamCustom)
                {
                    spec3DCam.yawDeg = (float) xml->getDoubleAttribute ("spec3dCamYaw", spec3DCam.yawDeg);
                    spec3DCam.pitchDeg = (float) xml->getDoubleAttribute ("spec3dCamPitch", spec3DCam.pitchDeg);
                    spec3DCam.distance = (float) xml->getDoubleAttribute ("spec3dCamDist", spec3DCam.distance);
                    spec3DCam.panX = (float) xml->getDoubleAttribute ("spec3dCamPanX", spec3DCam.panX);
                    spec3DCam.panY = (float) xml->getDoubleAttribute ("spec3dCamPanY", spec3DCam.panY);
                    spec3DCam.panZ = (float) xml->getDoubleAttribute ("spec3dCamPanZ", spec3DCam.panZ);
                }
                // Live layout flags may also live on UiPrefs (older host sessions / file prefs).
                scopeModeOnLoad = xml->getBoolAttribute ("scopeMode", scopeModeOnLoad);
                uiCompactOnLoad = xml->getBoolAttribute ("uiCompact", uiCompactOnLoad);
                modPanelOnLoad = xml->getBoolAttribute ("modPanelOpen", modPanelOnLoad);
    }

    if (mainComponent != nullptr)
    {
        mainComponent->setEcoMode (ecoEnabled, false);
        mainComponent->setDisableGlowShadowEffects (disableGlow, false);
        mainComponent->setScopeTapPost (scopeTapPost, false);
        mainComponent->setScopeEnabledOrder (scopeModules, false);
        mainComponent->setScopeStripHeightPx (scopeStripH, false);
        mainComponent->setScopeStripLayout (scopeStrip, false);
        mainComponent->setScopeSplitNorm (scopeSplitX, scopeSplitY);
        if (scopeFractionsStr.isNotEmpty())
            mainComponent->setScopeStripFractions (
                ScopeLayoutPresets::decodeFractions (scopeFractionsStr, (int) scopeModules.size()));
        // Restore Scope mode as last left (geometry above already applied).
        mainComponent->setScopeMode (scopeModeOnLoad, false);
        mainComponent->getFrequencyResponseComponent().setPianoDisplayOn (pianoDisplayOnLoad, false);
        mainComponent->setSpec3DMeshQuality (
            spec3DMeshQuality <= 0 ? Spectrogram3DComponent::MeshQuality::low
                                  : (spec3DMeshQuality == 1 ? Spectrogram3DComponent::MeshQuality::medium
                                     : (spec3DMeshQuality == 2 ? Spectrogram3DComponent::MeshQuality::high
                                                              : Spectrogram3DComponent::MeshQuality::ultra)),
            false);
        mainComponent->setSpec3DMsaaLevel (
            spec3DMsaaLevel == 0 ? Spectrogram3DComponent::MsaaLevel::off
                                : (spec3DMsaaLevel == 8 ? Spectrogram3DComponent::MsaaLevel::x8
                                   : (spec3DMsaaLevel == 16 ? Spectrogram3DComponent::MsaaLevel::x16
                                                           : Spectrogram3DComponent::MsaaLevel::x4)),
            false);
        mainComponent->setSpec3DTransparentBackground (spec3DTransparentBg, false);
        mainComponent->setSpec3DReverseFrequencyAxis (spec3DReverseFreq, false);
        mainComponent->setSpec3DMeshHeight (spec3DMeshHeight, false);
        mainComponent->setSpec3DFreqMeshBias (spec3DFreqMeshBias, false);
        mainComponent->setSpec3DFreqMeshBiasPivot (spec3DFreqMeshBiasPivot, false);
        mainComponent->setSpec3DLightingEnabled (spec3DLighting, false);
        mainComponent->setSpec3DLightingAmount (spec3DLightingAmt, false);
        mainComponent->setSpec3DLightAzimuthDeg (spec3DLightAz, false);
        mainComponent->setSpec3DLightElevationDeg (spec3DLightEl, false);
        mainComponent->setSpec3DSpecularAmount (spec3DSpecular, false);
        mainComponent->setSpec3DRoughnessAmount (spec3DRoughness, false);
        mainComponent->setSpec3DMetalnessAmount (spec3DMetalness, false);
        mainComponent->setSpec3DRimAmount (spec3DRim, false);
        mainComponent->setSpec3DLightColour (juce::Colour (spec3DLightCol), false);
        mainComponent->setSpec3DRimColour (juce::Colour (spec3DRimCol), false);
        mainComponent->setSpec3DDomeFillEnabled (spec3DDome, false);
        mainComponent->setSpec3DDomeFillStrength (spec3DDomeStr, false);
        mainComponent->setSpec3DDomeSkyColour (juce::Colour (spec3DDomeSky), false);
        mainComponent->setSpec3DDomeGroundColour (juce::Colour (spec3DDomeGround), false);
        mainComponent->setSpec3DDomeTextureCustomPath (spec3DDomeTexPath, false);
        mainComponent->setSpec3DDomeTextureSource (
            spec3DDomeTexSrc == 1 ? Spectrogram3DComponent::DomeTextureSource::custom
                                 : Spectrogram3DComponent::DomeTextureSource::veniceSunset,
            false);
        mainComponent->setSpec3DDomeTextureEnabled (spec3DDomeTex, false);
        mainComponent->setSpec3DSsgiEnabled (spec3DSsgi, false);
        mainComponent->setSpec3DSsgiStrength (spec3DSsgiStr, false);
        mainComponent->setSpec3DSsgiRadius (spec3DSsgiRad, false);
        mainComponent->setSpec3DSsgiQuality (
            spec3DSsgiQuality <= 0 ? Spectrogram3DComponent::ShadowQuality::low
                                  : (spec3DSsgiQuality == 1 ? Spectrogram3DComponent::ShadowQuality::medium
                                  : (spec3DSsgiQuality == 2 ? Spectrogram3DComponent::ShadowQuality::high
                                                           : Spectrogram3DComponent::ShadowQuality::ultra)),
            false);
        // SSGI temporal/denoise/half-res/mesh-normals kept in Spectrogram3DComponent for later
        // use, but not loaded from prefs (Look UI removed — use Quality Ultra instead).
        mainComponent->setSpec3DSsgiTemporalEnabled (false, false);
        mainComponent->setSpec3DSsgiDenoiseEnabled (false, false);
        mainComponent->setSpec3DSsgiHalfResEnabled (false, false);
        mainComponent->setSpec3DSsgiMeshNormalsEnabled (false, false);
        mainComponent->setSpec3DSsrEnabled (spec3DSsr, false);
        mainComponent->setSpec3DSsrStrength (spec3DSsrStr, false);
        mainComponent->setSpec3DSsrDistance (spec3DSsrDist, false);
        mainComponent->setSpec3DSsrThickness (spec3DSsrThick, false);
        mainComponent->setSpec3DSsrQuality (
            spec3DSsrQuality <= 0 ? Spectrogram3DComponent::ShadowQuality::low
                                 : (spec3DSsrQuality == 1 ? Spectrogram3DComponent::ShadowQuality::medium
                                 : (spec3DSsrQuality == 2 ? Spectrogram3DComponent::ShadowQuality::high
                                                          : Spectrogram3DComponent::ShadowQuality::ultra)),
            false);
        mainComponent->setSpec3DSsrFresnel (spec3DSsrFresnel, false);
        mainComponent->setSpec3DSsrRoughnessInfluence (spec3DSsrRoughInf, false);
        mainComponent->setSpec3DSsrIntensity (spec3DSsrIntensity, false);
        mainComponent->setSpec3DSsrEdgeFade (spec3DSsrEdgeFade, false);
        mainComponent->setSpec3DSsrMetallicBias (spec3DSsrMetalBias, false);
        mainComponent->setSpec3DSsrDomeFallback (spec3DSsrDomeFb, false);
        mainComponent->setSpec3DEnergyConservingEnabled (spec3DEnergyConserve, false);
        mainComponent->setSpec3DTonemapEnabled (spec3DTonemap, false);
        mainComponent->setSpec3DTonemapExposureStops (spec3DExposure, false);
        {
            const int g = juce::jlimit (0, 5, spec3DGrade);
            mainComponent->setSpec3DColorGrade (
                static_cast<Spectrogram3DComponent::ColorGrade> (g), false);
        }
        mainComponent->setSpec3DContactShadowEnabled (spec3DContactShadow, false);
        mainComponent->setSpec3DContactShadowStrength (spec3DContactShadowStr, false);
        mainComponent->setSpec3DSelfShadowEnabled (spec3DSelfShadow, false);
        mainComponent->setSpec3DSelfShadowStrength (spec3DSelfShadowStr, false);
        mainComponent->setSpec3DSelfShadowBias (spec3DSelfShadowBias, false);
        mainComponent->setSpec3DSelfShadowSoftness (spec3DSelfShadowSoft, false);
        mainComponent->setSpec3DSelfShadowQuality (
            spec3DSelfShadowQuality <= 0 ? Spectrogram3DComponent::ShadowQuality::low
                                        : (spec3DSelfShadowQuality >= 2
                                               ? Spectrogram3DComponent::ShadowQuality::high
                                               : Spectrogram3DComponent::ShadowQuality::medium),
            false);
        mainComponent->setSpec3DCastShadowsEnabled (spec3DCastShadows, false);
        {
            using Res = Spectrogram3DComponent::ShadowMapResolution;
            const auto res = spec3DShadowMapRes <= 512 ? Res::r512
                           : (spec3DShadowMapRes <= 1024 ? Res::r1024
                              : (spec3DShadowMapRes <= 2048 ? Res::r2048 : Res::r4096));
            mainComponent->setSpec3DShadowMapResolution (res, false);
        }
        mainComponent->setSpec3DShadowCascadeCount (spec3DShadowCascades, false);
        mainComponent->setSpec3DShadowCascadeDistributionExponent (spec3DShadowCascadeDist, false);
        mainComponent->setSpec3DShadowCascadeTransitionFraction (spec3DShadowCascadeTrans, false);
        mainComponent->setSpec3DDebugSphereEnabled (spec3DDebugSphere, false);
        mainComponent->setSpec3DDebugSphereDiameter (spec3DDebugSphereDiam, false);
        mainComponent->setSpec3DDebugSpherePosition (
            { spec3DDebugSphereX, spec3DDebugSphereY, spec3DDebugSphereZ }, false);
        mainComponent->setSpec3DDebugSphereAlbedo (juce::Colour ((juce::uint32) spec3DDebugSphereAlbedo),
                                                  false);
        mainComponent->setSpec3DDebugSphereRoughness (spec3DDebugSphereRough, false);
        mainComponent->setSpec3DDebugSphereMetalness (spec3DDebugSphereMetal, false);
        mainComponent->setSpec3DDebugSphereSpecular (spec3DDebugSphereSpec, false);
        mainComponent->setSpec3DSsaoEnabled (spec3DSsao, false);
        mainComponent->setSpec3DSsaoStrength (spec3DSsaoStr, false);
        mainComponent->setSpec3DSsaoRadius (spec3DSsaoRad, false);
        mainComponent->setSpec3DBloomEnabled (spec3DBloom, false);
        mainComponent->setSpec3DBloomStrength (spec3DBloomStr, false);
        mainComponent->setSpec3DBloomThreshold (spec3DBloomThr, false);
        mainComponent->setSpec3DDofEnabled (spec3DDof, false);
        mainComponent->setSpec3DDofFocusDistance (spec3DDofFocus, false);
        mainComponent->setSpec3DDofFStop (spec3DDofFStop, false);
        mainComponent->setSpec3DDofFocalLengthMm (spec3DDofFocalMm, false);
        mainComponent->setSpec3DDofQuality (
            spec3DDofQuality <= 0 ? Spectrogram3DComponent::ShadowQuality::low
                                 : (spec3DDofQuality >= 2 ? Spectrogram3DComponent::ShadowQuality::high
                                                         : Spectrogram3DComponent::ShadowQuality::medium),
            false);
        mainComponent->setSpec3DDofBlurScale (spec3DDofBlurScale, false);
        mainComponent->setSpec3DDofCocDilate (spec3DDofCocDilate, false);
        mainComponent->setSpec3DDofEdgeSpill (spec3DDofEdgeSpill, false);
        mainComponent->setSpec3DClosedMeshEnabled (spec3DClosedMesh, false);
        mainComponent->setSpec3DAutoRotateEnabled (spec3DAutoRotate, false);
        mainComponent->setSpec3DAutoRotatePeriodSec (spec3DAutoRotatePeriod, false);
        mainComponent->setSpec3DZoomOscillateDepth (spec3DZoomOscDepth, false);
        mainComponent->setSpec3DZoomOscillatePeriodSec (spec3DZoomOscPeriod, false);
        mainComponent->setSpec3DZoomOscillateEnabled (spec3DZoomOsc, false);
        mainComponent->setSpec3DAudioLevelTarget (
            static_cast<Spectrogram3DComponent::AudioLevelTarget> (
                juce::jlimit (0, 6, spec3DAudioTarget)),
            false);
        mainComponent->setSpec3DAudioLevelMinPercent (spec3DAudioMinPct, false);
        mainComponent->setSpec3DAudioLevelMaxPercent (spec3DAudioMaxPct, false);
        mainComponent->setSpec3DAudioLevelHpHz (spec3DAudioHp, false);
        mainComponent->setSpec3DAudioLevelLpHz (spec3DAudioLp, false);
        mainComponent->setSpec3DAudioLevelThresholdDb (spec3DAudioThresh, false);
        mainComponent->setSpec3DAudioLevelSpeed (
            static_cast<Spectrogram3DComponent::AudioLevelSpeed> (
                juce::jlimit (0, 2, spec3DAudioSpeed)),
            false);
        mainComponent->setSpec3DAudioLevelAffectPlayhead (spec3DAudioPlayhead, false);
        mainComponent->setSpec3DAudioLevelAffectAntiPlayhead (spec3DAudioAntiPlayhead, false);
        mainComponent->setSpec3DAudioLevelModEnabled (spec3DAudioLevel, false);
        mainComponent->setSpec3DNormalCuspAngleDeg (spec3DNormalCusp, false);
        mainComponent->setSpec3DNormalWeighting (
            static_cast<Spectrogram3DComponent::NormalWeighting> (
                juce::jlimit (0, 3, spec3DNormalWeight)),
            false);
        mainComponent->setSpec3DSssEnabled (spec3DSss, false);
        mainComponent->setSpec3DSssStrength (spec3DSssStr, false);
        mainComponent->setSpec3DSssWrap (spec3DSssWrap, false);
        mainComponent->setSpec3DSssTransmission (spec3DSssTrans, false);
        mainComponent->setSpec3DSssTint (juce::Colour (spec3DSssTint), false);
        mainComponent->setSpec3DSssRadius (spec3DSssRad, false);
        mainComponent->setSpec3DSssContrast (spec3DSssContrast, false);
        mainComponent->setSpec3DSssQuality (
            spec3DSssQuality <= 0 ? Spectrogram3DComponent::ShadowQuality::low
                                 : (spec3DSssQuality >= 2 ? Spectrogram3DComponent::ShadowQuality::high
                                                         : Spectrogram3DComponent::ShadowQuality::medium),
            false);
        mainComponent->setSpec3DSssThicknessScale (spec3DSssThickScale, false);
        mainComponent->setSpec3DSssMaxThickness (spec3DSssMaxThick, false);
        mainComponent->setSpec3DParticleModeEnabled (spec3DParticle, false);
        mainComponent->setSpec3DParticleEmitMode (spec3DParticleEmitMode, false);
        mainComponent->setSpec3DParticleEmission (spec3DParticleEmission, false);
        mainComponent->setSpec3DParticleRiseSpeed (spec3DParticleSpeed, false);
        mainComponent->setSpec3DParticleVelRandom (spec3DParticleVelRandom, false);
        mainComponent->setSpec3DParticleLifespan (spec3DParticleLifespan, false);
        mainComponent->setSpec3DParticleLifespanRandom (spec3DParticleLifespanRandom, false);
        mainComponent->setSpec3DParticleSize (spec3DParticleSize, false);
        mainComponent->setSpec3DParticleEmissiveEnabled (spec3DParticleEmissive, false);
        mainComponent->setSpec3DParticleEmissiveStrength (spec3DParticleEmissiveStr, false);
        mainComponent->setSpec3DParticleRoughness (spec3DParticleRough, false);
        mainComponent->setSpec3DParticleMetalness (spec3DParticleMetal, false);
        mainComponent->setSpec3DParticleSpecular (spec3DParticleSpec, false);
        for (int i = 0; i < kParticleModSlotCount; ++i)
            mainComponent->setSpec3DParticleModSlot (i, spec3DParticleMods[(size_t) i], false);
        if (spec3DCamCustom)
            mainComponent->setSpec3DDefaultCamera (spec3DCam, true);
        // Restore floating 3D / expanded overlays as last left.
        mainComponent->setSpec3DMode (spec3DOnLoad, false);
        if (spec3DFrameCustom && spec3DFrameW > 0 && spec3DFrameH > 0)
            mainComponent->setSpec3DFrameBounds (spec3DFrameX, spec3DFrameY,
                                                 spec3DFrameW, spec3DFrameH);
        mainComponent->setOscExpanded (oscExpandedOnLoad, false);
        mainComponent->setGonExpanded (gonExpandedOnLoad, false);
        mainComponent->setSpecExpanded (specExpandedOnLoad, false);

        // Theme colours: host session first, else last_ui_theme.xml (disk — same path as dice flags).
        if (haveSessionTheme || audioProcessor.hasSessionUiTheme())
            mainComponent->reapplySessionUiThemeFromProcessor();
        else if (loadLastUiThemeFromDisk())
            mainComponent->reapplySessionUiThemeFromProcessor();

        if (sessionGlobalUi.isValid() && sessionGlobalUi.hasType ("GlobalUi"))
            mainComponent->applyGlobalUiModules (sessionGlobalUi);

        // Dice / accessibility scopes from ui_prefs always win after palette restore.
        auto& c = mainComponent->getSharedResources().sharedColors;
        c.randomizeFaceplateMod = randFaceplate;
        c.randomizeGraphModule = randGraph;
        c.randomizeMenuModule = randMenu;
        c.randomizeRampFftBars = randRampFft;
        c.randomizeRampSpectrogram = randRampSpec;
        c.randomizeRampSpectrogram3D = randRampSpec3D;
        c.randomizeRampSpectrumFill = randRampFill;
        c.orderedRampGradation = orderedRampGradation;
        mainComponent->getSharedResources().makeActive();
    }

    // Compact / mod strip as last left (after MainComponent prefs so window height matches).
    if (modPanelOpen != modPanelOnLoad)
    {
        modPanelOpen = modPanelOnLoad;
        if (mainComponent != nullptr)
            mainComponent->getFrequencyResponseComponent().syncModButton (modPanelOpen);
    }
    if (uiCompact != uiCompactOnLoad)
    {
        uiCompact = uiCompactOnLoad;
        applyCompactUi();
    }
    else if (modPanelOnLoad)
    {
        applyCompactUi(); // refresh height for mod panel
    }

    syncScopeModeButton();
    syncScopeModeLayout();
    setFaceplateBank (faceplateBank, false);
}

void EqEditor::requestSaveUiPrefs() noexcept
{
    uiPrefsSavePending = true;
    // ~250 ms after the last change — responsive scrubbing, one write when idle.
    uiPrefsSaveDueMs = juce::Time::getMillisecondCounter() + 250;
    if (! isTimerRunning())
        startTimer (50);
}

void EqEditor::saveUiPrefs() const
{
    uiPrefsSavePending = false;
    auto xml = std::make_unique<juce::XmlElement> ("UiPrefs");
    xml->setAttribute ("tooltipsEnabled", tooltipsEnabled);
    xml->setAttribute ("ecoEnabled", mainComponent != nullptr && mainComponent->isEcoMode());
    xml->setAttribute ("disableGlowShadowEffects",
                       mainComponent != nullptr && mainComponent->areGlowShadowEffectsDisabled());
    xml->setAttribute ("scopeTapPost", mainComponent != nullptr && mainComponent->isScopeTapPost());
    if (mainComponent != nullptr)
    {
        xml->setAttribute ("scopeStripHeightPx", mainComponent->getScopeStripHeightPx());
        xml->setAttribute ("scopeStripLayout", mainComponent->isScopeStripLayout());
        xml->setAttribute ("scopeSplitX", (double) mainComponent->getScopeSplitX());
        xml->setAttribute ("scopeSplitY", (double) mainComponent->getScopeSplitY());
        xml->setAttribute ("scopeStripFractions",
                           ScopeLayoutPresets::encodeFractions (mainComponent->getScopeStripFractions()));
        xml->setAttribute ("scopeModules",
                           ScopeModules::orderToString (mainComponent->getScopeEnabledOrder()));
        const auto& c = mainComponent->getSharedResources().sharedColors;
        xml->setAttribute ("randFaceplateMod", c.randomizeFaceplateMod);
        xml->setAttribute ("randGraph", c.randomizeGraphModule);
        xml->setAttribute ("randMenu", c.randomizeMenuModule);
        xml->setAttribute ("randRampFft", c.randomizeRampFftBars);
        xml->setAttribute ("randRampSpec", c.randomizeRampSpectrogram);
        xml->setAttribute ("randRampSpec3D", c.randomizeRampSpectrogram3D);
        xml->setAttribute ("randRampFill", c.randomizeRampSpectrumFill);
        xml->setAttribute ("orderedRampGradation", c.orderedRampGradation);
    }
    xml->setAttribute ("lastScopeWidth", lastScopeWidth);
    xml->setAttribute ("lastScopeHeight", lastScopeHeight);
    xml->setAttribute ("lastScopeWasStrip", lastScopeWasStrip);
    xml->setAttribute ("lastStripScopeWidth", lastStripScopeWidth);
    xml->setAttribute ("lastStripScopeHeight", lastStripScopeHeight);
    xml->setAttribute ("lastTiledScopeWidth", lastTiledScopeWidth);
    xml->setAttribute ("lastTiledScopeHeight", lastTiledScopeHeight);
    xml->setAttribute ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));
    xml->setAttribute ("faceplateBank", faceplateBank);
    xml->setAttribute ("pianoDisplay",
                       mainComponent != nullptr
                           && mainComponent->getFrequencyResponseComponent().isPianoDisplayOn());
    xml->setAttribute ("spec3d", mainComponent != nullptr && mainComponent->isSpec3DMode());
    if (mainComponent != nullptr)
    {
        xml->setAttribute ("spec3dFrameCustom", mainComponent->hasSpec3DFrameBounds());
        xml->setAttribute ("spec3dFrameX", mainComponent->getSpec3DFramePreferredX());
        xml->setAttribute ("spec3dFrameY", mainComponent->getSpec3DFramePreferredY());
        xml->setAttribute ("spec3dFrameW", mainComponent->getSpec3DFramePreferredW());
        xml->setAttribute ("spec3dFrameH", mainComponent->getSpec3DFramePreferredH());
        const auto q = mainComponent->getSpec3DMeshQuality();
        xml->setAttribute ("spec3dMeshQuality",
                           q == Spectrogram3DComponent::MeshQuality::low ? 0
                               : (q == Spectrogram3DComponent::MeshQuality::medium ? 1
                                  : (q == Spectrogram3DComponent::MeshQuality::high ? 2 : 3)));
        xml->setAttribute ("spec3dMsaaLevel", (int) mainComponent->getSpec3DMsaaLevel());
        xml->setAttribute ("spec3dMsaa", mainComponent->isSpec3DMultisampling()); // legacy
        xml->setAttribute ("spec3dTransparentBg", mainComponent->isSpec3DTransparentBackground());
        xml->setAttribute ("spec3dReverseFreq", mainComponent->isSpec3DReverseFrequencyAxis());
        xml->setAttribute ("spec3dMeshHeight", (double) mainComponent->getSpec3DMeshHeight());
        xml->setAttribute ("spec3dFreqMeshBias", (double) mainComponent->getSpec3DFreqMeshBias());
        xml->setAttribute ("spec3dFreqMeshBiasPivot",
                           (double) mainComponent->getSpec3DFreqMeshBiasPivot());
        xml->setAttribute ("spec3dLighting", mainComponent->isSpec3DLightingEnabled());
        xml->setAttribute ("spec3dLightingAmt", (double) mainComponent->getSpec3DLightingAmount());
        xml->setAttribute ("spec3dLightAz", (double) mainComponent->getSpec3DLightAzimuthDeg());
        xml->setAttribute ("spec3dLightEl", (double) mainComponent->getSpec3DLightElevationDeg());
        xml->setAttribute ("spec3dSpecular", (double) mainComponent->getSpec3DSpecularAmount());
        xml->setAttribute ("spec3dRoughness", (double) mainComponent->getSpec3DRoughnessAmount());
        xml->setAttribute ("spec3dMetalness", (double) mainComponent->getSpec3DMetalnessAmount());
        xml->setAttribute ("spec3dRim", (double) mainComponent->getSpec3DRimAmount());
        xml->setAttribute ("spec3dLightCol", (int) mainComponent->getSpec3DLightColour().getARGB());
        xml->setAttribute ("spec3dRimCol", (int) mainComponent->getSpec3DRimColour().getARGB());
        xml->setAttribute ("spec3dDome", mainComponent->isSpec3DDomeFillEnabled());
        xml->setAttribute ("spec3dDomeStr", (double) mainComponent->getSpec3DDomeFillStrength());
        xml->setAttribute ("spec3dDomeSky", (int) mainComponent->getSpec3DDomeSkyColour().getARGB());
        xml->setAttribute ("spec3dDomeGround", (int) mainComponent->getSpec3DDomeGroundColour().getARGB());
        xml->setAttribute ("spec3dDomeTex", mainComponent->isSpec3DDomeTextureEnabled());
        xml->setAttribute ("spec3dDomeTexSrc",
                           mainComponent->getSpec3DDomeTextureSource()
                               == Spectrogram3DComponent::DomeTextureSource::custom ? 1 : 0);
        xml->setAttribute ("spec3dDomeTexPath", mainComponent->getSpec3DDomeTextureCustomPath());
        xml->setAttribute ("spec3dSsgi", mainComponent->isSpec3DSsgiEnabled());
        xml->setAttribute ("spec3dSsgiStr", (double) mainComponent->getSpec3DSsgiStrength());
        xml->setAttribute ("spec3dSsgiRad", (double) mainComponent->getSpec3DSsgiRadius());
        {
            const auto q = mainComponent->getSpec3DSsgiQuality();
            xml->setAttribute ("spec3dSsgiQuality",
                               q == Spectrogram3DComponent::ShadowQuality::low ? 0
                                   : (q == Spectrogram3DComponent::ShadowQuality::medium ? 1
                                   : (q == Spectrogram3DComponent::ShadowQuality::high ? 2 : 3)));
        }
        xml->setAttribute ("spec3dSsr", mainComponent->isSpec3DSsrEnabled());
        xml->setAttribute ("spec3dSsrStr", (double) mainComponent->getSpec3DSsrStrength());
        xml->setAttribute ("spec3dSsrDist", (double) mainComponent->getSpec3DSsrDistance());
        xml->setAttribute ("spec3dSsrThick", (double) mainComponent->getSpec3DSsrThickness());
        {
            const auto q = mainComponent->getSpec3DSsrQuality();
            xml->setAttribute ("spec3dSsrQuality",
                               q == Spectrogram3DComponent::ShadowQuality::low ? 0
                                   : (q == Spectrogram3DComponent::ShadowQuality::medium ? 1
                                   : (q == Spectrogram3DComponent::ShadowQuality::high ? 2 : 3)));
        }
        xml->setAttribute ("spec3dSsrFresnel", (double) mainComponent->getSpec3DSsrFresnel());
        xml->setAttribute ("spec3dSsrRoughInf", (double) mainComponent->getSpec3DSsrRoughnessInfluence());
        xml->setAttribute ("spec3dSsrIntensity", (double) mainComponent->getSpec3DSsrIntensity());
        xml->setAttribute ("spec3dSsrEdgeFade", (double) mainComponent->getSpec3DSsrEdgeFade());
        xml->setAttribute ("spec3dSsrMetalBias", (double) mainComponent->getSpec3DSsrMetallicBias());
        xml->setAttribute ("spec3dSsrDomeFb", (double) mainComponent->getSpec3DSsrDomeFallback());
        xml->setAttribute ("spec3dEnergyConserve", mainComponent->isSpec3DEnergyConservingEnabled());
        xml->setAttribute ("spec3dTonemap", mainComponent->isSpec3DTonemapEnabled());
        xml->setAttribute ("spec3dExposure", (double) mainComponent->getSpec3DTonemapExposureStops());
        xml->setAttribute ("spec3dGrade", (int) mainComponent->getSpec3DColorGrade());
        xml->setAttribute ("spec3dContactShadow", mainComponent->isSpec3DContactShadowEnabled());
        xml->setAttribute ("spec3dContactShadowStr", (double) mainComponent->getSpec3DContactShadowStrength());
        xml->setAttribute ("spec3dSelfShadow", mainComponent->isSpec3DSelfShadowEnabled());
        xml->setAttribute ("spec3dSelfShadowStr", (double) mainComponent->getSpec3DSelfShadowStrength());
        xml->setAttribute ("spec3dSelfShadowBias", (double) mainComponent->getSpec3DSelfShadowBias());
        xml->setAttribute ("spec3dSelfShadowSoft", (double) mainComponent->getSpec3DSelfShadowSoftness());
        {
            const auto q = mainComponent->getSpec3DSelfShadowQuality();
            xml->setAttribute ("spec3dSelfShadowQuality",
                               q == Spectrogram3DComponent::ShadowQuality::low ? 0
                                   : (q == Spectrogram3DComponent::ShadowQuality::high ? 2 : 1));
        }
        xml->setAttribute ("spec3dCastShadows", mainComponent->isSpec3DCastShadowsEnabled());
        xml->setAttribute ("spec3dShadowMapRes", (int) mainComponent->getSpec3DShadowMapResolution());
        xml->setAttribute ("spec3dShadowCascades", mainComponent->getSpec3DShadowCascadeCount());
        xml->setAttribute ("spec3dShadowCascadeDist",
                           (double) mainComponent->getSpec3DShadowCascadeDistributionExponent());
        xml->setAttribute ("spec3dShadowCascadeTrans",
                           (double) mainComponent->getSpec3DShadowCascadeTransitionFraction());
        xml->setAttribute ("spec3dDebugSphere", mainComponent->isSpec3DDebugSphereEnabled());
        xml->setAttribute ("spec3dDebugSphereDiam",
                           (double) mainComponent->getSpec3DDebugSphereDiameter());
        {
            const auto p = mainComponent->getSpec3DDebugSpherePosition();
            xml->setAttribute ("spec3dDebugSphereX", (double) p.x);
            xml->setAttribute ("spec3dDebugSphereY", (double) p.y);
            xml->setAttribute ("spec3dDebugSphereZ", (double) p.z);
        }
        xml->setAttribute ("spec3dDebugSphereAlbedo",
                           (int) mainComponent->getSpec3DDebugSphereAlbedo().getARGB());
        xml->setAttribute ("spec3dDebugSphereRough",
                           (double) mainComponent->getSpec3DDebugSphereRoughness());
        xml->setAttribute ("spec3dDebugSphereMetal",
                           (double) mainComponent->getSpec3DDebugSphereMetalness());
        xml->setAttribute ("spec3dDebugSphereSpec",
                           (double) mainComponent->getSpec3DDebugSphereSpecular());
        xml->setAttribute ("spec3dSsao", mainComponent->isSpec3DSsaoEnabled());
        xml->setAttribute ("spec3dSsaoStr", (double) mainComponent->getSpec3DSsaoStrength());
        xml->setAttribute ("spec3dSsaoRad", (double) mainComponent->getSpec3DSsaoRadius());
        xml->setAttribute ("spec3dBloom", mainComponent->isSpec3DBloomEnabled());
        xml->setAttribute ("spec3dBloomStr", (double) mainComponent->getSpec3DBloomStrength());
        xml->setAttribute ("spec3dBloomThr", (double) mainComponent->getSpec3DBloomThreshold());
        xml->setAttribute ("spec3dDof", mainComponent->isSpec3DDofEnabled());
        xml->setAttribute ("spec3dDofFocus", (double) mainComponent->getSpec3DDofFocusDistance());
        xml->setAttribute ("spec3dDofFStop", (double) mainComponent->getSpec3DDofFStop());
        xml->setAttribute ("spec3dDofFocalMm", (double) mainComponent->getSpec3DDofFocalLengthMm());
        // Legacy keys for older builds.
        xml->setAttribute ("spec3dDofAperture", (double) mainComponent->getSpec3DDofAperture());
        xml->setAttribute ("spec3dDofAmount", (double) mainComponent->getSpec3DDofAperture());
        {
            const auto q = mainComponent->getSpec3DDofQuality();
            xml->setAttribute ("spec3dDofQuality",
                               q == Spectrogram3DComponent::ShadowQuality::low ? 0
                                   : (q == Spectrogram3DComponent::ShadowQuality::high ? 2 : 1));
        }
        xml->setAttribute ("spec3dDofBlurScale", (double) mainComponent->getSpec3DDofBlurScale());
        xml->setAttribute ("spec3dDofCocDilate", (double) mainComponent->getSpec3DDofCocDilate());
        xml->setAttribute ("spec3dDofEdgeSpill", (double) mainComponent->getSpec3DDofEdgeSpill());
        xml->setAttribute ("spec3dClosedMesh", mainComponent->isSpec3DClosedMeshEnabled());
        xml->setAttribute ("spec3dAutoRotate", mainComponent->isSpec3DAutoRotateEnabled());
        xml->setAttribute ("spec3dAutoRotatePeriod",
                           (double) mainComponent->getSpec3DAutoRotatePeriodSec());
        xml->setAttribute ("spec3dZoomOsc", mainComponent->isSpec3DZoomOscillateEnabled());
        xml->setAttribute ("spec3dZoomOscDepth",
                           (double) mainComponent->getSpec3DZoomOscillateDepth());
        xml->setAttribute ("spec3dZoomOscPeriod",
                           (double) mainComponent->getSpec3DZoomOscillatePeriodSec());
        {
            auto seqTree = mainComponent->getSpec3DRampSequence().toValueTree();
            if (auto seqXml = seqTree.createXml())
                xml->setAttribute ("spec3dRampSequence", seqXml->toString());
        }
        xml->setAttribute ("spec3dAudioLevel", mainComponent->isSpec3DAudioLevelModEnabled());
        xml->setAttribute ("spec3dAudioTarget", (int) mainComponent->getSpec3DAudioLevelTarget());
        xml->setAttribute ("spec3dAudioMinPct",
                           (double) mainComponent->getSpec3DAudioLevelMinPercent());
        xml->setAttribute ("spec3dAudioMaxPct",
                           (double) mainComponent->getSpec3DAudioLevelMaxPercent());
        xml->setAttribute ("spec3dAudioHp", (double) mainComponent->getSpec3DAudioLevelHpHz());
        xml->setAttribute ("spec3dAudioLp", (double) mainComponent->getSpec3DAudioLevelLpHz());
        xml->setAttribute ("spec3dAudioThresh",
                           (double) mainComponent->getSpec3DAudioLevelThresholdDb());
        xml->setAttribute ("spec3dAudioSpeed", (int) mainComponent->getSpec3DAudioLevelSpeed());
        xml->setAttribute ("spec3dAudioPlayhead",
                           mainComponent->getSpec3DAudioLevelAffectPlayhead());
        xml->setAttribute ("spec3dAudioAntiPlayhead",
                           mainComponent->getSpec3DAudioLevelAffectAntiPlayhead());
        xml->setAttribute ("spec3dNormalCusp",
                           (double) mainComponent->getSpec3DNormalCuspAngleDeg());
        xml->setAttribute ("spec3dNormalWeight",
                           (int) mainComponent->getSpec3DNormalWeighting());
        xml->setAttribute ("spec3dSss", mainComponent->isSpec3DSssEnabled());
        xml->setAttribute ("spec3dSssStr", (double) mainComponent->getSpec3DSssStrength());
        xml->setAttribute ("spec3dSssWrap", (double) mainComponent->getSpec3DSssWrap());
        xml->setAttribute ("spec3dSssTrans", (double) mainComponent->getSpec3DSssTransmission());
        xml->setAttribute ("spec3dSssTint", (int) mainComponent->getSpec3DSssTint().getARGB());
        xml->setAttribute ("spec3dSssRad", (double) mainComponent->getSpec3DSssRadius());
        xml->setAttribute ("spec3dSssContrast", (double) mainComponent->getSpec3DSssContrast());
        {
            const auto q = mainComponent->getSpec3DSssQuality();
            xml->setAttribute ("spec3dSssQuality",
                               q == Spectrogram3DComponent::ShadowQuality::low ? 0
                                   : (q == Spectrogram3DComponent::ShadowQuality::high ? 2 : 1));
        }
        xml->setAttribute ("spec3dSssThickScale", (double) mainComponent->getSpec3DSssThicknessScale());
        xml->setAttribute ("spec3dSssMaxThick", (double) mainComponent->getSpec3DSssMaxThickness());
        xml->setAttribute ("spec3dParticleMode", mainComponent->isSpec3DParticleModeEnabled());
        xml->setAttribute ("spec3dParticleEmitMode", mainComponent->getSpec3DParticleEmitMode());
        xml->setAttribute ("spec3dParticleEmission", (double) mainComponent->getSpec3DParticleEmission());
        xml->setAttribute ("spec3dParticleEmissionV2", 1);
        xml->setAttribute ("spec3dParticleSpeed", (double) mainComponent->getSpec3DParticleRiseSpeed());
        xml->setAttribute ("spec3dParticleVelRandom", (double) mainComponent->getSpec3DParticleVelRandom());
        xml->setAttribute ("spec3dParticleLifespan", (double) mainComponent->getSpec3DParticleLifespan());
        xml->setAttribute ("spec3dParticleLifespanRandom", (double) mainComponent->getSpec3DParticleLifespanRandom());
        xml->setAttribute ("spec3dParticleSize", (double) mainComponent->getSpec3DParticleSize());
        xml->setAttribute ("spec3dParticleEmissive", mainComponent->isSpec3DParticleEmissiveEnabled());
        xml->setAttribute ("spec3dParticleEmissiveStr", (double) mainComponent->getSpec3DParticleEmissiveStrength());
        xml->setAttribute ("spec3dParticleRough", (double) mainComponent->getSpec3DParticleRoughness());
        xml->setAttribute ("spec3dParticleMetal", (double) mainComponent->getSpec3DParticleMetalness());
        xml->setAttribute ("spec3dParticleSpec", (double) mainComponent->getSpec3DParticleSpecular());
        for (int i = 0; i < kParticleModSlotCount; ++i)
        {
            const auto slot = mainComponent->getSpec3DParticleModSlot (i);
            xml->setAttribute ("spec3dPMod" + juce::String (i),
                               juce::String (slot.enabled ? 1 : 0) + ","
                               + juce::String ((int) slot.source) + ","
                               + juce::String ((int) slot.dest) + ","
                               + juce::String ((int) slot.op) + ","
                               + juce::String (slot.amount, 4) + ","
                               + juce::String (slot.constant, 4) + ","
                               + juce::String (slot.curveShape, 4) + ","
                               + juce::String (slot.thresholdEnabled ? 1 : 0) + ","
                               + juce::String (slot.threshold, 4) + ","
                               + juce::String (slot.attackMs, 2) + ","
                               + juce::String (slot.releaseMs, 2));
        }
        xml->setAttribute ("oscExpanded", mainComponent->isOscExpanded());
        xml->setAttribute ("gonExpanded", mainComponent->isGonExpanded());
        xml->setAttribute ("specExpanded", mainComponent->isSpecExpanded());
        const auto cam = mainComponent->getSpec3DDefaultCamera();
        const auto factory = Spectrogram3DComponent::getFactoryCameraState();
        const bool camCustom = std::abs (cam.yawDeg - factory.yawDeg) > 1.0e-3f
                            || std::abs (cam.pitchDeg - factory.pitchDeg) > 1.0e-3f
                            || std::abs (cam.distance - factory.distance) > 1.0e-3f
                            || std::abs (cam.panX - factory.panX) > 1.0e-3f
                            || std::abs (cam.panY - factory.panY) > 1.0e-3f
                            || std::abs (cam.panZ - factory.panZ) > 1.0e-3f;
        xml->setAttribute ("spec3dCamCustom", camCustom);
        if (camCustom)
        {
            xml->setAttribute ("spec3dCamYaw", (double) cam.yawDeg);
            xml->setAttribute ("spec3dCamPitch", (double) cam.pitchDeg);
            xml->setAttribute ("spec3dCamDist", (double) cam.distance);
            xml->setAttribute ("spec3dCamPanX", (double) cam.panX);
            xml->setAttribute ("spec3dCamPanY", (double) cam.panY);
            xml->setAttribute ("spec3dCamPanZ", (double) cam.panZ);
        }
    }

    // Live layout as last left (restored on host session reopen).
    xml->setAttribute ("scopeMode", mainComponent != nullptr && mainComponent->isScopeMode());
    xml->setAttribute ("uiCompact", uiCompact);
    xml->setAttribute ("modPanelOpen", modPanelOpen);

    auto file = getUiPrefsFile();
    file.getParentDirectory().createDirectory();
    if (file.getParentDirectory().isDirectory())
        xml->writeTo (file);

    // Full palette — same disk folder as dice flags (this is what actually survives Ableton).
    saveLastUiThemeToDisk();

    // Host project state: same prefs + theme + GlobalUi modules (best-effort for per-project).
    juce::ValueTree session ("UiSession");
    if (auto prefsTree = juce::ValueTree::fromXml (*xml); prefsTree.isValid())
        session.appendChild (std::move (prefsTree), nullptr);

    if (mainComponent != nullptr)
    {
        Theme theme (mainComponent->getSharedResources().sharedColors);
        if (auto themeXml = std::unique_ptr<juce::XmlElement> (theme.toXml()))
            if (auto themeTree = juce::ValueTree::fromXml (*themeXml); themeTree.isValid())
                session.appendChild (std::move (themeTree), nullptr);

        auto globalUi = mainComponent->captureGlobalUiModules();
        if (globalUi.isValid())
            session.appendChild (std::move (globalUi), nullptr);

        session.setProperty ("scopeMode", mainComponent->isScopeMode(), nullptr);
    }
    else
    {
        session.setProperty ("scopeMode", false, nullptr);
    }

    session.setProperty ("uiCompact", uiCompact, nullptr);
    session.setProperty ("modPanelOpen", modPanelOpen, nullptr);
    audioProcessor.storeSessionUiState (session);
}

void EqEditor::wireFaceplateKnobInteraction (juce::Slider& knob)
{
    // Component already is a MouseListener; avoid ambiguous EqEditor::MouseListener*.
    knob.addMouseListener (static_cast<juce::Component*> (this), false);
}

int EqEditor::faceplateColumnForSlider (const juce::Slider* slider) const noexcept
{
    if (slider == &knob1 || slider == &knobHpGain || slider == &knob2) return 0; // Band 1 / col0
    if (slider == &knob8 || slider == &knob9 || slider == &knob10) return 1;
    if (slider == &knob11 || slider == &knob12 || slider == &knob13) return 2;
    if (slider == &knob14 || slider == &knob15 || slider == &knob16) return 3;
    if (slider == &knob17 || slider == &knob18 || slider == &knob19) return 4;
    if (slider == &knob20 || slider == &knob21 || slider == &knob22) return 5;
    if (slider == &knob5 || slider == &knob6 || slider == &knob7) return 6;
    if (slider == &knob3 || slider == &knobLpGain || slider == &knob4) return 7;
    return -1;
}

int EqEditor::faceplateBandIndexForSlider (const juce::Slider* slider) const noexcept
{
    const int col = faceplateColumnForSlider (slider);
    if (col < 0)
        return -1;

    if (faceplateBank <= 0)
    {
        // Bank 1: OptionBox / highlight still use internal indices.
        constexpr int internalByCol[8] = { 4, 7, 0, 1, 2, 3, 6, 5 };
        return internalByCol[col];
    }

    return EqBand::globalFromBankSlot (faceplateBank, col);
}

void EqEditor::setFaceplateBank (int bankIndex, bool savePrefs)
{
    const int maxBank = juce::jmax (0, audioProcessor.getFaceplateBankCount() - 1);
    bankIndex = juce::jlimit (0, juce::jmax (maxBank, EqBand::kMaxBanks - 1), bankIndex);
    bankIndex = juce::jmin (bankIndex, EqBand::kMaxBanks - 1);

    if (bankIndex == faceplateBank)
    {
        updateFaceplateBankChrome();
        return;
    }

    faceplateBank = bankIndex;
    audioProcessor.ensureBankAvailable (faceplateBank);
    rebindFaceplateAttachments();
    updateFaceplateBandNumbers();
    updateFaceplateBankChrome();
    updateBandFaceplateGainVisibility();

    if (mainComponent != nullptr)
        mainComponent->getFrequencyResponseComponent().setPreferredCreateBank (faceplateBank);

    resized();
    if (savePrefs)
        saveUiPrefs();
}

void EqEditor::rebindFaceplateAttachments()
{
    auto bindColumn = [this] (int col,
                              juce::Slider& freqKnob,
                              juce::Slider& gainKnob,
                              juce::Slider& qKnob,
                              std::unique_ptr<SliderAttachment>& freqAtt,
                              std::unique_ptr<SliderAttachment>& gainAtt,
                              std::unique_ptr<SliderAttachment>& qAtt,
                              BandNumberButton& onOff)
    {
        const int global = EqBand::globalFromBankSlot (faceplateBank, col);
        const auto freqId = EqBand::frequencyParamIDForGlobal (global);
        const auto gainId = EqBand::gainParamIDForGlobal (global);
        const auto qId = EqBand::qParamIDForGlobal (global);
        const auto onId = EqBand::onOffParamIDForGlobal (global);

        auto attach = [this] (std::unique_ptr<SliderAttachment>& att,
                              const juce::String& id,
                              juce::Slider& slider)
        {
            att.reset();
            if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (audioProcessor.treeState.getParameter (id)))
            {
                const auto range = param->getNormalisableRange();
                slider.setNormalisableRange (juce::NormalisableRange<double> (
                    (double) range.start, (double) range.end,
                    (double) range.interval, (double) range.skew, range.symmetricSkew));
            }
            att = std::make_unique<SliderAttachment> (audioProcessor.treeState, id, slider);
        };

        attach (freqAtt, freqId, freqKnob);
        attach (gainAtt, gainId, gainKnob);
        attach (qAtt, qId, qKnob);
        onOff.setParameterID (onId);
    };

    // Display columns L→R: Band1 HP, Band2 LS, Band3–6, Band7 HS, Band8 LP
    bindColumn (0, knob1,  knobHpGain, knob2,  highpassCutoffAttachment, highpassGainAttachment, highpassQAttachment, *onOffButton1);
    bindColumn (1, knob8,  knob9,      knob10, lowShelfFrequencyAttachment, lowShelfGainAttachment, lowShelfQAttachment, *onOffButton4);
    bindColumn (2, knob11, knob12,     knob13, band1FrequencyAttachment, band1GainAttachment, band1QAttachment, *onOffButton5);
    bindColumn (3, knob14, knob15,     knob16, band2FrequencyAttachment, band2GainAttachment, band2QAttachment, *onOffButton6);
    bindColumn (4, knob17, knob18,     knob19, band3FrequencyAttachment, band3GainAttachment, band3QAttachment, *onOffButton7);
    bindColumn (5, knob20, knob21,     knob22, band4FrequencyAttachment, band4GainAttachment, band4QAttachment, *onOffButton8);
    bindColumn (6, knob5,  knob6,      knob7,  highShelfFrequencyAttachment, highShelfGainAttachment, highShelfQAttachment, *onOffButton3);
    bindColumn (7, knob3,  knobLpGain, knob4,  lowpassCutoffAttachment, lowpassGainAttachment, lowpassQAttachment, *onOffButton2);

    updateFaceplateBandNumbers();
}

void EqEditor::updateFaceplateBandNumbers()
{
    // Display columns L→R match bindColumn / placeOnOffCentered mapping.
    BandNumberButton* buttons[8] = {
        onOffButton1.get(), onOffButton4.get(), onOffButton5.get(), onOffButton6.get(),
        onOffButton7.get(), onOffButton8.get(), onOffButton3.get(), onOffButton2.get()
    };

    for (int col = 0; col < 8; ++col)
    {
        if (buttons[col] == nullptr)
            continue;
        const int global = EqBand::globalFromBankSlot (faceplateBank, col);
        const int bandNum = global + 1;
        buttons[col]->setBandNumber (bandNum);
        buttons[col]->setTooltip ("Band " + juce::String (bandNum)
                                  + " - click to turn this band on or off");
    }
}

void EqEditor::updateFaceplateBankChrome()
{
    const int count = audioProcessor.getFaceplateBankCount();
    faceplateBankPrevButton.setEnabled (faceplateBank > 0);
    faceplateBankNextButton.setEnabled (faceplateBank + 1 < EqBand::kMaxBanks
                                        || faceplateBank + 1 < count);

    const int bankShown = faceplateBank + 1;
    const int first = faceplateBank * EqBand::kBankSize + 1;
    const int last = first + EqBand::kBankSize - 1;
    faceplateBankPrevButton.setTooltip (
        "Previous band bank (currently bank " + juce::String (bankShown)
        + ", bands " + juce::String (first) + "-" + juce::String (last) + ")");
    faceplateBankNextButton.setTooltip (
        "Next band bank (currently bank " + juce::String (bankShown)
        + ", bands " + juce::String (first) + "-" + juce::String (last) + ")");
}

void EqEditor::showBandsFullSoftMaxFeedback()
{
    bandsFullToastLabel.setVisible (true);
    bandsFullToastTicks = 40; // ~2s at 50ms timer
    if (! isTimerRunning())
        startTimer (50);
    resized();
}

void EqEditor::openOptionBoxForFaceplateBand (int bandIndex)
{
    if (bandIndex < 0 || mainComponent == nullptr)
        return;
    mainComponent->getFrequencyResponseComponent().showOptionBoxForBand (bandIndex);
}

bool EqEditor::cycleFilterSlopeForBand (int bandIndex, int delta)
{
    juce::String slopeID, typeID;
    int fallbackType = FilterType::bell;

    if (bandIndex >= EqBand::kBankSize)
    {
        slopeID = EqBand::slopeParamIDForGlobal (bandIndex);
        typeID = FilterType::paramIDForGlobal (bandIndex);
        fallbackType = FilterType::bell;
    }
    else
    {
        slopeID = FilterSlope::paramIDForBandIndex (bandIndex);
        typeID = FilterType::paramIDForBandIndex (bandIndex);
        fallbackType = FilterType::defaultTypeForBandIndex (bandIndex);
    }

    if (slopeID.isEmpty())
        return false;

    const int type = BandChannel::readChoiceIndex (audioProcessor.treeState, typeID, fallbackType);
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
    auto typeIsHpLpCol = [this] (int col, int fallback) -> bool
    {
        const int g = EqBand::globalFromBankSlot (faceplateBank, col);
        return FilterType::isHpLp (
            BandChannel::readChoiceIndex (audioProcessor.treeState,
                                          FilterType::paramIDForGlobal (g), fallback));
    };

    auto setWheel = [] (juce::Slider& s, bool enable) { s.setScrollWheelEnabled (enable); };

    // When HP/LP, disable knob-value wheel so mouseWheelMove can cycle slope instead.
    const bool b1 = typeIsHpLpCol (0, FilterType::highpass);
    const bool b2 = typeIsHpLpCol (1, FilterType::lowShelf);
    const bool p1 = typeIsHpLpCol (2, FilterType::bell);
    const bool p2 = typeIsHpLpCol (3, FilterType::bell);
    const bool p3 = typeIsHpLpCol (4, FilterType::bell);
    const bool p4 = typeIsHpLpCol (5, FilterType::bell);
    const bool hs = typeIsHpLpCol (6, FilterType::highShelf);
    const bool b8 = typeIsHpLpCol (7, FilterType::lowpass);

    setWheel (knob1, ! b1); setWheel (knobHpGain, ! b1); setWheel (knob2, ! b1);
    setWheel (knob8, ! b2); setWheel (knob9, ! b2); setWheel (knob10, ! b2);
    setWheel (knob11, ! p1); setWheel (knob12, ! p1); setWheel (knob13, ! p1);
    setWheel (knob14, ! p2); setWheel (knob15, ! p2); setWheel (knob16, ! p2);
    setWheel (knob17, ! p3); setWheel (knob18, ! p3); setWheel (knob19, ! p3);
    setWheel (knob20, ! p4); setWheel (knob21, ! p4); setWheel (knob22, ! p4);
    setWheel (knob5, ! hs); setWheel (knob6, ! hs); setWheel (knob7, ! hs);
    setWheel (knob3, ! b8); setWheel (knobLpGain, ! b8); setWheel (knob4, ! b8);
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

    if (bandIndex >= EqBand::kBankSize)
    {
        setFaceplateBank (EqBand::bankFromGlobal (bandIndex), false);
        const int col = EqBand::slotInBankFromGlobal (bandIndex);
        // Map display column → highlight using Bank1 internal indices for the switch below.
        constexpr int internalByCol[8] = { 4, 7, 0, 1, 2, 3, 6, 5 };
        bandIndex = internalByCol[juce::jlimit (0, 7, col)];
    }

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
    // Faceplate knobs are APVTS-attached. Do NOT call updateBand*/updateHighpass here —
    // those snap IIR coeffs on the message thread and fight processBlock smoothing (zipper).
    if (slider == &sideCheckHpKnob || slider == &sideCheckLpKnob)
        enforceSideCheckHpLpOrder (slider);
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
    // On/off buttons update their own APVTS parameter in BandNumberButton::clicked().
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
    if (! hasForcedRepaint)
    {
        hasForcedRepaint = true;
        repaint();
    }

    if (bandsFullToastTicks > 0)
    {
        --bandsFullToastTicks;
        if (bandsFullToastTicks == 0)
            bandsFullToastLabel.setVisible (false);
    }

    if (uiPrefsSavePending
        && juce::Time::getMillisecondCounter() >= uiPrefsSaveDueMs)
    {
        saveUiPrefs();
    }

    if (hasForcedRepaint && bandsFullToastTicks <= 0 && ! uiPrefsSavePending)
        stopTimer();
}

void EqEditor::toggleCompactUi()
{
    uiCompact = ! uiCompact;
    applyCompactUi();
    requestSaveUiPrefs();
}

const SharedColors& EqEditor::themePalette() const noexcept
{
    if (themeColors != nullptr)
        return themeColors->sharedColors;
    if (auto* active = SharedResources::getActive())
        return active->sharedColors;
    static const SharedColors defaults;
    return defaults;
}

void EqEditor::applyFaceplateTheme()
{
    const auto& c = themePalette();
    brandWordmark.setBrandColour (c.pluginBrandText);

    auto styleChrome = [&c] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, c.pluginButtonBackground);
        b.setColour (juce::TextButton::buttonOnColourId, c.pluginButtonAccent);
        b.setColour (juce::TextButton::textColourOffId, c.pluginButtonText.withAlpha (0.85f));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.repaint();
    };

    auto attachGraphShadow = [this] (juce::TextButton& b)
    {
        b.setLookAndFeel (&graphOverlayButtonLookAndFeel);
        // Melatonin drop extends past local bounds; without this the blur is clipped away.
        b.setPaintingIsUnclipped (true);
    };
    styleChrome (helpTooltipsButton);
    styleChrome (autoGainButton);
    styleChrome (sideCheckButton);
    styleChrome (scopeModeButton);
    styleChrome (sideCheckSpeedButton);
    styleChrome (sideCheckHqButton);
    attachGraphShadow (helpTooltipsButton);
    attachGraphShadow (sideCheckButton);
    attachGraphShadow (scopeModeButton);
    attachGraphShadow (sideCheckSpeedButton);
    attachGraphShadow (sideCheckHqButton);

    faceplateBankPrevButton.setChromeColours (c.pluginButtonBackground,
                                              c.pluginButtonText.withAlpha (0.9f));
    faceplateBankNextButton.setChromeColours (c.pluginButtonBackground,
                                              c.pluginButtonText.withAlpha (0.9f));

    const auto labelColour = c.pluginButtonText.withAlpha (0.85f);
    auto themeLabel = [labelColour] (juce::Label& lab)
    {
        lab.setColour (juce::Label::textColourId, labelColour);
        lab.repaint();
    };

    themeLabel (sideCheckAmountLabel);
    themeLabel (sideCheckHpLabel);
    themeLabel (sideCheckLpLabel);

    // Band number buttons: off = dimmed knob arc; on = brighter ring + number.
    const auto powerColour = c.knobArc.withMultipliedBrightness (0.45f).withMultipliedSaturation (0.85f);
    auto themePower = [powerColour] (std::unique_ptr<BandNumberButton>& b)
    {
        if (b != nullptr)
        {
            b->setBaseColor (powerColour);
            b->repaint();
        }
    };
    themePower (onOffButton1);
    themePower (onOffButton2);
    themePower (onOffButton3);
    themePower (onOffButton4);
    themePower (onOffButton5);
    themePower (onOffButton6);
    themePower (onOffButton7);
    themePower (onOffButton8);
}

void EqEditor::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    auto applyKnob = [r] (auto& knob)
    {
        knob.setThemeColors (r);
        knob.repaint();
    };

    applyKnob (knob1);
    applyKnob (knobHpGain);
    applyKnob (knob2);
    applyKnob (knob3);
    applyKnob (knobLpGain);
    applyKnob (knob4);
    applyKnob (knob5);
    applyKnob (knob6);
    applyKnob (knob7);
    applyKnob (knob8);
    applyKnob (knob9);
    applyKnob (knob10);
    applyKnob (knob11);
    applyKnob (knob12);
    applyKnob (knob13);
    applyKnob (knob14);
    applyKnob (knob15);
    applyKnob (knob16);
    applyKnob (knob17);
    applyKnob (knob18);
    applyKnob (knob19);
    applyKnob (knob20);
    applyKnob (knob21);
    applyKnob (knob22);
    applyKnob (outputGainKnob);
    applyKnob (sideCheckAmountKnob);
    applyKnob (sideCheckHpKnob);
    applyKnob (sideCheckLpKnob);

    if (modSection != nullptr)
        modSection->setThemeColors (r);

    applyFaceplateTheme();
    repaint();
}

void EqEditor::toggleModPanel()
{
    modPanelOpen = ! modPanelOpen;
    applyCompactUi();
    if (mainComponent != nullptr)
        mainComponent->getFrequencyResponseComponent().syncModButton (modPanelOpen);
    requestSaveUiPrefs();
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
    const int stripH = isFaceplateSuppressed() ? 0 : getFaceplateHeightForWidth (width);
    const int modH = includeModPanel ? getModPanelHeightForGraphHeight (graphH) : 0;
    const int trimH = juce::jmax (22, juce::roundToInt (30.0f * scale));
    // Piano grows the graph host (MainComponent), not the faceplate — plot height stays via getPlotHeight().
    return graphTop + graphH + getPianoWindowExtra() + modH + stripH + trimH;
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

    setVis (faceplateBankPrevButton);
    setVis (faceplateBankNextButton);

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

    // Help (?) stays; Phase / Side Check hide in Scope Pre or strip.
    helpTooltipsButton.setVisible (true);
    const bool hideDsp = mainComponent != nullptr && mainComponent->shouldHideScopeDspChrome();
    phaseModeCombo.setVisible (! hideDsp);
    sideCheckButton.setVisible (! hideDsp);
    updateSideCheckAmountVisibility();
}

void EqEditor::applyCompactUi()
{
    setFaceplateVisible (! isFaceplateSuppressed());
    if (! isFaceplateSuppressed())
        updateBandFaceplateGainVisibility(); // restore per-type gain knobs after unhide

    if (mainComponent != nullptr)
        mainComponent->getFrequencyResponseComponent().syncUiModeButton (uiCompact);

    const int pianoExtra = getPianoWindowExtra();

    if (uiCompact)
    {
        // Store height without piano so restore + getExpandedEditorHeight don't double-count.
        savedEditorWidth = getWidth();
        savedEditorHeight = juce::jmax (1, getHeight() - pianoExtra);

        const int w = juce::jmax (600, savedEditorWidth);
        const int graphH = getGraphHeightForWidth (w);
        const int modH = modPanelOpen ? getModPanelHeightForGraphHeight (graphH) : 0;
        const int h = graphH + modH + pianoExtra;

        setConstrainer (nullptr);
        setSize (w, h);
    }
    else
    {
        const int w = savedEditorWidth > 0 ? savedEditorWidth : designWidth;
        // savedEditorHeight is piano-free; absolute helpers already add piano when open.
        const int h = savedEditorHeight > 0 ? (savedEditorHeight + pianoExtra)
                                            : getExpandedEditorHeight (w, modPanelOpen);

        // Free aspect — any ratio within size limits.
        resizeConstrainer.setFixedAspectRatio (0.0);
        resizeConstrainer.setSizeLimits (900, 500, 2400, 1600);
        setConstrainer (&resizeConstrainer);
        setSize (w, juce::jmax (h, getExpandedEditorHeight (w, modPanelOpen)));
    }

    // Absolute setSize paths must keep the delta-grow flag in sync with reality.
    pianoStripWindowApplied = (pianoExtra > 0);

    if (modSection != nullptr)
        modSection->setVisible (modPanelOpen);

    resized();
    repaint();
}
