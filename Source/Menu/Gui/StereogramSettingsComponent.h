#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"
#include "CustomScrollBar.h"

class StereogramSettingsComponent : public juce::Component,
                                    private juce::ChangeListener
{
public:
    StereogramSettingsComponent (SharedResources& resources,
                                 juce::AudioProcessorValueTreeState& state,
                                 ColourRampBank& colourRamps);
    ~StereogramSettingsComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

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

        juce::Label dotSizeLabel;
        juce::Slider dotSizeSlider;
        std::unique_ptr<SliderAttachment> dotSizeAttachment;

        juce::Label densityLabel;
        juce::Slider densitySlider;
        std::unique_ptr<SliderAttachment> densityAttachment;

        juce::Label fadeLabel;
        juce::Slider fadeSlider;
        std::unique_ptr<SliderAttachment> fadeAttachment;

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

        juce::ToggleButton useRampToggle { "Use colour ramp" };
        std::unique_ptr<ButtonAttachment> useRampAttachment;
        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<CustomScrollBar> customScrollBar;

    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StereogramSettingsComponent)
};
