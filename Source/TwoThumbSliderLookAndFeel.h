#pragma once
#pragma once

#include <JuceHeader.h>
#include "Menu/SharedResources.h"

class TwoThumbSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class Direction
    {
        Right, Left, Up, Down
    };

    void setArrowProperties(Direction newDirection, juce::Colour newColor, juce::Colour newColor2, juce::Colour newColor3, float newSize)
    {
        direction = newDirection;
        arrowColor = newColor;
        arrowOutlineColor = newColor2;
        fillBarColor = newColor3;
        arrowSize = newSize;
        customColorSet = true;
    }

    void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        drawLinearSliderThumb(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
    }

    void drawLinearSliderThumb(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        bool useDefaultLookAndFeel = false; // Change this to true to use the default look and feel

        if (useDefaultLookAndFeel) {
            LookAndFeel_V4::drawLinearSliderThumb(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
        else {

            // Identical settings to drawLinearSliderThumb
            float headWidth = 7.0f;
            float shaftWidth = 7.0f;
            float headHeight = 10.0f; // You can adjust this as needed
            float shaftHeight = 10.0f; // You can adjust this as needed
            float xOffset = -1.0f;

            // Calculate the position of the thumb, but it needs to be inverted for downward pointing
            float thumbY = juce::jlimit(static_cast<float>(y), static_cast<float>(y + height), sliderPos);
            float thumbX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;

            // Drawing path for a downward pointing arrow
            juce::Path path;
            path.startNewSubPath(thumbX + xOffset, thumbY + shaftHeight); // Start above the thumb position
            path.lineTo(thumbX + xOffset - headWidth, thumbY); // Point to the center of the slider position
            path.lineTo(thumbX + xOffset, thumbY - shaftHeight); // Draw up from the slider position
            path.lineTo(thumbX + xOffset + shaftWidth, thumbY - shaftHeight); // Complete the arrow shape
            path.lineTo(thumbX + xOffset + shaftWidth, thumbY + shaftHeight); // Close the path back to start
            path.closeSubPath();

            // Calculate the center of the path's bounding box
            juce::Rectangle<float> bounds = path.getBounds();
            float centerX = bounds.getX() + bounds.getWidth() / 2.0f;
            float centerY = bounds.getY() + bounds.getHeight() / 2.0f;

            // Apply a 90-degree rotation around the center
            path.applyTransform(juce::AffineTransform::rotation(juce::MathConstants<float>::pi / 2.0f, centerX, centerY));

            // Custom fill color
            g.setColour(arrowColor);
            g.fillPath(path);

            // Custom outline color
            g.setColour(arrowOutlineColor);
            g.strokePath(path, juce::PathStrokeType(2.0f));
        }
    }

    void drawLinearSliderBackground(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
         // Custom color for the range rectangle
    juce::Colour rangeRectColor = juce::Colour::fromRGB(100, 150, 200); // Example RGB color

    // Draw the rectangle representing the range
    if (style == juce::Slider::TwoValueHorizontal || style == juce::Slider::ThreeValueHorizontal)
    {
        g.setColour(rangeRectColor);
        g.fillRect(juce::Rectangle<float>(minSliderPos, y + (height / 2) - 2.5f, maxSliderPos - minSliderPos, 5.0f));
    }
    }

private:
    Direction direction = Direction::Down;
    juce::Colour arrowColor{ juce::Colour::fromRGB(100,90, 80)};
    juce::Colour fillBarColor{ juce::Colour::fromRGB(100,90, 190) };
    juce::Colour arrowOutlineColor{ juce::Colours::black };
    float arrowSize = 10.0f;
    bool customColorSet = false;
};


class TwoThumbSlider : public juce::Slider
{
public:
    TwoThumbSlider()
        :sharedResources(sharedResources)
    {
        


        setLookAndFeel(&twoThumbSliderLookAndFeel);  
        setSliderStyle(juce::Slider::TwoValueHorizontal);
        setRange(0.0, 1.0);  // Set the range from 0.0 to 1.0
        setMinValue(0.05);    // Set the initial position of the lower thumb
        setMaxValue(0.3);
        setArrowProperties(TwoThumbSliderLookAndFeel::Direction::Down, juce::Colours::gold, juce::Colours::gold, juce::Colours::gold, 20); // Now set arrow properties
       
    }

    void setArrowProperties(TwoThumbSliderLookAndFeel::Direction direction, juce::Colour arrowColor, juce::Colour arrowOutlineColor, juce::Colour fillBarColor, float size) {
        twoThumbSliderLookAndFeel.setArrowProperties(direction, arrowColor, arrowOutlineColor, fillBarColor, size);
        
        setColour(juce::Slider::backgroundColourId, fillBarColor);
        setColour(juce::Slider::trackColourId, fillBarColor);
    }

    ~TwoThumbSlider() {
        setLookAndFeel(nullptr);
    }

private:
    TwoThumbSliderLookAndFeel twoThumbSliderLookAndFeel;

    SharedResources& sharedResources;
};

