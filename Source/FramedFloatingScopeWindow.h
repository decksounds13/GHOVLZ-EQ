#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"
#include <functional>

class SharedResources;

/**
    Shared Melatonin framed floating window for analyser overlays (Osc / Gon / Spec / 3D).
    Move via frame chrome, resize via corner grip; content fills the inner pad.
*/
class FramedFloatingScopeWindow : public juce::Component
{
public:
    enum class ChromeMode { floating, docked };

    FramedFloatingScopeWindow();
    ~FramedFloatingScopeWindow() override;

    void setThemeColors (SharedResources* r) noexcept;
    void setChromeMode (ChromeMode mode) noexcept;
    ChromeMode getChromeMode() const noexcept { return chromeMode; }

    /** Soft panel fill (oscBackground @ ~90/255). Off draws opaque panel. */
    void setSoftFill (bool shouldSoftFill) noexcept;
    bool isSoftFill() const noexcept { return softFill; }

    void setContent (juce::Component* content) noexcept;
    juce::Component* getContent() const noexcept { return contentComp; }

    void setFrameActive (bool shouldBeActive) noexcept;
    bool isFrameActive() const noexcept { return frameActive; }

    void setResizeLimits (int maxW, int maxH) noexcept;
    void setMovementBounds (juce::Rectangle<int> parentLocalBounds) noexcept;

    int getShadowPad() const noexcept;
    static int getShadowPadForMode (ChromeMode mode) noexcept;
    juce::Rectangle<int> getInnerFrameLocal() const noexcept;
    juce::Rectangle<int> getContentLocal() const noexcept;

    std::function<void()> onEscape;
    std::function<void()> onUserResized;
    std::function<void()> onUserMoved;
    std::function<void()> onDoubleClick;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;

    static constexpr float kSoftFillAlpha = 90.0f / 255.0f;
    static constexpr float kCornerRadius = 12.0f;
    static constexpr int kContentInset = 3;

protected:
    /** Extra paint inside the rounded panel after soft/opaque fill (before stroke). */
    virtual void paintInsidePanel (juce::Graphics&, juce::Path& /*panel*/) {}

    juce::Colour getPanelFillColour() const noexcept;
    bool isInMoveChrome (juce::Point<int> localPos) const noexcept;

    SharedResources* theme = nullptr;

private:
    static constexpr int kShadowPadFloating = 14;
    static constexpr int kShadowPadDocked = 2;

    void applyChromeMode() noexcept;

    ChromeMode chromeMode = ChromeMode::floating;
    bool softFill = true;
    bool frameActive = false;
    juce::Component* contentComp = nullptr;

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    juce::ComponentDragger moveDragger;
    juce::ComponentBoundsConstrainer constrainer;
    bool movingByChrome = false;
    std::unique_ptr<juce::ResizableCornerComponent> resizer;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FramedFloatingScopeWindow)
};
