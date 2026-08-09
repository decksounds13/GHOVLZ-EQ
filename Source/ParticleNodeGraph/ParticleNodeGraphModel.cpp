#include "ParticleNodeGraphModel.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>

namespace ParticleNodeGraph
{

GraphModel::GraphModel()
{
    createDefaultGraph();
}

void GraphModel::clear()
{
    nodes_.clear();
    wires_.clear();
    selectedWireId_ = 0;
    nextNodeId_ = 1;
    nextWireId_ = 1;
    notify();
}

void GraphModel::createDefaultGraph()
{
    clear();
    const auto e = addNode (NodeKind::emitterSpectrogram, { 60.0f, 180.0f });
    const auto g = addNode (NodeKind::forceGravity, { 280.0f, 60.0f });
    const auto d = addNode (NodeKind::forceDrag, { 280.0f, 200.0f });
    const auto t = addNode (NodeKind::forceTurbulence, { 280.0f, 340.0f });
    const auto comb = addNode (NodeKind::combineForce, { 480.0f, 180.0f });
    const auto rate = addNode (NodeKind::constFloat, { 60.0f, 360.0f });
    if (auto* rn = findNode (rate))
        rn->setParam ("value", 4000.0f);
    const auto out = addNode (NodeKind::simOutput, { 720.0f, 180.0f });
    connect (e, "out", out, "emitter");
    connect (g, "out", comb, "in0");
    connect (d, "out", comb, "in1");
    connect (t, "out", comb, "in2");
    connect (comb, "out", out, "force");
    connect (rate, "out", out, "rate");
    clearSelection();
    notify();
}

uint32_t GraphModel::addNode (NodeKind kind, juce::Point<float> pos)
{
    GraphNode n;
    n.id = nextNodeId_++;
    n.kind = kind;
    n.pos = pos;
    const auto desc = describeNode (kind);
    for (int i = 0; i < desc.paramKeys.size(); ++i)
        n.params[desc.paramKeys[i]] = desc.paramDefaults[i];
    if (kind == NodeKind::comment)
        n.commentText = "Comment";
    nodes_.push_back (std::move (n));
    notify();
    return nodes_.back().id;
}

bool GraphModel::removeNode (uint32_t id)
{
    removeWiresTouching (id);
    const auto it = std::remove_if (nodes_.begin(), nodes_.end(),
                                    [id] (const GraphNode& n) { return n.id == id; });
    if (it == nodes_.end())
        return false;
    nodes_.erase (it, nodes_.end());
    notify();
    return true;
}

GraphNode* GraphModel::findNode (uint32_t id) noexcept
{
    for (auto& n : nodes_)
        if (n.id == id)
            return &n;
    return nullptr;
}

const GraphNode* GraphModel::findNode (uint32_t id) const noexcept
{
    for (const auto& n : nodes_)
        if (n.id == id)
            return &n;
    return nullptr;
}

const PinDesc* GraphModel::findPinDesc (const GraphNode& n, const juce::String& pinId, bool isInput) const
{
    const auto d = n.desc();
    const auto& list = isInput ? d.inputs : d.outputs;
    for (const auto& p : list)
        if (p.id == pinId)
            return &p;
    return nullptr;
}

bool GraphModel::wouldCreateCycle (uint32_t fromNode, uint32_t toNode) const
{
    // Edge fromNode → toNode; cycle if toNode can already reach fromNode.
    std::set<uint32_t> seen;
    std::vector<uint32_t> stack { toNode };
    while (! stack.empty())
    {
        const auto cur = stack.back();
        stack.pop_back();
        if (cur == fromNode)
            return true;
        if (! seen.insert (cur).second)
            continue;
        for (const auto& w : wires_)
            if (w.typeValid && w.fromNode == cur)
                stack.push_back (w.toNode);
    }
    return false;
}

uint32_t GraphModel::connect (uint32_t fromNode, const juce::String& fromPin,
                              uint32_t toNode, const juce::String& toPin)
{
    auto* a = findNode (fromNode);
    auto* b = findNode (toNode);
    if (a == nullptr || b == nullptr || fromNode == toNode)
        return 0;

    const auto* outDesc = findPinDesc (*a, fromPin, false);
    const auto* inDesc = findPinDesc (*b, toPin, true);
    if (outDesc == nullptr || inDesc == nullptr)
        return 0;

    // Fan-in pins keep multiple wires (forces, combine nodes, multi-emitter).
    const bool multiIn = pinAllowsMultiWire (b->kind, toPin)
                         && pinTypesCompatible (outDesc->type, inDesc->type);
    if (! multiIn)
    {
        wires_.erase (std::remove_if (wires_.begin(), wires_.end(),
                                      [&] (const GraphWire& w)
                                      {
                                          return w.toNode == toNode && w.toPin == toPin;
                                      }),
                      wires_.end());
    }
    else
    {
        for (const auto& w : wires_)
            if (w.fromNode == fromNode && w.fromPin == fromPin
                && w.toNode == toNode && w.toPin == toPin)
                return w.id;
    }

    GraphWire w;
    w.id = nextWireId_++;
    w.fromNode = fromNode;
    w.fromPin = fromPin;
    w.toNode = toNode;
    w.toPin = toPin;
    w.typeValid = pinTypesCompatible (outDesc->type, inDesc->type)
                  && ! wouldCreateCycle (fromNode, toNode);
    // Type mismatch: still store wire as invalid visual feedback.
    if (! pinTypesCompatible (outDesc->type, inDesc->type))
        w.typeValid = false;
    if (wouldCreateCycle (fromNode, toNode))
        w.typeValid = false;

    wires_.push_back (w);
    notify();
    return w.id;
}

bool GraphModel::removeWire (uint32_t id)
{
    const auto it = std::remove_if (wires_.begin(), wires_.end(),
                                    [id] (const GraphWire& w) { return w.id == id; });
    if (it == wires_.end())
        return false;
    wires_.erase (it, wires_.end());
    if (selectedWireId_ == id)
        selectedWireId_ = 0;
    notify();
    return true;
}

bool GraphModel::removeWiresTouching (uint32_t nodeId)
{
    const auto before = wires_.size();
    wires_.erase (std::remove_if (wires_.begin(), wires_.end(),
                                  [nodeId] (const GraphWire& w)
                                  {
                                      return w.fromNode == nodeId || w.toNode == nodeId;
                                  }),
                  wires_.end());
    if (wires_.size() != before)
    {
        selectedWireId_ = 0;
        notify();
        return true;
    }
    return false;
}

int GraphModel::removeWiresOnPin (uint32_t nodeId, const juce::String& pinId, bool isInput)
{
    const auto before = wires_.size();
    wires_.erase (std::remove_if (wires_.begin(), wires_.end(),
                                  [&] (const GraphWire& w)
                                  {
                                      if (isInput)
                                          return w.toNode == nodeId && w.toPin == pinId;
                                      return w.fromNode == nodeId && w.fromPin == pinId;
                                  }),
                  wires_.end());
    const int removed = (int) (before - wires_.size());
    if (removed > 0)
    {
        selectedWireId_ = 0;
        notify();
    }
    return removed;
}

void GraphModel::setNodeHeaderColour (uint32_t nodeId, uint32_t argb)
{
    if (auto* n = findNode (nodeId))
    {
        n->headerColourArgb = argb;
        notify();
    }
}

void GraphModel::arrangeNodes()
{
    if (nodes_.empty())
        return;

    // Longest-path layering from sources → sinks (UE Blueprint / Houdini style columns).
    std::map<uint32_t, int> indeg;
    std::map<uint32_t, std::vector<uint32_t>> outs;
    for (const auto& n : nodes_)
    {
        indeg[n.id] = 0;
        outs[n.id] = {};
    }
    for (const auto& w : wires_)
    {
        if (! w.typeValid)
            continue;
        if (indeg.count (w.toNode) && outs.count (w.fromNode))
        {
            outs[w.fromNode].push_back (w.toNode);
            indeg[w.toNode]++;
        }
    }

    std::map<uint32_t, int> level;
    std::vector<uint32_t> queue;
    for (const auto& n : nodes_)
        if (indeg[n.id] == 0)
        {
            level[n.id] = 0;
            queue.push_back (n.id);
        }

    // Kahn + push levels forward
    std::map<uint32_t, int> remaining = indeg;
    std::vector<uint32_t> q = queue;
    size_t qi = 0;
    while (qi < q.size())
    {
        const auto u = q[qi++];
        const int lu = level[u];
        for (auto v : outs[u])
        {
            level[v] = juce::jmax (level.count (v) ? level[v] : 0, lu + 1);
            if (--remaining[v] == 0)
                q.push_back (v);
        }
    }
    // Orphans / cycles: assign remaining
    int maxLevel = 0;
    for (const auto& n : nodes_)
    {
        if (! level.count (n.id))
            level[n.id] = 0;
        maxLevel = juce::jmax (maxLevel, level[n.id]);
    }

    std::vector<std::vector<uint32_t>> columns ((size_t) maxLevel + 1);
    for (const auto& n : nodes_)
        columns[(size_t) level[n.id]].push_back (n.id);

    // Prefer sim output on the rightmost column
    for (auto& col : columns)
        std::sort (col.begin(), col.end(), [&] (uint32_t a, uint32_t b)
        {
            const auto* na = findNode (a);
            const auto* nb = findNode (b);
            const bool oa = na && na->kind == NodeKind::simOutput;
            const bool ob = nb && nb->kind == NodeKind::simOutput;
            if (oa != ob)
                return ! oa && ob; // output last within column if mixed
            if (na && nb && std::abs (na->pos.y - nb->pos.y) > 1.0f)
                return na->pos.y < nb->pos.y;
            return a < b;
        });

    constexpr float kColW = 240.0f;
    constexpr float kRowGap = 28.0f;
    constexpr float kOriginX = 60.0f;
    constexpr float kOriginY = 50.0f;

    for (int c = 0; c <= maxLevel; ++c)
    {
        float y = kOriginY;
        for (auto id : columns[(size_t) c])
        {
            if (auto* n = findNode (id))
            {
                n->pos = { kOriginX + (float) c * kColW, y };
                y += GraphModel::nodeBounds (*n).getHeight() + kRowGap;
            }
        }
    }
    notify();
}

GraphWire* GraphModel::findWire (uint32_t id) noexcept
{
    for (auto& w : wires_)
        if (w.id == id)
            return &w;
    return nullptr;
}

void GraphModel::clearSelection()
{
    for (auto& n : nodes_)
        n.selected = false;
    selectedWireId_ = 0;
}

void GraphModel::selectNode (uint32_t id, bool addToSelection)
{
    if (! addToSelection)
        clearSelection();
    if (auto* n = findNode (id))
        n->selected = true;
    selectedWireId_ = 0;
}

void GraphModel::selectWire (uint32_t id)
{
    clearSelection();
    selectedWireId_ = id;
}

std::vector<uint32_t> GraphModel::selectedNodeIds() const
{
    std::vector<uint32_t> ids;
    for (const auto& n : nodes_)
        if (n.selected)
            ids.push_back (n.id);
    return ids;
}

void GraphModel::deleteSelection()
{
    if (selectedWireId_ != 0)
    {
        removeWire (selectedWireId_);
        return;
    }
    auto ids = selectedNodeIds();
    for (auto id : ids)
        removeNode (id);
}

void GraphModel::moveSelectedBy (juce::Point<float> delta)
{
    for (auto& n : nodes_)
        if (n.selected)
            n.pos += delta;
    if (! selectedNodeIds().empty())
        notify();
}

juce::Rectangle<float> GraphModel::nodeBounds (const GraphNode& n)
{
    const auto d = n.desc();
    const int rows = juce::jmax (d.inputs.size(), d.outputs.size());
    const float h = headerH() + juce::jmax (1, rows) * rowH() + 10.0f
                    + (float) d.paramKeys.size() * 18.0f;
    float w = nodeW();
    if (n.kind == NodeKind::comment)
        w = juce::jmax (120.0f, n.param ("w", 180.0f));
    return { n.pos.x, n.pos.y, w, h };
}

bool GraphModel::pinCentre (uint32_t nodeId, const juce::String& pinId, bool isInput,
                            juce::Point<float>& outCentre, PinType& outType) const
{
    const auto* n = findNode (nodeId);
    if (n == nullptr)
        return false;
    const auto d = n->desc();
    const auto& list = isInput ? d.inputs : d.outputs;
    int idx = -1;
    for (int i = 0; i < list.size(); ++i)
        if (list[i].id == pinId)
        {
            idx = i;
            outType = list[i].type;
            break;
        }
    if (idx < 0)
        return false;
    const auto b = nodeBounds (*n);
    const float y = b.getY() + headerH() + rowH() * (0.5f + (float) idx);
    outCentre = { isInput ? b.getX() : b.getRight(), y };
    return true;
}

std::optional<PinHit> GraphModel::hitTestPin (juce::Point<float> canvasPos, float hitPad) const
{
    std::optional<PinHit> best;
    float bestD = hitPad;
    for (const auto& n : nodes_)
    {
        const auto d = n.desc();
        auto consider = [&] (const juce::Array<PinDesc>& list, bool isInput)
        {
            for (int i = 0; i < list.size(); ++i)
            {
                juce::Point<float> c;
                PinType t;
                if (! pinCentre (n.id, list[i].id, isInput, c, t))
                    continue;
                const float dist = c.getDistanceFrom (canvasPos);
                if (dist <= bestD)
                {
                    bestD = dist;
                    best = PinHit { n.id, list[i].id, isInput, t, c };
                }
            }
        };
        consider (d.inputs, true);
        consider (d.outputs, false);
    }
    return best;
}

uint32_t GraphModel::hitTestNode (juce::Point<float> canvasPos) const
{
    for (int i = (int) nodes_.size() - 1; i >= 0; --i)
        if (nodeBounds (nodes_[(size_t) i]).contains (canvasPos))
            return nodes_[(size_t) i].id;
    return 0;
}

static float distPointToSegment (juce::Point<float> p, juce::Point<float> a, juce::Point<float> b)
{
    const auto ab = b - a;
    const float len2 = ab.x * ab.x + ab.y * ab.y;
    if (len2 < 1.0e-6f)
        return p.getDistanceFrom (a);
    float t = ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / len2;
    t = juce::jlimit (0.0f, 1.0f, t);
    const juce::Point<float> proj { a.x + t * ab.x, a.y + t * ab.y };
    return p.getDistanceFrom (proj);
}

uint32_t GraphModel::hitTestWire (juce::Point<float> canvasPos, float maxDist) const
{
    uint32_t best = 0;
    float bestD = maxDist;
    for (const auto& w : wires_)
    {
        juce::Point<float> a, b;
        PinType ta, tb;
        if (! pinCentre (w.fromNode, w.fromPin, false, a, ta))
            continue;
        if (! pinCentre (w.toNode, w.toPin, true, b, tb))
            continue;
        const float d = distPointToSegment (canvasPos, a, b);
        if (d < bestD)
        {
            bestD = d;
            best = w.id;
        }
    }
    return best;
}

juce::ValueTree GraphModel::toValueTree() const
{
    juce::ValueTree root ("ParticleNodeGraph");
    root.setProperty ("version", 1, nullptr);
    for (const auto& n : nodes_)
    {
        juce::ValueTree nt ("Node");
        nt.setProperty ("id", (int) n.id, nullptr);
        nt.setProperty ("kind", (int) n.kind, nullptr);
        nt.setProperty ("x", n.pos.x, nullptr);
        nt.setProperty ("y", n.pos.y, nullptr);
        nt.setProperty ("comment", n.commentText, nullptr);
        nt.setProperty ("headerArgb", (int) n.headerColourArgb, nullptr);
        for (const auto& kv : n.params)
            nt.setProperty ("p_" + kv.first, kv.second, nullptr);
        root.appendChild (nt, nullptr);
    }
    for (const auto& w : wires_)
    {
        juce::ValueTree wt ("Wire");
        wt.setProperty ("id", (int) w.id, nullptr);
        wt.setProperty ("from", (int) w.fromNode, nullptr);
        wt.setProperty ("fromPin", w.fromPin, nullptr);
        wt.setProperty ("to", (int) w.toNode, nullptr);
        wt.setProperty ("toPin", w.toPin, nullptr);
        wt.setProperty ("valid", w.typeValid, nullptr);
        root.appendChild (wt, nullptr);
    }
    return root;
}

void GraphModel::fromValueTree (const juce::ValueTree& tree)
{
    if (! tree.hasType ("ParticleNodeGraph"))
        return;
    nodes_.clear();
    wires_.clear();
    selectedWireId_ = 0;
    nextNodeId_ = 1;
    nextWireId_ = 1;

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto ch = tree.getChild (i);
        if (ch.hasType ("Node"))
        {
            GraphNode n;
            n.id = (uint32_t) (int) ch.getProperty ("id", 0);
            n.kind = (NodeKind) juce::jlimit (0, (int) NodeKind::count - 1,
                                              (int) ch.getProperty ("kind", 0));
            n.pos = { (float) ch.getProperty ("x", 0.0), (float) ch.getProperty ("y", 0.0) };
            n.commentText = ch.getProperty ("comment", "").toString();
            n.headerColourArgb = (uint32_t) (int) ch.getProperty ("headerArgb", 0);
            const auto desc = describeNode (n.kind);
            for (int p = 0; p < desc.paramKeys.size(); ++p)
            {
                const auto key = desc.paramKeys[p];
                n.params[key] = (float) ch.getProperty ("p_" + key, desc.paramDefaults[p]);
            }
            nextNodeId_ = juce::jmax (nextNodeId_, n.id + 1);
            nodes_.push_back (std::move (n));
        }
    }
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto ch = tree.getChild (i);
        if (! ch.hasType ("Wire"))
            continue;
        GraphWire w;
        w.id = (uint32_t) (int) ch.getProperty ("id", 0);
        w.fromNode = (uint32_t) (int) ch.getProperty ("from", 0);
        w.fromPin = ch.getProperty ("fromPin", "").toString();
        w.toNode = (uint32_t) (int) ch.getProperty ("to", 0);
        w.toPin = ch.getProperty ("toPin", "").toString();
        w.typeValid = (bool) ch.getProperty ("valid", true);
        // Re-validate
        if (auto* a = findNode (w.fromNode))
            if (auto* b = findNode (w.toNode))
            {
                const auto* od = findPinDesc (*a, w.fromPin, false);
                const auto* id = findPinDesc (*b, w.toPin, true);
                w.typeValid = od && id && pinTypesCompatible (od->type, id->type);
            }
        nextWireId_ = juce::jmax (nextWireId_, w.id + 1);
        wires_.push_back (std::move (w));
    }
    notify();
}

} // namespace ParticleNodeGraph
