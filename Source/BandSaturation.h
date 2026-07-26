#pragma once

#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>

/**
    Per-band saturation (Spectre-inspired).

    Pre:  EQ band first, then waveshape the full signal (emphasis into drive).
    Post: sat(EQ − dry) + dry — parallel harmonics shaped by gain/Q (Spectre-like).

    Nonlinear stage uses juce::dsp::Oversampling when factor > Off.
*/
namespace BandSaturation
{
    enum Model : int
    {
        tape = 0,
        tube,
        diode,
        dualTriode,
        numModels
    };

    enum Oversample : int
    {
        osOff = 0,
        os2x,
        os4x,
        os8x,
        numOversample
    };

    inline juce::StringArray getModelChoiceNames()
    {
        return { "Tape", "Tube", "Diode", "Dual-Triode" };
    }

    inline juce::StringArray getOversampleChoiceNames()
    {
        return { "Off", "2x", "4x", "8x" };
    }

    inline constexpr const char* oversampleParamId() noexcept { return "satOversample"; }

    /** Stage 2 — post-Spectral bus sat (global). */
    inline constexpr const char* spectralSatParamId() noexcept { return "spectralSat"; }
    inline constexpr const char* spectralSatModelParamId() noexcept { return "spectralSatModel"; }
    inline constexpr const char* spectralSatDriveParamId() noexcept { return "spectralSatDrive"; }
    inline constexpr const char* spectralSatOversampleParamId() noexcept { return "spectralSatOversample"; }

    constexpr float kMinSpectralSatDrive = 0.0f;
    constexpr float kMaxSpectralSatDrive = 1.0f;
    constexpr float kDefaultSpectralSatDrive = 0.35f;

    /** Clamp before nonlinear / OS — extreme EQ boosts can be huge or non-finite. */
    inline float sanitizeDriveSample (float x) noexcept
    {
        if (! std::isfinite (x))
            return 0.0f;
        // Soft ceiling so OS half-band filters never see astronomical peaks.
        constexpr float lim = 8.0f;
        return juce::jlimit (-lim, lim, x);
    }

