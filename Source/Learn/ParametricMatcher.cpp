#include "ParametricMatcher.h"
#include "BiquadMagnitude.h"
#include "../FilterType.h"
#include <cmath>
#include <vector>

namespace EqLearn
{

namespace
{
    /**
        Musical Learn bake (Pro-Q Match / AutoEq style projection):
        - Project residual onto fixed octave centres (not one global wide peak)
        - Musical Q defaults (≈0.9–1.6), not lobe-width → near-zero Q
        - Low/high shelf when tilt is broadband
        - Residual update after each band so multiple regions can fire
    */

    struct OctaveBand
    {
        float centreHz;
        float halfWidthOct; // ± this many octaves for mean residual
        float defaultQ;
        bool allowShelfPromote; // if very wide / strong, may become shelf
    };

    // Low → high so bass/mud get first budget (mix habits)
    static constexpr OctaveBand kOctaves[] = {
        {   70.0f, 0.55f, 0.95f, true  }, // fundamental / sub body
        {  150.0f, 0.45f, 1.05f, true  }, // low body
        {  280.0f, 0.40f, 1.15f, false }, // mud
        {  500.0f, 0.40f, 1.20f, false }, // box / low mid
        {  900.0f, 0.40f, 1.20f, false }, // low mids
        { 1600.0f, 0.40f, 1.25f, false }, // mids
        { 2800.0f, 0.40f, 1.30f, false }, // presence / harsh
        { 4500.0f, 0.40f, 1.25f, false }, // upper presence
        { 7000.0f, 0.45f, 1.10f, true  }, // air
    };

    float residualMae (const float* residual, const float* gridHz, int n) noexcept
    {
        if (residual == nullptr || n <= 0)
            return 0.0f;
        double sum = 0.0;
        int c = 0;
        for (int i = 0; i < n; ++i)
        {
            const float f = (gridHz != nullptr) ? gridHz[i] : 1000.0f;
            if (f < kShapeMatchMinHz || f > kShapeMatchMaxHz)
                continue;
            const float a = std::abs (residual[i]);
            if (a < 0.05f)
                continue;
            // Prefer scoring low/mid residual so we keep placing there
            const float w = (f < 500.0f) ? 1.4f : (f < 2000.0f ? 1.2f : 0.9f);
            sum += (double) (a * w);
            ++c;
        }
        return c > 0 ? (float) (sum / (double) c) : 0.0f;
    }

    void subtractBand (float* residual, int n,
                       int type, float fc, float g, float q,
                       const float* gridHz) noexcept
    {
        std::vector<float> h ((size_t) n);
        BiquadMagnitude::evaluateBandDb (type, fc, g, q, gridHz, h.data(), n);
        for (int i = 0; i < n; ++i)
            residual[i] -= h[(size_t) i];
    }

    float meanInOctave (const float* residual, const float* gridHz, int n,
                        float centreHz, float halfWidthOct) noexcept
    {
        const float lo = centreHz * std::pow (2.0f, -halfWidthOct);
        const float hi = centreHz * std::pow (2.0f,  halfWidthOct);
        double sum = 0.0, wsum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const float f = gridHz[i];
            if (f < lo || f > hi)
                continue;
            const float dOct = std::abs (std::log2 (f / centreHz));
            const float w = juce::jmax (0.0f, 1.0f - dOct / juce::jmax (0.05f, halfWidthOct));
            sum += (double) residual[i] * (double) w;
            wsum += (double) w;
        }
        return wsum > 1.0e-6 ? (float) (sum / wsum) : 0.0f;
    }

    float regionMean (const float* residual, const float* gridHz, int n,
                      float fLo, float fHi) noexcept
    {
        double sum = 0.0;
        int c = 0;
        for (int i = 0; i < n; ++i)
        {
            if (gridHz[i] >= fLo && gridHz[i] <= fHi)
            {
                sum += (double) residual[i];
                ++c;
            }
        }
        return c > 0 ? (float) (sum / (double) c) : 0.0f;
    }

    float residualSupportMaxHz (const float* residual, const float* gridHz, int n) noexcept
    {
        float maxF = 200.0f;
        for (int i = 0; i < n; ++i)
            if (std::abs (residual[i]) >= 0.15f)
                maxF = juce::jmax (maxF, gridHz[i]);
        return maxF;
    }

    bool tooClose (const juce::Array<BandProposal>& bands, float hz, float minOct) noexcept
    {
        for (const auto& b : bands)
        {
            const float oct = std::abs (std::log2 (juce::jmax (1.0f, hz)
                                                   / juce::jmax (1.0f, b.frequencyHz)));
            if (oct < minOct)
                return true;
        }
        return false;
    }

