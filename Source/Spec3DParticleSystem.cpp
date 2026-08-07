#include "Spec3DParticleSystem.h"
#include "Spectrogram3DComponent.h"
#include "SpectrogramComponent.h"
#include <cmath>

namespace
{
    // Billboard VS/FS: same GGX metalness lobe as waterfall / instanced meshes (N = screen-facing).
    // Per-particle emissive scale (matrix dest) arrives as pEm → vEm.
    constexpr const char* kParticleVS = R"(
        #version 150
        in vec3 centre;
        in vec4 colour;
        in vec2 corner;
        in float pSize;
        in float pEm;
        out vec4 vColour;
        out vec2 vCorner;
        out float vEm;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;
        uniform vec3 uCamRight;
        uniform vec3 uCamUp;
        uniform float uSize;

        void main()
        {
            vColour = colour;
            vCorner = corner;
            vEm = pEm;
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
        in float vEm;
        out vec4 fragColour;
        uniform float uEmissiveMode;
        uniform float uEmissiveStrength;
        uniform float uRoughness;
        uniform float uMetalness;
        uniform float uSpecular;
        uniform float uEnergyConserve;
        uniform float uLightingAmount;
        uniform vec3 uLightDirView;
        uniform vec3 uLightColour;

        // Same lobe as Spec3D waterfall colour shader.
        float distributionGGX (float NdotH, float rough)
        {
            float a = max (rough * rough, 0.0025);
            float a2 = a * a;
            float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
            return a2 / (3.14159265 * d * d);
        }
        float geometrySchlickGGX (float NdotX, float rough)
        {
            float r = rough + 1.0;
            float k = (r * r) / 8.0;
            return NdotX / (NdotX * (1.0 - k) + k);
        }
        vec3 fresnelSchlick (float cosTheta, vec3 F0)
        {
            return F0 + (1.0 - F0) * pow (clamp (1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        void main()
        {
            float d = length (vCorner);
            if (d > 1.0)
                discard;
            float alpha = smoothstep (1.0, 0.55, d) * clamp (vColour.a, 0.0, 1.0);

            vec3 albedo = max (vColour.rgb, vec3 (0.0));
            float emAmt = max (uEmissiveStrength, 0.0) * max (vEm, 0.0);
            // Legacy pure self-lit path (toggle).
            if (uEmissiveMode > 0.5)
            {
                fragColour = vec4 (albedo * max (emAmt, 1.0e-4), alpha);
                return;
            }

            vec3 N = vec3 (0.0, 0.0, 1.0);
            vec3 L = normalize (uLightDirView);
            vec3 V = vec3 (0.0, 0.0, 1.0);
            vec3 H = normalize (L + V);
            float NdotL = max (dot (N, L), 0.0);
            float NdotV = max (dot (N, V), 1.0e-4);
            float NdotH = max (dot (N, H), 0.0);
            float VdotH = max (dot (V, H), 0.0);
            float rough = clamp (uRoughness, 0.04, 1.0);
            float metal = clamp (uMetalness, 0.0, 1.0);
            float specAmt = clamp (uSpecular, 0.0, 1.0);
            vec3 lightCol = max (uLightColour, vec3 (0.0));
            vec3 F0 = mix (vec3 (0.04), albedo, metal);
            float D = distributionGGX (NdotH, rough);
            float G = geometrySchlickGGX (NdotL, rough) * geometrySchlickGGX (NdotV, rough);
            vec3 F = fresnelSchlick (VdotH, F0);
            vec3 specular = (D * G * F) / max (4.0 * NdotV * max (NdotL, 1.0e-4), 1.0e-4);
            specular *= specAmt * lightCol;
            float wrap = NdotL * 0.72 + 0.28;
            vec3 kd = albedo * (1.0 - metal);
            if (uEnergyConserve > 0.5)
                kd *= max (vec3 (0.0), vec3 (1.0) - F);
            vec3 diffuse = kd * (0.22 + 0.78 * wrap) * lightCol;
            float amt = clamp (uLightingAmount, 0.0, 1.0);
            vec3 lit = mix (albedo, diffuse + specular, amt);
            // Additive emissive (matrix-routable per particle via vEm).
            lit += albedo * emAmt;
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
    simTime = 0.0f;
    slotEnv.fill (0.0f);
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
        for (int c = 0; c < 3; ++c)
            randomSmoothV[i][c] = 0.0f;
    std::fill (emitAccum.begin(), emitAccum.end(), 0.0f);
}

void Spec3DParticleSystem::scrollHistory (int numCols)
{
    if (numCols <= 0 || pool.empty())
        return;

    // Free visualizer: particles do not track waterfall columns.
    if (owner.particleBindingMode == ParticleBindingMode::freeVisualizer)
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
    return ((float) col / (float) (owner.meshW - 1)) * 2.0f - 1.0f;
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

    int best = -1, bestCol = 0x7fffffff;
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
    float sum = 0.0f, peak = 0.0f;
    for (int bin = 0; bin < owner.meshH; ++bin)
    {
        const float db = owner.meshDb[(size_t) col * (size_t) owner.meshH + (size_t) bin];
        const float n = juce::jlimit (0.0f, 1.0f, (db - owner.lastMinDb) / denom);
        sum += n;
        peak = juce::jmax (peak, n);
    }
    float a = 0.55f * (sum / (float) juce::jmax (1, owner.meshH)) + 0.45f * peak;
    a = juce::jmax (a, owner.audioLevelLive01);
    return juce::jlimit (0.0f, 1.0f, a);
}

float Spec3DParticleSystem::applyOp (float base, float src, ParticleModOp op, float amount) noexcept
{
    // No artistic caps on amount or src â€” extremes are intentional.
    switch (op)
    {
        case ParticleModOp::set:      return base + amount * (src - base);
        case ParticleModOp::add:      return base + src * amount;
        case ParticleModOp::multiply:
        default:                      return base * (1.0f + amount * (src - 1.0f));
    }
}

int Spec3DParticleSystem::randomIndex (ParticleModSource src) noexcept
{
    switch (src)
    {
        case ParticleModSource::random1: return 0;
        case ParticleModSource::random2: return 1;
        case ParticleModSource::random3: return 2;
        default: return -1;
    }
}

bool Spec3DParticleSystem::isGlobalSource (ParticleModSource src) noexcept
{
    return src == ParticleModSource::amplitude
        || src == ParticleModSource::constant
        || randomIndex (src) >= 0;
}

void Spec3DParticleSystem::rollRandomChannels (int sourceIndex, float out[3]) noexcept
{
    out[0] = out[1] = out[2] = 0.0f;
    if (! juce::isPositiveAndBelow (sourceIndex, kParticleRandomSourceCount))
        return;
    const auto& gen = owner.particleRandomSources[(size_t) sourceIndex];
    const float lo = gen.minV, hi = gen.maxV;
    const int n = gen.dim == ParticleRandomDim::Vec3 ? 3
                : (gen.dim == ParticleRandomDim::Vec2 ? 2 : 1);
    for (int c = 0; c < n; ++c)
        out[c] = lo + rng.nextFloat() * (hi - lo);
    // Float → broadcast to xyz (isotropic). Vec2 → z = 0.
    if (n == 1)
    {
        out[1] = out[0];
        out[2] = out[0];
    }
    else if (n == 2)
    {
        out[2] = 0.0f;
    }
}

void Spec3DParticleSystem::tickRandomSources (float dt) noexcept
{
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
    {
        auto& gen = owner.particleRandomSources[(size_t) i];
        if (! gen.active)
            continue;

        if (gen.mode == ParticleRandomMode::PerFrame)
        {
            rollRandomChannels (i, randomSmoothV[i]);
        }
        else if (gen.mode == ParticleRandomMode::Smoothed)
        {
            float target[3];
            rollRandomChannels (i, target);
            const float tau = juce::jmax (0.001f, gen.smoothMs * 0.001f);
            const float k = 1.0f - std::exp (-dt / juce::jmax (1.0e-5f, tau));
            for (int c = 0; c < 3; ++c)
                randomSmoothV[i][c] += (target[c] - randomSmoothV[i][c]) * k;
        }
        // PerParticle: rolled at spawn into particle.randV[i]
    }
}

juce::Vector3D<float> Spec3DParticleSystem::sampleSourceVec (ParticleModSource src, float constant,
                                                             const Particle* p,
                                                             const FrameSources& frame) const
{
    const int ri = randomIndex (src);
    if (ri >= 0)
    {
        const auto& gen = owner.particleRandomSources[(size_t) ri];
        const float* ch = nullptr;
        if (gen.mode == ParticleRandomMode::PerParticle && p != nullptr)
            ch = p->randV[ri];
        else
            ch = frame.randomV[ri];
        return { ch[0], ch[1], ch[2] };
    }

    if (src == ParticleModSource::initVel)
    {
        if (p == nullptr)
            return {};
        // Full birth velocity as Vec3 (Float dests use .x = speed via sampleSource).
        return { p->velX, p->velY, p->velZ };
    }

    float s = 0.0f;
    switch (src)
    {
        case ParticleModSource::amplitude: s = frame.amplitude; break;
        case ParticleModSource::binDb:     s = p != nullptr ? p->binDb01 : 0.0f; break;
        case ParticleModSource::binFreq:   s = p != nullptr ? p->binFreq01 : 0.0f; break;
        case ParticleModSource::ageNorm:
            if (p == nullptr || p->maxLife < 1.0e-4f) s = 0.0f;
            else s = juce::jlimit (0.0f, 1.0f, p->age / p->maxLife);
            break;
        case ParticleModSource::history:
            if (p == nullptr || owner.meshW < 2) s = 1.0f;
            else s = juce::jlimit (0.0f, 1.0f, (float) p->col / (float) (owner.meshW - 1));
            break;
        case ParticleModSource::constant:
            s = juce::jlimit (0.0f, 1.0f, constant);
            break;
        case ParticleModSource::particleId:
            if (p == nullptr)
                s = 0.0f;
            else
            {
                // Well-distributed 0–1 from birth id (stable for the particle lifetime).
                uint32_t n = p->id;
                n ^= n >> 16;
                n *= 0x7feb352du;
                n ^= n >> 15;
                n *= 0x846ca68bu;
                n ^= n >> 16;
                s = (n & 0x00ffffffu) * (1.0f / 16777215.0f);
            }
            break;
        default:
            s = 0.0f;
            break;
    }
    // Scalar sources broadcast so a Float→Vec3 dest (e.g. init rot) is isotropic.
    return { s, s, s };
}

float Spec3DParticleSystem::sampleSource (ParticleModSource src, float constant,
                                          const Particle* p, const FrameSources& frame) const
{
    if (src == ParticleModSource::initVel)
    {
        if (p == nullptr)
            return 0.0f;
        // Speed magnitude (world units/s). Map-range / amount shape the scale.
        const float sp = std::sqrt (p->velX * p->velX + p->velY * p->velY + p->velZ * p->velZ);
        return sp;
    }
    return sampleSourceVec (src, constant, p, frame).x;
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
        raw = particleModApplyCurve (raw, slot.curveShape);
        // Map range may extend beyond 0â€“1 for extreme artistic control.
        raw = slot.mapMin + (slot.mapMax - slot.mapMin) * raw;
        slotGlobalSrc[(size_t) i] = raw;
        slotGlobalValid[(size_t) i] = true;
    }
}

float Spec3DParticleSystem::processSource (int slotIndex, const Particle* p,
                                           const FrameSources& frame) const noexcept
{
    return processSourceVec (slotIndex, p, frame).x;
}

juce::Vector3D<float> Spec3DParticleSystem::processSourceVec (int slotIndex, const Particle* p,
                                                              const FrameSources& frame) const noexcept
{
    if (! juce::isPositiveAndBelow (slotIndex, kParticleModSlotCount))
        return {};

    // Global threshold path only caches scalar (x). For Vec3 dests re-sample full vector
    // so Random Vec3 still drives all axes; threshold still gates on the x channel.
    const auto& slot = owner.particleModSlots[(size_t) slotIndex];
    auto v = sampleSourceVec (slot.source, slot.constant, p, frame);

    auto mapChannel = [&] (float raw) -> float
    {
        if (slot.thresholdEnabled)
        {
            const float thr = slot.threshold;
            const float denom = juce::jmax (1.0e-4f, 1.0f - thr);
            // If a global env was computed, use it for gate consistency on x only.
            if (slotGlobalValid[(size_t) slotIndex] && &raw == &v.x)
            {
                // fall through using raw after optional gate below
            }
            raw = raw <= thr ? 0.0f : (raw - thr) / denom;
        }
        raw = particleModApplyCurve (raw, slot.curveShape);
        return slot.mapMin + (slot.mapMax - slot.mapMin) * raw;
    };

    // Prefer cached global scalar for amplitude/constant float paths (threshold AR).
    if (slotGlobalValid[(size_t) slotIndex]
        && particleModDestType (particleModDestCanonical (slot.dest)) == ParticleValueType::Float)
    {
        return { slotGlobalSrc[(size_t) slotIndex], 0.0f, 0.0f };
    }

    if (slotGlobalValid[(size_t) slotIndex]
        && particleModDestType (particleModDestCanonical (slot.dest)) == ParticleValueType::Vec3)
    {
        // Gate entire vector by whether the global env (x path) passed threshold.
        const float gate = slotGlobalSrc[(size_t) slotIndex];
        if (slot.thresholdEnabled && gate <= 1.0e-6f)
            return {};
        // Re-map each channel without re-thresholding (env already gated).
        auto mapNoThr = [&] (float raw) -> float
        {
            raw = particleModApplyCurve (raw, slot.curveShape);
            return slot.mapMin + (slot.mapMax - slot.mapMin) * raw;
        };
        return { mapNoThr (v.x), mapNoThr (v.y), mapNoThr (v.z) };
    }

    return { mapChannel (v.x), mapChannel (v.y), mapChannel (v.z) };
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
    }
    else if (dest == ParticleModDest::colourHue)
    {
        float hue = (op == ParticleModOp::set) ? applyOp (0.0f, src, ParticleModOp::set, amount)
                                               : applyOp (0.5f, src, op, amount);
        setHue (p.baseR, p.baseG, p.baseB, hue);
    }
    else if (dest == ParticleModDest::emissive)
    {
        // Material emissive amount (separate from albedo) — drawn as additive glow.
        p.emissiveScale = applyOp (p.emissiveScale, src, op, amount);
    }
    else if (dest == ParticleModDest::alpha)
    {
        p.alpha = applyOp (p.alpha, src, op, amount);
    }

    // Albedo stays energy-correct for PBR; emissive is applied in the shader.
    p.r = p.baseR * p.colourGain;
    p.g = p.baseG * p.colourGain;
    p.b = p.baseB * p.colourGain;
}

void Spec3DParticleSystem::applySystemMods (float& emission, float& spawnJitter, ForceModScales& scales,
                                            const FrameSources& frame) noexcept
{
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled || slot.source == ParticleModSource::none)
            continue;

        float src = processSource (i, nullptr, frame);
        if (slot.invert)
            src = 1.0f - src;

        auto mulScale = [&] (float& scale, float& add)
        {
            if (slot.op == ParticleModOp::add)
                add = applyOp (add, src, ParticleModOp::add, slot.amount);
            else if (slot.op == ParticleModOp::set)
                scale = applyOp (scale, src, ParticleModOp::set, slot.amount);
            else
                scale = applyOp (scale, src, ParticleModOp::multiply, slot.amount);
        };

        switch (slot.dest)
        {
            case ParticleModDest::emission:
                emission = applyOp (emission, src, slot.op, slot.amount); break;
            case ParticleModDest::spawnJitter:
                spawnJitter = applyOp (spawnJitter, src, slot.op, slot.amount); break;
            case ParticleModDest::forceGravity:      mulScale (scales.gravity, scales.gravityAdd); break;
            case ParticleModDest::forceDrag:         mulScale (scales.drag, scales.dragAdd); break;
            case ParticleModDest::forceWindX:        mulScale (scales.windX, scales.windAddX); break;
            case ParticleModDest::forceWindY:        mulScale (scales.windY, scales.windAddY); break;
            case ParticleModDest::forceWindZ:        mulScale (scales.windZ, scales.windAddZ); break;
            case ParticleModDest::forceCurlStrength: mulScale (scales.curlStrength, scales.curlStrAdd); break;
            case ParticleModDest::forceCurlScale:    mulScale (scales.curlScale, scales.curlScaleAdd); break;
            case ParticleModDest::forceCurlSpeed:    mulScale (scales.curlSpeed, scales.curlSpeedAdd); break;
            case ParticleModDest::forceTurbulence:   mulScale (scales.turbulence, scales.turbAdd); break;
            default: break;
        }
    }
}

void Spec3DParticleSystem::applySpawnMods (Particle& p, float& lifespan, float& sizeScale, float& jitterScale,
                                           float& velX, float& velY, float& velZ,
                                           float& rotX, float& rotY, float& rotZ,
                                           const FrameSources& frame) noexcept
{
    // Keep particle velocity in sync so Init vel as a *source* sees intermediate results.
    auto syncVel = [&]()
    {
        p.velX = velX;
        p.velY = velY;
        p.velZ = velZ;
    };
    syncVel();

    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled || slot.source == ParticleModSource::none)
            continue;

