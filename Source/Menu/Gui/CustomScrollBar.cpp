#include "CustomScrollBar.h"

CustomScrollBar::CustomScrollBar (juce::ScrollBar& scrollBarToAttachTo)
    : scrollBar (scrollBarToAttachTo),
      customShadow (std::make_unique<shadows::StackShadow> (juce::Colours::black.withAlpha (0.65f),
                                                            juce::Point<int> (0, -2), 5, 1))
{
    setInterceptsMouseClicks (true, false);
    scrollBar.addListener (this);
    updateThumbPosition();
}

CustomScrollBar::CustomScrollBar (juce::ListBox& listBoxToAttachTo)
    : CustomScrollBar (listBoxToAttachTo.getVerticalScrollBar())
{
    listBoxToAttachTo.setRepaintsOnMouseActivity (false);

    if (auto* viewport = listBoxToAttachTo.getViewport())
    {
        viewport->setScrollBarsShown (false, false);
        viewport->setScrollBarThickness (0);
    }
}

CustomScrollBar::~CustomScrollBar()
{
    scrollBar.removeListener (this);
}

double CustomScrollBar::getMaxRangeStart() const
{
    return juce::jmax (0.0, scrollBar.getMaximumRangeLimit() - scrollBar.getCurrentRangeSize());
}

void CustomScrollBar::paint (juce::Graphics& g)
{
    const float cornerSize = 2.0f;

    juce::Path trackPath;
    trackPath.addRectangle (getLocalBounds().toFloat());

    g.setColour (trackBackgroundColour);
    g.fillPath (trackPath);
    innerShadow2.render (g, trackPath);

    juce::Path thumbPath;
    thumbPath.addRoundedRectangle (getThumbBounds().toFloat(), cornerSize);

    g.setColour (thumbBackgroundColour);
    g.fillPath (thumbPath);
    customShadow->drawInnerShadowForPath (g, thumbPath);
}

void CustomScrollBar::resized()
{
    updateThumbPosition();
}

void CustomScrollBar::updateThumbPosition()
{
    const double visibleRangeSize = scrollBar.getCurrentRangeSize();
    const double maximumRangeLimit = scrollBar.getMaximumRangeLimit();
    const double maxStart = getMaxRangeStart();

    if (maximumRangeLimit > 0.0)
        thumbSize = juce::jlimit (0.12, 1.0, visibleRangeSize / maximumRangeLimit);
    else
        thumbSize = 1.0;

    if (maxStart > 0.0)
        thumbPosition = juce::jlimit (0.0, 1.0, scrollBar.getCurrentRangeStart() / maxStart);
    else
        thumbPosition = 0.0;

    repaint();
}

juce::Rectangle<int> CustomScrollBar::getThumbBounds() const
{
    const int thumbHeight = juce::jmax (10, static_cast<int> (getHeight() * thumbSize));
    const int maxY = juce::jmax (0, getHeight() - thumbHeight);
    const int thumbY = juce::jlimit (0, maxY, static_cast<int> (std::round (maxY * thumbPosition)));
    return { 0, thumbY, getWidth(), thumbHeight };
}

void CustomScrollBar::setRangeStartFromThumbY (int thumbY)
{
    const int thumbHeight = juce::jmax (10, static_cast<int> (getHeight() * thumbSize));
    const int maxY = juce::jmax (0, getHeight() - thumbHeight);
    const double maxStart = getMaxRangeStart();

    if (maxY <= 0 || maxStart <= 0.0)
    {
        scrollBar.setCurrentRangeStart (0.0);
        return;
    }

    const double proportion = juce::jlimit (0.0, 1.0, (double) thumbY / (double) maxY);
    scrollBar.setCurrentRangeStart (proportion * maxStart);
}

void CustomScrollBar::mouseDown (const juce::MouseEvent& event)
{
    if (getMaxRangeStart() <= 0.0)
        return;

    const auto thumb = getThumbBounds();

    if (thumb.contains (event.getPosition()))
    {
        isDragging = true;
        dragGrabOffsetY = event.y - thumb.getY();
    }
    else
    {
        // Jump so the thumb centres on the click, then begin a drag.
        setRangeStartFromThumbY (event.y - thumb.getHeight() / 2);
        isDragging = true;
        dragGrabOffsetY = thumb.getHeight() / 2;
        updateThumbPosition();
    }
}

void CustomScrollBar::mouseDrag (const juce::MouseEvent& event)
{
    if (! isDragging)
        return;

    setRangeStartFromThumbY (event.y - dragGrabOffsetY);
}

void CustomScrollBar::mouseUp (const juce::MouseEvent&)
{
    isDragging = false;
}

void CustomScrollBar::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    juce::ignoreUnused (event);

    if (getMaxRangeStart() <= 0.0)
        return;

    const double page = scrollBar.getCurrentRangeSize();
    const double delta = (wheel.isReversed ? wheel.deltaY : -wheel.deltaY) * page * 0.35;
    scrollBar.setCurrentRangeStart (juce::jlimit (0.0, getMaxRangeStart(),
                                                   scrollBar.getCurrentRangeStart() + delta));
}

void CustomScrollBar::scrollBarMoved (juce::ScrollBar* scrollBarThatMoved, double)
{
    if (scrollBarThatMoved == &scrollBar)
        updateThumbPosition();
}

void CustomScrollBar::setThumbBackgroundColour (const juce::Colour& colour)
{
    thumbBackgroundColour = colour;
    repaint();
}

void CustomScrollBar::setThumbOutlineColour (const juce::Colour& colour)
{
    juce::ignoreUnused (colour);
}

void CustomScrollBar::setTrackBackgroundColour (const juce::Colour& colour)
{
    trackBackgroundColour = colour;
    repaint();
}
