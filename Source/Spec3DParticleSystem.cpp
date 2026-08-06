#include "Spec3DParticleSystem.h"
#include "Spectrogram3DComponent.h"
#include "SpectrogramComponent.h"
#include <cmath>

namespace
{
    constexpr const char* kParticleVS = R"(
        #version 150
        in vec3 centre;
        in vec4 colour;
        in vec2 corner;
        in float pSize;
        out vec4 vColour;
        out vec2 vCorner;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        uniform vec3 uCamRight;
        uniform vec3 uCamUp;
        uniform float uSize;

        void main()
        {
            vColour = colour;
            vCorner = corner;
            float s = max (uSize * pSize, 0.0005);
            vec3 world = centre
                       + uCamRight * (corner.x * s)
                       + uCamUp    * (corner.y * s);
            gl_Position = projectionMatrix * viewMatrix * vec4 (world, 1.0);
        }
    )";

    constexpr const char* kParticleFS = R"(
        #version 150
        in vec4 vColour;
        in vec2 vCorner;
        out vec4 fragColour;
        uniform float uEmissiveMode;
        uniform float uEmissiveStrength;
        uniform float uRoughness;
        uniform float uMetalness;
        uniform float uSpecular;
        uniform float uLightingAmount;
        uniform vec3 uLightDirView;
        uniform vec3 uLightColour;

        void main()
        {
            float d = length (vCorner);
            if (d > 1.0)
                discard;
            float alpha = smoothstep (1.0, 0.55, d) * clamp (vColour.a, 0.0, 1.0);

            vec3 albedo = max (vColour.rgb, vec3 (0.0));
            if (uEmissiveMode > 0.5)
            {
                fragColour = vec4 (albedo * max (uEmissiveStrength, 0.0), alpha);
                return;
            }

            vec3 N = vec3 (0.0, 0.0, 1.0);
            vec3 L = normalize (uLightDirView);
            float NdotL = max (dot (N, L), 0.0);
            float rough = clamp (uRoughness, 0.04, 1.0);
            float metal = clamp (uMetalness, 0.0, 1.0);
            float specAmt = clamp (uSpecular, 0.0, 1.0);
            vec3 F0 = mix (vec3 (0.04), albedo, metal);
            vec3 diffuse = albedo * NdotL * uLightColour;
            vec3 H = normalize (L + vec3 (0.0, 0.0, 1.0));
            float NdH = max (dot (N, H), 0.0);
            float spec = pow (NdH, mix (8.0, 128.0, 1.0 - rough)) * specAmt;
            vec3 specular = F0 * spec * uLightColour;
            float amt = clamp (uLightingAmount, 0.0, 1.0);
            vec3 lit = mix (albedo, diffuse + specular, amt);
            fragColour = vec4 (lit, alpha);
        }
    )";
}

Spec3DParticleSystem::Spec3DParticleSystem (Spectrogram3DComponent& o)
    : owner (o)
{
}

Spec3DParticleSystem::~Spec3DParticleSystem()
{
}

void Spec3DParticleSystem::ensurePool()
{
    const int bins = juce::jmax (1, owner.meshH);
    if ((int) emitAccum.size() != bins)
        emitAccum.assign ((size_t) bins, 0.0f);

    const int cols = juce::jmax (2, owner.meshW);
    const int cap = juce::jmin (kHardCap, juce::jmax (1024, bins * juce::jmin (96, cols)));
    if ((int) pool.size() != cap)
    {
        pool.assign ((size_t) cap, {});
        nextWrite = 0;
    }
}

void Spec3DParticleSystem::clear()
{
    for (auto& p : pool)
        p.alive = false;
    nextWrite = 0;
    emitGlobal = 0.0f;
    amplitudeSmooth = 0.0f;
    slotEnv.fill (0.0f);
    std::fill (emitAccum.begin(), emitAccum.end(), 0.0f);
}

