#include "HistogramComponent.h"
#include <algorithm>
#include <cmath>

namespace
{
    float cubicHermite (float y0, float y1, float y2, float y3, float t) noexcept
    {
        const float c0 = y1;
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * t + c2) * t + c1) * t + c0;
    }

    float measureTruePeakDb (const float* data, int numSamples, float hist[3]) noexcept
    {
        constexpr float silenceDb = -100.0f;
        if (data == nullptr || numSamples <= 0 || hist == nullptr)
            return silenceDb;

        float peak = 0.0f;
        float y0 = hist[0], y1 = hist[1], y2 = hist[2];

        for (int i = 0; i < numSamples; ++i)
        {
            const float y3 = data[i];
            peak = juce::jmax (peak, std::abs (y2));
            peak = juce::jmax (peak, std::abs (y3));
            for (float t = 0.25f; t < 1.0f; t += 0.25f)
                peak = juce::jmax (peak, std::abs (cubicHermite (y0, y1, y2, y3, t)));
            y0 = y1;
            y1 = y2;
            y2 = y3;
        }

        hist[0] = y0;
        hist[1] = y1;
        hist[2] = y2;
        return juce::Decibels::gainToDecibels (peak, silenceDb);
    }

    float msToLufs (double meanSquare) noexcept
    {
        if (meanSquare <= 1.0e-12)
            return -70.0f;
        return (float) (-0.691 + 10.0 * std::log10 (meanSquare));
    }
}

HistogramComponent::HistogramComponent()
{
    setOpaque (false);
    startTimerHz (30);
}

HistogramComponent::~HistogramComponent()
{
    stopTimer();
}

void HistogramComponent::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    updateKCoeffs (sampleRate);

    const juce::ScopedLock sl (historyLock);
    integSum = 0.0;
    integCount = 0;
    rmsMeanSquare = 0.0;
    columnTpMax = -100.0f;
    z1aL = z2aL = z1bL = z2bL = 0;
    z1aR = z2aR = z1bR = z2bR = 0;
    tpHistL[0] = tpHistL[1] = tpHistL[2] = 0;
    tpHistR[0] = tpHistR[1] = tpHistR[2] = 0;
    sampleAccum = 0.0;
    std::fill (histLufs.begin(), histLufs.end(), -70.0f);
    std::fill (histRms.begin(), histRms.end(), -100.0f);
    std::fill (histTruePeak.begin(), histTruePeak.end(), -100.0f);
    tpOverMarkers.clear();
    integratedLufs.store (-70.0f, std::memory_order_relaxed);
    averageRmsDb.store (-100.0f, std::memory_order_relaxed);
    columnTruePeakDb.store (-100.0f, std::memory_order_relaxed);
}

void HistogramComponent::resetIntegrated() noexcept
{
    const juce::ScopedLock sl (historyLock);
    integSum = 0.0;
    integCount = 0;
    integratedLufs.store (-70.0f, std::memory_order_relaxed);
}

