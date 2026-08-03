#include "GoniometerComponent.h"

GoniometerComponent::GoniometerComponent()
{
    setOpaque (false);
    setVisible (false);
}

GoniometerComponent::~GoniometerComponent()
{
    stopTimer();
}

void GoniometerComponent::setColourRamp (const GradientRamp& ramp)
{
    colourRamp = ramp;
    hasCustomRamp = ramp.isUsable();
    repaint();
}

void GoniometerComponent::clearColourRamp()
{
    hasCustomRamp = false;
    colourRamp = {};
    repaint();
}

const SharedColors& GoniometerComponent::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void GoniometerComponent::prepare (double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    sampleRateHz.store (sr, std::memory_order_relaxed);

    const int newCap = juce::jmax (kMaxPlotSamplesPerTick,
                                   (int) std::ceil (sr * (double) kMaxBufferSeconds));
    // Skip zero-fill realloc when capacity is unchanged (prepareToPlay can re-enter).
    if (newCap != capacity.load (std::memory_order_relaxed)
        || (int) ringL.size() != newCap
        || (int) ringR.size() != newCap)
    {
        ringL.assign ((size_t) newCap, 0.0f);
        ringR.assign ((size_t) newCap, 0.0f);
    }
    else
    {
        std::fill (ringL.begin(), ringL.end(), 0.0f);
        std::fill (ringR.begin(), ringR.end(), 0.0f);
    }
    writePos.store (0, std::memory_order_relaxed);
    capacity.store (newCap, std::memory_order_release);
    resetDisplay();
}

void GoniometerComponent::resetDisplay()
{
    ringReadPos = writePos.load (std::memory_order_acquire);
    plotReadPos = ringReadPos;
    corrSumLR = corrSumLL = corrSumRR = 0.0;
    correlation = 0.0f;
    correlationDisplay = 0.0f;
    lastGlowPath.clear();

    if (trailImage.isValid())
        trailImage.clear (trailImage.getBounds(), juce::Colours::transparentBlack);
}

void GoniometerComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
    setVisible (shouldEnable);

    if (shouldEnable)
    {
        resetDisplay();
        // Compact stays light; expanded can afford a bit more.
        startTimerHz (expanded ? 30 : 20);
    }
    else
    {
        stopTimer();
    }
}

void GoniometerComponent::setExpanded (bool shouldExpand) noexcept
{
    if (expanded == shouldExpand)
        return;

    expanded = shouldExpand;
    setInterceptsMouseClicks (! expanded, ! expanded);
    resetDisplay();

    if (enabled.load (std::memory_order_relaxed))
        startTimerHz (expanded ? 30 : 20);

    resized();
    repaint();
}

float GoniometerComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree == nullptr)
        return fallback;

    if (auto* v = valueTree->getRawParameterValue (id))
        return v->load();

    return fallback;
}

bool GoniometerComponent::isHighQuality() const
{
    if (valueTree == nullptr)
        return false;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (valueTree->getParameter ("GON_QUALITY_ID")))
        return choice->getIndex() >= 1;

    return false;
}

void GoniometerComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || left == nullptr || numSamples <= 0)
        return;

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0 || (int) ringL.size() != cap || (int) ringR.size() != cap)
        return;

    const float* r = right != nullptr ? right : left;
    int w = writePos.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        ringL[(size_t) w] = left[i];
        ringR[(size_t) w] = r[i];
        w = (w + 1) % cap;
    }

    writePos.store (w, std::memory_order_release);
}

void GoniometerComponent::ensureTrailImage (int plotSize)
{
    const int size = juce::jmax (8, plotSize);
    if (trailImage.isValid()
        && trailImage.getWidth() == size
        && trailImage.getHeight() == size)
        return;

    trailImage = juce::Image (juce::Image::ARGB, size, size, true);
}

