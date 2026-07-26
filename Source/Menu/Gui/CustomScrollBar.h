#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"
#include "shadows-main/shadows.h"

class CustomScrollBar : public juce::Component,
                        public juce::ScrollBar::Listener
{
public:
    explicit CustomScrollBar (juce::ScrollBar& scrollBarToAttachTo);
    explicit CustomScrollBar (juce::ListBox& listBoxToAttachTo);
    ~CustomScrollBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& event) override;
    void mouseDrag (const juce::MouseEvent& event) override;
    void mouseUp (const juce::MouseEvent& event) override;
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    void updateThumbPosition();
    void scrollBarMoved (juce::ScrollBar* scrollBarThatMoved, double newRangeStart) override;

    void setThumbBackgroundColour (const juce::Colour& colour);
    void setThumbOutlineColour (const juce::Colour& colour);
    void setTrackBackgroundColour (const juce::Colour& colour);

private:
    juce::ScrollBar& scrollBar;
    double thumbPosition = 0.0;
    double thumbSize = 1.0;
    bool isDragging = false;
    int dragGrabOffsetY = 0;

    juce::Colour thumbBackgroundColour = juce::Colour::fromRGB (200, 200, 200);
    juce::Colour thumbOutlineColour = juce::Colour::fromRGB (10, 10, 10);
    juce::Colour trackBackgroundColour = juce::Colour::fromRGB (100, 100, 100);

    juce::Rectangle<int> getThumbBounds() const;
    void setRangeStartFromThumbY (int thumbY);
    double getMaxRangeStart() const;

    melatonin::InnerShadow innerShadow1 = { { juce::Colours::black.withAlpha (0.8f), 4, { 2, 0 } } };
    melatonin::InnerShadow innerShadow2 = { { juce::Colours::black, 4, { 2, 0 } } };

    std::unique_ptr<shadows::StackShadow> customShadow;
};
