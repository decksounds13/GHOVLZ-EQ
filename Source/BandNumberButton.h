#pragma once
#include <JuceHeader.h>

/**
    Faceplate band on/off control: same glow-ring language as OnOffButton1,
    but a closed ring + band number (no power dash). OnOffButton1 is retained
    for OptionBox / future power-style use.
*/
class BandNumberButton : public juce::Button
{
public:
    BandNumberButton (juce::AudioProcessorValueTreeState& state, const juce::String& parameterID);
    ~BandNumberButton() override;

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
    void clicked() override;

    void setBaseColor (juce::Colour newColor) { baseColor = newColor; repaint(); }
    void setBandNumber (int numberOneBased);
    int getBandNumber() const noexcept { return bandNumber; }
    void setParameterID (const juce::String& newParamID);

    static juce::Colour brighterAndMoreSaturated (const juce::Colour& col,
                                                  float brightnessFactor,
                                                  float saturationFactor);

private:
    bool isButtonDown = true;
    int bandNumber = 1;
    juce::Colour baseColor = juce::Colours::grey;
    juce::AudioProcessorValueTreeState& treeState;
    juce::String parameterID;
};
