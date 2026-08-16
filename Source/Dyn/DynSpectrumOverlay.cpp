#include "DynSpectrumOverlay.h"

namespace
{
    constexpr float kMinHz = 20.0f;
    constexpr float kMaxHz = 20000.0f;
    constexpr float kMinDb = -60.0f;
    constexpr float kMaxDb = 0.0f;
    constexpr float kMakeupMin = -24.0f;
    constexpr float kMakeupMax = 24.0f;
    constexpr float kHitPx = 8.0f;
    constexpr float kArrow = 6.5f;
    constexpr float kArrowGap = 11.0f;

    juce::Colour shadeForBand (juce::Colour c, int band) noexcept
    {
        static const float kOff[] = { -1.00f, 0.70f, -0.40f, 0.95f, -0.75f, 0.35f };
        const float k = kOff[juce::jlimit (0, 5, band)];
        float h = 0.0f, s = 0.0f, v = 0.0f;
        c.getHSB (h, s, v);
        h = std::fmod (h + 1.0f + 0.028f * k, 1.0f);
        s = juce::jlimit (0.0f, 1.0f, s * (1.0f + 0.10f * k));
        v = juce::jlimit (0.08f, 1.0f, v * (1.0f + 0.10f * k));
        return juce::Colour::fromHSV (h, s, v, c.getFloatAlpha());
    }

    juce::Colour fallbackBand (int b)
    {
        static const juce::Colour k[] = {
            juce::Colour::fromRGB (100, 149, 237),
            juce::Colour::fromRGB (192, 96, 224),
            juce::Colour::fromRGB (32, 224, 224),
            juce::Colour::fromRGB (80, 96, 255),
            juce::Colour::fromRGB (40, 170, 70),
            juce::Colour::fromRGB (230, 50, 50)
        };
        return k[juce::jlimit (0, 5, b)];
    }

    void drawChevron (juce::Graphics& g, juce::Point<float> tip, juce::Point<float> a, juce::Point<float> b,
                      juce::Colour fill)
    {
        juce::Path p;
        p.addTriangle (tip, a, b);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.strokePath (p, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (fill);
        g.fillPath (p);
    }

    void drawArrowLeft (juce::Graphics& g, float cx, float cy, float s, juce::Colour c)
    {
        drawChevron (g, { cx - s, cy }, { cx + s * 0.45f, cy - s }, { cx + s * 0.45f, cy + s }, c);
    }

    void drawArrowRight (juce::Graphics& g, float cx, float cy, float s, juce::Colour c)
    {
        drawChevron (g, { cx + s, cy }, { cx - s * 0.45f, cy - s }, { cx - s * 0.45f, cy + s }, c);
    }

    void drawArrowUp (juce::Graphics& g, float cx, float cy, float s, juce::Colour c)
    {
        drawChevron (g, { cx, cy - s }, { cx - s, cy + s * 0.45f }, { cx + s, cy + s * 0.45f }, c);
    }

    void drawArrowDown (juce::Graphics& g, float cx, float cy, float s, juce::Colour c)
    {
        drawChevron (g, { cx, cy + s }, { cx - s, cy - s * 0.45f }, { cx + s, cy - s * 0.45f }, c);
    }

    juce::String formatSplitHz (float hz)
    {
        hz = juce::jlimit (kMinHz, kMaxHz, hz);
        if (hz >= 1000.0f)
        {
            const float k = hz / 1000.0f;
            return juce::String (k, k >= 10.0f ? 1 : 2) + " kHz";
        }
        return juce::String (juce::roundToInt (hz)) + " Hz";
    }

    juce::Rectangle<float> splitLabelBounds (float x, float hz, int viewW)
    {
        const auto font = SharedResources::uiFont (11.0f);
        const auto text = formatSplitHz (hz);
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.0f, 0.0f);
        const float textW = ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth();
        const float labW = textW + 12.0f;
        const float labH = 16.0f;
        const float labX = juce::jlimit (2.0f, (float) juce::jmax (3, viewW) - labW - 2.0f,
                                         x - labW * 0.5f);
        return { labX, 3.0f, labW, labH };
    }

    void drawPlusBadge (juce::Graphics& g, float cx, float cy, juce::Colour c)
    {
        const float r = 11.0f;
        g.setColour (juce::Colours::black.withAlpha (0.50f));
        g.fillEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f);
        g.setColour (c.withAlpha (0.95f));
        g.drawEllipse (cx - r, cy - r, r * 2.0f, r * 2.0f, 1.3f);

        const float arm = 5.5f;
        const float th  = 2.1f;
        g.setColour (c.brighter (0.25f));
        g.fillRoundedRectangle (cx - th * 0.5f, cy - arm, th, arm * 2.0f, 0.8f);
        g.fillRoundedRectangle (cx - arm, cy - th * 0.5f, arm * 2.0f, th, 0.8f);
    }
}

DynSpectrumOverlay::DynSpectrumOverlay (juce::AudioProcessorValueTreeState& s, DynCompressor& e)
    : state (s), engine (e)
{
    setOpaque (false);
    setInterceptsMouseClicks (true, false);
    startTimerHz (30);
}

