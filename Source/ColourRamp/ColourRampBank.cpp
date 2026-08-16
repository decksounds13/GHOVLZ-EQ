#include "ColourRampBank.h"
#include "../Menu/SharedResources.h"

ColourRampBank::ColourRampBank()
{
    // Sensible starter ramps (disabled until the user samples / edits).
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.0f, juce::Colours::black },
            { 0.45f, juce::Colour::fromRGB (255, 90, 40) },
            { 1.0f, juce::Colours::white }
        };
        ramps[(int) Target::fftBars] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (0, 0, 4) },
            { 0.5f, juce::Colour::fromRGB (212, 72, 66) },
            { 1.0f, juce::Colour::fromRGB (252, 255, 164) }
        };
        ramps[(int) Target::spectrogram] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (2, 0, 12) },
            { 0.4f, juce::Colour::fromRGB (40, 120, 255) },
            { 0.75f, juce::Colour::fromRGB (255, 90, 180) },
            { 1.0f, juce::Colour::fromRGB (255, 245, 200) }
        };
        ramps[(int) Target::spectrogram3D] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::bottomToTop;
        r.stops = {
            { 0.0f, juce::Colour::fromRGBA (58, 42, 32, 180) },
            { 1.0f, juce::Colour::fromRGBA (187, 219, 132, 200) }
        };
        r.enabled = true; // product default: spectrum fill gradient on out of the box
        ramps[(int) Target::spectrumFill] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (0, 0, 8) },
            { 0.5f, juce::Colour::fromRGB (80, 200, 255) },
            { 1.0f, juce::Colour::fromRGB (255, 255, 255) }
        };
        ramps[(int) Target::oscilloscope] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::gonLoudness;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (0, 0, 6) },
            { 0.55f, juce::Colour::fromRGB (255, 180, 60) },
            { 1.0f, juce::Colour::fromRGB (255, 255, 220) }
        };
        ramps[(int) Target::goniometer] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (0, 0, 4) },
            { 0.45f, juce::Colour::fromRGB (120, 80, 255) },
            { 1.0f, juce::Colour::fromRGB (255, 240, 200) }
        };
        ramps[(int) Target::stereogram] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (20, 10, 8) },
            { 0.55f, juce::Colour::fromRGB (230, 60, 55) },
            { 1.0f, juce::Colour::fromRGB (255, 230, 180) }
        };
        ramps[(int) Target::histogram] = std::move (r);
    }
    {
        // Default peak meter: green → yellow → orange → red (VU-style).
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.00f, juce::Colour::fromRGB (20, 140, 55) },
            { 0.55f, juce::Colour::fromRGB (210, 200, 40) },
            { 0.78f, juce::Colour::fromRGB (240, 130, 30) },
            { 1.00f, juce::Colour::fromRGB (230, 40, 35) }
        };
        r.enabled = true; // product default: meters use ramps out of the box
        ramps[(int) Target::meterPeak] = std::move (r);
    }
    {
        // Default RMS meter: cooler blue → cyan → white.
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = {
            { 0.00f, juce::Colour::fromRGB (25, 70, 160) },
            { 0.55f, juce::Colour::fromRGB (40, 180, 220) },
            { 1.00f, juce::Colour::fromRGB (230, 245, 255) }
        };
        r.enabled = true;
        ramps[(int) Target::meterRms] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::leftToRight;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (70, 200, 255) },
            { 0.5f, juce::Colour::fromRGB (255, 210, 80) },
            { 1.0f, juce::Colour::fromRGB (255, 90, 150) }
        };
        r.enabled = true; // product default: analyzer curves ramp L→R
        ramps[(int) Target::spectrumCurve] = std::move (r);
    }
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::leftToRight;
        r.stops = {
            { 0.0f, juce::Colour::fromRGB (90, 210, 255) },
            { 0.55f, juce::Colour::fromRGB (255, 215, 95) },
            { 1.0f, juce::Colour::fromRGB (255, 85, 165) }
        };
        r.enabled = true; // product default: EQ curves ramp L→R
        ramps[(int) Target::eqCurve] = std::move (r);
    }

    auto makeSpatial = [] (GradientRamp::MapMode mode,
                           std::initializer_list<GradientRamp::Stop> stops) -> GradientRamp
    {
        GradientRamp r;
        r.mapMode = mode;
        r.stops = stops;
        r.enabled = true;
        return r;
    };

    ramps[(int) Target::spectrumPreFill] = makeSpatial (
        GradientRamp::MapMode::bottomToTop,
        { { 0.0f, juce::Colour::fromRGBA (32, 48, 58, 160) },
          { 1.0f, juce::Colour::fromRGBA (120, 190, 210, 190) } });
    ramps[(int) Target::spectrumPreCurve] = makeSpatial (
        GradientRamp::MapMode::leftToRight,
        { { 0.0f, juce::Colour::fromRGB (60, 150, 200) },
          { 0.55f, juce::Colour::fromRGB (180, 210, 90) },
          { 1.0f, juce::Colour::fromRGB (220, 120, 80) } });
    ramps[(int) Target::spectrumHoldFill] = makeSpatial (
        GradientRamp::MapMode::bottomToTop,
        { { 0.0f, juce::Colour::fromRGBA (58, 32, 42, 160) },
          { 1.0f, juce::Colour::fromRGBA (219, 150, 132, 190) } });
    ramps[(int) Target::spectrumHoldCurve] = makeSpatial (
        GradientRamp::MapMode::leftToRight,
        { { 0.0f, juce::Colour::fromRGB (220, 170, 80) },
          { 0.55f, juce::Colour::fromRGB (255, 140, 90) },
          { 1.0f, juce::Colour::fromRGB (255, 80, 140) } });
    ramps[(int) Target::eqSumFill] = makeSpatial (
        GradientRamp::MapMode::topToBottom,
        { { 0.0f, juce::Colour::fromRGBA (255, 130, 30, 180) },
          { 1.0f, juce::Colour::fromRGBA (139, 105, 20, 100) } });
    ramps[(int) Target::eqBandCurve] = makeSpatial (
        GradientRamp::MapMode::leftToRight,
        { { 0.0f, juce::Colour::fromRGB (120, 220, 255) },
          { 0.5f, juce::Colour::fromRGB (255, 200, 80) },
          { 1.0f, juce::Colour::fromRGB (255, 90, 180) } });
    ramps[(int) Target::eqBandFill] = makeSpatial (
        GradientRamp::MapMode::bottomToTop,
        { { 0.0f, juce::Colour::fromRGBA (40, 50, 70, 140) },
          { 1.0f, juce::Colour::fromRGBA (200, 180, 90, 170) } });

    load();
    sanitizeMapModes();
    // After disk load (which clears Use on most targets), keep meter ramps on unless
    // the user explicitly saved them off — see applyFromValueTree.
}

