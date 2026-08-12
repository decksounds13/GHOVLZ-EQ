#include "OnOffButton1.h"
#include "KnobThemeHelpers.h"

OnOffButton1::OnOffButton1(juce::AudioProcessorValueTreeState& state, const juce::String& parameterID)
    : juce::Button("defaultName"), treeState(state), parameterID(parameterID)  // Initialize parameterID here
{
    isButtonDown = treeState.getParameter(parameterID)->getValue() > 0.5f;
    repaint();
}

OnOffButton1::~OnOffButton1()
{
    // Destructor code here
}

void OnOffButton1::paintButton(juce::Graphics& g, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{

    // Match BandNumberButton / faceplate knob arcs (was 2x boost on ON → mismatched glow).
    juce::Colour ink;
    if (isButtonDown)
        ink = KnobTheme::chromeActive (baseColor);
    else if (shouldDrawButtonAsHighlighted)
        ink = brighterAndMoreSaturated (KnobTheme::chromeInactive (baseColor), 1.15f, 1.1f);
    else
        ink = KnobTheme::chromeInactive (baseColor);

    g.setColour (ink);

    // Calculate the dimensions and position
    float lineWidth = 3.0f;  // The width of the line
    float circleDiameter = getWidth() * 0.7f - 2 * lineWidth;  // Subtract twice the line width
    float circleX = lineWidth;  // Adjust for line width
    float circleY = lineWidth;  // Adjust for line width
    float centerX = getWidth() / 2;
    float centerY = getHeight() / 2;
    float radius = circleDiameter / 2;
    float extendedRadius = 1.2f * radius;

    // Create path for the custom arc (320 degrees)
    juce::Path customArc;
    customArc.addCentredArc(centerX, centerY, radius, radius, 0, juce::degreesToRadians(20.0f), juce::degreesToRadians(340.0f), true);

    // Stroke the custom arc path
    g.strokePath(customArc, juce::PathStrokeType(lineWidth));

    // Draw the line segment from the center, extending beyond the radius
    g.drawLine(centerX, centerY, centerX, centerY - extendedRadius, lineWidth);

      // Create an outer circle path, which is slightly larger than the customArc
    float outerRadius = radius + 2;  // 4 pixels thicker
    juce::Path outerCircle;
    outerCircle.addEllipse(centerX - outerRadius, centerY - outerRadius, 2 * outerRadius, 2 * outerRadius);

    //juce::Path outerCircle;
    //outerCircle.addEllipse(centerX - outerRadius, centerY - outerRadius, 2 * outerRadius, 2 * outerRadius);

    // Set the color for the outer circle
    g.setColour(juce::Colours::black);  // Setting it to black for now

    // Stroke the outer circle path with 4-pixel line width
    g.strokePath(outerCircle, juce::PathStrokeType(4.0f));
}

void OnOffButton1::clicked()
{
    isButtonDown = ! isButtonDown;
    setToggleState (isButtonDown, juce::dontSendNotification);

    if (auto* param = treeState.getParameter (parameterID))
        param->setValueNotifyingHost (isButtonDown ? 1.0f : 0.0f);

    repaint();
}


juce::Colour OnOffButton1::brighterAndMoreSaturated(const juce::Colour& col, float brightnessFactor, float saturationFactor)
{
    float h, s, v;
    col.getHSB(h, s, v);

    // Increase brightness and saturation
    v *= brightnessFactor;
    s *= saturationFactor;

    // Clamp to valid range [0, 1]
    v = juce::jmin(v, 1.0f);
    s = juce::jmin(s, 1.0f);

    return juce::Colour::fromHSV(h, s, v, col.getFloatAlpha());
}

void OnOffButton1::setParameterID(const juce::String& newParamID)
{
    parameterID = newParamID;
    isButtonDown = treeState.getParameter(parameterID)->getValue() > 0.5f;
    repaint();
}