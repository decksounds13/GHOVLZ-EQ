#include "ModuleLookPresets.h"
#include "Menu/AnalyserDefaults.h"
#include <algorithm>

namespace ModuleLookPresets
{
const char* kindFolder (Kind k) noexcept
{
    switch (k)
    {
        case Kind::oscilloscope:  return "Oscilloscope";
        case Kind::goniometer:    return "Goniometer";
        case Kind::spectrogram:   return "Spectrogram";
        case Kind::spectrogram3D: return "Spectrogram3D";
        case Kind::spectrum:      return "Spectrum";
        case Kind::fft:           return "Fft";
        case Kind::histogram:     return "Histogram";
        case Kind::stereogram:    return "Stereogram";
        case Kind::loudness:      return "Loudness";
        case Kind::levelMeters:   return "LevelMeters";
        default:                  return "Unknown";
    }
}

const char* kindDisplayName (Kind k) noexcept
{
    switch (k)
    {
        case Kind::oscilloscope:  return "Oscilloscope";
        case Kind::goniometer:    return "Goniometer";
        case Kind::spectrogram:   return "Spectrogram";
        case Kind::spectrogram3D: return "3D Spectrogram";
        case Kind::spectrum:      return "Spectrum";
        case Kind::fft:           return "FFT Bars";
        case Kind::histogram:     return "Histogram";
        case Kind::stereogram:    return "Stereogram";
        case Kind::loudness:      return "Loudness";
        case Kind::levelMeters:   return "Level Meters";
        default:                  return "Module";
    }
}

static juce::File rootDir()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("ParametricEq")
        .getChildFile ("ModuleLook");
}

juce::File kindDirectory (Kind k) { return rootDir().getChildFile (kindFolder (k)); }
juce::File defaultsFile (Kind k)  { return kindDirectory (k).getChildFile ("default.xml"); }
juce::File libraryFile (Kind k)   { return kindDirectory (k).getChildFile ("presets.xml"); }

static juce::ValueTree ensureRoot (Kind k, const juce::ValueTree& state)
{
    if (state.isValid() && state.hasType ("ModuleLook"))
    {
        auto t = state.createCopy();
        t.setProperty ("kind", kindFolder (k), nullptr);
        return t;
    }
    juce::ValueTree wrap ("ModuleLook");
    wrap.setProperty ("kind", kindFolder (k), nullptr);
    if (state.isValid())
        wrap.appendChild (state.createCopy(), nullptr);
    return wrap;
}

static bool writeTree (const juce::File& file, const juce::ValueTree& state)
{
    if (! state.isValid())
        return false;
    file.getParentDirectory().createDirectory();
    if (auto xml = state.createXml())
        return xml->writeTo (file);
    return false;
}

bool saveDefault (Kind k, const juce::ValueTree& state)
{
    auto t = ensureRoot (k, state);
    t.setProperty ("role", "default", nullptr);
    t.setProperty ("savedAt", juce::Time::getCurrentTime().toISO8601 (true), nullptr);
    return writeTree (defaultsFile (k), t);
}

