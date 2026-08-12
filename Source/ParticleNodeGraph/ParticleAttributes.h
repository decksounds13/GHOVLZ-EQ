#pragma once

#include "ParticleDataTypes.h"
#include <array>
#include <vector>

/**
    Named particle attributes — the graph's data-oriented surface.
    Built-ins map onto Spec3DParticleSystem::Particle fields today.
    Custom slots reserved for user attrs / fluid coupling later.
*/
namespace ParticleNodeGraph
{

enum class AttrId : uint16_t
{
    // Spatial / motion
    position = 0,   // vec3
    velocity,       // vec3
    speed,          // float |v|

    // Life
    age,            // float seconds
    normalizedAge,  // float 0..1
    life,           // float max life seconds

    // Appearance
    size,           // float
    colour,         // colour (rgb + uses alpha attr for A when split)
    alpha,          // float
    emissive,       // float
    colourGain,     // float

    // Orientation
    orientation,    // vec3 euler rad (until quat lands)

    // Identity / random
    particleId,     // int (hashed to 0..1 when sampled as float)
    random0,        // float
    random1,
    random2,

    // Audio / spectrogram domain
    fft,            // float 0..1  (binDb01)
    binFreq,        // float 0..1
    history,        // float 0..1 column along trail

    // Height / trail
    targetY,        // float world Y bake
    posY,           // float current Y

    // ---- reserved extension band (fluids / custom) ----
    custom0 = 64,
    custom1,
    custom2,
    custom3,
    custom4,
    custom5,
    custom6,
    custom7,

    // Future field sampling hooks (not simulated yet)
    fieldDensity = 128,
    fieldVelocity,
    fieldTemperature,

    count = 256
};

inline DataType attrDataType (AttrId id) noexcept
{
    switch (id)
    {
        case AttrId::position:
        case AttrId::velocity:
        case AttrId::orientation:
        case AttrId::fieldVelocity:
            return DataType::vec3;
        case AttrId::colour:
            return DataType::colour;
        case AttrId::particleId:
            return DataType::intT;
        default:
            return DataType::floatT;
    }
}

inline const char* attrName (AttrId id) noexcept
{
    switch (id)
    {
        case AttrId::position:      return "Position";
        case AttrId::velocity:      return "Velocity";
        case AttrId::speed:         return "Speed";
        case AttrId::age:           return "Age";
        case AttrId::normalizedAge: return "Normalized Age";
        case AttrId::life:          return "Life";
        case AttrId::size:          return "Size";
        case AttrId::colour:        return "Colour";
        case AttrId::alpha:         return "Alpha";
        case AttrId::emissive:      return "Emissive";
        case AttrId::colourGain:    return "Colour Gain";
        case AttrId::orientation:   return "Orientation";
        case AttrId::particleId:    return "Particle Id";
        case AttrId::random0:       return "Random 0";
        case AttrId::random1:       return "Random 1";
        case AttrId::random2:       return "Random 2";
        case AttrId::fft:           return "FFT Strength";
        case AttrId::binFreq:       return "Bin Freq";
        case AttrId::history:       return "History";
        case AttrId::targetY:       return "Target Y";
        case AttrId::posY:          return "Position Y";
        case AttrId::custom0:       return "Custom 0";
        case AttrId::custom1:       return "Custom 1";
        case AttrId::custom2:       return "Custom 2";
        case AttrId::custom3:       return "Custom 3";
        case AttrId::custom4:       return "Custom 4";
        case AttrId::custom5:       return "Custom 5";
        case AttrId::custom6:       return "Custom 6";
        case AttrId::custom7:       return "Custom 7";
        case AttrId::fieldDensity:  return "Field Density";
        case AttrId::fieldVelocity: return "Field Velocity";
        case AttrId::fieldTemperature: return "Field Temperature";
        default:                    return "Attribute";
    }
}

inline juce::String attrMenuLabel (AttrId id)
{
    return juce::String (attrName (id)) + " (" + dataTypeName (attrDataType (id)) + ")";
}

/** Attributes present on spectrogram-born particles. */
inline bool attrAvailableOnSpectrogram (AttrId id) noexcept
{
    switch (id)
    {
        case AttrId::fieldDensity:
        case AttrId::fieldVelocity:
        case AttrId::fieldTemperature:
            return false; // not yet
        case AttrId::custom0: case AttrId::custom1: case AttrId::custom2: case AttrId::custom3:
        case AttrId::custom4: case AttrId::custom5: case AttrId::custom6: case AttrId::custom7:
            return false; // until custom layout ships
        default:
            return true;
    }
}

/** Point / geometric emitters: no mesh FFT/history. */
inline bool attrAvailableOnPoint (AttrId id) noexcept
{
    switch (id)
    {
        case AttrId::fft:
        case AttrId::binFreq:
        case AttrId::history:
        case AttrId::targetY:
            return false;
        default:
            return attrAvailableOnSpectrogram (id);
    }
}

/** Built-in attributes offered in filter/sample dropdowns (stable order). */
inline const AttrId* builtinAttrList (int& outCount) noexcept
{
    static const AttrId kList[] = {
        AttrId::fft,
        AttrId::binFreq,
        AttrId::history,
        AttrId::normalizedAge,
        AttrId::age,
        AttrId::life,
        AttrId::speed,
        AttrId::size,
        AttrId::alpha,
        AttrId::emissive,
        AttrId::colourGain,
        AttrId::posY,
        AttrId::targetY,
        AttrId::particleId,
        AttrId::random0,
        AttrId::random1,
        AttrId::random2,
        AttrId::position,
        AttrId::velocity,
        AttrId::colour,
        AttrId::orientation,
    };
    outCount = (int) (sizeof (kList) / sizeof (kList[0]));
    return kList;
}

/**
    Attribute filter (spawn/update cull).
    Pipeline: sample attr → normalize to 0..1 when needed → curve → map → invert → threshold gate.
    amount = probability of cull when failed (1 = hard filter).
*/
struct AttrFilter
{
    bool enabled = true;
    AttrId attr = AttrId::fft;
    float amount = 1.0f;
    float curveShape = 0.0f;
    float mapMin = 0.0f;
    float mapMax = 1.0f;
    bool invert = false;
    bool thresholdEnabled = true;
    float threshold = 0.25f;
    /** 0 = spawn, 1 = update, 2 = both */
    int stage = 2;
    /** 0 = keep if weight >= thr (after map), 1 = keep if below */
    int keepMode = 0;
};

/**
    Colour from attribute via 2-stop ramp (spawn/update).
    Multi-stop ramp assets can replace c0/c1 later without changing consumers.
*/
struct ColourRampOp
{
    bool enabled = true;
    AttrId sourceAttr = AttrId::fft;
    float c0[4] = { 0.05f, 0.05f, 0.12f, 1.0f };
    float c1[4] = { 1.0f, 0.55f, 0.15f, 1.0f };
    float curveShape = 0.0f;
    float mapMin = 0.0f;
    float mapMax = 1.0f;
    bool invert = false;
    /** 0 = spawn, 1 = update, 2 = both */
    int stage = 0;
};

/**
    Lightweight program bag produced by the graph compiler.
    Expand with full Op IR later; forces/emitters stay parallel for now.
    fieldOps reserved for fluid/field step.
*/
struct GraphProgram
{
    std::vector<AttrFilter>   filters;
    std::vector<ColourRampOp> colourRamps;
    // Future: std::vector<GraphOp> spawnOps, updateOps, fieldOps;
};

} // namespace ParticleNodeGraph
