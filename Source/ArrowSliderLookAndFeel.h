#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"

class ArrowSliderLookAndFeel : public juce::LookAndFeel_V4
{
public:
    enum class Direction
    {
        Right, Left, Up, Down
    };

    void setArrowProperties(Direction newDirection, juce::Colour newColor, juce::Colour newColor2, float newSize)
    {
        direction = newDirection;
        arrowColor = newColor;
        arrowOutlineColor = newColor2;
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
       
        if (style == juce::Slider::LinearVertical) {
            float headWidth = 12.0f;
            float shaftWidth = 12.0f;
            float headHeight = 12.0f;
            float shaftHeight = 12.0f;
            float xOffset = 3.0f;

            // Calculate the position of the thumb
            float thumbY = juce::jlimit(static_cast<float>(y), static_cast<float>(y + height), sliderPos);
            float thumbX = static_cast<float>(x) + static_cast<float>(width) * 0.5f;

            juce::Path path;
            path.startNewSubPath(thumbX + xOffset, thumbY - shaftHeight);
            path.lineTo(thumbX + xOffset + headWidth, thumbY);
            path.lineTo(thumbX + xOffset, thumbY + shaftHeight);
            path.lineTo(thumbX + xOffset - shaftWidth, thumbY + shaftHeight);
            path.lineTo(thumbX + xOffset - shaftWidth, thumbY - shaftHeight);
            path.closeSubPath();

            // Scale the path down, maintaining the center
            float scale = 0.8f; // Adjust this scale factor as needed
            juce::AffineTransform transform = juce::AffineTransform::scale(scale, scale, thumbX, thumbY);
            path.applyTransform(transform);

            shadow2.render(g, path);

            // Custom fill color
            g.setColour(arrowColor);
            g.fillPath(path);

            // Custom outline color
            g.setColour(juce::Colours::black);
            g.strokePath(path, juce::PathStrokeType(1.0f));



        }

        else {
            LookAndFeel_V4::drawLinearSliderThumb(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        }
    }
   
    void drawLinearSliderBackground(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPos, float minSliderPos, float maxSliderPos,
        const juce::Slider::SliderStyle style, juce::Slider& slider) override
    {
        // Do nothing here to hide the track
    }

private:
    Direction direction = Direction::Right;
    juce::Colour arrowColor{ juce::Colours::red };
    juce::Colour arrowOutlineColor{ juce::Colours::black };
    float arrowSize = 20.0f;
    bool customColorSet = false;

    melatonin::DropShadow shadow = { { juce::Colours::black, 12, { 0, 2 } } };

    melatonin::DropShadow shadow2 = { { juce::Colours::black, 15  , { 0, 2 } } };

};

class CustomSlider : public juce::Slider
{
    


public:
    CustomSlider() {
        setLookAndFeel(&arrowSliderLookAndFeel);

        setArrowProperties(ArrowSliderLookAndFeel::Direction::Right, juce::Colours::transparentBlack, juce::Colours::transparentBlack, 20);
    }

    void setArrowProperties(ArrowSliderLookAndFeel::Direction direction, juce::Colour arrowColor, juce::Colour arrowOutlineColor, float size) {
        arrowSliderLookAndFeel.setArrowProperties(direction, arrowColor, arrowOutlineColor, size);
    }

    float getVerticalThumbPosition() const
    {
        auto range = getRange();
        auto proportion = (getValue() - range.getStart()) / range.getLength();

        // Adjust proportion for the thumb size
        float thumbHeight = 20.0f; // Assuming a thumb height, adjust as needed
        float adjustedProportion = proportion * (getHeight() - thumbHeight);

        // Invert the Y-axis direction
        float invertedPosition = getHeight() - adjustedProportion - thumbHeight / 2;

        return invertedPosition;
    }



private:
    ArrowSliderLookAndFeel arrowSliderLookAndFeel;
};