        switch (slot.dest)
        {
            case ParticleModDest::emission:
            case ParticleModDest::forceGravity:
            case ParticleModDest::forceDrag:
            case ParticleModDest::forceWindX:
            case ParticleModDest::forceWindY:
            case ParticleModDest::forceWindZ:
            case ParticleModDest::forceCurlStrength:
            case ParticleModDest::forceCurlScale:
            case ParticleModDest::forceCurlSpeed:
            case ParticleModDest::forceTurbulence:
                continue;
            default: break;
        }

        float src = processSource (i, &p, frame);
        if (slot.invert)
            src = 1.0f - src;
        switch (slot.dest)
        {
            case ParticleModDest::riseSpeed:
                // Legacy float dest — Y only (same as rise-speed slider path).
                velY = applyOp (velY, src, slot.op, slot.amount);
                syncVel();
                break;
            case ParticleModDest::initVel:
            {
                auto v = processSourceVec (i, &p, frame);
                if (slot.invert)
                {
                    v.x = 1.0f - v.x;
                    v.y = 1.0f - v.y;
                    v.z = 1.0f - v.z;
                }
                // Float sources broadcast via sampleSourceVec → isotropic push.
                // Random Vec3 / Init vel source → full independent axes.
                velX = applyOp (velX, v.x, slot.op, slot.amount);
                velY = applyOp (velY, v.y, slot.op, slot.amount);
                velZ = applyOp (velZ, v.z, slot.op, slot.amount);
                syncVel();
                break;
            }
            case ParticleModDest::lifespan:
                if (lifespan > 0.0f || slot.op != ParticleModOp::multiply)
                    lifespan = applyOp (juce::jmax (0.0f, lifespan), src, slot.op, slot.amount);
                break;
            case ParticleModDest::size:
            case ParticleModDest::sizeScale:
                sizeScale = applyOp (sizeScale, src, slot.op, slot.amount); break;
            case ParticleModDest::spawnJitter:
                jitterScale = applyOp (jitterScale, src, slot.op, slot.amount); break;
            case ParticleModDest::initRot:
            case ParticleModDest::initRotY_legacy:
            case ParticleModDest::initRotZ_legacy:
            {
                auto v = processSourceVec (i, &p, frame);
                if (slot.invert)
                {
                    v.x = 1.0f - v.x;
                    v.y = 1.0f - v.y;
                    v.z = 1.0f - v.z;
                }
                rotX = applyOp (rotX, v.x, slot.op, slot.amount);
                rotY = applyOp (rotY, v.y, slot.op, slot.amount);
                rotZ = applyOp (rotZ, v.z, slot.op, slot.amount);
                break;
            }
            case ParticleModDest::colourGain:
            case ParticleModDest::colourHue:
            case ParticleModDest::emissive:
            case ParticleModDest::alpha:
                applyColourMods (p, slot.dest, src, slot.op, slot.amount); break;
            default: break;
        }
    }
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
            || slot.dest == ParticleModDest::initVel
            || slot.dest == ParticleModDest::lifespan
            || slot.dest == ParticleModDest::spawnJitter
            || particleModDestCanonical (slot.dest) == ParticleModDest::initRot
            || (int) slot.dest >= (int) ParticleModDest::forceGravity)
            continue;

        float src = processSource (i, &p, frame);
        if (slot.invert)
            src = 1.0f - src;
        if (slot.dest == ParticleModDest::size || slot.dest == ParticleModDest::sizeScale)
            sizeWork = applyOp (sizeSpawn, src, slot.op, slot.amount);
        else
            applyColourMods (p, slot.dest, src, slot.op, slot.amount);
    }

    p.r = p.baseR * p.colourGain;
    p.g = p.baseG * p.colourGain;
    p.b = p.baseB * p.colourGain;
    p.sizeScale = sizeWork;
}

