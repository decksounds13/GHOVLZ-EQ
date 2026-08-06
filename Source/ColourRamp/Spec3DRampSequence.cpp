#include "Spec3DRampSequence.h"
#include <cmath>
#include <algorithm>

namespace
{
    float wrapTime (float t, float length) noexcept
    {
        if (length <= 1.0e-6f)
            return 0.0f;
        t = std::fmod (t, length);
        if (t < 0.0f)
            t += length;
        return t;
    }

    float smootherstep (float u) noexcept
    {
        u = juce::jlimit (0.0f, 1.0f, u);
        return u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
    }

    // Hermite with endpoint derivatives (seconds domain).
    float hermite (float y0, float y1, float m0, float m1, float u, float dt) noexcept
    {
        const float u2 = u * u;
        const float u3 = u2 * u;
        const float h00 = 2.0f * u3 - 3.0f * u2 + 1.0f;
        const float h10 = u3 - 2.0f * u2 + u;
        const float h01 = -2.0f * u3 + 3.0f * u2;
        const float h11 = u3 - u2;
        return h00 * y0 + h10 * m0 * dt + h01 * y1 + h11 * m1 * dt;
    }
}

GradientRamp lerpRamps (const GradientRamp& a, const GradientRamp& b, float t, int poles)
{
    t = juce::jlimit (0.0f, 1.0f, t);
    poles = juce::jlimit (2, GradientRamp::kMaxStops, poles);

    GradientRamp aa = a;
    GradientRamp bb = b;
    if ((int) aa.stops.size() < 2)
        return bb.isUsable() ? bb : aa;
    if ((int) bb.stops.size() < 2)
        return aa;

    aa.resampleToCount (poles);
    bb.resampleToCount (poles);

    GradientRamp out;
    out.mapMode = a.mapMode;
    out.interpMode = t < 0.5f ? a.interpMode : b.interpMode;
    out.stops.resize ((size_t) poles);
    for (int i = 0; i < poles; ++i)
    {
        const float p = (float) i / (float) (poles - 1);
        out.stops[(size_t) i].position = p;
        out.stops[(size_t) i].colour = aa.colourAt (p).interpolatedWith (bb.colourAt (p), t);
    }
    out.enabled = true;
    ++out.revision;
    return out;
}

// ── Float envelope ──────────────────────────────────────────────────────────

void Spec3DFloatEnvelope::sortKeys() noexcept
{
    std::sort (keys.begin(), keys.end(),
               [] (const Spec3DFloatKey& a, const Spec3DFloatKey& b) { return a.timeSec < b.timeSec; });
}

float Spec3DFloatEnvelope::evaluate (float timeSec, float lengthSec) const noexcept
{
    if (keys.empty())
        return defaultV;

    const float t = wrapTime (timeSec, juce::jmax (1.0e-4f, lengthSec));
    const int n = (int) keys.size();

    if (n == 1 || t <= keys.front().timeSec)
        return juce::jlimit (minV, maxV, keys.front().value);
    if (t >= keys.back().timeSec)
        return juce::jlimit (minV, maxV, keys.back().value);

    for (int i = 0; i < n - 1; ++i)
    {
        const auto& a = keys[(size_t) i];
        const auto& b = keys[(size_t) i + 1];
        if (t > b.timeSec)
            continue;

        const float dt = juce::jmax (1.0e-5f, b.timeSec - a.timeSec);
        const float u = (t - a.timeSec) / dt;

        float v = a.value;
        switch (a.interp)
        {
            case Spec3DKeyInterp::step:
                v = a.value;
                break;
            case Spec3DKeyInterp::smooth:
                v = a.value + (b.value - a.value) * smootherstep (u);
                break;
            case Spec3DKeyInterp::bezier:
                v = hermite (a.value, b.value, a.outTangent, b.inTangent, u, dt);
                break;
            case Spec3DKeyInterp::linear:
            default:
                v = a.value + (b.value - a.value) * u;
                break;
        }
        return juce::jlimit (minV, maxV, v);
    }
    return juce::jlimit (minV, maxV, keys.back().value);
}

