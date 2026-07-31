#pragma once

#include "GradientRamp.h"
#include "RampPresetStore.h"

class SharedColors;

/** Three metering targets for path-sampled (or edited) colour ramps. */
class ColourRampBank : public juce::ChangeBroadcaster
{
public:
    enum class Target
    {
        fftBars = 0,
        spectrogram,
        spectrumFill,
        numTargets
    };

    ColourRampBank();

    static juce::String targetName (Target t);
    static Target clampTarget (int idx) noexcept;

    GradientRamp& get (Target t) noexcept;
    const GradientRamp& get (Target t) const noexcept;

    /** True when a row is highlighted as the path-sample destination. */
    bool hasActiveTarget() const noexcept { return activeTarget >= 0; }
    Target getActiveTarget() const noexcept;
    void setActiveTarget (Target t) noexcept;
    void clearActiveTarget() noexcept;

    void setRamp (Target t, GradientRamp ramp);
    /** Persist + notify listeners (edits that should hit disk). */
    void notifyEdited();
    /** Live preview only — no disk write (colour-picker drag). */
    void notifyPreview();

    /**
        2–10 random poles using Appearance H/S/V limits; enables Use.
        Optional per-target mask (fft / spectrogram / fill). Null = all three.
    */
    void randomizeRamps (const SharedColors& colours,
                         const bool* targetEnabled3 = nullptr);
    /** Turn Use off on all targets so built-in schemes (e.g. Heat) show again. */
    void disableAllCustomRamps();
    /** Turn Use off on one target (e.g. when Colour Scheme combo changes). */
    void disableCustomRamp (Target t);

    void load();
    void save() const;

    RampPresetStore& getPresets() noexcept { return presets; }
    const RampPresetStore& getPresets() const noexcept { return presets; }

    static juce::File getStoreFile();

private:
    void sanitizeMapModes();

    GradientRamp ramps[(int) Target::numTargets];
    int activeTarget = -1; // -1 = none selected (path sample needs an explicit pick)
    RampPresetStore presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ColourRampBank)
};
