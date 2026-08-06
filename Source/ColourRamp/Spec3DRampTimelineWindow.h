#pragma once

#include <JuceHeader.h>
#include "Spec3DRampTimelineComponent.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include <functional>

class ColourRampBank;
class SharedResources;

/** Soft-framed floating host for Spec3DRampTimelineComponent (Expand from menu). */
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

    std::function<void()> onClose;
    std::function<void()> onSequenceChanged;
    std::function<void()> onEnabledChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    static constexpr int kDragBarH = 26;
    static constexpr float kCornerRadius = 12.0f;

    SharedResources& resources;
    SharedResources* theme = nullptr;
    Spec3DRampTimelineComponent timeline;
    juce::TextButton closeButton;
    juce::ComponentDragger dragger;
    juce::ComponentBoundsConstrainer constrainer;
    std::unique_ptr<juce::ResizableBorderComponent> resizer;
    bool dragging = false;

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spec3DRampTimelineWindow)
};
