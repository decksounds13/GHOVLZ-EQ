#include "SharedResources.h"
#include "../KnobThemeHelpers.h"
#include <cmath>

SharedResources* SharedResources::activeInstance = nullptr;

namespace ThemeColorRegistry
{
    static const Entry kEntries[] = {
        // Menu
        { "Menu Background",              "MenuBackgroundGradientColor1",       &SharedColors::menuBackgroundGradientColor1 },
        { "Menu Background 2",            "MenuBackgroundGradientColor2",       &SharedColors::menuBackgroundGradientColor2 },
        { "Menu Border",                  "MenuTabBarBorderColor",              &SharedColors::menuTabBarBorderColor },
        { "Menu Button Background",       "MenuButtonGradientColor1",           &SharedColors::menuButtonGradientColor1 },
        { "Menu Button Background 2",     "MenuButtonGradientColor2",           &SharedColors::menuButtonGradientColor2 },
        { "Menu Button Text",             "MenuButtonTextColor1",               &SharedColors::menuButtonTextColor1 },
        { "Menu Label Text",              "MenuLabelTextColor1",                &SharedColors::menuLabelTextColor1 },
        { "Menu ListBox Background",      "MenuListBoxBackgroundGradientColor1",&SharedColors::menuListBoxBackgroundGradientColor1 },
        { "Menu ListBox Background 2",    "MenuListBoxBackgroundGradientColor2",&SharedColors::menuListBoxBackgroundGradientColor2 },
        { "Menu ListBox Selection",       "MenuListBoxSelectionColor1",         &SharedColors::menuListBoxSelectionColor1 },
        { "Menu ListBox Text",            "MenuListBoxTextColor1",              &SharedColors::menuListBoxTextColor1 },
        { "Menu Scroll Outline",          "MenuScrollBarOutlineColor1",         &SharedColors::menuScrollBarOutlineColor1 },
        { "Menu Scroll Thumb",            "MenuScrollBarThumbColor1",           &SharedColors::menuScrollBarThumbColor1 },
        { "Menu Scroll Track",            "MenuScrollBarTrackColor1",           &SharedColors::menuScrollBarTrackColor1 },
        { "Menu Slider Fill",             "MenuSliderFillColor",                &SharedColors::menuSliderFillColor },
        { "Menu TextBox Text",            "MenuTextBoxTextColor1",              &SharedColors::menuTextBoxTextColor1 },
        { "Menu Thin Border",             "MenuThinBorderColor",                &SharedColors::menuThinBorderColor },

        // Plugin
        { "Plugin Background",            "PluginBackground",                   &SharedColors::pluginBackground },
        { "Plugin Background 2",          "PluginBackground2",                  &SharedColors::pluginBackground2 },
        { "Plugin Brand Text",            "PluginBrandText",                    &SharedColors::pluginBrandText },
        { "Plugin Button Accent",         "PluginButtonAccent",                 &SharedColors::pluginButtonAccent },
        { "Plugin Button Background",     "PluginButtonBackground",             &SharedColors::pluginButtonBackground },
        { "Plugin Button Text",           "PluginButtonText",                   &SharedColors::pluginButtonText },
        { "Plugin Preset Background",     "PluginPresetBackground",             &SharedColors::pluginPresetBackground },
        { "Plugin Preset Text",           "PluginPresetText",                   &SharedColors::pluginPresetText },

        // Graph
        { "Graph Axis Text",              "GraphAxisText",                      &SharedColors::graphAxisText },
        { "Graph Background",             "GraphBackground",                    &SharedColors::graphBackground },
        { "Graph Background 2",           "GraphBackground2",                   &SharedColors::graphBackground2 },
        { "Graph Band 1",                 "GraphBand1",                         &SharedColors::graphBand1 },
        { "Graph Band 2",                 "GraphBand2",                         &SharedColors::graphBand2 },
        { "Graph Band 3",                 "GraphBand3",                         &SharedColors::graphBand3 },
        { "Graph Band 4",                 "GraphBand4",                         &SharedColors::graphBand4 },
        { "Graph Band 5",                 "GraphBand5",                         &SharedColors::graphBand5 },
        { "Graph Band 6",                 "GraphBand6",                         &SharedColors::graphBand6 },
        { "Graph Band 7",                 "GraphBand7",                         &SharedColors::graphBand7 },
        { "Graph Band 8",                 "GraphBand8",                         &SharedColors::graphBand8 },
        { "Graph Grid",                   "GraphGrid",                          &SharedColors::graphGrid },
        { "Graph Handle Outline",         "GraphHandleOutline",                 &SharedColors::graphHandleOutline },
        { "Graph Handle Text",            "GraphHandleText",                    &SharedColors::graphHandleText },
        { "Graph Overlay Background",     "GraphOverlayBackground",             &SharedColors::graphOverlayBackground },
        { "Graph Overlay Border",         "GraphOverlayBorder",                 &SharedColors::graphOverlayBorder },
        { "Graph Sum Curve",              "GraphSumCurve",                      &SharedColors::graphSumCurve },
        { "Graph Sum Fill Bottom",        "GraphSumFillBottom",                 &SharedColors::graphSumFillBottom },
        { "Graph Sum Fill Top",           "GraphSumFillTop",                    &SharedColors::graphSumFillTop },
        { "Graph Sum Glow",               "GraphSumGlow",                       &SharedColors::graphSumGlow },

        // Option
        { "Option Background",            "OptionBackground",                   &SharedColors::optionBackground },
        { "Option Border",                "OptionBorder",                       &SharedColors::optionBorder },
        { "Option Combo Background",      "OptionComboBackground",              &SharedColors::optionComboBackground },
        { "Option Combo Highlight",       "OptionComboHighlight",               &SharedColors::optionComboHighlight },
        { "Option Combo Text",            "OptionComboText",                    &SharedColors::optionComboText },
        { "Option Text",                  "OptionText",                         &SharedColors::optionText },

        // Knob
        { "Knob Arc",                     "KnobArc",                            &SharedColors::knobArc },
        { "Knob Multiply",                "KnobMultiply",                       &SharedColors::knobMultiply },
        { "Knob Popup Background",        "KnobPopupBackground",                &SharedColors::knobPopupBackground },
        { "Knob Popup Text",              "KnobPopupText",                      &SharedColors::knobPopupText },
        { "Knob Tint",                    "KnobTint",                           &SharedColors::knobTint },

        // Meter
        { "Meter Background",             "MeterBackground",                    &SharedColors::meterBackground },
        { "Meter Clip",                   "MeterClip",                          &SharedColors::meterClip },
        { "Meter Fill",                   "MeterFill",                          &SharedColors::meterFill },
        { "Meter Readout Text",           "MeterReadoutText",                   &SharedColors::meterReadoutText },

        // Oscilloscope / Goniometer (shared backgrounds + path)
        { "Osc/Gon Background",           "OscBackground",                      &SharedColors::oscBackground },
        { "Osc/Gon Background 2",         "OscBackground2",                     &SharedColors::oscBackground2 },
        { "Osc/Gon Glow",                 "OscGlow",                            &SharedColors::oscGlow },
        { "Osc/Gon Line",                 "OscLine",                            &SharedColors::oscLine },

        // Legacy gon members still exist for XML; kept in sync, not listed separately.
        // Corr meter colours stay gon-only:
        { "Gon Corr Negative",            "GonCorrNegative",                    &SharedColors::gonCorrNegative },
        { "Gon Corr Positive",            "GonCorrPositive",                    &SharedColors::gonCorrPositive },

        // Scope arrange
        { "Scope Drop Outline",           "ScopeDropOutline",                   &SharedColors::scopeDropOutline },

        // Mod
        { "Mod Accent",                   "ModAccent",                          &SharedColors::modAccent },
        { "Mod Background",               "ModBackground",                      &SharedColors::modBackground },
        { "Mod Border",                   "ModBorder",                          &SharedColors::modBorder },
        { "Mod Text",                     "ModText",                            &SharedColors::modText },

        // Spectrum
        { "Spectrum Background",          "SpectrumBackground",                 &SharedColors::spectrumBackground },
        { "Spectrum Background 2",        "SpectrumBackground2",                &SharedColors::spectrumBackground2 },
        { "Spectrum Fill",                "SpectrumFill",                       &SharedColors::spectrumFill },
        { "Spectrum Grid",                "SpectrumGrid",                       &SharedColors::spectrumGrid },
        { "Spectrum Line",                "SpectrumLine",                       &SharedColors::spectrumLine },
        { "Spectrum Text",                "SpectrumText",                       &SharedColors::spectrumText },
    };

