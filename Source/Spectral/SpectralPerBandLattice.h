#pragma once

#include <array>
#include <cmath>
#include "SpectralBandSettings.h"
#include "SpectralBinning.h"

/**
    Sandboxed per-band spectral lattice planner (optional S mode).

    Default / off path stays in SpectralDynamicsProcessor::rebuildBank() and
    tiles the global 20 Hz…maxHz lattice with a shared BP budget.

    When enabled, this helper:
      - Places each armed S band's slices only inside its Q influence range
        (local constant-Q lattice — distant bands no longer dilute each other)
      - Maps Res on a local scale: ~4 BPs (broad) → 128 BPs (surgical).
        The global path still uses 48…128; stuffing that into a narrow Q
        aperture made even "coarse" Res feel surgical.
      - Each slot keeps its own Res ideal; only if the sum exceeds the bank
        cap do counts scale down proportionally

    Side Check is untouched (separate processor / global lattice).

    Easy remove later: delete this file, the APVTS bool, the OptionBox Spectral
    S right-click "Per-band lattice" item, and the `if (perBandLatticeEnabled)`
    branch in rebuildBank/setBand.
*/
namespace SpectralPerBandLattice
{
    /** Same hard bank ceiling as the global path (keeps DSP arrays unchanged). */
    constexpr int kBankCapacity = SpectralBinning::kMaxBandpasses;

    /** Broad / musical end of Res in per-band mode (few wide slices). */
    constexpr int kMinBandpassBudget = 4;

    /** Fine end matches the global lattice ceiling. */
    constexpr int kMaxBandpassBudget = SpectralBinning::kMaxBandpasses; // 128

    inline constexpr const char* enabledParamId() noexcept { return "spectralPerBandLattice"; }

    /**
        Local Res → BP count: 4 @ coarsest (2.0 Hz) … 128 @ finest (0.5 Hz).
        Same 1/bw travel as the global mapper, different endpoints so coarse
        Res actually broadens control inside a Q aperture.
    */
    inline int bandpassBudgetForBandwidth (float bandwidthHz) noexcept
    {
        const float bw = SpectralBinning::clampBandwidthHz (bandwidthHz);
        const float invBw = 1.0f / bw;
        const float invCoarse = 1.0f / SpectralBinning::kTargetBandwidthHz;
        const float invFine = 1.0f / SpectralBinning::kMinBandwidthHz;
        const float t = (invBw - invCoarse) / (invFine - invCoarse);
        const float tClamped = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
        const int budget = (int) std::lround (
            (double) kMinBandpassBudget
            + (double) tClamped * (double) (kMaxBandpassBudget - kMinBandpassBudget));
        return budget < kMinBandpassBudget ? kMinBandpassBudget
             : (budget > kMaxBandpassBudget ? kMaxBandpassBudget : budget);
    }

    struct SlotRequest
    {
        bool active = false;
        float fLoHz = 20.0f;
        float fHiHz = 20000.0f;
        float bandwidthHz = SpectralBinning::kDefaultBandwidthHz;
    };

    struct SlotPlan
    {
        float placeLoHz = 20.0f;
        float placeHiHz = 20000.0f;
        int count = 0;
    };

    /**
        Plan local lattices for armed slots.
        @param maxHz   hearing-range ceiling (same as SpectralDynamicsProcessor::safeMaxCenterHz)
        @param out     per-slot placement + counts (inactive → count 0)
        @return        total allocated bandpasses (≤ kBankCapacity)
    */
    inline int plan (const std::array<SlotRequest, SpectralDynamics::kNumSlots>& requests,
                     float maxHz,
                     std::array<SlotPlan, SpectralDynamics::kNumSlots>& out) noexcept
    {
        out = {};
        const float hiCeil = maxHz > 20.0f ? maxHz : 20.0f;

        std::array<int, SpectralDynamics::kNumSlots> activeSlots {};
        std::array<int, SpectralDynamics::kNumSlots> idealCounts {};
        int numActive = 0;
        int totalIdeal = 0;

        for (int s = 0; s < SpectralDynamics::kNumSlots; ++s)
        {
            const auto& req = requests[(size_t) s];
            if (! req.active)
                continue;

            float lo = req.fLoHz < 20.0f ? 20.0f : req.fLoHz;
            float hi = req.fHiHz > hiCeil ? hiCeil : req.fHiHz;
            if (hi < lo)
            {
                const float t = lo;
                lo = hi;
                hi = t;
            }
            if (hi <= lo * 1.001f)
            {
                const float mid = std::sqrt (juce::jmax (20.0f, lo) * juce::jmax (lo, hiCeil));
                lo = juce::jmax (20.0f, mid * 0.97f);
                hi = juce::jmin (hiCeil, mid * 1.03f);
                if (hi <= lo)
                    hi = juce::jmin (hiCeil, lo * 1.03f);
            }

            out[(size_t) s].placeLoHz = lo;
            out[(size_t) s].placeHiHz = hi;

            const int ideal = juce::jmax (kMinBandpassBudget,
                                          bandpassBudgetForBandwidth (req.bandwidthHz));
            idealCounts[(size_t) s] = ideal;
            activeSlots[(size_t) numActive++] = s;
            totalIdeal += ideal;
        }

        if (numActive == 0)
            return 0;

        // Cap = full bank (not "finest Res among actives"). Each slot keeps its
        // own ideal until the shared array is full — then proportional scale.
        const int effectiveCap = kBankCapacity;
        std::array<int, SpectralDynamics::kNumSlots> allocated {};

        if (totalIdeal <= effectiveCap)
        {
            for (int i = 0; i < numActive; ++i)
            {
                const int s = activeSlots[(size_t) i];
                allocated[(size_t) s] = idealCounts[(size_t) s];
            }
        }
        else
        {
            int remaining = effectiveCap;
            for (int i = 0; i < numActive; ++i)
            {
                const int s = activeSlots[(size_t) i];
                allocated[(size_t) s] = 1;
                --remaining;
            }

            if (remaining > 0 && totalIdeal > numActive)
            {
                const int extraIdeal = totalIdeal - numActive;
                int assignedExtra = 0;
                for (int i = 0; i < numActive; ++i)
                {
                    const int s = activeSlots[(size_t) i];
                    const int shareIdeal = idealCounts[(size_t) s] - 1;
                    const int extra = (i == numActive - 1)
                        ? (remaining - assignedExtra)
                        : (int) std::floor ((double) remaining * (double) shareIdeal
                                            / (double) extraIdeal + 0.5);
                    const int clamped = juce::jmax (0, juce::jmin (remaining - assignedExtra, extra));
                    allocated[(size_t) s] += clamped;
                    assignedExtra += clamped;
                }
            }
        }

        int total = 0;
        for (int i = 0; i < numActive; ++i)
        {
            const int s = activeSlots[(size_t) i];
            out[(size_t) s].count = juce::jmax (1, allocated[(size_t) s]);
            total += out[(size_t) s].count;
        }
        return total;
    }
}
