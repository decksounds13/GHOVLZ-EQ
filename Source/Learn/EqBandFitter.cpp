#include "EqBandFitter.h"
#include "ParametricMatcher.h"
#include <cmath>
#include <vector>

namespace EqLearn
{

namespace
{
    float interpLogDb (const float* hz, const float* db, int n, float f) noexcept
    {
        if (hz == nullptr || db == nullptr || n <= 0)
            return 0.0f;

        f = juce::jmax (1.0f, f);
        if (n == 1 || f <= hz[0])
            return db[0];
        if (f >= hz[n - 1])
            return db[n - 1];

        int k = 0;
        while (k + 1 < n && hz[k + 1] < f)
            ++k;

        const float f0 = juce::jmax (1.0f, hz[k]);
        const float f1 = juce::jmax (f0 * 1.0001f, hz[k + 1]);
        const float t = (std::log (f) - std::log (f0))
                        / juce::jmax (1.0e-6f, std::log (f1) - std::log (f0));
        return db[k] + (db[k + 1] - db[k]) * juce::jlimit (0.0f, 1.0f, t);
    }

    /** 90% cumulative linear energy frequency of absolute spectrum. */
    float energyRolloffHz (const float* absDb, const float* gridHz, int n,
                           float peakDb) noexcept
    {
        const float floorDb = peakDb - kCorrectionActiveBelowPeakDb;
        double total = 0.0;
        std::vector<double> power ((size_t) n, 0.0);
        for (int i = 0; i < n; ++i)
        {
            if (gridHz[i] < kShapeMatchMinHz || gridHz[i] > kShapeMatchMaxHz)
                continue;
            if (absDb[i] < floorDb)
                continue;
            const float rel = absDb[i] - peakDb;
            const double p = (double) juce::Decibels::decibelsToGain (rel, -90.0f);
            power[(size_t) i] = juce::jmax (0.0, p);
            total += power[(size_t) i];
        }
        if (total < 1.0e-12)
            return kMinCorrectBandwidthHz;

        const double target = total * 0.90;
        double acc = 0.0;
        float lastF = 500.0f;
        for (int i = 0; i < n; ++i)
        {
            if (power[(size_t) i] <= 0.0)
                continue;
            acc += power[(size_t) i];
            lastF = gridHz[i];
            if (acc >= target)
                return juce::jmax (150.0f, lastF);
        }
        return juce::jmax (150.0f, lastF);
    }

    /** Soft activity 0..1 from absolute level vs peak (not a hard kill). */
    float activityWeight (float absDb, float peakDb) noexcept
    {
        const float lo = peakDb - kCorrectionActiveBelowPeakDb;       // weight → 0
        const float hi = peakDb - kCorrectionActiveBelowPeakDb * 0.45f; // weight → 1
        if (absDb <= lo)
            return 0.0f;
        if (absDb >= hi)
            return 1.0f;
        return (absDb - lo) / juce::jmax (1.0f, hi - lo);
    }

    void levelAlignWeighted (float* sDb, float* tDb, const float* weight, int n) noexcept
    {
        double sumS = 0.0, sumT = 0.0, wsum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const double w = (weight != nullptr) ? (double) weight[i] : 1.0;
            if (w < 1.0e-6)
                continue;
            sumS += (double) sDb[i] * w;
            sumT += (double) tDb[i] * w;
            wsum += w;
        }
        if (wsum < 1.0e-6)
            return;

        const float meanS = (float) (sumS / wsum);
        const float meanT = (float) (sumT / wsum);
        for (int i = 0; i < n; ++i)
        {
            sDb[i] -= meanS;
            tDb[i] -= meanT;
        }
    }
}

void BandFitter::makeFactoryTargetDb (Target target,
                                      const float* frequenciesHz,
                                      float* destDb,
                                      int numPoints) noexcept
{
    if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
        return;

    const float slope = (target == Target::flat) ? 0.0f : -3.0f;
    for (int i = 0; i < numPoints; ++i)
    {
        const float f = juce::jmax (1.0f, frequenciesHz[i]);
        destDb[i] = slope * std::log2 (f / kRefHz);
    }
    meanNormalize (destDb, numPoints);
}