    const Entry* getEntries() noexcept { return kEntries; }
    int getNumEntries() noexcept { return (int) (sizeof (kEntries) / sizeof (kEntries[0])); }
}

SharedColors::SharedColors()
{
    colorRandomizationFlags.assign ((size_t) getNumColors(), 0);
    syncSharedScopePathColours();
    syncFaceplateModScheme();
}

void SharedColors::syncFaceplateModScheme() noexcept
{
    // Faceplate (Plugin*), Mod, and Option box share one chrome family.
    // Knob Arc stays independent so it can randomize to vivid colours.
    modBackground = pluginBackground2;
    modBorder = pluginButtonBackground;
    modAccent = pluginButtonAccent;
    modText = pluginButtonText.withAlpha (217.0f / 255.0f);

    // Option box paints with Plugin Background / Background 2 directly; keep
    // Option* slots mirrored so the Appearance list stays consistent.
    optionBackground = pluginBackground2;
    optionBorder = pluginButtonBackground;
    optionText = pluginButtonText;
    optionComboBackground = pluginButtonBackground;
    optionComboText = pluginButtonText;
    optionComboHighlight = pluginButtonAccent;
}

SharedColors::RandomizeModule SharedColors::moduleForDisplayName (const juce::String& displayName) noexcept
{
    if (displayName.startsWith ("Menu "))
        return RandomizeModule::Menu;

    if (displayName.startsWith ("Plugin ")
        || displayName.startsWith ("Knob ")
        || displayName.startsWith ("Option ")
        || displayName.startsWith ("Mod "))
        return RandomizeModule::FaceplateMod;

    if (displayName.startsWith ("Graph ")
        || displayName.startsWith ("Spectrum ")
        || displayName.startsWith ("Meter ")
        || displayName.startsWith ("Osc ")
        || displayName.startsWith ("Gon ")
        || displayName.startsWith ("Osc/Gon ")
        || displayName.startsWith ("Scope "))
        return RandomizeModule::Graph;

    return RandomizeModule::Other;
}