    /** Musical peaking Q — never go hairline or "one giant blob". */
    float musicalPeakQ (float halfWidthOct) noexcept
    {
        // Narrower window → higher Q; floor at 0.85 so we don't ship ultra-wide bells
        const float q = 0.85f / juce::jmax (0.28f, halfWidthOct * 0.75f);
        return juce::jlimit (0.85f, 2.4f, q);
    }

    /** Fine-tune gain only (Fc/Q stay musical). */
    void refineGainOnly (BandProposal& p,
                         const float* residual, const float* gridHz, int n,
                         float maxGainDb)
    {
        auto lossWith = [&] (float g) -> float
        {
            std::vector<float> trial ((size_t) n);
            for (int i = 0; i < n; ++i)
                trial[(size_t) i] = residual[i];
            subtractBand (trial.data(), n, p.filterType, p.frequencyHz, g, p.q, gridHz);
            return residualMae (trial.data(), gridHz, n);
        };

        float bestG = p.gainDb;
        float bestL = lossWith (bestG);
        const float candidates[] = {
            bestG * 0.5f, bestG * 0.75f, bestG, bestG * 1.25f,
            bestG + 1.5f, bestG - 1.5f, bestG + 3.0f, bestG - 3.0f
        };
        for (float g : candidates)
        {
            g = juce::jlimit (-maxGainDb, maxGainDb, g);
            if (std::abs (g) < kMinProposalGainDb * 0.4f && std::abs (bestG) >= kMinProposalGainDb)
                continue;
            const float L = lossWith (g);
            if (L + 1.0e-5f < bestL)
            {
                bestL = L;
                bestG = g;
            }
        }
        p.gainDb = juce::jlimit (-maxGainDb, maxGainDb, bestG);
    }

    bool tryLowShelf (const float* residual, const float* gridHz, int n,
                      float maxGainDb, BandProposal& out)
    {
        const float lo = regionMean (residual, gridHz, n, 40.0f, 180.0f);
        const float mid = regionMean (residual, gridHz, n, 400.0f, 1500.0f);
        const float delta = lo - mid * 0.15f;
        if (std::abs (delta) < kShelfMinTiltDb)
            return false;

        out = {};
        out.isShelf = true;
        out.filterType = FilterType::lowShelf;
        out.q = kShelfQFixed;
        // Fc: where low energy transition sits
        out.frequencyHz = juce::jlimit (kLowShelfFcMin, kLowShelfFcMax,
                                        std::abs (lo) > std::abs (mid) ? 110.0f : 160.0f);
        out.gainDb = juce::jlimit (-maxGainDb, maxGainDb, delta);
        if (std::abs (out.gainDb) < kMinProposalGainDb)
            return false;
        refineGainOnly (out, residual, gridHz, n, maxGainDb);
        return std::abs (out.gainDb) >= kMinProposalGainDb * 0.75f;
    }

    bool tryHighShelf (const float* residual, const float* gridHz, int n,
                       float maxGainDb, float supportMaxHz, BandProposal& out)
    {
        if (supportMaxHz < 3500.0f)
            return false;

        const float mid = regionMean (residual, gridHz, n, 400.0f, 1500.0f);
        const float hi = regionMean (residual, gridHz, n, 5000.0f, juce::jmin (14000.0f, supportMaxHz));
        const float delta = hi - mid * 0.15f;
        if (std::abs (delta) < kShelfMinTiltDb)
            return false;

        out = {};
        out.isShelf = true;
        out.filterType = FilterType::highShelf;
        out.q = kShelfQFixed;
        out.frequencyHz = juce::jlimit (kHighShelfFcMin, kHighShelfFcMax, 6500.0f);
        out.gainDb = juce::jlimit (-maxGainDb, maxGainDb, delta);
        if (std::abs (out.gainDb) < kMinProposalGainDb)
            return false;
        refineGainOnly (out, residual, gridHz, n, maxGainDb);
        return std::abs (out.gainDb) >= kMinProposalGainDb * 0.75f;
    }
}