juce::String ColourRampBank::targetName (Target t)
{
    switch (t)
    {
        case Target::fftBars:       return "FFT Bars";
        case Target::spectrogram:   return "Spectrogram";
        case Target::spectrogram3D: return "Spectrogram 3D";
        case Target::spectrumFill:      return "Post Fill";
        case Target::oscilloscope:      return "Oscilloscope";
        case Target::goniometer:        return "Goniometer";
        case Target::stereogram:        return "Stereogram";
        case Target::histogram:         return "Histogram";
        case Target::meterPeak:         return "Meter Peak";
        case Target::meterRms:          return "Meter RMS";
        case Target::spectrumCurve:     return "Post Curve";
        case Target::eqCurve:           return "Sum Curve";
        case Target::spectrumPreFill:   return "Pre Fill";
        case Target::spectrumPreCurve:  return "Pre Curve";
        case Target::spectrumHoldFill:  return "Hold Fill";
        case Target::spectrumHoldCurve: return "Hold Curve";
        case Target::eqSumFill:         return "Sum Fill";
        case Target::eqBandCurve:       return "Band Curve";
        case Target::eqBandFill:        return "Band Fill";
        default:                        return "Ramp";
    }
}

ColourRampBank::Target ColourRampBank::clampTarget (int idx) noexcept
{
    return static_cast<Target> (juce::jlimit (0, (int) Target::numTargets - 1, idx));
}

GradientRamp& ColourRampBank::get (Target t) noexcept
{
    return ramps[(int) clampTarget ((int) t)];
}

const GradientRamp& ColourRampBank::get (Target t) const noexcept
{
    return ramps[(int) clampTarget ((int) t)];
}

ColourRampBank::Target ColourRampBank::getActiveTarget() const noexcept
{
    return clampTarget (activeTarget);
}

void ColourRampBank::setActiveTarget (Target t) noexcept
{
    activeTarget = (int) clampTarget ((int) t);
}

void ColourRampBank::clearActiveTarget() noexcept
{
    activeTarget = -1;
}

