#pragma once

#include <JuceHeader.h>
#include "ParticleForceModule.h"
#include <vector>
#include <cstdint>
#include <array>
#include <cmath>

class Spectrogram3DComponent;

// ── Particle mod matrix ─────────────────────────────────────────────────────

enum class ParticleModSource : int
{
    none = 0,
    amplitude,
    binDb,
    binFreq,
    ageNorm,
    history,
    constant,
    random1,
    random2,
    random3,
    /** Birth velocity (Float = speed magnitude; Vec3 dests sample XYZ). */
    initVel,
    /** Stable per-particle id hashed to 0–1 (unique for each birth). */
    particleId
};

enum class ParticleModDest : int
{
    emission = 0,
    riseSpeed,         // Float — modulates upward (Y) speed at spawn
    lifespan,
    size,
    colourGain,
    colourHue,
    emissive,
    alpha,
    spawnJitter,       // scale spawn position jitter
    sizeScale,         // alias clarity — same as size (kept for UI grouping)
    initRot,           // Vec3 spawn euler (radians) — one slot for all axes
    // Legacy single-axis ids (prefs remap → initRot). Keep values stable for older XML.
    initRotY_legacy = 11,
    initRotZ_legacy = 12,
    // System-stage force destinations scale matching modules in the stack
    forceGravity = 13,
    forceDrag,
    forceWindX,
    forceWindY,
    forceWindZ,
    forceCurlStrength,
    forceCurlScale,
    forceCurlSpeed,
    forceTurbulence,
    /** Vec3 birth linear velocity (world units/s). Float sources broadcast to XYZ. */
    initVel = 22
};

enum class ParticleModOp : int
{
    set = 0,
    multiply,
    add
};

/** Channel count of a value (Float / Vec2 / Vec3). Used for dest labels and random matching. */
enum class ParticleValueType : int
{
    Float = 0,
    Vec2 = 1,
    Vec3 = 2
};

inline ParticleValueType particleModDestType (ParticleModDest d) noexcept
{
    switch (d)
    {
        case ParticleModDest::initRot:
        case ParticleModDest::initRotY_legacy:
        case ParticleModDest::initRotZ_legacy:
        case ParticleModDest::initVel:
            return ParticleValueType::Vec3;
        default:
            return ParticleValueType::Float;
    }
}

inline const char* particleValueTypeLabel (ParticleValueType t) noexcept
{
    switch (t)
    {
        case ParticleValueType::Vec2: return "Vec2";
        case ParticleValueType::Vec3: return "Vec3";
        default:                      return "Float";
    }
}

/** Preferred channel type when using a source (for UI matching / Random dim hints). */
inline ParticleValueType particleModSourceType (ParticleModSource s) noexcept
{
    switch (s)
    {
        case ParticleModSource::initVel:
            return ParticleValueType::Vec3; // XYZ available; Float dests use speed
        case ParticleModSource::particleId:
        default:
            return ParticleValueType::Float;
    }
}

inline const char* particleModSourceBaseName (ParticleModSource s) noexcept
{
    switch (s)
    {
        case ParticleModSource::none:      return "None";
        case ParticleModSource::amplitude: return "Amplitude";
        case ParticleModSource::binDb:     return "Bin dB";
        case ParticleModSource::binFreq:   return "Bin freq";
        case ParticleModSource::ageNorm:   return "Age";
        case ParticleModSource::history:   return "History";
        case ParticleModSource::constant:  return "Constant";
        case ParticleModSource::random1:   return "Random 1";
        case ParticleModSource::random2:   return "Random 2";
        case ParticleModSource::random3:   return "Random 3";
        case ParticleModSource::initVel:   return "Init vel";
        case ParticleModSource::particleId: return "Particle id";
        default:                           return "Source";
    }
}

inline juce::String particleModSourceMenuLabel (ParticleModSource s)
{
    if (s == ParticleModSource::none)
        return "None";
    return juce::String (particleModSourceBaseName (s))
           + " (" + particleValueTypeLabel (particleModSourceType (s)) + ")";
}