// Cheap hash / value noise for curl
float Spec3DParticleSystem::hashNoise (int x, int y, int z) noexcept
{
    int n = x * 374761393 + y * 668265263 + z * 2147483647;
    n = (n ^ (n >> 13)) * 1274126177;
    n = n ^ (n >> 16);
    return (n & 0x7fffffff) / 2147483647.0f;
}

float Spec3DParticleSystem::valueNoise (float x, float y, float z) noexcept
{
    const int x0 = (int) std::floor (x), y0 = (int) std::floor (y), z0 = (int) std::floor (z);
    const float fx = x - (float) x0, fy = y - (float) y0, fz = z - (float) z0;
    const float u = fx * fx * (3.0f - 2.0f * fx);
    const float v = fy * fy * (3.0f - 2.0f * fy);
    const float w = fz * fz * (3.0f - 2.0f * fz);

    auto lerp = [] (float a, float b, float t) { return a + (b - a) * t; };
    const float n000 = hashNoise (x0, y0, z0), n100 = hashNoise (x0 + 1, y0, z0);
    const float n010 = hashNoise (x0, y0 + 1, z0), n110 = hashNoise (x0 + 1, y0 + 1, z0);
    const float n001 = hashNoise (x0, y0, z0 + 1), n101 = hashNoise (x0 + 1, y0, z0 + 1);
    const float n011 = hashNoise (x0, y0 + 1, z0 + 1), n111 = hashNoise (x0 + 1, y0 + 1, z0 + 1);
    const float x00 = lerp (n000, n100, u), x10 = lerp (n010, n110, u);
    const float x01 = lerp (n001, n101, u), x11 = lerp (n011, n111, u);
    return lerp (lerp (x00, x10, v), lerp (x01, x11, v), w) * 2.0f - 1.0f;
}

juce::Vector3D<float> Spec3DParticleSystem::curlNoise (float x, float y, float z) noexcept
{
    // Finite-difference curl of potential field (value noise per axis offset).
    constexpr float e = 0.08f;
    const float n1 = valueNoise (x, y + e, z), n2 = valueNoise (x, y - e, z);
    const float n3 = valueNoise (x, y, z + e), n4 = valueNoise (x, y, z - e);
    const float n5 = valueNoise (x + e, y, z), n6 = valueNoise (x - e, y, z);
    // Use phase-shifted fields for 3 potentials
    const float py1 = valueNoise (x + 17.1f, y + e, z + 3.2f);
    const float py2 = valueNoise (x + 17.1f, y - e, z + 3.2f);
    const float pz1 = valueNoise (x + 5.4f, y + 9.1f, z + e);
    const float pz2 = valueNoise (x + 5.4f, y + 9.1f, z - e);
    const float px1 = valueNoise (x + e, y + 2.3f, z + 11.0f);
    const float px2 = valueNoise (x - e, y + 2.3f, z + 11.0f);

    juce::ignoreUnused (n1, n2, n3, n4, n5, n6);
    // curl F = (dFz/dy - dFy/dz, dFx/dz - dFz/dx, dFy/dx - dFx/dy)
    const float dFz_dy = (valueNoise (x + 5.4f, y + e + 9.1f, z) - valueNoise (x + 5.4f, y - e + 9.1f, z)) / (2.0f * e);
    const float dFy_dz = (py1 - py2) / (2.0f * e); // wrong - simplify:
    juce::ignoreUnused (dFy_dz);

    // Simpler practical curl-ish field:
    const float cx = (valueNoise (x, y + e, z) - valueNoise (x, y - e, z)
                    - (valueNoise (x, y, z + e) - valueNoise (x, y, z - e))) / (2.0f * e);
    const float cy = (valueNoise (x, y, z + e) - valueNoise (x, y, z - e)
                    - (valueNoise (x + e, y, z) - valueNoise (x - e, y, z))) / (2.0f * e);
    const float cz = (valueNoise (x + e, y, z) - valueNoise (x - e, y, z)
                    - (valueNoise (x, y + e, z) - valueNoise (x, y - e, z))) / (2.0f * e);
    juce::ignoreUnused (dFz_dy, pz1, pz2, px1, px2, py1, py2);
    return { cx, cy, cz };
}

