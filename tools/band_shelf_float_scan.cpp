// Compare double vs float32 SOS evaluation — find f0 collapse.
#include <cmath>
#include <complex>
#include <cstdio>
#include <vector>
#include <algorithm>
#include <cstdint>

using Complex = std::complex<double>;
static constexpr double kPi = 3.141592653589793238462643383279502884;

Complex stabilizePole (Complex z)
{
    const double m = std::abs (z);
    if (m >= 1.0) return z * (0.9995 / std::max (m, 1e-12));
    return z;
}

struct StageD { double b0, b1, b2, a0, a1, a2; };
struct StageF { float b0, b1, b2, a0, a1, a2; };

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

bool appendHS (std::vector<StageD>& st, double fs, double fc, double gdb, int order,
               int* failPair = nullptr)
{
    const double pw = prewarp (fc / fs);
    int pi = 0;
    for (auto [pole, zero] : designAnalog (order, gdb))
    {
        Complex p = hpRoot (pw, pole, true);
        Complex z = hpRoot (pw, zero, false);
        const double a2 = std::norm (p);
        const double a1 = -2 * p.real();
        if (! (a2 < 1.0) || ! (std::abs (a1) < 1.0 + a2 + 1e-9))
        {
            if (failPair) *failPair = pi;
            return false;
        }
        // Check float stability after cast
        float fa1 = (float) a1, fa2 = (float) a2;
        if (! (fa2 < 1.0f) || ! (std::abs (fa1) < 1.0f + fa2 + 1e-6f))
        {
            if (failPair) *failPair = 1000 + pi;
            // still add double
        }
        st.push_back ({ 1, -2 * z.real(), std::norm (z), 1, a1, a2 });
        ++pi;
    }
    return true;
}

double magD (const std::vector<StageD>& st, double f, double fs)
{
    const double w = 2 * kPi * f / fs;
    Complex zm1 (std::cos (-w), std::sin (-w)), zm2 = zm1 * zm1, H (1, 0);
    for (auto& s : st)
        H *= (s.b0 + s.b1 * zm1 + s.b2 * zm2) / (s.a0 + s.a1 * zm1 + s.a2 * zm2);
    return std::abs (H);
}

double magF (const std::vector<StageF>& st, double f, double fs)
{
    const double w = 2 * kPi * f / fs;
    Complex zm1 (std::cos (-w), std::sin (-w)), zm2 = zm1 * zm1, H (1, 0);
    for (auto& s : st)
        H *= ((double) s.b0 + (double) s.b1 * zm1 + (double) s.b2 * zm2)
           / ((double) s.a0 + (double) s.a1 * zm1 + (double) s.a2 * zm2);
    return std::abs (H);
}

std::vector<StageF> toFloat (const std::vector<StageD>& st)
{
    std::vector<StageF> out;
    for (auto& s : st)
        out.push_back ({ (float) s.b0, (float) s.b1, (float) s.b2,
                         (float) s.a0, (float) s.a1, (float) s.a2 });
    return out;
}

// Also test cascade of JUCE-style RBJ high shelves (S=1) in float
StageD rbjHS (double fs, double f, double gainDb, double S = 1.0)
{
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2 * kPi * f / fs;
    const double cs = std::cos (w0), sn = std::sin (w0);
    const double AL = sn / 2 * std::sqrt ((A + 1 / A) * (1 / S - 1) + 2);
    const double sq = 2 * std::sqrt (A) * AL;
    double b0 = A * ((A + 1) + (A - 1) * cs + sq);
    double b1 = -2 * A * ((A - 1) + (A + 1) * cs);
    double b2 = A * ((A + 1) + (A - 1) * cs - sq);
    double a0 = (A + 1) - (A - 1) * cs + sq;
    double a1 = 2 * ((A - 1) - (A + 1) * cs);
    double a2 = (A + 1) - (A - 1) * cs - sq;
    return { b0 / a0, b1 / a0, b2 / a0, 1, a1 / a0, a2 / a0 };
}

