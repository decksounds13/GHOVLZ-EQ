#pragma once

#include <JuceHeader.h>
#include <cstdint>

/**
    Particle node-graph (Embergen / Houdini / UE Niagara-style).
    Typed pins, graph topology, compile → Spec3D particle stack.
*/
namespace ParticleNodeGraph
{
enum class PinType : uint8_t
{
    floatT = 0,
    vec3,
    boolT,
    force,      // force module contribution
    emitter,    // emitter configuration
    exec,       // optional flow (kept for future)
    any         // never stored; used for loose matching
};

inline const char* pinTypeName (PinType t) noexcept
{
    switch (t)
    {
        case PinType::floatT:  return "Float";
        case PinType::vec3:    return "Vector3";
        case PinType::boolT:   return "Bool";
        case PinType::force:   return "Force";
        case PinType::emitter: return "Emitter";
        case PinType::exec:    return "Exec";
        default:               return "Any";
    }
}

inline juce::Colour pinTypeColour (PinType t) noexcept
{
    switch (t)
    {
        case PinType::floatT:  return juce::Colour (0xff5ec8ff); // cyan
        case PinType::vec3:    return juce::Colour (0xffb07cff); // purple
        case PinType::boolT:   return juce::Colour (0xffff6b6b); // red
        case PinType::force:   return juce::Colour (0xffffb347); // amber
        case PinType::emitter: return juce::Colour (0xff7dffa3); // mint
        case PinType::exec:    return juce::Colour (0xffeeeeee);
        default:               return juce::Colours::grey;
    }
}

/** True if a wire from `from` can feed `to` (strict: same type). */
inline bool pinTypesCompatible (PinType from, PinType to) noexcept
{
    if (from == PinType::any || to == PinType::any)
        return true;
    return from == to;
}

enum class NodeKind : uint16_t
{
    // IO / system
    simOutput = 0,
    comment,

    // Emitters
    emitterSpectrogram,
    emitterPoint,

    // Forces
    forceGravity,
    forceDrag,
    forceWind,
    forceCurl,
    forceTurbulence,
    forceRotation,

    // Constants
    constFloat,
    constVec3,
    constBool,

    // Math (float)
    mathAdd,
    mathSub,
    mathMul,
    mathDiv,
    mathLerp,
    mathClamp,
    mathAbs,
    mathNegate,
    mathMin,
    mathMax,
    mathPow,
    mathSin,
    mathCos,

    // Vector
    makeVec3,
    breakVec3,
    vecLength,
    vecNormalize,
    vecScale,
    vecAdd,
    vecDot,

    // Utility
    floatToVec3, // broadcast float → vec3
    switchFloat,

    // Combine / merge (fan-in; multi-wire on each In pin)
    combineForce,    // merge Force → Force
    combineEmitter,  // merge Emitter → Emitter (primary + extras for future multi-emit)
    combineFloat,    // sum Float → Float
    combineVec3,     // sum Vector3 → Vector3

