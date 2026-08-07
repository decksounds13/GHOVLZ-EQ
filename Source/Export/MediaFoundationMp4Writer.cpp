#include "MediaFoundationMp4Writer.h"
#include "Spec3DExportSandbox.h"

// Full encoder body is only built when export is enabled (avoids plugin link/compile cost).
#if JUCE_WINDOWS && SPEC3D_EXPORT_ENABLED

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <wrl/client.h>

#pragma comment (lib, "mfplat.lib")
#pragma comment (lib, "mfreadwrite.lib")
#pragma comment (lib, "mfuuid.lib")

using Microsoft::WRL::ComPtr;

namespace
{
    UINT32 bitrateForQuality (int quality, int w, int h, double fps)
    {
        const double pixels = (double) w * (double) h;
        const double base = pixels * fps * 0.07; // rough bpp
        const double mult = quality <= 0 ? 0.55 : (quality >= 2 ? 1.6 : 1.0);
        return (UINT32) juce::jlimit (1'000'000.0, 80'000'000.0, base * mult);
    }
}

bool MediaFoundationMp4Writer::open (const juce::File& file,
                                     int w, int h, double framesPerSec,
                                     double audioSampleRate, int quality)
{
    close();
    width = juce::jmax (16, w);
    height = juce::jmax (16, h);
    // H.264 likes even dimensions
    width &= ~1;
    height &= ~1;
    fps = framesPerSec > 1.0 ? framesPerSec : 30.0;
    audioSr = audioSampleRate > 1.0 ? audioSampleRate : 48000.0;
    videoDurationHns = (int64_t) (10'000'000.0 / fps);

    HRESULT hr = MFStartup (MF_VERSION);
    if (FAILED (hr))
    {
        lastError = "MFStartup failed";
        return false;
    }

    ComPtr<IMFSinkWriter> writer;
    const auto path = file.getFullPathName();
    hr = MFCreateSinkWriterFromURL (path.toWideCharPointer(), nullptr, nullptr, &writer);
    if (FAILED (hr))
    {
        lastError = "MFCreateSinkWriterFromURL failed";
        MFShutdown();
        return false;
    }

    // ---- Video out (H.264) ----
    ComPtr<IMFMediaType> videoOut;
    hr = MFCreateMediaType (&videoOut);
    if (FAILED (hr)) { lastError = "videoOut type"; return false; }
    videoOut->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoOut->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_H264);
    videoOut->SetUINT32 (MF_MT_AVG_BITRATE, bitrateForQuality (quality, width, height, fps));
    videoOut->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize (videoOut.Get(), MF_MT_FRAME_SIZE, (UINT32) width, (UINT32) height);
    MFSetAttributeRatio (videoOut.Get(), MF_MT_FRAME_RATE, (UINT32) juce::roundToInt (fps * 1000.0), 1000);
    MFSetAttributeRatio (videoOut.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);

    DWORD videoIdx = 0;
    hr = writer->AddStream (videoOut.Get(), &videoIdx);
    videoStreamIndex = (uint32_t) videoIdx;
    if (FAILED (hr))
    {
        lastError = "AddStream video failed";
        return false;
    }

    // Video input RGB32
    ComPtr<IMFMediaType> videoIn;
    hr = MFCreateMediaType (&videoIn);
    if (FAILED (hr)) { lastError = "videoIn type"; return false; }
    videoIn->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Video);
    videoIn->SetGUID (MF_MT_SUBTYPE, MFVideoFormat_RGB32);
    videoIn->SetUINT32 (MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
    MFSetAttributeSize (videoIn.Get(), MF_MT_FRAME_SIZE, (UINT32) width, (UINT32) height);
    MFSetAttributeRatio (videoIn.Get(), MF_MT_FRAME_RATE, (UINT32) juce::roundToInt (fps * 1000.0), 1000);
    MFSetAttributeRatio (videoIn.Get(), MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
    videoIn->SetUINT32 (MF_MT_DEFAULT_STRIDE, (UINT32) (width * 4));

    hr = writer->SetInputMediaType ((DWORD) videoStreamIndex, videoIn.Get(), nullptr);
    if (FAILED (hr))
    {
        lastError = "SetInputMediaType video failed";
        return false;
    }

    // ---- Audio out (AAC) ----
    hasAudio = audioSr > 1.0;
    if (hasAudio)
    {
        ComPtr<IMFMediaType> audioOut;
        hr = MFCreateMediaType (&audioOut);
        if (FAILED (hr)) { lastError = "audioOut type"; return false; }
        audioOut->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
        audioOut->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_AAC);
        audioOut->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
        audioOut->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) juce::roundToInt (audioSr));
        audioOut->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
        audioOut->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND, 24000); // ~192 kbps

        DWORD audioIdx = 0;
        hr = writer->AddStream (audioOut.Get(), &audioIdx);
        audioStreamIndex = (uint32_t) audioIdx;
        if (FAILED (hr))
        {
            // Fall back to video-only rather than fail the whole export.
            hasAudio = false;
            lastError = "AddStream audio failed (continuing video-only)";
        }
        else
        {
            ComPtr<IMFMediaType> audioIn;
            hr = MFCreateMediaType (&audioIn);
            if (SUCCEEDED (hr))
            {
                audioIn->SetGUID (MF_MT_MAJOR_TYPE, MFMediaType_Audio);
                audioIn->SetGUID (MF_MT_SUBTYPE, MFAudioFormat_PCM);
                audioIn->SetUINT32 (MF_MT_AUDIO_BITS_PER_SAMPLE, 16);
                audioIn->SetUINT32 (MF_MT_AUDIO_SAMPLES_PER_SECOND, (UINT32) juce::roundToInt (audioSr));
                audioIn->SetUINT32 (MF_MT_AUDIO_NUM_CHANNELS, 2);
                audioIn->SetUINT32 (MF_MT_AUDIO_BLOCK_ALIGNMENT, 4);
                audioIn->SetUINT32 (MF_MT_AUDIO_AVG_BYTES_PER_SECOND,
                                    (UINT32) juce::roundToInt (audioSr) * 4);
                hr = writer->SetInputMediaType ((DWORD) audioStreamIndex, audioIn.Get(), nullptr);
                if (FAILED (hr))
                {
                    hasAudio = false;
                    lastError = "SetInputMediaType audio failed (video-only)";
                }
            }
        }
    }

    hr = writer->BeginWriting();
    if (FAILED (hr))
    {
        lastError = "BeginWriting failed";
        return false;
    }

    sinkWriter = writer.Detach();
    openOk = true;
    return true;
}

