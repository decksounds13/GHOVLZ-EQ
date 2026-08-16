#pragma once

#include <JuceHeader.h>
#include <functional>
#include "../Menu/SharedResources.h"
#include "DynParams.h"
#include "DynCompressor.h"

/** One well: incoming level, GR amount, and a draggable threshold. */
class GainReductionMeter : public juce::Component, private juce::Timer
{
public:
    GainReductionMeter (juce::AudioProcessorValueTreeState& s,
                        DynCompressor& e,
                        std::function<int()> bandFn)
        : state (s), engine (e), getBand (std::move (bandFn))
    {
        startTimerHz (30);
        setOpaque (false);
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
    }

    void setThemeColors (SharedResources* r) noexcept { theme = r; }
    static int getReadoutHeight() noexcept { return 18; }
    static int getScaleWidth() noexcept { return 24; }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        if (bounds.getWidth() < 8.0f || bounds.getHeight() < 20.0f)
            return;

        auto head = bounds.removeFromTop ((float) getReadoutHeight());
        const auto readFont = SharedResources::uiFont (12.0f);
        g.setFont (readFont);
        auto fmt1 = [] (float v)
        {
            return juce::String::formatted ("%.1f", (double) juce::jlimit (0.0f, 60.0f, v));
        };
        auto measure = [&readFont] (const juce::String& t)
        {
            juce::GlyphArrangement ga;
            ga.addLineOfText (readFont, t, 0.0f, 0.0f);
            return ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth() + 6.0f;
        };
        struct Amt
        {
            juce::String text;
            juce::Colour col;
            float w = 0.0f;
        };
        Amt amts[] = {
            { "Dwn " + fmt1 (readout), juce::Colour::fromRGB (255, 148, 28) },
            { "Up "  + fmt1 (upDb),    juce::Colour::fromRGB (72, 214, 118) },
            { "Clp " + fmt1 (clipAmt), juce::Colour::fromRGB (255, 36, 28) }
        };
        const juce::String shorts[] = {
            "D " + fmt1 (readout),
            "U " + fmt1 (upDb),
            "C " + fmt1 (clipAmt)
        };
        float gap = 5.0f;
        float groupW = 0.0f;
        for (auto& a : amts)
        {
            a.w = measure (a.text);
            groupW += a.w;
        }
        groupW += gap * 2.0f;
        if (groupW > head.getWidth() - 2.0f)
        {
            groupW = 0.0f;
            for (int i = 0; i < 3; ++i)
            {
                amts[i].text = shorts[i];
                amts[i].w = measure (amts[i].text);
                groupW += amts[i].w;
            }
            groupW += gap * 2.0f;
        }
        float x = head.getX() + juce::jmax (0.0f, (head.getWidth() - groupW) * 0.5f);
        for (auto& a : amts)
        {
            g.setColour (a.col);
            g.drawText (a.text,
                        juce::Rectangle<float> (x, head.getY(), a.w, head.getHeight()).toNearestInt(),
                        juce::Justification::centredLeft, false);
            x += a.w + gap;
        }

