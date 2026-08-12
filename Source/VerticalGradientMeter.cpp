#include "VerticalGradientMeter.h"

namespace
{
    constexpr float kMeterFloorDb = -60.0f;
    constexpr float kMeterCeilDb = 6.0f;
    constexpr float kSilenceDb = -100.0f;
}

VerticalGradientMeter::VerticalGradientMeter (std::function<float()>&& peakDbFunction,
                                              std::function<float()>&& rmsDbFunction,
                                              juce::AudioProcessorValueTreeState& state,
                                              MeterClipState& sharedClipState,
                                              int meterSlot)
    : peakSupplier (std::move (peakDbFunction)),
      rmsSupplier (std::move (rmsDbFunction)),
      treeState (state),
      clipState (sharedClipState),
      slot (meterSlot)
{
    // Meters sit over the EQ graph — only the clip indicator should capture clicks
    // so LP handles / range buttons underneath remain reachable.
    setInterceptsMouseClicks (true, false);
    setPaintingIsUnclipped (true);

    startTimerHz (30);
}

VerticalGradientMeter::~VerticalGradientMeter()
{
    stopTimer();
}

const SharedColors& VerticalGradientMeter::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void VerticalGradientMeter::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    rebuildPeakGradient();
    repaint();
}

void VerticalGradientMeter::setColourRamps (ColourRampBank* bank) noexcept
{
    colourRamps = bank;
    repaint();
}

void VerticalGradientMeter::rebuildPeakGradient()
{
    const auto& theme = colors();
    const auto bounds = getLocalBounds().toFloat();
    // Peak fill tracks Meter Fill so dice / theme changes update without a resize.
    gradient2 = juce::ColourGradient { theme.meterFill.withAlpha (150.0f / 255.0f),
                                       bounds.getBottomLeft(),
                                       theme.meterFill.brighter (0.2f).withAlpha (230.0f / 255.0f),
                                       bounds.getTopLeft(),
                                       false };
}

juce::ColourGradient VerticalGradientMeter::makeRampGradient (const GradientRamp& ramp,
                                                              juce::Rectangle<float> body,
                                                              float alphaMul) const
{
    // Vertical meter: bottom = quiet (0), top = loud (1).
    const juce::Point<float> p0 (body.getCentreX(), body.getBottom());
    const juce::Point<float> p1 (body.getCentreX(), body.getY());
    juce::ColourGradient grad (ramp.colourForDriver (0.0f).withMultipliedAlpha (alphaMul),
                               p0.x, p0.y,
                               ramp.colourForDriver (1.0f).withMultipliedAlpha (alphaMul),
                               p1.x, p1.y, false);

    constexpr int kSamples = 20;
    for (int i = 1; i < kSamples; ++i)
    {
        const float t = (float) i / (float) kSamples;
        grad.addColour ((double) t, ramp.colourForDriver (t).withMultipliedAlpha (alphaMul));
    }
    return grad;
}

void VerticalGradientMeter::paintBarGlow (juce::Graphics& g,
                                          juce::Rectangle<float> bar,
                                          juce::Colour colour,
                                          float radiusPx,
                                          float spreadPx,
                                          float opacity01) const
{
    if (bar.getHeight() < 0.5f || opacity01 < 0.02f || radiusPx < 0.25f)
        return;

    if (! SharedResources::glowShadowEffectsEnabled())
        return;

    const int layers = juce::jlimit (2, 10, (int) std::ceil (radiusPx * 0.45f) + 2);
    for (int i = layers; i >= 1; --i)
    {
        const float t = (float) i / (float) layers;
        const float expand = (radiusPx + spreadPx) * t;
        const float a = opacity01 * 0.18f * (1.0f - t * 0.85f);
        g.setColour (colour.withAlpha (juce::jlimit (0.0f, 1.0f, a)));
        g.fillRoundedRectangle (bar.expanded (expand * 0.55f, expand), 2.0f);
    }
}

VerticalGradientMeter::MeterMode VerticalGradientMeter::getMeterMode() const
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter ("METER_MODE_ID")))
        return static_cast<MeterMode> (juce::jlimit (0, 2, choice->getIndex()));

    return MeterMode::PeakAndRms;
}