void Spec3DParticleSystem::scrollHistory (int numCols)
{
    if (numCols <= 0 || pool.empty())
        return;

    for (auto& p : pool)
    {
        if (! p.alive)
            continue;
        p.col -= numCols;
        if (p.col < 0)
        {
            p.alive = false;
            continue;
        }
        p.x = columnToWorldX (p.col);
    }
}

float Spec3DParticleSystem::columnToWorldX (int col) const noexcept
{
    if (owner.meshW < 2)
        return 1.0f;
    col = juce::jlimit (0, owner.meshW - 1, col);
    const float u = (float) col / (float) (owner.meshW - 1);
    return u * 2.0f - 1.0f;
}

int Spec3DParticleSystem::allocSlot()
{
    if (pool.empty())
        return -1;

    for (int n = 0; n < (int) pool.size(); ++n)
    {
        const int i = (nextWrite + n) % (int) pool.size();
        if (! pool[(size_t) i].alive)
        {
            nextWrite = (i + 1) % (int) pool.size();
            return i;
        }
    }

    int best = -1;
    int bestCol = 0x7fffffff;
    for (int i = 0; i < (int) pool.size(); ++i)
    {
        const auto& p = pool[(size_t) i];
        if (p.settled && p.col < bestCol)
        {
            bestCol = p.col;
            best = i;
        }
    }
    if (best >= 0)
    {
        nextWrite = (best + 1) % (int) pool.size();
        return best;
    }

    const int i = nextWrite;
    nextWrite = (nextWrite + 1) % (int) pool.size();
    return i;
}

float Spec3DParticleSystem::sampleColumn (int bin, int col,
                                          float& outR, float& outG, float& outB, float& outZ,
                                          float& outDb01, float& outFreq01) const
{
    outR = outG = outB = 1.0f;
    outZ = 0.0f;
    outDb01 = 0.0f;
    outFreq01 = 0.0f;
    if (owner.meshW < 2 || owner.meshH < 2 || owner.meshDb.empty() || owner.lastBrightness < 0.0f)
        return 0.0f;

    bin = juce::jlimit (0, owner.meshH - 1, bin);
    col = juce::jlimit (0, owner.meshW - 1, col);
    const float db = owner.meshDb[(size_t) col * (size_t) owner.meshH + (size_t) bin];
    const float denom = juce::jmax (1.0f, owner.lastMaxDb - owner.lastMinDb);
    const float n = juce::jlimit (0.0f, 1.0f, (db - owner.lastMinDb) / denom);
    outDb01 = n;

    const float B = owner.freqMeshBiasB();
    const float P = owner.freqMeshBiasPivot;
    const float t = owner.meshH > 1 ? (float) bin / (float) (owner.meshH - 1) : 0.0f;
    const float freqU = Spectrogram3DComponent::freqAxisFromMeshT (t, B, P);
    outFreq01 = juce::jlimit (0.0f, 1.0f, freqU);
    outZ = owner.reverseFrequencyAxis ? (freqU * 2.0f - 1.0f)
                                      : ((1.0f - freqU) * 2.0f - 1.0f);

    if (owner.dataSource != nullptr)
    {
        const auto c = owner.dataSource->colourFromHistoryDb3D (
            db, owner.lastBrightness, owner.lastMinDb, owner.lastMaxDb);
        outR = c.getFloatRed();
        outG = c.getFloatGreen();
        outB = c.getFloatBlue();
    }
    return n * owner.meshHeight;
}

