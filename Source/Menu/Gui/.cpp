// Tabs.cpp
#include "Tabs.h"

CustomTab::CustomTab(const juce::String& name) : name(name)
{
    DBG("CustomTab constructor called: " + name);
    addAndMakeVisible(quadPicker);
    quadPicker.setVisible(true);
    setVisible(true);
}

void CustomTab::paint(juce::Graphics& g)
{
    DBG("CustomTab paint: " + name);
    // Common painting code for all tabs here
}

void CustomTab::resized()
{
    DBG("CustomTab resized: " + name);
    quadPicker.setBounds(10, 10, 700, 500);
    quadPicker.repaint();
}

Tabs::Tabs() : tabs(juce::TabbedButtonBar::TabsAtTop)
{
    DBG("Tabs constructor called");
    setSize(800, 600);

    tabs.addTab("Appearance", juce::Colours::transparentBlack, new CustomTab("Appearance"), true);
    tabs.addTab("Behavior", juce::Colours::transparentBlack, new CustomTab("Behavior"), true);
    tabs.addTab("About", juce::Colours::transparentBlack, new CustomTab("About"), true);
    tabs.setOutline(0);
    tabs.setTabBarDepth(70);
    tabs.setCurrentTabIndex(0);

    addAndMakeVisible(tabs);
    tabs.setCurrentTabIndex(0);
    DBG("Tabs created and added to parent");
}

void Tabs::paint(juce::Graphics&)
{
    // Custom painting for Tabs component
}

void Tabs::resized()
{
    tabs.setBounds(getLocalBounds());
}
