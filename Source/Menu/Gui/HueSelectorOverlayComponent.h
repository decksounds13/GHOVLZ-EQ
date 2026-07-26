#pragma once

#include <JuceHeader.h>

class HueSelectorOverlayComponent : public juce::Component {
public:
    HueSelectorOverlayComponent(juce::Component& hueSelector, juce::Slider& hueRangeSlider);

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::Component& hueSelector;
    juce::Slider& hueRangeSlider;

    void drawHueSelectorOverlay(juce::Graphics& g);
};
