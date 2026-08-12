#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <complex>
#include "SpectralBandSettings.h"
#include "SpectralBinning.h"

/**
    Original STFT spectral-dynamics engine (clean-room).

    - Hann + 75% OLA, magnitude-only GR, phase preserved.
    - Res places a log lattice of GR centres inside the band Q (like lattice BPs):
      coarse ≈ few wide lobes, fine ≈ many surgical notches — visible on the graph.
    - Attack / release smooth detect envelope and per-slice GR.
    - Mid detect (dry or SC); linked GR on L/R wet.
    - Not a port of any third-party plugin source.
*/
class SpectralFftEngine
{
public:
    static constexpr int kFftOrder = 11;              // 2048
    static constexpr int kFftSize = 1 << kFftOrder;
    static constexpr int kNumBins = kFftSize / 2 + 1; // 1025
    static constexpr int kOverlap = 4;                // 75%
    static constexpr int kHopSize = kFftSize / kOverlap;
    /**
        Max log-spaced GR centres per S band (finest Res).
        Matches lattice visibility range (coarse few … fine many).
    */
    static constexpr int kMaxSlices = 64;
    static constexpr int kMinSlices = 6;
    static constexpr int kMaxPublishPoints = kMaxSlices;

    SpectralFftEngine() = default;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    void clearBands() noexcept;
    void setBand (int slot, const SpectralDynamics::BandSettings& settings) noexcept;

    bool hasActiveBands() const noexcept { return activeBandCount > 0; }

    void process (juce::dsp::AudioBlock<float>& block,
                  const float* detectL,
                  const float* detectR,
                  const float* scDetectL,
                  const float* scDetectR);

    int getLatencySamples() const noexcept { return kFftSize; }

    void samplePublishedGrDb (int bandIndex, const float* frequenciesHz,
                              float* destDb, int numPoints) const;
    bool hasActiveGr (int bandIndex) const noexcept;
    float getPublishedPeakGrDb (int bandIndex) const noexcept;

    struct RuntimeStats
    {
        int armedSlots = 0;
        int bankedBandpasses = 0;   // Res slice count (UI)
        int gatedBandpasses = 0;
        int processingBandpasses = 0;
    };

    RuntimeStats getRuntimeStats() const noexcept;

private:
    struct BandSlot
    {
        SpectralDynamics::BandSettings settings;
        bool active = false;
    };

    struct PublishedCurve
    {
        std::array<float, kMaxPublishPoints> centerHz {};
        std::array<float, kMaxPublishPoints> grDb {};
        int count = 0;
    };

    void processSample (float& wetL, float& wetR,
                        float detectMid, float scMid) noexcept;
    void processFrame() noexcept;
    void publishGrCurves() noexcept;
    float bandMask (float frequencyHz, const SpectralDynamics::BandSettings& settings) const noexcept;
    float binFrequencyHz (int bin) const noexcept;
    float safeMaxCenterHz() const noexcept;
    /** Res → slice count inside the Q aperture (6 coarse … 64 fine). */
    static int sliceCountForRes (float bandwidthHz) noexcept;
    /** Q soft-mask footprint [fLo, fHi] for placing slices (same geometry as lattice). */
    void computeInfluenceRange (const SpectralDynamics::BandSettings& settings,
                                float& fLo, float& fHi) const noexcept;
    static float sampleEnvAtHz (const float* envDb, int numBins, float frequencyHz,
                                double sampleRate, int fftSize) noexcept;

    double sampleRate = 48000.0;
    int numChannels = 2;

    juce::dsp::FFT fft { kFftOrder };
    /** Periodic Hann: length N (last sample not duplicated). */
    std::array<float, kFftSize> window {};
    float windowCorrection = 2.0f / 3.0f; // Hann² @ 75% OLA

    int pos = 0;
    int hopCount = 0;

    std::array<float, kFftSize> inFifoL {};
    std::array<float, kFftSize> inFifoR {};
    std::array<float, kFftSize> detFifo {};
    std::array<float, kFftSize> scFifo {};
    std::array<float, kFftSize> outFifoL {};
    std::array<float, kFftSize> outFifoR {};

    /** Interleaved complex work (size 2*N). */
    std::array<float, kFftSize * 2> fftWork {};
    std::array<float, kFftSize * 2> fftWorkR {};

    /** Per-slot bin envelopes (detect path). */
    std::array<std::array<float, kNumBins>, SpectralDynamics::kNumSlots> envDbSlot {};
    /** Per-slot / per-Res-slice smoothed GR (dB). */
    std::array<std::array<float, kMaxSlices>, SpectralDynamics::kNumSlots> grDbSmoothSlice {};
    /** Last published / audio GR centres for this hop (slot-local). */
    std::array<std::array<float, kMaxSlices>, SpectralDynamics::kNumSlots> sliceCenterHz {};
    std::array<std::array<float, kMaxSlices>, SpectralDynamics::kNumSlots> sliceGrDb {};
    std::array<int, SpectralDynamics::kNumSlots> sliceCount {};

    std::array<float, kNumBins> grDbBin {};
    std::array<float, kNumBins> grLinBin {};

    std::array<BandSlot, SpectralDynamics::kNumSlots> bands {};
    int activeBandCount = 0;

    std::array<std::array<PublishedCurve, 2>, SpectralDynamics::kNumSlots> publishedGr {};
    std::array<std::atomic<int>, SpectralDynamics::kNumSlots> publishedIndex {};
    std::array<std::atomic<float>, SpectralDynamics::kNumSlots> publishedPeakGrDb {};

    std::atomic<int> publishedArmed { 0 };
    std::atomic<int> publishedBins { 0 };

    /** True after a full STFT frame pass; cleared when falling back to delay-only. */
    bool stftWasRunning = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectralFftEngine)
};
