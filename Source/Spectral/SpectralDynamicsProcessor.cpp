#include "SpectralDynamicsProcessor.h"
#include "../DynamicEq.h"

void SpectralDynamicsProcessor::prepare (double newSampleRate, int maximumBlockSize, int channels)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numChannels = juce::jmax (1, channels);
    maxBlockSize = juce::jmax (1, maximumBlockSize);

    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) maxBlockSize;
    spec.numChannels = 1;

    for (auto& unit : bank)
    {
        unit.filterL.prepare (spec);
        unit.filterR.prepare (spec);
        unit.filterDetect.prepare (spec);
        unit.filterL.reset();
        unit.filterR.reset();
        unit.filterDetect.reset();
        unit.active = false;
        unit.gateActive = false;
        unit.gateGain = 0.0f;
        unit.gateHoldBlocks = 0;
        unit.envelopeDb = DynamicEq::kSilenceFloorDb;
        unit.grLinear = 1.0f;
        unit.grTarget = 1.0f;
        unit.grDb = 0.0f;
        unit.lastCenterHz = -1.0f;
        unit.lastQ = -1.0f;
        unit.idleBlocks = 0;
        unit.sleepBlocksRemaining = 0;
    }

    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        publishedIndex[(size_t) s].store (0, std::memory_order_relaxed);
        publishedPeakGrDb[(size_t) s].store (0.0f, std::memory_order_relaxed);
        for (auto& buf : publishedGr[(size_t) s])
        {
            buf.count = 0;
            buf.centerHz.fill (0.0f);
            buf.grDb.fill (0.0f);
        }
    }

    activeBandpassCount = 0;
    bankDirty = true;
    wasActive = false;
    publishBlockCounter = 0;
}

void SpectralDynamicsProcessor::reset()
{
    for (auto& unit : bank)
    {
        unit.filterL.reset();
        unit.filterR.reset();
        unit.filterDetect.reset();
        unit.envelopeDb = DynamicEq::kSilenceFloorDb;
        unit.grLinear = 1.0f;
        unit.grTarget = 1.0f;
        unit.grDb = 0.0f;
        unit.sumSq = 0.0f;
        unit.gateActive = false;
        unit.gateGain = 0.0f;
        unit.gateHoldBlocks = 0;
        unit.idleBlocks = 0;
        unit.sleepBlocksRemaining = 0;
    }

    publishBlockCounter = 0;
    wasActive = false;

    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        publishedPeakGrDb[(size_t) s].store (0.0f, std::memory_order_relaxed);
        for (auto& buf : publishedGr[(size_t) s])
        {
            buf.count = 0;
            buf.grDb.fill (0.0f);
        }
    }
}

void SpectralDynamicsProcessor::clearBands() noexcept
{
    // EqProcessor clearBands()+setBand() every block. Do NOT force bankDirty here —
    // setBand only dirties on real setting changes; process() dirties if a slot drops.
    activeBandCount = 0;

    for (auto& band : bands)
    {
        band.wasActive = band.active;
        band.active = false;
        band.settings.enabled = false;
    }
}

float SpectralDynamicsProcessor::safeMaxCenterHz() const noexcept
{
    // Usable BP range: min(20 kHz display limit, 0.45×sr) — not Nyquist/4 or any ~5 kHz cap.
    const float bySr = (float) sampleRate * 0.45f;
    return juce::jmax (20.0f, juce::jmin (20000.0f, bySr));
}

void SpectralDynamicsProcessor::computeInfluenceRange (BandSlot& slot) const noexcept
{
    // Q-derived soft-mask footprint in log-f. Global lattice mode uses this for
    // diagnostics / gating only; SpectralPerBandLattice mode also uses it as the
    // local placement span. Freq + Q still soft-mask GR either way.
    const auto& s = slot.settings;
    const float maxHz = safeMaxCenterHz();
    const float fc = juce::jlimit (20.0f, maxHz, s.frequencyHz);
    const float q = juce::jmax (0.05f, s.q);

    const float sigma = juce::jmax (0.04f, 0.55f / q);
    // Soft-mask footprint out to ~2.45σ (mask ≈ 0.05).
    const float logHalf = sigma * 2.45f;

    // Shelves: same relative width at the knee, slight bias onto the shelf side.
    float loMul = std::exp (-logHalf);
    float hiMul = std::exp (+logHalf);
    switch (s.shape)
    {
        case SpectralDynamics::BandShape::highShelf:
            loMul = std::exp (-0.8f * logHalf);
            hiMul = std::exp (+1.2f * logHalf);
            break;
        case SpectralDynamics::BandShape::lowShelf:
            loMul = std::exp (-1.2f * logHalf);
            hiMul = std::exp (+0.8f * logHalf);
            break;
        case SpectralDynamics::BandShape::bell:
        default:
            break;
    }

    float fLo = fc * loMul;
    float fHi = fc * hiMul;

    fLo = juce::jlimit (20.0f, maxHz, fLo);
    fHi = juce::jlimit (20.0f, maxHz, fHi);
    if (fHi < fLo)
        std::swap (fLo, fHi);

    // Tiny floor so a single BP still has a valid log interval.
    if (fHi <= fLo * 1.001f)
    {
        fLo = juce::jmax (20.0f, fc * 0.97f);
        fHi = juce::jmin (maxHz, fc * 1.03f);
        if (fHi <= fLo)
            fHi = juce::jmin (maxHz, fLo * 1.03f);
    }

    slot.fLo = fLo;
    slot.fHi = fHi;
}

void SpectralDynamicsProcessor::setPerBandLatticeEnabled (bool enabled) noexcept
{
    if (perBandLatticeEnabled == enabled)
        return;
    perBandLatticeEnabled = enabled;
    bankDirty = true;
}

