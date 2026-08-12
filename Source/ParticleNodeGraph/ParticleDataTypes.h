#pragma once

#include <JuceHeader.h>
#include <cstdint>
#include <cmath>
#include <cstring>

/**
    Universal typed value system for the particle (and future field/fluid) graph.
    Channel data (bool…colour) plus domain handles (particles, force, field, …).

    Design goals (Houdini / Niagara / Embergen-style):
      - One Value payload for constants, uniforms, and attribute samples
      - Explicit converts; soft promote only where intentional (float→vec broadcast)
      - Domain handles reserved for fluids/fields without changing the core later
*/
namespace ParticleNodeGraph
{

/** Graph / attribute channel + domain types. */
enum class DataType : uint8_t
{
    boolT = 0,
    intT,
    floatT,
    vec2,
    vec3,
    vec4,
    colour,     // linear RGBA; storage = 4 floats, distinct UI/ops
    // Domain / handle types (not pure channel math)
    particles,  // particle stream (spawn→filter→update)
    emitter,    // emitter configuration (legacy + setup)
    force,      // force module contribution
    ramp,       // float or colour ramp asset handle
    curve,      // 1D curve asset handle
    texture,    // reserved
    field,      // reserved: density / velocity / temperature field (fluids later)
    exec,       // optional control flow
    any,        // wildcard match only
    count
};

inline bool isChannelType (DataType t) noexcept
{
    switch (t)
    {
        case DataType::boolT:
        case DataType::intT:
        case DataType::floatT:
        case DataType::vec2:
        case DataType::vec3:
        case DataType::vec4:
        case DataType::colour:
            return true;
        default:
            return false;
    }
}

inline bool isDomainType (DataType t) noexcept
{
    switch (t)
    {
        case DataType::particles:
        case DataType::emitter:
        case DataType::force:
        case DataType::ramp:
        case DataType::curve:
        case DataType::texture:
        case DataType::field:
            return true;
        default:
            return false;
    }
}

/** Number of float channels for channel types (0 for domain). */
inline int dataTypeChannels (DataType t) noexcept
{
    switch (t)
    {
        case DataType::boolT:
        case DataType::intT:
        case DataType::floatT:  return 1;
        case DataType::vec2:    return 2;
        case DataType::vec3:    return 3;
        case DataType::vec4:
        case DataType::colour:  return 4;
        default:                return 0;
    }
}

inline const char* dataTypeName (DataType t) noexcept
{
    switch (t)
    {
        case DataType::boolT:     return "Bool";
        case DataType::intT:      return "Int";
        case DataType::floatT:    return "Float";
        case DataType::vec2:      return "Vector2";
        case DataType::vec3:      return "Vector3";
        case DataType::vec4:      return "Vector4";
        case DataType::colour:    return "Colour";
        case DataType::particles: return "Particles";
        case DataType::emitter:   return "Emitter";
        case DataType::force:     return "Force";
        case DataType::ramp:      return "Ramp";
        case DataType::curve:     return "Curve";
        case DataType::texture:   return "Texture";
        case DataType::field:     return "Field";
        case DataType::exec:      return "Exec";
        default:                  return "Any";
    }
}

inline juce::Colour dataTypeColour (DataType t) noexcept
{
    switch (t)
    {
        case DataType::boolT:     return juce::Colour (0xffff6b6b);
        case DataType::intT:      return juce::Colour (0xff6bcb8a); // mint-green int
        case DataType::floatT:    return juce::Colour (0xff5ec8ff);
        case DataType::vec2:      return juce::Colour (0xff9b7cff);
        case DataType::vec3:      return juce::Colour (0xffb07cff);
        case DataType::vec4:      return juce::Colour (0xffc49bff);
        case DataType::colour:    return juce::Colour (0xffff9f43); // orange (distinct from force amber)
        case DataType::particles: return juce::Colour (0xff5ad4c8); // teal stream
        case DataType::emitter:   return juce::Colour (0xff7dffa3);
        case DataType::force:     return juce::Colour (0xffffb347);
        case DataType::ramp:      return juce::Colour (0xffff6ec7);
        case DataType::curve:     return juce::Colour (0xffe0a0ff);
        case DataType::texture:   return juce::Colour (0xff88aaff);
        case DataType::field:     return juce::Colour (0xff4ecdc4); // fluids later
        case DataType::exec:      return juce::Colour (0xffeeeeee);
        default:                  return juce::Colours::grey;
    }
}

/**
    Wire compatibility.
    - same type
    - any wildcard
    - particles ↔ emitter (bridge: emitter config seeds a particle stream)
    - colour ↔ vec4 (explicit soft link for wiring; prefer Convert for clarity)
*/
inline bool dataTypesCompatible (DataType from, DataType to) noexcept
{
    if (from == DataType::any || to == DataType::any)
        return true;
    if (from == to)
        return true;
    // Emitter output can feed a particles input (spawn stream entry).
    if (from == DataType::emitter && to == DataType::particles)
        return true;
    if (from == DataType::particles && to == DataType::emitter)
        return true;
    // Colour / vec4 soft wire (both 4-float).
    if ((from == DataType::colour && to == DataType::vec4)
        || (from == DataType::vec4 && to == DataType::colour))
        return true;
    return false;
}

/** Evaluation stage — when an op runs. Field/fluid sims will add Advection etc. later. */
enum class EvalStage : uint8_t
{
    either = 0,  // constant / utility
    spawn  = 1,  // birth only
    update = 2,  // every sim tick
    render = 3,  // draw bind (future)
    /** Reserved: field / fluid step (not implemented). */
    fieldStep = 4
};

inline const char* evalStageName (EvalStage s) noexcept
{
    switch (s)
    {
        case EvalStage::spawn:     return "Spawn";
        case EvalStage::update:    return "Update";
        case EvalStage::render:    return "Render";
        case EvalStage::fieldStep: return "Field";
        default:                   return "Either";
    }
}

/**
    Universal value: up to 4 float channels + int payload + optional asset handle.
    Domain types use handle (and optionally i for sub-kind).
*/
struct Value
{
    DataType type = DataType::floatT;
    float    f[4] { 0, 0, 0, 0 };
    int32_t  i = 0;
    uint32_t handle = 0;

