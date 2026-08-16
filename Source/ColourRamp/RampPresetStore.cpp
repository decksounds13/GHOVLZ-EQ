#include "RampPresetStore.h"

namespace
{
    using Stop = GradientRamp::Stop;

    GradientRamp makeRamp (std::initializer_list<Stop> stops,
                           GradientRamp::InterpMode interp = GradientRamp::InterpMode::soft)
    {
        GradientRamp r;
        r.stops.assign (stops);
        r.sortAndClamp();
        r.interpMode = interp;
        r.enabled = true;
        return r;
    }

    juce::Colour rgb (int r, int g, int b)
    {
        return juce::Colour::fromRGB ((juce::uint8) r, (juce::uint8) g, (juce::uint8) b);
    }

    void addFactory (juce::Array<RampPresetStore::Preset>& out,
                     const char* name,
                     GradientRamp ramp)
    {
        RampPresetStore::Preset p;
        p.name = name;
        p.ramp = std::move (ramp);
        p.isFactory = true;
        out.add (std::move (p));
    }
}

RampPresetStore::RampPresetStore()
{
    load();
}

juce::File RampPresetStore::getStoreFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("GhovlzDyn")
        .getChildFile ("ramp_presets.xml");
}

void RampPresetStore::seedFactoryPresets()
{
    addFactory (presets, "Magma", makeRamp ({
        { 0.00f, rgb (0, 0, 4) }, { 0.25f, rgb (80, 18, 123) },
        { 0.50f, rgb (182, 54, 121) }, { 0.75f, rgb (251, 135, 97) },
        { 1.00f, rgb (252, 253, 191) } }));
    addFactory (presets, "Inferno", makeRamp ({
        { 0.00f, rgb (0, 0, 4) }, { 0.25f, rgb (87, 16, 110) },
        { 0.50f, rgb (188, 55, 84) }, { 0.75f, rgb (249, 142, 9) },
        { 1.00f, rgb (252, 255, 164) } }));
    addFactory (presets, "Viridis", makeRamp ({
        { 0.00f, rgb (68, 1, 84) }, { 0.25f, rgb (59, 82, 139) },
        { 0.50f, rgb (33, 145, 140) }, { 0.75f, rgb (94, 201, 98) },
        { 1.00f, rgb (253, 231, 37) } }));
    addFactory (presets, "Plasma", makeRamp ({
        { 0.00f, rgb (13, 8, 135) }, { 0.25f, rgb (126, 3, 168) },
        { 0.50f, rgb (204, 71, 120) }, { 0.75f, rgb (248, 149, 64) },
        { 1.00f, rgb (240, 249, 33) } }));
    addFactory (presets, "Ice", makeRamp ({
        { 0.00f, rgb (0, 0, 20) }, { 0.35f, rgb (10, 40, 100) },
        { 0.65f, rgb (40, 140, 200) }, { 1.00f, rgb (220, 245, 255) } }));
    addFactory (presets, "Heat", makeRamp ({
        { 0.00f, rgb (0, 0, 0) }, { 0.30f, rgb (120, 0, 0) },
        { 0.55f, rgb (220, 60, 0) }, { 0.80f, rgb (255, 200, 40) },
        { 1.00f, rgb (255, 255, 230) } }));
    addFactory (presets, "Greyscale", makeRamp ({
        { 0.00f, rgb (0, 0, 0) }, { 0.50f, rgb (120, 120, 120) },
        { 1.00f, rgb (245, 245, 245) } }, GradientRamp::InterpMode::hard));
    addFactory (presets, "Sunset", makeRamp ({
        { 0.00f, rgb (20, 10, 40) }, { 0.30f, rgb (120, 40, 90) },
        { 0.55f, rgb (220, 90, 50) }, { 0.80f, rgb (255, 180, 80) },
        { 1.00f, rgb (255, 240, 200) } }));
    addFactory (presets, "Golden Hour", makeRamp ({
        { 0.00f, rgb (30, 18, 40) }, { 0.40f, rgb (160, 70, 40) },
        { 0.70f, rgb (230, 150, 60) }, { 1.00f, rgb (255, 230, 160) } }));
    addFactory (presets, "Ocean", makeRamp ({
        { 0.00f, rgb (0, 10, 30) }, { 0.35f, rgb (0, 60, 110) },
        { 0.65f, rgb (20, 140, 160) }, { 1.00f, rgb (180, 230, 220) } }));
    addFactory (presets, "Forest", makeRamp ({
        { 0.00f, rgb (5, 15, 5) }, { 0.35f, rgb (20, 60, 25) },
        { 0.65f, rgb (60, 130, 50) }, { 1.00f, rgb (200, 230, 120) } }));
    addFactory (presets, "Twilight", makeRamp ({
        { 0.00f, rgb (8, 6, 30) }, { 0.40f, rgb (50, 30, 100) },
        { 0.70f, rgb (140, 70, 140) }, { 1.00f, rgb (255, 180, 160) } }));
    addFactory (presets, "Aurora", makeRamp ({
        { 0.00f, rgb (5, 10, 30) }, { 0.30f, rgb (20, 80, 90) },
        { 0.55f, rgb (40, 180, 120) }, { 0.80f, rgb (160, 100, 200) },
        { 1.00f, rgb (240, 220, 255) } }));
    addFactory (presets, "Copper", makeRamp ({
        { 0.00f, rgb (15, 8, 5) }, { 0.40f, rgb (120, 50, 25) },
        { 0.70f, rgb (200, 110, 60) }, { 1.00f, rgb (255, 210, 160) } }));
    addFactory (presets, "Amber Glow", makeRamp ({
        { 0.00f, rgb (10, 5, 0) }, { 0.45f, rgb (160, 80, 10) },
        { 0.75f, rgb (240, 160, 40) }, { 1.00f, rgb (255, 240, 180) } }));
    addFactory (presets, "Blood Orange", makeRamp ({
        { 0.00f, rgb (20, 0, 10) }, { 0.40f, rgb (140, 20, 30) },
        { 0.70f, rgb (230, 80, 20) }, { 1.00f, rgb (255, 200, 120) } }));
    addFactory (presets, "Neon Pulse", makeRamp ({
        { 0.00f, rgb (5, 0, 20) }, { 0.35f, rgb (120, 0, 180) },
        { 0.60f, rgb (0, 200, 220) }, { 0.85f, rgb (255, 40, 160) },
        { 1.00f, rgb (255, 255, 200) } }, GradientRamp::InterpMode::hard));
    addFactory (presets, "Violet Storm", makeRamp ({
        { 0.00f, rgb (10, 0, 25) }, { 0.40f, rgb (70, 20, 120) },
        { 0.70f, rgb (150, 80, 200) }, { 1.00f, rgb (230, 200, 255) } }));
    addFactory (presets, "Cyan Fade", makeRamp ({
        { 0.00f, rgb (0, 15, 25) }, { 0.45f, rgb (0, 120, 140) },
        { 0.75f, rgb (80, 210, 220) }, { 1.00f, rgb (220, 255, 255) } }));
    addFactory (presets, "Mint Night", makeRamp ({
        { 0.00f, rgb (5, 15, 20) }, { 0.40f, rgb (20, 70, 70) },
        { 0.70f, rgb (80, 180, 140) }, { 1.00f, rgb (210, 255, 220) } }));

    // Level-meter friendly factory looks.
    addFactory (presets, "Blue Orange", makeRamp ({
        { 0.00f, rgb (15, 40, 160) }, { 0.45f, rgb (40, 120, 220) },
        { 0.72f, rgb (240, 150, 40) }, { 1.00f, rgb (255, 95, 20) } }));
    addFactory (presets, "Meter VU", makeRamp ({
        { 0.00f, rgb (18, 130, 50) }, { 0.50f, rgb (200, 195, 35) },
        { 0.75f, rgb (235, 125, 25) }, { 0.90f, rgb (230, 55, 30) },
        { 1.00f, rgb (200, 20, 25) } }));

    // Full-spectrum rainbow: start at blue, then every hue at max saturation / brightness.
    {
        GradientRamp rainbow;
        constexpr int kStops = 13; // blue → … → blue (full turn)
        for (int i = 0; i < kStops; ++i)
        {
            const float t = (float) i / (float) (kStops - 1);
            // JUCE hue: 0 = red; blue ≈ 0.666. Walk a full turn from blue.
            const float hue = std::fmod (2.0f / 3.0f + t, 1.0f);
            rainbow.stops.push_back ({ t, juce::Colour::fromHSV (hue, 1.0f, 1.0f, 1.0f) });
        }
        rainbow.sortAndClamp();
        rainbow.interpMode = GradientRamp::InterpMode::soft;
        rainbow.enabled = true;
        addFactory (presets, "Rainbow", std::move (rainbow));
    }
}

