#pragma once

#include <JuceHeader.h>
#include "../../Analyser.h"
#include "SpectrumBinOverlay.h"
#include "../../../ColourRamp/GradientRamp.h"
#include "../../../Menu/SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"

// ****************************************************************************
// GRAPH LINE CLASS
// ****************************************************************************
class GraphLine : public juce::Component
{
public:
    GraphLine (Analyser&);
    ~GraphLine() override;

    // ========================================================================
    void paint (juce::Graphics&) override;

    // ========================================================================
    void setScaleType (const bool);
    virtual void setColour (const juce::Colour&);
    void setFillColour (const juce::Colour& colour) noexcept { m_fillColour = colour; }
    void setHoldColour (const juce::Colour& colour) noexcept { m_holdColour = colour; }
    void setBinOverlayColour (juce::Colour colour) noexcept { m_binOverlay.setBaseColour (colour); }
    void setBinOverlayColourRamp (const GradientRamp* ramp) noexcept { m_binOverlay.setColourRamp (ramp); }
    void setSpectrumFillRamp (const GradientRamp* ramp) noexcept { m_spectrumPostFillRamp = ramp; }
    void setSpectrumCurveRamp (const GradientRamp* ramp) noexcept { m_spectrumPostCurveRamp = ramp; }
    void setSpectrumPreFillRamp (const GradientRamp* ramp) noexcept { m_spectrumPreFillRamp = ramp; }
    void setSpectrumPreCurveRamp (const GradientRamp* ramp) noexcept { m_spectrumPreCurveRamp = ramp; }
    void setSpectrumHoldFillRamp (const GradientRamp* ramp) noexcept { m_spectrumHoldFillRamp = ramp; }
    void setSpectrumHoldCurveRamp (const GradientRamp* ramp) noexcept { m_spectrumHoldCurveRamp = ramp; }
    void setSharedResources (SharedResources* resources) noexcept { m_sharedResources = resources; }
    void setAudioProcessorValueTreeState (juce::AudioProcessorValueTreeState* state);

protected:
    // ========================================================================
    /** Bin index → x in [0,1] on EQ axis (20 Hz … min(20 kHz, Nyquist)). */
    float normalizeValue (const int value);
    virtual float getScopeDataFromAnalyser (const size_t);
    virtual float getModifiedScopeDataFromAnalyser (const size_t);
    virtual void drawFrame (juce::Graphics&);

    float getSpectrumOpacity() const;
    float getSpectrumFillOpacity() const;
    float getSpectrumPathWidth() const;
    /** SPECTRUM_RESOLUTION_ID as 0…1 — used as curve on/off (Show Bins). */
    float getSpectrumResolution() const;
    /** SPECTRUM_CURVE_RES_ID: Off / Low / Med / High → display-space smooth radius (px). */
    float getSpectrumCurveSmoothRadiusPx() const;

    // ========================================================================
    Analyser& mr_analyser;
    juce::AudioProcessorValueTreeState* mr_valueTree = nullptr;
    SharedResources* m_sharedResources = nullptr;
    SpectrumBinOverlay m_binOverlay;
    const GradientRamp* m_spectrumPostFillRamp = nullptr;
    const GradientRamp* m_spectrumPostCurveRamp = nullptr;
    const GradientRamp* m_spectrumPreFillRamp = nullptr;
    const GradientRamp* m_spectrumPreCurveRamp = nullptr;
    const GradientRamp* m_spectrumHoldFillRamp = nullptr;
    const GradientRamp* m_spectrumHoldCurveRamp = nullptr;
    // Dual-layer Melatonin glow (bloom + core), same approach as FFT bins.
    melatonin::DropShadow m_curveGlow {
        {
            { juce::Colour::fromRGBA (187, 219, 132, 90), 16, { 0, 0 }, 2 },
            { juce::Colour::fromRGBA (230, 255, 170, 140), 6, { 0, 0 }, 0 }
        }
    };
    std::atomic<bool> m_isLogarithmicScale { true };
    juce::Colour m_colour { 0xff48bde8 };
    juce::Colour m_fillColour { juce::Colour::fromRGBA (115, 100, 63, 90) };
    juce::Colour m_holdColour { juce::Colour::fromRGBA (230, 170, 132, 105) };

    // Scratch buffers reused across paint calls (avoid per-frame heap churn).
    std::vector<float> m_columnPre;
    std::vector<float> m_columnPost;
    std::vector<float> m_columnHold;
    std::vector<float> m_columnPreB;
    std::vector<float> m_columnPostB;
    std::vector<float> m_columnHoldB;
    std::vector<float> m_fadePreCurve, m_fadePreFill, m_fadePostCurve, m_fadePostFill, m_fadeHoldCurve, m_fadeHoldFill;
    std::vector<float> m_fadePreCurveB, m_fadePreFillB, m_fadePostCurveB, m_fadePostFillB, m_fadeHoldCurveB, m_fadeHoldFillB;
    double m_lastFadeTimeMs = 0.0;
    std::vector<float> m_smoothScratch;
    std::vector<float> m_peakScratch;
    std::vector<char> m_hasBinScratch;
    std::vector<float> m_pointXScratch;
    std::vector<float> m_pointMagScratch;

    // ========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GraphLine)
};
