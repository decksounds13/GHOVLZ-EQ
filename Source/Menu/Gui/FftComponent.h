#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"

class FftComponent : public juce::Component,
                     private juce::ChangeListener
{
public:
    FftComponent (SharedResources& resources,
                  juce::AudioProcessorValueTreeState& state,
                  ColourRampBank& colourRamps);
    ~FftComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredContentHeight() const { return content.getPreferredHeight(); }

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    class Content : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener
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
        void parameterChanged (const juce::String& parameterID, float newValue) override;
        void syncShowBinsToggleFromParam();
        void applyShowBinsToggle();
        void styleSlider (juce::Slider& slider);
        void styleLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleSettingsCombo (juce::ComboBox& combo);
        void styleSaveDefaultButton (juce::TextButton& button);
        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ColourRampBank& colourRamps;
        ComboBoxLookAndFeel comboLookAndFeel;

        juce::Label titleLabel;
        juce::TextButton saveDefaultButton { "Save Default" };

        juce::ToggleButton showBarsToggle { "Show Bars" };
        std::unique_ptr<ButtonAttachment> showBarsAttachment;

        juce::ToggleButton showBinsToggle { "Show Bins" };

        juce::ToggleButton fullHeightToggle { "Full Height Bars" };
        std::unique_ptr<ButtonAttachment> fullHeightAttachment;

        juce::Label blockSizeLabel;
        juce::ComboBox blockSizeCombo;
        std::unique_ptr<ComboBoxAttachment> blockSizeAttachment;

        juce::Label refreshLabel;
        juce::Slider refreshSlider;
        std::unique_ptr<SliderAttachment> refreshAttachment;

        juce::Label opacityLabel;
        juce::Slider opacitySlider;
        std::unique_ptr<SliderAttachment> opacityAttachment;

        juce::Label barWidthLabel;
        juce::Slider barWidthSlider;
        std::unique_ptr<SliderAttachment> barWidthAttachment;

        juce::Label intensityLabel;
        juce::Slider intensitySlider;
        std::unique_ptr<SliderAttachment> intensityAttachment;

        juce::Label thresholdLabel;
        juce::Slider thresholdSlider;
        std::unique_ptr<SliderAttachment> thresholdAttachment;

        juce::ToggleButton glowToggle { "Glow" };
        std::unique_ptr<ButtonAttachment> glowAttachment;

        juce::Label glowRadiusLabel;
        juce::Slider glowRadiusSlider;
        std::unique_ptr<SliderAttachment> glowRadiusAttachment;

        juce::Label glowSpreadLabel;
        juce::Slider glowSpreadSlider;
        std::unique_ptr<SliderAttachment> glowSpreadAttachment;

        juce::Label glowOpacityLabel;
        juce::Slider glowOpacitySlider;
        std::unique_ptr<SliderAttachment> glowOpacityAttachment;

        juce::Label glowOffsetXLabel;
        juce::Slider glowOffsetXSlider;
        std::unique_ptr<SliderAttachment> glowOffsetXAttachment;

        juce::Label glowOffsetYLabel;
        juce::Slider glowOffsetYSlider;
        std::unique_ptr<SliderAttachment> glowOffsetYAttachment;

        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FftComponent)
};