juce::String RampPresetStore::resolvedUserName (juce::String name) const
{
    name = name.trim();
    if (name.isEmpty())
        return {};

    for (const auto& p : presets)
        if (p.isFactory && p.name.equalsIgnoreCase (name))
            return name + " User";

    return name;
}

bool RampPresetStore::containsUserName (juce::String name) const
{
    name = resolvedUserName (std::move (name));
    if (name.isEmpty())
        return false;

    for (const auto& p : presets)
        if (! p.isFactory && p.name.equalsIgnoreCase (name))
            return true;

    return false;
}

bool RampPresetStore::savePreset (juce::String name, const GradientRamp& ramp)
{
    name = resolvedUserName (std::move (name));
    if (name.isEmpty() || ramp.stops.size() < 2)
        return false;

    for (auto& p : presets)
    {
        if (! p.isFactory && p.name.equalsIgnoreCase (name))
        {
            p.ramp = ramp;
            p.ramp.enabled = true;
            ++p.ramp.revision;
            save();
            sendChangeMessage();
            return true;
        }
    }

    Preset p;
    p.name = name;
    p.ramp = ramp;
    p.ramp.enabled = true;
    p.isFactory = false;
    ++p.ramp.revision;
    presets.add (std::move (p));
    save();
    sendChangeMessage();
    return true;
}

