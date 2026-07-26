#pragma once

#include <JuceHeader.h>

namespace KnobBandHighlight
{
    inline constexpr const char* propertyID = "bandHighlight";

    inline bool isActive (const juce::Slider& slider)
    {
        return (bool) slider.getProperties().getWithDefault (propertyID, false);
    }

    inline void setActive (juce::Slider& slider, bool shouldHighlight)
    {
        slider.getProperties().set (propertyID, shouldHighlight);
        slider.repaint();
    }

    /** Makes orange ring colours ~10% more intense when a band is being manipulated. */
    inline juce::Colour intensify (juce::Colour colour, bool shouldHighlight)
    {
        if (! shouldHighlight)
            return colour;

        return colour.withMultipliedSaturation (1.1f).withMultipliedBrightness (1.1f);
    }
}
