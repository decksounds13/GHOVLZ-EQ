#pragma once

#include <JuceHeader.h>
#include "Spec3DRampSequence.h"
#include "GradientStripEditor.h"
#include "RampColorPickerPanel.h"
#include "../Menu/SharedResources.h"
#include "../Export/Spec3DExportSettings.h"
#include <functional>

class ColourRampBank;

/** Spec3D sequencer: Ramp lane + optional lighting automation lanes. */
class Spec3DRampTimelineComponent : public juce::Component,
                                    private juce::Timer
{
public:
    Spec3DRampTimelineComponent (SharedResources& resources,
                                 ColourRampBank& bank,
                                 Spec3DRampSequence& sequence);
    ~Spec3DRampTimelineComponent() override;

    void setExpandedLayout (bool expanded) noexcept;
    bool isExpandedLayout() const noexcept { return expandedLayout; }
    void setShowExpandButton (bool shouldShow) noexcept;
    void setPlayheadSec (float sec) noexcept;
    float getPlayheadSec() const noexcept { return playheadSec; }
    std::function<float()> playheadProvider;

    void setThemeColors (SharedResources* r) noexcept { theme = r != nullptr ? r : &resources; repaint(); }

    std::function<void()> onSequenceChanged;
    std::function<void()> onRequestExpand;
    std::function<void()> onEnabledChanged;
    /**
        Offline export of the selected timeline region (video + DAW audio).
        Host (MainComponent) runs Spec3DExportJob.
    */
    std::function<void (const Spec3DExportSettings&)> onExportRegionOffline;

    bool hasRegionSelection() const noexcept { return regionValid; }
    float getRegionStartSec() const noexcept { return juce::jmin (regionInSec, regionOutSec); }
    float getRegionEndSec() const noexcept { return juce::jmax (regionInSec, regionOutSec); }
    void clearRegionSelection() noexcept;
    void setRegionSelection (float startSec, float endSec) noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    int getPreferredHeight() const noexcept;

private:
    void timerCallback() override;
    void notifyChanged();
    void rebuildCaches();
    void layoutChrome();
    void styleChrome();
    const SharedColors& colors() const noexcept;

    juce::Rectangle<float> getTracksArea() const noexcept;
    juce::Rectangle<float> getRampLaneBounds() const noexcept;
    juce::Rectangle<float> getAutoLaneBounds (int autoIdx) const noexcept;
    float labelColW() const noexcept { return expandedLayout ? 64.0f : 52.0f; }
    float rampLaneH() const noexcept { return expandedLayout ? 52.0f : 36.0f; }
    float autoLaneH() const noexcept { return expandedLayout ? 44.0f : 32.0f; }

    int hitClipBody (juce::Point<float> p) const noexcept;
    int hitClipEdge (juce::Point<float> p, bool& leftEdge) const noexcept;
    int hitFadeHandle (juce::Point<float> p) const noexcept;
    bool hitEmptyAdd (juce::Point<float> p) const noexcept;
    int hitAutoLane (juce::Point<float> p) const noexcept;
    bool hitRampLabel (juce::Point<float> p) const noexcept;
    bool hitAutoLaneLabel (int autoIdx, juce::Point<float> p) const noexcept;
    juce::Rectangle<float> getRampLabelBounds() const noexcept;
    juce::Rectangle<float> getAutoLaneLabelBounds (int autoIdx) const noexcept;
    int hitFloatKey (int autoIdx, juce::Point<float> p) const noexcept;
    int hitColourKey (int autoIdx, juce::Point<float> p) const noexcept;

    void paintRampLane (juce::Graphics& g);
    void paintAutoLane (juce::Graphics& g, int autoIdx);
    void paintRegionOverlay (juce::Graphics& g) const;
    void paintRampInClip (juce::Graphics& g, juce::Rectangle<float> r, const GradientRamp& ramp) const;

    void showAddPresetPicker (juce::Component* anchor);
    void showChangePresetPicker (int clipIndex, juce::Component* anchor);
    void showClipContextMenu (int clipIndex, juce::Point<int> screenPos);
    void showRegionContextMenu (juce::Point<int> screenPos);
    void showExportRegionDialog();
    bool hitRegion (juce::Point<float> p) const noexcept;
    juce::Rectangle<float> getTrackContentBounds() const noexcept;
    float xToTimeFull (float x) const noexcept;
    float timeToXFull (float t) const noexcept;
    void openClipRampEditor (int clipIndex);
    void closeClipRampEditor (bool persist);
    void showAddLaneMenu();
    /** autoIdx < 0 = Ramp colour lane; else autoLanes[autoIdx]. */
    void showLaneLabelMenu (int autoIdx, juce::Point<int> screenPos);
    void toggleLaneEnabled (int autoIdx);
    void showKeyInterpMenu (int autoIdx, int keyIdx, bool isColour, juce::Point<int> screenPos);
    void beginEditFloatKeyValue (int autoIdx, int keyIdx);
    void openColourKeyPicker (int autoIdx, int keyIdx);

    SharedResources& resources;
    SharedResources* theme = nullptr;
    ColourRampBank& bank;
    Spec3DRampSequence& sequence;

    bool expandedLayout = false;
    bool showExpandButton = true;
    float playheadSec = 0.0f;
    int selectedClip = -1;
    int selectedAutoLane = -1;
    int selectedKey = -1;

    enum class DragMode
    {
        none, reorder, resizeLeft, resizeRight, fade,
        floatKey, colourKey, bezierHandle, selectRegion
    };
    DragMode dragMode = DragMode::none;
    int dragClip = -1;
    int dragAutoLane = -1;
    int dragKey = -1;
    bool dragBezierIn = false;
    float dragGrabX = 0.0f;
    int dropInsertAt = -1;
    float dragStartWeight = 1.0f;
    float dragStartNeighbourWeight = 1.0f;
    float dragStartFade = 0.0f;
    float dragStartKeyT = 0.0f;
    float dragStartKeyV = 0.0f;

    // Selected export region (sequence seconds). Drag empty track to set; RMB Export.
    bool regionValid = false;
    float regionInSec = 0.0f;
    float regionOutSec = 0.0f;

    struct ClipCache
    {
        juce::Rectangle<float> bounds;
    };
    std::vector<ClipCache> clipCaches;
    std::vector<Spec3DRampSequence::LaidOutClip> layout;

    juce::TextButton enableButton { "Seq" };
    juce::Slider lengthSlider;
    juce::Label lengthLabel;
    juce::TextButton addButton { "+" };
    juce::TextButton removeButton { "-" };
    juce::TextButton addLaneButton { "+Lane" };
    juce::TextButton expandButton { "Exp" };

    // Inline clip editor (expanded/pop-out preferred)
    std::unique_ptr<GradientStripEditor> clipEditor;
    int clipEditorIndex = -1;
    std::unique_ptr<RampColorPickerPanel> colourKeyPicker;
    int colourPickerLane = -1;
    int colourPickerKey = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spec3DRampTimelineComponent)
};