juce::ValueTree Spec3DFloatEnvelope::toValueTree (const juce::Identifier& type) const
{
    juce::ValueTree tree (type);
    tree.setProperty ("minV", (double) minV, nullptr);
    tree.setProperty ("maxV", (double) maxV, nullptr);
    tree.setProperty ("defaultV", (double) defaultV, nullptr);
    for (const auto& k : keys)
    {
        juce::ValueTree key ("Key");
        key.setProperty ("t", (double) k.timeSec, nullptr);
        key.setProperty ("v", (double) k.value, nullptr);
        key.setProperty ("interp", (int) k.interp, nullptr);
        key.setProperty ("inTan", (double) k.inTangent, nullptr);
        key.setProperty ("outTan", (double) k.outTangent, nullptr);
        tree.appendChild (std::move (key), nullptr);
    }
    return tree;
}

Spec3DFloatEnvelope Spec3DFloatEnvelope::fromValueTree (const juce::ValueTree& tree)
{
    Spec3DFloatEnvelope e;
    if (! tree.isValid())
        return e;
    e.minV = (float) (double) tree.getProperty ("minV", 0.0);
    e.maxV = (float) (double) tree.getProperty ("maxV", 1.0);
    e.defaultV = (float) (double) tree.getProperty ("defaultV", 0.5);
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto c = tree.getChild (i);
        if (! c.hasType ("Key"))
            continue;
        Spec3DFloatKey k;
        k.timeSec = (float) (double) c.getProperty ("t", 0.0);
        k.value = (float) (double) c.getProperty ("v", 0.0);
        k.interp = static_cast<Spec3DKeyInterp> (juce::jlimit (0, 3, (int) c.getProperty ("interp", 1)));
        k.inTangent = (float) (double) c.getProperty ("inTan", 0.0);
        k.outTangent = (float) (double) c.getProperty ("outTan", 0.0);
        e.keys.push_back (k);
    }
    e.sortKeys();
    return e;
}

// ── Colour envelope ─────────────────────────────────────────────────────────

void Spec3DColourEnvelope::sortKeys() noexcept
{
    std::sort (keys.begin(), keys.end(),
               [] (const Spec3DColourKey& a, const Spec3DColourKey& b) { return a.timeSec < b.timeSec; });
}

juce::Colour Spec3DColourEnvelope::evaluate (float timeSec, float lengthSec) const noexcept
{
    if (keys.empty())
        return defaultColour;

    const float t = wrapTime (timeSec, juce::jmax (1.0e-4f, lengthSec));
    const int n = (int) keys.size();
    if (n == 1 || t <= keys.front().timeSec)
        return keys.front().colour;
    if (t >= keys.back().timeSec)
        return keys.back().colour;

    for (int i = 0; i < n - 1; ++i)
    {
        const auto& a = keys[(size_t) i];
        const auto& b = keys[(size_t) i + 1];
        if (t > b.timeSec)
            continue;

        const float dt = juce::jmax (1.0e-5f, b.timeSec - a.timeSec);
        float u = (t - a.timeSec) / dt;
        switch (a.interp)
        {
            case Spec3DKeyInterp::step:   return a.colour;
            case Spec3DKeyInterp::smooth: u = smootherstep (u); break;
            case Spec3DKeyInterp::bezier:
            {
                // Time ease via hermite on 0..1 then colour lerp
                u = hermite (0.0f, 1.0f, a.outTangent, b.inTangent, u, 1.0f);
                u = juce::jlimit (0.0f, 1.0f, u);
                break;
            }
            default: break;
        }
        return a.colour.interpolatedWith (b.colour, u);
    }
    return keys.back().colour;
}

juce::ValueTree Spec3DColourEnvelope::toValueTree (const juce::Identifier& type) const
{
    juce::ValueTree tree (type);
    tree.setProperty ("defArgb", (int) defaultColour.getARGB(), nullptr);
    for (const auto& k : keys)
    {
        juce::ValueTree key ("Key");
        key.setProperty ("t", (double) k.timeSec, nullptr);
        key.setProperty ("argb", (int) k.colour.getARGB(), nullptr);
        key.setProperty ("interp", (int) k.interp, nullptr);
        key.setProperty ("inTan", (double) k.inTangent, nullptr);
        key.setProperty ("outTan", (double) k.outTangent, nullptr);
        tree.appendChild (std::move (key), nullptr);
    }
    return tree;
}

