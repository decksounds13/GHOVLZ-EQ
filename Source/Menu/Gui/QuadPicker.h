#pragma once
#include <JuceHeader.h>
#include <functional>  // Include this for std::function
#include "HueSelector.h"
#include "../SharedResources.h"
#include "UIElementsList.h"
#include "MelatoninBlur/melatonin/shadows.h"

class QuadPicker : public juce::Component, public HueSelector::Listener, public UIElementsList::Listener
{
public:
    // Update the constructor to accept SharedResources by reference
    explicit QuadPicker(SharedResources& sharedResources);

    void paint(juce::Graphics& g) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void setHue(float newHue);
    void setSelectedPosition(juce::Point<float> newPosition);
    void hueChanged(float newHue);
    void setColor(const juce::Colour& newColor);
    void onElementSelected(const juce::String& name, const juce::Colour& color);
    juce::Colour getSelectedColor() const;
    void resized();
    void updateGradientImage();
    const juce::Path& getShadowPath() const {
        return shadowPath; 
    }


    // Define onColorChanged
    std::function<void(juce::Colour)> onColorChanged;


private:

    juce::Path shadowPath;
    juce::Colour hueColor;
    juce::Point<float> selectedPosition;
    juce::Colour selectedColor;
    melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha(0.75f), 12, { 0, 2 } } };
    melatonin::DropShadow shadow2 = { { juce::Colours::black.withAlpha(0.75f), 6, { 0, 2 } } };
    juce::Colour lastHueColor;
    juce::Image gradientImage;

    bool isFirstPaint = true;
    int index = -1;

    SharedResources& sharedResources;

    bool shouldUpdatePosition;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(QuadPicker)
};
