#pragma once
#include <JuceHeader.h>
#include "UIElementsList.h"
#include "../SharedResources.h"
#include "../../ArrowSliderLookAndFeel.h"
#include "MelatoninBlur/melatonin/shadows.h"

class HueSelector : public juce::Component,
    public juce::Slider::Listener,
    public UIElementsList::Listener  
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void hueChanged(float newHue) {}
    };

    HueSelector(SharedResources& resources);
    ~HueSelector();

    void paint(juce::Graphics& g) override;
    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;
    void onElementSelected(const juce::String& name, const juce::Colour& color) override;  
    void addListener(Listener* newListener);
    void setColor(const juce::Colour& newColor);
    void updateHueSliderLookAndFeel(ArrowSliderLookAndFeel::Direction direction, juce::Colour arrowColor, juce::Colour arrowOutlineColor, float size);
    void generateGradientImage();
    void drawWhiteLine(juce::Graphics& g);

    juce::Rectangle<int> getGradientBounds() const; // Declaration

  
    std::function<void(float)> onHueChanged;

    std::function<void(juce::Colour)> onColorChanged;
   
    juce::Image gradientImage;

private:
    

    CustomSlider hueSlider;

    juce::ListenerList<Listener> listeners;

    ArrowSliderLookAndFeel arrowLookAndFeel;

    SharedResources& sharedResources;
  
    float alphaValue;
  
    melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha(0.75f), 12, { 0, 2 } } };

    melatonin::DropShadow shadow2 = { { juce::Colours::black.withAlpha(0.75f), 10, { 0, 2 } } };

    melatonin::DropShadow shadow3 = { { juce::Colours::black.withAlpha(0.75f), 6, { 0, 2 } } };
   
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HueSelector)
};