bool SharedColors::shouldRandomizeIndex (int index) const noexcept
{
    if (index < 0 || index >= getNumColors())
        return false;

    const auto name = juce::String (ThemeColorRegistry::getEntries()[index].displayName);
    switch (moduleForDisplayName (name))
    {
        case RandomizeModule::Menu:         return randomizeMenuModule;
        case RandomizeModule::FaceplateMod: return randomizeFaceplateMod;
        case RandomizeModule::Graph:        return randomizeGraphModule;
        default:                            return true;
    }
}

juce::Colour& SharedColors::colourAt (int index)
{
    jassert (index >= 0 && index < getNumColors());
    return this->*ThemeColorRegistry::getEntries()[index].member;
}

const juce::Colour& SharedColors::colourAt (int index) const
{
    jassert (index >= 0 && index < getNumColors());
    return this->*ThemeColorRegistry::getEntries()[index].member;
}

juce::Colour* SharedColors::findByDisplayName (const juce::String& name)
{
    const int i = ThemeColorRegistry::indexForDisplayName (name);
    return i >= 0 ? &colourAt (i) : nullptr;
}

const juce::Colour* SharedColors::findByDisplayName (const juce::String& name) const
{
    const int i = ThemeColorRegistry::indexForDisplayName (name);
    return i >= 0 ? &colourAt (i) : nullptr;
}

void SharedColors::syncSharedScopePathColours() noexcept
{
    // Oscilloscope and goniometer share one colour family.
    gonLine = oscLine;
    gonGlow = oscGlow;
    gonBackground = oscBackground;
    gonBackground2 = oscBackground2;
}