float Spec3DParticleSystem::playheadAmplitude01() const
{
    if (owner.meshW < 2 || owner.meshH < 2 || owner.meshDb.empty() || owner.lastBrightness < 0.0f)
        return 0.0f;

    const int col = owner.meshW - 1;
    const float denom = juce::jmax (1.0f, owner.lastMaxDb - owner.lastMinDb);
    float sum = 0.0f;
    float peak = 0.0f;
    const int bins = owner.meshH;
    for (int bin = 0; bin < bins; ++bin)
    {
        const float db = owner.meshDb[(size_t) col * (size_t) owner.meshH + (size_t) bin];
        const float n = juce::jlimit (0.0f, 1.0f, (db - owner.lastMinDb) / denom);
        sum += n;
        peak = juce::jmax (peak, n);
    }
    const float mean = sum / (float) juce::jmax (1, bins);
    // Blend mean + peak so broadband and spiky material both register.
    float a = 0.55f * mean + 0.45f * peak;
    // Fold in sidechain level when present (musical amplitude).
    a = juce::jmax (a, owner.audioLevelLive01);
    return juce::jlimit (0.0f, 1.0f, a);
}

float Spec3DParticleSystem::applyOp (float base, float src, ParticleModOp op, float amount) noexcept
{
    amount = juce::jmax (0.0f, amount);
    src = juce::jlimit (0.0f, 1.0f, src);
    switch (op)
    {
        case ParticleModOp::set:
            return base + amount * (src - base); // amount 1 → src
        case ParticleModOp::add:
            return base + src * amount;
        case ParticleModOp::multiply:
        default:
            return base * (1.0f + amount * (src - 1.0f)); // amount 1 → base*src
    }
}

float Spec3DParticleSystem::sampleSource (ParticleModSource src, float constant,
                                          const Particle* p, const FrameSources& frame) const
{
    switch (src)
    {
        case ParticleModSource::amplitude: return frame.amplitude;
        case ParticleModSource::binDb:     return p != nullptr ? p->binDb01 : 0.0f;
        case ParticleModSource::binFreq:   return p != nullptr ? p->binFreq01 : 0.0f;
        case ParticleModSource::ageNorm:
            if (p == nullptr || p->maxLife < 1.0e-4f)
                return 0.0f;
            return juce::jlimit (0.0f, 1.0f, p->age / p->maxLife);
        case ParticleModSource::history:
            if (p == nullptr || owner.meshW < 2)
                return 1.0f;
            return juce::jlimit (0.0f, 1.0f, (float) p->col / (float) (owner.meshW - 1));
        case ParticleModSource::constant:
            return juce::jlimit (0.0f, 1.0f, constant);
        case ParticleModSource::none:
        default:
            return 0.0f;
    }
}

bool Spec3DParticleSystem::isGlobalSource (ParticleModSource src) noexcept
{
    return src == ParticleModSource::amplitude || src == ParticleModSource::constant;
}

void Spec3DParticleSystem::tickGlobalThresholds (const FrameSources& frame, float dt) noexcept
{
    slotGlobalValid.fill (false);
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled || slot.source == ParticleModSource::none)
            continue;
        if (! isGlobalSource (slot.source))
            continue;

        float raw = sampleSource (slot.source, slot.constant, nullptr, frame);
        if (slot.thresholdEnabled)
        {
            float& env = slotEnv[(size_t) i];
            const float atkSec = juce::jmax (0.0001f, slot.attackMs * 0.001f);
            const float relSec = juce::jmax (0.0001f, slot.releaseMs * 0.001f);
            const float tau = (raw > env) ? atkSec : relSec;
            const float coef = 1.0f - std::exp (-dt / juce::jmax (1.0e-5f, tau));
            env += (raw - env) * coef;
            const float thr = juce::jlimit (0.0f, 0.99f, slot.threshold);
            raw = env <= thr ? 0.0f
                             : juce::jlimit (0.0f, 1.0f, (env - thr) / juce::jmax (1.0e-4f, 1.0f - thr));
        }
        slotGlobalSrc[(size_t) i] = particleModApplyCurve (raw, slot.curveShape);
        slotGlobalValid[(size_t) i] = true;
    }
}