bool MediaFoundationMp4Writer::writeVideoFrame (const juce::Image& argbTopDown, int64_t frameIndex)
{
    if (! openOk || sinkWriter == nullptr)
        return false;

    auto* writer = static_cast<IMFSinkWriter*> (sinkWriter);
    const int w = width;
    const int h = height;
    const DWORD cbBuffer = (DWORD) (w * h * 4);

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer (cbBuffer, &buffer);
    if (FAILED (hr))
        return false;

    BYTE* data = nullptr;
    hr = buffer->Lock (&data, nullptr, nullptr);
    if (FAILED (hr))
        return false;

    juce::Image img = argbTopDown;
    if (! img.isValid() || img.getWidth() != w || img.getHeight() != h)
        img = img.rescaled (w, h);

    // MF RGB32 is typically BGRA bottom-up for some paths; use top-down with negative stride
    // by flipping rows into BGRA.
    {
        juce::Image::BitmapData bd (img, juce::Image::BitmapData::readOnly);
        for (int y = 0; y < h; ++y)
        {
            const auto* src = reinterpret_cast<const juce::PixelARGB*> (bd.getLinePointer (y));
            auto* dst = reinterpret_cast<uint8_t*> (data + (size_t) y * (size_t) w * 4u);
            for (int x = 0; x < w; ++x)
            {
                const auto p = src[x];
                dst[0] = p.getBlue();
                dst[1] = p.getGreen();
                dst[2] = p.getRed();
                dst[3] = 255;
                dst += 4;
            }
        }
    }

    buffer->Unlock();
    buffer->SetCurrentLength (cbBuffer);

    ComPtr<IMFSample> sample;
    hr = MFCreateSample (&sample);
    if (FAILED (hr))
        return false;
    sample->AddBuffer (buffer.Get());
    sample->SetSampleTime (frameIndex * videoDurationHns);
    sample->SetSampleDuration (videoDurationHns);

    hr = writer->WriteSample ((DWORD) videoStreamIndex, sample.Get());
    return SUCCEEDED (hr);
}

