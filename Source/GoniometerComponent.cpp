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

const SharedColors& GoniometerComponent::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void GoniometerComponent::prepare (double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    sampleRateHz.store (sr, std::memory_order_relaxed);

    const int newCap = juce::jmax (1024, (int) std::ceil (sr * (double) kMaxBufferSeconds));
    ringL.assign ((size_t) newCap, 0.0f);
    ringR.assign ((size_t) newCap, 0.0f);
    writePos.store (0, std::memory_order_relaxed);
    capacity.store (newCap, std::memory_order_release);
    resetDisplay();
}

void GoniometerComponent::resetDisplay()
{
    ringReadPos = writePos.load (std::memory_order_acquire);
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
        startTimerHz (30);
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
        return true;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (valueTree->getParameter ("GON_QUALITY_ID")))
        return choice->getIndex() >= 1;

    return true;
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

    if (available > kMaxPlotSamplesPerTick)
    {
        ringReadPos = (w - kMaxPlotSamplesPerTick + cap) % cap;
        available = kMaxPlotSamplesPerTick;
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

void GoniometerComponent::buildGlowPath (juce::Path& outPath) const
{
    outPath.clear();

    auto plot = getPlotBounds().toFloat();
    if (plot.isEmpty())
        return;

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0)
        return;

    const bool highQuality = isHighQuality();
    const int n = juce::jmin (highQuality ? kMaxPlotSamplesPerTick : kMaxPlotSamplesPerTick / 3, cap);
    const int w = writePos.load (std::memory_order_acquire);
    int pos = (w - n + cap) % cap;

    const float cx = plot.getCentreX();
    const float cy = plot.getCentreY();
    const float scale = plot.getWidth() * 0.42f;
    const int step = highQuality ? 1 : 2;

    bool started = false;
    for (int i = 0; i < n; i += step)
    {
        const float L = ringL[(size_t) ((pos + i) % cap)];
        const float R = ringR[(size_t) ((pos + i) % cap)];
        const float mid = 0.70710678f * (L + R);
        const float side = 0.70710678f * (L - R);
        const float x = cx + side * scale;
        const float y = cy - mid * scale;

        if (! started)
        {
            outPath.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            outPath.lineTo (x, y);
        }
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

    // Lighter fade than before so the trail stays as bright as the oscilloscope.
    juce::Graphics tg (trailImage);
    tg.setColour (juce::Colours::black.withAlpha (highQuality ? 0.10f : 0.14f));
    tg.fillAll();
    tg.setOpacity (1.0f);

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0)
        return;

    const int w = writePos.load (std::memory_order_acquire);
    const int n = juce::jmin (highQuality ? kMaxPlotSamplesPerTick : kMaxPlotSamplesPerTick / 2, cap);
    int pos = (w - n + cap) % cap;

    const float size = (float) trailImage.getWidth();
    const float cx = size * 0.5f;
    const float cy = size * 0.5f;
    const float scale = size * 0.42f;
    const float point = juce::jmax (1.0f, lineWidth * (highQuality ? 0.85f : 0.7f));

    const auto& theme = colors();
    const auto ink = theme.oscLine.withMultipliedAlpha (lineOpacity);
    tg.setColour (ink);

    const int step = highQuality ? 1 : 2;
    for (int i = 0; i < n; i += step)
    {
        const float L = ringL[(size_t) pos];
        const float R = ringR[(size_t) pos];
        const float mid = 0.70710678f * (L + R);
        const float side = 0.70710678f * (L - R);
        const float x = cx + side * scale;
        const float y = cy - mid * scale;
        tg.fillEllipse (x - point * 0.5f, y - point * 0.5f, point, point);
        pos = (pos + step) % cap;
    }

    buildGlowPath (lastGlowPath);
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

void GoniometerComponent::paint (juce::Graphics& g)
{
    const auto& theme = colors();
    auto plot = getPlotBounds().toFloat();
    auto corr = getCorrelationBounds().toFloat();
    if (plot.isEmpty())
        return;

    const float lineOpacity = juce::jlimit (0.0f, 1.0f, loadFloatParam ("OSC_LINE_OPACITY_ID", 100.0f) * 0.01f);
    const float lineWidth = juce::jmax (0.5f,
                                        loadFloatParam (expanded ? "GON_EXPANDED_LINE_WIDTH_ID" : "GON_LINE_WIDTH_ID",
                                                        expanded ? 2.6f : 1.6f));
    const bool glowEnabled = loadFloatParam (expanded ? "GON_EXPANDED_GLOW_ENABLE_ID" : "GON_GLOW_ENABLE_ID", 1.0f) > 0.5f;
    const float glowOpacity = juce::jlimit (0.0f, 1.0f,
                                            loadFloatParam (expanded ? "GON_EXPANDED_GLOW_OPACITY_ID" : "GON_GLOW_OPACITY_ID", 75.0f) * 0.01f);
    const float glowRadius = juce::jmax (0.0f,
                                         loadFloatParam (expanded ? "GON_EXPANDED_GLOW_RADIUS_ID" : "GON_GLOW_RADIUS_ID",
                                                         expanded ? 18.0f : 6.0f));
    const float glowSpread = juce::jmax (0.0f,
                                         loadFloatParam (expanded ? "GON_EXPANDED_GLOW_SPREAD_ID" : "GON_GLOW_SPREAD_ID",
                                                         expanded ? 2.0f : 1.0f));
    const bool highQuality = isHighQuality();

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
        const auto bloom = theme.oscGlow.withAlpha (glowOpacity * 0.45f);
        const auto core = theme.oscGlow.brighter (0.15f).withAlpha (glowOpacity * 0.75f);
        const juce::PathStrokeType glowStroke (juce::jmax (1.0f, lineWidth + 0.5f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded);
        juce::Path glowShape;
        glowStroke.createStrokedPath (glowShape, lastGlowPath);

        plotGlow.setRadius ((double) glowRadius, 0);
        plotGlow.setSpread ((double) glowSpread, 0);
        plotGlow.setOffset (0, 0, 0);
        plotGlow.setColor (bloom, 0);

        plotGlow.setRadius (juce::jmax (2.0, (double) glowRadius * 0.35), 1);
        plotGlow.setSpread (0.0, 1);
        plotGlow.setOffset (0, 0, 1);
        plotGlow.setColor (core, 1);

        plotGlow.render (g, glowShape, expanded || ! highQuality);
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

    juce::ignoreUnused (lineOpacity);

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
