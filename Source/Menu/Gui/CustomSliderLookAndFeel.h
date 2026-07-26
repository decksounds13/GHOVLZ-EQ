#pragma once

#include <JuceHeader.h>
#include <MelatoninBlur/melatonin/shadows.h>

class CustomSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CustomSliderLookAndFeel(const juce::Colour& background, const juce::Colour& track, const juce::Colour& thumb);

    // Override the drawRotarySlider method to customize the slider appearance
    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle, juce::Slider& slider);

private:


    juce::Colour backgroundColour;
    juce::Colour trackColour;
    juce::Colour thumbColour;

    //CustomShadow shadow;
     melatonin::DropShadow shadow; // Add this line
};
