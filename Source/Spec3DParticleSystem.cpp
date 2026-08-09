#include "Spec3DParticleSystem.h"
#include "Spectrogram3DComponent.h"
#include "SpectrogramComponent.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <new>

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

void Spec3DParticleSystem::setMaxAliveBudget (int maxAlive) noexcept
{
    const juce::ScopedLock sl (simLock);

    const int newBudget = juce::jlimit (kMinMaxAlive, kHardCap, maxAlive);
    const int oldBudget = maxAliveBudget;
    maxAliveBudget = newBudget;

    // Raising Max must NEVER allocate or touch the pool / GPU buffers here.
    // (Doing so from the UI thread while GL is integrating caused 70k-set crashes.)
    // Growth happens lazily in ensurePool() during update/spawn only.
    if (newBudget >= oldBudget && (int) pool.size() <= newBudget)
        return;

    // Shrinking Max: cull then trim capacity. Keep it exception-safe.
    try
    {
        // Cull in chunks until under budget (one enforceAliveBudget only does 512).
        for (int guard = 0; guard < 64 && aliveCount > maxAliveBudget; ++guard)
            enforceAliveBudget();

        if ((int) pool.size() > maxAliveBudget)
        {
            // Kill anything still marked alive past the new end (indices will be destroyed).
            for (int i = maxAliveBudget; i < (int) pool.size(); ++i)
            {
                if (pool[(size_t) i].alive)
                    markDead (i);
            }
            pool.resize ((size_t) maxAliveBudget);
            if (nextWrite >= maxAliveBudget)
                nextWrite = 0;
            rebuildFreeList();
            recountAlive();
            // Pool indices changed shape — GPU SSBO layout must re-seed.
            gpuPoolResident = false;
            gpuResidentCapacity = 0;
            gpuSpawnPatches.clear();
            gpuFieldPatches.clear();
        }
    }
    catch (const std::bad_alloc&)
    {
        maxAliveBudget = juce::jmax (kMinMaxAlive, (int) pool.size());
        rebuildFreeList();
        recountAlive();
    }
    catch (...)
    {
        // Never propagate out of a noexcept budget setter into the host.
        maxAliveBudget = juce::jmax (kMinMaxAlive, juce::jmin (maxAliveBudget, (int) pool.size()));
    }
}

void Spec3DParticleSystem::ensurePool()
{
    // Caller must hold simLock (update / allocSlot paths).
    const int bins = juce::jmax (1, owner.meshH);
    if ((int) emitAccum.size() != bins)
        emitAccum.assign ((size_t) bins, 0.0f);

    const int budget = juce::jmin (kHardCap, juce::jmax (kMinMaxAlive, maxAliveBudget));
    // Grow toward Max as spawn needs slots. Double (min +8k) so we do not reallocate
    // every few thousand births — each grow was a multi-ms hitch (CPU + GPU SSBO).
    const int headroom = juce::jlimit (2048, 32768, juce::jmax (2048, budget / 8));
    int want = juce::jmax (kMinMaxAlive, aliveCount + headroom);
    if (want > (int) pool.size())
    {
        const int doubled = juce::jmax (want, juce::jmax ((int) pool.size() * 2, (int) pool.size() + 8192));
        want = juce::jmin (budget, doubled);
    }
    else
        want = (int) pool.size();

    if ((int) pool.size() < want)
    {
        const int old = (int) pool.size();
        try
        {
            pool.resize ((size_t) want); // default-construct new Particle slots
            freeList.reserve ((size_t) want);
            for (int i = old; i < want; ++i)
                freeList.push_back (i);
        }
        catch (const std::bad_alloc&)
        {
            if ((int) pool.size() > old)
            {
                try { pool.resize ((size_t) old); }
                catch (...) { /* keep whatever we have */ }
            }
            // Cap budget to allocated pool so spawn stops cleanly instead of thrashing.
            maxAliveBudget = juce::jmax (kMinMaxAlive, (int) pool.size());
            rebuildFreeList();
            recountAlive();
        }
        catch (...)
        {
            maxAliveBudget = juce::jmax (kMinMaxAlive, (int) pool.size());
        }
    }
    else if ((int) pool.size() > budget)
    {
        // Should be rare (budget shrink goes through setMaxAliveBudget).
        try
        {
            for (int i = budget; i < (int) pool.size(); ++i)
                if (pool[(size_t) i].alive)
                    markDead (i);
            pool.resize ((size_t) budget);
            if (nextWrite >= budget)
                nextWrite = 0;
            rebuildFreeList();
            recountAlive();
            gpuPoolResident = false;
            gpuResidentCapacity = 0;
        }
        catch (...)
        {
            maxAliveBudget = juce::jmax (kMinMaxAlive, (int) pool.size());
        }
    }
}

void Spec3DParticleSystem::recountAlive() noexcept
{
    int n = 0;
    for (const auto& p : pool)
        if (p.alive)
            ++n;
    aliveCount = n;
    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();
}

void Spec3DParticleSystem::rebuildAliveList() noexcept
{
    aliveList.clear();
    aliveList.reserve ((size_t) juce::jmax (aliveCount, 64));
    for (int i = 0; i < (int) pool.size(); ++i)
    {
        auto& p = pool[(size_t) i];
        if (! p.alive)
        {
            p.aliveListSlot = -1;
            continue;
        }
        p.aliveListSlot = (int) aliveList.size();
        aliveList.push_back (i);
    }
    aliveCount = (int) aliveList.size();
}

void Spec3DParticleSystem::registerAlive (int index) noexcept
{
    if (index < 0 || index >= (int) pool.size())
        return;
    auto& p = pool[(size_t) index];
    if (! p.alive || p.aliveListSlot >= 0)
        return;
    p.aliveListSlot = (int) aliveList.size();
    aliveList.push_back (index);
}

void Spec3DParticleSystem::rebuildFreeList() noexcept
{
    freeList.clear();
    freeList.reserve (pool.size());
    for (int i = 0; i < (int) pool.size(); ++i)
        if (! pool[(size_t) i].alive)
            freeList.push_back (i);
    rebuildAliveList();
}

void Spec3DParticleSystem::markDead (int index) noexcept
{
    if (index < 0 || index >= (int) pool.size())
        return;
    auto& p = pool[(size_t) index];
    if (! p.alive)
        return;
    p.alive = false;
    --aliveCount;

    // O(1) swap-remove from dense alive list.
    const int slot = p.aliveListSlot;
    if (slot >= 0 && slot < (int) aliveList.size())
    {
        const int last = (int) aliveList.size() - 1;
        if (slot != last)
        {
            const int moved = aliveList[(size_t) last];
            aliveList[(size_t) slot] = moved;
            if (moved >= 0 && moved < (int) pool.size())
                pool[(size_t) moved].aliveListSlot = slot;
        }
        aliveList.pop_back();
    }
    p.aliveListSlot = -1;
    freeList.push_back (index);

    // Free-visualizer GPU keeps drawing until the SSBO slot is cleared.
    if (owner.particleGpuSimEnabled && gpuPoolResident)
        queueGpuKill (index);
}

void Spec3DParticleSystem::enforceAliveBudget() noexcept
{
    lastCulledCount = 0;
    if (pool.empty())
    {
        aliveCount = 0;
        aliveList.clear();
        return;
    }

    // Trust dense list; only resync if counts drift (corruption / partial rebuild).
    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();

    int need = aliveCount - maxAliveBudget;
    if (need <= 0)
        return;

    // Prefer killing settled / low history-col first (trail fade), then oldest age.
    // Cap cull work per call so a spike never freezes the UI thread.
    constexpr int kMaxCullPerCall = 512;
    const int toKill = juce::jmin (need, kMaxCullPerCall);

    for (int k = 0; k < toKill; ++k)
    {
        int best = -1;
        int bestCol = 0x7fffffff;
        float bestAge = -1.0f;
        bool bestSettled = false;

        for (int ai = 0; ai < (int) aliveList.size(); ++ai)
        {
            const int i = aliveList[(size_t) ai];
            if (i < 0 || i >= (int) pool.size())
                continue;
            const auto& p = pool[(size_t) i];
            if (! p.alive)
                continue;

            // Priority: settled before un-settled; then older history (low col); then age.
            const bool better =
                (p.settled && ! bestSettled)
                || (p.settled == bestSettled
                    && (p.col < bestCol || (p.col == bestCol && p.age > bestAge)));

            if (best < 0 || better)
            {
                best = i;
                bestCol = p.col;
                bestAge = p.age;
                bestSettled = p.settled;
            }
        }

        if (best < 0)
            break;

        markDead (best);
        ++lastCulledCount;
    }
}

void Spec3DParticleSystem::clear()
{
    const juce::ScopedLock sl (simLock);
    for (auto& p : pool)
    {
        p.alive = false;
        p.aliveListSlot = -1;
    }
    nextWrite = 0;
    emitGlobal = 0.0f;
    amplitudeSmooth = 0.0f;
    simTime = 0.0f;
    aliveCount = 0;
    lastSpawnedCount = 0;
    lastCulledCount = 0;
    aliveList.clear();
    gpuPoolResident = false;
    gpuSpawnPatches.clear();
    gpuFieldPatches.clear();
    pendingHistoryScrollX = 0.0f;
    rebuildFreeList();
    slotEnv.fill (0.0f);
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
        for (int c = 0; c < 3; ++c)
            randomSmoothV[i][c] = 0.0f;
    std::fill (emitAccum.begin(), emitAccum.end(), 0.0f);
    pendingIntegrateDt = 0.0f;
    gpuInstancesValid = false;
}

void Spec3DParticleSystem::resetEmissionAccumulators() noexcept
{
    const juce::ScopedLock sl (simLock);
    emitGlobal = 0.0f;
    std::fill (emitAccum.begin(), emitAccum.end(), 0.0f);
}