ParametricMatcher::FitResult ParametricMatcher::match (const float* correctionDb,
                                                       const float* gridHz,
                                                       int n,
                                                       const Settings& settings)
{
    FitResult result;
    if (correctionDb == nullptr || gridHz == nullptr || n < 8)
        return result;

    const int maxBands = juce::jlimit (kMinMaxBands, kMaxMaxBands, settings.maxBands);
    const float maxG = juce::jmax (1.0f, settings.maxGainDb);

    std::vector<float> residual ((size_t) n);
    std::vector<float> targetC ((size_t) n);
    for (int i = 0; i < n; ++i)
    {
        residual[(size_t) i] = correctionDb[i];
        targetC[(size_t) i] = correctionDb[i];
    }

    // How much residual energy exists at all?
    float peakAbs = 0.0f;
    for (int i = 0; i < n; ++i)
        peakAbs = juce::jmax (peakAbs, std::abs (residual[(size_t) i]));

    if (peakAbs < kForceBandMinAbsDb * 0.6f)
        return result; // genuinely flat vs target

    int iters = 0;
    bool usedLS = false, usedHS = false;
    const float supportMax = residualSupportMaxHz (residual.data(), gridHz, n);

    // --- 1) Broadband low tilt → low shelf (not a giant mid bell) ---
    if (maxBands >= 1)
    {
        BandProposal ls;
        if (tryLowShelf (residual.data(), gridHz, n, maxG, ls))
        {
            subtractBand (residual.data(), n, ls.filterType, ls.frequencyHz, ls.gainDb, ls.q, gridHz);
            result.bands.add (ls);
            usedLS = true;
            ++iters;
        }
    }

    // --- 2) Octave centres: one band per region with musical Q ---
    //     Gain = local mean residual (can be cut OR boost). Sorted by |gain|.
    struct Candidate
    {
        float centreHz;
        float gainDb;
        float q;
        float score; // |gain| * importance
    };

    juce::Array<Candidate> cands;
    for (const auto& oct : kOctaves)
    {
        if (oct.centreHz > supportMax * 1.25f)
            continue;

        const float g = meanInOctave (residual.data(), gridHz, n, oct.centreHz, oct.halfWidthOct);
        if (std::abs (g) < kRegionMinAbsDb)
            continue;

        // Importance: prefer lows/mids slightly so we don't only grab one mid lobe
        float imp = 1.0f;
        if (oct.centreHz < 200.0f)       imp = 1.35f;
        else if (oct.centreHz < 600.0f)  imp = 1.25f;
        else if (oct.centreHz < 2000.0f) imp = 1.15f;
        else if (oct.centreHz > 5000.0f) imp = 0.85f;

        Candidate c;
        c.centreHz = oct.centreHz;
        c.gainDb = juce::jlimit (-maxG, maxG, g);
        c.q = musicalPeakQ (oct.halfWidthOct);
        // Use authored default as blend toward musical
        c.q = 0.5f * c.q + 0.5f * oct.defaultQ;
        c.q = juce::jlimit (0.85f, 2.4f, c.q);
        c.score = std::abs (c.gainDb) * imp;
        cands.add (c);
    }

    struct ByScore
    {
        static int compareElements (const Candidate& a, const Candidate& b) noexcept
        {
            if (a.score > b.score) return -1;
            if (a.score < b.score) return 1;
            return 0;
        }
    };
    ByScore cmp;
    cands.sort (cmp);

    for (int ci = 0; ci < cands.size(); ++ci)
    {
        if ((int) result.bands.size() >= maxBands)
            break;

        const auto& c = cands.getReference (ci);

        // Re-measure residual after previous placements (stale candidate gains)
        const float gNow = meanInOctave (residual.data(), gridHz, n, c.centreHz, 0.4f);
        if (std::abs (gNow) < kRegionMinAbsDb * 0.75f)
            continue;

        if (tooClose (result.bands, c.centreHz, kMinPeakSpacingOct * 0.9f))
            continue;

        // Skip low octaves if LS already owns that tilt
        if (usedLS && c.centreHz < 120.0f && std::abs (gNow) < 1.5f)
            continue;

        BandProposal p;
        p.isShelf = false;
        p.filterType = FilterType::bell;
        p.frequencyHz = juce::jlimit (kMinFreqHz, kMaxFreqHz, c.centreHz);
        p.gainDb = juce::jlimit (-maxG, maxG, gNow);
        p.q = c.q;

        // Promote strong low broadband to low shelf if we didn't already
        if (! usedLS && c.centreHz <= 160.0f && std::abs (gNow) >= 1.8f
            && (int) result.bands.size() < maxBands)
        {
            p.isShelf = true;
            p.filterType = FilterType::lowShelf;
            p.q = kShelfQFixed;
            p.frequencyHz = juce::jlimit (kLowShelfFcMin, kLowShelfFcMax, 120.0f);
            usedLS = true;
        }

        refineGainOnly (p, residual.data(), gridHz, n, maxG);
        if (std::abs (p.gainDb) < kMinProposalGainDb * 0.7f)
            continue;

        // Don't allow optimise to turn Q into a blob — reassert musical Q for bells
        if (! p.isShelf)
            p.q = juce::jlimit (0.85f, 2.4f, p.q);

        subtractBand (residual.data(), n, p.filterType, p.frequencyHz, p.gainDb, p.q, gridHz);
        result.bands.add (p);
        ++iters;
    }

    // --- 3) High shelf if residual still has top-end tilt ---
    if ((int) result.bands.size() < maxBands && ! usedHS)
    {
        BandProposal hs;
        if (tryHighShelf (residual.data(), gridHz, n, maxG, supportMax, hs))
        {
            if (! tooClose (result.bands, hs.frequencyHz, 0.7f))
            {
                subtractBand (residual.data(), n, hs.filterType, hs.frequencyHz, hs.gainDb, hs.q, gridHz);
                result.bands.add (hs);
                usedHS = true;
                ++iters;
            }
        }
    }

    // --- 4) Second pass: any remaining strong residual peaks by octave ---
    if ((int) result.bands.size() < maxBands)
    {
        for (const auto& oct : kOctaves)
        {
            if ((int) result.bands.size() >= maxBands)
                break;
            if (oct.centreHz > supportMax * 1.2f)
                continue;

            const float g = meanInOctave (residual.data(), gridHz, n, oct.centreHz, oct.halfWidthOct);
            if (std::abs (g) < kRegionMinAbsDb * 1.1f)
                continue;
            if (tooClose (result.bands, oct.centreHz, kMinPeakSpacingOct))
                continue;

            BandProposal p;
            p.filterType = FilterType::bell;
            p.isShelf = false;
            p.frequencyHz = oct.centreHz;
            p.gainDb = juce::jlimit (-maxG, maxG, g);
            p.q = juce::jlimit (0.85f, 2.4f, oct.defaultQ);
            refineGainOnly (p, residual.data(), gridHz, n, maxG);
            if (std::abs (p.gainDb) < kMinProposalGainDb)
                continue;

            subtractBand (residual.data(), n, p.filterType, p.frequencyHz, p.gainDb, p.q, gridHz);
            result.bands.add (p);
            ++iters;
        }
    }

    // --- 5) Absolute last resort: one musical peak (not ultra-wide) ---
    if (result.bands.isEmpty() && peakAbs >= kForceBandMinAbsDb)
    {
        int best = n / 3;
        float bestA = 0.0f;
        for (int i = 2; i < n - 2; ++i)
        {
            if (gridHz[i] < 50.0f || gridHz[i] > supportMax)
                continue;
            const float a = std::abs (residual[(size_t) i]);
            if (a > bestA)
            {
                bestA = a;
                best = i;
            }
        }
        BandProposal p;
        p.filterType = FilterType::bell;
        p.isShelf = false;
        p.frequencyHz = gridHz[best];
        p.gainDb = juce::jlimit (-maxG, maxG, residual[(size_t) best]);
        p.q = 1.1f; // musical default — never compute blob Q here
        if (std::abs (p.gainDb) >= kForceBandMinAbsDb * 0.8f)
        {
            result.bands.add (p);
            ++iters;
        }
    }

    // Final clamps
    for (auto& p : result.bands)
    {
        p.gainDb = juce::jlimit (-maxG, maxG, p.gainDb);
        p.frequencyHz = juce::jlimit (kMinFreqHz, kMaxFreqHz, p.frequencyHz);
        if (p.isShelf)
            p.q = kShelfQFixed;
        else
            p.q = juce::jlimit (0.85f, 2.5f, p.q);
    }

    // Drop near-zero extras only
    if (result.bands.size() > 1)
    {
        for (int i = result.bands.size(); --i >= 0;)
            if (std::abs (result.bands.getReference (i).gainDb) < kMinProposalGainDb * 0.55f)
                result.bands.remove (i);
    }

    std::vector<float> hCascade ((size_t) n, 0.0f);
    if (result.bands.size() > 0)
    {
        std::vector<BandProposal> flat ((size_t) result.bands.size());
        for (int i = 0; i < result.bands.size(); ++i)
            flat[(size_t) i] = result.bands.getReference (i);
        BiquadMagnitude::evaluateCascadeDb (flat.data(), (int) flat.size(),
                                            gridHz, hCascade.data(), n);
    }

    result.maeDb = BiquadMagnitude::meanAbsError (hCascade.data(), targetC.data(), n);
    result.residualMaeDb = residualMae (residual.data(), gridHz, n);
    result.iterations = iters;
    juce::ignoreUnused (usedLS, usedHS);

    return result;
}

} // namespace EqLearn
