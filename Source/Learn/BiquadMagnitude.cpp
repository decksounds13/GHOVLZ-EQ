#include "BiquadMagnitude.h"
#include "../FilterType.h"
#include <cmath>

namespace EqLearn
{
namespace BiquadMagnitude
{

namespace
{
    float magDbFromCoeffs (const juce::dsp::IIR::Coefficients<float>::Ptr& coeffs,
                           float freqHz,
                           double sampleRate) noexcept
    {
        if (coeffs == nullptr || sampleRate < 1.0)
            return 0.0f;

        const float* c = coeffs->getRawCoefficients();
        // JUCE layout: b0, b1, b2, a0, a1, a2
        const double b0 = (double) c[0];
        const double b1 = (double) c[1];
        const double b2 = (double) c[2];
        const double a0 = juce::jmax (1.0e-30, (double) c[3]);
        const double a1 = (double) c[4];
        const double a2 = (double) c[5];

        const double w = juce::MathConstants<double>::twoPi
                         * (double) juce::jlimit (1.0f, (float) (sampleRate * 0.49), freqHz)
                         / sampleRate;
        const double cosw = std::cos (w);
        const double sinw = std::sin (w);
        // z^{-1} = cos − j sin; z^{-2} = cos2 − j sin2
        const double cos2 = cosw * cosw - sinw * sinw;
        const double sin2 = 2.0 * cosw * sinw;

        const double nr = b0 + b1 * cosw + b2 * cos2;
        const double ni = -b1 * sinw - b2 * sin2;
        const double dr = a0 + a1 * cosw + a2 * cos2;
        const double di = -a1 * sinw - a2 * sin2;

        const double n2 = nr * nr + ni * ni;
        const double d2 = juce::jmax (1.0e-30, dr * dr + di * di);
        const double mag = std::sqrt (n2 / d2);
        return (float) (20.0 * std::log10 (juce::jmax (1.0e-12, mag)));
    }
}

void evaluateBandDb (int filterType,
                     float frequencyHz,
                     float gainDb,
                     float q,
                     const float* gridHz,
                     float* outDb,
                     int n,
                     double sampleRate) noexcept
{
    if (outDb == nullptr || n <= 0)
        return;

    if (gridHz == nullptr || std::abs (gainDb) < 1.0e-6f)
    {
        juce::FloatVectorOperations::clear (outDb, n);
        return;
    }

    auto coeffs = FilterType::makeCoefficients (filterType, sampleRate, frequencyHz, q, gainDb);
    for (int i = 0; i < n; ++i)
        outDb[i] = magDbFromCoeffs (coeffs, gridHz[i], sampleRate);
}

void evaluateCascadeDb (const BandProposal* bands,
                        int numBands,
                        const float* gridHz,
                        float* outDb,
                        int n,
                        double sampleRate) noexcept
{
    if (outDb == nullptr || n <= 0)
        return;

    juce::FloatVectorOperations::clear (outDb, n);
    if (bands == nullptr || numBands <= 0 || gridHz == nullptr)
        return;

    std::vector<float> one ((size_t) n);
    for (int b = 0; b < numBands; ++b)
    {
        const auto& p = bands[b];
        if (std::abs (p.gainDb) < 1.0e-5f)
            continue;
        evaluateBandDb (p.filterType, p.frequencyHz, p.gainDb, p.q,
                        gridHz, one.data(), n, sampleRate);
        for (int i = 0; i < n; ++i)
            outDb[i] += one[(size_t) i];
    }
}

float meanAbsError (const float* a, const float* b, int n) noexcept
{
    if (a == nullptr || b == nullptr || n <= 0)
        return 0.0f;

    double sum = 0.0;
    for (int i = 0; i < n; ++i)
        sum += (double) std::abs (a[i] - b[i]);
    return (float) (sum / (double) n);
}

} // namespace BiquadMagnitude
} // namespace EqLearn