void SpectralDynamicsProcessor::setBand (int slot, const SpectralDynamics::BandSettings& settings) noexcept
{
    if (slot < 0 || slot >= SpectralDynamics::kNumSlots)
        return;

    auto& band = bands[(size_t) slot];

    // Amount ~0 → idle (no bank cost). Band gain is unrelated — static makeup only.
    if (! settings.enabled || settings.amount < SpectralDynamics::kAmountEpsilon)
    {
        // Slot stays inactive (clearBands already cleared active). Rebuild if it was live.
        if (band.wasActive)
            bankDirty = true;
        return;
    }

    // Always persist incoming settings (incl. frequency) so nothing stays at the
    // BandSettings default (1000 Hz) or a stale handle position.
    // Default lattice: rebuild on Res / pack / arm only (global 20…maxHz grid).
    // Per-band lattice mode: freq / Q / shape also retile the local aperture.
    constexpr float freqEps = 1.0e-3f;
    constexpr float eps = 1.0e-3f;

    const bool justArmed = ! band.wasActive;
    const bool maskParamsChanged = justArmed
        || std::abs (band.settings.q - settings.q) >= eps
        || std::abs (band.settings.frequencyHz - settings.frequencyHz) >= freqEps
        || band.settings.shape != settings.shape;
    const bool latticeChanged = justArmed
        || std::abs (band.settings.bandwidthHz - settings.bandwidthHz) >= eps
        || band.settings.pack != settings.pack
        || (perBandLatticeEnabled && maskParamsChanged);
    const bool amountChanged = std::abs (band.settings.amount - settings.amount) >= eps;
    const bool expandChanged = band.settings.expand != settings.expand;
    const bool detectSrcChanged = band.settings.detectFromSidechain != settings.detectFromSidechain;

    band.settings = settings;
    band.settings.enabled = true;
    band.active = true;
    ++activeBandCount;

    computeInfluenceRange (band);

    if (latticeChanged)
    {
        bankDirty = true;
        return;
    }

    if (maskParamsChanged || amountChanged || expandChanged || detectSrcChanged)
    {
        // Soft-mask / signed max only — centres stay on the (global or local) lattice.
        const float signedMax = (settings.expand ? 1.0f : -1.0f)
                              * settings.amount * SpectralDynamics::kMaxCutDb;
        for (auto& unit : bank)
        {
            if (! unit.active || unit.ownerSlot != slot)
                continue;
            if (maskParamsChanged)
                unit.mask = bandMask (unit.centerHz, settings);
            unit.maxCutDb = signedMax;
            unit.detectFromSidechain = settings.detectFromSidechain;
        }
    }
}

float SpectralDynamicsProcessor::bandMask (float frequencyHz, const SpectralDynamics::BandSettings& settings,
                                           float sigmaScale) const noexcept
{
    // Soft Q-mask in log-frequency, always peaked at the handle fc (not aperture
    // linear mid, which sits below fc on a log axis for constant-Q bands).
    // sigmaScale > 1 widens σ for activation lookahead (CPU gate only).
    const float fc = juce::jlimit (20.0f, safeMaxCenterHz(), settings.frequencyHz);
    const float f = juce::jmax (1.0f, frequencyHz);
    const float q = juce::jmax (0.05f, settings.q);
    const float scale = juce::jmax (1.0f, sigmaScale);
    const float sigma = juce::jmax (0.04f, 0.55f / q) * scale;
    const float x = std::log (f / fc) / sigma;

    switch (settings.shape)
    {
        case SpectralDynamics::BandShape::highShelf:
        {
            const float step = 0.5f * (1.0f + std::tanh (x));
            const float bell = std::exp (-0.5f * x * x);
            return juce::jmax (step, bell * 0.35f);
        }
        case SpectralDynamics::BandShape::lowShelf:
        {
            const float step = 0.5f * (1.0f + std::tanh (-x));
            const float bell = std::exp (-0.5f * x * x);
            return juce::jmax (step, bell * 0.35f);
        }
        case SpectralDynamics::BandShape::bell:
        default:
            return std::exp (-0.5f * x * x);
    }
}

void SpectralDynamicsProcessor::clearUnitRuntimeState (BandpassUnit& unit) noexcept
{
    unit.filterL.reset();
    unit.filterR.reset();
    unit.filterDetect.reset();
    unit.envelopeDb = DynamicEq::kSilenceFloorDb;
    unit.grLinear = 1.0f;
    unit.grTarget = 1.0f;
    unit.grDb = 0.0f;
    unit.sumSq = 0.0f;
    unit.idleBlocks = 0;
    unit.sleepBlocksRemaining = 0;
}

bool SpectralDynamicsProcessor::unitIsWarm (const BandpassUnit& unit) noexcept
{
    return unit.gateActive || unit.gateGain > SpectralBinning::kGateGainEpsilon;
}

void SpectralDynamicsProcessor::advanceGateGain (BandpassUnit& unit, float stepPerSample) noexcept
{
    const float target = unit.gateActive ? 1.0f : 0.0f;
    const float step = juce::jmax (0.0f, stepPerSample);
    if (unit.gateGain < target)
        unit.gateGain = juce::jmin (target, unit.gateGain + step);
    else if (unit.gateGain > target)
        unit.gateGain = juce::jmax (target, unit.gateGain - step);

    if (! unit.gateActive && unit.gateGain <= SpectralBinning::kGateGainEpsilon)
    {
        unit.gateGain = 0.0f;
        unit.grLinear = 1.0f;
        unit.grTarget = 1.0f;
        unit.grDb = 0.0f;
        unit.sumSq = 0.0f;
        unit.envelopeDb = DynamicEq::kSilenceFloorDb;
        unit.idleBlocks = 0;
        unit.sleepBlocksRemaining = 0;
    }
}

