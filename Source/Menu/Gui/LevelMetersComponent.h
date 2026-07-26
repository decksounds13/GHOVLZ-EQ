#pragma once

#include <JuceHeader.h>

#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"
#include "CustomScrollBar.h"

class LevelMetersComponent : public juce::Component
{
public:
    LevelMetersComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
    ~LevelMetersComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    class Content : public juce::Component
    {
    public:
        Content (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
        ~Content() override;

        void resized() override;
        int getPreferredHeight() const;

    private:
        void styleSlider (juce::Slider& slider);
        void styleLabel (juce::Label& label);
        void styleSaveDefaultButton (juce::TextButton& button);
        void styleModeCombo (juce::ComboBox& combo);
        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;

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
    };

    SharedResources& sharedResources;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<CustomScrollBar> customScrollBar;

    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LevelMetersComponent)
};
