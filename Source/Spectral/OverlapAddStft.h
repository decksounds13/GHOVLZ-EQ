#pragma once

#include <JuceHeader.h>
#include <array>
#include <cmath>
#include <complex>
#include <cstring>

/**
    Stereo Hann 75% OLA STFT (same geometry as Spectral FFT).
    Frame callback sees interleaved complex L/R (JUCE real-only layout).
*/
class OverlapAddStft
{
public:
    static constexpr int kOrder = 11;
    static constexpr int kSize = 1 << kOrder;
    static constexpr int kBins = kSize / 2 + 1;
    static constexpr int kHop = kSize / 4;

    void prepare (double sr) noexcept
    {
        sampleRate = sr > 0.0 ? sr : 48000.0;
        const float delta = juce::MathConstants<float>::twoPi / (float) kSize;
        for (int i = 0; i < kSize; ++i)
            window[(size_t) i] = 0.5f * (1.0f - std::cos (delta * (float) i));
        reset();
        prepared = true;
    }

    void reset() noexcept
    {
        pos = 0;
        hopCount = 0;
        inL.fill (0.0f);
        inR.fill (0.0f);
        det.fill (0.0f);
        outL.fill (0.0f);
        outR.fill (0.0f);
    }

    int latencySamples() const noexcept { return kSize; }
    double getSampleRate() const noexcept { return sampleRate; }
    float binHz (int bin) const noexcept
    {
        return (float) bin * (float) sampleRate / (float) kSize;
    }

    /** onFrame (workL, workR, detMag, numBins) — work buffers are real-only FFT layout. */
    template <typename Fn>
    void process (juce::AudioBuffer<float>& buffer,
                  const float* detectL,
                  const float* detectR,
                  Fn&& onFrame) noexcept
    {
        if (! prepared)
            prepare (sampleRate);

        const int n = buffer.getNumSamples();
        const int ch = buffer.getNumChannels();
        if (n <= 0 || ch < 1)
            return;

        auto* l = buffer.getWritePointer (0);
        auto* r = ch > 1 ? buffer.getWritePointer (1) : nullptr;
        const float* dL = detectL != nullptr ? detectL : l;
        const float* dR = detectR != nullptr ? detectR : (r != nullptr ? r : dL);

        for (int i = 0; i < n; ++i)
        {
            float wetL = l[i];
            float wetR = r != nullptr ? r[i] : wetL;
            const float mid = 0.5f * (dL[i] + dR[i]);

            inL[(size_t) pos] = wetL;
            inR[(size_t) pos] = wetR;
            det[(size_t) pos] = mid;

            float yL = outL[(size_t) pos];
            float yR = outR[(size_t) pos];
            outL[(size_t) pos] = 0.0f;
            outR[(size_t) pos] = 0.0f;

            pos += 1;
            if (pos >= kSize)
                pos = 0;

            hopCount += 1;
            if (hopCount >= kHop)
            {
                hopCount = 0;
                runFrame (onFrame);
            }

            l[i] = yL;
            if (r != nullptr)
                r[i] = yR;
        }
    }

private:
    void copyFifo (const float* fifo, float* dest) const noexcept
    {
        const int first = kSize - pos;
        if (first > 0)
            std::memcpy (dest, fifo + pos, (size_t) first * sizeof (float));
        if (pos > 0)
            std::memcpy (dest + first, fifo, (size_t) pos * sizeof (float));
    }

    template <typename Fn>
    void runFrame (Fn&& onFrame) noexcept
    {
        copyFifo (inL.data(), workL.data());
        copyFifo (inR.data(), workR.data());
        copyFifo (det.data(), workDet.data());

        for (int i = 0; i < kSize; ++i)
        {
            workL[(size_t) i] *= window[(size_t) i];
            workR[(size_t) i] *= window[(size_t) i];
            workDet[(size_t) i] *= window[(size_t) i];
        }

        fft.performRealOnlyForwardTransform (workDet.data(), true);
        {
            auto* c = reinterpret_cast<std::complex<float>*> (workDet.data());
            for (int k = 0; k < kBins; ++k)
                detMag[(size_t) k] = std::abs (c[k]);
        }

        fft.performRealOnlyForwardTransform (workL.data(), true);
        fft.performRealOnlyForwardTransform (workR.data(), true);

        onFrame (workL.data(), workR.data(), detMag.data(), kBins);

        fft.performRealOnlyInverseTransform (workL.data());
        fft.performRealOnlyInverseTransform (workR.data());

        // Hann² @ 75% OLA → 1.5; inverse already /N in JUCE real-only.
        const float ola = windowCorrection;
        for (int i = 0; i < kSize; ++i)
        {
            const int idx = (pos + i) % kSize;
            const float w = window[(size_t) i] * ola;
            outL[(size_t) idx] += workL[(size_t) i] * w;
            outR[(size_t) idx] += workR[(size_t) i] * w;
        }
    }

    double sampleRate = 48000.0;
    bool prepared = false;
    int pos = 0;
    int hopCount = 0;
    juce::dsp::FFT fft { kOrder };
    std::array<float, kSize> window {};
    float windowCorrection = 2.0f / 3.0f;
    std::array<float, kSize> inL {}, inR {}, det {}, outL {}, outR {};
    std::array<float, kSize * 2> workL {}, workR {}, workDet {};
    std::array<float, kBins> detMag {};
};