void SpectralDynamicsProcessor::updateActiveSetGates() noexcept
{
    // Per banked slice: expanded (lookahead) mask → gateWanted (gateActive) with
    // activate/hold hysteresis. gateGain fades the GR path; IIR is cleared only
    // on cold re-arm (gain already ~0). Audible GR depth still uses unit.mask (σ=1).
    for (int i = 0; i < SpectralBinning::kMaxBandpasses; ++i)
    {
        auto& unit = bank[(size_t) i];
        if (! unit.active)
        {
            unit.gateActive = false;
            unit.gateGain = 0.0f;
            unit.gateHoldBlocks = 0;
            continue;
        }

        float actMask = 0.0f;
        if (unit.ownerSlot >= 0 && unit.ownerSlot < SpectralDynamics::kNumSlots
            && bands[(size_t) unit.ownerSlot].active)
        {
            actMask = bandMask (unit.centerHz,
                                bands[(size_t) unit.ownerSlot].settings,
                                SpectralBinning::kActivationSigmaScale);
        }

        if (! unit.gateActive)
        {
            if (actMask >= SpectralBinning::kActivateMaskThreshold)
            {
                // Cold arm only — never dump IIR into the mix mid-stream.
                if (unit.gateGain <= SpectralBinning::kGateGainEpsilon)
                    clearUnitRuntimeState (unit);
                unit.gateActive = true;
                unit.gateHoldBlocks = 0;
            }
        }
        else if (actMask >= SpectralBinning::kDeactivateMaskThreshold)
        {
            unit.gateHoldBlocks = 0;
        }
        else if (++unit.gateHoldBlocks >= SpectralBinning::kGateHoldBlocks)
        {
            // Begin fade-out; keep filters/envelope warm until gateGain ≈ 0.
            unit.gateActive = false;
            unit.gateHoldBlocks = 0;
        }
    }
}

bool SpectralDynamicsProcessor::updateBandpassCoeffs (BandpassUnit& unit) noexcept
{
    constexpr float eps = 1.0e-3f;
    if (std::abs (unit.lastCenterHz - unit.centerHz) < eps && std::abs (unit.lastQ - unit.q) < eps)
        return true;

    const float maxHz = safeMaxCenterHz();
    // JUCE makeBandPass requires 0 < f <= sr/2 and Q > 0. Skip only this BP if invalid
    // (do not abort the rest of the HF region).
    if (! (sampleRate > 0.0)
        || ! (unit.centerHz > 20.0f && unit.centerHz <= maxHz)
        || ! (unit.centerHz < (float) sampleRate * 0.499f)
        || ! (unit.q > 0.05f && unit.q <= 250.0f)
        || ! std::isfinite (unit.centerHz)
        || ! std::isfinite (unit.q))
    {
        return false;
    }

    auto coeffs = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, unit.centerHz, unit.q);
    if (coeffs == nullptr)
        return false;

    *unit.filterL.coefficients = *coeffs;
    *unit.filterR.coefficients = *coeffs;
    *unit.filterDetect.coefficients = *coeffs;
    // Clear delay state so the audible effect snaps to the new fc (no old-centre ring).
    unit.filterL.reset();
    unit.filterR.reset();
    unit.filterDetect.reset();
    unit.lastCenterHz = unit.centerHz;
    unit.lastQ = unit.q;
    return true;
}

