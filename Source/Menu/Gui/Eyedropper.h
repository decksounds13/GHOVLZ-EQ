#pragma once

#include <JuceHeader.h>
#include "../ScreenCaptureLite/ScreenCapture.h" // Include screen_capture_lite header

class Eyedropper : public juce::Component, public juce::Timer { // Inherit from juce::Timer
public:
    Eyedropper();
    virtual ~Eyedropper();

    void beginColorSelection(std::function<void(juce::Colour)> callback);
    void captureScreen();
    void timerCallback() override; // Override from juce::Timer
    void mouseDown(const juce::MouseEvent& event) override; // Override from juce::Component
    void setColor(const juce::Colour& newColor);

private:
    std::shared_ptr<SL::Screen_Capture::IScreenCaptureManager> framegrabber;
    std::function<void(juce::Colour)> colorSelectedCallback;
    juce::Colour hueColor;
    juce::Colour selectedColor;

};
