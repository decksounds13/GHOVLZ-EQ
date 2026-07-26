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

    FrequencyResponseComponent& getFrequencyResponseComponent() noexcept { return frequencyResponseComponent; }

    void onPresetApplied (const Theme& theme) override;
    void onPresetListChanged() override;

    void textEditorReturnKeyPressed (juce::TextEditor&) override;
    void textEditorFocusLost (juce::TextEditor&) override;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    /** Eco mode (FFT/analyser off). Persisted via EqEditor ui_prefs. Default off. */
    void setEcoMode (bool shouldEnable, bool notifyPrefs = true);
    bool isEcoMode() const noexcept { return ecoEnabled; }

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
    void syncAbButtons();
    void syncUndoRedoButtons();
    void showAbMenu (EqProcessor::AbSlot slot);
    void refreshPresetNameDisplay();
    void commitPresetNameEdit();
    void saveCurrentAppearancePreset();
    void showPresetPopupMenu();
    void cycleAppearancePreset (int delta);
    void layoutPresetChrome (float scale);
    void applyEcoMode (bool shouldEnable);
    void syncMeterChannelModeButton();
    void toggleMeterChannelMode();
    void setOscExpanded (bool shouldExpand);
    void syncOscToolButtons();
    void syncExpandedOscOverlayStack();
    void hostOptionBoxAboveExpandedOsc (bool shouldHost);

    static constexpr double m_marginInPixels{ 10 };

    /** Square chrome button that paints + / - / ST / L/R / ^ / v (never ellipsis). */
    class OscToolButton : public juce::Button
    {
    public:
        enum class Glyph { Plus, Minus, SummedStereo, SplitStereo, Expand, Collapse };

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

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const float corner = 3.0f;
            auto fill = juce::Colour::fromRGBA (60, 50, 35, 255);
            if (getToggleState())
                fill = juce::Colour::fromRGBA (180, 150, 55, 255);
            else if (down)
                fill = fill.brighter (0.15f);
            else if (highlighted)
                fill = fill.brighter (0.08f);

            g.setColour (fill);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colour::fromRGBA (90, 75, 50, 200));
            g.drawRoundedRectangle (r, corner, 1.0f);

            const auto ink = getToggleState() ? juce::Colours::black
                                              : juce::Colours::whitesmoke.withAlpha (0.9f);
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
            }
        }

    private:
        Glyph glyph;
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
    Menu menu;

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

    /** Top-right: Undo / Redo just left of Settings. Curved arrows (↶ ↷). */
    juce::TextButton undoButton { juce::CharPointer_UTF8 ("\xe2\x86\xb6") }; // ↶
    juce::TextButton redoButton { juce::CharPointer_UTF8 ("\xe2\x86\xb7") }; // ↷

    /** Top chrome row, just right of Save's X: Eco disables analyser/FFT visuals. */
    juce::TextButton ecoButton { "Eco" };

    /** OSC toggle — shows a compact beat-synced waveform between Eco and L/R. */
    juce::TextButton oscButton { "OSC" };
    OscilloscopeComponent oscilloscope;

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
    OscToolButton oscZoomInButton { OscToolButton::Glyph::Plus };
    OscToolButton oscZoomOutButton { OscToolButton::Glyph::Minus };
    OscToolButton oscChannelModeButton { OscToolButton::Glyph::SummedStereo };
    OscToolButton oscExpandButton { OscToolButton::Glyph::Expand };

    bool ecoEnabled = false;
    bool oscExpanded = false;
    bool refreshingPresetName = false;
    juce::Component* hostedWordmark = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MainComponent)
};

#endif // MAINCOMPONENT_H
