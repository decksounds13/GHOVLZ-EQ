#pragma once

#include <JuceHeader.h>
#include "../SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "shadows-main/shadows.h"

class CustomHeader2 : public juce::Component
                
{
public:
    CustomHeader2(SharedResources& sharedResources);
    ~CustomHeader2() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    std::unique_ptr<shadows::StackShadow> customShadow;
  
    SharedResources& sharedResources;
    melatonin::InnerShadow innerShadow = { { juce::Colours::black.withAlpha(0.75f), 5, { 0, 2 } } };
    // More member variables as needed
};
