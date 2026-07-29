#include "EqPresetStore.h"
#include "EqProcessor.h"

EqPresetStore::EqPresetStore (EqProcessor& processorToUse)
    : processor (processorToUse)
{
    loadFromXml();
    ensureDefault();
}

juce::File EqPresetStore::getPresetFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Decksounds")
                   .getChildFile ("ParametricEq")
                   .getChildFile ("Presets");
    if (! dir.exists())
        dir.createDirectory();
    return dir.getChildFile ("eq_presets.xml");
}

juce::String EqPresetStore::getName (int index) const
{
    if (index < 0 || index >= names.size())
        return {};
    return names[index];
}

juce::String EqPresetStore::getSelectedName() const
{
    return getName (selectedIndex);
}

juce::ValueTree EqPresetStore::captureState() const
{
    juce::MemoryBlock block;
    processor.getStateInformation (block);
    if (auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()))
        return juce::ValueTree::fromXml (*xml);
    return {};
}

void EqPresetStore::applyState (const juce::ValueTree& state)
{
    if (! state.isValid())
        return;

    if (auto xml = state.createXml())
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (*xml, block);
        processor.setStateInformation (block.getData(), (int) block.getSize());
    }
}

void EqPresetStore::ensureDefault()
{
    if (names.isEmpty())
    {
        names.add ("Default");
        states.add (captureState());
        selectedIndex = 0;
        persistToXml();
    }

    if (selectedIndex < 0 || selectedIndex >= names.size())
        selectedIndex = 0;
}

void EqPresetStore::apply (int index)
{
    if (index < 0 || index >= states.size())
        return;

    selectedIndex = index;
    applyState (states.getReference (index));

    if (onChanged)
        onChanged();
}

void EqPresetStore::cycle (int delta)
{
    const int n = names.size();
    if (n <= 0 || delta == 0)
        return;

    int index = selectedIndex;
    if (index < 0)
        index = 0;
    else
        index = (index + delta + n) % n;

    apply (index);
}

void EqPresetStore::saveOrUpdateWithName (const juce::String& name)
{
    auto trimmed = name.trim();
    if (trimmed.isEmpty())
        trimmed = "Preset";

    // Never overwrite a locked Default entry in-place — create/update another slot.
    if (trimmed.equalsIgnoreCase ("Default") && selectedIndex == 0 && names[0] == "Default")
        trimmed = "Preset";

    const auto state = captureState();

    for (int i = 0; i < names.size(); ++i)
    {
        if (names[i].equalsIgnoreCase (trimmed))
        {
            states.getReference (i) = state;
            selectedIndex = i;
            persistToXml();
            if (onChanged)
                onChanged();
            return;
        }
    }

    if (selectedIndex > 0 && selectedIndex < names.size())
    {
        names.set (selectedIndex, trimmed);
        states.getReference (selectedIndex) = state;
    }
    else
    {
        names.add (trimmed);
        states.add (state);
        selectedIndex = names.size() - 1;
    }

    persistToXml();
    if (onChanged)
        onChanged();
}

void EqPresetStore::renameSelected (const juce::String& newName)
{
    auto trimmed = newName.trim();
    if (trimmed.isEmpty() || selectedIndex < 0 || selectedIndex >= names.size())
        return;

    if (selectedIndex == 0 && names[0] == "Default")
        return; // keep Default name stable

    names.set (selectedIndex, trimmed);
    persistToXml();
    if (onChanged)
        onChanged();
}

void EqPresetStore::loadFromXml()
{
    names.clear();
    states.clear();
    selectedIndex = 0;

    const auto file = getPresetFile();
    if (! file.existsAsFile())
        return;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        selectedIndex = xml->getIntAttribute ("selected", 0);
        for (auto* child : xml->getChildIterator())
        {
            if (child->hasTagName ("Preset"))
            {
                names.add (child->getStringAttribute ("name", "Preset"));
                juce::ValueTree state;
                if (auto* stateXml = child->getChildByName ("STATE"))
                    if (auto* inner = stateXml->getFirstChildElement())
                        state = juce::ValueTree::fromXml (*inner);
                states.add (state);
            }
        }
    }
}

void EqPresetStore::persistToXml() const
{
    juce::XmlElement root ("EqPresets");
    root.setAttribute ("selected", selectedIndex);

    const int n = juce::jmin (names.size(), states.size());
    for (int i = 0; i < n; ++i)
    {
        auto* preset = root.createNewChildElement ("Preset");
        preset->setAttribute ("name", names[i]);
        if (states[i].isValid())
        {
            if (auto stateXml = states[i].createXml())
            {
                auto* wrap = new juce::XmlElement ("STATE");
                wrap->addChildElement (stateXml.release());
                preset->addChildElement (wrap);
            }
        }
    }

    getPresetFile().replaceWithText (root.toString());
}
