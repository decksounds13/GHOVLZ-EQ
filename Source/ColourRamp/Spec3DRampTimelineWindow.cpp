#include "Spec3DRampTimelineWindow.h"
#include "../Menu/SharedResources.h"

namespace
{
    class SoftResizeCorner final : public juce::ResizableCornerComponent
    {
    public:
        SoftResizeCorner (juce::Component* componentToResize,
                          juce::ComponentBoundsConstrainer* constrainerToUse)
            : juce::ResizableCornerComponent (componentToResize, constrainerToUse)
        {
            setOpaque (false);
            setMouseCursor (juce::MouseCursor::BottomRightCornerResizeCursor);
        }

        void paint (juce::Graphics& g) override
        {
            auto b = getLocalBounds().toFloat().reduced (2.5f);
            const bool hot = isMouseOverOrDragging();
            const float alpha = hot ? 0.85f : 0.50f;
            g.setColour (juce::Colours::whitesmoke.withAlpha (alpha));
            for (int i = 0; i < 3; ++i)
            {
                const float o = 3.0f + (float) i * 3.6f;
                g.drawLine (b.getRight() - o, b.getBottom(),
                            b.getRight(), b.getBottom() - o, hot ? 1.6f : 1.35f);
            }
            g.setColour (juce::Colours::goldenrod.withAlpha (hot ? 0.55f : 0.28f));
            g.drawLine (b.getRight() - 5.0f, b.getBottom(),
                        b.getRight(), b.getBottom() - 5.0f, 1.1f);
        }
    };
}

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
    setAlwaysOnTop (true); // among soft peers (same as framed Osc/Gon/Spec)

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
        setFrameActive (false);
        if (onClose != nullptr)
            onClose();
    };
    addAndMakeVisible (closeButton);

    setResizeLimits (1600, 600);
    resizer = std::make_unique<SoftResizeCorner> (this, &constrainer);
    addAndMakeVisible (*resizer);
    resizer->setAlwaysOnTop (true);

    setSize (560, 180);
}

Spec3DRampTimelineWindow::~Spec3DRampTimelineWindow() = default;

void Spec3DRampTimelineWindow::setThemeColors (SharedResources* r) noexcept
{
    theme = r != nullptr ? r : &resources;
    timeline.setThemeColors (theme);
    repaint();
}

void Spec3DRampTimelineWindow::setResizeLimits (int maxW, int maxH) noexcept
{
    const int pad = kShadowPad * 2;
    constrainer.setMinimumSize (kMinW + pad / 2, kMinH + pad / 2);
    constrainer.setMaximumSize (juce::jmax (kMinW, maxW),
                                juce::jmax (kMinH, maxH));
}

void Spec3DRampTimelineWindow::setMovementBounds (juce::Rectangle<int> parentLocalArea) noexcept
{
    movementArea = parentLocalArea;
    constrainer.area = parentLocalArea;
    // Same on-screen margin spirit as FramedFloatingScopeWindow::setMovementBounds.
    constrainer.setMinimumOnscreenAmounts (24, 24, 24, 24);
}

