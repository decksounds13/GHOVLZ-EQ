#pragma once

#include <JuceHeader.h>
#include <array>
#include "DynParams.h"
#include "DynCompressor.h"
#include "../Menu/SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"

/** Band tints, crossovers, threshold and makeup paths on the analyser. */
class DynSpectrumOverlay : public juce::Component,
                           private juce::Timer
{
public:
    DynSpectrumOverlay (juce::AudioProcessorValueTreeState& state, DynCompressor& engine);
    ~DynSpectrumOverlay() override = default;

    void paint (juce::Graphics& g) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
    void setThemeColors (SharedResources* r) noexcept { theme = r; }

private:
    enum class Drag { none, split, threshold, upThreshold, clipThreshold, makeup };

    struct Hit
    {
        Drag kind = Drag::none;
        int index = -1;
    };

    void timerCallback() override;

    int bandCount() const;
    float hzToX (float hz) const;
    float xToHz (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;
    float makeupToY (float db) const;
    float yToMakeup (float y) const;
    juce::Colour bandColour (int b) const;
    juce::Colour makeupColour() const;
    Hit hitTest (juce::Point<float> p) const;
    void updateHover (const juce::MouseEvent& e);
    void applyCursor (Drag kind);
    int hostBandAtX (float x) const;

    juce::AudioProcessorValueTreeState& state;
    DynCompressor& engine;
    SharedResources* theme = nullptr;

    Drag drag = Drag::none;
    int dragIndex = -1;
    Drag hover = Drag::none;
    int hoverIndex = -1;
    juce::Point<float> hoverPos;
    bool showGhostAdd = false;

    std::array<float, DynParams::kMaxBands> downAmt {};
    std::array<float, DynParams::kMaxBands> upAmt {};
    std::array<float, DynParams::kMaxBands> clipAmt {};

    melatonin::DropShadow splitShadow {
        {
            { juce::Colour::fromRGBA (0, 0, 0, 150), 10, { 0, 2 }, 2 },
            { juce::Colour::fromRGBA (0, 0, 0, 80), 4, { 0, 1 }, 0 }
        }
    };

    melatonin::DropShadow zoneShadow {
        {
            { juce::Colour::fromRGBA (0, 0, 0, 120), 12, { 0, 4 }, 1 },
            { juce::Colour::fromRGBA (0, 0, 0, 70), 5, { 0, 2 }, 0 }
        }
    };

    melatonin::DropShadow barShadow {
        {
            { juce::Colour::fromRGBA (0, 0, 0, 160), 8, { 0, 3 }, 1 },
            { juce::Colour::fromRGBA (0, 0, 0, 90), 3, { 0, 1 }, 0 }
        }
    };
};
