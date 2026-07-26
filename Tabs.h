// Tabs.h
#pragma once

#include <JuceHeader.h>
#include "QuadPicker.h"

class CustomTab : public juce::Component
{
public:
    CustomTab(const juce::String& name);
    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    juce::String name;
    QuadPicker quadPicker;
};

class Tabs : public juce::Component
{
public:
    Tabs();
    ~Tabs() override = default;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    juce::TabbedComponent tabs;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Tabs)
};