void Spec3DRampTimelineWindow::clampToMovementArea() noexcept
{
    if (! frameActive || movementArea.getWidth() < 32 || movementArea.getHeight() < 32)
        return;

    auto b = getBounds();
    b.setWidth  (juce::jmin (b.getWidth(),  movementArea.getWidth()));
    b.setHeight (juce::jmin (b.getHeight(), movementArea.getHeight()));
    // Prefer keeping min size when area allows.
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

void Spec3DRampTimelineWindow::setFrameActive (bool shouldBeActive) noexcept
{
    if (frameActive == shouldBeActive)
    {
        if (shouldBeActive)
        {
            setVisible (true);
            toFront (true);
        }
        return;
    }

    frameActive = shouldBeActive;
    setVisible (frameActive);
    if (resizer != nullptr)
        resizer->setVisible (frameActive);
    if (frameActive)
        toFront (true);
    resized();
    repaint();
}

void Spec3DRampTimelineWindow::paint (juce::Graphics& g)
{
    const auto& c = (theme != nullptr ? theme : &resources)->sharedColors;
    juce::Path panel;
    panel.addRoundedRectangle (getLocalBounds().toFloat().reduced ((float) kShadowPad * 0.35f),
                               kCornerRadius);
    if (SharedResources::glowShadowEffectsEnabled())
        panelShadow.render (g, panel);

    // Match framed floating scopes: soft osc panel fill.
    const auto fill = theme != nullptr
                          ? theme->sharedColors.oscBackground.withAlpha (90.0f / 255.0f)
                          : c.menuBackgroundGradientColor1.withAlpha (0.96f);
    g.setColour (fill);
    g.fillPath (panel);

    auto bar = getLocalBounds().reduced (kShadowPad / 2, kShadowPad / 2).removeFromTop (kDragBarH).toFloat();
    g.setColour (c.menuBackgroundGradientColor1.brighter (0.08f).withAlpha (0.92f));
    g.fillRoundedRectangle (bar.reduced (1.0f, 0.0f), 8.0f);

    g.setColour (c.menuLabelTextColor1.withAlpha (0.9f));
    g.setFont (juce::FontOptions().withHeight (13.0f).withStyle ("Bold"));
    auto title = getLocalBounds().reduced (kShadowPad / 2, kShadowPad / 2).removeFromTop (kDragBarH);
    title.removeFromRight (30);
    title.reduce (10, 0);
    g.drawText ("Spec3D Ramp Timeline", title, juce::Justification::centredLeft, false);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.strokePath (panel, juce::PathStrokeType (1.2f));
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.strokePath (panel, juce::PathStrokeType (1.0f),
                  juce::AffineTransform::translation (0.0f, 1.0f));
}

void Spec3DRampTimelineWindow::resized()
{
    auto r = getLocalBounds().reduced (kShadowPad / 2);
    auto bar = r.removeFromTop (kDragBarH);
    closeButton.setBounds (bar.removeFromRight (24).reduced (2, 2));
    closeButton.toFront (false);

    const auto& c = (theme != nullptr ? theme : &resources)->sharedColors;
    closeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
    closeButton.setColour (juce::TextButton::textColourOffId, c.menuLabelTextColor1.withAlpha (0.9f));

    timeline.setBounds (r.reduced (4, 2));
    if (resizer != nullptr)
    {
        const int pad = kShadowPad;
        resizer->setBounds (getWidth() - pad, getHeight() - pad, pad, pad);
        resizer->setVisible (frameActive);
        resizer->toFront (false);
    }
    closeButton.toFront (false);

    if (onUserMovedOrResized != nullptr && resizer != nullptr && resizer->isMouseButtonDown())
        onUserMovedOrResized();
}

bool Spec3DRampTimelineWindow::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        setFrameActive (false);
        if (onClose != nullptr)
            onClose();
        return true;
    }
    return false;
}

void Spec3DRampTimelineWindow::mouseDown (const juce::MouseEvent& e)
{
    dragging = e.position.y < (float) (kDragBarH + kShadowPad / 2)
               && ! closeButton.getBounds().contains (e.getPosition());
    if (dragging)
        dragger.startDraggingComponent (this, e);
}

void Spec3DRampTimelineWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging)
    {
        dragger.dragComponent (this, e, &constrainer);
        if (onUserMovedOrResized != nullptr)
            onUserMovedOrResized();
    }
}

void Spec3DRampTimelineWindow::mouseUp (const juce::MouseEvent&)
{
    dragging = false;
}

void Spec3DRampTimelineWindow::mouseMove (const juce::MouseEvent& e)
{
    const bool overBar = e.position.y < (float) (kDragBarH + kShadowPad / 2);
    setMouseCursor (overBar ? juce::MouseCursor::DraggingHandCursor
                            : juce::MouseCursor::NormalCursor);
}
