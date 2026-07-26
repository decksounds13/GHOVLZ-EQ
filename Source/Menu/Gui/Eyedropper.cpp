#include "Eyedropper.h"
#include <chrono>

Eyedropper::Eyedropper() {
    setMouseCursor(juce::MouseCursor::CrosshairCursor);
}

Eyedropper::~Eyedropper() {
}

void Eyedropper::beginColorSelection(std::function<void(juce::Colour)> callback) {
    colorSelectedCallback = callback;
    setAlwaysOnTop(true);
    setBounds(juce::Desktop::getInstance().getDisplays().getMainDisplay().userArea);
    addToDesktop(juce::ComponentPeer::windowIsTemporary);
    setVisible(true);
    startTimer(100);
    captureScreen(); // Start capturing the screen
}

void Eyedropper::mouseDown(const juce::MouseEvent& event) {
    stopTimer();
    setVisible(false);

    // The color selection is handled in captureScreen(), so nothing needed here
}

void Eyedropper::timerCallback() {
    // Optionally, update something periodically if needed
}


/*
void Eyedropper::captureScreen() {

    framegrabber = SL::Screen_Capture::CreateCaptureConfiguration([]() {
        return SL::Screen_Capture::GetMonitors();
        })->onNewFrame([&](const SL::Screen_Capture::Image& img, const SL::Screen_Capture::Monitor& monitor) {
            // This callback will be continuously called. Here, we need to capture the pixel color under the mouse cursor.
            auto mousePos = juce::Desktop::getMousePosition();
            if (mousePos.getX() >= monitor.OffsetX && mousePos.getX() < monitor.OffsetX + monitor.Width &&
                mousePos.getY() >= monitor.OffsetY && mousePos.getY() < monitor.OffsetY + monitor.Height) {

                auto imgData = startSrc(img);
                auto pixelColor = imgData[(mousePos.getY() - monitor.OffsetY) * img.RowStride + (mousePos.getX() - monitor.OffsetX) * img.PixelStride];

                // Convert BGRA to RGBA
                juce::Colour selectedColor = juce::Colour(pixelColor.B, pixelColor.G, pixelColor.R);

                if (colorSelectedCallback) {
                    colorSelectedCallback(selectedColor);
                    framegrabber->pause();  // Pause the screen capturing after selecting the color
                }
            }
            })->start_capturing();

            framegrabber->setFrameChangeInterval(std::chrono::milliseconds(100));

        
}



*/


