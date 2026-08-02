#pragma once

#include <JuceHeader.h>
#include <vector>

/** Ordered colour stops (0..1) with RGB interpolation. Cap at 128 poles. */
struct GradientRamp
{
    static constexpr int kMaxStops = 128;
    /** Default pole count after sampling / densify baseline. */
    static constexpr int kDefaultStops = 8;

    /** How the ramp is driven — intensity for FFT/Spec, axis for Spectrum Fill,
        goniometer diversion modes for the vectorscope. */
    enum class MapMode : int
    {
        intensityLowToHigh = 0, // quiet → loud / dark → bright
        intensityHighToLow,     // loud → quiet
        leftToRight,
        rightToLeft,
        topToBottom,
        bottomToTop,
        gonLoudness,            // sample loudness (√(L²+R²))
        gonDiversionX,          // |X| from plot centre (side)
        gonDiversionY,          // |Y| from plot centre (mid)
        gonDiversionXY,         // √(X²+Y²) from plot centre
        oscFreqLowToHigh,       // oscilloscope: zero-crossing rate low → high
        oscFreqHighToLow        // oscilloscope: zero-crossing rate high → low
    };

    /** Hard = linear between poles; Soft = smootherstep ease (less pole-focused). */
    enum class InterpMode : int
    {
        hard = 0,
        soft
    };

    struct Stop
    {
        float position = 0.0f; // 0..1 along ramp
        juce::Colour colour { juce::Colours::black };
    };

    std::vector<Stop> stops;
    bool enabled = false;
    MapMode mapMode = MapMode::intensityLowToHigh;
    InterpMode interpMode = InterpMode::hard;
    uint32_t revision = 0; // bump on every edit for dirty checks

    static float easeBlend (float u, InterpMode mode) noexcept
    {
        u = juce::jlimit (0.0f, 1.0f, u);
        if (mode == InterpMode::soft)
        {
            // Ken Perlin smootherstep — gentler shoulders around poles.
            return u * u * u * (u * (u * 6.0f - 15.0f) + 10.0f);
        }
        return u;
    }

    bool isUsable() const noexcept { return enabled && stops.size() >= 2; }
    bool isIntensityMap() const noexcept
    {
        return mapMode == MapMode::intensityLowToHigh || mapMode == MapMode::intensityHighToLow;
    }
    bool isSpatialMap() const noexcept
    {
        return mapMode == MapMode::leftToRight || mapMode == MapMode::rightToLeft
            || mapMode == MapMode::topToBottom || mapMode == MapMode::bottomToTop;
    }
    bool isGoniometerMap() const noexcept
    {
        return mapMode == MapMode::gonLoudness || mapMode == MapMode::gonDiversionX
            || mapMode == MapMode::gonDiversionY || mapMode == MapMode::gonDiversionXY;
    }
    bool isOscilloscopeMap() const noexcept
    {
        return isIntensityMap()
            || mapMode == MapMode::oscFreqLowToHigh
            || mapMode == MapMode::oscFreqHighToLow;
    }
    bool isOscilloscopeFrequencyMap() const noexcept
    {
        return mapMode == MapMode::oscFreqLowToHigh
            || mapMode == MapMode::oscFreqHighToLow;
    }

    /** Remap a 0..1 driver (intensity or normalised axis) through mapMode. */
    float mapDriver (float t01) const noexcept
    {
        t01 = juce::jlimit (0.0f, 1.0f, t01);
        switch (mapMode)
        {
            case MapMode::intensityHighToLow:
            case MapMode::rightToLeft:
            case MapMode::bottomToTop:
            case MapMode::oscFreqHighToLow:
                return 1.0f - t01;
            default:
                return t01;
        }
    }

    juce::Colour colourForDriver (float t01) const noexcept
    {
        return colourAt (mapDriver (t01));
    }

    void clear() noexcept
    {
        stops.clear();
        enabled = false;
        ++revision;
    }

    void setStops (std::vector<Stop> newStops)
    {
        stops = std::move (newStops);
        sortAndClamp();
        enabled = stops.size() >= 2;
        ++revision;
    }

