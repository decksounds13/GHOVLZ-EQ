#include "AnalyserDefaults.h"

juce::File AnalyserDefaults::getDefaultsFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("ParametricEq")
        .getChildFile ("analyser_defaults.xml");
}

juce::StringArray AnalyserDefaults::getParameterIds()
{
    return {
        "BLOCK_ID",
        "BINS_ID",
        "MAX_ID",
        "LIN_ID",
        "LOG_ID",
        "ST_ID",
        "MAX_HOLD_ID",
        "SPECTRUM_ANALYSER_ID",
        "SPECTRUM_PRE_CURVE_ID",
        "SPECTRUM_PRE_FILL_ID",
        "SPECTRUM_POST_CURVE_ID",
        "SPECTRUM_POST_FILL_ID",
        "SPECTRUM_HOLD_FILL_ID",
        "SPECTRUM_USE_RAMP_ID",
        "SPECTRUM_CURVE_RAMP_ID",
        "EQ_CURVE_RAMP_ID",
        "SPECTRUM_PRE_FILL_RAMP_ID",
        "SPECTRUM_PRE_CURVE_RAMP_ID",
        "SPECTRUM_HOLD_FILL_RAMP_ID",
        "SPECTRUM_HOLD_CURVE_RAMP_ID",
        "EQ_SUM_FILL_RAMP_ID",
        "EQ_BAND_CURVE_RAMP_ID",
        "EQ_BAND_FILL_RAMP_ID",
        "EQ_SHOW_CURVES_ID",
        "SPECTRUM_PRE_CURVE_FADE_ID",
        "SPECTRUM_PRE_FILL_FADE_ID",
        "SPECTRUM_POST_CURVE_FADE_ID",
        "SPECTRUM_POST_FILL_FADE_ID",
        "SPECTRUM_HOLD_CURVE_FADE_ID",
        "SPECTRUM_HOLD_FILL_FADE_ID",
        "EQ_CURVE_FADE_ID",
        "EQ_FILL_FADE_ID",
        "SPECTRUM_OPACITY_ID",
        "SPECTRUM_FILL_OPACITY_ID",
        "SPECTRUM_PATH_WIDTH_ID",
        "SPECTRUM_RESOLUTION_ID",
        "SPECTRUM_CURVE_RES_ID",
        "SPECTRUM_CHANNEL_ID",
        "SPECTRUM_OCTAVE_SMOOTH_ID",
        "SPECTRUM_FFT_BINS_ID",
        "EQ_DISPLAY_RANGE_ID",
        "EQ_BAND_PATH_WIDTH_ID",
        "EQ_SUM_PATH_WIDTH_ID",
        "EQ_SUM_GLOW_ENABLE_ID",
        "EQ_SUM_GLOW_RADIUS_ID",
        "EQ_SUM_GLOW_SPREAD_ID",
        "EQ_SUM_GLOW_OPACITY_ID",
        "SPECTRUM_GLOW_ENABLE_ID",
        "SPECTRUM_GLOW_RADIUS_ID",
        "SPECTRUM_GLOW_SPREAD_ID",
        "SPECTRUM_GLOW_OPACITY_ID",
        "OSC_LINE_OPACITY_ID",
        "OSC_LINE_WIDTH_ID",
        "OSC_GLOW_ENABLE_ID",
        "OSC_GLOW_RADIUS_ID",
        "OSC_GLOW_SPREAD_ID",
        "OSC_GLOW_OPACITY_ID",
        "OSC_EXPANDED_LINE_WIDTH_ID",
        "OSC_EXPANDED_GLOW_ENABLE_ID",
        "OSC_EXPANDED_GLOW_RADIUS_ID",
        "OSC_EXPANDED_GLOW_SPREAD_ID",
        "OSC_EXPANDED_GLOW_OPACITY_ID",
        "OSC_QUALITY_ID",
        "GON_LINE_OPACITY_ID",
        "GON_LINE_WIDTH_ID",
        "GON_GLOW_ENABLE_ID",
        "GON_GLOW_RADIUS_ID",
        "GON_GLOW_SPREAD_ID",
        "GON_GLOW_OPACITY_ID",
        "GON_EXPANDED_LINE_WIDTH_ID",
        "GON_EXPANDED_GLOW_ENABLE_ID",
        "GON_EXPANDED_GLOW_RADIUS_ID",
        "GON_EXPANDED_GLOW_SPREAD_ID",
        "GON_EXPANDED_GLOW_OPACITY_ID",
        "GON_QUALITY_ID",
        "GON_USE_RAMP_ID",
        "SPEC_COLOUR_SCHEME_ID",
        "SPEC_FFT_SIZE_ID",
        "SPEC_DISPLAY_RES_ID",
        "SPEC_CHANNEL_ID",
        "SPEC_SPEED_ID",
        "SPEC_BRIGHTNESS_ID",
        "SPEC_MIN_DB_ID",
        "SPEC_MAX_DB_ID",
        "SPEC_SMOOTH_ID",
        "SPEC_SOFTEN_ID",
        "SPEC_LOG_FREQ_ID",
        "SPEC_ENHANCED_FREQ_ID",
        "SPEC_ENHANCED_FREQ_3D_ID",
        "SPEC_ENHANCED_STRENGTH_ID",
        "SPEC_ENHANCED_LF_DETAIL_ID",
        "SPEC_ENHANCED_CROSSOVER_ID",
        "SPEC_FREEZE_ID",
        "STEREOGRAM_USE_RAMP_ID",
        "STEREOGRAM_DOT_SIZE_ID",
        "STEREOGRAM_DOT_DENSITY_ID",
        "STEREOGRAM_FADE_MS_ID",
        "STEREOGRAM_GLOW_ENABLE_ID",
        "STEREOGRAM_GLOW_RADIUS_ID",
        "STEREOGRAM_GLOW_SPREAD_ID",
        "STEREOGRAM_GLOW_OPACITY_ID",
        "HISTOGRAM_USE_RAMP_ID",
        "HISTOGRAM_SPEED_ID",
        "HISTOGRAM_LINE_WIDTH_ID",
        "HISTOGRAM_FILL_OPACITY_ID",
        "HISTOGRAM_MIN_DB_ID",
        "HISTOGRAM_MAX_DB_ID",
        "HISTOGRAM_SHOW_LUFS_ID",
        "HISTOGRAM_SHOW_RMS_ID",
        "HISTOGRAM_SHOW_TRUE_PEAK_ID",
        "HISTOGRAM_FREEZE_ID",
        "HISTOGRAM_GLOW_ENABLE_ID",
        "HISTOGRAM_GLOW_RADIUS_ID",
        "HISTOGRAM_GLOW_SPREAD_ID",
        "HISTOGRAM_GLOW_OPACITY_ID",
        "EQ_MULTICOLOR_BAND_FILL_ID",
        "EQ_BAND_CHROME_MATCH_HANDLES_ID",
        "SPECTRAL_METHOD_ID",
        "EQ_SHOW_CROSSHAIR_ID",
        "FFT_FULL_HEIGHT_ID",
        "FFT_RESOLUTION_ID",
        "FFT_OPACITY_ID",
        "FFT_BAR_WIDTH_ID",
        "FFT_INTENSITY_ID",
        "FFT_THRESHOLD_ID",
        "FFT_GLOW_ENABLE_ID",
        "FFT_GLOW_RADIUS_ID",
        "FFT_GLOW_SPREAD_ID",
        "FFT_GLOW_OPACITY_ID",
        "FFT_GLOW_OFFSET_X_ID",
        "FFT_GLOW_OFFSET_Y_ID",
        "METER_MODE_ID",
        "METER_CHANNEL_MODE_ID",
        "METER_READOUT_INTEGRATION_ID",
        "METER_FALL_ID",
        "METER_PEAK_HOLD_ID",
        "METER_CLIP_HOLD_ID",
        "METER_CLIP_THRESHOLD_ID",
        "METER_PEAK_GLOW_ENABLE_ID",
        "METER_PEAK_GLOW_THRESHOLD_ID",
        "METER_PEAK_GLOW_RADIUS_ID",
        "METER_PEAK_GLOW_SPREAD_ID",
        "METER_PEAK_GLOW_OPACITY_ID",
        "METER_RMS_GLOW_ENABLE_ID",
        "METER_RMS_GLOW_THRESHOLD_ID",
        "METER_RMS_GLOW_RADIUS_ID",
        "METER_RMS_GLOW_SPREAD_ID",
        "METER_RMS_GLOW_OPACITY_ID"
    };
}

