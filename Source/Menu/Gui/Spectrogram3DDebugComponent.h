#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/ColourSwatchEditor.h"
#include "../../Spectrogram3DComponent.h"
#include "../SharedResources.h"
#include "SettingsSection.h"

/** Spec3D lookdev: debug sphere / gizmo (cast-shadow settings live on 3D Spectrogram Look). */
class Spectrogram3DDebugComponent : public juce::Component
{
public:
    Spectrogram3DDebugComponent (SharedResources& resources,
                                 juce::AudioProcessorValueTreeState& state,
                                 ColourRampBank& colourRamps);
    ~Spectrogram3DDebugComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    int getPreferredContentHeight() const { return content.getPreferredHeight(); }
    void syncFromMain() { content.syncControlsFromMain(); }
    void syncDebugSphereFromMain() { content.syncDebugSphereFromMain(); }

private:
    class Content : public juce::Component
    {
    public:
        Content (SharedResources& resources,
                 juce::AudioProcessorValueTreeState& state,
                 ColourRampBank& colourRamps);
        ~Content() override;

        void resized() override;
        int getPreferredHeight() const;
        void syncControlsFromMain();
        void syncDebugSphereFromMain();

    private:
        void styleSlider (juce::Slider& slider);
        void styleLabel (juce::Label& label);
        void styleToggle (juce::ToggleButton& toggle);
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutToggle (juce::Rectangle<int>& area, juce::ToggleButton& toggle);
        void layoutColourEditor (juce::Rectangle<int>& area, ColourSwatchEditor& editor);
        void applyControlsToMain();
        void wireColourEditor (ColourSwatchEditor& editor);

        friend class Spectrogram3DDebugComponent;

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ColourRampBank& colourRamps;

        juce::Label titleLabel;
        juce::Label noteLabel;

        juce::ToggleButton sphereToggle { "Debug sphere" };
        juce::Label sphereSizeLabel;
        juce::Slider sphereSizeSlider;
        juce::Label sphereXLabel;
        juce::Slider sphereXSlider;
        juce::Label sphereYLabel;
        juce::Slider sphereYSlider;
        juce::Label sphereZLabel;
        juce::Slider sphereZSlider;
        ColourSwatchEditor albedoEditor { "Albedo" };
        juce::Label specularLabel;
        juce::Slider specularSlider;
        juce::Label roughLabel;
        juce::Slider roughSlider;
        juce::Label metalLabel;
        juce::Slider metalSlider;

        void wireSection (SettingsSection& section);
        SettingsSection sphereSection;
    };

    SharedResources& sharedResources;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spectrogram3DDebugComponent)
};