float Spec3DParticleSystem::processSource (int slotIndex, const Particle* p,
                                           const FrameSources& frame) const noexcept
{
    if (! juce::isPositiveAndBelow (slotIndex, kParticleModSlotCount))
        return 0.0f;

    if (slotGlobalValid[(size_t) slotIndex])
        return slotGlobalSrc[(size_t) slotIndex];

    const auto& slot = owner.particleModSlots[(size_t) slotIndex];
    float raw = sampleSource (slot.source, slot.constant, p, frame);

    if (slot.thresholdEnabled)
    {
        // Per-particle sources: soft gate only (no multi-advance envelope).
        const float thr = juce::jlimit (0.0f, 0.99f, slot.threshold);
        raw = raw <= thr ? 0.0f
                         : juce::jlimit (0.0f, 1.0f, (raw - thr) / juce::jmax (1.0e-4f, 1.0f - thr));
    }

    return particleModApplyCurve (raw, slot.curveShape);
}

void Spec3DParticleSystem::setHue (float& r, float& g, float& b, float hue01) noexcept
{
    hue01 = juce::jlimit (0.0f, 1.0f, hue01);
    const juce::Colour c = juce::Colour::fromFloatRGBA (r, g, b, 1.0f);
    const float sat = c.getSaturation();
    const float bri = c.getBrightness();
    const auto out = juce::Colour::fromHSV (hue01, sat > 0.05f ? sat : 0.85f, juce::jmax (0.15f, bri), 1.0f);
    r = out.getFloatRed();
    g = out.getFloatGreen();
    b = out.getFloatBlue();
}

void Spec3DParticleSystem::applyColourMods (Particle& p, ParticleModDest dest, float src,
                                            ParticleModOp op, float amount) const
{
    if (dest == ParticleModDest::colourGain)
    {
        p.colourGain = applyOp (p.colourGain, src, op, amount);
        p.colourGain = juce::jlimit (0.0f, 8.0f, p.colourGain);
    }
    else if (dest == ParticleModDest::colourHue)
    {
        // Hue is absolute-ish: Set uses src as hue; Multiply/Add still treat as 0..1 hue target via lerp.
        float hue = src;
        if (op == ParticleModOp::set)
            hue = applyOp (0.0f, src, ParticleModOp::set, amount);
        else
            hue = applyOp (0.5f, src, op, amount);
        setHue (p.baseR, p.baseG, p.baseB, hue);
    }
    else if (dest == ParticleModDest::emissive)
    {
        p.emissiveScale = applyOp (p.emissiveScale, src, op, amount);
        p.emissiveScale = juce::jlimit (0.0f, 8.0f, p.emissiveScale);
    }
    else if (dest == ParticleModDest::alpha)
    {
        p.alpha = applyOp (p.alpha, src, op, amount);
        p.alpha = juce::jlimit (0.0f, 1.0f, p.alpha);
    }

    p.r = juce::jlimit (0.0f, 8.0f, p.baseR * p.colourGain * p.emissiveScale);
    p.g = juce::jlimit (0.0f, 8.0f, p.baseG * p.colourGain * p.emissiveScale);
    p.b = juce::jlimit (0.0f, 8.0f, p.baseB * p.colourGain * p.emissiveScale);
}

void Spec3DParticleSystem::applyEmitterMods (float& emission, const FrameSources& frame) noexcept
{
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled || slot.source == ParticleModSource::none)
            continue;
        if (slot.dest != ParticleModDest::emission)
            continue;
        const float src = processSource (i, nullptr, frame);
        emission = applyOp (emission, src, slot.op, slot.amount);
    }
    emission = juce::jmax (0.0f, emission);
}

