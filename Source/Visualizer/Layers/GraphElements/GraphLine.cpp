#include "GraphLine.h"
#include "Menu/SharedResources.h"

namespace
{
    bool paramEnabled (juce::AudioProcessorValueTreeState* tree, const char* id, bool fallback = true)
    {
        if (tree == nullptr)
            return fallback;

        if (auto* p = tree->getRawParameterValue (id))
            return p->load() > 0.5f;

        return fallback;
    }

    // 240 used createPathWithRoundedCorners(128) on a single curve.
    // With pre/post/hold that cost triples — keep a light round for look without the lag.
    constexpr float kCornerRound = 12.0f;

    /** Display-space Gaussian blur along the x axis (helps LF plateaus look smooth). */
    void smoothColumns (std::vector<float>& columns, std::vector<float>& scratch, float radiusPx)
    {
        const int n = (int) columns.size();
        if (n < 3 || radiusPx < 0.5f)
            return;

        const float sigma = juce::jmax (0.5f, radiusPx * 0.5f);
        const int radius = juce::jlimit (1, 48, (int) std::ceil ((double) radiusPx));

        scratch.resize ((size_t) n);

        // 1-D Gaussian along columns; stack kernel (radius capped at 48).
        float kernel[97];
        float weightSum = 0.0f;
        const float invTwoSigmaSq = 1.0f / (2.0f * sigma * sigma);

        for (int k = -radius; k <= radius; ++k)
        {
            const float w = std::exp (-(float) (k * k) * invTwoSigmaSq);
            kernel[k + radius] = w;
            weightSum += w;
        }

        const float invWeight = 1.0f / juce::jmax (1.0e-6f, weightSum);

        for (int i = 0; i < n; ++i)
        {
            float acc = 0.0f;
            for (int k = -radius; k <= radius; ++k)
            {
                const int j = juce::jlimit (0, n - 1, i + k);
                acc += columns[(size_t) j] * kernel[k + radius];
            }
            scratch[(size_t) i] = acc * invWeight;
        }

        columns.swap (scratch);
    }

    /**
     * Standard analyzer reduction onto display columns:
     * - Multiple FFT bins in one pixel → peak (max) of those bins
     * - Pixel between sparse LF bins → interpolate in display-x between neighbouring bin centres
     *
     * This avoids denser log-space resampling of coarse magnitudes (which makes LF more jagged).
     */
    template <typename ReadBinFn>
    void reduceScopeToColumns (Analyser& analyser,
                               int lastBin,
                               bool logarithmic,
                               int numColumns,
                               float height,
                               ReadBinFn&& readBin,
                               std::vector<float>& outY,
                               std::vector<float>& peakScratch,
                               std::vector<char>& hasBinScratch,
                               std::vector<float>& pointX,
                               std::vector<float>& pointMag)
    {
        outY.assign ((size_t) numColumns, height);

        if (numColumns < 2 || lastBin < 1)
            return;

        pointX.clear();
        pointMag.clear();
        pointX.reserve ((size_t) lastBin);
        pointMag.reserve ((size_t) lastBin);

        for (int b = 1; b <= lastBin; ++b)
        {
            const float xNorm = analyser.getBinDisplayXNorm (b, logarithmic);
            if (xNorm < 0.0f || xNorm > 1.0f)
                continue;

            pointX.push_back (xNorm);
            pointMag.push_back (juce::jlimit (0.0f, 1.0f, readBin ((size_t) b)));
        }

        if (pointX.size() < 2)
            return;

        peakScratch.assign ((size_t) numColumns, -1.0f);
        hasBinScratch.assign ((size_t) numColumns, 0);

        for (size_t i = 0; i < pointX.size(); ++i)
        {
            const int col = juce::jlimit (0, numColumns - 1,
                                          (int) std::floor (pointX[i] * (float) numColumns));
            peakScratch[(size_t) col] = juce::jmax (peakScratch[(size_t) col], pointMag[i]);
            hasBinScratch[(size_t) col] = 1;
        }

        // Fill gaps (typical at LF on a log axis) by interpolating neighbouring bin centres in xNorm.
        size_t leftPt = 0;

        for (int c = 0; c < numColumns; ++c)
        {
            float mag;

            if (hasBinScratch[(size_t) c] != 0)
            {
                mag = peakScratch[(size_t) c];
            }
            else
            {
                const float xNorm = ((float) c + 0.5f) / (float) numColumns;

                while (leftPt + 1 < pointX.size() && pointX[leftPt + 1] < xNorm)
                    ++leftPt;

                const size_t rightPt = juce::jmin (pointX.size() - 1, leftPt + 1);
                const float ax = pointX[leftPt];
                const float bx = pointX[rightPt];
                const float span = bx - ax;

                if (span > 1.0e-6f)
                {
                    const float t = juce::jlimit (0.0f, 1.0f, (xNorm - ax) / span);
                    mag = pointMag[leftPt] + (pointMag[rightPt] - pointMag[leftPt]) * t;
                }
                else
                {
                    mag = pointMag[leftPt];
                }
            }

            outY[(size_t) c] = juce::jmap (mag, 0.0f, 1.0f, height, 0.0f);
        }
    }

