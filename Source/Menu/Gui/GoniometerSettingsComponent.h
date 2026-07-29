#pragma once

#include <JuceHeader.h>

#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"
#include "CustomScrollBar.h"

class GoniometerSettingsComponent : public juce::Component
{
public:
    GoniometerSettingsComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
    ~GoniometerSettingsComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
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
        void styleSectionLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void styleQualityCombo (juce::ComboBox& combo);
        void styleSaveDefaultButton (juce::TextButton& button);
        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ComboBoxLookAndFeel qualityLookAndFeel;

        juce::Label titleLabel;
        juce::TextButton saveDefaultButton { "Save Default" };

        juce::Label qualityLabel;
        juce::ComboBox qualityCombo;
        std::unique_ptr<ComboBoxAttachment> qualityAttachment;

        juce::Label lineOpacityLabel;
        juce::Slider lineOpacitySlider;
        std::unique_ptr<SliderAttachment> lineOpacityAttachment;

        juce::Label compactSectionLabel;
        juce::Label compactLineWidthLabel;
        juce::Slider compactLineWidthSlider;
        std::unique_ptr<SliderAttachment> compactLineWidthAttachment;
        juce::ToggleButton compactGlowToggle { "Glow" };
        std::unique_ptr<ButtonAttachment> compactGlowAttachment;
        juce::Label compactGlowRadiusLabel;
        juce::Slider compactGlowRadiusSlider;
        std::unique_ptr<SliderAttachment> compactGlowRadiusAttachment;
        juce::Label compactGlowSpreadLabel;
        juce::Slider compactGlowSpreadSlider;
        std::unique_ptr<SliderAttachment> compactGlowSpreadAttachment;
        juce::Label compactGlowOpacityLabel;
        juce::Slider compactGlowOpacitySlider;
        std::unique_ptr<SliderAttachment> compactGlowOpacityAttachment;

        juce::Label expandedSectionLabel;
        juce::Label expandedLineWidthLabel;
        juce::Slider expandedLineWidthSlider;
        std::unique_ptr<SliderAttachment> expandedLineWidthAttachment;
        juce::ToggleButton expandedGlowToggle { "Glow" };
        std::unique_ptr<ButtonAttachment> expandedGlowAttachment;
        juce::Label expandedGlowRadiusLabel;
        juce::Slider expandedGlowRadiusSlider;
        std::unique_ptr<SliderAttachment> expandedGlowRadiusAttachment;
        juce::Label expandedGlowSpreadLabel;
        juce::Slider expandedGlowSpreadSlider;
        std::unique_ptr<SliderAttachment> expandedGlowSpreadAttachment;
        juce::Label expandedGlowOpacityLabel;
        juce::Slider expandedGlowOpacitySlider;
        std::unique_ptr<SliderAttachment> expandedGlowOpacityAttachment;
    };

    SharedResources& sharedResources;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<CustomScrollBar> customScrollBar;

    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GoniometerSettingsComponent)
};