void DynSpectrumOverlay::timerCallback()
{
    constexpr float rise = 0.38f;
    constexpr float fall = 0.18f;
    for (int b = 0; b < DynParams::kMaxBands; ++b)
    {
        const float env = engine.getInputEnvelopeDb (b);
        const float d = juce::jlimit (0.0f, 60.0f, engine.getGainReductionDb (b));
        const float u = (env <= -60.0f) ? 0.0f
            : juce::jlimit (0.0f, 60.0f, engine.getUpwardGrDb (b));
        const float c = juce::jlimit (0.0f, 60.0f, engine.getClipDb (b));
        auto slide = [] (float& cur, float target)
        {
            cur += (target > cur ? rise : fall) * (target - cur);
            if (cur < 0.04f && target < 0.04f)
                cur = 0.0f;
        };
        slide (downAmt[(size_t) b], d);
        slide (upAmt[(size_t) b], u);
        slide (clipAmt[(size_t) b], c);
    }
    repaint();
}

int DynSpectrumOverlay::bandCount() const
{
    if (auto* p = state.getRawParameterValue (DynParams::countId()))
        return juce::jlimit (1, DynParams::kMaxBands, (int) std::lround (p->load()));
    return 1;
}

float DynSpectrumOverlay::hzToX (float hz) const
{
    const float a = std::log10 (kMinHz);
    const float b = std::log10 (kMaxHz);
    const float t = (std::log10 (juce::jlimit (kMinHz, kMaxHz, hz)) - a) / (b - a);
    return t * (float) getWidth();
}

float DynSpectrumOverlay::xToHz (float x) const
{
    const float a = std::log10 (kMinHz);
    const float b = std::log10 (kMaxHz);
    const float t = juce::jlimit (0.0f, 1.0f, x / (float) juce::jmax (1, getWidth()));
    return std::pow (10.0f, a + t * (b - a));
}

float DynSpectrumOverlay::dbToY (float db) const
{
    const float t = (juce::jlimit (kMinDb, kMaxDb, db) - kMaxDb) / (kMinDb - kMaxDb);
    return t * (float) getHeight();
}

float DynSpectrumOverlay::yToDb (float y) const
{
    const float t = juce::jlimit (0.0f, 1.0f, y / (float) juce::jmax (1, getHeight()));
    return kMaxDb + t * (kMinDb - kMaxDb);
}

float DynSpectrumOverlay::makeupToY (float db) const
{
    const float t = (kMakeupMax - juce::jlimit (kMakeupMin, kMakeupMax, db)) / (kMakeupMax - kMakeupMin);
    return 16.0f + t * ((float) getHeight() - 28.0f);
}

float DynSpectrumOverlay::yToMakeup (float y) const
{
    const float inner = (float) getHeight() - 28.0f;
    const float t = juce::jlimit (0.0f, 1.0f, (y - 16.0f) / juce::jmax (1.0f, inner));
    return kMakeupMax + t * (kMakeupMin - kMakeupMax);
}

juce::Colour DynSpectrumOverlay::bandColour (int b) const
{
    const auto& pal = theme != nullptr ? theme->sharedColors : SharedColors {};
    juce::Colour raw = fallbackBand (b);
    switch (juce::jlimit (0, 5, b))
    {
        case 0: raw = pal.graphBand1; break;
        case 1: raw = pal.graphBand2; break;
        case 2: raw = pal.graphBand3; break;
        case 3: raw = pal.graphBand4; break;
        case 4: raw = pal.graphBand5; break;
        case 5: raw = pal.graphBand6; break;
        default: break;
    }
    return pal.applyGraphBandMinSaturation (raw.withAlpha (1.0f));
}

juce::Colour DynSpectrumOverlay::makeupColour() const
{
    const auto& pal = theme != nullptr ? theme->sharedColors : SharedColors {};
    return pal.graphMakeupBar.withAlpha (1.0f);
}

