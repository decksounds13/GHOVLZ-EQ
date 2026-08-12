#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
#include <cmath>

/**
    Clean-room Auger–Flandrin spectrogram reassignment (study-only refs:
    Kodera / Auger–Flandrin / Fitz–Fulop arXiv:0903.3080).

    Three windows: h, h_T = (n-n_c)·h, h_D ≈ dh/dn.
    Per bin (when |X|² is significant):

      t̂ = t − Re{ X_T X* / |X|² }     (samples relative to window centre)
      ω̂ = ω + Im{ X_D X* / |X|² }     (rad/sample → Hz)

    Not a port of any third-party product source.
*/
class SpectrogramReassignment
{
public:
    void prepare (int fftSize, double sampleRate);

    int getFftSize() const noexcept { return fftSize; }
    int getNumBins() const noexcept { return numBins; }

    /**
        Compute IF (Hz) and group-delay offset (samples from window centre)
        for each bin. `samples` is unwindowed length fftSize.
        `workX` may already hold the complex STFT of h·x (interleaved re,im);
        if null, X is computed here. workT/workD must be 2*fftSize.
    */
    void compute (juce::dsp::FFT& fft,
                  const float* samples,
                  float* workX,
                  float* workT,
                  float* workD);

    const std::vector<float>& getIfHz() const noexcept { return ifHz; }
    const std::vector<float>& getTimeOffsetSamples() const noexcept { return timeOffsetSamples; }
    const std::vector<float>& getMagDb() const noexcept { return magDb; }

    /** Energy floor (dB) below which bins are not reassigned. */
    static constexpr float kEnergyFloorDb = -112.0f;

private:
    void buildWindows();

    int fftSize = 0;
    int numBins = 0;
    double sampleRate = 48000.0;

    std::vector<float> winH;
    std::vector<float> winT; // (n - n_c) * h  [samples]
    std::vector<float> winD; // dh/dn
    std::vector<float> tmp;  // windowed real scratch length N

    std::vector<float> ifHz;
    std::vector<float> timeOffsetSamples;
    std::vector<float> magDb;
};
