#include "GradientsSettingsComponent.h"

namespace
{
    constexpr int kScrollBarWidth = 11;
}

GradientsSettingsComponent::Content::Content (SharedResources& resources, ColourRampBank& b)
    : fftEditor (resources, GradientStripEditor::ModeFamily::intensity, &b.getPresets()),
      specEditor (resources, GradientStripEditor::ModeFamily::intensity, &b.getPresets()),
      spec3DEditor (resources, GradientStripEditor::ModeFamily::intensity, &b.getPresets()),
      fillEditor (resources, GradientStripEditor::ModeFamily::spatial, &b.getPresets()),
      bank (b)
{
    fftLabel.setText ("FFT Bars", juce::dontSendNotification);
    specLabel.setText ("Spectrogram (2D)", juce::dontSendNotification);
    spec3DLabel.setText ("Spectrogram 3D", juce::dontSendNotification);
    fillLabel.setText ("Spectrum Fill", juce::dontSendNotification);

    for (auto* l : { &fftLabel, &specLabel, &spec3DLabel, &fillLabel })
    {
        l->setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
        l->setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
        addAndMakeVisible (l);
    }

    addAndMakeVisible (fftEditor);
    addAndMakeVisible (specEditor);
    addAndMakeVisible (spec3DEditor);
    addAndMakeVisible (fillEditor);

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
    wire (fillEditor, ColourRampBank::Target::spectrumFill);
}

int GradientsSettingsComponent::Content::getPreferredHeight() const
{
    return 16
           + 20 + fftEditor.getPreferredHeight() + 12
           + 20 + specEditor.getPreferredHeight() + 12
           + 20 + spec3DEditor.getPreferredHeight() + 12
           + 20 + fillEditor.getPreferredHeight() + 14;
}

void GradientsSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (12, 10);

    fftLabel.setBounds (area.removeFromTop (20));
    fftEditor.setBounds (area.removeFromTop (fftEditor.getPreferredHeight()));
    area.removeFromTop (12);
    specLabel.setBounds (area.removeFromTop (20));
    specEditor.setBounds (area.removeFromTop (specEditor.getPreferredHeight()));
    area.removeFromTop (12);
    spec3DLabel.setBounds (area.removeFromTop (20));
    spec3DEditor.setBounds (area.removeFromTop (spec3DEditor.getPreferredHeight()));
    area.removeFromTop (12);
    fillLabel.setBounds (area.removeFromTop (20));
    fillEditor.setBounds (area.removeFromTop (fillEditor.getPreferredHeight()));
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
    content.fillEditor.setRamp (&bank.get (ColourRampBank::Target::spectrumFill));
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
