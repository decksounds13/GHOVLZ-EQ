#pragma once

#include "RampPresetStore.h"
#include <functional>

/** Shared paint for horizontal ramp swatches (clips, menus, fields). */
void paintRampSwatch (juce::Graphics& g, juce::Rectangle<float> bounds,
                      const GradientRamp& ramp, float corner = 2.5f);

/**
    Lightweight CallOut list of ramp presets with gradient swatches.
    Extracted from GradientStripEditor for reuse (timeline Add/Change menus).
*/
class RampPresetPicker final : public juce::Component
{
public:
    RampPresetPicker (RampPresetStore& store,
                      std::function<void (int index)> onPick,
                      std::function<void (int index)> onDelete = nullptr);

    ~RampPresetPicker() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class Row;

    RampPresetStore& store;
    std::function<void (int)> onPick, onDelete;
    juce::Viewport viewport;
    juce::Component list;
    juce::OwnedArray<Row> rows;
    juce::Label emptyLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RampPresetPicker)
};

/** Launch CallOutBox anchored to a component (or screen area). */
void showRampPresetPickerCallOut (RampPresetStore& store,
                                  juce::Component* anchor,
                                  std::function<void (int index)> onPick,
                                  std::function<void (int index)> onDelete = nullptr);