void HistogramComponent::updateKCoeffs (double sr)
{
    const double f = sr;
    auto bilinearShelf = [&] (double f0, double Gdb, float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double A = std::pow (10.0, Gdb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / f;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / 2.0 * std::sqrt (2.0);
        const double b0d =    A * ((A + 1) + (A - 1) * cosw + 2 * std::sqrt (A) * alpha);
        const double b1d = -2 * A * ((A - 1) + (A + 1) * cosw);
        const double b2d =    A * ((A + 1) + (A - 1) * cosw - 2 * std::sqrt (A) * alpha);
        const double a0d =        (A + 1) - (A - 1) * cosw + 2 * std::sqrt (A) * alpha;
        const double a1d =  2 * ((A - 1) - (A + 1) * cosw);
        const double a2d =        (A + 1) - (A - 1) * cosw - 2 * std::sqrt (A) * alpha;
        b0 = (float) (b0d / a0d); b1 = (float) (b1d / a0d); b2 = (float) (b2d / a0d);
        a1 = (float) (a1d / a0d); a2 = (float) (a2d / a0d);
    };

    auto bilinearHp = [&] (double f0, float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / f;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / 2.0 * std::sqrt (2.0);
        const double b0d =  (1 + cosw) / 2;
        const double b1d = -(1 + cosw);
        const double b2d =  (1 + cosw) / 2;
        const double a0d =   1 + alpha;
        const double a1d =  -2 * cosw;
        const double a2d =   1 - alpha;
        b0 = (float) (b0d / a0d); b1 = (float) (b1d / a0d); b2 = (float) (b2d / a0d);
        a1 = (float) (a1d / a0d); a2 = (float) (a2d / a0d);
    };

    bilinearShelf (1681.974450955533, 3.999843853973347, b0a, b1a, b2a, a1a, a2a);
    bilinearHp (38.13547087602444, b0b, b1b, b2b, a1b, a2b);
}

float HistogramComponent::processKWeight (float x, float& z1a, float& z2a, float& z1b, float& z2b) const noexcept
{
    const float v1 = x - a1a * z1a - a2a * z2a;
    const float yShelf = b0a * v1 + b1a * z1a + b2a * z2a;
    z2a = z1a;
    z1a = v1;

    const float v2 = yShelf - a1b * z1b - a2b * z2b;
    const float y = b0b * v2 + b1b * z1b + b2b * z2b;
    z2b = z1b;
    z1b = v2;
    return y;
}

void HistogramComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

void HistogramComponent::setColourRamp (const GradientRamp& ramp)
{
    colourRamp = ramp;
    hasCustomRamp = true;
    repaint();
}

void HistogramComponent::clearColourRamp()
{
    hasCustomRamp = false;
    repaint();
}

float HistogramComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree != nullptr)
        if (auto* raw = valueTree->getRawParameterValue (id))
            return raw->load();
    return fallback;
}

bool HistogramComponent::loadBoolParam (const char* id, bool fallback) const
{
    return loadFloatParam (id, fallback ? 1.0f : 0.0f) > 0.5f;
}

void HistogramComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || numSamples <= 0)
        return;

    const float* l = left != nullptr ? left : right;
    const float* r = right != nullptr ? right : left;
    if (l == nullptr)
        return;

    const juce::ScopedTryLock sl (historyLock);
    if (! sl.isLocked())
        return;

    double blockSumSq = 0.0;
    for (int i = 0; i < numSamples; ++i)
    {
        const float yl = processKWeight (l[i], z1aL, z2aL, z1bL, z2bL);
        const float yr = processKWeight (r[i], z1aR, z2aR, z1bR, z2bR);
        const double ms = 0.5 * ((double) yl * (double) yl + (double) yr * (double) yr);

        // Integrated LUFS — program average (same approach as Loudness meter).
        integSum += ms;
        ++integCount;

        const float s = 0.5f * (l[i] + r[i]);
        blockSumSq += (double) s * (double) s;
    }

    if ((integCount & 2047) == 0 && integCount > 0)
        integratedLufs.store (msToLufs (integSum / (double) integCount), std::memory_order_relaxed);

    // Long-window RMS (~3 s EMA) — averages, not block transients.
    const double blockMs = blockSumSq / (double) numSamples;
    const double alpha = 1.0 - std::exp (-(double) numSamples / (rmsTauSec * sampleRate));
    rmsMeanSquare += alpha * (blockMs - rmsMeanSquare);
    averageRmsDb.store (juce::Decibels::gainToDecibels ((float) std::sqrt (juce::jmax (0.0, rmsMeanSquare)), -100.0f),
                        std::memory_order_relaxed);

    // True Peak: keep column max until the UI samples a history point.
    const float tpL = measureTruePeakDb (l, numSamples, tpHistL);
    const float tpR = measureTruePeakDb (r, numSamples, tpHistR);
    columnTpMax = juce::jmax (columnTpMax, juce::jmax (tpL, tpR));
    columnTruePeakDb.store (columnTpMax, std::memory_order_relaxed);
}

