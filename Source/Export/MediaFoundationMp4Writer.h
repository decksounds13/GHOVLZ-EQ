#pragma once

#include <JuceHeader.h>

/**
    Minimal Windows Media Foundation Sink Writer: H.264 video + AAC stereo audio -> MP4.
    Video frames: juce::Image ARGB (top-down). Audio: interleaved float stereo.

    When SPEC3D_EXPORT_ENABLED is 0, methods are no-op stubs (sandbox). Full body is
    compiled only on Windows with export enabled.
*/
class MediaFoundationMp4Writer
{
public:
    MediaFoundationMp4Writer() = default;
    ~MediaFoundationMp4Writer() { close(); }

    /** quality 0..2 -> bitrate tier. */
    bool open (const juce::File& file,
               int width, int height, double fps,
               double audioSampleRate, int quality);

    bool writeVideoFrame (const juce::Image& argbTopDown, int64_t frameIndex);
    bool writeAudioBlock (const float* interleavedStereo, int numFrames, int64_t sampleOffset);

    bool close();
    bool isOpen() const noexcept { return openOk; }
    juce::String getLastError() const { return lastError; }

private:
    bool openOk = false;
    int width = 0, height = 0;
    double fps = 30.0;
    double audioSr = 48000.0;
    int64_t videoDurationHns = 0; // 100-ns units per frame
    juce::String lastError;

    void* sinkWriter = nullptr;   // IMFSinkWriter* (opaque; defined in .cpp)
    /** MF stream indices (DWORD on Windows; stored as uint32_t — no windows.h in header). */
    uint32_t videoStreamIndex = 0;
    uint32_t audioStreamIndex = 0;
    bool hasAudio = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MediaFoundationMp4Writer)
};