DynSpectrumOverlay::Hit DynSpectrumOverlay::hitTest (juce::Point<float> p) const
{
    const int n = bandCount();
    auto readF = [this] (const juce::String& id, float fb)
    {
        if (auto* par = state.getRawParameterValue (id))
            return par->load();
        return fb;
    };

    for (int b = 0; b < n - 1; ++b)
    {
        const float hz = readF (DynParams::splitId (b), 1000.0f);
        const float x = hzToX (hz);
        const auto lab = splitLabelBounds (x, hz, getWidth());
        if (lab.expanded (2.0f, 2.0f).contains (p))
            return { Drag::split, b };
        if (std::abs (p.x - x) < kHitPx)
            return { Drag::split, b };
    }

    std::array<float, DynParams::kMaxBands + 1> edges {};
    edges[0] = kMinHz;
    for (int b = 0; b < n - 1; ++b)
        edges[(size_t) b + 1] = readF (DynParams::splitId (b), 1000.0f);
    edges[(size_t) n] = kMaxHz;

    for (int b = 0; b < n; ++b)
    {
        const float x0 = hzToX (edges[(size_t) b]);
        const float x1 = hzToX (edges[(size_t) b + 1]);
        if (p.x < x0 || p.x > x1)
            continue;

        const float inset = juce::jmin (15.0f, juce::jmax (4.0f, (x1 - x0 - 8.0f) * 0.5f));
        const bool inBlock = (p.x >= x0 + inset && p.x <= x1 - inset);

        const float yDown = dbToY (readF (DynParams::thresholdId (b), -18.0f));
        const float yUp   = dbToY (readF (DynParams::upThresholdId (b), -36.0f));
        const float yClip = dbToY (readF (DynParams::clipThrId (b), 0.0f));
        if (inBlock && std::abs (p.y - yClip) < kHitPx + 3.0f)
            return { Drag::clipThreshold, b };
        if (inBlock && std::abs (p.y - yDown) < kHitPx + 3.0f)
            return { Drag::threshold, b };
        if (inBlock && std::abs (p.y - yUp) < kHitPx + 3.0f)
            return { Drag::upThreshold, b };

        const float yMake = makeupToY (readF (DynParams::makeupId (b), 0.0f));
        if (std::abs (p.y - yMake) < kHitPx)
            return { Drag::makeup, b };
    }

    return {};
}

int DynSpectrumOverlay::hostBandAtX (float x) const
{
    const int n = bandCount();
    auto readF = [this] (const juce::String& id, float fb)
    {
        if (auto* par = state.getRawParameterValue (id))
            return par->load();
        return fb;
    };

    std::array<float, DynParams::kMaxBands + 1> edges {};
    edges[0] = kMinHz;
    for (int b = 0; b < n - 1; ++b)
        edges[(size_t) b + 1] = readF (DynParams::splitId (b), 1000.0f);
    edges[(size_t) n] = kMaxHz;

    for (int b = 0; b < n; ++b)
    {
        const float x0 = hzToX (edges[(size_t) b]);
        const float x1 = hzToX (edges[(size_t) b + 1]);
        if (x >= x0 && x <= x1)
            return b;
    }
    return juce::jlimit (0, n - 1, 0);
}

void DynSpectrumOverlay::applyCursor (Drag kind)
{
    switch (kind)
    {
        case Drag::split:      setMouseCursor (juce::MouseCursor::LeftRightResizeCursor); break;
        case Drag::threshold:
        case Drag::upThreshold:
        case Drag::clipThreshold:
        case Drag::makeup:     setMouseCursor (juce::MouseCursor::UpDownResizeCursor); break;
        default:               setMouseCursor (juce::MouseCursor::NormalCursor); break;
    }
}

void DynSpectrumOverlay::updateHover (const juce::MouseEvent& e)
{
    hoverPos = e.position;

    if (drag != Drag::none)
    {
        hover = drag;
        hoverIndex = dragIndex;
        showGhostAdd = false;
        applyCursor (drag);
        return;
    }

    const auto h = hitTest (e.position);
    hover = h.kind;
    hoverIndex = h.index;
    showGhostAdd = (h.kind == Drag::none
                    && bandCount() < DynParams::kMaxBands
                    && getLocalBounds().toFloat().contains (e.position));
    applyCursor (h.kind);
}

