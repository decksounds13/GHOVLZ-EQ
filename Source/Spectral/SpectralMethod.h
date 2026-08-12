#pragma once

#include <JuceHeader.h>

/**
    Global spectral dynamics method (Settings → Spectrum).
    Lattice = existing IIR bandpass bank (default, zero latency).
    Fft = STFT magnitude GR (reports latency).
*/
namespace SpectralMethod
{
    enum class Kind
    {
        lattice = 0,
        fft = 1
    };

    inline constexpr const char* paramId() noexcept { return "SPECTRAL_METHOD_ID"; }

    inline juce::StringArray choiceNames()
    {
        return { "Lattice (zero latency)", "FFT (spectral)" };
    }

    inline Kind fromChoiceIndex (int index) noexcept
    {
        return index == 1 ? Kind::fft : Kind::lattice;
    }

    inline int toChoiceIndex (Kind k) noexcept
    {
        return k == Kind::fft ? 1 : 0;
    }
}
