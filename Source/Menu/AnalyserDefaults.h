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

    /** Remap legacy {512,1024,2048,4096,8192} BLOCK_ID norms so old "2048"
        (index 2 / 0.5) does not become 8192 on the current 4-choice list. */
    static void migrateBlockIdInState (juce::ValueTree& state);

private:
    juce::StringPairArray values;
    int fileVersion = 1;
};