void SpectralDynamicsProcessor::rebuildBank() noexcept
{
    // Preserve gate fades across rebuilds when the same (slot, centre) returns.
    // Hard-zeroing gateGain on every Res/arm rebuild was a major click source —
    // lattice geometry is unchanged; only soft-mask ownership moves.
    struct GateSnap
    {
        float centerHz = 0.0f;
        int ownerSlot = -1;
        float gateGain = 0.0f;
        bool gateActive = false;
        float envelopeDb = DynamicEq::kSilenceFloorDb;
        float grLinear = 1.0f;
        float grTarget = 1.0f;
        float grDb = 0.0f;
    };
    std::array<GateSnap, SpectralBinning::kMaxBandpasses> snaps {};
    int numSnaps = 0;
    for (int i = 0; i < SpectralBinning::kMaxBandpasses && numSnaps < SpectralBinning::kMaxBandpasses; ++i)
    {
        const auto& unit = bank[(size_t) i];
        if (! unit.active)
            continue;
        auto& snap = snaps[(size_t) numSnaps++];
        snap.centerHz = unit.centerHz;
        snap.ownerSlot = unit.ownerSlot;
        snap.gateGain = unit.gateGain;
        snap.gateActive = unit.gateActive;
        snap.envelopeDb = unit.envelopeDb;
        snap.grLinear = unit.grLinear;
        snap.grTarget = unit.grTarget;
        snap.grDb = unit.grDb;
    }

    activeBandpassCount = 0;

    for (auto& unit : bank)
    {
        unit.active = false;
        unit.gateActive = false;
        unit.gateGain = 0.0f;
        unit.gateHoldBlocks = 0;
        unit.idleBlocks = 0;
        unit.sleepBlocksRemaining = 0;
    }

    if (activeBandCount <= 0)
        return;

    std::array<int, SpectralDynamics::kNumSlots> activeSlots {};
    int numActiveSlots = 0;
    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        if (! bands[(size_t) s].active)
            continue;
        activeSlots[(size_t) numActiveSlots++] = s;
    }

    if (numActiveSlots == 0)
        return;

    const float maxHz = safeMaxCenterHz();
    std::array<int, SpectralDynamics::kNumSlots> allocated {};
    std::array<float, SpectralDynamics::kNumSlots> placeLoHz {};
    std::array<float, SpectralDynamics::kNumSlots> placeHiHz {};
    int effectiveCap = SpectralBinning::kMaxBandpasses;

    if (perBandLatticeEnabled)
    {
        // Sandboxed local lattices — each S band tiles its Q influence range.
        // Global path below is untouched when this flag is off.
        std::array<SpectralPerBandLattice::SlotRequest, SpectralDynamics::kNumSlots> requests {};
        for (int i = 0; i < numActiveSlots; ++i)
        {
            const int s = activeSlots[(size_t) i];
            auto& band = bands[(size_t) s];
            computeInfluenceRange (band);
            auto& req = requests[(size_t) s];
            req.active = true;
            req.fLoHz = band.fLo;
            req.fHiHz = band.fHi;
            req.bandwidthHz = band.settings.bandwidthHz;
        }

        std::array<SpectralPerBandLattice::SlotPlan, SpectralDynamics::kNumSlots> plans {};
        effectiveCap = SpectralPerBandLattice::plan (requests, maxHz, plans);
        effectiveCap = juce::jlimit (0, SpectralBinning::kMaxBandpasses, effectiveCap);

        for (int i = 0; i < numActiveSlots; ++i)
        {
            const int s = activeSlots[(size_t) i];
            allocated[(size_t) s] = plans[(size_t) s].count;
            placeLoHz[(size_t) s] = plans[(size_t) s].placeLoHz;
            placeHiHz[(size_t) s] = plans[(size_t) s].placeHiHz;
        }
    }
    else
    {
        // Ideal count: SpectralResHz budget tiles the global hearing-range lattice
        // (constant-Q). Same res → same slice count; Q only gates which run.
        std::array<int, SpectralDynamics::kNumSlots> idealCounts {};
        std::array<float, SpectralDynamics::kNumSlots> targetBwHz {};
        int totalIdeal = 0;

        for (int i = 0; i < numActiveSlots; ++i)
        {
            const int s = activeSlots[(size_t) i];
            const float targetBw = SpectralBinning::clampBandwidthHz (bands[(size_t) s].settings.bandwidthHz);
            targetBwHz[(size_t) s] = targetBw;
            const int ideal = juce::jmax (1, SpectralBinning::bandpassBudgetForBandwidth (targetBw));
            idealCounts[(size_t) s] = ideal;
            totalIdeal += ideal;
        }

        // Resolution-scaled budget: finest SpectralResHz among active S bands
        // sets the total (48 @ 2.0 Hz … 128 @ 0.5 Hz).
        float finestBw = SpectralBinning::kTargetBandwidthHz;
        for (int i = 0; i < numActiveSlots; ++i)
        {
            const int s = activeSlots[(size_t) i];
            finestBw = juce::jmin (finestBw, targetBwHz[(size_t) s]);
        }
        effectiveCap = juce::jmin (SpectralBinning::kMaxBandpasses,
                                   SpectralBinning::bandpassBudgetForBandwidth (finestBw));

        if (totalIdeal <= effectiveCap)
        {
            for (int i = 0; i < numActiveSlots; ++i)
            {
                const int s = activeSlots[(size_t) i];
                allocated[(size_t) s] = idealCounts[(size_t) s];
            }
        }
        else
        {
            // Proportional share of the resolution budget; every active S band gets ≥ 1.
            int remaining = effectiveCap;
            for (int i = 0; i < numActiveSlots; ++i)
            {
                const int s = activeSlots[(size_t) i];
                allocated[(size_t) s] = 1;
                --remaining;
            }

            if (remaining > 0 && totalIdeal > numActiveSlots)
            {
                const int extraIdeal = totalIdeal - numActiveSlots;
                int assignedExtra = 0;
                for (int i = 0; i < numActiveSlots; ++i)
                {
                    const int s = activeSlots[(size_t) i];
                    const int shareIdeal = idealCounts[(size_t) s] - 1;
                    const int extra = (i == numActiveSlots - 1)
                        ? (remaining - assignedExtra)
                        : (int) std::floor ((double) remaining * (double) shareIdeal / (double) extraIdeal + 0.5);
                    const int clamped = juce::jmax (0, juce::jmin (remaining - assignedExtra, extra));
                    allocated[(size_t) s] += clamped;
                    assignedExtra += clamped;
                }
            }
        }

        for (int i = 0; i < numActiveSlots; ++i)
        {
            const int s = activeSlots[(size_t) i];
            placeLoHz[(size_t) s] = 20.0f;
            placeHiHz[(size_t) s] = maxHz;
        }
    }

    int bpIndex = 0;
    for (int i = 0; i < numActiveSlots && bpIndex < effectiveCap; ++i)
    {
        const int s = activeSlots[(size_t) i];
        auto& band = bands[(size_t) s];
        if (! perBandLatticeEnabled)
            computeInfluenceRange (band);

        const int count = juce::jmax (1, allocated[(size_t) s]);

        // Global mode: hearing-range log lattice (20…maxHz). Per-band mode: local
        // Q aperture from SpectralPerBandLattice. Soft mask + gating still apply.
        const float placeLo = placeLoHz[(size_t) s];
        const float placeHi = placeHiHz[(size_t) s];
        if (placeHi <= placeLo)
            continue;

        const float logLo = std::log (placeLo);
        const float logHi = std::log (placeHi);
        const float logSpan = logHi - logLo;

        const float signedMax = (band.settings.expand ? 1.0f : -1.0f)
                              * band.settings.amount * SpectralDynamics::kMaxCutDb;

        for (int k = 0; k < count && bpIndex < effectiveCap; ++k)
        {
            auto& unit = bank[(size_t) bpIndex];
            const float u0 = (float) k / (float) count;
            const float u1 = (float) (k + 1) / (float) count;
            // Optional LF/HF pack warps uniform log t so more slices sit low or high.
            const float t0 = SpectralBinning::warpLatticeT (u0, band.settings.pack);
            const float t1 = SpectralBinning::warpLatticeT (u1, band.settings.pack);
            // Log-spaced slice edges; geometric-mean centre (constant-Q tiling).
            const float f0 = std::exp (logLo + logSpan * t0);
            const float f1 = std::exp (logLo + logSpan * t1);
            unit.centerHz = std::sqrt (f0 * f1);
            if (unit.centerHz > maxHz)
                continue; // skip only this slice — keep placing the rest of the HF region

            // Slice Hz width from log tiling (constant-Q).
            unit.bandwidthHz = juce::jmax (unit.centerHz * (1.0f / 200.0f), (f1 - f0) * 1.15f);
            unit.q = juce::jlimit (0.5f, 200.0f, unit.centerHz / unit.bandwidthHz);

            // Soft Q-mask: Gaussian / shelf taper — edges fade, no hard cliff.
            unit.mask = bandMask (unit.centerHz, band.settings);

            unit.maxCutDb = signedMax;
            unit.ownerSlot = s;
            unit.detectFromSidechain = band.settings.detectFromSidechain;
            unit.sumSq = 0.0f;
            unit.idleBlocks = 0;
            unit.sleepBlocksRemaining = 0;
            unit.gateActive = false;
            unit.gateGain = 0.0f;
            unit.gateHoldBlocks = 0;

            // Remember geometry before coeff update — only reuse gate if IIR stays warm.
            const float prevLastC = unit.lastCenterHz;
            const float prevLastQ = unit.lastQ;

            if (! updateBandpassCoeffs (unit))
                continue; // invalid coeffs at this centre — skip BP, do not kill HF region

            const bool coeffsKept = std::abs (prevLastC - unit.centerHz) < 1.0e-3f
                                    && std::abs (prevLastQ - unit.q) < 1.0e-3f;
            if (coeffsKept)
            {
                for (int si = 0; si < numSnaps; ++si)
                {
                    const auto& snap = snaps[(size_t) si];
                    if (snap.ownerSlot != s)
                        continue;
                    if (std::abs (snap.centerHz - unit.centerHz) > 0.25f)
                        continue;
                    unit.gateGain = snap.gateGain;
                    unit.gateActive = snap.gateActive
                                      || snap.gateGain > SpectralBinning::kGateGainEpsilon;
                    unit.envelopeDb = snap.envelopeDb;
                    unit.grLinear = snap.grLinear;
                    unit.grTarget = snap.grTarget;
                    unit.grDb = snap.grDb;
                    break;
                }
            }
            else
            {
                unit.envelopeDb = DynamicEq::kSilenceFloorDb;
                unit.grLinear = 1.0f;
                unit.grTarget = 1.0f;
                unit.grDb = 0.0f;
            }

            unit.active = true;
            ++activeBandpassCount;
            ++bpIndex;
        }
    }

    bankDirty = false;
}

