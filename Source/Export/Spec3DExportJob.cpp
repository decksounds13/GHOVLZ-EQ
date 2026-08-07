#include "Spec3DExportJob.h"
#include "Spec3DExportSandbox.h"
#include "MediaFoundationMp4Writer.h"
#include "../Spectrogram3DComponent.h"

#if SPEC3D_EXPORT_ENABLED
Spec3DExportJob::Spec3DExportJob (Spectrogram3DComponent& s,
                                  ExportAudioRingBuffer& audio,
                                  Spec3DExportSettings st)
    : juce::ThreadWithProgressWindow ("Exporting Spec3D region offline...", true, true),
      spec3d (s),
      audioRing (audio),
      settings (std::move (st))
{
    setStatusMessage ("Preparing export...");
}
#else
Spec3DExportJob::Spec3DExportJob (Spectrogram3DComponent& s,
                                  Spec3DExportSettings st)
    : juce::ThreadWithProgressWindow ("Exporting Spec3D region offline...", true, true),
      spec3d (s),
      settings (std::move (st))
{
    setStatusMessage ("Export sandboxed...");
}
#endif

void Spec3DExportJob::run()
{
#if ! SPEC3D_EXPORT_ENABLED
    juce::ignoreUnused (spec3d);
    if (onFinished)
        onFinished (false,
                    "Spec3D offline export is sandboxed for now "
                    "(likely a standalone feature later).\n"
                    "Set SPEC3D_EXPORT_ENABLED to 1 in Spec3DExportSandbox.h to re-enable.");
    return;
#else
    const float t0 = settings.startSec;
    const float t1 = settings.endSec;
    const float dur = juce::jmax (0.001f, t1 - t0);
    const int nFrames = settings.numVideoFrames();
    const double fps = settings.fps > 1.0 ? settings.fps : 30.0;

    if (settings.outputFile == juce::File())
    {
        if (onFinished)
            onFinished (false, "No output file.");
        return;
    }

    settings.outputFile.deleteFile();

    // Snapshot DAW audio for the region length (most recent ring contents).
    std::vector<float> audioInterleaved;
    const int audioFrames = audioRing.copyRecent ((double) dur, audioInterleaved);
    const double audioSr = audioRing.getSampleRate();

#if JUCE_WINDOWS
    MediaFoundationMp4Writer writer;
    if (! writer.open (settings.outputFile,
                       settings.width, settings.height, fps,
                       settings.includeAudio ? audioSr : 0.0,
                       settings.quality))
    {
        if (onFinished)
            onFinished (false, "Could not open encoder: " + writer.getLastError());
        return;
    }

    // Write audio first (optional) so A/V stay aligned in simple muxers.
    if (settings.includeAudio && audioFrames > 0 && ! audioInterleaved.empty())
    {
        setStatusMessage ("Writing audio...");
        // Chunk to ~0.25 s blocks for smoother cancel.
        const int block = juce::jmax (256, (int) (audioSr * 0.25));
        int offset = 0;
        while (offset < audioFrames && ! threadShouldExit())
        {
            const int n = juce::jmin (block, audioFrames - offset);
            if (! writer.writeAudioBlock (audioInterleaved.data() + (size_t) offset * 2u, n, offset))
            {
                // non-fatal; continue video
                break;
            }
            offset += n;
        }
    }

    for (int i = 0; i < nFrames; ++i)
    {
        if (threadShouldExit())
        {
            writer.close();
            settings.outputFile.deleteFile();
            if (onFinished)
                onFinished (false, "Export cancelled.");
            return;
        }

        const float t = t0 + (float) ((double) i / fps);
        setStatusMessage ("Frame " + juce::String (i + 1) + " / " + juce::String (nFrames)
                          + "  (" + juce::String (t, 2) + "s)");
        setProgress ((double) i / (double) juce::jmax (1, nFrames));

        // Seek morph on message thread (safe for component state).
        {
            juce::WaitableEvent done;
            juce::MessageManager::callAsync (
                [this, t, &done]
                {
                    spec3d.setRampTimelinePlayheadSec (t);
                    done.signal();
                });
            done.wait (-1);
        }

        juce::Image frame = spec3d.captureExportFrame (settings.width, settings.height);
        if (! frame.isValid())
        {
            writer.close();
            if (onFinished)
                onFinished (false,
                            "GL capture failed. Keep Spec3D visible (soft path) and try again.");
            return;
        }

        if (! writer.writeVideoFrame (frame, i))
        {
            writer.close();
            if (onFinished)
                onFinished (false, "Failed writing video frame " + juce::String (i));
            return;
        }
    }

    setStatusMessage ("Finalizing...");
    if (! writer.close())
    {
        if (onFinished)
            onFinished (false, "Finalize failed: " + writer.getLastError());
        return;
    }

    juce::String msg = "Exported " + juce::String (nFrames) + " frames to:\n"
                       + settings.outputFile.getFullPathName();
    if (settings.includeAudio)
    {
        const int avail = audioRing.getAvailableFrames();
        if (avail < audioFrames / 2)
            msg += "\n\nNote: audio history was shorter than the region; "
                   "leading silence may be present. Play audio through the "
                   "region before export for a full buffer.";
    }

    if (onFinished)
        onFinished (true, msg);
#else // !JUCE_WINDOWS
    juce::ignoreUnused (audioFrames, audioSr);
    if (onFinished)
        onFinished (false, "MP4 export requires Windows Media Foundation.");
#endif // JUCE_WINDOWS
#endif // SPEC3D_EXPORT_ENABLED
}
