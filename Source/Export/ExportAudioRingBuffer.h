#pragma once

#include <JuceHeader.h>
#include <vector>
#include <atomic>

/**
    Continuous stereo capture of plugin main-bus output for offline video export.
    Audio thread: push. Message / export thread: snapshot last N seconds.
*/
class ExportAudioRingBuffer
{
public:
    static constexpr double kDefaultCapacitySec = 300.0; // match Spec3D max timeline

    void prepare (double sampleRate, double capacitySec = kDefaultCapacitySec)
    {
        const double sr = sampleRate > 1.0 ? sampleRate : 48000.0;
        sampleRateHz.store (sr, std::memory_order_relaxed);
        const int n = juce::jmax (1024, (int) std::ceil (sr * capacitySec) * 2); // interleaved stereo
        {
            const juce::SpinLock::ScopedLockType sl (lock);
            buffer.assign ((size_t) n, 0.0f);
            writeIndex = 0;
            filled = 0;
        }
        capacitySamples.store (n / 2, std::memory_order_release); // frames
    }

    void reset() noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        writeIndex = 0;
        filled = 0;
        std::fill (buffer.begin(), buffer.end(), 0.0f);
    }

    /** Audio thread: append interleaved L,R samples (numFrames). */
    void push (const float* left, const float* right, int numFrames) noexcept
    {
        if (numFrames <= 0 || left == nullptr)
            return;

        const juce::SpinLock::ScopedTryLockType sl (lock);
        if (! sl.isLocked() || buffer.empty())
            return;

        const int capFrames = (int) (buffer.size() / 2);
        if (capFrames <= 0)
            return;

        for (int i = 0; i < numFrames; ++i)
        {
            const float l = left[i];
            const float r = right != nullptr ? right[i] : l;
            const size_t base = (size_t) writeIndex * 2u;
            buffer[base] = l;
            buffer[base + 1] = r;
            writeIndex = (writeIndex + 1) % capFrames;
            filled = juce::jmin (capFrames, filled + 1);
        }
    }

    double getSampleRate() const noexcept
    {
        return sampleRateHz.load (std::memory_order_relaxed);
    }

    /** Frames currently available (stereo). */
    int getAvailableFrames() const noexcept
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        return filled;
    }

    /**
        Copy the most recent durationSec of stereo audio into dest (interleaved).
        Returns number of frames written. Pads with silence if not enough history.
    */
    int copyRecent (double durationSec, std::vector<float>& destInterleaved) const
    {
        const double sr = getSampleRate();
        const int want = juce::jmax (1, (int) std::ceil (durationSec * sr));
        destInterleaved.assign ((size_t) want * 2u, 0.0f);

        const juce::SpinLock::ScopedLockType sl (lock);
        if (buffer.empty() || filled <= 0)
            return want;

        const int capFrames = (int) (buffer.size() / 2);
        const int n = juce::jmin (want, filled);
        // Oldest of the recent window starts n frames before writeIndex.
        int read = writeIndex - n;
        if (read < 0)
            read += capFrames;

        const int silencePad = want - n;
        for (int i = 0; i < n; ++i)
        {
            const size_t src = (size_t) ((read + i) % capFrames) * 2u;
            const size_t dst = (size_t) (silencePad + i) * 2u;
            destInterleaved[dst] = buffer[src];
            destInterleaved[dst + 1] = buffer[src + 1];
        }
        return want;
    }

private:
    mutable juce::SpinLock lock;
    std::vector<float> buffer;
    int writeIndex = 0;
    int filled = 0;
    std::atomic<double> sampleRateHz { 48000.0 };
    std::atomic<int> capacitySamples { 0 };
};