    count
};

inline const char* nodeKindName (NodeKind k) noexcept
{
    switch (k)
    {
        case NodeKind::simOutput:           return "Simulation Output";
        case NodeKind::comment:             return "Comment";
        case NodeKind::emitterSpectrogram:  return "Emitter: Spectrogram";
        case NodeKind::emitterPoint:        return "Emitter: Point";
        case NodeKind::forceGravity:        return "Force: Gravity";
        case NodeKind::forceDrag:           return "Force: Drag";
        case NodeKind::forceWind:           return "Force: Wind";
        case NodeKind::forceCurl:           return "Force: Curl Noise";
        case NodeKind::forceTurbulence:     return "Force: Turbulence";
        case NodeKind::forceRotation:       return "Force: Rotation";
        case NodeKind::constFloat:          return "Float";
        case NodeKind::constVec3:           return "Vector3";
        case NodeKind::constBool:           return "Bool";
        case NodeKind::mathAdd:             return "Add";
        case NodeKind::mathSub:             return "Subtract";
        case NodeKind::mathMul:             return "Multiply";
        case NodeKind::mathDiv:             return "Divide";
        case NodeKind::mathLerp:            return "Lerp";
        case NodeKind::mathClamp:           return "Clamp";
        case NodeKind::mathAbs:             return "Abs";
        case NodeKind::mathNegate:          return "Negate";
        case NodeKind::mathMin:             return "Min";
        case NodeKind::mathMax:             return "Max";
        case NodeKind::mathPow:             return "Power";
        case NodeKind::mathSin:             return "Sin";
        case NodeKind::mathCos:             return "Cos";
        case NodeKind::makeVec3:            return "Make Vector3";
        case NodeKind::breakVec3:           return "Break Vector3";
        case NodeKind::vecLength:           return "Vector Length";
        case NodeKind::vecNormalize:        return "Normalize";
        case NodeKind::vecScale:            return "Scale Vector";
        case NodeKind::vecAdd:              return "Add Vector";
        case NodeKind::vecDot:              return "Dot Product";
        case NodeKind::floatToVec3:         return "Float → Vector3";
        case NodeKind::switchFloat:         return "Switch (Float)";
        case NodeKind::combineForce:        return "Combine Forces";
        case NodeKind::combineEmitter:      return "Combine Emitters";
        case NodeKind::combineFloat:        return "Combine Float (Sum)";
        case NodeKind::combineVec3:         return "Combine Vector3 (Sum)";
        default:                            return "Node";
    }
}

inline juce::String nodeKindCategory (NodeKind k)
{
    switch (k)
    {
        case NodeKind::simOutput:
        case NodeKind::comment:
            return "System";
        case NodeKind::emitterSpectrogram:
        case NodeKind::emitterPoint:
            return "Emitters";
        case NodeKind::forceGravity:
        case NodeKind::forceDrag:
        case NodeKind::forceWind:
        case NodeKind::forceCurl:
        case NodeKind::forceTurbulence:
        case NodeKind::forceRotation:
            return "Forces";
        case NodeKind::combineForce:
        case NodeKind::combineEmitter:
        case NodeKind::combineFloat:
        case NodeKind::combineVec3:
            return "Combine";
        case NodeKind::constFloat:
        case NodeKind::constVec3:
        case NodeKind::constBool:
            return "Constants";
        case NodeKind::makeVec3:
        case NodeKind::breakVec3:
        case NodeKind::vecLength:
        case NodeKind::vecNormalize:
        case NodeKind::vecScale:
        case NodeKind::vecAdd:
        case NodeKind::vecDot:
        case NodeKind::floatToVec3:
            return "Vector";
        default:
            return "Math";
    }
}

/** Pins that accept multiple inbound wires (Niagara-style fan-in). */
inline bool pinAllowsMultiWire (NodeKind nodeKind, const juce::String& pinId) noexcept
{
    if (nodeKind == NodeKind::simOutput && (pinId == "force" || pinId == "emitter"))
        return true;
    if ((nodeKind == NodeKind::combineForce || nodeKind == NodeKind::combineEmitter
         || nodeKind == NodeKind::combineFloat || nodeKind == NodeKind::combineVec3)
        && pinId.startsWith ("in"))
        return true;
    return false;
}

struct PinDesc
{
    juce::String id;
    juce::String label;
    PinType type = PinType::floatT;
    bool isInput = true;
};

struct NodeDesc
{
    NodeKind kind = NodeKind::constFloat;
    juce::String title;
    juce::Array<PinDesc> inputs;
    juce::Array<PinDesc> outputs;
    /** Default scalar params stored on the node (UI knobs). */
    juce::Array<juce::String> paramKeys;
    juce::Array<float> paramDefaults;
};

/** Factory: pin layout + default params for each kind. */
NodeDesc describeNode (NodeKind kind);

} // namespace ParticleNodeGraph