void ColourRampBank::setRamp (Target t, GradientRamp ramp)
{
    // Keep map / blend; sampling enables Use so the new colours are visible.
    const auto keepMode = get (t).mapMode;
    const auto keepInterp = get (t).interpMode;
    get (t) = std::move (ramp);
    get (t).mapMode = keepMode;
    get (t).interpMode = keepInterp;
    get (t).enabled = get (t).stops.size() >= 2;
    ++get (t).revision;
    sanitizeMapModes();
    save();
    sendChangeMessage();
}

void ColourRampBank::notifyEdited()
{
    save();
    sendChangeMessage();
}

void ColourRampBank::notifyPreview()
{
    sendChangeMessage();
}

void ColourRampBank::randomizeRamp (GradientRamp& ramp,
                                    const SharedColors& colours,
                                    bool varyAlpha)
{
    auto& rng = juce::Random::getSystemRandom();
    const int n = rng.nextInt ({ 2, 11 }); // 2..10 inclusive
    std::vector<GradientRamp::Stop> stops;
    stops.reserve ((size_t) n);

    if (! colours.orderedRampGradation)
    {
        for (int i = 0; i < n; ++i)
        {
            GradientRamp::Stop s;
            if (i == 0)           s.position = 0.0f;
            else if (i == n - 1)  s.position = 1.0f;
            else                  s.position = rng.nextFloat();

            const float alpha = varyAlpha ? (0.35f + rng.nextFloat() * 0.55f) : 1.0f;
            s.colour = colours.randomColourInLimits (alpha);
            stops.push_back (s);
        }
    }
    else
    {
        // Ordered: random endpoint spans inside Appearance limits, evenly spaced stops.
        auto pickEnds = [&rng] (bool enabled, float lo, float hi) -> std::pair<float, float>
        {
            lo = juce::jlimit (0.0f, 1.0f, lo);
            hi = juce::jlimit (lo, 1.0f, hi);
            if (! enabled || hi <= lo + 1.0e-5f)
                return { lo, lo };

            float a = lo + rng.nextFloat() * (hi - lo);
            float b = lo + rng.nextFloat() * (hi - lo);
            // Random direction (dark→light / light→dark, etc.).
            if (rng.nextBool())
                std::swap (a, b);
            return { a, b };
        };

        const auto [h0, h1] = pickEnds (colours.randomizeHue,
                                        colours.hueLowerLimit, colours.hueUpperLimit);
        const auto [s0, s1] = pickEnds (colours.randomizeSaturation,
                                        colours.saturationLowerLimit, colours.saturationUpperLimit);
        const auto [v0, v1] = pickEnds (colours.randomizeBrightness,
                                        colours.brightnessLowerLimit, colours.brightnessUpperLimit);
        const auto [a0, a1] = varyAlpha
                                  ? pickEnds (true, 0.35f, 0.90f)
                                  : std::pair<float, float> { 1.0f, 1.0f };

        for (int i = 0; i < n; ++i)
        {
            const float t = (n > 1) ? (float) i / (float) (n - 1) : 0.0f;
            GradientRamp::Stop s;
            s.position = t;
            s.colour = juce::Colour::fromHSV (h0 + (h1 - h0) * t,
                                              s0 + (s1 - s0) * t,
                                              v0 + (v1 - v0) * t,
                                              a0 + (a1 - a0) * t);
            stops.push_back (s);
        }
    }

    ramp.stops = std::move (stops);
    ramp.sortAndClamp();
    ramp.interpMode = GradientRamp::InterpMode::soft;
    ramp.enabled = ramp.stops.size() >= 2;
    ++ramp.revision;
}

void ColourRampBank::randomizeRamps (const SharedColors& colours, const bool* targetEnabled, int maskCount)
{
    bool any = false;

    for (int ti = 0; ti < (int) Target::numTargets; ++ti)
    {
        if (targetEnabled != nullptr)
        {
            if (ti < maskCount && ! targetEnabled[ti])
                continue;
            if (ti >= maskCount)
                continue;
        }

        const bool varyAlpha = (ti == (int) Target::spectrumFill
                                || ti == (int) Target::spectrumPreFill
                                || ti == (int) Target::spectrumHoldFill
                                || ti == (int) Target::eqSumFill
                                || ti == (int) Target::eqBandFill);
        randomizeRamp (ramps[ti], colours, varyAlpha);
        any = true;
    }

    if (! any)
        return;

    sanitizeMapModes();
    save();
    sendChangeMessage();
}