    static Value makeBool (bool v) noexcept
    {
        Value r; r.type = DataType::boolT; r.i = v ? 1 : 0; r.f[0] = v ? 1.0f : 0.0f; return r;
    }
    static Value makeInt (int v) noexcept
    {
        Value r; r.type = DataType::intT; r.i = v; r.f[0] = (float) v; return r;
    }
    static Value makeFloat (float v) noexcept
    {
        Value r; r.type = DataType::floatT; r.f[0] = v; return r;
    }
    static Value makeVec2 (float x, float y) noexcept
    {
        Value r; r.type = DataType::vec2; r.f[0] = x; r.f[1] = y; return r;
    }
    static Value makeVec3 (float x, float y, float z) noexcept
    {
        Value r; r.type = DataType::vec3; r.f[0] = x; r.f[1] = y; r.f[2] = z; return r;
    }
    static Value makeVec4 (float x, float y, float z, float w) noexcept
    {
        Value r; r.type = DataType::vec4; r.f[0] = x; r.f[1] = y; r.f[2] = z; r.f[3] = w; return r;
    }
    static Value makeColour (float r, float g, float b, float a = 1.0f) noexcept
    {
        Value v; v.type = DataType::colour; v.f[0] = r; v.f[1] = g; v.f[2] = b; v.f[3] = a; return v;
    }
    static Value makeColour (juce::Colour c) noexcept
    {
        return makeColour (c.getFloatRed(), c.getFloatGreen(), c.getFloatBlue(), c.getFloatAlpha());
    }
    static Value makeHandle (DataType domain, uint32_t h, int sub = 0) noexcept
    {
        Value r; r.type = domain; r.handle = h; r.i = sub; return r;
    }

