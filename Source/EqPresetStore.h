#pragma once

#include <JuceHeader.h>

class EqProcessor;

/**
    Functionality (EQ / APVTS / A-B) presets — separate from Appearance UI colour themes.
    Stored under Documents/Decksounds/ParametricEq/Presets/eq_presets.xml
*/
class EqPresetStore
{
public:
    explicit EqPresetStore (EqProcessor& processorToUse);

    int getNumPresets() const noexcept { return names.size(); }
    int getSelectedIndex() const noexcept { return selectedIndex; }
    juce::String getName (int index) const;
    juce::String getSelectedName() const;

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
    void loadFromXml();
    void persistToXml() const;
    static juce::File getPresetFile();

    EqProcessor& processor;
    juce::StringArray names;
    juce::Array<juce::ValueTree> states;
    int selectedIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqPresetStore)
};
