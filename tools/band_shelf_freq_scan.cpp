// Scan band-shelf depth vs f0 to reproduce high-frequency collapse.
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

std::vector<std::pair<Complex, Complex>> designAnalog (int n, double gainDb)
{
    std::vector<std::pair<Complex, Complex>> out;
    const double n2 = n * 2.0;
    const double g = std::pow (std::abs (std::pow (10.0, gainDb / 20.0)), 1.0 / n2);
    const double gp = -1.0 / std::max (1e-12, g);
    const double gz = -g;
    for (int i = 1; i <= n / 2; ++i)
    {
        const double th = kPi * (0.5 - (2.0 * i - 1.0) / n2);
        out.push_back ({ std::polar (gp, th), std::polar (gz, th) });
    }
    return out;
}

double prewarp (double fcNorm)
{
    return 1.0 / std::max (std::tan (kPi * std::clamp (fcNorm, 1e-6, 0.499)), 1e-12);
}

Complex hpRoot (double pw, Complex s, bool isPole)
{
    Complex z = -(1.0 + s * pw) / (1.0 - s * pw);
    if (isPole) z = stabilizePole (z);
    return z;
}

Stage bq (Complex p, Complex z)
{
    return { 1, -2 * z.real(), std::norm (z), 1, -2 * p.real(), std::norm (p) };
}

bool appendHS (std::vector<Stage>& st, double fs, double fc, double gdb, int order)
{
    const double pw = prewarp (fc / fs);
    for (auto [pole, zero] : designAnalog (order, gdb))
    {
        Complex p = hpRoot (pw, pole, true);
        Complex z = hpRoot (pw, zero, false);
        const double a2 = std::norm (p);
        const double a1 = -2 * p.real();
        if (! (a2 < 1.0) || ! (std::abs (a1) < 1.0 + a2 + 1e-9))
            return false;
        st.push_back (bq (p, z));
    }
    return true;
}

double mag (const std::vector<Stage>& st, double f, double fs)
{
    const double w = 2 * kPi * f / fs;
    Complex zm1 (std::cos (-w), std::sin (-w)), zm2 = zm1 * zm1, H (1, 0);
    for (auto& s : st)
        H *= (s.b0 + s.b1 * zm1 + s.b2 * zm2) / (s.a0 + s.a1 * zm1 + s.a2 * zm2);
    return std::abs (H);
}

std::vector<Stage> build (double fs, double fLo, double fHi, double designDb)
{
    std::vector<Stage> st;
    if (! appendHS (st, fs, fLo, designDb, kEdgeOrder)) return {};
    if (! appendHS (st, fs, fHi, -designDb, kEdgeOrder)) return {};
    const double oob = std::max (5.0, fLo * 0.15);
    double m = mag (st, oob, fs);
    if (m < 1e-15) return {};
    double sc = 1.0 / m;
    st[0].b0 *= sc; st[0].b1 *= sc; st[0].b2 *= sc;
    return st;
}

void scan (double fs, double gainDb, double q)
{
    const float safeQ = (float) std::max (0.15, q);
    const double bwOct = std::clamp (1.05 / std::pow (safeQ, 0.55), 0.40, 3.2);
    std::printf ("\n=== gain=%+.1f Q=%.2f bw=%.2f oct  fs=%.0f ===\n", gainDb, q, bwOct, fs);
    std::printf ("%8s %8s %8s %8s %8s %8s %8s\n", "f0", "fLo", "fHi", "ctrDb", "oobDb", "hiDb", "desDb");

    const double freqs[] = {
        40, 80, 100, 150, 200, 250, 300, 400, 500, 600, 800,
        1000, 1200, 1500, 2000, 2500, 3000, 4000, 5000, 6000, 8000, 10000, 12000
    };

    for (double f0 : freqs)
    {
        double r = std::pow (2.0, 0.5 * bwOct);
        double fLo = f0 / r, fHi = f0 * r;
        const double fMax = fs * 0.45;
        if (fHi > fMax) { fHi = fMax; fLo = (f0 * f0) / fHi; }
        if (fLo < 18.0) { fLo = 18.0; fHi = std::min (fMax, (f0 * f0) / fLo); }

        double designDb = gainDb;
        std::vector<Stage> st;
        for (int iter = 0; iter < 4; ++iter)
        {
            st = build (fs, fLo, fHi, designDb);
            if (st.empty()) { designDb *= 1.2; continue; }
            double actualDb = 20 * std::log10 (std::max (1e-15, mag (st, f0, fs)));
            if (std::abs (actualDb - gainDb) < 0.2) break;
            if (std::abs (actualDb) < 0.15) { designDb = std::clamp (gainDb * 2.0, -48.0, 48.0); continue; }
            designDb *= std::clamp (gainDb / actualDb, 0.5, 3.0);
            designDb = std::clamp (designDb, -48.0, 48.0);
        }
        if (st.empty())
        {
            std::printf ("%8.0f EMPTY\n", f0);
            continue;
        }
        auto db = [&] (double f) { return 20 * std::log10 (std::max (1e-15, mag (st, f, fs))); };
        std::printf ("%8.0f %8.1f %8.1f %8.2f %8.2f %8.2f %8.2f\n",
                     f0, fLo, fHi, db (f0), db (std::max (10.0, fLo * 0.15)),
                     db (std::min (fs * 0.45, fHi * 4)), designDb);
    }
}

int main()
{
    scan (48000, -12, 0.5);
    scan (48000, -12, 1.0);
    scan (48000, -12, 2.0);
    scan (48000, -12, 0.3);
    scan (96000, -12, 1.0);
    return 0;
}