void Spec3DParticleSystem::integrateForceStack (Particle& p, const ForceModScales& scales, float dt) noexcept
{
    if (! owner.particleForcesEnabled)
        return;

    // Evaluate ordered stack (module order = evaluation order).
    for (const auto& mod : owner.particleForceStack)
    {
        if (! mod.enabled)
            continue;

        switch (mod.type)
        {
            case ParticleForceType::gravity:
            {
                const float g = mod.p[0] * scales.gravity + scales.gravityAdd;
                p.velY += g * dt;
                break;
            }
            case ParticleForceType::drag:
            {
                const float d = mod.p[0] * scales.drag + scales.dragAdd;
                if (std::abs (d) > 1.0e-8f)
                {
                    const float damp = std::exp (-std::abs (d) * dt);
                    p.velX *= damp; p.velY *= damp; p.velZ *= damp;
                }
                break;
            }
            case ParticleForceType::wind:
            {
                p.velX += (mod.p[0] * scales.windX + scales.windAddX) * dt;
                p.velY += (mod.p[1] * scales.windY + scales.windAddY) * dt;
                p.velZ += (mod.p[2] * scales.windZ + scales.windAddZ) * dt;
                break;
            }
            case ParticleForceType::curlNoise:
            {
                float str = mod.p[0] * scales.curlStrength + scales.curlStrAdd;
                float sc = mod.p[1] * scales.curlScale + scales.curlScaleAdd;
                float spd = mod.p[2] * scales.curlSpeed + scales.curlSpeedAdd;
                if (std::abs (str) > 1.0e-8f)
                {
                    if (sc == 0.0f) sc = 1.0e-4f;
                    const float t = simTime * spd;
                    const auto c = curlNoise (p.x * sc + t, p.y * sc, p.z * sc + t * 0.7f);
                    p.velX += c.x * str * dt;
                    p.velY += c.y * str * dt;
                    p.velZ += c.z * str * dt;
                }
                break;
            }
            case ParticleForceType::turbulence:
            {
                const float str = mod.p[0] * scales.turbulence + scales.turbAdd;
                if (std::abs (str) > 1.0e-8f)
                {
                    p.velX += (rng.nextFloat() * 2.0f - 1.0f) * str * dt;
                    p.velY += (rng.nextFloat() * 2.0f - 1.0f) * str * dt;
                    p.velZ += (rng.nextFloat() * 2.0f - 1.0f) * str * dt;
                }
                break;
            }
            case ParticleForceType::rotation:
            {
                // Angular rate (rad/s) on enabled axes. More force → faster spin.
                // linkAxes: p[0] shared; else p[0]/p[1]/p[2] independent.
                // randomDir: multiply by per-particle spinScale (−1…1) so each
                // particle has its own direction and relative speed.
                const float rateX = mod.linkAxes ? mod.p[0] : mod.p[0];
                const float rateY = mod.linkAxes ? mod.p[0] : mod.p[1];
                const float rateZ = mod.linkAxes ? mod.p[0] : mod.p[2];
                const float sx = mod.randomDir ? p.spinScaleX : 1.0f;
                const float sy = mod.randomDir ? p.spinScaleY : 1.0f;
                const float sz = mod.randomDir ? p.spinScaleZ : 1.0f;
                if (mod.axisX) p.rotX += rateX * sx * dt;
                if (mod.axisY) p.rotY += rateY * sy * dt;
                if (mod.axisZ) p.rotZ += rateZ * sz * dt;
                break;
            }
            default: break;
        }
    }

    p.x += p.velX * dt;
    p.y += p.velY * dt;
    p.z += p.velZ * dt;
}

void Spec3DParticleSystem::eulerToQuat (float rx, float ry, float rz,
                                        float& qx, float& qy, float& qz, float& qw) noexcept
{
    const float cx = std::cos (rx * 0.5f), sx = std::sin (rx * 0.5f);
    const float cy = std::cos (ry * 0.5f), sy = std::sin (ry * 0.5f);
    const float cz = std::cos (rz * 0.5f), sz = std::sin (rz * 0.5f);
    qw = cx * cy * cz + sx * sy * sz;
    qx = sx * cy * cz - cx * sy * sz;
    qy = cx * sy * cz + sx * cy * sz;
    qz = cx * cy * sz - sx * sy * cz;
}

float Spec3DParticleSystem::pickPlayheadBinF() noexcept
{
    const int bins = owner.meshH;
    if (bins <= 1)
        return 0.0f;

    // Energy-weighted random along the playhead: louder bands spawn more often,
    // but position is continuous (not snapped to integer bin centres).
    const int col = owner.meshW - 1;
    const float denom = juce::jmax (1.0f, owner.lastMaxDb - owner.lastMinDb);
    float sum = 0.0f;
    std::vector<float> weights ((size_t) bins);
    for (int b = 0; b < bins; ++b)
    {
        const float db = owner.meshDb[(size_t) col * (size_t) bins + (size_t) b];
        const float n = juce::jlimit (0.0f, 1.0f, (db - owner.lastMinDb) / denom);
        // Squared emphasis so peaks dominate without zeroing silence completely.
        const float w = n * n + 0.02f;
        weights[(size_t) b] = w;
        sum += w;
    }
    if (sum <= 1.0e-8f)
        return rng.nextFloat() * (float) (bins - 1);

    float pick = rng.nextFloat() * sum;
    int bin = bins - 1;
    for (int b = 0; b < bins; ++b)
    {
        pick -= weights[(size_t) b];
        if (pick <= 0.0f)
        {
            bin = b;
            break;
        }
    }
    // Continuous offset within the chosen band so particles don't stack on grid points.
    const float sub = rng.nextFloat();
    return juce::jlimit (0.0f, (float) (bins - 1), (float) bin + sub * 0.999f);
}

