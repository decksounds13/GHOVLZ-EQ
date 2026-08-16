#pragma once

#include <JuceHeader.h>
#include <functional>

/** Confirm before replacing a named DSP / UI / ramp / look / layout preset. */
namespace PresetOverwriteConfirm
{
    inline void run (const juce::String& kind,
                     const juce::String& name,
                     bool exists,
                     std::function<void()> onSave,
                     juce::Component* parent = nullptr)
    {
        if (! onSave)
            return;

        if (! exists)
        {
            onSave();
            return;
        }

        const auto shown = name.trim().isNotEmpty() ? name.trim() : juce::String ("Untitled");
        auto* aw = new juce::AlertWindow (
            "Overwrite preset?",
            "A " + kind + " named \"" + shown + "\" already exists. Overwrite it?",
            juce::AlertWindow::WarningIcon,
            parent);
        aw->addButton ("Overwrite", 1, juce::KeyPress (juce::KeyPress::returnKey));
        aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
        aw->enterModalState (true,
            juce::ModalCallbackFunction::create (
                [fn = std::move (onSave)] (int r)
                {
                    if (r == 1)
                        fn();
                }),
            true);
    }
}
