#pragma once

#include <JuceHeader.h>

#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../../ComboBoxLookAndFeel.h"
#include "../../Visualizer/SpectrumAnalysis.h"
#include "../SharedResources.h"
#include "SettingsSection.h"

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
        void syncRampControlsEnabled();

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

        /** Lattice (default, zero latency) or FFT spectral dynamics. */
        juce::Label spectralMethodLabel;
        juce::ComboBox spectralMethodCombo;
        std::unique_ptr<ComboBoxAttachment> spectralMethodAttachment;

        juce::Label refreshLabel;
        juce::Slider refreshSlider;
        std::unique_ptr<SliderAttachment> refreshAttachment;

        juce::Label avgLabel;
        juce::Slider avgSlider;
        std::unique_ptr<SliderAttachment> avgAttachment;

        juce::Label analysisLabel;
        juce::ComboBox analysisCombo;
        std::unique_ptr<ComboBoxAttachment> analysisAttachment;

        juce::Label octaveSmoothLabel;
        juce::ComboBox octaveSmoothCombo;
        std::unique_ptr<ComboBoxAttachment> octaveSmoothAttachment;

        juce::Label curveSmoothLabel;
        juce::ComboBox curveSmoothCombo;
        std::unique_ptr<ComboBoxAttachment> curveSmoothAttachment;
        ComboBoxLookAndFeel comboLookAndFeel;

        juce::Label channelColoursLabel;

        class ColourSwatch : public juce::Component
        {
        public:
            ColourSwatch (SharedResources& resources,
                          juce::Colour SharedColors::* member,
                          juce::String caption);
            void paint (juce::Graphics& g) override;
            void mouseUp (const juce::MouseEvent& e) override;
            void refresh() { repaint(); }

        private:
            void launchSelector();

            SharedResources& sharedResources;
            juce::Colour SharedColors::* colourMember;
            juce::String caption;
        };

        class ChannelColourRow : public juce::Component
        {
        public:
            ChannelColourRow (SharedResources& resources,
                              const juce::String& name,
                              juce::Colour SharedColors::* lineMember,
                              juce::Colour SharedColors::* fillMember);
            void resized() override;
            void refresh();

        private:
            juce::Label nameLabel;
            ColourSwatch lineSwatch;
            ColourSwatch fillSwatch;
        };

        ChannelColourRow leftColourRow;
        ChannelColourRow rightColourRow;
        ChannelColourRow midColourRow;
        ChannelColourRow sideColourRow;

        juce::ToggleButton multicolorBandFillToggle { "Multicolor Band Fill" };
        std::unique_ptr<ButtonAttachment> multicolorBandFillAttachment;

        /** Power rings + faceplate knob glow arcs use each band's handle colour. */
        juce::ToggleButton bandChromeMatchHandlesToggle { "Match power/glow to band colours" };
        std::unique_ptr<ButtonAttachment> bandChromeMatchHandlesAttachment;

        /** Floor Graph Band / matching faceplate power+glow saturation. */
        juce::ToggleButton bandMinSatEnableToggle { "Band min sat" };
        juce::Label bandMinSatLabel;
        juce::Slider bandMinSatSlider;
        juce::Label bandMinSatPercentLabel;

        juce::ToggleButton showCrosshairToggle { "Show Crosshair" };
        std::unique_ptr<ButtonAttachment> showCrosshairAttachment;

        juce::ToggleButton showEqCurvesToggle { "Show EQ Curves" };
        std::unique_ptr<ButtonAttachment> showEqCurvesAttachment;

        void syncBandMinSatControlsFromShared();
        void applyBandMinSatFromControls();
        void notifyHostSaveUiPrefs();

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

        juce::Label fadeSectionLabel;
        juce::Label preCurveFadeLabel;
        juce::Slider preCurveFadeSlider;
        std::unique_ptr<SliderAttachment> preCurveFadeAttachment;
        juce::Label preFillFadeLabel;
        juce::Slider preFillFadeSlider;
        std::unique_ptr<SliderAttachment> preFillFadeAttachment;
        juce::Label postCurveFadeLabel;
        juce::Slider postCurveFadeSlider;
        std::unique_ptr<SliderAttachment> postCurveFadeAttachment;
        juce::Label postFillFadeLabel;
        juce::Slider postFillFadeSlider;
        std::unique_ptr<SliderAttachment> postFillFadeAttachment;
        juce::Label holdCurveFadeLabel;
        juce::Slider holdCurveFadeSlider;
        std::unique_ptr<SliderAttachment> holdCurveFadeAttachment;
        juce::Label holdFillFadeLabel;
        juce::Slider holdFillFadeSlider;
        std::unique_ptr<SliderAttachment> holdFillFadeAttachment;
        juce::Label eqCurveFadeLabel;
        juce::Slider eqCurveFadeSlider;
        std::unique_ptr<SliderAttachment> eqCurveFadeAttachment;
        juce::Label eqFillFadeLabel;
        juce::Slider eqFillFadeSlider;
        std::unique_ptr<SliderAttachment> eqFillFadeAttachment;

        juce::ToggleButton useRampToggle { "Use post fill ramp" };
        std::unique_ptr<ButtonAttachment> useRampAttachment;
        juce::Label gradientLabel;
        GradientStripEditor gradientEditor;

        juce::ToggleButton useCurveRampToggle { "Use post curve ramp" };
        std::unique_ptr<ButtonAttachment> useCurveRampAttachment;
        juce::Label curveGradientLabel;
        GradientStripEditor curveGradientEditor;

        juce::ToggleButton usePreFillRampToggle { "Use pre fill ramp" };
        std::unique_ptr<ButtonAttachment> usePreFillRampAttachment;
        juce::Label preFillGradientLabel;
        GradientStripEditor preFillGradientEditor;

        juce::ToggleButton usePreCurveRampToggle { "Use pre curve ramp" };
        std::unique_ptr<ButtonAttachment> usePreCurveRampAttachment;
        juce::Label preCurveGradientLabel;
        GradientStripEditor preCurveGradientEditor;

        juce::ToggleButton useHoldFillRampToggle { "Use hold fill ramp" };
        std::unique_ptr<ButtonAttachment> useHoldFillRampAttachment;
        juce::Label holdFillGradientLabel;
        GradientStripEditor holdFillGradientEditor;

        juce::ToggleButton useHoldCurveRampToggle { "Use hold curve ramp" };
        std::unique_ptr<ButtonAttachment> useHoldCurveRampAttachment;
        juce::Label holdCurveGradientLabel;
        GradientStripEditor holdCurveGradientEditor;

        juce::ToggleButton useEqCurveRampToggle { "Use sum curve ramp" };
        std::unique_ptr<ButtonAttachment> useEqCurveRampAttachment;
        juce::Label eqCurveGradientLabel;
        GradientStripEditor eqCurveGradientEditor;

        juce::ToggleButton useEqSumFillRampToggle { "Use sum fill ramp" };
        std::unique_ptr<ButtonAttachment> useEqSumFillRampAttachment;
        juce::Label eqSumFillGradientLabel;
        GradientStripEditor eqSumFillGradientEditor;

        juce::ToggleButton useEqBandCurveRampToggle { "Use band curve ramp" };
        std::unique_ptr<ButtonAttachment> useEqBandCurveRampAttachment;
        juce::Label eqBandCurveGradientLabel;
        GradientStripEditor eqBandCurveGradientEditor;

        juce::ToggleButton useEqBandFillRampToggle { "Use band fill ramp" };
        std::unique_ptr<ButtonAttachment> useEqBandFillRampAttachment;
        juce::Label eqBandFillGradientLabel;
        GradientStripEditor eqBandFillGradientEditor;

        enum { scaleRadioGroup = 0x5c41e };

        void wireSection (SettingsSection& section);
        SettingsSection analysisSection;
        SettingsSection displaySection;
        SettingsSection strokeSection;
        SettingsSection glowSection;
        SettingsSection rampsSection;
    };

    SharedResources& sharedResources;
    ColourRampBank& colourRamps;
    Content content;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumComponent)
};