void BandFitter::resampleLog (const float* srcDb, const float* srcHz, int srcN,
                              const float* gridHz, float* destDb, int gridN) noexcept
{
    if (destDb == nullptr || gridHz == nullptr || gridN <= 0)
        return;

    if (srcDb == nullptr || srcHz == nullptr || srcN <= 0)
    {
        juce::FloatVectorOperations::clear (destDb, gridN);
        return;
    }

    for (int i = 0; i < gridN; ++i)
        destDb[i] = interpLogDb (srcHz, srcDb, srcN, gridHz[i]);
}

void BandFitter::meanNormalize (float* db, int n) noexcept
{
    if (db == nullptr || n <= 0)
        return;

    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += (double) db[i];
    const float mean = (float) (sum / (double) n);
    for (int i = 0; i < n; ++i)
        db[i] -= mean;
}

void BandFitter::smoothOctaves (const float* gridHz, float* db, int n, float octaves) noexcept
{
    if (gridHz == nullptr || db == nullptr || n <= 2 || octaves <= 0.01f)
        return;

    std::vector<float> tmp ((size_t) n);
    const float halfOct = octaves * 0.5f;

    for (int i = 0; i < n; ++i)
    {
        const float f0 = juce::jmax (1.0f, gridHz[i]);
        const float lo = f0 * std::pow (2.0f, -halfOct);
        const float hi = f0 * std::pow (2.0f, halfOct);
        double sum = 0.0;
        double wsum = 0.0;
        for (int j = 0; j < n; ++j)
        {
            const float f = gridHz[j];
            if (f < lo || f > hi)
                continue;
            const float dOct = std::abs (std::log2 (juce::jmax (1.0f, f) / f0));
            const float w = juce::jmax (0.0f, 1.0f - dOct / juce::jmax (0.05f, halfOct));
            sum += (double) db[j] * (double) w;
            wsum += (double) w;
        }
        tmp[(size_t) i] = (wsum > 1.0e-9) ? (float) (sum / wsum) : db[i];
    }

    for (int i = 0; i < n; ++i)
        db[i] = tmp[(size_t) i];
}

