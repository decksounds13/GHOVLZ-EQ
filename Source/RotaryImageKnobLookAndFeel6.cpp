#include "RotaryImageKnobLookAndFeel6.h"
#include "EqEditor.h"
#include "KnobBandHighlight.h"
#include <JuceHeader.h> 
#include "BinaryData.h"

//class EqEditor;

RotaryImageKnobLookAndFeel6::RotaryImageKnobLookAndFeel6()
    : frames(100), minValue(0.36f), maxValue(0.80f)  // Correctly formatted member initializer list
{
    // Load the stitched knob image and knob face image from their respective sources
    knobImage = EqProcessor::darkKnob4_StitchedImage;


}

RotaryImageKnobLookAndFeel6::~RotaryImageKnobLookAndFeel6()
{
}

void RotaryImageKnobLookAndFeel6::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    if (knobImage.isValid())
    {
        // Get the parameter's current value
        const float parameterValue = slider.getValue();

        // Map the parameter's value to the frame range
        int frameId = static_cast<int>((parameterValue - minValue) / (maxValue - minValue) * frames);
        frameId = juce::jlimit(0, frames - 1, frameId); // Ensure frameId stays within valid range

        const int frameWidth = knobImage.getWidth();
        const int frameHeight = knobImage.getHeight() / frames;

        // Scaling factors for knobFaceImage
        float xScale = 1.1;
        float yScale = 1.1;

        // Manual offsets for knobFaceImage
        int manualXOffset = 0;
        int manualYOffset = -2;

        // Calculate the new dimensions
        int scaledWidth = static_cast<int>(width * xScale);
        int scaledHeight = static_cast<int>(height * yScale);

        // Offsets for positioning knobFaceImage
        int xOffset = (scaledWidth - width) / 2;
        int yOffset = (scaledHeight - height) / 2;

        // Draw knobFaceImage first so it's beneath knobImage
        // g.drawImage(knobFaceImage, x - xOffset + manualXOffset, y - yOffset + manualYOffset, scaledWidth, scaledHeight,
        //     0, 0, knobFaceImage.getWidth(), knobFaceImage.getHeight(), false);

        // Draw the knob image
        g.drawImage(knobImage, x, y, width, height,
            0, frameId * frameHeight, frameWidth, frameHeight, false);

        g.setImageResamplingQuality(juce::Graphics::ResamplingQuality::highResamplingQuality);


        // Create a Path object for the filled arc
        juce::Path filledArc;

        // Normalize the slider position to a 0-1 range
        float normalizedSliderPos = (sliderPos - minValue) / (maxValue - minValue);

        // Define start and end angles in degrees
        float rotaryStartAngleInDegrees = -145.0f;
        float rotaryEndAngleInDegrees = 145.0f;

        // Convert to radians
        float rotaryStartAngle = juce::degreesToRadians(rotaryStartAngleInDegrees);
        float rotaryEndAngle = juce::degreesToRadians(rotaryEndAngleInDegrees);

        // Calculate the end angle based on the normalized slider position
        float endAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * normalizedSliderPos;

        // Add pie segment to represent the filled part of the slider
        filledArc.addPieSegment(juce::Rectangle<float>(x, y, width, height), rotaryStartAngle, endAngle, 0.8f);

        // Create colour gradient
        const bool bandHighlight = KnobBandHighlight::isActive (slider);
        juce::Colour brightOrange = KnobBandHighlight::intensify (juce::Colour(255, 110, 0), bandHighlight);
        juce::Colour darkOrange = KnobBandHighlight::intensify (juce::Colour(140, 20, 0), bandHighlight);
        juce::ColourGradient gradient(darkOrange, x, y,
            brightOrange, x + width, y + height, false);

        // Set color based on mouse state
        if (slider.isMouseOverOrDragging() || bandHighlight)
        {
            g.setColour(brightOrange);
        }
        else
        {
            g.setGradientFill(gradient);
        }

        // Draw the filled arc
        g.fillPath(filledArc);
    }
    else
    {
        // If the image is not valid, draw a placeholder
        juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height,
            sliderPos, rotaryStartAngle, rotaryEndAngle, slider);

        // DBG("Image Invalid");
    }
}



