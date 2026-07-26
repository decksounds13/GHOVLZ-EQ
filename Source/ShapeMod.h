#pragma once

#include <JuceHeader.h>
#include "LfoMod.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <vector>

/**
    Custom Shape modulator — multi-point bipolar curve (−1..1) with optional
    soft corners + global smooth, driven by the same rate/sync/phase/retrig
    model as the LFOs. Curve geometry is stored outside APVTS (ValueTree).
*/
namespace ShapeMod
{
    inline constexpr const char* rateParamId()     noexcept { return "shapeRate"; }
    inline constexpr const char* rateSyncParamId() noexcept { return "shapeRateSync"; }
    inline constexpr const char* syncDivParamId()  noexcept { return "shapeSyncDiv"; }
    inline constexpr const char* phaseParamId()    noexcept { return "shapePhase"; }
    inline constexpr const char* retriggerParamId() noexcept { return "shapeRetrig"; }
    inline constexpr const char* smoothParamId()   noexcept { return "shapeSmooth"; }

    inline constexpr int kMaxPoints = 32;
    inline constexpr int kLutSize = 512;
    inline constexpr const char* kStateType = "SHAPE_CURVE";
    inline constexpr const char* kPointType = "PT";

    inline juce::NormalisableRange<float> smoothRange()
    {
        return { 0.0f, 100.0f, 1.0f };
    }

    struct Point
    {
        float x = 0.0f;   // phase 0..1
        float y = 0.0f;   // bipolar −1..1
        bool soft = false;
    };

