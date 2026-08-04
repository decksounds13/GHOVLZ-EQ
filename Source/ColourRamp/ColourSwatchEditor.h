#pragma once

#include <JuceHeader.h>
#include "RampColorPickerPanel.h"

/**
    Label + colour swatch that expands an inline RampColorPickerPanel
    (same UX as gradient poles — never a nested CallOutBox).
*/
class ColourSwatchEditor : public juce::Component
{
public:
    explicit ColourSwatchEditor (const juce::String& labelText);
    ~ColourSwatchEditor() override = default;

    void setColour (juce::Colour c, juce::NotificationType notification = juce::dontSendNotification);
    juce::Colour getColour() const noexcept { return colour; }

    void setExpanded (bool shouldExpand);
    bool isExpanded() const noexcept { return picker != nullptr; }

    std::function<void (juce::Colour)> onColourChanged;
    std::function<void()> onHeightChanged;

    int getPreferredHeight() const;
    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;

    static constexpr int kSwatchRowH = 28;

private:
    void openPicker();
    void closePicker (bool notifyHeight);

    juce::Label label;
    juce::Colour colour { juce::Colours::white };
    juce::Rectangle<int> swatchBounds;
    std::unique_ptr<RampColorPickerPanel> picker;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourSwatchEditor)
};
