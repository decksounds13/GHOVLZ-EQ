#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../SharedResources.h"

class SpectrumComponent : public juce::Component,
                          private juce::ChangeListener
{
public:
    SpectrumComponent (SharedResources& resources,
                       juce::AudioProcessorValueTreeState& state,
                       ColourRampBank& colourRamps);
    ~SpectrumComponent() override;

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
        void styleScaleButton (juce::TextButton& button);
        void styleSaveDefaultButton (juce::TextButton& button);
        void styleSettingsCombo (juce::ComboBox& combo);

        void saveAnalyserDefaults();
        void layoutSliderRow (juce::Rectangle<int>& area, juce::Label& label, juce::Slider& slider);
        void layoutComboRow (juce::Rectangle<int>& area, juce::Label& label, juce::ComboBox& combo);
        void layoutTogglePair (juce::Rectangle<int>& area, juce::ToggleButton& left, juce::ToggleButton& right);

        SharedResources& sharedResources;
        juce::AudioProcessorValueTreeState& treeState;
        ColourRampBank& colourRamps;

        juce::Label titleLabel;
        juce::TextButton saveDefaultButton { "Save Default" };

        juce::ToggleButton showBinsToggle { "Show Bins" };
        juce::ToggleButton enableToggle { "Show Analyser" };
        std::unique_ptr<ButtonAttachment> enableAttachment;

        juce::Label blockSizeLabel;
        juce::ComboBox blockSizeCombo;
        std::unique_ptr<ComboBoxAttachment> blockSizeAttachment;

        juce::Label refreshLabel;
        juce::Slider refreshSlider;
        std::unique_ptr<SliderAttachment> refreshAttachment;

        juce::Label avgLabel;
        juce::Slider avgSlider;
        std::unique_ptr<SliderAttachment> avgAttachment;

        juce::Label curveSmoothLabel;
        juce::ComboBox curveSmoothCombo;
        std::unique_ptr<ComboBoxAttachment> curveSmoothAttachment;
        ComboBoxLookAndFeel comboLookAndFeel;

        juce::ToggleButton multicolorBandFillToggle { "Multicolor Band Fill" };
        std::unique_ptr<ButtonAttachment> multicolorBandFillAttachment;

        juce::ToggleButton showCrosshairToggle { "Show Crosshair" };
        std::unique_ptr<ButtonAttachment> showCrosshairAttachment;

        juce::Label layersLabel;
        juce::ToggleButton preCurveToggle { "Pre Curve" };
        juce::ToggleButton preFillToggle { "Pre Fill" };
        juce::ToggleButton postCurveToggle { "Post Curve" };
        juce::ToggleButton postFillToggle { "Post Fill" };
        juce::ToggleButton holdCurveToggle { "Hold Curve" };
        juce::ToggleButton holdFillToggle { "Hold Fill" };

        std::unique_ptr<ButtonAttachment> preCurveAttachment;
        std::unique_ptr<ButtonAttachment> preFillAttachment;
        std::unique_ptr<ButtonAttachment> postCurveAttachment;
        std::unique_ptr<ButtonAttachment> postFillAttachment;
        std::unique_ptr<ButtonAttachment> holdCurveAttachment;
        std::unique_ptr<ButtonAttachment> holdFillAttachment;

        juce::Label scaleLabel;
        juce::TextButton linButton { "Lin" };
        juce::TextButton logButton { "Log" };
        juce::TextButton stButton { "ST" };
        std::unique_ptr<ButtonAttachment> linAttachment;
        std::unique_ptr<ButtonAttachment> logAttachment;
        std::unique_ptr<ButtonAttachment> stAttachment;

        juce::Label opacityLabel;
        juce::Slider opacitySlider;
        std::unique_ptr<SliderAttachment> opacityAttachment;

        juce::Label fillOpacityLabel;
        juce::Slider fillOpacitySlider;
        std::unique_ptr<SliderAttachment> fillOpacityAttachment;

        juce::Label pathWidthLabel;
        juce::Slider pathWidthSlider;
        std::unique_ptr<SliderAttachment> pathWidthAttachment;

        juce::Label bandLineWidthLabel;
        juce::Slider bandLineWidthSlider;
        std::unique_ptr<SliderAttachment> bandLineWidthAttachment;

        juce::Label sumLineWidthLabel;
        juce::Slider sumLineWidthSlider;
        std::unique_ptr<SliderAttachment> sumLineWidthAttachment;

        juce::ToggleButton sumGlowToggle { "Sum Glow" };
        std::unique_ptr<ButtonAttachment> sumGlowAttachment;

        juce::Label sumGlowRadiusLabel;
        juce::Slider sumGlowRadiusSlider;
        std::unique_ptr<SliderAttachment> sumGlowRadiusAttachment;

        juce::Label sumGlowSpreadLabel;
        juce::Slider sumGlowSpreadSlider;
        std::unique_ptr<SliderAttachment> sumGlowSpreadAttachment;

        juce::Label sumGlowOpacityLabel;
        juce::Slider sumGlowOpacitySlider;
        std::unique_ptr<SliderAttachment> sumGlowOpacityAttachment;

        juce::ToggleButton postGlowToggle { "Post Glow" };
        std::unique_ptr<ButtonAttachment> postGlowAttachment;

        juce::Label spectrumGlowRadiusLabel;
        juce::Slider spectrumGlowRadiusSlider;
        std::unique_ptr<SliderAttachment> spectrumGlowRadiusAttachment;

        juce::Label spectrumGlowSpreadLabel;
        juce::Slider spectrumGlowSpreadSlider;
        std::unique_ptr<SliderAttachment> spectrumGlowSpreadAttachment;

        juce::Label spectrumGlowOpacityLabel;
        juce::Slider spectrumGlowOpacitySlider;
        std::unique_ptr<SliderAttachment> spectrumGlowOpacityAttachment;

        juce::Label holdTimeLabel;
        juce::Slider holdTimeSlider;
        std::unique_ptr<SliderAttachment> holdTimeAttachment;

        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;

        enum { scaleRadioGroup = 0x5c41e };
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumComponent)
};
