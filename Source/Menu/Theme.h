#pragma once
#include <JuceHeader.h>
#include "SharedResources.h"
#include <cstdint>

class Theme {
public:
    // Default constructor
    Theme();

    // Constructor that accepts shared colors
    Theme(const SharedColors& colors);

    // Constructor that accepts presetName, createdTime, modifiedTime, and colors
    Theme(const juce::String& presetName, const juce::Time& createdTime, const juce::Time& modifiedTime, const SharedColors& colors);

    // Function to convert Theme to XML
    juce::XmlElement* toXml() const;

    // Function to initialize Theme from XML
    void fromXml(const juce::XmlElement& xmlElement);

    // Accessor for colors
    const SharedColors& getColors() const { return colors; }

    /** Full plugin APVTS / host-style state (bands, analyser prefs, A/B snapshots). */
    const juce::ValueTree& getPluginState() const noexcept { return pluginState; }
    bool hasPluginState() const noexcept { return pluginState.isValid(); }
    void setPluginState (const juce::ValueTree& state) { pluginState = state; }
    void clearPluginState() { pluginState = {}; }

    void savePresetsToXML(juce::Array<Theme>& themes,
        const juce::StringArray& presetNames,
        const juce::Array<juce::Time>& themeCreatedTimes,
        const juce::Array<juce::Time>& themeModifiedTimes);


    // Function to update the modified timestamp
    void setModified(const juce::Time& newTime) {
        modified = newTime;
    }

    // Function to update the colors
    void setColors(const SharedColors& newColors) {
        colors = newColors;
    }

    void setCreated(const juce::Time& newTime) {
        created = newTime;
    }


    // Accessors for name, created, and modified
    juce::String getName() const { return name; }
    juce::Time getCreated() const { return created; }
    juce::Time getModified() const { return modified; }

private:
    SharedColors colors;
    juce::Time created;
    juce::Time modified;
    juce::String name;
    juce::ValueTree pluginState;

    // Helper function to get color attribute names
    juce::StringArray getColorAttributeNames() const;

    // Helper function to set colors by name
    void setColorFromName(const juce::String& colorName, const juce::Colour& color);
};