    void sortAndClamp()
    {
        if (stops.size() > (size_t) kMaxStops)
            stops.resize ((size_t) kMaxStops);

        std::sort (stops.begin(), stops.end(),
                   [] (const Stop& a, const Stop& b) { return a.position < b.position; });

        for (auto& s : stops)
            s.position = juce::jlimit (0.0f, 1.0f, s.position);
    }

    static constexpr int kPoleSteps[] = { 2, 3, 4, 6, 8, 10, 14, 18, 24, 32, 40, 56, 72, 96, 128 };

    /** Next useful pole count below current (gentle steps — not half). Min 2. */
    static int nextSimplifyCount (int current) noexcept
    {
        for (int i = (int) (sizeof (kPoleSteps) / sizeof (kPoleSteps[0])) - 1; i >= 0; --i)
            if (current > kPoleSteps[i])
                return kPoleSteps[i];
        return 2;
    }

    /** Next useful pole count above current. Cap at kMaxStops. */
    static int nextDensifyCount (int current) noexcept
    {
        for (int s : kPoleSteps)
            if (current < s)
                return juce::jmin (s, kMaxStops);
        return kMaxStops;
    }

    void resampleToCount (int target)
    {
        target = juce::jlimit (2, kMaxStops, target);
        if ((int) stops.size() < 2 || target == (int) stops.size())
            return;

        std::vector<Stop> rebuilt;
        rebuilt.reserve ((size_t) target);
        for (int i = 0; i < target; ++i)
        {
            const float t = (float) i / (float) (target - 1);
            Stop s;
            s.position = t;
            s.colour = colourAt (t);
            rebuilt.push_back (s);
        }
        stops = std::move (rebuilt);
        ++revision;
    }

    /** Resample to fewer poles along the ramp (keeps endpoints). */
    void simplifyOneStep()
    {
        const int n = (int) stops.size();
        if (n <= 2)
            return;
        resampleToCount (nextSimplifyCount (n));
    }

    /** Resample to more poles for smoother gradations. */
    void densifyOneStep()
    {
        const int n = (int) stops.size();
        if (n >= kMaxStops || n < 2)
            return;
        resampleToCount (nextDensifyCount (n));
    }

    /** Reverse stop positions along the ramp (visual invert). */
    void invertStops() noexcept
    {
        if (stops.size() < 2)
            return;
        for (auto& s : stops)
            s.position = 1.0f - s.position;
        sortAndClamp();
        ++revision;
    }

    juce::Colour colourAt (float t) const noexcept
    {
        if (stops.empty())
            return juce::Colours::black;
        if (stops.size() == 1)
            return stops.front().colour;

        t = juce::jlimit (0.0f, 1.0f, t);
        if (t <= stops.front().position)
            return stops.front().colour;
        if (t >= stops.back().position)
            return stops.back().colour;

        for (size_t i = 1; i < stops.size(); ++i)
        {
            if (t <= stops[i].position)
            {
                const auto& a = stops[i - 1];
                const auto& b = stops[i];
                const float span = juce::jmax (1.0e-6f, b.position - a.position);
                const float u = easeBlend ((t - a.position) / span, interpMode);
                return a.colour.interpolatedWith (b.colour, u);
            }
        }

        return stops.back().colour;
    }

    /** Build a 256-entry LUT (spectrogram intensity → colour). Honours invert. */
    void fillLut (juce::PixelARGB* lut, int lutSize) const noexcept
    {
        if (lut == nullptr || lutSize <= 1)
            return;

        for (int i = 0; i < lutSize; ++i)
        {
            const float t = (float) i / (float) (lutSize - 1);
            lut[i] = colourForDriver (t).getPixelARGB();
        }
    }

