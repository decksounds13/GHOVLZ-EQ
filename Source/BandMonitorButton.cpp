#include "BandMonitorButton.h"

BandMonitorButton::BandMonitorButton()
    : juce::Button ("bandMonitor")
{
    setClickingTogglesState (true);
    setTooltip ("Monitor - solo this band's full processing (works even when the band is off)");
}

void BandMonitorButton::setListening (bool shouldListen)
{
    listening = shouldListen;
    setToggleState (shouldListen, juce::dontSendNotification);
    repaint();
}

juce::Colour BandMonitorButton::brighterAndMoreSaturated (const juce::Colour& col,
                                                          float brightnessFactor,
                                                          float saturationFactor)
{
    float h, s, v;
    col.getHSB (h, s, v);
    v = juce::jmin (1.0f, v * brightnessFactor);
    s = juce::jmin (1.0f, s * saturationFactor);
    return juce::Colour::fromHSV (h, s, v, col.getFloatAlpha());
}

void BandMonitorButton::paintButton (juce::Graphics& g, bool shouldDrawButtonAsHighlighted,
                                     bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);
    listening = getToggleState();

    if (listening)
        g.setColour (brighterAndMoreSaturated (baseColor, 2.0f, 2.0f));
    else if (shouldDrawButtonAsHighlighted)
        g.setColour (brighterAndMoreSaturated (baseColor, 1.4f, 1.4f));
    else
        g.setColour (baseColor);

    const float lineWidth = 2.4f;
    const float diameter = (float) getWidth() * 0.70f - 2.0f * lineWidth;
    const float centerX = (float) getWidth() * 0.5f;
    const float centerY = (float) getHeight() * 0.5f;
    const float radius = diameter * 0.5f;

    juce::Path ring;
    ring.addEllipse (centerX - radius, centerY - radius, diameter, diameter);
    g.strokePath (ring, juce::PathStrokeType (lineWidth));

    // Outer black rim (match OnOffButton1).
    const float outerRadius = radius + 2.0f;
    juce::Path outer;
    outer.addEllipse (centerX - outerRadius, centerY - outerRadius,
                      outerRadius * 2.0f, outerRadius * 2.0f);
    g.setColour (juce::Colours::black);
    g.strokePath (outer, juce::PathStrokeType (4.0f));

    // Headphones glyph — cups + headband, scaled to the inner disc.
    if (listening)
        g.setColour (brighterAndMoreSaturated (baseColor, 2.0f, 2.0f));
    else if (shouldDrawButtonAsHighlighted)
        g.setColour (brighterAndMoreSaturated (baseColor, 1.4f, 1.4f));
    else
        g.setColour (baseColor);

    const float s = radius * 0.92f;
    const float cupW = s * 0.34f;
    const float cupH = s * 0.52f;
    const float cupY = centerY - cupH * 0.15f;
    const float leftCupX = centerX - s * 0.55f;
    const float rightCupX = centerX + s * 0.55f - cupW;

    auto leftCup = juce::Rectangle<float> (leftCupX, cupY, cupW, cupH);
    auto rightCup = juce::Rectangle<float> (rightCupX, cupY, cupW, cupH);
    g.fillRoundedRectangle (leftCup, cupW * 0.35f);
    g.fillRoundedRectangle (rightCup, cupW * 0.35f);

    juce::Path band;
    const float bandY = cupY + cupH * 0.12f;
    const float bandR = s * 0.58f;
    band.addCentredArc (centerX, bandY + bandR * 0.15f, bandR, bandR * 0.85f,
                        0.0f, juce::MathConstants<float>::pi * 1.12f,
                        juce::MathConstants<float>::pi * 1.88f, true);
    g.strokePath (band, juce::PathStrokeType (lineWidth * 0.95f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}
