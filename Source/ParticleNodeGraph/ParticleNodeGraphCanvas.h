#pragma once

#include "ParticleNodeGraphModel.h"
#include "ParticleNodeGraphCompiler.h"
#include <JuceHeader.h>
#include <vector>

class Spectrogram3DComponent;
class SharedResources;

namespace ParticleNodeGraph
{

/**
    Interactive node canvas - industry-standard graph editor UX:
    undo/redo, copy/paste/duplicate, grid snap, type-safe wiring,
    pin RMB create+wire, live apply, property edit, save/load.
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

    void applyGraphToTarget (bool quiet = false);
    void loadDefaultGraph();
    void clearGraph();

    bool isLiveApplyEnabled() const noexcept { return liveApply_; }
    void setLiveApplyEnabled (bool on) noexcept;
    bool isSnapEnabled() const noexcept { return snapEnabled_; }
    void setSnapEnabled (bool on) noexcept { snapEnabled_ = on; }

    void fitViewToGraph();
    void fitViewToSelection();
    void arrangeGraph();

    void undo();
    void redo();
    bool canUndo() const noexcept { return ! undoStack_.empty(); }
    bool canRedo() const noexcept { return ! redoStack_.empty(); }
    void pushUndo();

    void copySelection();
    void pasteClipboard();
    void duplicateSelection();
    void cutSelection();

    bool saveGraphToFile (const juce::File& file);
    bool loadGraphFromFile (const juce::File& file);
    void saveGraphDefault();
    void loadGraphDefault();
    static juce::File defaultGraphFile();

    void editNodeProperties (uint32_t nodeId);

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
    std::function<void(LogLevel, const juce::String&)> onLog;
    std::function<void()> onHistoryChanged; // undo/redo availability

private:
    void timerCallback() override;
    void scheduleLiveApply();
    void flushLiveApply();
    void log (LogLevel level, const juce::String& text);
    juce::Point<float> screenToCanvas (juce::Point<float> screen) const;
    juce::Point<float> canvasToScreen (juce::Point<float> canvas) const;
    void drawGrid (juce::Graphics& g) const;
    void drawWire (juce::Graphics& g, juce::Point<float> a, juce::Point<float> b,
                   juce::Colour col, float thickness) const;
    void drawNode (juce::Graphics& g, const GraphNode& n) const;
    void showCreateMenu (juce::Point<int> screenPos, juce::Point<float> canvasPos);
    void showPinCreateMenu (const PinHit& pin, juce::Point<int> screenPos);
    void showNodeContextMenu (uint32_t nodeId, juce::Point<int> screenPos);
    void updateHover (juce::Point<float> canvasPos);
    uint32_t createAndConnectToPin (const PinHit& pin, NodeKind kind);
    void restoreSnapshot (const juce::ValueTree& snap);
    bool isPinCompatibleHighlight (const PinHit& pin) const;

    GraphModel model_;
    SharedResources* theme_ = nullptr;
    Spectrogram3DComponent* target_ = nullptr;

    bool liveApply_ = true;
    bool snapEnabled_ = true;
    bool applyPending_ = false;
    int applyCooldownTicks_ = 0;
    bool suppressApply_ = false;
    bool suppressHistory_ = false;

    static constexpr int kMaxUndo = 64;
    std::vector<juce::ValueTree> undoStack_;
    std::vector<juce::ValueTree> redoStack_;
    juce::ValueTree clipboard_;

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
    bool movePushedUndo_ = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleNodeGraphCanvas)
};

/** Collapsible bottom console for compile / debug messages. */
class GraphOutputPanel : public juce::Component
{
public:
    GraphOutputPanel();
    void paint (juce::Graphics& g) override;
    void resized() override;

    void append (LogLevel level, const juce::String& text);
    void clear();
    bool isExpanded() const noexcept { return expanded_; }
    void setExpanded (bool on);
    void toggle() { setExpanded (! expanded_); }
    int preferredHeight() const noexcept { return expanded_ ? expandedH_ : collapsedH_; }

    std::function<void()> onExpandChanged;

private:
    juce::TextEditor log_;
    juce::TextButton clearBtn_ { "Clear" };
    juce::TextButton toggleBtn_ { "Hide Output" };
    bool expanded_ = true;
    int expandedH_ = 140;
    int collapsedH_ = 28;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphOutputPanel)
};

class ParticleNodeGraphWindow : public juce::DocumentWindow,
                                public juce::MenuBarModel
{
public:
    ParticleNodeGraphWindow (Spectrogram3DComponent& target, SharedResources* theme);
    ~ParticleNodeGraphWindow() override;

    ParticleNodeGraphCanvas* getCanvas() noexcept;
    void closeButtonPressed() override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelIndex) override;

    std::function<void()> onClosed;

private:
    class Content;
    Content* content_ = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParticleNodeGraphWindow)
};

} // namespace ParticleNodeGraph
