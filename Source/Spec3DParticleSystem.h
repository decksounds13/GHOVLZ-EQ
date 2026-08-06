#pragma once

#include <JuceHeader.h>
#include <vector>
#include <cstdint>
#include <array>
#include <cmath>

class Spectrogram3DComponent;

// ── Particle mod matrix (Niagara-inspired, fixed slots) ─────────────────────

enum class ParticleModSource : int
{
    none = 0,
    amplitude,   // playhead energy 0..1 (smoothed)
    binDb,       // this particle's column/bin height 0..1
    binFreq,     // frequency axis 0..1 for particle bin
    ageNorm,     // age/maxLife (0 if indefinite)
    history,     // column age 0=old .. 1=now
    constant     // row constant slider
};

enum class ParticleModDest : int
{
    emission = 0,  // emitter rate (emitter stage)
    riseSpeed,     // spawn
    lifespan,      // spawn
    size,          // spawn + update
    colourGain,    // spawn + update (multiply RGB)
    colourHue,     // spawn + update (set hue from source)
    emissive,      // spawn + update (brightness scale)
    alpha          // spawn + update
};

enum class ParticleModOp : int
{
    set = 0,       // lerp(base, src, amount)
    multiply,      // base * lerp(1, src, amount)
    add            // base + src * amount
};

struct ParticleModSlot
{
    bool enabled = false;
    ParticleModSource source = ParticleModSource::none;
    ParticleModDest dest = ParticleModDest::emission;
    ParticleModOp op = ParticleModOp::multiply;
    float amount = 1.0f;
    float constant = 0.5f;
    /** Serum-style response curve: 0 = linear 1:1, + = ease-in (x^k), − = ease-out. Range −1..1. */
    float curveShape = 0.0f;
    /** When true: envelope-follow source, soft-gate above threshold (attack/release). */
    bool thresholdEnabled = false;
    float threshold = 0.25f;   // 0..1
    float attackMs = 10.0f;    // 0.1..2000
    float releaseMs = 80.0f;   // 0.1..5000
};

static constexpr int kParticleModSlotCount = 8;

/** Map 0..1 through bipolar curve shape (−1..1). 0 = identity. */
inline float particleModApplyCurve (float x, float shape) noexcept
{
    x = juce::jlimit (0.0f, 1.0f, x);
    shape = juce::jlimit (-1.0f, 1.0f, shape);
    if (std::abs (shape) < 1.0e-4f)
        return x;
    // shape +1 → power 4 (slow then steep), shape −1 → power 0.25 (steep then flat)
    const float power = std::pow (4.0f, shape);
    return std::pow (juce::jmax (0.0f, x), power);
}

/**
    Modular Spec3D particle spectrogram (CPU sim + GPU billboards).
    Lazy GL init. Continuous emit = uniform random bins. Slice = phase-locked columns.
    Mod matrix: source → dest bindings applied at emitter / spawn / update stages.
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
    void draw (const juce::Matrix3D<float>& projection,
               const juce::Matrix3D<float>& view,
               juce::Vector3D<float> camRight,
               juce::Vector3D<float> camUp);

    bool hasGlResources() const noexcept { return glReady; }

    static constexpr int kHardCap = 24576;

private:
    struct Particle
    {
        float x = 0, y = 0, z = 0;
        float velY = 0;
        float targetY = 0;
        float r = 1, g = 1, b = 1;
        float baseR = 1, baseG = 1, baseB = 1; // ramp colour before mod
        float age = 0;
        float maxLife = -1.0f; // < 0 = indefinite
        float sizeScale = 1.0f;
        float colourGain = 1.0f;
        float emissiveScale = 1.0f;
        float alpha = 1.0f;
        float binDb01 = 0.0f;
        float binFreq01 = 0.0f;
        int bin = 0;
        int col = 0;
        bool alive = false;
        bool settled = false;
    };

    struct GpuVert
    {
        float px, py, pz;
        float r, g, b, a;
        float cx, cy;
        float size;
    };

    struct FrameSources
    {
        float amplitude = 0.0f;
    };

    Spectrogram3DComponent& owner;

    std::vector<Particle> pool;
    std::vector<float> emitAccum;
    float emitGlobal = 0.0f;
    float amplitudeSmooth = 0.0f;
    int nextWrite = 0;
    juce::Random rng { 0x53c33dU };
    /** Per-slot envelope followers (only advanced when that slot's threshold is on). */
    std::array<float, kParticleModSlotCount> slotEnv {};
    /** Cached processed global sources (amplitude/constant) after threshold+curve for this frame. */
    std::array<float, kParticleModSlotCount> slotGlobalSrc {};
    std::array<bool, kParticleModSlotCount> slotGlobalValid {};

    void ensurePool();
    void spawnAtBin (int bin, float riseSpeedBase, float lifespanBase, float sizeBase);
    float sampleColumn (int bin, int col,
                        float& outR, float& outG, float& outB, float& outZ,
                        float& outDb01, float& outFreq01) const;
    float columnToWorldX (int col) const noexcept;
    int allocSlot();
    float playheadAmplitude01() const;
    float sampleSource (ParticleModSource src, float constant,
                        const Particle* p, const FrameSources& frame) const;
    static bool isGlobalSource (ParticleModSource src) noexcept;
    /** Advance envelopes once/frame for global thresholded slots. */
    void tickGlobalThresholds (const FrameSources& frame, float dt) noexcept;
    /** Sample → optional threshold → curve. Global thresholded slots use frame cache. */
    float processSource (int slotIndex, const Particle* p, const FrameSources& frame) const noexcept;
    static float applyOp (float base, float src, ParticleModOp op, float amount) noexcept;
    void applyEmitterMods (float& emission, const FrameSources& frame) noexcept;
    void applySpawnMods (Particle& p, float& riseSpeed, float& lifespan, float& sizeScale,
                         const FrameSources& frame) noexcept;
    void applyUpdateMods (Particle& p, const FrameSources& frame) noexcept;
    void applyColourMods (Particle& p, ParticleModDest dest, float src, ParticleModOp op, float amount) const;
    static void setHue (float& r, float& g, float& b, float hue01) noexcept;

    bool glReady = false;
    std::unique_ptr<juce::OpenGLShaderProgram> program;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aPos;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aCol;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aCorner;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> aSize;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uProj;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uView;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uCamRight;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uCamUp;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uSize;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uEmissiveMode;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uEmissiveStrength;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uRoughness;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uMetalness;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uSpecular;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uLightingAmount;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uLightDirView;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> uLightColour;

    unsigned int vbo = 0;
    std::vector<GpuVert> gpuVerts;

    bool createProgram (juce::OpenGLContext& context);
};
