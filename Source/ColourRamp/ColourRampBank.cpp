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
        r.mapMode = GradientRamp::MapMode::bottomToTop;
        r.stops = {
            { 0.0f, juce::Colour::fromRGBA (58, 42, 32, 180) },
            { 1.0f, juce::Colour::fromRGBA (187, 219, 132, 200) }
        };
        ramps[(int) Target::spectrumFill] = std::move (r);
    }

    load();
    sanitizeMapModes();
}

juce::String ColourRampBank::targetName (Target t)
{
    switch (t)
    {
        case Target::fftBars:      return "FFT Bars";
        case Target::spectrogram:  return "Spectrogram";
        case Target::spectrumFill: return "Spectrum Fill";
        default:                   return "Ramp";
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

void ColourRampBank::randomizeRamps (const SharedColors& colours, const bool* targetEnabled3)
{
    auto& rng = juce::Random::getSystemRandom();
    bool any = false;

    for (int ti = 0; ti < (int) Target::numTargets; ++ti)
    {
        if (targetEnabled3 != nullptr && ! targetEnabled3[ti])
            continue;

        auto& ramp = ramps[ti];
        const int n = rng.nextInt ({ 2, 11 }); // 2..10 inclusive
        std::vector<GradientRamp::Stop> stops;
        stops.reserve ((size_t) n);

        // Always pin endpoints; scatter interiors.
        for (int i = 0; i < n; ++i)
        {
            GradientRamp::Stop s;
            if (i == 0)           s.position = 0.0f;
            else if (i == n - 1)  s.position = 1.0f;
            else                  s.position = rng.nextFloat();

            // Spectrum fill keeps some alpha variety for overlays.
            const float alpha = (ti == (int) Target::spectrumFill)
                                    ? (0.35f + rng.nextFloat() * 0.55f)
                                    : 1.0f;
            s.colour = colours.randomColourInLimits (alpha);
            stops.push_back (s);
        }

        ramp.stops = std::move (stops);
        ramp.sortAndClamp();
        // Soft blend pairs well with sparse random poles.
        ramp.interpMode = GradientRamp::InterpMode::soft;
        ramp.enabled = ramp.stops.size() >= 2;
        ++ramp.revision;
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

juce::File ColourRampBank::getStoreFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("ParametricEq")
        .getChildFile ("colour_ramps.xml");
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
        loadOne (Target::spectrumFill, "SpectrumFill");
        sanitizeMapModes();

        // schemaVersion < 1: older builds force-enabled path samples on disk, which
        // silently replaced Magma/Inferno on every reload. Keep stop colours, but
        // start with Use off so built-in schemes are the default again.
        const int schema = (int) tree.getProperty ("schemaVersion", 0);
        if (schema < 1)
        {
            for (int ti = 0; ti < (int) Target::numTargets; ++ti)
            {
                ramps[ti].enabled = false;
                ++ramps[ti].revision;
            }
            save();
        }
    }
}

void ColourRampBank::sanitizeMapModes()
{
    // FFT / Spec = intensity axis; Spectrum Fill = spatial axis.
    for (auto t : { Target::fftBars, Target::spectrogram })
    {
        auto& r = ramps[(int) t];
        if (! r.isIntensityMap())
            r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
    }

    auto& fill = ramps[(int) Target::spectrumFill];
    if (! fill.isSpatialMap())
        fill.mapMode = GradientRamp::MapMode::bottomToTop;
}

void ColourRampBank::save() const
{
    juce::ValueTree tree ("ColourRamps");
    tree.setProperty ("schemaVersion", 1, nullptr);
    tree.setProperty ("activeTarget", (int) activeTarget, nullptr);
    tree.appendChild (ramps[(int) Target::fftBars].toValueTree ("FftBars"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrogram].toValueTree ("Spectrogram"), nullptr);
    tree.appendChild (ramps[(int) Target::spectrumFill].toValueTree ("SpectrumFill"), nullptr);

    if (auto xml = tree.createXml())
    {
        const auto file = getStoreFile();
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}
