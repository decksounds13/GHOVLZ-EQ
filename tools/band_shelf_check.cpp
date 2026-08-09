// Standalone verification matching FilterBandShelf.h (dual HO HS + gain lock).
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
    if (m >= 1.0) return z * (0.9995 / std::max (m, 1e-12));
    return z;
}

struct Stage { double b0, b1, b2, a0, a1, a2; };
struct AnalogPz { Complex pole, zero; bool isReal; };

std::vector<AnalogPz> designAnalogLowShelf (int numPoles, double gainDb)
{
    std::vector<AnalogPz> out;
    const double n2 = (double) numPoles * 2.0;
    const double g = std::pow (std::abs (std::pow (10.0, gainDb / 20.0)), 1.0 / n2);
    const double gp = -1.0 / std::max (1e-12, g);
    const double gz = -g;
    for (int i = 1; i <= numPoles / 2; ++i)
    {
        const double theta = kPi * (0.5 - (2.0 * (double) i - 1.0) / n2);
        out.push_back ({ std::polar (gp, theta), std::polar (gz, theta), false });
    }
    return out;
}

double highPassPrewarp (double fcNorm)
{
    return 1.0 / std::max (std::tan (kPi * std::clamp (fcNorm, 1e-6, 0.499)), 1e-12);
}

Complex hpRoot (double prewarp, Complex s, bool isPole)
{
    Complex z = -(1.0 + s * prewarp) / (1.0 - s * prewarp);
    if (isPole) z = stabilizePole (z);
    return z;
}

Stage biquadConj (Complex pole, Complex zero)
{
    return { 1, -2 * zero.real(), std::norm (zero), 1, -2 * pole.real(), std::norm (pole) };
}

void appendHS (std::vector<Stage>& stages, double fs, double fcHz, double gainDb, int order)
{
    const double pre = highPassPrewarp (fcHz / fs);
    for (const auto& ap : designAnalogLowShelf (order, gainDb))
        stages.push_back (biquadConj (hpRoot (pre, ap.pole, true), hpRoot (pre, ap.zero, false)));
}

double evalMag (const std::vector<Stage>& stages, double freqHz, double fs)
{
    const double w = 2.0 * kPi * freqHz / fs;
    Complex zm1 (std::cos (-w), std::sin (-w)), zm2 = zm1 * zm1, H (1, 0);
    for (const auto& s : stages)
        H *= (s.b0 + s.b1 * zm1 + s.b2 * zm2) / (s.a0 + s.a1 * zm1 + s.a2 * zm2);
    return std::abs (H);
}

std::vector<Stage> build (double fs, double fLo, double fHi, double designDb)
{
    std::vector<Stage> stages;
    appendHS (stages, fs, fLo, designDb, kEdgeOrder);
    appendHS (stages, fs, fHi, -designDb, kEdgeOrder);
    const double oob = std::max (5.0, fLo * 0.15);
    const double sc = 1.0 / evalMag (stages, oob, fs);
    stages[0].b0 *= sc; stages[0].b1 *= sc; stages[0].b2 *= sc;
    return stages;
}

std::vector<Stage> makeBandShelf (double fs, double f0, double q, double gainDb)
{
    const float safeQ = (float) std::max (0.15, q);
    const double bwOct = std::clamp (1.05 / std::pow (safeQ, 0.55), 0.40, 3.2);
    double r = std::pow (2.0, 0.5 * bwOct);
    double fLo = f0 / r, fHi = f0 * r;
    const double fMax = fs * 0.45;
    if (fHi > fMax) { fHi = fMax; fLo = (f0 * f0) / fHi; }
    if (fLo < 18.0) { fLo = 18.0; fHi = std::min (fMax, (f0 * f0) / fLo); }

    double designDb = gainDb;
    std::vector<Stage> stages;
    for (int iter = 0; iter < 4; ++iter)
    {
        stages = build (fs, fLo, fHi, designDb);
        const double actualDb = 20.0 * std::log10 (std::max (1e-15, evalMag (stages, f0, fs)));
        if (std::abs (actualDb - gainDb) < 0.20) break;
        if (std::abs (actualDb) < 0.15)
        {
            designDb = std::clamp (gainDb * 2.0, -48.0, 48.0);
            continue;
        }
        designDb *= std::clamp (gainDb / actualDb, 0.5, 3.0);
        designDb = std::clamp (designDb, -48.0, 48.0);
    }
    return stages;
}

void report (const char* label, const std::vector<Stage>& st, double fs, double f0, double gainDb, double q)
{
    auto db = [&] (double f) { return 20.0 * std::log10 (std::max (1e-15, evalMag (st, f, fs))); };
    const float safeQ = (float) std::max (0.15, q);
    const double bwOct = std::clamp (1.05 / std::pow (safeQ, 0.55), 0.40, 3.2);
    const double r = std::pow (2.0, 0.5 * bwOct);
    std::printf ("%s Q=%5.2f bw=%.2f  OOB=%+6.2f  midL=%+6.2f  ctr=%+6.2f  midR=%+6.2f  OOB_hi=%+6.2f  tgt=%+5.1f\n",
                 label, q, bwOct,
                 db (std::max (10.0, f0 / r * 0.2)),
                 db (f0 / std::sqrt (r)),
                 db (f0),
                 db (f0 * std::sqrt (r)),
                 db (std::min (fs * 0.45, f0 * r * 5.0)),
                 gainDb);
}

int main()
{
    const double fs = 48000, f0 = 1000;
    const double qs[] = { 0.15, 0.23, 0.5, 0.7, 1.0, 2.0, 5.0, 10.0 };

    std::printf ("=== Dual O8 + gain-lock, cut -12 @ 1 kHz ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, f0, q, -12.0), fs, f0, -12.0, q);

    std::printf ("\n=== Boost +12 ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, f0, q, +12.0), fs, f0, +12.0, q);

    std::printf ("\n=== Cut -12 @ 8 kHz ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, 8000, q, -12.0), fs, 8000, -12.0, q);

    std::printf ("\n=== Cut -6 @ 200 Hz ===\n");
    for (double q : qs)
        report ("HO", makeBandShelf (fs, 200, q, -6.0), fs, 200, -6.0, q);

    return 0;
}