inline const char* particleModDestBaseName (ParticleModDest d) noexcept
{
    switch (d)
    {
        case ParticleModDest::emission:         return "Emission";
        case ParticleModDest::riseSpeed:        return "Init vel Y"; // legacy float dest
        case ParticleModDest::lifespan:         return "Lifespan";
        case ParticleModDest::size:             return "Size";
        case ParticleModDest::colourGain:       return "Colour gain";
        case ParticleModDest::colourHue:        return "Colour hue";
        case ParticleModDest::emissive:         return "Emissive";
        case ParticleModDest::alpha:            return "Alpha";
        case ParticleModDest::spawnJitter:      return "Spawn jitter";
        case ParticleModDest::sizeScale:        return "Size scale";
        case ParticleModDest::initRot:
        case ParticleModDest::initRotY_legacy:
        case ParticleModDest::initRotZ_legacy:  return "Init rot";
        case ParticleModDest::forceGravity:     return "Force gravity";
        case ParticleModDest::forceDrag:        return "Force drag";
        case ParticleModDest::forceWindX:       return "Force wind X";
        case ParticleModDest::forceWindY:       return "Force wind Y";
        case ParticleModDest::forceWindZ:       return "Force wind Z";
        case ParticleModDest::forceCurlStrength:return "Force curl str";
        case ParticleModDest::forceCurlScale:   return "Force curl scale";
        case ParticleModDest::forceCurlSpeed:   return "Force curl speed";
        case ParticleModDest::forceTurbulence:  return "Force turbulence";
        case ParticleModDest::initVel:          return "Init vel";
        default:                                return "Dest";
    }
}

/** Menu label with type suffix, e.g. "Init rot (Vec3)". */
inline juce::String particleModDestMenuLabel (ParticleModDest d)
{
    return juce::String (particleModDestBaseName (d))
           + " (" + particleValueTypeLabel (particleModDestType (d)) + ")";
}

/** Normalize legacy single-axis init rot dests to initRot. */
inline ParticleModDest particleModDestCanonical (ParticleModDest d) noexcept
{
    if (d == ParticleModDest::initRotY_legacy || d == ParticleModDest::initRotZ_legacy)
        return ParticleModDest::initRot;
    return d;
}

struct ParticleModSlot
{
    bool enabled = false;
    ParticleModSource source = ParticleModSource::none;
    ParticleModDest dest = ParticleModDest::emission;
    ParticleModOp op = ParticleModOp::multiply;
    float amount = 1.0f;
    float constant = 0.5f;
    float curveShape = 0.0f;   // −1..1 transfer curve
    float mapMin = 0.0f;       // map-range low
    float mapMax = 1.0f;       // map-range high
    /** After curve + map range: use (1 − s) so high source drives low dest. */
    bool invert = false;
    bool thresholdEnabled = false;
    float threshold = 0.25f;
    float attackMs = 10.0f;
    float releaseMs = 80.0f;
};

static constexpr int kParticleModSlotCount = 8;
static constexpr int kParticleRandomSourceCount = 3;

enum class ParticleRandomDim : int { Float = 0, Vec2 = 1, Vec3 = 2 };
enum class ParticleRandomMode : int { PerParticle = 0, PerFrame = 1, Smoothed = 2 };

inline ParticleValueType particleRandomDimType (ParticleRandomDim d) noexcept
{
    switch (d)
    {
        case ParticleRandomDim::Vec2: return ParticleValueType::Vec2;
        case ParticleRandomDim::Vec3: return ParticleValueType::Vec3;
        default:                      return ParticleValueType::Float;
    }
}

inline const char* particleRandomDimMenuLabel (ParticleRandomDim d) noexcept
{
    switch (d)
    {
        case ParticleRandomDim::Vec2: return "Vec2 (2 channels)";
        case ParticleRandomDim::Vec3: return "Vec3 (3 channels)";
        default:                      return "Float (1 channel)";
    }
}

struct ParticleRandomSource
{
    bool active = false; // shown when any matrix row uses it
    ParticleRandomDim dim = ParticleRandomDim::Float;
    ParticleRandomMode mode = ParticleRandomMode::PerParticle;
    float minV = 0.0f;
    float maxV = 1.0f;
    float smoothMs = 50.0f;
};

enum class ParticleBindingMode : int
{
    spectrogramTrail = 0, // waterfall lock / rise-to-height
    freeVisualizer = 1    // free motion; spectrum as sources only
};

/** Map 0..1 through bipolar curve shape (−1..1). 0 = identity. */
inline float particleModApplyCurve (float x, float shape) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    shape = juce::jlimit (-1.0f, 1.0f, shape);
    if (std::abs (shape) < 1.0e-4f)
        return x;
    const float power = std::pow (4.0f, shape);
    return std::pow (juce::jmax (0.0f, x), power);
}