void BandFitter::fit (const float* sourceDb,
                      const float* sourceHz,
                      int sourceN,
                      const float* targetDb,
                      const float* targetHz,
                      int targetN,
                      const Settings& settings,
                      juce::Array<BandProposal>& out,
                      float* outMaeDb,
                      float* outResidualMaeDb)
{
    out.clearQuick();
    if (outMaeDb != nullptr)
        *outMaeDb = 0.0f;
    if (outResidualMaeDb != nullptr)
        *outResidualMaeDb = 0.0f;

    if (sourceDb == nullptr || sourceHz == nullptr || sourceN < 8)
        return;

    const int gridN = kFitGridPoints;
    std::vector<float> gridHz ((size_t) gridN);
    std::vector<float> sAbs ((size_t) gridN);
    std::vector<float> sDb ((size_t) gridN);
    std::vector<float> tDb ((size_t) gridN);
    std::vector<float> cDb ((size_t) gridN);
    std::vector<float> weight ((size_t) gridN, 0.0f);

    const double logMin = std::log ((double) kMinFreqHz);
    const double logMax = std::log ((double) kMaxFreqHz);
    for (int i = 0; i < gridN; ++i)
    {
        const double t = (gridN == 1) ? 0.5 : (double) i / (double) (gridN - 1);
        gridHz[(size_t) i] = (float) std::exp (logMin + t * (logMax - logMin));
    }

    resampleLog (sourceDb, sourceHz, sourceN, gridHz.data(), sAbs.data(), gridN);
    for (int i = 0; i < gridN; ++i)
        sDb[(size_t) i] = sAbs[(size_t) i];

    float peakDb = -200.0f;
    for (int i = 0; i < gridN; ++i)
        if (gridHz[(size_t) i] >= kShapeMatchMinHz && gridHz[(size_t) i] <= kShapeMatchMaxHz)
            peakDb = juce::jmax (peakDb, sAbs[(size_t) i]);

    if (peakDb < -100.0f)
        return;

    for (int i = 0; i < gridN; ++i)
    {
        const float f = gridHz[(size_t) i];
        if (f < kShapeMatchMinHz || f > kShapeMatchMaxHz)
            weight[(size_t) i] = 0.0f;
        else
            weight[(size_t) i] = activityWeight (sAbs[(size_t) i], peakDb);
    }

    // Ensure some weight exists (very quiet / display-compressed spectra)
    {
        double wsum = 0.0;
        for (int i = 0; i < gridN; ++i)
            wsum += (double) weight[(size_t) i];
        if (wsum < 1.0)
        {
            for (int i = 0; i < gridN; ++i)
            {
                const float f = gridHz[(size_t) i];
                if (f >= kShapeMatchMinHz && f <= kShapeMatchMaxHz)
                    weight[(size_t) i] = juce::jmax (weight[(size_t) i], 0.35f);
            }
        }
    }

    const float rolloffHz = energyRolloffHz (sAbs.data(), gridHz.data(), gridN, peakDb);
    // Always allow correction through at least midrange; expand with source bandwidth
    const float maxCorrectHz = juce::jmin (kShapeMatchMaxHz,
                                           juce::jmax (kMinCorrectBandwidthHz,
                                                       rolloffHz * kActiveBandwidthMargin));

    if (targetDb != nullptr && targetHz != nullptr && targetN > 0)
        resampleLog (targetDb, targetHz, targetN, gridHz.data(), tDb.data(), gridN);
    else
    {
        const float slope = (settings.target == Target::flat) ? 0.0f : -3.0f;
        for (int i = 0; i < gridN; ++i)
        {
            const float f = juce::jmax (1.0f, gridHz[(size_t) i]);
            tDb[(size_t) i] = slope * std::log2 (f / kRefHz);
        }
    }

    levelAlignWeighted (sDb.data(), tDb.data(), weight.data(), gridN);

    const float strength = juce::jlimit (kMinStrength, kMaxStrength, settings.strength);
    const float maxG = juce::jmax (1.0f, settings.maxGainDb);

    for (int i = 0; i < gridN; ++i)
    {
        const float f = gridHz[(size_t) i];
        const float w = weight[(size_t) i];

        if (w < 0.02f || f < kShapeMatchMinHz || f > kShapeMatchMaxHz)
        {
            cDb[(size_t) i] = 0.0f;
            continue;
        }

        float e = (tDb[(size_t) i] - sDb[(size_t) i]) * strength * w;

        // Soft HF taper above instrument bandwidth (not a brick wall)
        if (f > maxCorrectHz * 0.55f)
        {
            const float t = (f - maxCorrectHz * 0.55f)
                            / juce::jmax (1.0f, maxCorrectHz * 0.45f);
            e *= juce::jmax (0.0f, 1.0f - t * t);
        }
        if (f > maxCorrectHz)
            e = 0.0f;

        cDb[(size_t) i] = juce::jlimit (-maxG, maxG, e);
    }

    smoothOctaves (gridHz.data(), cDb.data(), gridN, 0.20f);

    // Re-apply soft HF limit after smooth (smooth can smear into silence)
    for (int i = 0; i < gridN; ++i)
    {
        const float f = gridHz[(size_t) i];
        if (f > maxCorrectHz || weight[(size_t) i] < 0.02f)
            cDb[(size_t) i] = 0.0f;
        else
            cDb[(size_t) i] *= weight[(size_t) i];
    }

    // Mild HF-noise guard: only kill pure HF residual if lows are truly empty
    {
        float lowMid = 0.0f, hi = 0.0f;
        int nLo = 0, nHi = 0;
        for (int i = 0; i < gridN; ++i)
        {
            const float f = gridHz[(size_t) i];
            const float a = std::abs (cDb[(size_t) i]);
            if (f < 2000.0f) { lowMid += a; ++nLo; }
            else if (f <= maxCorrectHz) { hi += a; ++nHi; }
        }
        const float mLo = nLo > 0 ? lowMid / (float) nLo : 0.0f;
        const float mHi = nHi > 0 ? hi / (float) nHi : 0.0f;
        if (mHi > 2.5f * mLo + 1.5f && mLo < 0.35f)
        {
            for (int i = 0; i < gridN; ++i)
                if (gridHz[(size_t) i] >= 3000.0f)
                    cDb[(size_t) i] *= 0.15f; // attenuate, don't hard-zero all residual
        }
    }

    auto fit = ParametricMatcher::match (cDb.data(), gridHz.data(), gridN, settings);
    out = std::move (fit.bands);

    if (outMaeDb != nullptr)
        *outMaeDb = fit.maeDb;
    if (outResidualMaeDb != nullptr)
        *outResidualMaeDb = fit.residualMaeDb;
}

} // namespace EqLearn