juce::Rectangle<int> GoniometerComponent::getPlotBounds() const noexcept
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return {};

    if (expanded)
    {
        // Largest centered square — correlation overlays this square's right edge.
        const int side = juce::jmin (bounds.getWidth(), bounds.getHeight());
        return { bounds.getCentreX() - side / 2,
                 bounds.getCentreY() - side / 2,
                 side,
                 side };
    }

    const int side = juce::jmin (kWindowHeightPx, bounds.getHeight());
    return { bounds.getX(), bounds.getY() + (bounds.getHeight() - side) / 2, side, side };
}

juce::Rectangle<int> GoniometerComponent::getCorrelationBounds() const noexcept
{
    auto bounds = getLocalBounds();
    if (bounds.isEmpty())
        return {};

    if (expanded)
    {
        const int corrW = juce::jmax (kCorrelationWidthPx + 4, bounds.getWidth() / 36);
        auto plot = getPlotBounds();
        // Flush to the right edge of the darkened square (on top of it).
        return { plot.getRight() - corrW,
                 plot.getY(),
                 corrW,
                 plot.getHeight() };
    }

    const int corrW = kCorrelationWidthPx;
    const int side = juce::jmin (kWindowHeightPx, bounds.getHeight());
    return { bounds.getRight() - corrW,
             bounds.getY() + (bounds.getHeight() - side) / 2,
             corrW,
             side };
}

void GoniometerComponent::updateCorrelationFromRing()
{
    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0)
        return;

    const int w = writePos.load (std::memory_order_acquire);
    int available = w - ringReadPos;
    if (available < 0)
        available += cap;

    // Correlation only needs a short window — keep this cheap on the message thread.
    constexpr int kMaxCorrSamplesPerTick = 512;
    if (available > kMaxCorrSamplesPerTick)
    {
        ringReadPos = (w - kMaxCorrSamplesPerTick + cap) % cap;
        available = kMaxCorrSamplesPerTick;
    }

    const double sr = sampleRateHz.load (std::memory_order_relaxed);
    const double windowSamples = juce::jmax (64.0, sr * 0.30);
    const double decay = std::exp (-(double) available / windowSamples);

    corrSumLR *= decay;
    corrSumLL *= decay;
    corrSumRR *= decay;

    int pos = ringReadPos;
    for (int i = 0; i < available; ++i)
    {
        const double L = (double) ringL[(size_t) pos];
        const double R = (double) ringR[(size_t) pos];
        corrSumLR += L * R;
        corrSumLL += L * L;
        corrSumRR += R * R;
        pos = (pos + 1) % cap;
    }
    ringReadPos = pos;

    const double denom = std::sqrt (corrSumLL * corrSumRR);
    if (denom > 1.0e-12)
        correlation = (float) juce::jlimit (-1.0, 1.0, corrSumLR / denom);
    else
        correlation = 0.0f;

    const float a = 0.25f;
    correlationDisplay += a * (correlation - correlationDisplay);
}

void GoniometerComponent::buildGlowPath (juce::Path& outPath, float pointRadius) const
{
    outPath.clear();

    auto plot = getPlotBounds().toFloat();
    if (plot.isEmpty())
        return;

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0)
        return;

    // Sparse cloud only — Melatonin re-blurs whenever the path changes.
    const int maxGlow = expanded ? (isHighQuality() ? 96 : 48) : (isHighQuality() ? 48 : 24);
    const int n = juce::jmin (maxGlow * 2, cap);
    const int w = writePos.load (std::memory_order_acquire);
    const int step = juce::jmax (1, n / maxGlow);
    const float cx = plot.getCentreX();
    const float cy = plot.getCentreY();
    const float scale = plot.getWidth() * 0.42f;
    const float r = juce::jmax (1.1f, pointRadius);

    int pos = (w - n + cap) % cap;
    for (int i = 0; i < n; i += step)
    {
        const float L = ringL[(size_t) pos];
        const float R = ringR[(size_t) pos];
        const float mid = 0.70710678f * (L + R);
        const float side = 0.70710678f * (L - R);
        const float x = cx + side * scale;
        const float y = cy - mid * scale;
        outPath.addEllipse (x - r, y - r, r * 2.0f, r * 2.0f);
        pos = (pos + step) % cap;
    }
}