    void appendColumnPath (juce::Path& path, const std::vector<float>& columnY, float width)
    {
        const int n = (int) columnY.size();
        if (n < 2)
            return;

        const float dx = width / (float) n;

        path.startNewSubPath (dx * 0.5f, columnY[0]);
        for (int i = 1; i < n; ++i)
            path.lineTo (dx * ((float) i + 0.5f), columnY[(size_t) i]);
    }
}

GraphLine::GraphLine (Analyser& analyser) : mr_analyser (analyser)
{
    // Path changes every analyser frame — skip melatonin path-equality cache work.
    m_curveGlow.setBypassCache (true);
}

GraphLine::~GraphLine() = default;

void GraphLine::paint (juce::Graphics& g)
{
    g.setColour (m_colour);
    drawFrame (g);
}

void GraphLine::setScaleType (const bool isLogarithmic)
{
    m_isLogarithmicScale.store (isLogarithmic);
}

void GraphLine::setColour (const juce::Colour& colour)
{
    m_colour = colour;
}

void GraphLine::setAudioProcessorValueTreeState (juce::AudioProcessorValueTreeState* state)
{
    mr_valueTree = state;
}

float GraphLine::getSpectrumOpacity() const
{
    if (mr_valueTree == nullptr || mr_valueTree->getRawParameterValue ("SPECTRUM_OPACITY_ID") == nullptr)
        return 1.0f;

    return juce::jlimit (0.0f, 1.0f, mr_valueTree->getRawParameterValue ("SPECTRUM_OPACITY_ID")->load() * 0.01f);
}

float GraphLine::getSpectrumFillOpacity() const
{
    if (mr_valueTree == nullptr || mr_valueTree->getRawParameterValue ("SPECTRUM_FILL_OPACITY_ID") == nullptr)
        return 1.0f;

    return juce::jlimit (0.0f, 1.0f, mr_valueTree->getRawParameterValue ("SPECTRUM_FILL_OPACITY_ID")->load() * 0.01f);
}

float GraphLine::getSpectrumPathWidth() const
{
    if (mr_valueTree == nullptr || mr_valueTree->getRawParameterValue ("SPECTRUM_PATH_WIDTH_ID") == nullptr)
        return 2.0f;

    return juce::jlimit (0.5f, 8.0f, mr_valueTree->getRawParameterValue ("SPECTRUM_PATH_WIDTH_ID")->load());
}

float GraphLine::getSpectrumResolution() const
{
    if (mr_valueTree == nullptr || mr_valueTree->getRawParameterValue ("SPECTRUM_RESOLUTION_ID") == nullptr)
        return 1.0f;

    return juce::jlimit (0.0f, 1.0f, mr_valueTree->getRawParameterValue ("SPECTRUM_RESOLUTION_ID")->load() * 0.01f);
}

float GraphLine::getSpectrumCurveSmoothRadiusPx() const
{
    // Off / Low / Med / High — display-space Gaussian radius in pixels.
    // Cosmetic only: does not add FFT resolution (use BLOCK_ID for that).
    static constexpr float kRadii[] = { 0.0f, 2.5f, 6.0f, 12.0f };

    if (mr_valueTree == nullptr)
        return kRadii[2]; // Med — good default at FFT 2048

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (mr_valueTree->getParameter ("SPECTRUM_CURVE_RES_ID")))
        return kRadii[juce::jlimit (0, 3, choice->getIndex())];

    return kRadii[2];
}

float GraphLine::normalizeValue (const int value)
{
    // Hz → x on EQ display axis (20 Hz … min(20 kHz, Nyquist)), not bin-index / Nyquist stretch.
    return mr_analyser.getBinDisplayXNorm (value, m_isLogarithmicScale.load());
}

float GraphLine::getScopeDataFromAnalyser (const size_t index)
{
    return mr_analyser.getScopeData (index);
}