        auto well = bounds.reduced (1.0f);
        g.setColour (juce::Colour::fromRGB (12, 11, 9));
        g.fillRoundedRectangle (well, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (well, 2.0f, 1.0f);

        auto body = well.reduced (2.0f);
        if (body.getHeight() < 8.0f)
            return;

        const float arrowW = 11.0f;
        const float scaleW = (float) getScaleWidth();
        auto bar = body.withTrimmedLeft (arrowW + 2.0f).withTrimmedRight (scaleW);

        auto yOf = [&bar] (float db)
        {
            const float t = (juce::jlimit (-60.0f, 0.0f, db) + 60.0f) / 60.0f;
            return bar.getBottom() - t * bar.getHeight();
        };

        const float inset = 3.0f;
        auto zone = bar.reduced (inset, 2.0f);
        const float yDown = juce::jlimit (zone.getY(), zone.getBottom(), yOf (downThrDb));
        const float yUp   = juce::jlimit (zone.getY(), zone.getBottom(), yOf (upThrDb));
        const float yClip = juce::jlimit (zone.getY(), zone.getBottom(), yOf (clipThrDb));

        auto downFill = juce::Rectangle<float> (zone.getX(), zone.getY(),
                                                zone.getWidth(), juce::jmax (4.0f, yDown - zone.getY()));
        juce::ColourGradient downRamp (juce::Colour::fromRGB (255, 132, 16).withAlpha (0.16f),
                                       downFill.getX(), downFill.getY(),
                                       juce::Colour::fromRGB (255, 148, 28).withAlpha (0.38f),
                                       downFill.getX(), downFill.getBottom(), false);
        g.setGradientFill (downRamp);
        g.fillRoundedRectangle (downFill, 2.0f);

        auto upFill = juce::Rectangle<float> (zone.getX(), yUp,
                                              zone.getWidth(), juce::jmax (4.0f, zone.getBottom() - yUp));
        juce::ColourGradient upRamp (juce::Colour::fromRGB (232, 180, 74).withAlpha (0.36f),
                                     upFill.getX(), upFill.getY(),
                                     juce::Colour::fromRGB (160, 110, 30).withAlpha (0.14f),
                                     upFill.getX(), upFill.getBottom(), false);
        g.setGradientFill (upRamp);
        g.fillRoundedRectangle (upFill, 2.0f);

        auto clipZone = juce::Rectangle<float> (zone.getX(), zone.getY(),
                                                zone.getWidth(), juce::jmax (3.0f, yClip - zone.getY()));
        juce::ColourGradient clipZoneRamp (juce::Colour::fromRGB (255, 36, 28).withAlpha (0.14f),
                                           clipZone.getX(), clipZone.getY(),
                                           juce::Colour::fromRGB (180, 8, 12).withAlpha (0.32f),
                                           clipZone.getX(), clipZone.getBottom(), false);
        g.setGradientFill (clipZoneRamp);
        g.fillRoundedRectangle (clipZone, 2.0f);

        const float inY = yOf (levelDb);
        auto levelFill = juce::Rectangle<float> (bar.getX(), inY, bar.getWidth(), bar.getBottom() - inY);
        if (levelFill.getHeight() > 0.5f)
        {
            juce::ColourGradient levelRamp (juce::Colour::fromRGB (18, 48, 42).withAlpha (0.42f),
                                            levelFill.getX(), levelFill.getBottom(),
                                            juce::Colour::fromRGB (56, 168, 88).withAlpha (0.50f),
                                            levelFill.getX(), levelFill.getY(), false);
            levelRamp.addColour (0.50, juce::Colour::fromRGB (36, 122, 68).withAlpha (0.46f));
            g.setGradientFill (levelRamp);
            g.fillRect (levelFill);
        }

        if (upDb > 0.08f)
        {
            const float upTopY = yOf (-60.0f + juce::jmin (60.0f, upDb));
            auto given = juce::Rectangle<float> (bar.getX(), upTopY, bar.getWidth(),
                                                juce::jmax (0.5f, bar.getBottom() - upTopY));
            juce::ColourGradient upAmt (juce::Colour::fromRGB (72, 214, 118).withAlpha (0.48f),
                                        given.getX(), given.getY(),
                                        juce::Colour::fromRGB (22, 96, 48).withAlpha (0.28f),
                                        given.getX(), given.getBottom(), false);
            g.setGradientFill (upAmt);
            g.fillRect (given);
        }

        if (grDb > 0.08f)
        {
            const float grEndY = yOf (-grDb);
            auto taken = juce::Rectangle<float> (bar.getX(), bar.getY(),
                                                bar.getWidth(),
                                                juce::jmax (0.5f, grEndY - bar.getY()));
            juce::ColourGradient grRamp (juce::Colour::fromRGB (255, 132, 16).withAlpha (0.62f),
                                         taken.getX(), taken.getY(),
                                         juce::Colour::fromRGB (196, 72, 8).withAlpha (0.46f),
                                         taken.getX(), taken.getBottom(), false);
            g.setGradientFill (grRamp);
            g.fillRect (taken);
        }

        if (clipAmt > 0.08f)
        {
            const float clipEndY = yOf (-juce::jmin (60.0f, clipAmt));
            auto clipR = juce::Rectangle<float> (bar.getX(), bar.getY(),
                                                 bar.getWidth(),
                                                 juce::jmax (0.5f, clipEndY - bar.getY()));
            juce::ColourGradient clipRamp (juce::Colour::fromRGB (255, 28, 22).withAlpha (0.70f),
                                           clipR.getX(), clipR.getY(),
                                           juce::Colour::fromRGB (168, 6, 10).withAlpha (0.52f),
                                           clipR.getX(), clipR.getBottom(), false);
            g.setGradientFill (clipRamp);
            g.fillRect (clipR);
        }

        const float holdY = yOf (levelDb);
        g.setColour (juce::Colours::white.withAlpha (0.70f));
        g.fillRect (bar.getX(), holdY, bar.getWidth(), 1.0f);

        auto drawThrBar = [&] (float y, juce::Colour fill)
        {
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.fillRoundedRectangle (bar.getX(), y - 2.6f, bar.getWidth(), 5.2f, 1.6f);
            g.setColour (fill);
            g.fillRoundedRectangle (bar.getX() + 0.5f, y - 2.0f, bar.getWidth() - 1.0f, 4.0f, 1.4f);
        };
        drawThrBar (yDown, juce::Colour::fromRGB (255, 156, 36));
        drawThrBar (yUp,   juce::Colour::fromRGB (232, 196, 90));
        drawThrBar (yClip, juce::Colour::fromRGB (255, 48, 36));

        auto drawArrow = [&] (float y, juce::Colour fill)
        {
            juce::Path arrow;
            arrow.addTriangle (body.getX(), y - 5.0f, body.getX(), y + 5.0f, body.getX() + arrowW, y);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.fillPath (arrow);
            juce::Path inner;
            inner.addTriangle (body.getX() + 1.0f, y - 3.6f,
                               body.getX() + 1.0f, y + 3.6f,
                               body.getX() + arrowW - 1.2f, y);
            g.setColour (fill);
            g.fillPath (inner);
        };
        drawArrow (yDown, juce::Colour::fromRGB (255, 148, 28));
        drawArrow (yUp,   juce::Colour::fromRGB (232, 196, 90));
        drawArrow (yClip, juce::Colour::fromRGB (255, 48, 36));

        const float ticks[] = { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f };
        g.setFont (SharedResources::uiFont (10.0f));
        for (float db : ticks)
        {
            const float y = yOf (db);
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRect (bar.getRight() - 3.0f, y, 3.0f, 1.0f);
            auto label = (db > -0.1f) ? juce::String ("0") : juce::String ((int) db);
            auto tickR = juce::Rectangle<float> (bar.getRight() + 2.0f, y - 6.0f, scaleW - 2.0f, 12.0f);
            g.setColour (juce::Colours::white.withAlpha (0.62f));
            g.drawText (label, tickR.toNearestInt(), juce::Justification::centredLeft, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragKind = hitKindAt (e.position.y);
        dragThreshold (e.position.y);
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        dragThreshold (e.position.y);
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        if (dragKind == DragKind::up)
            DynParams::writeUpThr (state, currentBand(), -36.0f);
        else if (dragKind == DragKind::clip)
            DynParams::writeClipThr (state, currentBand(), 0.0f);
        else
            DynParams::writeDownThr (state, currentBand(), -18.0f);
    }

private:
    juce::Rectangle<float> barBody() const
    {
        auto bounds = getLocalBounds().toFloat();
        bounds.removeFromTop ((float) getReadoutHeight());
        return bounds.reduced (3.0f).withTrimmedLeft (13.0f)
                                    .withTrimmedRight ((float) getScaleWidth());
    }

    int currentBand() const noexcept
    {
        return getBand ? juce::jlimit (0, DynParams::kMaxBands - 1, getBand()) : 0;
    }

    enum class DragKind { down, up, clip };

    DragKind hitKindAt (float y) const noexcept
    {
        const auto bar = barBody();
        if (bar.getHeight() < 4.0f)
            return DragKind::down;
        const float t = juce::jlimit (0.0f, 1.0f, (bar.getBottom() - y) / bar.getHeight());
        const float db = -60.0f + 60.0f * t;
        const float dDown = std::abs (db - downThrDb);
        const float dUp   = std::abs (db - upThrDb);
        const float dClip = std::abs (db - clipThrDb);
        if (dClip <= dDown && dClip <= dUp)
            return DragKind::clip;
        if (dUp <= dDown)
            return DragKind::up;
        return DragKind::down;
    }

    void dragThreshold (float y)
    {
        const auto bar = barBody();
        if (bar.getHeight() < 4.0f)
            return;
        const float t = juce::jlimit (0.0f, 1.0f, (bar.getBottom() - y) / bar.getHeight());
        const float db = -60.0f + 60.0f * t;
        if (dragKind == DragKind::up)
            DynParams::writeUpThr (state, currentBand(), db);
        else if (dragKind == DragKind::clip)
            DynParams::writeClipThr (state, currentBand(), db);
        else
            DynParams::writeDownThr (state, currentBand(), db);
    }

    float readMs (const char* id, float fallback) const noexcept
    {
        if (auto* p = state.getRawParameterValue (id))
            return p->load();
        return fallback;
    }

    static float coeffForMs (float ms, float hz) noexcept
    {
        const float tau = juce::jmax (0.001f, ms * 0.001f);
        return 1.0f - std::exp (-1.0f / (tau * hz));
    }

    void timerCallback() override
    {
        const int band = currentBand();
        const float envIn = engine.getInputEnvelopeDb (band);
        const float nowGr = juce::jlimit (0.0f, 60.0f, engine.getGainReductionDb (band));
        const float nowUp = (envIn <= -60.0f) ? 0.0f
            : juce::jlimit (0.0f, 60.0f, engine.getUpwardGrDb (band));
        const float nowClip = juce::jlimit (0.0f, 60.0f, engine.getClipDb (band));
        const float nowIn = juce::jlimit (-60.0f, 0.0f, envIn);
        downThrDb = -18.0f;
        upThrDb = -36.0f;
        clipThrDb = 0.0f;
        if (auto* p = state.getRawParameterValue (DynParams::thresholdId (band)))
            downThrDb = p->load();
        if (auto* p = state.getRawParameterValue (DynParams::upThresholdId (band)))
            upThrDb = p->load();
        if (auto* p = state.getRawParameterValue (DynParams::clipThrId (band)))
            clipThrDb = p->load();
        float ratio = 4.0f;
        if (auto* p = state.getRawParameterValue (DynParams::ratioId (band)))
            ratio = p->load();
        gainIsUp = ratio < 0.999f;

        const float avgMs  = juce::jlimit (20.0f, 2000.0f, readMs (DynParams::grAvgMsId(), 180.0f));
        const float fallMs = juce::jlimit (20.0f, 4000.0f, readMs (DynParams::grFallMsId(), 400.0f));
        const float riseA = coeffForMs (avgMs, 30.0f);
        const float fallA = coeffForMs (fallMs, 30.0f);

        if (nowGr > grDb) grDb += riseA * (nowGr - grDb);
        else              grDb += fallA * (nowGr - grDb);
        if (nowUp > upDb) upDb += riseA * (nowUp - upDb);
        else              upDb += fallA * (nowUp - upDb);
        if (nowClip > clipAmt) clipAmt += riseA * (nowClip - clipAmt);
        else                   clipAmt += fallA * (nowClip - clipAmt);

        if (nowGr > readout) readout += riseA * (nowGr - readout);
        else                 readout += riseA * (nowGr - readout);

        if (nowIn > levelDb) levelDb += riseA * (nowIn - levelDb);
        else                 levelDb += fallA * (nowIn - levelDb);

        repaint();
    }

    juce::AudioProcessorValueTreeState& state;
    DynCompressor& engine;
    std::function<int()> getBand;
    SharedResources* theme = nullptr;
    float grDb = 0.0f;
    float upDb = 0.0f;
    float clipAmt = 0.0f;
    float readout = 0.0f;
    float levelDb = -60.0f;
    float downThrDb = -18.0f;
    float upThrDb = -36.0f;
    float clipThrDb = 0.0f;
    bool gainIsUp = false;
    DragKind dragKind = DragKind::down;
};
