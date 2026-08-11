#pragma once

#include <JuceHeader.h>
#include "Spec3DRampTimelineComponent.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include <functional>

class ColourRampBank;
class SharedResources;

/**
    Floating host for Spec3DRampTimelineComponent when Expanded from Look menu.

    Treated like maximized analyser overlays (Osc / Gon / Spec frames):
    soft child of MainComponent, same available-area clamp / tool-column inset,
    drag + corner resize constrained to that rect — not a separate desktop peer.
*/
class Spec3DRampTimelineWindow : public juce::Component
{
public:
    Spec3DRampTimelineWindow (SharedResources& resources,
                              ColourRampBank& bank,
                              Spec3DRampSequence& sequence);
    ~Spec3DRampTimelineWindow() override;

    Spec3DRampTimelineComponent& getTimeline() noexcept { return timeline; }

    void setThemeColors (SharedResources* r) noexcept;
    void setPlayheadSec (float sec) noexcept { timeline.setPlayheadSec (sec); }

    /** Match FramedFloatingScopeWindow resize limits (parent-local max size). */
    void setResizeLimits (int maxW, int maxH) noexcept;

    /** Parent-local movement / size clamp rect (framed-scope available area). */
    void setMovementBounds (juce::Rectangle<int> parentLocalArea) noexcept;

    /** Pin size+position inside the current movement area (after host layout). */
    void clampToMovementArea() noexcept;

    bool isFrameActive() const noexcept { return frameActive; }
    void setFrameActive (bool shouldBeActive) noexcept;

    std::function<void()> onClose;
    std::function<void()> onSequenceChanged;
    std::function<void()> onEnabledChanged;
    std::function<void()> onUserMovedOrResized;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    static constexpr int kDragBarH = 26;
    static constexpr float kCornerRadius = 12.0f;
    static constexpr int kMinW = 420;
    static constexpr int kMinH = 160;
    static constexpr int kShadowPad = 14;

    /** Keeps drag/resize inside parent-local movementBounds (like framed scopes). */
    struct AreaConstrainer final : public juce::ComponentBoundsConstrainer
    {
        juce::Rectangle<int> area;

        void checkBounds (juce::Rectangle<int>& bounds,
                          const juce::Rectangle<int>& previousBounds,
                          const juce::Rectangle<int>& limits,
                          bool isStretchingTop,
                          bool isStretchingLeft,
                          bool isStretchingBottom,
                          bool isStretchingRight) override
        {
            juce::ComponentBoundsConstrainer::checkBounds (bounds, previousBounds, limits,
                                                           isStretchingTop, isStretchingLeft,
                                                           isStretchingBottom, isStretchingRight);
            if (area.getWidth() < 32 || area.getHeight() < 32)
                return;

            bounds.setWidth  (juce::jmin (bounds.getWidth(),  area.getWidth()));
            bounds.setHeight (juce::jmin (bounds.getHeight(), area.getHeight()));
            const int maxX = juce::jmax (area.getX(), area.getRight()  - bounds.getWidth());
            const int maxY = juce::jmax (area.getY(), area.getBottom() - bounds.getHeight());
            bounds.setX (juce::jlimit (area.getX(), maxX, bounds.getX()));
            bounds.setY (juce::jlimit (area.getY(), maxY, bounds.getY()));
        }
    };

    SharedResources& resources;
    SharedResources* theme = nullptr;
    Spec3DRampTimelineComponent timeline;
    juce::TextButton closeButton;
    juce::ComponentDragger dragger;
    AreaConstrainer constrainer;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;
    juce::Rectangle<int> movementArea;
    bool dragging = false;
    bool frameActive = false;

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spec3DRampTimelineWindow)
};