void DynSpectrumOverlay::paint (juce::Graphics& g)
{
    const int n = bandCount();
    const int sel = [&]
    {
        if (auto* p = state.getRawParameterValue (DynParams::selectedId()))
            return juce::jlimit (0, n - 1, (int) std::lround (p->load()));
        return 0;
    }();

    auto readF = [this] (const juce::String& id, float fb)
    {
        if (auto* p = state.getRawParameterValue (id))
            return p->load();
        return fb;
    };

    std::array<float, DynParams::kMaxBands + 1> edges {};
    edges[0] = kMinHz;
    for (int b = 0; b < n - 1; ++b)
        edges[(size_t) b + 1] = readF (DynParams::splitId (b), 1000.0f);
    edges[(size_t) n] = kMaxHz;

    const auto mkCol = makeupColour();

    for (int b = 0; b < n; ++b)
    {
        const float x0 = hzToX (edges[(size_t) b]);
        const float x1 = hzToX (edges[(size_t) b + 1]);
        int bandNameY = 22;
        auto c = bandColour (b);
        const bool hotThr = (hover == Drag::threshold && hoverIndex == b)
                         || (drag == Drag::threshold && dragIndex == b);
        const bool hotUp  = (hover == Drag::upThreshold && hoverIndex == b)
                         || (drag == Drag::upThreshold && dragIndex == b);
        const bool hotClip = (hover == Drag::clipThreshold && hoverIndex == b)
                          || (drag == Drag::clipThreshold && dragIndex == b);
        const bool hotMk  = (hover == Drag::makeup && hoverIndex == b)
                         || (drag == Drag::makeup && dragIndex == b);

        g.setColour (c.withAlpha (b == sel ? 0.20f : 0.10f));
        g.fillRect (x0, 0.0f, juce::jmax (1.0f, x1 - x0), (float) getHeight());

        // Full-width band level — same dB scale as threshold, 40% transparent.
        {
            const float env = engine.getInputEnvelopeDb (b);
            if (env > kMinDb + 0.5f)
            {
                const float yLvl = dbToY (env);
                const float w = juce::jmax (1.0f, x1 - x0);
                const float bottom = (float) getHeight();
                auto meter = juce::Rectangle<float> (x0, yLvl, w, juce::jmax (0.0f, bottom - yLvl));
                g.setColour (c.withAlpha (0.60f));
                g.fillRect (meter);
                g.setColour (c.brighter (0.20f).withAlpha (0.60f));
                g.fillRect (x0, yLvl, w, 2.0f);
            }
        }

        const float downThr = readF (DynParams::thresholdId (b), -18.0f);
        const float upThr   = readF (DynParams::upThresholdId (b), -36.0f);
        const float clipThr = readF (DynParams::clipThrId (b), 0.0f);
        const float yDown = dbToY (downThr);
        const float yUp   = dbToY (upThr);
        const float yClip = dbToY (clipThr);
        const float yTop  = dbToY (0.0f);
        const float yBot  = dbToY (-60.0f);
        const float inset = juce::jmin (15.0f, juce::jmax (4.0f, (x1 - x0 - 8.0f) * 0.5f));
        const float bx = x0 + inset;
        const float bw = juce::jmax (8.0f, x1 - x0 - inset * 2.0f);

        const auto dLo  = shadeForBand (juce::Colour::fromRGB (255, 132, 16), b);
        const auto dHi  = shadeForBand (juce::Colour::fromRGB (255, 148, 28), b);
        const auto dCap = shadeForBand (juce::Colour::fromRGB (255, 156, 36), b);
        const auto dEdge = shadeForBand (juce::Colour::fromRGB (255, 168, 48), b);
        const auto uLo  = shadeForBand (juce::Colour::fromRGB (160, 110, 30), b);
        const auto uHi  = shadeForBand (juce::Colour::fromRGB (232, 180, 74), b);
        const auto uCap = shadeForBand (juce::Colour::fromRGB (232, 196, 90), b);
        const auto uEdge = shadeForBand (juce::Colour::fromRGB (240, 208, 120), b);
        const auto grLo = shadeForBand (juce::Colour::fromRGB (196, 72, 8), b);
        const auto grHi = shadeForBand (juce::Colour::fromRGB (255, 132, 16), b);
        const auto clLo = juce::Colour::fromRGB (168, 6, 10);
        const auto clHi = juce::Colour::fromRGB (255, 28, 22);
        const auto upALo = shadeForBand (juce::Colour::fromRGB (22, 96, 48), b);
        const auto upAHi = shadeForBand (juce::Colour::fromRGB (72, 214, 118), b);

        auto downFill = juce::Rectangle<float> (bx, yTop, bw, juce::jmax (5.0f, yDown - yTop));
        auto upFill = juce::Rectangle<float> (bx, yUp, bw, juce::jmax (5.0f, yBot - yUp));

        if (SharedResources::glowShadowEffectsEnabled())
        {
            juce::Path downPath, upPath;
            downPath.addRoundedRectangle (downFill, 3.0f);
            upPath.addRoundedRectangle (upFill, 3.0f);
            zoneShadow.setRadius (12.0, 0);
            zoneShadow.setSpread (1.0, 0);
            zoneShadow.setOffset (0, 4, 0);
            zoneShadow.setColor (juce::Colours::black.withAlpha (0.48f), 0);
            zoneShadow.setRadius (5.0, 1);
            zoneShadow.setSpread (0.0, 1);
            zoneShadow.setOffset (0, 2, 1);
            zoneShadow.setColor (juce::Colours::black.withAlpha (0.28f), 1);
            zoneShadow.render (g, downPath);
            zoneShadow.render (g, upPath);
        }

        juce::ColourGradient downRamp (dLo.withAlpha (hotThr ? 0.38f : 0.22f),
                                       downFill.getX(), downFill.getY(),
                                       dHi.withAlpha (hotThr ? 0.62f : 0.46f),
                                       downFill.getX(), downFill.getBottom(), false);
        g.setGradientFill (downRamp);
        g.fillRoundedRectangle (downFill, 3.0f);
        g.setColour (dEdge.withAlpha (0.50f));
        g.drawRoundedRectangle (downFill, 3.0f, 1.0f);

        juce::ColourGradient upRamp (uHi.withAlpha (hotUp ? 0.55f : 0.42f),
                                     upFill.getX(), upFill.getY(),
                                     uLo.withAlpha (hotUp ? 0.28f : 0.18f),
                                     upFill.getX(), upFill.getBottom(), false);
        g.setGradientFill (upRamp);
        g.fillRoundedRectangle (upFill, 3.0f);
        g.setColour (uEdge.withAlpha (0.40f));
        g.drawRoundedRectangle (upFill, 3.0f, 1.0f);

        auto clipFill = juce::Rectangle<float> (bx, yTop, bw, juce::jmax (5.0f, yClip - yTop));
        juce::ColourGradient clipZoneRamp (clHi.withAlpha (hotClip ? 0.36f : 0.18f),
                                           clipFill.getX(), clipFill.getY(),
                                           clLo.withAlpha (hotClip ? 0.50f : 0.30f),
                                           clipFill.getX(), clipFill.getBottom(), false);
        g.setGradientFill (clipZoneRamp);
        g.fillRoundedRectangle (clipFill, 3.0f);

        const float downGr = downAmt[(size_t) b];
        const float clipGr = clipAmt[(size_t) b];

        if (downGr > 0.08f)
        {
            const float y0 = dbToY (0.0f);
            const float y1 = dbToY (-downGr);
            auto taken = juce::Rectangle<float> (bx, y0, bw, juce::jmax (0.5f, y1 - y0));
            juce::ColourGradient downAmtRamp (grHi.withAlpha (0.88f),
                                              taken.getX(), taken.getY(),
                                              grLo.withAlpha (0.62f),
                                              taken.getX(), taken.getBottom(), false);
            g.setGradientFill (downAmtRamp);
            g.fillRoundedRectangle (taken, 3.0f);
        }

        if (clipGr > 0.08f)
        {
            const float y0 = dbToY (0.0f);
            const float y1 = dbToY (-clipGr);
            auto clipR = juce::Rectangle<float> (bx, y0, bw, juce::jmax (0.5f, y1 - y0));
            juce::ColourGradient clipRamp (clHi.withAlpha (0.94f),
                                           clipR.getX(), clipR.getY(),
                                           clLo.withAlpha (0.86f),
                                           clipR.getX(), clipR.getBottom(), false);
            g.setGradientFill (clipRamp);
            g.fillRoundedRectangle (clipR, 3.0f);
            g.setColour (juce::Colour::fromRGB (255, 64, 48).withAlpha (0.95f));
            g.fillRoundedRectangle (clipR.getX(), clipR.getBottom() - 3.0f, clipR.getWidth(), 3.0f, 1.5f);
        }

        const float upGr = upAmt[(size_t) b];
        if (upGr > 0.08f)
        {
            const float yAmtBot = dbToY (-60.0f);
            const float yAmtTop = dbToY (-60.0f + juce::jmin (60.0f, upGr));
            auto given = juce::Rectangle<float> (bx, yAmtTop, bw, juce::jmax (0.5f, yAmtBot - yAmtTop));
            juce::ColourGradient upAmtRamp (upAHi.withAlpha (0.50f),
                                            given.getX(), given.getY(),
                                            upALo.withAlpha (0.28f),
                                            given.getX(), given.getBottom(), false);
            g.setGradientFill (upAmtRamp);
            g.fillRoundedRectangle (given, 3.0f);
        }

        auto downBar = juce::Rectangle<float> (bx, yDown - 2.5f, bw, 5.0f);
        auto upBar   = juce::Rectangle<float> (bx, yUp - 2.5f, bw, 5.0f);
        auto clipBar = juce::Rectangle<float> (bx, yClip - 2.5f, bw, 5.0f);
        if (SharedResources::glowShadowEffectsEnabled())
        {
            juce::Path bp;
            bp.addRoundedRectangle (downBar, 2.0f);
            bp.addRoundedRectangle (upBar, 2.0f);
            bp.addRoundedRectangle (clipBar, 2.0f);
            barShadow.setRadius (8.0, 0);
            barShadow.setSpread (1.0, 0);
            barShadow.setOffset (0, 3, 0);
            barShadow.setColor (juce::Colours::black.withAlpha (0.62f), 0);
            barShadow.setRadius (3.0, 1);
            barShadow.setSpread (0.0, 1);
            barShadow.setOffset (0, 1, 1);
            barShadow.setColor (juce::Colours::black.withAlpha (0.36f), 1);
            barShadow.render (g, bp);
        }
        g.setColour (dCap.withAlpha (hotThr ? 1.0f : 0.94f));
        g.fillRoundedRectangle (downBar, 2.0f);
        g.setColour (uCap.withAlpha (hotUp ? 1.0f : 0.92f));
        g.fillRoundedRectangle (upBar, 2.0f);
        g.setColour (juce::Colour::fromRGB (255, 48, 36).withAlpha (hotClip ? 1.0f : 0.94f));
        g.fillRoundedRectangle (clipBar, 2.0f);

        // Live clip / down at the top of the band, up at the bottom.
        {
            const auto amtFont = SharedResources::uiFont (15.0f);
            auto measure = [&amtFont] (const juce::String& t)
            {
                juce::GlyphArrangement ga;
                ga.addLineOfText (amtFont, t, 0.0f, 0.0f);
                return ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth();
            };
            const float labH = 20.0f;
            auto chip = [&] (float x, float y, const juce::String& text, juce::Colour col) -> float
            {
                const float tw = measure (text);
                const float labW = tw + 12.0f;
                const auto r = juce::Rectangle<float> (x, y, labW, labH);
                g.setColour (juce::Colours::black.withAlpha (0.55f));
                g.fillRoundedRectangle (r, 3.0f);
                g.setFont (amtFont);
                g.setColour (col);
                g.drawText (text, r.toNearestInt(), juce::Justification::centred, false);
                return labW;
            };
            auto centreX = [&] (float w)
            {
                return x0 + juce::jmax (0.0f, (x1 - x0 - w) * 0.5f);
            };

            const auto clipTxt = "Clp " + juce::String::formatted ("%.1f", (double) clipGr);
            const auto downTxt = "Dwn " + juce::String::formatted ("%.1f", (double) downGr);
            const auto upTxt   = "Up "  + juce::String::formatted ("%.1f", (double) upGr);
            const auto clipCol = juce::Colour::fromRGB (255, 36, 28);
            const auto downCol = juce::Colour::fromRGB (255, 148, 28);
            const auto upCol   = juce::Colour::fromRGB (72, 214, 118);
            const float clipW = measure (clipTxt) + 12.0f;
            const float downW = measure (downTxt) + 12.0f;
            const float upW   = measure (upTxt)   + 12.0f;
            const float gap = 6.0f;
            const float topY = 3.0f;
            const float bandW = juce::jmax (1.0f, x1 - x0);
            const bool sideBySide = (clipW + downW + gap <= bandW);

            g.saveState();
            g.reduceClipRegion (juce::Rectangle<int> (juce::roundToInt (x0) + 1, 0,
                                                     juce::jmax (1, juce::roundToInt (x1 - x0) - 2),
                                                     getHeight()));

            if (sideBySide)
            {
                const float groupW = clipW + gap + downW;
                float x = centreX (groupW);
                x += chip (x, topY, clipTxt, clipCol) + gap;
                chip (x, topY, downTxt, downCol);
            }
            else
            {
                chip (centreX (clipW), topY, clipTxt, clipCol);
                chip (centreX (downW), topY + labH + 2.0f, downTxt, downCol);
            }

            chip (centreX (upW), (float) getHeight() - labH - 3.0f, upTxt, upCol);
            g.restoreState();

            bandNameY = sideBySide ? (int) (topY + labH + 3.0f) : (int) (topY + labH * 2.0f + 5.0f);
        }

        const auto thrFont = SharedResources::uiFont (15.0f);
        auto drawThrLab = [&] (const juce::String& text, float x, float y, juce::Colour col)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (thrFont, text, 0.0f, 0.0f);
            const int tw = (int) std::ceil (ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth()) + 8;
            g.setFont (thrFont);
            g.setColour (col);
            g.drawText (text, juce::roundToInt (x), juce::roundToInt (y), tw, 20,
                        juce::Justification::centredLeft, false);
        };
        auto labYAbove = [] (float yBar) { return (yBar < 28.0f) ? yBar + 6.0f : yBar - 22.0f; };
        drawThrLab ("Down " + juce::String (downThr, 1) + " dB",
                    bx + 6.0f, labYAbove (yDown),
                    shadeForBand (juce::Colour::fromRGB (255, 176, 72), b));
        drawThrLab ("Up " + juce::String (upThr, 1) + " dB",
                    bx + 6.0f, yUp + 6.0f,
                    shadeForBand (juce::Colour::fromRGB (240, 208, 128), b));
        drawThrLab ("Clip " + juce::String (clipThr, 1) + " dB",
                    bx + 6.0f, labYAbove (yClip),
                    juce::Colour::fromRGB (255, 90, 72));

        const float make = readF (DynParams::makeupId (b), 0.0f);
        const float yMake = makeupToY (make);
        g.setColour (hotMk ? mkCol.brighter (0.25f) : mkCol);
        g.drawLine (x0 + 4.0f, yMake, x1 - 4.0f, yMake, hotMk ? 3.4f : 2.2f);
        g.fillEllipse (x0 + 2.0f, yMake - 4.0f, 8.0f, 8.0f);
        g.fillEllipse (x1 - 10.0f, yMake - 4.0f, 8.0f, 8.0f);

        g.setColour (mkCol.brighter (0.20f));
        auto mkTxt = "Makeup " + juce::String (make, 1) + " dB";
        g.drawText (mkTxt, juce::roundToInt (x0 + 12.0f), juce::roundToInt (yMake - 16.0f),
                    150, 14, juce::Justification::centredLeft, false);

        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.setFont (SharedResources::uiFont (11.0f));
        g.drawText ("Band " + juce::String (b + 1),
                    juce::roundToInt (x0 + 8.0f), bandNameY, 70, 16,
                    juce::Justification::centredLeft, false);
    }

    for (int b = 0; b < n - 1; ++b)
    {
        const float hz = edges[(size_t) b + 1];
        const float x = hzToX (hz);
        const bool hot = (hover == Drag::split && hoverIndex == b)
                      || (drag == Drag::split && dragIndex == b);
        const float thick = hot ? 5.0f : 4.0f;
        const auto lineCol = bandColour (b + 1).brighter (hot ? 0.25f : 0.0f);
        const auto lab = splitLabelBounds (x, hz, getWidth());
        const float gapBot = lab.getBottom() + 2.0f;

        juce::Path split;
        split.startNewSubPath (x, gapBot);
        split.lineTo (x, (float) getHeight());

        if (SharedResources::glowShadowEffectsEnabled())
        {
            splitShadow.setRadius (10.0, 0);
            splitShadow.setSpread (2.0, 0);
            splitShadow.setOffset (0, 2, 0);
            splitShadow.setColor (juce::Colours::black.withAlpha (0.55f), 0);
            splitShadow.setRadius (4.0, 1);
            splitShadow.setSpread (0.0, 1);
            splitShadow.setOffset (0, 1, 1);
            splitShadow.setColor (juce::Colours::black.withAlpha (0.32f), 1);
            splitShadow.render (g, split,
                                juce::PathStrokeType (thick + 0.5f,
                                                      juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded),
                                true);
        }

        g.setColour (lineCol);
        g.strokePath (split, juce::PathStrokeType (thick,
                                                   juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));

        g.setColour (juce::Colours::black.withAlpha (hot ? 0.62f : 0.48f));
        g.fillRoundedRectangle (lab, 3.0f);
        g.setColour (lineCol.withAlpha (hot ? 0.90f : 0.55f));
        g.drawRoundedRectangle (lab, 3.0f, 1.0f);
        g.setFont (SharedResources::uiFont (11.0f));
        g.setColour (juce::Colours::white.withAlpha (hot ? 1.0f : 0.92f));
        g.drawText (formatSplitHz (hz), lab.toNearestInt(), juce::Justification::centred, false);
    }

    // Hover / drag arrows — left/right on splits, up/down on bars.
    const float h = (float) getHeight();
    if (hover == Drag::split && hoverIndex >= 0 && hoverIndex < n - 1)
    {
        const float x = hzToX (readF (DynParams::splitId (hoverIndex), 1000.0f));
        const float y = juce::jlimit (18.0f, h - 18.0f, hoverPos.y);
        drawArrowLeft  (g, x - kArrowGap, y, kArrow, bandColour (hoverIndex));
        drawArrowRight (g, x + kArrowGap, y, kArrow, bandColour (hoverIndex + 1));
    }
    else if ((hover == Drag::threshold || hover == Drag::upThreshold
              || hover == Drag::clipThreshold || hover == Drag::makeup)
             && hoverIndex >= 0 && hoverIndex < n)
    {
        const float x0 = hzToX (edges[(size_t) hoverIndex]);
        const float x1 = hzToX (edges[(size_t) hoverIndex + 1]);
        const float x = juce::jlimit (x0 + 16.0f, x1 - 16.0f, hoverPos.x);
        const float y = hover == Drag::threshold
            ? dbToY (readF (DynParams::thresholdId (hoverIndex), -18.0f))
            : (hover == Drag::upThreshold
                ? dbToY (readF (DynParams::upThresholdId (hoverIndex), -36.0f))
                : (hover == Drag::clipThreshold
                    ? dbToY (readF (DynParams::clipThrId (hoverIndex), 0.0f))
                    : makeupToY (readF (DynParams::makeupId (hoverIndex), 0.0f))));
        const auto col = hover == Drag::threshold
            ? shadeForBand (juce::Colour::fromRGB (255, 148, 28), hoverIndex)
            : (hover == Drag::upThreshold
                ? shadeForBand (juce::Colour::fromRGB (232, 196, 90), hoverIndex)
                : (hover == Drag::clipThreshold
                    ? juce::Colour::fromRGB (255, 48, 36)
                    : mkCol));
        drawArrowUp   (g, x, y - kArrowGap, kArrow, col);
        drawArrowDown (g, x, y + kArrowGap, kArrow, col);
    }

    if (showGhostAdd && n < DynParams::kMaxBands)
    {
        const int host = hostBandAtX (hoverPos.x);
        const float hx0 = hzToX (edges[(size_t) host]);
        const float hx1 = hzToX (edges[(size_t) host + 1]);
        const float half = juce::jlimit (16.0f, 44.0f, (hx1 - hx0) * 0.22f);
        const float gx0 = juce::jmax (hx0 + 1.0f, hoverPos.x - half);
        const float gx1 = juce::jmin (hx1 - 1.0f, hoverPos.x + half);
        const auto ghost = bandColour (n);

        if (gx1 > gx0 + 2.0f)
        {
            juce::ColourGradient wash (ghost.withAlpha (0.00f), gx0, 0.0f,
                                       ghost.withAlpha (0.00f), gx1, 0.0f, false);
            wash.addColour (0.5, ghost.withAlpha (0.18f));
            g.setGradientFill (wash);
            g.fillRect (gx0, 0.0f, gx1 - gx0, h);

            g.setColour (ghost.withAlpha (0.55f));
            const float dashes[] = { 4.0f, 3.0f };
            g.drawDashedLine ({ hoverPos.x, 0.0f, hoverPos.x, h }, dashes, 2, 1.2f);
        }

        const float badgeY = juce::jlimit (28.0f, h - 28.0f, hoverPos.y);
        drawPlusBadge (g, hoverPos.x, badgeY, ghost);

        const auto addFont = SharedResources::uiFont (11.0f);
        const juce::String addWord { "Add Band" };
        juce::GlyphArrangement ga;
        ga.addLineOfText (addFont, addWord, 0.0f, 0.0f);
        const int labW = (int) std::ceil (ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth()) + 8;
        const int labX = juce::roundToInt (hoverPos.x + 16.0f);
        if (labX + labW < getWidth() - 8)
        {
            g.setFont (addFont);
            g.setColour (ghost.brighter (0.20f).withAlpha (0.90f));
            g.drawText (addWord, labX, juce::roundToInt (badgeY - 8.0f), labW, 16,
                        juce::Justification::centredLeft, false);
        }
    }

    g.setFont (SharedResources::uiFont (10.5f));
    g.setColour (juce::Colours::white.withAlpha (0.40f));
    g.drawText (n < DynParams::kMaxBands ? "Double-click to add a band    Drag a split to move it"
                                         : "Drag a split to move it",
                getLocalBounds().withY (22).withHeight (16).reduced (8, 0),
                juce::Justification::centredRight, false);
}

