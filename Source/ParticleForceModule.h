#pragma once

#include <JuceHeader.h>
#include <array>
#include <cstdint>

/** Ordered force modules (Niagara-style stack). Evaluated top→bottom each sim tick. */
enum class ParticleForceType : int
{
    gravity = 0,
    drag,
    wind,
    curlNoise,
    turbulence,
    rotation, // angular rate on mesh (rad/s); more force → faster spin
    count
};

inline const char* particleForceTypeName (ParticleForceType t) noexcept
{
    switch (t)
    {
        case ParticleForceType::gravity:    return "Gravity";
        case ParticleForceType::drag:       return "Drag";
        case ParticleForceType::wind:       return "Wind";
        case ParticleForceType::curlNoise:  return "Curl noise";
        case ParticleForceType::turbulence: return "Turbulence";
        case ParticleForceType::rotation:   return "Rotation";
        default:                            return "Force";
    }
}

struct ParticleForceModule
{
    ParticleForceType type = ParticleForceType::gravity;
    bool enabled = true;
    /** Param pack — meaning depends on type:
        gravity:    p[0] = accel Y (neg = down; +Y up so -g for down)
        drag:       p[0] = drag coeff
        wind:       p[0..2] = accel XYZ
        curlNoise:  p[0]=strength, p[1]=scale, p[2]=speed
        turbulence: p[0]=strength
        rotation:   p[0..2] = angular rate XYZ (rad/s). If linkAxes, p[0] drives all enabled axes.
    */
    float p[4] = { 0, 0, 0, 0 };
    uint32_t uid = 0; // stable id for UI / prefs

    // ── Rotation-force axis controls (ignored by other types) ──────────────
    /** Apply spin on this axis. */
    bool axisX = true, axisY = true, axisZ = true;
    /** When true, p[0] is used for every enabled axis (same force on all). */
    bool linkAxes = true;
    /** When true, each particle keeps a random ± sign per axis (tumble). */
    bool randomDir = true;
};

static constexpr int kParticleForceStackMax = 16;

enum class ParticleMeshShape : int
{
    sphere = 0,
    cube = 1,
    billboard = 2 // legacy soft sprite
};

inline ParticleForceModule makeDefaultForceModule (ParticleForceType t, uint32_t uid) noexcept
{
    ParticleForceModule m;
    m.type = t;
    m.enabled = true;
    m.uid = uid;
    switch (t)
    {
        case ParticleForceType::gravity:    m.p[0] = -2.0f; break;
        case ParticleForceType::drag:       m.p[0] = 1.0f; break;
        case ParticleForceType::wind:       m.p[0] = 0.5f; break;
        case ParticleForceType::curlNoise:  m.p[0] = 1.0f; m.p[1] = 2.0f; m.p[2] = 0.4f; break;
        case ParticleForceType::turbulence: m.p[0] = 0.5f; break;
        case ParticleForceType::rotation:
            m.p[0] = 2.0f; // rad/s — moderate tumble
            m.p[1] = 2.0f;
            m.p[2] = 2.0f;
            m.axisX = m.axisY = m.axisZ = true;
            m.linkAxes = true;
            m.randomDir = true;
            break;
        default: break;
    }
    return m;
}
