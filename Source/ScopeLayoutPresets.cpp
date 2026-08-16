#include "ScopeLayoutPresets.h"

namespace ScopeLayoutPresets
{
juce::File getStoreFile()
{
    return juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
        .getChildFile ("Decksounds")
        .getChildFile ("GhovlzDyn")
        .getChildFile ("scope_layouts.xml");
}

juce::String encodeFractions (const std::vector<float>& fracs)
{
    juce::StringArray parts;
    for (float f : fracs)
        parts.add (juce::String (f, 4));
    return parts.joinIntoString (",");
}

std::vector<float> decodeFractions (const juce::String& s, int expectedCount)
{
    std::vector<float> out;
    auto parts = juce::StringArray::fromTokens (s, ",", {});
    for (const auto& p : parts)
        out.push_back ((float) p.getDoubleValue());
    if (expectedCount > 0 && (int) out.size() != expectedCount)
        out.assign ((size_t) expectedCount, expectedCount > 0 ? 1.0f / (float) expectedCount : 1.0f);
    return out;
}

static ScopeLayoutPreset parseLayout (const juce::XmlElement& xml)
{
    ScopeLayoutPreset p;
    p.name = xml.getStringAttribute ("name", "Untitled");
    p.strip = xml.getBoolAttribute ("strip", false);
    p.modules = ScopeModules::orderFromString (xml.getStringAttribute ("modules",
                                                                       ScopeModules::orderToString (ScopeModules::defaultEnabledOrder())));
    p.stripHeightPx = xml.getIntAttribute ("stripHeightPx", 200);
    p.splitX = (float) xml.getDoubleAttribute ("splitX", 0.5);
    p.splitY = (float) xml.getDoubleAttribute ("splitY", 0.5);
    p.stripFractions = decodeFractions (xml.getStringAttribute ("fractions"), (int) p.modules.size());
    p.factoryId = xml.getStringAttribute ("factoryId");
    if (auto* pane = xml.getChildByName ("Pane"))
        p.viewportXml = pane->toString();

    if (auto* rampsXml = xml.getChildByName ("ColourRamps"))
        p.colourRamps = juce::ValueTree::fromXml (*rampsXml);

    return p;
}

static std::unique_ptr<juce::XmlElement> toXml (const ScopeLayoutPreset& p)
{
    auto xml = std::make_unique<juce::XmlElement> ("Layout");
    xml->setAttribute ("name", p.name);
    xml->setAttribute ("strip", p.strip);
    xml->setAttribute ("modules", ScopeModules::orderToString (p.modules));
    xml->setAttribute ("fractions", encodeFractions (p.stripFractions));
    xml->setAttribute ("stripHeightPx", p.stripHeightPx);
    xml->setAttribute ("splitX", (double) p.splitX);
    xml->setAttribute ("splitY", (double) p.splitY);
    if (p.factoryId.isNotEmpty())
        xml->setAttribute ("factoryId", p.factoryId);
    if (p.viewportXml.isNotEmpty())
        if (auto pane = juce::parseXML (p.viewportXml))
            if (pane->hasTagName ("Pane"))
                xml->addChildElement (pane.release());

    if (p.colourRamps.isValid() && p.colourRamps.hasType ("ColourRamps"))
        if (auto rampsXml = p.colourRamps.createXml())
            xml->addChildElement (rampsXml.release());

    return xml;
}

std::vector<ScopeLayoutPreset> loadAll()
{
    std::vector<ScopeLayoutPreset> out;
    const auto file = getStoreFile();
    if (! file.existsAsFile())
        return out;
    if (auto xml = juce::parseXML (file))
    {
        if (xml->hasTagName ("ScopeLayouts"))
        {
            for (auto* child : xml->getChildIterator())
                if (child->hasTagName ("Layout"))
                    out.push_back (parseLayout (*child));
        }
    }
    return out;
}

std::vector<ScopeLayoutPreset> loadForMode (bool stripMode)
{
    std::vector<ScopeLayoutPreset> out;
    for (auto& p : loadAll())
        if (p.strip == stripMode)
            out.push_back (std::move (p));
    return out;
}

bool containsName (const juce::String& name, bool stripMode)
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return false;

    for (const auto& p : loadForMode (stripMode))
        if (p.name.equalsIgnoreCase (trimmed))
            return true;

    return false;
}

bool savePreset (const ScopeLayoutPreset& preset)
{
    auto all = loadAll();
    bool replaced = false;
    for (auto& p : all)
    {
        if (p.strip == preset.strip && p.name.equalsIgnoreCase (preset.name))
        {
            p = preset;
            replaced = true;
            break;
        }
    }
    if (! replaced)
        all.push_back (preset);

    auto root = std::make_unique<juce::XmlElement> ("ScopeLayouts");
    for (const auto& p : all)
        root->addChildElement (toXml (p).release());

    auto file = getStoreFile();
    file.getParentDirectory().createDirectory();
    return root->writeTo (file);
}

bool deletePreset (const juce::String& name, bool stripMode)
{
    auto all = loadAll();
    const auto n = (int) all.size();
    all.erase (std::remove_if (all.begin(), all.end(),
                               [&] (const ScopeLayoutPreset& p)
                               { return p.name == name && p.strip == stripMode; }),
               all.end());
    if ((int) all.size() == n)
        return false;

    auto root = std::make_unique<juce::XmlElement> ("ScopeLayouts");
    for (const auto& p : all)
        root->addChildElement (toXml (p).release());
    return root->writeTo (getStoreFile());
}
} // namespace ScopeLayoutPresets
