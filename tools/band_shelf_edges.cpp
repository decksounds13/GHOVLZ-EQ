// Probe band-shelf magnitude near DC / Nyquist for edge pops.
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <algorithm>

// Minimal reimplementation matching FilterBandShelf after JUCE a0-drop layout.
using Complex = std::complex<double>;
static constexpr double kPi = 3.141592653589793238462643383279502884;
static constexpr int kEdgeOrder = 8;

Complex stab (Complex z)
{
    const double m = std::abs (z);
    return m >= 1.0 ? z * (0.9995 / std::max (m, 1e-12)) : z;
}

struct Stage { double b0, b1, b2, a1, a2; }; // JUCE layout (a0=1 dropped)

std::vector<std::pair<Complex, Complex>> design (int n, double gdb)
{
    std::vector<std::pair<Complex, Complex>> out;
    const double n2 = n * 2.0;
    const double g = std::pow (std::abs (std::pow (10.0, gdb / 20.0)), 1.0 / n2);
    const double gp = -1.0 / std::max (g, 1e-12), gz = -g;
    for (int i = 1; i <= n / 2; ++i)
    {
        const double th = kPi * (0.5 - (2.0 * i - 1.0) / n2);
        out.push_back ({ std::polar (gp, th), std::polar (gz, th) });
    }
    return out;
}

double prewarp (double fcN)
{
    return 1.0 / std::max (std::tan (kPi * std::clamp (fcN, 1e-6, 0.499)), 1e-12);
}

Complex hp (double pw, Complex s, bool pole)
{
    Complex z = -(1.0 + s * pw) / (1.0 - s * pw);
    return pole ? stab (z) : z;
}

void appendHS (std::vector<Stage>& st, double fs, double fc, double gdb)
{
    const double pw = prewarp (fc / fs);
    for (auto [p, z] : design (kEdgeOrder, gdb))
    {
        Complex P = hp (pw, p, true), Z = hp (pw, z, false);
        st.push_back ({ 1.0, -2 * Z.real(), std::norm (Z), -2 * P.real(), std::norm (P) });
    }
}

double mag (const std::vector<Stage>& st, double f, double fs)
{
    const double w = 2 * kPi * std::min (f, fs * 0.5 * 0.999999) / fs;
    Complex zm1 (std::cos (-w), std::sin (-w)), zm2 = zm1 * zm1, H (1, 0);
    for (auto& s : st)
    {
        Complex num = s.b0 + s.b1 * zm1 + s.b2 * zm2;
        Complex den = 1.0 + s.a1 * zm1 + s.a2 * zm2;
        H *= num / den;
    }
    return std::abs (H);
}

std::vector<Stage> make (double fs, double f0, double q, double gainDb, double* outDes = nullptr)
{
    const double bw = std::clamp (1.05 / std::pow (std::max (0.15, q), 0.55), 0.40, 3.2);
    double r = std::pow (2.0, 0.5 * bw);
    double fLo = f0 / r, fHi = f0 * r;
    const double fMax = fs * 0.45;
    if (fHi > fMax) { fHi = fMax; fLo = (f0 * f0) / fHi; }
    if (fLo < 18.0) { fLo = 18.0; fHi = std::min (fMax, (f0 * f0) / fLo); }

    double designDb = gainDb;
    std::vector<Stage> st;
    for (int it = 0; it < 4; ++it)
    {
        st.clear();
        appendHS (st, fs, fLo, designDb);
        appendHS (st, fs, fHi, -designDb);
        double oob = std::max (5.0, fLo * 0.15);
        double sc = 1.0 / mag (st, oob, fs);
        st[0].b0 *= sc; st[0].b1 *= sc; st[0].b2 *= sc;

        double ad = 20 * std::log10 (std::max (1e-15, mag (st, f0, fs)));
        if (std::abs (ad - gainDb) < 0.2) break;
        if (std::abs (ad) < 0.15) { designDb = std::clamp (gainDb * 2.0, -48.0, 48.0); continue; }
        designDb *= std::clamp (gainDb / ad, 0.5, 3.0);
        designDb = std::clamp (designDb, -48.0, 48.0);
    }
    if (outDes) *outDes = designDb;
    return st;
}

void dumpCurve (double fs, double f0, double q, double gdb)
{
    double des = 0;
    auto st = make (fs, f0, q, gdb, &des);
    std::printf ("\nf0=%.0f Q=%.2f gain=%+.0f des=%+.1f\n", f0, q, gdb, des);
    // log-spaced samples like a graph
    for (double f = 20; f < fs * 0.49; f *= 1.25)
    {
        double db = 20 * std::log10 (std::max (1e-15, mag (st, f, fs)));
        std::printf ("  %8.1f Hz  %+7.2f dB\n", f, db);
    }
    // dense near Nyquist
    for (double f : { fs * 0.4, fs * 0.45, fs * 0.48, fs * 0.49, fs * 0.499 })
    {
        double db = 20 * std::log10 (std::max (1e-15, mag (st, f, fs)));
        std::printf ("  %8.1f Hz  %+7.2f dB  (edge)\n", f, db);
    }
}

int main()
{
    const double fs = 48000;
    // Mid stable
    dumpCurve (fs, 1000, 0.7, -12);
    // Low edge of graph
    dumpCurve (fs, 40, 0.7, -12);
    dumpCurve (fs, 80, 0.5, -12);
    // High edge
    dumpCurve (fs, 8000, 0.7, -12);
    dumpCurve (fs, 12000, 0.7, -12);
    dumpCurve (fs, 16000, 0.5, -12);

    // Sweep f0 and report OOB_lo, centre, OOB_hi, and max |dB| over log grid
    std::printf ("\n=== Sweep f0, report extremes ===\n");
    std::printf ("%7s %8s %8s %8s %8s %8s\n", "f0", "ctr", "oobLo", "oobHi", "minDb", "maxDb");
    for (double f0 = 30; f0 <= 18000; f0 *= 1.2)
    {
        auto st = make (fs, f0, 0.7, -12);
        double minDb = 0, maxDb = -999;
        for (double f = 20; f < fs * 0.49; f *= 1.08)
        {
            double db = 20 * std::log10 (std::max (1e-15, mag (st, f, fs)));
            minDb = std::min (minDb, db);
            maxDb = std::max (maxDb, db);
        }
        auto db = [&] (double f) { return 20 * std::log10 (std::max (1e-15, mag (st, f, fs))); };
        std::printf ("%7.0f %8.2f %8.2f %8.2f %8.2f %8.2f\n",
                     f0, db (f0), db (20), db (fs * 0.49), minDb, maxDb);
    }
    return 0;
}
