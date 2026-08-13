#pragma once

#include <JuceHeader.h>
#include "../../GraphOverlayButtonLookAndFeel.h"
#include "../SharedResources.h"

/**
    Full-width Settings accordion header: title on the left, chevron on the right.
    Pages keep owning their controls; this only draws the bar and reports open state.
*/
class SettingsSection : public juce::Component
{
public:
    static constexpr int kHeaderH = 28;

    SettingsSection (SharedResources& resources,
                     juce::String sectionId,
                     juce::String title,
                     bool defaultOpen)
        : sharedResources (resources),
          id (std::move (sectionId)),
          titleText (std::move (title)),
          defaultOpen (defaultOpen)
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    bool isOpen() const noexcept
    {
        return sharedResources.isSettingsSectionOpen (id, defaultOpen);
    }

    void setOpen (bool shouldBeOpen)
    {
        if (isOpen() == shouldBeOpen)
            return;
        sharedResources.setSettingsSectionOpen (id, shouldBeOpen);
        repaint();
        if (onChanged != nullptr)
            onChanged();
    }

    void toggle() { setOpen (! isOpen()); }

    std::function<void()> onChanged;

    /** Header strip from the top of pageArea. */
    void placeHeader (juce::Rectangle<int>& pageArea)
    {
        setBounds (pageArea.removeFromTop (kHeaderH));
        toFront (false);
        pageArea.removeFromTop (isOpen() ? 6 : 8);
    }

    int heightFor (int bodyH) const noexcept
    {
        return kHeaderH + (isOpen() ? 6 + juce::jmax (0, bodyH) : 8);
    }

    void applyVisible (std::initializer_list<juce::Component*> children)
    {
        for (auto* c : children)
            if (c != nullptr)
                c->setVisible (isOpen());
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (e.mouseWasClicked() && getLocalBounds().contains (e.getPosition()))
            toggle();
    }

    void paint (juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat().reduced (0.5f);
        const auto& pal = sharedResources.sharedColors;
        auto fill = pal.menuSectionHeader;
        fill = GraphOverlayButtonLookAndFeel::adjustForInteraction (
            fill, isMouseOver(), isMouseButtonDown());
        GraphOverlayButtonLookAndFeel::paintChromeFace (g, bounds, fill,
                                                        GraphOverlayButtonLookAndFeel::cornerRadius(),
                                                        isMouseOver() || isMouseButtonDown());

        auto ink = pal.legibleTextOn (pal.menuSectionText, fill);
        g.setColour (ink);
        g.setFont (pal.makeUiFont (14.0f, true));

        auto textR = getLocalBounds().reduced (12, 0);
        textR.removeFromRight (26);
        g.drawText (titleText, textR, juce::Justification::centredLeft, false);

        // Down = closed (can open). Up = open.
        const float cx = (float) getWidth() - 16.0f;
        const float cy = (float) getHeight() * 0.5f;
        const float w = 6.0f;
        const float h = 4.0f;
        juce::Path chev;
        if (isOpen())
        {
            chev.addTriangle (cx - w, cy + h, cx + w, cy + h, cx, cy - h);
        }
        else
        {
            chev.addTriangle (cx - w, cy - h, cx + w, cy - h, cx, cy + h);
        }
        g.fillPath (chev);
    }

private:
    SharedResources& sharedResources;
    juce::String id;
    juce::String titleText;
    bool defaultOpen = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsSection)
};
