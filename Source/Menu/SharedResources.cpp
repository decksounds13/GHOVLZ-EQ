#include "SharedResources.h"
#include "../KnobThemeHelpers.h"
#include "BinaryData.h"
#include <cmath>
#include <initializer_list>

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
        { "Menu Section Header",          "MenuSectionHeader",                  &SharedColors::menuSectionHeader },
        { "Menu Section Text",            "MenuSectionText",                    &SharedColors::menuSectionText },
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
        { "Graph Cursor Info",            "GraphCursorInfo",                    &SharedColors::graphCursorInfoText },
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
        { "Spectrum L Fill",              "SpectrumLFill",                      &SharedColors::spectrumFillL },
        { "Spectrum L Line",              "SpectrumLLine",                      &SharedColors::spectrumLineL },
        { "Spectrum Line",                "SpectrumLine",                       &SharedColors::spectrumLine },
        { "Spectrum Mid Fill",            "SpectrumMidFill",                    &SharedColors::spectrumFillMid },
        { "Spectrum Mid Line",            "SpectrumMidLine",                    &SharedColors::spectrumLineMid },
        { "Spectrum R Fill",              "SpectrumRFill",                      &SharedColors::spectrumFillR },
        { "Spectrum R Line",              "SpectrumRLine",                      &SharedColors::spectrumLineR },
        { "Spectrum Side Fill",           "SpectrumSideFill",                   &SharedColors::spectrumFillSide },
        { "Spectrum Side Line",           "SpectrumSideLine",                   &SharedColors::spectrumLineSide },
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
    if (name == "Graph Cursor Info")
        return randomizeGraphCursorInfo;

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

juce::Colour SharedColors::applyGraphBandMinSaturation (juce::Colour c) const noexcept
{
    if (! graphBandRandomMinSatEnabled)
        return c;

    float h = 0.0f, s = 0.0f, v = 0.0f;
    c.getHSB (h, s, v);
    const float minS = juce::jlimit (0.0f, 1.0f, graphBandRandomMinSaturation);
    s = juce::jmax (minS, s);
    return juce::Colour::fromHSV (h, s, v, c.getFloatAlpha());
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

    // Last resort: pure white / black — mid-sat band fills often need this for handle numbers.
    if (contrastRatio (best, background) < minRatio * 0.9f)
    {
        const float a = text.getFloatAlpha();
        const auto white = juce::Colours::white.withAlpha (a);
        const auto black = juce::Colours::black.withAlpha (a);
        best = (contrastRatio (white, background) >= contrastRatio (black, background)) ? white : black;
    }

    return best;
}

juce::Colour ensureTextOnBackgrounds (juce::Colour text,
                                      std::initializer_list<juce::Colour> backgrounds,
                                      float minRatio) noexcept
{
    auto out = text;
    for (int pass = 0; pass < 4; ++pass)
    {
        bool allOk = true;
        for (auto bg : backgrounds)
        {
            if (contrastRatio (out, bg.withAlpha (1.0f)) < minRatio)
            {
                out = ensureTextOnBackground (out, bg.withAlpha (1.0f), minRatio);
                allOk = false;
            }
        }
        if (allOk)
            break;
    }
    return out;
}

juce::Colour nudgeFillAwayFromBackground (juce::Colour fill, juce::Colour background,
                                          float minRatio) noexcept
{
    if (contrastRatio (fill, background) >= minRatio)
        return fill;

    const float h = fill.getHue();
    const float s = juce::jmax (fill.getSaturation(), 0.35f); // keep some chroma
    const float a = fill.getFloatAlpha();
    const float bgY = relativeLuminance (background);
    const bool lighten = bgY < 0.5f;

    auto atV = [&] (float v) noexcept
    {
        return juce::Colour::fromHSV (h, s, juce::jlimit (0.0f, 1.0f, v), a);
    };

    float v = fill.getBrightness();
    for (int i = 0; i < 12; ++i)
    {
        v = lighten ? juce::jmin (1.0f, v + 0.06f) : juce::jmax (0.0f, v - 0.06f);
        const auto cand = atV (v);
        if (contrastRatio (cand, background) >= minRatio)
            return cand;
    }
    return atV (lighten ? 0.92f : 0.18f);
}
} // namespace