void ColourRampBank::disableAllCustomRamps()
{
    for (int ti = 0; ti < (int) Target::numTargets; ++ti)
    {
        ramps[ti].enabled = false;
        ++ramps[ti].revision;
    }
    save();
    sendChangeMessage();
}

void ColourRampBank::disableCustomRamp (Target t)
{
    auto& ramp = ramps[(int) clampTarget ((int) t)];
    if (! ramp.enabled)
        return;

    ramp.enabled = false;
    ++ramp.revision;
    save();
    sendChangeMessage();
}

juce::File ColourRampBank::getStoreFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("GhovlzDyn")
        .getChildFile ("colour_ramps.xml");
}

juce::ValueTree ColourRampBank::toValueTree() const
{
    juce::ValueTree tree ("ColourRamps");
    tree.setProperty ("schemaVersion", 2, nullptr);
    tree.setProperty ("activeTarget", (int) activeTarget, nullptr);
    tree.appendChild (ramps[(int) Target::fftBars].toValueTree ("FftBars"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrogram].toValueTree ("Spectrogram"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrogram3D].toValueTree ("Spectrogram3D"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumFill].toValueTree ("SpectrumFill"), nullptr);
    tree.appendChild (ramps[(int) Target::oscilloscope].toValueTree ("Oscilloscope"), nullptr);
    tree.appendChild (ramps[(int) Target::goniometer].toValueTree ("Goniometer"), nullptr);
    tree.appendChild (ramps[(int) Target::stereogram].toValueTree ("Stereogram"), nullptr);
    tree.appendChild (ramps[(int) Target::histogram].toValueTree ("Histogram"), nullptr);
    tree.appendChild (ramps[(int) Target::meterPeak].toValueTree ("MeterPeak"), nullptr);
    tree.appendChild (ramps[(int) Target::meterRms].toValueTree ("MeterRms"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumCurve].toValueTree ("SpectrumCurve"), nullptr);
    tree.appendChild (ramps[(int) Target::eqCurve].toValueTree ("EqCurve"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumPreFill].toValueTree ("SpectrumPreFill"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumPreCurve].toValueTree ("SpectrumPreCurve"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumHoldFill].toValueTree ("SpectrumHoldFill"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumHoldCurve].toValueTree ("SpectrumHoldCurve"), nullptr);
    tree.appendChild (ramps[(int) Target::eqSumFill].toValueTree ("EqSumFill"), nullptr);
    tree.appendChild (ramps[(int) Target::eqBandCurve].toValueTree ("EqBandCurve"), nullptr);
    tree.appendChild (ramps[(int) Target::eqBandFill].toValueTree ("EqBandFill"), nullptr);
    return tree;
}

void ColourRampBank::applyFromValueTree (const juce::ValueTree& tree, bool forceCustomRampsOff)
{
    if (! tree.hasType ("ColourRamps"))
        return;

    {
        const int stored = (int) tree.getProperty ("activeTarget", -1);
        activeTarget = (stored < 0) ? -1 : (int) clampTarget (stored);
    }

    auto loadOne = [&] (Target t, const juce::Identifier& id)
    {
        auto child = tree.getChildWithName (id);
        if (child.isValid())
            ramps[(int) t] = GradientRamp::fromValueTree (child);
    };

    loadOne (Target::fftBars, "FftBars");
    loadOne (Target::spectrogram, "Spectrogram");
    loadOne (Target::spectrogram3D, "Spectrogram3D");
    loadOne (Target::spectrumFill, "SpectrumFill");
    loadOne (Target::oscilloscope, "Oscilloscope");
    loadOne (Target::goniometer, "Goniometer");
    loadOne (Target::stereogram, "Stereogram");
    loadOne (Target::histogram, "Histogram");
    loadOne (Target::meterPeak, "MeterPeak");
    loadOne (Target::meterRms, "MeterRms");
    loadOne (Target::spectrumCurve, "SpectrumCurve");
    loadOne (Target::eqCurve, "EqCurve");
    loadOne (Target::spectrumPreFill, "SpectrumPreFill");
    loadOne (Target::spectrumPreCurve, "SpectrumPreCurve");
    loadOne (Target::spectrumHoldFill, "SpectrumHoldFill");
    loadOne (Target::spectrumHoldCurve, "SpectrumHoldCurve");
    loadOne (Target::eqSumFill, "EqSumFill");
    loadOne (Target::eqBandCurve, "EqBandCurve");
    loadOne (Target::eqBandFill, "EqBandFill");
    sanitizeMapModes();

    if (! forceCustomRampsOff)
        return;

    // Disk load: keep stops, but never silently override built-in colour schemes
    // (FFT/Spec/…). Spectrum fill / curve + EQ curve + level meters keep Use as
    // loaded (constructor default is on when the child was missing).
    for (int ti = 0; ti < (int) Target::numTargets; ++ti)
    {
        if (ti == (int) Target::meterPeak || ti == (int) Target::meterRms
            || ti == (int) Target::spectrumFill
            || ti == (int) Target::spectrumCurve
            || ti == (int) Target::eqCurve
            || ti == (int) Target::spectrumPreFill
            || ti == (int) Target::spectrumPreCurve
            || ti == (int) Target::spectrumHoldFill
            || ti == (int) Target::spectrumHoldCurve
            || ti == (int) Target::eqSumFill
            || ti == (int) Target::eqBandCurve
            || ti == (int) Target::eqBandFill)
            continue;

        if (ramps[ti].enabled)
        {
            ramps[ti].enabled = false;
            ++ramps[ti].revision;
        }
    }
}

