#pragma once
#include <JuceHeader.h>
#include <functional>
#include "HueSelector.h"
#include "../SharedResources.h"
#include "UIElementsList.h"
#include "MelatoninBlur/melatonin/shadows.h"

class QuadPicker : public juce::Component, public HueSelector::Listener, public UIElementsList::Listener
{
public:
    explicit QuadPicker (SharedResources& sharedResources);

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;

    void setHue (float newHue);
    void setSelectedPosition (juce::Point<float> newPosition);
    void hueChanged (float newHue) override;
    void setColor (const juce::Colour& newColor);
    void onElementSelected (const juce::String& name, const juce::Colour& color) override;
    juce::Colour getSelectedColor() const;
    void updateGradientImage();
    /** True while the user is scrubbing the SV pad (skip external setColor feedback). */
    bool isDraggingColour() const noexcept { return draggingColour; }

    const juce::Path& getShadowPath() const { return shadowPath; }

    std::function<void(juce::Colour)> onColorChanged;

private:
    void applyPointerPosition (juce::Point<float> localPos);
    void syncPositionFromSelectedColor();

    juce::Path shadowPath;
    juce::Colour hueColor;
    juce::Point<float> selectedPosition;
    juce::Colour selectedColor;
    melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha (0.75f), 12, { 0, 2 } } };
    melatonin::DropShadow shadow2 = { { juce::Colours::black.withAlpha (0.75f), 6, { 0, 2 } } };
    juce::Colour lastHueColor;
    juce::Image gradientImage;

    bool isFirstPaint = true;
    bool draggingColour = false;
    int index = -1;

    SharedResources& sharedResources;

    bool shouldUpdatePosition = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (QuadPicker)
};
