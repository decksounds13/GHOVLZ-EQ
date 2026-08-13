#pragma once

#include <JuceHeader.h>
#include "../../ColourRamp/ColourRampBank.h"
#include "../../ColourRamp/GradientStripEditor.h"
#include "../SharedResources.h"
#include "CustomScrollBar.h"

class GradientsSettingsComponent : public juce::Component,
                                   private juce::ChangeListener
{
public:
    GradientsSettingsComponent (SharedResources& resources, ColourRampBank& bank);
    ~GradientsSettingsComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void syncFromBank();

    class Content : public juce::Component
    {
    public:
        Content (SharedResources& resources, ColourRampBank& bank);
        void resized() override;
        int getPreferredHeight() const;

        juce::Label fftLabel, specLabel, spec3DLabel;
        juce::Label preFillLabel, preCurveLabel, fillLabel, curveLabel, holdFillLabel, holdCurveLabel;
        juce::Label eqCurveLabel, eqSumFillLabel, eqBandCurveLabel, eqBandFillLabel;
        GradientStripEditor fftEditor, specEditor, spec3DEditor;
        GradientStripEditor preFillEditor, preCurveEditor, fillEditor, curveEditor;
        GradientStripEditor holdFillEditor, holdCurveEditor;
        GradientStripEditor eqCurveEditor, eqSumFillEditor, eqBandCurveEditor, eqBandFillEditor;
        ColourRampBank& bank;
    };

    SharedResources& sharedResources;
    ColourRampBank& bank;
    juce::Viewport viewport;
    Content content;
    std::unique_ptr<CustomScrollBar> customScrollBar;
    void syncScrollBarColours();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GradientsSettingsComponent)
};
