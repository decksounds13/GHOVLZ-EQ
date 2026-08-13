#include "GradientsSettingsComponent.h"

namespace
{
    constexpr int kScrollBarWidth = 11;

    void placeBlock (juce::Rectangle<int>& area, juce::Label& label, GradientStripEditor& editor)
    {
        label.setBounds (area.removeFromTop (20));
        editor.setBounds (area.removeFromTop (editor.getPreferredHeight()));
        area.removeFromTop (12);
    }
}

GradientsSettingsComponent::Content::Content (SharedResources& resources, ColourRampBank& b)
    : fftEditor (resources, GradientStripEditor::ModeFamily::intensity, &b.getPresets()),
      specEditor (resources, GradientStripEditor::ModeFamily::intensity, &b.getPresets()),
      spec3DEditor (resources, GradientStripEditor::ModeFamily::intensity, &b.getPresets()),
      preFillEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      preCurveEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      fillEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      curveEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      holdFillEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      holdCurveEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      eqCurveEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      eqSumFillEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      eqBandCurveEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      eqBandFillEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      bank (b)
{
    fftLabel.setText ("FFT Bars", juce::dontSendNotification);
    specLabel.setText ("Spectrogram (2D)", juce::dontSendNotification);
    spec3DLabel.setText ("Spectrogram 3D", juce::dontSendNotification);
    preFillLabel.setText ("Pre Fill", juce::dontSendNotification);
    preCurveLabel.setText ("Pre Curve", juce::dontSendNotification);
    fillLabel.setText ("Post Fill", juce::dontSendNotification);
    curveLabel.setText ("Post Curve", juce::dontSendNotification);
    holdFillLabel.setText ("Hold Fill", juce::dontSendNotification);
    holdCurveLabel.setText ("Hold Curve", juce::dontSendNotification);
    eqCurveLabel.setText ("Sum Curve", juce::dontSendNotification);
    eqSumFillLabel.setText ("Sum Fill", juce::dontSendNotification);
    eqBandCurveLabel.setText ("Band Curve", juce::dontSendNotification);
    eqBandFillLabel.setText ("Band Fill", juce::dontSendNotification);

    for (auto* l : { &fftLabel, &specLabel, &spec3DLabel,
                     &preFillLabel, &preCurveLabel, &fillLabel, &curveLabel,
                     &holdFillLabel, &holdCurveLabel,
                     &eqCurveLabel, &eqSumFillLabel, &eqBandCurveLabel, &eqBandFillLabel })
    {
        l->setFont (SharedResources::uiFont (15.0f));
        l->setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
        addAndMakeVisible (l);
    }

    for (auto* e : { &fftEditor, &specEditor, &spec3DEditor,
                     &preFillEditor, &preCurveEditor, &fillEditor, &curveEditor,
                     &holdFillEditor, &holdCurveEditor,
                     &eqCurveEditor, &eqSumFillEditor, &eqBandCurveEditor, &eqBandFillEditor })
        addAndMakeVisible (e);

    auto wire = [this] (GradientStripEditor& ed, ColourRampBank::Target t)
    {
        ed.setRamp (&bank.get (t));
        ed.onRampChanged = [this] { bank.notifyEdited(); };
        ed.onRampPreview = [this] { bank.notifyPreview(); };
        ed.onPreferredHeightChanged = [this]
        {
            if (auto* parent = findParentComponentOfClass<GradientsSettingsComponent>())
                parent->resized();
            else
                resized();
        };
    };

    wire (fftEditor, ColourRampBank::Target::fftBars);
    wire (specEditor, ColourRampBank::Target::spectrogram);
    wire (spec3DEditor, ColourRampBank::Target::spectrogram3D);
    wire (preFillEditor, ColourRampBank::Target::spectrumPreFill);
    wire (preCurveEditor, ColourRampBank::Target::spectrumPreCurve);
    wire (fillEditor, ColourRampBank::Target::spectrumFill);
    wire (curveEditor, ColourRampBank::Target::spectrumCurve);
    wire (holdFillEditor, ColourRampBank::Target::spectrumHoldFill);
    wire (holdCurveEditor, ColourRampBank::Target::spectrumHoldCurve);
    wire (eqCurveEditor, ColourRampBank::Target::eqCurve);
    wire (eqSumFillEditor, ColourRampBank::Target::eqSumFill);
    wire (eqBandCurveEditor, ColourRampBank::Target::eqBandCurve);
    wire (eqBandFillEditor, ColourRampBank::Target::eqBandFill);
}