bool VerticalGradientMeter::isMsChannelMode() const
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter ("METER_CHANNEL_MODE_ID")))
        return choice->getIndex() == 1;

    if (auto* raw = treeState.getRawParameterValue ("METER_CHANNEL_MODE_ID"))
        return raw->load() >= 0.5f;

    return false;
}

juce::String VerticalGradientMeter::getChannelLabel() const
{
    if (isMsChannelMode())
        return slot == 0 ? "M" : "S";

    return slot == 0 ? "L" : "R";
}

float VerticalGradientMeter::getParamFloat (const char* id, float fallback) const
{
    if (auto* raw = treeState.getRawParameterValue (id))
        return raw->load();

    return fallback;
}

bool VerticalGradientMeter::getParamBool (const char* id, bool fallback) const
{
    if (auto* raw = treeState.getRawParameterValue (id))
        return raw->load() > 0.5f;
    return fallback;
}

float VerticalGradientMeter::dbToY (float db, float height) noexcept
{
    return juce::jmap (juce::jlimit (kMeterFloorDb, kMeterCeilDb, db),
                       kMeterFloorDb, kMeterCeilDb, 0.0f, height);
}

void VerticalGradientMeter::setTextChromeVisible (bool shouldShow) noexcept
{
    if (textChromeVisible == shouldShow)
        return;
    textChromeVisible = shouldShow;
    repaint();
}

juce::Rectangle<float> VerticalGradientMeter::getMeterBodyBounds() const
{
    if (! textChromeVisible)
        return getLocalBounds().toFloat().reduced (0.0f, 4.0f);
    return getLocalBounds().toFloat().reduced (0.0f, 40.0f);
}

juce::Rectangle<float> VerticalGradientMeter::getClipIndicatorBounds() const
{
    const auto body = getMeterBodyBounds();
    if (! textChromeVisible)
        return { body.getX(), body.getY(), body.getWidth(), juce::jmin (4.0f, body.getHeight() * 0.08f) };
    return { body.getX(), body.getY() - 20.0f, body.getWidth(), 15.0f };
}

juce::Rectangle<float> VerticalGradientMeter::getReadoutBounds() const
{
    // Wide enough for "-12.3" without ellipsis; may extend past the narrow bar.
    const float w = 48.0f;
    return { (static_cast<float> (getWidth()) - w) * 0.5f, 0.0f, w, 18.0f };
}

juce::Rectangle<float> VerticalGradientMeter::getChannelLabelBounds() const
{
    const auto body = getMeterBodyBounds();
    const float h = 14.0f;
    return { body.getX() - 4.0f, body.getBottom() + 2.0f, body.getWidth() + 8.0f, h };
}