void Spec3DParticleSystem::applySpawnMods (Particle& p, float& riseSpeed, float& lifespan, float& sizeScale,
                                           const FrameSources& frame) noexcept
{
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled || slot.source == ParticleModSource::none)
            continue;

        const float src = processSource (i, &p, frame);
        switch (slot.dest)
        {
            case ParticleModDest::riseSpeed:
                riseSpeed = applyOp (riseSpeed, src, slot.op, slot.amount);
                break;
            case ParticleModDest::lifespan:
                if (lifespan > 0.0f || slot.op == ParticleModOp::set || slot.op == ParticleModOp::add)
                    lifespan = applyOp (juce::jmax (0.0f, lifespan), src, slot.op, slot.amount);
                break;
            case ParticleModDest::size:
                sizeScale = applyOp (sizeScale, src, slot.op, slot.amount);
                break;
            case ParticleModDest::colourGain:
            case ParticleModDest::colourHue:
            case ParticleModDest::emissive:
            case ParticleModDest::alpha:
                applyColourMods (p, slot.dest, src, slot.op, slot.amount);
                break;
            default:
                break;
        }
    }
    riseSpeed = juce::jmax (0.02f, riseSpeed);
    sizeScale = juce::jlimit (0.05f, 8.0f, sizeScale);
    if (lifespan > 0.0f)
        lifespan = juce::jmax (0.02f, lifespan);
}

void Spec3DParticleSystem::applyUpdateMods (Particle& p, const FrameSources& frame) noexcept
{
    p.colourGain = 1.0f;
    p.emissiveScale = 1.0f;
    p.alpha = 1.0f;
    const float sizeSpawn = p.sizeScale;
    float sizeWork = sizeSpawn;

    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled || slot.source == ParticleModSource::none)
            continue;

        if (slot.dest == ParticleModDest::emission
            || slot.dest == ParticleModDest::riseSpeed
            || slot.dest == ParticleModDest::lifespan)
            continue;

        const float src = processSource (i, &p, frame);
        switch (slot.dest)
        {
            case ParticleModDest::size:
                sizeWork = applyOp (sizeSpawn, src, slot.op, slot.amount);
                break;
            case ParticleModDest::colourGain:
            case ParticleModDest::colourHue:
            case ParticleModDest::emissive:
            case ParticleModDest::alpha:
                applyColourMods (p, slot.dest, src, slot.op, slot.amount);
                break;
            default:
                break;
        }
    }

    p.r = juce::jlimit (0.0f, 8.0f, p.baseR * p.colourGain * p.emissiveScale);
    p.g = juce::jlimit (0.0f, 8.0f, p.baseG * p.colourGain * p.emissiveScale);
    p.b = juce::jlimit (0.0f, 8.0f, p.baseB * p.colourGain * p.emissiveScale);
    p.sizeScale = juce::jlimit (0.05f, 8.0f, sizeWork);
}

void Spec3DParticleSystem::spawnAtBin (int bin, float riseSpeedBase, float lifespanBase, float sizeBase)
{
    const int col = owner.meshW - 1;
    float r, g, b, z, db01, freq01;
    const float targetY = sampleColumn (bin, col, r, g, b, z, db01, freq01);
    if (targetY < 1.0e-4f)
        return;

    const int slot = allocSlot();
    if (slot < 0)
        return;

    float velY = juce::jmax (0.05f, riseSpeedBase);
    const float velR = juce::jlimit (0.0f, 1.0f, owner.particleVelRandom);
    if (velR > 1.0e-5f)
    {
        const float mul = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR;
        velY = juce::jmax (0.02f, velY * mul);
    }

    float maxLife = -1.0f;
    float lifeSec = lifespanBase;
    if (lifeSec > 1.0e-4f)
    {
        const float lifeR = juce::jlimit (0.0f, 1.0f, owner.particleLifespanRandom);
        if (lifeR > 1.0e-5f)
        {
            const float mul = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * lifeR;
            lifeSec = juce::jmax (0.02f, lifeSec * mul);
        }
        maxLife = lifeSec;
    }

    auto& p = pool[(size_t) slot];
    p.alive = true;
    p.settled = false;
    p.bin = bin;
    p.col = col;
    p.x = columnToWorldX (col);
    p.y = 0.0f;
    p.z = z;
    p.velY = velY;
    p.targetY = targetY;
    p.baseR = r; p.baseG = g; p.baseB = b;
    p.r = r; p.g = g; p.b = b;
    p.age = 0.0f;
    p.maxLife = maxLife;
    p.sizeScale = sizeBase;
    p.colourGain = 1.0f;
    p.emissiveScale = 1.0f;
    p.alpha = 1.0f;
    p.binDb01 = db01;
    p.binFreq01 = freq01;

    FrameSources frame;
    frame.amplitude = amplitudeSmooth;
    float rise = velY;
    float life = maxLife > 0.0f ? maxLife : 0.0f;
    float sizeSc = sizeBase;
    applySpawnMods (p, rise, life, sizeSc, frame);
    p.velY = rise;
    p.sizeScale = sizeSc;
    if (life > 1.0e-4f)
        p.maxLife = life;
    else if (lifespanBase <= 1.0e-4f && life <= 1.0e-4f)
        p.maxLife = -1.0f;
}