void SpectralDynamicsProcessor::finishBlockEnvelopes (int numSamples) noexcept
{
    if (numSamples <= 0)
        return;

    const float invN = 1.0f / (float) numSamples;

    // Pass 1: update per-BP envelopes from dry sidechain energy.
    // Low-fc BPs get a period floor on A/R so block RMS (≪ 1 cycle) cannot chatter GR.
    for (int i = 0; i < SpectralBinning::kMaxBandpasses; ++i)
    {
        auto& unit = bank[(size_t) i];
        if (! unit.active || ! unitIsWarm (unit))
            continue;

        if (unit.sleepBlocksRemaining > 0)
        {
            --unit.sleepBlocksRemaining;
            unit.grDb = 0.0f;
            unit.grTarget = 1.0f; // fade via per-sample smooth — never hard-snap grLinear
            unit.sumSq = 0.0f;
            continue;
        }

        const int slot = juce::jlimit (0, SpectralDynamics::kNumSlots - 1, unit.ownerSlot);
        const auto& settings = bands[(size_t) slot].settings;

        const float periodMs = 1000.0f / juce::jmax (1.0f, unit.centerHz);
        const float periodFloorMs = juce::jmin (
            SpectralBinning::kMaxPeriodEnvelopeMs,
            periodMs * SpectralBinning::kMinEnvelopePeriods);
        const float atkMs = juce::jmax (DynamicEq::clampAttackMs (settings.attackMs), periodFloorMs);
        const float relMs = juce::jmax (DynamicEq::clampReleaseMs (settings.releaseMs), periodFloorMs);
        const float atk = DynamicEq::coeffForTimeMs (atkMs, sampleRate, numSamples);
        const float rel = DynamicEq::coeffForTimeMs (relMs, sampleRate, numSamples);

        const float rms = std::sqrt (unit.sumSq * invN);
        const float levelDb = juce::Decibels::gainToDecibels (rms, DynamicEq::kSilenceFloorDb);
        const float coeff = levelDb > unit.envelopeDb ? atk : rel;
        unit.envelopeDb = coeff * unit.envelopeDb + (1.0f - coeff) * levelDb;
        unit.sumSq = 0.0f;
    }

    // Pass 2: per-slot neighbor contrast → resonance prominence → signed GR.
    // Slot-mean prominence follows spectral tilt (music/bass-heavy → louder LF
    // slices), so engage×mask peaked ~half a Q-width below fc. Log-spaced
    // neighbor Laplacian cancels a smooth tilt and only fires on peaks.
    // Suppress: maxCutDb < 0. Expand: maxCutDb > 0. Soft Q-mask still peaks at fc.
    std::array<std::array<int, SpectralBinning::kMaxBandpasses>, SpectralDynamics::kNumSlots> slotIdx {};
    std::array<int, SpectralDynamics::kNumSlots> slotCounts {};
    slotCounts.fill (0);

    for (int i = 0; i < SpectralBinning::kMaxBandpasses; ++i)
    {
        const auto& unit = bank[(size_t) i];
        if (! unit.active || ! unitIsWarm (unit) || unit.sleepBlocksRemaining > 0)
            continue;
        if (unit.ownerSlot < 0 || unit.ownerSlot >= SpectralDynamics::kNumSlots)
            continue;
        if (unit.envelopeDb < SpectralDynamics::kDetectFloorDb)
            continue;

        const int s = unit.ownerSlot;
        const int n = slotCounts[(size_t) s];
        // rebuildBank writes each slot's centres in ascending frequency order.
        slotIdx[(size_t) s][(size_t) n] = i;
        ++slotCounts[(size_t) s];
    }

    std::array<float, SpectralBinning::kMaxBandpasses> engageByUnit {};
    engageByUnit.fill (0.0f);

    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        const int n = slotCounts[(size_t) s];
        if (n <= 0)
            continue;

        // Single-slice banks: always-on within the Q mask.
        if (n == 1)
        {
            engageByUnit[(size_t) slotIdx[(size_t) s][0]] = 1.0f;
            continue;
        }

        for (int j = 0; j < n; ++j)
        {
            const int ui = slotIdx[(size_t) s][(size_t) j];
            const float env = bank[(size_t) ui].envelopeDb;

            float baseline = env;
            if (j == 0)
            {
                baseline = bank[(size_t) slotIdx[(size_t) s][1]].envelopeDb;
            }
            else if (j == n - 1)
            {
                baseline = bank[(size_t) slotIdx[(size_t) s][(size_t) (n - 2)]].envelopeDb;
            }
            else
            {
                const float left = bank[(size_t) slotIdx[(size_t) s][(size_t) (j - 1)]].envelopeDb;
                const float right = bank[(size_t) slotIdx[(size_t) s][(size_t) (j + 1)]].envelopeDb;
                baseline = 0.5f * (left + right);
            }

            const float prominence = env - baseline;
            engageByUnit[(size_t) ui] = juce::jlimit (
                0.0f, 1.0f, prominence / SpectralDynamics::kResonanceFullRangeDb);
        }
    }

    for (int i = 0; i < SpectralBinning::kMaxBandpasses; ++i)
    {
        auto& unit = bank[(size_t) i];
        if (! unit.active || ! unitIsWarm (unit))
            continue;

        if (unit.sleepBlocksRemaining > 0)
            continue;

        float gr = 0.0f;
        if (unit.ownerSlot >= 0 && unit.ownerSlot < SpectralDynamics::kNumSlots
            && unit.envelopeDb >= SpectralDynamics::kDetectFloorDb
            && slotCounts[(size_t) unit.ownerSlot] > 0)
        {
            // maxCutDb is signed (±Amount * kMaxCutDb); mask tapers at Q edges (peaks at fc).
            gr = unit.maxCutDb * engageByUnit[(size_t) i] * unit.mask;
        }

        unit.grDb = juce::jlimit (SpectralDynamics::kGrDbFloor,
                                  SpectralDynamics::kGrDbCeiling,
                                  gr);
        unit.grTarget = juce::Decibels::decibelsToGain (unit.grDb);

        // Idle/sleep from audible contribution (after gate fade), not raw GR.
        // Do not hard-snap grLinear — sleep only sets grTarget=1; filters keep
        // running until the smoothed path is transparent (avoids LF pops).
        const float audibleGr = std::abs (unit.grDb) * unit.gateGain;
        if (audibleGr < SpectralBinning::kIdleGrDb)
        {
            if (++unit.idleBlocks >= SpectralBinning::kQuietBlocksBeforeSleep)
            {
                unit.sleepBlocksRemaining = SpectralBinning::kSleepBlocks;
                unit.idleBlocks = 0;
                unit.grDb = 0.0f;
                unit.grTarget = 1.0f;
            }
        }
        else
        {
            unit.idleBlocks = 0;
        }
    }
}

