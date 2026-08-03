#pragma once

#include <JuceHeader.h>
#include <cmath>

/** Equal-tempered note helpers (A4 = 440 Hz, C4 = MIDI 60 / Roland middle C). */
namespace MusicNote
{
    constexpr float kA4Hz = 440.0f;
    constexpr int kA4Midi = 69;
    constexpr int kC4Midi = 60;
    /** 88-key grand (FabFilter Pro-Q piano display): A0 (27.5 Hz) .. C8 (4186 Hz). */
    constexpr int kPianoLowestMidi = 21;   // A0
    constexpr int kPianoHighestMidi = 108; // C8

    inline float midiToHz (int midiNote) noexcept
    {
        return kA4Hz * std::pow (2.0f, (float) (midiNote - kA4Midi) / 12.0f);
    }

    inline float hzToMidiFloat (float hz) noexcept
    {
        const float f = juce::jmax (1.0e-3f, hz);
        return (float) kA4Midi + 12.0f * std::log2 (f / kA4Hz);
    }

    inline int hzToNearestMidi (float hz) noexcept
    {
        return (int) std::lround (hzToMidiFloat (hz));
    }

    inline float snapHzToNearestNote (float hz) noexcept
    {
        return midiToHz (hzToNearestMidi (hz));
    }

    inline juce::String midiToName (int midiNote) noexcept
    {
        static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
        const int n = ((midiNote % 12) + 12) % 12;
        const int octave = (midiNote / 12) - 1; // C4 = 60 → octave 4
        return juce::String (names[n]) + juce::String (octave);
    }

    inline juce::String formatNoteWithCents (float hz) noexcept
    {
        const float midiF = hzToMidiFloat (hz);
        const int midi = (int) std::lround (midiF);
        const float cents = (midiF - (float) midi) * 100.0f;
        juce::String s = midiToName (midi);
        if (std::abs (cents) >= 0.5f)
            s += (cents >= 0.0f ? " +" : " ") + juce::String (cents, 0);
        return s;
    }

    inline bool isBlackKey (int midiNote) noexcept
    {
        switch (((midiNote % 12) + 12) % 12)
        {
            case 1: case 3: case 6: case 8: case 10: return true;
            default: return false;
        }
    }
}
