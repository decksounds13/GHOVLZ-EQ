#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/ColourSwatchEditor.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../../Spectrogram3DComponent.h"
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
    /** Sync Look toggles from Main before Menu measures scroll height. */
    void syncFromMain() { content.syncControlsFromMain(); }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

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
        void syncGradientFromBank();

    private:
        void styleSlider (juce::Slider& slider);
        void styleLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleCombo (juce::ComboBox& combo);
        void styleSaveDefaultButton (juce::TextButton& button);
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);
        void layoutToggle (juce::Rectangle<int>& area, juce::ToggleButton& toggle);
        void layoutColourEditor (juce::Rectangle<int>& area, ColourSwatchEditor& editor);
        void setLookChildVisible (juce::Component& c, bool vis);
        void updateLookDevVisibility();
        void requestParentRelayout();
        void syncControlsFromMain();
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

        juce::Label titleLabel;

        juce::ToggleButton enable3DToggle { "Enable 3D view (expanded / Scope)" };
        juce::ToggleButton enhancedFreq3DToggle { "Enhanced Frequency (3D)" };
        std::unique_ptr<ButtonAttachment> enhancedFreq3DAttachment;
        juce::Label meshQualityLabel;
        juce::ComboBox meshQualityCombo;
        juce::Label meshHeightLabel;
        juce::Slider meshHeightSlider;
        juce::Label freqMeshBiasLabel;
        juce::Slider freqMeshBiasSlider;
        juce::Label msaaLabel;
        juce::ComboBox msaaCombo;
        juce::ToggleButton transparentBgToggle { "Soft background (like Osc/Gon)" };
        juce::ToggleButton reverseFreqAxisToggle { "Reverse frequency axis" };
        juce::ToggleButton closedMeshToggle { "Closed mesh (solid)" };
        juce::Label softAngleLabel;
        juce::Slider softAngleSlider;
        juce::TextButton resetCameraButton { "Reset 3D Camera" };

        juce::Label lookLabel;
        juce::ToggleButton audioLevelToggle { "Audio level affects" };
        juce::Label audioLevelTargetLabel;
        juce::ComboBox audioLevelTargetCombo;
        juce::Label audioLevelRangeLabel;
        juce::Slider audioLevelRangeSlider;
        juce::Label audioLevelRangeValueLabel;
        juce::Label audioLevelHpLabel;
        juce::Slider audioLevelHpSlider;
        juce::Label audioLevelLpLabel;
        juce::Slider audioLevelLpSlider;
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

        juce::ToggleButton dofToggle { "Depth of field" };
        juce::Label dofFocusLabel;
        juce::Slider dofFocusSlider;
        juce::Label dofApertureLabel;
        juce::Slider dofApertureSlider;
        juce::Label dofQualityLabel;
        juce::ComboBox dofQualityCombo;
        juce::Label dofBlurScaleLabel;
        juce::Slider dofBlurScaleSlider;
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

        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spectrogram3DSettingsComponent)
};