    inline float resolveRateHz (juce::AudioProcessorValueTreeState& treeState, double bpm) noexcept
    {
        const bool sync = treeState.getRawParameterValue (rateSyncParamId()) != nullptr
                          && treeState.getRawParameterValue (rateSyncParamId())->load() > 0.5f;
        if (sync)
        {
            int div = LfoMod::kDefaultSyncDiv;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    treeState.getParameter (syncDivParamId())))
                div = p->getIndex();
            return LfoMod::syncDivToHz (div, bpm);
        }

        if (auto* v = treeState.getRawParameterValue (rateParamId()))
            return juce::jmax (0.05f, v->load());
        return 1.0f;
    }

    /** Piecewise linear curve; soft points get local rounded (Bezier) fillets. */
    class Curve
    {
    public:
        Curve() { resetToDefault(); }

        void resetToDefault()
        {
            points.clear();
            points.push_back ({ 0.0f, -1.0f, false });
            points.push_back ({ 1.0f,  1.0f, false });
            smoothPercent = 0.0f;
            dirty = true;
        }

        const std::vector<Point>& getPoints() const noexcept { return points; }

        void setSmoothPercent (float pct) noexcept
        {
            smoothPercent = juce::jlimit (0.0f, 100.0f, pct);
            dirty = true;
        }

        float getSmoothPercent() const noexcept { return smoothPercent; }

        void setPoints (std::vector<Point> newPoints)
        {
            points = std::move (newPoints);
            sanitise();
            dirty = true;
        }

        bool addPoint (float x, float y)
        {
            if ((int) points.size() >= kMaxPoints)
                return false;

            // Empty = flat "no modulation"; first add restores a baseline then inserts.
            if (points.empty())
            {
                points.push_back ({ 0.0f, -1.0f, false });
                points.push_back ({ 1.0f, -1.0f, false });
            }

            Point p { juce::jlimit (0.0f, 1.0f, x),
                      juce::jlimit (-1.0f, 1.0f, y),
                      false };
            points.push_back (p);
            sanitise();
            dirty = true;
            return true;
        }

        bool removePoint (int index)
        {
            if (index < 0 || index >= (int) points.size())
                return false;

            points.erase (points.begin() + index);

            // Fewer than 2 points → empty flat line at −1 (no modulation).
            if ((int) points.size() < 2)
                points.clear();
            else
                sanitise();

            dirty = true;
            return true;
        }

        /** True when the curve is cleared (flat −1 / no modulation). */
        bool isEmpty() const noexcept { return points.empty(); }

        bool movePoint (int index, float x, float y)
        {
            if (index < 0 || index >= (int) points.size())
                return false;

            // Endpoints stay pinned in X so the loop wraps cleanly.
            if (index == 0)
                x = 0.0f;
            else if (index == (int) points.size() - 1)
                x = 1.0f;
            else
                x = juce::jlimit (0.001f, 0.999f, x);

            points[(size_t) index].x = x;
            points[(size_t) index].y = juce::jlimit (-1.0f, 1.0f, y);
            sanitise();
            dirty = true;
            return true;
        }

        bool setPointSoft (int index, bool soft)
        {
            if (index < 0 || index >= (int) points.size())
                return false;

            points[(size_t) index].soft = soft;
            dirty = true;
            return true;
        }

        void rebuildLutIfNeeded() const
        {
            if (! dirty)
                return;

            std::array<float, kLutSize> raw {};
            for (int i = 0; i < kLutSize; ++i)
            {
                const float t = (float) i / (float) (kLutSize - 1);
                raw[(size_t) i] = evaluateRaw (t);
            }

            const float smooth01 = smoothPercent * 0.01f;
            if (smooth01 <= 0.001f)
            {
                lut = raw;
            }
            else
            {
                // Circular moving average — wraps for LFO-style loops.
                const int radius = juce::jmax (1, (int) std::round (smooth01 * 48.0f));
                for (int i = 0; i < kLutSize; ++i)
                {
                    double sum = 0.0;
                    int count = 0;
                    for (int d = -radius; d <= radius; ++d)
                    {
                        int j = i + d;
                        while (j < 0) j += kLutSize;
                        while (j >= kLutSize) j -= kLutSize;
                        sum += (double) raw[(size_t) j];
                        ++count;
                    }
                    lut[(size_t) i] = (float) (sum / (double) juce::jmax (1, count));
                }
            }

            dirty = false;
        }

        float evaluate (float phase01) const noexcept
        {
            rebuildLutIfNeeded();
            const float p = phase01 - std::floor (phase01);
            const float fIdx = p * (float) (kLutSize - 1);
            const int i0 = juce::jlimit (0, kLutSize - 1, (int) fIdx);
            const int i1 = juce::jmin (kLutSize - 1, i0 + 1);
            const float frac = fIdx - (float) i0;
            return lut[(size_t) i0] + (lut[(size_t) i1] - lut[(size_t) i0]) * frac;
        }

        juce::ValueTree toValueTree() const
        {
            juce::ValueTree tree (kStateType);
            tree.setProperty ("smooth", smoothPercent, nullptr);
            for (const auto& p : points)
            {
                juce::ValueTree pt (kPointType);
                pt.setProperty ("x", p.x, nullptr);
                pt.setProperty ("y", p.y, nullptr);
                pt.setProperty ("soft", p.soft, nullptr);
                tree.appendChild (pt, nullptr);
            }
            return tree;
        }

        void fromValueTree (const juce::ValueTree& tree)
        {
            if (! tree.hasType (kStateType))
            {
                resetToDefault();
                return;
            }

            smoothPercent = (float) tree.getProperty ("smooth", 0.0f);
            points.clear();
            for (int i = 0; i < tree.getNumChildren(); ++i)
            {
                const auto child = tree.getChild (i);
                if (! child.hasType (kPointType))
                    continue;

                Point p;
                p.x = (float) child.getProperty ("x", 0.0f);
                p.y = (float) child.getProperty ("y", 0.0f);
                p.soft = (bool) child.getProperty ("soft", false);
                points.push_back (p);
            }

            if ((int) points.size() < 2)
                points.clear(); // saved empty / flat "no modulation"
            else
                sanitise();

            dirty = true;
        }

        void copyLutTo (std::array<float, kLutSize>& dest) const
        {
            rebuildLutIfNeeded();
            dest = lut;
        }

    private:
        void sanitise()
        {
            if (points.empty())
                return;

            std::sort (points.begin(), points.end(),
                       [] (const Point& a, const Point& b) { return a.x < b.x; });

            points.front().x = 0.0f;
            points.back().x = 1.0f;

            // Collapse duplicates that share nearly the same X (keep average Y).
            std::vector<Point> unique;
            unique.reserve (points.size());
            for (const auto& p : points)
            {
                if (! unique.empty() && std::abs (unique.back().x - p.x) < 1.0e-4f)
                {
                    unique.back().y = 0.5f * (unique.back().y + p.y);
                    unique.back().soft = unique.back().soft || p.soft;
                }
                else
                {
                    unique.push_back (p);
                }
            }

            if ((int) unique.size() < 2)
                points.clear(); // flat −1 / no modulation
            else
                points = std::move (unique);
        }

        float evaluateRaw (float phase01) const noexcept
        {
            const float p = phase01 - std::floor (phase01);
            const int n = (int) points.size();
            if (n < 2)
                return -1.0f; // empty: flat line at bottom (no modulation)

            // Find segment [i0, i1] containing p.
            int i0 = 0;
            for (int i = 0; i < n - 1; ++i)
            {
                if (p >= points[(size_t) i].x && p <= points[(size_t) i + 1].x)
                {
                    i0 = i;
                    break;
                }
                if (p >= points[(size_t) i].x)
                    i0 = i;
            }

            const int i1 = juce::jmin (n - 1, i0 + 1);
            const auto& a = points[(size_t) i0];
            const auto& b = points[(size_t) i1];
            const float span = juce::jmax (1.0e-6f, b.x - a.x);

            // Soft = local fillet at the vertex (quadratic Bezier toward the point).
            // Hard = sharp corner. Mid-segment stays linear (no whole-segment S-curve).
            float rLeft = 0.0f;
            float rRight = 0.0f;

            if (a.soft && i0 > 0)
            {
                const float prevSpan = juce::jmax (1.0e-6f, a.x - points[(size_t) i0 - 1].x);
                rLeft = 0.5f * juce::jmin (prevSpan, span);
            }

            if (b.soft && i1 < n - 1)
            {
                const float nextSpan = juce::jmax (1.0e-6f, points[(size_t) i1 + 1].x - b.x);
                rRight = 0.5f * juce::jmin (span, nextSpan);
            }

            // Keep fillets from overlapping inside this segment.
            if (rLeft + rRight > span)
            {
                const float scale = span / juce::jmax (1.0e-6f, rLeft + rRight);
                rLeft *= scale;
                rRight *= scale;
            }

            auto lerp = [] (float u, float v, float t) noexcept { return u + (v - u) * t; };

            // Right half of fillet at A (control = A).
            if (rLeft > 1.0e-6f && p <= a.x + rLeft && i0 > 0)
            {
                const auto& prev = points[(size_t) i0 - 1];
                const float sT = (a.x - rLeft - prev.x) / juce::jmax (1.0e-6f, a.x - prev.x);
                const float sy = lerp (prev.y, a.y, sT);
                const float ey = lerp (a.y, b.y, rLeft / span);
                // x runs a.x-rLeft → a.x+rLeft ⇒ t = (p - (a.x-rLeft)) / (2*rLeft)
                const float t = juce::jlimit (0.0f, 1.0f, (p - (a.x - rLeft)) / (2.0f * rLeft));
                const float omt = 1.0f - t;
                return omt * omt * sy + 2.0f * omt * t * a.y + t * t * ey;
            }

            // Left half of fillet at B (control = B).
            if (rRight > 1.0e-6f && p >= b.x - rRight && i1 < n - 1)
            {
                const auto& next = points[(size_t) i1 + 1];
                const float sy = lerp (a.y, b.y, 1.0f - rRight / span);
                const float eT = rRight / juce::jmax (1.0e-6f, next.x - b.x);
                const float ey = lerp (b.y, next.y, eT);
                const float t = juce::jlimit (0.0f, 1.0f, (p - (b.x - rRight)) / (2.0f * rRight));
                const float omt = 1.0f - t;
                return omt * omt * sy + 2.0f * omt * t * b.y + t * t * ey;
            }

            const float t = juce::jlimit (0.0f, 1.0f, (p - a.x) / span);
            return lerp (a.y, b.y, t);
        }

        std::vector<Point> points;
        float smoothPercent = 0.0f;
        mutable std::array<float, kLutSize> lut {};
        mutable bool dirty = true;
    };

    /** Audio-thread reader: double-buffered LUT + LFO-style voice. */
    class Engine
    {
    public:
        LfoMod::Voice voice;
        Curve uiCurve; // edited on message thread only

        void prepare()
        {
            voice.reset();
            publishFromUi();
        }

        void publishFromUi()
        {
            const int write = 1 - published.load (std::memory_order_relaxed);
            uiCurve.copyLutTo (buffers[(size_t) write]);
            published.store (write, std::memory_order_release);
        }

        float evaluatePublished (float phase01) const noexcept
        {
            const int idx = published.load (std::memory_order_acquire);
            const auto& lut = buffers[(size_t) idx];
            const float p = phase01 - std::floor (phase01);
            const float fIdx = p * (float) (kLutSize - 1);
            const int i0 = juce::jlimit (0, kLutSize - 1, (int) fIdx);
            const int i1 = juce::jmin (kLutSize - 1, i0 + 1);
            const float frac = fIdx - (float) i0;
            return lut[(size_t) i0] + (lut[(size_t) i1] - lut[(size_t) i0]) * frac;
        }

        juce::ValueTree toValueTree() const { return uiCurve.toValueTree(); }

        void fromValueTree (const juce::ValueTree& tree)
        {
            uiCurve.fromValueTree (tree);
            publishFromUi();
        }

    private:
        std::array<std::array<float, kLutSize>, 2> buffers {};
        std::atomic<int> published { 0 };
    };

    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
    {
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            rateParamId(), "Shape Rate", LfoMod::rateHzRange(), 1.0f));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            rateSyncParamId(), "Shape Rate Sync", false));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            syncDivParamId(), "Shape Sync Div",
            LfoMod::getSyncDivNames(), LfoMod::kDefaultSyncDiv));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            phaseParamId(), "Shape Phase", LfoMod::phaseRange(), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            retriggerParamId(), "Shape Retrigger",
            LfoMod::getRetrigModeNames(), LfoMod::retrigOff));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            smoothParamId(), "Shape Smooth", smoothRange(), 0.0f));
    }
}
