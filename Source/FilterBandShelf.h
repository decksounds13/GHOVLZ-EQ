#pragma once

#include <JuceHeader.h>
#include <complex>
#include <vector>
#include <cmath>

/**
    Band shelf — dual higher-order high-shelf edges (log-symmetric about f0).

    Construction:
      f_lo = f0 / r ,  f_hi = f0 * r     (r = 2^{bwOct/2})
      H(z) = HS_N(f_lo, +G) · HS_N(f_hi, −G)

    Each edge is a Butterworth-style analog low-shelf of order N, mapped with
    Falco's high-pass bilinear transform (DSPFilters HighShelf family).
    N=8 → 4 SOS per edge → 8 SOS total (= FilterSlope::maxBiquadStages).

    Why dual high shelves (not low-shelf→band-pass)?
      BP maps are arithmetic in Hz; wide musical bands leave the handle and
      collapse toward DC. Dual HS edges are log-symmetric about the handle.

    Q maps ONLY to bandwidth in octaves. After design we:
      1) Normalise |H| out-of-band to 0 dB
      2) If the plateau is short of the handle (edges overlapping when narrow),
         redesign with inflated |gainDb| so |H(f0)| matches the handle.
         Overall b[] scale is NOT used for depth — that would drag the floor.

    Port pieces from Vinnie Falco DSPFilters (MIT):
      Butterworth.cpp  AnalogLowShelf
      PoleFilter.cpp   HighPassTransform
*/
namespace FilterBandShelf
{
    using Complex = std::complex<double>;
    static constexpr double kPi = 3.141592653589793238462643383279502884;

    /** Order per edge. Dual → 2*(N/2) = N SOS for even N. N=8 → 8 stages. */
    static constexpr int kEdgeOrder = 8;

    inline Complex addmul (Complex c, double v, Complex c1) noexcept
    {
        return { c.real() + v * c1.real(), c.imag() + v * c1.imag() };
    }

    inline bool isFiniteC (Complex z) noexcept
    {
        return std::isfinite (z.real()) && std::isfinite (z.imag());
    }

    /** Stabilize poles only (|p|≥1 → pull inside). Never apply to zeros. */
    inline Complex stabilizePole (Complex z) noexcept
    {
        if (! isFiniteC (z))
            return Complex (0.0, 0.0);
        const double m = std::abs (z);
        if (m >= 1.0)
            return z * (0.9995 / juce::jmax (m, 1.0e-12));
        return z;
    }

    // ── Analog low-shelf prototype (Falco AnalogLowShelf::design) ───────────

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

        // g = |10^(dB/20)|^(1/(2N));  gp = -1/g;  gz = -g
        // std::polar with negative rho → LHP roots (Falco relies on this).
        const double n2 = (double) numPoles * 2.0;
        const double gLin = std::pow (10.0, gainDb / 20.0);
        const double g = std::pow (std::abs (gLin), 1.0 / n2);
        const double gp = -1.0 / juce::jmax (1.0e-12, g);
        const double gz = -g;