bool MediaFoundationMp4Writer::writeAudioBlock (const float* interleavedStereo, int numFrames, int64_t sampleOffset)
{
    if (! openOk || ! hasAudio || sinkWriter == nullptr || interleavedStereo == nullptr || numFrames <= 0)
        return ! hasAudio; // treat as success if no audio stream

    auto* writer = static_cast<IMFSinkWriter*> (sinkWriter);
    const DWORD cbBuffer = (DWORD) (numFrames * 4); // 16-bit stereo

    ComPtr<IMFMediaBuffer> buffer;
    HRESULT hr = MFCreateMemoryBuffer (cbBuffer, &buffer);
    if (FAILED (hr))
        return false;

    BYTE* data = nullptr;
    hr = buffer->Lock (&data, nullptr, nullptr);
    if (FAILED (hr))
        return false;

    auto* dst = reinterpret_cast<int16_t*> (data);
    for (int i = 0; i < numFrames * 2; ++i)
    {
        const float s = juce::jlimit (-1.0f, 1.0f, interleavedStereo[i]);
        dst[i] = (int16_t) juce::roundToInt (s * 32767.0f);
    }

    buffer->Unlock();
    buffer->SetCurrentLength (cbBuffer);

    ComPtr<IMFSample> sample;
    hr = MFCreateSample (&sample);
    if (FAILED (hr))
        return false;
    sample->AddBuffer (buffer.Get());

    const int64_t hns = (int64_t) ((double) sampleOffset * 10'000'000.0 / audioSr);
    const int64_t dur = (int64_t) ((double) numFrames * 10'000'000.0 / audioSr);
    sample->SetSampleTime (hns);
    sample->SetSampleDuration (dur);

    hr = writer->WriteSample ((DWORD) audioStreamIndex, sample.Get());
    return SUCCEEDED (hr);
}

bool MediaFoundationMp4Writer::close()
{
    bool ok = true;
    if (sinkWriter != nullptr)
    {
        auto* writer = static_cast<IMFSinkWriter*> (sinkWriter);
        const HRESULT hr = writer->Finalize();
        ok = SUCCEEDED (hr);
        writer->Release();
        sinkWriter = nullptr;
        MFShutdown();
    }
    openOk = false;
    hasAudio = false;
    return ok;
}

#else // ! (JUCE_WINDOWS && SPEC3D_EXPORT_ENABLED)

// Sandboxed stubs — full encoder retained above for later standalone / re-enable.
bool MediaFoundationMp4Writer::open (const juce::File&, int, int, double, double, int)
{
    lastError = "Spec3D export is sandboxed (SPEC3D_EXPORT_ENABLED=0).";
    return false;
}
bool MediaFoundationMp4Writer::writeVideoFrame (const juce::Image&, int64_t) { return false; }
bool MediaFoundationMp4Writer::writeAudioBlock (const float*, int, int64_t) { return false; }
bool MediaFoundationMp4Writer::close() { openOk = false; return true; }

#endif