juce::StringArray AnalyserDefaults::getBlockSizeNames()
{
    return { "2048", "4096", "8192", "16384" };
}

AnalyserDefaults AnalyserDefaults::load()
{
    AnalyserDefaults defaults;
    const auto file = getDefaultsFile();

    if (! file.existsAsFile())
        return defaults;

    if (auto xml = juce::parseXML (file))
    {
        if (! xml->hasTagName ("AnalyserDefaults"))
            return defaults;

        defaults.fileVersion = xml->getIntAttribute ("version", 1);

        // Read every saved attribute. A whitelist dropped new IDs (OSC_USE_RAMP,
        // REFRESH, …) so Save Default looked like it worked but did not reload.
        for (int i = 0; i < xml->getNumAttributes(); ++i)
        {
            const auto name = xml->getAttributeName (i);
            if (name == "version" || name == "savedAt")
                continue;
            defaults.values.set (name, xml->getAttributeValue (i));
        }
    }

    return defaults;
}

static void writeAnalyserAttr (juce::XmlElement& xml,
                               juce::AudioProcessorValueTreeState& treeState,
                               const juce::String& id)
{
    auto* param = treeState.getParameter (id);
    if (param == nullptr)
        return;

    if (id == "BLOCK_ID")
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        {
            const auto names = AnalyserDefaults::getBlockSizeNames();
            const int idx = juce::jlimit (0, names.size() - 1, choice->getIndex());
            xml.setAttribute (id, names[idx]);
            return;
        }
    }

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (param))
        xml.setAttribute (id, choice->getIndex());
    else if (auto* raw = treeState.getRawParameterValue (id))
        xml.setAttribute (id, (double) raw->load());
}

