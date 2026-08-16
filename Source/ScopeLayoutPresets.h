#pragma once

#include "ScopeModules.h"
#include <JuceHeader.h>
#include <vector>

/** Persisted Scope arrange snapshots (strip vs tiled are separate lists).
    Optionally embeds a ColourRampBank ValueTree snapshot (not RampPresetStore).
    Newer presets store a Viewport BSP tree XML string. */
struct ScopeLayoutPreset
{
    juce::String name;
    bool strip = false;
    std::vector<ScopeModuleId> modules = ScopeModules::defaultEnabledOrder();
    std::vector<float> stripFractions;
    int stripHeightPx = 200;
    float splitX = 0.5f;
    float splitY = 0.5f;
    /** Serialized ScopeViewport::Node (XmlElement "Pane"). Empty = legacy 2x2 / row. */
    juce::String viewportXml;
    /** Factory id (h50, grid2, image, …) when this is a built-in, else empty. */
    juce::String factoryId;
    /** Inline colour-ramp snapshot (ColourRamps ValueTree). Empty = geometry only. */
    juce::ValueTree colourRamps;
};

namespace ScopeLayoutPresets
{
juce::File getStoreFile();

std::vector<ScopeLayoutPreset> loadAll();
std::vector<ScopeLayoutPreset> loadForMode (bool stripMode);

bool savePreset (const ScopeLayoutPreset& preset); // replaces same name+mode
bool containsName (const juce::String& name, bool stripMode);
bool deletePreset (const juce::String& name, bool stripMode);

juce::String encodeFractions (const std::vector<float>& fracs);
std::vector<float> decodeFractions (const juce::String& s, int expectedCount);
} // namespace ScopeLayoutPresets
