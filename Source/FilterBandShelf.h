#pragma once

#include <JuceHeader.h>
#include <cmath>

/**
    Band-shelf (flat-top plateau), log-symmetric about f0.

    Construction (stable, industry-standard dual-edge form):
      • Geometric band edges: f_lo = f0 / r, f_hi = f0 · r  (r = 2^{bwOct/2})
        → symmetric on a log-frequency axis (what EQ graphs show).
      • Rising edge:  N cascaded high-shelves at f_lo with +G/N each
      • Falling edge: N cascaded high-shelves at f_hi with −G/N each
      • N = 4 → order-8 edges (steep enough for a flat top at moderate widths)

    Higher order at each edge (not pulling f_lo/f_hi together) is what keeps a
    plateau when the band is narrow — a single soft shelf pair becomes a bell.

    Nyquist: f_hi is clamped and f_lo is re-mirrored about f0 so the shape
    stays log-centred and poles stay well inside the unit circle (no blow-ups).

    Q maps to bandwidth in octaves (independent of edge order).

    Refs (edge-order / shelving theory):
      Holters & Zölzer, parametric higher-order shelving (EUSIPCO 2006 / DAFx-06)
      — dual geometric high-shelf cascade is the stable practical form of a
        band-limited shelf; full allpass-shifted prototype is equivalent in the
        ideal case but brittle near Nyquist in fixed-point bilinear cascades.
*/
namespace FilterBandShelf
{
    /** Stages per edge (total SOS = 2 · kEdgeOrder). Must stay ≤ FilterSlope::maxBiquadStages (8). */
    static constexpr int kEdgeOrder = 4;

    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeStages (double sampleRate, float frequencyHz, float q, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;

        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        const double fMax = fs * 0.45; // stay clear of Nyquist for RBJ shelves
        const double f0 = juce::jlimit (25.0, fMax * 0.95, (double) frequencyHz);

        if (std::abs (gainDb) < 1.0e-3f)
            return stages;

        // Q → bandwidth in octaves (higher Q = narrower plateau).
        // Q 0.5 → ~1.7 oct, Q 1 → ~1.2 oct, Q 2 → ~0.85 oct, Q 5 → ~0.55 oct
        const float safeQ = juce::jmax (0.15f, q);
        const double bwOct = (double) juce::jlimit (0.5f, 3.0f, 1.2f / std::sqrt (safeQ));

        double r = std::pow (2.0, 0.5 * bwOct);
        double fLo = f0 / r;
        double fHi = f0 * r;

        // Clamp high edge; re-mirror low edge so geometric centre stays f0
        // (log-symmetric) and shelves never sit on top of Nyquist.
        if (fHi > fMax)
        {
            fHi = fMax;
            fLo = (f0 * f0) / fHi;
        }
        if (fLo < 20.0)
        {
            fLo = 20.0;
            fHi = (f0 * f0) / fLo;
            if (fHi > fMax)
                fHi = fMax;
        }

        // Minimum separation so edges don't collapse into a peak
        if (fHi / fLo < 1.15)
        {
            const double mid = std::sqrt (fLo * fHi);
            fLo = mid / std::sqrt (1.15);
            fHi = mid * std::sqrt (1.15);
            fLo = juce::jmax (20.0, fLo);
            fHi = juce::jmin (fMax, fHi);
        }

        const float fLoF = (float) fLo;
        const float fHiF = (float) fHi;

        // Split gain evenly across edge stages (dB domain) → total ±gainDb,
        // steeper transition than one soft shelf of full gain.
        const float stageDb = gainDb / (float) kEdgeOrder;
        const float gUp = juce::Decibels::decibelsToGain (stageDb);
        const float gDn = juce::Decibels::decibelsToGain (-stageDb);

        // Q ≈ 1/√2 → smooth Butterworth-like shelf corners (no resonant peaking)
        constexpr float kShelfQ = 0.70710678f;

        for (int i = 0; i < kEdgeOrder; ++i)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, fLoF, kShelfQ, gUp));

        for (int i = 0; i < kEdgeOrder; ++i)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, fHiF, kShelfQ, gDn));

        return stages;
    }
}
