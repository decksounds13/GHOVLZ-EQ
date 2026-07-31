#pragma once

#include <JuceHeader.h>
#include "../../Analyser.h"
#include "../../../ColourRamp/GradientRamp.h"
#include "MelatoninBlur/melatonin/shadows.h"

/** Draws Fruity-EQ-style FFT bin bars with optional Melatonin glow. */
class SpectrumBinOverlay
{
public:
    SpectrumBinOverlay();

    void setBaseColour (juce::Colour colour) noexcept { baseColour = colour; }
    void setColourRamp (const GradientRamp* ramp) noexcept { colourRamp = ramp; }

    void paint (juce::Graphics& g,
                Analyser& analyser,
                juce::Rectangle<float> bounds,
                bool logarithmic,
                juce::AudioProcessorValueTreeState* valueTree);

private:
    struct Settings
    {
        float opacity = 1.0f;
        float barWidthRatio = 0.92f;
        float intensity = 1.4f;
        float threshold = 0.35f;
        float resolution = 1.0f;
        bool glowEnabled = true;
        float glowRadius = 16.0f;
        float glowSpread = 8.0f;
        float glowOpacity = 0.80f;
        float glowOffsetX = 0.0f;
        float glowOffsetY = 0.0f;
        bool fullHeight = false;
    };

    struct BinSample
    {
        float xNorm = 0.0f;
        float peak = 0.0f;
    };

    struct Bar
    {
        float x = 0.0f, y = 0.0f, w = 0.0f, h = 0.0f, peak = 0.0f;
    };

    Settings readSettings (juce::AudioProcessorValueTreeState* valueTree) const;
    float normalizeBinIndex (int binIndex, Analyser& analyser, bool logarithmic) const;
    float shapePresence (float peak, float intensityScale, float threshold) const;
    juce::Colour colourForIntensity (float peak, float opacity, float intensityScale, float threshold) const;

    /** Map analyser peak → bar top Y (matches GraphLine). Intensity is colour-only. */
    static float mapBarTopY (float peak, float intensity, bool fullHeight,
                             float top, float bottom) noexcept;

    melatonin::DropShadow glowShadow;
    std::vector<BinSample> activeBins;
    std::vector<Bar> bars;
    std::vector<float> glowColumnPeaks;
    juce::Colour baseColour { juce::Colour::fromRGBA (255, 90, 40, 160) };
    const GradientRamp* colourRamp = nullptr;
};
