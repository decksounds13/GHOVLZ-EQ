#pragma once
#include <JuceHeader.h>
#include <array>
#include "ComboBoxLookAndFeel.h"
#include "RotaryImageKnobForOptionBox.h"
#include "OnOffButton1.h"
#include "TextButtonLookAndFeel.h"
#include "FilterSlope.h"
#include "BandChannel.h"
#include "DynamicEq.h"
#include "Spectral/SpectralBandSettings.h"
#include "Spectral/SpectralPerBandLattice.h"
#include "BandSaturation.h"
#include "BandSidechain.h"
#include "Menu/SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"

class EqProcessor;

class OptionBoxMenu : public juce::Component,
                      public juce::Button::Listener,
                      public juce::ComboBox::Listener,
                      public juce::Slider::Listener,
                      public juce::AudioProcessorValueTreeState::Listener,
                      private juce::Timer
{
public:
    OptionBoxMenu (juce::AudioProcessorValueTreeState& state, EqProcessor& processor);
    ~OptionBoxMenu() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void buttonClicked(juce::Button* button) override;
    void comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged) override;
    void resized() override;
    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void sliderValueChanged (juce::Slider*) override {}
    void sliderDragStarted (juce::Slider* slider) override;
    void sliderDragEnded (juce::Slider* slider) override;

    void setCurrentBandName(const std::string& name);
    bool isVisible() const;
    void setInitialPosition(int x, int y);
    void setDraggable(bool shouldBeDraggable);
    /**
        Select the OptionBox target band.
        @param index Internal Bank 1 slot 0–7, or global display index 8–63 (Band 9–64).
    */
    void setCurrentBandIndex(int index, const std::string bandNames[]);
    void setInteractionFaded (bool shouldFade);
    /** Keep OptionBox visual size locked to the plugin scale after parent resize. */
    void updateUiScaleFromParent();
    /** Internal 0–7, or global display 8–63 for extended bands. */
    int getCurrentBandIndex() const { return currentBandIndex; }

    static constexpr int designWidth = 150;
    /** Spectral + A/R room; shorter header/Q share a row (see topCrop in resized). */
    static constexpr int designHeight = 302;
    /** Pixels removed from the old 340 layout so the box top can sit lower on screen. */
    static constexpr int designTopCrop = 37;
    static constexpr float designParentWidth = 1200.0f;

    std::function<void()> onMouseEnterCallback;
    std::function<void()> onMouseExitCallback;
    /** Fired with current band index on OptionBox knob drag start; -1 on end. */
    std::function<void(int)> onBandKnobDragHighlight;
    /** Fired when < > cycles the active band (box stays put). */
    std::function<void(int)> onBandCycled;
    /** Optional: start an undo transaction before OptionBox parameter edits. */
    juce::UndoManager* undoManager = nullptr;

    void setThemeColors (SharedResources* r) noexcept;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void bindKnobsToBand (int index);
    void clearAttachments();
    void setupFilterModelMenu (int bandIndex);
    void syncChannelModeButtons();
    void syncSpectralPackButton();
    void syncSatControls();
    void showSatContextMenu();
    void showSpectralSatContextMenu();
    void syncSpectralSatControls();
    void setChannelMode (int mode);
    void cycleSpectralPackMode();
    void cycleBand (int delta);
    void updateDisplayAlpha();
    void updateDynamicControlsVisibility();
    void bindDynamicControls (int bandIndex);
    /** Bind Res to per-band *SpectralResHz when PB on, else global spectralResHz. */
    void bindSpectralResSlider (int bandIndex);
    bool isPerBandLatticeEnabled() const;
    void setupFilterSlopeMenu (int bandIndex);
    void updateFilterSlopeVisibility();
    bool currentBandShowsFilterSlope() const;
    bool isCurrentBandEnabled() const;
    bool currentBandSupportsDynamic() const;
    bool currentBandSupportsSpectral() const;
    bool currentBandSupportsSat() const;
    bool currentBandSupportsSidechain() const;
    juce::String getOnOffParamIDForBand (int bandIndex) const;
    void listenToCurrentBandOnOff (bool shouldListen);
    void listenToCurrentBandDynamic (bool shouldListen);
    void listenToCurrentBandSpectral (bool shouldListen);
    void timerCallback() override;
    void paintDynThresholdMeter (juce::Graphics& g);
    float getUiScale() const;
    void applyUiScale();
    int getVisualWidth() const;
    int getVisualHeight() const;
    /** Hard-clamp so the transformed visual bounds never leave the graph parent. */
    void constrainVisualBoundsToParent();
    const SharedColors& colors() const noexcept;
    void applyThemeToChildControls();

    bool isBeingDragged = false;
    juce::Point<int> lastMousePosition;
    bool isDraggable = true;
    int dragOffsetX = 0;
    int dragOffsetY = 0;
    int anchorHandleX = 0;
    int anchorHandleY = 0;
    bool isMouseOverChildComponent = false;
    bool interactionFaded = false;
    int separatorY;
    /** Bank 1: internal DSP index 0–7. Extended: global display index 8–63. */
    int currentBandIndex = -1;
    juce::String currentOnOffParamID;
    juce::String currentDynamicParamID;
    juce::String currentSpectralParamID;
    juce::String currentSpectralPackParamID;

    juce::ComboBox customComboBox;
    /** HP/LP slope — visible whenever the band's filter model is Highpass/Lowpass. */
    juce::ComboBox filterSlopeComboBox;
    juce::Label bandNameLabel;
    juce::TextButton prevBandButton { "<" };
    juce::TextButton nextBandButton { ">" };
    std::string currentBandName;
    std::array<std::string, 8> cachedBandNames {};
    juce::Line<float> separator;

    SharedResources* themeColors = nullptr;

    /** Soft drop shadow under the floating OptionBox (Melatonin). */
    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 14, { 0, 5 }, 0 }
    };

    ComboBoxLookAndFeel customLookAndFeel;

    std::unique_ptr<OnOffButton1> onOffButton1;

    RotaryImageKnobForOptionBox rotaryImageKnobForOptionBox1;
    RotaryImageKnobForOptionBox rotaryImageKnobForOptionBox2;
    RotaryImageKnobForOptionBox rotaryImageKnobForOptionBox3;

    std::unique_ptr<SliderAttachment> frequencyAttachment;
    std::unique_ptr<SliderAttachment> gainAttachment;
    std::unique_ptr<SliderAttachment> qAttachment;
    std::unique_ptr<SliderAttachment> dynThresholdAttachment;
    std::unique_ptr<SliderAttachment> attackMsAttachment;
    std::unique_ptr<SliderAttachment> releaseMsAttachment;
    std::unique_ptr<SliderAttachment> spectralBandwidthAttachment;
    /** Inverted vertical Amount: slider 0 at bottom = max Amount. */
    std::unique_ptr<juce::ParameterAttachment> spectralAmountAttachment;
    std::unique_ptr<ComboBoxAttachment> filterModelAttachment;
    std::unique_ptr<ComboBoxAttachment> filterSlopeAttachment;
    std::unique_ptr<ButtonAttachment> dynamicButtonAttachment;
    std::unique_ptr<ButtonAttachment> spectralButtonAttachment;
    std::unique_ptr<ButtonAttachment> spectralExpandButtonAttachment;
    std::unique_ptr<ButtonAttachment> spectralPerBandLatticeAttachment;
    std::unique_ptr<ButtonAttachment> satButtonAttachment;
    std::unique_ptr<ButtonAttachment> satPostButtonAttachment;
    std::unique_ptr<SliderAttachment> satDriveAttachment;
    std::unique_ptr<ButtonAttachment> spectralSatButtonAttachment;
    std::unique_ptr<ButtonAttachment> sidechainButtonAttachment;
    std::unique_ptr<ButtonAttachment> sidechainMidiButtonAttachment;
    std::unique_ptr<SliderAttachment> spectralSatDriveAttachment;

    juce::Label frequencyLabel;
    juce::Label gainLabel;
    juce::Label qLabel;
    juce::Label attackLabel;
    juce::Label releaseLabel;
    juce::Label spectralResLabel;
    juce::Label spectralAmountLabel;

    juce::TextButton midSelectorButton;
    juce::TextButton sideSelectorButton;
    juce::TextButton leftSelectorButton;
    juce::TextButton rightSelectorButton;

    /** External sidechain duck (audio bus or MIDI). Centered under M/S/L/R. */
    juce::TextButton sidechainButton { "Sidechain" };
    /** MIDI trigger mode — visible when Sidechain is on. */
    juce::TextButton sidechainMidiButton { "M" };

    /** Dynamic EQ toggle (Pro-Q style); matches M/S/L/R button look. */
    juce::TextButton dynamicButton;
    /** Spectral dynamics toggle (Pro-Q Spectral style). */
    juce::TextButton spectralButton;
    /** Invert Amount direction: expand (boost) resonances instead of suppress. */
    juce::TextButton spectralExpandButton;
    /**
        Global lattice pack (shared by all S bands). Label: Flat / LF / HF.
        Click cycles Flat → LF → HF → Flat.
    */
    juce::TextButton spectralPackButton { "FL" };
    /**
        Optional per-band local lattice (sandboxed). Off = legacy global grid.
        Visible only while S is on; sits above the Res slider.
    */
    juce::TextButton spectralPerBandLatticeButton { "PB" };
    /**
        Per-band sat enable (right of filter model). Orange glow when on.
        Right-click: model + oversample menu. Pre/Post appears when enabled.
    */
    juce::TextButton satButton { "Sat" };
    juce::TextButton satPrePostButton { "Pre" };
    /**
        Post-mode drive (−12…+12 dB into the shaper on EQ−dry).
        Same compact size as Side Check HP/LP; only when Sat + Post.
    */
    RotaryImageKnobForOptionBox satDriveKnob;
    /**
        Stage 2 — global post-Spectral sat. Visible when S is on.
        Right-click: model + oversample. Drive knob (½ A/R) only when → is on.
    */
    juce::TextButton spectralSatButton;
    /** Post-Spectral drive — half the size of A/R knobs; only when → sat is on. */
    RotaryImageKnobForOptionBox spectralSatDriveKnob;
    juce::Slider dynThresholdSlider;
    /** Spectral resolution / target BP bandwidth (Hz); vertical, closest to D/S when S is on. */
    juce::Slider spectralBandwidthSlider;
    /**
        Spectral Amount (0–2). Vertical to the right of Res; inverted so pull-down = more.
        Bound to *SpectralDepth params.
    */
    juce::Slider spectralAmountSlider;
    /** Compact attack / release knobs — D and/or S (shared envelope params). */
    RotaryImageKnobForOptionBox attackKnob;
    RotaryImageKnobForOptionBox releaseKnob;

    TextButtonLookAndFeel myTextButtonLookAndFeel;

    /** Inactive Sidechain reads greyed; active keeps yellow. Font 2pt under shared buttons. */
    class SidechainTextButtonLookAndFeel : public TextButtonLookAndFeel
    {
    public:
        SidechainTextButtonLookAndFeel() : TextButtonLookAndFeel (14.0f) {}

        void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                             bool shouldDrawButtonAsHighlighted,
                             bool shouldDrawButtonAsDown) override
        {
            juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
            g.setColour (button.getToggleState()
                             ? button.findColour (juce::TextButton::textColourOnId)
                             : button.findColour (juce::TextButton::textColourOffId));
            g.setFont (juce::Font ("Lato Black", 14.0f, juce::Font::plain));
            g.drawText (button.getButtonText(), button.getLocalBounds(),
                        juce::Justification::centred, true);
        }
    };

    SidechainTextButtonLookAndFeel sidechainButtonLookAndFeel;

    juce::AudioProcessorValueTreeState& treeState;
    EqProcessor& processor;
    float displayedDynEnvelopeDb = -120.0f;
};
