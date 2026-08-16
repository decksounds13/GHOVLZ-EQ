#pragma once

#include <JuceHeader.h>

class EqProcessor;

/**
    Functionality (EQ / APVTS / A-B) presets — separate from Appearance UI colour themes.
    Stored under Documents/Decksounds/GhovlzDyn/Presets/eq_presets.xml
*/
class EqPresetStore
{
public:
    explicit EqPresetStore (EqProcessor& processorToUse);

    int getNumPresets() const noexcept { return names.size(); }
    int getSelectedIndex() const noexcept { return selectedIndex; }
    juce::String getName (int index) const;
    juce::String getSelectedName() const;
    int indexOfName (const juce::String& name) const
    {
        const auto trimmed = name.trim();
        if (trimmed.isEmpty())
            return -1;

        for (int i = 0; i < names.size(); ++i)
            if (names[i].equalsIgnoreCase (trimmed))
                return i;

        return -1;
    }
    bool containsName (const juce::String& name) const { return indexOfName (name) >= 0; }

    void apply (int index);
    void cycle (int delta);
    /** Save current plugin state under name (overwrite same name, else append). */
    void saveOrUpdateWithName (const juce::String& name);
    void renameSelected (const juce::String& newName);

    std::function<void()> onChanged;

private:
    juce::ValueTree captureState() const;
    void applyState (const juce::ValueTree& state);
    void ensureDefault();
    void ensureFactoryOtt();
    void loadFromXml();
    void persistToXml() const;
    static void setTreeParam (juce::ValueTree& root, const juce::String& id, float value);
    juce::ValueTree makeOttState (bool xferSplits) const;
    juce::ValueTree makeOttStateN (int bands, const float* splits, int numSplits) const;
    static juce::File getPresetFile();

    EqProcessor& processor;
    juce::StringArray names;
    juce::Array<juce::ValueTree> states;
    int selectedIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqPresetStore)
};
