#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <array>
#include "SpectralBandSettings.h"
#include "SpectralBinning.h"

/**
    Shared spectral dynamics engine (Pro-Q-style Spectral).

    Architecture
    ------------
    - ONE coarse IIR bandpass bank for all active S bands (not per-band STFT).
    - Runs once at the end of the EQ chain: GR is applied to the post-EQ signal.
    - Sidechain detect is pre-EQ / dry so resonance detection is independent of
      static band gain (makeup boost does not fight the detector).
      Per-slot detectFromSidechain switches that detect to the plugin Sidechain
      bus (external source) instead of the main track.
    - Hard bypass (zero filter cost) when no S band is enabled.
    - No FFT in the audio path; no allocations on the audio thread.

    Model
    -----
    - Global / fixed log bandpass lattice: slices are always log-spaced across
      the hearing-range grid (20 Hz…maxHz). Q does not shrink the lattice.
    - Band frequency + Q act as a soft mask over which slices apply GR
      (Gaussian / shelf taper in log-f; edges → ~0).
    - Active-set CPU gating: expanded (wider-σ) mask + hysteresis sets gateWanted
      ahead of the audible GR region and holds briefly when leaving; a smoothed
      gateGain (0..1) fades each slice's GR contribution. Hard-skip only after
      fade-out; IIR/envelope cleared on cold re-arm (not mid-stream).
      Narrow Q → few warm slices → real CPU savings; wide Q → most run.
    - Global SpectralResHz (0.5–2.0) sets BP count on that lattice for all S bands
      (48 @ 2.0 Hz → 128 @ 0.5 Hz). Constant-Q packing per centre.
    - Mid (L+R)/2 level detect per BP on the dry sidechain → envelope
      (per-slot attack/release from shared A/R params, default 20 / 200 ms).
    - No user threshold. Prominence vs log-neighbor baseline (tilt-robust) → GR.
      Suppress:  grDb = -Amount * kMaxCutDb * engage * mask
      Expand:    grDb = +Amount * kMaxCutDb * engage * mask
    - Static band gain remains the IIR EQ offset (makeup).
    - Reconstruct: wet + sum(bp_i(wet) * (grLinear_i - 1)) — transparent at GR=0.
    - Stereo-linked GR (mid detect, same GR on L/R).
    - Idle BPs (|GR| < ~0.05 dB) may sleep a few blocks to save CPU.

    Display
    -------
    Publishes one signed GR point per BP centre (notches for cuts / peaks for expand).
    UI sampling reconstructs smooth log-f lobes onto the dense EQ display grid — never
    stroke a sparse polyline of BP centres after path resampling.
*/
class SpectralDynamicsProcessor
{
public:
    SpectralDynamicsProcessor() = default;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    /** Disable all slots for the upcoming block (call once before setBand). */
    void clearBands() noexcept;

    /**
        Enable/configure a slot for this block.
        slot: 0..kNumSlots-1 (see SpectralDynamics::slotForBandIndex).
    */
    void setBand (int slot, const SpectralDynamics::BandSettings& settings) noexcept;

    /** True if at least one S band is armed for the next process() call. */
    bool hasActiveBands() const noexcept { return activeBandCount > 0; }

    /**
        Last-block runtime counters (audio thread publishes; UI may sample).
        Use to verify S is actually processing: banked = lattice size,
        gated = warm slices (wanted or fading), processing = warm and not sleeping.
    */
    struct RuntimeStats
    {
        int armedSlots = 0;
        int bankedBandpasses = 0;
        int gatedBandpasses = 0;
        int processingBandpasses = 0;
    };

    RuntimeStats getRuntimeStats() const noexcept;

    /**
        In-place process on post-EQ audio.
        detectL/R: main-track pre-EQ dry (default spectral detect).
        scDetectL/R: optional Sidechain bus for slots with detectFromSidechain.
        If null, falls back to the main block (legacy) / silence for SC slots.
        No-op (zero filter cost) when no bands are active.
        Must be called at most once per processBlock after clearBands/setBand.
    */
    void process (juce::dsp::AudioBlock<float>& block,
                  const float* detectL = nullptr,
                  const float* detectR = nullptr,
                  const float* scDetectL = nullptr,
                  const float* scDetectR = nullptr);

    /** IIR path reports no algorithmic latency. */
    int getLatencySamples() const noexcept { return 0; }

    /**
        Sample published spectral GR (dB, signed) for a UI bandIndex onto an arbitrary
        frequency grid (call with the same dense log-f display samples as the EQ curve).
        Reconstructs overlapping Hann lobes in log-f (not sparse polyline between centres).
        Thread-safe vs audio via a generation-locked double buffer.
    */
    void samplePublishedGrDb (int bandIndex, const float* frequenciesHz, float* destDb, int numPoints) const;

