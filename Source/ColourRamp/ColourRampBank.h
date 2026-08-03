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
        oscilloscope,
        goniometer,
        stereogram,
        histogram,
        spectrogram3D, // appended — keep prior ordinals stable for prefs / masks
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
        Replace one ramp with 2–10 random poles (Appearance H/S/V limits) and enable Use.
        varyAlpha: spectrum-fill style translucent stops.
    */
    static void randomizeRamp (GradientRamp& ramp,
                               const SharedColors& colours,
                               bool varyAlpha = false);

    /**
        2–10 random poles using Appearance H/S/V limits; enables Use.
        Optional per-target mask; maskCount limits how many entries are read (legacy dice uses 3).
        Null mask = all targets.
    */
    void randomizeRamps (const SharedColors& colours,
                         const bool* targetEnabled = nullptr,
                         int maskCount = 0);
    /** Turn Use off on all targets so built-in schemes (e.g. Heat) show again. */
    void disableAllCustomRamps();
    /** Turn Use off on one target (e.g. when Colour Scheme combo changes). */
    void disableCustomRamp (Target t);

    void load();
    void save() const;

    /** Session snapshot (keeps Use/enabled flags). Disk load forces Use off. */
    juce::ValueTree toValueTree() const;
    void applyFromValueTree (const juce::ValueTree& tree, bool forceCustomRampsOff);

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
