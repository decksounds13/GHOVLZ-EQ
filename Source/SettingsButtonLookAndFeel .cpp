#include "SettingsButtonLookAndFeel.h"

SettingsButtonLookAndFeel::SettingsButtonLookAndFeel() = default;

SettingsButtonLookAndFeel::~SettingsButtonLookAndFeel() = default;

void SettingsButtonLookAndFeel::drawButtonBackground (juce::Graphics& g,
                                                      juce::Button& button,
                                                      const juce::Colour& backgroundColour,
                                                      bool shouldDrawButtonAsHighlighted,
                                                      bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (backgroundColour);

    const auto& c = palette();
    const auto idle = c.pluginButtonBackground.brighter (0.12f);
    const auto hover = c.pluginButtonBackground.brighter (0.28f);
    const auto down = c.pluginButtonAccent;

    auto reducedBounds = button.getLocalBounds().toFloat().reduced (button.getWidth() * 0.1f,
                                                                    button.getHeight() * 0.1f);

    juce::Path path;
    const float cornerSize = 2.0f;
    path.addRoundedRectangle (reducedBounds, cornerSize);
    shadowPath = path;

    if (SharedResources::glowShadowEffectsEnabled())
        shadow.render (g, path);

    if (button.isMouseButtonDown() || shouldDrawButtonAsDown)
        g.setColour (down);
    else if (shouldDrawButtonAsHighlighted)
        g.setColour (hover);
    else
        g.setColour (idle);

    g.fillRoundedRectangle (reducedBounds, cornerSize);

    g.setColour (c.pluginButtonBackground.darker (0.55f));
    g.drawRoundedRectangle (reducedBounds.reduced (1.0f), cornerSize, 2.0f);
}

void SettingsButtonLookAndFeel::drawButtonText (juce::Graphics& g,
                                                juce::TextButton& button,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto& c = palette();
    g.setColour (c.pluginButtonText.withAlpha (0.85f));

    const int paddingX = juce::roundToInt (button.getWidth() * 0.23f);
    const int paddingY = juce::roundToInt (button.getHeight() * 0.23f);
    const int lineLength = button.getWidth() - 2 * paddingX;
    const int lineHeight = 3;
    const int gapBetweenLines = (button.getHeight() - 2 * paddingY - (3 * lineHeight)) / 2;

    for (int i = 0; i < 3; ++i)
        g.fillRect (paddingX, paddingY + i * (lineHeight + gapBetweenLines), lineLength, lineHeight);
}