Spec3DColourEnvelope Spec3DColourEnvelope::fromValueTree (const juce::ValueTree& tree)
{
    Spec3DColourEnvelope e;
    if (! tree.isValid())
        return e;
    e.defaultColour = juce::Colour ((juce::uint32) (int) tree.getProperty ("defArgb", (int) 0xffffffff));
    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto c = tree.getChild (i);
        if (! c.hasType ("Key"))
            continue;
        Spec3DColourKey k;
        k.timeSec = (float) (double) c.getProperty ("t", 0.0);
        k.colour = juce::Colour ((juce::uint32) (int) c.getProperty ("argb", (int) 0xffffffff));
        k.interp = static_cast<Spec3DKeyInterp> (juce::jlimit (0, 3, (int) c.getProperty ("interp", 1)));
        k.inTangent = (float) (double) c.getProperty ("inTan", 0.0);
        k.outTangent = (float) (double) c.getProperty ("outTan", 0.0);
        e.keys.push_back (k);
    }
    e.sortKeys();
    return e;
}

// ── Lanes ───────────────────────────────────────────────────────────────────

juce::String Spec3DSeqLane::defaultLabel (Spec3DSeqLaneType t)
{
    switch (t)
    {
        case Spec3DSeqLaneType::colourRamp:     return "Ramp";
        case Spec3DSeqLaneType::lightAmount:    return "Key amt";
        case Spec3DSeqLaneType::lightAzimuth:   return "Key az";
        case Spec3DSeqLaneType::lightElevation: return "Key el";
        case Spec3DSeqLaneType::lightColour:    return "Key col";
        case Spec3DSeqLaneType::rimAmount:      return "Rim amt";
        case Spec3DSeqLaneType::rimColour:      return "Rim col";
        default: return "Lane";
    }
}

Spec3DSeqLane Spec3DSeqLane::makeDefault (Spec3DSeqLaneType t)
{
    Spec3DSeqLane lane;
    lane.type = t;
    lane.label = defaultLabel (t);
    switch (t)
    {
        case Spec3DSeqLaneType::lightAmount:
            lane.floatEnv = { 0.0f, 1.0f, 0.7f, {} };
            break;
        case Spec3DSeqLaneType::lightAzimuth:
            lane.floatEnv = { -180.0f, 180.0f, -40.0f, {} };
            break;
        case Spec3DSeqLaneType::lightElevation:
            lane.floatEnv = { 5.0f, 89.0f, 55.0f, {} };
            break;
        case Spec3DSeqLaneType::rimAmount:
            lane.floatEnv = { 0.0f, 1.0f, 0.22f, {} };
            break;
        case Spec3DSeqLaneType::lightColour:
        case Spec3DSeqLaneType::rimColour:
            lane.colourEnv = {};
            break;
        default:
            break;
    }
    return lane;
}

// ── Sequence ────────────────────────────────────────────────────────────────

void Spec3DRampSequence::clamp() noexcept
{
    lengthSec = juce::jlimit (kMinLengthSec, kMaxLengthSec, lengthSec);
    defaultCrossfadeSec = juce::jmax (0.0f, defaultCrossfadeSec);
    if ((int) clips.size() > kMaxClips)
        clips.resize ((size_t) kMaxClips);
    for (auto& c : clips)
    {
        c.weight = juce::jmax (kMinWeight, c.weight);
        c.crossfadeOutSec = juce::jmax (0.0f, c.crossfadeOutSec);
        if (c.ramp.stops.size() >= 2)
            c.ramp.enabled = true;
    }
}

bool Spec3DRampSequence::hasLaneType (Spec3DSeqLaneType t) const noexcept
{
    if (t == Spec3DSeqLaneType::colourRamp)
        return true;
    for (const auto& l : autoLanes)
        if (l.type == t)
            return true;
    return false;
}

bool Spec3DRampSequence::addLane (Spec3DSeqLaneType t)
{
    if (t == Spec3DSeqLaneType::colourRamp || hasLaneType (t))
        return false;
    autoLanes.push_back (Spec3DSeqLane::makeDefault (t));
    return true;
}

void Spec3DRampSequence::removeLaneAt (int autoLaneIndex)
{
    if (juce::isPositiveAndBelow (autoLaneIndex, (int) autoLanes.size()))
        autoLanes.erase (autoLanes.begin() + autoLaneIndex);
}

