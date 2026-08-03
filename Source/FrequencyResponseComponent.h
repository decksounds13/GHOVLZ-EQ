#pragma once

#include "EqProcessor.h"
#include "OptionBoxMenu.h"
#include "Menu/SharedResources.h"
#include "RotaryImageKnobForOptionBox.h"
#include <JuceHeader.h>
#include "BinaryData.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include <array>
#include <cmath>
#include <functional>

class EqProcessor; // Forward declaration
class EqEditor;    // Forward declaration

class CustomTimer; // Forward declaration

/** Mini piano-keys icon toggle for the graph bottom chrome. */
class PianoIconButton : public juce::TextButton
{
public:
    PianoIconButton()
        : juce::TextButton ("")
    {
        setClickingTogglesState (true);
        setTooltip ("Piano - show note keyboard under the graph for note-snapped band placement");
    }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        const bool on = getToggleState();
        auto fill = findColour (on ? buttonOnColourId : buttonColourId);
        if (isButtonDown)
            fill = fill.darker (0.15f);
        else if (isMouseOverButton)
            fill = fill.brighter (0.08f);

        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (findColour (on ? textColourOnId : textColourOffId).withAlpha (0.35f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        auto keys = r.reduced (3.5f, 4.0f);
        const auto ink = findColour (on ? textColourOnId : textColourOffId);
        g.setColour (ink.withAlpha (on ? 0.95f : 0.85f));
        g.fillRoundedRectangle (keys, 1.5f);

        const float whiteW = keys.getWidth() / 5.0f;
        g.setColour (fill.withAlpha (0.55f));
        for (int i = 1; i < 5; ++i)
            g.drawVerticalLine (juce::roundToInt (keys.getX() + whiteW * (float) i),
                                keys.getY() + 1.0f, keys.getBottom() - 1.0f);

        g.setColour (juce::Colours::black.withAlpha (on ? 0.75f : 0.65f));
        const float blackW = whiteW * 0.55f;
        const float blackH = keys.getHeight() * 0.58f;
        const float centres[3] = { 1.0f, 2.0f, 4.0f };
        for (float centre : centres)
        {
            const float cx = keys.getX() + whiteW * centre;
            g.fillRect (cx - blackW * 0.5f, keys.getY(), blackW, blackH);
        }
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PianoIconButton)
};

/** Match freeze toggle — large centered snowflake glyph. */
class MatchFreezeButton : public juce::TextButton
{
public:
    MatchFreezeButton()
        : juce::TextButton ("")
    {
        setClickingTogglesState (true);
        setTooltip ("Freeze - hold the current match target. Right-click or use the curve menu to save it.");
    }

