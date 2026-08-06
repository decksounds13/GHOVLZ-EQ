#pragma once

#include <JuceHeader.h>

/** Persists Spectrum / FFT / EQ-display appearance defaults (like theme presets). */
class AnalyserDefaults
{
public:
    /** Load saved defaults from Documents/Decksounds/ParametricEq/analyser_defaults.xml */
    static AnalyserDefaults load();

    /** Write current APVTS values for analyser appearance params. */
    static bool saveFrom (juce::AudioProcessorValueTreeState& treeState);

    /** Merge only these IDs into analyser_defaults.xml (keeps other modules). */
    static bool mergeIdsFrom (juce::AudioProcessorValueTreeState& treeState,
                              const juce::StringArray& ids);

    float getFloat (const juce::String& id, float fallback) const;
    bool  getBool  (const juce::String& id, bool fallback) const;
    int   getInt   (const juce::String& id, int fallback) const;

    /** Resolve BLOCK_ID to current choice index (0=2048 … 3=16384).
        Accepts size names ("2048") and migrates legacy v1 indices from the
        old {512,1024,2048,4096,8192} list so saved "2" still means 2048. */
    int getBlockIndex (int fallback = 0) const;

    static juce::File getDefaultsFile();
    static juce::StringArray getParameterIds();

    /** Current choice names for BLOCK_ID (index order). */
    static juce::StringArray getBlockSizeNames();

    /** Write stable blockSizeName + schema onto an APVTS state tree before save. */
    static void stampBlockIdInState (juce::ValueTree& state, int blockIndex);

    /** Repair BLOCK_ID from blockSizeName, and remap legacy/normalised dumps
        so APVTS unnormalised choice indices stay correct (8192 = index 2). */
    static void migrateBlockIdInState (juce::ValueTree& state);

private:
    juce::StringPairArray values;
    int fileVersion = 1;
};