void Spec3DParticleSystem::update (float dtSeconds)
{
    if (owner.meshH < 2 || owner.meshW < 2 || owner.meshDb.empty())
        return;

    dtSeconds = juce::jlimit (0.0f, 0.1f, dtSeconds);
    ensurePool();

    if (owner.dataSource != nullptr)
        owner.dataSource->refreshColourLutFor3D();

    // Smooth playhead amplitude for emission + matrix sources.
    const float ampInst = playheadAmplitude01();
    const float smoothK = 1.0f - std::exp (-dtSeconds * 12.0f);
    amplitudeSmooth += (ampInst - amplitudeSmooth) * smoothK;

    FrameSources frame;
    frame.amplitude = amplitudeSmooth;

    tickGlobalThresholds (frame, dtSeconds);

    float emission = owner.particleEmission;
    applyEmitterMods (emission, frame);

    const int bins = owner.meshH;
    // emission slider 0..5 → particles/sec (scales with bin count). Higher than v1.
    const float totalRate = emission * (float) bins * 12.0f;

    float riseBase = owner.particleRiseSpeed;
    float lifeBase = owner.particleLifespan;
    float sizeBase = 1.0f;

    if (owner.particleEmitMode == Spectrogram3DComponent::ParticleEmitMode::continuous)
    {
        // Uniform random bins — cheap, even surface fill over time.
        emitGlobal += totalRate * dtSeconds;
        int spawned = 0;
        constexpr int kMaxSpawnPerFrame = 1024;
        while (emitGlobal >= 1.0f && spawned < kMaxSpawnPerFrame)
        {
            emitGlobal -= 1.0f;
            const int bin = bins > 1 ? rng.nextInt (bins) : 0;
            spawnAtBin (bin, riseBase, lifeBase, sizeBase);
            ++spawned;
        }
        if (emitGlobal > 16.0f)
            emitGlobal = 16.0f;
    }
    else
    {
        const float perBin = totalRate / (float) juce::jmax (1, bins);
        for (int bin = 0; bin < bins; ++bin)
        {
            emitAccum[(size_t) bin] += perBin * dtSeconds;
            while (emitAccum[(size_t) bin] >= 1.0f)
            {
                emitAccum[(size_t) bin] -= 1.0f;
                spawnAtBin (bin, riseBase, lifeBase, sizeBase);
            }
        }
    }

    for (auto& p : pool)
    {
        if (! p.alive)
            continue;

        if (p.maxLife >= 0.0f)
        {
            p.age += dtSeconds;
            if (p.age >= p.maxLife)
            {
                p.alive = false;
                continue;
            }
        }

        float r, g, b, z, db01, freq01;
        const float colHeight = sampleColumn (p.bin, p.col, r, g, b, z, db01, freq01);
        p.x = columnToWorldX (p.col);
        p.z = z;
        p.baseR = r; p.baseG = g; p.baseB = b;
        p.binDb01 = db01;
        p.binFreq01 = freq01;
        p.targetY = colHeight;

        applyUpdateMods (p, frame);

        if (p.settled)
        {
            if (p.y > p.targetY)
                p.y = p.targetY;
            continue;
        }

        p.y += p.velY * dtSeconds;
        if (p.y >= p.targetY)
        {
            p.y = p.targetY;
            p.velY = 0.0f;
            p.settled = true;
        }
    }
}

