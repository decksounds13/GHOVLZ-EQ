#pragma once

#include <JuceHeader.h>
#include "ShapeMod.h"
#include "ComboBoxLookAndFeel.h"
#include "Menu/SharedResources.h"

/** Display / edit canvas for the Shape modulator curve — colours match LFO previews (Mod*). */
class ShapeCurveEditor : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    explicit ShapeCurveEditor (ShapeMod::Engine& engineToEdit)
        : engine (engineToEdit)
    {
        setWantsKeyboardFocus (true);
        setTooltip ("Shape curve. Double-click to edit. Double-click adds/removes points "
                    "(clear all for a flat line / no modulation). Right-click a point for Soft.");
    }

    void setThemeColors (SharedResources* r) noexcept
    {
        themeColors = r;
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        const auto& c = colors();
        auto bounds = getLocalBounds().toFloat();
        g.setColour (c.modBackground.withAlpha (230.0f / 255.0f));
        g.fillRoundedRectangle (bounds, 3.0f);

        const auto plot = plotBounds();
        g.setColour (c.modBackground.darker (0.35f));
        g.fillRoundedRectangle (plot, 2.0f);

        const float midY = toScreen (0.0f, 0.0f).y;
        g.setColour (c.modText.withAlpha (0.12f));
        g.drawHorizontalLine ((int) midY, plot.getX(), plot.getRight());

        juce::Path path;
        const int steps = juce::jmax (32, (int) plot.getWidth());
        for (int i = 0; i <= steps; ++i)
        {
            const float t = (float) i / (float) steps;
            const float y = engine.uiCurve.evaluate (t);
            const auto p = toScreen (t, y);
            if (i == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }
        // Same stroke colour as LFO waveform previews.
        g.setColour (c.modAccent);
        g.strokePath (path, juce::PathStrokeType (1.6f));

        if (playheadActive)
        {
            const float x = toScreen (playhead, 0.0f).x;
            g.setColour (c.modAccent.brighter (0.35f).withAlpha (0.65f));
            g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
        }

        if (editing)
        {
            g.setColour (c.modAccent.withAlpha (200.0f / 255.0f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 3.0f, 1.2f);

            const auto& pts = engine.uiCurve.getPoints();
            for (int i = 0; i < (int) pts.size(); ++i)
            {
                const auto& pt = pts[(size_t) i];
                const auto ptScreen = toScreen (pt.x, pt.y);
                const auto hard = c.modAccent.brighter (0.35f);
                const auto soft = c.modAccent.withRotatedHue (0.45f).brighter (0.15f);
                g.setColour (pt.soft ? soft : hard);
                g.fillEllipse (ptScreen.x - 4.0f, ptScreen.y - 4.0f, 8.0f, 8.0f);
                g.setColour (juce::Colours::black.withAlpha (0.7f));
                g.drawEllipse (ptScreen.x - 4.0f, ptScreen.y - 4.0f, 8.0f, 8.0f, 1.0f);
            }
        }
        else
        {
            g.setColour (c.modText.withAlpha (0.35f));
            g.setFont (juce::FontOptions (9.0f));
            g.drawText ("dbl-click to edit", plot, juce::Justification::centredBottom, false);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (! editing)
            return;

        grabKeyboardFocus();
        const int hit = hitTestPoint (e.position);

        if (e.mods.isPopupMenu())
        {
            if (hit >= 0)
                showPointMenu (hit);
            return;
        }

        dragIndex = hit;
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        if (! editing || dragIndex < 0)
            return;

        const auto n = toNorm (e.position);
        if (engine.uiCurve.movePoint (dragIndex, n.x, n.y))
        {
            dragIndex = hitTestPoint (e.position, 20.0f);
            notifyChanged();
        }
    }

    void mouseUp (const juce::MouseEvent&) override { dragIndex = -1; }

    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        grabKeyboardFocus();

        if (! editing)
        {
            editing = true;
            repaint();
            return;
        }

        const int hit = hitTestPoint (e.position);
        if (hit >= 0)
        {
            if (engine.uiCurve.removePoint (hit))
                notifyChanged();
            return;
        }

        const auto n = toNorm (e.position);
        if (engine.uiCurve.addPoint (n.x, n.y))
            notifyChanged();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key == juce::KeyPress::escapeKey && editing)
        {
            editing = false;
            dragIndex = -1;
            repaint();
            return true;
        }
        return false;
    }

    void setPlayheadPhase (float phase01) noexcept { playhead = phase01; }
    void setPlayheadActive (bool shouldShow) noexcept { playheadActive = shouldShow; }
    bool isEditing() const noexcept { return editing; }

    std::function<void()> onCurveChanged;

private:
    ShapeMod::Engine& engine;
    SharedResources* themeColors = nullptr;
    bool editing = false;
    int dragIndex = -1;
    float playhead = 0.0f;
    bool playheadActive = false;

    const SharedColors& colors() const noexcept
    {
        static const SharedColors defaults;
        return themeColors != nullptr ? themeColors->sharedColors : defaults;
    }

    juce::Rectangle<float> plotBounds() const
    {
        return getLocalBounds().toFloat().reduced (4.0f, 3.0f);
    }

    juce::Point<float> toScreen (float x, float y) const
    {
        const auto r = plotBounds();
        return { r.getX() + x * r.getWidth(),
                 r.getY() + (1.0f - (y + 1.0f) * 0.5f) * r.getHeight() };
    }

    juce::Point<float> toNorm (juce::Point<float> screen) const
    {
        const auto r = plotBounds();
        const float x = juce::jlimit (0.0f, 1.0f, (screen.x - r.getX()) / juce::jmax (1.0f, r.getWidth()));
        const float yN = juce::jlimit (0.0f, 1.0f, (screen.y - r.getY()) / juce::jmax (1.0f, r.getHeight()));
        return { x, 1.0f - 2.0f * yN };
    }

    int hitTestPoint (juce::Point<float> screen, float radiusPx = 7.0f) const
    {
        const auto& pts = engine.uiCurve.getPoints();
        int best = -1;
        float bestD = radiusPx * radiusPx;
        for (int i = 0; i < (int) pts.size(); ++i)
        {
            const auto s = toScreen (pts[(size_t) i].x, pts[(size_t) i].y);
            const float d = s.getDistanceSquaredFrom (screen);
            if (d <= bestD)
            {
                bestD = d;
                best = i;
            }
        }
        return best;
    }

    void notifyChanged()
    {
        engine.publishFromUi();
        if (onCurveChanged != nullptr)
            onCurveChanged();
        repaint();
    }

    void showPointMenu (int index)
    {
        const auto& pts = engine.uiCurve.getPoints();
        if (index < 0 || index >= (int) pts.size())
            return;

        const bool soft = pts[(size_t) index].soft;
        juce::PopupMenu menu;
        menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
        menu.addItem (1, "Hard", true, ! soft);
        menu.addItem (2, "Soft", true, soft);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [safe = juce::Component::SafePointer<ShapeCurveEditor> (this), index] (int result)
                            {
                                if (safe == nullptr || result <= 0)
                                    return;
                                if (safe->engine.uiCurve.setPointSoft (index, result == 2))
                                    safe->notifyChanged();
                            });
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ShapeCurveEditor)
};
