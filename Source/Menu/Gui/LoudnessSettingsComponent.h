#pragma once

#include <JuceHeader.h>

#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"
#include "CustomScrollBar.h"

class LoudnessSettingsComponent : public juce::Component
{
public:
    LoudnessSettingsComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
    ~LoudnessSettingsComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Content : public juce::Component
    {
    public:
        Content (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
        ~Content() override;

        void resized() override;
        int getPreferredHeight() const;

    private:
        void styleLabel (juce::Label& label);
        void styleCombo (juce::ComboBox& combo);
        void syncTargetComboFromParam();
        void applyTargetFromCombo();

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ComboBoxLookAndFeel comboLookAndFeel;

        juce::Label titleLabel;
        juce::Label targetLabel;
        juce::ComboBox targetCombo;
        juce::Label resetNoteLabel;
    };

    SharedResources& sharedResources;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<CustomScrollBar> customScrollBar;

    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LoudnessSettingsComponent)
};