bool Spec3DParticleSystem::createProgram (juce::OpenGLContext& context)
{
    program = std::make_unique<juce::OpenGLShaderProgram> (context);
    if (! program->addVertexShader (kParticleVS)
        || ! program->addFragmentShader (kParticleFS)
        || ! program->link())
    {
        DBG ("Spec3D particle shader: " + program->getLastError());
        program.reset();
        return false;
    }

    aPos = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*program, "centre");
    aCol = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*program, "colour");
    aCorner = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*program, "corner");
    aSize = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*program, "pSize");
    uProj = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "projectionMatrix");
    uView = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "viewMatrix");
    uCamRight = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uCamRight");
    uCamUp = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uCamUp");
    uSize = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uSize");
    uEmissiveMode = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uEmissiveMode");
    uEmissiveStrength = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uEmissiveStrength");
    uRoughness = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uRoughness");
    uMetalness = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uMetalness");
    uSpecular = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uSpecular");
    uLightingAmount = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uLightingAmount");
    uLightDirView = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uLightDirView");
    uLightColour = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*program, "uLightColour");
    return true;
}

void Spec3DParticleSystem::ensureGl (juce::OpenGLContext& context)
{
    if (glReady)
        return;

    if (! createProgram (context))
        return;

    juce::gl::glGenBuffers (1, &vbo);
    glReady = (vbo != 0 && program != nullptr);
}

void Spec3DParticleSystem::releaseGl()
{
    if (vbo != 0)
    {
        juce::gl::glDeleteBuffers (1, &vbo);
        vbo = 0;
    }
    program.reset();
    aPos.reset();
    aCol.reset();
    aCorner.reset();
    aSize.reset();
    uProj.reset();
    uView.reset();
    uCamRight.reset();
    uCamUp.reset();
    uSize.reset();
    uEmissiveMode.reset();
    uEmissiveStrength.reset();
    uRoughness.reset();
    uMetalness.reset();
    uSpecular.reset();
    uLightingAmount.reset();
    uLightDirView.reset();
    uLightColour.reset();
    glReady = false;
}