void Spec3DParticleSystem::spawnAtPlayhead (float binF, float lifespanBase,
                                            float sizeBase, float spawnJitterScale)
{
    const int col = owner.meshW - 1;
    const int bins = owner.meshH;
    if (bins < 1)
        return;

    binF = juce::jlimit (0.0f, (float) juce::jmax (0, bins - 1), binF);
    const int b0 = juce::jlimit (0, bins - 1, (int) std::floor (binF));
    const int b1 = juce::jmin (bins - 1, b0 + 1);
    const float fr = binF - (float) b0;

    float r0, g0, b0c, z0, db0, f0;
    float r1, g1, b1c, z1, db1, f1;
    const float y0 = sampleColumn (b0, col, r0, g0, b0c, z0, db0, f0);
    const float y1 = sampleColumn (b1, col, r1, g1, b1c, z1, db1, f1);
    const float targetY = y0 * (1.0f - fr) + y1 * fr;
    if (targetY < 1.0e-4f && owner.particleBindingMode == ParticleBindingMode::spectrogramTrail)
        return;

    const float r = r0 * (1.0f - fr) + r1 * fr;
    const float g = g0 * (1.0f - fr) + g1 * fr;
    const float bcol = b0c * (1.0f - fr) + b1c * fr;
    const float z = z0 * (1.0f - fr) + z1 * fr;
    const float db01 = db0 * (1.0f - fr) + db1 * fr;
    const float freq01 = f0 * (1.0f - fr) + f1 * fr;

    const int slot = allocSlot();
    if (slot < 0)
        return;

    // Base initial velocity (Vec3) + optional per-axis random scale.
    float velX = owner.particleInitVelX;
    float velY = owner.particleInitVelY;
    float velZ = owner.particleInitVelZ;
    const float velR = owner.particleVelRandom;
    if (std::abs (velR) > 1.0e-5f)
    {
        velX *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
        velY *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
        velZ *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
    }

    float maxLife = -1.0f;
    float lifeSec = lifespanBase;
    // Free visualizer: default lifespan if indefinite to avoid infinite pool fill
    if (lifeSec <= 1.0e-4f && owner.particleBindingMode == ParticleBindingMode::freeVisualizer)
        lifeSec = 4.0f;
    if (lifeSec > 1.0e-4f)
    {
        const float lifeR = owner.particleLifespanRandom;
        if (std::abs (lifeR) > 1.0e-5f)
            lifeSec = lifeSec * (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * lifeR);
        maxLife = lifeSec;
    }

    auto& p = pool[(size_t) slot];
    p.alive = true;
    p.settled = false;
    p.id = nextParticleId++;
    if (nextParticleId == 0) // skip 0 so hash never collapses on unset particles
        nextParticleId = 1;
    p.bin = b0;
    p.binF = binF;
    p.col = col;
    p.x = columnToWorldX (col);
    p.y = 0.0f;
    p.z = z;
    p.velX = velX;
    p.velY = velY;
    p.velZ = velZ;
    p.targetY = juce::jmax (targetY, 0.01f);
    p.baseR = r; p.baseG = g; p.baseB = bcol;
    p.r = r; p.g = g; p.b = bcol;
    p.age = 0.0f;
    p.maxLife = maxLife;
    p.sizeScale = sizeBase;
    p.colourGain = 1.0f;
    p.emissiveScale = 1.0f;
    p.alpha = 1.0f;
    p.binDb01 = db01;
    p.binFreq01 = freq01;
    p.spawnOffX = p.spawnOffY = p.spawnOffZ = 0.0f;

    // Initial rotation (degrees → radians) + optional random
    float rotX = juce::degreesToRadians (owner.particleInitRotX);
    float rotY = juce::degreesToRadians (owner.particleInitRotY);
    float rotZ = juce::degreesToRadians (owner.particleInitRotZ);

    // Stable per-particle hash from slot + bin so every particle differs
    // (not one shared random for the whole cloud).
    auto particleHash01 = [] (uint32_t n) noexcept
    {
        n ^= n >> 16;
        n *= 0x7feb352du;
        n ^= n >> 15;
        n *= 0x846ca68bu;
        n ^= n >> 16;
        return (n & 0x00ffffffu) * (1.0f / 16777215.0f);
    };
    const uint32_t seed = (uint32_t) slot * 0x9E3779B9u
                        ^ (uint32_t) b0 * 747796405u
                        ^ (uint32_t) (col * 1597334677u)
                        ^ (uint32_t) (binF * 1000.0f);

    const float rotRnd = owner.particleInitRotRandom;
    if (std::abs (rotRnd) > 1.0e-6f)
    {
        const float span = juce::MathConstants<float>::twoPi * rotRnd;
        // Unique offset per axis × particle (hash-driven, not one cloud-wide roll).
        rotX += (particleHash01 (seed + 11u) * 2.0f - 1.0f) * span;
        rotY += (particleHash01 (seed + 29u) * 2.0f - 1.0f) * span;
        rotZ += (particleHash01 (seed + 47u) * 2.0f - 1.0f) * span;
    }

    // Continuous −1…1 spin scales for rotation force (unique per particle & axis).
    p.spinScaleX = particleHash01 (seed + 101u) * 2.0f - 1.0f;
    p.spinScaleY = particleHash01 (seed + 203u) * 2.0f - 1.0f;
    p.spinScaleZ = particleHash01 (seed + 307u) * 2.0f - 1.0f;
    // Avoid near-zero scales that look "stuck" while neighbours tumble.
    auto boostScale = [] (float s) noexcept
    {
        const float mag = juce::jmax (0.25f, std::abs (s));
        return (s < 0.0f ? -1.0f : 1.0f) * mag;
    };
    p.spinScaleX = boostScale (p.spinScaleX);
    p.spinScaleY = boostScale (p.spinScaleY);
    p.spinScaleZ = boostScale (p.spinScaleZ);

    for (int i = 0; i < kParticleRandomSourceCount; ++i)
        rollRandomChannels (i, p.randV[i]);

    FrameSources frame;
    frame.amplitude = amplitudeSmooth;
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
        for (int c = 0; c < 3; ++c)
            frame.randomV[i][c] = randomSmoothV[i][c];

    float life = maxLife > 0.0f ? maxLife : 0.0f;
    float sizeSc = sizeBase;
    float jitterSc = spawnJitterScale;
    float vx = p.velX, vy = p.velY, vz = p.velZ;
    applySpawnMods (p, life, sizeSc, jitterSc, vx, vy, vz, rotX, rotY, rotZ, frame);
    p.velX = vx;
    p.velY = vy;
    p.velZ = vz;
    p.sizeScale = sizeSc;
    p.rotX = rotX; p.rotY = rotY; p.rotZ = rotZ;
    if (life > 1.0e-4f)
        p.maxLife = life;

    // Scatter spawn location (default jitter > 0). Stored as offsets so trail lock
    // does not snap every particle back onto the exact mesh sample.
    const float j = juce::jmax (0.0f, owner.particleSpawnJitter) * juce::jmax (0.0f, jitterSc);
    if (j > 1.0e-6f)
    {
        p.spawnOffX = (rng.nextFloat() * 2.0f - 1.0f) * j;
        p.spawnOffY = rng.nextFloat() * j * 0.35f;
        p.spawnOffZ = (rng.nextFloat() * 2.0f - 1.0f) * j;
    }
    p.x += p.spawnOffX;
    p.y += p.spawnOffY;
    p.z += p.spawnOffZ;
}