/**
    Modular Spec3D particle system.
    CPU sim + GPU path: instanced low-poly mesh (sphere/cube) or billboard sprites.
    Force stack (ordered modules); mod matrix; trail / free visualizer binding.
*/
class Spec3DParticleSystem
{
public:
    explicit Spec3DParticleSystem (Spectrogram3DComponent& owner);
    ~Spec3DParticleSystem();

    Spec3DParticleSystem (const Spec3DParticleSystem&) = delete;
    Spec3DParticleSystem& operator= (const Spec3DParticleSystem&) = delete;

    void update (float dtSeconds);
    void scrollHistory (int numCols);
    void clear();

    void ensureGl (juce::OpenGLContext& context);
    void releaseGl();
    /**
        Run deferred force/age integrate on the OpenGL thread.
        When GPU sim is preferred and compute is ready, dispatches the compute path;
        otherwise falls back to CPU integrate. Safe no-op if nothing is pending.
    */
    void integrateOnGlThread();
    void draw (const juce::Matrix3D<float>& projection,
               const juce::Matrix3D<float>& view,
               juce::Vector3D<float> camRight,
               juce::Vector3D<float> camUp);

    bool hasGlResources() const noexcept { return glReady; }
    /** True if compute integrate program is ready (GL 4.3 / ARB_compute_shader). */
    bool isGpuSimAvailable() const noexcept { return gpuComputeReady; }

    /**
        Absolute pool ceiling (1M for stress tests). ~200B/particle → ~200MB at 1M.
        Raise Max particles in settings to exercise 100k+; default budget stays modest.
    */
    static constexpr int kHardCap = 1'048'576;
    /** Default live budget (everyday safe). Not the hard ceiling. */
    static constexpr int kDefaultMaxAlive = 8192;
    static constexpr int kMinMaxAlive = 256;
    /** Hybrid GPU integrate+full-readback is counterproductive above this. */
    static constexpr int kGpuReadbackMaxAlive = 24576;

    int getAliveCount() const noexcept { return aliveCount; }
    int getPoolCapacity() const noexcept { return (int) pool.size(); }
    int getMaxAliveBudget() const noexcept { return maxAliveBudget; }
    void setMaxAliveBudget (int maxAlive) noexcept;
    int getLastSpawnedCount() const noexcept { return lastSpawnedCount; }
    int getLastCulledCount() const noexcept { return lastCulledCount; }
    /** True when at budget — spawn is blocked / throttled. */
    bool isAtParticleBudget() const noexcept { return aliveCount >= maxAliveBudget; }

private:
    struct Particle
    {
        float x = 0, y = 0, z = 0;
        float velX = 0, velY = 0, velZ = 0;
        float targetY = 0;
        float r = 1, g = 1, b = 1;
        float baseR = 1, baseG = 1, baseB = 1;
        float age = 0;
        float maxLife = -1.0f;
        float sizeScale = 1.0f;
        float colourGain = 1.0f;
        float emissiveScale = 1.0f;
        float alpha = 1.0f;
        float rotX = 0, rotY = 0, rotZ = 0; // euler radians
        /**
            Per-particle spin multipliers in −1…1 (set once at spawn).
            Rotation force rate is multiplied by these so every particle tumbles
            differently (direction + relative speed), not one shared random.
        */
        float spinScaleX = 1.0f, spinScaleY = 1.0f, spinScaleZ = 1.0f;
        float binDb01 = 0.0f;
        float binFreq01 = 0.0f;
        /** Unique birth id (matrix source Particle id → hashed 0–1). */
        uint32_t id = 0;
        /** Continuous mesh-row coordinate (0..meshH-1); not only integer bin centers. */
        float binF = 0.0f;
        /** Spawn scatter retained under trail lock (exact grid is optional via jitter=0). */
        float spawnOffX = 0, spawnOffY = 0, spawnOffZ = 0;
        /** Per random source: up to 3 channels (Float / Vec2 / Vec3). [source][xyz]. */
        float randV[kParticleRandomSourceCount][3] = {};
        int bin = 0;
        int col = 0;
        bool alive = false;
        bool settled = false;
    };

    /** Per-instance GPU payload for mesh instancing. */
    struct GpuInstance
    {
        float px, py, pz;
        float sx; // uniform scale
        float qx, qy, qz, qw; // rotation quaternion
        float r, g, b, a;     // albedo + alpha
        float em;             // per-particle emissive scale (matrix dest)
    };

    /** Billboard fallback vertex. */
    struct GpuBillboardVert
    {
        float px, py, pz;
        float r, g, b, a;
        float cx, cy;
        float size;
        float em; // per-particle emissive scale (matrix dest)
    };

