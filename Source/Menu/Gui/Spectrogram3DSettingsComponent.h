#pragma once

#include <JuceHeader.h>
#include <array>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/ColourSwatchEditor.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../../Spectrogram3DComponent.h"
#include "../../RotaryImageKnob1.h"
#include "ParticleModCurveEditor.h"
#include "ParticleForceStackComponent.h"
#include "CustomTwoValueSliderLookAndFeel.h"
#include "../SharedResources.h"

class Spectrogram3DSettingsComponent : public juce::Component,
                                       private juce::ChangeListener
{
public:
    Spectrogram3DSettingsComponent (SharedResources& resources,
                                    juce::AudioProcessorValueTreeState& state,
                                    ColourRampBank& colourRamps);
    ~Spectrogram3DSettingsComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredContentHeight() const { return content.getPreferredHeight(); }
    /** Content lays out into the Settings panel width — does not force the frame wider. */
    int getPreferredContentWidth() const { return content.getPreferredWidth(); }
    /** Sync Look toggles from Main before Menu measures scroll height. */
    void syncFromMain() { content.syncControlsFromMain(); }
    /** Mirror live DOF focus (Ctrl/Cmd+LMB pick) onto the Focus Distance slider. */
    void syncDofFocusFromMain() { content.syncDofFocusFromMain(); }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    class Content : public juce::Component
    {
    public:
        Content (SharedResources& resources,
                 juce::AudioProcessorValueTreeState& state,
                 ColourRampBank& colourRamps);
        ~Content() override;

        void resized() override;
        int getPreferredHeight() const;
        int getPreferredWidth() const;
        void syncGradientFromBank();

    private:
        void styleSlider (juce::Slider& slider);
        /** Drag stays within setRange; typed text can set values outside range (stored as actual). */
        void wireUncappedTextEntry (juce::Slider& slider);
        static double getSliderActual (const juce::Slider& slider);
        static void setSliderActual (juce::Slider& slider, double actual);
        void styleLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleCombo (juce::ComboBox& combo);
        void styleSaveDefaultButton (juce::TextButton& button);
        void saveModuleLookDefault();
        void saveModuleLookPreset();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);
        void layoutToggle (juce::Rectangle<int>& area, juce::ToggleButton& toggle);
        void layoutColourEditor (juce::Rectangle<int>& area, ColourSwatchEditor& editor);
        void setLookChildVisible (juce::Component& c, bool vis);
        void updateLookDevVisibility();
        void requestParentRelayout();
        void syncControlsFromMain();
        void syncDofFocusFromMain();
        void applyControlsToMain();
        void applyLookControlsToMain();
        void applyStructureControlsToMain();
        void browseDomeTextureFile();
        void wireColourEditor (ColourSwatchEditor& editor);

        friend class Spectrogram3DSettingsComponent;

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ColourRampBank& colourRamps;
        ComboBoxLookAndFeel comboLookAndFeel;
        /** Two-thumb range slider chrome (particle mod matrix). */
        CustomTwoValueSliderLookAndFeel particleRangeLnF;

        juce::Label titleLabel;

        juce::ToggleButton enable3DToggle { "Enable 3D view (expanded / Scope)" };
        juce::ToggleButton enhancedFreq3DToggle { "Enhanced Frequency (3D)" };
        std::unique_ptr<ButtonAttachment> enhancedFreq3DAttachment;
        juce::Label fftSizeLabel;
        juce::ComboBox fftSizeCombo;
        std::unique_ptr<ComboBoxAttachment> fftSizeAttachment;
        juce::Label meshQualityLabel;
        juce::ComboBox meshQualityCombo;
        juce::Label meshHeightLabel;
        juce::Slider meshHeightSlider;
        juce::Label freqMeshBiasLabel;
        juce::Slider freqMeshBiasSlider;
        juce::Label freqMeshBiasPivotLabel;
        juce::Slider freqMeshBiasPivotSlider;
        juce::Label msaaLabel;
        juce::ComboBox msaaCombo;
        juce::ToggleButton transparentBgToggle { "Soft background (like Osc/Gon)" };
        juce::ToggleButton reverseFreqAxisToggle { "Reverse frequency axis" };
        juce::ToggleButton closedMeshToggle { "Closed mesh (solid)" };
        juce::Label softAngleLabel;
        juce::Slider softAngleSlider;
        juce::TextButton resetCameraButton { "Reset 3D Camera" };
        juce::TextButton saveDefaultButton { "Save Default" };
        juce::TextButton savePresetButton { "Save Preset..." };

        juce::Label lookLabel;
        juce::ToggleButton audioLevelToggle { "Audio level affects" };
        juce::Label audioLevelTargetLabel;
        juce::ComboBox audioLevelTargetCombo;
        juce::Label audioLevelMinPctLabel;
        juce::Slider audioLevelMinPctSlider;
        juce::Label audioLevelMaxPctLabel;
        juce::Slider audioLevelMaxPctSlider;
        juce::Label audioLevelHpLabel;
        juce::Slider audioLevelHpSlider;
        juce::Label audioLevelLpLabel;
        juce::Slider audioLevelLpSlider;
        juce::Label audioLevelThresholdLabel;
        juce::Slider audioLevelThresholdSlider;
        juce::Label audioLevelSpeedLabel;
        juce::ComboBox audioLevelSpeedCombo;
        juce::ToggleButton audioAffectPlayheadToggle { "Affect playhead" };
        juce::ToggleButton audioAffectAntiPlayheadToggle { "Affect anti-playhead" };
        juce::ToggleButton lightingToggle { "Lighting" };
        juce::Label lightingAmountLabel;
        juce::Slider lightingAmountSlider;
        juce::Label lightAzimuthLabel;
        juce::Slider lightAzimuthSlider;
        juce::Label lightElevationLabel;
        juce::Slider lightElevationSlider;
        juce::Label specularLabel;
        juce::Slider specularSlider;
        juce::Label roughnessLabel;
        juce::Slider roughnessSlider;
        juce::Label metalnessLabel;
        juce::Slider metalnessSlider;
        juce::ToggleButton energyConserveToggle { "Energy conserving (1-F)" };
        juce::Label rimLabel;
        juce::Slider rimSlider;
        ColourSwatchEditor lightColourEditor { "Light Color" };
        ColourSwatchEditor rimColourEditor { "Rim Color" };

        juce::ToggleButton domeFillToggle { "Dome fill" };
        juce::Label domeFillStrengthLabel;
        juce::Slider domeFillStrengthSlider;
        juce::ToggleButton domeTextureToggle { "Dome texture" };
        juce::Label domeTextureLabel;
        juce::ComboBox domeTextureCombo;
        ColourSwatchEditor domeSkyEditor { "Dome Sky" };
        ColourSwatchEditor domeGroundEditor { "Dome Ground" };
        std::unique_ptr<juce::FileChooser> domeTextureChooser;

        juce::ToggleButton ssgiToggle { "SSGI (screen-space GI)" };
        juce::Label ssgiStrengthLabel;
        juce::Slider ssgiStrengthSlider;
        juce::Label ssgiRadiusLabel;
        juce::Slider ssgiRadiusSlider;
        juce::Label ssgiQualityLabel;
        juce::ComboBox ssgiQualityCombo;

        juce::ToggleButton ssrToggle { "SSR (screen-space reflections)" };
        juce::Label ssrStrengthLabel;
        juce::Slider ssrStrengthSlider;
        juce::Label ssrDistanceLabel;
        juce::Slider ssrDistanceSlider;
        juce::Label ssrThicknessLabel;
        juce::Slider ssrThicknessSlider;
        juce::Label ssrQualityLabel;
        juce::ComboBox ssrQualityCombo;
        juce::Label ssrFresnelLabel;
        juce::Slider ssrFresnelSlider;
        juce::Label ssrRoughInfLabel;
        juce::Slider ssrRoughInfSlider;
        juce::Label ssrIntensityLabel;
        juce::Slider ssrIntensitySlider;
        juce::Label ssrEdgeFadeLabel;
        juce::Slider ssrEdgeFadeSlider;
        juce::Label ssrMetalBiasLabel;
        juce::Slider ssrMetalBiasSlider;
        juce::Label ssrDomeFbLabel;
        juce::Slider ssrDomeFbSlider;

        juce::ToggleButton contactShadowToggle { "Contact shadow" };
        juce::Label contactShadowStrengthLabel;
        juce::Slider contactShadowStrengthSlider;

        juce::ToggleButton selfShadowToggle { "Self-shadowing" };
        juce::Label selfShadowStrengthLabel;
        juce::Slider selfShadowStrengthSlider;
        juce::Label selfShadowBiasLabel;
        juce::Slider selfShadowBiasSlider;
        juce::Label selfShadowSoftnessLabel;
        juce::Slider selfShadowSoftnessSlider;
        juce::Label selfShadowQualityLabel;
        juce::ComboBox selfShadowQualityCombo;

        juce::ToggleButton castShadowsToggle { "Cast Shadows (shadow map)" };
        juce::Label shadowResLabel;
        juce::ComboBox shadowResCombo;
        juce::Label cascadeCountLabel;
        juce::ComboBox cascadeCountCombo;
        juce::Label cascadeDistLabel;
        juce::Slider cascadeDistSlider;
        juce::Label cascadeTransLabel;
        juce::Slider cascadeTransSlider;

        juce::ToggleButton ssaoToggle { "Ambient occlusion" };
        juce::Label ssaoStrengthLabel;
        juce::Slider ssaoStrengthSlider;
        juce::Label ssaoRadiusLabel;
        juce::Slider ssaoRadiusSlider;

        juce::ToggleButton bloomToggle { "Peak glow (bloom)" };
        juce::Label bloomStrengthLabel;
        juce::Slider bloomStrengthSlider;
        juce::Label bloomThresholdLabel;
        juce::Slider bloomThresholdSlider;

        juce::ToggleButton motionBlurToggle { "Motion blur" };
        juce::Label motionBlurAmountLabel;
        juce::Slider motionBlurAmountSlider;
        juce::Label motionBlurMaxLabel;
        juce::Slider motionBlurMaxSlider;
        juce::Label motionBlurQualityLabel;
        juce::ComboBox motionBlurQualityCombo;

        juce::ToggleButton dofToggle { "Depth of field" };
        juce::Label dofFocusLabel;
        juce::Slider dofFocusSlider;
        juce::Label dofFStopLabel;
        juce::Slider dofFStopSlider;
        juce::Label dofFocalLengthLabel;
        juce::Slider dofFocalLengthSlider;
        juce::Label dofQualityLabel;
        juce::ComboBox dofQualityCombo;
        juce::Label dofCocDilateLabel;
        juce::Slider dofCocDilateSlider;
        juce::Label dofEdgeSpillLabel;
        juce::Slider dofEdgeSpillSlider;

        juce::ToggleButton tonemapToggle { "Tonemap / grade" };
        juce::Label exposureLabel;
        juce::Slider exposureSlider;
        juce::Label gradeLabel;
        juce::ComboBox gradeCombo;

        juce::ToggleButton sssToggle { "Subsurface (SSS)" };
        juce::Label sssStrengthLabel;
        juce::Slider sssStrengthSlider;
        juce::Label sssWrapLabel;
        juce::Slider sssWrapSlider;
        juce::Label sssTransmissionLabel;
        juce::Slider sssTransmissionSlider;
        juce::Label sssTintRLabel;
        juce::Slider sssTintRSlider;
        juce::Label sssTintGLabel;
        juce::Slider sssTintGSlider;
        juce::Label sssTintBLabel;
        juce::Slider sssTintBSlider;
        juce::Label sssRadiusLabel;
        juce::Slider sssRadiusSlider;
        juce::Label sssContrastLabel;
        juce::Slider sssContrastSlider;
        juce::Label sssQualityLabel;
        juce::ComboBox sssQualityCombo;
        juce::Label sssThickScaleLabel;
        juce::Slider sssThickScaleSlider;
        juce::Label sssMaxThickLabel;
        juce::Slider sssMaxThickSlider;

        juce::ToggleButton particleToggle { "Particle mode" };
        juce::ToggleButton particleGpuSimToggle { "GPU particle integrate" };
        juce::Label particleMaxAliveLabel;
        juce::Slider particleMaxAliveSlider;
        juce::ToggleButton particleDebugOverlayToggle { "Show particle count (debug)" };
        juce::TextButton particleClearButton { "Clear particles" };
        juce::Label particleBindingLabel;
        juce::ComboBox particleBindingCombo;
        juce::Label particleEmitModeLabel;
        juce::ComboBox particleEmitModeCombo;
        juce::Label particleEmissionLabel;
        juce::Slider particleEmissionSlider;
        juce::Label particleSpawnJitterLabel;
        juce::Slider particleSpawnJitterSlider;
        juce::Label particleInitVelXLabel;
        juce::Slider particleInitVelXSlider;
        juce::Label particleInitVelYLabel;
        juce::Slider particleInitVelYSlider;
        juce::Label particleInitVelZLabel;
        juce::Slider particleInitVelZSlider;
        juce::Label particleVelRandomLabel;
        juce::Slider particleVelRandomSlider;
        juce::Label particleLifespanLabel;
        juce::Slider particleLifespanSlider;
        juce::Label particleLifespanRandomLabel;
        juce::Slider particleLifespanRandomSlider;
        juce::Label particleSizeLabel;
        juce::Slider particleSizeSlider;
        /** Dual-thumb size scale range at spawn (min / max arrows). */
        juce::Label particleSizeRandomLabel;
        juce::Slider particleSizeRandomSlider;
        juce::Label particleSizeRandomMinReadout, particleSizeRandomMaxReadout;
        juce::ToggleButton particleEmissiveToggle { "Unlit emissive only" };
        juce::Label particleEmissiveStrLabel;
        juce::Slider particleEmissiveStrSlider;
        juce::Label particleRoughLabel;
        juce::Slider particleRoughSlider;
        juce::Label particleMetalLabel;
        juce::Slider particleMetalSlider;
        juce::Label particleSpecLabel;
        juce::Slider particleSpecSlider;

        juce::Label particleMeshLabel;
        juce::ComboBox particleMeshCombo;
        juce::Label particleInitRotXLabel;
        juce::Slider particleInitRotXSlider;
        juce::Label particleInitRotYLabel;
        juce::Slider particleInitRotYSlider;
        juce::Label particleInitRotZLabel;
        juce::Slider particleInitRotZSlider;
        juce::Label particleInitRotRndLabel;
        juce::Slider particleInitRotRndSlider;

        juce::ToggleButton particleForcesToggle { "Enable forces" };
        juce::ToggleButton particleWaterfallLockToggle { "Waterfall lock X" };
        std::unique_ptr<ParticleForceStackComponent> particleForceStack;

        juce::Label particleModLabel;
        juce::Label particleModHintLabel;
        juce::Label particleModHdrOn, particleModHdrSrc, particleModHdrThr;
        juce::Label particleModHdrDst, particleModHdrOp, particleModHdrCurve;
        juce::Label particleModHdrRange, particleModHdrInv, particleModHdrAmt;
        struct ParticleModRow
        {
            juce::ToggleButton enable { "On" };
            juce::ComboBox source;
            juce::ToggleButton thresholdToggle { "Thr" };
            juce::Slider thresholdSlider;
            RotaryImageKnob1 attackKnob;
            RotaryImageKnob1 releaseKnob;
            juce::ComboBox dest;
            juce::ComboBox op;
            ParticleModCurveEditor curve;
            /** Single dual-thumb range (min←→max arrows), not two separate sliders. */
            juce::Slider rangeSlider;
            juce::Label rangeMinReadout, rangeMaxReadout; // 3 d.p. labels beside the bar
            juce::ToggleButton invertToggle { "Inv" };
            juce::Slider amount;
            juce::Slider constant;
        };
        std::array<ParticleModRow, kParticleModSlotCount> particleModRows;

        juce::Label particleSourcesLabel;
        struct RandomSourceRow
        {
            juce::Label title;
            juce::Label dimLabel;
            juce::ComboBox dimCombo;
            juce::Label modeLabel;
            juce::ComboBox modeCombo;
            juce::Label minLabel;
            juce::Slider minSlider;
            juce::Label maxLabel;
            juce::Slider maxSlider;
            juce::Label smoothLabel;
            juce::Slider smoothSlider;
        };
        std::array<RandomSourceRow, kParticleRandomSourceCount> particleRandomRows;

        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spectrogram3DSettingsComponent)
};
