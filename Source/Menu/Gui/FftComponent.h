#pragma once

#include <JuceHeader.h>

#include "../SharedResources.h"
#include "CustomScrollBar.h"

class FftComponent : public juce::Component
{
public:
    FftComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
    ~FftComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    class Content : public juce::Component,
                    private juce::AudioProcessorValueTreeState::Listener
    {
    public:
        Content (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
        ~Content() override;

        void resized() override;
        int getPreferredHeight() const;

    private:
        void parameterChanged (const juce::String& parameterID, float newValue) override;
        void syncShowBinsToggleFromParam();
        void applyShowBinsToggle();
        void styleSlider (juce::Slider& slider);
        void styleLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleSaveDefaultButton (juce::TextButton& button);
        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;

        juce::Label titleLabel;
        juce::TextButton saveDefaultButton { "Save Default" };

        juce::ToggleButton showBarsToggle { "Show Bars" };
        std::unique_ptr<ButtonAttachment> showBarsAttachment;

        juce::ToggleButton showBinsToggle { "Show Bins" };

        juce::ToggleButton fullHeightToggle { "Full Height Bars" };
        std::unique_ptr<ButtonAttachment> fullHeightAttachment;

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
    };

    SharedResources& sharedResources;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<CustomScrollBar> customScrollBar;

    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FftComponent)
};