bool RampPresetStore::applyPreset (int index, GradientRamp& dest) const
{
    if (! juce::isPositiveAndBelow (index, presets.size()))
        return false;

    const auto keepMode = dest.mapMode;
    dest = presets.getReference (index).ramp;
    dest.mapMode = keepMode;
    dest.enabled = dest.stops.size() >= 2;
    ++dest.revision;
    return true;
}

bool RampPresetStore::renamePreset (int index, juce::String newName)
{
    if (! juce::isPositiveAndBelow (index, presets.size()))
        return false;
    if (presets.getReference (index).isFactory)
        return false;
    newName = newName.trim();
    if (newName.isEmpty())
        return false;
    presets.getReference (index).name = newName;
    save();
    sendChangeMessage();
    return true;
}

bool RampPresetStore::deletePreset (int index)
{
    if (! juce::isPositiveAndBelow (index, presets.size()))
        return false;
    if (presets.getReference (index).isFactory)
        return false;
    presets.remove (index);
    save();
    sendChangeMessage();
    return true;
}

void RampPresetStore::load()
{
    presets.clear();
    seedFactoryPresets();

    const auto file = getStoreFile();
    if (! file.existsAsFile())
        return;

    if (auto xml = juce::parseXML (file))
    {
        auto tree = juce::ValueTree::fromXml (*xml);
        if (! tree.hasType ("RampPresets"))
            return;

        for (int i = 0; i < tree.getNumChildren(); ++i)
        {
            auto child = tree.getChild (i);
            if (! child.hasType ("Preset"))
                continue;

            Preset p;
            p.name = child.getProperty ("name", "Ramp").toString();
            p.ramp = GradientRamp::fromValueTree (child.getChildWithName ("Ramp"));
            p.isFactory = false;

            bool clashesFactory = false;
            for (const auto& existing : presets)
                if (existing.isFactory && existing.name.equalsIgnoreCase (p.name))
                {
                    clashesFactory = true;
                    break;
                }

            if (clashesFactory || p.ramp.stops.size() < 2)
                continue;

            presets.add (std::move (p));
        }
    }
}

void RampPresetStore::save() const
{
    juce::ValueTree tree ("RampPresets");
    for (const auto& p : presets)
    {
        if (p.isFactory)
            continue;

        juce::ValueTree node ("Preset");
        node.setProperty ("name", p.name, nullptr);
        node.appendChild (p.ramp.toValueTree ("Ramp"), nullptr);
        tree.appendChild (node, nullptr);
    }

    if (auto xml = tree.createXml())
    {
        const auto file = getStoreFile();
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}
