#include "RotaryImageKnobLookAndFeel3.h"
#include "EqEditor.h"
#include "KnobBandHighlight.h"
#include "KnobThemeHelpers.h"
#include <JuceHeader.h> 
#include "BinaryData.h"

//class EqEditor;

RotaryImageKnobLookAndFeel3::RotaryImageKnobLookAndFeel3()
    : frames(100), minValue(-24.0f), maxValue(24.0f)  // Correctly formatted member initializer list
{
    // Load the stitched knob image and knob face image from their respective sources
    knobImage = EqProcessor::darkKnob4_StitchedImage;

}

RotaryImageKnobLookAndFeel3::~RotaryImageKnobLookAndFeel3()
{
}

void RotaryImageKnobLookAndFeel3::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    if (knobImage.isValid())
    {
        // Get the parameter's current value
        const float parameterValue = slider.getValue();

        // Map the parameter's value to the frame range
        int frameId = static_cast<int>((parameterValue - minValue) / (maxValue - minValue) * frames);
        frameId = juce::jlimit(0, frames - 1, frameId);

        // Dimensions for the stitched image frames
        const int frameWidth = knobImage.getWidth();
        const int frameHeight = knobImage.getHeight() / frames;

        // Draw the knob image
        const auto& theme = KnobTheme::colors (themeColors);
        KnobTheme::drawArtwork (g, knobImage, x, y, width, height,
            0, frameId * frameHeight, frameWidth, frameHeight, theme);

        // Existing code for resampling quality and other settings...

      // Create a Path object for the filled arc
        juce::Path filledArc;
        // Normalize the parameter value to 0-1 range
        float normalizedValue = (parameterValue - minValue) / (maxValue - minValue);

        // Calculate the middle angle (which will represent the default or 'zero' value)
        float middleAngle = (rotaryStartAngle + rotaryEndAngle) / 2;

        // Invert the middle angle by 180 degrees (pi radians)
        middleAngle += juce::MathConstants<float>::pi;  // Add pi to invert by 180 degrees

        // Calculate the angle span, i.e., the angle corresponding to max - min dB
        float angleSpan = rotaryEndAngle - rotaryStartAngle;

        // Determine the angle based on the normalized value
        float angleDelta = (normalizedValue - 0.5f) * angleSpan;

        // Negate the angleDelta to make it travel in the opposite direction
        angleDelta = -angleDelta;

        // Start and end angles for the filled arc
        float startAngle = middleAngle;
        float endAngle = middleAngle + angleDelta;

        // Add pie segment to represent the filled part of the slider
        filledArc.addPieSegment(juce::Rectangle<float>(x, y, width, height),
            std::min(startAngle, endAngle),
            std::max(startAngle, endAngle), 0.8f);

        // Define colors
        const bool bandHighlight = KnobBandHighlight::isActive (slider);
        juce::Colour brightOrange = KnobTheme::arcBright (theme, bandHighlight, &slider);
        juce::Colour darkOrange = KnobTheme::arcDark (theme, bandHighlight, &slider);
        juce::Colour brightGreen = KnobBandHighlight::intensify (juce::Colour(140, 50, 20), bandHighlight);

        // Normalize the parameter value to a 0-1 range
        float normalizedValuePositive = (parameterValue - 0) / (maxValue - 0);
        float normalizedValueNegative = (0 - parameterValue) / (0 - minValue);

        // Calculate the color based on the normalized value
        juce::Colour currentColor;
        if (parameterValue > 0) {
            currentColor = darkOrange.interpolatedWith(brightOrange, normalizedValuePositive);
        }
        else {
            currentColor = darkOrange.interpolatedWith(brightGreen, normalizedValueNegative);
        }


        // Create the gradients
        juce::ColourGradient gradientPositive(darkOrange, x, y, brightOrange, x + width, y + height, false);
        juce::ColourGradient gradientNegative(darkOrange, x, y, brightGreen, x + width, y + height, false);

        // Choose the gradient based on the current value
        juce::ColourGradient chosenGradient;
        if (parameterValue > 0) {
            chosenGradient = gradientPositive;
        }
        else {
            chosenGradient = gradientNegative;
        }

        // Set the gradient
        g.setGradientFill(chosenGradient);

        // Draw the filled arc
        g.fillPath(filledArc);

        // Extra intensity ring cue while this band is being manipulated.
        if (bandHighlight)
        {
            g.setColour (brightOrange.withAlpha (0.45f));
            g.strokePath (filledArc, juce::PathStrokeType (1.5f));
        }


    }
    else
    {
        // Fallback if image is not valid
        juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height,
            sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
    }
}