void HistogramComponent::ensureHistorySize (int width)
{
    const int n = juce::jmax (32, width);
    if ((int) histLufs.size() == n)
        return;

    histLufs.assign ((size_t) n, -70.0f);
    histRms.assign ((size_t) n, -100.0f);
    histTruePeak.assign ((size_t) n, -100.0f);
    tpOverMarkers.clear();
}

void HistogramComponent::appendHistory (float lufs, float rmsDb, float truePeakDb)
{
    const juce::ScopedLock sl (historyLock);
    if (histLufs.empty())
        return;

    const size_t n = histLufs.size();
    if (n > 1)
    {
        std::move (histLufs.begin() + 1, histLufs.end(), histLufs.begin());
        std::move (histRms.begin() + 1, histRms.end(), histRms.begin());
        std::move (histTruePeak.begin() + 1, histTruePeak.end(), histTruePeak.begin());
    }
    histLufs.back() = lufs;
    histRms.back() = rmsDb;
    histTruePeak.back() = truePeakDb;

    // Scroll existing TP-over markers with history; drop ones that fall off the left.
    for (auto& m : tpOverMarkers)
        --m.column;
    tpOverMarkers.erase (std::remove_if (tpOverMarkers.begin(), tpOverMarkers.end(),
                                         [] (const TpOverMarker& m) { return m.column < 0; }),
                         tpOverMarkers.end());

    if (truePeakDb > 0.0f)
        tpOverMarkers.push_back ({ (int) n - 1, truePeakDb });
}

void HistogramComponent::timerCallback()
{
    if (! enabled.load (std::memory_order_relaxed) || ! isVisible())
        return;

    if (loadBoolParam ("HISTOGRAM_FREEZE_ID", false))
    {
        repaint();
        return;
    }

    const float speed = juce::jlimit (1.0f, 100.0f, loadFloatParam ("HISTOGRAM_SPEED_ID", 35.0f));
    // Slower default scroll so averages read over a longer window (Youlean-style).
    const double colsPerSec = juce::jmap ((double) speed, 1.0, 100.0, 1.5, 24.0);
    sampleAccum += colsPerSec / 30.0;

    const float lufs = integratedLufs.load (std::memory_order_relaxed);
    const float rms = averageRmsDb.load (std::memory_order_relaxed);

    while (sampleAccum >= 1.0)
    {
        sampleAccum -= 1.0;
        const float tp = columnTruePeakDb.load (std::memory_order_relaxed);
        appendHistory (lufs, rms, tp);

        // Start a new column's true-peak max after sampling.
        {
            const juce::ScopedLock sl (historyLock);
            columnTpMax = -100.0f;
            columnTruePeakDb.store (-100.0f, std::memory_order_relaxed);
        }
    }

    repaint();
}

float HistogramComponent::levelToY (float levelDb, juce::Rectangle<float> plot) const noexcept
{
    const float minDb = loadFloatParam ("HISTOGRAM_MIN_DB_ID", -60.0f);
    const float maxDb = loadFloatParam ("HISTOGRAM_MAX_DB_ID", 0.0f);
    const float lo = juce::jmin (minDb, maxDb);
    const float hi = juce::jmax (minDb, maxDb);
    const float t = juce::jlimit (0.0f, 1.0f, (levelDb - lo) / juce::jmax (0.001f, hi - lo));
    return plot.getBottom() - t * plot.getHeight();
}

juce::Colour HistogramComponent::seriesColour (int series, float levelNorm) const
{
    // 0 = Integrated LUFS, 1 = RMS, 2 = True Peak
    juce::Colour base;
    switch (series)
    {
        case 0:  base = juce::Colour::fromRGB (240, 220, 120); break; // Int. LUFS gold
        case 1:  base = juce::Colour::fromRGB (120, 200, 255); break; // RMS cyan
        default: base = juce::Colour::fromRGB (230, 60, 55); break;   // True Peak red
    }

    const bool useRamp = loadBoolParam ("HISTOGRAM_USE_RAMP_ID", false)
                         && hasCustomRamp && colourRamp.isUsable();
    if (! useRamp)
        return base;

    return colourRamp.colourForDriver (levelNorm).interpolatedWith (base, 0.4f);
}

