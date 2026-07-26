#include "LinearPhaseEqEngine.h"

LinearPhaseEqEngine::LinearPhaseEqEngine() = default;

LinearPhaseEqEngine::~LinearPhaseEqEngine()
{
    releaseResources();
}

void LinearPhaseEqEngine::prepare (double newSampleRate, int maxBlockSize, int channels)
{
    juce::ignoreUnused (maxBlockSize);

    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    numChannels = juce::jlimit (1, 2, channels);
    prepared = true;
    responseDirty = true;

    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    ir.assign ((size_t) irLength, 0.0f);
    timeDomain.assign ((size_t) fftSize, {});
    freqDomain.assign ((size_t) fftSize, {});

    for (int ch = 0; ch < 2; ++ch)
    {
        delayLine[(size_t) ch].assign ((size_t) irLength, 0.0f);
        delayPos[(size_t) ch] = 0;
    }

    reset();
    rebuildImpulseResponse();
}

void LinearPhaseEqEngine::reset() noexcept
{
    for (int ch = 0; ch < 2; ++ch)
    {
        std::fill (delayLine[(size_t) ch].begin(), delayLine[(size_t) ch].end(), 0.0f);
        delayPos[(size_t) ch] = 0;
    }
}

void LinearPhaseEqEngine::releaseResources()
{
    prepared = false;
    fft.reset();
    ir.clear();
    timeDomain.clear();
    freqDomain.clear();
    for (int ch = 0; ch < 2; ++ch)
        delayLine[(size_t) ch].clear();
}

void LinearPhaseEqEngine::setBands (const BandSpec* newBands, int numBands) noexcept
{
    const int n = juce::jlimit (0, maxBands, numBands);
    bool changed = (n != bandCount);

    auto nearlyEqual = [] (float a, float b) noexcept
    {
        return std::abs (a - b) <= 1.0e-5f;
    };

    for (int i = 0; i < n && ! changed; ++i)
    {
        const auto& a = bands[(size_t) i];
        const auto& b = newBands[i];
        changed = a.enabled != b.enabled
                  || a.isHighpass != b.isHighpass
                  || a.isLowpass != b.isLowpass
                  || a.type != b.type
                  || a.slope != b.slope
                  || ! nearlyEqual (a.frequency, b.frequency)
                  || ! nearlyEqual (a.q, b.q)
                  || ! nearlyEqual (a.gainDb, b.gainDb);
    }

    bandCount = n;
    for (int i = 0; i < bandCount; ++i)
        bands[(size_t) i] = newBands[i];
    for (int i = bandCount; i < maxBands; ++i)
        bands[(size_t) i] = {};

    if (changed)
        responseDirty = true;
}

void LinearPhaseEqEngine::cacheBandCoefficients()
{
    const double sr = sampleRate;

    for (int b = 0; b < maxBands; ++b)
    {
        bandCoeffs[(size_t) b].clear();
        bandActive[(size_t) b] = false;

        if (b >= bandCount)
            continue;

        const auto& band = bands[(size_t) b];
        if (! band.enabled)
            continue;

        bandActive[(size_t) b] = true;

        if (band.isHighpass)
        {
            bandCoeffs[(size_t) b] = FilterSlope::makeHighpassCoeffs (sr, band.frequency, band.q, band.slope);
            continue;
        }

        if (band.isLowpass)
        {
            bandCoeffs[(size_t) b] = FilterSlope::makeLowpassCoeffs (sr, band.frequency, band.q, band.slope);
            continue;
        }

        bandCoeffs[(size_t) b].add (FilterType::makeCoefficients (band.type, sr, band.frequency, band.q, band.gainDb));
    }
}

float LinearPhaseEqEngine::magnitudeAt (float frequencyHz) const
{
    float mag = 1.0f;
    const double sr = sampleRate;
    const float f = juce::jlimit (0.0f, (float) (sr * 0.5), frequencyHz);

    for (int b = 0; b < maxBands; ++b)
    {
        if (! bandActive[(size_t) b])
            continue;

        for (auto* coeffs : bandCoeffs[(size_t) b])
            if (coeffs != nullptr)
                mag *= (float) coeffs->getMagnitudeForFrequency ((double) f, sr);
    }

    // Allow true zero at stopband (HP DC / LP Nyquist). Do not floor here —
    // a tiny DC target previously crushed highpass IRs during renormalization.
    return juce::jmax (0.0f, mag);
}

