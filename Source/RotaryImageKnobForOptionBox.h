#pragma once

#include <JuceHeader.h>
#include "ControlReset.h"
#include <functional>
#include "MelatoninBlur/melatonin/shadows.h"
#include "Menu/SharedResources.h"
#include "RotaryImageKnobLookAndFeel1.h"
#include "RotaryImageKnobLookAndFeel2.h"
#include "RotaryImageKnobLookAndFeel3.h"
#include "RotaryImageKnobLookAndFeel4.h"
#include "RotaryImageKnobLookAndFeel5.h"
#include "RotaryImageKnobLookAndFeel6.h"

class RotaryImageKnobForOptionBox : public ResettableSlider
{
public:
    RotaryImageKnobForOptionBox();
    ~RotaryImageKnobForOptionBox();

    void paint(juce::Graphics& g) override;

    void setCustomRange(double newMin, double newMax, double newInterval);

    /** Compact OptionBox A/R knobs: no value text box, hover does not expand layout. */
    void setCompactNoValueBox (bool shouldBeCompact);

    void mouseEnter(const juce::MouseEvent& event) override;
    void mouseExit(const juce::MouseEvent& event) override;
    void mouseDown (const juce::MouseEvent& event) override;

    void setThemeColors (SharedResources* r) noexcept;

    /** If set, right-click / popup runs this instead of starting a drag. */
    std::function<void()> onPopupMenu;

private:
    void refreshValuePopup (bool show);

    bool compactNoValueBox = false;
    SharedResources* themeColors = nullptr;
    melatonin::DropShadow knobDropShadow {
        { juce::Colours::black.withAlpha (0.42f), 5, { 0, 2 }, 0 }
    };

    RotaryImageKnobLookAndFeel1 rotaryImageKnobLookAndFeel1;
    RotaryImageKnobLookAndFeel2 rotaryImageKnobLookAndFeel2;
    RotaryImageKnobLookAndFeel3 rotaryImageKnobLookAndFeel3;
    RotaryImageKnobLookAndFeel4 rotaryImageKnobLookAndFeel4;
    RotaryImageKnobLookAndFeel5 rotaryImageKnobLookAndFeel5;
    RotaryImageKnobLookAndFeel6 rotaryImageKnobLookAndFeel6;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RotaryImageKnobForOptionBox)
};

