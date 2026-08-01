#pragma once

#include <JuceHeader.h>
#include <array>
#include <complex>
#include <vector>
#include "FilterType.h"
#include "FilterSlope.h"

/**
    Medium-quality linear-phase EQ (Pro-Q-style V1).

    Builds a symmetric FIR whose magnitude matches the product of the same IIR
    band responses used in Minimum Phase mode, then convolves in-place.
    Host latency = group delay = (irLength - 1) / 2.

    V1 notes:
    - Stereo processing only (per-band Mid/Side/L/R channel modes are ignored).
    - Dynamic (D) effective gains supported via IR rebuild when bands change.
    - Spectral (S) remains a separate post-EQ stage in EqProcessor.
*/
class LinearPhaseEqEngine
{
public:
    /** Design FFT 2048 → FIR 1025 taps (~10.7 ms group delay @ 48 kHz). */
    static constexpr int fftOrder = 11;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int irLength = (fftSize / 2) + 1; // 1025 (odd)
    static constexpr int groupDelay = (irLength - 1) / 2; // 512
    static constexpr int maxBands = 64; // EqBand::kMaxBanks * kBankSize

    struct BandSpec
    {
        bool enabled = false;
        bool isHighpass = false;
        bool isLowpass = false;
        int type = FilterType::bell;
        int slope = FilterSlope::db12;
        float frequency = 1000.0f;
        float q = 0.707f;
        float gainDb = 0.0f;
    };

    LinearPhaseEqEngine();
    ~LinearPhaseEqEngine();

    void prepare (double newSampleRate, int maxBlockSize, int numChannels);
    void reset() noexcept;
    void releaseResources();

    void setBands (const BandSpec* bands, int numBands) noexcept;
    void process (juce::dsp::AudioBlock<float>& block);

    int getLatencySamples() const noexcept { return prepared ? groupDelay : 0; }

private:
    void cacheBandCoefficients();
    void rebuildImpulseResponse();
    float magnitudeAt (float frequencyHz) const;

    double sampleRate = 48000.0;
    int numChannels = 2;
    bool prepared = false;
    bool responseDirty = true;

    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> ir;
    std::array<std::vector<float>, 2> delayLine; // length irLength, circular
    std::array<int, 2> delayPos { {} };

    std::vector<std::complex<float>> timeDomain;
    std::vector<std::complex<float>> freqDomain;

    std::array<BandSpec, maxBands> bands {};
    int bandCount = 0;

    /** Cached IIR stages per band — rebuilt with setBands, used by magnitudeAt. */
    std::array<juce::ReferenceCountedArray<juce::dsp::IIR::Coefficients<float>>, maxBands> bandCoeffs {};
    std::array<bool, maxBands> bandActive {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LinearPhaseEqEngine)
};
