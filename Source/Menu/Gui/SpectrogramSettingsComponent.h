#pragma once

#include <JuceHeader.h>

#include "../../ComboBoxLookAndFeel.h"
#include "../../SpectrogramComponent.h"
#include "../SharedResources.h"
#include "CustomScrollBar.h"

/** Colour-scheme combo: ramp swatch + name (matches Gradients Load preset rows). */
class ColourSchemeComboLookAndFeel : public ComboBoxLookAndFeel
{
public:
    void getIdealPopupMenuItemSize (const juce::String& text, bool isSeparator,
                                    int standardMenuItemHeight,
                                    int& idealWidth, int& idealHeight) override;

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu,
                            const juce::String& text, const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColour) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    static SpectrogramComponent::ColourScheme schemeFromName (const juce::String& name) noexcept;
    static void paintSchemeSwatch (juce::Graphics& g, juce::Rectangle<float> swatch,
                                   SpectrogramComponent::ColourScheme scheme);
};

class SpectrogramSettingsComponent : public juce::Component
{
public:
    SpectrogramSettingsComponent (SharedResources& resources, juce::AudioProcessorValueTreeState& state);
    ~SpectrogramSettingsComponent() override;

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
        void styleCombo (juce::ComboBox& combo);
        void styleSaveDefaultButton (juce::TextButton& button);
        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ComboBoxLookAndFeel comboLookAndFeel;
        ColourSchemeComboLookAndFeel colourSchemeLookAndFeel;

        juce::Label titleLabel;
        juce::TextButton saveDefaultButton { "Save Default" };

        juce::Label lookSectionLabel;
        juce::Label colourSchemeLabel;
        juce::ComboBox colourSchemeCombo;
        std::unique_ptr<ComboBoxAttachment> colourSchemeAttachment;

        juce::Label brightnessLabel;
        juce::Slider brightnessSlider;
        std::unique_ptr<SliderAttachment> brightnessAttachment;

        juce::Label behaviourSectionLabel;
        juce::Label fftSizeLabel;
        juce::ComboBox fftSizeCombo;
        std::unique_ptr<ComboBoxAttachment> fftSizeAttachment;

        juce::Label displayResLabel;
        juce::ComboBox displayResCombo;
        std::unique_ptr<ComboBoxAttachment> displayResAttachment;

        juce::Label channelLabel;
        juce::ComboBox channelCombo;
        std::unique_ptr<ComboBoxAttachment> channelAttachment;

        juce::Label speedLabel;
        juce::Slider speedSlider;
        std::unique_ptr<SliderAttachment> speedAttachment;

        juce::Label minDbLabel;
        juce::Slider minDbSlider;
        std::unique_ptr<SliderAttachment> minDbAttachment;

        juce::Label maxDbLabel;
        juce::Slider maxDbSlider;
        std::unique_ptr<SliderAttachment> maxDbAttachment;

        juce::Label smoothLabel;
        juce::Slider smoothSlider;
        std::unique_ptr<SliderAttachment> smoothAttachment;

        juce::Label softenLabel;
        juce::Slider softenSlider;
        std::unique_ptr<SliderAttachment> softenAttachment;

        juce::ToggleButton logFreqToggle { "Log Frequency" };
        std::unique_ptr<ButtonAttachment> logFreqAttachment;

        juce::ToggleButton enhancedFreqToggle { "Enhanced Frequency" };
        std::unique_ptr<ButtonAttachment> enhancedFreqAttachment;

        juce::Label enhancedStrengthLabel;
        juce::Slider enhancedStrengthSlider;
        std::unique_ptr<SliderAttachment> enhancedStrengthAttachment;

        juce::Label enhancedLfDetailLabel;
        juce::ComboBox enhancedLfDetailCombo;
        std::unique_ptr<ComboBoxAttachment> enhancedLfDetailAttachment;

        juce::Label enhancedCrossoverLabel;
        juce::Slider enhancedCrossoverSlider;
        std::unique_ptr<SliderAttachment> enhancedCrossoverAttachment;

        juce::ToggleButton freezeToggle { "Freeze" };
        std::unique_ptr<ButtonAttachment> freezeAttachment;
    };

    SharedResources& sharedResources;
    Content content;
    juce::Viewport viewport;
    std::unique_ptr<CustomScrollBar> customScrollBar;

    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrogramSettingsComponent)
};