        const int pairs = numPoles / 2;
        for (int i = 1; i <= pairs; ++i)
        {
            const double theta = kPi * (0.5 - (2.0 * (double) i - 1.0) / n2);
            AnalogPz ap;
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

    // ── High-pass bilinear (Falco HighPassTransform) ────────────────────────

    inline double highPassPrewarp (double fcNorm) noexcept
    {
        const double fc = juce::jlimit (1.0e-6, 0.499, fcNorm);
        const double t = std::tan (kPi * fc);
        if (t < 1.0e-12)
            return 1.0e12;
        return 1.0 / t;
    }

    inline Complex highPassTransformRoot (double prewarp, Complex s, bool isPole) noexcept
    {
        if (! isFiniteC (s))
            return isPole ? Complex (0.0, 0.0) : Complex (1.0, 0.0);

        if (std::abs (s) > 1.0e12)
            return Complex (1.0, 0.0);

        Complex c = s * prewarp;
        const Complex one (1.0, 0.0);
        if (std::abs (one - c) < 1.0e-14)
            return Complex (isPole ? 0.0 : 1.0, 0.0);

        Complex z = -(one + c) / (one - c);
        if (! isFiniteC (z))
            return isPole ? Complex (0.0, 0.0) : Complex (1.0, 0.0);

        if (isPole)
            z = stabilizePole (z);
        // zeros: leave alone — |z| may be > 1 (required for boosts)
        return z;
    }

    // ── Biquads ─────────────────────────────────────────────────────────────

    inline juce::dsp::IIR::Coefficients<float>::Ptr biquadFromConjPair (
        Complex pole, Complex zero)
    {
        if (! isFiniteC (pole) || ! isFiniteC (zero))
            return {};

        const double a1 = -2.0 * pole.real();
        const double a2 = std::norm (pole);
        if (! (a2 < 1.0) || ! (std::abs (a1) < 1.0 + a2 + 1.0e-9))
            return {};

        const double b0 = 1.0;
        const double b1 = -2.0 * zero.real();
        const double b2 = std::norm (zero);

        if (! std::isfinite (b1) || ! std::isfinite (b2))
            return {};

        return new juce::dsp::IIR::Coefficients<float> (
            (float) b0, (float) b1, (float) b2,
            1.0f, (float) a1, (float) a2);
    }

    inline juce::dsp::IIR::Coefficients<float>::Ptr biquadOnePole (
        Complex pole, Complex zero)
    {
        if (! isFiniteC (pole) || ! isFiniteC (zero))
            return {};
        if (std::abs (pole.imag()) > 1.0e-9 || std::abs (zero.imag()) > 1.0e-9)
            return {};

        const double p = pole.real();
        const double z = zero.real();
        if (std::abs (p) >= 1.0)
            return {};

        return new juce::dsp::IIR::Coefficients<float> (
            1.0f, (float) (-z), 0.0f,
            1.0f, (float) (-p), 0.0f);
    }

    // ── Cascade helpers ─────────────────────────────────────────────────────

    /**
        Magnitude of one JUCE IIR section.

        Important: Coefficients::assign() divides by a0 and DROPS a0 from the
        stored array. Layout is therefore:
          order 1 → [b0, b1, a1]                 (3 floats)
          order 2 → [b0, b1, b2, a1, a2]         (5 floats)
        Matching Filter::processSample / getMagnitudeForFrequency.
        Reading c[3..5] as if a0 were still present corrupts design-time
        normalise/gain-lock and made the shelf collapse above ~200 Hz.
    */
    inline double evalSectionMagAt (
        const juce::dsp::IIR::Coefficients<float>& sec,
        double freqHz, double sampleRate) noexcept
    {
        const double nyq = 0.5 * sampleRate;
        const double f = juce::jmin (freqHz, nyq * 0.999999);
        const double w = 2.0 * kPi * f / sampleRate;
        // z^{-1}
        const Complex zm1 (std::cos (-w), std::sin (-w));

        const float* c = sec.getRawCoefficients();
        const size_t order = sec.getFilterOrder();
        const int n = sec.coefficients.size();
        if (c == nullptr || n < 2 || order < 1)
            return 1.0;

        // Same as juce::dsp::IIR::Coefficients::getMagnitudeForFrequency
        Complex num (0.0, 0.0);
        Complex factor (1.0, 0.0);
        for (size_t k = 0; k <= order; ++k)
        {
            num += (double) c[k] * factor;
            factor *= zm1;
        }

        Complex den (1.0, 0.0);
        factor = zm1;
        for (size_t k = order + 1; k <= 2 * order; ++k)
        {
            // Stored array has length 2*order+1 (a0 removed): indices order+1 .. 2*order
            // map to a1 .. a_order after the b's.
            if ((int) k >= n)
                break;
            den += (double) c[k] * factor;
            factor *= zm1;
        }

        if (std::abs (den) < 1.0e-30 || ! isFiniteC (num) || ! isFiniteC (den))
            return 1.0;

        const double mag = std::abs (num / den);
        return (std::isfinite (mag) && mag > 1.0e-30) ? mag : 1.0;
    }

    inline double evalCascadeMagAt (
        const juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& stages,
        double freqHz, double sampleRate) noexcept
    {
        if (stages.isEmpty() || sampleRate <= 0.0 || freqHz < 0.0)
            return 1.0;

        double mag = 1.0;
        for (int i = 0; i < stages.size(); ++i)
        {
            const juce::dsp::IIR::Coefficients<float>::Ptr sec = stages.getUnchecked (i);
            if (sec == nullptr)
                continue;
            mag *= evalSectionMagAt (*sec, freqHz, sampleRate);
        }

        return (std::isfinite (mag) && mag > 1.0e-30) ? mag : 1.0;
    }

    inline void scaleFirstStage (
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& stages,
        double linearScale) noexcept
    {
        if (stages.isEmpty() || ! std::isfinite (linearScale) || linearScale <= 0.0)
            return;
        juce::dsp::IIR::Coefficients<float>::Ptr s0 = stages.getUnchecked (0);
        if (s0 == nullptr)
            return;
        // Stored layout: [b0, b1, b2, a1, a2] — scale all b's (first order+1 entries).
        float* c = s0->getRawCoefficients();
        const size_t order = s0->getFilterOrder();
        for (size_t k = 0; k <= order; ++k)
            c[k] = (float) ((double) c[k] * linearScale);
    }

    /** Append one higher-order high shelf at cutoffHz with gainDb. */
    inline bool appendHighShelf (
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>& stages,
        double sampleRate, double cutoffHz, double gainDb, int order)
    {
        if (std::abs (gainDb) < 1.0e-6 || order < 1)
            return true;

        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        const double fc = juce::jlimit (10.0, fs * 0.45, cutoffHz);
        const double prewarp = highPassPrewarp (fc / fs);
        const auto analog = designAnalogLowShelf (order, gainDb);

        for (const auto& ap : analog)
        {
            if (ap.isReal)
            {
                const Complex p = highPassTransformRoot (prewarp, ap.pole, true);
                const Complex z = highPassTransformRoot (prewarp, ap.zero, false);
                if (auto s = biquadOnePole (p, z))
                    stages.add (std::move (s));
                else
                    return false;
            }
            else
            {
                const Complex p = highPassTransformRoot (prewarp, ap.pole, true);
                const Complex z = highPassTransformRoot (prewarp, ap.zero, false);
                if (auto s = biquadFromConjPair (p, z))
                    stages.add (std::move (s));
                else
                    return false;
            }
        }

        return true;
    }

    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeSoftDualEdge (double sampleRate, double fLo, double fHi, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        const int nEdge = 4;
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

    /** Build dual-edge cascade for a design gain (may differ from handle gain). */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        buildDualEdges (double sampleRate, double fLo, double fHi, double designGainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;
        if (! appendHighShelf (stages, sampleRate, fLo, designGainDb, kEdgeOrder)
            || ! appendHighShelf (stages, sampleRate, fHi, -designGainDb, kEdgeOrder)
            || stages.isEmpty())
        {
            return {};
        }

        while (stages.size() > 8)
            stages.removeLast();

        // Normalise floor (well below lower edge) to 0 dB
        const double oobHz = juce::jmax (5.0, fLo * 0.15);
        const double magOob = evalCascadeMagAt (stages, oobHz, sampleRate);
        if (magOob > 1.0e-12 && std::isfinite (magOob))
        {
            const double scale = 1.0 / magOob;
            if (std::isfinite (scale) && scale > 0.0 && scale < 1.0e5)
                scaleFirstStage (stages, scale);
        }

        return stages;
    }

    // ── Public ──────────────────────────────────────────────────────────────

    /**
        @param q  Higher → narrower. Maps to ~0.40…3.2 octaves of plateau width.
                  Does not change plateau depth — only the edge separation.
    */
    inline juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>
        makeStages (double sampleRate, float frequencyHz, float q, float gainDb)
    {
        juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>> stages;

        const double fs = sampleRate > 1.0 ? sampleRate : 48000.0;
        if (std::abs (gainDb) < 1.0e-3f)
            return stages;

        const double fMax = fs * 0.45;
        const double f0 = juce::jlimit (25.0, fMax * 0.98, (double) frequencyHz);
        const float safeQ = juce::jmax (0.15f, q);

        // Q → bandwidth in octaves (WIDTH ONLY).
        // Floor ~0.40 oct so max-Q still has a short plateau (not a pure peak).
        // Q 0.15 → ~2.9 oct,  0.5 → ~1.5,  1 → ~1.05,  2 → ~0.72,  5 → ~0.45,  10 → ~0.40
        const double bwOct = (double) juce::jlimit (
            0.40f, 3.2f,
            1.05f / std::pow (safeQ, 0.55f));

        double r = std::pow (2.0, 0.5 * bwOct);
        double fLo = f0 / r;
        double fHi = f0 * r;

        // Nyquist / DC — re-mirror about f0 so the handle stays log-centre
        if (fHi > fMax)
        {
            fHi = fMax;
            fLo = (f0 * f0) / fHi;
        }
        if (fLo < 18.0)
        {
            fLo = 18.0;
            fHi = juce::jmin (fMax, (f0 * f0) / fLo);
        }
        if (fHi / fLo < 1.23)
        {
            const double mid = std::sqrt (fLo * fHi);
            fLo = juce::jmax (18.0, mid / std::sqrt (1.23));
            fHi = juce::jmin (fMax, mid * std::sqrt (1.23));
        }

        const double targetLin = (double) juce::Decibels::decibelsToGain (gainDb);
        const double targetDb = (double) gainDb;

        // Iterative design-gain lock: when edges overlap (narrow Q), the cascade
        // depth is less than |G|. Inflate |designGainDb| until |H(f0)| matches
        // the handle. OOB stays ~0 dB because each redesign re-normalises the floor.
        double designDb = targetDb;
        for (int iter = 0; iter < 4; ++iter)
        {
            stages = buildDualEdges (fs, fLo, fHi, designDb);
            if (stages.isEmpty())
                return makeSoftDualEdge (sampleRate, fLo, fHi, gainDb);

            const double magC = evalCascadeMagAt (stages, f0, fs);
            if (! (magC > 1.0e-15) || ! std::isfinite (magC))
                return makeSoftDualEdge (sampleRate, fLo, fHi, gainDb);

            const double actualDb = 20.0 * std::log10 (magC);
            const double errDb = std::abs (actualDb - targetDb);
            if (errDb < 0.20)
                break;

            // Avoid division by near-zero depth
            if (std::abs (actualDb) < 0.15)
            {
                designDb = targetDb * (iter == 0 ? 2.0 : designDb / targetDb * 1.5);
                designDb = juce::jlimit (-48.0, 48.0, designDb);
                continue;
            }

            // designDb' so next actual ≈ target (depth scales roughly with design gain)
            const double ratio = targetDb / actualDb; // same sign → positive
            designDb *= juce::jlimit (0.5, 3.0, ratio);
            designDb = juce::jlimit (-48.0, 48.0, designDb);
        }

        // Final finite-coeff check
        for (int i = 0; i < stages.size(); ++i)
        {
            const auto sec = stages.getUnchecked (i);
            if (sec == nullptr)
                return makeSoftDualEdge (sampleRate, fLo, fHi, gainDb);
            const float* c = sec->getRawCoefficients();
            for (int k = 0; k < 6; ++k)
                if (! std::isfinite (c[k]))
                    return makeSoftDualEdge (sampleRate, fLo, fHi, gainDb);
        }

        juce::ignoreUnused (targetLin);
        return stages;
    }
}