void Spec3DParticleSystem::scrollHistory (int numCols)
{
    if (numCols <= 0 || pool.empty())
        return;

    // Free visualizer / geometric emitters: particles do not track waterfall columns.
    if (owner.particleBindingMode == ParticleBindingMode::freeVisualizer
        || owner.particleEmitterType != ParticleEmitterType::spectrogram)
        return;

    const juce::ScopedLock sl (simLock);

    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();

    // Uniform world-X step for one column: x = col/(W-1)*2-1 → Δcol=-1 ⇒ Δx = -2/(W-1).
    const float scrollDx = (owner.meshW > 1)
        ? (-(float) numCols * 2.0f / (float) (owner.meshW - 1))
        : 0.0f;

    if (owner.particleGpuSimEnabled && gpuComputeReady)
        pendingHistoryScrollX += scrollDx;

    for (int ai = (int) aliveList.size() - 1; ai >= 0; --ai)
    {
        const int i = aliveList[(size_t) ai];
        if (i < 0 || i >= (int) pool.size())
            continue;
        auto& p = pool[(size_t) i];
        if (! p.alive)
            continue;
        p.col -= numCols;
        if (p.col < 0)
        {
            markDead (i);
            continue;
        }
        p.x = columnToWorldX (p.col) + p.spawnOffX;
        // Y was baked at spawn — never touch it on scroll.
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
    // Hard stop at Max — never recycle mid-spawn (O(n) scan at 70k hitch/crash risk).
    // enforceAliveBudget() + lifespan handle turnover; spawn simply no-ops at cap.
    if (aliveCount >= maxAliveBudget)
        return -1;

    // O(1) free-list path (required for 100k+).
    while (! freeList.empty())
    {
        const int i = freeList.back();
        freeList.pop_back();
        if (i >= 0 && i < (int) pool.size() && ! pool[(size_t) i].alive)
            return i;
    }

    // Grow toward budget if free-list empty but slots missing.
    if ((int) pool.size() < maxAliveBudget)
    {
        ensurePool();
        // ensurePool may clamp budget on OOM
        if (aliveCount >= maxAliveBudget)
            return -1;
        while (! freeList.empty())
        {
            const int i = freeList.back();
            freeList.pop_back();
            if (i >= 0 && i < (int) pool.size() && ! pool[(size_t) i].alive)
                return i;
        }
    }

    // Consistency repair (should be rare).
    if (aliveCount < (int) pool.size())
    {
        rebuildFreeList();
        if (! freeList.empty())
        {
            const int i = freeList.back();
            freeList.pop_back();
            if (i >= 0 && i < (int) pool.size() && ! pool[(size_t) i].alive)
                return i;
        }
    }

    return -1;
}

float Spec3DParticleSystem::sampleColumn (int bin, int col,
                                          float& outR, float& outG, float& outB, float& outZ,
                                          float& outDb01, float& outFreq01,
                                          bool wantColour) const
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

    // Colour LUT is the expensive part — skip when only height / trail XZ is needed.
    if (wantColour && owner.dataSource != nullptr)
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
        // Full birth velocity as Vec3. Prefer particle state (post-slider / mid-mod);
        // fall back to Look init-vel sliders for system-stage rows (no particle yet).
        if (p != nullptr)
            return { p->velX, p->velY, p->velZ };
        return { owner.particleInitVelX, owner.particleInitVelY, owner.particleInitVelZ };
    }

    if (src == ParticleModSource::initVelX
        || src == ParticleModSource::initVelY
        || src == ParticleModSource::initVelZ)
    {
        float axis = 0.0f;
        if (p != nullptr)
        {
            axis = src == ParticleModSource::initVelX ? p->velX
                 : (src == ParticleModSource::initVelY ? p->velY : p->velZ);
        }
        else
        {
            axis = src == ParticleModSource::initVelX ? owner.particleInitVelX
                 : (src == ParticleModSource::initVelY ? owner.particleInitVelY
                                                       : owner.particleInitVelZ);
        }
        return { axis, axis, axis };
    }

    float s = 0.0f;
    switch (src)
    {
        case ParticleModSource::amplitude: s = frame.amplitude; break;
        case ParticleModSource::binDb:
            // FFT strength 0–1 = colour-ramp intensity at this particle's freq bin.
            // Prefer live mesh sample (scrolls with trail col); fall back to birth bake.
            if (p != nullptr)
            {
                if (owner.meshW >= 2 && owner.meshH >= 2 && ! owner.meshDb.empty()
                    && owner.lastBrightness >= 0.0f)
                {
                    const int col = juce::jlimit (0, owner.meshW - 1, p->col);
                    const float binF = juce::jlimit (0.0f, (float) juce::jmax (0, owner.meshH - 1), p->binF);
                    const int b0 = juce::jlimit (0, owner.meshH - 1, (int) std::floor (binF));
                    const int b1 = juce::jmin (owner.meshH - 1, b0 + 1);
                    const float fr = binF - (float) b0;
                    const float denom = juce::jmax (1.0f, owner.lastMaxDb - owner.lastMinDb);
                    const float db0 = owner.meshDb[(size_t) col * (size_t) owner.meshH + (size_t) b0];
                    const float db1 = owner.meshDb[(size_t) col * (size_t) owner.meshH + (size_t) b1];
                    const float n0 = juce::jlimit (0.0f, 1.0f, (db0 - owner.lastMinDb) / denom);
                    const float n1 = juce::jlimit (0.0f, 1.0f, (db1 - owner.lastMinDb) / denom);
                    s = n0 * (1.0f - fr) + n1 * fr;
                }
                else
                    s = p->binDb01;
            }
            break;
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
        // Speed magnitude (world units/s) for Float dests.
        const auto v = sampleSourceVec (src, constant, p, frame);
        return std::sqrt (v.x * v.x + v.y * v.y + v.z * v.z);
    }
    // initVelX/Y/Z and other scalars: .x is the channel.
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
            case ParticleModDest::initVelY:
                // Float dest — Y only (riseSpeed is legacy name for Init vel Y).
                velY = applyOp (velY, src, slot.op, slot.amount);
                syncVel();
                break;
            case ParticleModDest::initVelX:
                velX = applyOp (velX, src, slot.op, slot.amount);
                syncVel();
                break;
            case ParticleModDest::initVelZ:
                velZ = applyOp (velZ, src, slot.op, slot.amount);
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
                // Init vel (Vec3) / Random Vec3 → full independent axes.
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
            || slot.dest == ParticleModDest::initVelX
            || slot.dest == ParticleModDest::initVelY
            || slot.dest == ParticleModDest::initVelZ
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

    // Snapshot stack so UI/GL can update forces without racing this loop.
    const auto stackCopy = owner.particleForceStack;

    // Evaluate ordered stack (module order = evaluation order).
    for (const auto& mod : stackCopy)
    {
        if (! mod.enabled)
            continue;

        auto p0 = std::isfinite (mod.p[0]) ? mod.p[0] : 0.0f;
        auto p1 = std::isfinite (mod.p[1]) ? mod.p[1] : 0.0f;
        auto p2 = std::isfinite (mod.p[2]) ? mod.p[2] : 0.0f;

        switch (mod.type)
        {
            case ParticleForceType::gravity:
            {
                const float g = p0 * scales.gravity + scales.gravityAdd;
                p.velY += g * dt;
                break;
            }
            case ParticleForceType::drag:
            {
                const float d = p0 * scales.drag + scales.dragAdd;
                if (std::abs (d) > 1.0e-8f)
                {
                    // Cap to avoid exp overflow / denorm thrash at extreme typed values.
                    const float damp = std::exp (-juce::jmin (40.0f, std::abs (d) * dt));
                    p.velX *= damp; p.velY *= damp; p.velZ *= damp;
                }
                break;
            }
            case ParticleForceType::wind:
            {
                p.velX += (p0 * scales.windX + scales.windAddX) * dt;
                p.velY += (p1 * scales.windY + scales.windAddY) * dt;
                p.velZ += (p2 * scales.windZ + scales.windAddZ) * dt;
                break;
            }
            case ParticleForceType::curlNoise:
            {
                float str = p0 * scales.curlStrength + scales.curlStrAdd;
                float sc = p1 * scales.curlScale + scales.curlScaleAdd;
                float spd = p2 * scales.curlSpeed + scales.curlSpeedAdd;
                if (std::abs (str) > 1.0e-8f)
                {
                    sc = juce::jlimit (1.0e-4f, 8.0f, std::abs (sc) < 1.0e-6f ? 1.0e-4f : sc);
                    const float t = simTime * spd;
                    // Bound sample domain so hashNoise int coords stay sane.
                    const float sx = juce::jlimit (-50.0f, 50.0f, p.x * sc + t);
                    const float sy = juce::jlimit (-50.0f, 50.0f, p.y * sc);
                    const float sz = juce::jlimit (-50.0f, 50.0f, p.z * sc + t * 0.7f);
                    const auto c = curlNoise (sx, sy, sz);
                    p.velX += c.x * str * dt;
                    p.velY += c.y * str * dt;
                    p.velZ += c.z * str * dt;
                }
                break;
            }
            case ParticleForceType::turbulence:
            {
                const float str = p0 * scales.turbulence + scales.turbAdd;
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
                const float rateX = p0;
                const float rateY = mod.linkAxes ? p0 : p1;
                const float rateZ = mod.linkAxes ? p0 : p2;
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

    // Soft clamp velocity so runaway forces can't NaN positions.
    constexpr float kMaxVel = 50.0f;
    p.velX = juce::jlimit (-kMaxVel, kMaxVel, p.velX);
    p.velY = juce::jlimit (-kMaxVel, kMaxVel, p.velY);
    p.velZ = juce::jlimit (-kMaxVel, kMaxVel, p.velZ);

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

    // Continuous: random continuous frequency coordinate (0..bins-1).
    // Mix energy-weighted CDF with uniform so it never collapses to peak-only "columns"
    // (which looked identical to Slice).
    const int col = owner.meshW - 1;
    const float uMax = (float) juce::jmax (0, bins - 1);

    // ~35% pure uniform scatter along the axis.
    if (rng.nextFloat() < 0.35f || owner.meshDb.empty() || col < 0 || owner.lastBrightness < 0.0f)
        return rng.nextFloat() * uMax;

    const float denom = juce::jmax (1.0f, owner.lastMaxDb - owner.lastMinDb);
    if ((int) playheadWeights.size() != bins)
        playheadWeights.assign ((size_t) bins, 0.0f);

    float sum = 0.0f;
    for (int b = 0; b < bins; ++b)
    {
        const float db = owner.meshDb[(size_t) col * (size_t) bins + (size_t) b];
        const float n = juce::jlimit (0.0f, 1.0f, (db - owner.lastMinDb) / denom);
        // Mild peak bias + strong floor → continuous cloud, not hard bands.
        const float w = n * n + 0.15f;
        sum += w;
        playheadWeights[(size_t) b] = sum;
    }
    if (sum <= 1.0e-8f)
        return rng.nextFloat() * uMax;

    const float pick = rng.nextFloat() * sum;
    int lo = 0, hi = bins - 1;
    while (lo < hi)
    {
        const int mid = (lo + hi) >> 1;
        if (playheadWeights[(size_t) mid] < pick)
            lo = mid + 1;
        else
            hi = mid;
    }
    const int b = lo;
    const float cdfLo = b > 0 ? playheadWeights[(size_t) b - 1] : 0.0f;
    const float cdfHi = playheadWeights[(size_t) b];
    const float span = juce::jmax (1.0e-8f, cdfHi - cdfLo);
    const float frac = juce::jlimit (0.0f, 0.999f, (pick - cdfLo) / span);
    return juce::jlimit (0.0f, uMax, (float) b + frac);
}

void Spec3DParticleSystem::pickHistorySpawn (int& outCol, float& outBinF) noexcept
{
    outCol = juce::jmax (0, owner.meshW - 1);
    outBinF = 0.0f;
    const int cols = owner.meshW;
    const int bins = owner.meshH;
    if (cols < 1 || bins < 1 || owner.meshDb.empty() || owner.lastBrightness < 0.0f)
    {
        outBinF = pickPlayheadBinF();
        return;
    }

    // Energy-weighted pick over the full W×H field (history surface emitter).
    const float denom = juce::jmax (1.0f, owner.lastMaxDb - owner.lastMinDb);
    const int n = cols * bins;
    if ((int) playheadWeights.size() < n)
        playheadWeights.assign ((size_t) n, 0.0f);

    float sum = 0.0f;
    for (int c = 0; c < cols; ++c)
    {
        for (int b = 0; b < bins; ++b)
        {
            const float db = owner.meshDb[(size_t) c * (size_t) bins + (size_t) b];
            const float nn = juce::jlimit (0.0f, 1.0f, (db - owner.lastMinDb) / denom);
            const float w = nn * nn + 0.05f;
            sum += w;
            playheadWeights[(size_t) (c * bins + b)] = sum;
        }
    }
    if (sum <= 1.0e-8f)
    {
        outCol = rng.nextInt (cols);
        outBinF = rng.nextFloat() * (float) juce::jmax (0, bins - 1);
        return;
    }

    const float pick = rng.nextFloat() * sum;
    int lo = 0, hi = n - 1;
    while (lo < hi)
    {
        const int mid = (lo + hi) >> 1;
        if (playheadWeights[(size_t) mid] < pick)
            lo = mid + 1;
        else
            hi = mid;
    }
    outCol = lo / bins;
    const int b = lo % bins;
    outBinF = (float) b + rng.nextFloat() * 0.999f;
    outCol = juce::jlimit (0, cols - 1, outCol);
    outBinF = juce::jlimit (0.0f, (float) juce::jmax (0, bins - 1), outBinF);
}

void Spec3DParticleSystem::spawnAtPlayhead (float binF, float lifespanBase,
                                            float sizeBase, float spawnJitterScale)
{
    // Default / playhead surface: only the live tip column (never history).
    spawnAtColumn (owner.meshW - 1, binF, lifespanBase, sizeBase, spawnJitterScale);
}

void Spec3DParticleSystem::spawnAtColumn (int col, float binF, float lifespanBase,
                                          float sizeBase, float spawnJitterScale)
{
    const int bins = owner.meshH;
    if (bins < 1 || owner.meshW < 1)
        return;

    col = juce::jlimit (0, owner.meshW - 1, col);

    binF = juce::jlimit (0.0f, (float) juce::jmax (0, bins - 1), binF);
    const int b0 = juce::jlimit (0, bins - 1, (int) std::floor (binF));
    const int b1 = juce::jmin (bins - 1, b0 + 1);
    const float fr = binF - (float) b0;

    float r0, g0, b0c, z0, db0, f0;
    float r1, g1, b1c, z1, db1, f1;
    const float y0 = sampleColumn (b0, col, r0, g0, b0c, z0, db0, f0);
    const float y1 = sampleColumn (b1, col, r1, g1, b1c, z1, db1, f1);
    // Still spawn on near-silent bins (tiny floor height) so continuous mode does not
    // burn the whole frame's spawn budget when energy-weighted picks hit quiet areas.
    const float targetY = juce::jmax (1.0e-4f, y0 * (1.0f - fr) + y1 * fr);

    const float r = r0 * (1.0f - fr) + r1 * fr;
    const float g = g0 * (1.0f - fr) + g1 * fr;
    const float bcol = b0c * (1.0f - fr) + b1c * fr;
    const float z = z0 * (1.0f - fr) + z1 * fr;
    const float db01 = db0 * (1.0f - fr) + db1 * fr;
    const float freq01 = f0 * (1.0f - fr) + f1 * fr;

    // Spectrogram trail spawn (unchanged semantics): ground y, bake targetY, col scroll.
    ParticleSpawnSample s;
    s.x = columnToWorldX (col);
    s.y = 0.0f;
    s.z = z;
    s.velX = owner.particleInitVelX;
    s.velY = owner.particleInitVelY;
    s.velZ = owner.particleInitVelZ;
    {
        const float velR = owner.particleVelRandom;
        if (std::abs (velR) > 1.0e-5f)
        {
            s.velX *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
            s.velY *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
            s.velZ *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
        }
    }
    s.targetY = juce::jmax (targetY, 0.01f);
    s.r = r; s.g = g; s.b = bcol;
    s.binDb01 = db01;
    s.binFreq01 = freq01;
    s.binF = binF;
    s.col = col;
    s.bin = b0;
    // Trail binding only when spectrogram trail mode; free still births at y=0 (same as before).
    s.trailBound = (owner.particleBindingMode != ParticleBindingMode::freeVisualizer);

    finalizeSpawn (s, lifespanBase, sizeBase, spawnJitterScale);
}

void Spec3DParticleSystem::sampleSprayDirection (float& outX, float& outY, float& outZ) noexcept
{
    // Aim: yaw around Y, pitch from +Y (0 = horizon +Z-ish after yaw, 90 = +Y up).
    const float yaw = juce::degreesToRadians (owner.particleSprayYawDeg);
    const float pitch = juce::degreesToRadians (owner.particleSprayPitchDeg);
    // Base direction: pitch 90 → (0,1,0); pitch 0 → (sin(yaw), 0, cos(yaw)).
    const float cp = std::cos (pitch);
    const float sp = std::sin (pitch);
    juce::Vector3D<float> base { std::sin (yaw) * cp, sp, std::cos (yaw) * cp };
    const float bl = std::sqrt (base.x * base.x + base.y * base.y + base.z * base.z);
    if (bl > 1.0e-8f) { base.x /= bl; base.y /= bl; base.z /= bl; }
    else { base = { 0, 1, 0 }; }

    const float spreadRad = juce::degreesToRadians (juce::jlimit (0.0f, 180.0f, owner.particleSpraySpreadDeg));
    if (spreadRad < 1.0e-6f)
    {
        outX = base.x; outY = base.y; outZ = base.z;
        return;
    }

    // Uniform solid-angle cone around base.
    const float cosMax = std::cos (spreadRad);
    const float u = rng.nextFloat();
    const float cosA = cosMax + (1.0f - cosMax) * u;
    const float sinA = std::sqrt (juce::jmax (0.0f, 1.0f - cosA * cosA));
    const float phi = rng.nextFloat() * juce::MathConstants<float>::twoPi;

    // Orthonormal frame with base as Y'.
    juce::Vector3D<float> arb = (std::abs (base.y) < 0.9f) ? juce::Vector3D<float> { 0, 1, 0 }
                                                           : juce::Vector3D<float> { 1, 0, 0 };
    juce::Vector3D<float> t { arb.y * base.z - arb.z * base.y,
                              arb.z * base.x - arb.x * base.z,
                              arb.x * base.y - arb.y * base.x };
    float tl = std::sqrt (t.x * t.x + t.y * t.y + t.z * t.z);
    if (tl > 1.0e-8f) { t.x /= tl; t.y /= tl; t.z /= tl; }
    juce::Vector3D<float> b { base.y * t.z - base.z * t.y,
                              base.z * t.x - base.x * t.z,
                              base.x * t.y - base.y * t.x };

    outX = base.x * cosA + t.x * (sinA * std::cos (phi)) + b.x * (sinA * std::sin (phi));
    outY = base.y * cosA + t.y * (sinA * std::cos (phi)) + b.y * (sinA * std::sin (phi));
    outZ = base.z * cosA + t.z * (sinA * std::cos (phi)) + b.z * (sinA * std::sin (phi));
    const float ol = std::sqrt (outX * outX + outY * outY + outZ * outZ);
    if (ol > 1.0e-8f) { outX /= ol; outY /= ol; outZ /= ol; }
}

bool Spec3DParticleSystem::samplePointEmitter (ParticleSpawnSample& out) noexcept
{
    out = {};
    out.trailBound = false;
    out.x = owner.particleEmitterPosX;
    out.y = owner.particleEmitterPosY;
    out.z = owner.particleEmitterPosZ;
    out.targetY = out.y;
    out.col = juce::jmax (0, owner.meshW - 1);
    out.bin = 0;
    out.binF = 0.0f;
    out.r = out.g = out.b = 1.0f;

    float dx, dy, dz;
    sampleSprayDirection (dx, dy, dz);

    float spdMin = owner.particleSpraySpeedMin;
    float spdMax = owner.particleSpraySpeedMax;
    if (spdMax < spdMin)
        std::swap (spdMin, spdMax);
    // If speeds unset, fall back to init-vel magnitude (spectrogram defaults still apply there).
    if (spdMax < 1.0e-6f && spdMin < 1.0e-6f)
    {
        const float ix = owner.particleInitVelX, iy = owner.particleInitVelY, iz = owner.particleInitVelZ;
        const float mag = std::sqrt (ix * ix + iy * iy + iz * iz);
        spdMin = spdMax = (mag > 1.0e-6f ? mag : 1.0f);
    }
    float speed = spdMin;
    if (spdMax > spdMin + 1.0e-6f)
        speed = spdMin + rng.nextFloat() * (spdMax - spdMin);

    out.velX = dx * speed;
    out.velY = dy * speed;
    out.velZ = dz * speed;
    const float velR = owner.particleVelRandom;
    if (std::abs (velR) > 1.0e-5f)
    {
        out.velX *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
        out.velY *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
        out.velZ *= (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * velR);
    }
    return true;
}

bool Spec3DParticleSystem::trySpawnOne (float lifespanBase, float sizeBase, float spawnJitterScale,
                                        float binFOrNeg1)
{
    const auto type = owner.particleEmitterType;
    if (type == ParticleEmitterType::point)
    {
        ParticleSpawnSample s;
        if (! samplePointEmitter (s))
            return false;
        const int before = aliveCount;
        finalizeSpawn (s, lifespanBase, sizeBase, spawnJitterScale);
        return aliveCount > before;
    }

    // Spectrogram (default) + reserved shape types fall through to mesh until implemented.
    if (type != ParticleEmitterType::spectrogram)
    {
        // Future sphere/box/disc/cone: for now behave as point so UI previews don't go silent.
        ParticleSpawnSample s;
        if (! samplePointEmitter (s))
            return false;
        const int before = aliveCount;
        finalizeSpawn (s, lifespanBase, sizeBase, spawnJitterScale);
        return aliveCount > before;
    }

    const int before = aliveCount;
    if (binFOrNeg1 >= 0.0f)
        spawnAtPlayhead (binFOrNeg1, lifespanBase, sizeBase, spawnJitterScale);
    else
        spawnAtPlayhead (pickPlayheadBinF(), lifespanBase, sizeBase, spawnJitterScale);
    return aliveCount > before;
}

void Spec3DParticleSystem::finalizeSpawn (const ParticleSpawnSample& sample, float lifespanBase,
                                          float sizeBase, float spawnJitterScale)
{
    if (aliveCount >= maxAliveBudget)
        return;

    const int slot = allocSlot();
    if (slot < 0)
        return;

    float maxLife = -1.0f;
    float lifeSec = lifespanBase;
    const bool freeMode = ! sample.trailBound
                          || owner.particleBindingMode == ParticleBindingMode::freeVisualizer;
    // Free visualizer: default lifespan if indefinite to avoid infinite pool fill.
    if (lifeSec <= 1.0e-4f && freeMode)
        lifeSec = 4.0f;
    if (lifeSec > 1.0e-4f)
    {
        const float lifeR = owner.particleLifespanRandom;
        if (std::abs (lifeR) > 1.0e-5f)
            lifeSec = lifeSec * (1.0f + (rng.nextFloat() * 2.0f - 1.0f) * lifeR);
        maxLife = lifeSec;
    }

    auto& p = pool[(size_t) slot];
    const bool wasAlive = p.alive;
    p.alive = true;
    if (! wasAlive)
    {
        ++aliveCount;
        p.aliveListSlot = -1;
        registerAlive (slot);
    }
    else if (p.aliveListSlot < 0)
    {
        registerAlive (slot);
    }
    p.settled = false;
    p.id = nextParticleId++;
    if (nextParticleId == 0)
        nextParticleId = 1;
    p.bin = sample.bin;
    p.binF = sample.binF;
    p.col = sample.col;
    p.x = sample.x;
    p.y = sample.y;
    p.z = sample.z;
    p.velX = sample.velX;
    p.velY = sample.velY;
    p.velZ = sample.velZ;
    p.targetY = juce::jmax (sample.targetY, 0.01f);
    p.baseR = sample.r; p.baseG = sample.g; p.baseB = sample.b;
    p.r = sample.r; p.g = sample.g; p.b = sample.b;
    p.age = 0.0f;
    p.maxLife = maxLife;
    p.sizeScale = sizeBase;
    p.colourGain = 1.0f;
    p.emissiveScale = 1.0f;
    p.alpha = 1.0f;
    p.binDb01 = sample.binDb01;
    p.binFreq01 = sample.binFreq01;
    p.spawnOffX = p.spawnOffY = p.spawnOffZ = 0.0f;

    float rotX = juce::degreesToRadians (owner.particleInitRotX);
    float rotY = juce::degreesToRadians (owner.particleInitRotY);
    float rotZ = juce::degreesToRadians (owner.particleInitRotZ);

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
                        ^ (uint32_t) sample.bin * 747796405u
                        ^ (uint32_t) (sample.col * 1597334677u)
                        ^ (uint32_t) (sample.binF * 1000.0f)
                        ^ (uint32_t) (sample.x * 1000.0f);

    const float rotRnd = owner.particleInitRotRandom;
    if (std::abs (rotRnd) > 1.0e-6f)
    {
        const float span = juce::MathConstants<float>::twoPi * rotRnd;
        rotX += (particleHash01 (seed + 11u) * 2.0f - 1.0f) * span;
        rotY += (particleHash01 (seed + 29u) * 2.0f - 1.0f) * span;
        rotZ += (particleHash01 (seed + 47u) * 2.0f - 1.0f) * span;
    }

    p.spinScaleX = particleHash01 (seed + 101u) * 2.0f - 1.0f;
    p.spinScaleY = particleHash01 (seed + 203u) * 2.0f - 1.0f;
    p.spinScaleZ = particleHash01 (seed + 307u) * 2.0f - 1.0f;
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
    {
        const float lo = juce::jmin (owner.particleSizeRandomMin, owner.particleSizeRandomMax);
        const float hi = juce::jmax (owner.particleSizeRandomMin, owner.particleSizeRandomMax);
        if (hi > lo + 1.0e-6f)
            sizeSc *= lo + rng.nextFloat() * (hi - lo);
        else
            sizeSc *= lo;
        sizeSc = juce::jmax (1.0e-4f, sizeSc);
    }
    float jitterSc = spawnJitterScale;
    float vx = p.velX, vy = p.velY, vz = p.velZ;
    applySpawnMods (p, life, sizeSc, jitterSc, vx, vy, vz, rotX, rotY, rotZ, frame);
    p.velX = vx;
    p.velY = vy;
    p.velZ = vz;
    p.sizeScale = juce::jmax (1.0e-4f, sizeSc);
    p.rotX = rotX; p.rotY = rotY; p.rotZ = rotZ;
    if (life > 1.0e-4f)
        p.maxLife = life;

    const float jAmt = juce::jmax (0.0f, owner.particleSpawnJitter) * juce::jmax (0.0f, jitterSc);
    if (jAmt > 1.0e-6f)
    {
        p.spawnOffX = (rng.nextFloat() * 2.0f - 1.0f) * jAmt;
        p.spawnOffY = rng.nextFloat() * jAmt * 0.35f;
        p.spawnOffZ = (rng.nextFloat() * 2.0f - 1.0f) * jAmt;
    }
    p.x += p.spawnOffX;
    p.z += p.spawnOffZ;
    p.y += p.spawnOffY;
    p.settled = false;

    if (owner.particleGpuSimEnabled && gpuComputeReady
        && (int) pool.size() <= kHardCap)
    {
        queueGpuSlotPatch (slot, freeMode);
    }
}

void Spec3DParticleSystem::fillGpuParticleFromPool (int poolIndex, bool freeMode, GpuSimParticle& g) const noexcept
{
    g = {};
    if (poolIndex < 0 || poolIndex >= (int) pool.size())
        return;
    const auto& p = pool[(size_t) poolIndex];
    g.px = p.x; g.py = p.y; g.pz = p.z; g.age = p.age;
    g.vx = p.velX; g.vy = p.velY; g.vz = p.velZ; g.maxLife = p.maxLife;
    g.spinX = p.spinScaleX; g.spinY = p.spinScaleY; g.spinZ = p.spinScaleZ;
    int fl = p.alive ? 1 : 0;
    if (p.settled) fl |= 2;
    if (freeMode) fl |= 4;
    if (! freeMode && owner.particleWaterfallLock) fl |= 8;
    g.flags = (float) fl;
    g.targetY = p.targetY; g.spawnOffY = p.spawnOffY;
    g.rotX = p.rotX; g.rotY = p.rotY; g.rotZ = p.rotZ;
    g.sizeScale = p.sizeScale; g.em = p.emissiveScale; g.alpha = p.alpha;
    g.r = p.r; g.g = p.g; g.b = p.b;
    g.poolIndex = (float) poolIndex;
}

void Spec3DParticleSystem::queueGpuSlotPatch (int poolIndex, bool freeMode) noexcept
{
    if (poolIndex < 0 || poolIndex >= (int) pool.size())
        return;
    GpuSimParticle g;
    fillGpuParticleFromPool (poolIndex, freeMode, g);
    for (auto& patch : gpuSpawnPatches)
    {
        if (patch.first == poolIndex)
        {
            patch.second = g;
            return;
        }
    }
    gpuSpawnPatches.emplace_back (poolIndex, g);
}

void Spec3DParticleSystem::queueGpuFieldPatch (int poolIndex) noexcept
{
    if (poolIndex < 0 || poolIndex >= (int) pool.size())
        return;
    const auto& p = pool[(size_t) poolIndex];
    if (! p.alive)
        return;
    // When forces are on, py on CPU is stale (GPU owns it) — still pass for no-force glue path.
    for (auto& fp : gpuFieldPatches)
    {
        if (fp.index == poolIndex)
        {
            fp.px = p.x;
            fp.py = p.y;
            fp.pz = p.z;
            fp.targetY = p.targetY;
            fp.spawnOffY = p.spawnOffY;
            return;
        }
    }
    gpuFieldPatches.push_back ({ poolIndex, p.x, p.y, p.z, p.targetY, p.spawnOffY });
}

void Spec3DParticleSystem::queueGpuKill (int poolIndex) noexcept
{
    if (poolIndex < 0 || poolIndex >= (int) pool.size())
        return;

    // Full-slot dead upload (flags=0). Overwrites any pending birth/field for this index.
    GpuSimParticle g {};
    g.poolIndex = (float) poolIndex;
    g.flags = 0.0f;

    for (auto& patch : gpuSpawnPatches)
    {
        if (patch.first == poolIndex)
        {
            patch.second = g;
            return;
        }
    }
    gpuSpawnPatches.emplace_back (poolIndex, g);

    // Drop stale field patches so we do not revive px/targetY on a dead slot.
    gpuFieldPatches.erase (std::remove_if (gpuFieldPatches.begin(), gpuFieldPatches.end(),
                                           [poolIndex] (const GpuFieldPatch& fp)
                                           { return fp.index == poolIndex; }),
                           gpuFieldPatches.end());
}

void Spec3DParticleSystem::update (float dtSeconds)
{
    if (owner.meshH < 2 || owner.meshW < 2 || owner.meshDb.empty())
        return;

    const double t0 = juce::Time::getMillisecondCounterHiRes();

    // Cap catch-up so a hitch cannot dump multi-frame work (settings-tunable).
    // Sub-frame dt is fine (60 Hz timer); ignore microscopic residual calls.
    const float simHz = juce::jmax (10.0f, owner.particleSimCatchupHz);
    const float emitHz = juce::jmax (15.0f, owner.particleEmitCatchupHz);
    const float maxSimDt = 1.0f / simHz;
    const float maxEmitDt = 1.0f / emitHz;
    dtSeconds = juce::jlimit (0.0f, maxSimDt, dtSeconds);
    if (dtSeconds < 1.0e-4f)
        return;

    // Emission uses a tighter (or equal) dt cap than sim after a long frame.
    const float emitDt = juce::jmin (dtSeconds, maxEmitDt);

    const juce::ScopedLock sl (simLock);

    ensurePool();
    simTime += dtSeconds;

    // Hitch recovery: climb load tiers when the last frame blew budget; ease off after
    // several healthy frames. Keeps the system moving instead of stacking more work.
    if (lastUpdateMs > 33.0f)       // < ~30 fps
        loadLevel = juce::jmin (3, loadLevel + 1);
    else if (lastUpdateMs > 20.0f)  // < ~50 fps
        loadLevel = juce::jmin (3, juce::jmax (1, loadLevel));
    else if (lastUpdateMs < 12.0f)  // comfortably under 60 fps budget
    {
        ++loadLevelGoodFrames;
        if (loadLevelGoodFrames >= 8)
        {
            loadLevel = juce::jmax (0, loadLevel - 1);
            loadLevelGoodFrames = 0;
        }
    }
    else
        loadLevelGoodFrames = 0;

    // LUT is revision-gated (cheap) — only refresh occasionally under load.
    if (owner.dataSource != nullptr
        && (loadLevel == 0 || (colourPassCursor & 3) == 0))
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

    // Spawns this frame: honour emission rate; settings Max is the only count limit.
    const float emissionPerSec = juce::jmax (0.0f, emission);

    if (freeList.empty() && aliveCount < (int) pool.size())
        rebuildFreeList();

    // At/over settings Max: cull excess, stop spawn (must not crash).
    if (aliveCount >= maxAliveBudget || loadLevel >= 2)
        enforceAliveBudget();

    // Rate is enforced ONLY by emitGlobal / per-bin accumulators.
    // spawnBudget is remaining Max slots only — no soft pressure or secondary rate caps.
    const int spawnBudget = juce::jmax (0, maxAliveBudget - aliveCount);
    const int bins = owner.meshH;
    const float totalRate = (spawnBudget > 0) ? emissionPerSec : 0.0f;
    float lifeBase = owner.particleLifespan;
    const float sizeBase = 1.0f;
    // Geometric emitters always free-integrate (no waterfall column scroll).
    const bool geoEmitter = (owner.particleEmitterType != ParticleEmitterType::spectrogram);
    const bool freeMode = geoEmitter
                          || owner.particleBindingMode == ParticleBindingMode::freeVisualizer;
    // Slice is spectrogram-only; geometric emitters always use continuous rate clock.
    const bool continuousEmit = geoEmitter
                                || (owner.particleEmitMode
                                    == Spectrogram3DComponent::ParticleEmitMode::continuous);
    lastSpawnedCount = 0;

    if (spawnBudget > 0 && totalRate > 1.0e-8f)
    {
        if (continuousEmit)
        {
            // Continuous: spectrogram energy-weighted tip, or geometric emitter samples.
            for (auto& a : emitAccum)
                a = 0.0f;

            emitGlobal += totalRate * emitDt;
            // Do not accumulate more debt than remaining Max slots (settings only).
            if (emitGlobal > (float) spawnBudget)
                emitGlobal = (float) spawnBudget;

            int failStreak = 0;
            while (emitGlobal >= 1.0f && lastSpawnedCount < spawnBudget
                   && aliveCount < maxAliveBudget)
            {
                emitGlobal -= 1.0f;
                if (trySpawnOne (lifeBase, sizeBase, spawnJitter, -1.0f))
                {
                    ++lastSpawnedCount;
                    failStreak = 0;
                }
                else
                {
                    ++failStreak;
                    if (failStreak >= 8 || aliveCount >= maxAliveBudget
                        || ((int) freeList.size() == 0 && aliveCount >= (int) pool.size()))
                        break;
                }
            }
        }
        else
        {
            // Slice: independent per-bin clocks → grid-like columns on the playhead tip.
            emitGlobal = 0.0f;
            const float perBin = totalRate / (float) juce::jmax (1, bins);
            for (int bin = 0; bin < bins && lastSpawnedCount < spawnBudget
                              && aliveCount < maxAliveBudget; ++bin)
            {
                emitAccum[(size_t) bin] += perBin * emitDt;
                // Soft per-bin backlog (default ~0.25s at full rate) — settings-tunable.
                const float backlogSec = juce::jmax (0.02f, owner.particleSliceBacklogSec);
                const float maxBinDebt = juce::jmax (4.0f, perBin * backlogSec);
                if (emitAccum[(size_t) bin] > maxBinDebt)
                    emitAccum[(size_t) bin] = maxBinDebt;
                int failStreak = 0;
                while (emitAccum[(size_t) bin] >= 1.0f && lastSpawnedCount < spawnBudget
                       && aliveCount < maxAliveBudget)
                {
                    emitAccum[(size_t) bin] -= 1.0f;
                    const float binF = (float) bin + rng.nextFloat() * 0.999f;
                    if (trySpawnOne (lifeBase, sizeBase, spawnJitter, binF))
                    {
                        ++lastSpawnedCount;
                        failStreak = 0;
                    }
                    else
                    {
                        ++failStreak;
                        if (failStreak >= 4)
                            break;
                    }
                }
            }
        }
    }
    else if (aliveCount >= maxAliveBudget)
    {
        emitGlobal = juce::jmin (emitGlobal, 1.0f);
        for (auto& a : emitAccum)
            a = juce::jmin (a, 1.0f);
    }

    // GPU when enabled — only limit is settings Max (pool/alive already clamped by spawn).
    const bool useGpuIntegrate = owner.particleGpuSimEnabled && gpuComputeReady
                                 && aliveCount <= maxAliveBudget
                                 && (int) pool.size() <= kHardCap;

    // Trail X from history column. Height/z stay baked at spawn.
    // Live FFT / colour mods: CPU integrate only — under GPU, CPU y/age/size are stale
    // and must never be written back into the resident SSBO (that re-grounds scrolled
    // particles and looks like the whole history is an emitter).
    {
        int colourStride = useGpuIntegrate ? 64
                         : (aliveCount > 50000 ? 16
                         : aliveCount > 20000 ? 8
                         : aliveCount > 8000  ? 4
                         : aliveCount > 3000  ? 2 : 1);
        if (loadLevel >= 1) colourStride = juce::jmax (colourStride, 16);
        if (loadLevel >= 2) colourStride = juce::jmax (colourStride, 32);
        colourPassCursor = (colourPassCursor + 1) % juce::jmax (1, colourStride);
        int colourIdx = 0;

        // Trail + CPU: any matrix row that needs live per-particle sources/dests.
        bool needTrailModPass = false;
        if (! freeMode && ! useGpuIntegrate)
        {
            for (int i = 0; i < kParticleModSlotCount; ++i)
            {
                const auto& slot = owner.particleModSlots[(size_t) i];
                if (! slot.enabled || slot.source == ParticleModSource::none)
                    continue;
                if (slot.source == ParticleModSource::binDb
                    || slot.source == ParticleModSource::ageNorm
                    || slot.source == ParticleModSource::history
                    || slot.dest == ParticleModDest::size
                    || slot.dest == ParticleModDest::sizeScale
                    || slot.dest == ParticleModDest::emissive
                    || slot.dest == ParticleModDest::alpha
                    || slot.dest == ParticleModDest::colourGain
                    || slot.dest == ParticleModDest::colourHue)
                {
                    needTrailModPass = true;
                    break;
                }
            }
        }

        if ((int) aliveList.size() != aliveCount)
            rebuildAliveList();

        for (int ai = 0; ai < (int) aliveList.size(); ++ai)
        {
            const int pi = aliveList[(size_t) ai];
            if (pi < 0 || pi >= (int) pool.size())
                continue;
            auto& p = pool[(size_t) pi];
            if (! p.alive)
                continue;

            if (! freeMode)
                p.x = columnToWorldX (p.col) + p.spawnOffX;

            // Trail: height/z baked at spawn — never touch targetY/y/z from mesh again.
            // GPU trail: stop here (GPU owns py/age; CPU pack must not drive draw).
            if (! freeMode && useGpuIntegrate)
                continue;

            if (! freeMode)
            {
                // Trail + CPU: live FFT strength for mod matrix (no height re-bake).
                if (! needTrailModPass)
                    continue;
                if ((colourIdx % colourStride) != (colourPassCursor % colourStride))
                {
                    ++colourIdx;
                    continue;
                }
                ++colourIdx;

                float r0, g0, bb0, z0, db0, f0;
                float r1, g1, bb1, z1, db1, f1;
                const int binsLocal = owner.meshH;
                const float binF = juce::jlimit (0.0f, (float) juce::jmax (0, binsLocal - 1), p.binF);
                const int b0 = juce::jlimit (0, juce::jmax (0, binsLocal - 1), (int) std::floor (binF));
                const int b1 = juce::jmin (binsLocal - 1, b0 + 1);
                const float fr = binF - (float) b0;
                const int colS = juce::jlimit (0, juce::jmax (0, owner.meshW - 1), p.col);
                sampleColumn (b0, colS, r0, g0, bb0, z0, db0, f0, false);
                sampleColumn (b1, colS, r1, g1, bb1, z1, db1, f1, false);
                p.binDb01 = db0 * (1.0f - fr) + db1 * fr;
                applyUpdateMods (p, frame);
                continue;
            }

            // Free mode only: optional colour refresh (no height re-bake).
            const bool doColour = ! useGpuIntegrate
                                  && (colourIdx % colourStride) == (colourPassCursor % colourStride);
            ++colourIdx;
            if (! doColour)
                continue;

            const int binsLocal = owner.meshH;
            const float binF = juce::jlimit (0.0f, (float) juce::jmax (0, binsLocal - 1), p.binF);
            const int b0 = juce::jlimit (0, juce::jmax (0, binsLocal - 1), (int) std::floor (binF));
            const int b1 = juce::jmin (binsLocal - 1, b0 + 1);
            const float fr = binF - (float) b0;
            float r0, g0, bb0, z0, db0, f0;
            float r1, g1, bb1, z1, db1, f1;
            sampleColumn (b0, juce::jmax (0, owner.meshW - 1), r0, g0, bb0, z0, db0, f0, true);
            sampleColumn (b1, juce::jmax (0, owner.meshW - 1), r1, g1, bb1, z1, db1, f1, true);
            p.baseR = r0 * (1.0f - fr) + r1 * fr;
            p.baseG = g0 * (1.0f - fr) + g1 * fr;
            p.baseB = bb0 * (1.0f - fr) + bb1 * fr;
            p.binDb01 = db0 * (1.0f - fr) + db1 * fr;
            applyUpdateMods (p, frame);
        }
    }

    // ── Integrate ──────────────────────────────────────────────────────────
    // GPU: resident pool-index SSBO, sparse patches, death-list only (no full readback).

    if (useGpuIntegrate)
    {
        // Cap deferred GPU dt the same way (no multi-frame pile-up).
        const float maxPend = 1.0f / juce::jmax (10.0f, owner.particleSimCatchupHz);
        pendingIntegrateDt = juce::jmin (maxPend, pendingIntegrateDt + dtSeconds);
        pendingForceScales = forceScales;
        pendingFreeMode = freeMode;
        // Do NOT clear gpuInstancesValid here — if GL skips a frame, last compact is better
        // than falling back to packing y=0 ghosts.
    }
    else
    {
        const float maxPend = 1.0f / juce::jmax (10.0f, owner.particleSimCatchupHz);
        const float dt = juce::jmin (maxPend, dtSeconds + pendingIntegrateDt);
        pendingIntegrateDt = 0.0f;
        cpuIntegrateAll (dt, forceScales, freeMode);
        if (aliveCount > maxAliveBudget || loadLevel >= 2)
            enforceAliveBudget();
        gpuInstancesValid = false;
    }

    lastUpdateMs = (float) (juce::Time::getMillisecondCounterHiRes() - t0);
}

void Spec3DParticleSystem::cpuIntegrateAll (float dt, const ForceModScales& scales, bool freeMode) noexcept
{
    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();

    // Reverse walk: markDead swap-removes with last entry.
    for (int ai = (int) aliveList.size() - 1; ai >= 0; --ai)
    {
        const int i = aliveList[(size_t) ai];
        if (i < 0 || i >= (int) pool.size())
            continue;
        auto& p = pool[(size_t) i];
        if (! p.alive)
            continue;

        if (p.maxLife >= 0.0f)
        {
            p.age += dt;
            if (p.age >= p.maxLife)
            {
                markDead (i);
                continue;
            }
        }

        // Trail X follows history column. targetY is baked at spawn — never re-sampled.
        if (! freeMode)
            p.x = columnToWorldX (p.col) + p.spawnOffX;

        if (owner.particleForcesEnabled)
        {
            integrateForceStack (p, scales, dt);
            if (! freeMode && owner.particleWaterfallLock)
            {
                p.x = columnToWorldX (p.col) + p.spawnOffX;
                p.velX = 0.0f;
            }
            // Rise toward baked height; once there, forces own motion (no mesh re-read).
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
        else if (! freeMode)
        {
            // No forces: rise from ground with init velY toward baked height, then hold.
            if (! p.settled)
            {
                p.y += p.velY * dt;
                const float capY = p.targetY + p.spawnOffY;
                if (p.y >= capY)
                {
                    p.y = capY;
                    p.velY = 0.0f;
                    p.settled = true;
                }
            }
            else
            {
                p.velX = p.velY = p.velZ = 0.0f;
                p.y = p.targetY + p.spawnOffY; // hold bake (targetY immutable)
            }
        }
        else
        {
            p.y += p.velY * dt;
            p.x += p.velX * dt;
            p.z += p.velZ * dt;
        }

        if (freeMode)
        {
            if (p.y < -0.5f || p.y > 4.0f || std::abs (p.x) > 3.0f || std::abs (p.z) > 3.0f)
                markDead (i);
        }
    }
}

void Spec3DParticleSystem::integrateOnGlThread()
{
    const juce::ScopedLock sl (simLock);

    if (pendingIntegrateDt <= 1.0e-8f || pool.empty())
        return;

    // Match CPU update / settings sim catch-up cap.
    const float maxDt = 1.0f / juce::jmax (10.0f, owner.particleSimCatchupHz);
    const float dt = juce::jlimit (0.0f, maxDt, pendingIntegrateDt);
    pendingIntegrateDt = 0.0f;
    const auto scales = pendingForceScales;
    const bool freeMode = pendingFreeMode;

    const bool useGpu = owner.particleGpuSimEnabled && gpuComputeReady && glReady
                        && (int) pool.size() <= kHardCap;
    if (useGpu)
        gpuIntegrateAndCompact (dt, scales, freeMode);
    else
    {
        cpuIntegrateAll (dt, scales, freeMode);
        gpuInstancesValid = false;
        gpuPoolResident = false;
    }

    enforceAliveBudget();
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
    // Optional compute path — failure leaves CPU sim fully working.
    if (glReady)
        createComputeProgram();
}

void Spec3DParticleSystem::releaseGl()
{
    using namespace juce::gl;
    releaseComputeProgram();
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

    // GPU owns positions after birth. Never fall back to packing CPU y (stays 0 under the
    // GPU path) — that re-grounds every scrolled particle across the trail and looks like
    // the whole history is the emitter. Skip the frame if compact has no valid buffer yet.
    if (owner.particleGpuSimEnabled && gpuComputeReady)
    {
        if (gpuInstancesValid && gpuDrawnInstances > 0 && instanceSsbo != 0)
            drawInstancedMeshesFromGpu (projection, view);
        return;
    }

    // CPU instance pack (CPU integrate path only — y is integrated each update).
    const juce::ScopedLock sl (simLock);

    instances.clear();
    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();
    // Draw all alive (settings Max is the ceiling). OOM → skip frame, never crash.
    const int drawCap = juce::jmin (aliveCount, maxAliveBudget);
    try
    {
        instances.reserve ((size_t) juce::jmax (0, drawCap));
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    for (int ai = 0; ai < (int) aliveList.size(); ++ai)
    {
        const int pi = aliveList[(size_t) ai];
        if (pi < 0 || pi >= (int) pool.size())
            continue;
        const auto& p = pool[(size_t) pi];
        if (! p.alive) continue;
        if ((int) instances.size() >= drawCap)
            break;
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

    // GPU path owns positions — CPU y stays at spawn. Packing billboards from the CPU
    // pool would ground every scrolled particle; draw GPU-compacted meshes instead.
    if (owner.particleGpuSimEnabled && gpuComputeReady)
    {
        if (gpuInstancesValid && gpuDrawnInstances > 0 && instanceSsbo != 0)
            drawInstancedMeshesFromGpu (projection, view);
        return;
    }

    gpuVerts.clear();
    const int drawCap = juce::jmin (aliveCount, maxAliveBudget);
    try
    {
        gpuVerts.reserve ((size_t) juce::jmax (0, drawCap) * 6u);
    }
    catch (const std::bad_alloc&)
    {
        return;
    }
    const float corners[6][2] = {
        { -1,-1 },{ 1,-1 },{ 1,1 },{ -1,-1 },{ 1,1 },{ -1,1 }
    };
    int drawn = 0;
    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();
    for (int ai = 0; ai < (int) aliveList.size(); ++ai)
    {
        const int pi = aliveList[(size_t) ai];
        if (pi < 0 || pi >= (int) pool.size())
            continue;
        const auto& p = pool[(size_t) pi];
        if (! p.alive) continue;
        if (drawn >= drawCap)
            break;
        ++drawn;
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

bool Spec3DParticleSystem::createComputeProgram()
{
    using namespace juce::gl;
    releaseComputeProgram();

    if (glDispatchCompute == nullptr || glBindBufferBase == nullptr
        || glMemoryBarrier == nullptr)
        return false;

    static constexpr const char* kComputeSrc = R"(
#version 430
layout(local_size_x = 64) in;

struct ParticleGpu {
    float px, py, pz, age;
    float vx, vy, vz, maxLife;
    float spinX, spinY, spinZ, flags;
    float targetY, spawnOffY, rotX, rotY;
    float rotZ, sizeScale, em, alpha;
    float r, g, b, poolIndex;
};

struct ForceGpu {
    int type;
    int enabled;
    int axisMask;
    int flags;
    float p0, p1, p2, p3;
};

layout(std430, binding = 0) buffer ParticleBuf { ParticleGpu particles[]; };
layout(std430, binding = 1) readonly buffer ForceBuf { ForceGpu forces[]; };
layout(std430, binding = 4) buffer DeathBuf { int deathIndices[]; };
layout(std430, binding = 5) buffer DeathCounter { uint deathCount; };

uniform int uCount;
uniform int uForceCount;
uniform float uDt;
uniform float uSimTime;
uniform int uDeathCap;
uniform float uHistoryScrollX;

float hashNoise(int x, int y, int z)
{
    int n = x * 374761393 + y * 668265263 + z * 2147483647;
    n = (n ^ (n >> 13)) * 1274126177;
    n = n ^ (n >> 16);
    return float(n & 0x7fffffff) / 2147483647.0;
}

float valueNoise(float x, float y, float z)
{
    int x0 = int(floor(x)), y0 = int(floor(y)), z0 = int(floor(z));
    float fx = x - float(x0), fy = y - float(y0), fz = z - float(z0);
    float u = fx * fx * (3.0 - 2.0 * fx);
    float v = fy * fy * (3.0 - 2.0 * fy);
    float w = fz * fz * (3.0 - 2.0 * fz);
    float n000 = hashNoise(x0, y0, z0), n100 = hashNoise(x0+1, y0, z0);
    float n010 = hashNoise(x0, y0+1, z0), n110 = hashNoise(x0+1, y0+1, z0);
    float n001 = hashNoise(x0, y0, z0+1), n101 = hashNoise(x0+1, y0, z0+1);
    float n011 = hashNoise(x0, y0+1, z0+1), n111 = hashNoise(x0+1, y0+1, z0+1);
    float x00 = mix(n000, n100, u), x10 = mix(n010, n110, u);
    float x01 = mix(n001, n101, u), x11 = mix(n011, n111, u);
    return mix(mix(x00, x10, v), mix(x01, x11, v), w) * 2.0 - 1.0;
}

vec3 curlNoise(float x, float y, float z)
{
    float e = 0.08;
    float cx = (valueNoise(x, y+e, z) - valueNoise(x, y-e, z)
              - (valueNoise(x, y, z+e) - valueNoise(x, y, z-e))) / (2.0 * e);
    float cy = (valueNoise(x, y, z+e) - valueNoise(x, y, z-e)
              - (valueNoise(x+e, y, z) - valueNoise(x-e, y, z))) / (2.0 * e);
    float cz = (valueNoise(x+e, y, z) - valueNoise(x-e, y, z)
              - (valueNoise(x, y+e, z) - valueNoise(x, y-e, z))) / (2.0 * e);
    return vec3(cx, cy, cz);
}

// Deterministic turbulence from particle index + time (no RNG on GPU).
float turbHash(uint n)
{
    n ^= n >> 16u;
    n *= 0x7feb352du;
    n ^= n >> 15u;
    n *= 0x846ca68bu;
    n ^= n >> 16u;
    return float(n & 0x00ffffffu) * (1.0 / 16777215.0) * 2.0 - 1.0;
}

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (int(i) >= uCount) return;

    ParticleGpu p = particles[i];
    int fl = int(p.flags + 0.5);
    if ((fl & 1) == 0) return; // dead

    bool freeMode = (fl & 4) != 0;
    bool settled  = (fl & 2) != 0;
    bool lockX    = (fl & 8) != 0;
    float dt = uDt;

    // Trail: apply accumulated history scroll in world X (one uniform for all particles).
    if (! freeMode && abs(uHistoryScrollX) > 1.0e-12)
        p.px += uHistoryScrollX;

    // Age / kill — record poolIndex for CPU free-list (Phase 2: no full writeback).
    if (p.maxLife >= 0.0)
    {
        p.age += dt;
        if (p.age >= p.maxLife)
        {
            p.flags = 0.0;
            particles[i] = p;
            uint di = atomicAdd (deathCount, 1u);
            if (int(di) < uDeathCap)
                deathIndices[di] = int(p.poolIndex + 0.5);
            return;
        }
    }

    bool forcesOn = uForceCount > 0;
    if (forcesOn)
    {
        for (int f = 0; f < uForceCount; ++f)
        {
            ForceGpu m = forces[f];
            if (m.enabled == 0) continue;
            if (m.type == 0) // gravity
                p.vy += m.p0 * dt;
            else if (m.type == 1) // drag
            {
                float d = abs(m.p0);
                if (d > 1.0e-8)
                {
                    float damp = exp(-d * dt);
                    p.vx *= damp; p.vy *= damp; p.vz *= damp;
                }
            }
            else if (m.type == 2) // wind
            {
                p.vx += m.p0 * dt;
                p.vy += m.p1 * dt;
                p.vz += m.p2 * dt;
            }
            else if (m.type == 3) // curl
            {
                float str = m.p0, sc = m.p1, spd = m.p2;
                if (abs(str) > 1.0e-8)
                {
                    if (abs(sc) < 1.0e-6) sc = 1.0e-4;
                    float t = uSimTime * spd;
                    vec3 c = curlNoise(p.px * sc + t, p.py * sc, p.pz * sc + t * 0.7);
                    p.vx += c.x * str * dt;
                    p.vy += c.y * str * dt;
                    p.vz += c.z * str * dt;
                }
            }
            else if (m.type == 4) // turbulence
            {
                float str = m.p0;
                if (abs(str) > 1.0e-8)
                {
                    uint seed = i * 747796405u + uint(uSimTime * 1000.0);
                    p.vx += turbHash(seed + 11u) * str * dt;
                    p.vy += turbHash(seed + 29u) * str * dt;
                    p.vz += turbHash(seed + 47u) * str * dt;
                }
            }
            else if (m.type == 5) // rotation
            {
                float rateX = m.p0;
                float rateY = ((m.flags & 1) != 0) ? m.p0 : m.p1;
                float rateZ = ((m.flags & 1) != 0) ? m.p0 : m.p2;
                float sx = ((m.flags & 2) != 0) ? p.spinX : 1.0;
                float sy = ((m.flags & 2) != 0) ? p.spinY : 1.0;
                float sz = ((m.flags & 2) != 0) ? p.spinZ : 1.0;
                if ((m.axisMask & 1) != 0) p.rotX += rateX * sx * dt;
                if ((m.axisMask & 2) != 0) p.rotY += rateY * sy * dt;
                if ((m.axisMask & 4) != 0) p.rotZ += rateZ * sz * dt;
            }
        }

        // Forces: integrate. targetY is bake-only (spawn sample), never live mesh.
        if (! freeMode && lockX)
        {
            p.vx = 0.0;
            p.py += p.vy * dt;
            p.pz += p.vz * dt;
        }
        else
        {
            p.px += p.vx * dt;
            p.py += p.vy * dt;
            p.pz += p.vz * dt;
        }
        // Rise from ground until baked height; then forces keep moving if any.
        if (! freeMode && ! settled)
        {
            float capY = p.targetY + p.spawnOffY;
            if (p.py >= capY)
            {
                p.py = capY;
                if (p.vy > 0.0) p.vy = 0.0;
                settled = true;
            }
        }
    }
    else
    {
        // No force modules: rise from ground to baked height, then hold.
        if (freeMode)
        {
            p.px += p.vx * dt;
            p.py += p.vy * dt;
            p.pz += p.vz * dt;
        }
        else if (settled)
        {
            p.vx = 0.0;
            p.vy = 0.0;
            p.vz = 0.0;
            p.py = p.targetY + p.spawnOffY; // immutable bake
        }
        else
        {
            p.py += p.vy * dt;
            float capY = p.targetY + p.spawnOffY;
            if (p.py >= capY)
            {
                p.py = capY;
                p.vy = 0.0;
                settled = true;
            }
        }
    }

    if (freeMode)
    {
        if (p.py < -0.5 || p.py > 4.0 || abs(p.px) > 3.0 || abs(p.pz) > 3.0)
        {
            p.flags = 0.0;
            particles[i] = p;
            uint di = atomicAdd (deathCount, 1u);
            if (int(di) < uDeathCap)
                deathIndices[di] = int(p.poolIndex + 0.5);
            return;
        }
    }

    fl = 1; // alive
    if (settled) fl |= 2;
    if (freeMode) fl |= 4;
    if (lockX) fl |= 8;
    p.flags = float(fl);
    particles[i] = p;
}
)";

    GLuint cs = glCreateShader (GL_COMPUTE_SHADER);
    const char* src = kComputeSrc;
    glShaderSource (cs, 1, &src, nullptr);
    glCompileShader (cs);
    GLint ok = GL_FALSE;
    glGetShaderiv (cs, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE)
    {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog (cs, 1024, &len, log);
        DBG ("Particle compute compile: " + juce::String (log, (size_t) len));
        glDeleteShader (cs);
        return false;
    }

    computeProgram = glCreateProgram();
    glAttachShader (computeProgram, cs);
    glLinkProgram (computeProgram);
    glDeleteShader (cs);
    glGetProgramiv (computeProgram, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE)
    {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog (computeProgram, 1024, &len, log);
        DBG ("Particle compute link: " + juce::String (log, (size_t) len));
        glDeleteProgram (computeProgram);
        computeProgram = 0;
        return false;
    }

    computeUCount = glGetUniformLocation (computeProgram, "uCount");
    computeUForceCount = glGetUniformLocation (computeProgram, "uForceCount");
    computeUDt = glGetUniformLocation (computeProgram, "uDt");
    computeUSimTime = glGetUniformLocation (computeProgram, "uSimTime");
    computeUDeathCap = glGetUniformLocation (computeProgram, "uDeathCap");
    computeUHistoryScrollX = glGetUniformLocation (computeProgram, "uHistoryScrollX");

    glGenBuffers (1, &particleSsbo);
    glGenBuffers (1, &forceSsbo);
    glGenBuffers (1, &instanceSsbo);
    glGenBuffers (1, &counterSsbo);
    glGenBuffers (1, &deathSsbo);
    glGenBuffers (1, &deathCountSsbo);
    gpuComputeReady = (particleSsbo != 0 && forceSsbo != 0 && computeProgram != 0
                       && instanceSsbo != 0 && counterSsbo != 0
                       && deathSsbo != 0 && deathCountSsbo != 0);
    if (! gpuComputeReady)
    {
        releaseComputeProgram();
        return false;
    }

    // Start with a modest SSBO; grow with pool (copy-preserve) up to settings Max / hard cap.
    {
        const int initialSlots = juce::jmin (kHardCap, juce::jmax (kDefaultMaxAlive, 16384));
        const GLsizeiptr initialBytes =
            (GLsizeiptr) ((size_t) initialSlots * sizeof (GpuSimParticle));
        glBindBuffer (GL_SHADER_STORAGE_BUFFER, particleSsbo);
        glBufferData (GL_SHADER_STORAGE_BUFFER, initialBytes, nullptr, GL_DYNAMIC_COPY);
        glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0);
        gpuParticleSsboCap = (size_t) initialBytes;
    }
    gpuPoolResident = false;
    gpuResidentCapacity = 0;

    createCompactProgram();
    return gpuComputeReady;
}

bool Spec3DParticleSystem::createCompactProgram()
{
    using namespace juce::gl;
    gpuCompactReady = false;
    if (compactProgram) { glDeleteProgram (compactProgram); compactProgram = 0; }

    static constexpr const char* kCompactSrc = R"(
#version 430
layout(local_size_x = 64) in;

struct ParticleGpu {
    float px, py, pz, age;
    float vx, vy, vz, maxLife;
    float spinX, spinY, spinZ, flags;
    float targetY, spawnOffY, rotX, rotY;
    float rotZ, sizeScale, em, alpha;
    float r, g, b, poolIndex;
};

struct InstanceGpu {
    float px, py, pz, sx;
    float qx, qy, qz, qw;
    float r, g, b, a;
    float em, pad0, pad1, pad2;
};

layout(std430, binding = 0) readonly buffer ParticleBuf { ParticleGpu particles[]; };
layout(std430, binding = 2) writeonly buffer InstanceBuf { InstanceGpu instances[]; };
layout(std430, binding = 3) buffer CounterBuf { uint count; };

uniform int uCount;
uniform float uBaseSize;

void eulerToQuat(float rx, float ry, float rz, out float qx, out float qy, out float qz, out float qw)
{
    float cx = cos(rx * 0.5), sx = sin(rx * 0.5);
    float cy = cos(ry * 0.5), sy = sin(ry * 0.5);
    float cz = cos(rz * 0.5), sz = sin(rz * 0.5);
    qw = cx * cy * cz + sx * sy * sz;
    qx = sx * cy * cz - cx * sy * sz;
    qy = cx * sy * cz + sx * cy * sz;
    qz = cx * cy * sz - sx * sy * cz;
}

void main()
{
    uint i = gl_GlobalInvocationID.x;
    if (int(i) >= uCount) return;
    ParticleGpu p = particles[i];
    int fl = int(p.flags + 0.5);
    if ((fl & 1) == 0) return;

    uint slot = atomicAdd(count, 1u);
    InstanceGpu o;
    o.px = p.px; o.py = p.py; o.pz = p.pz;
    o.sx = max(p.sizeScale, 1.0e-4) * max(uBaseSize, 1.0e-4) * 2.0;
    eulerToQuat(p.rotX, p.rotY, p.rotZ, o.qx, o.qy, o.qz, o.qw);
    o.r = p.r; o.g = p.g; o.b = p.b; o.a = p.alpha;
    o.em = p.em;
    o.pad0 = o.pad1 = o.pad2 = 0.0;
    instances[slot] = o;
}
)";

    GLuint cs = glCreateShader (GL_COMPUTE_SHADER);
    const char* src = kCompactSrc;
    glShaderSource (cs, 1, &src, nullptr);
    glCompileShader (cs);
    GLint ok = GL_FALSE;
    glGetShaderiv (cs, GL_COMPILE_STATUS, &ok);
    if (ok != GL_TRUE)
    {
        char log[1024];
        GLsizei len = 0;
        glGetShaderInfoLog (cs, 1024, &len, log);
        DBG ("Particle compact compile: " + juce::String (log, (size_t) len));
        glDeleteShader (cs);
        return false;
    }

    compactProgram = glCreateProgram();
    glAttachShader (compactProgram, cs);
    glLinkProgram (compactProgram);
    glDeleteShader (cs);
    glGetProgramiv (compactProgram, GL_LINK_STATUS, &ok);
    if (ok != GL_TRUE)
    {
        char log[1024];
        GLsizei len = 0;
        glGetProgramInfoLog (compactProgram, 1024, &len, log);
        DBG ("Particle compact link: " + juce::String (log, (size_t) len));
        glDeleteProgram (compactProgram);
        compactProgram = 0;
        return false;
    }

    compactUCount = glGetUniformLocation (compactProgram, "uCount");
    // uBaseSize may be -1 if optimized out — set each dispatch
    gpuCompactReady = (compactProgram != 0);
    return gpuCompactReady;
}

void Spec3DParticleSystem::releaseComputeProgram()
{
    using namespace juce::gl;
    if (particleSsbo) { glDeleteBuffers (1, &particleSsbo); particleSsbo = 0; }
    if (forceSsbo) { glDeleteBuffers (1, &forceSsbo); forceSsbo = 0; }
    if (instanceSsbo) { glDeleteBuffers (1, &instanceSsbo); instanceSsbo = 0; }
    if (counterSsbo) { glDeleteBuffers (1, &counterSsbo); counterSsbo = 0; }
    if (deathSsbo) { glDeleteBuffers (1, &deathSsbo); deathSsbo = 0; }
    if (deathCountSsbo) { glDeleteBuffers (1, &deathCountSsbo); deathCountSsbo = 0; }
    gpuParticleSsboCap = gpuForceSsboCap = gpuInstanceSsboCap = gpuDeathSsboCap = 0;
    gpuPoolResident = false;
    gpuResidentCapacity = 0;
    gpuCounterBufsSized = false;
    gpuSpawnPatches.clear();
    gpuFieldPatches.clear();
    pendingHistoryScrollX = 0.0f;
    if (computeProgram) { glDeleteProgram (computeProgram); computeProgram = 0; }
    if (compactProgram) { glDeleteProgram (compactProgram); compactProgram = 0; }
    computeUCount = computeUForceCount = computeUDt = computeUSimTime = -1;
    computeUDeathCap = computeUHistoryScrollX = -1;
    compactUCount = -1;
    gpuComputeReady = false;
    gpuCompactReady = false;
    gpuSimPack.clear();
    gpuForcePack.clear();
    gpuPackedAlive = 0;
    gpuDrawnInstances = 0;
    gpuInstancesValid = false;
}

void Spec3DParticleSystem::bakeForcePack (const ForceModScales& scales)
{
    gpuForcePack.clear();
    if (owner.particleForcesEnabled)
    {
        // Copy so UI can replace the vector while GL bakes/integrates.
        const auto stackCopy = owner.particleForceStack;
        for (const auto& mod : stackCopy)
        {
            if (! mod.enabled)
                continue;
            GpuForceMod f;
            f.type = (int) mod.type;
            f.enabled = 1;
            f.axisMask = (mod.axisX ? 1 : 0) | (mod.axisY ? 2 : 0) | (mod.axisZ ? 4 : 0);
            f.flags = (mod.linkAxes ? 1 : 0) | (mod.randomDir ? 2 : 0);
            const float p0 = std::isfinite (mod.p[0]) ? mod.p[0] : 0.0f;
            const float p1 = std::isfinite (mod.p[1]) ? mod.p[1] : 0.0f;
            const float p2 = std::isfinite (mod.p[2]) ? mod.p[2] : 0.0f;
            switch (mod.type)
            {
                case ParticleForceType::gravity:
                    f.p0 = p0 * scales.gravity + scales.gravityAdd; break;
                case ParticleForceType::drag:
                    f.p0 = p0 * scales.drag + scales.dragAdd; break;
                case ParticleForceType::wind:
                    f.p0 = p0 * scales.windX + scales.windAddX;
                    f.p1 = p1 * scales.windY + scales.windAddY;
                    f.p2 = p2 * scales.windZ + scales.windAddZ;
                    break;
                case ParticleForceType::curlNoise:
                    f.p0 = p0 * scales.curlStrength + scales.curlStrAdd;
                    f.p1 = p1 * scales.curlScale + scales.curlScaleAdd;
                    f.p2 = p2 * scales.curlSpeed + scales.curlSpeedAdd;
                    break;
                case ParticleForceType::turbulence:
                    f.p0 = p0 * scales.turbulence + scales.turbAdd; break;
                case ParticleForceType::rotation:
                    f.p0 = p0; f.p1 = p1; f.p2 = p2; break;
                default: break;
            }
            gpuForcePack.push_back (f);
        }
    }
    if (gpuForcePack.empty())
    {
        GpuForceMod dummy {};
        dummy.enabled = 0;
        gpuForcePack.push_back (dummy);
    }
}

void Spec3DParticleSystem::packAliveToGpuSim (bool freeMode)
{
    gpuSimPack.clear();
    if ((int) aliveList.size() != aliveCount)
        rebuildAliveList();
    gpuSimPack.reserve ((size_t) juce::jmin (aliveCount, maxAliveBudget));
    for (int ai = 0; ai < (int) aliveList.size(); ++ai)
    {
        const int i = aliveList[(size_t) ai];
        if (i < 0 || i >= (int) pool.size())
            continue;
        const auto& p = pool[(size_t) i];
        if (! p.alive)
            continue;
        if ((int) gpuSimPack.size() >= maxAliveBudget)
            break;
        GpuSimParticle g;
        g.px = p.x; g.py = p.y; g.pz = p.z; g.age = p.age;
        g.vx = p.velX; g.vy = p.velY; g.vz = p.velZ; g.maxLife = p.maxLife;
        g.spinX = p.spinScaleX; g.spinY = p.spinScaleY; g.spinZ = p.spinScaleZ;
        int fl = 1;
        if (p.settled) fl |= 2;
        if (freeMode) fl |= 4;
        if (! freeMode && owner.particleWaterfallLock) fl |= 8;
        g.flags = (float) fl;
        g.targetY = p.targetY; g.spawnOffY = p.spawnOffY;
        g.rotX = p.rotX; g.rotY = p.rotY; g.rotZ = p.rotZ;
        g.sizeScale = p.sizeScale;
        g.em = p.emissiveScale;
        g.alpha = p.alpha;
        g.r = p.r; g.g = p.g; g.b = p.b;
        g.poolIndex = (float) i;
        gpuSimPack.push_back (g);
    }
    gpuPackedAlive = (int) gpuSimPack.size();
}

void Spec3DParticleSystem::gpuIntegrateAndCompact (float dt, const ForceModScales& scales, bool freeMode)
{
    using namespace juce::gl;
    gpuInstancesValid = false;
    gpuDrawnInstances = 0;
    if (! gpuComputeReady || computeProgram == 0 || particleSsbo == 0
        || deathSsbo == 0 || deathCountSsbo == 0)
        return;

    // Cap by allocated pool size (lazy growth) and absolute hard cap only.
    const int poolCap = juce::jmax (1, juce::jmin (kHardCap, (int) pool.size()));
    if (aliveCount > maxAliveBudget)
        enforceAliveBudget();

    if (aliveCount <= 0 && gpuSpawnPatches.empty() && ! gpuPoolResident)
        return;

    bakeForcePack (scales);
    int realForceCount = 0;
    for (const auto& f : gpuForcePack)
        if (f.enabled)
            ++realForceCount;

    const GLsizeiptr particleBytes = (GLsizeiptr) ((size_t) poolCap * sizeof (GpuSimParticle));
    const GLsizeiptr forceBytes = (GLsizeiptr) (gpuForcePack.size() * sizeof (GpuForceMod));
    const GLsizeiptr instanceBytes = (GLsizeiptr) ((size_t) juce::jmax (1, aliveCount) * sizeof (GpuInstance));
    // Death list must cover the whole pool — a fixed 8k cap left GPU-dead slots stuck on CPU.
    const int deathCap = poolCap;
    const GLsizeiptr deathBytes = (GLsizeiptr) ((size_t) deathCap * sizeof (int));

    // Grow force/instance/death SSBOs (fully rewritten each frame — wipe OK).
    // Particle SSBO grows with pool (copy-preserve above).
    auto ensureCap = [] (unsigned int buf, size_t& cap, GLsizeiptr need, GLenum usage)
    {
        if (need <= 0)
            return;
        if ((size_t) need > cap)
        {
            const size_t grown = juce::jmax ((size_t) need, cap + cap / 2 + 65536);
            glBindBuffer (GL_SHADER_STORAGE_BUFFER, buf);
            glBufferData (GL_SHADER_STORAGE_BUFFER, (GLsizeiptr) grown, nullptr, usage);
            cap = grown;
        }
    };

    // Grow particle SSBO without CPU readback (was a multi-MB glGetBufferSubData stall
    // every pool expand — felt like random stutters even when average FPS was fine).
    if ((size_t) particleBytes > gpuParticleSsboCap)
    {
        const size_t oldCap = gpuParticleSsboCap;
        const size_t grown = juce::jmax ((size_t) particleBytes,
                                         oldCap + oldCap / 2 + (size_t) (8192 * sizeof (GpuSimParticle)));
        GLuint newSsbo = 0;
        glGenBuffers (1, &newSsbo);
        if (newSsbo != 0)
        {
            glBindBuffer (GL_COPY_WRITE_BUFFER, newSsbo);
            glBufferData (GL_COPY_WRITE_BUFFER, (GLsizeiptr) grown, nullptr, GL_DYNAMIC_COPY);
            if (oldCap > 0 && particleSsbo != 0 && gpuPoolResident)
            {
                glBindBuffer (GL_COPY_READ_BUFFER, particleSsbo);
                glCopyBufferSubData (GL_COPY_READ_BUFFER, GL_COPY_WRITE_BUFFER,
                                     0, 0, (GLsizeiptr) oldCap);
                glBindBuffer (GL_COPY_READ_BUFFER, 0);
            }
            else if (oldCap > 0 && ! gpuPoolResident)
            {
                // No live GPU data to preserve.
            }
            glBindBuffer (GL_COPY_WRITE_BUFFER, 0);
            if (particleSsbo != 0)
                glDeleteBuffers (1, &particleSsbo);
            particleSsbo = newSsbo;
            gpuParticleSsboCap = grown;
            if (oldCap == 0 || ! gpuPoolResident)
            {
                gpuPoolResident = false;
                gpuResidentCapacity = 0;
            }
        }
        else
        {
            // Fallback: orphan grow (lose resident; re-seed from CPU next block).
            glBindBuffer (GL_SHADER_STORAGE_BUFFER, particleSsbo);
            glBufferData (GL_SHADER_STORAGE_BUFFER, (GLsizeiptr) grown, nullptr, GL_DYNAMIC_COPY);
            gpuParticleSsboCap = grown;
            gpuPoolResident = false;
            gpuResidentCapacity = 0;
        }
    }

    ensureCap (forceSsbo, gpuForceSsboCap, forceBytes, GL_DYNAMIC_DRAW);
    ensureCap (instanceSsbo, gpuInstanceSsboCap, instanceBytes, GL_DYNAMIC_COPY);
    ensureCap (deathSsbo, gpuDeathSsboCap, deathBytes, GL_DYNAMIC_COPY);

    glBindBuffer (GL_SHADER_STORAGE_BUFFER, particleSsbo);

    if (! gpuPoolResident)
    {
        // First resident frame only: seed from CPU. Guard alloc so a big pool cannot crash the host.
        try
        {
            gpuSimPack.assign ((size_t) poolCap, GpuSimParticle {});
            for (int i = 0; i < poolCap; ++i)
                fillGpuParticleFromPool (i, freeMode, gpuSimPack[(size_t) i]);
            glBufferSubData (GL_SHADER_STORAGE_BUFFER, 0, particleBytes, gpuSimPack.data());
            gpuPoolResident = true;
            gpuResidentCapacity = poolCap;
        }
        catch (...)
        {
            // Fall back to CPU integrate this frame; try GPU again later.
            gpuSimPack.clear();
            gpuPoolResident = false;
            gpuResidentCapacity = 0;
            cpuIntegrateAll (dt, scales, freeMode);
            return;
        }
    }
    else if (gpuResidentCapacity < poolCap)
    {
        // Pool grew: zero only the new logical slots in one upload (not per-slot SubData spam).
        const int newCount = poolCap - gpuResidentCapacity;
        if (newCount > 0)
        {
            try
            {
                std::vector<GpuSimParticle> fresh ((size_t) newCount, GpuSimParticle {});
                for (int i = 0; i < newCount; ++i)
                    fresh[(size_t) i].poolIndex = (float) (gpuResidentCapacity + i);
                glBufferSubData (GL_SHADER_STORAGE_BUFFER,
                                 (GLintptr) ((size_t) gpuResidentCapacity * sizeof (GpuSimParticle)),
                                 (GLsizeiptr) ((size_t) newCount * sizeof (GpuSimParticle)),
                                 fresh.data());
            }
            catch (...)
            {
                // Leave capacity as-is; births will patch individual slots.
            }
        }
        gpuResidentCapacity = poolCap;
    }
    else if (gpuResidentCapacity > poolCap)
    {
        gpuResidentCapacity = poolCap;
    }

    // Sparse uploads: births/kills (full slot) — all pending (no artificial drop).
    for (const auto& patch : gpuSpawnPatches)
    {
        if (patch.first < 0 || patch.first >= poolCap)
            continue;
        glBufferSubData (GL_SHADER_STORAGE_BUFFER,
                         (GLintptr) ((size_t) patch.first * sizeof (GpuSimParticle)),
                         (GLsizeiptr) sizeof (GpuSimParticle),
                         &patch.second);
    }
    gpuSpawnPatches.clear();

    // Height is baked at birth — drop any stale field patches (must never rewrite py/targetY).
    gpuFieldPatches.clear();

    glBindBuffer (GL_SHADER_STORAGE_BUFFER, forceSsbo);
    glBufferSubData (GL_SHADER_STORAGE_BUFFER, 0, forceBytes, gpuForcePack.data());

    // Size counter buffers once per context; SubData every frame (avoids VRAM fragmentation).
    {
        uint32_t zero = 0;
        if (! gpuCounterBufsSized)
        {
            glBindBuffer (GL_SHADER_STORAGE_BUFFER, counterSsbo);
            glBufferData (GL_SHADER_STORAGE_BUFFER, sizeof (uint32_t), &zero, GL_DYNAMIC_COPY);
            glBindBuffer (GL_SHADER_STORAGE_BUFFER, deathCountSsbo);
            glBufferData (GL_SHADER_STORAGE_BUFFER, sizeof (uint32_t), &zero, GL_DYNAMIC_COPY);
            gpuCounterBufsSized = true;
        }
        else
        {
            glBindBuffer (GL_SHADER_STORAGE_BUFFER, counterSsbo);
            glBufferSubData (GL_SHADER_STORAGE_BUFFER, 0, sizeof (uint32_t), &zero);
            glBindBuffer (GL_SHADER_STORAGE_BUFFER, deathCountSsbo);
            glBufferSubData (GL_SHADER_STORAGE_BUFFER, 0, sizeof (uint32_t), &zero);
        }
    }

    const int n = poolCap; // integrate whole allocated pool; dead slots early-out
    const float historyScrollX = pendingHistoryScrollX;
    pendingHistoryScrollX = 0.0f;

    glUseProgram (computeProgram);
    if (computeUCount >= 0) glUniform1i (computeUCount, n);
    if (computeUForceCount >= 0) glUniform1i (computeUForceCount, realForceCount);
    if (computeUDt >= 0) glUniform1f (computeUDt, dt);
    if (computeUSimTime >= 0) glUniform1f (computeUSimTime, simTime);
    if (computeUDeathCap >= 0) glUniform1i (computeUDeathCap, deathCap);
    if (computeUHistoryScrollX >= 0) glUniform1f (computeUHistoryScrollX, historyScrollX);
    glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, particleSsbo);
    glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 1, forceSsbo);
    glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 4, deathSsbo);
    glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 5, deathCountSsbo);
    glDispatchCompute ((GLuint) ((n + 63) / 64), 1, 1);
    glMemoryBarrier (GL_SHADER_STORAGE_BARRIER_BIT);

    if (gpuCompactReady && compactProgram != 0)
    {
        glUseProgram (compactProgram);
        if (compactUCount >= 0) glUniform1i (compactUCount, n);
        const int uBase = glGetUniformLocation (compactProgram, "uBaseSize");
        if (uBase >= 0) glUniform1f (uBase, owner.particleSize);
        glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 0, particleSsbo);
        glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 2, instanceSsbo);
        glBindBufferBase (GL_SHADER_STORAGE_BUFFER, 3, counterSsbo);
        glDispatchCompute ((GLuint) ((n + 63) / 64), 1, 1);
        glMemoryBarrier ((GLbitfield) GL_SHADER_STORAGE_BARRIER_BIT
                         | (GLbitfield) GL_VERTEX_ATTRIB_ARRAY_BARRIER_BIT);

        uint32_t drawn = 0;
        glBindBuffer (GL_SHADER_STORAGE_BUFFER, counterSsbo);
        glGetBufferSubData (GL_SHADER_STORAGE_BUFFER, 0, sizeof (uint32_t), &drawn);
        gpuDrawnInstances = (int) juce::jmin (drawn, (uint32_t) n);
        gpuInstancesValid = gpuDrawnInstances > 0;
    }

    // Death list only — free-list sync. Positions stay on GPU (no MB/frame readback).
    uint32_t deathCount = 0;
    glBindBuffer (GL_SHADER_STORAGE_BUFFER, deathCountSsbo);
    glGetBufferSubData (GL_SHADER_STORAGE_BUFFER, 0, sizeof (uint32_t), &deathCount);
    deathCount = juce::jmin (deathCount, (uint32_t) deathCap);
    if (deathCount > 0)
    {
        gpuDeathScratch.resize ((size_t) deathCount);
        glBindBuffer (GL_SHADER_STORAGE_BUFFER, deathSsbo);
        glGetBufferSubData (GL_SHADER_STORAGE_BUFFER, 0,
                            (GLsizeiptr) (deathCount * sizeof (int)),
                            gpuDeathScratch.data());
        for (uint32_t di = 0; di < deathCount; ++di)
        {
            const int pi = gpuDeathScratch[(size_t) di];
            if (pi >= 0 && pi < (int) pool.size() && pool[(size_t) pi].alive)
                markDead (pi);
        }
    }

    glBindBuffer (GL_SHADER_STORAGE_BUFFER, 0);
    glUseProgram (0);
    juce::ignoreUnused (freeMode);
}