    struct MeshVert
    {
        float px, py, pz;
        float nx, ny, nz;
    };

    struct FrameSources
    {
        float amplitude = 0.0f;
        /** Per random source channels (matches ParticleRandomDim). [source][xyz]. */
        float randomV[kParticleRandomSourceCount][3] = {};
    };

    /** Runtime force params after matrix (type-keyed scales applied to stack). */
    struct ForceModScales
    {
        float gravity = 1.0f;
        float drag = 1.0f;
        float windX = 1.0f, windY = 1.0f, windZ = 1.0f;
        float curlStrength = 1.0f, curlScale = 1.0f, curlSpeed = 1.0f;
        float turbulence = 1.0f;
        // Additive offsets from Set/Add ops accumulated separately
        float gravityAdd = 0, dragAdd = 0;
        float windAddX = 0, windAddY = 0, windAddZ = 0;
        float curlStrAdd = 0, curlScaleAdd = 0, curlSpeedAdd = 0;
        float turbAdd = 0;
    };

    Spectrogram3DComponent& owner;

    std::vector<Particle> pool;
    /** O(1) spawn alloc — required at 100k+ (linear free-slot scan is unusable). */
    std::vector<int> freeList;
    std::vector<float> emitAccum;
    float emitGlobal = 0.0f;
    float amplitudeSmooth = 0.0f;
    float simTime = 0.0f;
    int nextWrite = 0;
    int maxAliveBudget = kDefaultMaxAlive;
    int aliveCount = 0;
    int lastSpawnedCount = 0;
    int lastCulledCount = 0;
    /** Round-robin for throttled colour updates under load. */
    int colourPassCursor = 0;
    juce::Random rng { 0x53c33dU };
    /** Monotonic birth counter for ParticleModSource::particleId. */
    uint32_t nextParticleId = 1;
    std::array<float, kParticleModSlotCount> slotEnv {};
    std::array<float, kParticleModSlotCount> slotGlobalSrc {};
    std::array<bool, kParticleModSlotCount> slotGlobalValid {};
    /** Smoothed / per-frame random channels [source][xyz]. */
    float randomSmoothV[kParticleRandomSourceCount][3] = {};

    void ensurePool();
    void recountAlive() noexcept;
    void rebuildFreeList() noexcept;
    void markDead (int index) noexcept;
    /** Kill excess alive particles (settled / oldest first) down to maxAliveBudget. */
    void enforceAliveBudget() noexcept;
    /** Spawn on the playhead. binF is continuous in [0, meshH-1] (interpolated). */
    void spawnAtPlayhead (float binF, float lifespanBase, float sizeBase, float spawnJitterScale);
    /** Energy-weighted random binF on the playhead (louder bands emit more). */
    float pickPlayheadBinF() noexcept;
    float sampleColumn (int bin, int col,
                        float& outR, float& outG, float& outB, float& outZ,
                        float& outDb01, float& outFreq01) const;
    float columnToWorldX (int col) const noexcept;
    int allocSlot();
    float playheadAmplitude01() const;
    float sampleSource (ParticleModSource src, float constant,
                        const Particle* p, const FrameSources& frame) const;
    /** Multi-channel sample; Float sources broadcast to all axes. */
    juce::Vector3D<float> sampleSourceVec (ParticleModSource src, float constant,
                                           const Particle* p, const FrameSources& frame) const;
    void rollRandomChannels (int sourceIndex, float out[3]) noexcept;
    static bool isGlobalSource (ParticleModSource src) noexcept;
    static int randomIndex (ParticleModSource src) noexcept;
    void tickRandomSources (float dt) noexcept;
    void tickGlobalThresholds (const FrameSources& frame, float dt) noexcept;
    float processSource (int slotIndex, const Particle* p, const FrameSources& frame) const noexcept;
    juce::Vector3D<float> processSourceVec (int slotIndex, const Particle* p,
                                            const FrameSources& frame) const noexcept;
    static float applyOp (float base, float src, ParticleModOp op, float amount) noexcept;
    void applySystemMods (float& emission, float& spawnJitter, ForceModScales& scales,
                          const FrameSources& frame) noexcept;
    void applySpawnMods (Particle& p, float& lifespan, float& sizeScale, float& jitterScale,
                         float& velX, float& velY, float& velZ,
                         float& rotX, float& rotY, float& rotZ,
                         const FrameSources& frame) noexcept;
    void applyUpdateMods (Particle& p, const FrameSources& frame) noexcept;
    void applyColourMods (Particle& p, ParticleModDest dest, float src, ParticleModOp op, float amount) const;
    static void setHue (float& r, float& g, float& b, float hue01) noexcept;
    void integrateForceStack (Particle& p, const ForceModScales& scales, float dt) noexcept;
    /** CPU age + force integrate for the whole pool (message or GL thread). */
    void cpuIntegrateAll (float dt, const ForceModScales& scales, bool freeMode) noexcept;
    static juce::Vector3D<float> curlNoise (float x, float y, float z) noexcept;
    static float hashNoise (int x, int y, int z) noexcept;
    static float valueNoise (float x, float y, float z) noexcept;
    static void eulerToQuat (float rx, float ry, float rz,
                             float& qx, float& qy, float& qz, float& qw) noexcept;
    void buildUnitMeshes();
    bool createMeshProgram (juce::OpenGLContext& context);
    bool createBillboardProgram (juce::OpenGLContext& context);
    bool createComputeProgram();
    void releaseComputeProgram();
    /** Pack pool → SSBO, dispatch integrate, readback pos/vel/age/flags. Must be GL thread. */
    void gpuIntegrateAndReadback (float dt, const ForceModScales& scales, bool freeMode);
    void drawInstancedMeshes (const juce::Matrix3D<float>& projection,
                              const juce::Matrix3D<float>& view);
    void drawBillboards (const juce::Matrix3D<float>& projection,
                         const juce::Matrix3D<float>& view,
                         juce::Vector3D<float> camRight,
                         juce::Vector3D<float> camUp);

