#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"

class CustomTwoValueSliderLookAndFeel : public juce::LookAndFeel_V4 {
public:
    enum ThumbStyle { Round, Arrow, NarrowRectangle };
    enum ArrowOrientation { Up, Down, Left, Right };

    CustomTwoValueSliderLookAndFeel();
    ~CustomTwoValueSliderLookAndFeel();

    void setThumbStyle(ThumbStyle newStyle);
    void setArrowOrientation(ArrowOrientation orientation);

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    void drawThumb(juce::Graphics& g, float thumbX, float thumbY, int thumbWidth, int thumbHeight, juce::Slider& slider);

    void updateGlowShadowColor(const juce::Colour& newColor);
    void drawLabel(juce::Graphics& g, juce::Label& label) override;

    void setSliderTrackColor(const juce::Colour& newColour);
    void setSliderBackgroundColor(const juce::Colour& newColour);
    void setSliderThumbColor(const juce::Colour& newColour);
    void applyThemeColors(const juce::Colour& trackColor, const juce::Colour& backgroundColor, const juce::Colour& thumbColor);
    void updateSliderImage(juce::Slider& slider);
    void drawSliderToGraphicsContext(juce::Graphics& g, juce::Slider& slider, int width, int height);
  
    juce::Image createThumbImage(int thumbWidth, int thumbHeight, juce::Slider& slider);
   
    juce::Image createTrackImage(int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider);
 
    void updateTrackImage(juce::Slider& slider, float minSliderPos, float maxSliderPos);

    void updateThumbImage(juce::Slider& slider, float minSliderPos, float maxSliderPos);

private:
    ThumbStyle thumbStyle;
    ArrowOrientation arrowOrientation;
 
    juce::Image trackImage;
    juce::Image thumbImage;
    bool trackImageIsValid = false;
    bool thumbImageIsValid = false;



    melatonin::DropShadow shadow = { { juce::Colours::black, 10, { 0, 2 } } };
    melatonin::DropShadow glowShadow = { { juce::Colours::green, 6, { 0, 0 } } };
    melatonin::InnerShadow innerShadow = { { juce::Colours::black, 3, { 0, -1 } } };
    melatonin::InnerShadow innerShadow2 = { { juce::Colours::black, 4, { 0, 2 } } };
    melatonin::InnerShadow innerShadow3 = { { juce::Colours::black.withAlpha(0.6f), 4, {0, 2} } };
    melatonin::InnerShadow innerShadowVertical = { { juce::Colours::black, 4, { 2, 0 } } };

    juce::Colour sliderTrackColor;
    juce::Colour sliderBackgroundColor;
    juce::Colour sliderThumbColor;

    void createArrowShape(juce::Path& path, int width, int height);
    void applyArrowTransformation(juce::Path& path, float thumbX, float thumbY);
    float getRotationForOrientation() const;

    juce::Array<juce::Button*> slidersUsingCustomLookAndFeel;
    juce::OwnedArray<juce::Slider> twoValueSliders;
};
