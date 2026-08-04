#include "CustomTabBarLookAndFeel.h"

CustomTabBarLookAndFeel::CustomTabBarLookAndFeel()
{
}

int CustomTabBarLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth)
{
    juce::ignoreUnused (tabDepth);
    // Fit the label tightly — only a few px of side padding past the text.
    const juce::Font font (juce::FontOptions ("Lato Black", 14.0f, juce::Font::plain));
    const float textW = juce::GlyphArrangement::getStringWidth (font, button.getButtonText());
    return juce::jmax (28, juce::roundToInt (textW) + 12);
}

void CustomTabBarLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                            bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (isMouseDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (1.0f, 2.0f);
    const bool isActive = button.isFrontTab();

    auto textColour = juce::Colours::whitesmoke.withAlpha (isActive ? 1.0f : (isMouseOver ? 0.85f : 0.55f));

    if (isActive || isMouseOver)
    {
        g.setColour (juce::Colours::whitesmoke.withAlpha (isActive ? 0.12f : 0.06f));
        g.fillRoundedRectangle (bounds, 5.0f);
    }

    g.setFont (juce::Font (juce::FontOptions ("Lato Black", 14.0f, juce::Font::plain)));
    g.setColour (textColour);
    g.drawText (button.getButtonText(),
                button.getLocalBounds().reduced (4, 0),
                juce::Justification::centred,
                false);
}
