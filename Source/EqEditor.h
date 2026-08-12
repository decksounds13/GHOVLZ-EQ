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
#include "BandNumberButton.h"
#include "MainComponent.h"
#include "BinaryData.h"
#include "BrandWordmark.h"
#include "PhaseMode.h"
#include "ModSectionComponent.h"
#include "ComboBoxLookAndFeel.h"
#include "Menu/SharedResources.h"
#include "GraphOverlayButtonLookAndFeel.h"

// Forward declaration
class MainComponent;

class FrequencyResponseComponent;

//class RotaryImageKnobLookAndFeel1;

//class RotaryImageKnob1; 


class EqEditor : public juce::AudioProcessorEditor,
    public juce::Slider::Listener,
    public juce::MouseListener,
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
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

    void buttonClicked(juce::Button* button) override;

    /** Highlight faceplate knobs for bandIndex (Bank1 internal 0–7, or global 8–63). Pass -1 to clear. */
    void setBandManipulationHighlight (int bandIndex);
    /** Faceplate bank pager (0-based). Persisted in UI prefs. */
    void setFaceplateBank (int bankIndex, bool savePrefs = true);
    int getFaceplateBank() const noexcept { return faceplateBank; }

    /** Toggle graph-only compact UI (hides faceplate knobs). */
    void toggleCompactUi();
    bool isUiCompact() const noexcept { return uiCompact; }

    /** Toggle LFO / mod matrix panel under the spectrum graph. */
    void toggleModPanel();
    bool isModPanelOpen() const noexcept { return modPanelOpen; }
    void syncModButton (bool isOpen);

    void setThemeColors (SharedResources* r) noexcept;
    /** Power rings + knob glow arcs (Knob Arc or handle multicolours + optional min sat). */
    void applyFaceplateBandChrome();

    /**
        Z-order: OptionBox above Phase / SideCheck / Scope chrome; Settings menu
        stays above the box when open. Called from MainComponent and resized().
    */
    void raiseOptionBoxStack();

    void loadUiPrefs();
    /** Immediate prefs write (shutdown / explicit). Prefer requestSaveUiPrefs while scrubbing UI. */
    void saveUiPrefs() const;
    /** Coalesce rapid Look/settings changes into one disk write (avoids hitching sliders). */
    void requestSaveUiPrefs() noexcept;
    /** Last-used UI colours (Documents/…/last_ui_theme.xml — same reliability as dice prefs). */
    void saveLastUiThemeToDisk() const;
    /** @param into optional target (needed while MainComponent is still constructing). */
    bool loadLastUiThemeFromDisk (class SharedResources* into = nullptr);
    void syncScopeModeButton();
    void showScopeTapMenu();
    /** Re-layout brand + bottom chrome after Scope mode toggles (hide logo, shift ? cluster). */
    void syncScopeModeLayout();
    /** True while Scope mode is active (faceplate / OptionBox suppressed). */
    bool isScopeModeActive() const noexcept;
    /** Editor height for Scope strip mode: chrome + strip (default strip 200 design px). */
    int getScopeStripWindowHeight (int width) const;
    /** Grow/shrink editor height by the piano strip (called from FrequencyResponseComponent). */
    void applyPianoStripWindowHeight (bool pianoOn);

    void paint(juce::Graphics&) override;
    void resized() override;

    std::function<void()> handle1DragStart;
    std::function<void()> handle1DragEnd;

    void timerCallback() override;

    void initializeSharedImages();

