#pragma once

#include <JuceHeader.h>
#include "../DynamicEq.h"
#include "../FilterType.h"
#include "../EqBand.h"
#include "SpectralBinning.h"

/**
    Per-band controls for spectral dynamic processing (Pro-Q-style spectral).

    User-facing (S mode): Amount + Res + Expand + Attack/Release + Pack + optional PB.
    - Amount (0–1): how aggressively resonances inside the Q aperture are
      processed. UI: vertical slider, pull down = more (inverted LinearVertical).
    - Res: with PB on (default) each band's *SpectralResHz is independent
      (~4–128 BPs in that band's Q). With PB off, global spectralResHz is
      linked across all S bands on the shared lattice.
    - Amount (*SpectralDepth): always per-band.
    - Expand (bool): invert GR sign → boost/exaggerate resonances instead of cut.
    - Pack (global Flat/LF/HF): warps where Res budget sits on the lattice for all S bands.
    - PB / spectralPerBandLattice (default on): local lattices via
      SpectralPerBandLattice. Off = legacy global grid (Side Check-style).
    - Attack / release: shared per-band params with D mode (defaults 20 / 200 ms).

    Internal:
    - No user threshold — resonance prominence vs log-neighbor baseline drives GR.
    - Band gain is static makeup only (not a spectral target).
*/
namespace SpectralDynamics
{
    /** Default envelope times for S (overridden by per-band A/R params). */
    constexpr float attackMs = DynamicEq::attackMs;   // 20 ms
    constexpr float releaseMs = DynamicEq::releaseMs; // 200 ms

    /**
        Safety clamp for spectral GR (dB).
        Negative = suppress, positive = expand (when Expand is on).
    */
    constexpr float kGrDbFloor = -60.0f;
    constexpr float kGrDbCeiling = 60.0f;

    /** Amount: 0 = min processing, 2 = max resonance cut/boost (2× prior ceiling). */
    constexpr float kMinSpectralAmount = 0.0f;
    constexpr float kMaxSpectralAmount = 2.0f;
    constexpr float kDefaultSpectralAmount = 0.5f;

    /** Max |GR| (dB) at Amount = 1 when a slice is fully prominent (Amount 2 → 48 dB). */
    constexpr float kMaxCutDb = 24.0f;

    /**
        dB above the log-neighbor envelope baseline for full engagement.
        Smooth / tilted spectra → little GR; resonant peaks stick out → more processing.
    */
    constexpr float kResonanceFullRangeDb = 12.0f;

    /** Absolute silence floor — ignore slices this quiet (avoid noise GR). */
    constexpr float kDetectFloorDb = -90.0f;

    /** Below this Amount the slot is treated as inactive (no bank cost). */
    constexpr float kAmountEpsilon = 0.001f;

    /** Legacy aliases — param IDs still use *SpectralDepth for session stability. */
    constexpr float kMinSpectralDepth = kMinSpectralAmount;
    constexpr float kMaxSpectralDepth = kMaxSpectralAmount;
    constexpr float kDefaultSpectralDepth = kDefaultSpectralAmount;

    /**
        Slots in the shared SpectralDynamicsProcessor.
        Bank 1 (8) remains fully wired; extended banks register S params for state
        but arm only when slot < kNumSlots (Bank 1) until the lattice is scaled.
    */
    constexpr int kNumSlots = 8;

