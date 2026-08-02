#include "StructuralSplitEngine.h"
#include <cmath>

void StructuralSplitEngine::prepare (double sampleRate, int maxBlockSize)
{
    sampleRateHz = sampleRate > 0.0 ? sampleRate : 48000.0;
    // Fast ~1 ms, slow ~40 ms — onset-sensitive difference.
    const float fastMs = 1.0f;
    const float slowMs = 40.0f;
    coeffFast = 1.0f - std::exp (-1.0f / (fastMs * 0.001f * (float) sampleRateHz));
    coeffSlow = 1.0f - std::exp (-1.0f / (slowMs * 0.001f * (float) sampleRateHz));
    const int n = juce::jmax (1, maxBlockSize);
    scratchDetect.assign ((size_t) n, 0.0f);
    scratchTGain.assign ((size_t) n, 0.0f);
    scratchSGain.assign ((size_t) n, 0.0f);
    reset();
}

void StructuralSplitEngine::reset() noexcept
{
    envFast = 0.0f;
    envSlow = 0.0f;
}

void StructuralSplitEngine::computeGains (const float* detect,
                                         int numSamples,
                                         float separation01,
                                         float* transientGain,
                                         float* sustainGain) noexcept
{
    if (detect == nullptr || transientGain == nullptr || sustainGain == nullptr || numSamples <= 0)
        return;

    // Map 0..1 separation → sensitivity. Mid (0.5) is a balanced default.
    const float sep = juce::jlimit (0.0f, 1.0f, separation01);
    const float sensitivity = juce::jmap (sep, 0.0f, 1.0f, 2.0f, 14.0f);

    for (int i = 0; i < numSamples; ++i)
    {
        const float x = std::abs (detect[i]);
        envFast += coeffFast * (x - envFast);
        envSlow += coeffSlow * (x - envSlow);
        const float onset = juce::jmax (0.0f, envFast - envSlow);
        // Soft knee into 0..1 transient weight.
        float t = onset * sensitivity;
        t = t / (1.0f + t); // soft saturate
        t = juce::jlimit (0.0f, 1.0f, t);
        transientGain[i] = t;
        sustainGain[i] = 1.0f - t;
    }
}

void StructuralSplitEngine::splitStereo (const float* inL, const float* inR,
                                         float* tL, float* tR,
                                         float* sL, float* sR,
                                         int numSamples,
                                         float separation01) noexcept
{
    if (inL == nullptr || tL == nullptr || sL == nullptr || numSamples <= 0)
        return;

    if ((int) scratchDetect.size() < numSamples)
    {
        scratchDetect.resize ((size_t) numSamples);
        scratchTGain.resize ((size_t) numSamples);
        scratchSGain.resize ((size_t) numSamples);
    }

    const float* r = inR != nullptr ? inR : inL;
    for (int i = 0; i < numSamples; ++i)
        scratchDetect[(size_t) i] = 0.5f * (inL[i] + r[i]);

    computeGains (scratchDetect.data(), numSamples, separation01,
                  scratchTGain.data(), scratchSGain.data());

    for (int i = 0; i < numSamples; ++i)
    {
        const float tg = scratchTGain[(size_t) i];
        const float sg = scratchSGain[(size_t) i];
        tL[i] = inL[i] * tg;
        sL[i] = inL[i] * sg;
        if (tR != nullptr && sR != nullptr)
        {
            tR[i] = r[i] * tg;
            sR[i] = r[i] * sg;
        }
    }
}

void StructuralSplitEngine::mixDeltaWithMask (float* wetL, float* wetR,
                                              const float* dryL, const float* dryR,
                                              const float* mask,
                                              int numSamples,
                                              int numChannels) noexcept
{
    if (wetL == nullptr || dryL == nullptr || mask == nullptr || numSamples <= 0)
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float m = mask[i];
        const float dL = dryL[i];
        wetL[i] = dL + (wetL[i] - dL) * m;
    }

    if (numChannels > 1 && wetR != nullptr && dryR != nullptr)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            const float m = mask[i];
            const float dR = dryR[i];
            wetR[i] = dR + (wetR[i] - dR) * m;
        }
    }
}
