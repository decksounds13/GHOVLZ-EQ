#pragma once

#include <JuceHeader.h>

/**
    Particle emitter geometry source.
    spectrogram (default) = existing playhead / history mesh spawn.
    Geometric types sample shape + spray under free visualizer binding.
*/
enum class ParticleEmitterType : int
{
    spectrogram = 0,
    point = 1,
    // Reserved (later phases):
    sphere = 2,
    box = 3,
    disc = 4,
    cone = 5
};

/** Surface vs filled volume (ignored for point). */
enum class ParticleEmitDomain : int
{
    surface = 0,
    volume = 1
};

/**
    One birth candidate from any emitter.
    Spectrogram trail: y≈0 + targetY bake, col for scroll.
    Geometric / free: world position + velocity already filled.
*/
struct ParticleSpawnSample
{
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float velX = 0.0f, velY = 0.0f, velZ = 0.0f;
    /** Trail rise cap (spectrogram). Free: usually equals y. */
    float targetY = 0.01f;
    float r = 1.0f, g = 1.0f, b = 1.0f;
    float binDb01 = 0.0f;
    float binFreq01 = 0.0f;
    float binF = 0.0f;
    int col = 0;
    int bin = 0;
    /** When true: trail binding (column scroll, rise from ground). */
    bool trailBound = true;
};
