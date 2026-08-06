#pragma once
#include <JuceHeader.h>
#include "../SharedResources.h"
#include "../Theme.h"
#include "CustomScrollBar.h"
#include "CustomHeader.h"
#include "MelatoninBlur/melatonin/shadows.h"

// Forward declaration of UIElementsList
class UIElementsList;
class EqProcessor;
class CustomHeader;

class ThemeList : public juce::ListBoxModel,
    public juce::Component,
    public juce::TextEditor::Listener,
    public juce::KeyListener,
    public juce::Timer
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onPresetApplied(const Theme& theme) = 0;
        /** Fired when selection or names change (add / overwrite / delete / rename). */
        virtual void onPresetListChanged() {}
    };

    explicit ThemeList(SharedResources& resources);
    ~ThemeList() override;

    /** Wire the EQ processor so Save/Load include full plugin state (APVTS + A/B). */
    void setProcessor (EqProcessor* processorToUse) noexcept { processor = processorToUse; }
    EqProcessor* getProcessor() const noexcept { return processor; }

    /** Capture/apply modular Global UI pieces (ramps, scope layout, module looks). */
    void setGlobalUiCapture (std::function<juce::ValueTree()> fn) { captureGlobalUi = std::move (fn); }
    void setGlobalUiApply (std::function<void (const juce::ValueTree&)> fn) { applyGlobalUi = std::move (fn); }

    void paintOverChildren(juce::Graphics& g) override;
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void resized() override;

    /** Append a preset: colours + GlobalUi modules (not EQ).
        If appendDateSuffix is true (Appearance "New"), appends _MM-DD-YYYY. */
    void addPreset (const juce::String& name, bool appendDateSuffix = true);
    void saveCurrentPreset (const juce::String& name);
    /**
        Apply colours always.
        GlobalUi modules when present (unless applyGlobalUiModules is false).
        Legacy full EQ STATE only when applyPluginState is true.
    */
    void applyPreset (int index, bool applyPluginState = false, bool applyGlobalUiModules = true);
    void updateUI();
    void createDefaultPreset();
    void overwritePreset (int index, const juce::String& name);
    void deletePreset (int index);
    /** Copy an existing UI theme (including Default) into a new uniquely named entry. */
    void duplicatePreset (int index);
    /** Rename in-place (keeps colours/state); blocked for Default (index 0). */
    void renamePreset (int index, const juce::String& newName);
    /**
        Chrome Save: update the current non-default selection (rename if needed),
        or create a new preset when Default/none is selected.
        Name collisions (other than Default) overwrite that entry.
     */
    void saveOrUpdateWithName (const juce::String& name);

    int getSelectedRow() const;
    juce::String getPresetName(int index) const;
    juce::String getSelectedPresetName() const;
    int findPresetIndexByName (const juce::String& name) const;
    void listBoxDataChanged();
    void textEditorReturnKeyPressed(juce::TextEditor& editor) override;
    void updateScrollBarColors(const juce::Colour& trackColor, const juce::Colour& thumbColor, const juce::Colour& outlineColor);

    void timerCallback() override;

    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void paint(juce::Graphics& g) override;

    void addListener(Listener* newListener);
    void removeListener(Listener* listener);

    void setUIElementsList(UIElementsList* list);

    void setColumnWidths(int col1Width, int col2Width, int col3Width) {
        this->newCol1Width = col1Width;
        this->newCol2Width = col2Width;
        this->newCol3Width = col3Width;
        listBox.updateContent();
        listBox.repaint();
    }

    bool keyPressed(const juce::KeyPress& key, Component* originatingComponent) override;

    void updateColumnWidths(int col1, int col2, int col3);

    const juce::Path& getShadowPath() const {
        return shadowPath;
    }

private:
    juce::ValueTree capturePluginState() const;
    void applyPluginState (const juce::ValueTree& state);
    juce::ValueTree captureCurrentGlobalUi() const;
    void persistPresetsToXml();

    juce::Array<juce::Time> themeCreatedTimes;
    juce::Array<juce::Time> themeModifiedTimes;
    juce::StringArray presetNames;
    juce::Array<Theme> presets;
    int selectedRow{ -1 };

    juce::ListenerList<Listener> listeners;

    void loadPresetsFromXML();

    UIElementsList* uiElementsList{ nullptr };
    EqProcessor* processor{ nullptr };
    std::function<juce::ValueTree()> captureGlobalUi;
    std::function<void (const juce::ValueTree&)> applyGlobalUi;

    std::unique_ptr<CustomScrollBar> customScrollBar;

    int editedRow{ -1 };
    juce::TextEditor textEditor;
    bool editingActive{ false };
    int hoveredRow = -1;
    bool needsRepainting = false;
    juce::ListBox listBox;
    SharedResources& sharedResources;
    std::unique_ptr<CustomHeader> customHeader;
    int height;
    bool isTextEditorActive = false;
    int currentScrollPosition = 0;
    int headerHeight = 30;
    bool cursorOverComponent = false;

    int newCol1Width, newCol2Width, newCol3Width;

    juce::Path shadowPath;
    melatonin::InnerShadow innerShadow = { { juce::Colours::black.withAlpha(0.8f), 14, {- 2, -2} }};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ThemeList)
};
