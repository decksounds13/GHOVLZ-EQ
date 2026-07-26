#pragma once

#include <JuceHeader.h>

/** Shared popup / combo menu colours — matches Mod section LFO shape < > toggles. */
namespace PluginMenuTheme
{
    inline juce::Colour background() noexcept { return juce::Colour::fromRGBA (60, 50, 35, 255); }
    inline juce::Colour highlight() noexcept  { return juce::Colour::fromRGBA (180, 150, 55, 255); }
    inline juce::Colour text() noexcept       { return juce::Colours::whitesmoke.withAlpha (0.9f); }
    inline juce::Colour textOnHighlight() noexcept { return juce::Colours::black; }

    inline void applyColours (juce::LookAndFeel& lf)
    {
        lf.setColour (juce::PopupMenu::backgroundColourId, background());
        lf.setColour (juce::PopupMenu::textColourId, text());
        lf.setColour (juce::PopupMenu::headerTextColourId, text());
        lf.setColour (juce::PopupMenu::highlightedBackgroundColourId, highlight());
        lf.setColour (juce::PopupMenu::highlightedTextColourId, textOnHighlight());
        lf.setColour (juce::ComboBox::backgroundColourId, background());
        lf.setColour (juce::ComboBox::outlineColourId, juce::Colour::fromRGBA (30, 25, 18, 255));
        lf.setColour (juce::ComboBox::buttonColourId, background());
        lf.setColour (juce::ComboBox::arrowColourId, text());
        lf.setColour (juce::ComboBox::textColourId, text());
        lf.setColour (juce::ComboBox::focusedOutlineColourId, highlight());
    }
}

/**
    Colours only — same approach as Spectrum / Level Meters ComboBoxes.
*/
class ComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ComboBoxLookAndFeel()
    {
        PluginMenuTheme::applyColours (*this);
    }

    static ComboBoxLookAndFeel& sharedForPopupMenus()
    {
        static ComboBoxLookAndFeel instance;
        return instance;
    }
};

/**
    Choice control that matches the working preset menu:
    - Display field shows only the current selection (one string)
    - Click opens PopupMenu::showMenuAsync (never ComboBox's built-in list)
*/
class ParamChoiceButton : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::AudioProcessorValueTreeState::Listener
{
public:
    ParamChoiceButton (juce::AudioProcessorValueTreeState& stateToUse, const juce::String& parameterID)
        : treeState (stateToUse), paramId (parameterID)
    {
        setRepaintsOnMouseActivity (true);
        treeState.addParameterListener (paramId, this);
        syncFromParameter();
    }

    ~ParamChoiceButton() override
    {
        treeState.removeParameterListener (paramId, this);
    }

    void parameterChanged (const juce::String& id, float) override
    {
        if (id != paramId)
            return;

        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<ParamChoiceButton> (this)]
                                         {
                                             if (safe != nullptr)
                                                 safe->syncFromParameter();
                                         });
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto fill = PluginMenuTheme::background();
        if (isMouseButtonDown())
            fill = fill.brighter (0.18f);
        else if (isMouseOver())
            fill = fill.brighter (0.1f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (juce::Colour::fromRGBA (30, 25, 18, 255));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

        constexpr int arrowW = 16;
        g.setColour (PluginMenuTheme::text());
        g.setFont (juce::FontOptions (12.0f));
        g.drawText (currentText,
                    getLocalBounds().reduced (6, 0).withTrimmedRight (arrowW),
                    juce::Justification::centredLeft,
                    true);

        juce::Path arrow;
        const float cx = (float) getWidth() - (float) arrowW * 0.5f;
        const float cy = (float) getHeight() * 0.5f;
        arrow.addTriangle (cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.5f);
        g.fillPath (arrow);
    }

    void mouseDown (const juce::MouseEvent&) override
    {
        if (! isEnabled())
            return;
        showMenu();
    }

    void enablementChanged() override { repaint(); }

private:
    void syncFromParameter()
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (paramId)))
        {
            const int idx = juce::jlimit (0, juce::jmax (0, choice->choices.size() - 1), choice->getIndex());
            currentText = choice->choices.isEmpty() ? juce::String() : choice->choices[idx];
            repaint();
        }
    }

    void showMenu()
    {
        auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (paramId));
        if (choice == nullptr || choice->choices.isEmpty())
            return;

        juce::PopupMenu menu;
        menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

        const int current = choice->getIndex();
        for (int i = 0; i < choice->choices.size(); ++i)
            menu.addItem (i + 1, choice->choices[i], true, i == current);

        // Identical path to MainComponent::showPresetPopupMenu().
        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<ParamChoiceButton> (this)] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;

                                auto* param = dynamic_cast<juce::AudioParameterChoice*> (
                                    safe->treeState.getParameter (safe->paramId));
                                if (param == nullptr)
                                    return;

                                const int index = juce::jlimit (0, param->choices.size() - 1, result - 1);
                                param->beginChangeGesture();
                                param->setValueNotifyingHost (param->convertTo0to1 ((float) index));
                                param->endChangeGesture();
                                safe->syncFromParameter();
                            });
    }

    juce::AudioProcessorValueTreeState& treeState;
    juce::String paramId;
    juce::String currentText;
};
