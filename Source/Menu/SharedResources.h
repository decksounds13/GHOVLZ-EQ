#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "ThemeColorRegistry.h"

class SharedColors
{
public:
    float saturationLowerLimit = 0.1f;
    float saturationUpperLimit = 0.35f;
    float hueLowerLimit = 0.0f;
    float hueUpperLimit = 1.0f;
    float brightnessLowerLimit = 0.0f;
    float brightnessUpperLimit = 1.0f;

    bool randomizeHue = true;
    bool randomizeSaturation = true;
    bool randomizeBrightness = true;
    bool randomizeAlpha = true;

    static juce::Colour createColorWithOptionalAlpha (int r, int g, int b, int a)
    {
        if (a == 255)
            return juce::Colour ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b);
        return juce::Colour::fromRGBA ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b, (juce::uint8) a);
    }

    //--------------------------------------------------------------------------
    // Menu
    //--------------------------------------------------------------------------
    juce::Colour menuBackgroundGradientColor1 = createColorWithOptionalAlpha (54, 48, 41, 255);
    juce::Colour menuBackgroundGradientColor2 = createColorWithOptionalAlpha (0, 0, 0, 255);
    juce::Colour menuListBoxBackgroundGradientColor1 = createColorWithOptionalAlpha (60, 47, 39, 255);
    juce::Colour menuListBoxBackgroundGradientColor2 = createColorWithOptionalAlpha (0, 0, 0, 255);
    juce::Colour menuTabBarBorderColor = createColorWithOptionalAlpha (125, 125, 125, 255);
    juce::Colour menuThinBorderColor = createColorWithOptionalAlpha (125, 125, 125, 255);
    juce::Colour menuButtonGradientColor1 = createColorWithOptionalAlpha (54, 48, 41, 255);
    juce::Colour menuButtonGradientColor2 = createColorWithOptionalAlpha (0, 0, 0, 255);
    juce::Colour menuButtonTextColor1 = createColorWithOptionalAlpha (245, 245, 245, 217);
    juce::Colour menuLabelTextColor1 = createColorWithOptionalAlpha (245, 245, 245, 217);
    juce::Colour menuScrollBarTrackColor1 = createColorWithOptionalAlpha (80, 80, 80, 255);
    juce::Colour menuScrollBarThumbColor1 = createColorWithOptionalAlpha (140, 140, 140, 255);
    juce::Colour menuScrollBarOutlineColor1 = createColorWithOptionalAlpha (20, 20, 20, 255);
    juce::Colour menuListBoxTextColor1 = createColorWithOptionalAlpha (245, 245, 245, 255);
    juce::Colour menuListBoxSelectionColor1 = createColorWithOptionalAlpha (220, 220, 220, 255);
    juce::Colour menuTextBoxTextColor1 = createColorWithOptionalAlpha (245, 245, 245, 217);
    juce::Colour menuSliderFillColor = createColorWithOptionalAlpha (218, 165, 32, 255); // goldenrod

    //--------------------------------------------------------------------------
    // Plugin chrome
    //--------------------------------------------------------------------------
    juce::Colour pluginBackground = createColorWithOptionalAlpha (10, 10, 10, 255);
    juce::Colour pluginBackground2 = createColorWithOptionalAlpha (60, 55, 50, 255);
    juce::Colour pluginButtonBackground = createColorWithOptionalAlpha (60, 50, 35, 255);
    juce::Colour pluginButtonAccent = createColorWithOptionalAlpha (180, 150, 55, 255);
    juce::Colour pluginButtonText = createColorWithOptionalAlpha (245, 245, 245, 255);
    juce::Colour pluginPresetBackground = createColorWithOptionalAlpha (40, 32, 24, 255);
    juce::Colour pluginPresetText = createColorWithOptionalAlpha (245, 245, 245, 204);
    juce::Colour pluginBrandText = createColorWithOptionalAlpha (245, 245, 245, 255);

    //--------------------------------------------------------------------------
    // Graph / EQ response
    //--------------------------------------------------------------------------
    juce::Colour graphBackground = createColorWithOptionalAlpha (10, 10, 10, 255);
    juce::Colour graphBackground2 = createColorWithOptionalAlpha (60, 55, 50, 255);
    juce::Colour graphGrid = createColorWithOptionalAlpha (245, 245, 245, 40);
    juce::Colour graphAxisText = createColorWithOptionalAlpha (245, 245, 245, 180);
    juce::Colour graphSumCurve = createColorWithOptionalAlpha (218, 165, 32, 204);
    juce::Colour graphSumFillTop = createColorWithOptionalAlpha (255, 130, 30, 180);
    juce::Colour graphSumFillBottom = createColorWithOptionalAlpha (139, 105, 20, 102);
    juce::Colour graphSumGlow = createColorWithOptionalAlpha (255, 180, 60, 130);
    juce::Colour graphBand1 = createColorWithOptionalAlpha (100, 149, 237, 102);
    juce::Colour graphBand2 = createColorWithOptionalAlpha (128, 0, 128, 102);
    juce::Colour graphBand3 = createColorWithOptionalAlpha (0, 255, 255, 102);
    juce::Colour graphBand4 = createColorWithOptionalAlpha (0, 0, 255, 89);
    juce::Colour graphBand5 = createColorWithOptionalAlpha (0, 128, 0, 89);
    juce::Colour graphBand6 = createColorWithOptionalAlpha (255, 0, 0, 115);
    juce::Colour graphBand7 = createColorWithOptionalAlpha (222, 184, 135, 128);
    juce::Colour graphBand8 = createColorWithOptionalAlpha (205, 92, 92, 128);
    juce::Colour graphHandleOutline = createColorWithOptionalAlpha (1, 1, 1, 255);
    juce::Colour graphHandleText = createColorWithOptionalAlpha (229, 189, 128, 255);
    juce::Colour graphOverlayBackground = createColorWithOptionalAlpha (65, 60, 55, 160);
    juce::Colour graphOverlayBorder = createColorWithOptionalAlpha (20, 10, 5, 150);

    //--------------------------------------------------------------------------
    // Option box
    //--------------------------------------------------------------------------
    juce::Colour optionBackground = createColorWithOptionalAlpha (28, 24, 20, 230);
    juce::Colour optionBorder = createColorWithOptionalAlpha (20, 10, 5, 180);
    juce::Colour optionText = createColorWithOptionalAlpha (245, 245, 245, 217);
    juce::Colour optionComboBackground = createColorWithOptionalAlpha (40, 32, 24, 255);
    juce::Colour optionComboHighlight = createColorWithOptionalAlpha (180, 150, 55, 255);
    juce::Colour optionComboText = createColorWithOptionalAlpha (245, 245, 245, 230);

    //--------------------------------------------------------------------------
    // Knobs
    //--------------------------------------------------------------------------
    juce::Colour knobArc = createColorWithOptionalAlpha (255, 110, 0, 255);
    /** Near-white = little change; RGB acts as a multiply filter on the knob artwork. */
    juce::Colour knobMultiply = createColorWithOptionalAlpha (255, 255, 255, 255);
    /** Soft colour wash over artwork; alpha controls strength (kept moderate). */
    juce::Colour knobTint = juce::Colour::fromRGBA ((juce::uint8) 255, (juce::uint8) 110, (juce::uint8) 0, (juce::uint8) 0);
    juce::Colour knobPopupBackground = createColorWithOptionalAlpha (0, 0, 0, 180);
    juce::Colour knobPopupText = createColorWithOptionalAlpha (245, 245, 245, 255);

    //--------------------------------------------------------------------------
    // Meters
    //--------------------------------------------------------------------------
    juce::Colour meterBackground = createColorWithOptionalAlpha (20, 18, 14, 255);
    juce::Colour meterFill = createColorWithOptionalAlpha (180, 150, 55, 255);
    juce::Colour meterClip = createColorWithOptionalAlpha (220, 40, 40, 255);
    /** Shared numerical readout colour (level meters, loudness, histogram labels). */
    juce::Colour meterReadoutText = createColorWithOptionalAlpha (245, 245, 245, 235);

    //--------------------------------------------------------------------------
    // Oscilloscope
    //--------------------------------------------------------------------------
    juce::Colour oscBackground = createColorWithOptionalAlpha (12, 10, 8, 255);
    juce::Colour oscBackground2 = createColorWithOptionalAlpha (40, 32, 24, 255);
    juce::Colour oscLine = createColorWithOptionalAlpha (220, 190, 120, 255);
    juce::Colour oscGlow = createColorWithOptionalAlpha (220, 190, 120, 160);

    //--------------------------------------------------------------------------
    // Goniometer
    //--------------------------------------------------------------------------
    juce::Colour gonBackground = createColorWithOptionalAlpha (12, 10, 8, 255);
    juce::Colour gonBackground2 = createColorWithOptionalAlpha (40, 32, 24, 255);
    juce::Colour gonLine = createColorWithOptionalAlpha (220, 190, 120, 255);
    juce::Colour gonGlow = createColorWithOptionalAlpha (220, 190, 120, 160);
    juce::Colour gonCorrPositive = createColorWithOptionalAlpha (80, 200, 100, 255);
    juce::Colour gonCorrNegative = createColorWithOptionalAlpha (220, 70, 70, 255);

    /** Drop-target outline while rearranging Scope mode panes (dice / Graph module). */
    juce::Colour scopeDropOutline = createColorWithOptionalAlpha (220, 190, 90, 255);

    //--------------------------------------------------------------------------
    // Mod section (kept in family with Plugin chrome — syncFaceplateModScheme)
    //--------------------------------------------------------------------------
    juce::Colour modBackground = createColorWithOptionalAlpha (60, 55, 50, 255);
    juce::Colour modBorder = createColorWithOptionalAlpha (60, 50, 35, 255);
    juce::Colour modText = createColorWithOptionalAlpha (245, 245, 245, 217);
    juce::Colour modAccent = createColorWithOptionalAlpha (180, 150, 55, 255);

    //--------------------------------------------------------------------------
    // Spectrum analyser
    //--------------------------------------------------------------------------
    juce::Colour spectrumBackground = createColorWithOptionalAlpha (10, 10, 10, 255);
    juce::Colour spectrumBackground2 = createColorWithOptionalAlpha (60, 55, 50, 255);
    juce::Colour spectrumGrid = createColorWithOptionalAlpha (70, 70, 70, 255);
    juce::Colour spectrumText = createColorWithOptionalAlpha (132, 132, 132, 255);
    juce::Colour spectrumLine = createColorWithOptionalAlpha (90, 160, 90, 255);
    juce::Colour spectrumFill = createColorWithOptionalAlpha (255, 90, 40, 160);

    /** Per-slot randomization gates (index = ThemeColorRegistry entry order). */
    std::vector<uint8_t> colorRandomizationFlags;

    /** Rand. All / dice scopes — right-click Rand. All or dice to toggle. */
    bool randomizeFaceplateMod = true;
    bool randomizeGraphModule = true;
    bool randomizeMenuModule = true;

    /** Dice: per colour-ramp randomization gates (left-click honours these). */
    bool randomizeRampFftBars = true;
    bool randomizeRampSpectrogram = true;
    bool randomizeRampSpectrogram3D = true;
    bool randomizeRampSpectrumFill = true;

    SharedColors();

    void setRandomizeHue (bool enabled) { randomizeHue = enabled; }
    void setRandomizeSaturation (bool enabled) { randomizeSaturation = enabled; }
    void setRandomizeBrightness (bool enabled) { randomizeBrightness = enabled; }
    void setRandomizeAlpha (bool enabled) { randomizeAlpha = enabled; }

    /** Keep mod panel colours in the same family as plugin / faceplate chrome. */
    void syncFaceplateModScheme() noexcept;

    /** Classify a registry display name into a Rand. All scope. */
    enum class RandomizeModule { FaceplateMod, Graph, Menu, Other };
    static RandomizeModule moduleForDisplayName (const juce::String& displayName) noexcept;
    bool shouldRandomizeIndex (int index) const noexcept;

    void setBrightnessRange (float lower, float upper)
    {
        brightnessLowerLimit = lower;
        brightnessUpperLimit = upper;
    }

    void setHueRange (float lower, float upper)
    {
        hueLowerLimit = lower;
        hueUpperLimit = upper;
    }

    void setSaturationRange (float lower, float upper)
    {
        saturationLowerLimit = lower;
        saturationUpperLimit = upper;
    }

    int getNumColors() const noexcept { return ThemeColorRegistry::getNumEntries(); }

    juce::Colour& colourAt (int index);
    const juce::Colour& colourAt (int index) const;
    juce::Colour* findByDisplayName (const juce::String& name);
    const juce::Colour* findByDisplayName (const juce::String& name) const;

    /** Writes colour if force, or if colorRandomizationFlags[index] is set. */
    void setColourAt (int index, juce::Colour newColor, bool force = false);
    void setColourByDisplayName (const juce::String& name, juce::Colour newColor, bool force = false);

    void toggleColorRandomizationFlags (const juce::Array<int>& paletteIndices);
    void updateColorsDirectly (juce::Colour newColor, const juce::Array<int>& paletteIndices);
    void randomizeColors();
    juce::Colour randomizeSelectedColorsWithinRange();

    /** One HSV colour inside the Appearance H/S/V limit sliders (and H/S/V gates). */
    juce::Colour randomColourInLimits (float alpha = 1.0f) const;

    /** Keep gonLine/gonGlow mirrored to oscLine/oscGlow. */
    void syncSharedScopePathColours() noexcept;

    // Legacy setters (gated by flags) — kept for Menu::updateColors etc.
    void setMenuBackgroundGradientColor1 (juce::Colour c) { setColourByDisplayName ("Menu Background", c); }
    void setMenuBackgroundGradientColor2 (juce::Colour c) { setColourByDisplayName ("Menu Background 2", c); }
    void setMenuListBoxBackgroundGradientColor1 (juce::Colour c) { setColourByDisplayName ("Menu ListBox Background", c); }
    void setMenuListBoxBackgroundGradientColor2 (juce::Colour c) { setColourByDisplayName ("Menu ListBox Background 2", c); }
    void setMenuTabBarBorderColor (juce::Colour c) { setColourByDisplayName ("Menu Border", c); }
    void setMenuThinBorderColor (juce::Colour c) { setColourByDisplayName ("Menu Thin Border", c); }
    void setMenuButtonGradientColor1 (juce::Colour c) { setColourByDisplayName ("Menu Button Background", c); }
    void setMenuButtonGradientColor2 (juce::Colour c) { setColourByDisplayName ("Menu Button Background 2", c); }
    void setMenuButtonTextColor1 (juce::Colour c) { setColourByDisplayName ("Menu Button Text", c); }
    void setMenuLabelTextColor1 (juce::Colour c) { setColourByDisplayName ("Menu Label Text", c); }
    void setMenuScrollBarTrackColor1 (juce::Colour c) { setColourByDisplayName ("Menu Scroll Track", c); }
    void setMenuScrollBarThumbColor1 (juce::Colour c) { setColourByDisplayName ("Menu Scroll Thumb", c); }
    void setMenuScrollBarOutlineColor1 (juce::Colour c) { setColourByDisplayName ("Menu Scroll Outline", c); }
    void setMenuListBoxTextColor1 (juce::Colour c) { setColourByDisplayName ("Menu ListBox Text", c); }
    void setMenuListBoxSelectionColor1 (juce::Colour c) { setColourByDisplayName ("Menu ListBox Selection", c); }
    void setMenuTextBoxTextColor1 (juce::Colour c) { setColourByDisplayName ("Menu TextBox Text", c); }
};

class SharedResources
{
public:
    SharedColors sharedColors;

    SharedResources() { makeActive(); }
    ~SharedResources()
    {
        if (activeInstance == this)
            activeInstance = nullptr;
    }

    /** Live theme used by knobs/meters when a local themeColors pointer was never set. */
    void makeActive() noexcept { activeInstance = this; }
    static SharedResources* getActive() noexcept { return activeInstance; }

    /** Global UI toggle — skips Melatonin glow / drop-shadow passes when false. */
    bool disableGlowShadowEffects = false;

    static bool glowShadowEffectsEnabled() noexcept
    {
        if (auto* active = getActive())
            return ! active->disableGlowShadowEffects;
        return true;
    }

    static SharedColors getDefaultTheme()
    {
        return SharedColors{};
    }

private:
    static SharedResources* activeInstance;
};