    void paintButton (juce::Graphics& g, bool isMouseOverButton, bool isButtonDown) override
    {
        const bool on = getToggleState();
        auto fill = findColour (on ? buttonOnColourId : buttonColourId);
        if (isButtonDown)
            fill = fill.darker (0.15f);
        else if (isMouseOverButton)
            fill = fill.brighter (0.08f);

        auto r = getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (fill);
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (findColour (on ? textColourOnId : textColourOffId).withAlpha (0.35f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        const auto ink = findColour (on ? textColourOnId : textColourOffId)
                             .withAlpha (on ? 0.95f : 0.88f);
        const float cx = r.getCentreX();
        const float cy = r.getCentreY();
        const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.38f;
        g.setColour (ink);

        auto strokeArm = [&g, cx, cy, rad] (float angle)
        {
            juce::Path p;
            const float c = std::cos (angle);
            const float s = std::sin (angle);
            p.startNewSubPath (cx - c * rad, cy - s * rad);
            p.lineTo (cx + c * rad, cy + s * rad);
            // Side branches near each tip.
            const float br = rad * 0.42f;
            const float tip = rad * 0.62f;
            for (float dir : { 1.0f, -1.0f })
            {
                const float tx = cx + c * tip * dir;
                const float ty = cy + s * tip * dir;
                const float px = -s;
                const float py = c;
                p.startNewSubPath (tx - c * br * 0.35f, ty - s * br * 0.35f);
                p.lineTo (tx + px * br * 0.55f, ty + py * br * 0.55f);
                p.startNewSubPath (tx - c * br * 0.35f, ty - s * br * 0.35f);
                p.lineTo (tx - px * br * 0.55f, ty - py * br * 0.55f);
            }
            g.strokePath (p, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
        };

        constexpr float pi = juce::MathConstants<float>::pi;
        for (int i = 0; i < 3; ++i)
            strokeArm ((float) i * (pi / 3.0f));
        g.fillEllipse (cx - 1.6f, cy - 1.6f, 3.2f, 3.2f);
    }

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MatchFreezeButton)
};

/** Compact-UI output gain: click-drag vertically. Shift/Alt = fine (JUCE Slider convention). */
class OutputGainScrubber : public juce::Component,
                           private juce::AudioProcessorValueTreeState::Listener
{
public:
    OutputGainScrubber (juce::AudioProcessorValueTreeState& state, juce::UndoManager* undoMgr);
    ~OutputGainScrubber() override;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseEnter (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void refreshText();
    void beginGesture();
    void endGesture();

    juce::AudioProcessorValueTreeState& treeState;
    juce::UndoManager* undoManager = nullptr;
    juce::String displayText { "0.0 dB" };
    float dragStartDb = 0.0f;
    bool gestureActive = false;
    bool hovered = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OutputGainScrubber)
};

class FrequencyResponseComponent : public juce::Component,
    public juce::AudioProcessorValueTreeState::Listener,
    public juce::ComponentListener,
    private juce::Timer
{

public:
    FrequencyResponseComponent(EqProcessor& processor);
    ~FrequencyResponseComponent();

    std::function<void()> handle1DragStart;
    std::function<void()> handle1DragEnd;

    void resized() override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void mouseMove(const juce::MouseEvent& event) override;

    void showOptionBoxForHandle (int bandIndex, float handlePosX, float handlePosY);
    /** Open / focus OptionBox for a band using its current handle position (faceplate clicks). */
    void showOptionBoxForBand (int bandIndex);
    void showHandleModMenu (int bandIndex);
    void resetBandToDefaultsAndDeactivate (int bandIndex);
    void activateOrSelectBandAtFrequency (float frequencyHz);
    int bandIndexForFrequencyZone (float frequencyHz) const;
    float xToFrequency (float x) const;
    void updateAuditionBandpassFromMouse (const juce::MouseEvent& event);
    void setOptionBoxInteractionFaded (bool shouldFade);

    OptionBoxMenu* getOptionBoxMenu() noexcept { return optionBoxMenu.get(); }
    void setOptionBoxVisible (bool shouldBeVisible);
    /** True while OptionBox is open on this band (Bank1 0–7 or global 8–63). */
    bool isOptionBoxSelectingBand (int bandIndex) const noexcept;

    /** Called with band index (Bank1 internal 0–7, or global display 8–63); -1 to clear. */
    std::function<void(int)> onBandManipulationHighlight;
    /** Fired when the OptionBox is shown/hidden so the host can fix overlay z-order. */
    std::function<void()> onOptionBoxVisibilityChanged;
    /** Jump faceplate pager to this bank (0-based) after create/grow. */
    std::function<void(int)> onFaceplateBankJump;
    /** Soft-max (64) reached — show brief UI feedback. */
    std::function<void()> onBandsFullSoftMax;
    /** Faceplate bank used as preferred create target (banks 2+). */
    void setPreferredCreateBank (int bank) noexcept { preferredCreateBank = juce::jlimit (0, EqBand::kMaxBanks - 1, bank); }

    /** Sync ▲/▼ minimize control with editor compact state (lives on the graph, not faceplate). */
    void syncUiModeButton (bool isCompact);
    void syncModButton (bool isOpen);

    void updateBand1();
    void updateBand2();
    void updateBand3();
    void updateBand4();
    void updateLowpass();
    void updateHighpass();
    void updateLowShelf();
    void updateHighShelf();

    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void setCoefficients(const juce::dsp::IIR::Coefficients<float>& newCoefficients, double newSampleRate)
    {
        coefficients = newCoefficients;
        sampleRate = newSampleRate;
        repaint();
    }

    void paint(juce::Graphics& g) override;

    void setThemeColors (SharedResources* r) noexcept;

    void setEditor(EqEditor* newEditor) {
        editor = newEditor;
    }

    /**
        Place Match cluster: X from leftTopInLocal.x (editor-converted); Y always matches
        Mod / Proportional Q (piano-aware bottom chrome row). Returns strip bounds in FRC local.
    */
    juce::Rectangle<int> layoutMatchChromeAt (juce::Point<int> leftTopInLocal, int btnH, int matchBtnW);
    /** Bottom inset reserved for piano strip (0 or 50). Window grows by this when enabled. */
    int getPianoStripHeight() const noexcept;
    /**
        Height of Match / Mod / P chrome row above the piano (margin + button).
        OpenGL expanded Spec must stay clear of this — native peers ignore z-order.
    */
    int getBottomGraphChromeHeight() const noexcept;
    /** Graph / handle / curve height — excludes piano strip so enabling piano does not rescale the EQ. */
    int getPlotHeight() const noexcept;
    bool isPianoDisplayOn() const noexcept { return pianoDisplayOn; }
    void setPianoDisplayOn (bool shouldShow, bool savePrefs = true);

    float currentBandGain = 0.0f;
    float currentBandFrequency = 0.0f;
    float currentBandQ = 0.0f;
   
    std::function<void(const juce::Rectangle<int>&)> onDirtyBounds;

private:

    SharedResources* themeColors = nullptr;

    const SharedColors& colors() const noexcept;
    void applyThemeToChildControls();

    juce::dsp::IIR::Coefficients<float> coefficients;

    double sampleRate = 48000.0;

    bool isMouseDragging = false;

    float dotX = 0.0f;
    float dotY = 0.0f;
    float dot2X = 0.0f;

    float x = 0.0f;
    float y = 0.0f;
    float newX = 0.0f;
    float newY = 0.0f;

    float dragOffsetX = 0.0f;
    float dragOffsetY = 0.0f;
    float dragOffsetX2 = 0.0f;
    float dragOffsetY2 = 0.0f;
    float dragOffsetX3 = 0.0f;
    float dragOffsetY3 = 0.0f;
    float dragOffsetX4 = 0.0f;
    float dragOffsetY4 = 0.0f;
    float dragOffsetX5 = 0.0f;
    float dragOffsetY5 = 0.0f;
    float dragOffsetX6 = 0.0f;
    float dragOffsetY6 = 0.0f;
    float dragOffsetX7 = 0.0f;
    float dragOffsetY7 = 0.0f;
    float dragOffsetX8 = 0.0f;
    float dragOffsetY8 = 0.0f;

    
    bool isHandle1Dragging = false;
    bool isHandle2Dragging = false;
    bool isHandle3Dragging = false;
    bool isHandle4Dragging = false;
    bool isHandle5Dragging = false;
    bool isHandle6Dragging = false;
    bool isHandle7Dragging = false;
    bool isHandle8Dragging = false;
    

    float dragOffsetXHandle1 = 0.0f;
    float dragOffsetYHandle1 = 0.0f;
    float dragOffsetXHandle2 = 0.0f;
    float dragOffsetYHandle2 = 0.0f;
    float dragOffsetXHandle3 = 0.0f;
    float dragOffsetYHandle3 = 0.0f;
    float dragOffsetXHandle4 = 0.0f;
    float dragOffsetYHandle4 = 0.0f;
    float dragOffsetXHandle5 = 0.0f;
    float dragOffsetYHandle5 = 0.0f;
    float dragOffsetXHandle6 = 0.0f;
    float dragOffsetYHandle6 = 0.0f;
    float dragOffsetXHandle7 = 0.0f;
    float dragOffsetYHandle7 = 0.0f;
    float dragOffsetXHandle8 = 0.0f;
    float dragOffsetYHandle8 = 0.0f;

    float handleX = -1000.0f;
    float handleY = -1000.0f;
    float handleX2 = -1000.0f;
    float handleY2 = -1000.0f;
    float handleX3 = -1000.0f;
    float handleY3 = -1000.0f;
    float handleX4 = -1000.0f;
    float handleY4 = -1000.0f;
    float handleX5 = -1000.0f;
    float handleY5 = -1000.0f;
    float handleX6 = -1000.0f;
    float handleY6 = -1000.0f;
    float handleX7 = -1000.0f;
    float handleY7 = -1000.0f;
    float handleX8 = -1000.0f;
    float handleY8 = -1000.0f;
    int previousGraphWidth = 0;
    int previousGraphHeight = 0;

    bool shouldRedrawBand1 = false;
    bool shouldRedrawBand2 = false;
    bool shouldRedrawBand3 = false;
    bool shouldRedrawBand4 = false;
    bool shouldRedrawLowpass = false;
    bool shouldRedrawHighpass = false;
    bool shouldRedrawLowShelf = false;
    bool shouldRedrawHighShelf = false;

    bool isMouseHoveringOverHandle1 = false;
    bool isMouseHoveringOverHandle2 = false;
    bool isMouseHoveringOverHandle3 = false;
    bool isMouseHoveringOverHandle4 = false;
    bool isMouseHoveringOverHandle5 = false;
    bool isMouseHoveringOverHandle6 = false;
    bool isMouseHoveringOverHandle7 = false;
    bool isMouseHoveringOverHandle8 = false;

    juce::Label band1Label;
    juce::Label band2Label;
    juce::Label band3Label;
    juce::Label band4Label;
    juce::Label band5Label;
    juce::Label band6Label;
    juce::Label band7Label;
    juce::Label band8Label;

    juce::Label infoLabel;

    float cursorX = 0.0f;
    float cursorY = 0.0;
    bool mouseInside = false;

    float arrayCurrentBandGain[8];
    float arrayCurrentBandFrequency[8];
    float arrayCurrentBandQ[8];

    bool isMouseHoveringOverHandle[8] = { false };
    bool isHandleDragging[8] = { false };

    bool timerActive = false;
    int timerCounter = 0; // To keep track of how many times the timer has been fired

    bool isOptionBoxVisible = false;


    EqEditor* editor = nullptr;
    EqProcessor& processor;

    bool isAnyHandleMouseOver = false;
    bool anyHandleDragging = false;

    int downsamplingStep = 8;  // Only add a point every 5 steps

    juce::Path band1ResponsePath;
    bool needsUpdateBand1 = true;
    
    juce::Path band2ResponsePath;
    bool needsUpdateBand2 = true;
  
    juce::Path band3ResponsePath;
    bool needsUpdateBand3 = true;
  
    juce::Path band4ResponsePath;
    bool needsUpdateBand4 = true;
  
    juce::Path highpassResponsePath;
    bool needsUpdateHighpass = true;
  
    juce::Path lowpassResponsePath;
    bool needsUpdateLowpass = true;
   
    juce::Path highShelfResponsePath;
    bool needsUpdateHighShelf = true;
  
    juce::Path lowShelfResponsePath;
    bool needsUpdateLowShelf = true;
  
    juce::Path combinedResponsePath;
    bool needsUpdateCombined = true;

    // Melatonin dual-layer glow for the cumulative EQ sum curve.
    melatonin::DropShadow sumCurveGlow {
        {
            { juce::Colour::fromRGBA (255, 180, 60, 90), 16, { 0, 0 }, 2 },
            { juce::Colour::fromRGBA (255, 220, 120, 140), 6, { 0, 0 }, 0 }
        }
    };

    /** Soft drop under graph chrome buttons / knobs (separates them from the spectrum). */
    melatonin::DropShadow chromeDropShadow {
        { juce::Colours::black.withAlpha (0.42f), 5, { 0, 2 }, 0 }
    };
    void paintGraphChromeShadows (juce::Graphics& g);

    // Last effective gains used for the magnitude curve (dynamic EQ animation).
    float lastDynCurveGain1 = 1.0e9f;
    float lastDynCurveGain2 = 1.0e9f;
    float lastDynCurveGain3 = 1.0e9f;
    float lastDynCurveGain4 = 1.0e9f;
    float lastDynCurveGainHS = 1.0e9f;
    float lastDynCurveGainLS = 1.0e9f;

    /** True when any on band has Dynamic (D) or Spectral (S), or Side Check is on — drives the curve animation timer. */
    bool anyActiveDynamicEq() const;
    bool anyStructuralSplitArmed() const;
    void syncStructuralSplitChrome();
    void layoutStructuralSplitChrome();
    void setStructuralSplitSolo (StructuralSplit::Solo solo);
    void layoutMatchChrome();
    void syncMatchChrome();
    void requestMatchEnable();
    void disableMatch();
    void showMatchCurveMenu();
    void updateLiveMatchCaptureIfNeeded();
    /** While any D/S+On band is active: disable image buffer, run ~45 Hz force-rebuild timer.
        When idle: restore buffering and stop the timer. */
    void syncDynamicCurveTimer();
    /** Mark dyn/spectral-active bands (and the sum) dirty so the next paint rebuilds. */
    void markActiveDynamicBandsDirty();
    void timerCallback() override;

    // Cached per-band magnitude responses (dB), sized to component width.
    // Rebuilt from APVTS target values (not audio-smoothed coeffs) so the curve tracks knobs/handles instantly.
    std::vector<float> responseBand1, responseBand2, responseBand3, responseBand4;
    std::vector<float> responseHighpass, responseLowpass, responseHighShelf, responseLowShelf;
    std::vector<float> responseCombined;
    /** Alt+drag bandpass audition magnitude (dB) + path. */
    bool auditionBandpassDragging = false;
    std::vector<float> responseAuditionBp;
    juce::Path auditionBandpassPath;
    /** Scratch for sum-only IIR when a band's display curve is at target/range but D/SC is live. */
    std::vector<float> responseDynSumScratch;
    /** Per extended-band magnitude (dB) for individual curves + sum. */
    std::array<std::vector<float>, EqBand::kMaxBands - EqBand::kBankSize> responseExtended {};
    std::array<juce::Path, EqBand::kMaxBands - EqBand::kBankSize> extendedResponsePaths {};
    bool needsUpdateExtended = true;

    /** Spectral Amount visualization (second curve/handle when S is on). Slot = DSP 0–7. */
    static constexpr int kNumSpectralSlots = SpectralDynamics::kNumSlots;
    std::array<std::vector<float>, kNumSpectralSlots> responseSpectralAmount {};
    std::array<juce::Path, kNumSpectralSlots> spectralAmountPaths {};
    bool needsUpdateSpectralAmount = true;

    static constexpr int kNumExtended = EqBand::kMaxBands - EqBand::kBankSize;
    struct ExtendedHandleState
    {
        float x = -1000.0f;
        float y = -1000.0f;
        bool hovering = false;
        bool dragging = false;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;
    };
    std::array<ExtendedHandleState, kNumExtended> extendedHandles {};
    int activeExtendedGlobal = -1;

    struct SpectralAmountHandleState
    {
        float x = -1000.0f;
        float y = -1000.0f;
        bool hovering = false;
        bool dragging = false;
        float dragOffsetX = 0.0f;
        float dragOffsetY = 0.0f;
    };
    std::array<SpectralAmountHandleState, kNumSpectralSlots> spectralAmountHandles {};
    int activeSpectralAmountSlot = -1;
    int preferredCreateBank = 0;
    /** Scratch buffer for sampling published spectral GR onto display frequencies. */
    std::vector<float> spectralGrScratch;
    /** Per-frame summed spectral GR target (all S bands) before UI temporal smooth. */
    std::vector<float> spectralGrTarget;
    /** Temporally smoothed spectral GR added into responseCombined. */
    std::vector<float> spectralGrSmoothed;
    bool spectralGrSmoothedValid = false;
    /** Scratch buffer for sampling published Side Check GR onto display frequencies. */
    std::vector<float> sideCheckGrScratch;
    /** Match target curve (dB) + GR for graph overlay / sum. */
    std::vector<float> matchTargetScratch;
    std::vector<float> matchGrScratch;
    juce::Path matchTargetPath;

    void ensureResponseBufferSize (int width);
    void rebuildMagnitudeResponsesIfNeeded (int width);
    static void fillMagnitudeResponse (const juce::dsp::IIR::Coefficients<float>::Ptr& coeffs,
                                       const std::vector<float>& frequencies,
                                       double sr,
                                       std::vector<float>& outDb,
                                       int step);

    float getEqDisplayRangeDb() const; // 6, 12, 24, or 36
    float dbToY (float db, float height) const; // jmap db from -r..r to height..0
    float yToDb (float y, float height) const; // inverse
    float getBandPathWidth() const; // from EQ_BAND_PATH_WIDTH_ID, default 3
    float getSumPathWidth() const; // from EQ_SUM_PATH_WIDTH_ID, default 3
    bool isMulticolorBandFill() const; // EQ_MULTICOLOR_BAND_FILL_ID, default true
    bool isShowCrosshair() const; // EQ_SHOW_CROSSHAIR_ID, default true
    /** Per-band fill: multicolor palette when on; mono golden boost/cut when off. */
    juce::Colour resolveBandFillColour (juce::Colour multicolorFill, bool isBoostOrPass) const;
    bool bandGainIsBoost (const char* gainParamId, const char* typeParamId) const;
    bool bandGainIsBoost (const juce::String& gainParamId, const juce::String& typeParamId) const;

    void syncEqRangeControls();
    void adjustEqDisplayRange (int delta);

    juce::Path intelligentDownsample(
        const juce::Path& originalPath,
        const std::vector<float>& compositeResponse,
        int w, int h);


    juce::Path intelligentDownsampleToBottom(
        const juce::Path& originalPath,
        const std::vector<float>& compositeResponse,
        int w, int h);

    juce::Path intelligentDownsampleHighpass(
        const juce::Path& originalPath,
        const std::vector<float>& compositeResponse,
        int w, int h);

    juce::Path intelligentDownsampleLowpass(
        const juce::Path& originalPath,
        const std::vector<float>& compositeResponse,
        int w, int h);


    juce::Path simpleDownsample(
        const juce::Path& originalPath,
        const std::vector<float>& compositeResponse,
        int w, int h,
        int downsampleFactor
    );

    /** Close an open magnitude curve to the 0 dB centreline for fill (stroke stays open). */
    juce::Path closeShelfFillPath (const juce::Path& curvePath, float height) const;

    // Internal indices → Band 1–8 display names (see EqBand.h).
    // 0–3 peaking = Band 3–6; 4 HP = Band 1; 5 LP = Band 8; 6 HS = Band 7; 7 LS = Band 2.
    std::string arrayBandName[8] = { "Band 3", "Band 4", "Band 5", "Band 6", "Band 1", "Band 8", "Band 7", "Band 2" };
    std::string currentBandName;

    
    int activeBand = -1; // Initialize it to -1 to indicate no active band at the start

    int downsampleFactor = 5;

    int decimationFactor;  
    float adaptiveThreshold; 
    
    juce::AudioProcessorValueTreeState& parameters;  

    std::vector<float> logFrequencies;

    void precomputeLogFrequencies()
    {
        int w = getWidth();  // Or however you determine 'w'
        logFrequencies.clear();
        logFrequencies.reserve(w);
        double logMin = std::log10(20);  // 20 Hz
        double logMax = std::log10(20000);  // 20000 Hz
        for (int i = 0; i < w; ++i)
        {
            double logFreq = logMin + (logMax - logMin) * i / static_cast<double>(w - 1);
            logFrequencies.push_back(std::pow(10.0, logFreq));
        }
    }



    bool isSmoothKnob1AtTarget = processor.getSmoothKnob1Value() == processor.getTargetValueForSmoothKnob1();
    bool isSmoothKnob2AtTarget = processor.getSmoothKnob2Value() == processor.getTargetValueForSmoothKnob2();
    bool isSmoothKnob3AtTarget = processor.getSmoothKnob3Value() == processor.getTargetValueForSmoothKnob3();
    bool isSmoothKnob4AtTarget = processor.getSmoothKnob4Value() == processor.getTargetValueForSmoothKnob4();
    bool isSmoothKnob5AtTarget = processor.getSmoothKnob5Value() == processor.getTargetValueForSmoothKnob5();
    bool isSmoothKnob6AtTarget = processor.getSmoothKnob6Value() == processor.getTargetValueForSmoothKnob6();
    bool isSmoothKnob7AtTarget = processor.getSmoothKnob7Value() == processor.getTargetValueForSmoothKnob7();
    bool isSmoothKnob8AtTarget = processor.getSmoothKnob8Value() == processor.getTargetValueForSmoothKnob8();
    bool isSmoothKnob9AtTarget = processor.getSmoothKnob9Value() == processor.getTargetValueForSmoothKnob9();
    bool isSmoothKnob10AtTarget = processor.getSmoothKnob10Value() == processor.getTargetValueForSmoothKnob10();
    //bool isSmoothKnob11AtTarget = processor.getSmoothKnob11Value() == processor.getTargetValueForSmoothKnob11();
    //bool isSmoothKnob12AtTarget = processor.getSmoothKnob12Value() == processor.getTargetValueForSmoothKnob12();
    //bool isSmoothKnob13AtTarget = processor.getSmoothKnob13Value() == processor.getTargetValueForSmoothKnob13();
    bool isSmoothKnob14AtTarget = processor.getSmoothKnob14Value() == processor.getTargetValueForSmoothKnob14();
    bool isSmoothKnob15AtTarget = processor.getSmoothKnob15Value() == processor.getTargetValueForSmoothKnob15();
    bool isSmoothKnob16AtTarget = processor.getSmoothKnob16Value() == processor.getTargetValueForSmoothKnob16();
    bool isSmoothKnob17AtTarget = processor.getSmoothKnob17Value() == processor.getTargetValueForSmoothKnob17();
    bool isSmoothKnob18AtTarget = processor.getSmoothKnob18Value() == processor.getTargetValueForSmoothKnob18();
    bool isSmoothKnob19AtTarget = processor.getSmoothKnob19Value() == processor.getTargetValueForSmoothKnob19();
    bool isSmoothKnob20AtTarget = processor.getSmoothKnob20Value() == processor.getTargetValueForSmoothKnob20();
    bool isSmoothKnob21AtTarget = processor.getSmoothKnob21Value() == processor.getTargetValueForSmoothKnob21();
    bool isSmoothKnob22AtTarget = processor.getSmoothKnob22Value() == processor.getTargetValueForSmoothKnob22();

    bool highpassOnOff;

    std::unique_ptr<OnOffButton1> onOffButton1;
    std::unique_ptr<OnOffButton1> onOffButton2;
    std::unique_ptr<OnOffButton1> onOffButton3;
    std::unique_ptr<OnOffButton1> onOffButton4;
    std::unique_ptr<OnOffButton1> onOffButton5;
    std::unique_ptr<OnOffButton1> onOffButton6;
    std::unique_ptr<OnOffButton1> onOffButton7;
    std::unique_ptr<OnOffButton1> onOffButton8;

    float logMin, logMax;

    std::unique_ptr<OptionBoxMenu> optionBoxMenu;

    /** Right-click alternation: first opens OptionBox, second opens mod menu for same handle. */
    int lastOptionBoxBandIndex = -1;
    bool lastHandlePopupWasOptionBox = false;

    juce::TextButton uiModeButton { juce::CharPointer_UTF8 ("\xe2\x96\xb2") }; // ▲ when full
    juce::TextButton eqRangeMinusButton { "-" };
    juce::TextButton eqRangePlusButton { "+" };
    juce::Label eqRangeLabel;
    juce::TextButton modButton { "Mod" };
    juce::TextButton proportionalQButton { "P" };
    juce::TextButton autoGainButton { "A" };
    /** Graph-bottom Match cluster: Match | v | AMT | HP | LP | freeze. */
    juce::TextButton matchButton { "Match" };
    juce::TextButton matchCurveButton { "v" };
    MatchFreezeButton matchFreezeButton;
    RotaryImageKnobForOptionBox matchAmountKnob;
    juce::Label matchAmountLabel;
    RotaryImageKnobForOptionBox matchHpKnob;
    RotaryImageKnobForOptionBox matchLpKnob;
    juce::Label matchHpLabel;
    juce::Label matchLpLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> matchAmountAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> matchHpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> matchLpAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> matchFreezeAttachment;
    bool matchEnableDialogOpen = false;
    bool matchHpLpGapGuard = false;
    void enforceMatchHpLpGap (juce::Slider* changed);
    void showMatchHpLpSlopeMenu (bool forHp);
    /** Anchor for Match left edge X (Y ignored — Match uses Mod/P bottom chrome row). */
    juce::Point<int> matchChromeLeftTop {};
    int matchChromeBtnH = 18;
    int matchChromeMatchW = 48;
    bool matchChromeHasAnchor = false;
    juce::Rectangle<int> matchChromeBounds {};

    /** FabFilter-style piano under bottom chrome (50 px when on). Hidden by default. */
    static constexpr int kPianoStripHeightPx = 50;
    PianoIconButton pianoDisplayButton;
    bool pianoDisplayOn = false;
    int pianoDragBandIndex = -1;
    void paintPianoStrip (juce::Graphics& g);
    bool handlePianoMouseDown (const juce::MouseEvent& event);
    bool handlePianoMouseDrag (const juce::MouseEvent& event);
    bool handlePianoMouseUp (const juce::MouseEvent& event);
    float frequencyToX (float freqHz) const;
    void setBandFrequencySnappedToNote (int bandIndex, float freqHz);
    /** Bottom-center Transient / Sustain solo + Separation (visible when any split mode != Off). */
    juce::TextButton splitSoloTButton { "T" };
    juce::TextButton splitSoloSButton { "S" };
    juce::Slider splitSeparationKnob;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> proportionalQAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoGainAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> splitSeparationAttachment;
    OutputGainScrubber outputGainScrubber;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FrequencyResponseComponent)
};
