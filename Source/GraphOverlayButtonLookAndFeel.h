#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"
#include "Menu/SharedResources.h"

/** Slight Melatonin drop under graph / top-chrome TextButtons. */
class GraphOverlayButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (backgroundColour);
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);

        if (SharedResources::glowShadowEffectsEnabled())
        {
            juce::Path p;
            p.addRoundedRectangle (bounds, 3.0f);
            dropShadow.render (g, p);
        }

        auto fill = button.getToggleState()
                        ? button.findColour (juce::TextButton::buttonOnColourId)
                        : button.findColour (juce::TextButton::buttonColourId);
        if (shouldDrawButtonAsDown)
            fill = fill.brighter (0.15f);
        else if (shouldDrawButtonAsHighlighted)
            fill = fill.brighter (0.08f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawRoundedRectangle (bounds, 3.0f, 1.0f);
    }

    /** Shared shadow for custom-painted chrome (OscToolButton, ParamChoiceButton, …). */
    static void renderRoundedDrop (juce::Graphics& g, juce::Rectangle<float> bounds, float corner = 3.0f)
    {
        if (! SharedResources::glowShadowEffectsEnabled())
            return;
        juce::Path p;
        p.addRoundedRectangle (bounds, corner);
        sharedDrop().render (g, p);
    }

private:
    melatonin::DropShadow dropShadow {
        { juce::Colours::black.withAlpha (0.42f), 5, { 0, 2 }, 0 }
    };

    static melatonin::DropShadow& sharedDrop()
    {
        static melatonin::DropShadow s {
            { juce::Colours::black.withAlpha (0.42f), 5, { 0, 2 }, 0 }
        };
        return s;
    }
};
