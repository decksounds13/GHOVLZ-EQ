#include "FramedFloatingScopeWindow.h"
#include "Menu/SharedResources.h"

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

FramedFloatingScopeWindow::FramedFloatingScopeWindow()
{
    setOpaque (false);
    setVisible (false);
    setInterceptsMouseClicks (true, true);
    setWantsKeyboardFocus (true);

    constrainer.setMinimumSize (180 + kShadowPadFloating * 2, 120 + kShadowPadFloating * 2);
    constrainer.setMaximumSize (4000, 3000);
    resizer = std::make_unique<SoftResizeCorner> (this, &constrainer);
    addAndMakeVisible (*resizer);
    resizer->setAlwaysOnTop (true);
    applyChromeMode();
}

FramedFloatingScopeWindow::~FramedFloatingScopeWindow()
{
    if (contentComp != nullptr && contentComp->getParentComponent() == this)
        removeChildComponent (contentComp);
    resizer.reset();
}

void FramedFloatingScopeWindow::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    repaint();
}

void FramedFloatingScopeWindow::setChromeMode (ChromeMode mode) noexcept
{
    if (chromeMode == mode)
        return;
    chromeMode = mode;
    applyChromeMode();
    resized();
    repaint();
}

void FramedFloatingScopeWindow::applyChromeMode() noexcept
{
    if (resizer != nullptr)
        resizer->setVisible (chromeMode == ChromeMode::floating && frameActive);
}

void FramedFloatingScopeWindow::setSoftFill (bool shouldSoftFill) noexcept
{
    if (softFill == shouldSoftFill)
        return;
    softFill = shouldSoftFill;
    setOpaque (! softFill);
    repaint();
}

void FramedFloatingScopeWindow::setContent (juce::Component* content) noexcept
{
    if (contentComp == content)
        return;

    if (contentComp != nullptr && contentComp->getParentComponent() == this)
        removeChildComponent (contentComp);

    contentComp = content;
    if (contentComp != nullptr)
    {
        addAndMakeVisible (contentComp);
        contentComp->toBack();
    }
    resized();
}

void FramedFloatingScopeWindow::setFrameActive (bool shouldBeActive) noexcept
{
    if (frameActive == shouldBeActive)
        return;
    frameActive = shouldBeActive;
    setVisible (frameActive);
    applyChromeMode();
    resized();
    repaint();
}

void FramedFloatingScopeWindow::setResizeLimits (int maxW, int maxH) noexcept
{
    const int pad = getShadowPad() * 2;
    constrainer.setMinimumSize (180 + pad, 120 + pad);
    constrainer.setMaximumSize (juce::jmax (180 + pad, maxW),
                                juce::jmax (120 + pad, maxH));
}

void FramedFloatingScopeWindow::setMovementBounds (juce::Rectangle<int>) noexcept
{
    constrainer.setMinimumOnscreenAmounts (24, 24, 24, 24);
}

int FramedFloatingScopeWindow::getShadowPad() const noexcept
{
    return getShadowPadForMode (chromeMode);
}

int FramedFloatingScopeWindow::getShadowPadForMode (ChromeMode mode) noexcept
{
    return mode == ChromeMode::docked ? kShadowPadDocked : kShadowPadFloating;
}

juce::Rectangle<int> FramedFloatingScopeWindow::getInnerFrameLocal() const noexcept
{
    return getLocalBounds().reduced (getShadowPad());
}

juce::Rectangle<int> FramedFloatingScopeWindow::getContentLocal() const noexcept
{
    return getInnerFrameLocal().reduced (chromeMode == ChromeMode::docked ? 1 : kContentInset);
}

juce::Colour FramedFloatingScopeWindow::getPanelFillColour() const noexcept
{
    if (theme != nullptr)
    {
        const auto base = softFill ? theme->sharedColors.oscBackground
                                   : theme->sharedColors.pluginBackground.darker (0.15f);
        return softFill ? base.withAlpha (kSoftFillAlpha) : base;
    }
    return softFill ? juce::Colour::fromFloatRGBA (0.06f, 0.07f, 0.09f, kSoftFillAlpha)
                    : juce::Colour (0xff12151a);
}

void FramedFloatingScopeWindow::resized()
{
    const int pad = getShadowPad();
    if (contentComp != nullptr)
        contentComp->setBounds (getContentLocal());

    if (resizer != nullptr)
    {
        resizer->setBounds (getWidth() - pad, getHeight() - pad, pad, pad);
        resizer->setVisible (chromeMode == ChromeMode::floating && frameActive);
        resizer->toFront (false);
    }

    if (onUserResized != nullptr && resizer != nullptr && resizer->isMouseButtonDown())
        onUserResized();
}

void FramedFloatingScopeWindow::paint (juce::Graphics& g)
{
    const auto inner = getInnerFrameLocal().toFloat();
    const float radius = chromeMode == ChromeMode::docked ? 4.0f : kCornerRadius;
    juce::Path panel;
    panel.addRoundedRectangle (inner, radius);

    if (chromeMode == ChromeMode::floating
        && (theme == nullptr || ! theme->disableGlowShadowEffects))
        panelShadow.render (g, panel);

    g.setColour (getPanelFillColour());
    g.fillPath (panel);

    paintInsidePanel (g, panel);

    g.setColour (juce::Colours::white.withAlpha (chromeMode == ChromeMode::docked ? 0.12f : 0.22f));
    g.strokePath (panel, juce::PathStrokeType (chromeMode == ChromeMode::docked ? 1.0f : 1.2f));
    if (chromeMode == ChromeMode::floating)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.strokePath (panel, juce::PathStrokeType (1.0f),
                      juce::AffineTransform::translation (0.0f, 1.0f));
    }
}

bool FramedFloatingScopeWindow::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        if (onEscape != nullptr)
            onEscape();
        return true;
    }
    return false;
}

bool FramedFloatingScopeWindow::isInMoveChrome (juce::Point<int> localPos) const noexcept
{
    if (chromeMode != ChromeMode::floating || ! frameActive)
        return false;
    if (getContentLocal().contains (localPos))
        return false;
    if (resizer != nullptr && resizer->getBounds().contains (localPos))
        return false;
    return getLocalBounds().contains (localPos);
}

void FramedFloatingScopeWindow::mouseDown (const juce::MouseEvent& e)
{
    movingByChrome = isInMoveChrome (e.getPosition()) && e.mods.isLeftButtonDown()
                     && ! e.mods.isPopupMenu();
    if (movingByChrome)
        moveDragger.startDraggingComponent (this, e);
}

void FramedFloatingScopeWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (! movingByChrome)
        return;
    moveDragger.dragComponent (this, e, &constrainer);
    if (onUserMoved != nullptr)
        onUserMoved();
}

void FramedFloatingScopeWindow::mouseUp (const juce::MouseEvent&)
{
    movingByChrome = false;
}

void FramedFloatingScopeWindow::mouseMove (const juce::MouseEvent& e)
{
    setMouseCursor (isInMoveChrome (e.getPosition()) ? juce::MouseCursor::DraggingHandCursor
                                                     : juce::MouseCursor::NormalCursor);
}

void FramedFloatingScopeWindow::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick != nullptr)
        onDoubleClick();
}