void LinearPhaseEqEngine::rebuildImpulseResponse()
{
    if (fft == nullptr || (int) ir.size() != irLength)
        return;

    cacheBandCoefficients();

    // Zero-phase Hermitian spectrum from desired magnitude, then rotate to
    // causal linear-phase FIR of length irLength (symmetric about groupDelay).
    for (int k = 0; k <= fftSize / 2; ++k)
    {
        const float freq = (float) (k * sampleRate / (double) fftSize);
        freqDomain[(size_t) k] = { magnitudeAt (freq), 0.0f };
    }

    for (int k = 1; k < fftSize / 2; ++k)
        freqDomain[(size_t) (fftSize - k)] = std::conj (freqDomain[(size_t) k]);

    fft->perform (freqDomain.data(), timeDomain.data(), true);

    for (int n = 0; n < irLength; ++n)
    {
        const int src = (n - groupDelay + fftSize) % fftSize;
        ir[(size_t) n] = timeDomain[(size_t) src].real() / (float) fftSize;
    }

    // Light Hann taper reduces design-FFT time aliasing; keep IR ~symmetric.
    for (int n = 0; n < irLength; ++n)
    {
        const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi
                                                 * (float) n / (float) (irLength - 1));
        ir[(size_t) n] *= w;
    }

    // Restore passband gain after windowing. DC restore is correct for LP /
    // bass-centred responses, but fatal for highpass (target DC ≈ 0 → scale
    // crushes the whole IR). Use Nyquist when that is the stronger passband.
    const float nyquist = (float) (sampleRate * 0.5);
    const float targetDc = magnitudeAt (0.0f);
    const float targetNyq = magnitudeAt (nyquist);

    float sumDc = 0.0f;
    float sumNyq = 0.0f;
    for (int n = 0; n < irLength; ++n)
    {
        const float c = ir[(size_t) n];
        sumDc += c;
        sumNyq += ((n & 1) != 0 ? -c : c);
    }

    float scale = 1.0f;
    if (targetDc >= targetNyq)
    {
        if (std::abs (sumDc) > 1.0e-12f)
            scale = targetDc / sumDc;
    }
    else if (std::abs (sumNyq) > 1.0e-12f)
    {
        scale = targetNyq / sumNyq;
    }

    if (std::abs (scale - 1.0f) > 1.0e-6f && std::isfinite (scale))
        for (float& c : ir)
            c *= scale;

    responseDirty = false;
}

void LinearPhaseEqEngine::process (juce::dsp::AudioBlock<float>& block)
{
    if (! prepared || block.getNumSamples() == 0)
        return;

    if (responseDirty)
        rebuildImpulseResponse();

    const int n = (int) block.getNumSamples();
    const int chans = juce::jmin (numChannels, (int) block.getNumChannels());

    if (chans <= 0 || (int) ir.size() != irLength)
        return;

    for (int ch = 0; ch < chans; ++ch)
    {
        auto* data = block.getChannelPointer ((size_t) ch);
        auto& delay = delayLine[(size_t) ch];
        int pos = delayPos[(size_t) ch];

        if ((int) delay.size() != irLength)
            continue;

        for (int i = 0; i < n; ++i)
        {
            delay[(size_t) pos] = data[i];

            float y = 0.0f;
            int j = pos;
            for (int k = 0; k < irLength; ++k)
            {
                y += ir[(size_t) k] * delay[(size_t) j];
                j = (j - 1 + irLength) % irLength;
            }

            pos = (pos + 1) % irLength;
            data[i] = y;
        }

        delayPos[(size_t) ch] = pos;
    }

    if (chans == 1 && (int) block.getNumChannels() > 1)
        juce::FloatVectorOperations::copy (block.getChannelPointer (1),
                                           block.getChannelPointer (0),
                                           n);
}
