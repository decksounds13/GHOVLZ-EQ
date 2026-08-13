#include "CustomHeader2.h"

CustomHeader2::CustomHeader2 (SharedResources& sharedResourcesIn)
    : sharedResources (sharedResourcesIn),
      customShadow (std::make_unique<shadows::StackShadow> (juce::Colours::black.withAlpha (0.65f), juce::Point<int> (0, 2), 6, 1))
{
}

CustomHeader2::~CustomHeader2() = default;

void CustomHeader2::setSortIndicator (bool active, bool ascending)
{
    sortActive = active;
    sortAscending = ascending;
    repaint();
}

void CustomHeader2::mouseUp (const juce::MouseEvent& event)
{
    if (event.mouseWasClicked() && getLocalBounds().contains (event.getPosition()))
        listeners.call ([] (Listener& l) { l.elementNameHeaderClicked(); });
}

void CustomHeader2::paint (juce::Graphics& g)
{
    float cornerSize = 14.0f;
    juce::Rectangle<int> parentBounds = getParentComponent()->getLocalBounds();
    juce::Rectangle<float> headerBounds = parentBounds.expanded (1.0f).toFloat();

    juce::Path headerPath;
    headerPath.addRoundedRectangle (headerBounds, cornerSize);

    g.saveState();
    g.reduceClipRegion (headerPath);

    juce::ColourGradient gradient (
        sharedResources.sharedColors.menuListBoxBackgroundGradientColor1,
        juce::Point<float> ((float) getWidth() / 2.0f, (float) getHeight() * -1.5f),
        sharedResources.sharedColors.menuListBoxBackgroundGradientColor2,
        juce::Point<float> ((float) getWidth() / 2.0f, (float) getHeight() * 1.5f),
        false);

    g.setGradientFill (gradient);
    g.fillRect (headerBounds);
    customShadow->drawInnerShadowForPath (g, headerPath);

    g.setFont (SharedResources::uiFont (14.0f, true));
    g.setColour (sharedResources.sharedColors.menuLabelTextColor1);

    juce::String label = "Element Name";
    if (sortActive)
        label += sortAscending ? "  A-Z" : "  Z-A";
    g.drawText (label, 15, 0, getWidth() - 20, getHeight(), juce::Justification::centredLeft);

    g.restoreState();
}

void CustomHeader2::resized()
{
    repaint();
}
