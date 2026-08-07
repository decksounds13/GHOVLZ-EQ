#pragma once

#include <JuceHeader.h>
#include <complex>
#include <vector>
#include <cmath>
#include <algorithm>

/**
    Holters–Zölzer band-shelf (Tier C).

    Continuous-time Butterworth-based low-shelf of order M, lowpass→bandpass
    spectral transform to centre f0 with bandwidth B, bilinear transform to
    digital, then SOS cascade.

    Refs:
      Holters & Zölzer, "Parametric higher-order shelving filters", EUSIPCO 2006
      Holters & Zölzer, "Graphic equalizer design using higher-order recursive
        filters", DAFx-06
*/
namespace FilterBandShelf
{
    using Complex = std::complex<double>;

    /** Low-shelf prototype order. Band-shelf digital order = 2·M (M biquad pairs). */
    static constexpr int kProtoOrderM = 4; // → 8 poles → 4 SOS (fits maxBiquadStages)

    inline Complex bltAnalogToDigital (Complex s, double fs)
    {
        // z = (2 fs + s) / (2 fs − s)
        const Complex twoFs (2.0 * fs, 0.0);
        return (twoFs + s) / (twoFs - s);
    }

    /** Reflect unstable poles (|z|>1) for numerical safety. */
    inline Complex stabilizePole (Complex z)
    {
        const double mag = std::abs (z);
        if (mag > 1.0 && mag > 1.0e-12)
            return z / (mag * mag); // reflect through unit circle
        return z;
    }

    /**
        LP→BP: each analog prototype pole/zero p becomes two roots of
        s² − (B·p)·s + ω0² = 0.
    */
    inline void expandLpToBp (Complex p, double B, double w0,
                              std::vector<Complex>& out)
    {
        const Complex disc = (B * p) * (B * p) - Complex (4.0 * w0 * w0, 0.0);
        const Complex root = std::sqrt (disc);
        out.push_back (0.5 * (B * p + root));
        out.push_back (0.5 * (B * p - root));
    }

