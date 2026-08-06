#include "Theme.h"
#include "ThemeColorRegistry.h"
#include <cstdint>

Theme::Theme()
{
    created = juce::Time::getCurrentTime();
    modified = created;
}

Theme::Theme (const SharedColors& colorsIn)
    : colors (colorsIn)
{
}

Theme::Theme (const juce::String& presetName,
              const juce::Time& createdTime,
              const juce::Time& modifiedTime,
              const SharedColors& colorsIn)
    : colors (colorsIn),
      created (createdTime),
      modified (modifiedTime),
      name (presetName)
{
}

juce::XmlElement* Theme::toXml() const
{
    auto xml = new juce::XmlElement ("Theme");

    auto toHexString = [] (const juce::Colour& colour) -> juce::String
    {
        if (colour.getAlpha() == 255)
            return juce::String::formatted ("%02X%02X%02X",
                                            colour.getRed(), colour.getGreen(), colour.getBlue());
        return juce::String::formatted ("%02X%02X%02X%02X",
                                        colour.getRed(), colour.getGreen(), colour.getBlue(), colour.getAlpha());
    };

    const auto* entries = ThemeColorRegistry::getEntries();
    const int n = ThemeColorRegistry::getNumEntries();
    juce::StringArray attrNames;
    for (int i = 0; i < n; ++i)
    {
        xml->setAttribute (entries[i].xmlAttr, toHexString (colors.colourAt (i)));
        attrNames.add (entries[i].xmlAttr);
    }

    // Keep writing legacy attrs so older builds can still read path/background colours.
    xml->setAttribute ("GonLine", toHexString (colors.gonLine));
    xml->setAttribute ("GonGlow", toHexString (colors.gonGlow));
    xml->setAttribute ("GonBackground", toHexString (colors.gonBackground));
    xml->setAttribute ("GonBackground2", toHexString (colors.gonBackground2));

    xml->setAttribute ("ColorAttributeNames", attrNames.joinIntoString (","));
    xml->setAttribute ("Name", name);
    xml->setAttribute ("Created", created.toISO8601 (true));
    xml->setAttribute ("Modified", modified.toISO8601 (true));

    if (pluginState.isValid())
    {
        if (auto stateXml = pluginState.createXml())
        {
            auto* wrapper = new juce::XmlElement ("STATE");
            wrapper->addChildElement (stateXml.release());
            xml->addChildElement (wrapper);
        }
    }

    if (globalUi.isValid() && globalUi.hasType ("GlobalUi"))
    {
        if (auto bundleXml = globalUi.createXml())
            xml->addChildElement (bundleXml.release());
    }

    return xml;
}