bool AnalyserDefaults::saveFrom (juce::AudioProcessorValueTreeState& treeState)
{
    auto xml = std::make_unique<juce::XmlElement> ("AnalyserDefaults");
    // v2: BLOCK_ID stored as size name ("2048") so choice-list reshuffles stay stable.
    xml->setAttribute ("version", 2);
    xml->setAttribute ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));

    for (const auto& id : getParameterIds())
        writeAnalyserAttr (*xml, treeState, id);

    auto file = getDefaultsFile();
    file.getParentDirectory().createDirectory();
    if (! file.getParentDirectory().isDirectory())
        return false;

    return xml->writeTo (file);
}

bool AnalyserDefaults::mergeIdsFrom (juce::AudioProcessorValueTreeState& treeState,
                                     const juce::StringArray& ids)
{
    if (ids.isEmpty())
        return true;

    auto file = getDefaultsFile();
    std::unique_ptr<juce::XmlElement> xml;
    if (file.existsAsFile())
        xml = juce::parseXML (file);
    if (xml == nullptr || ! xml->hasTagName ("AnalyserDefaults"))
        xml = std::make_unique<juce::XmlElement> ("AnalyserDefaults");

    xml->setAttribute ("version", 2);
    xml->setAttribute ("savedAt", juce::Time::getCurrentTime().toISO8601 (true));
    for (const auto& id : ids)
        writeAnalyserAttr (*xml, treeState, id);

    file.getParentDirectory().createDirectory();
    if (! file.getParentDirectory().isDirectory())
        return false;
    return xml->writeTo (file);
}

float AnalyserDefaults::getFloat (const juce::String& id, float fallback) const
{
    if (! values.containsKey (id))
        return fallback;

    return (float) values[id].getDoubleValue();
}

bool AnalyserDefaults::getBool (const juce::String& id, bool fallback) const
{
    if (! values.containsKey (id))
        return fallback;

    return values[id].getDoubleValue() >= 0.5;
}