float GraphLine::getModifiedScopeDataFromAnalyser (const size_t index)
{
    return mr_analyser.getModifiedScopeData (index);
}

void GraphLine::drawFrame (juce::Graphics& g)
{
    const auto width = static_cast<float> (getLocalBounds().getWidth());
    const auto height = static_cast<float> (getLocalBounds().getHeight());
    const int scopeSize = static_cast<int> (mr_analyser.getScopeSize());

    if (scopeSize < 2 || width < 2.0f || height < 2.0f)
        return;

    const int lastBin = mr_analyser.getHighestDisplayBinIndex();
    if (lastBin < 1)
        return;

    const bool showPreCurve = paramEnabled (mr_valueTree, "SPECTRUM_PRE_CURVE_ID", true);
    const bool showPreFill = paramEnabled (mr_valueTree, "SPECTRUM_PRE_FILL_ID", true);
    const bool showPostCurve = paramEnabled (mr_valueTree, "SPECTRUM_POST_CURVE_ID", true);
    const bool showPostFill = paramEnabled (mr_valueTree, "SPECTRUM_POST_FILL_ID", true);
    const bool showHoldCurve = paramEnabled (mr_valueTree, "MAX_ID", true);
    const bool showHoldFill = paramEnabled (mr_valueTree, "SPECTRUM_HOLD_FILL_ID", true);

    // Bar overlay on/off is SPECTRUM_FFT_BINS_ID only (FFT_RESOLUTION_ID is draw density).
    const bool binsWanted = paramEnabled (mr_valueTree, "SPECTRUM_FFT_BINS_ID", true);

    if (! showPreCurve && ! showPreFill && ! showPostCurve && ! showPostFill
        && ! showHoldCurve && ! showHoldFill && ! binsWanted)
        return;

    const float resolution = getSpectrumResolution();
    const bool drawSpectrumPaths = resolution > 0.0f;

    juce::Path preCurve;
    juce::Path postCurve;
    juce::Path holdCurve;

    const bool needPre = showPreCurve || showPreFill;
    const bool needPost = showPostCurve || showPostFill;
    const bool needHold = showHoldCurve || showHoldFill;

    if (drawSpectrumPaths)
    {
        // One sample per display column — peak-reduce FFT bins, interpolate sparse LF gaps.
        // SPECTRUM_CURVE_RES_ID is Curve Smoothness (display blur), not point density.
        const bool logarithmic = m_isLogarithmicScale.load();
        const int numColumns = juce::jmax (32, (int) std::lround ((double) width));
        const float smoothRadius = getSpectrumCurveSmoothRadiusPx();

        if (needPre)
        {
            reduceScopeToColumns (mr_analyser, lastBin, logarithmic, numColumns, height,
                                  [this] (size_t i) { return mr_analyser.getScopePreData (i); },
                                  m_columnPre, m_peakScratch, m_hasBinScratch,
                                  m_pointXScratch, m_pointMagScratch);
            smoothColumns (m_columnPre, m_smoothScratch, smoothRadius);
            appendColumnPath (preCurve, m_columnPre, width);
        }

        if (needPost)
        {
            reduceScopeToColumns (mr_analyser, lastBin, logarithmic, numColumns, height,
                                  [this] (size_t i) { return getScopeDataFromAnalyser (i); },
                                  m_columnPost, m_peakScratch, m_hasBinScratch,
                                  m_pointXScratch, m_pointMagScratch);
            smoothColumns (m_columnPost, m_smoothScratch, smoothRadius);
            appendColumnPath (postCurve, m_columnPost, width);
        }

        if (needHold)
        {
            reduceScopeToColumns (mr_analyser, lastBin, logarithmic, numColumns, height,
                                  [this] (size_t i) { return mr_analyser.getScopeMaximumsData (i); },
                                  m_columnHold, m_peakScratch, m_hasBinScratch,
                                  m_pointXScratch, m_pointMagScratch);
            smoothColumns (m_columnHold, m_smoothScratch, smoothRadius);
            appendColumnPath (holdCurve, m_columnHold, width);
        }
    }

    const float lineOpacity = getSpectrumOpacity();
    const float fillOpacity = lineOpacity * getSpectrumFillOpacity();
    const float pathWidth = getSpectrumPathWidth();

    auto closeFill = [width, height] (juce::Path curve) -> juce::Path
    {
        // Classic 240 fill close — do not change (Draw Density rewrite broke this).
        curve.lineTo (width, height);
        curve.lineTo (0.0f, height);
        curve.closeSubPath();
        return curve;
    };

    // --- Hold (gold/orange) ---
    if (drawSpectrumPaths && showHoldFill && needHold)
    {
        g.setColour (juce::Colour::fromRGBA (169, 100, 20, 90).withMultipliedAlpha (fillOpacity));
        g.fillPath (closeFill (holdCurve));
    }

    if (drawSpectrumPaths && showHoldCurve && needHold)
    {
        g.setColour (juce::Colour::fromRGBA (230, 170, 132, 105).withMultipliedAlpha (lineOpacity));
        g.strokePath (holdCurve.createPathWithRoundedCorners (kCornerRound), juce::PathStrokeType (pathWidth));
    }

    // --- Pre (dark brown, sits behind post) ---
    if (drawSpectrumPaths && showPreFill && needPre)
    {
        g.setColour (juce::Colour::fromRGBA (58, 42, 32, 110).withMultipliedAlpha (fillOpacity));
        g.fillPath (closeFill (preCurve));
    }

    if (drawSpectrumPaths && showPreCurve && needPre)
    {
        g.setColour (juce::Colour::fromRGBA (105, 78, 58, 140).withMultipliedAlpha (lineOpacity * 0.85f));
        g.strokePath (preCurve.createPathWithRoundedCorners (kCornerRound),
                      juce::PathStrokeType (juce::jmax (0.75f, pathWidth * 0.85f)));
    }

    // --- Post fill (240-ish warm fill under the live curve) ---
    if (drawSpectrumPaths && showPostFill && needPost)
    {
        g.setColour (juce::Colour::fromRGBA (115, 100, 63, 90).withMultipliedAlpha (fillOpacity));
        g.fillPath (closeFill (postCurve));
    }

    if (binsWanted)
    {
        m_binOverlay.paint (g,
                            mr_analyser,
                            { 0.0f, 0.0f, width, height },
                            m_isLogarithmicScale.load(),
                            mr_valueTree);
    }

    // --- Post curve + Melatonin glow (radius / spread / opacity from Spectrum settings) ---
    if (drawSpectrumPaths && showPostCurve && needPost)
    {
        const auto roundedPost = postCurve.createPathWithRoundedCorners (kCornerRound);

        auto loadGlow = [this] (const char* id, float fallback) -> float
        {
            if (mr_valueTree == nullptr || mr_valueTree->getRawParameterValue (id) == nullptr)
                return fallback;
            return mr_valueTree->getRawParameterValue (id)->load();
        };

        const bool glowEnabled = SharedResources::glowShadowEffectsEnabled()
                                 && loadGlow ("SPECTRUM_GLOW_ENABLE_ID", 1.0f) > 0.5f;
        if (glowEnabled)
        {
            const float glowOpacityPct = loadGlow ("SPECTRUM_GLOW_OPACITY_ID", 70.0f);
            const float glowRadius = loadGlow ("SPECTRUM_GLOW_RADIUS_ID", 12.0f);
            const float glowSpread = loadGlow ("SPECTRUM_GLOW_SPREAD_ID", 2.0f);
            const float glowAlpha = juce::jlimit (0.0f, 1.0f, glowOpacityPct * 0.01f * lineOpacity);

            if (glowAlpha > 0.05f && glowRadius > 0.5f)
            {
                const auto bloomColour = juce::Colour::fromRGBA (187, 219, 132, 130).withAlpha (glowAlpha * 0.45f);
                const auto coreColour = juce::Colour::fromRGBA (230, 255, 170, 180).withAlpha (glowAlpha * 0.75f);

                m_curveGlow.setRadius ((double) juce::jlimit (0.0f, 80.0f, glowRadius), 0);
                m_curveGlow.setSpread ((double) juce::jlimit (0.0f, 40.0f, glowSpread), 0);
                m_curveGlow.setOffset (0, 0, 0);
                m_curveGlow.setColor (bloomColour, 0);

                m_curveGlow.setRadius (juce::jmax (2.0, (double) glowRadius * 0.35), 1);
                m_curveGlow.setSpread (0.0, 1);
                m_curveGlow.setOffset (0, 0, 1);
                m_curveGlow.setColor (coreColour, 1);

                m_curveGlow.render (g, roundedPost,
                                    juce::PathStrokeType (juce::jmax (1.0f, pathWidth + 0.5f)), true);
            }
        }

        // Match 240 post stroke colour closely.
        g.setColour (juce::Colour::fromRGBA (187, 219, 132, 105).withMultipliedAlpha (lineOpacity));
        g.strokePath (roundedPost, juce::PathStrokeType (pathWidth));
    }
}
