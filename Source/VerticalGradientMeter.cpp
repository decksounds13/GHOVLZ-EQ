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

    if (showPeak)
    {
        g.setGradientFill (gradient2);
        const float scaledY = dbToY (displayedPeakDb, bounds.getHeight());
        g.fillRect (juce::Rectangle<float> (bounds.getX(),
                                            bounds.getBottom() - scaledY,
                                            bounds.getWidth(),
                                            scaledY));
    }

    if (showRms)
    {
        // Peak+RMS: brighter / more opaque so the RMS lane isn't washed by the peak fill under it.
        const float rmsAlpha = mode == MeterMode::PeakAndRms ? 0.92f : 1.0f;
        g.setColour (theme.meterFill.brighter (mode == MeterMode::PeakAndRms ? 0.15f : 0.0f)
                                 .withAlpha (rmsAlpha));
        const float scaledY = dbToY (displayedRmsDb, bounds.getHeight());
        const float barW = mode == MeterMode::PeakAndRms
                               ? juce::jmax (2.0f, bounds.getWidth() * 0.45f)
                               : bounds.getWidth();
        const float x = bounds.getCentreX() - barW * 0.5f;
        g.fillRect (juce::Rectangle<float> (x, bounds.getBottom() - scaledY, barW, scaledY));
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