void GoniometerComponent::fadeAndPlotTrail()
{
    auto plot = getPlotBounds();
    if (plot.isEmpty())
        return;

    ensureTrailImage (plot.getWidth());
    if (! trailImage.isValid())
        return;

    // Shared with oscilloscope (Osc/Gon Line theme + OSC line opacity).
    const float lineOpacity = juce::jlimit (0.0f, 1.0f, loadFloatParam ("OSC_LINE_OPACITY_ID", 100.0f) * 0.01f);
    const float lineWidth = juce::jmax (0.5f,
                                        loadFloatParam (expanded ? "GON_EXPANDED_LINE_WIDTH_ID" : "GON_LINE_WIDTH_ID",
                                                        expanded ? 2.6f : 1.6f));
    const bool highQuality = isHighQuality();

    juce::Graphics tg (trailImage);
    // Persistence fade — trail image holds history, so we only ink NEW samples below.
    tg.setColour (juce::Colours::black.withAlpha (highQuality ? 0.12f : 0.18f));
    tg.fillAll();
    tg.setOpacity (1.0f);

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0)
        return;

    const int w = writePos.load (std::memory_order_acquire);
    int available = w - plotReadPos;
    if (available < 0)
        available += cap;

    if (available <= 0)
    {
        // Still rebuild sparse glow from recent ring so bloom tracks motion.
    }
    else
    {
        // Hard cap so one timer tick never rasterizes thousands of ellipses.
        const int maxPts = expanded
                               ? (highQuality ? 384 : 192)
                               : (highQuality ? 192 : 96);
        int step = 1;
        if (available > maxPts)
        {
            step = (available + maxPts - 1) / maxPts;
            // Drop backlog — keep the newest window.
            plotReadPos = (w - maxPts * step + cap) % cap;
            available = maxPts * step;
        }

        const float size = (float) trailImage.getWidth();
        const float cx = size * 0.5f;
        const float cy = size * 0.5f;
        const float scale = size * 0.42f;
        const float point = juce::jmax (1.0f, lineWidth * (highQuality ? 0.85f : 0.7f));
        const bool useRect = point <= 2.25f; // fillRect is much cheaper than fillEllipse

        const auto& theme = colors();
        const bool useRamp = loadFloatParam ("GON_USE_RAMP_ID", 1.0f) > 0.5f
                             && hasCustomRamp && colourRamp.isUsable();
        const auto solidInk = theme.oscLine.withMultipliedAlpha (lineOpacity);

        int pos = plotReadPos;
        int drawn = 0;
        while (drawn < available)
        {
            const float L = ringL[(size_t) pos];
            const float R = ringR[(size_t) pos];
            const float mid = 0.70710678f * (L + R);
            const float side = 0.70710678f * (L - R);
            const float x = cx + side * scale;
            const float y = cy - mid * scale;

            if (useRamp)
            {
                float driver = 0.0f;
                switch (colourRamp.mapMode)
                {
                    case GradientRamp::MapMode::gonDiversionX:
                        driver = juce::jlimit (0.0f, 1.0f, std::abs (side));
                        break;
                    case GradientRamp::MapMode::gonDiversionY:
                        driver = juce::jlimit (0.0f, 1.0f, std::abs (mid));
                        break;
                    case GradientRamp::MapMode::gonDiversionXY:
                        driver = juce::jlimit (0.0f, 1.0f, std::sqrt (mid * mid + side * side));
                        break;
                    case GradientRamp::MapMode::gonLoudness:
                    default:
                        driver = juce::jlimit (0.0f, 1.0f, std::sqrt (L * L + R * R));
                        break;
                }
                tg.setColour (colourRamp.colourForDriver (driver).withMultipliedAlpha (lineOpacity));
            }
            else
            {
                tg.setColour (solidInk);
            }

            if (useRect)
                tg.fillRect (x - point * 0.5f, y - point * 0.5f, point, point);
            else
                tg.fillEllipse (x - point * 0.5f, y - point * 0.5f, point, point);

            pos = (pos + step) % cap;
            drawn += step;
        }
        plotReadPos = w;
    }

    const bool glowEnabled = loadFloatParam (expanded ? "GON_EXPANDED_GLOW_ENABLE_ID" : "GON_GLOW_ENABLE_ID",
                                             0.0f) > 0.5f;
    if (glowEnabled && SharedResources::glowShadowEffectsEnabled())
    {
        const float point = juce::jmax (1.0f, lineWidth * (highQuality ? 0.85f : 0.7f));
        buildGlowPath (lastGlowPath, point);
    }
    else
    {
        lastGlowPath.clear();
    }
}

