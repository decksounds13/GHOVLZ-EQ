#pragma once

#include <JuceHeader.h>
#include <complex>
#include <vector>
#include <cmath>

/**
    Butterworth band-shelf — Falco DSPFilters path (MIT), hardened for EQ use.

    Analog low-shelf (order N) → Constantinides band-pass map → SOS.
    Digital pole count = 2N → N biquads.

    Width: Q maps to bandwidth in octaves (can go ~0.2 oct).
    High-f safety: clamp band edges away from 0/Nyquist, stabilize poles,
    reject NaN sections, clamp overall scale so the curve never “explodes”.

    https://github.com/vinniefalco/DSPFilters  (Butterworth.cpp / PoleFilter.cpp)
*/
namespace FilterBandShelf
{
    using Complex = std::complex<double>;
    static constexpr double kPi = 3.141592653589793238462643383279502884;

    /** Analog order 6 → 6 SOS. Steeper edges than order 4 (better narrow flats). */
    static constexpr int kAnalogOrder = 6;

    inline Complex addmul (Complex c, double v, Complex c1) noexcept
    {
        return { c.real() + v * c1.real(), c.imag() + v * c1.imag() };
    }

    inline bool isFiniteC (Complex z) noexcept
    {
        return std::isfinite (z.real()) && std::isfinite (z.imag());
    }

    inline Complex stabilizePole (Complex z) noexcept
    {
        if (! isFiniteC (z))
            return Complex (0.0, 0.0);
        const double m = std::abs (z);
        // Keep poles strictly inside the unit circle
        if (m >= 0.998)
            return z * (0.997 / juce::jmax (m, 1.0e-12));
        return z;
    }

    // ── Analog low-shelf (Falco AnalogLowShelf::design) ─────────────────────

    struct AnalogPz
    {
        Complex pole {};
        Complex zero {};
        bool isReal = false;
    };

    inline std::vector<AnalogPz> designAnalogLowShelf (int numPoles, double gainDb)
    {
        std::vector<AnalogPz> out;
        if (numPoles < 1)
            return out;

        const double n2 = (double) numPoles * 2.0;
        const double gAbs = std::pow (10.0, std::abs (gainDb) / 20.0);
        // Match Falco: g from signed dB (cuts use gainDb < 0)
        const double gLin = std::pow (10.0, gainDb / 20.0);
        const double g = std::pow (std::abs (gLin), 1.0 / n2);
        juce::ignoreUnused (gAbs);

        // Falco: gp = -1/g, gz = -g  (negative radius into std::polar)
        const double gp = -1.0 / juce::jmax (1.0e-9, g);
        const double gz = -g;

        const int pairs = numPoles / 2;
        for (int i = 1; i <= pairs; ++i)
        {
            const double theta = kPi * (0.5 - (2.0 * (double) i - 1.0) / n2);
            AnalogPz ap;
            // C++ polar(rho,theta) with rho<0 → opposite half-plane (Falco relies on this)
            ap.pole = std::polar (gp, theta);
            ap.zero = std::polar (gz, theta);
            ap.isReal = false;
            out.push_back (ap);
        }

        if (numPoles & 1)
        {
            AnalogPz ap;
            ap.pole = Complex (gp, 0.0);
            ap.zero = Complex (gz, 0.0);
            ap.isReal = true;
            out.push_back (ap);
        }

        return out;
    }

    // ── Band-pass transform (Falco BandPassTransform) ───────────────────────

    struct BpCtx
    {
        double a = 0, b = 0, a2 = 0, b2 = 0, ab = 0, ab_2 = 0;
        bool valid = false;
    };

    inline BpCtx makeBpCtx (double fcNorm, double fwNorm) noexcept
    {
        BpCtx c;
        // Require a usable band interior away from DC / Nyquist
        if (fcNorm <= 0.001 || fcNorm >= 0.49 || fwNorm <= 1.0e-6)
            return c;

        double half = 0.5 * fwNorm;
        // Shrink width until edges are safe
        if (fcNorm - half < 0.0015)
            half = fcNorm - 0.0015;
        if (fcNorm + half > 0.48)
            half = 0.48 - fcNorm;
        if (half < 0.0008)
            return c;

        const double fw = 2.0 * half;
        const double ww = 2.0 * kPi * fw;

        double wc2 = 2.0 * kPi * fcNorm - ww * 0.5;
        double wc  = wc2 + ww;
        if (wc2 < 1.0e-6) wc2 = 1.0e-6;
        if (wc  > kPi - 1.0e-6) wc = kPi - 1.0e-6;
        if (wc <= wc2 + 1.0e-6)
            return c;

        const double cosHalf = std::cos ((wc - wc2) * 0.5);
        const double sinHalf = std::sin ((wc - wc2) * 0.5);
        if (std::abs (cosHalf) < 1.0e-9 || std::abs (sinHalf) < 1.0e-12)
            return c;

        c.a = std::cos ((wc + wc2) * 0.5) / cosHalf;
        c.b = 1.0 / std::tan ((wc - wc2) * 0.5);
        // Guard insane b (near-zero bandwidth in practice)
        if (! std::isfinite (c.a) || ! std::isfinite (c.b) || std::abs (c.b) > 1.0e6)
            return c;

        c.a2 = c.a * c.a;
        c.b2 = c.b * c.b;
        c.ab = c.a * c.b;
        c.ab_2 = 2.0 * c.ab;
        c.valid = true;
        return c;
    }

