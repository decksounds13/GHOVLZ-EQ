#pragma once

#include <JuceHeader.h>


class QuadPickerOverlayComponent : public juce::Component {
public:
    QuadPickerOverlayComponent(juce::Component& quadPicker, juce::Slider& brightnessRangeSlider, juce::Slider& saturationRangeSlider);

    void paint(juce::Graphics& g) override;
  
    void resized() override;

    void setSaturationRandomizationEnabled(bool isEnabled);
    void setBrightnessRandomizationEnabled(bool isEnabled);
    void mouseMove(const juce::MouseEvent& event) override;

private:
    juce::Component& quadPicker;
    juce::Slider& brightnessRangeSlider; 
    juce::Slider& saturationRangeSlider; 

    bool isSaturationRandomizationEnabled = false;
    bool isBrightnessRandomizationEnabled = false;

    double brightnessUpper;
    double brightnessLower;
    double saturationUpper;
    double saturationLower;


    bool isMouseOverSaturationSlider;
    bool isMouseOverBrightnessSlider;
    bool isMouseOverHorizontalPartOfSaturationSlider;
    bool isMouseOverHorizontalPartOfBrightnessSlider;
    int mouseXPosition;
    int mouseYPosition;
};


