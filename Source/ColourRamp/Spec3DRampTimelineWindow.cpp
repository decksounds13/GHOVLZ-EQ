#include "Spec3DRampTimelineWindow.h"
#include "../Menu/SharedResources.h"

Spec3DRampTimelineWindow::Spec3DRampTimelineWindow (SharedResources& resourcesIn,
                                                    ColourRampBank& bank,
                                                    Spec3DRampSequence& sequence)
    : resources (resourcesIn),
      theme (&resourcesIn),
      timeline (resourcesIn, bank, sequence)
{
    setOpaque (false);
    setVisible (false);

    timeline.setExpandedLayout (true);
    timeline.setShowExpandButton (false);
    timeline.onSequenceChanged = [this]
    {
        if (onSequenceChanged != nullptr)
            onSequenceChanged();
    };
    timeline.onEnabledChanged = [this]
    {
        if (onEnabledChanged != nullptr)
            onEnabledChanged();
    };
    addAndMakeVisible (timeline);

    closeButton.setButtonText (juce::String::charToString ((juce::juce_wchar) 0x00D7));
    closeButton.setTooltip ("Close");
    closeButton.onClick = [this]
    {
        setVisible (false);
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (closeButton);

    constrainer.setMinimumSize (420, 160);
    constrainer.setMaximumSize (1600, 600);
    resizer = std::make_unique<juce::ResizableBorderComponent> (this, &constrainer);
    resizer->setBorderThickness ({ 5, 5, 5, 5 });
    addAndMakeVisible (*resizer);

    setSize (560, 180);
}

Spec3DRampTimelineWindow::~Spec3DRampTimelineWindow() = default;

void Spec3DRampTimelineWindow::setThemeColors (SharedResources* r) noexcept
{
    theme = r != nullptr ? r : &resources;
    timeline.setThemeColors (theme);
    repaint();
}

void Spec3DRampTimelineWindow::paint (juce::Graphics& g)
{
    const auto& c = (theme != nullptr ? theme : &resources)->sharedColors;
    juce::Path panel;
    panel.addRoundedRectangle (getLocalBounds().toFloat().reduced (2.0f), kCornerRadius);
    if (SharedResources::glowShadowEffectsEnabled())
        panelShadow.render (g, panel);

    g.setColour (c.menuBackgroundGradientColor1.withAlpha (0.96f));
    g.fillPath (panel);

    auto bar = getLocalBounds().removeFromTop (kDragBarH).toFloat().reduced (2.0f, 0.0f);
    g.setColour (c.menuBackgroundGradientColor1.brighter (0.08f).withAlpha (0.95f));
    g.fillRoundedRectangle (bar.getX() + 1.0f, bar.getY() + 1.0f,
                            bar.getWidth() - 2.0f, bar.getHeight() + 6.0f, 10.0f);

    g.setColour (c.menuLabelTextColor1.withAlpha (0.9f));
    g.setFont (juce::FontOptions().withHeight (13.0f).withStyle ("Bold"));
    auto title = getLocalBounds().removeFromTop (kDragBarH).reduced (12, 0);
    title.removeFromRight (30);
    g.drawText ("Spec3D Ramp Timeline", title, juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.strokePath (panel, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.strokePath (panel, juce::PathStrokeType (1.0f),
                  juce::AffineTransform::translation (0.0f, 1.0f));
}

void Spec3DRampTimelineWindow::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto bar = r.removeFromTop (kDragBarH);
    closeButton.setBounds (bar.removeFromRight (24).reduced (2, 2));
    closeButton.toFront (false);

    const auto& c = (theme != nullptr ? theme : &resources)->sharedColors;
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, c.menuLabelTextColor1.withAlpha (0.9f));

    timeline.setBounds (r.reduced (4, 2));
    if (resizer != nullptr)
    {
        resizer->setBounds (getLocalBounds());
        resizer->toFront (false);
    }
    closeButton.toFront (false);
}

void Spec3DRampTimelineWindow::mouseDown (const juce::MouseEvent& e)
{
    dragging = e.position.y < (float) kDragBarH && ! closeButton.getBounds().contains (e.getPosition());
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void Spec3DRampTimelineWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
        dragger.dragComponent (this, e, &constrainer);
}

void Spec3DRampTimelineWindow::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

void Spec3DRampTimelineWindow::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (e.position.y < (float) kDragBarH ? juce::MouseCursor::DraggingHandCursor
                                                     : juce::MouseCursor::NormalCursor);
}