void ColourRampBank::load()
{
    const auto file = getStoreFile();
    if (! file.existsAsFile())
        return;

    if (auto xml = juce::parseXML (file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType ("ColourRamps"))
            return;

        bool anyWereEnabled = false;
        for (auto id : { juce::Identifier ("FftBars"), juce::Identifier ("Spectrogram"), juce::Identifier ("Spectrogram3D"),
                         juce::Identifier ("SpectrumFill"), juce::Identifier ("Oscilloscope"), juce::Identifier ("Goniometer"),
                         juce::Identifier ("Stereogram"), juce::Identifier ("Histogram"),
                         juce::Identifier ("MeterPeak"), juce::Identifier ("MeterRms"),
                         juce::Identifier ("SpectrumCurve"), juce::Identifier ("EqCurve"),
                         juce::Identifier ("SpectrumPreFill"), juce::Identifier ("SpectrumPreCurve"),
                         juce::Identifier ("SpectrumHoldFill"), juce::Identifier ("SpectrumHoldCurve"),
                         juce::Identifier ("EqSumFill"), juce::Identifier ("EqBandCurve"),
                         juce::Identifier ("EqBandFill") })
        {
            auto child = tree.getChildWithName (id);
            if (child.isValid() && (bool) child.getProperty ("enabled", false))
                anyWereEnabled = true;
        }

        applyFromValueTree (tree, true);

        const int schema = (int) tree.getProperty ("schemaVersion", 0);
        if (schema < 2 || anyWereEnabled)
            save();
    }
}

void ColourRampBank::sanitizeMapModes()
{
    // FFT / Spec / Stereogram / Histogram = intensity; Osc = amp+freq; Fill = spatial; Gon = diversion.
    for (auto t : { Target::fftBars, Target::spectrogram, Target::spectrogram3D,
                    Target::stereogram, Target::histogram, Target::meterPeak, Target::meterRms })
    {
        auto& r = ramps[(int) t];
        if (! r.isIntensityMap())
            r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
    }

    auto& osc = ramps[(int) Target::oscilloscope];
    if (! osc.isOscilloscopeMap())
        osc.mapMode = GradientRamp::MapMode::intensityLowToHigh;

    for (auto t : { Target::spectrumFill, Target::spectrumPreFill, Target::spectrumHoldFill,
                    Target::eqSumFill, Target::eqBandFill })
    {
        auto& r = ramps[(int) t];
        if (! r.isSpatialMap())
            r.mapMode = GradientRamp::MapMode::bottomToTop;
    }

    for (auto t : { Target::spectrumCurve, Target::spectrumPreCurve, Target::spectrumHoldCurve,
                    Target::eqCurve, Target::eqBandCurve })
    {
        auto& r = ramps[(int) t];
        if (! r.isSpatialMap())
            r.mapMode = GradientRamp::MapMode::leftToRight;
    }

    auto& gon = ramps[(int) Target::goniometer];
    // Migrate legacy intensity maps → Loudness; otherwise clamp to a goniometer mode.
    if (gon.mapMode == GradientRamp::MapMode::intensityLowToHigh
        || gon.mapMode == GradientRamp::MapMode::intensityHighToLow)
        gon.mapMode = GradientRamp::MapMode::gonLoudness;
    else if (! gon.isGoniometerMap())
        gon.mapMode = GradientRamp::MapMode::gonLoudness;
}

void ColourRampBank::save() const
{
    if (auto xml = toValueTree().createXml())
    {
        const auto file = getStoreFile();
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}
