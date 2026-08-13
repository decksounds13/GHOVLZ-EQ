#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../SharedResources.h"
#include "SettingsSection.h"

class HistogramSettingsComponent : public juce::Component,
                                   private juce::ChangeListener
{
public:
    HistogramSettingsComponent (SharedResources& resources,
                                juce::AudioProcessorValueTreeState& state,
                                ColourRampBank& colourRamps);
    ~HistogramSettingsComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredContentHeight() const { return content.getPreferredHeight(); }

private:
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;

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
        void syncRampControlsEnabled();

    private:
        void styleLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleSlider (juce::Slider& slider);
        void styleSectionLabel (juce::Label& label);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ColourRampBank& colourRamps;

        juce::Label titleLabel;

        juce::ToggleButton useRampToggle { "Use colour ramp" };
        std::unique_ptr<ButtonAttachment> useRampAttachment;
        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;

        juce::Label behaviourSectionLabel;
        juce::Label speedLabel;
        juce::Slider speedSlider;
        std::unique_ptr<SliderAttachment> speedAttachment;
        juce::Label lineWidthLabel;
        juce::Slider lineWidthSlider;
        std::unique_ptr<SliderAttachment> lineWidthAttachment;
        juce::Label fillOpacityLabel;
        juce::Slider fillOpacitySlider;
        std::unique_ptr<SliderAttachment> fillOpacityAttachment;
        juce::Label minDbLabel;
        juce::Slider minDbSlider;
        std::unique_ptr<SliderAttachment> minDbAttachment;
        juce::Label maxDbLabel;
        juce::Slider maxDbSlider;
        std::unique_ptr<SliderAttachment> maxDbAttachment;

        juce::Label tracesSectionLabel;
        juce::ToggleButton showLufsToggle { "Show Integrated LUFS" };
        std::unique_ptr<ButtonAttachment> showLufsAttachment;
        juce::ToggleButton showRmsToggle { "Show RMS" };
        std::unique_ptr<ButtonAttachment> showRmsAttachment;
        juce::ToggleButton showTruePeakToggle { "Show True Peak" };
        std::unique_ptr<ButtonAttachment> showTruePeakAttachment;
        juce::ToggleButton freezeToggle { "Freeze" };
        std::unique_ptr<ButtonAttachment> freezeAttachment;

        juce::Label glowSectionLabel;
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

        void wireSection (SettingsSection& section);
        SettingsSection displaySection;
        SettingsSection tracesSection;
        SettingsSection glowSection;
        SettingsSection rampSection;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HistogramSettingsComponent)
};