juce::ValueTree Spec3DRampSequence::toValueTree() const
{
    juce::ValueTree root ("Spec3DRampSequence");
    root.setProperty ("enabled", enabled, nullptr);
    root.setProperty ("rampLaneEnabled", rampLaneEnabled, nullptr);
    root.setProperty ("lengthSec", (double) lengthSec, nullptr);
    root.setProperty ("defaultCrossfadeSec", (double) defaultCrossfadeSec, nullptr);

    for (const auto& c : clips)
    {
        juce::ValueTree clip ("Clip");
        clip.setProperty ("name", c.displayName, nullptr);
        clip.setProperty ("preset", c.displayName, nullptr); // legacy
        clip.setProperty ("weight", (double) c.weight, nullptr);
        clip.setProperty ("crossfadeOutSec", (double) c.crossfadeOutSec, nullptr);
        clip.appendChild (c.ramp.toValueTree ("Ramp"), nullptr);
        root.appendChild (std::move (clip), nullptr);
    }

    for (const auto& lane : autoLanes)
    {
        juce::ValueTree lt ("Lane");
        lt.setProperty ("type", (int) lane.type, nullptr);
        lt.setProperty ("label", lane.label, nullptr);
        lt.setProperty ("enabled", lane.enabled, nullptr);
        if (lane.isColourLane())
            lt.appendChild (lane.colourEnv.toValueTree ("ColourEnv"), nullptr);
        else
            lt.appendChild (lane.floatEnv.toValueTree ("FloatEnv"), nullptr);
        root.appendChild (std::move (lt), nullptr);
    }
    return root;
}

Spec3DRampSequence Spec3DRampSequence::fromValueTree (const juce::ValueTree& tree)
{
    Spec3DRampSequence s;
    if (! tree.isValid() || ! tree.hasType ("Spec3DRampSequence"))
        return s;

    s.enabled = (bool) tree.getProperty ("enabled", false);
    s.rampLaneEnabled = (bool) tree.getProperty ("rampLaneEnabled", true);
    s.lengthSec = (float) (double) tree.getProperty ("lengthSec", (double) kDefaultLengthSec);
    s.defaultCrossfadeSec = (float) (double) tree.getProperty ("defaultCrossfadeSec",
                                                              (double) kDefaultCrossfadeSec);

    for (int i = 0; i < tree.getNumChildren(); ++i)
    {
        const auto child = tree.getChild (i);
        if (child.hasType ("Clip") && (int) s.clips.size() < kMaxClips)
        {
            Spec3DRampClip c;
            c.displayName = child.getProperty ("name", child.getProperty ("preset")).toString();
            c.weight = (float) (double) child.getProperty ("weight", 1.0);
            c.crossfadeOutSec = (float) (double) child.getProperty ("crossfadeOutSec",
                                                                   (double) s.defaultCrossfadeSec);
            auto rampTree = child.getChildWithName ("Ramp");
            if (rampTree.isValid())
                c.ramp = GradientRamp::fromValueTree (rampTree);
            s.clips.push_back (std::move (c));
        }
        else if (child.hasType ("Lane"))
        {
            const auto type = static_cast<Spec3DSeqLaneType> (
                juce::jlimit (1, 6, (int) child.getProperty ("type", 1)));
            Spec3DSeqLane lane = Spec3DSeqLane::makeDefault (type);
            lane.label = child.getProperty ("label", lane.label).toString();
            lane.enabled = (bool) child.getProperty ("enabled", true);
            if (auto ce = child.getChildWithName ("ColourEnv"); ce.isValid())
                lane.colourEnv = Spec3DColourEnvelope::fromValueTree (ce);
            if (auto fe = child.getChildWithName ("FloatEnv"); fe.isValid())
                lane.floatEnv = Spec3DFloatEnvelope::fromValueTree (fe);
            s.autoLanes.push_back (std::move (lane));
        }
    }
    s.clamp();
    return s;
}

void Spec3DRampSequence::hydrateFromStore (const RampPresetStore& store)
{
    for (auto& c : clips)
    {
        if (c.ramp.stops.size() >= 2)
        {
            c.ramp.enabled = true;
            continue;
        }
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        if (resolvePreset (store, c.displayName, r) || resolvePreset (store, c.displayName, r))
        {
            c.ramp = std::move (r);
            c.ramp.enabled = true;
        }
    }
}

