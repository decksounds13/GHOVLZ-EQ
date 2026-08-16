#include "TransferCurveComponent.h"

TransferCurveComponent::TransferCurveComponent (juce::AudioProcessorValueTreeState& s,
                                                std::function<int()> selectedBand)
    : state (s), getBand (std::move (selectedBand))
{
    trailIn.fill (-140.0f);
    startTimerHz (36);
    setOpaque (true);
    setBufferedToImage (true);
}

void TransferCurveComponent::setCompact (bool should) noexcept
{
    compact = should;
    setOpaque (! should);
    repaint();
}

float TransferCurveComponent::readF (const juce::String& id, float fallback) const noexcept
{
    if (auto* p = state.getRawParameterValue (id))
        return p->load();
    return fallback;
}

float TransferCurveComponent::compressedOutDb (float inDb, float threshold, float ratio, float knee) noexcept
{
    return DynParams::transferOutDb (inDb, threshold, threshold - 18.0f, ratio, knee);
}

void TransferCurveComponent::timerCallback()
{
    const int band = getBand ? juce::jlimit (0, DynParams::kMaxBands - 1, getBand()) : 0;
    if (band != lastBand)
    {
        displayedInDb = -140.0f;
        peakInDb = -140.0f;
        trailIn.fill (-140.0f);
        trailWrite = 0;
        trailCount = 0;
        lastBand = band;
    }

    const float target = engine != nullptr ? engine->getInputEnvelopeDb (band) : -140.0f;
    const float rise = 0.55f;
    const float fall = 0.22f;
    if (target > displayedInDb)
        displayedInDb += rise * (target - displayedInDb);
    else
        displayedInDb += fall * (target - displayedInDb);

    if (target > peakInDb)
        peakInDb = target;
    else
        peakInDb = juce::jmax (target, peakInDb - 0.55f);

    trailIn[(size_t) trailWrite] = displayedInDb;
    trailWrite = (trailWrite + 1) % kTrail;
    trailCount = juce::jmin (kTrail, trailCount + 1);

    repaint();
}

void TransferCurveComponent::drawLiveBall (juce::Graphics& g, juce::Point<float> c, float r,
                                           juce::Colour fill, juce::Colour glow, bool core) const
{
    if (SharedResources::glowShadowEffectsEnabled())
    {
        g.setColour (glow.withAlpha (0.38f));
        g.fillEllipse (c.x - r - 3.0f, c.y - r - 3.0f, (r + 3.0f) * 2.0f, (r + 3.0f) * 2.0f);
    }

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (c.x - r - 1.15f, c.y - r - 1.15f, (r + 1.15f) * 2.0f, (r + 1.15f) * 2.0f);
    g.setColour (fill);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    if (core)
    {
        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.fillEllipse (c.x - r * 0.32f, c.y - r * 0.46f, r * 0.62f, r * 0.50f);
    }
}

