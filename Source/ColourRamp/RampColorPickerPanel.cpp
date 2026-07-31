#include "RampColorPickerPanel.h"

namespace
{
    void styleSlider (juce::Slider& s)
    {
        s.setRange (0.0, 1.0, 0.001);
        s.setSliderSnapsToMousePosition (false);
        s.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke);
        s.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.35f));
        s.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        s.setColour (juce::Slider::thumbColourId, juce::Colours::whitesmoke);
        s.setColour (juce::Slider::trackColourId, juce::Colours::goldenrod.withAlpha (0.55f));
        s.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 18);
    }

    void styleLabel (juce::Label& l, const juce::String& text)
    {
        l.setText (text, juce::dontSendNotification);
        l.setFont (juce::FontOptions (11.0f));
        l.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.75f));
        l.setJustificationType (juce::Justification::centredLeft);
    }
}

RampColorPickerPanel::RampColorPickerPanel (juce::Colour initial)
    : current (initial)
{
    setSize (280, kPreferredHeight);

    styleLabel (hueLabel, "Hue");
    styleLabel (satLabel, "Sat");
    styleLabel (briLabel, "Bri");
    addAndMakeVisible (hueLabel);
    addAndMakeVisible (satLabel);
    addAndMakeVisible (briLabel);

    styleSlider (hue);
    styleSlider (sat);
    styleSlider (bri);
    addAndMakeVisible (hue);
    addAndMakeVisible (sat);
    addAndMakeVisible (bri);

    auto onSlide = [this]
    {
        if (! suppressCallbacks)
            applyFromSliders();
    };
    hue.onValueChange = onSlide;
    sat.onValueChange = onSlide;
    bri.onValueChange = onSlide;

    doneButton.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.45f));
    doneButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke);
    doneButton.onClick = [this]
    {
        if (onDone != nullptr)
            onDone();
    };
    addAndMakeVisible (doneButton);

    syncSlidersFromColour();
}

void RampColorPickerPanel::syncSlidersFromColour()
{
    float h = 0.0f, s = 0.0f, b = 0.0f;
    current.getHSB (h, s, b);

    suppressCallbacks = true;
    hue.setValue (h, juce::dontSendNotification);
    sat.setValue (s, juce::dontSendNotification);
    bri.setValue (b, juce::dontSendNotification);
    suppressCallbacks = false;
}

void RampColorPickerPanel::applyFromSliders()
{
    const float a = current.getFloatAlpha();
    current = juce::Colour::fromHSV ((float) hue.getValue(),
                                     (float) sat.getValue(),
                                     (float) bri.getValue(),
                                     a);
    repaint();

    if (onColourChanged != nullptr)
        onColourChanged (current);
}

void RampColorPickerPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillRoundedRectangle (bounds, 4.0f);
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.22f));
    g.drawRoundedRectangle (bounds, 4.0f, 1.0f);

    auto swatch = getLocalBounds().removeFromRight (52).reduced (8, 10).withTrimmedBottom (28);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (swatch.toFloat(), 3.0f);
    g.setColour (current);
    g.fillRoundedRectangle (swatch.toFloat().reduced (1.0f), 3.0f);
}

void RampColorPickerPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    auto right = area.removeFromRight (52);
    doneButton.setBounds (right.removeFromBottom (24).reduced (2, 0));

    auto row = [&area] (juce::Label& lab, juce::Slider& slider)
    {
        auto r = area.removeFromTop (24);
        lab.setBounds (r.removeFromLeft (28));
        slider.setBounds (r);
        area.removeFromTop (2);
    };

    row (hueLabel, hue);
    row (satLabel, sat);
    row (briLabel, bri);
}