void SpectralDynamicsProcessor::publishGrCurves() noexcept
{
    for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
    {
        const int writeIdx = 1 - publishedIndex[(size_t) s].load (std::memory_order_relaxed);
        auto& dest = publishedGr[(size_t) s][(size_t) writeIdx];
        dest.count = 0;

        float peak = 0.0f;
        if (bands[(size_t) s].active)
        {
            for (int i = 0; i < SpectralBinning::kMaxBandpasses; ++i)
            {
                const auto& unit = bank[(size_t) i];
                if (! unit.active || unit.ownerSlot != s)
                    continue;

                dest.centerHz[(size_t) dest.count] = unit.centerHz;
                // Publish smoothed audible GR so UI matches the anti-zipper mix.
                const float audibleGrDb = juce::Decibels::gainToDecibels (
                    juce::jmax (1.0e-6f, unit.grLinear), -100.0f) * unit.gateGain;
                dest.grDb[(size_t) dest.count] = audibleGrDb;
                // Strongest engagement by magnitude; keep sign for cut/boost UI.
                if (std::abs (audibleGrDb) > std::abs (peak))
                    peak = audibleGrDb;
                ++dest.count;
            }
        }

        publishedIndex[(size_t) s].store (writeIdx, std::memory_order_release);
        publishedPeakGrDb[(size_t) s].store (peak, std::memory_order_relaxed);
    }
}

