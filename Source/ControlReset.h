#pragma once

#include <JuceHeader.h>
#include <functional>
#include "ComboBoxLookAndFeel.h"

/**
    Drop-in Slider with plugin-standard reset:
      • Double-click → default
      • Alt-click or Ctrl/Cmd-click → default
      • Right-click → "Reset to Default" menu item

    Call setControlDefault() after setRange (and after APVTS attachments).
*/
class ResettableSlider : public juce::Slider
{
public:
    ResettableSlider()
    {
        // Match JUCE APVTS / stock behaviour: Alt-click returns to default.
        // Double-click also enabled once a default is registered.
        setPopupMenuEnabled (false);
    }

    /** Register the value used by double-click / Alt / Ctrl / right-click reset. */
    void setControlDefault (double value)
    {
        defaultValue = value;
        hasDefault = true;
        isTwoValueDefault = false;
        const double inRange = juce::jlimit (getMinimum(), getMaximum(), value);
        setDoubleClickReturnValue (true, inRange, juce::ModifierKeys::altModifier);
    }

    /** Two-value (min/max thumb) defaults. */
    void setControlDefaults (double minValue, double maxValue)
    {
        defaultMin = juce::jmin (minValue, maxValue);
        defaultMax = juce::jmax (minValue, maxValue);
        hasDefault = true;
        isTwoValueDefault = true;
        // Keep JUCE double-click path happy (clamped into range).
        setDoubleClickReturnValue (true, juce::jlimit (getMinimum(), getMaximum(), defaultMin),
                                   juce::ModifierKeys::altModifier);
    }

    bool hasControlDefault() const noexcept { return hasDefault; }
    double getControlDefault() const noexcept { return defaultValue; }

    std::function<void()> onControlReset;

    void resetToControlDefault()
    {
        if (! hasDefault)
        {
            // Fall back to JUCE double-click value if an attachment set it.
            if (isDoubleClickReturnEnabled())
            {
                setValue (getDoubleClickReturnValue(), juce::sendNotificationSync);
                if (onControlReset)
                    onControlReset();
            }
            return;
        }

        if (isTwoValueDefault
            && (getSliderStyle() == TwoValueHorizontal || getSliderStyle() == TwoValueVertical
                || getSliderStyle() == ThreeValueHorizontal || getSliderStyle() == ThreeValueVertical))
        {
            const double lo = juce::jlimit (getMinimum(), getMaximum(), defaultMin);
            const double hi = juce::jlimit (getMinimum(), getMaximum(), defaultMax);
            setMinValue (lo, juce::dontSendNotification);
            setMaxValue (hi, juce::sendNotificationSync);
        }
        else
        {
            const double inRange = juce::jlimit (getMinimum(), getMaximum(), defaultValue);
            setValue (inRange, juce::sendNotificationSync);
            // Uncapped text entry stores a parallel "actual" — keep it in sync.
            static const juce::Identifier kParticleSliderActual ("particleSliderActual");
            getProperties().set (kParticleSliderActual, defaultValue);
            updateText();
        }

        if (onControlReset)
            onControlReset();
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! isEnabled())
        {
            juce::Slider::mouseDown (e);
            return;
        }

        if (e.mods.isPopupMenu())
        {
            showResetPopupMenu();
            return;
        }

        // Alt / Ctrl / Cmd + click → reset (in addition to double-click).
        const auto keyMods = e.mods.withoutMouseButtons();
        if (keyMods == juce::ModifierKeys::altModifier
            || keyMods == juce::ModifierKeys::ctrlModifier
            || keyMods == juce::ModifierKeys::commandModifier)
        {
            resetToControlDefault();
            return;
        }

        juce::Slider::mouseDown (e);
    }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        juce::ignoreUnused (e);
        if (isEnabled())
            resetToControlDefault();
        else
            juce::Slider::mouseDoubleClick (e);
    }

private:
    void showResetPopupMenu()
    {
        juce::PopupMenu m;
        m.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
        m.addItem (1, "Reset to Default", hasDefault || isDoubleClickReturnEnabled());

        // Keep useful rotary mode items when relevant.
        if (isRotary())
        {
            m.addSeparator();
            m.addItem (2, "Use circular dragging", true, getSliderStyle() == Rotary);
            m.addItem (3, "Use left-right dragging", true, getSliderStyle() == RotaryHorizontalDrag);
            m.addItem (4, "Use up-down dragging", true, getSliderStyle() == RotaryVerticalDrag);
            m.addItem (5, "Use left-right/up-down dragging", true,
                       getSliderStyle() == RotaryHorizontalVerticalDrag);
        }

        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this).withMousePosition(),
                         [safe = juce::Component::SafePointer<ResettableSlider> (this)] (int result)
                         {
                             if (safe == nullptr || result == 0)
                                 return;
                             if (result == 1)
                                 safe->resetToControlDefault();
                             else if (result == 2)
                                 safe->setSliderStyle (Rotary);
                             else if (result == 3)
                                 safe->setSliderStyle (RotaryHorizontalDrag);
                             else if (result == 4)
                                 safe->setSliderStyle (RotaryVerticalDrag);
                             else if (result == 5)
                                 safe->setSliderStyle (RotaryHorizontalVerticalDrag);
                         });
    }

    double defaultValue = 0.0;
    double defaultMin = 0.0, defaultMax = 1.0;
    bool hasDefault = false;
    bool isTwoValueDefault = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ResettableSlider)
};

/** Wire double-click / Alt defaults on a plain juce::Slider (no right-click menu). */
inline void controlResetEnableDoubleClick (juce::Slider& slider, double defaultValue)
{
    const double inRange = juce::jlimit (slider.getMinimum(), slider.getMaximum(), defaultValue);
    slider.setDoubleClickReturnValue (true, inRange, juce::ModifierKeys::altModifier);
    slider.getProperties().set ("controlDefaultValue", defaultValue);
}

/** If slider is ResettableSlider, set full reset default; else double-click/Alt only. */
inline void controlResetEnable (juce::Slider& slider, double defaultValue)
{
    if (auto* rs = dynamic_cast<ResettableSlider*> (&slider))
        rs->setControlDefault (defaultValue);
    else
        controlResetEnableDoubleClick (slider, defaultValue);
}

inline void controlResetEnableTwoValue (juce::Slider& slider, double defaultMin, double defaultMax)
{
    if (auto* rs = dynamic_cast<ResettableSlider*> (&slider))
        rs->setControlDefaults (defaultMin, defaultMax);
    else
    {
        controlResetEnableDoubleClick (slider, defaultMin);
        slider.getProperties().set ("controlDefaultMin", defaultMin);
        slider.getProperties().set ("controlDefaultMax", defaultMax);
    }
}