    inline juce::String spectralParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1Spectral";
            case 1: return "band2Spectral";
            case 2: return "band3Spectral";
            case 3: return "band4Spectral";
            case 4: return "highpassSpectral";
            case 5: return "lowpassSpectral";
            case 6: return "highShelfSpectral";
            case 7: return "lowShelfSpectral";
            default: return {};
        }
    }

    /** Global spectral resolution (Hz) — used when PB is off (linked mode). */
    inline constexpr const char* spectralResHzParamId() noexcept { return "spectralResHz"; }

    /** Per-band Res IDs — live when PB is on; mirrored from global only when PB is off. */
    inline juce::String spectralResHzParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SpectralResHz";
            case 1: return "band2SpectralResHz";
            case 2: return "band3SpectralResHz";
            case 3: return "band4SpectralResHz";
            case 4: return "highpassSpectralResHz";
            case 5: return "lowpassSpectralResHz";
            case 6: return "highShelfSpectralResHz";
            case 7: return "lowShelfSpectralResHz";
            default: return {};
        }
    }

    inline juce::StringArray allLegacySpectralResHzParamIds()
    {
        return { "band1SpectralResHz", "band2SpectralResHz", "band3SpectralResHz",
                 "band4SpectralResHz", "highpassSpectralResHz", "lowpassSpectralResHz",
                 "highShelfSpectralResHz", "lowShelfSpectralResHz" };
    }

    /**
        Per-band spectral Amount (stored as *SpectralDepth param IDs).
        Range 0–1; scales resonance-cut / expand depth.
    */
    inline juce::String spectralAmountParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SpectralDepth";
            case 1: return "band2SpectralDepth";
            case 2: return "band3SpectralDepth";
            case 3: return "band4SpectralDepth";
            case 4: return "highpassSpectralDepth";
            case 5: return "lowpassSpectralDepth";
            case 6: return "highShelfSpectralDepth";
            case 7: return "lowShelfSpectralDepth";
            default: return {};
        }
    }

    /** @deprecated Prefer spectralAmountParamIDForBandIndex — same IDs. */
    inline juce::String spectralDepthParamIDForBandIndex (int bandIndex)
    {
        return spectralAmountParamIDForBandIndex (bandIndex);
    }

    /** Per-band expand invert (boost resonances instead of cutting). */
    inline juce::String spectralExpandParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SpectralExpand";
            case 1: return "band2SpectralExpand";
            case 2: return "band3SpectralExpand";
            case 3: return "band4SpectralExpand";
            case 4: return "highpassSpectralExpand";
            case 5: return "lowpassSpectralExpand";
            case 6: return "highShelfSpectralExpand";
            case 7: return "lowShelfSpectralExpand";
            default: return {};
        }
    }

    inline juce::String spectralParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < EqBand::kBankSize)
            return spectralParamIDForBandIndex (EqBand::internalFromDisplay (globalDisplay));
        return EqBand::extendedParamID (globalDisplay, "Spectral");
    }

    inline juce::String spectralResHzParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < EqBand::kBankSize)
            return spectralResHzParamIDForBandIndex (EqBand::internalFromDisplay (globalDisplay));
        return EqBand::extendedParamID (globalDisplay, "SpectralResHz");
    }

    inline juce::String spectralAmountParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < EqBand::kBankSize)
            return spectralAmountParamIDForBandIndex (EqBand::internalFromDisplay (globalDisplay));
        return EqBand::extendedParamID (globalDisplay, "SpectralDepth");
    }

    inline juce::String spectralExpandParamIDForGlobal (int globalDisplay)
    {
        if (globalDisplay < EqBand::kBankSize)
            return spectralExpandParamIDForBandIndex (EqBand::internalFromDisplay (globalDisplay));
        return EqBand::extendedParamID (globalDisplay, "SpectralExpand");
    }

    /** Global lattice pack (one for all spectral bands): Flat / LF / HF. */
    inline constexpr const char* spectralPackParamId() noexcept { return "spectralPack"; }

    inline juce::StringArray getPackModeChoiceNames()
    {
        return { "Flat", "LF", "HF" };
    }

    inline SpectralBinning::PackMode packModeFromChoiceIndex (int index) noexcept
    {
        switch (index)
        {
            case 1:  return SpectralBinning::PackMode::lf;
            case 2:  return SpectralBinning::PackMode::hf;
            default: return SpectralBinning::PackMode::flat;
        }
    }

    inline int packModeToChoiceIndex (SpectralBinning::PackMode mode) noexcept
    {
        switch (mode)
        {
            case SpectralBinning::PackMode::lf: return 1;
            case SpectralBinning::PackMode::hf: return 2;
            case SpectralBinning::PackMode::flat:
            default: return 0;
        }
    }

    /** Cycle Flat → LF → HF → Flat. */
    inline int nextPackChoiceIndex (int current) noexcept
    {
        return (juce::jlimit (0, 2, current) + 1) % 3;
    }

    /** Spectral lattice is Bank 1 only (internal indices 0–7). Extended bands hide S. */
    inline bool supportsSpectral (int bandIndex)
    {
        return bandIndex >= 0 && bandIndex < EqBand::kBankSize;
    }

    /** Map UI bandIndex → engine slot, or -1 if unsupported. */
    inline int slotForBandIndex (int bandIndex) noexcept
    {
        if (bandIndex >= 0 && bandIndex < kNumSlots)
            return bandIndex;
        return -1;
    }

    enum class BandShape
    {
        bell = 0,
        highShelf,
        lowShelf
    };

    /** Spectral soft-mask follows the band's current EQ type (not the slot default). */
    inline BandShape shapeFromFilterType (int filterType) noexcept
    {
        switch (filterType)
        {
            case FilterType::highShelf: return BandShape::highShelf;
            case FilterType::lowShelf:  return BandShape::lowShelf;
            default:                    return BandShape::bell;
        }
    }

    struct BandSettings
    {
        bool enabled = false;
        float frequencyHz = 1000.0f;
        float q = 0.67f;
        /** Target bandpass width (Hz); clamped to SpectralBinning min/max. */
        float bandwidthHz = SpectralBinning::kDefaultBandwidthHz;
        /**
            Processing amount 0–1. Scales how deep resonances are cut/boosted
            within the Q aperture (independent of static band gain).
        */
        float amount = kDefaultSpectralAmount;
        /** When true, GR is positive (expand resonances) instead of cut. */
        bool expand = false;
        /** Global lattice pack applied when this slot rebuilds (Flat / LF / HF). */
        SpectralBinning::PackMode pack = SpectralBinning::PackMode::flat;
        /** Envelope attack (ms) — shared UI param with Dynamic EQ. */
        float attackMs = SpectralDynamics::attackMs;
        /** Envelope release (ms) — shared UI param with Dynamic EQ. */
        float releaseMs = SpectralDynamics::releaseMs;
        BandShape shape = BandShape::bell;
        /**
            When true, resonance detect uses the plugin Sidechain input bus
            (not the main track). Used when band Sidechain is on with Spectral.
        */
        bool detectFromSidechain = false;
    };
}
