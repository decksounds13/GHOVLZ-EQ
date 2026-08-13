#include "BandNumberButton.h"
#include "KnobThemeHelpers.h"
#include "GraphOverlayButtonLookAndFeel.h"
#include "Menu/SharedResources.h"

BandNumberButton::BandNumberButton (juce::AudioProcessorValueTreeState& state,
                                    const juce::String& parameterIDIn)
    : juce::Button ("bandNumber"),
      treeState (state),
      parameterID (parameterIDIn)
{
    if (auto* p = treeState.getParameter (parameterID))
        isButtonDown = p->getValue() > 0.5f;
    setToggleState (isButtonDown, juce::dontSendNotification);
}

BandNumberButton::~BandNumberButton() = default;

void BandNumberButton::setBandNumber (int numberOneBased)
{
    bandNumber = juce::jmax (1, numberOneBased);
    repaint();
}

void BandNumberButton::setParameterID (const juce::String& newParamID)
{
    parameterID = newParamID;
    if (auto* p = treeState.getParameter (parameterID))
        isButtonDown = p->getValue() > 0.5f;
    setToggleState (isButtonDown, juce::dontSendNotification);
    repaint();
}

void BandNumberButton::paintButton (juce::Graphics& g,
                                    bool shouldDrawButtonAsHighlighted,
                                    bool shouldDrawButtonAsDown)
{
    juce::ignoreUnused (shouldDrawButtonAsDown);

    juce::Colour ink = isButtonDown ? KnobTheme::chromeActive (baseColor)
                                    : KnobTheme::chromeInactive (baseColor);
    ink = GraphOverlayButtonLookAndFeel::adjustForInteraction (ink, shouldDrawButtonAsHighlighted,
                                                               shouldDrawButtonAsDown);

    const float lineWidth = 3.0f;
    const float circleDiameter = (float) getWidth() * 0.7f - 2.0f * lineWidth;
    const float centerX = (float) getWidth() * 0.5f;
    const float centerY = (float) getHeight() * 0.5f;
    const float radius = circleDiameter * 0.5f;

    // Closed glow ring (no power-gap / dash).
    g.setColour (ink);
    juce::Path ring;
    ring.addCentredArc (centerX, centerY, radius, radius, 0.0f,
                        0.0f, juce::MathConstants<float>::twoPi, true);
    g.strokePath (ring, juce::PathStrokeType (lineWidth));

    // Band number uses the same ink as the ring.
    const auto numberText = juce::String (bandNumber);
    const float fontScale = numberText.length() >= 2 ? 0.95f : 1.15f;
    const float fontH = juce::jmax (8.0f, radius * fontScale);
    if (auto* active = SharedResources::getActive())
        g.setFont (active->sharedColors.makeUiFont (fontH, true));
    else
        g.setFont (juce::FontOptions().withHeight (fontH).withStyle ("Bold"));
    g.drawFittedText (numberText,
                      getLocalBounds().reduced (juce::roundToInt (lineWidth + 1.0f)),
                      juce::Justification::centred,
                      1);

    const float outerRadius = radius + 2.0f;
    juce::Path outerCircle;
    outerCircle.addEllipse (centerX - outerRadius, centerY - outerRadius,
                            2.0f * outerRadius, 2.0f * outerRadius);
    g.setColour (juce::Colours::black);
    g.strokePath (outerCircle, juce::PathStrokeType (4.0f));
}

void BandNumberButton::clicked()
{
    isButtonDown = ! isButtonDown;
    setToggleState (isButtonDown, juce::dontSendNotification);

    if (auto* param = treeState.getParameter (parameterID))
        param->setValueNotifyingHost (isButtonDown ? 1.0f : 0.0f);

    repaint();
}

juce::Colour BandNumberButton::brighterAndMoreSaturated (const juce::Colour& col,
                                                         float brightnessFactor,
                                                         float saturationFactor)
{
    float h, s, v;
    col.getHSB (h, s, v);
    v = juce::jmin (1.0f, v * brightnessFactor);
    s = juce::jmin (1.0f, s * saturationFactor);
    return juce::Colour::fromHSV (h, s, v, col.getFloatAlpha());
}
