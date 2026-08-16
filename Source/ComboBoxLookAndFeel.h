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

    /** Primary label ink on the popup fill (Option Combo Text — keep hue). */
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

    /** Dropdown / popup panel radius — not the chrome button radius. */
    inline float popupCorner() noexcept
    {
        return juce::jlimit (0.0f, 16.0f, colors().menuPopupCornerRadius);
    }

    inline bool popupDrawOutline() noexcept
    {
        return colors().menuPopupOutline;
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
        // Popup items: Option Combo Text vs the panel only (hue survives dice).
        const auto popupInk = PluginMenuTheme::text();
        const auto hlInk = PluginMenuTheme::textOnHighlight();
        // Closed Settings combo still sits on the Menu page — extra wash contrast.
        const auto fieldInk = c.dropdownTextOn (c.optionComboText, c.optionComboBackground);
        setColour (juce::PopupMenu::backgroundColourId, c.optionComboBackground);
        setColour (juce::PopupMenu::textColourId, popupInk);
        setColour (juce::PopupMenu::headerTextColourId, popupInk);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, c.optionComboHighlight);
        setColour (juce::PopupMenu::highlightedTextColourId, hlInk);
        setColour (juce::ComboBox::backgroundColourId, c.optionComboBackground);
        setColour (juce::ComboBox::outlineColourId, c.optionBorder);
        setColour (juce::ComboBox::buttonColourId, c.optionComboBackground);
        setColour (juce::ComboBox::arrowColourId, fieldInk);
        setColour (juce::ComboBox::textColourId, fieldInk);
        setColour (juce::ComboBox::focusedOutlineColourId, c.optionComboHighlight);
    }

    /** Soft top→bottom form used by popup panels and highlight rows. */
    static void fillMenuGradient (juce::Graphics& g,
                                  juce::Rectangle<float> bounds,
                                  juce::Colour fill,
                                  float corner = 0.0f)
    {
        GraphOverlayButtonLookAndFeel::fillRoundedGradient (g, bounds, fill, corner);
    }

    void drawPopupMenuBackground (juce::Graphics& g, int width, int height) override
    {
        // Refresh cached colour IDs from the live theme (dice / UI randomize).
        applyThemeColours();
        const auto& c = colors();
        auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
        const float corner = PluginMenuTheme::popupCorner();

        // Soft vertical gradation (was flat fillAll).
        fillMenuGradient (g, bounds, c.optionComboBackground, corner);

        if (PluginMenuTheme::popupDrawOutline())
        {
            g.setColour (c.optionBorder.withAlpha (0.85f));
            if (corner > 0.5f)
                g.drawRoundedRectangle (bounds.reduced (0.5f), corner, 1.0f);
            else
                g.drawRect (bounds.reduced (0.5f), 1.0f);
        }
    }

    void drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                            bool isSeparator, bool isActive, bool isHighlighted,
                            bool isTicked, bool hasSubMenu, const juce::String& text,
                            const juce::String& shortcutKeyText,
                            const juce::Drawable* icon, const juce::Colour* textColourToUse) override
    {
        // Re-bind colours every paint so open menus track dice without reopening.
        applyThemeColours();
        const auto& c = colors();

        if (isSeparator)
        {
            auto r = area.reduced (5, 0);
            r.removeFromTop (juce::roundToInt ((float) r.getHeight() * 0.5f) - 1);
            g.setColour (c.optionBorder.withAlpha (0.55f));
            g.fillRect (r.removeFromTop (1));
            return;
        }

        auto r = area.toFloat().reduced (1.0f, 1.0f);
        const float rowCorner = juce::jmin (4.0f, PluginMenuTheme::popupCorner() * 0.75f);

        if (isHighlighted && isActive)
        {
            // Highlight row: same soft chrome gradation as buttons.
            fillMenuGradient (g, r, c.optionComboHighlight, rowCorner);
            g.setColour (juce::Colours::black.withAlpha (0.28f));
            g.drawRoundedRectangle (r, rowCorner, 1.0f);
        }

        const bool useHighlightInk = isHighlighted && isActive;
        const auto rowFill = useHighlightInk ? c.optionComboHighlight : c.optionComboBackground;
        auto textColour = textColourToUse != nullptr
                              ? c.legibleTextOn (*textColourToUse, rowFill)
                              : (useHighlightInk
                                     ? PluginMenuTheme::textOnHighlight()
                                     : PluginMenuTheme::text());
        if (! isActive)
            textColour = textColour.withMultipliedAlpha (0.45f);

        g.setColour (textColour);
        g.setFont (c.makeUiFont (13.0f));

        auto textArea = area.reduced (10, 0);
        if (hasSubMenu)
            textArea.removeFromRight (16);
        if (isTicked)
            textArea.removeFromLeft (12);

        if (icon != nullptr)
        {
            auto iconArea = textArea.removeFromLeft (juce::jmin (textArea.getHeight(), 22)).toFloat();
            icon->drawWithin (g, iconArea, juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize, 1.0f);
            textArea.removeFromLeft (4);
        }

        if (isTicked)
        {
            const float tickSize = 8.0f;
            const float cx = (float) area.getX() + 10.0f;
            const float cy = (float) area.getCentreY();
            juce::Path tick;
            tick.startNewSubPath (cx - tickSize * 0.35f, cy);
            tick.lineTo (cx - tickSize * 0.05f, cy + tickSize * 0.35f);
            tick.lineTo (cx + tickSize * 0.45f, cy - tickSize * 0.4f);
            g.strokePath (tick, juce::PathStrokeType (1.6f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
        }

        g.drawText (text, textArea, juce::Justification::centredLeft, false);

        if (shortcutKeyText.isNotEmpty())
        {
            g.setFont (c.makeUiFont (11.0f));
            g.setColour (textColour.withMultipliedAlpha (0.7f));
            g.drawText (shortcutKeyText, area.reduced (10, 0),
                        juce::Justification::centredRight, false);
        }

        if (hasSubMenu)
        {
            const float arrowX = (float) area.getRight() - 12.0f;
            const float arrowY = (float) area.getCentreY();
            juce::Path arrow;
            arrow.addTriangle (arrowX - 2.0f, arrowY - 4.0f,
                               arrowX - 2.0f, arrowY + 4.0f,
                               arrowX + 3.5f, arrowY);
            g.setColour (textColour.withMultipliedAlpha (0.85f));
            g.fillPath (arrow);
        }
    }

    void drawPopupMenuSectionHeader (juce::Graphics& g, const juce::Rectangle<int>& area,
                                     const juce::String& sectionName) override
    {
        applyThemeColours();
        const auto& c = colors();
        const auto ink = c.legibleTextOn (c.optionComboText, c.optionComboBackground)
                             .withAlpha (0.88f);
        g.setFont (c.makeUiFont (12.0f).boldened());
        g.setColour (ink);
        g.drawText (sectionName, area.reduced (10, 0), juce::Justification::centredLeft, false);
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override
    {
        juce::ignoreUnused (buttonX, buttonY, buttonW, buttonH);

        applyThemeColours();
        const auto& c = colors();
        auto cornerSize = box.findParentComponentOfClass<juce::ChoicePropertyComponent>() != nullptr
                              ? 0.0f : GraphOverlayButtonLookAndFeel::cornerRadius();
        auto bounds = juce::Rectangle<int> (0, 0, width, height).toFloat();

        const bool hot = box.isMouseOver (true) || isButtonDown || box.isPopupActive();
        auto fill = GraphOverlayButtonLookAndFeel::adjustForInteraction (
            c.optionComboBackground, box.isMouseOver (true), isButtonDown || box.isPopupActive());
        GraphOverlayButtonLookAndFeel::paintChromeFace (g, bounds, fill, cornerSize, hot);
        g.setColour (c.optionBorder);
        g.drawRoundedRectangle (bounds.reduced (0.5f), cornerSize, 1.0f);

        juce::Rectangle<int> arrowZone (width - 14, 0, 12, height);
        juce::Path path;
        path.startNewSubPath ((float) arrowZone.getX() + 2.0f, (float) arrowZone.getCentreY() - 2.0f);
        path.lineTo ((float) arrowZone.getCentreX(), (float) arrowZone.getCentreY() + 2.5f);
        path.lineTo ((float) arrowZone.getRight() - 2.0f, (float) arrowZone.getCentreY() - 2.0f);
        const auto arrowInk = c.dropdownTextOn (c.optionComboText, fill);
        g.setColour (arrowInk.withAlpha (box.isEnabled() ? 0.95f : 0.35f));
        g.strokePath (path, juce::PathStrokeType (1.4f));
    }

    juce::Font getComboBoxFont (juce::ComboBox& box) override
    {
        juce::ignoreUnused (box);
        return colors().makeUiFont (11.0f);
    }

    juce::Font getPopupMenuFont() override
    {
        return colors().makeUiFont (13.0f);
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        const float h = label.getFont().getHeight() > 1.0f ? label.getFont().getHeight() : 12.0f;
        return colors().makeUiFont (h);
    }

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override
    {
        juce::ignoreUnused (button);
        return colors().makeUiFont (juce::jmin (15.0f, (float) buttonHeight * 0.55f));
    }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (3, 1, juce::jmax (1, box.getWidth() - 16), box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
        label.setJustificationType (juce::Justification::centredLeft);
        label.setMinimumHorizontalScale (1.0f);
        const auto& c = colors();
        const auto ink = c.dropdownTextOn (c.optionComboText, c.optionComboBackground)
                             .withAlpha (box.isEnabled() ? 0.95f : 0.35f);
        label.setColour (juce::Label::textColourId, ink);
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
        auto fill = useChromeColours ? chromeFill
                                     : lf.findColour (juce::ComboBox::backgroundColourId);
        auto ink = useChromeColours ? chromeInk
                                    : lf.findColour (juce::ComboBox::textColourId);
        if (auto* active = SharedResources::getActive())
            ink = active->sharedColors.legibleTextOn (ink, fill);

        const float corner = GraphOverlayButtonLookAndFeel::cornerRadius();
        GraphOverlayButtonLookAndFeel::paintChromeButton (g, bounds, fill,
                                                          isMouseOver(), isMouseButtonDown(),
                                                          corner);

        constexpr int arrowW = 16;
        g.setColour (ink.withAlpha (isEnabled() ? 0.95f : 0.40f));
        if (auto* active = SharedResources::getActive())
            g.setFont (active->sharedColors.makeUiFont (14.0f));
        else
            g.setFont (juce::FontOptions (14.0f));
        g.drawText (currentText,
                    getLocalBounds().reduced (6, 0).withTrimmedRight (arrowW),
                    juce::Justification::centredLeft,
                    false);

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

    /** Match faceplate chrome (Scope / Auto Gain / ?) instead of menu combo ink. */
    void setChromeColours (juce::Colour fill, juce::Colour ink)
    {
        chromeFill = fill;
        chromeInk = ink;
        useChromeColours = true;
        repaint();
    }

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
    juce::Colour chromeFill, chromeInk;
    bool useChromeColours = false;
};
