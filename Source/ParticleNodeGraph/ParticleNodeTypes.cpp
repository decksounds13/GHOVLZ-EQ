#include "ParticleNodeTypes.h"

namespace ParticleNodeGraph
{
namespace
{
PinDesc inPin (const char* id, const char* label, PinType t)
{
    return { id, label, t, true };
}
PinDesc outPin (const char* id, const char* label, PinType t)
{
    return { id, label, t, false };
}
} // namespace

NodeDesc describeNode (NodeKind kind)
{
    NodeDesc d;
    d.kind = kind;
    d.title = nodeKindName (kind);
    d.stage = nodeKindDefaultStage (kind);

    auto addP = [&] (const char* key, float def)
    {
        d.paramKeys.add (key);
        d.paramDefaults.add (def);
    };

    switch (kind)
    {
        case NodeKind::simOutput:
            // particles preferred; emitter still accepted via compatibility
            d.inputs.add (inPin ("particles", "Particles", PinType::particles));
            d.inputs.add (inPin ("emitter", "Emitter", PinType::emitter)); // legacy alias pin
            d.inputs.add (inPin ("force", "Forces", PinType::force));
            d.inputs.add (inPin ("rate", "Emission", PinType::floatT));
            d.inputs.add (inPin ("life", "Lifespan", PinType::floatT));
            d.inputs.add (inPin ("size", "Size", PinType::floatT));
            d.inputs.add (inPin ("enabled", "Enabled", PinType::boolT));
            addP ("defaultRate", 4000.0f);
            addP ("defaultLife", 2.0f);
            addP ("defaultSize", 0.02f);
            break;

        case NodeKind::comment:
            addP ("w", 180.0f);
            addP ("h", 60.0f);
            break;

        case NodeKind::emitterSpectrogram:
            d.outputs.add (outPin ("out", "Particles", PinType::particles));
            addP ("continuous", 1.0f);
            addP ("jitter", 0.0025f);
            break;

        case NodeKind::emitterPoint:
            d.outputs.add (outPin ("out", "Particles", PinType::particles));
            d.inputs.add (inPin ("pos", "Position", PinType::vec3));
            d.inputs.add (inPin ("vel", "Velocity", PinType::vec3));
            addP ("px", 0.0f);
            addP ("py", 0.25f);
            addP ("pz", 0.0f);
            addP ("vx", 0.0f);
            addP ("vy", 1.0f);
            addP ("vz", 0.0f);
            break;

        case NodeKind::forceGravity:
            d.outputs.add (outPin ("out", "Force", PinType::force));
            d.inputs.add (inPin ("strength", "Strength", PinType::floatT));
            addP ("strength", -0.02f);
            break;
        case NodeKind::forceDrag:
            d.outputs.add (outPin ("out", "Force", PinType::force));
            d.inputs.add (inPin ("strength", "Strength", PinType::floatT));
            addP ("strength", 0.01f);
            break;
        case NodeKind::forceWind:
            d.outputs.add (outPin ("out", "Force", PinType::force));
            d.inputs.add (inPin ("accel", "Accel", PinType::vec3));
            addP ("x", 0.005f);
            addP ("y", 0.0f);
            addP ("z", 0.0f);
            break;
        case NodeKind::forceCurl:
            d.outputs.add (outPin ("out", "Force", PinType::force));
            d.inputs.add (inPin ("strength", "Strength", PinType::floatT));
            d.inputs.add (inPin ("scale", "Scale", PinType::floatT));
            d.inputs.add (inPin ("speed", "Speed", PinType::floatT));
            addP ("strength", 0.01f);
            addP ("scale", 0.25f);
            addP ("speed", 0.04f);
            break;
        case NodeKind::forceTurbulence:
            d.outputs.add (outPin ("out", "Force", PinType::force));
            d.inputs.add (inPin ("strength", "Strength", PinType::floatT));
            addP ("strength", 0.005f);
            break;
        case NodeKind::forceRotation:
            d.outputs.add (outPin ("out", "Force", PinType::force));
            d.inputs.add (inPin ("rate", "Rate", PinType::vec3));
            addP ("x", 0.25f);
            addP ("y", 0.25f);
            addP ("z", 0.25f);
            break;

        case NodeKind::constFloat:
            d.outputs.add (outPin ("out", "Value", PinType::floatT));
            addP ("value", 1.0f);
            break;
        case NodeKind::constInt:
            d.outputs.add (outPin ("out", "Value", PinType::intT));
            addP ("value", 0.0f);
            break;
        case NodeKind::constVec2:
            d.outputs.add (outPin ("out", "Value", PinType::vec2));
            addP ("x", 0.0f);
            addP ("y", 0.0f);
            break;
        case NodeKind::constVec3:
            d.outputs.add (outPin ("out", "Value", PinType::vec3));
            addP ("x", 0.0f);
            addP ("y", 0.0f);
            addP ("z", 0.0f);
            break;
        case NodeKind::constVec4:
            d.outputs.add (outPin ("out", "Value", PinType::vec4));
            addP ("x", 0.0f);
            addP ("y", 0.0f);
            addP ("z", 0.0f);
            addP ("w", 1.0f);
            break;
        case NodeKind::constBool:
            d.outputs.add (outPin ("out", "Value", PinType::boolT));
            addP ("value", 1.0f);
            break;
        case NodeKind::constColour:
            d.outputs.add (outPin ("out", "Colour", PinType::colour));
            addP ("r", 1.0f);
            addP ("g", 0.55f);
            addP ("b", 0.15f);
            addP ("a", 1.0f);
            break;

        case NodeKind::mathAdd:
        case NodeKind::mathSub:
        case NodeKind::mathMul:
        case NodeKind::mathDiv:
        case NodeKind::mathMin:
        case NodeKind::mathMax:
        case NodeKind::mathPow:
            d.inputs.add (inPin ("a", "A", PinType::floatT));
            d.inputs.add (inPin ("b", "B", PinType::floatT));
            d.outputs.add (outPin ("out", "Result", PinType::floatT));
            addP ("b", kind == NodeKind::mathMul || kind == NodeKind::mathPow ? 1.0f : 0.0f);
            break;
        case NodeKind::mathLerp:
            d.inputs.add (inPin ("a", "A", PinType::floatT));
            d.inputs.add (inPin ("b", "B", PinType::floatT));
            d.inputs.add (inPin ("t", "T", PinType::floatT));
            d.outputs.add (outPin ("out", "Result", PinType::floatT));
            addP ("t", 0.5f);
            break;
        case NodeKind::mathClamp:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.inputs.add (inPin ("min", "Min", PinType::floatT));
            d.inputs.add (inPin ("max", "Max", PinType::floatT));
            d.outputs.add (outPin ("out", "Result", PinType::floatT));
            addP ("min", 0.0f);
            addP ("max", 1.0f);
            break;
        case NodeKind::mathAbs:
        case NodeKind::mathNegate:
        case NodeKind::mathSin:
        case NodeKind::mathCos:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.outputs.add (outPin ("out", "Result", PinType::floatT));
            break;

        case NodeKind::makeVec2:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.inputs.add (inPin ("y", "Y", PinType::floatT));
            d.outputs.add (outPin ("out", "Vector", PinType::vec2));
            break;
        case NodeKind::breakVec2:
            d.inputs.add (inPin ("v", "Vector", PinType::vec2));
            d.outputs.add (outPin ("x", "X", PinType::floatT));
            d.outputs.add (outPin ("y", "Y", PinType::floatT));
            break;
        case NodeKind::makeVec3:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.inputs.add (inPin ("y", "Y", PinType::floatT));
            d.inputs.add (inPin ("z", "Z", PinType::floatT));
            d.outputs.add (outPin ("out", "Vector", PinType::vec3));
            break;
        case NodeKind::breakVec3:
            d.inputs.add (inPin ("v", "Vector", PinType::vec3));
            d.outputs.add (outPin ("x", "X", PinType::floatT));
            d.outputs.add (outPin ("y", "Y", PinType::floatT));
            d.outputs.add (outPin ("z", "Z", PinType::floatT));
            break;
        case NodeKind::makeVec4:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.inputs.add (inPin ("y", "Y", PinType::floatT));
            d.inputs.add (inPin ("z", "Z", PinType::floatT));
            d.inputs.add (inPin ("w", "W", PinType::floatT));
            d.outputs.add (outPin ("out", "Vector", PinType::vec4));
            break;
        case NodeKind::breakVec4:
            d.inputs.add (inPin ("v", "Vector", PinType::vec4));
            d.outputs.add (outPin ("x", "X", PinType::floatT));
            d.outputs.add (outPin ("y", "Y", PinType::floatT));
            d.outputs.add (outPin ("z", "Z", PinType::floatT));
            d.outputs.add (outPin ("w", "W", PinType::floatT));
            break;
        case NodeKind::makeColour:
            d.inputs.add (inPin ("r", "R", PinType::floatT));
            d.inputs.add (inPin ("g", "G", PinType::floatT));
            d.inputs.add (inPin ("b", "B", PinType::floatT));
            d.inputs.add (inPin ("a", "A", PinType::floatT));
            d.outputs.add (outPin ("out", "Colour", PinType::colour));
            addP ("r", 1.0f);
            addP ("g", 1.0f);
            addP ("b", 1.0f);
            addP ("a", 1.0f);
            break;
        case NodeKind::breakColour:
            d.inputs.add (inPin ("c", "Colour", PinType::colour));
            d.outputs.add (outPin ("r", "R", PinType::floatT));
            d.outputs.add (outPin ("g", "G", PinType::floatT));
            d.outputs.add (outPin ("b", "B", PinType::floatT));
            d.outputs.add (outPin ("a", "A", PinType::floatT));
            break;
        case NodeKind::vecLength:
        case NodeKind::vecNormalize:
            d.inputs.add (inPin ("v", "Vector", PinType::vec3));
            d.outputs.add (outPin ("out", kind == NodeKind::vecLength ? "Length" : "Vector",
                                   kind == NodeKind::vecLength ? PinType::floatT : PinType::vec3));
            break;
        case NodeKind::vecScale:
            d.inputs.add (inPin ("v", "Vector", PinType::vec3));
            d.inputs.add (inPin ("s", "Scale", PinType::floatT));
            d.outputs.add (outPin ("out", "Vector", PinType::vec3));
            addP ("s", 1.0f);
            break;
        case NodeKind::vecAdd:
            d.inputs.add (inPin ("a", "A", PinType::vec3));
            d.inputs.add (inPin ("b", "B", PinType::vec3));
            d.outputs.add (outPin ("out", "Vector", PinType::vec3));
            break;
        case NodeKind::vecDot:
            d.inputs.add (inPin ("a", "A", PinType::vec3));
            d.inputs.add (inPin ("b", "B", PinType::vec3));
            d.outputs.add (outPin ("out", "Dot", PinType::floatT));
            break;
        case NodeKind::floatToVec3:
            d.inputs.add (inPin ("s", "Scalar", PinType::floatT));
            d.outputs.add (outPin ("out", "Vector", PinType::vec3));
            break;
        case NodeKind::switchFloat:
            d.inputs.add (inPin ("sel", "Select", PinType::boolT));
            d.inputs.add (inPin ("a", "False", PinType::floatT));
            d.inputs.add (inPin ("b", "True", PinType::floatT));
            d.outputs.add (outPin ("out", "Result", PinType::floatT));
            break;

        case NodeKind::colourLerp:
            d.inputs.add (inPin ("a", "A", PinType::colour));
            d.inputs.add (inPin ("b", "B", PinType::colour));
            d.inputs.add (inPin ("t", "T", PinType::floatT));
            d.outputs.add (outPin ("out", "Colour", PinType::colour));
            addP ("t", 0.5f);
            break;
        case NodeKind::colourMul:
            d.inputs.add (inPin ("a", "A", PinType::colour));
            d.inputs.add (inPin ("b", "B", PinType::colour));
            d.outputs.add (outPin ("out", "Colour", PinType::colour));
            break;

        case NodeKind::mapRange:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.outputs.add (outPin ("out", "Result", PinType::floatT));
            addP ("inMin", 0.0f);
            addP ("inMax", 1.0f);
            addP ("outMin", 0.0f);
            addP ("outMax", 1.0f);
            break;
        case NodeKind::thresholdGate:
            d.inputs.add (inPin ("x", "X", PinType::floatT));
            d.outputs.add (outPin ("out", "Gate", PinType::floatT));
            d.outputs.add (outPin ("pass", "Pass", PinType::boolT));
            addP ("threshold", 0.25f);
            break;

        case NodeKind::convertToFloat:
            d.inputs.add (inPin ("in", "In", PinType::any));
            d.outputs.add (outPin ("out", "Float", PinType::floatT));
            break;
        case NodeKind::convertToVec3:
            d.inputs.add (inPin ("in", "In", PinType::any));
            d.outputs.add (outPin ("out", "Vector3", PinType::vec3));
            break;
        case NodeKind::convertToColour:
            d.inputs.add (inPin ("in", "In", PinType::any));
            d.outputs.add (outPin ("out", "Colour", PinType::colour));
            break;

        case NodeKind::uniformTime:
        case NodeKind::uniformDelta:
        case NodeKind::uniformAmplitude:
            d.outputs.add (outPin ("out", "Value", PinType::floatT));
            break;

        case NodeKind::filterAttr:
            d.inputs.add (inPin ("in", "Particles", PinType::particles));
            d.outputs.add (outPin ("out", "Particles", PinType::particles));
            // attr id as float param (enum index into builtin list)
            addP ("attr", (float) AttrId::fft);
            addP ("amount", 1.0f);
            addP ("threshold", 0.25f);
            addP ("thresholdOn", 1.0f);
            addP ("curve", 0.0f);
            addP ("mapMin", 0.0f);
            addP ("mapMax", 1.0f);
            addP ("invert", 0.0f);
            addP ("stage", 2.0f);   // 0 spawn, 1 update, 2 both
            addP ("keepMode", 0.0f); // 0 keep if >= thr, 1 keep if <
            break;

        case NodeKind::colourRampAttr:
            d.inputs.add (inPin ("in", "Particles", PinType::particles));
            d.outputs.add (outPin ("out", "Particles", PinType::particles));
            d.inputs.add (inPin ("c0", "Colour A", PinType::colour));
            d.inputs.add (inPin ("c1", "Colour B", PinType::colour));
            addP ("attr", (float) AttrId::fft);
            addP ("curve", 0.0f);
            addP ("mapMin", 0.0f);
            addP ("mapMax", 1.0f);
            addP ("invert", 0.0f);
            addP ("stage", 0.0f); // spawn default
            addP ("c0r", 0.05f); addP ("c0g", 0.05f); addP ("c0b", 0.12f); addP ("c0a", 1.0f);
            addP ("c1r", 1.0f);  addP ("c1g", 0.55f); addP ("c1b", 0.15f); addP ("c1a", 1.0f);
            break;

        case NodeKind::combineForce:
        case NodeKind::combineEmitter:
        case NodeKind::combineFloat:
        case NodeKind::combineVec3:
        case NodeKind::combineParticles:
        {
            const PinType t = kind == NodeKind::combineForce ? PinType::force
                            : (kind == NodeKind::combineEmitter ? PinType::emitter
                            : (kind == NodeKind::combineVec3 ? PinType::vec3
                            : (kind == NodeKind::combineParticles ? PinType::particles
                                                                  : PinType::floatT)));
            static const char* ids[] = { "in0", "in1", "in2", "in3", "in4", "in5" };
            static const char* labs[] = { "In 1", "In 2", "In 3", "In 4", "In 5", "In 6" };
            for (int i = 0; i < 6; ++i)
                d.inputs.add (inPin (ids[i], labs[i], t));
            d.outputs.add (outPin ("out", "Combined", t));
            break;
        }

        default:
            break;
    }
    return d;
}
} // namespace ParticleNodeGraph
