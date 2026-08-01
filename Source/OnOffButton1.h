#pragma once
#include <JuceHeader.h>

/** Classic power-symbol on/off (gap + dash). Kept for OptionBox and future reuse.
    Faceplate bands use BandNumberButton instead. */
class OnOffButton1 : public juce::Button
{
public:
    OnOffButton1(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID);
    ~OnOffButton1() override;

    void paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void clicked() override;

    void setBaseColor(juce::Colour newColor) { baseColor = newColor; repaint(); }
    static juce::Colour brighterAndMoreSaturated(const juce::Colour& col, float brightnessFactor, float saturationFactor);
    void setParameterID(const juce::String& newParamID);

private:
    bool isButtonDown = true; 
    juce::Colour baseColor = juce::Colours::grey; 
    juce::AudioProcessorValueTreeState& treeState;
    juce::String parameterID;
};
