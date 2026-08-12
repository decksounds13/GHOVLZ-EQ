#include "SpectrogramReassignment.h"

void SpectrogramReassignment::prepare (int newSize, double sr)
{
    const int n = juce::jmax (0, newSize);
    const double rate = sr > 0.0 ? sr : 48000.0;
    if (n == fftSize && std::abs (rate - sampleRate) < 1.0e-6 && ! winH.empty())
        return;

    fftSize = n;
    sampleRate = rate;
    numBins = fftSize > 0 ? fftSize / 2 + 1 : 0;

    winH.assign ((size_t) fftSize, 0.0f);
    winT.assign ((size_t) fftSize, 0.0f);
    winD.assign ((size_t) fftSize, 0.0f);
    tmp.assign ((size_t) fftSize, 0.0f);
    ifHz.assign ((size_t) numBins, 0.0f);
    timeOffsetSamples.assign ((size_t) numBins, 0.0f);
    magDb.assign ((size_t) numBins, -120.0f);

    if (fftSize > 0)
        buildWindows();
}

void SpectrogramReassignment::buildWindows()
{
    // Periodic Hann (matches juce Hann on length N).
    const float invN = 1.0f / (float) fftSize;
    const float twoPi = juce::MathConstants<float>::twoPi;
    const float centre = 0.5f * (float) (fftSize - 1);

    for (int i = 0; i < fftSize; ++i)
    {
        const float h = 0.5f * (1.0f - std::cos (twoPi * (float) i * invN));
        winH[(size_t) i] = h;
        // Time ramp relative to window centre (samples) — Re{X_T X*/|X|²} → samples.
        winT[(size_t) i] = ((float) i - centre) * h;
        // d/dn of periodic Hann: 0.5 * (2π/N) * sin(2π i / N)
        winD[(size_t) i] = 0.5f * twoPi * invN * std::sin (twoPi * (float) i * invN);
    }
}

void SpectrogramReassignment::compute (juce::dsp::FFT& fft,
                                       const float* samples,
                                       float* workX,
                                       float* workT,
                                       float* workD)
{
    if (fftSize <= 0 || samples == nullptr || workX == nullptr
        || workT == nullptr || workD == nullptr || numBins < 3)
        return;

    auto forwardWindowed = [&] (const float* win, float* work)
    {
        for (int i = 0; i < fftSize; ++i)
            tmp[(size_t) i] = samples[i] * win[i];
        std::fill (work, work + fftSize * 2, 0.0f);
        std::copy (tmp.begin(), tmp.end(), work);
        fft.performRealOnlyForwardTransform (work, true);
    };

    // Always compute all three with matching windows (h / t·h / h').
    forwardWindowed (winH.data(), workX);
    forwardWindowed (winT.data(), workT);
    forwardWindowed (winD.data(), workD);

    const float twoPi = juce::MathConstants<float>::twoPi;
    const float binHz = (float) sampleRate / (float) fftSize;
    const float invTwoPi = 1.0f / twoPi;

    for (int k = 0; k < numBins; ++k)
    {
        const float re = workX[(size_t) (k * 2)];
        const float im = workX[(size_t) (k * 2 + 1)];
        const float mag2 = re * re + im * im;
        const float mag = std::sqrt (mag2) / (float) fftSize;
        const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-12f), -120.0f);
        magDb[(size_t) k] = db;

        const float centreHz = (float) k * binHz;
        ifHz[(size_t) k] = centreHz;
        timeOffsetSamples[(size_t) k] = 0.0f;

        if (k == 0 || k == numBins - 1 || db < kEnergyFloorDb || mag2 < 1.0e-30f)
            continue;

        const float reT = workT[(size_t) (k * 2)];
        const float imT = workT[(size_t) (k * 2 + 1)];
        const float reD = workD[(size_t) (k * 2)];
        const float imD = workD[(size_t) (k * 2 + 1)];

        // X_T * conj(X) / |X|²  and  X_D * conj(X) / |X|²
        const float invMag2 = 1.0f / mag2;
        // (a+bi)(c-di) = ac+bd + i(bc-ad)
        const float tRe = (reT * re + imT * im) * invMag2;
        // const float tIm = (imT * re - reT * im) * invMag2; // unused
        // const float dRe = (reD * re + imD * im) * invMag2;
        const float dIm = (imD * re - reD * im) * invMag2;

        // Auger–Flandrin: t̂ = t − Re{…}, ω̂ = ω + Im{…}
        // time offset from window centre (samples): −Re{X_T X*/|X|²}
        float tOff = -tRe;
        // ω in rad/sample: 2π k / N ; correction Im{X_D X*/|X|²} is also rad/sample
        // when h_D = dh/dn.
        float omega = twoPi * (float) k / (float) fftSize;
        float omegaHat = omega + dIm;
        float fHat = omegaHat * invTwoPi * (float) sampleRate;

        if (! std::isfinite (fHat) || fHat < 1.0f)
            fHat = centreHz;
        if (! std::isfinite (tOff))
            tOff = 0.0f;

        // Sanity clamps (separability / noise).
        tOff = juce::jlimit (-0.5f * (float) fftSize, 0.5f * (float) fftSize, tOff);
        // IF should stay near the channel (reject cross-term wild jumps).
        const float maxJumpHz = binHz * 2.5f;
        if (std::abs (fHat - centreHz) > maxJumpHz)
            fHat = centreHz;

        ifHz[(size_t) k] = fHat;
        timeOffsetSamples[(size_t) k] = tOff;
    }
}
