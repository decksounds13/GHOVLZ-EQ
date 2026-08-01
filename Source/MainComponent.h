#ifndef MAINCOMPONENT_H
#define MAINCOMPONENT_H

#include <JuceHeader.h>
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
#include "EqPresetStore.h"
#include "ColourRamp/ColourRampBank.h"
#include "ColourRamp/PathSampleOverlay.h"

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

    /** Scope-mode tap only (compact chrome scopes unchanged). Persisted via ui_prefs. */
    void setScopeTapPost (bool shouldTapPost, bool notifyPrefs = true);
    bool isScopeTapPost() const noexcept { return scopeTapPost; }

    /** Global Melatonin glow / drop-shadow bypass. Persisted via ui_prefs. */
    void setDisableGlowShadowEffects (bool shouldDisable, bool notifyPrefs = true);
    bool areGlowShadowEffectsDisabled() const noexcept;

    SharedResources& getSharedResources() noexcept { return sharedResources; }
    const SharedResources& getSharedResources() const noexcept { return sharedResources; }

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
    void syncMeterChannelModeButton();
    void toggleMeterChannelMode();
    void setOscExpanded (bool shouldExpand);
    void setGonExpanded (bool shouldExpand);
    void setSpecExpanded (bool shouldExpand);
    void syncOscToolButtons();
    void syncGonToolButtons();
    void syncSpecToolButtons();
    void syncScopeChromeButtonOpacity();
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
        enum class Glyph { Plus, Minus, SummedStereo, SplitStereo, Expand, Collapse, Dice };

        explicit OscToolButton (Glyph g)
            : juce::Button ({}), glyph (g)
        {
            setClickingTogglesState (false);
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
            }
        }

    private:
        Glyph glyph;
        SharedResources* themeResources = nullptr;
    };

    // LookAndFeels must outlive any components that use them.
    SettingsButtonLookAndFeel customLookAndFeel;
    TextButtonLookAndFeel textButtonLookAndFeel;

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

    juce::TextButton menuToggleButton{ "Toggle Menu" };

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
    juce::TextButton uiThemeButton { "UI" };
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

    /** Gon toggle — sits under OSC; square goniometer + correlation on the right. */
    juce::TextButton gonButton { "Gon" };
    GoniometerComponent goniometer;

    /** Spec toggle — spectrogram strip between UI dice and the EQ preset bar. */
    juce::TextButton specButton { "Spec" };
    SpectrogramComponent spectrogram;

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

    OscToolButton oscZoomInButton { OscToolButton::Glyph::Plus };
    OscToolButton oscZoomOutButton { OscToolButton::Glyph::Minus };
    OscToolButton oscChannelModeButton { OscToolButton::Glyph::SummedStereo };
    OscToolButton oscExpandButton { OscToolButton::Glyph::Expand };
    OscToolButton gonExpandButton { OscToolButton::Glyph::Expand };
    OscToolButton specSpeedUpButton { OscToolButton::Glyph::Plus };
    OscToolButton specSpeedDownButton { OscToolButton::Glyph::Minus };
    OscToolButton specExpandButton { OscToolButton::Glyph::Expand };
    ScopeSplitOverlay scopeSplitOverlay { *this };

    bool ecoEnabled = false;
    bool scopeModeEnabled = false;
    /** Scope-mode Pre/Post tap preference (false = Pre). Persisted; Scope itself is not. */
    bool scopeTapPost = false;
    bool oscExpanded = false;
    bool gonExpanded = false;
    bool specExpanded = false;
    bool scopesBeforeEcoOsc = true;
    bool scopesBeforeEcoGon = false;
    bool scopesBeforeEcoSpec = true;
    bool refreshingPresetName = false;
    juce::Component* hostedWordmark = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

#endif // MAINCOMPONENT_H
