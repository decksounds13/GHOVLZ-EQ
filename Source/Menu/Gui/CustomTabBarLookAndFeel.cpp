#include "CustomTabBarLookAndFeel.h"
#include "../../GraphOverlayButtonLookAndFeel.h"
#include "../SharedResources.h"

CustomTabBarLookAndFeel::CustomTabBarLookAndFeel() = default;

int CustomTabBarLookAndFeel::bestWidthForLabel (const juce::String& text) noexcept
{
    juce::Font font (13.0f);
    if (auto* active = SharedResources::getActive())
        font = active->sharedColors.makeUiFont (13.0f);
    const float textW = juce::GlyphArrangement::getStringWidth (font, text);
    constexpr int pad = 16;
    return juce::jmax (36, juce::roundToInt (textW) + pad);
}

int CustomTabBarLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth)
{
    juce::ignoreUnused (tabDepth);
    return bestWidthForLabel (button.getButtonText());
}

void CustomTabBarLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                            bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (isMouseDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const bool isActive = button.isFrontTab();
    const float corner = GraphOverlayButtonLookAndFeel::cornerRadius();

    if (isActive)
    {
        GraphOverlayButtonLookAndFeel::paintChromeButton (g, bounds, activePill);
        g.setColour (ink.withAlpha (0.85f));
        const float y = bounds.getBottom() - 2.0f;
        g.fillRoundedRectangle (bounds.getX() + 6.0f, y, bounds.getWidth() - 12.0f, 2.0f, 1.0f);
    }
    else if (isMouseOver)
    {
        GraphOverlayButtonLookAndFeel::paintChromeButton (
            g, bounds, activePill.withMultipliedAlpha (0.55f), true, isMouseDown);
    }

    const float alpha = isActive ? 1.0f : (isMouseOver ? 0.88f : 0.58f);
    if (auto* active = SharedResources::getActive())
        g.setFont (active->sharedColors.makeUiFont (tabFont().getHeight()));
    else
        g.setFont (tabFont());
    g.setColour (ink.withAlpha (alpha));
    // false = no ellipsis — width is sized via getTabButtonBestWidth
    g.drawText (button.getButtonText(),
                button.getLocalBounds().reduced (6, 0),
                juce::Justification::centred,
                false);
}