    bool asBool() const noexcept
    {
        if (type == DataType::boolT || type == DataType::intT) return i != 0;
        return f[0] > 0.5f;
    }
    int asInt() const noexcept
    {
        if (type == DataType::intT || type == DataType::boolT) return i;
        return (int) std::lround (f[0]);
    }
    float asFloat() const noexcept { return f[0]; }
    juce::Vector3D<float> asVec3() const noexcept { return { f[0], f[1], f[2] }; }
    juce::Colour asColour() const noexcept
    {
        return juce::Colour::fromFloatRGBA (f[0], f[1], f[2],
                                            type == DataType::colour || type == DataType::vec4 ? f[3] : 1.0f);
    }

    bool hasChannelData() const noexcept { return isChannelType (type); }
};

/** Map-range + bipolar curve (shared with mod matrix semantics). */
inline float applyCurve01 (float x, float shape) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    shape = juce::jlimit (-1.0f, 1.0f, shape);
    if (std::abs (shape) < 1.0e-4f)
        return x;
    const float power = std::pow (4.0f, shape);
    return std::pow (juce::jmax (0.0f, x), power);
}

inline float mapRange (float x, float inMin, float inMax, float outMin, float outMax) noexcept
{
    const float d = inMax - inMin;
    const float t = std::abs (d) > 1.0e-12f ? (x - inMin) / d : 0.0f;
    return outMin + (outMax - outMin) * t;
}

/** Sample a 2-stop colour ramp (extend later to multi-stop assets). */
inline Value sampleColourRamp2 (float t, const Value& c0, const Value& c1, float curveShape = 0.0f,
                                bool invert = false) noexcept
{
    t = applyCurve01 (t, curveShape);
    if (invert) t = 1.0f - t;
    Value r;
    r.type = DataType::colour;
    for (int i = 0; i < 4; ++i)
        r.f[i] = c0.f[i] + (c1.f[i] - c0.f[i]) * t;
    return r;
}

/** Convert between channel types (explicit). Returns empty float on failure. */
inline Value convertValue (const Value& v, DataType to) noexcept
{
    if (v.type == to)
        return v;
    Value r;
    r.type = to;
    if (to == DataType::boolT)
    {
        r.i = v.asBool() ? 1 : 0;
        r.f[0] = r.i ? 1.0f : 0.0f;
        return r;
    }
    if (to == DataType::intT)
    {
        r.i = v.asInt();
        r.f[0] = (float) r.i;
        return r;
    }
    if (to == DataType::floatT)
    {
        // vec length for multi-channel; colour uses luminance-ish
        if (v.type == DataType::vec2)
            r.f[0] = std::sqrt (v.f[0] * v.f[0] + v.f[1] * v.f[1]);
        else if (v.type == DataType::vec3)
            r.f[0] = std::sqrt (v.f[0] * v.f[0] + v.f[1] * v.f[1] + v.f[2] * v.f[2]);
        else if (v.type == DataType::vec4 || v.type == DataType::colour)
            r.f[0] = 0.2126f * v.f[0] + 0.7152f * v.f[1] + 0.0722f * v.f[2];
        else
            r.f[0] = v.f[0];
        return r;
    }
    if (to == DataType::vec2 || to == DataType::vec3 || to == DataType::vec4 || to == DataType::colour)
    {
        const int n = dataTypeChannels (to);
        // Broadcast scalar
        if (v.type == DataType::floatT || v.type == DataType::intT || v.type == DataType::boolT)
        {
            const float s = (v.type == DataType::floatT) ? v.f[0] : (float) v.asInt();
            for (int i = 0; i < n; ++i)
                r.f[i] = s;
            if (to == DataType::colour && n == 4)
                r.f[3] = 1.0f;
            return r;
        }
        for (int i = 0; i < n; ++i)
            r.f[i] = v.f[i < dataTypeChannels (v.type) ? i : 0];
        if (to == DataType::colour && dataTypeChannels (v.type) < 4)
            r.f[3] = 1.0f;
        return r;
    }
    // Domain types: only same-type or empty
    r.type = DataType::floatT;
    return r;
}

} // namespace ParticleNodeGraph