void Spec3DRampSequence::buildLayout (std::vector<LaidOutClip>& out) const
{
    out.clear();
    const int n = (int) clips.size();
    if (n <= 0)
        return;

    float sumW = 0.0f;
    for (const auto& c : clips)
        sumW += juce::jmax (kMinWeight, c.weight);
    if (sumW <= 1.0e-6f)
        sumW = 1.0f;

    const float len = juce::jlimit (kMinLengthSec, kMaxLengthSec, lengthSec);
    float cursor = 0.0f;
    out.reserve ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        LaidOutClip L;
        L.index = i;
        L.startSec = cursor;
        const float w = juce::jmax (kMinWeight, clips[(size_t) i].weight);
        const float dur = len * (w / sumW);
        L.endSec = (i == n - 1) ? len : (cursor + dur);
        cursor = L.endSec;
        out.push_back (L);
    }

    for (int i = 0; i < n; ++i)
    {
        const int next = (i + 1) % n;
        const float body = juce::jmax (1.0e-4f, out[(size_t) i].endSec - out[(size_t) i].startSec);
        const float nextBody = juce::jmax (1.0e-4f, out[(size_t) next].endSec - out[(size_t) next].startSec);
        const float maxFade = juce::jmin (body * 0.5f, nextBody * 0.5f);
        out[(size_t) i].fadeOutSec = juce::jlimit (0.0f, maxFade, clips[(size_t) i].crossfadeOutSec);
    }
}

bool Spec3DRampSequence::resolvePreset (const RampPresetStore& store, const juce::String& name,
                                        GradientRamp& dest)
{
    if (name.isEmpty())
        return false;
    const auto& presets = store.getPresets();
    for (int i = 0; i < presets.size(); ++i)
        if (presets.getReference (i).name.equalsIgnoreCase (name))
            return store.applyPreset (i, dest);
    return false;
}

bool Spec3DRampSequence::evaluate (float timeSec, GradientRamp& out) const
{
    if (! enabled || ! rampLaneEnabled || clips.empty())
        return false;

    std::vector<LaidOutClip> layout;
    buildLayout (layout);
    if (layout.empty())
        return false;

    const float len = juce::jlimit (kMinLengthSec, kMaxLengthSec, lengthSec);
    const float t = wrapTime (timeSec, len);
    const int n = (int) layout.size();

    int solid = 0;
    for (int i = 0; i < n; ++i)
    {
        if (t >= layout[(size_t) i].startSec && t < layout[(size_t) i].endSec)
        {
            solid = i;
            break;
        }
        if (i == n - 1)
            solid = i;
    }

    const auto& L = layout[(size_t) solid];
    const auto& clipA = clips[(size_t) L.index];
    if (! clipA.ramp.isUsable() && clipA.ramp.stops.size() < 2)
        return false;

    GradientRamp rampA = clipA.ramp;
    rampA.enabled = true;

    const float fadeStart = L.endSec - L.fadeOutSec;
    const bool inFade = L.fadeOutSec > 1.0e-5f && t >= fadeStart && t < L.endSec;

    if (! inFade)
    {
        // Keep clip revision — do not bump every tick or solid holds thrash LUT/mesh.
        out = rampA;
        out.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        out.enabled = true;
        return true;
    }

    const int nextIdx = layout[(size_t) ((solid + 1) % n)].index;
    const auto& clipB = clips[(size_t) nextIdx];
    if (! clipB.ramp.isUsable() && clipB.ramp.stops.size() < 2)
    {
        out = rampA;
        out.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        out.enabled = true;
        return true;
    }

    GradientRamp rampB = clipB.ramp;
    rampB.enabled = true;
    const float u = L.fadeOutSec > 1.0e-6f
                        ? juce::jlimit (0.0f, 1.0f, (t - fadeStart) / L.fadeOutSec)
                        : 1.0f;
    out = lerpRamps (rampA, rampB, u, 12); // 12 poles — lighter than 16
    out.mapMode = GradientRamp::MapMode::intensityLowToHigh;
    return out.isUsable();
}
