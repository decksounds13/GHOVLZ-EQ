#include "EqPresetStore.h"
#include "EqProcessor.h"
#include "Dyn/DynParams.h"

EqPresetStore::EqPresetStore (EqProcessor& processorToUse)
    : processor (processorToUse)
{
    loadFromXml();
    ensureDefault();
    ensureFactoryOtt();
}

juce::File EqPresetStore::getPresetFile()
{
    auto dir = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                   .getChildFile ("Decksounds")
                   .getChildFile ("GhovlzDyn")
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

void EqPresetStore::setTreeParam (juce::ValueTree& root, const juce::String& id, float value)
{
    for (int i = 0; i < root.getNumChildren(); ++i)
    {
        auto child = root.getChild (i);
        if (child.getProperty ("id").toString() == id)
        {
            child.setProperty ("value", (double) value, nullptr);
            return;
        }
    }

    juce::ValueTree p ("PARAM");
    p.setProperty ("id", id, nullptr);
    p.setProperty ("value", (double) value, nullptr);
    root.appendChild (p, nullptr);
}

juce::ValueTree EqPresetStore::makeOttState (bool) const
{
    // Ableton Live 12 Core Library Multiband Dynamics / OTT.adv
    const float splits[] = { 88.3f, 2500.0f };
    return makeOttStateN (3, splits, 2);
}

juce::ValueTree EqPresetStore::makeOttStateN (int bands, const float* splits, int numSplits) const
{
    auto state = captureState();
    if (! state.isValid())
        return {};

    bands = juce::jlimit (1, DynParams::kMaxBands, bands);
    setTreeParam (state, DynParams::countId(), (float) bands);
    setTreeParam (state, DynParams::selectedId(), 0.0f);
    setTreeParam (state, DynParams::faceAllId(), 1.0f);
    setTreeParam (state, DynParams::lookaheadId(), 0.0f);
    setTreeParam (state, DynParams::timeId(), 100.0f);
    setTreeParam (state, DynParams::amountId(), 100.0f);
    setTreeParam (state, DynParams::downAmtId(), 100.0f);
    setTreeParam (state, DynParams::upAmtId(), 5.0f);
    setTreeParam (state, DynParams::detectId(), (float) DynParams::detectRms);

    // Official 3-band OTT.adv (Low / Mid / High). N-band presets lerp these anchors.
    const float atkA[3]  = { 47.8f, 22.4f, 13.5f };
    const float relA[3]  = { 282.0f, 282.0f, 132.0f };
    const float downA[3] = { -33.8f, -30.3f, -35.5f };
    const float upA[3]   = { -40.8f, -41.8f, -40.8f };
    const float mkA[3]   = { 10.3f, 5.7f, 10.3f };
    const float ratioA[3] = { 66.7f, 66.7f, 100.0f };

    auto lerp3 = [] (float t, const float* a) -> float
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        if (t <= 0.5f)
            return a[0] + (a[1] - a[0]) * (t * 2.0f);
        return a[1] + (a[2] - a[1]) * ((t - 0.5f) * 2.0f);
    };

    for (int b = 0; b < bands; ++b)
    {
        const float t = bands > 1 ? (float) b / (float) (bands - 1) : 0.0f;
        setTreeParam (state, DynParams::onId (b), 1.0f);
        setTreeParam (state, DynParams::soloId (b), 0.0f);
        setTreeParam (state, DynParams::autoId (b), 0.0f);
        setTreeParam (state, DynParams::thresholdId (b), lerp3 (t, downA));
        setTreeParam (state, DynParams::upThresholdId (b), lerp3 (t, upA));
        setTreeParam (state, DynParams::ratioId (b), lerp3 (t, ratioA));
        setTreeParam (state, DynParams::attackId (b), lerp3 (t, atkA));
        setTreeParam (state, DynParams::releaseId (b), lerp3 (t, relA));
        setTreeParam (state, DynParams::kneeId (b), 8.0f);
        setTreeParam (state, DynParams::makeupId (b), lerp3 (t, mkA));
        setTreeParam (state, DynParams::mixId (b), 100.0f);
        setTreeParam (state, DynParams::clipId (b), 0.0f);
        setTreeParam (state, DynParams::clipThrId (b), 0.0f);
        if (b < numSplits)
            setTreeParam (state, DynParams::splitId (b), splits[b]);
    }

    return state;
}

void EqPresetStore::ensureFactoryOtt()
{
    bool changed = false;
    auto upsert = [this, &changed] (const juce::String& name, juce::ValueTree s)
    {
        if (! s.isValid())
            return;
        const int i = indexOfName (name);
        if (i >= 0)
        {
            states.set (i, s);
            changed = true;
        }
        else
        {
            names.add (name);
            states.add (s);
            changed = true;
        }
    };

    upsert ("OTT", makeOttState (false));
    upsert ("OTT Xfer", makeOttState (true));

    const float s4[] = { 88.3f, 400.0f, 2500.0f };
    const float s5[] = { 80.0f, 250.0f, 800.0f, 2500.0f };
    const float s6[] = { 80.0f, 200.0f, 500.0f, 1500.0f, 4000.0f };
    upsert ("OTT 4", makeOttStateN (4, s4, 3));
    upsert ("OTT 5", makeOttStateN (5, s5, 4));
    upsert ("OTT 6", makeOttStateN (6, s6, 5));

    if (changed)
        persistToXml();
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