    /** GPU sim particle (std430, 5×vec4 = 80 bytes). */
    struct alignas (16) GpuSimParticle
    {
        float px, py, pz, age;
        float vx, vy, vz, maxLife;
        float spinX, spinY, spinZ, flags; // flags bits: 1 alive, 2 settled, 4 free, 8 lockX
        float targetY, spawnOffY, rotX, rotY;
        float rotZ, pad0, pad1, pad2;
    };

    /** Force module for compute (scales baked on CPU). */
    struct alignas (16) GpuForceMod
    {
        int type = 0;
        int enabled = 0;
        int axisMask = 7; // bit0 X, bit1 Y, bit2 Z
        int flags = 0;    // bit0 linkAxes, bit1 randomDir
        float p0 = 0, p1 = 0, p2 = 0, p3 = 0;
    };

    bool glReady = false;
    bool gpuComputeReady = false;
    unsigned int computeProgram = 0;
    unsigned int particleSsbo = 0;
    unsigned int forceSsbo = 0;
    int computeUCount = -1, computeUForceCount = -1, computeUDt = -1, computeUSimTime = -1;
    std::vector<GpuSimParticle> gpuSimPack;
    std::vector<GpuForceMod> gpuForcePack;
    /** Deferred integrate (GPU must run on GL thread; update() may be message-thread). */
    float pendingIntegrateDt = 0.0f;
    ForceModScales pendingForceScales {};
    bool pendingFreeMode = false;
    // Shared lighting uniforms used by both programs
    std::unique_ptr<juce::OpenGLShaderProgram> meshProgram;
    std::unique_ptr<juce::OpenGLShaderProgram> billboardProgram;
    // Mesh mesh attrs
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aMeshPos, aMeshNrm;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aInstPos, aInstScale, aInstQuat, aInstCol, aInstEm;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uMeshProj, uMeshView;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uMeshEmissiveMode, uMeshEmissiveStr;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uMeshRough, uMeshMetal, uMeshSpec;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uMeshLightAmt, uMeshLightDir, uMeshLightCol;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uMeshEnergyConserve;
    // Billboard attrs (legacy path)
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aPos, aCol, aCorner, aSize, aEm;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uProj, uView, uCamRight, uCamUp, uSize;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uEmissiveMode, uEmissiveStrength;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uRoughness, uMetalness, uSpecular;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uEnergyConserve;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uLightingAmount, uLightDirView, uLightColour;

    unsigned int meshVbo = 0, meshIbo = 0, instanceVbo = 0;
    unsigned int billboardVbo = 0;
    int sphereIndexCount = 0, cubeIndexCount = 0;
    int sphereVertexOffset = 0, cubeVertexOffset = 0;
    int sphereIndexOffset = 0, cubeIndexOffset = 0;
    std::vector<MeshVert> meshVerts;
    std::vector<uint16_t> meshIndices;
    std::vector<GpuInstance> instances;
    std::vector<GpuBillboardVert> gpuVerts;
};