void Spec3DParticleSystem::update (float dtSeconds)
{
    if (owner.meshH < 2 || owner.meshW < 2 || owner.meshDb.empty())
        return;

    dtSeconds = juce::jlimit (0.0f, 0.1f, dtSeconds);
    ensurePool();
    simTime += dtSeconds;

    if (owner.dataSource != nullptr)
        owner.dataSource->refreshColourLutFor3D();

    // Mark random sources active if any matrix row uses them
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
        owner.particleRandomSources[(size_t) i].active = false;
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto& slot = owner.particleModSlots[(size_t) i];
        if (! slot.enabled) continue;
        const int ri = randomIndex (slot.source);
        if (ri >= 0)
            owner.particleRandomSources[(size_t) ri].active = true;
    }

    tickRandomSources (dtSeconds);

    const float ampInst = playheadAmplitude01();
    amplitudeSmooth += (ampInst - amplitudeSmooth) * (1.0f - std::exp (-dtSeconds * 12.0f));

    FrameSources frame;
    frame.amplitude = amplitudeSmooth;
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
        for (int c = 0; c < 3; ++c)
            frame.randomV[i][c] = randomSmoothV[i][c];

    tickGlobalThresholds (frame, dtSeconds);

    ForceModScales forceScales;
    float emission = owner.particleEmission;
    float spawnJitter = 1.0f;
    applySystemMods (emission, spawnJitter, forceScales, frame);

    const int bins = owner.meshH;
    const float totalRate = emission * (float) bins * 12.0f;
    float lifeBase = owner.particleLifespan;
    const float sizeBase = 1.0f;
    const bool freeMode = owner.particleBindingMode == ParticleBindingMode::freeVisualizer;

    if (owner.particleEmitMode == Spectrogram3DComponent::ParticleEmitMode::continuous)
    {
        // Random playhead samples (energy-weighted continuous binF + scatter).
        emitGlobal += totalRate * dtSeconds;
        int spawned = 0;
        while (emitGlobal >= 1.0f && spawned < 1024)
        {
            emitGlobal -= 1.0f;
            spawnAtPlayhead (pickPlayheadBinF(), lifeBase, sizeBase, spawnJitter);
            ++spawned;
        }
        if (emitGlobal > 16.0f) emitGlobal = 16.0f;
    }
    else
    {
        // Slice mode: each bin emits on its own clock, but still with continuous
        // sub-bin offset + scatter so particles aren't identical grid clones.
        const float perBin = totalRate / (float) juce::jmax (1, bins);
        for (int bin = 0; bin < bins; ++bin)
        {
            emitAccum[(size_t) bin] += perBin * dtSeconds;
            while (emitAccum[(size_t) bin] >= 1.0f)
            {
                emitAccum[(size_t) bin] -= 1.0f;
                const float binF = (float) bin + rng.nextFloat() * 0.999f;
                spawnAtPlayhead (binF, lifeBase, sizeBase, spawnJitter);
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

        // Re-sample playhead using continuous binF (interp), not integer bin only.
        const int bins = owner.meshH;
        const float binF = juce::jlimit (0.0f, (float) juce::jmax (0, bins - 1), p.binF);
        const int b0 = juce::jlimit (0, juce::jmax (0, bins - 1), (int) std::floor (binF));
        const int b1 = juce::jmin (bins - 1, b0 + 1);
        const float fr = binF - (float) b0;
        float r0, g0, bb0, z0, db0, f0;
        float r1, g1, bb1, z1, db1, f1;
        const float y0 = sampleColumn (b0, p.col, r0, g0, bb0, z0, db0, f0);
        const float y1 = sampleColumn (b1, p.col, r1, g1, bb1, z1, db1, f1);
        p.bin = b0;
        p.binDb01 = db0 * (1.0f - fr) + db1 * fr;
        p.binFreq01 = f0 * (1.0f - fr) + f1 * fr;
        p.baseR = r0 * (1.0f - fr) + r1 * fr;
        p.baseG = g0 * (1.0f - fr) + g1 * fr;
        p.baseB = bb0 * (1.0f - fr) + bb1 * fr;
        const float z = z0 * (1.0f - fr) + z1 * fr;
        p.targetY = juce::jmax (y0 * (1.0f - fr) + y1 * fr, 0.01f);

        if (! freeMode)
        {
            // Trail: follow history column + continuous frequency, keep spawn scatter.
            p.x = columnToWorldX (p.col) + p.spawnOffX;
            p.z = z + p.spawnOffZ;
        }

        applyUpdateMods (p, frame);

        if (owner.particleForcesEnabled)
        {
            integrateForceStack (p, forceScales, dtSeconds);
            if (! freeMode && owner.particleWaterfallLock)
            {
                p.x = columnToWorldX (p.col) + p.spawnOffX;
                p.velX = 0.0f;
            }
            if (! freeMode && ! p.settled)
            {
                const float capY = p.targetY + p.spawnOffY;
                if (p.y >= capY)
                {
                    p.y = capY;
                    if (p.velY > 0.0f) p.velY = 0.0f;
                    p.settled = true;
                }
            }
        }
        else
        {
            if (! freeMode)
            {
                p.x = columnToWorldX (p.col) + p.spawnOffX;
                p.z = z + p.spawnOffZ;
            }
            if (p.settled)
            {
                if (p.y > p.targetY + p.spawnOffY)
                    p.y = p.targetY + p.spawnOffY;
            }
            else
            {
                p.y += p.velY * dtSeconds;
                const float capY = p.targetY + p.spawnOffY;
                if (p.y >= capY)
                {
                    p.y = capY;
                    p.velY = 0.0f;
                    p.settled = true;
                }
            }
        }

        // Kill particles that leave a reasonable volume in free mode
        if (freeMode)
        {
            if (p.y < -0.5f || p.y > 4.0f || std::abs (p.x) > 3.0f || std::abs (p.z) > 3.0f)
                p.alive = false;
        }
    }
}


void Spec3DParticleSystem::buildUnitMeshes()
{
    meshVerts.clear();
    meshIndices.clear();

    // --- Low-res UV sphere (8 segments x 6 rings) ---
    sphereVertexOffset = 0;
    sphereIndexOffset = 0;
    constexpr int segs = 8, rings = 6;
    for (int y = 0; y <= rings; ++y)
    {
        const float v = (float) y / (float) rings;
        const float phi = v * juce::MathConstants<float>::pi;
        const float sp = std::sin (phi), cp = std::cos (phi);
        for (int x = 0; x <= segs; ++x)
        {
            const float u = (float) x / (float) segs;
            const float th = u * juce::MathConstants<float>::twoPi;
            const float st = std::sin (th), ct = std::cos (th);
            MeshVert mv;
            mv.nx = st * sp; mv.ny = cp; mv.nz = ct * sp;
            mv.px = mv.nx * 0.5f; mv.py = mv.ny * 0.5f; mv.pz = mv.nz * 0.5f;
            meshVerts.push_back (mv);
        }
    }
    for (int y = 0; y < rings; ++y)
        for (int x = 0; x < segs; ++x)
        {
            const uint16_t i0 = (uint16_t) (y * (segs + 1) + x);
            const uint16_t i1 = (uint16_t) (i0 + 1);
            const uint16_t i2 = (uint16_t) (i0 + (segs + 1));
            const uint16_t i3 = (uint16_t) (i2 + 1);
            meshIndices.push_back (i0); meshIndices.push_back (i2); meshIndices.push_back (i1);
            meshIndices.push_back (i1); meshIndices.push_back (i2); meshIndices.push_back (i3);
        }
    sphereIndexCount = (int) meshIndices.size();

    // --- Unit cube ---
    cubeVertexOffset = (int) meshVerts.size();
    cubeIndexOffset = (int) meshIndices.size();
    const float h = 0.5f;
    const MeshVert cubeV[24] = {
        // +Z
        {-h,-h, h, 0,0,1},{ h,-h, h, 0,0,1},{ h, h, h, 0,0,1},{-h, h, h, 0,0,1},
        // -Z
        { h,-h,-h, 0,0,-1},{-h,-h,-h, 0,0,-1},{-h, h,-h, 0,0,-1},{ h, h,-h, 0,0,-1},
        // +X
        { h,-h, h, 1,0,0},{ h,-h,-h, 1,0,0},{ h, h,-h, 1,0,0},{ h, h, h, 1,0,0},
        // -X
        {-h,-h,-h,-1,0,0},{-h,-h, h,-1,0,0},{-h, h, h,-1,0,0},{-h, h,-h,-1,0,0},
        // +Y
        {-h, h, h, 0,1,0},{ h, h, h, 0,1,0},{ h, h,-h, 0,1,0},{-h, h,-h, 0,1,0},
        // -Y
        {-h,-h,-h, 0,-1,0},{ h,-h,-h, 0,-1,0},{ h,-h, h, 0,-1,0},{-h,-h, h, 0,-1,0},
    };
    for (auto& v : cubeV) meshVerts.push_back (v);
    const uint16_t base = (uint16_t) cubeVertexOffset;
    for (int f = 0; f < 6; ++f)
    {
        const uint16_t i = (uint16_t) (base + f * 4);
        meshIndices.push_back (i); meshIndices.push_back ((uint16_t)(i+1)); meshIndices.push_back ((uint16_t)(i+2));
        meshIndices.push_back (i); meshIndices.push_back ((uint16_t)(i+2)); meshIndices.push_back ((uint16_t)(i+3));
    }
    cubeIndexCount = (int) meshIndices.size() - cubeIndexOffset;
}

bool Spec3DParticleSystem::createMeshProgram (juce::OpenGLContext& context)
{
    // PBR path mirrors Spectrogram3D waterfall (GGX / Smith / Schlick metalness workflow).
    static constexpr const char* vs = R"(
        #version 150
        in vec3 meshPos;
        in vec3 meshNrm;
        in vec3 instPos;
        in float instScale;
        in vec4 instQuat;
        in vec4 instCol;
        in float instEm;
        out vec3 vNrm;
        out vec3 vViewDir;
        out vec4 vCol;
        out float vEm;
        uniform mat4 projectionMatrix;
        uniform mat4 viewMatrix;

        vec3 qrot(vec4 q, vec3 v) {
            return v + 2.0 * cross(q.xyz, cross(q.xyz, v) + q.w * v);
        }
        void main() {
            vec3 world = qrot(instQuat, meshPos * instScale) + instPos;
            vec4 viewPos = viewMatrix * vec4(world, 1.0);
            vViewDir = normalize(-viewPos.xyz);
            vNrm = normalize(mat3(viewMatrix) * qrot(instQuat, meshNrm));
            vCol = instCol;
            vEm = instEm;
            gl_Position = projectionMatrix * viewPos;
        }
    )";
    static constexpr const char* fs = R"(
        #version 150
        in vec3 vNrm;
        in vec3 vViewDir;
        in vec4 vCol;
        in float vEm;
        out vec4 fragColour;
        uniform float uEmissiveMode;     // 1 = unlit emissive only (legacy)
        uniform float uEmissiveStrength; // material emissive amount (additive when lit)
        uniform float uRoughness;
        uniform float uMetalness;
        uniform float uSpecular;
        uniform float uEnergyConserve;
        uniform float uLightingAmount;
        uniform vec3 uLightDirView;
        uniform vec3 uLightColour;

        // Same lobe as Spec3D waterfall colour shader.
        float distributionGGX (float NdotH, float rough)
        {
            float a = max (rough * rough, 0.0025);
            float a2 = a * a;
            float d = (NdotH * NdotH) * (a2 - 1.0) + 1.0;
            return a2 / (3.14159265 * d * d);
        }
        float geometrySchlickGGX (float NdotX, float rough)
        {
            float r = rough + 1.0;
            float k = (r * r) / 8.0;
            return NdotX / (NdotX * (1.0 - k) + k);
        }
        vec3 fresnelSchlick (float cosTheta, vec3 F0)
        {
            return F0 + (1.0 - F0) * pow (clamp (1.0 - cosTheta, 0.0, 1.0), 5.0);
        }

        void main() {
            vec3 albedo = max(vCol.rgb, vec3(0.0));
            float alpha = clamp(vCol.a, 0.0, 1.0);
            float emAmt = max(uEmissiveStrength, 0.0) * max(vEm, 0.0);

            // Legacy pure self-lit path (toggle).
            if (uEmissiveMode > 0.5) {
                fragColour = vec4(albedo * max(emAmt, 1.0e-4), alpha);
                return;
            }

            float baseAmt = clamp(uLightingAmount, 0.0, 1.0);
            vec3 N = normalize(vNrm);
            vec3 L = normalize(uLightDirView);
            vec3 V = normalize(vViewDir);
            vec3 H = normalize(L + V);
            float NdotL = max(dot(N, L), 0.0);
            float NdotV = max(dot(N, V), 1.0e-4);
            float NdotH = max(dot(N, H), 0.0);
            float VdotH = max(dot(V, H), 0.0);

            float rough = clamp(uRoughness, 0.04, 1.0);
            float metal = clamp(uMetalness, 0.0, 1.0);
            float specAmt = clamp(uSpecular, 0.0, 1.0);
            vec3 lightCol = max(uLightColour, vec3(0.0));
            vec3 F0 = mix(vec3(0.04), albedo, metal);
            float D = distributionGGX(NdotH, rough);
            float G = geometrySchlickGGX(NdotL, rough) * geometrySchlickGGX(NdotV, rough);
            vec3 F = fresnelSchlick(VdotH, F0);
            vec3 specular = (D * G * F) / max(4.0 * NdotV * max(NdotL, 1.0e-4), 1.0e-4);
            specular *= specAmt * lightCol;

            float wrap = NdotL * 0.72 + 0.28;
            vec3 kd = albedo * (1.0 - metal);
            if (uEnergyConserve > 0.5)
                kd *= max(vec3(0.0), vec3(1.0) - F);
            vec3 diffuse = kd * (0.22 + 0.78 * wrap) * lightCol;

            vec3 lit = mix(albedo, diffuse + specular, baseAmt);
            // Additive emissive (matrix-routable per particle via vEm).
            lit += albedo * emAmt;
            fragColour = vec4(lit, alpha);
        }
    )";

    meshProgram = std::make_unique<juce::OpenGLShaderProgram> (context);
    if (! meshProgram->addVertexShader (vs) || ! meshProgram->addFragmentShader (fs) || ! meshProgram->link())
    {
        DBG ("Spec3D particle mesh shader: " + meshProgram->getLastError());
        meshProgram.reset();
        return false;
    }
    aMeshPos = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "meshPos");
    aMeshNrm = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "meshNrm");
    aInstPos = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "instPos");
    aInstScale = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "instScale");
    aInstQuat = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "instQuat");
    aInstCol = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "instCol");
    aInstEm = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*meshProgram, "instEm");
    uMeshProj = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "projectionMatrix");
    uMeshView = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "viewMatrix");
    uMeshEmissiveMode = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uEmissiveMode");
    uMeshEmissiveStr = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uEmissiveStrength");
    uMeshRough = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uRoughness");
    uMeshMetal = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uMetalness");
    uMeshSpec = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uSpecular");
    uMeshEnergyConserve = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uEnergyConserve");
    uMeshLightAmt = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uLightingAmount");
    uMeshLightDir = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uLightDirView");
    uMeshLightCol = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*meshProgram, "uLightColour");
    return true;
}

