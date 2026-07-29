#pragma once

#include <JuceHeader.h>

/**
 * Single source of truth for Appearance UI Elements:
 * display name (list), XML attribute (theme presets), and SharedColors member.
 *
 * Prefixes group related colours when sorting A–Z (Menu, Plugin, Graph, …).
 */
class SharedColors;

namespace ThemeColorRegistry
{
    struct Entry
    {
        const char* displayName;
        const char* xmlAttr;
        juce::Colour SharedColors::* member;
    };

    /** Entries are defined in SharedResources.cpp after SharedColors members exist. */
    const Entry* getEntries() noexcept;
    int getNumEntries() noexcept;

    inline int indexForDisplayName (const juce::String& name) noexcept
    {
        const auto* entries = getEntries();
        const int n = getNumEntries();
        for (int i = 0; i < n; ++i)
            if (name == entries[i].displayName)
                return i;
        return -1;
    }
}