void TransferCurveComponent::paint (juce::Graphics& g)
{
    const auto& c = theme != nullptr ? theme->sharedColors : SharedColors {};
    auto bounds = getLocalBounds().toFloat();
    if (bounds.getWidth() < 8.0f || bounds.getHeight() < 8.0f)
        return;

    const int band = getBand ? juce::jlimit (0, DynParams::kMaxBands - 1, getBand()) : 0;
    const float thr    = readF (DynParams::thresholdId (band), -18.0f);
    const float upThr  = readF (DynParams::upThresholdId (band), -36.0f);
    const float ratio  = DynParams::scaleRatio (readF (DynParams::ratioId (band), 4.0f),
                                                readF (DynParams::amountId(), 100.0f));
    const float downAmt = readF (DynParams::downAmtId(), 100.0f);
    const float upAmt   = readF (DynParams::upAmtId(), 100.0f);
    const float knee   = juce::jmax (0.0f, readF (DynParams::kneeId (band), 8.0f));
    auto law = [&] (float inDb)
    {
        return DynParams::transferOutDb (inDb, thr, upThr, ratio, knee, downAmt, upAmt);
    };

    // Fill the full rect first so rounded-corner leftovers cannot flash white.
    g.fillAll (c.graphBackground);
    g.setGradientFill (juce::ColourGradient (c.graphBackground2.interpolatedWith (c.graphBackground, 0.35f),
                                             bounds.getX(), bounds.getY(),
                                             c.graphBackground, bounds.getX(), bounds.getBottom(), false));
    g.fillRect (bounds);

    auto work = bounds.reduced (compact ? 8.0f : 14.0f, compact ? 8.0f : 14.0f);

    if (! compact)
    {
        auto head = work.removeFromTop (16.0f);
        work.removeFromTop (3.0f);

        const auto titleFont = SharedResources::uiFont (10.5f);
        g.setFont (titleFont);
        g.setColour (c.graphAxisText.withAlpha (0.62f));
        g.drawText ("Transfer", head.toNearestInt(), juce::Justification::centredLeft, false);

        juce::String readout = DynParams::formatRatio (ratio)
                               + "    Knee " + juce::String (knee, 1) + " dB";
        g.setColour (c.graphAxisText.withAlpha (0.74f));
        g.drawText (readout, head.toNearestInt(), juce::Justification::centredRight, false);
    }

    const float side = juce::jmin (work.getWidth(), work.getHeight());
    auto plot = work.withSizeKeepingCentre (side, side);
    if (plot.getWidth() < 16.0f)
        return;

    g.setColour (c.graphBackground.darker (0.12f));
    g.fillRect (plot);
    g.setColour (c.graphGrid.withMultipliedAlpha (0.22f));
    g.drawRect (plot, 1.0f);

    auto xOf = [&plot] (float db)
    {
        return plot.getX() + (juce::jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f * plot.getWidth();
    };
    auto yOf = [&plot] (float db)
    {
        return plot.getBottom() - (juce::jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f * plot.getHeight();
    };

    const bool live = displayedInDb > -72.0f;
    const float liveIn = juce::jlimit (-60.0f, 0.0f, displayedInDb);
    const float liveOut = law (liveIn);
    const float peakIn = juce::jlimit (-60.0f, 0.0f, peakInDb);
    const float liveX = xOf (liveIn);
    const float liveY = yOf (liveOut);

    // Current-level wash (Pro-C "area shows the signal").
    if (live)
    {
        auto wash = juce::Rectangle<float> (plot.getX() + 1.0f, plot.getY() + 1.0f,
                                            juce::jmax (0.0f, liveX - plot.getX() - 1.0f),
                                            plot.getHeight() - 2.0f);
        juce::ColourGradient fill (c.graphSumFillTop.withAlpha (compact ? 0.10f : 0.16f),
                                   wash.getX(), wash.getY(),
                                   c.graphSumFillBottom.withAlpha (0.03f),
                                   wash.getX(), wash.getBottom(), false);
        g.setGradientFill (fill);
        g.fillRect (wash);
    }

    g.setColour (c.graphGrid.withMultipliedAlpha (0.18f));
    for (int db = -48; db <= -12; db += 12)
    {
        const float x = xOf ((float) db);
        const float y = yOf ((float) db);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX() + 1.0f, plot.getRight() - 1.0f);
        g.drawVerticalLine   (juce::roundToInt (x), plot.getY() + 1.0f, plot.getBottom() - 1.0f);
    }

    {
        juce::Path unity;
        unity.startNewSubPath (plot.getX(), plot.getBottom());
        unity.lineTo (plot.getRight(), plot.getY());
        g.setColour (c.graphGrid.withMultipliedAlpha (0.32f));
        const float dashes[] = { 4.0f, 3.0f };
        juce::PathStrokeType unityStroke (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
        juce::Path dashed;
        unityStroke.createDashedStroke (dashed, unity, dashes, 2);
        g.strokePath (dashed, juce::PathStrokeType (1.0f));
    }

    if (knee > 0.25f)
    {
        const float x0 = juce::jlimit (plot.getX(), plot.getRight(), xOf (thr - 0.5f * knee));
        const float x1 = juce::jlimit (plot.getX(), plot.getRight(), xOf (thr + 0.5f * knee));
        if (x1 > x0 + 1.0f)
        {
            juce::ColourGradient wash (c.graphSumGlow.withAlpha (0.00f), x0, plot.getY(),
                                       c.graphSumGlow.withAlpha (0.00f), x1, plot.getY(), false);
            wash.addColour (0.5, c.graphSumGlow.withAlpha (compact ? 0.10f : 0.14f));
            g.setGradientFill (wash);
            g.fillRect (x0, plot.getY() + 1.0f, x1 - x0, plot.getHeight() - 2.0f);
        }
    }

    const int nPts = compact ? 96 : 192;
    juce::Path curve;
    juce::Path grFill;
    for (int i = 0; i <= nPts; ++i)
    {
        const float inDb = -60.0f + 60.0f * (float) i / (float) nPts;
        const float x = xOf (inDb);
        const float y = yOf (law (inDb));
        if (i == 0)
        {
            curve.startNewSubPath (x, y);
            grFill.startNewSubPath (x, yOf (inDb));
        }
        else
        {
            curve.lineTo (x, y);
        }
        grFill.lineTo (x, y);
    }
    for (int i = nPts; i >= 0; --i)
    {
        const float inDb = -60.0f + 60.0f * (float) i / (float) nPts;
        grFill.lineTo (xOf (inDb), yOf (inDb));
    }
    grFill.closeSubPath();

    const auto smooth = curve.createPathWithRoundedCorners (3.0f);
    const float lineW = compact ? 1.55f : 2.15f;
    const auto dimStroke = juce::PathStrokeType (lineW,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded);

    {
        juce::Graphics::ScopedSaveState clip (g);
        g.reduceClipRegion (plot.toNearestInt().expanded (1));

        g.setGradientFill (juce::ColourGradient (c.graphSumFillTop.withAlpha (compact ? 0.22f : 0.32f),
                                                 plot.getX(), plot.getY(),
                                                 c.graphSumFillBottom.withAlpha (compact ? 0.06f : 0.10f),
                                                 plot.getX(), plot.getBottom(), false));
        g.fillPath (grFill);

        // Rest of the curve stays quiet (Pro-C: unlit path).
        g.setColour (c.graphSumCurve.withAlpha (0.28f));
        g.strokePath (smooth, dimStroke);

        // Lit path up to the live input.
        if (live)
        {
            auto litClip = plot;
            litClip.setRight (juce::jmin (plot.getRight(), liveX + 1.0f));
            g.reduceClipRegion (litClip.toNearestInt());

            if (SharedResources::glowShadowEffectsEnabled())
            {
                const auto bloom = c.graphSumGlow.withAlpha (compact ? 0.34f : 0.46f);
                const auto core  = c.graphSumGlow.brighter (0.18f).withAlpha (compact ? 0.58f : 0.76f);
                const float rad  = compact ? 8.0f : 14.0f;

                curveGlow.setRadius ((double) rad, 0);
                curveGlow.setSpread (compact ? 1.0 : 2.0, 0);
                curveGlow.setOffset (0, 0, 0);
                curveGlow.setColor (bloom, 0);
                curveGlow.setRadius ((double) juce::jmax (2.0f, rad * 0.35f), 1);
                curveGlow.setSpread (0.0, 1);
                curveGlow.setOffset (0, 0, 1);
                curveGlow.setColor (core, 1);
                curveGlow.render (g, smooth,
                                  juce::PathStrokeType (lineW + 0.4f,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded),
                                  true);
            }

            g.setColour (c.graphSumCurve);
            g.strokePath (smooth, juce::PathStrokeType (lineW,
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
            g.setColour (c.graphSumCurve.brighter (0.45f).withAlpha (0.55f));
            g.strokePath (smooth, juce::PathStrokeType (juce::jmax (0.8f, lineW * 0.45f),
                                                        juce::PathStrokeType::curved,
                                                        juce::PathStrokeType::rounded));
        }
    }

    {
        const float dashes[] = { 3.5f, 2.5f };
        g.setColour (juce::Colour::fromRGB (61, 204, 106).withAlpha (0.70f));
        g.drawDashedLine ({ xOf (thr), plot.getY() + 1.0f, xOf (thr), plot.getBottom() - 1.0f }, dashes, 2, 1.15f);
        g.setColour (juce::Colour::fromRGB (232, 180, 74).withAlpha (0.70f));
        g.drawDashedLine ({ xOf (upThr), plot.getY() + 1.0f, xOf (upThr), plot.getBottom() - 1.0f }, dashes, 2, 1.15f);
    }

    auto drawThrDot = [&] (float db, juce::Colour fill)
    {
        const float hx = xOf (db);
        const float hy = yOf (law (db));
        const float r  = compact ? 2.6f : 3.4f;
        g.setColour (c.graphHandleOutline.withAlpha (0.80f));
        g.fillEllipse (hx - r - 0.9f, hy - r - 0.9f, (r + 0.9f) * 2.0f, (r + 0.9f) * 2.0f);
        g.setColour (fill);
        g.fillEllipse (hx - r, hy - r, r * 2.0f, r * 2.0f);
    };
    drawThrDot (thr, juce::Colour::fromRGB (255, 148, 28).withAlpha (0.90f));
    drawThrDot (upThr, juce::Colour::fromRGB (232, 196, 96).withAlpha (0.90f));

    if (live)
    {
        const float dashes[] = { 3.0f, 2.5f };

        // Axis hairlines from the ball (input on X, output on Y).
        g.setColour (c.graphSumCurve.withAlpha (0.28f));
        g.drawDashedLine ({ liveX, plot.getBottom() - 1.0f, liveX, liveY }, dashes, 2, 1.0f);
        g.drawDashedLine ({ plot.getX() + 1.0f, liveY, liveX, liveY }, dashes, 2, 1.0f);

        // GR stub: unity → compressed (how much is being taken).
        const float unityY = yOf (liveIn);
        if (std::abs (unityY - liveY) > 1.5f)
        {
            g.setColour (c.knobArc.withAlpha (0.70f));
            g.drawLine (liveX, unityY, liveX, liveY, compact ? 1.3f : 1.6f);
        }

        // Fading comet of recent envelope positions.
        if (! compact)
        {
            for (int i = 0; i < trailCount; ++i)
            {
                const int idx = (trailWrite - 1 - i + kTrail * 4) % kTrail;
                const float tin = trailIn[(size_t) idx];
                if (tin < -72.0f)
                    continue;
                const float tx = xOf (tin);
                const float ty = yOf (law (tin));
                const float a = 0.10f * (1.0f - (float) i / (float) kTrail);
                const float tr = 2.2f * (1.0f - 0.45f * (float) i / (float) kTrail);
                g.setColour (c.graphSumGlow.withAlpha (a));
                g.fillEllipse (tx - tr, ty - tr, tr * 2.0f, tr * 2.0f);
            }
        }

        // Peak-hold ring (falls back slower than the ball).
        if (peakIn > liveIn + 0.4f)
        {
            const float px = xOf (peakIn);
            const float py = yOf (law (peakIn));
            const float pr = compact ? 3.2f : 4.4f;
            g.setColour (c.graphSumCurve.withAlpha (0.45f));
            g.drawEllipse (px - pr, py - pr, pr * 2.0f, pr * 2.0f, 1.15f);
        }

        drawLiveBall (g, { liveX, liveY },
                      compact ? 3.6f : 5.1f,
                      c.graphSumCurve.brighter (0.15f),
                      c.graphSumGlow,
                      true);
    }

    if (! compact && plot.getWidth() >= 72.0f)
    {
        const auto axisFont = SharedResources::uiFont (10.0f);
        g.setFont (axisFont);
        g.setColour (c.graphAxisText.withAlpha (0.48f));

        g.drawText ("0 dB",
                    juce::Rectangle<int> (juce::roundToInt (plot.getX()) + 3,
                                          juce::roundToInt (plot.getY()) + 2, 40, 12),
                    juce::Justification::topLeft, false);
        g.drawText ("-60 dB",
                    juce::Rectangle<int> (juce::roundToInt (plot.getX()) + 3,
                                          juce::roundToInt (plot.getBottom()) - 14, 52, 12),
                    juce::Justification::bottomLeft, false);
        g.drawText ("In",
                    juce::Rectangle<int> (juce::roundToInt (plot.getRight()) - 28,
                                          juce::roundToInt (plot.getBottom()) - 14, 26, 12),
                    juce::Justification::bottomRight, false);
        g.drawText ("Out",
                    juce::Rectangle<int> (juce::roundToInt (plot.getX()) + 3,
                                          juce::roundToInt (plot.getY()) + 14, 36, 12),
                    juce::Justification::topLeft, false);
    }
}
