#pragma once

#include <JuceHeader.h>

/** Settings for Spec3D offline region export (NLE-style). */
struct Spec3DExportSettings
{
    float startSec = 0.0f;
    float endSec = 1.0f;
    int width = 1920;
    int height = 1080;
    double fps = 30.0;
    /** 0 = Draft, 1 = High, 2 = Maximum (bitrate tier). */
    int quality = 1;
    juce::File outputFile;
    /** Always true per product requirement (DAW audio muxed). */
    bool includeAudio = true;

    float durationSec() const noexcept
    {
        return juce::jmax (0.0f, endSec - startSec);
    }

    int numVideoFrames() const noexcept
    {
        return juce::jmax (1, (int) std::ceil ((double) durationSec() * fps));
    }

    static int widthForPreset (int id) // 1..4
    {
        switch (id)
        {
            case 1:  return 1280;
            case 3:  return 2560;
            case 4:  return 3840;
            default: return 1920;
        }
    }
    static int heightForPreset (int id)
    {
        switch (id)
        {
            case 1:  return 720;
            case 3:  return 1440;
            case 4:  return 2160;
            default: return 1080;
        }
    }
    static double fpsForPreset (int id) // 1..4
    {
        switch (id)
        {
            case 1:  return 24.0;
            case 2:  return 25.0;
            case 4:  return 60.0;
            default: return 30.0;
        }
    }
};
