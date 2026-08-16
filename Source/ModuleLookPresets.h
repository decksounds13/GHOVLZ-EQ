#pragma once

#include <JuceHeader.h>
#include <vector>

/**
    Per-module look / UI settings (not EQ DSP).

    Documents/Decksounds/GhovlzDyn/ModuleLook/<Kind>/
      default.xml  — "Save as default"
      presets.xml  — named presets

    Capture/apply lives in MainComponent; this only persists ValueTrees.
*/
namespace ModuleLookPresets
{
enum class Kind
{
    oscilloscope = 0,
    goniometer,
    spectrogram,
    spectrogram3D,
    spectrum,
    fft,
    histogram,
    stereogram,
    loudness,
    levelMeters,
    numKinds
};

const char* kindFolder (Kind k) noexcept;
const char* kindDisplayName (Kind k) noexcept;

juce::File kindDirectory (Kind k);
juce::File defaultsFile (Kind k);
juce::File libraryFile (Kind k);

bool saveDefault (Kind k, const juce::ValueTree& state);
juce::ValueTree loadDefault (Kind k);

bool saveNamed (Kind k, juce::String name, const juce::ValueTree& state);
bool containsName (Kind k, const juce::String& name);
juce::ValueTree loadNamed (Kind k, const juce::String& name);
bool deleteNamed (Kind k, const juce::String& name);
std::vector<juce::String> listNames (Kind k);

juce::StringArray parameterIdsForKind (Kind k);

juce::ValueTree captureFromApvts (Kind k, juce::AudioProcessorValueTreeState& treeState);
void applyToApvts (Kind k, juce::AudioProcessorValueTreeState& treeState, const juce::ValueTree& look);

/** Module default.xml + merge those IDs into analyser_defaults.xml. */
bool saveDefaultFromApvts (Kind k, juce::AudioProcessorValueTreeState& treeState);
} // namespace ModuleLookPresets
