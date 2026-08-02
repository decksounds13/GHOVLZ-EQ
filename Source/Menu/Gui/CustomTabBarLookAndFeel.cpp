#include "CustomTabBarLookAndFeel.h"

CustomTabBarLookAndFeel::CustomTabBarLookAndFeel()
{
}

int CustomTabBarLookAndFeel::getTabButtonBestWidth (juce::TabBarButton& button, int tabDepth)
{
    juce::ignoreUnused (tabDepth);
    const auto& text = button.getButtonText();
    // Sized so a page of 6 tabs leaves room for the ◀ ▶ page arrows on the right.
    if (text.equalsIgnoreCase ("FFT"))
        return 70;
    if (text.equalsIgnoreCase ("Spectrum"))
        return 100;
    if (text.equalsIgnoreCase ("Oscilloscope"))
        return 120;
    if (text.equalsIgnoreCase ("Goniometer"))
        return 110;
    if (text.equalsIgnoreCase ("Spectrogram"))
        return 120;
    if (text.equalsIgnoreCase ("Gradients"))
        return 100;
    if (text.equalsIgnoreCase ("Level Meters"))
        return 115;
    if (text.equalsIgnoreCase ("Loudness"))
        return 100;
    if (text.equalsIgnoreCase ("Stereogram"))
        return 110;
    if (text.equalsIgnoreCase ("Histogram"))
        return 105;
    if (text.containsIgnoreCase ("Appearance"))
        return 110;
    return 100;
}

void CustomTabBarLookAndFeel::drawTabButton (juce::TabBarButton& button, juce::Graphics& g,
                                            bool isMouseOver, bool isMouseDown)
{
    juce::ignoreUnused (isMouseDown);

    const auto bounds = button.getLocalBounds().toFloat().reduced (2.0f, 2.0f);
    const bool isActive = button.isFrontTab();

    auto textColour = juce::Colours::whitesmoke.withAlpha (isActive ? 1.0f : (isMouseOver ? 0.85f : 0.55f));

    if (isActive || isMouseOver)
    {
        g.setColour (juce::Colours::whitesmoke.withAlpha (isActive ? 0.12f : 0.06f));
        g.fillRoundedRectangle (bounds, 8.0f);
    }

    g.setFont (juce::Font ("Lato Black", 16.0f, juce::Font::plain));
    g.setColour (textColour);
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, true);
}
