#pragma once
#include <JuceHeader.h>
#include "Menu/SharedResources.h"

class EqEditor;


class RotaryImageKnobLookAndFeel1 : public juce::LookAndFeel_V4
{
public:
    RotaryImageKnobLookAndFeel1();
    ~RotaryImageKnobLookAndFeel1();

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider) override;

    // Function to set the knob image dynamically
   // void setKnobImage(const juce::Image& knobImage);

    void setFrames(int newFrames) { frames = newFrames; }
    void setMinValue(float newMinValue) { minValue = newMinValue; }
    void setMaxValue(float newMaxValue) { maxValue = newMaxValue; }

    void setThemeColors (SharedResources* r) noexcept { themeColors = r; }

    bool isImageValid() const;


private:
    juce::Image knobImage;
    juce::Image knobFaceImage;
    juce::Image resizedKnobImage;
   
    int frames;
    float minValue;
    float maxValue;
    juce::Path filledArc;

    juce::Image segmentedArcImage;

    SharedResources* themeColors = nullptr;
};
