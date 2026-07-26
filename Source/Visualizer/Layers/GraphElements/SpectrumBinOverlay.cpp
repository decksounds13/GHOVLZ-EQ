#include "SpectrumBinOverlay.h"

SpectrumBinOverlay::SpectrumBinOverlay()
    : glowShadow ({
          { juce::Colour::fromRGBA (255, 35, 110, 160), 28, { 0, 0 }, 8 },
          { juce::Colour::fromRGBA (255, 90, 40, 200), 10, { 0, 0 }, 3 }
      })
{
    // Path changes every audio frame — skip melatonin path-equality cache work.
    glowShadow.setBypassCache (true);
}

SpectrumBinOverlay::Settings SpectrumBinOverlay::readSettings (juce::AudioProcessorValueTreeState* valueTree) const
{
    Settings s;

    if (valueTree == nullptr)
        return s;

    auto load = [valueTree] (const char* id, float fallback) -> float
    {
        if (auto* p = valueTree->getRawParameterValue (id))
            return p->load();
        return fallback;
    };

    s.opacity = juce::jlimit (0.0f, 1.0f, load ("FFT_OPACITY_ID", 66.6f) * 0.01f);
    // 10–150% of inter-bin / column spacing (can slightly overlap above 100%).
    s.barWidthRatio = juce::jlimit (0.10f, 1.50f, load ("FFT_BAR_WIDTH_ID", 112.9f) * 0.01f);
    s.intensity = juce::jlimit (0.25f, 3.0f, load ("FFT_INTENSITY_ID", 222.1f) * 0.01f);
    s.threshold = juce::jlimit (0.0f, 1.0f, load ("FFT_THRESHOLD_ID", 95.8f) * 0.01f);
    // Draw density: 0–100% (Show Bins toggle). Visibility is SPECTRUM_FFT_BINS_ID.
    s.resolution = juce::jlimit (0.0f, 1.0f, load ("FFT_RESOLUTION_ID", 32.9f) * 0.01f);
    s.glowEnabled = load ("FFT_GLOW_ENABLE_ID", 1.0f) > 0.5f;
    s.glowRadius = juce::jlimit (0.0f, 250.0f, load ("FFT_GLOW_RADIUS_ID", 94.1f));
    s.glowSpread = juce::jlimit (0.0f, 250.0f, load ("FFT_GLOW_SPREAD_ID", 15.1f));
    s.glowOpacity = juce::jlimit (0.0f, 1.0f, load ("FFT_GLOW_OPACITY_ID", 85.5f) * 0.01f);
    s.glowOffsetX = juce::jlimit (-40.0f, 40.0f, load ("FFT_GLOW_OFFSET_X_ID", 0.0f));
    s.glowOffsetY = juce::jlimit (-40.0f, 40.0f, load ("FFT_GLOW_OFFSET_Y_ID", 0.0f));

    if (auto* p = valueTree->getRawParameterValue ("FFT_FULL_HEIGHT_ID"))
        s.fullHeight = p->load() > 0.5f;

    return s;
}

float SpectrumBinOverlay::normalizeBinIndex (int binIndex, Analyser& analyser, bool logarithmic) const
{
    return analyser.getBinDisplayXNorm (binIndex, logarithmic);
}

float SpectrumBinOverlay::shapePresence (float peak, float intensityScale, float threshold) const
{
    float x = juce::jlimit (0.0f, 1.0f, peak * intensityScale);
    const float gate = juce::jlimit (0.0f, 1.0f, threshold);

    // Soft floor: quiet energy below this is crushed toward zero.
    const float softFloor = gate * 0.55f;
    if (softFloor > 0.0001f)
    {
        if (x <= softFloor)
            x *= 0.12f * (1.0f - gate); // residual hush under the floor
        else
            x = (x - softFloor) / (1.0f - softFloor);
    }

    // Steeper gamma as threshold rises so fundamentals / strong harmonics pop.
    const float exponent = juce::jmap (gate, 0.0f, 1.0f, 0.55f, 2.6f);
    return std::pow (juce::jlimit (0.0f, 1.0f, x), exponent);
}

juce::Colour SpectrumBinOverlay::colourForIntensity (float peak, float opacity, float intensityScale, float threshold) const
{
    const float shaped = shapePresence (peak, intensityScale, threshold);

    const auto cool = juce::Colour::fromRGB (160, 20, 120);
    const auto warm = juce::Colour::fromRGB (255, 45, 95);
    const auto hot  = juce::Colour::fromRGB (255, 140, 40);

    juce::Colour colour;
    if (shaped < 0.45f)
        colour = cool.interpolatedWith (warm, shaped / 0.45f);
    else
        colour = warm.interpolatedWith (hot, (shaped - 0.45f) / 0.55f);

    // Quieter shaped values also lose alpha harder (not a linear fade).
    const float alpha = juce::jmap (shaped * shaped, 0.0f, 1.0f, 0.08f, 1.0f) * opacity;
    return colour.withMultipliedBrightness (0.85f + 0.45f * shaped).withAlpha (alpha);
}

