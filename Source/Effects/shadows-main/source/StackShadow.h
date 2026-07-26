#pragma once
#include <JuceHeader.h>

namespace shadows
{

struct StackShadow
{
    /** Creates a default stack-shadow effect. */
    StackShadow() = default;

    /** Creates a stack-shadow object with the given parameters. */
    StackShadow (juce::Colour shadowColour, juce::Point<int> offset, int blur, int spread) noexcept;

    /** Renders a stack-shadow-based outer shadow on the shape of a path. */
    void drawOuterShadowForPath (juce::Graphics& g, const juce::Path& path) const;

    void drawFixedOuterShadowForPath(juce::Graphics& g, const juce::Path& path) const;

    /** Renders a stack-shadow-based inner shadow on the shape of a path. */
    void drawInnerShadowForPath (juce::Graphics& g, const juce::Path& path) const;

    void blurImage(juce::Image& img, unsigned int radius);

    /** The colour with which to render the shadow. */
    juce::Colour colour { juce::Colours::black.withAlpha (0.05f) };

    /** The offset of the shadow. */
    juce::Point<int> offset { 0, 0 };

    /** The ammount of blur of the shadow. */
    int blur { 4 };

    /** The spread of the shadow. */
    int spread { 0 };
};

} // namespace shadows
