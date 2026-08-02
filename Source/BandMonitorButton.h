#pragma once
#include <JuceHeader.h>

/**
    Round OptionBox control matching OnOffButton1 chrome, with a headphones glyph.
    Latching toggle — listen/solo the current band's full processing chain.
*/
class BandMonitorButton : public juce::Button
{
public:
    BandMonitorButton();
    ~BandMonitorButton() override = default;

    void paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                      bool shouldDrawButtonAsDown) override;

    void setBaseColor (juce::Colour newColor) { baseColor = newColor; repaint(); }
    void setListening (bool shouldListen);
    bool isListening() const noexcept { return listening; }

private:
    static juce::Colour brighterAndMoreSaturated (const juce::Colour& col,
                                                  float brightnessFactor,
                                                  float saturationFactor);

    juce::Colour baseColor { juce::Colours::grey };
    bool listening = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BandMonitorButton)
};