int GradientsSettingsComponent::Content::getPreferredHeight() const
{
    auto block = [] (const GradientStripEditor& e) { return 20 + e.getPreferredHeight() + 12; };
    return 16
           + block (fftEditor) + block (specEditor) + block (spec3DEditor)
           + block (preFillEditor) + block (preCurveEditor)
           + block (fillEditor) + block (curveEditor)
           + block (holdFillEditor) + block (holdCurveEditor)
           + block (eqCurveEditor) + block (eqSumFillEditor)
           + block (eqBandCurveEditor) + block (eqBandFillEditor);
}

void GradientsSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (12, 10);
    placeBlock (area, fftLabel, fftEditor);
    placeBlock (area, specLabel, specEditor);
    placeBlock (area, spec3DLabel, spec3DEditor);
    placeBlock (area, preFillLabel, preFillEditor);
    placeBlock (area, preCurveLabel, preCurveEditor);
    placeBlock (area, fillLabel, fillEditor);
    placeBlock (area, curveLabel, curveEditor);
    placeBlock (area, holdFillLabel, holdFillEditor);
    placeBlock (area, holdCurveLabel, holdCurveEditor);
    placeBlock (area, eqCurveLabel, eqCurveEditor);
    placeBlock (area, eqSumFillLabel, eqSumFillEditor);
    placeBlock (area, eqBandCurveLabel, eqBandCurveEditor);
    placeBlock (area, eqBandFillLabel, eqBandFillEditor);
}

GradientsSettingsComponent::GradientsSettingsComponent (SharedResources& resources, ColourRampBank& b)
    : sharedResources (resources),
      bank (b),
      content (resources, b)
{
    bank.addChangeListener (this);

    viewport.setViewedComponent (&content, false);
    viewport.setScrollBarsShown (false, false);
    addAndMakeVisible (viewport);

    customScrollBar = std::make_unique<CustomScrollBar> (viewport.getVerticalScrollBar());
    addAndMakeVisible (*customScrollBar);
    syncScrollBarColours();
}

GradientsSettingsComponent::~GradientsSettingsComponent()
{
    bank.removeChangeListener (this);
    viewport.setViewedComponent (nullptr, false);
}

void GradientsSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    syncFromBank();
}

void GradientsSettingsComponent::syncFromBank()
{
    content.fftEditor.setRamp (&bank.get (ColourRampBank::Target::fftBars));
    content.specEditor.setRamp (&bank.get (ColourRampBank::Target::spectrogram));
    content.spec3DEditor.setRamp (&bank.get (ColourRampBank::Target::spectrogram3D));
    content.preFillEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumPreFill));
    content.preCurveEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumPreCurve));
    content.fillEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumFill));
    content.curveEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumCurve));
    content.holdFillEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumHoldFill));
    content.holdCurveEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumHoldCurve));
    content.eqCurveEditor.setRamp (&bank.get (ColourRampBank::Target::eqCurve));
    content.eqSumFillEditor.setRamp (&bank.get (ColourRampBank::Target::eqSumFill));
    content.eqBandCurveEditor.setRamp (&bank.get (ColourRampBank::Target::eqBandCurve));
    content.eqBandFillEditor.setRamp (&bank.get (ColourRampBank::Target::eqBandFill));
    content.repaint();
}

void GradientsSettingsComponent::syncScrollBarColours()
{
    if (customScrollBar == nullptr)
        return;
    customScrollBar->setTrackBackgroundColour (sharedResources.sharedColors.menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour (sharedResources.sharedColors.menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour (sharedResources.sharedColors.menuScrollBarOutlineColor1);
}

void GradientsSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void GradientsSettingsComponent::resized()
{
    syncScrollBarColours();
    auto bounds = getLocalBounds();
    if (customScrollBar != nullptr)
        customScrollBar->setBounds (bounds.removeFromRight (kScrollBarWidth));
    viewport.setBounds (bounds);
    content.setSize (viewport.getWidth(), juce::jmax (viewport.getHeight(), content.getPreferredHeight()));
    content.resized();
    if (customScrollBar != nullptr)
        customScrollBar->updateThumbPosition();
}
