#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Menu/SharedResources.h"

/** Shared clip / peak-hold state for a stereo meter pair (L+R or M/S). */
struct MeterClipState
{
    bool clipping = false;
    float heldPeakDb = -100.0f;
    double clipClearAtMs = 0.0;

    void reset() noexcept
    {
        clipping = false;
        heldPeakDb = -100.0f;
        clipClearAtMs = 0.0;
    }
};

/**
 * Vertical level meter with dB readout.
 * Peak / RMS / Peak+RMS modes and ballistics come from APVTS (Level Meters menu).
 * Channel label (L/R or M/S) follows METER_CHANNEL_MODE_ID.
 */
class VerticalGradientMeter : public juce::Component,
                              public juce::Timer
{
public:
    /** slot 0 = left / Mid, slot 1 = right / Side */
    VerticalGradientMeter (std::function<float()>&& peakDbFunction,
                           std::function<float()>&& rmsDbFunction,
                           juce::AudioProcessorValueTreeState& state,
                           MeterClipState& sharedClipState,
                           int meterSlot = 0);

    ~VerticalGradientMeter() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool hitTest (int x, int y) override;
    void mouseDown (const juce::MouseEvent& event) override;
    void timerCallback() override;

    void setThemeColors (SharedResources* r) noexcept { themeColors = r; repaint(); }

private:
    enum class MeterMode { Peak = 0, Rms = 1, PeakAndRms = 2 };

    MeterMode getMeterMode() const;
    bool isMsChannelMode() const;
    juce::String getChannelLabel() const;
    float getParamFloat (const char* id, float fallback) const;
    static float dbToY (float db, float height) noexcept;
    juce::Rectangle<float> getMeterBodyBounds() const;
    juce::Rectangle<float> getClipIndicatorBounds() const;
    juce::Rectangle<float> getReadoutBounds() const;
    juce::Rectangle<float> getChannelLabelBounds() const;

    const SharedColors& colors() const noexcept;

    std::function<float()> peakSupplier;
    std::function<float()> rmsSupplier;
    juce::AudioProcessorValueTreeState& treeState;
    MeterClipState& clipState;
    int slot = 0;
    SharedResources* themeColors = nullptr;

    juce::ColourGradient gradient2{};

    float displayedPeakDb = -100.0f;
    float displayedRmsDb = -100.0f;
    float readoutDb = -100.0f;
    float peakHoldDb = -100.0f;
    double peakHoldUntilMs = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VerticalGradientMeter)
};