    inline void bpTransformRoot (const BpCtx& bp, Complex s, Complex& o0, Complex& o1) noexcept
    {
        if (! isFiniteC (s))
        {
            o0 = Complex (-1.0, 0.0);
            o1 = Complex (1.0, 0.0);
            return;
        }

        // Avoid division by zero in (1-s) when s≈1
        if (std::abs (Complex (1.0, 0.0) - s) < 1.0e-12)
        {
            o0 = Complex (-0.999, 0.0);
            o1 = Complex (0.999, 0.0);
            return;
        }

        Complex c = (Complex (1.0, 0.0) + s) / (Complex (1.0, 0.0) - s);
        if (! isFiniteC (c))
        {
            o0 = o1 = Complex (0.0, 0.0);
            return;
        }

        Complex v (0.0, 0.0);
        v = addmul (v, 4.0 * (bp.b2 * (bp.a2 - 1.0) + 1.0), c);
        v += 8.0 * (bp.b2 * (bp.a2 - 1.0) - 1.0);
        v *= c;
        v += 4.0 * (bp.b2 * (bp.a2 - 1.0) + 1.0);
        v = std::sqrt (v);

        Complex u = -v;
        u = addmul (u, bp.ab_2, c);
        u += bp.ab_2;

        v = addmul (v, bp.ab_2, c);
        v += bp.ab_2;

        Complex d (0.0, 0.0);
        d = addmul (d, 2.0 * (bp.b - 1.0), c);
        d += 2.0 * (1.0 + bp.b);

        if (std::abs (d) < 1.0e-18 || ! isFiniteC (u) || ! isFiniteC (v))
        {
            o0 = o1 = Complex (0.0, 0.0);
            return;
        }

        o0 = stabilizePole (u / d);
        o1 = stabilizePole (v / d);
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr biquadFromConjPair (
        Complex pole, Complex zero, double scale)
    {
        if (! isFiniteC (pole) || ! isFiniteC (zero) || ! std::isfinite (scale))
            return {};

        const double a1 = -2.0 * pole.real();
        const double a2 = std::norm (pole);
        // Stability: |a2| < 1 for conjugate pair inside unit circle
        if (a2 >= 0.999 || std::abs (a1) >= 1.0 + a2 + 1.0e-6)
            return {};

        const double b0 = scale;
        const double b1 = scale * (-2.0 * zero.real());
        const double b2 = scale * std::norm (zero);

        if (! std::isfinite (b0) || ! std::isfinite (b1) || ! std::isfinite (b2)
            || ! std::isfinite (a1) || ! std::isfinite (a2))
            return {};

        return new juce::dsp::IIR::Coefficients<float> (
            (float) b0, (float) b1, (float) b2,
            1.0f, (float) a1, (float) a2);
    }

    /** Stable log-symmetric dual-edge fallback (never blows up). */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeDualEdgeFallback (double sampleRate, double f0, double bwOct, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        const double fMax = fs * 0.45;

        double r = std::pow (2.0, 0.5 * juce::jmax (0.15, bwOct));
        double fLo = f0 / r;
        double fHi = f0 * r;
        if (fHi > fMax) { fHi = fMax; fLo = (f0 * f0) / fHi; }
        if (fLo < 20.0) { fLo = 20.0; fHi = juce::jmin (fMax, (f0 * f0) / fLo); }
        if (fHi <= fLo * 1.05)
            fHi = juce::jmin (fMax, fLo * 1.08);

        // More stages when narrow (steeper edges)
        const int nEdge = (bwOct < 0.45) ? 4 : (bwOct < 0.9 ? 3 : 2);
        const float stageDb = gainDb / (float) nEdge;
        const float gU = juce::Decibels::decibelsToGain (stageDb);
        const float gD = juce::Decibels::decibelsToGain (-stageDb);
        constexpr float sQ = 0.70710678f;

        for (int i = 0; i < nEdge; ++i)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, (float) fLo, sQ, gU));
        for (int i = 0; i < nEdge; ++i)
            stages.add (juce::dsp::IIR::Coefficients<float>::makeHighShelf (
                sampleRate, (float) fHi, sQ, gD));
        return stages;
    }

    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeStages (double sampleRate, float frequencyHz, float q, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;

        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        if (std::abs (gainDb) < 1.0e-3f)
            return stages;

        // Don't let f0 sit in the last ~15% of Nyquist for BP map
        const double f0 = juce::jlimit (30.0, fs * 0.38, (double) frequencyHz);
        const float safeQ = juce::jmax (0.15f, q);

        // Narrower than before: Q 8 → ~0.22 oct, Q 4 → ~0.35, Q 1 → ~0.9, Q 0.4 → ~1.6
        const double bwOct = (double) juce::jlimit (0.18f, 2.5f, 0.85f / std::sqrt (safeQ));

        double widthHz = f0 * (std::pow (2.0, 0.5 * bwOct) - std::pow (2.0, -0.5 * bwOct));
        // Allow quite narrow absolute width
        widthHz = juce::jlimit (f0 * 0.03, f0 * 2.5, widthHz);

        const double fc = f0 / fs;
        double fw = widthHz / fs;

        auto bp = makeBpCtx (fc, fw);
        if (! bp.valid)
        {
            // Retry with smaller width
            fw *= 0.5;
            bp = makeBpCtx (fc, fw);
        }
        if (! bp.valid)
            return makeDualEdgeFallback (sampleRate, f0, bwOct, gainDb);

        const auto analog = designAnalogLowShelf (kAnalogOrder, (double) gainDb);

        for (const auto& ap : analog)
        {
            Complex p0, p1, z0, z1;
            bpTransformRoot (bp, ap.pole, p0, p1);
            bpTransformRoot (bp, ap.zero, z0, z1);

            // Falco: transform first of conjugate pair → two digital roots;
            // each becomes a conjugate biquad via conj(p), conj(z).
            if (auto s0 = biquadFromConjPair (p0, z0, 1.0))
                stages.add (std::move (s0));
            if (auto s1 = biquadFromConjPair (p1, z1, 1.0))
                stages.add (std::move (s1));
        }

        if (stages.isEmpty())
            return makeDualEdgeFallback (sampleRate, f0, bwOct, gainDb);

        // Scale |H(f0)| → target gain; clamp scale so bad layouts can't explode
        {
            const double w = 2.0 * kPi * f0 / fs;
            const Complex ejw (std::cos (w), std::sin (w));
            Complex H (1.0, 0.0);

            for (int i = 0; i < stages.size(); ++i)
            {
                const juce::dsp::IIR::Coefficients<float>::Ptr sec = stages.getUnchecked (i);
                if (sec == nullptr)
                    continue;
                const Complex z1 = std::conj (ejw);
                const Complex z2 = z1 * z1;
                const float* c = sec->getRawCoefficients();
                const Complex num = (double) c[0] + (double) c[1] * z1 + (double) c[2] * z2;
                const Complex den = (double) c[3] + (double) c[4] * z1 + (double) c[5] * z2;
                if (std::abs (den) > 1.0e-30 && isFiniteC (num) && isFiniteC (den))
                    H *= num / den;
            }

            if (! isFiniteC (H) || std::abs (H) < 1.0e-9 || std::abs (H) > 1.0e6)
                return makeDualEdgeFallback (sampleRate, f0, bwOct, gainDb);

            const double target = std::pow (10.0, (double) gainDb / 20.0);
            double scale = target / std::abs (H);
            // Prevent insane make-up gain from a near-null at f0
            scale = juce::jlimit (1.0e-3, 50.0, scale);

            juce::dsp::IIR::Coefficients<float>::Ptr s0 = stages.getUnchecked (0);
            if (s0 != nullptr)
            {
                float* c = s0->getRawCoefficients();
                c[0] = (float) ((double) c[0] * scale);
                c[1] = (float) ((double) c[1] * scale);
                c[2] = (float) ((double) c[2] * scale);
            }
        }

        // Final sanity: reject cascade if any coeff is non-finite
        for (int i = 0; i < stages.size(); ++i)
        {
            const auto sec = stages.getUnchecked (i);
            if (sec == nullptr)
                return makeDualEdgeFallback (sampleRate, f0, bwOct, gainDb);
            const float* c = sec->getRawCoefficients();
            for (int k = 0; k < 6; ++k)
                if (! std::isfinite (c[k]))
                    return makeDualEdgeFallback (sampleRate, f0, bwOct, gainDb);
        }

        while (stages.size() > 8)
            stages.removeLast();

        return stages;
    }
}