void Theme::fromXml (const juce::XmlElement& xmlElement)
{
    auto ensureAlphaAndCheckColor = [] (const juce::String& hexColor, juce::Colour fallback) -> juce::Colour
    {
        if (hexColor.isEmpty())
            return fallback;

        juce::String colorString = hexColor;
        if (colorString.length() == 6)
            colorString = "ff" + colorString;

        if (colorString.equalsIgnoreCase ("0000FF00"))
            return fallback;

        return juce::Colour::fromString (colorString);
    };

    // Start from current defaults so missing (new) attributes keep factory colours.
    colors = SharedColors{};

    const auto* entries = ThemeColorRegistry::getEntries();
    const int n = ThemeColorRegistry::getNumEntries();
    for (int i = 0; i < n; ++i)
    {
        const auto attr = xmlElement.getStringAttribute (entries[i].xmlAttr);
        colors.colourAt (i) = ensureAlphaAndCheckColor (attr, colors.colourAt (i));
    }

    // Legacy themes may still store separate Gon* — prefer Osc* when present,
    // otherwise adopt the old gon values, then force them to stay unified.
    {
        const auto oscLineAttr = xmlElement.getStringAttribute ("OscLine");
        const auto gonLineAttr = xmlElement.getStringAttribute ("GonLine");
        if (oscLineAttr.isEmpty() && gonLineAttr.isNotEmpty())
            colors.oscLine = ensureAlphaAndCheckColor (gonLineAttr, colors.oscLine);

        const auto oscGlowAttr = xmlElement.getStringAttribute ("OscGlow");
        const auto gonGlowAttr = xmlElement.getStringAttribute ("GonGlow");
        if (oscGlowAttr.isEmpty() && gonGlowAttr.isNotEmpty())
            colors.oscGlow = ensureAlphaAndCheckColor (gonGlowAttr, colors.oscGlow);

        const auto oscBgAttr = xmlElement.getStringAttribute ("OscBackground");
        const auto gonBgAttr = xmlElement.getStringAttribute ("GonBackground");
        if (oscBgAttr.isEmpty() && gonBgAttr.isNotEmpty())
            colors.oscBackground = ensureAlphaAndCheckColor (gonBgAttr, colors.oscBackground);

        const auto oscBg2Attr = xmlElement.getStringAttribute ("OscBackground2");
        const auto gonBg2Attr = xmlElement.getStringAttribute ("GonBackground2");
        if (oscBg2Attr.isEmpty() && gonBg2Attr.isNotEmpty())
            colors.oscBackground2 = ensureAlphaAndCheckColor (gonBg2Attr, colors.oscBackground2);

        colors.syncSharedScopePathColours();
        colors.syncFaceplateModScheme();
    }

    name = xmlElement.getStringAttribute ("Name", "Unnamed");

    juce::String createdString = xmlElement.getStringAttribute ("Created");
    created = createdString.isEmpty() ? juce::Time::getCurrentTime()
                                      : juce::Time::fromISO8601 (createdString);

    juce::String lastModifiedString = xmlElement.getStringAttribute ("Modified");
    modified = lastModifiedString.isEmpty() ? juce::Time::getCurrentTime()
                                            : juce::Time::fromISO8601 (lastModifiedString);

    pluginState = {};
    if (auto* stateWrapper = xmlElement.getChildByName ("STATE"))
        if (auto* stateChild = stateWrapper->getFirstChildElement())
            pluginState = juce::ValueTree::fromXml (*stateChild);

    globalUi = {};
    if (auto* globalEl = xmlElement.getChildByName ("GlobalUi"))
        globalUi = juce::ValueTree::fromXml (*globalEl);
}

void Theme::savePresetsToXML (juce::Array<Theme>& themes,
                              const juce::StringArray& presetNames,
                              const juce::Array<juce::Time>& themeCreatedTimes,
                              const juce::Array<juce::Time>& themeModifiedTimes)
{
    juce::File presetDirectory = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                     .getChildFile ("Decksounds")
                                     .getChildFile ("ParametricEq")
                                     .getChildFile ("Themes");

    if (! presetDirectory.exists())
        presetDirectory.createDirectory();

    juce::File presetFile = presetDirectory.getChildFile ("presets.xml");
    juce::XmlElement xml ("Presets");

    const int count = juce::jmin (themes.size(), presetNames.size());
    for (int i = 0; i < count; ++i)
    {
        auto themeElement = themes[i].toXml();
        themeElement->setAttribute ("name", presetNames[i]);

        const auto createdTime = (i < themeCreatedTimes.size()) ? themeCreatedTimes[i] : themes[i].getCreated();
        const auto modifiedTime = (i < themeModifiedTimes.size()) ? themeModifiedTimes[i] : themes[i].getModified();

        themeElement->setAttribute ("Created", createdTime.toISO8601 (true));
        themeElement->setAttribute ("Modified", modifiedTime.toISO8601 (true));
        xml.addChildElement (themeElement);
    }

    xml.writeToFile (presetFile, {}, "UTF-8", 60);
}

juce::StringArray Theme::getColorAttributeNames() const
{
    juce::StringArray names;
    const auto* entries = ThemeColorRegistry::getEntries();
    const int n = ThemeColorRegistry::getNumEntries();
    for (int i = 0; i < n; ++i)
        names.add (entries[i].xmlAttr);
    return names;
}