    bool hasActiveGr (int bandIndex) const noexcept;
    float getPublishedPeakGrDb (int bandIndex) const noexcept;

private:
    struct BandSlot
    {
        SpectralDynamics::BandSettings settings;
        /** Q-derived soft-mask footprint [fLo, fHi] (diagnostics / UI); placement is global. */
        float fLo = 0.0f;
        float fHi = 0.0f;
        bool active = false;
        /** Snapshot from clearBands() so process() can detect de-armed slots. */
        bool wasActive = false;
    };

    struct BandpassUnit
    {
        /** Apply path: stereo BPs on post-EQ wet. */
        juce::dsp::IIR::Filter<float> filterL;
        juce::dsp::IIR::Filter<float> filterR;
        /** Detect path: mono BP on pre-EQ mid sidechain. */
        juce::dsp::IIR::Filter<float> filterDetect;
        float centerHz = 1000.0f;
        float bandwidthHz = SpectralBinning::kDefaultBandwidthHz;
        float q = 1.0f;
        float mask = 1.0f;
        /** Signed max GR (dB) after Amount * kMaxCutDb (± for expand). */
        float maxCutDb = 0.0f;
        float envelopeDb = DynamicEq::kSilenceFloorDb;
        /** Smoothed audible GR (linear); follows grTarget per sample. */
        float grLinear = 1.0f;
        /** Block-rate GR target from prominence; never applied as a hard step. */
        float grTarget = 1.0f;
        float grDb = 0.0f;
        float sumSq = 0.0f;
        int ownerSlot = -1;
        /** Allocated in the bank for this block's geometry. */
        bool active = false;
        /**
            Hysteresis target from expanded mask: true while the slice is wanted.
            Audio uses smoothed gateGain; hard-skip only when !gateActive and
            gateGain ≈ 0 after fade-out.
        */
        bool gateActive = false;
        /** Smoothed 0..1 GR-path weight (anti-zipper); follows gateActive. */
        float gateGain = 0.0f;
        /** Blocks spent below deactivate threshold while still gated on. */
        int gateHoldBlocks = 0;
        float lastCenterHz = -1.0f;
        float lastQ = -1.0f;
        int idleBlocks = 0;
        int sleepBlocksRemaining = 0;
        /** True → detect from external SC bus mid (else main-track pre-EQ detect). */
        bool detectFromSidechain = false;
    };

    struct PublishedCurve
    {
        std::array<float, SpectralBinning::kMaxBandpasses> centerHz {};
        std::array<float, SpectralBinning::kMaxBandpasses> grDb {};
        int count = 0;
    };

    void rebuildBank() noexcept;
    void computeInfluenceRange (BandSlot& slot) const noexcept;
    /**
        Soft Q-mask in log-frequency. sigmaScale > 1 widens σ for activation
        lookahead without changing the GR mask (callers pass 1 for audible GR).
    */
    float bandMask (float frequencyHz, const SpectralDynamics::BandSettings& settings,
                    float sigmaScale = 1.0f) const noexcept;
    /** Union-of-expanded-masks active-set gate with activate/hold hysteresis. */
    void updateActiveSetGates() noexcept;
    /** True while slice should run BP/envelope (wanted or still fading). */
    static bool unitIsWarm (const BandpassUnit& unit) noexcept;
    /** Advance smoothed gateGain toward gateActive by one sample step. */
    static void advanceGateGain (BandpassUnit& unit, float stepPerSample) noexcept;
    void clearUnitRuntimeState (BandpassUnit& unit) noexcept;
    /** Returns false if coeffs could not be built (caller should skip this BP only). */
    bool updateBandpassCoeffs (BandpassUnit& unit) noexcept;
    void finishBlockEnvelopes (int numSamples) noexcept;
    void publishGrCurves() noexcept;
    /** max BP centre: min(20 kHz, 0.45×sr). */
    float safeMaxCenterHz() const noexcept;

    double sampleRate = 48000.0;
    int numChannels = 2;
    int maxBlockSize = 512;

    std::array<BandSlot, SpectralDynamics::kNumSlots> bands {};
    int activeBandCount = 0;
    bool bankDirty = true;

    std::array<BandpassUnit, SpectralBinning::kMaxBandpasses> bank {};
    int activeBandpassCount = 0;

    int publishBlockCounter = 0;
    bool wasActive = false;

    // Double-buffered published GR per slot (one point per BP centre).
    std::array<std::array<PublishedCurve, 2>, SpectralDynamics::kNumSlots> publishedGr {};
    std::array<std::atomic<int>, SpectralDynamics::kNumSlots> publishedIndex {};
    std::array<std::atomic<float>, SpectralDynamics::kNumSlots> publishedPeakGrDb {};

    std::atomic<int> publishedArmedSlots { 0 };
    std::atomic<int> publishedBankedBandpasses { 0 };
    std::atomic<int> publishedGatedBandpasses { 0 };
    std::atomic<int> publishedProcessingBandpasses { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralDynamicsProcessor)
};
