#include "RotaryImageKnobLookAndFeel1.h"
#include "EqEditor.h"
#include "KnobBandHighlight.h"
#include "KnobThemeHelpers.h"
//#include "Effects/shadows-main/shadows.h" 
#include <JuceHeader.h> 
#include "BinaryData.h"


RotaryImageKnobLookAndFeel1::RotaryImageKnobLookAndFeel1()
    : frames(100), minValue(20.0f), maxValue(20000.0f)  // Correctly formatted member initializer list
{
 
    knobImage = EqProcessor::darkKnob4_StitchedImage;  
}

RotaryImageKnobLookAndFeel1::~RotaryImageKnobLookAndFeel1()
{
}

void RotaryImageKnobLookAndFeel1::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float rotaryStartAngle, float rotaryEndAngle, juce::Slider& slider)
{
    if (knobImage.isValid())
    {
        // Scaling factor
        float scaleFactor = 1.0f;

        int scaledWidth = width * scaleFactor;
        int scaledHeight = height * scaleFactor;
        int xOffset = (width - scaledWidth) / 2;
        int yOffset = (height - scaledHeight) / 2;

        // Customize StackShadow here

        juce::Point<int> customOffset(0, 0);
        int customBlur = 8;  // Reduced blur value
        int customSpread = 2;

        // Use the slider's NormalisableRange (skew) so log-ish freq knobs don't
        // visually pin near the stop until ~200 Hz on a 20–20k range.
        juce::ignoreUnused (sliderPos);
        const float normalizedSliderPos = juce::jlimit (
            0.0f, 1.0f, (float) slider.valueToProportionOfLength (slider.getValue()));

        // Map the normalized position to an alpha value between 0 and 255
        //int alphaValue = static_cast<int>(normalizedSliderPos * 255);

        int alphaValue = 100;

        juce::Colour customShadowColor = juce::Colour::fromRGBA(200, 120, 0, alphaValue);
        const bool bandHighlight = KnobBandHighlight::isActive (slider);
        const auto& theme = KnobTheme::colors (themeColors);

        if (slider.isMouseOverOrDragging() || bandHighlight)
        {
            customSpread = bandHighlight ? 7 : 6;
            customShadowColor = KnobBandHighlight::intensify (
                theme.knobArc.withAlpha ((float) alphaValue / 255.0f), bandHighlight);
        }
        else
        {
            customSpread = 1;
            customShadowColor = KnobTheme::arcDark (theme, bandHighlight).withAlpha ((float) (alphaValue - 50) / 255.0f);
        }

        // shadows::StackShadow stackShadow(customShadowColor, customOffset, customBlur, customSpread);

         // Clear any previous path data
        filledArc.clear();

        // Map skewed proportion → stitched frame (not raw linear Hz).
        int frameId = static_cast<int> (normalizedSliderPos * (float) frames);
        frameId = juce::jlimit(0, frames - 1, frameId);

        const int frameWidth = knobImage.getWidth();
        const int frameHeight = knobImage.getHeight() / frames;

        // Draw the knob image at the scaled size
        KnobTheme::drawArtwork (g, knobImage,
            x + xOffset, y + yOffset, scaledWidth, scaledHeight,
            0, frameId * frameHeight, frameWidth, frameHeight,
            theme);

        g.setImageResamplingQuality(juce::Graphics::ResamplingQuality::highResamplingQuality);

        // Create a Path object for the filled arc
        juce::Path filledArc;

        float rotaryStartAngleInDegrees = -145.0f;
        float rotaryEndAngleInDegrees = 145.0f;

        float rotaryStartAngle = juce::degreesToRadians(rotaryStartAngleInDegrees);
        float rotaryEndAngle = juce::degreesToRadians(rotaryEndAngleInDegrees);

        float endAngle = rotaryStartAngle + (rotaryEndAngle - rotaryStartAngle) * normalizedSliderPos;

        // Number of segments to divide the arc for the filled portion
        const int numSegments = 50;

        // Initialize a temporary Path object to represent each segment
        juce::Path segmentArc;


        // Create colour references for the gradient start and end
        juce::Colour brightOrange = KnobTheme::arcBright (theme, bandHighlight);
        juce::Colour darkOrange = KnobTheme::arcDark (theme, bandHighlight);

        // Loop for the filled arc
        for (int i = numSegments - 1; i >= 0; --i) {
            float t = static_cast<float>(i) / (numSegments - 1);
            float angle = rotaryStartAngle + (endAngle - rotaryStartAngle) * t;
            float nextT = static_cast<float>(i - 1) / (numSegments - 1);
            float nextAngle = rotaryStartAngle + (endAngle - rotaryStartAngle) * nextT;

            // Clear the path and add a new pie segment
            segmentArc.clear();
            segmentArc.addPieSegment(juce::Rectangle<float>(x + xOffset, y + yOffset, scaledWidth, scaledHeight), angle, nextAngle, 0.8f);

            // Interpolate the color for this segment based on its position along the arc
            juce::Colour interpolatedColor = juce::Colour::fromHSV(
                darkOrange.getHue() + (brightOrange.getHue() - darkOrange.getHue()) * t,
                darkOrange.getSaturation() + (brightOrange.getSaturation() - darkOrange.getSaturation()) * t,
                darkOrange.getBrightness() + (brightOrange.getBrightness() - darkOrange.getBrightness()) * t,
                1.0f  // alpha
            );

            // Set the color and fill the segment for the first arc
            g.setColour(interpolatedColor);
            g.fillPath(segmentArc);
        }

        // Always a single segment for the shadow arc

        //juce::Path shadowArc;

        //shadowArc.addPieSegment(juce::Rectangle<float>(x + xOffset, y + yOffset, scaledWidth, scaledHeight), rotaryStartAngle, endAngle, 0.8f);
       // stackShadow.drawFixedOuterShadowForPath(g, shadowArc);

    }

    else
    {
        juce::LookAndFeel_V4::drawRotarySlider(g, x, y, width, height,
            sliderPos, rotaryStartAngle, rotaryEndAngle, slider);
    }
}




bool RotaryImageKnobLookAndFeel1::isImageValid() const {
    return knobImage.isValid();
}

