#pragma once

#include <JuceHeader.h>
#include <array>
#include <map>
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
    /** Settings accordion header fill / title (dice Menu scope). */
    juce::Colour menuSectionHeader = createColorWithOptionalAlpha (72, 62, 48, 255);
    juce::Colour menuSectionText = createColorWithOptionalAlpha (245, 245, 245, 230);

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
    /** Floating cursor / crosshair readout next to the pointer. Default white. */
    juce::Colour graphCursorInfoText = createColorWithOptionalAlpha (255, 255, 255, 255);
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
    juce::Colour spectrumLineL = createColorWithOptionalAlpha (70, 190, 220, 255);
    juce::Colour spectrumFillL = createColorWithOptionalAlpha (70, 190, 220, 130);
    juce::Colour spectrumLineR = createColorWithOptionalAlpha (230, 140, 70, 255);
    juce::Colour spectrumFillR = createColorWithOptionalAlpha (230, 140, 70, 130);
    juce::Colour spectrumLineMid = createColorWithOptionalAlpha (90, 200, 120, 255);
    juce::Colour spectrumFillMid = createColorWithOptionalAlpha (90, 200, 120, 130);
    juce::Colour spectrumLineSide = createColorWithOptionalAlpha (200, 90, 210, 255);
    juce::Colour spectrumFillSide = createColorWithOptionalAlpha (200, 90, 210, 130);

    /** Per-slot randomization gates (index = ThemeColorRegistry entry order). */
    std::vector<uint8_t> colorRandomizationFlags;

    /** Rand. All / dice scopes — right-click Rand. All or dice to toggle. */
    bool randomizeFaceplateMod = true;
    bool randomizeGraphModule = true;
    bool randomizeMenuModule = true;
    /** Dice: cursor / crosshair info label colour (separate from Graph). */
    bool randomizeGraphCursorInfo = true;

    /** Dice: per colour-ramp randomization gates (left-click honours these). */
    bool randomizeRampFftBars = true;
    bool randomizeRampSpectrogram = true;
    bool randomizeRampSpectrogram3D = true;
    bool randomizeRampSpectrumFill = true;       // Post Fill
    bool randomizeRampSpectrumCurve = true;      // Post Curve
    bool randomizeRampSpectrumPreFill = true;
    bool randomizeRampSpectrumPreCurve = true;
    bool randomizeRampSpectrumHoldFill = true;
    bool randomizeRampSpectrumHoldCurve = true;
    bool randomizeRampEqCurve = true;            // Sum Curve
    bool randomizeRampEqSumFill = true;
    bool randomizeRampEqBandCurve = true;
    bool randomizeRampEqBandFill = true;
    /** Peak + RMS meter bar ramps (both targets). */
    bool randomizeRampLevelMeters = true;

    /**
        When randomizing ramps: pick H/S/V endpoint spans (within Appearance limits)
        and space stops evenly between them. Off = classic independent random stops.
    */
    bool orderedRampGradation = true;

    /**
        Accessibility: keep text readable on faceplate chrome, mod panel, option box,
        graph-top UI menus / PopupMenus, Scope cards, meters, graph handles, and menus.
        On by default globally (ui_prefs + Appearance); can disable in Appearance.
        After randomize / theme load, adjusts text value (V only) vs its background.
    */
    bool enforceLegibleText = true;
    /** 0 = mild separation, 1 = strong. Only used when enforceLegibleText. */
    float textContrastAmount = 0.55f;

    /**
        Floating OptionBox panel body opacity (0–1). Default 0.90 so the spectrum
        peeks through. Not a theme colour; persisted in ui_prefs. Dice does not touch it.
    */
    float optionBoxOpacity = 0.90f;

    /**
        Chrome button corner radius (px). Appearance → Chrome. Also drives Melatonin
        outline-blur softness so the rim reads against the background. Default 6.
        Does not change graph band handles (those stay circular).
    */
    float buttonCornerRadius = 6.0f;

    /**
        Popup / dropdown panel corner radius (px). Independent of button radius.
        Appearance → Chrome. Default 6. 0 = square menus.
    */
    float menuPopupCornerRadius = 6.0f;
    /** Draw a 1 px outline around popup / dropdown panels. Default on. */
    bool menuPopupOutline = true;

    /**
        Soft Melatonin edge glow on chrome buttons (Appearance → Chrome).
        Still requires the global glow/shadow master switch. Default on.
    */
    bool buttonGlowEnabled = true;
    /**
        When true, button edge glow only paints while hovered or pressed.
        When false, glow is always on (if buttonGlowEnabled). Default on.
    */
    bool buttonGlowOnlyOnHover = true;

    /**
        Global UI typeface name (Appearance → Chrome → UI font).
        System families when installed; "Pirulen" is bundled. Default Lato.
        Used plugin-wide for chrome, menus, scopes, OptionBox, Appearance, etc.
    */
    juce::String uiFontName { "Lato" };

    /**
        When true, makeUiFont() uses a bold weight (or bold style) globally.
        Appearance → Chrome → Bold text. Default off.
    */
    bool uiFontBold = false;

    /**
        Font height for the floating cursor / crosshair info label (Appearance).
        Default 12. Not a theme colour; persisted in ui_prefs.
    */
    float graphCursorInfoFontSize = 12.0f;

    /** Lato / Pirulen / other favourites first, then every installed family. */
    static juce::StringArray getUiFontCatalogue() noexcept;

    /** True if makeNamedUiFont will bind a real face (not a silent default fallback). */
    static bool isUiFontAvailable (const juce::String& catalogueName) noexcept;

    /** Installed family that matches a catalogue name, or empty if none. */
    static juce::String resolveUiTypefaceName (const juce::String& requested) noexcept;

    /**
        Build a UI font by catalogue / family name at height.
        Resolves aliases and installed family names. Missing faces fall back to Lato / Segoe UI.
    */
    static juce::Font makeNamedUiFont (const juce::String& catalogueName,
                                       float height,
                                       bool bold = false);

    /**
        Build the global UI font at height.
        @param boldExtra  force bold even when uiFontBold is off (e.g. section headers).
                          When uiFontBold is on, result is always bold.
    */
    juce::Font makeUiFont (float height, bool boldExtra = false) const;

    /**
        When true, Graph Band 1–8 (and matching faceplate power/glow chrome) never
        drop below graphBandRandomMinSaturation. Spectrum settings; ui_prefs.
    */
    bool graphBandRandomMinSatEnabled = true;
    /**
        Floor on Graph Band saturation (0–1). Default 0.25 so bands never go fully grey.
        Only applied when graphBandRandomMinSatEnabled. Does not change H/S/V range sliders.
    */
    float graphBandRandomMinSaturation = 0.25f;

    /** Enforce min sat on a colour when the Spectrum toggle is on; otherwise returns c. */
    juce::Colour applyGraphBandMinSaturation (juce::Colour c) const noexcept;

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

    /**
        Re-apply text legibility against current backgrounds (text V only).
        No-op when enforceLegibleText is false.
    */
    void enforceLegibleTextContrast() noexcept;

    /**
        Per-pixel / per-widget legibility (e.g. band-handle numbers on multi-colour fills).
        When enforceLegibleText is off, returns text unchanged.
        Adjusts value first (keeps hue); falls back to white/black if still weak.
    */
    juce::Colour legibleTextOn (juce::Colour text, juce::Colour background) const noexcept;

    /**
        Dropdown / popup ink: readable on the field fill and on both Settings
        Menu Background colours (the wash the list sits over).
    */
    juce::Colour dropdownTextOn (juce::Colour text, juce::Colour fieldFill) const noexcept;

    /**
        Nudge a fill (dropdown, chip) off a wash so it still reads as a field.
        No-op when enforceLegibleText is off.
    */
    juce::Colour legibleFillOn (juce::Colour fill, juce::Colour background) const noexcept;

    /**
        Nudge an opaque handle fill so it still reads on the graph background
        without fully discarding the band colour. No-op when enforceLegibleText is off.
    */
    juce::Colour legibleHandleFill (juce::Colour fill, juce::Colour graphBackground) const noexcept;

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

    /**
        Plugin-wide UI font from the active SharedColors (Appearance selector + bold).
        Safe when no instance is active (plain default height).
        Prefer this over hardcoded "Lato" / FontOptions name strings.
    */
    static juce::Font uiFont (float height, bool boldExtra = false)
    {
        if (auto* active = getActive())
            return active->sharedColors.makeUiFont (height, boldExtra);
        return juce::Font (juce::FontOptions (juce::jmax (1.0f, height)));
    }

    /**
        Walk a component tree and push the global UI font onto Labels / TextEditors,
        then repaint Buttons / ComboBoxes so paint-time fonts refresh.
        Call after Appearance font/bold changes.
    */
    static void applyUiFontsRecursively (juce::Component& root);

    /** Settings accordion open flags (ui_prefs). Missing id → defaultOpen. */
    bool isSettingsSectionOpen (const juce::String& id, bool defaultOpen) const;
    void setSettingsSectionOpen (const juce::String& id, bool open);
    juce::String encodeSettingsSectionState() const;
    void decodeSettingsSectionState (const juce::String& encoded);

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
    std::map<juce::String, bool> settingsSectionOpen;
};
