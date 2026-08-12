#pragma once

#include "ParticleDataTypes.h"
#include "ParticleAttributes.h"
#include <cstdint>

/**
    Particle node-graph (Embergen / Houdini / UE Niagara-style).
    Typed pins map onto DataType; compile -> Spec3D stack + attribute program.
*/
namespace ParticleNodeGraph
{

/** Pin types - mirrors DataType for wiring; kept as PinType for existing call sites. */
enum class PinType : uint8_t
{
    floatT = 0,
    vec3,
    boolT,
    force,
    emitter,
    exec,
    any,
    // Extended universal types
    intT,
    vec2,
    vec4,
    colour,
    particles,
    ramp,
    curve,
    field   // reserved: fluids / sparse fields later
};

inline DataType pinTypeToDataType (PinType t) noexcept
{
    switch (t)
    {
        case PinType::floatT:    return DataType::floatT;
        case PinType::vec3:      return DataType::vec3;
        case PinType::boolT:     return DataType::boolT;
        case PinType::force:     return DataType::force;
        case PinType::emitter:   return DataType::emitter;
        case PinType::exec:      return DataType::exec;
        case PinType::intT:      return DataType::intT;
        case PinType::vec2:      return DataType::vec2;
        case PinType::vec4:      return DataType::vec4;
        case PinType::colour:    return DataType::colour;
        case PinType::particles: return DataType::particles;
        case PinType::ramp:      return DataType::ramp;
        case PinType::curve:     return DataType::curve;
        case PinType::field:     return DataType::field;
        default:                 return DataType::any;
    }
}

inline PinType dataTypeToPinType (DataType t) noexcept
{
    switch (t)
    {
        case DataType::floatT:    return PinType::floatT;
        case DataType::vec3:      return PinType::vec3;
        case DataType::boolT:     return PinType::boolT;
        case DataType::force:     return PinType::force;
        case DataType::emitter:   return PinType::emitter;
        case DataType::exec:      return PinType::exec;
        case DataType::intT:      return PinType::intT;
        case DataType::vec2:      return PinType::vec2;
        case DataType::vec4:      return PinType::vec4;
        case DataType::colour:    return PinType::colour;
        case DataType::particles: return PinType::particles;
        case DataType::ramp:      return PinType::ramp;
        case DataType::curve:     return PinType::curve;
        case DataType::field:     return PinType::field;
        default:                 return PinType::any;
    }
}

inline const char* pinTypeName (PinType t) noexcept
{
    return dataTypeName (pinTypeToDataType (t));
}

inline juce::Colour pinTypeColour (PinType t) noexcept
{
    return dataTypeColour (pinTypeToDataType (t));
}

inline bool pinTypesCompatible (PinType from, PinType to) noexcept
{
    return dataTypesCompatible (pinTypeToDataType (from), pinTypeToDataType (to));
}

enum class NodeKind : uint16_t
{
    // IO / system  (ids stable - serialized in ValueTree)
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
    floatToVec3,
    switchFloat,

    // Combine
    combineForce,
    combineEmitter,
    combineFloat,
    combineVec3,

    // ── Framework expansion (append only) ──────────────────────────
    constInt,
    constVec2,
    constVec4,
    constColour,

    makeVec2,
    breakVec2,
    makeVec4,
    breakVec4,
    makeColour,
    breakColour,

    colourLerp,
    colourMul,

    mapRange,
    thresholdGate,

    convertToFloat,
    convertToVec3,
    convertToColour,

    uniformTime,
    uniformDelta,
    uniformAmplitude,

