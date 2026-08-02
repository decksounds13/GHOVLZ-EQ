#include "CustomScrollBar.h"

CustomScrollBar::CustomScrollBar (juce::ScrollBar& scrollBarToAttachTo,
                                  Orientation orientationIn)
    : scrollBar (scrollBarToAttachTo),
      orientation (orientationIn),
      customShadow (std::make_unique<shadows::StackShadow> (juce::Colours::black.withAlpha (0.65f),
                                                            juce::Point<int> (0, -2), 5, 1))
{
    setInterceptsMouseClicks (true, false);
    scrollBar.addListener (this);
    updateThumbPosition();
}

CustomScrollBar::CustomScrollBar (juce::ListBox& listBoxToAttachTo)
    : CustomScrollBar (listBoxToAttachTo.getVerticalScrollBar(), Orientation::vertical)
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
    if (orientation == Orientation::horizontal)
    {
        const int thumbW = juce::jmax (10, static_cast<int> (getWidth() * thumbSize));
        const int maxX = juce::jmax (0, getWidth() - thumbW);
        const int thumbX = juce::jlimit (0, maxX, static_cast<int> (std::round (maxX * thumbPosition)));
        return { thumbX, 0, thumbW, getHeight() };
    }

    const int thumbHeight = juce::jmax (10, static_cast<int> (getHeight() * thumbSize));
    const int maxY = juce::jmax (0, getHeight() - thumbHeight);
    const int thumbY = juce::jlimit (0, maxY, static_cast<int> (std::round (maxY * thumbPosition)));
    return { 0, thumbY, getWidth(), thumbHeight };
}

void CustomScrollBar::setRangeStartFromThumbPos (int thumbPos)
{
    const double maxStart = getMaxRangeStart();
    const int trackLen = orientation == Orientation::horizontal ? getWidth() : getHeight();
    const int thumbLen = juce::jmax (10, static_cast<int> (trackLen * thumbSize));
    const int maxPos = juce::jmax (0, trackLen - thumbLen);

    if (maxPos <= 0 || maxStart <= 0.0)
    {
        scrollBar.setCurrentRangeStart (0.0);
        return;
    }

    const double proportion = juce::jlimit (0.0, 1.0, (double) thumbPos / (double) maxPos);
    scrollBar.setCurrentRangeStart (proportion * maxStart);
}

void CustomScrollBar::mouseDown (const juce::MouseEvent& event)
{
    if (getMaxRangeStart() <= 0.0)
        return;

    const auto thumb = getThumbBounds();
    const int clickPos = orientation == Orientation::horizontal ? event.x : event.y;
    const int thumbStart = orientation == Orientation::horizontal ? thumb.getX() : thumb.getY();
    const int thumbLen = orientation == Orientation::horizontal ? thumb.getWidth() : thumb.getHeight();

    if (thumb.contains (event.getPosition()))
    {
        isDragging = true;
        dragGrabOffset = clickPos - thumbStart;
    }
    else
    {
        setRangeStartFromThumbPos (clickPos - thumbLen / 2);
        isDragging = true;
        dragGrabOffset = thumbLen / 2;
        updateThumbPosition();
    }
}

void CustomScrollBar::mouseDrag (const juce::MouseEvent& event)
{
    if (! isDragging)
        return;

    const int clickPos = orientation == Orientation::horizontal ? event.x : event.y;
    setRangeStartFromThumbPos (clickPos - dragGrabOffset);
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
    const float axis = orientation == Orientation::horizontal
                           ? (std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY)
                           : wheel.deltaY;
    const double delta = (wheel.isReversed ? axis : -axis) * page * 0.35;
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