void SharedColors::enforceLegibleTextContrast() noexcept
{
    if (! enforceLegibleText)
        return;

    const float minRatio = minRatioFromAmount (textContrastAmount);
    const auto graphBg = blendBg (graphBackground, graphBackground2);
    const auto oscBg = blendBg (oscBackground, oscBackground2);
    const auto gonBg = blendBg (gonBackground, gonBackground2);
    const auto spectrumBg = blendBg (spectrumBackground, spectrumBackground2);
    // Scope cards / maximize fills sit on a slightly darkened osc well.
    const auto scopeWell = oscBackground.darker (0.08f);

    auto fix = [&] (juce::Colour& text, juce::Colour bg)
    {
        text = ensureTextOnBackground (text, bg, minRatio);
    };

    const auto menuBg = blendBg (menuBackgroundGradientColor1, menuBackgroundGradientColor2);
    const auto menuTitleBg = menuBackgroundGradientColor1.brighter (0.08f);

    fix (menuButtonTextColor1, blendBg (menuButtonGradientColor1, menuButtonGradientColor2));
    fix (menuSectionText, menuSectionHeader);
    fix (menuLabelTextColor1, menuBg);
    fix (menuLabelTextColor1, menuTitleBg);
    fix (menuListBoxTextColor1, blendBg (menuListBoxBackgroundGradientColor1, menuListBoxBackgroundGradientColor2));
    fix (menuListBoxTextColor1, menuListBoxSelectionColor1);
    fix (menuTextBoxTextColor1, blendBg (menuBackgroundGradientColor1, menuBackgroundGradientColor2));

    // Faceplate: chrome buttons sit on pluginButtonBackground; labels / Freq-Q-Gain
    // group titles and brand wordmark sit on the plugin faceplate wash.
    const auto pluginFaceBg = blendBg (pluginBackground, pluginBackground2);
    fix (pluginButtonText, pluginButtonBackground);
    fix (pluginButtonText, pluginFaceBg);
    fix (pluginButtonText, pluginBackground);
    fix (pluginButtonText, pluginBackground2);
    fix (pluginPresetText, pluginPresetBackground);
    fix (pluginBrandText, pluginFaceBg);
    fix (pluginBrandText, pluginBackground);

    fix (graphAxisText, graphBg);
    fix (graphCursorInfoText, graphBackground);
    fix (graphCursorInfoText, graphBackground2);
    fix (graphCursorInfoText, graphBg);
    fix (graphCursorInfoText, graphOverlayBackground);
    // Handle numbers are drawn on band-coloured disks (not the graph); global
    // graphHandleText is only a seed — per-handle ink is resolved at paint time.
    // Still fix vs graph for crosshairs / fallbacks that use the same slot.
    fix (graphHandleText, graphBg);
    fix (graphHandleOutline, graphBg);

    // Option box + combo / graph-top UI menus (PluginMenuTheme + PopupMenu LAF).
    fix (optionText, optionBackground);
    fix (optionText, optionBackground.darker (0.35f));
    // Dropdown field must sit off the Settings wash, then ink vs that field
    // and vs Menu Background / Background 2 (the page the list sits on).
    optionComboBackground = nudgeFillAwayFromBackground (
        optionComboBackground.withAlpha (1.0f), menuBg, minRatio);
    optionComboText = ensureTextOnBackgrounds (
        optionComboText,
        { optionComboBackground, menuBackgroundGradientColor1, menuBackgroundGradientColor2, menuBg },
        minRatio);

    // Mod panel: full strip gradient + brighter button faces + column cards.
    fix (modText, modBackground);
    fix (modText, modBackground.darker (0.55f));
    fix (modText, modBackground.brighter (0.15f));
    fix (modText, modBackground.darker (0.35f));

    fix (knobPopupText, knobPopupBackground);
    fix (meterReadoutText, meterBackground);
    // Scope mode modules (level / loudness / histogram / etc.) paint on osc wells.
    fix (meterReadoutText, scopeWell);
    fix (meterReadoutText, oscBg);
    fix (spectrumText, spectrumBg);

    // Scope grid / zoom / goniometer labels share graphAxisText + scopeDropOutline ink.
    fix (graphAxisText, scopeWell);
    fix (graphAxisText, oscBg);
    fix (graphAxisText, gonBg);
    fix (scopeDropOutline, scopeWell);
}

