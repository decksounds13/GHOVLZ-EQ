#pragma once

#include <JuceHeader.h>
#include "ComboBoxLookAndFeel.h"
#include "LfoMod.h"

/**
    Retrigger control: left-click toggles Off ↔ MIDI,
    right-click chooses Off / MIDI.
*/
class RetrigTextButton : public juce::TextButton
{
public:
    RetrigTextButton() = default;

    explicit RetrigTextButton (const juce::String& buttonText)
        : juce::TextButton (buttonText)
    {
    }

    std::function<void()> onPopupMenu;

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu() && onPopupMenu != nullptr)
        {
            onPopupMenu();
            return;
        }

        juce::TextButton::mouseDown (e);
    }
};

inline juce::String retrigButtonTooltip()
{
    return "Restart this modulator when a MIDI note starts.\n"
           "Left-click or right-click: Off / MIDI.\n"
           "Ableton: make a MIDI track and set MIDI To -> this track and this plugin "
           "(same setup as Serum FX).";
}

inline int getRetrigModeIndex (juce::AudioProcessorValueTreeState& treeState,
                               const juce::String& retrigParamId)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (retrigParamId)))
        return juce::jlimit (0, LfoMod::numRetrigModes - 1, p->getIndex());
    return LfoMod::retrigOff;
}

inline void setRetrigModeIndex (juce::RangedAudioParameter* param, int mode)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (param))
    {
        const int m = juce::jlimit (0, LfoMod::numRetrigModes - 1, mode);
        if (p->getIndex() == m)
            return;
        p->beginChangeGesture();
        *p = m;
        p->endChangeGesture();
    }
}

inline void setRetrigModeIndex (juce::AudioProcessorValueTreeState& treeState,
                                const juce::String& retrigParamId,
                                int mode)
{
    setRetrigModeIndex (treeState.getParameter (retrigParamId), mode);
}

inline void syncRetrigButton (RetrigTextButton& button, int mode)
{
    button.setToggleState (mode != LfoMod::retrigOff, juce::dontSendNotification);
    button.setButtonText (mode == LfoMod::retrigMidi ? "M" : "N");
}

inline void showRetrigModeMenu (juce::Component* target,
                                juce::AudioProcessorValueTreeState& treeState,
                                const juce::String& retrigParamId)
{
    const int mode = getRetrigModeIndex (treeState, retrigParamId);
    auto* param = treeState.getParameter (retrigParamId);

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Off", true, mode == LfoMod::retrigOff);
    menu.addItem (2, "MIDI", true, mode == LfoMod::retrigMidi);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (target),
                        [param] (int result)
                        {
                            if (result <= 0)
                                return;
                            setRetrigModeIndex (param, result - 1);
                        });
}

inline void wireRetrigButton (RetrigTextButton& button,
                              juce::AudioProcessorValueTreeState& treeState,
                              const juce::String& retrigParamId)
{
    button.setClickingTogglesState (false);
    button.setTooltip (retrigButtonTooltip());
    button.onPopupMenu = [&button, &treeState, retrigParamId]
    {
        showRetrigModeMenu (&button, treeState, retrigParamId);
    };
    button.onClick = [&treeState, retrigParamId]
    {
        const int mode = getRetrigModeIndex (treeState, retrigParamId);
        setRetrigModeIndex (treeState, retrigParamId,
                            mode == LfoMod::retrigOff ? (int) LfoMod::retrigMidi
                                                      : (int) LfoMod::retrigOff);
    };
    syncRetrigButton (button, getRetrigModeIndex (treeState, retrigParamId));
}
