#pragma once
#include <JuceHeader.h>
#include "Menu/SharedResources.h"

class EqEditor;


class RotaryImageKnobLookAndFeel2 : public juce::LookAndFeel_V4
{
public:
    RotaryImageKnobLookAndFeel2();
    ~RotaryImageKnobLookAndFeel2();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

    // Function to set the knob image dynamically
   // void setKnobImage(const juce::Image& knobImage);

    void setFrames(int newFrames) { frames = newFrames; }
    void setMinValue(float newMinValue) { minValue = newMinValue; }
    void setMaxValue(float newMaxValue) { maxValue = newMaxValue; }

    void setThemeColors (SharedResources* r) noexcept { themeColors = r; }


private:
    juce::Image knobImage;
    juce::Image knobFaceImage;
    juce::Image resizedKnobImage;
   
    int frames;
    float minValue;
    float maxValue;

    SharedResources* themeColors = nullptr;

};