    inline juce::String satParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1Sat";
            case 1: return "band2Sat";
            case 2: return "band3Sat";
            case 3: return "band4Sat";
            case 6: return "highShelfSat";
            case 7: return "lowShelfSat";
            default: return {};
        }
    }

    inline juce::String satModelParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SatModel";
            case 1: return "band2SatModel";
            case 2: return "band3SatModel";
            case 3: return "band4SatModel";
            case 6: return "highShelfSatModel";
            case 7: return "lowShelfSatModel";
            default: return {};
        }
    }

    /** false/0 = Pre, true/1 = Post */
    inline juce::String satPostParamIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1SatPost";
            case 1: return "band2SatPost";
            case 2: return "band3SatPost";
            case 3: return "band4SatPost";
            case 6: return "highShelfSatPost";
            case 7: return "lowShelfSatPost";
            default: return {};
        }
    }

    /** Same bands as Dynamic / Spectral (gain-using types only at runtime). */
    inline bool supportsSat (int bandIndex)
    {
        return bandIndex == 0 || bandIndex == 1 || bandIndex == 2 || bandIndex == 3
            || bandIndex == 6 || bandIndex == 7;
    }

    inline float shapeSample (float x, int model) noexcept
    {
        x = sanitizeDriveSample (x);

        switch (model)
        {
            case tape:
            {
                const float d = 1.35f;
                return std::tanh (x * d) / std::tanh (d);
            }
            case diode:
            {
                const float d = 2.4f;
                const float y = x * d;
                return y / (1.0f + std::abs (y));
            }
            case dualTriode:
            {
                const float a = std::tanh (x * 1.1f);
                return std::tanh (a * 1.6f + 0.08f * a * a);
            }
            case tube:
            default:
            {
                const float d = 1.8f;
                return std::tanh (x * d) / std::tanh (d);
            }
        }
    }

    class Engine
    {
    public:
        void prepare (double sampleRate, int samplesPerBlock, int numChannels)
        {
            sr = sampleRate > 0.0 ? sampleRate : 48000.0;
            maxBlock = juce::jmax (1, samplesPerBlock);
            channels = juce::jmax (1, numChannels);
            dryBuffer.setSize (channels, maxBlock, false, false, true);
            diffBuffer.setSize (channels, maxBlock, false, false, true);
            workBuffer.setSize (channels, maxBlock, false, false, true);
            rebuildOversampling();
            reset();
        }

        void reset() noexcept
        {
            if (oversampling != nullptr)
                oversampling->reset();
        }

        void releaseResources()
        {
            oversampling.reset();
            dryBuffer.setSize (0, 0);
            diffBuffer.setSize (0, 0);
            workBuffer.setSize (0, 0);
        }

        /** 0=Off, 1=2x, 2=4x, 3=8x — uses juce::dsp::Oversampling. */
        void setOversampleIndex (int index)
        {
            const int clamped = juce::jlimit (0, numOversample - 1, index);
            if (clamped == osIndex)
            {
                // Off needs no instance; On must already have one.
                if (clamped == osOff || oversampling != nullptr)
                    return;
            }

            osIndex = clamped;
            rebuildOversampling();
        }

        int getOversampleIndex() const noexcept { return osIndex; }

        int getLatencySamples() const noexcept
        {
            if (oversampling == nullptr || osIndex == osOff)
                return 0;
            return (int) oversampling->getLatencyInSamples();
        }

        void setModel (int modelIndex) noexcept
        {
            model = juce::jlimit (0, numModels - 1, modelIndex);
        }

        /** Store pre-EQ dry for a following processPost. */
        void captureDry (const juce::dsp::AudioBlock<float>& block)
        {
            const int n = (int) block.getNumSamples();
            const int chans = (int) block.getNumChannels();
            if (n <= 0 || chans <= 0)
            {
                drySamples = 0;
                dryChannels = 0;
                return;
            }

            ensureCapacity (chans, n);
            for (int ch = 0; ch < chans; ++ch)
                dryBuffer.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), n);
            drySamples = n;
            dryChannels = chans;
        }

        /** Pre mode: waveshape in place (call after EQ). */
        void processPre (juce::dsp::AudioBlock<float>& block)
        {
            applyShaper (block);
        }

        /**
            Stage 2 bus sat: drive gain into waveshaper, blend with dry.
            drive01 0 = bypass-ish mild, 1 = hot. mix follows drive (0→transparent-ish).
        */
        void processBusDriven (juce::dsp::AudioBlock<float>& block, float drive01)
        {
            const int n = (int) block.getNumSamples();
            const int chans = (int) block.getNumChannels();
            if (n <= 0 || chans <= 0)
                return;

            const float drive = juce::jlimit (0.0f, 1.0f, drive01);
            if (drive < 1.0e-4f)
                return;

            // 0→1 maps to ~0 dB…+18 dB into the shaper; blend wet by drive.
            const float preGain = juce::Decibels::decibelsToGain (drive * 18.0f);
            const float wet = drive;
            const float dryMix = 1.0f - wet * 0.85f; // keep some dry even at full drive

            ensureCapacity (chans, n);
            for (int ch = 0; ch < chans; ++ch)
                dryBuffer.copyFrom (ch, 0, block.getChannelPointer ((size_t) ch), n);

            for (int ch = 0; ch < chans; ++ch)
            {
                auto* d = block.getChannelPointer ((size_t) ch);
                for (int i = 0; i < n; ++i)
                    d[i] = sanitizeDriveSample (d[i] * preGain);
            }

            applyShaper (block);

            for (int ch = 0; ch < chans; ++ch)
            {
                auto* out = block.getChannelPointer ((size_t) ch);
                const float* dry = dryBuffer.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                {
                    const float y = dry[i] * dryMix + out[i] * wet;
                    out[i] = std::isfinite (y) ? y : dry[i];
                }
            }
        }

        /**
            Post mode: block is post-EQ. Replaces with dry + sat(EQ − dry).
            Requires captureDry() from the pre-EQ block first.
        */
        void processPost (juce::dsp::AudioBlock<float>& eqBlock)
        {
            const int n = (int) eqBlock.getNumSamples();
            const int chans = (int) eqBlock.getNumChannels();
            if (n <= 0 || chans <= 0)
                return;

            if (n != drySamples || chans != dryChannels)
            {
                // Mismatch — don't risk a bad OS pass; fall back to Pre-style on wet.
                applyShaper (eqBlock);
                return;
            }

            ensureCapacity (chans, n);
            for (int ch = 0; ch < chans; ++ch)
            {
                auto* eq = eqBlock.getChannelPointer ((size_t) ch);
                const float* dry = dryBuffer.getReadPointer (ch);
                float* diff = diffBuffer.getWritePointer (ch);
                for (int i = 0; i < n; ++i)
                    diff[i] = sanitizeDriveSample (eq[i] - dry[i]);
            }

            {
                juce::dsp::AudioBlock<float> diffBlock (diffBuffer.getArrayOfWritePointers(),
                                                       (size_t) chans,
                                                       (size_t) n);
                applyShaper (diffBlock);
            }

            for (int ch = 0; ch < chans; ++ch)
            {
                auto* eq = eqBlock.getChannelPointer ((size_t) ch);
                const float* dry = dryBuffer.getReadPointer (ch);
                const float* harm = diffBuffer.getReadPointer (ch);
                for (int i = 0; i < n; ++i)
                {
                    const float y = dry[i] + harm[i];
                    eq[i] = std::isfinite (y) ? y : dry[i];
                }
            }
        }

    private:
        void ensureCapacity (int chans, int n)
        {
            const int needCh = juce::jmax (chans, channels);
            const int needN = juce::jmax (n, maxBlock);
            bool rebuildOs = false;

            // Grow maxBlock if the host ever delivers a larger buffer than prepare.
            if (n > maxBlock)
            {
                maxBlock = n;
                rebuildOs = true;
            }

            if (chans > channels)
            {
                channels = chans;
                rebuildOs = true;
            }

            if (dryBuffer.getNumChannels() < needCh || dryBuffer.getNumSamples() < needN)
                dryBuffer.setSize (needCh, needN, false, false, true);
            if (diffBuffer.getNumChannels() < needCh || diffBuffer.getNumSamples() < needN)
                diffBuffer.setSize (needCh, needN, false, false, true);
            if (workBuffer.getNumChannels() < needCh || workBuffer.getNumSamples() < needN)
                workBuffer.setSize (needCh, needN, false, false, true);

            if (rebuildOs)
                rebuildOversampling();
        }

        void rebuildOversampling()
        {
            oversampling.reset();
            if (osIndex == osOff)
                return;

            // factor: 1→2x, 2→4x, 3→8x
            const size_t factor = (size_t) osIndex;
            oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) juce::jmax (1, channels),
                factor,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true);
            oversampling->initProcessing ((size_t) juce::jmax (1, maxBlock));
            oversampling->reset();
        }

        void applyShaper (juce::dsp::AudioBlock<float>& block)
        {
            const int n = (int) block.getNumSamples();
            const int chans = (int) block.getNumChannels();
            if (n <= 0 || chans <= 0)
                return;

            ensureCapacity (chans, n);

            // Internal buffer channel count always matches the Oversampling object.
            for (int ch = 0; ch < channels; ++ch)
            {
                float* dst = workBuffer.getWritePointer (ch);
                if (ch < chans)
                {
                    const float* src = block.getChannelPointer ((size_t) ch);
                    for (int i = 0; i < n; ++i)
                        dst[i] = sanitizeDriveSample (src[i]);
                }
                else
                {
                    juce::FloatVectorOperations::clear (dst, n);
                }
            }

            juce::dsp::AudioBlock<float> workSub (workBuffer.getArrayOfWritePointers(),
                                                 (size_t) channels,
                                                 (size_t) n);

            if (oversampling != nullptr && osIndex != osOff && n <= maxBlock)
            {
                auto osBlock = oversampling->processSamplesUp (workSub);
                shapeBlock (osBlock);
                oversampling->processSamplesDown (workSub);
            }
            else
            {
                shapeBlock (workSub);
            }

            for (int ch = 0; ch < chans; ++ch)
            {
                auto* dst = block.getChannelPointer ((size_t) ch);
                const float* src = workBuffer.getReadPointer (ch);
                juce::FloatVectorOperations::copy (dst, src, n);
            }
        }

        void shapeBlock (juce::dsp::AudioBlock<float>& block) noexcept
        {
            const int n = (int) block.getNumSamples();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* d = block.getChannelPointer (ch);
                for (int i = 0; i < n; ++i)
                    d[i] = shapeSample (d[i], model);
            }
        }

        double sr = 48000.0;
        int maxBlock = 512;
        int channels = 2;
        int osIndex = osOff;
        int model = tube;
        int drySamples = 0;
        int dryChannels = 0;
        juce::AudioBuffer<float> dryBuffer;
        juce::AudioBuffer<float> diffBuffer;
        juce::AudioBuffer<float> workBuffer;
        std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    };
}
