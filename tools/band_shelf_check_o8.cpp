// Standalone verification of dual higher-order high-shelf band shelf.
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <algorithm>

using Complex = std::complex<double>;
static constexpr double kPi = 3.141592653589793238462643383279502884;
static constexpr int kEdgeOrder = 8;

Complex stabilizePole (Complex z)
{
    const double m = std::abs (z);
    if (m >= 1.0)
        return z * (0.9995 / std::max (m, 1e-12));
    return z;
}

struct Stage { double b0, b1, b2, a0, a1, a2; };

struct AnalogPz { Complex pole, zero; bool isReal; };

std::vector<AnalogPz> designAnalogLowShelf (int numPoles, double gainDb)
{
    std::vector<AnalogPz> out;
    const double n2 = (double) numPoles * 2.0;
    const double gLin = std::pow (10.0, gainDb / 20.0);
    const double g = std::pow (std::abs (gLin), 1.0 / n2);
    const double gp = -1.0 / std::max (1e-12, g);
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

double highPassPrewarp (double fcNorm)
{
    const double fc = std::clamp (fcNorm, 1e-6, 0.499);
    const double t = std::tan (kPi * fc);
    return 1.0 / std::max (t, 1e-12);
}

Complex hpRoot (double prewarp, Complex s, bool isPole)
{
    Complex c = s * prewarp;
    Complex z = -(1.0 + c) / (1.0 - c);
    if (isPole) z = stabilizePole (z);
    return z;
}

Stage biquadConj (Complex pole, Complex zero)
{
    Stage s;
    s.a0 = 1; s.a1 = -2.0 * pole.real(); s.a2 = std::norm (pole);
    s.b0 = 1; s.b1 = -2.0 * zero.real(); s.b2 = std::norm (zero);
    return s;
}

void appendHS (std::vector<Stage>& stages, double fs, double fcHz, double gainDb, int order)
{
    const double pre = highPassPrewarp (fcHz / fs);
    for (const auto& ap : designAnalogLowShelf (order, gainDb))
    {
        Complex p = hpRoot (pre, ap.pole, true);
        Complex z = hpRoot (pre, ap.zero, false);
        stages.push_back (biquadConj (p, z));
    }
}

double evalMag (const std::vector<Stage>& stages, double freqHz, double fs)
{
    const double w = 2.0 * kPi * freqHz / fs;
    Complex zm1 (std::cos (-w), std::sin (-w));
    Complex zm2 = zm1 * zm1;
    Complex H (1.0, 0.0);
    for (const auto& s : stages)
    {
        Complex num = s.b0 + s.b1 * zm1 + s.b2 * zm2;
        Complex den = s.a0 + s.a1 * zm1 + s.a2 * zm2;
        H *= num / den;
    }
    return std::abs (H);
}

std::vector<Stage> makeBandShelf (double fs, double f0, double q, double gainDb)
{
    const float safeQ = (float) std::max (0.15, q);
    const double bwOct = std::clamp (1.05 / std::pow (safeQ, 0.55), 0.35, 3.2);
    double r = std::pow (2.0, 0.5 * bwOct);
    double fLo = f0 / r;
    double fHi = f0 * r;
    const double fMax = fs * 0.45;
    if (fHi > fMax) { fHi = fMax; fLo = (f0 * f0) / fHi; }
    if (fLo < 18.0) { fLo = 18.0; fHi = std::min (fMax, (f0 * f0) / fLo); }

    std::vector<Stage> stages;
    appendHS (stages, fs, fLo, gainDb, kEdgeOrder);
    appendHS (stages, fs, fHi, -gainDb, kEdgeOrder);

    // OOB norm below f_lo
    const double oobHz = std::max (5.0, fLo * 0.15);
    const double magOob = evalMag (stages, oobHz, fs);
    const double scale = 1.0 / magOob;
    stages[0].b0 *= scale; stages[0].b1 *= scale; stages[0].b2 *= scale;
    return stages;
}

// Soft dual JUCE-like (identical Q=0.707 half-gain cascades) for comparison
std::vector<Stage> makeSoftDual (double fs, double f0, double q, double gainDb)
{
    // Approximate soft shelf with peaking-style alpha — skip, just report dual soft via RBJ formula
    // Use RBJ high shelf twice
    auto rbjHS = [] (double fs, double f, double gainDb, double S) -> Stage {
        const double A = std::pow (10.0, gainDb / 40.0);
        const double w0 = 2 * kPi * f / fs;
        const double cs = std::cos (w0), sn = std::sin (w0);
        const double AL = sn / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
        const double sq = 2 * std::sqrt (A) * AL;
        Stage s;
        double b0 = A * ((A + 1) + (A - 1) * cs + sq);
        double b1 = -2 * A * ((A - 1) + (A + 1) * cs);
        double b2 = A * ((A + 1) + (A - 1) * cs - sq);
        double a0 = (A + 1) - (A - 1) * cs + sq;
        double a1 = 2 * ((A - 1) - (A + 1) * cs);
        double a2 = (A + 1) - (A - 1) * cs - sq;
        s.b0 = b0 / a0; s.b1 = b1 / a0; s.b2 = b2 / a0;
        s.a0 = 1; s.a1 = a1 / a0; s.a2 = a2 / a0;
        return s;
    };

    const float safeQ = (float) std::max (0.15, q);
    const double bwOct = std::clamp (1.05 / std::pow (safeQ, 0.55), 0.35, 3.2);
    double r = std::pow (2.0, 0.5 * bwOct);
    double fLo = f0 / r, fHi = f0 * r;
    const int n = 3;
    std::vector<Stage> stages;
    for (int i = 0; i < n; ++i)
        stages.push_back (rbjHS (fs, fLo, gainDb / n, 1.0));
    for (int i = 0; i < n; ++i)
        stages.push_back (rbjHS (fs, fHi, -gainDb / n, 1.0));
    return stages;
}

void report (const char* label, const std::vector<Stage>& st, double fs, double f0, double gainDb, double q)
{
    if (st.empty()) { std::printf ("%s Q=%.2f EMPTY\n", label, q); return; }
    auto db = [&] (double f) {
        return 20.0 * std::log10 (std::max (1e-15, evalMag (st, f, fs)));
    };

    const float safeQ = (float) std::max (0.15, q);
    const double bwOct = std::clamp (1.05 / std::pow (safeQ, 0.55), 0.35, 3.2);
    const double r = std::pow (2.0, 0.5 * bwOct);
    const double fLo = f0 / r, fHi = f0 * r;

    const double dLo = db (std::max (10.0, fLo * 0.2));
    const double dC  = db (f0);
    const double dHi = db (std::min (fs * 0.45, fHi * 5.0));
    const double dMidL = db (f0 / std::sqrt (r));
    const double dMidR = db (f0 * std::sqrt (r));
    const double dEdgeL = db (fLo);
    const double dEdgeR = db (fHi);

    std::printf ("%s Q=%5.2f bw=%.2f  OOB=%+6.2f  midL=%+6.2f  ctr=%+6.2f  midR=%+6.2f  "
                 "edgeL=%+5.1f edgeR=%+5.1f  OOB_hi=%+6.2f  tgt=%+5.1f\n",
                 label, q, bwOct, dLo, dMidL, dC, dMidR, dEdgeL, dEdgeR, dHi, gainDb);
}

int main()
{
    const double fs = 48000.0;
    const double f0 = 1000.0;
    const double qs[] = { 0.15, 0.23, 0.5, 0.7, 1.0, 2.0, 5.0, 10.0 };

    std::printf ("=== Dual order-%d HS, cut -12 dB @ 1 kHz ===\n", kEdgeOrder);
    for (double q : qs)
        report ("HO", makeBandShelf (fs, f0, q, -12.0), fs, f0, -12.0, q);

    std::printf ("\n=== Soft dual (3x RBJ) for comparison ===\n");
    for (double q : qs)
        report ("SF", makeSoftDual (fs, f0, q, -12.0), fs, f0, -12.0, q);

    std::printf ("\n=== Dual HO boost +12 dB ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, f0, q, +12.0), fs, f0, +12.0, q);

    std::printf ("\n=== Dual HO cut -12 @ 8 kHz ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, 8000.0, q, -12.0), fs, 8000.0, -12.0, q);

    std::printf ("\n=== Dual HO cut -6 @ 200 Hz ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, 200.0, q, -6.0), fs, 200.0, -6.0, q);

    return 0;
}
