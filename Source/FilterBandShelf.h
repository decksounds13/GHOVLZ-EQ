#pragma once

#include <JuceHeader.h>
#include <complex>
#include <vector>
#include <cmath>

/**
    Butterworth band-shelf (Falco / DSPFilters style).

    Analog low-shelf prototype → band-pass bilinear transform → SOS cascade.
    Port adapted from Vinnie Falco's DSPFilters (MIT License):
      https://github.com/vinniefalco/DSPFilters
      Butterworth.cpp  AnalogLowShelf + BandShelfBase::setup
      PoleFilter.cpp   BandPassTransform

    Order-4 analog → 8 digital poles → 4 biquads. Edges steepen with order,
    so narrow bandwidth still plateaus instead of collapsing to a soft bell.

    Competitiveness (honest):
      • Same DSP family as Ozone min-phase “Band Shelf” (flat-top / block boost).
      • Pro-Q has no named band shelf; high-order peaking can mimic wide flats.
      • Shape quality is competitive for min-phase IIR mixing/mastering use.
      • Ozone Digital / FIR “surgical” modes can still look sharper (and cost latency).
*/
namespace FilterBandShelf
{
    using Complex = std::complex<double>;
    static constexpr double kPi = 3.141592653589793238462643383279502884;

    /** Analog shelf order N → digital band-shelf has 2N poles → N biquads. */
    static constexpr int kAnalogOrder = 4;

    inline Complex addmul (Complex c, double v, Complex c1) noexcept
    {
        return { c.real() + v * c1.real(), c.imag() + v * c1.imag() };
    }

    inline Complex stabilize (Complex z) noexcept
    {
        const double m = std::abs (z);
        if (m > 0.999999 && m > 1.0e-12)
            return z * (0.999999 / m);
        return z;
    }

    // ── Analog low-shelf poles/zeros (Falco AnalogLowShelf::design) ─────────

    struct AnalogPair
    {
        Complex pole {}; // first of conjugate pair (or real if single)
        Complex zero {};
        bool isReal = false;
    };