namespace
{
    const juce::StringArray& installedTypefaceNames()
    {
        static const juce::StringArray names = []
        {
            auto list = juce::Font::findAllTypefaceNames();
            list.sort (true);
            return list;
        }();
        return names;
    }

    juce::String alnumLower (const juce::String& s)
    {
        return s.retainCharacters ("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789")
            .toLowerCase();
    }

    juce::Typeface::Ptr bundledPirulenTypeface()
    {
        static juce::Typeface::Ptr tf =
            juce::Typeface::createSystemTypefaceFor (BinaryData::pirulen_ttf,
                                                     BinaryData::pirulen_ttfSize);
        return tf;
    }

    juce::Typeface::Ptr typefaceFromFamilyName (const juce::String& family)
    {
        if (family.isEmpty())
            return {};

        const juce::Font probe (juce::FontOptions().withName (family).withHeight (16.0f));
        auto tf = probe.getTypefacePtr();
        if (tf == nullptr)
            return {};

        // JUCE silently substitutes a default face when the family is missing.
        const auto bound = tf->getName();
        if (bound.isNotEmpty()
            && ! bound.equalsIgnoreCase (family)
            && ! bound.containsIgnoreCase (family)
            && ! family.containsIgnoreCase (bound)
            && alnumLower (bound) != alnumLower (family))
            return {};

        return tf;
    }
}

juce::String SharedColors::resolveUiTypefaceName (const juce::String& requested) noexcept
{
    if (requested.isEmpty() || requested.equalsIgnoreCase ("Pirulen"))
        return requested;

    const auto& installed = installedTypefaceNames();

    auto findExact = [&] (const juce::String& query) -> juce::String
    {
        for (const auto& n : installed)
            if (n.equalsIgnoreCase (query))
                return n;
        return {};
    };

    if (auto hit = findExact (requested); hit.isNotEmpty())
        return hit;

    const auto want = alnumLower (requested);
    if (want.isNotEmpty())
    {
        for (const auto& n : installed)
            if (alnumLower (n) == want)
                return n;
    }

    // Shortest installed name that starts with the request ("Segoe UI Variable" → Display).
    juce::String prefixHit;
    for (const auto& n : installed)
    {
        if (n.startsWithIgnoreCase (requested)
            && (prefixHit.isEmpty() || n.length() < prefixHit.length()))
            prefixHit = n;
    }
    if (prefixHit.isNotEmpty())
        return prefixHit;

    static const std::pair<const char*, const char*> aliases[] = {
        { "Segoe UI Variable", "Segoe UI" },
        { "Helvetica Neue",    "Helvetica" },
        { "Avenir Next",       "Avenir" },
        { "Source Sans 3",     "Source Sans Pro" },
        { "Cascadia Code",     "Cascadia Mono" },
        { "IBM Plex Sans",     "IBM Plex Sans Regular" },
        { "Roboto Condensed",  "Roboto" },
    };

    for (const auto& alias : aliases)
        if (requested.equalsIgnoreCase (alias.first))
            if (auto hit = findExact (alias.second); hit.isNotEmpty())
                return hit;

    return {};
}

bool SharedColors::isUiFontAvailable (const juce::String& catalogueName) noexcept
{
    if (catalogueName.equalsIgnoreCase ("Pirulen"))
        return bundledPirulenTypeface() != nullptr;

    return typefaceFromFamilyName (resolveUiTypefaceName (catalogueName)) != nullptr;
}

