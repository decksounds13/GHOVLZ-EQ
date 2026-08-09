#pragma once

#include "ParticleNodeTypes.h"
#include <vector>
#include <map>
#include <optional>

namespace ParticleNodeGraph
{

struct GraphNode
{
    uint32_t id = 0;
    NodeKind kind = NodeKind::constFloat;
    juce::Point<float> pos { 0, 0 }; // canvas space (top-left of node body)
    juce::String commentText;
    std::map<juce::String, float> params;
    bool selected = false;
    /** 0 = default header chrome; else custom header ARGB. */
    uint32_t headerColourArgb = 0;

    NodeDesc desc() const { return describeNode (kind); }
    float param (const juce::String& key, float fallback = 0.0f) const
    {
        auto it = params.find (key);
        return it != params.end() ? it->second : fallback;
    }
    void setParam (const juce::String& key, float v) { params[key] = v; }
    bool hasCustomHeaderColour() const noexcept { return headerColourArgb != 0; }
    juce::Colour headerColour() const noexcept
    {
        return headerColourArgb != 0 ? juce::Colour (headerColourArgb)
                                     : juce::Colour (0xff343a48);
    }
};

struct GraphWire
{
    uint32_t id = 0;
    uint32_t fromNode = 0;
    juce::String fromPin;
    uint32_t toNode = 0;
    juce::String toPin;
    bool typeValid = true; // false → draw red, non-functional
};

/** Hit test helpers (canvas coords). */
struct PinHit
{
    uint32_t nodeId = 0;
    juce::String pinId;
    bool isInput = true;
    PinType type = PinType::floatT;
    juce::Point<float> centre;
};

class GraphModel
{
public:
    GraphModel();

    void clear();
    void createDefaultGraph();

    uint32_t addNode (NodeKind kind, juce::Point<float> pos);
    bool removeNode (uint32_t id);
    GraphNode* findNode (uint32_t id) noexcept;
    const GraphNode* findNode (uint32_t id) const noexcept;
    const std::vector<GraphNode>& nodes() const noexcept { return nodes_; }

    /** Attempt connection. Returns wire id, or 0 on hard failure.
        Type mismatch still creates the wire but marks typeValid=false. */
    uint32_t connect (uint32_t fromNode, const juce::String& fromPin,
                      uint32_t toNode, const juce::String& toPin);
    bool removeWire (uint32_t id);
    bool removeWiresTouching (uint32_t nodeId);
    /** Remove all wires attached to a specific pin (input or output). */
    int removeWiresOnPin (uint32_t nodeId, const juce::String& pinId, bool isInput);
    const std::vector<GraphWire>& wires() const noexcept { return wires_; }
    GraphWire* findWire (uint32_t id) noexcept;

    void clearSelection();
    void selectNode (uint32_t id, bool addToSelection);
    void selectWire (uint32_t id);
    std::vector<uint32_t> selectedNodeIds() const;
    uint32_t selectedWireId() const noexcept { return selectedWireId_; }

    void deleteSelection();
    void moveSelectedBy (juce::Point<float> delta);
    /** Layered left→right layout (Houdini / UE style). */
    void arrangeNodes();
    void setNodeHeaderColour (uint32_t nodeId, uint32_t argb);

    /** Geometry for layout (node body size). */
    static juce::Rectangle<float> nodeBounds (const GraphNode& n);
    static float pinRadius() noexcept { return 6.0f; }
    static float headerH() noexcept { return 26.0f; }
    static float rowH() noexcept { return 22.0f; }
    static float nodeW() noexcept { return 180.0f; }

    bool pinCentre (uint32_t nodeId, const juce::String& pinId, bool isInput,
                    juce::Point<float>& outCentre, PinType& outType) const;
    std::optional<PinHit> hitTestPin (juce::Point<float> canvasPos, float hitPad = 10.0f) const;
    uint32_t hitTestNode (juce::Point<float> canvasPos) const;
    uint32_t hitTestWire (juce::Point<float> canvasPos, float maxDist = 8.0f) const;

    juce::ValueTree toValueTree() const;
    void fromValueTree (const juce::ValueTree& tree);

    std::function<void()> onChanged;

private:
    void notify() { if (onChanged) onChanged(); }
    uint32_t nextNodeId_ = 1;
    uint32_t nextWireId_ = 1;
    std::vector<GraphNode> nodes_;
    std::vector<GraphWire> wires_;
    uint32_t selectedWireId_ = 0;

    const PinDesc* findPinDesc (const GraphNode& n, const juce::String& pinId, bool isInput) const;
    bool wouldCreateCycle (uint32_t fromNode, uint32_t toNode) const;
};

} // namespace ParticleNodeGraph
