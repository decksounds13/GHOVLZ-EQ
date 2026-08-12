#pragma once

#include <JuceHeader.h>

/**
    Non-interactive overlay on the SV pad: randomize-range shading and guides.
    Must not paint stray lines when randomize is off (hover flags must be zero-init).
*/
class QuadPickerOverlayComponent : public juce::Component
{
public:
    QuadPickerOverlayComponent (juce::Component& quadPicker,
                                juce::Slider& brightnessRangeSlider,
                                juce::Slider& saturationRangeSlider);

    void paint (juce::Graphics& g) override;
    void resized() override;

    void setSaturationRandomizationEnabled (bool isEnabled);
    void setBrightnessRandomizationEnabled (bool isEnabled);
    void mouseMove (const juce::MouseEvent& event) override;
    void mouseExit (const juce::MouseEvent& event) override;

private:
    juce::Component& quadPicker;
    juce::Slider& brightnessRangeSlider;
    juce::Slider& saturationRangeSlider;

    bool isSaturationRandomizationEnabled = false;
    bool isBrightnessRandomizationEnabled = false;

    bool isMouseOverSaturationSlider = false;
    bool isMouseOverBrightnessSlider = false;
    bool isMouseOverHorizontalPartOfSaturationSlider = false;
    bool isMouseOverHorizontalPartOfBrightnessSlider = false;
    int mouseXPosition = 0;
    int mouseYPosition = 0;
};
