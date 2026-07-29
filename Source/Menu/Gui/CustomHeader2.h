#pragma once

#include <JuceHeader.h>
#include "../SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "shadows-main/shadows.h"

class CustomHeader2 : public juce::Component
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void elementNameHeaderClicked() = 0;
    };

    CustomHeader2 (SharedResources& sharedResources);
    ~CustomHeader2() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseUp (const juce::MouseEvent& event) override;

    void addListener (Listener* listener) { listeners.add (listener); }
    void removeListener (Listener* listener) { listeners.remove (listener); }

    void setSortIndicator (bool active, bool ascending);

private:
    std::unique_ptr<shadows::StackShadow> customShadow;
    SharedResources& sharedResources;
    juce::ListenerList<Listener> listeners;
    bool sortActive = false;
    bool sortAscending = true;
    melatonin::InnerShadow innerShadow = { { juce::Colours::black.withAlpha (0.75f), 5, { 0, 2 } } };
};