    /** Spectrum-fill gradient across a paint rectangle (spatial map modes). */
    juce::ColourGradient makeSpatialGradient (juce::Rectangle<float> bounds, float alphaMul) const
    {
        juce::Point<float> p0, p1;
        switch (mapMode)
        {
            case MapMode::rightToLeft:
                p0 = { bounds.getRight(), bounds.getCentreY() };
                p1 = { bounds.getX(), bounds.getCentreY() };
                break;
            case MapMode::topToBottom:
                p0 = { bounds.getCentreX(), bounds.getY() };
                p1 = { bounds.getCentreX(), bounds.getBottom() };
                break;
            case MapMode::bottomToTop:
                p0 = { bounds.getCentreX(), bounds.getBottom() };
                p1 = { bounds.getCentreX(), bounds.getY() };
                break;
            case MapMode::leftToRight:
            default:
                p0 = { bounds.getX(), bounds.getCentreY() };
                p1 = { bounds.getRight(), bounds.getCentreY() };
                break;
        }

        // Spatial endpoints already encode direction — sample stops 0→1 along that axis.
        juce::ColourGradient grad (colourAt (0.0f).withMultipliedAlpha (alphaMul), p0.x, p0.y,
                                   colourAt (1.0f).withMultipliedAlpha (alphaMul), p1.x, p1.y, false);

        if (interpMode == InterpMode::soft)
        {
            // Dense samples so JUCE's linear ColourGradient still reads soft.
            constexpr int kSamples = 24;
            for (int i = 1; i < kSamples; ++i)
            {
                const float t = (float) i / (float) kSamples;
                grad.addColour ((double) t, colourAt (t).withMultipliedAlpha (alphaMul));
            }
        }
        else
        {
            for (size_t i = 1; i + 1 < stops.size(); ++i)
                grad.addColour ((double) stops[i].position,
                                stops[i].colour.withMultipliedAlpha (alphaMul));
        }
        return grad;
    }

    static juce::String mapModeName (MapMode m)
    {
        switch (m)
        {
            case MapMode::intensityLowToHigh: return "Quiet to Loud";
            case MapMode::intensityHighToLow: return "Loud to Quiet";
            case MapMode::leftToRight:        return "Left to Right";
            case MapMode::rightToLeft:        return "Right to Left";
            case MapMode::topToBottom:        return "Top to Bottom";
            case MapMode::bottomToTop:        return "Bottom to Top";
            case MapMode::gonLoudness:        return "Loudness";
            case MapMode::gonDiversionX:      return "X from Centre";
            case MapMode::gonDiversionY:      return "Y from Centre";
            case MapMode::gonDiversionXY:     return "X+Y from Centre";
            case MapMode::oscFreqLowToHigh:   return "Low to High";
            case MapMode::oscFreqHighToLow:   return "High to Low";
            default:                          return "Map";
        }
    }

    juce::ValueTree toValueTree (const juce::Identifier& type) const
    {
        juce::ValueTree tree (type);
        tree.setProperty ("enabled", enabled, nullptr);
        tree.setProperty ("mapMode", (int) mapMode, nullptr);
        tree.setProperty ("interpMode", (int) interpMode, nullptr);
        for (const auto& s : stops)
        {
            juce::ValueTree stop ("Stop");
            stop.setProperty ("pos", (double) s.position, nullptr);
            stop.setProperty ("argb", (int) s.colour.getARGB(), nullptr);
            tree.appendChild (stop, nullptr);
        }
        return tree;
    }

    static GradientRamp fromValueTree (const juce::ValueTree& tree)
    {
        GradientRamp ramp;
        if (! tree.isValid())
            return ramp;

        ramp.enabled = (bool) tree.getProperty ("enabled", false);
        ramp.mapMode = static_cast<MapMode> (juce::jlimit (
            0, (int) MapMode::oscFreqHighToLow, (int) tree.getProperty ("mapMode", (int) MapMode::intensityLowToHigh)));
        ramp.interpMode = static_cast<InterpMode> (juce::jlimit (
            0, (int) InterpMode::soft, (int) tree.getProperty ("interpMode", (int) InterpMode::hard)));
        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            const auto child = tree.getChild (i);
            if (! child.hasType ("Stop"))
                continue;

            Stop s;
            s.position = (float) (double) child.getProperty ("pos", 0.0);
            s.colour = juce::Colour ((juce::uint32) (int) child.getProperty ("argb", (int) 0xff000000));
            ramp.stops.push_back (s);
        }
        ramp.sortAndClamp();
        if (ramp.stops.size() < 2)
            ramp.enabled = false;
        ++ramp.revision;
        return ramp;
    }
};