void SharedColors::setColourAt (int index, juce::Colour newColor, bool force)
{
    if (index < 0 || index >= getNumColors())
        return;

    if (! force)
    {
        if (index >= (int) colorRandomizationFlags.size() || ! colorRandomizationFlags[(size_t) index])
            return;
    }

    const auto name = juce::String (ThemeColorRegistry::getEntries()[index].displayName);
    if (name == "Knob Multiply")
        newColor = KnobTheme::clampMultiply (newColor);
    else if (name == "Knob Tint")
        newColor = KnobTheme::clampTint (newColor);

    colourAt (index) = newColor;
    syncSharedScopePathColours();

    if (name.startsWith ("Plugin "))
        syncFaceplateModScheme();
}

void SharedColors::setColourByDisplayName (const juce::String& name, juce::Colour newColor, bool force)
{
    setColourAt (ThemeColorRegistry::indexForDisplayName (name), newColor, force);
}

void SharedColors::toggleColorRandomizationFlags (const juce::Array<int>& paletteIndices)
{
    std::fill (colorRandomizationFlags.begin(), colorRandomizationFlags.end(), 0);
    for (auto index : paletteIndices)
        if (index >= 0 && index < (int) colorRandomizationFlags.size())
            colorRandomizationFlags[(size_t) index] = 1;
}

void SharedColors::updateColorsDirectly (juce::Colour newColor, const juce::Array<int>& paletteIndices)
{
    auto originalFlags = colorRandomizationFlags;
    for (auto index : paletteIndices)
    {
        if (index >= 0 && index < (int) colorRandomizationFlags.size())
            colorRandomizationFlags[(size_t) index] = 1;
        setColourAt (index, newColor, false);
    }
    colorRandomizationFlags = std::move (originalFlags);
}

juce::Colour SharedColors::randomColourInLimits (float alpha) const
{
    auto& rng = juce::Random::getSystemRandom();
    const float h = randomizeHue
                        ? rng.nextFloat() * (hueUpperLimit - hueLowerLimit) + hueLowerLimit
                        : hueLowerLimit;
    const float s = randomizeSaturation
                        ? rng.nextFloat() * (saturationUpperLimit - saturationLowerLimit) + saturationLowerLimit
                        : saturationLowerLimit;
    const float b = randomizeBrightness
                        ? rng.nextFloat() * (brightnessUpperLimit - brightnessLowerLimit) + brightnessLowerLimit
                        : brightnessLowerLimit;
    return juce::Colour::fromHSV (h, s, b, juce::jlimit (0.0f, 1.0f, alpha));
}

namespace
{
float relativeLuminance (juce::Colour c) noexcept
{
    auto lin = [] (float channel) noexcept
    {
        channel = juce::jlimit (0.0f, 1.0f, channel);
        return channel <= 0.04045f ? channel / 12.92f
                                   : std::pow ((channel + 0.055f) / 1.055f, 2.4f);
    };
    return 0.2126f * lin (c.getFloatRed())
         + 0.7152f * lin (c.getFloatGreen())
         + 0.0722f * lin (c.getFloatBlue());
}

float contrastRatio (juce::Colour a, juce::Colour b) noexcept
{
    const float l1 = relativeLuminance (a);
    const float l2 = relativeLuminance (b);
    const float lighter = juce::jmax (l1, l2);
    const float darker  = juce::jmin (l1, l2);
    return (lighter + 0.05f) / (darker + 0.05f);
}

juce::Colour blendBg (juce::Colour a, juce::Colour b) noexcept
{
    return a.interpolatedWith (b, 0.5f);
}

float minRatioFromAmount (float amount01) noexcept
{
    const float t = juce::jlimit (0.0f, 1.0f, amount01);
    return 2.5f + t * 4.5f; // 2.5 … 7.0
}

/** Adjust text value (V) only — hue and saturation stay fixed. */
juce::Colour ensureTextOnBackground (juce::Colour text, juce::Colour background,
                                     float minRatio) noexcept
{
    if (contrastRatio (text, background) >= minRatio)
        return text;

    const float h = text.getHue();
    const float s = text.getSaturation();
    const float a = text.getFloatAlpha();
    const float bgY = relativeLuminance (background);
    const bool preferLight = bgY < 0.5f;

    auto atV = [&] (float v) noexcept
    {
        return juce::Colour::fromHSV (h, s, juce::jlimit (0.0f, 1.0f, v), a);
    };

    float lo = preferLight ? text.getBrightness() : 0.0f;
    float hi = preferLight ? 1.0f : text.getBrightness();
    if (preferLight)
        lo = juce::jmin (lo, 0.5f);
    else
        hi = juce::jmax (hi, 0.5f);

    juce::Colour best = preferLight ? atV (1.0f) : atV (0.0f);
    for (int i = 0; i < 14; ++i)
    {
        const float mid = 0.5f * (lo + hi);
        const auto cand = atV (mid);
        if (contrastRatio (cand, background) >= minRatio)
        {
            best = cand;
            if (preferLight)
                hi = mid;
            else
                lo = mid;
        }
        else
        {
            if (preferLight)
                lo = mid;
            else
                hi = mid;
        }
    }

    if (contrastRatio (best, background) < minRatio)
    {
        const auto other = preferLight ? atV (0.0f) : atV (1.0f);
        if (contrastRatio (other, background) > contrastRatio (best, background))
            best = other;
    }

    return best;
}
} // namespace