juce::ValueTree loadDefault (Kind k)
{
    const auto f = defaultsFile (k);
    if (! f.existsAsFile())
        return {};
    if (auto xml = juce::parseXML (f))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

static std::vector<std::pair<juce::String, juce::ValueTree>> loadEntries (Kind k)
{
    std::vector<std::pair<juce::String, juce::ValueTree>> out;
    if (auto xml = juce::parseXML (libraryFile (k)))
    {
        if (! xml->hasTagName ("ModuleLookPresets"))
            return out;
        for (auto* child : xml->getChildIterator())
        {
            if (! child->hasTagName ("Preset"))
                continue;
            const auto n = child->getStringAttribute ("name");
            if (n.isEmpty())
                continue;
            if (auto* st = child->getChildByName ("ModuleLook"))
                out.emplace_back (n, juce::ValueTree::fromXml (*st));
            else if (child->getFirstChildElement() != nullptr)
                out.emplace_back (n, juce::ValueTree::fromXml (*child->getFirstChildElement()));
        }
    }
    return out;
}

static bool writeEntries (Kind k, const std::vector<std::pair<juce::String, juce::ValueTree>>& entries)
{
    auto root = std::make_unique<juce::XmlElement> ("ModuleLookPresets");
    root->setAttribute ("kind", kindFolder (k));
    for (const auto& e : entries)
    {
        auto* p = root->createNewChildElement ("Preset");
        p->setAttribute ("name", e.first);
        if (e.second.isValid())
            if (auto st = e.second.createXml())
                p->addChildElement (st.release());
    }
    auto f = libraryFile (k);
    f.getParentDirectory().createDirectory();
    return root->writeTo (f);
}

bool saveNamed (Kind k, juce::String name, const juce::ValueTree& state)
{
    name = name.trim();
    if (name.isEmpty())
        name = juce::String (kindDisplayName (k)) + " Look";

    auto entries = loadEntries (k);
    auto body = ensureRoot (k, state);
    body.setProperty ("role", "preset", nullptr);
    body.setProperty ("savedAt", juce::Time::getCurrentTime().toISO8601 (true), nullptr);

    bool replaced = false;
    for (auto& e : entries)
    {
        if (e.first == name)
        {
            e.second = body;
            replaced = true;
            break;
        }
    }
    if (! replaced)
        entries.emplace_back (name, body);
    return writeEntries (k, entries);
}

juce::ValueTree loadNamed (Kind k, const juce::String& name)
{
    for (const auto& e : loadEntries (k))
        if (e.first == name)
            return e.second;
    return {};
}

bool deleteNamed (Kind k, const juce::String& name)
{
    auto entries = loadEntries (k);
    const auto before = entries.size();
    entries.erase (std::remove_if (entries.begin(), entries.end(),
                                   [&] (const auto& e) { return e.first == name; }),
                   entries.end());
    if (entries.size() == before)
        return false;
    return writeEntries (k, entries);
}

std::vector<juce::String> listNames (Kind k)
{
    std::vector<juce::String> out;
    for (const auto& e : loadEntries (k))
        out.push_back (e.first);
    return out;
}

juce::StringArray parameterIdsForKind (Kind k)
{
    switch (k)
    {
        case Kind::oscilloscope:
            return {
                "OSC_USE_RAMP_ID", "OSC_LINE_OPACITY_ID", "OSC_LINE_WIDTH_ID",
                "OSC_GLOW_ENABLE_ID", "OSC_GLOW_RADIUS_ID", "OSC_GLOW_SPREAD_ID", "OSC_GLOW_OPACITY_ID",
                "OSC_EXPANDED_LINE_WIDTH_ID",
                "OSC_EXPANDED_GLOW_ENABLE_ID", "OSC_EXPANDED_GLOW_RADIUS_ID",
                "OSC_EXPANDED_GLOW_SPREAD_ID", "OSC_EXPANDED_GLOW_OPACITY_ID",
                "OSC_QUALITY_ID"
            };
        case Kind::goniometer:
            return {
                "GON_LINE_OPACITY_ID", "GON_LINE_WIDTH_ID",
                "GON_GLOW_ENABLE_ID", "GON_GLOW_RADIUS_ID", "GON_GLOW_SPREAD_ID", "GON_GLOW_OPACITY_ID",
                "GON_EXPANDED_LINE_WIDTH_ID",
                "GON_EXPANDED_GLOW_ENABLE_ID", "GON_EXPANDED_GLOW_RADIUS_ID",
                "GON_EXPANDED_GLOW_SPREAD_ID", "GON_EXPANDED_GLOW_OPACITY_ID",
                "GON_QUALITY_ID", "GON_USE_RAMP_ID"
            };
        case Kind::spectrogram:
            return {
                "SPEC_COLOUR_SCHEME_ID", "SPEC_FFT_SIZE_ID", "SPEC_DISPLAY_RES_ID", "SPEC_CHANNEL_ID",
                "SPEC_SPEED_ID", "SPEC_BRIGHTNESS_ID", "SPEC_MIN_DB_ID", "SPEC_MAX_DB_ID",
                "SPEC_SMOOTH_ID", "SPEC_SOFTEN_ID", "SPEC_LOG_FREQ_ID",
                "SPEC_ENHANCED_FREQ_ID", "SPEC_ENHANCED_STRENGTH_ID",
                "SPEC_ENHANCED_LF_DETAIL_ID", "SPEC_ENHANCED_CROSSOVER_ID", "SPEC_FREEZE_ID"
            };
        case Kind::spectrogram3D:
            return {
                "SPEC_FFT_SIZE_ID", "SPEC_ENHANCED_FREQ_3D_ID",
                "SPEC_ENHANCED_STRENGTH_ID", "SPEC_ENHANCED_LF_DETAIL_ID", "SPEC_ENHANCED_CROSSOVER_ID"
            };
        case Kind::spectrum:
            return {
                "BLOCK_ID", "BINS_ID", "MAX_ID", "LIN_ID", "LOG_ID", "ST_ID", "MAX_HOLD_ID",
                "SPECTRUM_ANALYSER_ID", "SPECTRUM_PRE_CURVE_ID", "SPECTRUM_PRE_FILL_ID",
                "SPECTRUM_POST_CURVE_ID", "SPECTRUM_POST_FILL_ID", "SPECTRUM_HOLD_FILL_ID",
                "SPECTRUM_USE_RAMP_ID", "SPECTRUM_CURVE_RAMP_ID", "EQ_CURVE_RAMP_ID",
                "SPECTRUM_PRE_FILL_RAMP_ID", "SPECTRUM_PRE_CURVE_RAMP_ID",
                "SPECTRUM_HOLD_FILL_RAMP_ID", "SPECTRUM_HOLD_CURVE_RAMP_ID",
                "EQ_SUM_FILL_RAMP_ID", "EQ_BAND_CURVE_RAMP_ID", "EQ_BAND_FILL_RAMP_ID",
                "EQ_SHOW_CURVES_ID",
                "SPECTRUM_PRE_CURVE_FADE_ID", "SPECTRUM_PRE_FILL_FADE_ID",
                "SPECTRUM_POST_CURVE_FADE_ID", "SPECTRUM_POST_FILL_FADE_ID",
                "SPECTRUM_HOLD_CURVE_FADE_ID", "SPECTRUM_HOLD_FILL_FADE_ID",
                "EQ_CURVE_FADE_ID", "EQ_FILL_FADE_ID",
                "SPECTRUM_OPACITY_ID", "SPECTRUM_FILL_OPACITY_ID", "SPECTRUM_PATH_WIDTH_ID",
                "SPECTRUM_RESOLUTION_ID", "SPECTRUM_CURVE_RES_ID", "SPECTRUM_CHANNEL_ID",
                "SPECTRUM_OCTAVE_SMOOTH_ID", "SPECTRUM_FFT_BINS_ID",
                "SPECTRUM_GLOW_ENABLE_ID", "SPECTRUM_GLOW_RADIUS_ID",
                "SPECTRUM_GLOW_SPREAD_ID", "SPECTRUM_GLOW_OPACITY_ID",
                "SPECTRAL_METHOD_ID", "EQ_MULTICOLOR_BAND_FILL_ID",
                "EQ_BAND_CHROME_MATCH_HANDLES_ID", "EQ_SHOW_CROSSHAIR_ID"
            };
        case Kind::fft:
            return {
                "FFT_FULL_HEIGHT_ID", "FFT_RESOLUTION_ID", "FFT_OPACITY_ID", "FFT_BAR_WIDTH_ID",
                "FFT_INTENSITY_ID", "FFT_THRESHOLD_ID",
                "FFT_GLOW_ENABLE_ID", "FFT_GLOW_RADIUS_ID", "FFT_GLOW_SPREAD_ID", "FFT_GLOW_OPACITY_ID",
                "FFT_GLOW_OFFSET_X_ID", "FFT_GLOW_OFFSET_Y_ID"
            };
        case Kind::histogram:
            return {
                "HISTOGRAM_USE_RAMP_ID", "HISTOGRAM_SPEED_ID", "HISTOGRAM_LINE_WIDTH_ID",
                "HISTOGRAM_FILL_OPACITY_ID", "HISTOGRAM_MIN_DB_ID", "HISTOGRAM_MAX_DB_ID",
                "HISTOGRAM_SHOW_LUFS_ID", "HISTOGRAM_SHOW_RMS_ID", "HISTOGRAM_SHOW_TRUE_PEAK_ID",
                "HISTOGRAM_FREEZE_ID",
                "HISTOGRAM_GLOW_ENABLE_ID", "HISTOGRAM_GLOW_RADIUS_ID",
                "HISTOGRAM_GLOW_SPREAD_ID", "HISTOGRAM_GLOW_OPACITY_ID"
            };
        case Kind::stereogram:
            return {
                "STEREOGRAM_USE_RAMP_ID", "STEREOGRAM_DOT_SIZE_ID", "STEREOGRAM_DOT_DENSITY_ID",
                "STEREOGRAM_FADE_MS_ID",
                "STEREOGRAM_GLOW_ENABLE_ID", "STEREOGRAM_GLOW_RADIUS_ID",
                "STEREOGRAM_GLOW_SPREAD_ID", "STEREOGRAM_GLOW_OPACITY_ID"
            };
        case Kind::levelMeters:
            return {
                "METER_MODE_ID", "METER_CHANNEL_MODE_ID", "METER_READOUT_INTEGRATION_ID",
                "METER_FALL_ID", "METER_PEAK_HOLD_ID", "METER_CLIP_HOLD_ID", "METER_CLIP_THRESHOLD_ID",
                "METER_PEAK_GLOW_ENABLE_ID", "METER_PEAK_GLOW_THRESHOLD_ID",
                "METER_PEAK_GLOW_RADIUS_ID", "METER_PEAK_GLOW_SPREAD_ID", "METER_PEAK_GLOW_OPACITY_ID",
                "METER_RMS_GLOW_ENABLE_ID", "METER_RMS_GLOW_THRESHOLD_ID",
                "METER_RMS_GLOW_RADIUS_ID", "METER_RMS_GLOW_SPREAD_ID", "METER_RMS_GLOW_OPACITY_ID"
            };
        case Kind::loudness:
            return { "LOUDNESS_TARGET_ID" };
        default:
            return {};
    }
}

static void writeParam (juce::ValueTree& params,
                        juce::AudioProcessorValueTreeState& ts,
                        const juce::String& id)
{
    auto* param = ts.getParameter (id);
    if (param == nullptr)
        return;
    if (id == "BLOCK_ID")
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            const auto names = AnalyserDefaults::getBlockSizeNames();
            const int idx = juce::jlimit (0, names.size() - 1, choice->getIndex());
            params.setProperty (id, names[idx], nullptr);
            return;
        }
    }
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        params.setProperty (id, choice->getIndex(), nullptr);
        return;
    }
    if (auto* raw = ts.getRawParameterValue (id))
        params.setProperty (id, (double) raw->load(), nullptr);
}

