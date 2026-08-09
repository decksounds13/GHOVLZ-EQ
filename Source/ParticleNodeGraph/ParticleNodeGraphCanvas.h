#pragma once

#include "ParticleNodeGraphModel.h"
#include "ParticleNodeGraphCompiler.h"
#include <JuceHeader.h>

class Spectrogram3DComponent;
class SharedResources;

namespace ParticleNodeGraph
{

/**
    Interactive node canvas: grid, pan (MMB / space+LMB), zoom (wheel),
    box-select, wire drag with type validation, right-click create menu.
*/
class ParticleNodeGraphCanvas : public juce::Component,
                                private juce::Timer
{
public:
    ParticleNodeGraphCanvas();
    ~ParticleNodeGraphCanvas() override;

    GraphModel& model() noexcept { return model_; }
    const GraphModel& model() const noexcept { return model_; }

    void setTheme (SharedResources* r) noexcept { theme_ = r; repaint(); }
    void setTarget (Spectrogram3DComponent* s) noexcept { target_ = s; }

    void applyGraphToTarget();
    void loadDefaultGraph();

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    bool keyPressed (const juce::KeyPress& key) override;

    std::function<void()> onGraphApplied;
    std::function<void(const juce::String&)> onStatus;

private:
    void timerCallback() override;
    juce::Point<float> screenToCanvas (juce::Point<float> screen) const;
    juce::Point<float> canvasToScreen (juce::Point<float> canvas) const;
    void drawGrid (juce::Graphics& g) const;
    void drawWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                   juce::Colour col, float thickness) const;
    void drawNode (juce::Graphics& g, const GraphNode& n) const;
    void showCreateMenu (juce::Point<int> screenPos, juce::Point<float> canvasPos);
    void showNodeContextMenu (uint32_t nodeId, juce::Point<int> screenPos);
    void updateHover (juce::Point<float> canvasPos);
    void arrangeGraph();

    GraphModel model_;
    SharedResources* theme_ = nullptr;
    Spectrogram3DComponent* target_ = nullptr;

    float viewScale_ = 1.0f;
    juce::Point<float> viewOffset_ { 0, 0 };

    enum class DragMode { none, pan, moveNodes, wire, boxSelect };
    DragMode dragMode_ = DragMode::none;
    juce::Point<float> dragStartScreen_;
    juce::Point<float> dragStartCanvas_;
    juce::Point<float> lastCanvas_;
    juce::Point<float> viewOffsetAtDragStart_;
    std::optional<PinHit> wireFrom_;
    juce::Point<float> wireCursor_;
    juce::Rectangle<float> boxSelectRect_;
    std::optional<PinHit> hoverPin_;
    uint32_t hoverNode_ = 0;
    uint32_t hoverWire_ = 0;
    juce::String hoverTip_;
    juce::Point<int> hoverTipPos_;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleNodeGraphCanvas)
};

class ParticleNodeGraphWindow : public juce::DocumentWindow
{
public:
    ParticleNodeGraphWindow (Spectrogram3DComponent& target, SharedResources* theme);
    ~ParticleNodeGraphWindow() override;

    ParticleNodeGraphCanvas* getCanvas() noexcept;
    void closeButtonPressed() override;

    std::function<void()> onClosed;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleNodeGraphWindow)
};

} // namespace ParticleNodeGraph
