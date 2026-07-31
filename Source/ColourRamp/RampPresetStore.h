#pragma once

#include "GradientRamp.h"

/** Plugin-wide named colour-ramp presets (factory + user). */
class RampPresetStore : public juce::ChangeBroadcaster
{
public:
    struct Preset
    {
        juce::String name;
        GradientRamp ramp;
        bool isFactory = false;
    };

    RampPresetStore();

    const juce::Array<Preset>& getPresets() const noexcept { return presets; }
    int size() const noexcept { return presets.size(); }

    bool savePreset (juce::String name, const GradientRamp& ramp);
    bool applyPreset (int index, GradientRamp& dest) const;
    bool renamePreset (int index, juce::String newName);
    bool deletePreset (int index);

    void load();
    void save() const;

    static juce::File getStoreFile();

private:
    void seedFactoryPresets();

    juce::Array<Preset> presets;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RampPresetStore)
};
