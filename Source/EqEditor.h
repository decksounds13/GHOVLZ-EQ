#pragma once

#include <JuceHeader.h> // Include the JUCE header or necessary headers for your project
#include "EqProcessor.h"
#include "FrequencyResponseComponent.h"
#include "RotaryImageKnob1.h"
#include "RotaryImageKnob2.h"
#include "RotaryImageKnob3.h"
#include "RotaryImageKnobForOptionBox.h"
#include "RotaryImageKnobLookAndFeel1.h"
#include "RotaryImageKnobLookAndFeel2.h"
#include "RotaryImageKnobLookAndFeel3.h"
#include "RotaryImageKnobLookAndFeel4.h"
#include "RotaryImageKnobLookAndFeel5.h"
#include "OnOffButton1.h"
#include "MainComponent.h"
#include "BinaryData.h"
#include "BrandWordmark.h"
#include "PhaseMode.h"
#include "ModSectionComponent.h"
#include "ComboBoxLookAndFeel.h"

// Forward declaration
class MainComponent;

class FrequencyResponseComponent;

//class RotaryImageKnobLookAndFeel1;

//class RotaryImageKnob1; 


class EqEditor : public juce::AudioProcessorEditor,
    public juce::Slider::Listener,
    public juce::ComponentListener,
    public juce::AudioProcessorValueTreeState::Listener,
    public juce::Button::Listener,
    public juce::Timer