static void applyParam (juce::AudioProcessorValueTreeState& ts,
                        const juce::String& id,
                        const juce::var& value)
{
    auto* param = ts.getParameter (id);
    if (param == nullptr)
        return;

    if (id == "BLOCK_ID")
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            const auto names = AnalyserDefaults::getBlockSizeNames();
            int idx = value.isString() ? names.indexOf (value.toString()) : (int) value;
            if (idx < 0 && value.isString())
                idx = value.toString().getIntValue();
            const int n = choice->getAllValueStrings().size();
            if (idx >= 0 && idx < n)
                choice->setValueNotifyingHost (choice->convertTo0to1 ((float) idx));
            return;
        }
    }
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        const int idx = (int) value;
        const int n = choice->getAllValueStrings().size();
        if (idx >= 0 && idx < n)
            choice->setValueNotifyingHost (choice->convertTo0to1 ((float) idx));
        return;
    }
    if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
    {
        ranged->setValueNotifyingHost (ranged->convertTo0to1 ((float) value));
        return;
    }
    param->setValueNotifyingHost ((float) value);
}

juce::ValueTree captureFromApvts (Kind k, juce::AudioProcessorValueTreeState& treeState)
{
    juce::ValueTree root ("ModuleLook");
    root.setProperty ("kind", kindFolder (k), nullptr);
    juce::ValueTree params ("Params");
    for (const auto& id : parameterIdsForKind (k))
        writeParam (params, treeState, id);
    if (params.getNumProperties() > 0)
        root.appendChild (std::move (params), nullptr);
    return root;
}

void applyToApvts (Kind k, juce::AudioProcessorValueTreeState& treeState, const juce::ValueTree& look)
{
    if (! look.isValid())
        return;
    juce::ValueTree params = look.getChildWithName ("Params");
    if (! params.isValid() && look.hasType ("Params"))
        params = look;
    if (params.isValid())
    {
        for (int i = 0; i < params.getNumProperties(); ++i)
        {
            const auto name = params.getPropertyName (i);
            applyParam (treeState, name.toString(), params.getProperty (name));
        }
        return;
    }
    for (const auto& id : parameterIdsForKind (k))
        if (look.hasProperty (id))
            applyParam (treeState, id, look.getProperty (id));
}

bool saveDefaultFromApvts (Kind k, juce::AudioProcessorValueTreeState& treeState)
{
    const auto look = captureFromApvts (k, treeState);
    const bool okM = saveDefault (k, look);
    const bool okA = AnalyserDefaults::mergeIdsFrom (treeState, parameterIdsForKind (k));
    return okM && okA;
}
} // namespace ModuleLookPresets
