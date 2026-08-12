#pragma once

#include <JuceHeader.h>
#include <vector>

/** Enableable Scope strip/quad modules (stable IDs for prefs). */
enum class ScopeModuleId : uint8_t
{
    levelIn = 0,       // Level Meter 1 (default Input)
    spectrogram = 1,   // Spectrograph
    spectrum = 2,      // Analyzer (FFT / spectrum)
    goniometer = 3,
    oscilloscope = 4,
    stereogram = 5,
    loudness = 6,
    levelOut = 7,      // Level Meter 2 (default Output)
    histogram = 8,
    spectrogram3D = 9, // OpenGL heightfield (optional; strip + tiled)
    thd = 10,          // Broadband spectral THD (optional)
    numModules
};

namespace ScopeModules
{
inline constexpr int kNumModules = (int) ScopeModuleId::numModules;

inline const char* idToKey (ScopeModuleId id) noexcept
{
    switch (id)
    {
        case ScopeModuleId::levelIn:      return "LevelIn";
        case ScopeModuleId::spectrogram:  return "Spectrogram";
        case ScopeModuleId::spectrum:     return "Spectrum";
        case ScopeModuleId::goniometer:   return "Goniometer";
        case ScopeModuleId::oscilloscope: return "Oscilloscope";
        case ScopeModuleId::stereogram:   return "Stereogram";
        case ScopeModuleId::loudness:     return "Loudness";
        case ScopeModuleId::levelOut:     return "LevelOut";
        case ScopeModuleId::histogram:     return "Histogram";
        case ScopeModuleId::spectrogram3D: return "Spectrogram3D";
        case ScopeModuleId::thd:           return "Thd";
        default:                           return "Unknown";
    }
}

inline juce::String idToLabel (ScopeModuleId id)
{
    switch (id)
    {
        case ScopeModuleId::levelIn:       return "Level Meter 1";
        case ScopeModuleId::spectrogram:   return "Spectrograph";
        case ScopeModuleId::spectrum:      return "Analyzer";
        case ScopeModuleId::goniometer:    return "Goniometer";
        case ScopeModuleId::oscilloscope:  return "Oscilloscope";
        case ScopeModuleId::stereogram:    return "Stereogram";
        case ScopeModuleId::loudness:      return "Loudness";
        case ScopeModuleId::levelOut:      return "Level Meter 2 (Output)";
        case ScopeModuleId::histogram:     return "Histogram";
        case ScopeModuleId::spectrogram3D: return "Spectrogram 3D";
        case ScopeModuleId::thd:           return "THD";
        default:                           return "Module";
    }
}

inline ScopeModuleId keyToId (const juce::String& key) noexcept
{
    if (key == "LevelIn")       return ScopeModuleId::levelIn;
    if (key == "Spectrogram")   return ScopeModuleId::spectrogram;
    if (key == "Spectrum")      return ScopeModuleId::spectrum;
    if (key == "Goniometer")    return ScopeModuleId::goniometer;
    if (key == "Oscilloscope")  return ScopeModuleId::oscilloscope;
    if (key == "Stereogram")    return ScopeModuleId::stereogram;
    if (key == "Loudness")      return ScopeModuleId::loudness;
    if (key == "LevelOut")      return ScopeModuleId::levelOut;
    if (key == "Histogram")     return ScopeModuleId::histogram;
    if (key == "Spectrogram3D") return ScopeModuleId::spectrogram3D;
    if (key == "Thd" || key == "THD") return ScopeModuleId::thd;
    return ScopeModuleId::spectrum;
}

/** Default enabled order for Scope strip. */
inline std::vector<ScopeModuleId> defaultEnabledOrder()
{
    return {
        ScopeModuleId::levelIn,
        ScopeModuleId::spectrogram,
        ScopeModuleId::spectrum,
        ScopeModuleId::goniometer,
        ScopeModuleId::loudness,
        ScopeModuleId::levelOut
    };
}

inline juce::String orderToString (const std::vector<ScopeModuleId>& order)
{
    juce::StringArray parts;
    for (auto id : order)
        parts.add (idToKey (id));
    return parts.joinIntoString (",");
}

inline std::vector<ScopeModuleId> orderFromString (const juce::String& s)
{
    auto parts = juce::StringArray::fromTokens (s, ",", {});
    std::vector<ScopeModuleId> out;
    std::array<bool, kNumModules> seen {};
    for (const auto& p : parts)
    {
        const auto id = keyToId (p.trim());
        const int idx = (int) id;
        if (idx < 0 || idx >= kNumModules || seen[(size_t) idx])
            continue;
        seen[(size_t) idx] = true;
        out.push_back (id);
    }
    if (out.empty())
        return defaultEnabledOrder();
    return out;
}
} // namespace ScopeModules