{
public:
    EqEditor(EqProcessor&, juce::AudioProcessorValueTreeState& treeState, Analyser& analyser);
    ~EqEditor() override;

    void parameterChanged(const juce::String& parameterID, float newValue) override; // Note the 'juce::' before String
    void sliderValueChanged(juce::Slider* slider) override;
    void sliderDragStarted (juce::Slider* slider) override;
    void sliderDragEnded (juce::Slider* slider) override;

    void buttonClicked(juce::Button* button) override;

    /** Highlight faceplate knobs for bandIndex (0-7). Pass -1 to clear. */
    void setBandManipulationHighlight (int bandIndex);

    /** Toggle graph-only compact UI (hides faceplate knobs). */
    void toggleCompactUi();
    bool isUiCompact() const noexcept { return uiCompact; }

    /** Toggle LFO / mod matrix panel under the spectrum graph. */
    void toggleModPanel();
    bool isModPanelOpen() const noexcept { return modPanelOpen; }
    void syncModButton (bool isOpen);

    void loadUiPrefs();
    void saveUiPrefs() const;

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> handle1DragStart;
    std::function<void()> handle1DragEnd;

  //  static const char* DarkKnob4_Stitched_png;
  //  static const char* DarkKnob4_Stitched_pngSize;

   // static juce::Image darkKnob4_StitchedImage;
        
    void timerCallback() override;

    void initializeSharedImages();

    //RotaryImageKnob1& getKnob1();


private:
    void applyCompactUi();
    void setFaceplateVisible (bool shouldShow);
    void layoutBrandWordmark (int outClusterLeftX);
    void layoutHelpTooltipsButton();
    void layoutPhaseModeCombo();
    void layoutSideCheckButton();
    void updateSideCheckAmountVisibility();
    void updateSideCheckSpeedButtonText();
    void toggleSideCheckSpeed();
    /** Keep HP strictly below LP (min gap) when either SC range knob moves. */
    void enforceSideCheckHpLpOrder (juce::Slider* changed);
    void applyTooltipsEnabled();
    static juce::File getUiPrefsFile();
    int getTopBandHeight() const;
    int getGraphHeightForWidth (int width) const;
    /** Mod panel / faceplate strip height (matched so expanded UI isn't overly tall). */
    int getModPanelHeightForGraphHeight (int graphHeight) const;
    /** Faceplate knob section height — same as mod strip when expanded. */
    int getFaceplateHeightForWidth (int width) const;
    int getBottomTrimHeight() const;
    /** Total editor height for expanded (non-compact) layout. */
    int getExpandedEditorHeight (int width, bool includeModPanel) const;

    float leftMargin;

    EqProcessor& audioProcessor;

    //Highpass
    RotaryImageKnob1 knob1;
    RotaryImageKnob2 knob2;
   
    //Lowpass
    RotaryImageKnob1 knob3;
    RotaryImageKnob2 knob4;
   
    //High Shelf — same knob classes as Band 1
    RotaryImageKnob1 knob5; // Freq
    RotaryImageKnob3 knob6; // Gain
    RotaryImageKnob2 knob7; // Q

    //Low Shelf
    RotaryImageKnob1 knob8; // Freq
    RotaryImageKnob3 knob9; // Gain
    RotaryImageKnob2 knob10; // Q
    
    //Band 1
    RotaryImageKnob1 knob11;
    RotaryImageKnob3 knob12;
    RotaryImageKnob2 knob13;

    //Band2
    RotaryImageKnob1 knob14;
    RotaryImageKnob3 knob15;
    RotaryImageKnob2 knob16;

    //Band3
    RotaryImageKnob1 knob17;
    RotaryImageKnob3 knob18;
    RotaryImageKnob2 knob19;

    //Band4
    RotaryImageKnob1 knob20;
    RotaryImageKnob3 knob21;
    RotaryImageKnob2 knob22;

    // Output gain (faceplate, expanded mode)
    RotaryImageKnob3 outputGainKnob;
    juce::TextButton autoGainButton { "A" };
    juce::TextButton sideCheckButton { "SideCheck" };
    /** Amount 0-1; visible only while Side Check is enabled. */
    juce::Slider sideCheckAmountSlider;
    /** Fast / Med / Slow ballistics toggle; shows current state; visible with Amount while SC on. */
    juce::TextButton sideCheckSpeedButton { "Fast" };
    /** HQ on = BP lattice (default); off = 3-band shelf/bell eco. Visible with Amount while SC on. */
    juce::TextButton sideCheckHqButton { "HQ" };
    /** Compact HP/LP range knobs — effect band for Side Check (visible with Amount). */
    RotaryImageKnobForOptionBox sideCheckHpKnob;
    RotaryImageKnobForOptionBox sideCheckLpKnob;
    juce::Label sideCheckHpLabel;
    juce::Label sideCheckLpLabel;

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
  
    std::unique_ptr<SliderAttachment> highpassCutoffAttachment;
    std::unique_ptr<SliderAttachment> highpassQAttachment;
  
    std::unique_ptr<SliderAttachment> lowpassCutoffAttachment;
    std::unique_ptr<SliderAttachment> lowpassQAttachment;
  
    std::unique_ptr<SliderAttachment> highShelfGainAttachment;
    std::unique_ptr<SliderAttachment> highShelfFrequencyAttachment;
    std::unique_ptr<SliderAttachment> highShelfQAttachment;
  
    std::unique_ptr<SliderAttachment> lowShelfGainAttachment;
    std::unique_ptr<SliderAttachment> lowShelfFrequencyAttachment;
    std::unique_ptr<SliderAttachment> lowShelfQAttachment;
  
    std::unique_ptr<SliderAttachment> band1FrequencyAttachment;
    std::unique_ptr<SliderAttachment> band1GainAttachment;
    std::unique_ptr<SliderAttachment> band1QAttachment;
   
    std::unique_ptr<SliderAttachment> band2FrequencyAttachment;
    std::unique_ptr<SliderAttachment> band2GainAttachment;
    std::unique_ptr<SliderAttachment> band2QAttachment;
  
    std::unique_ptr<SliderAttachment> band3FrequencyAttachment;
    std::unique_ptr<SliderAttachment> band3GainAttachment;
    std::unique_ptr<SliderAttachment> band3QAttachment;
   
    std::unique_ptr<SliderAttachment> band4FrequencyAttachment;
    std::unique_ptr<SliderAttachment> band4GainAttachment;
    std::unique_ptr<SliderAttachment> band4QAttachment;
    std::unique_ptr<SliderAttachment> outputGainAttachment;
    std::unique_ptr<ButtonAttachment> autoGainAttachment;
    std::unique_ptr<ButtonAttachment> sideCheckAttachment;
    std::unique_ptr<SliderAttachment> sideCheckAmountAttachment;
    std::unique_ptr<ButtonAttachment> sideCheckHqAttachment;
    std::unique_ptr<SliderAttachment> sideCheckHpAttachment;
    std::unique_ptr<SliderAttachment> sideCheckLpAttachment;


    //std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> testButtonAttachment;

   // std::unique_ptr<CustomLookAndFeel> customLookAndFeel;

    juce::GroupComponent border1;
    juce::GroupComponent border2;
    juce::GroupComponent border3;
    juce::GroupComponent border4;
    juce::GroupComponent border5;
    juce::GroupComponent border6;
    juce::GroupComponent border7;
    juce::GroupComponent border8;

    juce::GroupComponent border9;
    juce::GroupComponent border10;
    juce::GroupComponent border11;
    juce::GroupComponent border12;
    juce::GroupComponent border13;
    juce::GroupComponent border14;
    juce::GroupComponent border15;
    juce::GroupComponent border16;

    juce::GroupComponent border17;
    juce::GroupComponent border18;
    juce::GroupComponent border19;
    juce::GroupComponent border20;
    juce::GroupComponent border21;
    juce::GroupComponent border22;
    juce::GroupComponent borderOutputGain;

    juce::Label bandNumberLabel1;
    juce::Label bandNumberLabel2;
    juce::Label bandNumberLabel3;
    juce::Label bandNumberLabel4;
    juce::Label bandNumberLabel5;
    juce::Label bandNumberLabel6;
    juce::Label bandNumberLabel7;
    juce::Label bandNumberLabel8;

   // RotaryImageKnobLookAndFeel1 rotaryImageKnobLookAndFeel1;
   
    //Highpass  
    std::unique_ptr<RotaryImageKnobLookAndFeel1> lookAndFeel1 = std::make_unique<RotaryImageKnobLookAndFeel1>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel2 = std::make_unique<RotaryImageKnobLookAndFeel2>();
    
    //Lowpass
    std::unique_ptr<RotaryImageKnobLookAndFeel1> lookAndFeel3 = std::make_unique<RotaryImageKnobLookAndFeel1>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel4 = std::make_unique<RotaryImageKnobLookAndFeel2>();
  
    //High Shelf
    std::unique_ptr<RotaryImageKnobLookAndFeel5> lookAndFeel5 = std::make_unique<RotaryImageKnobLookAndFeel5>();
    std::unique_ptr<RotaryImageKnobLookAndFeel3> lookAndFeel6 = std::make_unique<RotaryImageKnobLookAndFeel3>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel7 = std::make_unique<RotaryImageKnobLookAndFeel2>();

    //Low Shelf
    std::unique_ptr<RotaryImageKnobLookAndFeel4> lookAndFeel8 = std::make_unique<RotaryImageKnobLookAndFeel4>();
    std::unique_ptr<RotaryImageKnobLookAndFeel3> lookAndFeel9 = std::make_unique<RotaryImageKnobLookAndFeel3>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel10 = std::make_unique<RotaryImageKnobLookAndFeel2>();

    std::unique_ptr<RotaryImageKnobLookAndFeel4> lookAndFeel11 = std::make_unique<RotaryImageKnobLookAndFeel4>();
    std::unique_ptr<RotaryImageKnobLookAndFeel3> lookAndFeel12 = std::make_unique<RotaryImageKnobLookAndFeel3>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel13 = std::make_unique<RotaryImageKnobLookAndFeel2>();

    std::unique_ptr<RotaryImageKnobLookAndFeel4> lookAndFeel14 = std::make_unique<RotaryImageKnobLookAndFeel4>();
    std::unique_ptr<RotaryImageKnobLookAndFeel3> lookAndFeel15 = std::make_unique<RotaryImageKnobLookAndFeel3>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel16 = std::make_unique<RotaryImageKnobLookAndFeel2>();

    std::unique_ptr<RotaryImageKnobLookAndFeel4> lookAndFeel17 = std::make_unique<RotaryImageKnobLookAndFeel4>();
    std::unique_ptr<RotaryImageKnobLookAndFeel3> lookAndFeel18 = std::make_unique<RotaryImageKnobLookAndFeel3>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel19 = std::make_unique<RotaryImageKnobLookAndFeel2>();

    std::unique_ptr<RotaryImageKnobLookAndFeel4> lookAndFeel20 = std::make_unique<RotaryImageKnobLookAndFeel4>();
    std::unique_ptr<RotaryImageKnobLookAndFeel3> lookAndFeel21 = std::make_unique<RotaryImageKnobLookAndFeel3>();
    std::unique_ptr<RotaryImageKnobLookAndFeel2> lookAndFeel22 = std::make_unique<RotaryImageKnobLookAndFeel2>();
   
   // juce::OpenGLContext openGLContext;

    std::unique_ptr<FrequencyResponseComponent> frequencyResponseComponent;

    //MainComponent mainComponent;

    std::unique_ptr<MainComponent> mainComponent;

    BrandWordmark brandWordmark;
    juce::TooltipWindow tooltipWindow { this, 500 };

    /** Bottom chrome: toggle plugin tooltips (lit = enabled). Visible in compact and expanded. */
    juce::TextButton helpTooltipsButton { "?" };
    bool tooltipsEnabled = true;

    /** Bottom chrome: Minimum Phase / Linear Phase processing mode. */
    ParamChoiceButton phaseModeCombo;

    bool isButtonUpdate = false;

    bool hasForcedRepaint = false;

    std::unique_ptr<OnOffButton1> onOffButton1;
    std::unique_ptr<OnOffButton1> onOffButton2;
    std::unique_ptr<OnOffButton1> onOffButton3;
    std::unique_ptr<OnOffButton1> onOffButton4;
    std::unique_ptr<OnOffButton1> onOffButton5;
    std::unique_ptr<OnOffButton1> onOffButton6;
    std::unique_ptr<OnOffButton1> onOffButton7;
    std::unique_ptr<OnOffButton1> onOffButton8;

    juce::TextButton testButton;

    juce::Colour backgroundColour{ juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f) };

    static constexpr double m_marginInPixels{ 10 };
    static constexpr int designWidth = 1200;
    static constexpr int designHeight = 850;

    juce::ComponentBoundsConstrainer resizeConstrainer;

    bool uiCompact = true;
    int savedEditorWidth = designWidth;
    int savedEditorHeight = designHeight;

    bool modPanelOpen = false;
    std::unique_ptr<ModSectionComponent> modSection;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqEditor)

 
};