void VerticalGradientMeter::paint (juce::Graphics& g)
{
    const auto& theme = colors();
    const auto bounds = getMeterBodyBounds();
    const auto mode = getMeterMode();
    rebuildPeakGradient();

    const juce::Colour customColor = theme.meterBackground.withAlpha (100.0f / 255.0f);
    g.setColour (customColor);
    g.fillRect (bounds);

    const bool showPeak = mode == MeterMode::Peak || mode == MeterMode::PeakAndRms;
    const bool showRms = mode == MeterMode::Rms || mode == MeterMode::PeakAndRms;

    const GradientRamp* peakRamp = nullptr;
    const GradientRamp* rmsRamp = nullptr;
    if (colourRamps != nullptr)
    {
        const auto& pr = colourRamps->get (ColourRampBank::Target::meterPeak);
        const auto& rr = colourRamps->get (ColourRampBank::Target::meterRms);
        if (pr.isUsable())
            peakRamp = &pr;
        if (rr.isUsable())
            rmsRamp = &rr;
    }

    auto fillBar = [&] (juce::Rectangle<float> bar, float levelDb,
                        const GradientRamp* ramp, float solidAlpha,
                        juce::Colour solidCol, bool useGlow,
                        const char* glowEnId, const char* thrId,
                        const char* radId, const char* sprId, const char* opaId)
    {
        if (bar.getHeight() < 0.5f)
            return;

        juce::Colour tipCol = solidCol;
        if (ramp != nullptr)
        {
            const float t01 = juce::jlimit (
                0.0f, 1.0f,
                (juce::jlimit (kMeterFloorDb, kMeterCeilDb, levelDb) - kMeterFloorDb)
                    / (kMeterCeilDb - kMeterFloorDb));
            tipCol = ramp->colourForDriver (t01).withMultipliedAlpha (solidAlpha);
            g.setGradientFill (makeRampGradient (*ramp, bounds, solidAlpha));
        }
        else
        {
            g.setColour (solidCol.withAlpha (solidAlpha));
        }

        if (useGlow && getParamBool (glowEnId, false)
            && levelDb >= getParamFloat (thrId, -18.0f))
        {
            const float radius = getParamFloat (radId, 12.0f);
            const float spread = getParamFloat (sprId, 4.0f);
            const float opacity = juce::jlimit (0.0f, 1.0f, getParamFloat (opaId, 70.0f) * 0.01f);
            paintBarGlow (g, bar, tipCol, radius, spread, opacity);
        }

        if (ramp != nullptr)
            g.setGradientFill (makeRampGradient (*ramp, bounds, solidAlpha));
        else
            g.setColour (solidCol.withAlpha (solidAlpha));

        g.fillRect (bar);
    };

    if (showPeak)
    {
        const float scaledY = dbToY (displayedPeakDb, bounds.getHeight());
        const auto bar = juce::Rectangle<float> (bounds.getX(),
                                                 bounds.getBottom() - scaledY,
                                                 bounds.getWidth(),
                                                 scaledY);
        if (peakRamp != nullptr)
            fillBar (bar, displayedPeakDb, peakRamp, 0.95f, theme.meterFill, true,
                     "METER_PEAK_GLOW_ENABLE_ID", "METER_PEAK_GLOW_THRESHOLD_ID",
                     "METER_PEAK_GLOW_RADIUS_ID", "METER_PEAK_GLOW_SPREAD_ID",
                     "METER_PEAK_GLOW_OPACITY_ID");
        else
        {
            // Theme gradient (legacy).
            g.setGradientFill (gradient2);
            if (getParamBool ("METER_PEAK_GLOW_ENABLE_ID", false)
                && displayedPeakDb >= getParamFloat ("METER_PEAK_GLOW_THRESHOLD_ID", -18.0f))
            {
                paintBarGlow (g, bar, theme.meterFill.brighter (0.2f),
                              getParamFloat ("METER_PEAK_GLOW_RADIUS_ID", 12.0f),
                              getParamFloat ("METER_PEAK_GLOW_SPREAD_ID", 4.0f),
                              juce::jlimit (0.0f, 1.0f,
                                            getParamFloat ("METER_PEAK_GLOW_OPACITY_ID", 70.0f) * 0.01f));
            }
            g.setGradientFill (gradient2);
            g.fillRect (bar);
        }
    }

    if (showRms)
    {
        const float rmsAlpha = mode == MeterMode::PeakAndRms ? 0.92f : 1.0f;
        const float scaledY = dbToY (displayedRmsDb, bounds.getHeight());
        const float barW = mode == MeterMode::PeakAndRms
                               ? juce::jmax (2.0f, bounds.getWidth() * 0.45f)
                               : bounds.getWidth();
        const float x = bounds.getCentreX() - barW * 0.5f;
        const auto bar = juce::Rectangle<float> (x, bounds.getBottom() - scaledY, barW, scaledY);
        const auto solid = theme.meterFill.brighter (mode == MeterMode::PeakAndRms ? 0.15f : 0.0f);

        fillBar (bar, displayedRmsDb, rmsRamp, rmsAlpha, solid, true,
                 "METER_RMS_GLOW_ENABLE_ID", "METER_RMS_GLOW_THRESHOLD_ID",
                 "METER_RMS_GLOW_RADIUS_ID", "METER_RMS_GLOW_SPREAD_ID",
                 "METER_RMS_GLOW_OPACITY_ID");
    }

    // Peak-hold tick
    if (showPeak && peakHoldDb > kMeterFloorDb + 0.5f)
    {
        const float y = bounds.getBottom() - dbToY (peakHoldDb, bounds.getHeight());
        g.setColour (theme.graphAxisText.withAlpha (0.85f));
        g.fillRect (bounds.getX(), y - 1.0f, bounds.getWidth(), 2.0f);
    }

    g.setColour (clipState.clipping ? theme.meterClip
                                    : customColor);
    g.fillRect (getClipIndicatorBounds());

    if (! textChromeVisible)
        return;

    // Readout: 0.1 dB resolution, painted unclipped so it is never "..."
    const float textDb = std::max (readoutDb, -99.9f);
    const auto text = juce::String (textDb, 1);
    g.setFont (juce::FontOptions ("Lato Black", 10.0f, juce::Font::plain));
    g.setColour (clipState.clipping ? theme.meterClip
                                    : theme.meterReadoutText.withAlpha (0.95f));
    g.drawText (text, getReadoutBounds(), juce::Justification::centred, false);

    g.setFont (juce::FontOptions ("Lato Black", 11.0f, juce::Font::plain));
    g.setColour (theme.graphAxisText.withAlpha (0.75f));
    g.drawText (getChannelLabel(), getChannelLabelBounds(), juce::Justification::centred, false);
}

