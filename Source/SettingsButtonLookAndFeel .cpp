#include "SettingsButtonLookAndFeel.h"
#include "GraphOverlayButtonLookAndFeel.h"

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

    // Gear icon is small — cap radius so the control stays readable.
    const float cornerSize = juce::jmin (GraphOverlayButtonLookAndFeel::cornerRadius(),
                                         juce::jmin (reducedBounds.getWidth(),
                                                     reducedBounds.getHeight()) * 0.35f);

    juce::Colour face = idle;
    if (button.isMouseButtonDown() || shouldDrawButtonAsDown)
        face = down;
    else if (shouldDrawButtonAsHighlighted)
        face = hover;

    const bool hot = shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown
                     || button.isMouseButtonDown();
    GraphOverlayButtonLookAndFeel::paintChromeFace (g, reducedBounds, face, cornerSize, hot);

    juce::Path path;
    path.addRoundedRectangle (reducedBounds, cornerSize);
    shadowPath = path;
}

void SettingsButtonLookAndFeel::drawButtonText (juce::Graphics& g,
                                                juce::TextButton& button,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);

    const auto& c = palette();
    // Match styleChromeButton / OscToolButton off-state ink (Plugin Button Text on bg).
    g.setColour (c.legibleTextOn (c.pluginButtonText, c.pluginButtonBackground).withAlpha (0.92f));

    const int paddingX = juce::roundToInt (button.getWidth() * 0.23f);
    const int paddingY = juce::roundToInt (button.getHeight() * 0.23f);
    const int lineLength = button.getWidth() - 2 * paddingX;
    const int innerH = juce::jmax (3, button.getHeight() - 2 * paddingY);
    const int lineHeight = juce::jlimit (1, 3, innerH / 5);
    const int gapBetweenLines = juce::jmax (1, (innerH - 3 * lineHeight) / 2);

    for (int i = 0; i < 3; ++i)
        g.fillRect (paddingX, paddingY + i * (lineHeight + gapBetweenLines), lineLength, lineHeight);
}
