#pragma once

#include <JuceHeader.h>
#include "GradientRamp.h"

/** Full-plugin transparent overlay: click-drag samples UI pixels under the path. */
class PathSampleOverlay : public juce::Component
{
public:
    PathSampleOverlay();

    std::function<void (GradientRamp)> onSampled;
    std::function<void()> onCancelled;

    void beginSession (juce::Component& sampleRoot);
    void endSession();

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

private:
    GradientRamp buildRampFromPath() const;

    juce::Component::SafePointer<juce::Component> root;
    std::vector<juce::Point<float>> pathPoints;
    bool dragging = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PathSampleOverlay)
};