void GoniometerComponent::timerCallback()
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    updateCorrelationFromRing();
    fadeAndPlotTrail();
    repaint();
}

void GoniometerComponent::resized()
{
    ensureTrailImage (getPlotBounds().getWidth());
}

void GoniometerComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onShowContextMenu != nullptr)
        onShowContextMenu();
}

void GoniometerComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick != nullptr)
        onDoubleClick();
}

void GoniometerComponent::paint (juce::Graphics& g)
{
    const auto& theme = colors();
    auto plot = getPlotBounds().toFloat();
    auto corr = getCorrelationBounds().toFloat();
    if (plot.isEmpty())
        return;

    const bool glowEnabled = loadFloatParam (expanded ? "GON_EXPANDED_GLOW_ENABLE_ID" : "GON_GLOW_ENABLE_ID", 0.0f) > 0.5f;
    const float glowOpacity = juce::jlimit (0.0f, 1.0f,
                                            loadFloatParam (expanded ? "GON_EXPANDED_GLOW_OPACITY_ID" : "GON_GLOW_OPACITY_ID", 75.0f) * 0.01f);
    const float glowRadius = juce::jmax (0.0f,
                                         loadFloatParam (expanded ? "GON_EXPANDED_GLOW_RADIUS_ID" : "GON_GLOW_RADIUS_ID",
                                                         expanded ? 18.0f : 6.0f));
    const float glowSpread = juce::jmax (0.0f,
                                         loadFloatParam (expanded ? "GON_EXPANDED_GLOW_SPREAD_ID" : "GON_GLOW_SPREAD_ID",
                                                         expanded ? 2.0f : 1.0f));

    g.setColour (theme.oscBackground.withAlpha (expanded ? 90.0f / 255.0f : 210.0f / 255.0f));
    g.fillRoundedRectangle (plot, expanded ? 2.0f : 3.0f);
    g.setColour (theme.oscBackground2.withAlpha (180.0f / 255.0f));
    g.drawRoundedRectangle (plot.reduced (0.5f), expanded ? 2.0f : 3.0f, 1.0f);

    {
        const auto c = plot.getCentre();
        g.setColour (theme.oscBackground2.withAlpha (90.0f / 255.0f));
        g.drawLine (c.x, plot.getY() + 3.0f, c.x, plot.getBottom() - 3.0f, 1.0f);
        g.drawLine (plot.getX() + 3.0f, c.y, plot.getRight() - 3.0f, c.y, 1.0f);
        g.setColour (theme.oscBackground2.withAlpha (55.0f / 255.0f));
        g.drawLine (plot.getX() + 4.0f, plot.getBottom() - 4.0f, plot.getRight() - 4.0f, plot.getY() + 4.0f, 1.0f);
        g.drawLine (plot.getX() + 4.0f, plot.getY() + 4.0f, plot.getRight() - 4.0f, plot.getBottom() - 4.0f, 1.0f);
    }

    if (glowEnabled && SharedResources::glowShadowEffectsEnabled()
        && glowOpacity > 0.05f && glowRadius > 0.5f && ! lastGlowPath.isEmpty())
    {
        // Filled point cloud — Melatonin only expands/blurs fills (spread is a no-op on strokes).
        const auto bloom = theme.oscGlow.withAlpha (glowOpacity * 0.50f);
        const auto core = theme.oscGlow.brighter (0.15f).withAlpha (glowOpacity * 0.80f);

        plotGlow.setRadius ((double) glowRadius, 0);
        plotGlow.setSpread ((double) glowSpread, 0);
        plotGlow.setOffset (0, 0, 0);
        plotGlow.setColor (bloom, 0);

        plotGlow.setRadius (juce::jmax (2.0, (double) glowRadius * 0.35), 1);
        plotGlow.setSpread (0.0, 1);
        plotGlow.setOffset (0, 0, 1);
        plotGlow.setColor (core, 1);

        // Compact High-quality: allow Melatonin cache; expanded always refreshes.
        const bool forceLowQuality = expanded || ! isHighQuality();
        plotGlow.render (g, lastGlowPath, forceLowQuality);
    }

    if (trailImage.isValid())
        g.drawImage (trailImage, plot);

    {
        const float fontH = expanded ? 11.0f : 8.0f;
        g.setFont (juce::FontOptions().withHeight (fontH));
        g.setColour (theme.graphAxisText.withAlpha (0.55f));
        g.drawText ("M",
                    juce::Rectangle<float> (plot.getCentreX() - 8.0f, plot.getY() + 2.0f, 16.0f, fontH + 2.0f),
                    juce::Justification::centred, false);
        g.drawText ("L",
                    juce::Rectangle<float> (plot.getX() + 2.0f, plot.getBottom() - fontH - 4.0f, 12.0f, fontH + 2.0f),
                    juce::Justification::centred, false);
        g.drawText ("R",
                    juce::Rectangle<float> (plot.getRight() - 14.0f, plot.getBottom() - fontH - 4.0f, 12.0f, fontH + 2.0f),
                    juce::Justification::centred, false);
    }

    if (! corr.isEmpty())
    {
        g.setColour (theme.oscBackground.withAlpha (expanded ? 160.0f / 255.0f : 210.0f / 255.0f));
        g.fillRoundedRectangle (corr, 2.0f);
        g.setColour (theme.oscBackground2.withAlpha (200.0f / 255.0f));
        g.drawRoundedRectangle (corr.reduced (0.5f), 2.0f, 1.0f);

        const float midY = corr.getCentreY();
        g.setColour (theme.oscBackground2.withAlpha (120.0f / 255.0f));
        g.drawLine (corr.getX() + 2.0f, midY, corr.getRight() - 2.0f, midY, 1.0f);

        juce::ColourGradient guide (theme.gonCorrPositive.withAlpha (40.0f / 255.0f), corr.getCentreX(), corr.getY(),
                                    theme.gonCorrNegative.withAlpha (40.0f / 255.0f), corr.getCentreX(), corr.getBottom(), false);
        guide.addColour (0.5, theme.oscLine.withAlpha (25.0f / 255.0f));
        g.setGradientFill (guide);
        g.fillRoundedRectangle (corr.reduced (2.0f), 1.5f);

        const float y = juce::jmap (correlationDisplay, -1.0f, 1.0f, corr.getBottom() - 3.0f, corr.getY() + 3.0f);
        const float needleH = expanded ? 4.0f : 3.0f;
        auto needle = juce::Rectangle<float> (corr.getX() + 1.5f, y - needleH * 0.5f,
                                             corr.getWidth() - 3.0f, needleH);
        const auto needleColour = correlationDisplay >= 0.0f
                                      ? theme.gonCorrPositive.withAlpha (230.0f / 255.0f)
                                      : theme.gonCorrNegative.withAlpha (230.0f / 255.0f);
        g.setColour (needleColour);
        g.fillRoundedRectangle (needle, 1.0f);

        if (expanded)
        {
            g.setFont (juce::FontOptions().withHeight (10.0f));
            g.setColour (theme.graphAxisText.withAlpha (0.75f));
            g.drawText ("+1", corr.withHeight (12.0f).translated (0.0f, 2.0f),
                        juce::Justification::centred, false);
            g.drawText ("0",
                        juce::Rectangle<float> (corr.getX(), midY - 6.0f, corr.getWidth(), 12.0f),
                        juce::Justification::centred, false);
            g.drawText ("-1",
                        juce::Rectangle<float> (corr.getX(), corr.getBottom() - 14.0f, corr.getWidth(), 12.0f),
                        juce::Justification::centred, false);
        }
    }
}
