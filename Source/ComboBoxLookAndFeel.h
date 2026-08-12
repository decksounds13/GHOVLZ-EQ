#pragma once

#include <JuceHeader.h>
#include "Menu/SharedResources.h"
#include "GraphOverlayButtonLookAndFeel.h"

/**
    Shared popup / chrome-menu colours for the graph-top UI panel, context menus,
    and related CallOutBox chrome.

    Live SharedColors (Plugin/Option family) so dice / Faceplate randomize hits
    these panels; Legible text is applied vs each surface.
    Falls back to factory-like defaults when no SharedResources is active.
*/
namespace PluginMenuTheme
{
    inline const SharedColors& colors() noexcept
    {
        static const SharedColors defaults;
        if (auto* active = SharedResources::getActive())
            return active->sharedColors;
        return defaults;
    }

    /** Panel / row fill — Option Combo Background (synced from Plugin chrome). */
    inline juce::Colour background() noexcept
    {
        return colors().optionComboBackground;
    }

    /** Hover / selected row + accent. */
    inline juce::Colour highlight() noexcept
    {
        return colors().optionComboHighlight;
    }

    /** Primary label ink on panel background. */
    inline juce::Colour text() noexcept
    {
        const auto& c = colors();
        return c.legibleTextOn (c.optionComboText, c.optionComboBackground)
            .withAlpha (0.95f);
    }

    /** Label ink on highlight/accent rows (black when accent is light, white when dark). */
    inline juce::Colour textOnHighlight() noexcept
    {
        const auto& c = colors();
        return c.legibleTextOn (juce::Colours::black, c.optionComboHighlight);
    }

    inline juce::Colour outline() noexcept
    {
        return colors().optionBorder;
    }

    inline void applyColours (juce::LookAndFeel& lf)
    {
        const auto bg = background();
        const auto hl = highlight();
        const auto ink = text();
        const auto inkOn = textOnHighlight();
        lf.setColour (juce::PopupMenu::backgroundColourId, bg);
        lf.setColour (juce::PopupMenu::textColourId, ink);
        lf.setColour (juce::PopupMenu::headerTextColourId, ink);
        lf.setColour (juce::PopupMenu::highlightedBackgroundColourId, hl);
        lf.setColour (juce::PopupMenu::highlightedTextColourId, inkOn);
        lf.setColour (juce::ComboBox::backgroundColourId, bg);
        lf.setColour (juce::ComboBox::outlineColourId, outline());
        lf.setColour (juce::ComboBox::buttonColourId, bg);
        lf.setColour (juce::ComboBox::arrowColourId, ink);
        lf.setColour (juce::ComboBox::textColourId, ink);
        lf.setColour (juce::ComboBox::focusedOutlineColourId, hl);
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
        // Legible text: combo / popup ink vs field fill; highlight row ink vs accent.
        const auto comboInk = c.legibleTextOn (c.optionComboText, c.optionComboBackground);
        const auto hlInk = c.legibleTextOn (juce::Colours::black, c.optionComboHighlight);
        setColour (juce::PopupMenu::backgroundColourId, c.optionComboBackground);
        setColour (juce::PopupMenu::textColourId, comboInk);
        setColour (juce::PopupMenu::headerTextColourId, comboInk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.optionComboHighlight);
        setColour (juce::PopupMenu::highlightedTextColourId, hlInk);
        setColour (juce::ComboBox::backgroundColourId, c.optionComboBackground);
        setColour (juce::ComboBox::outlineColourId, c.optionBorder);
        setColour (juce::ComboBox::buttonColourId, c.optionComboBackground);
        setColour (juce::ComboBox::arrowColourId, comboInk);
        setColour (juce::ComboBox::textColourId, comboInk);
        setColour (juce::ComboBox::focusedOutlineColourId, c.optionComboHighlight);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        // Refresh cached colour IDs from the live theme (dice / UI randomize).
        applyThemeColours();
        g.fillAll (colors().optionComboBackground);
        juce::ignoreUnused (width, height);
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColourToUse) override
    {
        // Re-bind colours every paint so open menus track dice without reopening.
        applyThemeColours();
        LookAndFeel_V4::drawPopupMenuItem (g, area, isSeparator, isActive, isHighlighted,
                                           isTicked, hasSubMenu, text, shortcutKeyText,
                                           icon, textColourToUse);
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
        const auto arrowInk = c.legibleTextOn (c.optionComboText, c.optionComboBackground);
        g.setColour (arrowInk.withAlpha (box.isEnabled() ? 0.95f : 0.35f));
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
        // Melatonin drop extends past local bounds; without this the blur is clipped away.
        setPaintingIsUnclipped (true);
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

        GraphOverlayButtonLookAndFeel::renderRoundedDrop (g, bounds.reduced (0.5f), 2.0f);
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