void DynSpectrumOverlay::mouseDown (const juce::MouseEvent& e)
{
    hoverPos = e.position;

    if (e.mods.isPopupMenu())
    {
        const int n = bandCount();
        const auto hit = hitTest (e.position);
        const int band = (hit.kind != Drag::none && hit.index >= 0) ? hit.index
                                                                    : hostBandAtX (e.position.x);

        juce::PopupMenu menu;
        menu.addItem (1, "Delete Band", n > 1);
        menu.addItem (2, "Solo");
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this, band] (int r)
                            {
                                if (r == 1)
                                    DynParams::removeBandAt (state, band);
                                else if (r == 2)
                                    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (
                                            state.getParameter (DynParams::soloId (band))))
                                        *p = ! (p->get());
                            });
        return;
    }

    const auto hit = hitTest (e.position);
    if (hit.kind != Drag::none)
    {
        drag = hit.kind;
        dragIndex = hit.index;
        hover = hit.kind;
        hoverIndex = hit.index;
        applyCursor (hit.kind);

        if (hit.kind == Drag::threshold || hit.kind == Drag::upThreshold
            || hit.kind == Drag::clipThreshold || hit.kind == Drag::makeup)
            if (auto* s = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (DynParams::selectedId())))
                *s = hit.index;
        return;
    }

    if (auto* s = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (DynParams::selectedId())))
        *s = hostBandAtX (e.position.x);
}