    /** Pair conjugate roots (from full set) into upper-half representatives. */
    inline std::vector<Complex> upperHalfPairs (std::vector<Complex> roots)
    {
        std::vector<Complex> upper;
        std::vector<bool> used ((size_t) roots.size(), false);

        for (size_t i = 0; i < roots.size(); ++i)
        {
            if (used[i])
                continue;

            // Prefer roots with Im >= 0 as the pair representative.
            if (roots[i].imag() < -1.0e-10)
                continue;

            // Find conjugate partner
            int partner = -1;
            double best = 1.0e100;
            for (size_t j = 0; j < roots.size(); ++j)
            {
                if ((int) j == (int) i || used[j])
                    continue;
                const double d = std::abs (roots[j] - std::conj (roots[i]));
                if (d < best)
                {
                    best = d;
                    partner = (int) j;
                }
            }

            used[i] = true;
            if (partner >= 0)
                used[(size_t) partner] = true;

            // Real root (or unpaired): treat as its own pair with itself
            if (std::abs (roots[i].imag()) < 1.0e-10)
                upper.push_back (Complex (roots[i].real(), 0.0));
            else
                upper.push_back (roots[i]);
        }

        // Any leftover real poles with Im < 0 that were skipped
        for (size_t i = 0; i < roots.size(); ++i)
        {
            if (used[i])
                continue;
            if (std::abs (roots[i].imag()) < 1.0e-10)
            {
                used[i] = true;
                upper.push_back (Complex (roots[i].real(), 0.0));
            }
        }

        return upper;
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr biquadFromConjugatePair (
        Complex zeroUpper, Complex poleUpper, double sectionGain)
    {
        // (1 − 2 Re(z) z⁻¹ + |z|² z⁻²) / (1 − 2 Re(p) z⁻¹ + |p|² z⁻²)
        const double zr = zeroUpper.real();
        const double zAbs2 = std::norm (zeroUpper); // |z|²
        const double pr = poleUpper.real();
        const double pAbs2 = std::norm (poleUpper);

        const double b0 = sectionGain;
        const double b1 = sectionGain * (-2.0 * zr);
        const double b2 = sectionGain * zAbs2;
        const double a1 = -2.0 * pr;
        const double a2 = pAbs2;

        return new juce::dsp::IIR::Coefficients<float> (
            (float) b0, (float) b1, (float) b2,
            1.0f, (float) a1, (float) a2);
    }

    /**
        Holters–Zölzer band-shelf stages.

        @param frequencyHz  Centre frequency f0
        @param q            Bandwidth control (higher Q → narrower plateau)
        @param gainDb       Plateau height in dB
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeStages (double sampleRate, float frequencyHz, float q, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;

        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        const double f0 = juce::jlimit (20.0, fs * 0.45, (double) frequencyHz);
        const double g = (double) juce::Decibels::decibelsToGain (gainDb);

        // Near-unity gain → no processing
        if (std::abs (gainDb) < 1.0e-3f)
            return stages;

        // Q → bandwidth in octaves (musical, independent of edge order).
        // Q≈0.5 → ~2.0 oct, Q≈1 → ~1.2 oct, Q≈3 → ~0.7 oct, Q≈8 → ~0.5 oct
        const float safeQ = juce::jmax (0.15f, q);
        const double bwOct = (double) juce::jlimit (0.5f, 3.5f, 1.2f / std::sqrt (safeQ));

        const double fLo = f0 / std::pow (2.0, 0.5 * bwOct);
        const double fHi = f0 * std::pow (2.0, 0.5 * bwOct);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0;
        const double B  = 2.0 * juce::MathConstants<double>::pi * juce::jmax (1.0, fHi - fLo);

        constexpr int M = kProtoOrderM;
        const double gRoot = std::pow (std::abs (g), 1.0 / (double) M);
        // Preserve cut vs boost: g may be < 1
        const double gRootSigned = (g >= 0.0) ? gRoot : gRoot; // g always > 0 linear
        juce::ignoreUnused (gRootSigned);

        // ── 1. Analog low-shelf poles/zeros (Holters eq. 3), then LP→BP ─────
        std::vector<Complex> analogPoles, analogZeros;
        analogPoles.reserve ((size_t) (2 * M));
        analogZeros.reserve ((size_t) (2 * M));

        for (int m = 1; m <= M; ++m)
        {
            const double alpha = juce::MathConstants<double>::pi
                                 * (double) (2 * m - 1) / (double) (2 * M);
            const Complex ej (std::cos (alpha), std::sin (alpha));
            const Complex pLs = -ej;                 // pole of normalised LS
            const Complex zLs = -gRoot * ej;         // zero of normalised LS

            expandLpToBp (pLs, B, w0, analogPoles);
            expandLpToBp (zLs, B, w0, analogZeros);
        }

        // ── 2. Bilinear transform to digital z-plane ────────────────────────
        std::vector<Complex> digPoles, digZeros;
        digPoles.reserve (analogPoles.size());
        digZeros.reserve (analogZeros.size());

        for (auto s : analogPoles)
            digPoles.push_back (stabilizePole (bltAnalogToDigital (s, fs)));
        for (auto s : analogZeros)
            digZeros.push_back (bltAnalogToDigital (s, fs));

        // ── 3. Pair into conjugate biquads ──────────────────────────────────
        auto polePairs = upperHalfPairs (digPoles);
        auto zeroPairs = upperHalfPairs (digZeros);

        // Match count (should be M for even expansion)
        const int nSec = (int) std::min (polePairs.size(), zeroPairs.size());
        if (nSec <= 0)
            return stages;

        // Sort both by angle for stable pairing
        auto byAngle = [] (Complex a, Complex b)
        {
            return std::arg (a) < std::arg (b);
        };
        std::sort (polePairs.begin(), polePairs.end(), byAngle);
        std::sort (zeroPairs.begin(), zeroPairs.end(), byAngle);

        // Build unit-gain SOS first, then overall scale
        for (int i = 0; i < nSec; ++i)
            stages.add (biquadFromConjugatePair (zeroPairs[(size_t) i],
                                                 polePairs[(size_t) i],
                                                 1.0));

        // ── 4. Scale so |H(e^{jω0})| = |g| at the band centre ──────────────
        {
            const double w = 2.0 * juce::MathConstants<double>::pi * f0 / fs;
            const Complex ejw (std::cos (w), std::sin (w));
            Complex H (1.0, 0.0);

            for (int i = 0; i < stages.size(); ++i)
            {
                const juce::dsp::IIR::Coefficients<float>::Ptr c = stages.getUnchecked (i);
                if (c == nullptr)
                    continue;
                // H_i(z) = (b0 + b1 z⁻¹ + b2 z⁻²) / (a0 + a1 z⁻¹ + a2 z⁻²)
                const Complex zm1 = std::conj (ejw);
                const Complex zm2 = zm1 * zm1;
                const float* a = c->getRawCoefficients();
                // JUCE layout: b0,b1,b2,a0,a1,a2
                const Complex num = (double) a[0] + (double) a[1] * zm1 + (double) a[2] * zm2;
                const Complex den = (double) a[3] + (double) a[4] * zm1 + (double) a[5] * zm2;
                if (std::abs (den) > 1.0e-30)
                    H *= num / den;
            }

            const double hMag = std::abs (H);
            if (hMag > 1.0e-12 && stages.size() > 0)
            {
                const juce::dsp::IIR::Coefficients<float>::Ptr c0 = stages.getUnchecked (0);
                if (c0 != nullptr)
                {
                    const double scale = std::abs (g) / hMag;
                    float* a = c0->getRawCoefficients();
                    a[0] = (float) ((double) a[0] * scale);
                    a[1] = (float) ((double) a[1] * scale);
                    a[2] = (float) ((double) a[2] * scale);
                }
            }
        }

        // Cap stages to max cascade depth used by the processor
        while (stages.size() > 8)
            stages.removeLast();

        return stages;
    }
}
