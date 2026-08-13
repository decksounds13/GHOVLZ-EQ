#pragma once

#include <JuceHeader.h>

/**
    Spectrum-analyser channel source and Span-style fractional-octave smoothing.

    Channel and octave-smooth combo indices must stay in lockstep with
    SPECTRUM_CHANNEL_ID / SPECTRUM_OCTAVE_SMOOTH_ID.
*/
namespace SpectrumAnalysis
{
    inline constexpr const char* channelParamId() noexcept { return "SPECTRUM_CHANNEL_ID"; }
    inline constexpr const char* octaveSmoothParamId() noexcept { return "SPECTRUM_OCTAVE_SMOOTH_ID"; }

    enum class Channel : int
    {
        both = 0,
        left,
        right,
        mid,
        side,
        midAndSide,
        leftAndRight
    };

    enum class OctaveSmooth : int
    {
        off = 0,
        oct1_24,
        oct1_12,
        oct1_6,
        oct1_3,
        oct1_2,
        oct1
    };

    inline juce::StringArray channelNames()
    {
        return { "Both", "Left", "Right", "Mid", "Side", "Mid + Side", "Left + Right" };
    }

    inline juce::StringArray octaveSmoothNames()
    {
        return { "Off", "1/24 oct", "1/12 oct", "1/6 oct", "1/3 oct", "1/2 oct", "1 oct" };
    }

    inline Channel channelFromIndex (int index) noexcept
    {
        return static_cast<Channel> (juce::jlimit (0, (int) Channel::leftAndRight, index));
    }

    inline bool isOverlay (Channel channel) noexcept
    {
        return channel == Channel::midAndSide || channel == Channel::leftAndRight;
    }

    inline float octaveWidthFromIndex (int index) noexcept
    {
        static constexpr float widths[] = {
            0.0f,
            1.0f / 24.0f,
            1.0f / 12.0f,
            1.0f / 6.0f,
            1.0f / 3.0f,
            0.5f,
            1.0f
        };

        return widths[juce::jlimit (0, 6, index)];
    }
}
