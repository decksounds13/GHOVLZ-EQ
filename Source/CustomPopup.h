#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/cached_blur.h" // Make sure to include Melatonin's CachedBlur

class CustomPopup : public juce::Component, private juce::Timer {
public:
    enum Mode { Normal, TransparentNotify };

    CustomPopup();
    void setMessage(const juce::String& newMessage);
    void setMode(Mode mode);
    void setColors(const juce::Colour& bg, const juce::Colour& outline, const juce::Colour& text);
    void setDisplayDuration(int durationMs);
    void updateBackgroundBlur(); // Method to update the blur

protected:
    void paint(juce::Graphics& g) override;

private:
    void timerCallback() override;

    juce::String message;
    Mode currentMode;
    juce::Colour backgroundColor;
    juce::Colour textColor;
    int displayDurationMs;
    int fadeOutDurationMs;
    float alpha;
    bool fadingOut;
    melatonin::CachedBlur blur{ 12 }; // Blur object
};
