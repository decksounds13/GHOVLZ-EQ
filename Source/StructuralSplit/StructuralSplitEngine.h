#pragma once

#include <JuceHeader.h>
#include <vector>

/**
    Complementary transient / sustain splitter (envelope-based).

    Fast vs slow envelope difference → transient gain; sustain = 1 - transient.
    T + S ≈ input. Separation (0–100) scales how aggressively onsets are classed
    as transient.
*/
class StructuralSplitEngine
{
public:
    void prepare (double sampleRate, int maxBlockSize);
    void reset() noexcept;

    /**
        Fill transientGain[0..numSamples) and sustainGain[0..numSamples) from a
        mono detect signal (typically mid of dry L/R). Gains are 0..1 and sum to 1.
    */
    void computeGains (const float* detect,
                       int numSamples,
                       float separation01,
                       float* transientGain,
                       float* sustainGain) noexcept;

    /** Apply complementary split of stereo inL/inR into tL/tR and sL/sR. */
    void splitStereo (const float* inL, const float* inR,
                      float* tL, float* tR,
                      float* sL, float* sR,
                      int numSamples,
                      float separation01) noexcept;

    /**
        Mix wet/dry using a per-sample mask: out = dry + (wet - dry) * mask.
        mask == transientGain for Transient mode, sustainGain for Sustain mode.
    */
    static void mixDeltaWithMask (float* wetL, float* wetR,
                                  const float* dryL, const float* dryR,
                                  const float* mask,
                                  int numSamples,
                                  int numChannels) noexcept;

private:
    double sampleRateHz = 48000.0;
    float envFast = 0.0f;
    float envSlow = 0.0f;
    float coeffFast = 0.0f;
    float coeffSlow = 0.0f;

    std::vector<float> scratchDetect;
    std::vector<float> scratchTGain;
    std::vector<float> scratchSGain;
};