void SpectralDynamicsProcessor::samplePublishedGrDb (int bandIndex, const float* frequenciesHz,
                                                    float* destDb, int numPoints) const
{
    if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
        return;

    const int slot = SpectralDynamics::slotForBandIndex (bandIndex);
    if (slot < 0)
    {
        juce::FloatVectorOperations::clear (destDb, numPoints);
        return;
    }

    const int idx = publishedIndex[(size_t) slot].load (std::memory_order_acquire);
    const auto& src = publishedGr[(size_t) slot][(size_t) juce::jlimit (0, 1, idx)];
    if (src.count <= 0)
    {
        juce::FloatVectorOperations::clear (destDb, numPoints);
        return;
    }

    const float nyquist = (float) sampleRate * 0.5f;
    const int n = src.count;

    // Reconstruct display GR on the caller's dense frequency grid (typically 1/px log-f).
    // Audio applies GR through overlapping bandpasses: H = 1 + Σ Bp_i*(g_i-1).
    // Piecewise-linear dB between sparse BP centres made sharp V notches on the sum curve;
    // Hann lobes in log-f (zero at neighbors, flat peak) match that overlap and stay C1-smooth.
    std::array<float, SpectralBinning::kMaxBandpasses> logC {};
    std::array<float, SpectralBinning::kMaxBandpasses> invHalfLog {};
    std::array<float, SpectralBinning::kMaxBandpasses> gLinMinus1 {};

    for (int k = 0; k < n; ++k)
    {
        const float c = juce::jmax (1.0f, src.centerHz[(size_t) k]);
        logC[(size_t) k] = std::log (c);

        float halfLog = 0.15f; // single-slice fallback (~±15% relative)
        if (n >= 2)
        {
            if (k == 0)
            {
                const float c1 = juce::jmax (c * 1.0001f, src.centerHz[1]);
                halfLog = juce::jmax (1.0e-4f, std::log (c1 / c));
            }
            else if (k == n - 1)
            {
                const float c0 = juce::jmax (1.0f, src.centerHz[(size_t) (k - 1)]);
                halfLog = juce::jmax (1.0e-4f, std::log (c / c0));
            }
            else
            {
                const float c0 = juce::jmax (1.0f, src.centerHz[(size_t) (k - 1)]);
                const float c1 = juce::jmax (c * 1.0001f, src.centerHz[(size_t) (k + 1)]);
                // Reach ~0 at neighboring centres on a constant-Q lattice.
                halfLog = juce::jmax (1.0e-4f, 0.5f * std::log (c1 / c0));
            }
        }

        invHalfLog[(size_t) k] = 1.0f / halfLog;
        gLinMinus1[(size_t) k] = juce::Decibels::decibelsToGain (src.grDb[(size_t) k]) - 1.0f;
    }

    // frequenciesHz is the display log grid (monotonic). Slide the active lobe window.
    int kFirst = 0;

    for (int i = 0; i < numPoints; ++i)
    {
        const float f = juce::jlimit (1.0f, nyquist, frequenciesHz[i]);
        const float logF = std::log (f);

        while (kFirst < n
               && (logF - logC[(size_t) kFirst]) * invHalfLog[(size_t) kFirst] > 1.0f)
            ++kFirst;

        float h = 1.0f;
        for (int k = kFirst; k < n; ++k)
        {
            const float x = (logF - logC[(size_t) k]) * invHalfLog[(size_t) k];
            if (x < -1.0f)
                break; // later centres are higher; no overlap yet / anymore
            if (x > 1.0f)
                continue;

            const float w = 0.5f * (1.0f + std::cos (juce::MathConstants<float>::pi * x));
            h += w * gLinMinus1[(size_t) k];
        }

        destDb[i] = juce::Decibels::gainToDecibels (juce::jmax (1.0e-6f, h), -100.0f);
    }
}

bool SpectralDynamicsProcessor::hasActiveGr (int bandIndex) const noexcept
{
    const int slot = SpectralDynamics::slotForBandIndex (bandIndex);
    if (slot < 0)
        return false;
    return std::abs (publishedPeakGrDb[(size_t) slot].load (std::memory_order_relaxed)) > 0.01f;
}

float SpectralDynamicsProcessor::getPublishedPeakGrDb (int bandIndex) const noexcept
{
    const int slot = SpectralDynamics::slotForBandIndex (bandIndex);
    if (slot < 0)
        return 0.0f;
    return publishedPeakGrDb[(size_t) slot].load (std::memory_order_relaxed);
}

SpectralDynamicsProcessor::RuntimeStats SpectralDynamicsProcessor::getRuntimeStats() const noexcept
{
    RuntimeStats s;
    s.armedSlots = publishedArmedSlots.load (std::memory_order_relaxed);
    s.bankedBandpasses = publishedBankedBandpasses.load (std::memory_order_relaxed);
    s.gatedBandpasses = publishedGatedBandpasses.load (std::memory_order_relaxed);
    s.processingBandpasses = publishedProcessingBandpasses.load (std::memory_order_relaxed);
    return s;
}

