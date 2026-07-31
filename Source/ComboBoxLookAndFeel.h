#pragma once

#include <JuceHeader.h>
#include "Menu/SharedResources.h"

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
    Colours + slightly tighter combo text so OptionBox labels fit without "...".
*/
class ComboBoxLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ComboBoxLookAndFeel()
    {
        applyThemeColours();
    }

    void setThemeColors (SharedResources* r) noexcept
    {
        themeColors = r;
        applyThemeColours();
    }

    static ComboBoxLookAndFeel& sharedForPopupMenus()
    {
        static ComboBoxLookAndFeel instance;
        return instance;
    }

    void applyThemeColours()
    {
        const auto& c = colors();
        setColour (juce::PopupMenu::backgroundColourId, c.optionComboBackground);
        setColour (juce::PopupMenu::textColourId, c.optionComboText);
        setColour (juce::PopupMenu::headerTextColourId, c.optionComboText);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.optionComboHighlight);
        setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::black);
        setColour (juce::ComboBox::backgroundColourId, c.optionComboBackground);
        setColour (juce::ComboBox::outlineColourId, c.optionBorder);
        setColour (juce::ComboBox::buttonColourId, c.optionComboBackground);
        setColour (juce::ComboBox::arrowColourId, c.optionComboText);
        setColour (juce::ComboBox::textColourId, c.optionComboText);
        setColour (juce::ComboBox::focusedOutlineColourId, c.optionComboHighlight);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        // Refresh cached colour IDs from the live theme (dice / UI randomize).
        applyThemeColours();
        g.fillAll (colors().optionComboBackground);
        juce::ignoreUnused (width, height);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        juce::ignoreUnused (isButtonDown, buttonX, buttonY, buttonW, buttonH);

        const auto& c = colors();
        auto cornerSize = box.findParentComponentOfClass<juce::ChoicePropertyComponent>() != nullptr
                              ? 0.0f : 2.0f;
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();

        g.setColour (c.optionComboBackground);
        g.fillRoundedRectangle (bounds, cornerSize);
        g.setColour (c.optionBorder);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);

        juce::Rectangle<int> arrowZone (width - 14, 0, 12, height);
        juce::Path path;
        path.startNewSubPath ((float) arrowZone.getX() + 2.0f, (float) arrowZone.getCentreY() - 2.0f);
        path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 2.5f);
        path.lineTo ((float) arrowZone.getRight() - 2.0f, (float) arrowZone.getCentreY() - 2.0f);
        g.setColour (c.optionComboText.withAlpha (box.isEnabled() ? 0.95f : 0.35f));
        g.strokePath (path, juce::PathStrokeType (1.4f));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        juce::ignoreUnused (box);
        return juce::Font ("Lato Black", 11.0f, juce::Font::plain);
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (3, 1, juce::jmax (1, box.getWidth() - 16), box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (0.75f); // prefer slight squeeze over "..."
    }

protected:
    SharedResources* themeColors = nullptr;

    const SharedColors& colors() const noexcept
    {
        static const SharedColors defaultColors;
        if (themeColors != nullptr)
            return themeColors->sharedColors;
        if (auto* active = SharedResources::getActive())
            return active->sharedColors;
        return defaultColors;
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
        auto& lf = ComboBoxLookAndFeel::sharedForPopupMenus();
        auto bounds = getLocalBounds().toFloat();
        auto fill = lf.findColour (juce::ComboBox::backgroundColourId);
        if (isMouseButtonDown())
            fill = fill.brighter (0.18f);
        else if (isMouseOver())
            fill = fill.brighter (0.1f);

        g.setColour (fill);
        g.fillRoundedRectangle (bounds, 2.0f);
        g.setColour (lf.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 2.0f, 1.0f);

        constexpr int arrowW = 16;
        g.setColour (lf.findColour (juce::ComboBox::textColourId));
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
