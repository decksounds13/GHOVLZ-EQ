#pragma once

#include "ParticleNodeGraphModel.h"
#include "../ParticleForceModule.h"
#include "../ParticleEmitterTypes.h"
#include <vector>

class Spectrogram3DComponent;

namespace ParticleNodeGraph
{

/** Result of compiling the graph into Spec3D particle parameters. */
struct CompileResult
{
    bool ok = false;
    juce::String message;

    bool particleModeEnabled = true;
    bool forcesEnabled = true;
    float emission = 4000.0f;
    float lifespan = 2.0f;
    float size = 0.02f;
    float spawnJitter = 0.0025f;
    ParticleEmitterType emitterType = ParticleEmitterType::spectrogram;
    bool continuousEmit = true;
    float emitterPosX = 0.0f, emitterPosY = 0.25f, emitterPosZ = 0.0f;
    float initVelX = 0.0f, initVelY = 1.0f, initVelZ = 0.0f;
    std::vector<ParticleForceModule> forces;
};

/** Evaluate graph constants/math (offline) and extract emitter + force stack. */
CompileResult compileGraph (const GraphModel& model, uint32_t forceUidSeed = 1);

/** Apply compile result to live Spec3D component (message thread). */
void applyCompileResult (Spectrogram3DComponent& spec3d, const CompileResult& r);

} // namespace ParticleNodeGraph
