#include "CustomTabBarLookAndFeel.h"

CustomTabBarLookAndFeel::CustomTabBarLookAndFeel() = default;

int CustomTabBarLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth)
{
    juce::ignoreUnused (tabDepth);
    // Size to full label — never rely on JUCE ellipsis compression.
    const auto font = tabFont();
    const float textW = juce::GlyphArrangement::getStringWidth (font, button.getButtonText());
    constexpr int pad = 16; // horizontal padding so last glyph never clips
    return juce::jmax (36, juce::roundToInt (textW) + pad);
}

void CustomTabBarLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                            bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (isMouseDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const bool isActive = button.isFrontTab();

    if (isActive)
    {
        g.setColour (activePill);
        g.fillRoundedRectangle (bounds, 5.0f);
        // Active underline (clear selected state without ellipsis tricks)
        g.setColour (ink.withAlpha (0.85f));
        const float y = bounds.getBottom() - 2.0f;
        g.fillRoundedRectangle (bounds.getX() + 6.0f, y, bounds.getWidth() - 12.0f, 2.0f, 1.0f);
    }
    else if (isMouseOver)
    {
        g.setColour (activePill.withMultipliedAlpha (0.55f));
        g.fillRoundedRectangle (bounds, 5.0f);
    }

    const float alpha = isActive ? 1.0f : (isMouseOver ? 0.88f : 0.58f);
    g.setFont (tabFont());
    g.setColour (ink.withAlpha (alpha));
    // false = no ellipsis — width is sized via getTabButtonBestWidth
    g.drawText (button.getButtonText(),
                button.getLocalBounds().reduced (6, 0),
                juce::Justification::centred,
                false);
}