void SharedColors::enforceLegibleTextContrast() noexcept
{
    if (! enforceLegibleText)
        return;

    const float minRatio = minRatioFromAmount (textContrastAmount);

    auto fix = [&] (juce::Colour& text, juce::Colour bg)
    {
        text = ensureTextOnBackground (text, bg, minRatio);
    };

    fix (menuButtonTextColor1, blendBg (menuButtonGradientColor1, menuButtonGradientColor2));
    fix (menuLabelTextColor1, blendBg (menuBackgroundGradientColor1, menuBackgroundGradientColor2));
    fix (menuListBoxTextColor1, blendBg (menuListBoxBackgroundGradientColor1, menuListBoxBackgroundGradientColor2));
    fix (menuListBoxTextColor1, menuListBoxSelectionColor1);
    fix (menuTextBoxTextColor1, blendBg (menuBackgroundGradientColor1, menuBackgroundGradientColor2));

    fix (pluginButtonText, pluginButtonBackground);
    fix (pluginPresetText, pluginPresetBackground);
    fix (pluginBrandText, blendBg (pluginBackground, pluginBackground2));

    fix (graphAxisText, blendBg (graphBackground, graphBackground2));
    fix (graphHandleText, graphBackground);

    fix (optionText, optionBackground);
    fix (optionComboText, optionComboBackground);
    fix (modText, modBackground);

    fix (knobPopupText, knobPopupBackground);
    fix (meterReadoutText, meterBackground);
    fix (spectrumText, blendBg (spectrumBackground, spectrumBackground2));
}

void SharedColors::randomizeColors()
{
    auto randomHue = [this]() -> float
    {
        const float range = hueUpperLimit - hueLowerLimit;
        return randomizeHue ? juce::Random::getSystemRandom().nextFloat() * range + hueLowerLimit : hueLowerLimit;
    };
    auto randomSaturation = [this]() -> float
    {
        return randomizeSaturation
                   ? juce::Random::getSystemRandom().nextFloat() * (saturationUpperLimit - saturationLowerLimit) + saturationLowerLimit
                   : saturationLowerLimit;
    };
    auto randomBrightness = [this]() -> float
    {
        return randomizeBrightness
                   ? juce::Random::getSystemRandom().nextFloat() * (brightnessUpperLimit - brightnessLowerLimit) + brightnessLowerLimit
                   : brightnessLowerLimit;
    };

    // Keep each slot's factory-default alpha — picker/random are RGB/HSV-only.
    static const SharedColors factoryDefaults;
    for (int i = 0; i < getNumColors(); ++i)
    {
        if (! shouldRandomizeIndex (i))
            continue;

        // Mod / Option are derived from Plugin chrome after Faceplate/Mod randomize.
        // Knob Arc is randomized on its own (vivid arcs); do not skip it.
        const auto name = juce::String (ThemeColorRegistry::getEntries()[i].displayName);
        if ((name.startsWith ("Mod ") || name.startsWith ("Option ")) && randomizeFaceplateMod)
            continue;

        const float keepAlpha = factoryDefaults.colourAt (i).getFloatAlpha();
        auto colour = juce::Colour::fromHSV (randomHue(), randomSaturation(), randomBrightness(), keepAlpha);

        // Knob arcs need enough chroma/value to read against the stitched knob art.
        if (name == "Knob Arc")
        {
            const float sat = juce::jmax (0.55f, colour.getSaturation());
            const float bri = juce::jlimit (0.45f, 1.0f, colour.getBrightness());
            colour = juce::Colour::fromHSV (colour.getHue(), sat, bri, keepAlpha);
        }
        else if (name == "Knob Multiply")
        {
            // Soft filter: mid sat, bright — never near black.
            const float sat = juce::jlimit (0.05f, 0.40f, colour.getSaturation());
            const float bri = juce::jlimit (0.60f, 0.95f, colour.getBrightness());
            colour = KnobTheme::clampMultiply (juce::Colour::fromHSV (colour.getHue(), sat, bri, 1.0f));
        }
        else if (name == "Knob Tint")
        {
            // Gentle wash — moderate sat/bri and capped alpha.
            const float sat = juce::jlimit (0.20f, 0.65f, colour.getSaturation());
            const float bri = juce::jlimit (0.35f, 0.80f, colour.getBrightness());
            const float alpha = 0.12f + juce::Random::getSystemRandom().nextFloat() * 0.26f; // 0.12–0.38
            colour = KnobTheme::clampTint (juce::Colour::fromHSV (colour.getHue(), sat, bri, alpha));
        }

        colourAt (i) = colour;
    }
    syncSharedScopePathColours();
    if (randomizeFaceplateMod)
        syncFaceplateModScheme();
}

