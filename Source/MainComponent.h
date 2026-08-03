#ifndef MAINCOMPONENT_H
#define MAINCOMPONENT_H

#include <JuceHeader.h>
#include <array>
#include "FrequencyResponseComponent.h"
#include "VerticalGradientMeter.h"
#include "SettingsButtonLookAndFeel.h"
#include "Visualizer/Visualizer.h"
#include "Controls/Controls.h"
#include "Menu/SharedResources.h"
#include "EqProcessor.h"
#include "Menu/Menu.h"
#include "Menu/Gui/ThemeList.h"
#include "OscilloscopeComponent.h"
#include "GoniometerComponent.h"
#include "SpectrogramComponent.h"
#include "Spectrogram3DComponent.h"
#include "FramedFloatingScopeWindow.h"
#include "ScopeModules.h"
#include "ScopeLayoutPresets.h"
#include "ScopeLevelMeterModule.h"
#include "LoudnessComponent.h"
#include "StereogramComponent.h"
#include "HistogramComponent.h"
#include "EqPresetStore.h"
#include "ColourRamp/ColourRampBank.h"
#include "ColourRamp/PathSampleOverlay.h"
#include "GraphOverlayButtonLookAndFeel.h"

class MainComponent : public juce::Component,
    public juce::ComponentListener,
    public ThemeList::Listener,
    public juce::TextEditor::Listener,
    public juce::ChangeListener,
    public juce::AudioProcessorValueTreeState::Listener
{
public:
    MainComponent(EqProcessor& p, Analyser& analyser, juce::AudioProcessorValueTreeState& treeState, EqEditor& editor);
    ~MainComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;

    void parameterChanged (const juce::String& parameterID, float newValue) override;

    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseDown(const juce::MouseEvent& event) override;

    void setOptionBoxInteractionFaded (bool shouldFade);
    void setBandManipulationHighlight (int bandIndex);

    /** Host the brand wordmark above the graph but beneath Settings / menu / preset chrome. */
    void hostBrandWordmark (juce::Component& wordmark);
    /** Wordmark under chrome; open menu covers preset bar; Settings stays above the menu. */
    void raiseMenuSystemAboveWordmark();
    /** Call after wordmark bounds change so the preset bar sits under the logo. */
    void relayoutPresetChrome();
    bool isMenuVisible() const noexcept { return menu.isVisible(); }
    /** Place/clamp the floating Settings panel (fixed content size, free outer aspect). */
    void layoutSettingsMenu();

    /** Select a ramp target, close Settings if open, then start UI path sampling. */
    void beginRampSamplingForTarget (ColourRampBank::Target target);
    void setOrderedRampGradation (bool shouldEnable, bool notifyPrefs = true);
    bool isOrderedRampGradation() const noexcept;

    FrequencyResponseComponent& getFrequencyResponseComponent() noexcept { return frequencyResponseComponent; }

    void onPresetApplied (const Theme& theme) override;
    void onPresetListChanged() override;

    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorFocusLost (juce::TextEditor&) override;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void componentMovedOrResized (juce::Component& component, bool wasMoved, bool wasResized) override;

    /** Eco mode (FFT/analyser off + all scopes off). Persisted via EqEditor ui_prefs. Default off. */
    void setEcoMode (bool shouldEnable, bool notifyPrefs = true);
    bool isEcoMode() const noexcept { return ecoEnabled; }

    /** Quad Scope view: Gon | Spectrum / Osc | Spec. Pre = analyzer dry; Post = EQ DSP on. */
    void setScopeMode (bool shouldEnable, bool notifyPrefs = true);
    bool isScopeMode() const noexcept { return scopeModeEnabled; }

    /** Scope arrange: false = 2×2 quad, true = side-by-side strip (EQ graph hidden). */
    void setScopeStripLayout (bool shouldUseStrip, bool notifyPrefs = true);
    bool isScopeStripLayout() const noexcept { return scopeStripLayout; }

    /** Strip height in design pixels (default 200). Drag the strip bottom edge to resize. */
    void setScopeStripHeightPx (int heightDesignPx, bool notifyPrefs = true);
    /** Update stored strip height from the current window size (no layout side-effects). */
    void syncStripHeightFromWindow (int windowW, int windowH) noexcept;
    int getScopeStripHeightPx() const noexcept { return scopeStripHeightPx; }

    /** Enabled module order for Scope panes (see ScopeModuleId). */
    const std::vector<ScopeModuleId>& getScopeEnabledOrder() const noexcept { return scopeEnabledOrder; }
    void setScopeEnabledOrder (const std::vector<ScopeModuleId>& order, bool notifyPrefs = true);

    bool isScopeModuleEnabled (ScopeModuleId id) const noexcept;
    void setScopeModuleEnabled (ScopeModuleId id, bool enabled, bool notifyPrefs = true);
    /** Right-click module menu (tiled + strip): module actions + Remove Module. */
    void showScopeModuleContextMenu (ScopeModuleId id, juce::Component* anchor);

    /** Scope-mode tap only (compact chrome scopes unchanged). Persisted via ui_prefs. */
    void setScopeTapPost (bool shouldTapPost, bool notifyPrefs = true);
    bool isScopeTapPost() const noexcept { return scopeTapPost; }
    /** Scope + Pre: analyzer-only (DSP off) — hide EQ chrome. */
    bool isScopeAnalyzerOnly() const noexcept { return scopeModeEnabled && ! scopeTapPost; }
    /** Strip Scope (or Pre): hide phase / Side Check / DSP chrome. */
    bool shouldHideScopeDspChrome() const noexcept
    {
        return isScopeAnalyzerOnly() || (scopeModeEnabled && scopeStripLayout);
    }

    ScopeLayoutPreset captureScopeLayoutPreset (const juce::String& name) const;
    void applyScopeLayoutPreset (const ScopeLayoutPreset& preset, bool notifyPrefs = true);
    float getScopeSplitX() const noexcept { return scopeSplitOverlay.getSplitX(); }
    float getScopeSplitY() const noexcept { return scopeSplitOverlay.getSplitY(); }
    void setScopeSplitNorm (float xNorm, float yNorm) noexcept;

    const std::vector<float>& getScopeStripFractions() const noexcept { return scopeStripFractions; }
    void setScopeStripFractions (const std::vector<float>& fracs);

    /** Global Melatonin glow / drop-shadow bypass. Persisted via ui_prefs. */
    void setDisableGlowShadowEffects (bool shouldDisable, bool notifyPrefs = true);
    bool areGlowShadowEffectsDisabled() const noexcept;

    SharedResources& getSharedResources() noexcept { return sharedResources; }
    const SharedResources& getSharedResources() const noexcept { return sharedResources; }

    /** Expanded/Scope OpenGL spectrogram heightfield. Compact strip stays 2D. */
    void setSpec3DMode (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DMode() const noexcept { return spec3DEnabled; }
    void setSpec3DMeshQuality (Spectrogram3DComponent::MeshQuality q, bool notifyPrefs = true);
    void setSpec3DFreqMeshBias (float amount01, bool notifyPrefs = true);
    float getSpec3DFreqMeshBias() const noexcept;
    Spectrogram3DComponent::MeshQuality getSpec3DMeshQuality() const noexcept;
    void setSpec3DMsaaLevel (Spectrogram3DComponent::MsaaLevel level, bool notifyPrefs = true);
    Spectrogram3DComponent::MsaaLevel getSpec3DMsaaLevel() const noexcept;
    bool isSpec3DMultisampling() const noexcept;
    void setSpec3DTransparentBackground (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DTransparentBackground() const noexcept;
    void setSpec3DReverseFrequencyAxis (bool shouldReverse, bool notifyPrefs = true);
    bool isSpec3DReverseFrequencyAxis() const noexcept;
    void setSpec3DMeshHeight (float heightWorld, bool notifyPrefs = true);
    float getSpec3DMeshHeight() const noexcept;

    void setSpec3DLightingEnabled (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DLightingEnabled() const noexcept;
    void setSpec3DLightingAmount (float amount01, bool notifyPrefs = true);
    float getSpec3DLightingAmount() const noexcept;
    void setSpec3DLightAzimuthDeg (float deg, bool notifyPrefs = true);
    float getSpec3DLightAzimuthDeg() const noexcept;
    void setSpec3DLightElevationDeg (float deg, bool notifyPrefs = true);
    float getSpec3DLightElevationDeg() const noexcept;
    void setSpec3DSpecularAmount (float amount01, bool notifyPrefs = true);
    float getSpec3DSpecularAmount() const noexcept;
    void setSpec3DRoughnessAmount (float amount01, bool notifyPrefs = true);
    float getSpec3DRoughnessAmount() const noexcept;
    void setSpec3DRimAmount (float amount01, bool notifyPrefs = true);
    float getSpec3DRimAmount() const noexcept;
    void setSpec3DContactShadowEnabled (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DContactShadowEnabled() const noexcept;
    void setSpec3DContactShadowStrength (float amount01, bool notifyPrefs = true);
    float getSpec3DContactShadowStrength() const noexcept;
    void setSpec3DSelfShadowEnabled (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DSelfShadowEnabled() const noexcept;
    void setSpec3DSelfShadowStrength (float amount01, bool notifyPrefs = true);
    float getSpec3DSelfShadowStrength() const noexcept;
    void setSpec3DSelfShadowBias (float bias01, bool notifyPrefs = true);
    float getSpec3DSelfShadowBias() const noexcept;
    void setSpec3DSelfShadowSoftness (float amount01, bool notifyPrefs = true);
    float getSpec3DSelfShadowSoftness() const noexcept;
    void setSpec3DSelfShadowQuality (Spectrogram3DComponent::ShadowQuality q, bool notifyPrefs = true);
    Spectrogram3DComponent::ShadowQuality getSpec3DSelfShadowQuality() const noexcept;
    void setSpec3DSsaoEnabled (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DSsaoEnabled() const noexcept;
    void setSpec3DSsaoStrength (float amount01, bool notifyPrefs = true);
    float getSpec3DSsaoStrength() const noexcept;
    void setSpec3DSsaoRadius (float radius, bool notifyPrefs = true);
    float getSpec3DSsaoRadius() const noexcept;
    void setSpec3DBloomEnabled (bool shouldEnable, bool notifyPrefs = true);
    bool isSpec3DBloomEnabled() const noexcept;
    void setSpec3DBloomStrength (float amount01, bool notifyPrefs = true);
    float getSpec3DBloomStrength() const noexcept;
    void setSpec3DBloomThreshold (float amount01, bool notifyPrefs = true);
    float getSpec3DBloomThreshold() const noexcept;

    void resetSpec3DCamera() noexcept { spectrogram3D.resetCamera(); }
    void setSpec3DDefaultCamera (const Spectrogram3DComponent::CameraState& state, bool applyNow = true) noexcept;
    Spectrogram3DComponent::CameraState getSpec3DDefaultCamera() const noexcept;

    void setOscExpanded (bool shouldExpand, bool notifyPrefs = true);
    void setGonExpanded (bool shouldExpand, bool notifyPrefs = true);
    void setSpecExpanded (bool shouldExpand, bool notifyPrefs = true);
    bool isOscExpanded() const noexcept { return oscExpanded; }
    bool isGonExpanded() const noexcept { return gonExpanded; }
    bool isSpecExpanded() const noexcept { return specExpanded; }
    bool isOscFullGraph() const noexcept { return oscFullGraph; }
    bool isGonFullGraph() const noexcept { return gonFullGraph; }
    bool isSpecFullGraph() const noexcept { return specFullGraph; }
    void toggleOscFullGraph();
    void toggleGonFullGraph();
    void toggleSpecFullGraph();
    /** Restore a maximized overlay after editor reopen (enables the module if needed). */
    void restoreExpandedScope (bool osc, bool gon, bool spec);
    void closeSettingsMenu();
    /** True when a Settings dismiss-catcher point lies over an expanded analyser window / its tools. */
    bool isPointOverSettingsDismissExempt (int catcherX, int catcherY,
                                           const juce::Component& catcher) const noexcept;

    /** Union of OSC / Gon / Spec toggle bounds (MainComponent local) for Scope chrome alignment. */
    juce::Rectangle<int> getAnalyserToggleColumnBounds() const noexcept
    {
        return oscButton.getBounds()
            .getUnion (gonButton.getBounds())
            .getUnion (specButton.getBounds());
    }

private:
    /** TextButton that exposes a right-click callback for A/B/C/D snapshot menus. */
    class AbSlotButton : public juce::TextButton
    {
    public:
        AbSlotButton (const juce::String& name) : juce::TextButton (name) {}

        std::function<void()> onPopupMenu;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu())
            {
                if (onPopupMenu != nullptr)
                    onPopupMenu();
                return;
            }

            juce::TextButton::mouseDown (e);
        }
    };

    void styleChromeButton (juce::TextButton& button);
    void stylePresetNameEditor();
    void applyThemeToChildComponents();
    void syncAbButtons();
    void syncUndoRedoButtons();
    void showAbMenu (EqProcessor::AbSlot slot);
    void refreshPresetNameDisplay();
    void commitPresetNameEdit();
    void showPresetPopupMenu();
    void showUiThemePopupMenu();
    void randomizeUiTheme();
    /** Left-click dice: randomize every checked UI scope + colour ramp. */
    void runDiceRandomize();
    void showRandomizeDiceMenu();
    void randomizeColourRamps();
    void disableCustomColourRamps();
    void persistSessionUiTheme();
    void restoreSessionUiThemeIfAny();
    void cycleEqPreset (int delta);
    void saveCurrentEqPreset();
    void layoutPresetChrome (float scale);
    void applyEcoMode (bool shouldEnable);
    void applyScopeMode (bool shouldEnable);
    void layoutScopeModePanes (float scale);
    void placeScopePane (ScopeModuleId moduleId, juce::Rectangle<int> pane, int toolH, int toolSize, int toolGap);
    void placeSpectrogram3DPane (juce::Rectangle<int> view, juce::Rectangle<int> overlayTools,
                                 int toolH, int toolSize, int toolGap);
    void syncScopeModuleEnabledStates();
    void applyScopePaneReorder (int fromSlot, int toSlot, bool insertBefore);
    void syncMeterChannelModeButton();
    void toggleMeterChannelMode();
    void syncOscToolButtons();
    void syncGonToolButtons();
    void syncSpecToolButtons();
    void syncSpec3DPresentation();
    void raiseSpecToolButtons();
    /** Expanded Osc/Gon/Spec bounds — excludes piano strip when open. */
    juce::Rectangle<int> getExpandedScopeContentBounds() const;
    /** Bottom of top chrome (bypass/presets/settings) — GL must stay below this. */
    int getTopChromeClearY() const;
    /**
        Expanded Spec: place view + tool column with zero overlap.
        Required for 3D — OpenGL uses a native HWND that ignores JUCE z-order.
    */
    void layoutExpandedSpectrogramWithTools (int btnSize, int btnGap);
    void syncScopeChromeButtonOpacity();
    bool collapseAnyExpandedScope();
    void syncExpandedOscOverlayStack();
    void beginRampSampling();
    void applyColourRampsToMeters();
    void hostOptionBoxAboveExpandedOsc (bool shouldHost);
    void applyGoniometerActive (bool shouldEnable);
    void applySpectrogramActive (bool shouldEnable);
    void disableAllScopes();

    static constexpr double m_marginInPixels{ 10 };

    /** Square chrome button that paints + / - / ST / L/R / ^ / v (never ellipsis). */
    class OscToolButton : public juce::Button
    {
    public:
        enum class Glyph { Plus, Minus, SummedStereo, SplitStereo, Expand, Collapse, Dice,
                           StripLayout, GridLayout, Cube };

        explicit OscToolButton (Glyph g)
            : juce::Button ({}), glyph (g)
        {
            setClickingTogglesState (false);
            // Melatonin drop extends past local bounds; without this the blur is clipped away.
            setPaintingIsUnclipped (true);
        }

        void setGlyph (Glyph g)
        {
            if (glyph == g)
                return;
            glyph = g;
            repaint();
        }

        void setThemeResources (SharedResources* resources) noexcept
        {
            themeResources = resources;
            repaint();
        }

        std::function<void()> onPopupMenu;

        void mouseDown (const juce::MouseEvent& e) override
        {
            if (e.mods.isPopupMenu() && onPopupMenu != nullptr)
            {
                onPopupMenu();
                return;
            }

            juce::Button::mouseDown (e);
        }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            static const SharedColors defaultColors;
            const auto& pal = themeResources != nullptr ? themeResources->sharedColors : defaultColors;

            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float corner = 3.0f;
            auto fill = pal.pluginButtonBackground;
            if (getToggleState())
                fill = pal.pluginButtonAccent;
            else if (down)
                fill = fill.brighter (0.15f);
            else if (highlighted)
                fill = fill.brighter (0.08f);

            GraphOverlayButtonLookAndFeel::renderRoundedDrop (g, r, corner);
            g.setColour (fill);
            g.fillRoundedRectangle (r, corner);
            g.setColour (pal.pluginButtonAccent.withAlpha (200.0f / 255.0f));
            g.drawRoundedRectangle (r, corner, 1.0f);

            const auto ink = getToggleState() ? juce::Colours::black
                                              : pal.pluginButtonText.withAlpha (0.9f);
            g.setColour (ink);

            const float s = juce::jmin (r.getWidth(), r.getHeight());
            const auto c = r.getCentre();
            const float arm = s * 0.28f;
            const float thick = juce::jmax (1.5f, s * 0.12f);

            switch (glyph)
            {
                case Glyph::Plus:
                    g.fillRect (c.x - arm, c.y - thick * 0.5f, arm * 2.0f, thick);
                    g.fillRect (c.x - thick * 0.5f, c.y - arm, thick, arm * 2.0f);
                    break;
                case Glyph::Minus:
                    g.fillRect (c.x - arm, c.y - thick * 0.5f, arm * 2.0f, thick);
                    break;
                case Glyph::SummedStereo:
                case Glyph::SplitStereo:
                {
                    const bool split = (glyph == Glyph::SplitStereo);
                    const float fontH = s * (split ? 0.38f : 0.42f);
                    g.setFont (juce::FontOptions().withHeight (fontH).withStyle ("Bold"));
                    g.drawText (split ? "L/R" : "ST",
                                getLocalBounds(),
                                juce::Justification::centred,
                                false);
                    break;
                }
                case Glyph::Expand:
                case Glyph::Collapse:
                {
                    juce::Path chevron;
                    const float dir = (glyph == Glyph::Expand) ? -1.0f : 1.0f;
                    const float y0 = c.y + dir * arm * 0.55f;
                    const float y1 = c.y - dir * arm * 0.55f;
                    chevron.startNewSubPath (c.x - arm, y0);
                    chevron.lineTo (c.x, y1);
                    chevron.lineTo (c.x + arm, y0);
                    g.strokePath (chevron, juce::PathStrokeType (thick,
                                                                juce::PathStrokeType::curved,
                                                                juce::PathStrokeType::rounded));
                    break;
                }
                case Glyph::Dice:
                {
                    const float die = s * 0.52f;
                    auto dieR = juce::Rectangle<float> (c.x - die * 0.5f, c.y - die * 0.5f, die, die);
                    g.drawRoundedRectangle (dieR, die * 0.12f, juce::jmax (1.2f, thick * 0.65f));
                    const float dot = juce::jmax (1.2f, die * 0.12f);
                    auto paintDot = [&] (float nx, float ny)
                    {
                        g.fillEllipse (c.x + nx * die * 0.22f - dot * 0.5f,
                                       c.y + ny * die * 0.22f - dot * 0.5f,
                                       dot, dot);
                    };
                    paintDot (-1.0f, -1.0f);
                    paintDot (1.0f, -1.0f);
                    paintDot (0.0f, 0.0f);
                    paintDot (-1.0f, 1.0f);
                    paintDot (1.0f, 1.0f);
                    break;
                }
                case Glyph::StripLayout:
                    // Horizontal line = strip layout.
                    g.fillRect (c.x - arm * 1.15f, c.y - thick * 0.5f, arm * 2.3f, thick);
                    break;
                case Glyph::GridLayout:
                {
                    // Square = tiled / grid layout.
                    const float side = arm * 1.55f;
                    g.drawRect (c.x - side * 0.5f, c.y - side * 0.5f, side, side,
                                juce::jmax (1.5f, thick * 0.85f));
                    break;
                }
                case Glyph::Cube:
                {
                    // Isometric cube for Spec 3D mode.
                    const float s = arm * 1.05f;
                    juce::Path p;
                    p.startNewSubPath (c.x, c.y - s);
                    p.lineTo (c.x + s, c.y - s * 0.35f);
                    p.lineTo (c.x + s, c.y + s * 0.55f);
                    p.lineTo (c.x, c.y + s);
                    p.lineTo (c.x - s, c.y + s * 0.55f);
                    p.lineTo (c.x - s, c.y - s * 0.35f);
                    p.closeSubPath();
                    g.strokePath (p, juce::PathStrokeType (juce::jmax (1.2f, thick * 0.7f)));
                    g.drawLine (c.x, c.y - s, c.x, c.y + s * 0.15f, juce::jmax (1.0f, thick * 0.55f));
                    g.drawLine (c.x - s, c.y - s * 0.35f, c.x, c.y + s * 0.15f, juce::jmax (1.0f, thick * 0.55f));
                    g.drawLine (c.x + s, c.y - s * 0.35f, c.x, c.y + s * 0.15f, juce::jmax (1.0f, thick * 0.55f));
                    break;
                }
            }
        }

        void setIdleAlpha (float a) noexcept { idleAlpha = a; refreshAlpha(); }
        void mouseEnter (const juce::MouseEvent& e) override
        {
            setAlpha (1.0f);
            juce::Button::mouseEnter (e);
        }
        void mouseExit (const juce::MouseEvent& e) override
        {
            setAlpha (idleAlpha);
            juce::Button::mouseExit (e);
        }
        void refreshAlpha() noexcept { setAlpha (isMouseOver (true) ? 1.0f : idleAlpha); }

    private:
        Glyph glyph;
        SharedResources* themeResources = nullptr;
        float idleAlpha = 1.0f;
    };

    /** Chrome button: idle alpha until hovered (strip overlays use 50%). */
    class HoverFadeButton : public juce::TextButton
    {
    public:
        HoverFadeButton (const juce::String& name) : juce::TextButton (name) {}
        void setIdleAlpha (float a) noexcept { idleAlpha = a; refreshAlpha(); }
        void mouseEnter (const juce::MouseEvent& e) override
        {
            setAlpha (1.0f);
            juce::TextButton::mouseEnter (e);
        }
        void mouseExit (const juce::MouseEvent& e) override
        {
            setAlpha (idleAlpha);
            juce::TextButton::mouseExit (e);
        }
        void refreshAlpha() noexcept { setAlpha (isMouseOver (true) ? 1.0f : idleAlpha); }
    private:
        float idleAlpha = 1.0f;
    };

    // LookAndFeels must outlive any components that use them.
    SettingsButtonLookAndFeel customLookAndFeel;
    TextButtonLookAndFeel textButtonLookAndFeel;
    GraphOverlayButtonLookAndFeel chromeButtonLookAndFeel;

    FrequencyResponseComponent frequencyResponseComponent;
    Visualizer m_visualizer;
    Controls m_controls;
    EqProcessor& processor;
    EqEditor& editor;
    SharedResources sharedResources;
    ColourRampBank colourRamps;
    Menu menu;
    /** Last user-placed settings panel bounds (empty until first layout / user move). */
    juce::Rectangle<int> settingsMenuBounds;
    bool settingsMenuBoundsFromUser = false;
    bool updatingSettingsMenuBounds = false;
    std::unique_ptr<EqPresetStore> eqPresets;

    MeterClipState inputMeterClip;
    MeterClipState outputMeterClip;

    VerticalGradientMeter verticalGradientMeterL;
    VerticalGradientMeter verticalGradientMeterR;
    VerticalGradientMeter verticalGradientMeterPostL;
    VerticalGradientMeter verticalGradientMeterPostR;

    /** Cycles L/R ↔ M/S meter channel mode (APVTS METER_CHANNEL_MODE_ID). */
    juce::TextButton meterChannelModeButton { "L/R" };

    HoverFadeButton menuToggleButton{ "Toggle Menu" };

    juce::TextButton bypassButton { "Bypass" };
    AbSlotButton slotAButton { "A" };
    AbSlotButton slotBButton { "B" };
    AbSlotButton slotCButton { "C" };
    AbSlotButton slotDButton { "D" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;

    /** Preset chrome: ◀ | editable name | ▼ | ▶ | Save */
    juce::TextButton presetPrevButton { juce::String::charToString ((juce::juce_wchar) 0x25C0) };
    juce::TextEditor presetNameEditor;
    juce::TextButton presetMenuButton { juce::String::charToString ((juce::juce_wchar) 0x25BC) };
    juce::TextButton presetNextButton { juce::String::charToString ((juce::juce_wchar) 0x25B6) };
    juce::TextButton presetSaveButton { "Save" };

    /** Left of preset bar: UI theme picker + dice randomize (ramps live in the UI dropdown). */
    HoverFadeButton uiThemeButton { "UI" };
    OscToolButton uiRandomizeButton { OscToolButton::Glyph::Dice };
    PathSampleOverlay rampSampleOverlay;

    /** Top-right: Undo / Redo just left of Settings. Curved arrows (↶ ↷). */
    juce::TextButton undoButton { juce::CharPointer_UTF8 ("\xe2\x86\xb6") }; // ↶
    juce::TextButton redoButton { juce::CharPointer_UTF8 ("\xe2\x86\xb7") }; // ↷

    /** Top chrome row, just right of Save's X: Eco disables analyser/FFT visuals. */
    juce::TextButton ecoButton { "Eco" };

    /** OSC toggle — shows a compact beat-synced waveform between Eco and L/R. */
    juce::TextButton oscButton { "OSC" };
    OscilloscopeComponent oscilloscope;
    FramedFloatingScopeWindow oscFrame;

    /** Gon toggle — sits under OSC; square goniometer + correlation on the right. */
    juce::TextButton gonButton { "Gon" };
    GoniometerComponent goniometer;
    FramedFloatingScopeWindow gonFrame;

    /** Spec toggle — spectrogram strip between UI dice and the EQ preset bar. */
    juce::TextButton specButton { "Spec" };
    SpectrogramComponent spectrogram;
    FramedFloatingScopeWindow specFrame;
    Spectrogram3DComponent spectrogram3D;

    ScopeLevelMeterModule levelMeterIn;
    ScopeLevelMeterModule levelMeterOut;
    LoudnessComponent loudnessMeter;
    StereogramComponent stereogram;
    HistogramComponent histogram;

    /** Dims UI under an expanded scope (clicks pass through). */
    class OscDimmerComponent : public juce::Component
    {
    public:
        void paint (juce::Graphics& g) override
        {
            g.fillAll (juce::Colours::black.withAlpha (0.20f));
        }
    };

    OscDimmerComponent oscDimmer;

    /**
        Closes Settings when clicking outside the menu.
        Expanded analyser windows (Osc/Gon/Spec/3D) are exempt so lookdev can
        be tweaked while orbiting / interacting with those views.
    */
    class MenuDismissCatcher : public juce::Component
    {
    public:
        explicit MenuDismissCatcher (MainComponent& o) : owner (o)
        {
            setOpaque (false);
            setInterceptsMouseClicks (true, false);
        }

        void mouseDown (const juce::MouseEvent&) override { owner.closeSettingsMenu(); }

        bool hitTest (int x, int y) override
        {
            if (owner.isPointOverSettingsDismissExempt (x, y, *this))
                return false;
            return true;
        }

    private:
        MainComponent& owner;
    };

    MenuDismissCatcher menuDismissCatcher { *this };

    /** Hit-tests only near the crosshair; drag H/V split ratios for Scope mode. */
    class ScopeSplitOverlay : public juce::Component
    {
    public:
        explicit ScopeSplitOverlay (MainComponent& owner) : main (owner) {}

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        bool hitTest (int x, int y) override;

        void setSplitNorm (float xNorm, float yNorm) noexcept;
        float getSplitX() const noexcept { return splitX; }
        float getSplitY() const noexcept { return splitY; }

    private:
        enum class Drag { none, vertical, horizontal, both };
        Drag hitZone (juce::Point<int> p) const noexcept;
        int splitXPx() const noexcept;
        int splitYPx() const noexcept;

        MainComponent& main;
        float splitX = 0.5f;
        float splitY = 0.5f;
        Drag drag = Drag::none;
        static constexpr int kHitPad = 6;
    };

    /** Drag Scope panes by their top edge; swap (quad) or insert (strip).
        Strip: bottom edge = height; vertical dividers = column widths. */
    class ScopeArrangeOverlay : public juce::Component
    {
    public:
        explicit ScopeArrangeOverlay (MainComponent& owner) : main (owner)
        {
            setInterceptsMouseClicks (true, false);
        }

        void paint (juce::Graphics& g) override;
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        bool hitTest (int x, int y) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;

        void setSlotBounds (const std::vector<juce::Rectangle<int>>& bounds) noexcept
        {
            slotBounds = bounds;
        }

        void setStripBounds (juce::Rectangle<int> bounds) noexcept
        {
            stripBounds = bounds;
        }

    private:
        int hitDragHandle (juce::Point<int> p) const noexcept;
        bool hitResizeEdge (juce::Point<int> p) const noexcept;
        int hitColumnDivider (juce::Point<int> p) const noexcept;
        int hitPaneEdgeForHover (juce::Point<int> p) const noexcept;
        void updateDropTarget (juce::Point<int> p) noexcept;
        void updateHoverCursor (juce::Point<int> p);

        MainComponent& main;
        std::vector<juce::Rectangle<int>> slotBounds {};
        juce::Rectangle<int> stripBounds {};
        int dragFromSlot = -1;
        int dropSlot = -1;
        bool dropInsertBefore = false;
        bool resizingStrip = false;
        bool hoverResize = false;
        int resizingColumn = -1; // divider index i = boundary between slot i and i+1
        int hoverColumnDivider = -1;
        int hoverPaneOutline = -1; // slot index to outline on hover
        juce::Point<int> dragPos {};
        juce::Point<float> dragGrabOffset {}; // mouse relative to pane top-left at grab
        static constexpr int kHandleH = 10;
        static constexpr int kInsertBand = 14;
        static constexpr int kResizeHitPad = 6;
    };

    OscToolButton oscZoomInButton { OscToolButton::Glyph::Plus };
    OscToolButton oscZoomOutButton { OscToolButton::Glyph::Minus };
    OscToolButton oscChannelModeButton { OscToolButton::Glyph::SummedStereo };
    OscToolButton oscExpandButton { OscToolButton::Glyph::Expand };
    OscToolButton gonExpandButton { OscToolButton::Glyph::Expand };
    OscToolButton specSpeedUpButton { OscToolButton::Glyph::Plus };
    OscToolButton specSpeedDownButton { OscToolButton::Glyph::Minus };
    OscToolButton specExpandButton { OscToolButton::Glyph::Expand };
    OscToolButton spec3DButton { OscToolButton::Glyph::Cube };
    ScopeSplitOverlay scopeSplitOverlay { *this };
    ScopeArrangeOverlay scopeArrangeOverlay { *this };

    OscToolButton arrangeButton { OscToolButton::Glyph::GridLayout };

    bool ecoEnabled = false;
    bool scopeModeEnabled = false;
    /** false = 2×2 quad (when N==4) or grid; true = horizontal strip (EQ graph hidden). */
    bool scopeStripLayout = false;
    std::vector<ScopeModuleId> scopeEnabledOrder = ScopeModules::defaultEnabledOrder();
    /** Strip column width fractions (sum ≈ 1). Sized to match scopeEnabledOrder. */
    std::vector<float> scopeStripFractions;
    void ensureScopeStripFractions();
    void setScopeStripColumnFraction (int leftSlot, float leftFrac);
    static constexpr int kScopeStripHeightDefaultPx = 200;
    static constexpr int kScopeStripHeightMinPx = 80;
    static constexpr float kScopeStripMinFrac = 0.08f;
    /** Strip height in design pixels (scaled by UI width / 1200). Default 200. */
    int scopeStripHeightPx = kScopeStripHeightDefaultPx;
    /** Scope-mode Pre/Post tap preference (false = Pre). Persisted; Scope itself is not. */
    bool scopeTapPost = false;
    bool oscExpanded = false;
    bool gonExpanded = false;
    bool specExpanded = false;
    /** When expanded: false = framed floating window, true = edge-to-edge full graph. */
    bool oscFullGraph = false;
    bool gonFullGraph = false;
    bool specFullGraph = false;
    bool spec3DEnabled = false;
    /** User-dragged expanded 3D window size/position (component bounds, includes shadow pad). */
    bool spec3DBoundsCustom = false;
    int spec3DPreferredW = 0;
    int spec3DPreferredH = 0;
    int spec3DPreferredX = 0;
    int spec3DPreferredY = 0;
    bool oscFrameBoundsCustom = false;
    int oscFramePreferredW = 0, oscFramePreferredH = 0, oscFramePreferredX = 0, oscFramePreferredY = 0;
    bool gonFrameBoundsCustom = false;
    int gonFramePreferredW = 0, gonFramePreferredH = 0, gonFramePreferredX = 0, gonFramePreferredY = 0;
    bool specFrameBoundsCustom = false;
    int specFramePreferredW = 0, specFramePreferredH = 0, specFramePreferredX = 0, specFramePreferredY = 0;

    juce::Rectangle<int> getFramedScopeAvailableArea() const;
    int getFramedToolButtonSize() const noexcept;
    int getFramedToolColumnWidth() const noexcept;
    void clampComponentWithToolColumn (juce::Component& frame, int toolColW);
    void placeToolColumnBesideFrame (juce::Rectangle<int> frameBounds,
                                     int btnSize, int btnGap,
                                     const std::initializer_list<OscToolButton*>& buttons);
    void syncOscFramedTools();
    void syncGonFramedTools();
    void syncSpecFramedTools();
    void syncSpec3DFramedTools();
    void layoutFramedScopeWindow (FramedFloatingScopeWindow& frame,
                                  bool& boundsCustom,
                                  int& prefW, int& prefH, int& prefX, int& prefY,
                                  int defaultW, int defaultH,
                                  bool gonSquareShape);
    void deactivateAnalyserFrames();
    void rememberFrameBounds (FramedFloatingScopeWindow& frame,
                              bool& boundsCustom,
                              int& prefW, int& prefH, int& prefX, int& prefY);
    bool scopesBeforeEcoOsc = true;
    bool scopesBeforeEcoGon = false;
    bool scopesBeforeEcoSpec = true;
    bool refreshingPresetName = false;
    juce::Component* hostedWordmark = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

#endif // MAINCOMPONENT_H
