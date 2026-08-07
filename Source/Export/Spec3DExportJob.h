#pragma once

#include "Spec3DExportSandbox.h"
#include "Spec3DExportSettings.h"
#if SPEC3D_EXPORT_ENABLED
#include "ExportAudioRingBuffer.h"
#endif
#include <functional>

class Spectrogram3DComponent;
class ExportAudioRingBuffer;

/**
    Runs offline Spec3D region export on a background thread:
    seek playhead -> capture GL frame -> encode MP4 (Windows MF) + mux DAW audio.

    When SPEC3D_EXPORT_ENABLED is 0, run() reports sandboxed and returns
    (code path retained for later standalone / re-enable).
*/
class Spec3DExportJob : public juce::ThreadWithProgressWindow
{
public:
#if SPEC3D_EXPORT_ENABLED
    Spec3DExportJob (Spectrogram3DComponent& spec3d,
                     ExportAudioRingBuffer& audioRing,
                     Spec3DExportSettings settings);
#else
    /** Sandbox ctor — audio ring not required while export is disabled. */
    Spec3DExportJob (Spectrogram3DComponent& spec3d,
                     Spec3DExportSettings settings);
#endif

    void run() override;

    std::function<void (bool success, juce::String message)> onFinished;

private:
    Spectrogram3DComponent& spec3d;
#if SPEC3D_EXPORT_ENABLED
    ExportAudioRingBuffer& audioRing;
#endif
    Spec3DExportSettings settings;
};
