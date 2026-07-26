#pragma once

#include <cmath>

/**
    Coarse bandpass-bank geometry for spectral dynamics (S).

    Realtime path is a small bank of IIR bandpasses hard-capped so CPU stays cheap.
    No FFT in the audio path.

    Placement is a global / fixed log lattice with Q as a soft mask:
    - Slices are always log-spaced across the hearing-range grid (20 Hz…maxHz).
      Band freq + Q do NOT rebuild a local aperture bank — they soft-mask GR
      and drive active-set CPU gating (narrow Q → few slices process).
    - SpectralResHz (0.5–2.0) sets how many log-spaced slices tile that grid
      (budget 48 @ 2.0 Hz → 128 @ 0.5 Hz). Each BP’s bandwidth scales with its
      centre (constant-Q packing).
    - The EQ-Q mask depth-weights GR continuously (edges → ~0); gating turns
      far slices fully off for CPU (expanded σ + hysteresis).
*/
namespace SpectralBinning
{
    /** Finest resolution (maps to densest BP budget). */
    constexpr float kMinBandwidthHz = 0.5f;

    /**
        Coarsest SpectralResHz (slider right end).
        Chosen as 2.0 Hz so travel stays in the useful fine band (~0.5–1.5)
        with a little headroom; lower values → more bandpasses within the Q footprint.
    */
    constexpr float kTargetBandwidthHz = 2.0f;

    /** Default SpectralResHz — mid useful zone. */
    constexpr float kDefaultBandwidthHz = 1.0f;

    /**
        Absolute hard cap / bank array size across ALL S bands combined.
        Runtime budget scales with resolution (see bandpassBudgetForBandwidth):
        2.0 Hz → 48, 0.5 Hz → 128, so finer SpectralResHz actually builds more BPs.
    */
    constexpr int kMaxBandpasses = 128;

    /** Coarsest resolution budget (at kTargetBandwidthHz / 2.0 Hz). */
    constexpr int kMinBandpassBudget = 48;

    /**
        Historical widen factor on the musical Q span. Placement now follows the
        GR mask (~2.45σ in log-f, still ∝ 1/Q); kept for documentation / tuning.
    */
    constexpr float kQSpanWiden = 1.2f;

    /**
        Soft floor for published / idle decisions only. Audible GR still uses a
        continuous mask; CPU gating uses the activation thresholds below.
    */
    constexpr float kMaskEpsilon = 0.001f;

    /**
        Active-set CPU gating (lookahead / hysteresis) — does not change GR depth.
        Activation uses a wider log-σ than the GR mask so IIR + envelopes warm up
        before the audible region and linger briefly when the handle leaves.
        Multi-band: each banked slice is gated by its owning slot's expanded mask;
        the processed set is the union of those per-band active sets.

        Anti-zipper: gateWanted (hysteresis) drives a smoothed gateGain 0..1 that
        scales the slice's GR mix. Filters stay warm while gain > ε; hard-skip and
        cold IIR reset only after fade-out (or on cold re-arm).
    */
    /** Widen GR σ for activation lookahead (~1.55× buffer in log-f). */
    constexpr float kActivationSigmaScale = 1.55f;
    /** Turn a slice on when expanded mask ≥ this (ahead of audible GR). */
    constexpr float kActivateMaskThreshold = 0.035f;
    /** Begin hold / allow off when expanded mask < this (wider hysteresis). */
    constexpr float kDeactivateMaskThreshold = 0.01f;
    /** Stay wanted this many process blocks after falling below deactivate. */
    constexpr int kGateHoldBlocks = 12;
    /** Fade gateGain 0↔1 over this many ms (GR path anti-zipper). */
    constexpr float kGateFadeMs = 10.0f;
    /**
        Per-sample smooth of grLinear toward the block GR target.
        Block-rate GR steps zipper badly on low-fc BPs (long ringing + unstable
        per-block RMS); this is the main anti-click for that path.
    */
    constexpr float kGrSmoothMs = 8.0f;
    /** Hard-skip / treat as fully off below this smoothed gate. */
    constexpr float kGateGainEpsilon = 1.0e-4f;

    /** |GR| below this for several blocks → duty-cycle skip that BP. */
    constexpr float kIdleGrDb = 0.05f;

    /**
        Low-fc envelope floor: at least this many periods of the BP centre
        before attack/release can be that fast. Stops block-RMS chatter when
        a block holds ≪ 1 cycle (e.g. 40 Hz @ 48 kHz / 512).
    */
    constexpr float kMinEnvelopePeriods = 2.5f;
    /** Cap on the period-derived envelope floor (ms). */
    constexpr float kMaxPeriodEnvelopeMs = 120.0f;

    /**
        LF/HF pack: warp uniform log-lattice parameter t∈[0,1] so more slices
        land at the low or high end. gamma > 1 → stronger bias (1.75 is musical).
    */
    constexpr float kPackWarpGamma = 1.75f;

    enum class PackMode : int
    {
        flat = 0,
        lf = 1,
        hf = 2
    };

    /** Map uniform slice edge t to packed t (monotonic). Flat = identity. */
    inline float warpLatticeT (float t, PackMode mode,
                               float gamma = kPackWarpGamma) noexcept
    {
        const float u = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const float g = gamma < 1.0f ? 1.0f : gamma;
        switch (mode)
        {
            case PackMode::lf:  return std::pow (u, g);                 // denser lows
            case PackMode::hf:  return 1.0f - std::pow (1.0f - u, g);   // denser highs
            case PackMode::flat:
            default:            return u;
        }
    }

    /** Quiet blocks before a BP may sleep; sleep duration in process blocks. */
    constexpr int kQuietBlocksBeforeSleep = 4;
    constexpr int kSleepBlocks = 4;

    /** Publish UI GR curve every N process blocks. */
    constexpr int kPublishBlockStride = 2;

    inline float clampBandwidthHz (float hz) noexcept
    {
        return hz < kMinBandwidthHz ? kMinBandwidthHz
             : (hz > kTargetBandwidthHz ? kTargetBandwidthHz : hz);
    }

    /**
        Runtime BP budget for SpectralResHz: 48 @ 2.0 Hz → 128 @ 0.5 Hz
        (linear in 1/bw). Count tiles the global hearing-range lattice; each
        slice’s Hz width then scales with its centre (constant-Q packing).
        Narrow Q saves CPU via gating, not by shrinking this budget.
    */
    inline int bandpassBudgetForBandwidth (float bandwidthHz) noexcept
    {
        const float bw = clampBandwidthHz (bandwidthHz);
        const float invBw = 1.0f / bw;
        const float invCoarse = 1.0f / kTargetBandwidthHz;
        const float invFine = 1.0f / kMinBandwidthHz;
        const float t = (invBw - invCoarse) / (invFine - invCoarse);
        const float tClamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const int budget = (int) std::lround (
            (double) kMinBandpassBudget
            + (double) tClamped * (double) (kMaxBandpasses - kMinBandpassBudget));
        return budget < kMinBandpassBudget ? kMinBandpassBudget
             : (budget > kMaxBandpasses ? kMaxBandpasses : budget);
    }
}