private:
    const SharedColors& themePalette() const noexcept;
    void applyFaceplateTheme();

    void applyCompactUi();
    void setFaceplateVisible (bool shouldShow);
    /** Faceplate knobs/strip hidden in compact UI or Scope mode. */
    bool isFaceplateSuppressed() const noexcept;
    /** Show faceplate Gain knobs only when that band's filter type uses gain. */
    void updateBandFaceplateGainVisibility();
    /** Band index for OptionBox: Bank1 internal 0–7, or global 8–63 when paged. -1 if none. */
    int faceplateBandIndexForSlider (const juce::Slider* slider) const noexcept;
    /** Display column 0–7 for a faceplate knob, or -1. */
    int faceplateColumnForSlider (const juce::Slider* slider) const noexcept;
    void wireFaceplateKnobInteraction (juce::Slider& knob);
    void openOptionBoxForFaceplateBand (int bandIndex);
    void rebindFaceplateAttachments();
    void updateFaceplateBandNumbers();
    void updateFaceplateBankChrome();
    void showBandsFullSoftMaxFeedback();
    bool cycleFilterSlopeForBand (int bandIndex, int delta);
    void updateFaceplateSlopeWheelMode();
    void layoutBrandWordmark (int outClusterLeftX);
    void layoutHelpTooltipsButton();
    void layoutPhaseModeCombo();
    void layoutSideCheckButton();
    void layoutScopeModeButton();
    void showSideCheckHpLpSlopeMenu (bool forHp);
    void updateSideCheckAmountVisibility();
    void updateSideCheckSpeedButtonText();
    void toggleSideCheckSpeed();
    /** Keep HP strictly below LP (min gap) when either SC range knob moves. */
    void enforceSideCheckHpLpOrder (juce::Slider* changed);
    void applyTooltipsEnabled();
    static juce::File getUiPrefsFile();
    static juce::File getLastUiThemeFile();
    int getTopBandHeight() const;
    int getGraphHeightForWidth (int width) const;
    /** Mod panel / faceplate strip height (matched so expanded UI isn't overly tall). */
    int getModPanelHeightForGraphHeight (int graphHeight) const;
    /** Faceplate knob section height — same as mod strip when expanded. */
    int getFaceplateHeightForWidth (int width) const;
    int getBottomTrimHeight() const;
    /** Extra editor height for the piano strip (0 when hidden). Kept out of the plot via getPlotHeight(). */
    int getPianoWindowExtra() const noexcept;
    /** Total editor height for expanded (non-compact) layout, including piano when open. */
    int getExpandedEditorHeight (int width, bool includeModPanel) const;

    float leftMargin;

    EqProcessor& audioProcessor;

    // Band 1 (default HP) — Freq / Gain / Q (Gain hidden while type is HP/LP)
    RotaryImageKnob1 knob1;      // Freq
    RotaryImageKnob3 knobHpGain; // Gain
    RotaryImageKnob2 knob2;      // Q
   
    // Band 8 (default LP)
    RotaryImageKnob1 knob3;      // Freq
    RotaryImageKnob3 knobLpGain; // Gain
    RotaryImageKnob2 knob4;      // Q
   
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

    /** TextButton with right-click Pre/Post menu for Scope mode. */
    class ScopeModeButton : public juce::TextButton
    {
    public:
        ScopeModeButton() : juce::TextButton ("Scope") {}

        std::function<void()> onPopupMenu;

        void setIdleAlpha (float a) noexcept { idleAlpha = a; refreshAlpha(); }
        void mouseEnter (const juce::MouseEvent& e) override
        {
            setAlpha (1.0f);
            juce::TextButton::mouseEnter (e);
        }
        void mouseExit (const juce::MouseEvent& e) override
        {
            setAlpha (idleAlpha);
            juce::TextButton::mouseExit (e);
        }
        void refreshAlpha() noexcept { setAlpha (isMouseOver (true) ? 1.0f : idleAlpha); }

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onPopupMenu != nullptr)
                    onPopupMenu();
                return;
            }

            juce::TextButton::mouseDown (e);
        }

    private:
        float idleAlpha = 1.0f;
    };

    /** Small faceplate bank pager — paints a chevron (avoids TextButton "..." ellipsis). */
    class BankArrowButton : public juce::Button
    {
    public:
        explicit BankArrowButton (bool pointRightIn)
            : juce::Button (pointRightIn ? "bankNext" : "bankPrev"),
              pointRight (pointRightIn)
        {
            setClickingTogglesState (false);
        }

        void setChromeColours (juce::Colour fill, juce::Colour ink) noexcept
        {
            fillColour = fill;
            inkColour = ink;
            repaint();
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto bounds = getLocalBounds().toFloat().reduced (0.5f);
            auto fill = fillColour;
            auto ink = inkColour;
            if (! isEnabled())
            {
                fill = fill.withMultipliedAlpha (0.45f);
                ink = ink.withMultipliedAlpha (0.4f);
            }
            else if (down)
            {
                fill = fill.brighter (0.15f);
                ink = ink.brighter (0.1f);
            }
            else if (highlighted)
            {
                fill = fill.brighter (0.1f);
                ink = ink.brighter (0.08f);
            }

            GraphOverlayButtonLookAndFeel::fillRoundedGradient (g, bounds, fill, 3.0f);
            g.setColour (ink.withAlpha (0.35f));
            g.drawRoundedRectangle (bounds, 3.0f, 1.0f);

            // Chevron
            const float cx = bounds.getCentreX();
            const float cy = bounds.getCentreY();
            const float halfH = juce::jmin (bounds.getHeight(), bounds.getWidth()) * 0.22f;
            const float halfW = halfH * 0.85f;
            const float dir = pointRight ? 1.0f : -1.0f;

            juce::Path chevron;
            chevron.startNewSubPath (cx - dir * halfW, cy - halfH);
            chevron.lineTo (cx + dir * halfW, cy);
            chevron.lineTo (cx - dir * halfW, cy + halfH);
            g.setColour (ink);
            g.strokePath (chevron, juce::PathStrokeType (2.0f,
                                                         juce::PathStrokeType::curved,
                                                         juce::PathStrokeType::rounded));
        }

    private:
        bool pointRight = false;
        juce::Colour fillColour { juce::Colour::fromRGBA (50, 42, 28, 255) };
        juce::Colour inkColour { juce::Colours::whitesmoke.withAlpha (0.9f) };
    };

    /** Quad Scope metering view; right-click chooses Pre (analyzer) / Post (DSP on). */
    ScopeModeButton scopeModeButton;
    /** Amount 0-1 image knob; visible only while Side Check is enabled. */
    RotaryImageKnobForOptionBox sideCheckAmountKnob;
    juce::Label sideCheckAmountLabel;
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
    std::unique_ptr<SliderAttachment> highpassGainAttachment;
    std::unique_ptr<SliderAttachment> highpassQAttachment;
    std::unique_ptr<SliderAttachment> lowpassGainAttachment;
  
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
    SharedResources* themeColors = nullptr;

    /** Bottom chrome: toggle plugin tooltips (lit = enabled). Visible in compact and expanded. */
    juce::TextButton helpTooltipsButton { "?" };
    GraphOverlayButtonLookAndFeel graphOverlayButtonLookAndFeel;
    bool tooltipsEnabled = true;

    /** Bottom chrome: Minimum Phase / Linear Phase processing mode. */
    ParamChoiceButton phaseModeCombo;

    bool isButtonUpdate = false;

    bool hasForcedRepaint = false;
    mutable bool uiPrefsSavePending = false;
    mutable juce::uint32 uiPrefsSaveDueMs = 0;

    std::unique_ptr<BandNumberButton> onOffButton1;
    std::unique_ptr<BandNumberButton> onOffButton2;
    std::unique_ptr<BandNumberButton> onOffButton3;
    std::unique_ptr<BandNumberButton> onOffButton4;
    std::unique_ptr<BandNumberButton> onOffButton5;
    std::unique_ptr<BandNumberButton> onOffButton6;
    std::unique_ptr<BandNumberButton> onOffButton7;
    std::unique_ptr<BandNumberButton> onOffButton8;

    juce::TextButton testButton;

    juce::Colour backgroundColour{ juce::Colour::fromFloatRGBA(0.0f, 0.0f, 0.0f, 0.0f) };

    static constexpr double m_marginInPixels{ 10 };
    static constexpr int designWidth = 1200;
    static constexpr int designHeight = 850;

    juce::ComponentBoundsConstrainer resizeConstrainer;

    bool uiCompact = true;
    int savedEditorWidth = designWidth;
    int savedEditorHeight = designHeight;
    /** Captures editor size when entering Scope so strip/quad can restore it on exit. */
    bool holdingSizeBeforeScope = false;
    /** True while Scope size was last applied as strip (tracks arrange switches). */
    bool appliedScopeStrip = false;
    /** Persisted Scope window sizes per arrange mode (restored on re-enter / switch). */
    int lastStripScopeWidth = 0;
    int lastStripScopeHeight = 0;
    int lastTiledScopeWidth = 0;
    int lastTiledScopeHeight = 0;
    /** Legacy single-size prefs (migrated on load; still written for compatibility). */
    int lastScopeWidth = 0;
    int lastScopeHeight = 0;
    bool lastScopeWasStrip = false;

    void captureCurrentScopeWindowSize();
    void applyStripScopeWindowSize();
    void applyTiledScopeWindowSize();

    bool modPanelOpen = false;
    std::unique_ptr<ModSectionComponent> modSection;

    /** True after editor height has been grown for the piano strip. */
    bool pianoStripWindowApplied = false;

    int faceplateBank = 0;
    BankArrowButton faceplateBankPrevButton { false };
    BankArrowButton faceplateBankNextButton { true };
    juce::Label bandsFullToastLabel;
    int bandsFullToastTicks = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqEditor)

 
};