juce::StringArray SharedColors::getUiFontCatalogue() noexcept
{
    // Favourites first (Lato / Lato Black / Pirulen / …), then every other
    // installed family so the list can scroll.
    static const juce::String favourites[] = {
        "Lato",
        "Lato Black",
        "Pirulen",
        "Bahnschrift",
        "Segoe UI",
        "Segoe UI Variable",
        "Segoe UI Semibold",
        "Calibri",
        "Candara",
        "Corbel",
        "Century Gothic",
        "Arial",
        "Arial Narrow",
        "Helvetica Neue",
        "Futura",
        "Avenir",
        "Avenir Next",
        "Montserrat",
        "Inter",
        "Roboto",
        "Roboto Condensed",
        "Roboto Medium",
        "Poppins",
        "Manrope",
        "Outfit",
        "DM Sans",
        "Plus Jakarta Sans",
        "Urbanist",
        "Space Grotesk",
        "Space Mono",
        "IBM Plex Sans",
        "IBM Plex Mono",
        "Source Sans 3",
        "Source Sans Pro",
        "Noto Sans",
        "Exo 2",
        "Orbitron",
        "Rajdhani",
        "Titillium Web",
        "Saira",
        "Saira Condensed",
        "Oxanium",
        "Audiowide",
        "Michroma",
        "Syncopate",
        "Chakra Petch",
        "Teko",
        "Quantico",
        "Electrolize",
        "Share Tech Mono",
        "Cascadia Code",
        "Consolas",
        "JetBrains Mono"
    };

    juce::StringArray out;
    auto addUnique = [&out] (const juce::String& name)
    {
        if (name.isNotEmpty() && out.indexOf (name, true) < 0)
            out.add (name);
    };

    for (const auto& name : favourites)
        if (isUiFontAvailable (name))
            addUnique (name);

    for (const auto& name : installedTypefaceNames())
        addUnique (name);

    if (auto* active = SharedResources::getActive())
        addUnique (active->sharedColors.uiFontName);

    if (out.isEmpty())
        out.add ("Segoe UI");

    return out;
}

juce::Font SharedColors::makeNamedUiFont (const juce::String& catalogueName,
                                          float height,
                                          bool bold)
{
    height = juce::jmax (1.0f, height);
    const juce::String requested = catalogueName.isNotEmpty() ? catalogueName : juce::String ("Lato");

    juce::Typeface::Ptr tf;
    if (requested.equalsIgnoreCase ("Pirulen"))
        tf = bundledPirulenTypeface();

    if (tf == nullptr)
    {
        juce::String face = resolveUiTypefaceName (requested);
        if (bold)
        {
            if (requested.equalsIgnoreCase ("Lato") || face.equalsIgnoreCase ("Lato"))
            {
                if (auto black = resolveUiTypefaceName ("Lato Black"); black.isNotEmpty())
                    face = black;
            }
            else if (requested.equalsIgnoreCase ("Segoe UI"))
            {
                if (auto semi = resolveUiTypefaceName ("Segoe UI Semibold"); semi.isNotEmpty())
                    face = semi;
            }
            else if (requested.equalsIgnoreCase ("Roboto"))
            {
                if (auto med = resolveUiTypefaceName ("Roboto Medium"); med.isNotEmpty())
                    face = med;
            }
        }
        tf = typefaceFromFamilyName (face);
    }

    if (tf == nullptr)
        tf = typefaceFromFamilyName (resolveUiTypefaceName ("Lato"));
    if (tf == nullptr)
        tf = typefaceFromFamilyName (resolveUiTypefaceName ("Segoe UI"));
    if (tf == nullptr)
        tf = typefaceFromFamilyName ("Arial");

    if (tf != nullptr)
    {
        auto f = juce::Font (juce::FontOptions (tf).withHeight (height));
        const bool alreadyBoldFace = tf->getName().containsIgnoreCase ("Black")
                                     || tf->getName().containsIgnoreCase ("Bold")
                                     || tf->getName().containsIgnoreCase ("Semibold")
                                     || tf->getName().containsIgnoreCase ("Medium");
        return (bold && ! alreadyBoldFace) ? f.boldened() : f;
    }

    return juce::Font (juce::FontOptions().withHeight (height));
}

juce::Font SharedColors::makeUiFont (float height, bool boldExtra) const
{
    return makeNamedUiFont (uiFontName, height, uiFontBold || boldExtra);
}

bool SharedResources::isSettingsSectionOpen (const juce::String& id, bool defaultOpen) const
{
    const auto it = settingsSectionOpen.find (id);
    if (it == settingsSectionOpen.end())
        return defaultOpen;
    return it->second;
}

void SharedResources::setSettingsSectionOpen (const juce::String& id, bool open)
{
    settingsSectionOpen[id] = open;
}

juce::String SharedResources::encodeSettingsSectionState() const
{
    juce::StringArray parts;
    for (const auto& kv : settingsSectionOpen)
        parts.add (kv.first + "=" + (kv.second ? "1" : "0"));
    return parts.joinIntoString (";");
}

