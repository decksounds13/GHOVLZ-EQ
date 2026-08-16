#include "RotaryImageKnobLookAndFeel2.h"
#include "EqEditor.h"
#include "KnobBandHighlight.h"
#include "KnobThemeHelpers.h"
#include <JuceHeader.h> 
#include "BinaryData.h"

//class EqEditor;

RotaryImageKnobLookAndFeel2::RotaryImageKnobLookAndFeel2()
    : frames(100), minValue(0.15f), maxValue(10.0f)  // Correctly formatted member initializer list
{
    // Load the stitched knob image and knob face image from their respective sources
    knobImage = EqProcessor::darkKnob4_StitchedImage;

}

RotaryImageKnobLookAndFeel2::~RotaryImageKnobLookAndFeel2()
{
}

void RotaryImageKnobLookAndFeel2::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    if (knobImage.isValid())
    {
        juce::ignoreUnused (sliderPos, rotaryStartAngle, rotaryEndAngle);
        const float normalizedSliderPos = juce::jlimit (
            0.0f, 1.0f, (float) slider.valueToProportionOfLength (slider.getValue()));
        int frameId = juce::jlimit (0, frames - 1,
                                    (int) std::lround (normalizedSliderPos * (float) (frames - 1)));

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
        const auto& theme = KnobTheme::colors (themeColors);
        KnobTheme::drawArtwork (g, knobImage, x, y, width, height,
            0, frameId * frameHeight, frameWidth, frameHeight, theme);

        g.setImageResamplingQuality(juce::Graphics::ResamplingQuality::highResamplingQuality);

        juce::Path filledArc;
        const float startRad = juce::degreesToRadians (-145.0f);
        const float endRad   = juce::degreesToRadians (145.0f);
        const float endAngle = startRad + (endRad - startRad) * normalizedSliderPos;

        // Add pie segment to represent the filled part of the slider
        filledArc.addPieSegment (juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height),
                                 startRad, endAngle, 0.8f);

        // Create colour gradient
        const bool bandHighlight = KnobBandHighlight::isActive (slider);
        juce::Colour brightOrange = KnobTheme::arcBright (theme, bandHighlight, &slider);
        juce::Colour darkOrange = KnobTheme::arcDark (theme, bandHighlight, &slider);
        juce::ColourGradient gradient(darkOrange, x, y,
            brightOrange, x + width, y + height, false);

        // Set color based on mouse state
        if (slider.isMouseOverOrDragging() || bandHighlight)
        {
            g.setColour(brightOrange);  // Bright orange when mouse-over, dragging, or band active
        }
        else
        {
            // Apply gradient fill if not in mouse-over state
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

        //DBG("Image Invalid");
    }
}