void scanOrder (int order, double fs, double gainDb, double q)
{
    const double bwOct = std::clamp (1.05 / std::pow (std::max (0.15, q), 0.55), 0.40, 3.2);
    std::printf ("\n=== order=%d gain=%+.0f Q=%.2f bw=%.2f ===\n", order, gainDb, q, bwOct);
    std::printf ("%7s %8s %8s %8s %8s %6s\n", "f0", "dblDb", "fltDb", "rbjDb", "desDb", "fail");

    for (double f0 = 40; f0 <= 2000; f0 *= 1.15)
    {
        double r = std::pow (2.0, 0.5 * bwOct);
        double fLo = f0 / r, fHi = f0 * r;
        double designDb = gainDb;
        std::vector<StageD> st;
        int fail = -1;
        for (int iter = 0; iter < 4; ++iter)
        {
            st.clear();
            int f1 = -1, f2 = -1;
            bool ok1 = appendHS (st, fs, fLo, designDb, order, &f1);
            bool ok2 = ok1 && appendHS (st, fs, fHi, -designDb, order, &f2);
            if (! ok1 || ! ok2) { fail = ok1 ? f2 : f1; break; }
            // OOB scale
            double oob = std::max (5.0, fLo * 0.15);
            double sc = 1.0 / magD (st, oob, fs);
            st[0].b0 *= sc; st[0].b1 *= sc; st[0].b2 *= sc;
            double ad = 20 * std::log10 (std::max (1e-15, magD (st, f0, fs)));
            if (std::abs (ad - gainDb) < 0.2) break;
            if (std::abs (ad) < 0.15) { designDb = std::clamp (gainDb * 2.0, -48.0, 48.0); continue; }
            designDb *= std::clamp (gainDb / ad, 0.5, 3.0);
            designDb = std::clamp (designDb, -48.0, 48.0);
        }

        double dblDb = st.empty() ? -999 : 20 * std::log10 (std::max (1e-15, magD (st, f0, fs)));
        double fltDb = -999;
        if (! st.empty())
        {
            auto ff = toFloat (st);
            fltDb = 20 * std::log10 (std::max (1e-15, magF (ff, f0, fs)));
        }

        // RBJ cascade 4+4
        std::vector<StageD> rbj;
        const int n = 4;
        for (int i = 0; i < n; ++i) rbj.push_back (rbjHS (fs, fLo, gainDb / n));
        for (int i = 0; i < n; ++i) rbj.push_back (rbjHS (fs, fHi, -gainDb / n));
        auto rbjF = toFloat (rbj);
        double rbjDb = 20 * std::log10 (std::max (1e-15, magF (rbjF, f0, fs)));

        std::printf ("%7.1f %8.2f %8.2f %8.2f %8.2f %6d\n",
                     f0, dblDb, fltDb, rbjDb, designDb, fail);
    }
}

// Fine scan around 200Hz
void fineScan (int order, double fs)
{
    std::printf ("\n=== FINE SCAN order=%d around 150-400 Hz, Q=0.7, -12dB ===\n", order);
    const double q = 0.7, gainDb = -12;
    const double bwOct = std::clamp (1.05 / std::pow (q, 0.55), 0.40, 3.2);
    for (double f0 = 150; f0 <= 400; f0 += 5)
    {
        double r = std::pow (2.0, 0.5 * bwOct);
        double fLo = f0 / r, fHi = f0 * r;
        std::vector<StageD> st;
        appendHS (st, fs, fLo, gainDb, order);
        appendHS (st, fs, fHi, -gainDb, order);
        double oob = std::max (5.0, fLo * 0.15);
        double sc = 1.0 / magD (st, oob, fs);
        st[0].b0 *= sc; st[0].b1 *= sc; st[0].b2 *= sc;
        auto ff = toFloat (st);
        // sample plateau flatness
        double c = 20 * std::log10 (std::max (1e-15, magF (ff, f0, fs)));
        double m1 = 20 * std::log10 (std::max (1e-15, magF (ff, f0 * 0.85, fs)));
        double m2 = 20 * std::log10 (std::max (1e-15, magF (ff, f0 * 1.15, fs)));
        double lo = 20 * std::log10 (std::max (1e-15, magF (ff, fLo * 0.2, fs)));
        // pole radii after float
        double maxA2 = 0;
        for (auto& s : ff) maxA2 = std::max (maxA2, (double) s.a2);
        std::printf ("f0=%6.1f ctr=%+6.2f mid=%+5.1f/%+5.1f oob=%+5.1f max|p|^2=%.6f stages=%d\n",
                     f0, c, m1, m2, lo, maxA2, (int) st.size());
    }
}

int main()
{
    scanOrder (8, 48000, -12, 0.7);
    scanOrder (4, 48000, -12, 0.7);
    scanOrder (2, 48000, -12, 0.7);
    fineScan (8, 48000);
    fineScan (4, 48000);

    // Check pole magnitudes for HS at various cutoffs with large design gain
    std::printf ("\n=== Pole |p| after HP map, order=8 gain=-24, various fc ===\n");
    for (double fc : { 50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0 })
    {
        double pw = prewarp (fc / 48000.0);
        double maxP = 0, minP = 1e9;
        int nOut = 0;
        for (auto [pole, zero] : designAnalog (8, -24))
        {
            Complex p = hpRoot (pw, pole, true);
            double m = std::abs (p);
            maxP = std::max (maxP, m);
            minP = std::min (minP, m);
            if (m >= 1.0) ++nOut;
        }
        std::printf ("fc=%7.0f prewarp=%.4g  |p| [%.6f .. %.6f] nOut=%d\n",
                     fc, pw, minP, maxP, nOut);
    }
    return 0;
}
