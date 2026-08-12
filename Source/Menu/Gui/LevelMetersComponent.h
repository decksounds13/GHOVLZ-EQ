#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"

class LevelMetersComponent : public juce::Component,
                             private juce::ChangeListener
{
public:
    LevelMetersComponent (SharedResources& resources,
                          juce::AudioProcessorValueTreeState& state,
                          ColourRampBank& colourRamps);
    ~LevelMetersComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredContentHeight() const { return content.getPreferredHeight(); }

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
        void syncGradientFromBank();

    private:
        void styleSlider (juce::Slider& slider);
        void styleLabel (juce::Label& label);
        void styleSectionLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleSaveDefaultButton (juce::TextButton& button);
        void styleModeCombo (juce::ComboBox& combo);
        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);
        void layoutToggle (juce::Rectangle<int>& area, juce::ToggleButton& toggle);
        void updateGlowVisibility();
        void requestParentRelayout();

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ColourRampBank& colourRamps;

        juce::Label titleLabel;
        juce::TextButton saveDefaultButton { "Save Default" };

        juce::Label modeLabel;
        juce::ComboBox modeCombo;
        std::unique_ptr<ComboBoxAttachment> modeAttachment;
        ComboBoxLookAndFeel modeLookAndFeel;

        juce::Label channelModeLabel;
        juce::ComboBox channelModeCombo;
        std::unique_ptr<ComboBoxAttachment> channelModeAttachment;
        ComboBoxLookAndFeel channelModeLookAndFeel;

        juce::Label readoutIntegrationLabel;
        juce::Slider readoutIntegrationSlider;
        std::unique_ptr<SliderAttachment> readoutIntegrationAttachment;

        juce::Label fallLabel;
        juce::Slider fallSlider;
        std::unique_ptr<SliderAttachment> fallAttachment;

        juce::Label peakHoldLabel;
        juce::Slider peakHoldSlider;
        std::unique_ptr<SliderAttachment> peakHoldAttachment;

        juce::Label clipHoldLabel;
        juce::Slider clipHoldSlider;
        std::unique_ptr<SliderAttachment> clipHoldAttachment;

        juce::Label clipThresholdLabel;
        juce::Slider clipThresholdSlider;
        std::unique_ptr<SliderAttachment> clipThresholdAttachment;

        juce::Label peakRampSectionLabel;
        juce::Label peakRampLabel;
        GradientStripEditor peakRampEditor;

        juce::ToggleButton peakGlowToggle { "Peak Glow" };
        std::unique_ptr<ButtonAttachment> peakGlowAttachment;
        juce::Label peakGlowThresholdLabel;
        juce::Slider peakGlowThresholdSlider;
        std::unique_ptr<SliderAttachment> peakGlowThresholdAttachment;
        juce::Label peakGlowRadiusLabel;
        juce::Slider peakGlowRadiusSlider;
        std::unique_ptr<SliderAttachment> peakGlowRadiusAttachment;
        juce::Label peakGlowSpreadLabel;
        juce::Slider peakGlowSpreadSlider;
        std::unique_ptr<SliderAttachment> peakGlowSpreadAttachment;
        juce::Label peakGlowOpacityLabel;
        juce::Slider peakGlowOpacitySlider;
        std::unique_ptr<SliderAttachment> peakGlowOpacityAttachment;

        juce::Label rmsRampSectionLabel;
        juce::Label rmsRampLabel;
        GradientStripEditor rmsRampEditor;

        juce::ToggleButton rmsGlowToggle { "RMS Glow" };
        std::unique_ptr<ButtonAttachment> rmsGlowAttachment;
        juce::Label rmsGlowThresholdLabel;
        juce::Slider rmsGlowThresholdSlider;
        std::unique_ptr<SliderAttachment> rmsGlowThresholdAttachment;
        juce::Label rmsGlowRadiusLabel;
        juce::Slider rmsGlowRadiusSlider;
        std::unique_ptr<SliderAttachment> rmsGlowRadiusAttachment;
        juce::Label rmsGlowSpreadLabel;
        juce::Slider rmsGlowSpreadSlider;
        std::unique_ptr<SliderAttachment> rmsGlowSpreadAttachment;
        juce::Label rmsGlowOpacityLabel;
        juce::Slider rmsGlowOpacitySlider;
        std::unique_ptr<SliderAttachment> rmsGlowOpacityAttachment;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMetersComponent)
};
