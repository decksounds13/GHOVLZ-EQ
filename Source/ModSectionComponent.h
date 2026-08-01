#pragma once

#include <JuceHeader.h>
#include "LfoMod.h"
#include "ShapeMod.h"
#include "ShapeCurveEditor.h"
#include "ComboBoxLookAndFeel.h"
#include "TextButtonLookAndFeel.h"
#include "RotaryImageKnobForOptionBox.h"
#include "RetrigButton.h"
#include "Menu/SharedResources.h"

class EqProcessor;
class ShapeEditorPopup;

/**
    Mod section: 3 LFOs + Shape curve + Envelope Follower + 20-slot matrix.
    Height is owned by EqEditor. Matrix stays on the right of the separator.
*/
class ModSectionComponent : public juce::Component,
                            public juce::Timer
{
public:
    ModSectionComponent (EqProcessor& processor);
    ~ModSectionComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    void setThemeColors (SharedResources* r) noexcept;
    const SharedColors& getSharedColors() const noexcept { return colors(); }

private:
    const SharedColors& colors() const noexcept;
    void applyThemeToChildControls();
    /** Image knob with right-click for Hz / Sync (same gesture as before). */
    class RateImageKnob : public RotaryImageKnobForOptionBox
    {
    public:
        std::function<void()> onPopupMenu;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onPopupMenu != nullptr)
            {
                onPopupMenu();
                return;
            }
            RotaryImageKnobForOptionBox::mouseDown (e);
        }
    };

    struct LfoColumn : public juce::Component,
                       public juce::AudioProcessorValueTreeState::Listener
    {
        LfoColumn (juce::AudioProcessorValueTreeState& state, int index, ModSectionComponent& ownerSection);
        ~LfoColumn() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void refreshShapeFromParam();
        void cycleShape (int delta);
        void syncRateModeUi();
        void showRateModeMenu();
        void showRetrigModeMenu();
        void setPlayhead (float phase01, bool active);
        void parameterChanged (const juce::String& parameterID, float newValue) override;

        int lfoIndex = 0;
        juce::AudioProcessorValueTreeState& treeState;
        ModSectionComponent& ownerSection;
        bool rateSyncMode = false;
        float playheadPhase = 0.0f;
        bool playheadActive = false;

        juce::Label title;
        ParamChoiceButton shapeCombo;
        juce::TextButton prevShape { "<" };
        juce::TextButton nextShape { ">" };
        RateImageKnob rateSlider;
        RetrigTextButton retrigButton { "N" };
        RotaryImageKnobForOptionBox phaseSlider;
        juce::Label rateLabel;
        juce::Label phaseLabel;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaseAttach;

        TextButtonLookAndFeel buttonLf;
        int currentShape = LfoMod::sine;
    };

    struct ShapeColumn : public juce::Component,
                         public juce::AudioProcessorValueTreeState::Listener
    {
        ShapeColumn (EqProcessor& processor, ModSectionComponent& owner);
        ~ShapeColumn() override;

        void paint (juce::Graphics& g) override;
        void resized() override;
        void syncRateModeUi();
        void showRateModeMenu();
        void showRetrigModeMenu();
        void syncSmoothToCurve();
        void parameterChanged (const juce::String& parameterID, float newValue) override;
        void refreshPlayhead();
        void openExpandedEditor();

        EqProcessor& processor;
        ModSectionComponent& owner;
        juce::AudioProcessorValueTreeState& treeState;
        bool rateSyncMode = false;

        juce::Label title;
        juce::TextButton expandButton { juce::CharPointer_UTF8 ("\xe2\x86\x95") }; // ↕
        ShapeCurveEditor editor;
        RateImageKnob rateSlider;
        RetrigTextButton retrigButton { "N" };
        RotaryImageKnobForOptionBox phaseSlider;
        RotaryImageKnobForOptionBox smoothSlider;
        juce::Label rateLabel;
        juce::Label phaseLabel;
        juce::Label smoothLabel;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> rateAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> phaseAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> smoothAttach;

        TextButtonLookAndFeel buttonLf;
    };

    struct EnvFollowerColumn : public juce::Component
    {
        EnvFollowerColumn (EqProcessor& processor, ModSectionComponent& ownerSection);
        void resized() override;
        void paint (juce::Graphics& g) override;
        void refreshMeter();

        EqProcessor& processor;
        ModSectionComponent& ownerSection;
        juce::Label title;
        juce::Slider thresholdSlider;
        RotaryImageKnobForOptionBox attackSlider;
        RotaryImageKnobForOptionBox releaseSlider;
        juce::Label threshLabel;
        juce::Label attackLabel;
        juce::Label releaseLabel;
        float displayedEnvDb = -60.0f;

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> threshAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attackAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> releaseAttach;
    };

    struct MatrixRow : public juce::Component,
                       public juce::AudioProcessorValueTreeState::Listener
    {
        MatrixRow (juce::AudioProcessorValueTreeState& state, int slotIndex, TextButtonLookAndFeel& buttonLf);
        ~MatrixRow() override;

        void resized() override;
        void syncPolarButton();
        void parameterChanged (const juce::String& parameterID, float newValue) override;

        int slot = 0;
        juce::AudioProcessorValueTreeState& treeState;
        juce::Label indexLabel;
        ParamChoiceButton sourceCombo;
        juce::TextButton polarButton { "<>" };
        juce::Slider amountSlider;
        ParamChoiceButton destCombo;
        juce::ToggleButton enabledToggle { "On" };

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> polarAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttach;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> enabledAttach;
    };

    struct MatrixContent : public juce::Component
    {
        MatrixContent (juce::AudioProcessorValueTreeState& state, TextButtonLookAndFeel& buttonLf);
        void resized() override;

        std::array<std::unique_ptr<MatrixRow>, LfoMod::kNumMatrixSlots> rows;
        static constexpr int kRowH = 24;
        static constexpr int kRowGap = 2;
    };

    void showShapeEditorPopup();

    EqProcessor& processor;
    juce::AudioProcessorValueTreeState& treeState;
    SharedResources* themeColors = nullptr;
    std::array<std::unique_ptr<LfoColumn>, LfoMod::kNumLfos> columns;
    std::unique_ptr<ShapeColumn> shapeColumn;
    std::unique_ptr<EnvFollowerColumn> envColumn;
    juce::Label matrixTitle;
    juce::Viewport matrixViewport;
    std::unique_ptr<MatrixContent> matrixContent;
    TextButtonLookAndFeel sharedButtonLf;
    std::unique_ptr<ShapeEditorPopup> shapePopup;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ModSectionComponent)
};