int AnalyserDefaults::getInt (const juce::String& id, int fallback) const
{
    if (! values.containsKey (id))
        return fallback;

    return values[id].getIntValue();
}

int AnalyserDefaults::getBlockIndex (int fallback) const
{
    if (! values.containsKey ("BLOCK_ID"))
        return juce::jlimit (0, 3, fallback);

    const auto text = values["BLOCK_ID"].trim();
    const auto names = getBlockSizeNames();

    // Prefer stable size names (v2+).
    for (int i = 0; i < names.size(); ++i)
        if (text.equalsIgnoreCase (names[i]))
            return i;

    // Legacy sizes removed from the menu → nearest available (2048).
    if (text == "512" || text == "1024")
        return 0;

    const int raw = text.getIntValue();

    // v1 saved a raw choice index against {512,1024,2048,4096,8192}.
    // Index 2 meant 2048 then; after rollback index 2 is 8192 — remap.
    if (fileVersion <= 1 && raw >= 0 && raw <= 4)
    {
        static constexpr int oldFiveToNewFour[] = { 0, 0, 0, 1, 2 };
        return oldFiveToNewFour[raw];
    }

    if (raw >= 0 && raw <= 3)
        return raw;

    // Out-of-range / garbage → safe default (2048).
    return juce::jlimit (0, 3, fallback);
}

void AnalyserDefaults::stampBlockIdInState (juce::ValueTree& state, int blockIndex)
{
    const auto names = getBlockSizeNames();
    const int idx = juce::jlimit (0, names.size() - 1, blockIndex);
    state.setProperty ("deckStateSchema", 2, nullptr);
    state.setProperty ("blockSizeName", names[idx], nullptr);

    // APVTS PARAM values are unnormalised (choice index), same as highShelfType=2.0 etc.
    // Writing a 0–1 norm here made 8192 (index 2 → 0.666) reload as index 0/1.
    const float unnorm = (float) idx;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (child.hasType ("PARAM") && child["id"].toString() == "BLOCK_ID")
        {
            child.setProperty ("value", unnorm, nullptr);
            return;
        }
    }
}

void AnalyserDefaults::migrateBlockIdInState (juce::ValueTree& state)
{
    const auto names = getBlockSizeNames();

    // Prefer stable size name whenever present (repairs bad normalised BLOCK_ID dumps).
    if (state.hasProperty ("blockSizeName"))
    {
        const int idx = names.indexOf (state.getProperty ("blockSizeName").toString());
        if (idx >= 0)
            stampBlockIdInState (state, idx);
        return;
    }

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);
        if (! child.hasType ("PARAM") || child["id"].toString() != "BLOCK_ID")
            continue;

        const float stored = (float) child.getProperty ("value");

        // Already a clean choice index for the current 4-item list.
        if (stored >= 0.0f && stored <= 3.0f && std::abs (stored - std::round (stored)) < 0.01f)
        {
            stampBlockIdInState (state, (int) std::lround (stored));
            return;
        }

        // Buggy normalised dumps: 0, 1/3, 2/3, 1 → indices 0–3.
        static constexpr float kNorms[] = { 0.0f, 1.0f / 3.0f, 2.0f / 3.0f, 1.0f };
        for (int n = 0; n < 4; ++n)
        {
            if (std::abs (stored - kNorms[n]) < 0.02f)
            {
                stampBlockIdInState (state, n);
                return;
            }
        }

        if ((int) state.getProperty ("deckStateSchema", 0) >= 2)
            return;

        // Legacy 5-choice list used norms k/4. Old default 2048 was index 2 (0.5).
        const int oldIndex = juce::jlimit (0, 4, (int) std::lround ((double) stored * 4.0));
        const float expected = oldIndex / 4.0f;
        if (std::abs (stored - expected) < 0.02f && oldIndex >= 1 && oldIndex <= 3)
        {
            static constexpr int oldFiveToNewFour[] = { 0, 0, 0, 1, 2 };
            stampBlockIdInState (state, oldFiveToNewFour[oldIndex]);
        }
        return;
    }
}