void Spec3DParticleSystem::gpuIntegrateAndReadback (float dt, const ForceModScales& scales, bool freeMode)
{
    // Back-compat entry: use dense compact path.
    gpuIntegrateAndCompact (dt, scales, freeMode);
}

void Spec3DParticleSystem::drawInstancedMeshesFromGpu (const juce::Matrix3D<float>& projection,
                                                       const juce::Matrix3D<float>& view)
{
    using namespace juce::gl;
    if (meshProgram == nullptr || meshVbo == 0 || instanceSsbo == 0 || gpuDrawnInstances <= 0)
        return;

    const bool useCube = owner.particleMeshShape == ParticleMeshShape::cube;
    const int indexCount = useCube ? cubeIndexCount : sphereIndexCount;
    const int indexOff = useCube ? cubeIndexOffset : sphereIndexOffset;
    if (indexCount <= 0) return;

    const float az = juce::degreesToRadians (owner.lightAzimuthDeg);
    const float el = juce::degreesToRadians (juce::jlimit (5.0f, 89.0f, owner.lightElevationDeg));
    juce::Vector3D<float> lightWorld { std::cos (el) * std::sin (az), std::sin (el), std::cos (el) * std::cos (az) };
    juce::Vector3D<float> right, up, forward;
    owner.cameraBasis (right, up, forward);
    const float lx = lightWorld.x * right.x + lightWorld.y * right.y + lightWorld.z * right.z;
    const float ly = lightWorld.x * up.x + lightWorld.y * up.y + lightWorld.z * up.z;
    const float lz = -(lightWorld.x * forward.x + lightWorld.y * forward.y + lightWorld.z * forward.z);
    const float llen = juce::jmax (1.0e-5f, std::sqrt (lx * lx + ly * ly + lz * lz));

    meshProgram->use();
    if (uMeshProj) uMeshProj->setMatrix4 (projection.mat, 1, false);
    if (uMeshView) uMeshView->setMatrix4 (view.mat, 1, false);
    if (uMeshEmissiveMode) uMeshEmissiveMode->set (owner.particleEmissiveEnabled ? 1.0f : 0.0f);
    if (uMeshEmissiveStr) uMeshEmissiveStr->set (owner.particleEmissiveStrength);
    if (uMeshRough) uMeshRough->set (owner.particleRoughness);
    if (uMeshMetal) uMeshMetal->set (owner.particleMetalness);
    if (uMeshSpec) uMeshSpec->set (owner.particleSpecular);
    if (uMeshEnergyConserve) uMeshEnergyConserve->set (owner.energyConservingEnabled ? 1.0f : 0.0f);
    if (uMeshLightAmt) uMeshLightAmt->set (owner.lightingEnabled ? owner.lightingAmount : 0.0f);
    if (uMeshLightDir) uMeshLightDir->set (lx / llen, ly / llen, lz / llen);
    if (uMeshLightCol) uMeshLightCol->set (owner.lightColour.getFloatRed(),
                                           owner.lightColour.getFloatGreen(),
                                           owner.lightColour.getFloatBlue());

    glBindBuffer (GL_ARRAY_BUFFER, meshVbo);
    glBindBuffer (GL_ELEMENT_ARRAY_BUFFER, meshIbo);
    const GLsizei meshStride = (GLsizei) sizeof (MeshVert);
    if (aMeshPos && aMeshPos->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aMeshPos->attributeID);
        glVertexAttribPointer ((GLuint) aMeshPos->attributeID, 3, GL_FLOAT, GL_FALSE, meshStride,
                               (void*) offsetof (MeshVert, px));
        glVertexAttribDivisor ((GLuint) aMeshPos->attributeID, 0);
    }
    if (aMeshNrm && aMeshNrm->attributeID >= 0)
    {
        glEnableVertexAttribArray ((GLuint) aMeshNrm->attributeID);
        glVertexAttribPointer ((GLuint) aMeshNrm->attributeID, 3, GL_FLOAT, GL_FALSE, meshStride,
                               (void*) offsetof (MeshVert, nx));
        glVertexAttribDivisor ((GLuint) aMeshNrm->attributeID, 0);
    }

    // Instance attributes from GPU compact SSBO (same layout as GpuInstance).
    glBindBuffer (GL_ARRAY_BUFFER, instanceSsbo);
    const GLsizei iStride = (GLsizei) sizeof (GpuInstance);
    auto enableInst = [&] (auto& attr, int comps, size_t off)
    {
        if (attr && attr->attributeID >= 0)
        {
            glEnableVertexAttribArray ((GLuint) attr->attributeID);
            glVertexAttribPointer ((GLuint) attr->attributeID, comps, GL_FLOAT, GL_FALSE, iStride, (void*) off);
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
                             (GLsizei) gpuDrawnInstances);

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
