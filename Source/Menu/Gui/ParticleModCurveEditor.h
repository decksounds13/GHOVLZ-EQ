#pragma once

#include <JuceHeader.h>
#include "../../Spec3DParticleSystem.h"

/**
    Serum-style mini transfer curve for the particle mod matrix.
    Default: 1:1 diagonal. Vertical drag bends power curve (−1..+1).
    Double-click resets to linear.
*/
class ParticleModCurveEditor : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    ParticleModCurveEditor()
    {
        setMouseCursor (juce::MouseCursor::UpDownResizeCursor);
        SettableTooltipClient::setTooltip ("Drag up/down to bend the response curve. Double-click = linear.");
    }

    void setShape (float s) noexcept
    {
        s = juce::jlimit (-1.0f, 1.0f, s);
        if (std::abs (s - shape) < 1.0e-5f)
            return;
        shape = s;
        repaint();
    }

    float getShape() const noexcept { return shape; }

    std::function<void()> onShapeChanged;

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r, 2.0f);
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.22f));
        g.drawRoundedRectangle (r, 2.0f, 1.0f);

        // Grid diagonal guide
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawLine (r.getX(), r.getBottom(), r.getRight(), r.getY(), 1.0f);

        juce::Path path;
        const int steps = 24;
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float yv = particleModApplyCurve (t, shape);
            const float x = r.getX() + t * r.getWidth();
            const float y = r.getBottom() - yv * r.getHeight();
            if (i == 0)
                path.startNewSubPath (x, y);
            else
                path.lineTo (x, y);
        }

        g.setColour (juce::Colours::goldenrod.withAlpha (0.95f));
        g.strokePath (path, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        dragStartY = e.position.y;
        dragStartShape = shape;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        // Drag down (dy+) → positive shape (ease-in parabola up-to-the-right)
        // Drag up (dy−) → negative shape (inverse ease-out)
        const float dy = e.position.y - dragStartY;
        const float sens = 1.0f / juce::jmax (12.0f, (float) getHeight());
        setShape (dragStartShape + dy * sens);
        if (onShapeChanged)
            onShapeChanged();
    }

    void mouseDoubleClick (const juce::MouseEvent&) override
    {
        setShape (0.0f);
        if (onShapeChanged)
            onShapeChanged();
    }

private:
    float shape = 0.0f;
    float dragStartY = 0.0f;
    float dragStartShape = 0.0f;
};
