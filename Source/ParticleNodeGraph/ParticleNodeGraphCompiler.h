#pragma once

#include "ParticleNodeGraphModel.h"
#include "ParticleAttributes.h"
#include "../ParticleForceModule.h"
#include "../ParticleEmitterTypes.h"
#include <vector>

class Spectrogram3DComponent;

namespace ParticleNodeGraph
{

/** Log line for the graph output console. */
enum class LogLevel : uint8_t { info = 0, success, warning, error, debug };

struct CompileLogLine
{
    LogLevel level = LogLevel::info;
    juce::String text;
};

/** Result of compiling the graph into Spec3D particle parameters + attribute program. */
struct CompileResult
{
    bool ok = false;
    juce::String message;
    std::vector<CompileLogLine> log;

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

    /** Attribute program (filters, colour ramps). */
    GraphProgram program;

    void addLog (LogLevel level, const juce::String& text)
    {
        log.push_back ({ level, text });
        if (level == LogLevel::error && message.isEmpty())
            message = text;
    }
};

/** Evaluate graph constants/math (offline) and extract emitter + force stack + attr program. */
CompileResult compileGraph (const GraphModel& model, uint32_t forceUidSeed = 1);

/** Apply compile result to live Spec3D component (message thread). Seamless hot-reload. */
void applyCompileResult (Spectrogram3DComponent& spec3d, const CompileResult& r);

} // namespace ParticleNodeGraph
