#include "EqSourceClassifier.h"
#include <cmath>

namespace EqLearn
{

namespace
{
    float linearPowerFromDb (float db) noexcept
    {
        const float clamped = juce::jmax (-100.0f, db);
        return std::pow (10.0f, clamped * 0.1f);
    }
}

SpectralFeatures SourceClassifier::extractFeatures (const float* spectrumDb,
                                                    const float* frequenciesHz,
                                                    int numBins,
                                                    float crestDb,
                                                    float transientRatio,
                                                    bool hasTransientStats) noexcept
{
    SpectralFeatures f;
    f.crestDb = juce::jlimit (0.0f, 40.0f, crestDb);
    f.transientRatio = juce::jlimit (0.0f, 1.0f, transientRatio);
    f.hasTransientStats = hasTransientStats;

    if (spectrumDb == nullptr || frequenciesHz == nullptr || numBins < 4)
        return f;

    double sumP = 0.0;
    double sumF = 0.0;
    double sumLog = 0.0;
    double meanDb = 0.0;
    int used = 0;

    double bandP[6] = { 0, 0, 0, 0, 0, 0 };

    for (int i = 0; i < numBins; ++i)
    {
        const float hz = frequenciesHz[i];
        if (hz < kMinFreqHz || hz > kMaxFreqHz)
            continue;

        const float db = spectrumDb[i];
        const double p = (double) linearPowerFromDb (db);
        if (p <= 0.0)
            continue;

        sumP += p;
        sumF += p * (double) hz;
        sumLog += std::log (p + 1.0e-20);
        meanDb += (double) db;
        ++used;

        if (hz < 80.0f)        bandP[0] += p;
        else if (hz < 250.0f)  bandP[1] += p;
        else if (hz < 500.0f)  bandP[2] += p;
        else if (hz < 2000.0f) bandP[3] += p;
        else if (hz < 6000.0f) bandP[4] += p;
        else                   bandP[5] += p;
    }

    if (used <= 0 || sumP <= 1.0e-30)
        return f;

    f.meanDb = (float) (meanDb / (double) used);
    f.centroidHz = (float) (sumF / sumP);

    const double geo = std::exp (sumLog / (double) used);
    const double arith = sumP / (double) used;
    f.flatness = (float) juce::jlimit (0.0, 1.0, geo / juce::jmax (1.0e-30, arith));

    const double target = 0.85 * sumP;
    double cum = 0.0;
    f.rolloffHz = kMaxFreqHz;
    for (int i = 0; i < numBins; ++i)
    {
        const float hz = frequenciesHz[i];
        if (hz < kMinFreqHz)
            continue;
        cum += (double) linearPowerFromDb (spectrumDb[i]);
        if (cum >= target)
        {
            f.rolloffHz = hz;
            break;
        }
    }

    const double inv = 1.0 / sumP;
    f.energySub = (float) (bandP[0] * inv);
    f.energyBass = (float) (bandP[1] * inv);
    f.energyLowMid = (float) (bandP[2] * inv);
    f.energyMid = (float) (bandP[3] * inv);
    f.energyPresence = (float) (bandP[4] * inv);
    f.energyAir = (float) (bandP[5] * inv);

    return f;
}

Classification SourceClassifier::classify (const SpectralFeatures& feat,
                                           float minConfidence) noexcept
{
    Classification c;
    c.features = feat;
    c.scores.fill (0.0f);

    const float subBass = feat.energySub + feat.energyBass;
    const float midHi = feat.energyMid + feat.energyPresence;
    const float top = feat.energyPresence + feat.energyAir;
    const float t = feat.transientRatio; // 0 sustain .. 1 transient
    const bool useTs = feat.hasTransientStats;

    auto& sc = c.scores;

    // Bass: sub+bass, low centroid, low air; prefers sustain (held notes)
    {
        float s = 0.0f;
        s += juce::jlimit (0.0f, 1.2f, subBass / 0.45f);
        s += juce::jlimit (0.0f, 1.0f, (1.0f - juce::jlimit (0.0f, 1.0f, feat.centroidHz / 800.0f)));
        s += juce::jlimit (0.0f, 0.8f, 1.0f - feat.energyAir / 0.12f);
        s += juce::jlimit (0.0f, 0.5f, 1.0f - feat.flatness);
        if (useTs)
            s += juce::jlimit (0.0f, 0.65f, 1.0f - t / 0.55f); // sustain-heavy
        sc[(size_t) SourceClass::bass] = s;
    }

    // Vocals: mid-presence; moderate transient (consonants ~0.25-0.4)
    {
        float s = 0.0f;
        s += juce::jlimit (0.0f, 1.2f, midHi / 0.40f);
        s += juce::jlimit (0.0f, 1.0f, feat.energyPresence / 0.22f);
        s += juce::jlimit (0.0f, 0.8f, 1.0f - subBass / 0.35f);
        s += (feat.centroidHz > 400.0f && feat.centroidHz < 3500.0f) ? 0.7f : 0.1f;
        s += juce::jlimit (0.0f, 0.4f, 1.0f - std::abs (feat.flatness - 0.25f) * 2.0f);
        if (useTs)
            s += juce::jlimit (0.0f, 0.55f, 1.0f - std::abs (t - 0.32f) * 3.0f);
        sc[(size_t) SourceClass::vocals] = s;
    }

    // Guitar: mid-forward; pick attacks -> moderate-high T
    {
        float s = 0.0f;
        s += juce::jlimit (0.0f, 1.1f, feat.energyMid / 0.28f);
        s += juce::jlimit (0.0f, 0.9f, feat.energyPresence / 0.25f);
        s += juce::jlimit (0.0f, 0.7f, 1.0f - feat.energySub / 0.15f);
        s += (feat.centroidHz > 500.0f && feat.centroidHz < 4000.0f) ? 0.6f : 0.15f;
        s += juce::jlimit (0.0f, 0.4f, 1.0f - feat.flatness);
        if (useTs)
            s += juce::jlimit (0.0f, 0.6f, 1.0f - std::abs (t - 0.42f) * 2.5f);
        sc[(size_t) SourceClass::guitar] = s;
    }

    // Synth: flatter / air; pads = low T, slight preference for sustain-side
    {
        float s = 0.0f;
        s += juce::jlimit (0.0f, 1.2f, feat.flatness / 0.35f);
        s += juce::jlimit (0.0f, 0.8f, top / 0.35f);
        s += juce::jlimit (0.0f, 0.6f, 1.0f - subBass / 0.40f);
        s += (feat.rolloffHz > 6000.0f) ? 0.5f : 0.1f;
        if (useTs)
            s += juce::jlimit (0.0f, 0.5f, 1.0f - t * 0.9f);
        sc[(size_t) SourceClass::synth] = s;
    }

    // Drums: high crest + high transient ratio (same detector as Split)
    {
        float s = 0.0f;
        s += juce::jlimit (0.0f, 1.4f, (feat.crestDb - 8.0f) / 12.0f);
        s += juce::jlimit (0.0f, 0.8f, feat.flatness / 0.40f);
        const float maxBand = juce::jmax (feat.energySub,
            juce::jmax (feat.energyBass,
            juce::jmax (feat.energyMid, juce::jmax (feat.energyPresence, feat.energyAir))));
        s += juce::jlimit (0.0f, 0.8f, 1.0f - maxBand);
        s += (feat.energySub > 0.08f && feat.energyAir > 0.08f) ? 0.5f : 0.1f;
        if (useTs)
            s += juce::jlimit (0.0f, 1.35f, (t - 0.22f) / 0.40f); // strong T weight
        sc[(size_t) SourceClass::drums] = s;
    }

    // Mix: balanced spectrum + mid T/S
    {
        float s = 0.0f;
        s += juce::jlimit (0.0f, 0.9f, 1.0f - std::abs (subBass - 0.28f) * 2.0f);
        s += juce::jlimit (0.0f, 0.9f, 1.0f - std::abs (top - 0.30f) * 2.0f);
        s += juce::jlimit (0.0f, 0.6f, 1.0f - std::abs (feat.crestDb - 12.0f) / 15.0f);
        s += (feat.centroidHz > 300.0f && feat.centroidHz < 2500.0f) ? 0.5f : 0.15f;
        if (useTs)
            s += juce::jlimit (0.0f, 0.45f, 1.0f - std::abs (t - 0.35f) * 2.2f);
        sc[(size_t) SourceClass::mix] = s;
    }

    int best = (int) SourceClass::mix;
    int second = (int) SourceClass::bass;
    float bestS = -1.0f;
    float secondS = -1.0f;

    for (int i = (int) SourceClass::bass; i < (int) SourceClass::numClasses; ++i)
    {
        const float v = sc[(size_t) i];
        if (v > bestS)
        {
            secondS = bestS;
            second = best;
            bestS = v;
            best = i;
        }
        else if (v > secondS)
        {
            secondS = v;
            second = i;
        }
    }

    juce::ignoreUnused (second);

    const float margin = juce::jmax (0.0f, bestS - secondS);
    // Slightly higher score ceiling when T/S terms are active
    const float scoreCeil = useTs ? 4.0f : 3.2f;
    const float absNorm = juce::jlimit (0.0f, 1.0f, bestS / scoreCeil);
    const float marginNorm = juce::jlimit (0.0f, 1.0f, margin / 1.4f);
    float conf = 0.55f * absNorm + 0.45f * marginNorm;

    if (margin < 0.35f && sc[(size_t) SourceClass::mix] > bestS * 0.85f)
    {
        best = (int) SourceClass::mix;
        conf *= 0.85f;
    }

    conf = juce::jlimit (0.0f, 1.0f, conf);
    const float minC = juce::jlimit (0.15f, 0.8f, minConfidence);
    const float minScore = useTs ? 1.0f : 0.9f;

    if (conf < minC || bestS < minScore)
    {
        c.label = SourceClass::unknown;
        c.confidence = conf;
        c.summary = "Unknown (" + juce::String (juce::roundToInt (conf * 100.0f)) + "% - low confidence)";
    }
    else
    {
        c.label = (SourceClass) best;
        c.confidence = conf;
        c.summary = "likely " + sourceClassName (c.label)
                    + " (" + juce::String (juce::roundToInt (conf * 100.0f)) + "%)";
        if (useTs)
            c.summary += " | T " + juce::String (juce::roundToInt (t * 100.0f)) + "%";
    }

    return c;
}

Classification SourceClassifier::classify (const float* spectrumDb,
                                           const float* frequenciesHz,
                                           int numBins,
                                           float crestDb,
                                           float minConfidence,
                                           float transientRatio,
                                           bool hasTransientStats) noexcept
{
    return classify (extractFeatures (spectrumDb, frequenciesHz, numBins, crestDb,
                                      transientRatio, hasTransientStats),
                     minConfidence);
}

} // namespace EqLearn