void DynSpectrumOverlay::mouseDoubleClick (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;
    if (hitTest (e.position).kind == Drag::split)
        return;
    DynParams::insertSplitAtHz (state, xToHz (e.position.x));
}

void DynSpectrumOverlay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel)
{
    const int band = hostBandAtX (e.position.x);
    auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (DynParams::ratioId (band)));
    if (p == nullptr)
        return;

    const float delta = (wheel.deltaY != 0.0f ? wheel.deltaY : -wheel.deltaX) * (wheel.isReversed ? -1.0f : 1.0f);
    const auto range = DynParams::ratioRange();
    const float mul = std::pow (1.08f, delta * 8.0f);
    *p = range.snapToLegalValue (p->get() * mul);
}

void DynSpectrumOverlay::mouseDrag (const juce::MouseEvent& e)
{
    hoverPos = e.position;
    if (drag == Drag::none || dragIndex < 0)
        return;

    if (drag == Drag::split)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (DynParams::splitId (dragIndex))))
            *p = xToHz (e.position.x);
    }
    else if (drag == Drag::threshold)
    {
        DynParams::writeDownThr (state, dragIndex, yToDb (e.position.y));
    }
    else if (drag == Drag::upThreshold)
    {
        DynParams::writeUpThr (state, dragIndex, yToDb (e.position.y));
    }
    else if (drag == Drag::clipThreshold)
    {
        DynParams::writeClipThr (state, dragIndex, yToDb (e.position.y));
    }
    else if (drag == Drag::makeup)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (DynParams::makeupId (dragIndex))))
            *p = yToMakeup (e.position.y);
    }
}

void DynSpectrumOverlay::mouseUp (const juce::MouseEvent& e)
{
    drag = Drag::none;
    dragIndex = -1;
    updateHover (e);
}

void DynSpectrumOverlay::mouseMove (const juce::MouseEvent& e)
{
    updateHover (e);
}

void DynSpectrumOverlay::mouseExit (const juce::MouseEvent&)
{
    if (drag != Drag::none)
        return;
    hover = Drag::none;
    hoverIndex = -1;
    showGhostAdd = false;
    setMouseCursor (juce::MouseCursor::NormalCursor);
}
