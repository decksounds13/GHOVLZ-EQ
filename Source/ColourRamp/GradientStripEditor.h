#pragma once

#include <JuceHeader.h>
#include "GradientRamp.h"
#include "RampPresetStore.h"
#include "RampColorPickerPanel.h"
#include "../Menu/SharedResources.h"

/** Photoshop / Substance-style gradient strip with arrow stops + preset rail. */
class GradientStripEditor : public juce::Component,
                            private juce::ChangeListener
{
public:
    enum class ModeFamily
    {
        intensity, // FFT / Spec
        spatial    // Spectrum Fill
    };

    GradientStripEditor (SharedResources& resources,
                         ModeFamily family,
                         RampPresetStore* presetStore = nullptr);

    ~GradientStripEditor() override;

    void setRamp (GradientRamp* ramp);
    void setPresetStore (RampPresetStore* store);
    void setCompact (bool shouldBeCompact) noexcept;
    void setUiScale (float scale) noexcept;
    int getPreferredHeight() const noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    std::function<void()> onRampChanged;
    /** Optional: live colour updates without persisting (falls back to onRampChanged). */
    std::function<void()> onRampPreview;
    /** Fired when preferred height changes (inline colour picker open/close). */
    std::function<void()> onPreferredHeightChanged;
    /** Begin UI path-sampling into this editor's ramp (host wires active target + overlay). */
    std::function<void()> onSamplePath;

private:
    /** Combo-like field: current gradient swatch + preset name (opens preset picker). */
    class PresetFieldButton final : public juce::Button
    {
    public:
        PresetFieldButton();
        void setDisplay (const GradientRamp* ramp, juce::String name);
        void paintButton (juce::Graphics& g, bool highlighted, bool down) override;

    private:
        GradientRamp displayRamp;
        juce::String displayName { "Choose preset..." };
        bool hasRamp = false;
    };

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    int hitTestStop (juce::Point<float> p) const;
    juce::Rectangle<float> stripBounds() const;
    void openPickerForStop (int index);
    void closePolePicker (bool persist);
    void notifyChanged();
    void notifyHeightChanged();
    void rebuildMapCombo();
    void syncControlsFromRamp();
    void syncPresetField();
    void simplifyClicked();
    void densifyClicked();
    void savePresetClicked();
    void showPresetMenu();
    void samplePathClicked();
    int findMatchingPresetIndex() const;

    SharedResources& sharedResources;
    ModeFamily modeFamily;
    RampPresetStore* presets = nullptr;
    bool compact = true;

    juce::ToggleButton enableToggle { "Use" };
    juce::Label mapLabel;
    juce::ComboBox mapCombo;
    juce::TextButton simplifyButton { "Less" };
    juce::TextButton densifyButton { "More" };
    /** Pole count; click toggles Hard / Soft blend (no extra toolbar buttons). */
    juce::TextButton stopCountButton;
    juce::TextButton savePresetButton { "Save" };
    PresetFieldButton presetField;
    juce::TextButton samplePathButton { "Sample Path" };
    GradientRamp* ramp = nullptr;
    float uiScale = 1.0f;
    int selectedPresetIndex = -1;

    int dragIndex = -1;
    int selectedIndex = -1;
    juce::Point<float> mouseDownPos;
    bool didDrag = false;

    std::unique_ptr<RampColorPickerPanel> polePicker;
    int polePickerIndex = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GradientStripEditor)
};