    /** Filter particle stream by attribute (amount / threshold / curve). */
    filterAttr,
    /** Colour particles from attribute via 2-stop ramp. */
    colourRampAttr,
    combineParticles,

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
        case NodeKind::constInt:            return "Int";
        case NodeKind::constVec2:           return "Vector2";
        case NodeKind::constVec4:           return "Vector4";
        case NodeKind::constColour:         return "Colour";
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
        case NodeKind::makeVec2:            return "Make Vector2";
        case NodeKind::breakVec2:           return "Break Vector2";
        case NodeKind::makeVec3:            return "Make Vector3";
        case NodeKind::breakVec3:           return "Break Vector3";
        case NodeKind::makeVec4:            return "Make Vector4";
        case NodeKind::breakVec4:           return "Break Vector4";
        case NodeKind::makeColour:          return "Make Colour";
        case NodeKind::breakColour:         return "Break Colour";
        case NodeKind::vecLength:           return "Vector Length";
        case NodeKind::vecNormalize:        return "Normalize";
        case NodeKind::vecScale:            return "Scale Vector";
        case NodeKind::vecAdd:              return "Add Vector";
        case NodeKind::vecDot:              return "Dot Product";
        case NodeKind::floatToVec3:         return "Float -> Vector3";
        case NodeKind::switchFloat:         return "Switch (Float)";
        case NodeKind::colourLerp:          return "Lerp Colour";
        case NodeKind::colourMul:           return "Multiply Colour";
        case NodeKind::mapRange:            return "Map Range";
        case NodeKind::thresholdGate:       return "Threshold";
        case NodeKind::convertToFloat:      return "To Float";
        case NodeKind::convertToVec3:       return "To Vector3";
        case NodeKind::convertToColour:     return "To Colour";
        case NodeKind::uniformTime:         return "Uniform: Time";
        case NodeKind::uniformDelta:        return "Uniform: Delta";
        case NodeKind::uniformAmplitude:    return "Uniform: Amplitude";
        case NodeKind::filterAttr:          return "Filter by Attribute";
        case NodeKind::colourRampAttr:      return "Colour Ramp (Attribute)";
        case NodeKind::combineForce:        return "Combine Forces";
        case NodeKind::combineEmitter:      return "Combine Emitters";
        case NodeKind::combineFloat:        return "Combine Float (Sum)";
        case NodeKind::combineVec3:         return "Combine Vector3 (Sum)";
        case NodeKind::combineParticles:    return "Combine Particles";
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
        case NodeKind::combineParticles:
            return "Combine";
        case NodeKind::constFloat:
        case NodeKind::constVec2:
        case NodeKind::constVec3:
        case NodeKind::constVec4:
        case NodeKind::constBool:
        case NodeKind::constInt:
        case NodeKind::constColour:
            return "Constants";
        case NodeKind::makeVec2:
        case NodeKind::breakVec2:
        case NodeKind::makeVec3:
        case NodeKind::breakVec3:
        case NodeKind::makeVec4:
        case NodeKind::breakVec4:
        case NodeKind::vecLength:
        case NodeKind::vecNormalize:
        case NodeKind::vecScale:
        case NodeKind::vecAdd:
        case NodeKind::vecDot:
        case NodeKind::floatToVec3:
            return "Vector";
        case NodeKind::makeColour:
        case NodeKind::breakColour:
        case NodeKind::colourLerp:
        case NodeKind::colourMul:
        case NodeKind::colourRampAttr:
            return "Colour";
        case NodeKind::filterAttr:
            return "Attributes";
        case NodeKind::uniformTime:
        case NodeKind::uniformDelta:
        case NodeKind::uniformAmplitude:
            return "Uniforms";
        case NodeKind::mapRange:
        case NodeKind::thresholdGate:
        case NodeKind::convertToFloat:
        case NodeKind::convertToVec3:
        case NodeKind::convertToColour:
            return "Utility";
        default:
            return "Math";
    }
}

/** Default evaluation stage for a node kind. */
inline EvalStage nodeKindDefaultStage (NodeKind k) noexcept
{
    switch (k)
    {
        case NodeKind::filterAttr:
        case NodeKind::colourRampAttr:
            return EvalStage::either; // stage stored in params
        case NodeKind::forceGravity:
        case NodeKind::forceDrag:
        case NodeKind::forceWind:
        case NodeKind::forceCurl:
        case NodeKind::forceTurbulence:
        case NodeKind::forceRotation:
            return EvalStage::update;
        case NodeKind::emitterSpectrogram:
        case NodeKind::emitterPoint:
            return EvalStage::spawn;
        default:
            return EvalStage::either;
    }
}

/** Pins that accept multiple inbound wires (Niagara-style fan-in). */
inline bool pinAllowsMultiWire (NodeKind nodeKind, const juce::String& pinId) noexcept
{
    if (nodeKind == NodeKind::simOutput
        && (pinId == "force" || pinId == "emitter" || pinId == "particles"))
        return true;
    if ((nodeKind == NodeKind::combineForce || nodeKind == NodeKind::combineEmitter
         || nodeKind == NodeKind::combineFloat || nodeKind == NodeKind::combineVec3
         || nodeKind == NodeKind::combineParticles)
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
    juce::Array<juce::String> paramKeys;
    juce::Array<float> paramDefaults;
    EvalStage stage = EvalStage::either;
};

NodeDesc describeNode (NodeKind kind);

} // namespace ParticleNodeGraph