    inline std::vector<AnalogPair> designAnalogLowShelf (int numPoles, double gainDb)
    {
        std::vector<AnalogPair> out;
        if (numPoles < 1)
            return out;

        const double n2 = (double) numPoles * 2.0;
        const double gLin = std::pow (10.0, gainDb / 20.0);
        const double g = std::pow (std::abs (gLin), 1.0 / n2);
        const double gp = -1.0 / juce::jmax (1.0e-12, g);
        const double gz = -g;

        const int pairs = numPoles / 2;
        for (int i = 1; i <= pairs; ++i)
        {
            const double theta = kPi * (0.5 - (2.0 * (double) i - 1.0) / n2);
            AnalogPair ap;
            ap.pole = std::polar (gp, theta);
            ap.zero = std::polar (gz, theta);
            ap.isReal = false;
            out.push_back (ap);
        }

        if (numPoles & 1)
        {
            AnalogPair ap;
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
    };

    inline BpCtx makeBpCtx (double fcNorm, double fwNorm) noexcept
    {
        BpCtx c;
        const double ww = 2.0 * kPi * fwNorm;
        double wc2 = 2.0 * kPi * fcNorm - ww * 0.5;
        double wc  = wc2 + ww;
        if (wc2 < 1.0e-8) wc2 = 1.0e-8;
        if (wc  > kPi - 1.0e-8) wc = kPi - 1.0e-8;

        c.a = std::cos ((wc + wc2) * 0.5) / std::cos ((wc - wc2) * 0.5);
        c.b = 1.0 / std::tan ((wc - wc2) * 0.5);
        c.a2 = c.a * c.a;
        c.b2 = c.b * c.b;
        c.ab = c.a * c.b;
        c.ab_2 = 2.0 * c.ab;
        return c;
    }

    /** Map one analog s-root → two digital z-roots (Falco transform). */
    inline void bpTransformRoot (const BpCtx& bp, Complex s, Complex& o0, Complex& o1) noexcept
    {
        if (! std::isfinite (s.real()) || ! std::isfinite (s.imag()))
        {
            o0 = Complex (-1.0, 0.0);
            o1 = Complex (1.0, 0.0);
            return;
        }

        Complex c = (Complex (1.0, 0.0) + s) / (Complex (1.0, 0.0) - s);

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

        if (std::abs (d) < 1.0e-30)
        {
            o0 = o1 = Complex (0.0, 0.0);
            return;
        }

        o0 = stabilize (u / d);
        o1 = stabilize (v / d);
    }

    // ── Biquad from conjugate pole/zero (Falco setTwoPole + applyScale) ─────

    inline juce::dsp::IIR::Coefficients<float>::Ptr biquadConj (
        Complex pole, Complex zero, double scale)
    {
        const double a1 = -2.0 * pole.real();
        const double a2 = std::norm (pole);
        const double b0 = scale;
        const double b1 = scale * (-2.0 * zero.real());
        const double b2 = scale * std::norm (zero);

        return new juce::dsp::IIR::Coefficients<float> (
            (float) b0, (float) b1, (float) b2,
            1.0f, (float) a1, (float) a2);
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr biquadTwoReal (
        Complex p1, Complex z1, Complex p2, Complex z2, double scale)
    {
        // Two real poles/zeros in one biquad (Falco setTwoPole real branch)
        const double a1 = -(p1.real() + p2.real());
        const double a2 = p1.real() * p2.real();
        const double b0 = scale;
        const double b1 = scale * (-(z1.real() + z2.real()));
        const double b2 = scale * (z1.real() * z2.real());

        return new juce::dsp::IIR::Coefficients<float> (
            (float) b0, (float) b1, (float) b2,
            1.0f, (float) a1, (float) a2);
    }

    // ── Public ──────────────────────────────────────────────────────────────

    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeStages (double sampleRate, float frequencyHz, float q, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;

        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        if (std::abs (gainDb) < 1.0e-3f)
            return stages;

        const double f0 = juce::jlimit (25.0, fs * 0.42, (double) frequencyHz);
        const float safeQ = juce::jmax (0.15f, q);

        // Bandwidth in octaves → absolute Hz (Falco widthFrequency)
        const double bwOct = (double) juce::jlimit (0.35f, 2.8f, 1.15f / std::sqrt (safeQ));
        double widthHz = f0 * (std::pow (2.0, 0.5 * bwOct) - std::pow (2.0, -0.5 * bwOct));
        widthHz = juce::jlimit (f0 * 0.08, f0 * 2.8, widthHz);

        // Keep band edges away from 0 / Nyquist (BP map is singular at extremes)
        const double fc = f0 / fs;
        double fw = widthHz / fs;
        if (fc - 0.5 * fw < 0.002)
            fw = 2.0 * (fc - 0.002);
        if (fc + 0.5 * fw > 0.48)
            fw = 2.0 * (0.48 - fc);
        if (fw < 0.004)
            fw = 0.004;

        const auto analog = designAnalogLowShelf (kAnalogOrder, (double) gainDb);
        const auto bp = makeBpCtx (fc, fw);

        // Falco: for each analog complex pair, transform first, add two conj biquads
        for (const auto& ap : analog)
        {
            Complex pA, pB, zA, zB;
            bpTransformRoot (bp, ap.pole, pA, pB);
            bpTransformRoot (bp, ap.zero, zA, zB);

            if (ap.isReal)
            {
                // Real analog root → two digital roots; pack if both nearly real
                if (std::abs (pA.imag()) < 1.0e-8 && std::abs (pB.imag()) < 1.0e-8)
                    stages.add (biquadTwoReal (pA, zA, pB, zB, 1.0));
                else
                {
                    stages.add (biquadConj (pA, zA, 1.0));
                    stages.add (biquadConj (pB, zB, 1.0));
                }
            }
            else
            {
                // Conjugate analog pair implied — Falco only stores first; transform
                // of conjugate is conjugate of transform (asserted in debug builds).
                stages.add (biquadConj (pA, zA, 1.0));
                stages.add (biquadConj (pB, zB, 1.0));
            }
        }

        if (stages.isEmpty())
            return stages;

        // Normalise |H| at centre frequency to the requested linear gain
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
                if (std::abs (den) > 1.0e-30)
                    H *= num / den;
            }

            const double mag = std::abs (H);
            const double target = std::pow (10.0, (double) gainDb / 20.0);
            if (mag > 1.0e-12)
            {
                const double scale = target / mag;
                juce::dsp::IIR::Coefficients<float>::Ptr s0 = stages.getUnchecked (0);
                if (s0 != nullptr)
                {
                    float* c = s0->getRawCoefficients();
                    c[0] = (float) ((double) c[0] * scale);
                    c[1] = (float) ((double) c[1] * scale);
                    c[2] = (float) ((double) c[2] * scale);
                }
            }
        }

        while (stages.size() > 8)
            stages.removeLast();

        return stages;
    }
}
