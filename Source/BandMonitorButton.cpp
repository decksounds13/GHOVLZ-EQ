#include "BandMonitorButton.h"
#include "KnobThemeHelpers.h"

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

    // Match power / knob chrome active-inactive language.
    juce::Colour ink;
    if (listening)
        ink = KnobTheme::chromeActive (baseColor);
    else if (shouldDrawButtonAsHighlighted)
        ink = brighterAndMoreSaturated (KnobTheme::chromeInactive (baseColor), 1.15f, 1.1f);
    else
        ink = KnobTheme::chromeInactive (baseColor);

    const float lineWidth = 2.2f;
    const float diameter = (float) juce::jmin (getWidth(), getHeight()) * 0.72f - 2.0f * lineWidth;
    const float centerX = (float) getWidth() * 0.5f;
    const float centerY = (float) getHeight() * 0.5f;
    const float radius = juce::jmax (4.0f, diameter * 0.5f);

    // Glow ring (match OnOff / power style).
    g.setColour (ink);
    juce::Path ring;
    ring.addEllipse (centerX - radius, centerY - radius, diameter, diameter);
    g.strokePath (ring, juce::PathStrokeType (lineWidth));

    const float outerRadius = radius + 2.0f;
    juce::Path outer;
    outer.addEllipse (centerX - outerRadius, centerY - outerRadius,
                      outerRadius * 2.0f, outerRadius * 2.0f);
    g.setColour (juce::Colours::black);
    g.strokePath (outer, juce::PathStrokeType (3.5f));

    // ------------------------------------------------------------------
    // Headphones: two ear cups + headband. Built for small faces (~16–24 px).
    // Cup geometry is chunky so it still reads when the ring is tight.
    // ------------------------------------------------------------------
    g.setColour (ink);

    const float s = radius * 0.95f;
    // Vertical oval cups (classic cans).
    const float cupW = s * 0.38f;
    const float cupH = s * 0.58f;
    const float cupCy = centerY + s * 0.06f;
    const float cupInset = s * 0.48f; // distance from centre to cup centre

    const auto leftCup = juce::Rectangle<float> (centerX - cupInset - cupW * 0.5f,
                                                 cupCy - cupH * 0.5f,
                                                 cupW, cupH);
    const auto rightCup = juce::Rectangle<float> (centerX + cupInset - cupW * 0.5f,
                                                  cupCy - cupH * 0.5f,
                                                  cupW, cupH);

    // Outer cup shells.
    g.fillEllipse (leftCup);
    g.fillEllipse (rightCup);

    // Inner pads (cutouts) so cups read as hollow earpieces, not blobs.
    const float padInsetX = cupW * 0.28f;
    const float padInsetY = cupH * 0.22f;
    g.setColour (juce::Colours::black.withAlpha (listening ? 0.55f : 0.70f));
    g.fillEllipse (leftCup.reduced (padInsetX, padInsetY));
    g.fillEllipse (rightCup.reduced (padInsetX, padInsetY));

    g.setColour (ink);

    // Headband: thick arc over the top connecting the cups.
    // JUCE arcs: 0 = +X (right), clockwise. Top = 3pi/2. Left=pi, right=0/2pi.
    juce::Path headband;
    const float bandCx = centerX;
    const float bandCy = cupCy - cupH * 0.05f;
    const float bandRx = cupInset + cupW * 0.08f;
    const float bandRy = s * 0.72f;
    // From upper-left cup through top to upper-right cup (clockwise: pi → 2pi).
    const float fromA = juce::MathConstants<float>::pi + 0.18f;
    const float toA = juce::MathConstants<float>::twoPi - 0.18f;
    headband.addCentredArc (bandCx, bandCy, bandRx, bandRy, 0.0f, fromA, toA, true);
    g.strokePath (headband, juce::PathStrokeType (juce::jmax (2.0f, lineWidth * 1.15f),
                                                  juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));

    // Short stems from headband ends into each cup (reads as headband hinges).
    const float stemLen = cupH * 0.18f;
    g.drawLine (leftCup.getCentreX(), leftCup.getY() + 1.0f,
                leftCup.getCentreX(), leftCup.getY() + stemLen, lineWidth * 0.9f);
    g.drawLine (rightCup.getCentreX(), rightCup.getY() + 1.0f,
                rightCup.getCentreX(), rightCup.getY() + stemLen, lineWidth * 0.9f);
}
