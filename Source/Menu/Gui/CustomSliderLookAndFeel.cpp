#include "CustomSliderLookAndFeel.h"
#include <JuceHeader.h>
#include <MelatoninBlur/melatonin_blur.h>

CustomSliderLookAndFeel::CustomSliderLookAndFeel(const juce::Colour& background, const juce::Colour& track, const juce::Colour& thumb)
    : shadow({ {juce::Colours::black, 8, {0, 0}, 3} }),
    backgroundColour(background),
    trackColour(track),
    thumbColour(thumb)
{
    // Set your custom LookAndFeel colors here
    setColour(juce::Slider::backgroundColourId, juce::Colour(30, 20, 10));
    setColour(juce::Slider::trackColourId, juce::Colour(80, 75, 70));
    setColour(juce::Slider::thumbColourId, juce::Colour(100, 96, 90));
}

void CustomSliderLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle, juce::Slider& slider)
{
      if (minSliderPos == maxSliderPos)
    {
        // Handle the case where minSliderPos and maxSliderPos are equal
        // You can set minSliderPos and maxSliderPos to different values here
        minSliderPos = 0.0f;
        maxSliderPos = 1.0f;
    }

    float normalizedMinSliderPos = minSliderPos / 255.0f;
    float normalizedMaxSliderPos = maxSliderPos / 255.0f;

    // Normalize the sliderPos within the range defined by normalizedMinSliderPos and normalizedMaxSliderPos
    float normalizedSliderPos = (sliderPos - normalizedMinSliderPos) / (normalizedMaxSliderPos - normalizedMinSliderPos);
    normalizedSliderPos = juce::jlimit(0.0f, 1.0f, normalizedSliderPos);

    //DBG("sliderPos before drawLinearSlider: " << normalizedSliderPos);
    //DBG("minSliderPos before drawLinearSlider: " << normalizedMinSliderPos);
    //DBG("maxSliderPos before drawLinearSlider: " << normalizedMaxSliderPos);

    // Draw the slider background
    g.setColour(backgroundColour);
    g.fillRect(x, y, width, height);

    // Calculate the bounds for the thumb
    int thumbWidth = 20; // Adjust the thumb width as neededr
    int thumbXPos = static_cast<int>(juce::jmap(normalizedSliderPos, normalizedMinSliderPos, normalizedMaxSliderPos, static_cast<float>(x), static_cast<float>(x + width - thumbWidth)));
    juce::Rectangle<int> thumbBounds(thumbXPos, y, thumbWidth, height);

    // Create a Path for the thumb
    juce::Path thumbPath;
    thumbPath.addRoundedRectangle(static_cast<float>(thumbBounds.getX()), static_cast<float>(thumbBounds.getY()), static_cast<float>(thumbBounds.getWidth()), static_cast<float>(thumbBounds.getHeight()), 4.0f); // Adjust the corner radius as needed

    // Render the drop shadow
    shadow.render(g, thumbPath);

    // Draw the thumb
    g.setColour(thumbColour);
    g.fillPath(thumbPath);

    // Draw the slider track
    g.setColour(trackColour);
    g.fillRect(x, y + height / 2 - 2, width, 4); // Adjust the track height as needed
}