void Spec3DParticleSystem::draw (const juce::Matrix3D<float>& projection,
                                 const juce::Matrix3D<float>& view,
                                 juce::Vector3D<float> camRight,
                                 juce::Vector3D<float> camUp)
{
    using namespace juce::gl;
    if (! glReady || program == nullptr || vbo == 0)
        return;

    gpuVerts.clear();
    const float corners[6][2] = {
        { -1.0f, -1.0f }, { 1.0f, -1.0f }, { 1.0f, 1.0f },
        { -1.0f, -1.0f }, { 1.0f, 1.0f }, { -1.0f, 1.0f }
    };

    for (const auto& p : pool)
    {
        if (! p.alive)
            continue;
        for (const auto& c : corners)
        {
            GpuVert v;
            v.px = p.x; v.py = p.y; v.pz = p.z;
            v.r = p.r; v.g = p.g; v.b = p.b;
            v.a = p.alpha;
            v.cx = c[0]; v.cy = c[1];
            v.size = p.sizeScale;
            gpuVerts.push_back (v);
        }
    }

    if (gpuVerts.empty())
        return;

    const float az = juce::degreesToRadians (owner.lightAzimuthDeg);
    const float el = juce::degreesToRadians (juce::jlimit (5.0f, 89.0f, owner.lightElevationDeg));
    const juce::Vector3D<float> lightWorld {
        std::cos (el) * std::sin (az),
        std::sin (el),
        std::cos (el) * std::cos (az)
    };
    juce::Vector3D<float> right, up, forward;
    owner.cameraBasis (right, up, forward);
    juce::ignoreUnused (up);
    const float lx = lightWorld.x * right.x + lightWorld.y * right.y + lightWorld.z * right.z;
    const float ly = lightWorld.x * camUp.x + lightWorld.y * camUp.y + lightWorld.z * camUp.z;
    const float lz = -(lightWorld.x * forward.x + lightWorld.y * forward.y + lightWorld.z * forward.z);
    const float llen = juce::jmax (1.0e-5f, std::sqrt (lx * lx + ly * ly + lz * lz));

    program->use();
    if (uProj != nullptr) uProj->setMatrix4 (projection.mat, 1, false);
    if (uView != nullptr) uView->setMatrix4 (view.mat, 1, false);
    if (uCamRight != nullptr) uCamRight->set (camRight.x, camRight.y, camRight.z);
    if (uCamUp != nullptr) uCamUp->set (camUp.x, camUp.y, camUp.z);
    if (uSize != nullptr) uSize->set (juce::jmax (0.001f, owner.particleSize));
    if (uEmissiveMode != nullptr) uEmissiveMode->set (owner.particleEmissiveEnabled ? 1.0f : 0.0f);
    if (uEmissiveStrength != nullptr) uEmissiveStrength->set (owner.particleEmissiveStrength);
    if (uRoughness != nullptr) uRoughness->set (owner.particleRoughness);
    if (uMetalness != nullptr) uMetalness->set (owner.particleMetalness);
    if (uSpecular != nullptr) uSpecular->set (owner.particleSpecular);
    if (uLightingAmount != nullptr)
        uLightingAmount->set (owner.lightingEnabled ? owner.lightingAmount : 0.0f);
    if (uLightDirView != nullptr) uLightDirView->set (lx / llen, ly / llen, lz / llen);
    if (uLightColour != nullptr)
        uLightColour->set (owner.lightColour.getFloatRed(),
                           owner.lightColour.getFloatGreen(),
                           owner.lightColour.getFloatBlue());

    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER,
                  (GLsizeiptr) (gpuVerts.size() * sizeof (GpuVert)),
                  gpuVerts.data(),
                  GL_STREAM_DRAW);

    const GLsizei stride = (GLsizei) sizeof (GpuVert);
    if (aPos != nullptr && aPos->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aPos->attributeID);
        glVertexAttribPointer ((GLuint) aPos->attributeID, 3, GL_FLOAT, GL_FALSE, stride,
                               (const void*) offsetof (GpuVert, px));
    }
    if (aCol != nullptr && aCol->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aCol->attributeID);
        glVertexAttribPointer ((GLuint) aCol->attributeID, 4, GL_FLOAT, GL_FALSE, stride,
                               (const void*) offsetof (GpuVert, r));
    }
    if (aCorner != nullptr && aCorner->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aCorner->attributeID);
        glVertexAttribPointer ((GLuint) aCorner->attributeID, 2, GL_FLOAT, GL_FALSE, stride,
                               (const void*) offsetof (GpuVert, cx));
    }
    if (aSize != nullptr && aSize->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aSize->attributeID);
        glVertexAttribPointer ((GLuint) aSize->attributeID, 1, GL_FLOAT, GL_FALSE, stride,
                               (const void*) offsetof (GpuVert, size));
    }

    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask (GL_TRUE);
    glEnable (GL_DEPTH_TEST);
    glDisable (GL_CULL_FACE);

    glDrawArrays (GL_TRIANGLES, 0, (GLsizei) gpuVerts.size());

    if (aPos != nullptr && aPos->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) aPos->attributeID);
    if (aCol != nullptr && aCol->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) aCol->attributeID);
    if (aCorner != nullptr && aCorner->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) aCorner->attributeID);
    if (aSize != nullptr && aSize->attributeID >= 0)
        glDisableVertexAttribArray ((GLuint) aSize->attributeID);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glDisable (GL_BLEND);
}
