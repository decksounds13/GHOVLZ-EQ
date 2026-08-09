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
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);
    setAlwaysOnTop (true);

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

    constrainer.setMinimumSize (kMinW, kMinH);
    constrainer.setMaximumSize (1600, 600);
    // Corner grip only — a full-window ResizableBorder steals title-bar mouse hits.
    resizer = std::make_unique<juce::ResizableCornerComponent> (this, &constrainer);
    addAndMakeVisible (*resizer);
    resizer->setAlwaysOnTop (true);

    setSize (720, 200);
}

Spec3DRampTimelineWindow::~Spec3DRampTimelineWindow() = default;

void Spec3DRampTimelineWindow::setThemeColors (SharedResources* r) noexcept
{
    theme = r != nullptr ? r : &resources;
    timeline.setThemeColors (theme);
    repaint();
}

void Spec3DRampTimelineWindow::setMovementArea (juce::Rectangle<int> parentLocalArea) noexcept
{
    movementArea = parentLocalArea;
    constrainer.area = parentLocalArea;

    if (parentLocalArea.getWidth() >= 32 && parentLocalArea.getHeight() >= 32)
    {
        constrainer.setMaximumSize (juce::jmax (kMinW, parentLocalArea.getWidth()),
                                    juce::jmax (kMinH, parentLocalArea.getHeight()));
    }
}

void Spec3DRampTimelineWindow::clampToMovementArea() noexcept
{
    if (! isVisible() || movementArea.getWidth() < 32 || movementArea.getHeight() < 32)
        return;

    auto b = getBounds();
    b.setWidth  (juce::jmin (b.getWidth(),  movementArea.getWidth()));
    b.setHeight (juce::jmin (b.getHeight(), movementArea.getHeight()));
    if (movementArea.getWidth()  >= kMinW)
        b.setWidth  (juce::jmax (kMinW, b.getWidth()));
    if (movementArea.getHeight() >= kMinH)
        b.setHeight (juce::jmax (kMinH, b.getHeight()));
    if (b.getWidth() > movementArea.getWidth())
        b.setWidth (movementArea.getWidth());
    if (b.getHeight() > movementArea.getHeight())
        b.setHeight (movementArea.getHeight());

    const int maxX = juce::jmax (movementArea.getX(), movementArea.getRight()  - b.getWidth());
    const int maxY = juce::jmax (movementArea.getY(), movementArea.getBottom() - b.getHeight());
    b.setX (juce::jlimit (movementArea.getX(), maxX, b.getX()));
    b.setY (juce::jlimit (movementArea.getY(), maxY, b.getY()));
    setBounds (b);
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

    // Title bar (this is the drag surface — not covered by timeline content).
    auto bar = getLocalBounds().removeFromTop (kDragBarH).toFloat().reduced (2.0f, 0.0f);
    g.setColour (c.menuBackgroundGradientColor1.brighter (0.08f).withAlpha (0.95f));
    g.fillRoundedRectangle (bar.getX() + 1.0f, bar.getY() + 1.0f,
                            bar.getWidth() - 2.0f, bar.getHeight() + 4.0f, 10.0f);

    g.setColour (c.menuLabelTextColor1.withAlpha (0.9f));
    g.setFont (juce::FontOptions().withHeight (13.0f).withStyle ("Bold"));
    auto title = getLocalBounds().removeFromTop (kDragBarH).reduced (12, 0);
    title.removeFromRight (30);
    g.drawText ("Spec3D Ramp Timeline", title, juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.strokePath (panel, juce::PathStrokeType (1.2f));
}

void Spec3DRampTimelineWindow::resized()
{
    auto r = getLocalBounds().reduced (4);
    auto bar = r.removeFromTop (kDragBarH);
    // Leave the rest of the bar empty for drag hits (only close is a child on the bar).
    closeButton.setBounds (bar.removeFromRight (26).reduced (2, 2));
    closeButton.toFront (false);

    const auto& c = (theme != nullptr ? theme : &resources)->sharedColors;
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, c.menuLabelTextColor1.withAlpha (0.9f));

    // Timeline sits BELOW the title bar so drag always hits the window, not content.
    timeline.setBounds (r.reduced (4, 2));

    if (resizer != nullptr)
    {
        constexpr int kGrip = 18;
        resizer->setBounds (getWidth() - kGrip, getHeight() - kGrip, kGrip, kGrip);
        resizer->toFront (false);
    }
    closeButton.toFront (false);
}

bool Spec3DRampTimelineWindow::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        setVisible (false);
        if (onClose != nullptr)
            onClose();
        return true;
    }
    return false;
}

void Spec3DRampTimelineWindow::mouseDown (const juce::MouseEvent& e)
{
    // Title bar drag (timeline is laid out below this region).
    dragging = e.position.y < (float) kDragBarH
               && ! closeButton.getBounds().contains (e.getPosition())
               && e.mods.isLeftButtonDown()
               && ! e.mods.isPopupMenu();
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void Spec3DRampTimelineWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;
    dragger.dragComponent (this, e, &constrainer);
    if (onUserMovedOrResized != nullptr)
        onUserMovedOrResized();
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
