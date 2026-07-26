static void renderDropShadow(juce::Graphics& g, const juce::Path& path, juce::Colour color, const int radius = 1, const juce::Point<int> offset = { 0, 0 }, int spread = 0)
{
    if (radius < 1)
        return;

    auto area = (path.getBounds().getSmallestIntegerContainer() + offset)
        .expanded(radius + spread + 1)
        .getIntersection(g.getClipBounds().expanded(radius + spread + 1));

    if (area.getWidth() < 2 || area.getHeight() < 2)
        return;

    // spread enlarges or shrinks the path before blurring it
    auto spreadPath = juce::Path(path);
    if (spread != 0)
    {
        area.expand(spread, spread);
        auto bounds = path.getBounds().expanded(spread);
        spreadPath.scaleToFit(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), true);
    }

    juce::Image renderedPath(juce::Image::SingleChannel, area.getWidth(), area.getHeight(), true);

    juce::Graphics g2(renderedPath);
    g2.setColour(juce::Colours::white);
    g2.fillPath((spread != 0) ? spreadPath : path, juce::AffineTransform::translation((float)(offset.x - area.getX()), (float)(offset.y - area.getY())));
    applyStackBlur(renderedPath, radius);

    g.setColour(color);
    g.drawImageAt(renderedPath, area.getX(), area.getY(), true);
}