void SharedResources::decodeSettingsSectionState (const juce::String& encoded)
{
    settingsSectionOpen.clear();
    auto parts = juce::StringArray::fromTokens (encoded, ";", "");
    for (const auto& p : parts)
    {
        const int eq = p.indexOfChar ('=');
        if (eq <= 0)
            continue;
        settingsSectionOpen[p.substring (0, eq)] = p.substring (eq + 1).getIntValue() != 0;
    }
}

void SharedResources::applyUiFontsRecursively (juce::Component& root)
{
    // Never walk a live CallOutBox — font hover used to do this and ComboBox
    // lookAndFeelChanged() dismissed the box mid-event (plugin crash).
    if (dynamic_cast<juce::CallOutBox*> (&root) != nullptr)
        return;

    if (auto* label = dynamic_cast<juce::Label*> (&root))
    {
        const float h = label->getFont().getHeight() > 1.0f ? label->getFont().getHeight() : 12.0f;
        // Preserve intentional bold section headers via boldExtra if already boldened height style
        // is unknown — use global bold flag only.
        label->setFont (uiFont (h));
        label->setMinimumHorizontalScale (1.0f);
    }
    else if (auto* editor = dynamic_cast<juce::TextEditor*> (&root))
    {
        const float h = editor->getFont().getHeight() > 1.0f ? editor->getFont().getHeight() : 13.0f;
        editor->applyFontToAllText (uiFont (h));
    }
    else if (auto* button = dynamic_cast<juce::Button*> (&root))
    {
        juce::ignoreUnused (button);
        root.repaint();
    }
    else if (dynamic_cast<juce::ComboBox*> (&root) != nullptr)
    {
        // ComboBox::lookAndFeelChanged() hides the popup. Just repaint.
        root.repaint();
    }
    else
    {
        root.repaint();
    }

    for (int i = 0; i < root.getNumChildComponents(); ++i)
        if (auto* child = root.getChildComponent (i))
            applyUiFontsRecursively (*child);
}

juce::Colour SharedColors::legibleTextOn (juce::Colour text, juce::Colour background) const noexcept
{
    if (! enforceLegibleText)
        return text;

    return ensureTextOnBackground (text, background.withAlpha (1.0f),
                                   minRatioFromAmount (textContrastAmount));
}

juce::Colour SharedColors::dropdownTextOn (juce::Colour text, juce::Colour fieldFill) const noexcept
{
    if (! enforceLegibleText)
        return text;

    const float minRatio = minRatioFromAmount (textContrastAmount);
    const auto menuBg = blendBg (menuBackgroundGradientColor1, menuBackgroundGradientColor2);
    return ensureTextOnBackgrounds (
        text,
        { fieldFill, menuBackgroundGradientColor1, menuBackgroundGradientColor2, menuBg },
        minRatio);
}

juce::Colour SharedColors::legibleFillOn (juce::Colour fill, juce::Colour background) const noexcept
{
    if (! enforceLegibleText)
        return fill;

    return nudgeFillAwayFromBackground (fill.withAlpha (juce::jmax (fill.getFloatAlpha(), 0.92f)),
                                        background.withAlpha (1.0f),
                                        minRatioFromAmount (textContrastAmount));
}

juce::Colour SharedColors::legibleHandleFill (juce::Colour fill, juce::Colour graphBackground) const noexcept
{
    const auto solid = fill.withAlpha (1.0f);
    if (! enforceLegibleText)
        return solid;

    // Softer target than text — preserve band identity, just keep the disk visible.
    constexpr float kHandleMinRatio = 1.85f;
    return nudgeFillAwayFromBackground (solid, graphBackground.withAlpha (1.0f), kHandleMinRatio);
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
        else if (name.startsWith ("Graph Band"))
        {
            colour = applyGraphBandMinSaturation (
                juce::Colour::fromHSV (colour.getHue(), colour.getSaturation(),
                                       colour.getBrightness(), keepAlpha));
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
        else if (name.startsWith ("Graph Band"))
        {
            last = applyGraphBandMinSaturation (
                juce::Colour::fromHSV (last.getHue(), last.getSaturation(),
                                       last.getBrightness(), keepAlpha));
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
