#pragma once

#include <JuceHeader.h>
#include "../Menu/SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "DynParams.h"
#include "DynCompressor.h"

/** Square input/output transfer plot. Knee, ratio, and a live signal ball. */
class TransferCurveComponent : public juce::Component,
                               private juce::Timer
{
public:
    TransferCurveComponent (juce::AudioProcessorValueTreeState& s, std::function<int()> selectedBand);
    ~TransferCurveComponent() override = default;

    void setThemeColors (SharedResources* r) noexcept { theme = r; }
    void setEngine (DynCompressor* e) noexcept { engine = e; }
    void setCompact (bool should) noexcept;

    void paint (juce::Graphics& g) override;

private:
    void timerCallback() override;

    float readF (const juce::String& id, float fallback) const noexcept;
    static float compressedOutDb (float inDb, float threshold, float ratio, float knee) noexcept;
    void drawLiveBall (juce::Graphics& g, juce::Point<float> c, float r,
                       juce::Colour fill, juce::Colour glow, bool core) const;

    juce::AudioProcessorValueTreeState& state;
    std::function<int()> getBand;
    SharedResources* theme = nullptr;
    DynCompressor* engine = nullptr;
    bool compact = false;

    float displayedInDb = -140.0f;
    float peakInDb = -140.0f;
    int lastBand = -1;

    static constexpr int kTrail = 10;
    std::array<float, kTrail> trailIn {};
    int trailWrite = 0;
    int trailCount = 0;

    melatonin::DropShadow curveGlow {
        {
            { juce::Colour::fromRGBA (255, 180, 60, 90), 14, { 0, 0 }, 2 },
            { juce::Colour::fromRGBA (255, 220, 120, 140), 5, { 0, 0 }, 0 }
        }
    };
};