bool Spec3DParticleSystem::createBillboardProgram (juce::OpenGLContext& context)
{
    billboardProgram = std::make_unique<juce::OpenGLShaderProgram> (context);
    if (! billboardProgram->addVertexShader (kParticleVS)
        || ! billboardProgram->addFragmentShader (kParticleFS)
        || ! billboardProgram->link())
    {
        billboardProgram.reset();
        return false;
    }
    aPos = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*billboardProgram, "centre");
    aCol = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*billboardProgram, "colour");
    aCorner = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*billboardProgram, "corner");
    aSize = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*billboardProgram, "pSize");
    aEm = std::make_unique<juce::OpenGLShaderProgram::Attribute> (*billboardProgram, "pEm");
    uProj = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "projectionMatrix");
    uView = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "viewMatrix");
    uCamRight = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uCamRight");
    uCamUp = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uCamUp");
    uSize = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uSize");
    uEmissiveMode = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uEmissiveMode");
    uEmissiveStrength = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uEmissiveStrength");
    uRoughness = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uRoughness");
    uMetalness = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uMetalness");
    uSpecular = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uSpecular");
    uEnergyConserve = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uEnergyConserve");
    uLightingAmount = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uLightingAmount");
    uLightDirView = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uLightDirView");
    uLightColour = std::make_unique<juce::OpenGLShaderProgram::Uniform> (*billboardProgram, "uLightColour");
    return true;
}

void Spec3DParticleSystem::ensureGl (juce::OpenGLContext& context)
{
    if (glReady) return;
    buildUnitMeshes();
    if (! createMeshProgram (context) || ! createBillboardProgram (context))
        return;

    using namespace juce::gl;
    glGenBuffers (1, &meshVbo);
    glGenBuffers (1, &meshIbo);
    glGenBuffers (1, &instanceVbo);
    glGenBuffers (1, &billboardVbo);

    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (meshVerts.size() * sizeof (MeshVert)), meshVerts.data(), GL_STATIC_DRAW);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);
    glBufferData (GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr) (meshIndices.size() * sizeof (uint16_t)), meshIndices.data(), GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);

    glReady = (meshVbo != 0 && meshIbo != 0 && instanceVbo != 0 && billboardVbo != 0);
}

void Spec3DParticleSystem::releaseGl()
{
    using namespace juce::gl;
    if (meshVbo) { glDeleteBuffers (1, &meshVbo); meshVbo = 0; }
    if (meshIbo) { glDeleteBuffers (1, &meshIbo); meshIbo = 0; }
    if (instanceVbo) { glDeleteBuffers (1, &instanceVbo); instanceVbo = 0; }
    if (billboardVbo) { glDeleteBuffers (1, &billboardVbo); billboardVbo = 0; }
    meshProgram.reset(); billboardProgram.reset();
    aMeshPos.reset(); aMeshNrm.reset();
    aInstPos.reset(); aInstScale.reset(); aInstQuat.reset(); aInstCol.reset(); aInstEm.reset();
    uMeshProj.reset(); uMeshView.reset(); uMeshEmissiveMode.reset(); uMeshEmissiveStr.reset();
    uMeshRough.reset(); uMeshMetal.reset(); uMeshSpec.reset(); uMeshEnergyConserve.reset();
    uMeshLightAmt.reset(); uMeshLightDir.reset(); uMeshLightCol.reset();
    aPos.reset(); aCol.reset(); aCorner.reset(); aSize.reset(); aEm.reset();
    uProj.reset(); uView.reset(); uCamRight.reset(); uCamUp.reset(); uSize.reset();
    uEmissiveMode.reset(); uEmissiveStrength.reset(); uRoughness.reset(); uMetalness.reset();
    uSpecular.reset(); uEnergyConserve.reset();
    uLightingAmount.reset(); uLightDirView.reset(); uLightColour.reset();
    glReady = false;
}