void SpectralDynamicsProcessor::process (juce::dsp::AudioBlock<float>& block,
                                         const float* detectL,
                                         const float* detectR,
                                         const float* scDetectL,
                                         const float* scDetectR)
{
    // Hard bypass: zero bandpass cost when no S bands are on.
    if (activeBandCount == 0 || block.getNumSamples() == 0)
    {
        if (wasActive)
        {
            for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
            {
                const int writeIdx = 1 - publishedIndex[(size_t) s].load (std::memory_order_relaxed);
                auto& dest = publishedGr[(size_t) s][(size_t) writeIdx];
                dest.count = 0;
                publishedIndex[(size_t) s].store (writeIdx, std::memory_order_release);
                publishedPeakGrDb[(size_t) s].store (0.0f, std::memory_order_relaxed);
            }

            for (auto& unit : bank)
            {
                unit.active = false;
                unit.gateActive = false;
                unit.gateGain = 0.0f;
                unit.gateHoldBlocks = 0;
                unit.grLinear = 1.0f;
                unit.grTarget = 1.0f;
                unit.grDb = 0.0f;
                unit.envelopeDb = DynamicEq::kSilenceFloorDb;
            }

            activeBandpassCount = 0;
            wasActive = false;
        }

        publishedArmedSlots.store (0, std::memory_order_relaxed);
        publishedBankedBandpasses.store (0, std::memory_order_relaxed);
        publishedGatedBandpasses.store (0, std::memory_order_relaxed);
        publishedProcessingBandpasses.store (0, std::memory_order_relaxed);
        return;
    }

    // A slot that was in the bank but not re-armed this block must trigger rebuild.
    for (const auto& band : bands)
    {
        if (band.wasActive && ! band.active)
        {
            bankDirty = true;
            break;
        }
    }

    if (bankDirty)
        rebuildBank();

    if (activeBandpassCount <= 0)
    {
        publishedArmedSlots.store (activeBandCount, std::memory_order_relaxed);
        publishedBankedBandpasses.store (0, std::memory_order_relaxed);
        publishedGatedBandpasses.store (0, std::memory_order_relaxed);
        publishedProcessingBandpasses.store (0, std::memory_order_relaxed);
        return;
    }

    // Expand/contract the realtime active set from soft masks (lookahead + hold).
    updateActiveSetGates();

    const int numSamples = (int) block.getNumSamples();
    const float gateStep = (float) (1.0 / juce::jmax (1.0,
        sampleRate * (double) SpectralBinning::kGateFadeMs * 0.001));
    const float grSmoothCoeff = 1.0f - std::exp (-1.0f / juce::jmax (1.0f,
        (float) sampleRate * SpectralBinning::kGrSmoothMs * 0.001f));

    int gatedCount = 0;
    int processingCount = 0;
    for (int i = 0; i < activeBandpassCount; ++i)
    {
        const auto& unit = bank[(size_t) i];
        if (! unit.active || ! unitIsWarm (unit))
            continue;
        ++gatedCount;
        if (unit.sleepBlocksRemaining <= 0
            || std::abs (unit.grLinear - 1.0f) * unit.gateGain > SpectralBinning::kGateGainEpsilon)
            ++processingCount;
    }

    publishedArmedSlots.store (activeBandCount, std::memory_order_relaxed);
    publishedBankedBandpasses.store (activeBandpassCount, std::memory_order_relaxed);
    publishedGatedBandpasses.store (gatedCount, std::memory_order_relaxed);
    publishedProcessingBandpasses.store (processingCount, std::memory_order_relaxed);

    if (! wasActive)
    {
        for (auto& unit : bank)
        {
            if (! unit.active || ! unit.gateActive)
                continue;
            clearUnitRuntimeState (unit);
            unit.gateGain = 0.0f; // fade in from silence on first arm
        }
        wasActive = true;
    }

    // Nothing warm/awake → still advance gate fades (sleepers / early-off), then envelopes.
    if (processingCount <= 0)
    {
        const float blockGateStep = gateStep * (float) numSamples;
        const float blockGrCoeff = 1.0f - std::pow (1.0f - grSmoothCoeff, (float) numSamples);
        for (int i = 0; i < activeBandpassCount; ++i)
        {
            auto& unit = bank[(size_t) i];
            if (! unit.active || ! unitIsWarm (unit))
                continue;
            // Block-rate catch-up when the sample loop is skipped (all sleeping).
            advanceGateGain (unit, blockGateStep);
            unit.grLinear += (unit.grTarget - unit.grLinear) * blockGrCoeff;
        }

        finishBlockEnvelopes (numSamples);
        if (++publishBlockCounter >= SpectralBinning::kPublishBlockStride)
        {
            publishBlockCounter = 0;
            publishGrCurves();
        }
        return;
    }

    const int channels = juce::jmin (numChannels, (int) block.getNumChannels());
    auto* left = block.getChannelPointer (0);
    auto* right = channels > 1 ? block.getChannelPointer (1) : nullptr;

    // Main-track pre-EQ detect (default). External SC bus for detectFromSidechain slots.
    const float* detL = detectL != nullptr ? detectL : left;
    const float* detR = detectR != nullptr ? detectR
                                           : (detectL != nullptr ? detectL
                                                                 : (right != nullptr ? right : left));
    const float* scL = scDetectL;
    const float* scR = scDetectR != nullptr ? scDetectR
                                            : (scDetectL != nullptr ? scDetectL : nullptr);

    // Bank is densely packed from index 0 in rebuildBank.
    const int bpLimit = activeBandpassCount;

    for (int n = 0; n < numSamples; ++n)
    {
        const float inL = left[n];
        const float inR = right != nullptr ? right[n] : inL;
        const float mainMid = 0.5f * (detL[n] + detR[n]);
        const float scMid = (scL != nullptr) ? 0.5f * (scL[n] + (scR != nullptr ? scR[n] : scL[n]))
                                             : 0.0f;
        float outL = inL;
        float outR = inR;

        for (int i = 0; i < bpLimit; ++i)
        {
            auto& unit = bank[(size_t) i];
            if (! unit.active || ! unitIsWarm (unit))
                continue;

            advanceGateGain (unit, gateStep);
            // Anti-zipper: never apply block GR as a step (LF BPs click hard).
            unit.grLinear += (unit.grTarget - unit.grLinear) * grSmoothCoeff;

            const float mix = std::abs (unit.grLinear - 1.0f) * unit.gateGain;
            // Sleep may skip only once the smoothed path is already transparent.
            if (unit.gateGain <= SpectralBinning::kGateGainEpsilon
                || (unit.sleepBlocksRemaining > 0 && mix <= SpectralBinning::kGateGainEpsilon))
                continue;

            // Detect: main pre-EQ mid, or Sidechain bus when slot has SC armed with S.
            const float detectMid = unit.detectFromSidechain ? scMid : mainMid;
            const float bpDet = unit.filterDetect.processSample (detectMid);
            unit.sumSq += bpDet * bpDet;

            // Apply signed GR to post-EQ wet bandpasses; gateGain fades the mix.
            const float bpL = unit.filterL.processSample (inL);
            const float bpR = right != nullptr ? unit.filterR.processSample (inR) : bpL;

            const float wet = (unit.grLinear - 1.0f) * unit.gateGain;
            outL += bpL * wet;
            outR += bpR * wet;
        }

        left[n] = outL;
        if (right != nullptr)
            right[n] = outR;
    }

    finishBlockEnvelopes (numSamples);

    if (++publishBlockCounter >= SpectralBinning::kPublishBlockStride)
    {
        publishBlockCounter = 0;
        publishGrCurves();
    }
}