float SpectrumBinOverlay::mapBarTopY (float peak, float intensity, bool fullHeight,
                                      float top, float bottom) noexcept
{
    juce::ignoreUnused (intensity);

    if (fullHeight)
        return top;

    // Match GraphLine 1:1 (peak on analyser 0..1 scale). Intensity is colour-only.
    const float heightNorm = juce::jlimit (0.0f, 1.0f, peak);
    return juce::jmap (heightNorm, 0.0f, 1.0f, bottom, top);
}

void SpectrumBinOverlay::paint (juce::Graphics& g,
                                Analyser& analyser,
                                juce::Rectangle<float> bounds,
                                bool logarithmic,
                                juce::AudioProcessorValueTreeState* valueTree)
{
    const int scopeSize = static_cast<int> (analyser.getScopeSize());
    if (scopeSize < 2 || bounds.isEmpty())
        return;

    const auto settings = readSettings (valueTree);
    if (settings.opacity <= 0.001f)
        return;

    // When Show Bins density is 0, still draw bars using the built-in default (~33%).
    // Visibility is gated by SPECTRUM_FFT_BINS_ID (Show Bars), not resolution.
    const float resolution = settings.resolution > 0.0f ? settings.resolution : 0.329f;

    const float width = bounds.getWidth();
    const float left = bounds.getX();
    const float top = bounds.getY();
    const float bottom = bounds.getBottom();

    const int lastBin = analyser.getHighestDisplayBinIndex();
    if (lastBin < 1)
        return;

    // Base cull floor rises with threshold so quiet bins aren't drawn at all.
    const float amplitudeFloor = juce::jmap (settings.intensity, 0.25f, 3.0f, 0.12f, 0.045f)
                               + settings.threshold * 0.22f;

    activeBins.clear();
    bars.clear();

    if (activeBins.capacity() < (size_t) lastBin)
        activeBins.reserve ((size_t) lastBin);

    for (int i = 1; i <= lastBin; ++i)
    {
        const float xNorm = normalizeBinIndex (i, analyser, logarithmic);
        if (xNorm < 0.0f || xNorm > 1.0f)
            continue;

        const float peak = analyser.getScopeData (static_cast<size_t> (i));
        if (peak < amplitudeFloor)
            continue;

        activeBins.push_back ({ xNorm, peak });
    }

    if (activeBins.empty())
        return;

    const int maxColumns = juce::jlimit (
        32,
        (int) width,
        (int) std::ceil (width * resolution));

    // Full per-bin bars only when they fit the column budget.
    // At 2048 (1024 bins) this can stay per-bin; larger FFTs always column-collapse.
    // GraphLine fill closing is unaffected — this is bars/glow only.
    const bool fullResolution = resolution >= 0.995f
                                && activeBins.size() <= (size_t) maxColumns
                                && scopeSize <= 2048;

    bars.reserve (fullResolution ? activeBins.size() : (size_t) maxColumns);

    if (fullResolution)
    {
        for (size_t n = 0; n < activeBins.size(); ++n)
        {
            const auto& sample = activeBins[n];
            const float x = left + sample.xNorm * width;

            float spacing;
            if (activeBins.size() == 1)
                spacing = juce::jmax (1.5f, width / static_cast<float> (lastBin));
            else if (n + 1 < activeBins.size())
                spacing = (activeBins[n + 1].xNorm - sample.xNorm) * width;
            else
                spacing = (sample.xNorm - activeBins[n - 1].xNorm) * width;

            spacing = juce::jmax (0.75f, spacing);

            // Width tracks spacing × ratio (no hard 6px cap — that made the slider useless).
            const float barWidth = juce::jmax (0.5f, spacing * settings.barWidthRatio);
            const float y = mapBarTopY (sample.peak, settings.intensity, settings.fullHeight, top, bottom);
            const float barHeight = bottom - y;
            if (barHeight < 0.75f)
                continue;

            bars.push_back ({ x - barWidth * 0.5f, y, barWidth, barHeight, sample.peak });
        }
    }
    else
    {
        glowColumnPeaks.assign ((size_t) maxColumns, 0.0f);
        for (const auto& sample : activeBins)
        {
            const int column = juce::jlimit (
                0, maxColumns - 1,
                (int) (sample.xNorm * (float) (maxColumns - 1)));
            glowColumnPeaks[(size_t) column] = juce::jmax (glowColumnPeaks[(size_t) column], sample.peak);
        }

        const float columnWidth = width / (float) maxColumns;
        const float barWidth = juce::jmax (0.5f, columnWidth * settings.barWidthRatio);

        for (int c = 0; c < maxColumns; ++c)
        {
            const float peak = glowColumnPeaks[(size_t) c];
            if (peak < amplitudeFloor)
                continue;

            const float x = left + (c + 0.5f) * columnWidth;
            const float y = mapBarTopY (peak, settings.intensity, settings.fullHeight, top, bottom);
            const float barHeight = bottom - y;
            if (barHeight < 0.75f)
                continue;

            bars.push_back ({ x - barWidth * 0.5f, y, barWidth, barHeight, peak });
        }
    }

    if (bars.empty())
        return;

    // Glow always uses a column silhouette (keeps Melatonin cheaper at high radius).
    // Soft-cap matches param max (250) so the slider is honest; column density still
    // eases off as radius grows to keep large blurs workable.
    if (settings.glowEnabled && settings.glowRadius > 0.5f && settings.glowOpacity > 0.01f)
    {
        const float glowRadius = juce::jmin (settings.glowRadius, 250.0f);
        const double columnScale = glowRadius > 80.0f ? 0.45
                                   : glowRadius > 28.0f ? 0.55
                                   : glowRadius > 16.0f ? 0.65
                                   : 1.0;
        const int glowColumns = juce::jlimit (
            24,
            maxColumns,
            (int) std::ceil ((double) maxColumns * columnScale));

        glowColumnPeaks.assign ((size_t) glowColumns, 0.0f);

        for (const auto& sample : activeBins)
        {
            const int column = juce::jlimit (
                0, glowColumns - 1,
                (int) (sample.xNorm * (float) (glowColumns - 1)));
            glowColumnPeaks[(size_t) column] = juce::jmax (glowColumnPeaks[(size_t) column], sample.peak);
        }

        juce::Path glowPath;
        const float columnWidth = width / (float) glowColumns;
        const float barWidth = juce::jmax (0.5f, columnWidth * settings.barWidthRatio);
        const float spread = juce::jmin (settings.glowSpread, 250.0f);

        for (int c = 0; c < glowColumns; ++c)
        {
            const float peak = glowColumnPeaks[(size_t) c];
            if (peak <= amplitudeFloor)
                continue;

            if (shapePresence (peak, settings.intensity, settings.threshold) < 0.04f)
                continue;

            const float x = left + (c + 0.5f) * columnWidth;
            const float y = mapBarTopY (peak, settings.intensity, settings.fullHeight, top, bottom);
            const float barHeight = bottom - y;
            if (barHeight < 0.75f)
                continue;

            glowPath.addRectangle (x - barWidth * 0.5f - spread,
                                   y - spread,
                                   barWidth + spread * 2.0f,
                                   barHeight + spread * 2.0f);
        }

        if (! glowPath.isEmpty())
        {
            const float glowAlpha = juce::jlimit (
                0.0f, 1.0f,
                settings.glowOpacity * settings.opacity * (0.65f + 0.35f * settings.intensity));

            const auto bloomColour = juce::Colour::fromRGB (255, 35, 110)
                                         .withMultipliedBrightness (1.05f + 0.25f * juce::jmin (1.0f, settings.intensity))
                                         .withAlpha (glowAlpha * 0.55f);

            const auto coreColour = juce::Colour::fromRGB (255, 90, 40)
                                        .withAlpha (glowAlpha * 0.85f);

            const int offsetX = juce::roundToInt (settings.glowOffsetX);
            const int offsetY = juce::roundToInt (settings.glowOffsetY);

            glowShadow.setRadius (glowRadius, 0);
            glowShadow.setSpread (0.0, 0);
            glowShadow.setOffset (offsetX, offsetY, 0);
            glowShadow.setColor (bloomColour, 0);

            glowShadow.setRadius (juce::jmax (2.0, (double) glowRadius * 0.35), 1);
            glowShadow.setSpread (0.0, 1);
            glowShadow.setOffset (offsetX, offsetY, 1);
            glowShadow.setColor (coreColour, 1);

            glowShadow.render (g, glowPath, true);
        }
    }

    for (const auto& bar : bars)
    {
        const float presence = shapePresence (bar.peak, settings.intensity, settings.threshold);
        if (presence < 0.02f)
            continue;

        const auto colour = colourForIntensity (bar.peak, settings.opacity, settings.intensity, settings.threshold);
        g.setColour (colour);
        g.fillRect (bar.x, bar.y, bar.w, bar.h);
    }
}