void Spec3DParticleSystem::drawInstancedMeshes (const juce::Matrix3D<float>& projection,
                                                  const juce::Matrix3D<float>& view)
{
    using namespace juce::gl;
    if (meshProgram == nullptr || meshVbo == 0 || instanceVbo == 0) return;

    instances.clear();
    for (const auto& p : pool)
    {
        if (! p.alive) continue;
        GpuInstance inst;
        inst.px = p.x; inst.py = p.y; inst.pz = p.z;
        inst.sx = p.sizeScale * juce::jmax (1.0e-4f, owner.particleSize) * 2.0f;
        eulerToQuat (p.rotX, p.rotY, p.rotZ, inst.qx, inst.qy, inst.qz, inst.qw);
        inst.r = p.r; inst.g = p.g; inst.b = p.b; inst.a = p.alpha;
        inst.em = p.emissiveScale;
        instances.push_back (inst);
    }
    if (instances.empty()) return;

    const bool useCube = owner.particleMeshShape == ParticleMeshShape::cube;
    const int indexCount = useCube ? cubeIndexCount : sphereIndexCount;
    const int indexOff = useCube ? cubeIndexOffset : sphereIndexOffset;
    if (indexCount <= 0) return;

    // Light dir in view space
    const float az = juce::degreesToRadians (owner.lightAzimuthDeg);
    const float el = juce::degreesToRadians (juce::jlimit (5.0f, 89.0f, owner.lightElevationDeg));
    juce::Vector3D<float> lightWorld { std::cos (el) * std::sin (az), std::sin (el), std::cos (el) * std::cos (az) };
    juce::Vector3D<float> right, up, forward;
    owner.cameraBasis (right, up, forward);
    const float lx = lightWorld.x * right.x + lightWorld.y * right.y + lightWorld.z * right.z;
    const float ly = lightWorld.x * up.x + lightWorld.y * up.y + lightWorld.z * up.z;
    const float lz = -(lightWorld.x * forward.x + lightWorld.y * forward.y + lightWorld.z * forward.z);
    const float llen = juce::jmax (1.0e-5f, std::sqrt (lx*lx + ly*ly + lz*lz));

    meshProgram->use();
    if (uMeshProj) uMeshProj->setMatrix4 (projection.mat, 1, false);
    if (uMeshView) uMeshView->setMatrix4 (view.mat, 1, false);
    // Unlit emissive-only when toggle on; otherwise lit PBR + additive emissive.
    if (uMeshEmissiveMode) uMeshEmissiveMode->set (owner.particleEmissiveEnabled ? 1.0f : 0.0f);
    if (uMeshEmissiveStr) uMeshEmissiveStr->set (owner.particleEmissiveStrength);
    if (uMeshRough) uMeshRough->set (owner.particleRoughness);
    if (uMeshMetal) uMeshMetal->set (owner.particleMetalness);
    if (uMeshSpec) uMeshSpec->set (owner.particleSpecular);
    if (uMeshEnergyConserve) uMeshEnergyConserve->set (owner.energyConservingEnabled ? 1.0f : 0.0f);
    if (uMeshLightAmt) uMeshLightAmt->set (owner.lightingEnabled ? owner.lightingAmount : 0.0f);
    if (uMeshLightDir) uMeshLightDir->set (lx/llen, ly/llen, lz/llen);
    if (uMeshLightCol) uMeshLightCol->set (owner.lightColour.getFloatRed(), owner.lightColour.getFloatGreen(), owner.lightColour.getFloatBlue());

    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);
    const GLsizei meshStride = (GLsizei) sizeof (MeshVert);
    if (aMeshPos && aMeshPos->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aMeshPos->attributeID);
        glVertexAttribPointer ((GLuint) aMeshPos->attributeID, 3, GL_FLOAT, GL_FALSE, meshStride, (void*) offsetof (MeshVert, px));
        glVertexAttribDivisor ((GLuint) aMeshPos->attributeID, 0);
    }
    if (aMeshNrm && aMeshNrm->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aMeshNrm->attributeID);
        glVertexAttribPointer ((GLuint) aMeshNrm->attributeID, 3, GL_FLOAT, GL_FALSE, meshStride, (void*) offsetof (MeshVert, nx));
        glVertexAttribDivisor ((GLuint) aMeshNrm->attributeID, 0);
    }

    glBindBuffer (GL_ARRAY_BUFFER, instanceVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr) (instances.size() * sizeof (GpuInstance)), instances.data(), GL_STREAM_DRAW);
    const GLsizei iStride = (GLsizei) sizeof (GpuInstance);
    auto enableInst = [&] (auto& attr, int n, size_t off)
    {
        if (attr && attr->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) attr->attributeID);
            glVertexAttribPointer ((GLuint) attr->attributeID, n, GL_FLOAT, GL_FALSE, iStride, (void*) off);
            glVertexAttribDivisor ((GLuint) attr->attributeID, 1);
        }
    };
    enableInst (aInstPos, 3, offsetof (GpuInstance, px));
    enableInst (aInstScale, 1, offsetof (GpuInstance, sx));
    enableInst (aInstQuat, 4, offsetof (GpuInstance, qx));
    enableInst (aInstCol, 4, offsetof (GpuInstance, r));
    enableInst (aInstEm, 1, offsetof (GpuInstance, em));

    glEnable (GL_DEPTH_TEST);
    glDepthMask (GL_TRUE);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_CULL_FACE);
    glCullFace (GL_BACK);

    glDrawElementsInstanced (GL_TRIANGLES, indexCount, GL_UNSIGNED_SHORT,
                             (void*) (sizeof (uint16_t) * (size_t) indexOff),
                             (GLsizei) instances.size());

    auto disable = [&] (auto& attr)
    {
        if (attr && attr->attributeID >= 0)
        {
            glVertexAttribDivisor ((GLuint) attr->attributeID, 0);
            glDisableVertexAttribArray ((GLuint) attr->attributeID);
        }
    };
    disable (aMeshPos); disable (aMeshNrm);
    disable (aInstPos); disable (aInstScale); disable (aInstQuat); disable (aInstCol); disable (aInstEm);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, 0);
    glDisable (GL_CULL_FACE);
    glDisable (GL_BLEND);
}

void Spec3DParticleSystem::drawBillboards (const juce::Matrix3D<float>& projection,
                                           const juce::Matrix3D<float>& view,
                                           juce::Vector3D<float> camRight,
                                           juce::Vector3D<float> camUp)
{
    using namespace juce::gl;
    if (billboardProgram == nullptr || billboardVbo == 0) return;

    gpuVerts.clear();
    const float corners[6][2] = {
        { -1,-1 },{ 1,-1 },{ 1,1 },{ -1,-1 },{ 1,1 },{ -1,1 }
    };
    for (const auto& p : pool)
    {
        if (! p.alive) continue;
        for (const auto& c : corners)
        {
            GpuBillboardVert v;
            v.px = p.x; v.py = p.y; v.pz = p.z;
            v.r = p.r; v.g = p.g; v.b = p.b; v.a = p.alpha;
            v.cx = c[0]; v.cy = c[1];
            v.size = p.sizeScale;
            v.em = p.emissiveScale;
            gpuVerts.push_back (v);
        }
    }
    if (gpuVerts.empty()) return;

    const float az = juce::degreesToRadians (owner.lightAzimuthDeg);
    const float el = juce::degreesToRadians (juce::jlimit (5.0f, 89.0f, owner.lightElevationDeg));
    juce::Vector3D<float> lightWorld { std::cos (el) * std::sin (az), std::sin (el), std::cos (el) * std::cos (az) };
    juce::Vector3D<float> right, up, forward;
    owner.cameraBasis (right, up, forward);
    const float lx = lightWorld.x * right.x + lightWorld.y * right.y + lightWorld.z * right.z;
    const float ly = lightWorld.x * camUp.x + lightWorld.y * camUp.y + lightWorld.z * camUp.z;
    const float lz = -(lightWorld.x * forward.x + lightWorld.y * forward.y + lightWorld.z * forward.z);
    const float llen = juce::jmax (1.0e-5f, std::sqrt (lx*lx+ly*ly+lz*lz));

    billboardProgram->use();
    if (uProj) uProj->setMatrix4 (projection.mat, 1, false);
    if (uView) uView->setMatrix4 (view.mat, 1, false);
    if (uCamRight) uCamRight->set (camRight.x, camRight.y, camRight.z);
    if (uCamUp) uCamUp->set (camUp.x, camUp.y, camUp.z);
    if (uSize) uSize->set (owner.particleSize != 0.0f ? owner.particleSize : 1.0e-4f);
    if (uEmissiveMode) uEmissiveMode->set (owner.particleEmissiveEnabled ? 1.0f : 0.0f);
    if (uEmissiveStrength) uEmissiveStrength->set (owner.particleEmissiveStrength);
    if (uRoughness) uRoughness->set (owner.particleRoughness);
    if (uMetalness) uMetalness->set (owner.particleMetalness);
    if (uSpecular) uSpecular->set (owner.particleSpecular);
    if (uEnergyConserve) uEnergyConserve->set (owner.energyConservingEnabled ? 1.0f : 0.0f);
    if (uLightingAmount) uLightingAmount->set (owner.lightingEnabled ? owner.lightingAmount : 0.0f);
    if (uLightDirView) uLightDirView->set (lx/llen, ly/llen, lz/llen);
    if (uLightColour) uLightColour->set (owner.lightColour.getFloatRed(), owner.lightColour.getFloatGreen(), owner.lightColour.getFloatBlue());

    glBindBuffer (GL_ARRAY_BUFFER, billboardVbo);
    glBufferData (GL_ARRAY_BUFFER, (GLsizeiptr)(gpuVerts.size()*sizeof(GpuBillboardVert)), gpuVerts.data(), GL_STREAM_DRAW);
    const GLsizei stride = (GLsizei) sizeof (GpuBillboardVert);
    auto en = [&](auto& a, int n, size_t off) {
        if (a && a->attributeID >= 0) {
            glEnableVertexAttribArray ((GLuint)a->attributeID);
            glVertexAttribPointer ((GLuint)a->attributeID, n, GL_FLOAT, GL_FALSE, stride, (void*)off);
        }
    };
    en (aPos, 3, offsetof (GpuBillboardVert, px));
    en (aCol, 4, offsetof (GpuBillboardVert, r));
    en (aCorner, 2, offsetof (GpuBillboardVert, cx));
    en (aSize, 1, offsetof (GpuBillboardVert, size));
    en (aEm, 1, offsetof (GpuBillboardVert, em));
    glEnable (GL_BLEND); glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable (GL_DEPTH_TEST); glDepthMask (GL_TRUE);
    glDrawArrays (GL_TRIANGLES, 0, (GLsizei) gpuVerts.size());
    auto dis = [&](auto& a) { if (a && a->attributeID >= 0) glDisableVertexAttribArray ((GLuint)a->attributeID); };
    dis (aPos); dis (aCol); dis (aCorner); dis (aSize); dis (aEm);
    glBindBuffer (GL_ARRAY_BUFFER, 0); glDisable (GL_BLEND);
}

void Spec3DParticleSystem::draw (const juce::Matrix3D<float>& projection,
                                 const juce::Matrix3D<float>& view,
                                 juce::Vector3D<float> camRight,
                                 juce::Vector3D<float> camUp)
{
    if (! glReady) return;
    if (owner.particleMeshShape == ParticleMeshShape::billboard)
        drawBillboards (projection, view, camRight, camUp);
    else
        drawInstancedMeshes (projection, view);
}