void HistogramComponent::paintSeries (juce::Graphics& g,
                                      juce::Rectangle<float> plot,
                                      const std::vector<float>& history,
                                      juce::Colour lineCol,
                                      float lineWidth,
                                      float fillOpacity,
                                      bool glowEnabled,
                                      float glowOpacity,
                                      float glowRadius,
                                      float glowSpread) const
{
    if (history.size() < 2 || plot.getWidth() < 2.0f)
        return;

    juce::Path line, fill;
    const float dx = plot.getWidth() / (float) (history.size() - 1);
    bool started = false;

    for (size_t i = 0; i < history.size(); ++i)
    {
        const float x = plot.getX() + dx * (float) i;
        const float y = levelToY (history[i], plot);
        if (! started)
        {
            line.startNewSubPath (x, y);
            fill.startNewSubPath (x, plot.getBottom());
            fill.lineTo (x, y);
            started = true;
        }
        else
        {
            line.lineTo (x, y);
            fill.lineTo (x, y);
        }
    }

    fill.lineTo (plot.getRight(), plot.getBottom());
    fill.closeSubPath();

    if (fillOpacity > 0.01f)
    {
        g.setColour (lineCol.withMultipliedAlpha (fillOpacity));
        g.fillPath (fill);
    }

    if (glowEnabled && glowOpacity > 0.01f && glowRadius > 0.1f)
    {
        const int layers = juce::jlimit (1, 6, (int) std::ceil (glowRadius / juce::jmax (0.5f, glowSpread)));
        for (int i = layers; i >= 1; --i)
        {
            const float t = (float) i / (float) layers;
            const float w = lineWidth + glowRadius * t;
            const float a = glowOpacity * (1.0f - t) * 0.35f;
            g.setColour (lineCol.withMultipliedAlpha (a));
            g.strokePath (line, juce::PathStrokeType (w, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }
    }

    g.setColour (lineCol);
    g.strokePath (line, juce::PathStrokeType (lineWidth, juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}

void HistogramComponent::paintScale (juce::Graphics& g, juce::Rectangle<float> area, juce::Colour textCol) const
{
    const float minDb = loadFloatParam ("HISTOGRAM_MIN_DB_ID", -60.0f);
    const float maxDb = loadFloatParam ("HISTOGRAM_MAX_DB_ID", 0.0f);
    const float lo = juce::jmin (minDb, maxDb);
    const float hi = juce::jmax (minDb, maxDb);

    static constexpr float candidates[] = { 6.0f, 0.0f, -6.0f, -12.0f, -18.0f, -24.0f,
                                            -30.0f, -36.0f, -48.0f, -60.0f, -70.0f };
    const float fontH = juce::jlimit (7.0f, 9.0f, area.getWidth() * 0.45f);
    g.setFont (juce::Font (juce::FontOptions (fontH)));
    g.setColour (textCol.withAlpha (0.55f));

    for (float tick : candidates)
    {
        if (tick < lo - 0.01f || tick > hi + 0.01f)
            continue;
        const float t = (tick - lo) / juce::jmax (0.001f, hi - lo);
        const float y = area.getBottom() - t * area.getHeight();
        const auto label = tick > 0.0f ? ("+" + juce::String ((int) tick))
                                       : juce::String ((int) tick);
        g.drawText (label,
                    juce::Rectangle<float> (area.getX(), y - fontH * 0.5f, area.getWidth(), fontH),
                    juce::Justification::centred, false);
    }
}

void HistogramComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto bg = themeColors != nullptr ? themeColors->sharedColors.oscBackground
                                           : juce::Colour::fromRGB (16, 14, 12);
    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 3.0f);

    const auto readoutText = themeColors != nullptr ? themeColors->sharedColors.meterReadoutText
                                                    : juce::Colours::whitesmoke.withAlpha (0.92f);
    const auto textCol = readoutText;

    auto area = getLocalBounds().reduced (4);
    const int titleH = 12;
    const int legendH = 14;
    const int scaleW = 26;
    area.removeFromTop (titleH);
    auto legend = area.removeFromBottom (legendH);
    auto scaleArea = area.removeFromRight (scaleW).toFloat();
    area.removeFromRight (3);
    auto plot = area.toFloat();

    std::vector<TpOverMarker> markersCopy;
    std::vector<float> lufsCopy, rmsCopy, tpCopy;
    {
        const juce::ScopedLock sl (historyLock);
        ensureHistorySize (juce::jmax (32, (int) plot.getWidth()));
        markersCopy = tpOverMarkers;
        lufsCopy = histLufs;
        rmsCopy = histRms;
        tpCopy = histTruePeak;
    }

    paintScale (g, scaleArea, textCol);

    g.setColour (textCol.withAlpha (0.12f));
    const float minDb = loadFloatParam ("HISTOGRAM_MIN_DB_ID", -60.0f);
    const float maxDb = loadFloatParam ("HISTOGRAM_MAX_DB_ID", 0.0f);
    for (float tick : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f })
    {
        if (tick < juce::jmin (minDb, maxDb) || tick > juce::jmax (minDb, maxDb))
            continue;
        const float y = levelToY (tick, plot);
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());
    }

    const float lineW = juce::jlimit (0.5f, 8.0f, loadFloatParam ("HISTOGRAM_LINE_WIDTH_ID", 2.0f));
    const float fillOp = juce::jlimit (0.0f, 1.0f, loadFloatParam ("HISTOGRAM_FILL_OPACITY_ID", 35.0f) * 0.01f);
    const bool glowOn = loadBoolParam ("HISTOGRAM_GLOW_ENABLE_ID", true);
    const float glowOp = juce::jlimit (0.0f, 1.0f, loadFloatParam ("HISTOGRAM_GLOW_OPACITY_ID", 70.0f) * 0.01f);
    const float glowR = juce::jmax (0.0f, loadFloatParam ("HISTOGRAM_GLOW_RADIUS_ID", 8.0f));
    const float glowS = juce::jmax (0.0f, loadFloatParam ("HISTOGRAM_GLOW_SPREAD_ID", 1.5f));

    const bool showLufs = loadBoolParam ("HISTOGRAM_SHOW_LUFS_ID", true);
    const bool showRms = loadBoolParam ("HISTOGRAM_SHOW_RMS_ID", true);
    const bool showTp = loadBoolParam ("HISTOGRAM_SHOW_TRUE_PEAK_ID", true);

    auto paintOne = [&] (int series, const std::vector<float>& hist,
                         float widthMul, float fillMul, bool glow)
    {
        if (hist.empty())
            return;
        const float last = hist.back();
        const float minV = juce::jmin (minDb, maxDb);
        const float maxV = juce::jmax (minDb, maxDb);
        const float norm = juce::jlimit (0.0f, 1.0f, (last - minV) / juce::jmax (0.001f, maxV - minV));
        paintSeries (g, plot, hist, seriesColour (series, norm),
                     lineW * widthMul, fillOp * fillMul, glow, glowOp, glowR, glowS);
    };

    // True Peak behind (lighter); averages (RMS / Int. LUFS) in front — Youlean-style priority.
    if (showTp)   paintOne (2, tpCopy, 0.85f, 0.45f, false);
    if (showRms)  paintOne (1, rmsCopy, 1.0f, 1.0f, glowOn);
    if (showLufs) paintOne (0, lufsCopy, 1.05f, 1.0f, glowOn);

    // Red vertical lines where True Peak exceeded 0 dBFS; label shows overshoot (+2.4).
    if (! markersCopy.empty() && lufsCopy.size() > 1)
    {
        const int n = (int) lufsCopy.size();
        const float dx = plot.getWidth() / (float) (n - 1);
        const auto lineCol = themeColors != nullptr ? themeColors->sharedColors.meterClip
                                                    : juce::Colour::fromRGB (220, 40, 40);
        const float labelH = 11.0f;
        auto labelFont = juce::Font (juce::FontOptions (labelH).withName ("Lato Black"));
        g.setFont (labelFont);

        for (const auto& m : markersCopy)
        {
            if (m.column < 0 || m.column >= n)
                continue;

            const float x = plot.getX() + dx * (float) m.column;
            g.setColour (lineCol.withAlpha (0.9f));
            g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.5f);

            const auto label = "+" + juce::String (m.overDb, 1);
            const float labelW = juce::jmax (28.0f,
                juce::GlyphArrangement::getStringWidth (labelFont, label) + 4.0f);
            auto labelArea = juce::Rectangle<float> (x - labelW * 0.5f, plot.getY() + 1.0f, labelW, labelH + 2.0f);
            labelArea = labelArea.constrainedWithin (plot);
            g.setColour (readoutText);
            g.drawText (label, labelArea, juce::Justification::centred, false);
        }
    }

    // Curve key: left-justified, tight packing (no even spacing / trailing gap).
    auto legFont = juce::Font (juce::FontOptions (9.0f));
    g.setFont (legFont);
    auto leg = legend;
    auto drawLeg = [&] (const juce::String& name, juce::Colour c, bool on)
    {
        if (! on)
            return;
        const float swatch = 8.0f;
        const float gap = 3.0f;
        const float textW = juce::GlyphArrangement::getStringWidth (legFont, name);
        const int chipW = juce::jmax (1, juce::roundToInt (swatch + gap + textW + 2.0f));
        auto chip = leg.removeFromLeft (chipW);
        auto sw = chip.removeFromLeft ((int) swatch).toFloat()
                      .withSizeKeepingCentre (swatch, swatch);
        g.setColour (c.withAlpha (0.9f));
        g.fillRoundedRectangle (sw, 1.5f);
        chip.removeFromLeft ((int) gap);
        g.setColour (readoutText.withAlpha (0.85f));
        g.drawText (name, chip, juce::Justification::centredLeft, false);
        leg.removeFromLeft (4); // small gap before next key only
    };

    drawLeg ("Int. LUFS", seriesColour (0, 0.7f), showLufs);
    drawLeg ("RMS", seriesColour (1, 0.7f), showRms);
    drawLeg ("True Peak", seriesColour (2, 0.7f), showTp);

    g.setFont (juce::Font (juce::FontOptions (7.0f)));
    g.setColour (readoutText.withAlpha (0.55f));
    g.drawText ("dB", scaleArea.getSmallestIntegerContainer().withY (legend.getY()).withHeight (legendH),
                juce::Justification::centred, false);
}

void HistogramComponent::resized()
{
    const juce::ScopedLock sl (historyLock);
    ensureHistorySize (juce::jmax (32, getWidth() - 40));
}

void HistogramComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void HistogramComponent::showContextMenu()
{
    if (onShowContextMenu != nullptr)
    {
        onShowContextMenu();
        return;
    }

    juce::PopupMenu menu;
    menu.addItem (1, "Freeze", true, loadBoolParam ("HISTOGRAM_FREEZE_ID", false));
    menu.addItem (2, "Reset Integrated");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<HistogramComponent> (this)] (int r)
                        {
                            if (safe == nullptr || r <= 0)
                                return;
                            if (r == 2)
                            {
                                safe->resetIntegrated();
                                return;
                            }
                            if (r == 1 && safe->valueTree != nullptr)
                                if (auto* p = safe->valueTree->getParameter ("HISTOGRAM_FREEZE_ID"))
                                    p->setValueNotifyingHost (p->getValue() < 0.5f ? 1.0f : 0.0f);
                        });
}
