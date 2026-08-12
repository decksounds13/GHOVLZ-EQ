#include "ParticleNodeGraphCompiler.h"
#include "../Spectrogram3DComponent.h"
#include "../Spec3DParticleSystem.h"
#include <cmath>
#include <map>
#include <set>
#include <algorithm>

namespace ParticleNodeGraph
{
namespace
{
struct Eval
{
    float f = 0.0f;
    juce::Vector3D<float> v { 0, 0, 0 };
    float v4[4] { 0, 0, 0, 0 };
    bool b = false;
    bool hasF = false, hasV = false, hasV4 = false, hasB = false;
};

using Cache = std::map<uint64_t, Eval>;

static uint64_t pinKey (uint32_t node, const juce::String& pin)
{
    return ((uint64_t) node << 32) ^ (uint64_t) pin.hashCode();
}

static const GraphWire* findWireTo (const GraphModel& m, uint32_t toNode, const juce::String& toPin)
{
    for (const auto& w : m.wires())
        if (w.typeValid && w.toNode == toNode && w.toPin == toPin)
            return &w;
    return nullptr;
}

static std::vector<const GraphWire*> findAllWiresIntoNode (const GraphModel& m, uint32_t toNode)
{
    std::vector<const GraphWire*> out;
    for (const auto& w : m.wires())
        if (w.typeValid && w.toNode == toNode)
            out.push_back (&w);
    std::sort (out.begin(), out.end(), [&] (const GraphWire* a, const GraphWire* b)
    {
        if (a->toPin != b->toPin)
            return a->toPin < b->toPin;
        const auto* na = m.findNode (a->fromNode);
        const auto* nb = m.findNode (b->fromNode);
        const float ya = na != nullptr ? na->pos.y : 0.0f;
        const float yb = nb != nullptr ? nb->pos.y : 0.0f;
        return ya < yb;
    });
    return out;
}

static Eval evalPin (const GraphModel& model, uint32_t nodeId, const juce::String& pinId,
                     bool isInput, Cache& cache, std::set<uint32_t>& stack);

static Eval evalNodeOutput (const GraphModel& model, uint32_t nodeId, const juce::String& outPin,
                            Cache& cache, std::set<uint32_t>& stack)
{
    const uint64_t key = pinKey (nodeId, outPin);
    if (auto it = cache.find (key); it != cache.end())
        return it->second;

    if (stack.count (nodeId))
        return {};
    stack.insert (nodeId);

    const auto* n = model.findNode (nodeId);
    Eval r;
    if (n == nullptr)
    {
        stack.erase (nodeId);
        return r;
    }

    auto inF = [&] (const char* pin, float def) -> float
    {
        auto e = evalPin (model, nodeId, pin, true, cache, stack);
        return e.hasF ? e.f : def;
    };
    auto inV = [&] (const char* pin, juce::Vector3D<float> def) -> juce::Vector3D<float>
    {
        auto e = evalPin (model, nodeId, pin, true, cache, stack);
        return e.hasV ? e.v : def;
    };
    auto inB = [&] (const char* pin, bool def) -> bool
    {
        auto e = evalPin (model, nodeId, pin, true, cache, stack);
        return e.hasB ? e.b : def;
    };
    auto inCol = [&] (const char* pin, float dr, float dg, float db, float da) -> Eval
    {
        auto e = evalPin (model, nodeId, pin, true, cache, stack);
        if (e.hasV4) return e;
        if (e.hasV)
        {
            e.v4[0] = e.v.x; e.v4[1] = e.v.y; e.v4[2] = e.v.z; e.v4[3] = 1.0f;
            e.hasV4 = true;
            return e;
        }
        e.v4[0] = dr; e.v4[1] = dg; e.v4[2] = db; e.v4[3] = da;
        e.hasV4 = true;
        return e;
    };

    switch (n->kind)
    {
        case NodeKind::constFloat:
            r.f = n->param ("value", 0.0f);
            r.hasF = true;
            break;
        case NodeKind::constInt:
            r.f = n->param ("value", 0.0f);
            r.hasF = true;
            r.b = r.f != 0.0f;
            r.hasB = true;
            break;
        case NodeKind::constVec2:
            r.v = { n->param ("x"), n->param ("y"), 0.0f };
            r.hasV = true;
            r.v4[0] = r.v.x; r.v4[1] = r.v.y;
            r.hasV4 = true;
            break;
        case NodeKind::constVec3:
            r.v = { n->param ("x"), n->param ("y"), n->param ("z") };
            r.hasV = true;
            break;
        case NodeKind::constVec4:
        case NodeKind::constColour:
            r.v4[0] = n->param (n->kind == NodeKind::constColour ? "r" : "x", 0.0f);
            r.v4[1] = n->param (n->kind == NodeKind::constColour ? "g" : "y", 0.0f);
            r.v4[2] = n->param (n->kind == NodeKind::constColour ? "b" : "z", 0.0f);
            r.v4[3] = n->param (n->kind == NodeKind::constColour ? "a" : "w", 1.0f);
            r.hasV4 = true;
            r.v = { r.v4[0], r.v4[1], r.v4[2] };
            r.hasV = true;
            break;
        case NodeKind::constBool:
            r.b = n->param ("value", 1.0f) > 0.5f;
            r.hasB = true;
            r.f = r.b ? 1.0f : 0.0f;
            r.hasF = true;
            break;
        case NodeKind::mathAdd:
            r.f = inF ("a", 0.0f) + inF ("b", n->param ("b", 0.0f));
            r.hasF = true;
            break;
        case NodeKind::mathSub:
            r.f = inF ("a", 0.0f) - inF ("b", n->param ("b", 0.0f));
            r.hasF = true;
            break;
        case NodeKind::mathMul:
            r.f = inF ("a", 0.0f) * inF ("b", n->param ("b", 1.0f));
            r.hasF = true;
            break;
        case NodeKind::mathDiv:
        {
            const float b = inF ("b", n->param ("b", 1.0f));
            r.f = std::abs (b) > 1.0e-12f ? inF ("a", 0.0f) / b : 0.0f;
            r.hasF = true;
            break;
        }
        case NodeKind::mathLerp:
        {
            const float a = inF ("a", 0.0f), b = inF ("b", 1.0f), t = inF ("t", n->param ("t", 0.5f));
            r.f = a + (b - a) * t;
            r.hasF = true;
            break;
        }
        case NodeKind::mathClamp:
            r.f = juce::jlimit (inF ("min", n->param ("min", 0.0f)),
                                inF ("max", n->param ("max", 1.0f)),
                                inF ("x", 0.0f));
            r.hasF = true;
            break;
        case NodeKind::mathAbs:
            r.f = std::abs (inF ("x", 0.0f));
            r.hasF = true;
            break;
        case NodeKind::mathNegate:
            r.f = -inF ("x", 0.0f);
            r.hasF = true;
            break;
        case NodeKind::mathMin:
            r.f = juce::jmin (inF ("a", 0.0f), inF ("b", n->param ("b", 0.0f)));
            r.hasF = true;
            break;
        case NodeKind::mathMax:
            r.f = juce::jmax (inF ("a", 0.0f), inF ("b", n->param ("b", 0.0f)));
            r.hasF = true;
            break;
        case NodeKind::mathPow:
            r.f = std::pow (inF ("a", 1.0f), inF ("b", n->param ("b", 1.0f)));
            r.hasF = true;
            break;
        case NodeKind::mathSin:
            r.f = std::sin (inF ("x", 0.0f));
            r.hasF = true;
            break;
        case NodeKind::mathCos:
            r.f = std::cos (inF ("x", 0.0f));
            r.hasF = true;
            break;
        case NodeKind::makeVec2:
            r.v = { inF ("x", 0.0f), inF ("y", 0.0f), 0.0f };
            r.hasV = true;
            break;
        case NodeKind::makeVec3:
            r.v = { inF ("x", 0.0f), inF ("y", 0.0f), inF ("z", 0.0f) };
            r.hasV = true;
            break;
        case NodeKind::makeVec4:
            r.v4[0] = inF ("x", n->param ("x", 0.0f));
            r.v4[1] = inF ("y", n->param ("y", 0.0f));
            r.v4[2] = inF ("z", n->param ("z", 0.0f));
            r.v4[3] = inF ("w", n->param ("w", 1.0f));
            r.hasV4 = true;
            r.v = { r.v4[0], r.v4[1], r.v4[2] };
            r.hasV = true;
            break;
        case NodeKind::makeColour:
            r.v4[0] = inF ("r", n->param ("r", 1.0f));
            r.v4[1] = inF ("g", n->param ("g", 1.0f));
            r.v4[2] = inF ("b", n->param ("b", 1.0f));
            r.v4[3] = inF ("a", n->param ("a", 1.0f));
            r.hasV4 = true;
            r.v = { r.v4[0], r.v4[1], r.v4[2] };
            r.hasV = true;
            break;
        case NodeKind::breakVec2:
        {
            const auto v = inV ("v", {});
            if (outPin == "x") { r.f = v.x; r.hasF = true; }
            else { r.f = v.y; r.hasF = true; }
            break;
        }
        case NodeKind::breakVec3:
        {
            const auto v = inV ("v", {});
            if (outPin == "x") { r.f = v.x; r.hasF = true; }
            else if (outPin == "y") { r.f = v.y; r.hasF = true; }
            else { r.f = v.z; r.hasF = true; }
            break;
        }
        case NodeKind::breakVec4:
        case NodeKind::breakColour:
        {
            auto e = inCol (n->kind == NodeKind::breakColour ? "c" : "v", 0, 0, 0, 1);
            int idx = 0;
            if (outPin == "y" || outPin == "g") idx = 1;
            else if (outPin == "z" || outPin == "b") idx = 2;
            else if (outPin == "w" || outPin == "a") idx = 3;
            r.f = e.v4[idx];
            r.hasF = true;
            break;
        }
        case NodeKind::vecLength:
        {
            const auto v = inV ("v", {});
            r.f = std::sqrt (v.x * v.x + v.y * v.y + v.z * v.z);
            r.hasF = true;
            break;
        }
        case NodeKind::vecNormalize:
        {
            auto v = inV ("v", { 0, 1, 0 });
            const float len = std::sqrt (v.x * v.x + v.y * v.y + v.z * v.z);
            if (len > 1.0e-8f) { v.x /= len; v.y /= len; v.z /= len; }
            r.v = v;
            r.hasV = true;
            break;
        }
        case NodeKind::vecScale:
        {
            const auto v = inV ("v", {});
            const float s = inF ("s", n->param ("s", 1.0f));
            r.v = { v.x * s, v.y * s, v.z * s };
            r.hasV = true;
            break;
        }
        case NodeKind::vecAdd:
        {
            const auto a = inV ("a", {}), b = inV ("b", {});
            r.v = { a.x + b.x, a.y + b.y, a.z + b.z };
            r.hasV = true;
            break;
        }
        case NodeKind::vecDot:
        {
            const auto a = inV ("a", {}), b = inV ("b", {});
            r.f = a.x * b.x + a.y * b.y + a.z * b.z;
            r.hasF = true;
            break;
        }
        case NodeKind::floatToVec3:
        {
            const float s = inF ("s", 0.0f);
            r.v = { s, s, s };
            r.hasV = true;
            break;
        }
        case NodeKind::switchFloat:
            r.f = inB ("sel", false) ? inF ("b", 0.0f) : inF ("a", 0.0f);
            r.hasF = true;
            break;
        case NodeKind::colourLerp:
        {
            auto a = inCol ("a", 0, 0, 0, 1);
            auto b = inCol ("b", 1, 1, 1, 1);
            const float t = inF ("t", n->param ("t", 0.5f));
            for (int i = 0; i < 4; ++i)
                r.v4[i] = a.v4[i] + (b.v4[i] - a.v4[i]) * t;
            r.hasV4 = true;
            r.v = { r.v4[0], r.v4[1], r.v4[2] };
            r.hasV = true;
            break;
        }
        case NodeKind::colourMul:
        {
            auto a = inCol ("a", 1, 1, 1, 1);
            auto b = inCol ("b", 1, 1, 1, 1);
            for (int i = 0; i < 4; ++i)
                r.v4[i] = a.v4[i] * b.v4[i];
            r.hasV4 = true;
            r.v = { r.v4[0], r.v4[1], r.v4[2] };
            r.hasV = true;
            break;
        }
        case NodeKind::mapRange:
            r.f = mapRange (inF ("x", 0.0f),
                            n->param ("inMin", 0.0f), n->param ("inMax", 1.0f),
                            n->param ("outMin", 0.0f), n->param ("outMax", 1.0f));
            r.hasF = true;
            break;
        case NodeKind::thresholdGate:
        {
            const float x = inF ("x", 0.0f);
            const float thr = n->param ("threshold", 0.25f);
            const bool pass = x > thr;
            if (outPin == "pass") { r.b = pass; r.hasB = true; }
            else
            {
                r.f = pass ? juce::jlimit (0.0f, 1.0f, (x - thr) / juce::jmax (1.0e-4f, 1.0f - thr)) : 0.0f;
                r.hasF = true;
            }
            break;
        }
        case NodeKind::convertToFloat:
        {
            auto e = evalPin (model, nodeId, "in", true, cache, stack);
            if (e.hasF) r.f = e.f;
            else if (e.hasV)
                r.f = std::sqrt (e.v.x * e.v.x + e.v.y * e.v.y + e.v.z * e.v.z);
            else if (e.hasV4)
                r.f = 0.2126f * e.v4[0] + 0.7152f * e.v4[1] + 0.0722f * e.v4[2];
            else if (e.hasB) r.f = e.b ? 1.0f : 0.0f;
            r.hasF = true;
            break;
        }
        case NodeKind::convertToVec3:
        {
            auto e = evalPin (model, nodeId, "in", true, cache, stack);
            if (e.hasV) r.v = e.v;
            else if (e.hasV4) r.v = { e.v4[0], e.v4[1], e.v4[2] };
            else { const float s = e.hasF ? e.f : 0.0f; r.v = { s, s, s }; }
            r.hasV = true;
            break;
        }
        case NodeKind::convertToColour:
        {
            auto e = evalPin (model, nodeId, "in", true, cache, stack);
            if (e.hasV4) { for (int i = 0; i < 4; ++i) r.v4[i] = e.v4[i]; }
            else if (e.hasV) { r.v4[0] = e.v.x; r.v4[1] = e.v.y; r.v4[2] = e.v.z; r.v4[3] = 1.0f; }
            else { const float s = e.hasF ? e.f : 0.0f; r.v4[0] = r.v4[1] = r.v4[2] = s; r.v4[3] = 1.0f; }
            r.hasV4 = true;
            break;
        }
        case NodeKind::uniformTime:
        case NodeKind::uniformDelta:
        case NodeKind::uniformAmplitude:
            // Compile-time placeholder; runtime uniforms sampled in sim.
            r.f = 0.0f;
            r.hasF = true;
            break;
        case NodeKind::combineFloat:
        {
            float sum = 0.0f;
            int nIn = 0;
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
            {
                auto e = evalNodeOutput (model, w->fromNode, w->fromPin, cache, stack);
                if (e.hasF) { sum += e.f; ++nIn; }
            }
            r.f = sum;
            r.hasF = nIn > 0;
            break;
        }
        case NodeKind::combineVec3:
        {
            juce::Vector3D<float> sum { 0, 0, 0 };
            int nIn = 0;
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
            {
                auto e = evalNodeOutput (model, w->fromNode, w->fromPin, cache, stack);
                if (e.hasV) { sum.x += e.v.x; sum.y += e.v.y; sum.z += e.v.z; ++nIn; }
            }
            r.v = sum;
            r.hasV = nIn > 0;
            break;
        }
        default:
            break;
    }

    stack.erase (nodeId);
    cache[key] = r;
    return r;
}

static Eval evalPin (const GraphModel& model, uint32_t nodeId, const juce::String& pinId,
                     bool isInput, Cache& cache, std::set<uint32_t>& stack)
{
    if (! isInput)
        return evalNodeOutput (model, nodeId, pinId, cache, stack);

    if (const auto* w = findWireTo (model, nodeId, pinId))
        return evalNodeOutput (model, w->fromNode, w->fromPin, cache, stack);

    Eval e;
    if (const auto* n = model.findNode (nodeId))
    {
        if (pinId == "strength" || pinId == "s" || pinId == "x" || pinId == "value"
            || pinId == "a" || pinId == "b" || pinId == "t" || pinId == "min" || pinId == "max"
            || pinId == "scale" || pinId == "speed" || pinId == "rate" || pinId == "life"
            || pinId == "size" || pinId == "r" || pinId == "g" || pinId == "y" || pinId == "z"
            || pinId == "w")
        {
            e.f = n->param (pinId, n->param ("value", n->param ("strength", 0.0f)));
            e.hasF = true;
        }
    }
    return e;
}

static ParticleForceModule forceFromNode (const GraphNode& n, const GraphModel& model,
                                          Cache& cache, uint32_t& uid)
{
    std::set<uint32_t> stack;
    auto fval = [&] (const char* pin, const char* param, float def)
    {
        auto e = evalPin (model, n.id, pin, true, cache, stack);
        return e.hasF ? e.f : n.param (param, def);
    };
    auto vval = [&] (const char* pin, float dx, float dy, float dz)
    {
        auto e = evalPin (model, n.id, pin, true, cache, stack);
        if (e.hasV) return e.v;
        return juce::Vector3D<float> { n.param ("x", dx), n.param ("y", dy), n.param ("z", dz) };
    };

    ParticleForceType type = ParticleForceType::gravity;
    switch (n.kind)
    {
        case NodeKind::forceGravity:    type = ParticleForceType::gravity; break;
        case NodeKind::forceDrag:       type = ParticleForceType::drag; break;
        case NodeKind::forceWind:       type = ParticleForceType::wind; break;
        case NodeKind::forceCurl:       type = ParticleForceType::curlNoise; break;
        case NodeKind::forceTurbulence: type = ParticleForceType::turbulence; break;
        case NodeKind::forceRotation:   type = ParticleForceType::rotation; break;
        default: break;
    }
    auto m = makeDefaultForceModule (type, uid++);
    switch (type)
    {
        case ParticleForceType::gravity:
        case ParticleForceType::drag:
        case ParticleForceType::turbulence:
            m.p[0] = fval ("strength", "strength", m.p[0]);
            break;
        case ParticleForceType::wind:
        {
            const auto v = vval ("accel", 0.005f, 0.0f, 0.0f);
            m.p[0] = v.x; m.p[1] = v.y; m.p[2] = v.z;
            break;
        }
        case ParticleForceType::curlNoise:
            m.p[0] = fval ("strength", "strength", m.p[0]);
            m.p[1] = fval ("scale", "scale", m.p[1]);
            m.p[2] = fval ("speed", "speed", m.p[2]);
            break;
        case ParticleForceType::rotation:
        {
            const auto v = vval ("rate", 0.25f, 0.25f, 0.25f);
            m.p[0] = v.x; m.p[1] = v.y; m.p[2] = v.z;
            break;
        }
        default: break;
    }
    return m;
}

static void collectForcesFrom (const GraphModel& model, uint32_t nodeId, Cache& cache,
                               uint32_t& uid, std::vector<ParticleForceModule>& out,
                               std::set<uint32_t>& visiting)
{
    const auto* n = model.findNode (nodeId);
    if (n == nullptr || ! visiting.insert (nodeId).second)
        return;

    switch (n->kind)
    {
        case NodeKind::forceGravity:
        case NodeKind::forceDrag:
        case NodeKind::forceWind:
        case NodeKind::forceCurl:
        case NodeKind::forceTurbulence:
        case NodeKind::forceRotation:
            out.push_back (forceFromNode (*n, model, cache, uid));
            break;
        case NodeKind::combineForce:
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
                collectForcesFrom (model, w->fromNode, cache, uid, out, visiting);
            break;
        default:
            break;
    }
    visiting.erase (nodeId);
}

struct EmitterLeaf
{
    const GraphNode* node = nullptr;
    float orderY = 0.0f;
};

/** Walk particle/emitter stream: collect filters & colour ramps (downstream-first order reversed to spawn order). */
static void collectParticleStream (const GraphModel& model, uint32_t nodeId,
                                   std::vector<EmitterLeaf>& emitters,
                                   std::vector<AttrFilter>& filters,
                                   std::vector<ColourRampOp>& colourRamps,
                                   Cache& cache,
                                   std::set<uint32_t>& visiting,
                                   CompileResult& logSink)
{
    const auto* n = model.findNode (nodeId);
    if (n == nullptr || ! visiting.insert (nodeId).second)
        return;

    switch (n->kind)
    {
        case NodeKind::emitterSpectrogram:
        case NodeKind::emitterPoint:
            emitters.push_back ({ n, n->pos.y });
            break;

        case NodeKind::filterAttr:
        {
            // Walk upstream first so order is emitter -> filter1 -> filter2
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
                if (w->toPin == "in")
                    collectParticleStream (model, w->fromNode, emitters, filters, colourRamps,
                                           cache, visiting, logSink);
            AttrFilter f;
            f.enabled = true;
            f.attr = (AttrId) juce::jlimit (0, (int) AttrId::count - 1,
                                            (int) std::lround (n->param ("attr", (float) AttrId::fft)));
            f.amount = n->param ("amount", 1.0f);
            f.threshold = n->param ("threshold", 0.25f);
            f.thresholdEnabled = n->param ("thresholdOn", 1.0f) > 0.5f;
            f.curveShape = n->param ("curve", 0.0f);
            f.mapMin = n->param ("mapMin", 0.0f);
            f.mapMax = n->param ("mapMax", 1.0f);
            f.invert = n->param ("invert", 0.0f) > 0.5f;
            f.stage = juce::jlimit (0, 2, (int) std::lround (n->param ("stage", 2.0f)));
            f.keepMode = juce::jlimit (0, 1, (int) std::lround (n->param ("keepMode", 0.0f)));
            filters.push_back (f);
            logSink.addLog (LogLevel::debug,
                            "Filter: " + juce::String (attrName (f.attr))
                            + " thr=" + juce::String (f.threshold, 2));
            break;
        }

        case NodeKind::colourRampAttr:
        {
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
                if (w->toPin == "in")
                    collectParticleStream (model, w->fromNode, emitters, filters, colourRamps,
                                           cache, visiting, logSink);
            ColourRampOp op;
            op.enabled = true;
            op.sourceAttr = (AttrId) juce::jlimit (0, (int) AttrId::count - 1,
                                                   (int) std::lround (n->param ("attr", (float) AttrId::fft)));
            op.curveShape = n->param ("curve", 0.0f);
            op.mapMin = n->param ("mapMin", 0.0f);
            op.mapMax = n->param ("mapMax", 1.0f);
            op.invert = n->param ("invert", 0.0f) > 0.5f;
            op.stage = juce::jlimit (0, 2, (int) std::lround (n->param ("stage", 0.0f)));

            std::set<uint32_t> stack;
            auto c0 = evalPin (model, n->id, "c0", true, cache, stack);
            auto c1 = evalPin (model, n->id, "c1", true, cache, stack);
            if (c0.hasV4)
                for (int i = 0; i < 4; ++i) op.c0[i] = c0.v4[i];
            else
            {
                op.c0[0] = n->param ("c0r", 0.05f); op.c0[1] = n->param ("c0g", 0.05f);
                op.c0[2] = n->param ("c0b", 0.12f); op.c0[3] = n->param ("c0a", 1.0f);
            }
            if (c1.hasV4)
                for (int i = 0; i < 4; ++i) op.c1[i] = c1.v4[i];
            else
            {
                op.c1[0] = n->param ("c1r", 1.0f); op.c1[1] = n->param ("c1g", 0.55f);
                op.c1[2] = n->param ("c1b", 0.15f); op.c1[3] = n->param ("c1a", 1.0f);
            }
            colourRamps.push_back (op);
            logSink.addLog (LogLevel::debug,
                            "Colour ramp from " + juce::String (attrName (op.sourceAttr)));
            break;
        }

        case NodeKind::combineEmitter:
        case NodeKind::combineParticles:
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
                collectParticleStream (model, w->fromNode, emitters, filters, colourRamps,
                                       cache, visiting, logSink);
            break;

        default:
            // Unknown node on stream - try all inputs
            for (const auto* w : findAllWiresIntoNode (model, nodeId))
                collectParticleStream (model, w->fromNode, emitters, filters, colourRamps,
                                       cache, visiting, logSink);
            break;
    }
    visiting.erase (nodeId);
}

static void applyEmitterLeaf (const GraphModel& model, const GraphNode& en, Cache& cache,
                              CompileResult& r, std::set<uint32_t>& stack)
{
    if (en.kind == NodeKind::emitterSpectrogram)
    {
        r.emitterType = ParticleEmitterType::spectrogram;
        r.continuousEmit = en.param ("continuous", 1.0f) > 0.5f;
        r.spawnJitter = en.param ("jitter", 0.0025f);
    }
    else if (en.kind == NodeKind::emitterPoint)
    {
        r.emitterType = ParticleEmitterType::point;
        auto pos = evalPin (model, en.id, "pos", true, cache, stack);
        auto vel = evalPin (model, en.id, "vel", true, cache, stack);
        r.emitterPosX = pos.hasV ? pos.v.x : en.param ("px", 0.0f);
        r.emitterPosY = pos.hasV ? pos.v.y : en.param ("py", 0.25f);
        r.emitterPosZ = pos.hasV ? pos.v.z : en.param ("pz", 0.0f);
        r.initVelX = vel.hasV ? vel.v.x : en.param ("vx", 0.0f);
        r.initVelY = vel.hasV ? vel.v.y : en.param ("vy", 1.0f);
        r.initVelZ = vel.hasV ? vel.v.z : en.param ("vz", 0.0f);
    }
}

static int countInvalidWires (const GraphModel& model)
{
    int n = 0;
    for (const auto& w : model.wires())
        if (! w.typeValid)
            ++n;
    return n;
}
} // namespace

CompileResult compileGraph (const GraphModel& model, uint32_t forceUidSeed)
{
    CompileResult r;
    Cache cache;

    r.addLog (LogLevel::info, "Compiling particle graph...");

    const int badWires = countInvalidWires (model);
    if (badWires > 0)
        r.addLog (LogLevel::warning,
                  juce::String (badWires) + " type-mismatched wire(s) ignored.");

    const GraphNode* out = nullptr;
    for (const auto& n : model.nodes())
        if (n.kind == NodeKind::simOutput)
        {
            out = &n;
            break;
        }

    if (out == nullptr)
    {
        r.addLog (LogLevel::error, "Add a Simulation Output node.");
        r.message = "Add a Simulation Output node.";
        return r;
    }

    std::set<uint32_t> stack;
    auto rateE = evalPin (model, out->id, "rate", true, cache, stack);
    auto lifeE = evalPin (model, out->id, "life", true, cache, stack);
    auto sizeE = evalPin (model, out->id, "size", true, cache, stack);
    auto enE = evalPin (model, out->id, "enabled", true, cache, stack);

    r.emission = rateE.hasF ? rateE.f : out->param ("defaultRate", 4000.0f);
    r.lifespan = lifeE.hasF ? lifeE.f : out->param ("defaultLife", 2.0f);
    r.size = sizeE.hasF ? sizeE.f : out->param ("defaultSize", 0.02f);
    // Enabled pin only — do not default true (that forced particle mode whenever the
    // node graph applied / opened, and blocked Look → Particle mode off).
    if (enE.hasB)
    {
        r.particleModeEnabled = enE.b;
        r.particleModeSpecified = true;
    }
    else
    {
        r.particleModeSpecified = false;
        r.particleModeEnabled = false;
    }

    // Particle stream: particles pin + legacy emitter pin
    {
        std::vector<EmitterLeaf> leaves;
        std::set<uint32_t> visit;
        for (const auto& w : model.wires())
        {
            if (! w.typeValid || w.toNode != out->id)
                continue;
            if (w.toPin != "particles" && w.toPin != "emitter")
                continue;
            collectParticleStream (model, w.fromNode, leaves, r.program.filters,
                                   r.program.colourRamps, cache, visit, r);
        }
        std::sort (leaves.begin(), leaves.end(), [] (const EmitterLeaf& a, const EmitterLeaf& b)
        {
            return a.orderY < b.orderY;
        });

        const GraphNode* primary = nullptr;
        for (const auto& leaf : leaves)
            if (leaf.node != nullptr && leaf.node->kind == NodeKind::emitterSpectrogram)
            {
                primary = leaf.node;
                break;
            }
        if (primary == nullptr && ! leaves.empty())
            primary = leaves.front().node;

        if (primary != nullptr)
        {
            applyEmitterLeaf (model, *primary, cache, r, stack);
            r.addLog (LogLevel::info,
                      juce::String ("Emitter: ") + nodeKindName (primary->kind));
        }
        else
            r.addLog (LogLevel::warning, "No emitter on Particles stream - using previous emitter settings.");

        if (leaves.size() > 1)
            r.addLog (LogLevel::warning,
                      juce::String ((int) leaves.size())
                      + " emitters combined (primary active; multi-emit later).");
    }

    // Forces
    {
        uint32_t uid = forceUidSeed;
        std::set<uint32_t> visit;
        std::vector<std::pair<float, uint32_t>> roots;
        for (const auto& w : model.wires())
        {
            if (! w.typeValid || w.toNode != out->id || w.toPin != "force")
                continue;
            if (const auto* fn = model.findNode (w.fromNode))
                roots.push_back ({ fn->pos.y, fn->id });
        }
        std::sort (roots.begin(), roots.end(),
                   [] (const auto& a, const auto& b) { return a.first < b.first; });
        for (const auto& root : roots)
            collectForcesFrom (model, root.second, cache, uid, r.forces, visit);
    }

    r.forcesEnabled = ! r.forces.empty();
    r.ok = true;
    r.message = "Live: " + juce::String ((int) r.forces.size()) + " force(s), "
                + juce::String ((int) r.program.filters.size()) + " filter(s), "
                + juce::String ((int) r.program.colourRamps.size()) + " colour ramp(s), rate "
                + juce::String (r.emission, 0);
    r.addLog (LogLevel::success, r.message);
    return r;
}

void applyCompileResult (Spectrogram3DComponent& spec3d, const CompileResult& r)
{
    if (! r.ok)
        return;

    // Particle mode is owned by Look → "Particle mode" unless the graph has
    // Simulation Output → Enabled wired explicitly.
    if (r.particleModeSpecified)
        spec3d.setParticleModeEnabled (r.particleModeEnabled);
    spec3d.setParticleEmission (r.emission);
    spec3d.setParticleLifespan (r.lifespan);
    spec3d.setParticleSize (r.size);
    spec3d.setParticleSpawnJitter (r.spawnJitter);
    spec3d.setParticleEmitterType (r.emitterType);
    if (r.emitterType == ParticleEmitterType::spectrogram)
        spec3d.setParticleEmitMode (r.continuousEmit
                                        ? Spectrogram3DComponent::ParticleEmitMode::continuous
                                        : Spectrogram3DComponent::ParticleEmitMode::slice);
    else
        spec3d.setParticleBindingMode (ParticleBindingMode::freeVisualizer);
    spec3d.setParticleEmitterPos (r.emitterPosX, r.emitterPosY, r.emitterPosZ);
    spec3d.setParticleInitVelX (r.initVelX);
    spec3d.setParticleInitVelY (r.initVelY);
    spec3d.setParticleInitVelZ (r.initVelZ);
    spec3d.setParticleForcesEnabled (r.forcesEnabled);
    spec3d.setParticleForceStack (r.forces);
    spec3d.setParticleGraphProgram (r.program);
}

} // namespace ParticleNodeGraph
