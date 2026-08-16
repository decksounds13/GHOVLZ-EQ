#pragma once

#include <JuceHeader.h>
#include "../RotaryImageKnob1.h"
#include "../RotaryImageKnob2.h"
#include "../RotaryImageKnob3.h"
#include "../TextButtonLookAndFeel.h"
#include "../GraphOverlayButtonLookAndFeel.h"
#include "../Menu/SharedResources.h"
#include "DynParams.h"
#include "GainReductionMeter.h"
#include "TransferCurveComponent.h"
#include "DynCompressor.h"
#include "DynSplitLearn.h"
#include "../ComboBoxLookAndFeel.h"
#include "MelatoninBlur/melatonin/shadows.h"

class DynFaceplate : public juce::Component,
                     private juce::AudioProcessorValueTreeState::Listener,
                     private juce::AsyncUpdater
{
public:
    DynFaceplate (juce::AudioProcessorValueTreeState& s, DynCompressor& e, Analyser& analyser);
    ~DynFaceplate() override;

    void resized() override;
    void paint (juce::Graphics& g) override;
    void setThemeColors (SharedResources* r) noexcept;
    void refresh();

    int getSelectedBand() const;
    bool isShowingAll() const noexcept { return showingAll; }
    void toggleGlobalLearn();
    void showGlobalLearnMenu (juce::Component& target);
    void toggleBandLearn (int band);
    void showBandLearnMenu (int band, juce::Component& target);
    bool isLearnLit() const noexcept { return splitLearn.isLearning() && splitLearn.activeBand() < 0; }
    bool isBandLearnLit (int band) const noexcept
    {
        return splitLearn.isLearning() && splitLearn.activeBand() == band;
    }
    /** Packed Focus well width (knobs + GR), for centering next to the transfer. */
    int getFocusClusterWidth (int forHeight = 0) const noexcept;

    std::function<void()> onModeChanged;
    std::function<void()> onLearnChanged;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void bindFocus (int band);
    void rebuildAll();
    void updateFocusReadouts();
    void showClipModeMenu (int band, juce::Component& target);
    void showLearnMenu();
    void refreshLearnButtons();
    int bandCount() const;

    struct ClipKnob : public RotaryImageKnob2
    {
        std::function<void()> onPopupMenu;
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onPopupMenu != nullptr)
                    onPopupMenu();
                return;
            }
            RotaryImageKnob2::mouseDown (e);
        }
    };
    void parameterChanged (const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    juce::AudioProcessorValueTreeState& state;
    DynCompressor& engine;
    SharedResources* theme = nullptr;
    GraphOverlayButtonLookAndFeel chromeLaf;

    juce::TextButton focusButton { "Single" };
    juce::TextButton allButton { "All" };
    juce::TextButton addBandButton { "+" };
    struct LearnButton : public juce::TextButton
    {
        std::function<void()> onPopupMenu;
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
    };
    DynSplitLearn splitLearn;
    juce::Label selLabel;
    juce::TextButton onButton { "On" };
    juce::TextButton soloButton { "Solo" };
    LearnButton focusLearnButton;

    RotaryImageKnob1 thrKnob, upThrKnob, ratioKnob, attackKnob, releaseKnob;
    RotaryImageKnob2 kneeKnob, makeupKnob, mixKnob;
    ClipKnob clipKnob;
    juce::Label thrLab, upThrLab, ratioLab, attackLab, releaseLab, kneeLab, makeupLab, mixLab, clipLab;
    juce::Label thrVal, upThrVal, ratioVal, attackVal, releaseVal, kneeVal, makeupVal, mixVal, clipVal;

    std::unique_ptr<GainReductionMeter> focusGr;
    std::unique_ptr<SliderAttachment> thrAt, upThrAt, ratioAt, attackAt, releaseAt, kneeAt, makeupAt, mixAt, clipAt;
    std::unique_ptr<ButtonAttachment> onAt, soloAt;

    struct GlobalPanel : public juce::Component
    {
        GlobalPanel (juce::AudioProcessorValueTreeState& s);
        void resized() override;
        void paint (juce::Graphics& g) override;
        void setThemeColors (SharedResources* r) noexcept;
        void updateReadouts();

        juce::AudioProcessorValueTreeState& state;
        SharedResources* theme = nullptr;
        juce::Label title;
        RotaryImageKnob1 timeKnob;
        RotaryImageKnob2 amountKnob, downKnob, upKnob;
        juce::Label timeLab, amountLab, downLab, upLab;
        juce::Label timeVal, amountVal, downVal, upVal;
        std::unique_ptr<SliderAttachment> timeAt, amountAt, downAt, upAt;
    };

    GlobalPanel globalPanel;

    struct AllColumn : public juce::Component
    {
        AllColumn (juce::AudioProcessorValueTreeState& s, DynCompressor& e, int bandIndex,
                   GraphOverlayButtonLookAndFeel& chrome);
        ~AllColumn() override;
        void resized() override;
        void paint (juce::Graphics& g) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void setThemeColors (SharedResources* r) noexcept;

        int band = 0;
        juce::AudioProcessorValueTreeState& state;
        SharedResources* theme = nullptr;
        juce::Label title;
        juce::TextButton on { "On" }, solo { "Solo" };
        LearnButton learn;
        RotaryImageKnob1 thr, upThr, ratio, attack, release;
        RotaryImageKnob2 knee, makeup;
        ClipKnob clip;
        juce::Label tL, uL, rL, aL, eL, kL, mL, cL;
        std::unique_ptr<GainReductionMeter> gr;
        std::unique_ptr<TransferCurveComponent> xfer;
        std::unique_ptr<SliderAttachment> tA, uA, rA, aA, eA, kA, mA, cA;
        std::unique_ptr<ButtonAttachment> onA, soA;
    };

    juce::OwnedArray<AllColumn> columns;
    juce::Rectangle<int> focusWell;
    bool showingAll = false;

    melatonin::DropShadow moduleShadow {
        {
            { juce::Colour::fromRGBA (0, 0, 0, 140), 14, { 0, 5 }, 1 },
            { juce::Colour::fromRGBA (0, 0, 0, 80), 5, { 0, 2 }, 0 }
        }
    };

    void drawModuleShadow (juce::Graphics& g, juce::Rectangle<float> r);
};