bool VerticalGradientMeter::hitTest (int x, int y)
{
    return getClipIndicatorBounds().contains (static_cast<float> (x), static_cast<float> (y));
}

void VerticalGradientMeter::mouseDown (const juce::MouseEvent& event)
{
    juce::ignoreUnused (event);
    clipState.reset();
    peakHoldDb = -100.0f;
    peakHoldUntilMs = 0.0;
}

void VerticalGradientMeter::timerCallback()
{
    const float peakDb = peakSupplier != nullptr ? peakSupplier() : kSilenceDb;
    const float rmsDb = rmsSupplier != nullptr ? rmsSupplier() : kSilenceDb;
    const auto mode = getMeterMode();

    const float fallSec = juce::jmax (0.05f, getParamFloat ("METER_FALL_ID", 0.75f));
    const float integrationSec = juce::jmax (0.05f, getParamFloat ("METER_READOUT_INTEGRATION_ID", 0.45f) * 0.001f);
    const float peakHoldSec = juce::jmax (0.0f, getParamFloat ("METER_PEAK_HOLD_ID", 1.5f));
    const float clipHoldSec = juce::jmax (0.25f, getParamFloat ("METER_CLIP_HOLD_ID", 3.0f));
    const float clipThresholdDb = getParamFloat ("METER_CLIP_THRESHOLD_ID", 0.0f);

    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    const float dt = 1.0f / 30.0f;

    auto applyFall = [dt, fallSec] (float current, float target) -> float
    {
        if (target >= current)
            return target;

        const float coeff = 1.0f - std::exp (-dt / fallSec);
        return current + (target - current) * coeff;
    };

    displayedPeakDb = applyFall (displayedPeakDb, peakDb);
    displayedRmsDb = applyFall (displayedRmsDb, rmsDb);

    // Peak hold tick
    if (peakDb >= peakHoldDb)
    {
        peakHoldDb = peakDb;
        peakHoldUntilMs = nowMs + static_cast<double> (peakHoldSec * 1000.0f);
    }
    else if (nowMs >= peakHoldUntilMs)
    {
        peakHoldDb = applyFall (peakHoldDb, peakDb);
    }

    // Clip detection uses true sample-peak (Ableton-style), not RMS.
    if (peakDb >= clipThresholdDb)
    {
        clipState.clipping = true;
        clipState.heldPeakDb = juce::jmax (clipState.heldPeakDb, peakDb);
        clipState.clipClearAtMs = nowMs + static_cast<double> (clipHoldSec * 1000.0f);
    }
    else if (clipState.clipping && nowMs >= clipState.clipClearAtMs)
    {
        clipState.clipping = false;
        clipState.heldPeakDb = -100.0f;
    }

    float targetReadout = peakDb;
    if (mode == MeterMode::Rms)
        targetReadout = rmsDb;
    else if (mode == MeterMode::PeakAndRms)
        targetReadout = peakDb; // primary numeric = peak (matches DAW clip behaviour)

    // While clipping, hold the excursion peak in the readout so it stays readable.
    if (clipState.clipping && clipState.heldPeakDb > targetReadout)
        targetReadout = clipState.heldPeakDb;

    const float readoutCoeff = 1.0f - std::exp (-dt / integrationSec);
    readoutDb += (targetReadout - readoutDb) * readoutCoeff;

    repaint();
}

void VerticalGradientMeter::resized()
{
    rebuildPeakGradient();
}