juce::Colour SharedColors::randomizeSelectedColorsWithinRange()
{
    auto randomHue = [this]() -> float
    {
        const float range = hueUpperLimit - hueLowerLimit;
        return randomizeHue ? juce::Random::getSystemRandom().nextFloat() * range + hueLowerLimit : hueLowerLimit;
    };
    auto randomSaturation = [this]() -> float
    {
        return randomizeSaturation
                   ? juce::Random::getSystemRandom().nextFloat() * (saturationUpperLimit - saturationLowerLimit) + saturationLowerLimit
                   : saturationLowerLimit;
    };
    auto randomBrightness = [this]() -> float
    {
        return randomizeBrightness
                   ? juce::Random::getSystemRandom().nextFloat() * (brightnessUpperLimit - brightnessLowerLimit) + brightnessLowerLimit
                   : brightnessLowerLimit;
    };

    static const SharedColors factoryDefaults;
    juce::Colour last;
    bool touchedPlugin = false;
    for (int i = 0; i < getNumColors(); ++i)
    {
        // Inverted flag semantics (historical): randomize when flag is FALSE.
        if (i < (int) colorRandomizationFlags.size() && colorRandomizationFlags[(size_t) i])
            continue;
        const auto name = juce::String (ThemeColorRegistry::getEntries()[i].displayName);
        const float keepAlpha = factoryDefaults.colourAt (i).getFloatAlpha();
        last = juce::Colour::fromHSV (randomHue(), randomSaturation(), randomBrightness(), keepAlpha);

        if (name == "Knob Arc")
        {
            const float sat = juce::jmax (0.55f, last.getSaturation());
            const float bri = juce::jlimit (0.45f, 1.0f, last.getBrightness());
            last = juce::Colour::fromHSV (last.getHue(), sat, bri, keepAlpha);
        }
        else if (name == "Knob Multiply")
        {
            const float sat = juce::jlimit (0.05f, 0.40f, last.getSaturation());
            const float bri = juce::jlimit (0.60f, 0.95f, last.getBrightness());
            last = KnobTheme::clampMultiply (juce::Colour::fromHSV (last.getHue(), sat, bri, 1.0f));
        }
        else if (name == "Knob Tint")
        {
            const float sat = juce::jlimit (0.20f, 0.65f, last.getSaturation());
            const float bri = juce::jlimit (0.35f, 0.80f, last.getBrightness());
            const float alpha = 0.12f + juce::Random::getSystemRandom().nextFloat() * 0.26f;
            last = KnobTheme::clampTint (juce::Colour::fromHSV (last.getHue(), sat, bri, alpha));
        }

        colourAt (i) = last;
        if (name.startsWith ("Plugin "))
            touchedPlugin = true;
    }
    syncSharedScopePathColours();
    if (touchedPlugin)
        syncFaceplateModScheme();
    return last;
}
