#pragma once

#include <JuceHeader.h>

/**
    Compact HSV colour editor for gradient poles.
    Designed to live inline under the strip — never launched as a CallOutBox
    (nesting CallOutBoxes under the UI menu freezes the host).
*/
class RampColorPickerPanel : public juce::Component
{
public:
    explicit RampColorPickerPanel (juce::Colour initial);
    ~RampColorPickerPanel() override = default;

    std::function<void (juce::Colour)> onColourChanged;
    std::function<void()> onDone;

    void resized() override;
    void paint (juce::Graphics& g) override;
    juce::Colour getColour() const noexcept { return current; }

    static constexpr int kPreferredHeight = 118;

private:
    void syncSlidersFromColour();
    void applyFromSliders();

    juce::Colour current;
    juce::Slider hue { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider sat { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider bri { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label hueLabel, satLabel, briLabel;
    juce::TextButton doneButton { "Done" };
    bool suppressCallbacks = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RampColorPickerPanel)
};
