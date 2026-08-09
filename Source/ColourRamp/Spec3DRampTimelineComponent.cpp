#include "Spec3DRampTimelineComponent.h"
#include "ColourRampBank.h"
#include "RampPresetPicker.h"
#include "../ComboBoxLookAndFeel.h"
#include <algorithm>
#include <cmath>

namespace
{
    constexpr float kEdgeHit = 6.0f;
    constexpr float kFadeHit = 10.0f;
    constexpr float kKeyHit = 8.0f;

    GradientRamp makeDefaultRamp()
    {
        GradientRamp r;
        r.mapMode = GradientRamp::MapMode::intensityLowToHigh;
        r.stops = { { 0.0f, juce::Colours::black }, { 1.0f, juce::Colours::white } };
        r.enabled = true;
        ++r.revision;
        return r;
    }

    /** Snapshot undo for Spec3DRampSequence (NLE-style history). */
    class RampSequenceUndoAction final : public juce::UndoableAction
    {
    public:
        RampSequenceUndoAction (Spec3DRampSequence& seqIn,
                                juce::ValueTree beforeIn,
                                juce::ValueTree afterIn,
                                std::function<void()> notifyIn)
            : seq (seqIn),
              before (std::move (beforeIn)),
              after (std::move (afterIn)),
              notify (std::move (notifyIn))
        {
        }

        bool perform() override
        {
            seq.applyValueTree (after);
            if (notify) notify();
            return true;
        }

        bool undo() override
        {
            seq.applyValueTree (before);
            if (notify) notify();
            return true;
        }

        int getSizeInUnits() override { return 8; }

    private:
        Spec3DRampSequence& seq;
        juce::ValueTree before, after;
        std::function<void()> notify;
    };

    // ── Square toolbar icons (UE5 Sequencer / Premiere / Resolve language) ──

    void paintSelectIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.20f, r.getHeight() * 0.12f);
        juce::Path p;
        p.startNewSubPath (c.getX() + c.getWidth() * 0.12f, c.getY());
        p.lineTo (c.getX() + c.getWidth() * 0.12f, c.getBottom());
        p.lineTo (c.getX() + c.getWidth() * 0.40f, c.getY() + c.getHeight() * 0.60f);
        p.lineTo (c.getX() + c.getWidth() * 0.52f, c.getBottom() - c.getHeight() * 0.08f);
        p.lineTo (c.getX() + c.getWidth() * 0.66f, c.getY() + c.getHeight() * 0.76f);
        p.lineTo (c.getRight(), c.getY() + c.getHeight() * 0.52f);
        p.closeSubPath();
        g.setColour (ink);
        g.fillPath (p);
    }

    void paintRazorIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.16f, r.getHeight() * 0.16f);
        // Vertical cut line
        g.setColour (ink.withAlpha (0.55f));
        g.drawLine (c.getCentreX(), c.getY(), c.getCentreX(), c.getBottom(), 1.15f);
        // Blade
        juce::Path blade;
        blade.startNewSubPath (c.getX() + c.getWidth() * 0.08f, c.getY() + c.getHeight() * 0.62f);
        blade.lineTo (c.getX() + c.getWidth() * 0.52f, c.getY() + c.getHeight() * 0.08f);
        blade.lineTo (c.getX() + c.getWidth() * 0.68f, c.getY() + c.getHeight() * 0.28f);
        blade.lineTo (c.getX() + c.getWidth() * 0.28f, c.getBottom() - c.getHeight() * 0.05f);
        blade.closeSubPath();
        g.setColour (ink);
        g.fillPath (blade);
        g.drawLine (c.getX() + c.getWidth() * 0.58f, c.getY() + c.getHeight() * 0.22f,
                    c.getRight() - 1.0f, c.getY() + c.getHeight() * 0.78f, 2.0f);
    }

    void paintUndoIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.20f, r.getHeight() * 0.20f);
        g.setColour (ink);
        juce::Path arc;
        arc.addCentredArc (c.getCentreX() + c.getWidth() * 0.06f, c.getCentreY(),
                           c.getWidth() * 0.40f, c.getHeight() * 0.40f,
                           0.0f, -2.5f, 1.15f, true);
        g.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        const float ax = c.getX() + c.getWidth() * 0.10f;
        const float ay = c.getY() + c.getHeight() * 0.26f;
        juce::Path head;
        head.addTriangle (ax, ay, ax + 4.5f, ay - 0.5f, ax + 1.2f, ay + 4.5f);
        g.fillPath (head);
    }

    void paintRedoIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.20f, r.getHeight() * 0.20f);
        g.setColour (ink);
        juce::Path arc;
        arc.addCentredArc (c.getCentreX() - c.getWidth() * 0.06f, c.getCentreY(),
                           c.getWidth() * 0.40f, c.getHeight() * 0.40f,
                           0.0f, 2.5f, -1.15f, true);
        g.strokePath (arc, juce::PathStrokeType (1.7f, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        const float ax = c.getRight() - c.getWidth() * 0.10f;
        const float ay = c.getY() + c.getHeight() * 0.26f;
        juce::Path head;
        head.addTriangle (ax, ay, ax - 4.5f, ay - 0.5f, ax - 1.2f, ay + 4.5f);
        g.fillPath (head);
    }

    void paintDupIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.20f, r.getHeight() * 0.20f);
        const float w = c.getWidth() * 0.55f;
        const float h = c.getHeight() * 0.55f;
        g.setColour (ink.withAlpha (0.45f));
        g.drawRoundedRectangle (c.getX(), c.getY(), w, h, 1.5f, 1.3f);
        g.setColour (ink);
        g.drawRoundedRectangle (c.getRight() - w, c.getBottom() - h, w, h, 1.5f, 1.5f);
    }

    void paintDeleteIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.18f);
        g.setColour (ink);
        // Lid
        g.drawLine (c.getX() + c.getWidth() * 0.15f, c.getY() + c.getHeight() * 0.28f,
                    c.getRight() - c.getWidth() * 0.15f, c.getY() + c.getHeight() * 0.28f, 1.5f);
        g.drawLine (c.getCentreX() - c.getWidth() * 0.12f, c.getY() + c.getHeight() * 0.12f,
                    c.getCentreX() + c.getWidth() * 0.12f, c.getY() + c.getHeight() * 0.12f, 1.4f);
        // Body
        juce::Path body;
        body.startNewSubPath (c.getX() + c.getWidth() * 0.22f, c.getY() + c.getHeight() * 0.32f);
        body.lineTo (c.getX() + c.getWidth() * 0.30f, c.getBottom());
        body.lineTo (c.getX() + c.getWidth() * 0.70f, c.getBottom());
        body.lineTo (c.getX() + c.getWidth() * 0.78f, c.getY() + c.getHeight() * 0.32f);
        body.closeSubPath();
        g.strokePath (body, juce::PathStrokeType (1.4f));
        g.drawLine (c.getCentreX(), c.getY() + c.getHeight() * 0.40f,
                    c.getCentreX(), c.getBottom() - c.getHeight() * 0.12f, 1.2f);
    }

    void paintArrowLeftIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.22f);
        g.setColour (ink);
        juce::Path p;
        p.addTriangle (c.getX(), c.getCentreY(),
                       c.getX() + c.getWidth() * 0.55f, c.getY(),
                       c.getX() + c.getWidth() * 0.55f, c.getBottom());
        g.fillPath (p);
        g.fillRoundedRectangle (c.getX() + c.getWidth() * 0.42f,
                                c.getCentreY() - c.getHeight() * 0.14f,
                                c.getWidth() * 0.55f, c.getHeight() * 0.28f, 1.0f);
    }

    void paintArrowRightIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.22f);
        g.setColour (ink);
        juce::Path p;
        p.addTriangle (c.getRight(), c.getCentreY(),
                       c.getRight() - c.getWidth() * 0.55f, c.getY(),
                       c.getRight() - c.getWidth() * 0.55f, c.getBottom());
        g.fillPath (p);
        g.fillRoundedRectangle (c.getX(),
                                c.getCentreY() - c.getHeight() * 0.14f,
                                c.getWidth() * 0.55f, c.getHeight() * 0.28f, 1.0f);
    }

    void paintPlusIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.28f, r.getHeight() * 0.28f);
        g.setColour (ink);
        const float t = juce::jmax (1.6f, c.getWidth() * 0.18f);
        g.fillRoundedRectangle (c.getCentreX() - t * 0.5f, c.getY(), t, c.getHeight(), 1.0f);
        g.fillRoundedRectangle (c.getX(), c.getCentreY() - t * 0.5f, c.getWidth(), t, 1.0f);
    }

    void paintAddLaneIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.18f, r.getHeight() * 0.20f);
        g.setColour (ink.withAlpha (0.9f));
        // Three track lines
        for (int i = 0; i < 3; ++i)
        {
            const float y = c.getY() + c.getHeight() * (0.18f + 0.28f * (float) i);
            g.drawLine (c.getX(), y, c.getX() + c.getWidth() * 0.58f, y, 1.35f);
        }
        // Plus on the right
        const float px = c.getX() + c.getWidth() * 0.72f;
        const float py = c.getCentreY();
        const float s = c.getWidth() * 0.22f;
        g.drawLine (px - s, py, px + s, py, 1.6f);
        g.drawLine (px, py - s, px, py + s, 1.6f);
    }

    void paintExpandIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink)
    {
        auto c = r.reduced (r.getWidth() * 0.22f, r.getHeight() * 0.22f);
        g.setColour (ink);
        const float L = c.getWidth() * 0.32f;
        auto corner = [&] (float x, float y, float dx, float dy)
        {
            g.drawLine (x, y, x + dx * L, y, 1.5f);
            g.drawLine (x, y, x, y + dy * L, 1.5f);
        };
        corner (c.getX(), c.getY(), 1, 1);
        corner (c.getRight(), c.getY(), -1, 1);
        corner (c.getX(), c.getBottom(), 1, -1);
        corner (c.getRight(), c.getBottom(), -1, -1);
    }

    void paintSeqEnableIcon (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour ink, bool on)
    {
        auto c = r.reduced (r.getWidth() * 0.20f, r.getHeight() * 0.20f);
        // Film strip / sequence block
        g.setColour (ink);
        g.drawRoundedRectangle (c.reduced (0.5f), 2.0f, 1.4f);
        const float midY = c.getCentreY();
        g.drawLine (c.getX() + 2.0f, midY, c.getRight() - 2.0f, midY, 1.2f);
        // Play triangle when on
        if (on)
        {
            juce::Path play;
            const float cx = c.getCentreX() + c.getWidth() * 0.04f;
            play.addTriangle (cx - c.getWidth() * 0.12f, midY - c.getHeight() * 0.22f,
                              cx - c.getWidth() * 0.12f, midY + c.getHeight() * 0.22f,
                              cx + c.getWidth() * 0.22f, midY);
            g.fillPath (play);
        }
        else
        {
            // Sprocket ticks
            for (int i = 0; i < 3; ++i)
            {
                const float x = c.getX() + c.getWidth() * (0.25f + 0.25f * (float) i);
                g.fillEllipse (x - 1.2f, c.getY() + 2.0f, 2.4f, 2.4f);
                g.fillEllipse (x - 1.2f, c.getBottom() - 4.4f, 2.4f, 2.4f);
            }
        }
    }

    /** Square graphic toolbar button — UE5 Sequencer / Premiere style. */
    class SeqToolButton final : public juce::Button
    {
    public:
        enum class Glyph
        {
            select, razor, undo, redo,
            duplicate, deleteClip, nudgeLeft, nudgeRight,
            addClip, addLane, expand, seqEnable
        };

        explicit SeqToolButton (Glyph g)
            : juce::Button ({}), glyph (g)
        {
            setMouseCursor (juce::MouseCursor::PointingHandCursor);
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            // Square face, tight radius (editor chrome — not pill labels).
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            const bool on = getToggleState();
            auto fill = findColour (on ? juce::TextButton::buttonOnColourId
                                       : juce::TextButton::buttonColourId);
            if (down) fill = fill.darker (0.15f);
            else if (over) fill = fill.brighter (0.10f);

            g.setColour (fill);
            g.fillRoundedRectangle (r, 2.0f);

            // Inner top edge highlight
            g.setColour (juce::Colours::white.withAlpha (on ? 0.14f : (over ? 0.10f : 0.06f)));
            g.drawLine (r.getX() + 1.0f, r.getY() + 1.0f, r.getRight() - 1.0f, r.getY() + 1.0f, 1.0f);

            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawRoundedRectangle (r, 2.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (on ? 0.22f : 0.10f));
            g.drawRoundedRectangle (r.reduced (0.5f), 1.5f, 1.0f);

            const auto ink = (on ? juce::Colours::black
                                 : findColour (juce::TextButton::textColourOffId))
                                 .withAlpha (isEnabled() ? (on ? 0.92f : 0.90f) : 0.32f);
            auto icon = r.reduced (r.getWidth() * 0.12f);
            switch (glyph)
            {
                case Glyph::select:      paintSelectIcon (g, icon, ink); break;
                case Glyph::razor:       paintRazorIcon (g, icon, ink); break;
                case Glyph::undo:        paintUndoIcon (g, icon, ink); break;
                case Glyph::redo:        paintRedoIcon (g, icon, ink); break;
                case Glyph::duplicate:   paintDupIcon (g, icon, ink); break;
                case Glyph::deleteClip:  paintDeleteIcon (g, icon, ink); break;
                case Glyph::nudgeLeft:   paintArrowLeftIcon (g, icon, ink); break;
                case Glyph::nudgeRight:  paintArrowRightIcon (g, icon, ink); break;
                case Glyph::addClip:     paintPlusIcon (g, icon, ink); break;
                case Glyph::addLane:     paintAddLaneIcon (g, icon, ink); break;
                case Glyph::expand:      paintExpandIcon (g, icon, ink); break;
                case Glyph::seqEnable:   paintSeqEnableIcon (g, icon, ink, on); break;
            }
        }

        Glyph glyph;
    };

    SeqToolButton* asTool (juce::Button* b) noexcept
    {
        return dynamic_cast<SeqToolButton*> (b);
    }
}

Spec3DRampTimelineComponent::Spec3DRampTimelineComponent (SharedResources& resourcesIn,
                                                          ColourRampBank& bankIn,
                                                          Spec3DRampSequence& sequenceIn)
    : resources (resourcesIn), theme (&resourcesIn), bank (bankIn), sequence (sequenceIn)
{
    setOpaque (false);
    setWantsKeyboardFocus (true);
    sequence.hydrateFromStore (bank.getPresets());

    auto makeTool = [this] (SeqToolButton::Glyph g, const juce::String& tip) -> std::unique_ptr<juce::Button>
    {
        auto b = std::make_unique<SeqToolButton> (g);
        b->setTooltip (tip);
        addAndMakeVisible (*b);
        return b;
    };

    enableButton = makeTool (SeqToolButton::Glyph::seqEnable,
                             "Enable sequencer (ramp morph + automation)");
    enableButton->setClickingTogglesState (true);
    enableButton->onClick = [this]
    {
        beginEditSnapshot();
        sequence.enabled = enableButton->getToggleState();
        if (onEnabledChanged) onEnabledChanged();
        commitEdit ("Seq enable");
        repaint();
    };

    lengthLabel.setText ("Length", juce::dontSendNotification);
    lengthLabel.setJustificationType (juce::Justification::centredRight);
    lengthLabel.setMinimumHorizontalScale (1.0f);
    lengthLabel.setTooltip ("Timeline length (0.5 s - 5 min)");
    addAndMakeVisible (lengthLabel);

    lengthSlider.setRange (Spec3DRampSequence::kMinLengthSec, Spec3DRampSequence::kMaxLengthSec, 0.01);
    lengthSlider.setSkewFactorFromMidPoint (8.0);
    lengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 52, 16);
    lengthSlider.setNumDecimalPlacesToDisplay (2);
    lengthSlider.setTooltip ("Timeline length (0.5 s - 5 min). Crossfades: drag the X between clips.");
    lengthSlider.textFromValueFunction = [] (double v)
    {
        if (v < 60.0) return juce::String (v, v < 10.0 ? 2 : 1) + "s";
        return juce::String (v / 60.0, 2) + "m";
    };
    lengthSlider.valueFromTextFunction = [] (const juce::String& t)
    {
        auto s = t.trim().toLowerCase();
        if (s.endsWithChar ('m')) return s.dropLastCharacters (1).getDoubleValue() * 60.0;
        if (s.endsWithChar ('s')) return s.dropLastCharacters (1).getDoubleValue();
        return s.getDoubleValue();
    };
    lengthSlider.onDragStart = [this] { beginEditSnapshot(); };
    lengthSlider.onDragEnd = [this] { commitDragEdit ("Timeline length"); };
    lengthSlider.onValueChange = [this]
    {
        sequence.lengthSec = (float) lengthSlider.getValue();
        sequence.clamp();
        rebuildCaches();
        repaint();
    };
    addAndMakeVisible (lengthSlider);

    // Tool groups (always created; visibility depends on expanded layout)
    selectToolButton = makeTool (SeqToolButton::Glyph::select,
                                 "Selection tool (V) — select, move, trim");
    selectToolButton->setClickingTogglesState (true);
    selectToolButton->setRadioGroupId (0x51E0);
    selectToolButton->onClick = [this] { setEditTool (EditTool::select); };

    razorToolButton = makeTool (SeqToolButton::Glyph::razor,
                                "Razor tool (C) — click clip to cut at mouse");
    razorToolButton->setClickingTogglesState (true);
    razorToolButton->setRadioGroupId (0x51E0);
    razorToolButton->onClick = [this] { setEditTool (EditTool::razor); };

    undoToolButton = makeTool (SeqToolButton::Glyph::undo, "Undo (Ctrl+Z)");
    undoToolButton->onClick = [this]
    {
        if (getUndoManager().canUndo())
            getUndoManager().undo();
        updateEditToolEnabled();
    };

    redoToolButton = makeTool (SeqToolButton::Glyph::redo, "Redo (Ctrl+Shift+Z)");
    redoToolButton->onClick = [this]
    {
        if (getUndoManager().canRedo())
            getUndoManager().redo();
        updateEditToolEnabled();
    };

    dupButton = makeTool (SeqToolButton::Glyph::duplicate, "Duplicate selected clip");
    dupButton->onClick = [this] { duplicateSelectedClip(); };

    deleteButton = makeTool (SeqToolButton::Glyph::deleteClip, "Delete selected clip (Del)");
    deleteButton->onClick = [this] { deleteSelectedClip(); };

    moveLeftButton = makeTool (SeqToolButton::Glyph::nudgeLeft, "Nudge clip earlier");
    moveLeftButton->onClick = [this] { moveSelectedClip (-1); };

    moveRightButton = makeTool (SeqToolButton::Glyph::nudgeRight, "Nudge clip later");
    moveRightButton->onClick = [this] { moveSelectedClip (1); };

    addButton = makeTool (SeqToolButton::Glyph::addClip, "Add ramp clip");
    addButton->onClick = [this] { showAddPresetPicker (addButton.get()); };

    addLaneButton = makeTool (SeqToolButton::Glyph::addLane, "Add lighting automation lane");
    addLaneButton->onClick = [this] { showAddLaneMenu(); };

    expandButton = makeTool (SeqToolButton::Glyph::expand, "Expand sequencer window");
    expandButton->onClick = [this] { if (onRequestExpand) onRequestExpand(); };

    styleChrome();
    enableButton->setToggleState (sequence.enabled, juce::dontSendNotification);
    lengthSlider.setValue (sequence.lengthSec, juce::dontSendNotification);
    rebuildCaches();
    setEditTool (EditTool::select);
    updateEditToolEnabled();
    startTimerHz (12);
}

Spec3DRampTimelineComponent::~Spec3DRampTimelineComponent()
{
    stopTimer();
    closeClipRampEditor (false);
    colourKeyPicker.reset();
}

void Spec3DRampTimelineComponent::setExpandedLayout (bool expanded) noexcept
{
    expandedLayout = expanded;
    updateEditToolEnabled();
    resized();
    repaint();
}

void Spec3DRampTimelineComponent::setShowExpandButton (bool shouldShow) noexcept
{
    showExpandButton = shouldShow;
    if (expandButton != nullptr)
        expandButton->setVisible (shouldShow);
    resized();
}

void Spec3DRampTimelineComponent::setPlayheadSec (float sec) noexcept
{
    if (std::abs (playheadSec - sec) < 1.0e-4f)
        return;
    playheadSec = sec;
    updateEditToolEnabled();
    if (isShowing())
        repaint();
}

void Spec3DRampTimelineComponent::clearRegionSelection() noexcept
{
    regionValid = false;
    regionInSec = regionOutSec = 0.0f;
    repaint();
}

void Spec3DRampTimelineComponent::setRegionSelection (float startSec, float endSec) noexcept
{
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    regionInSec = juce::jlimit (0.0f, len, startSec);
    regionOutSec = juce::jlimit (0.0f, len, endSec);
    regionValid = std::abs (regionOutSec - regionInSec) > 1.0e-3f;
    repaint();
}

juce::Rectangle<float> Spec3DRampTimelineComponent::getTrackContentBounds() const noexcept
{
    auto area = getTracksArea();
    area.removeFromLeft (labelColW() + 4.0f);
    return area;
}

float Spec3DRampTimelineComponent::timeToXFull (float t) const noexcept
{
    const auto area = getTrackContentBounds();
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    return area.getX() + area.getWidth() * (t / len);
}

float Spec3DRampTimelineComponent::xToTimeFull (float x) const noexcept
{
    const auto area = getTrackContentBounds();
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    const float n = (x - area.getX()) / juce::jmax (1.0f, area.getWidth());
    return juce::jlimit (0.0f, len, n * len);
}

bool Spec3DRampTimelineComponent::hitRegion (juce::Point<float> p) const noexcept
{
    if (! regionValid)
        return false;
    const auto area = getTrackContentBounds();
    if (! area.contains (p))
        return false;
    const float x0 = timeToXFull (getRegionStartSec());
    const float x1 = timeToXFull (getRegionEndSec());
    return p.x >= x0 && p.x <= x1;
}

void Spec3DRampTimelineComponent::paintRegionOverlay (juce::Graphics& g) const
{
    if (! regionValid)
        return;
    const auto& c = colors();
    const auto area = getTrackContentBounds();
    const float x0 = juce::jmax (area.getX(), timeToXFull (getRegionStartSec()));
    const float x1 = juce::jmin (area.getRight(), timeToXFull (getRegionEndSec()));
    if (x1 <= x0)
        return;
    auto r = juce::Rectangle<float> (x0, area.getY(), x1 - x0, area.getHeight());
    g.setColour (c.pluginButtonAccent.withAlpha (0.16f));
    g.fillRect (r);
    g.setColour (c.pluginButtonAccent.withAlpha (0.85f));
    g.fillRect (x0 - 1.0f, area.getY(), 2.0f, area.getHeight());
    g.fillRect (x1 - 1.0f, area.getY(), 2.0f, area.getHeight());
    if (r.getWidth() > 48.0f)
    {
        g.setColour (juce::Colours::white.withAlpha (0.75f));
        g.setFont (juce::FontOptions (10.0f));
        const float dur = getRegionEndSec() - getRegionStartSec();
        g.drawFittedText ("Export " + juce::String (dur, 2) + "s",
                          r.reduced (4.0f, 2.0f).toNearestInt(),
                          juce::Justification::centredTop, 1);
    }
}

int Spec3DRampTimelineComponent::getPreferredHeight() const noexcept
{
    // Single chrome row (compact or expanded).
    const float chrome = expandedLayout ? 28.0f : 24.0f;
    constexpr float kLaneGap = 5.0f;
    const float lanes = rampLaneH() + kLaneGap
                        + (float) sequence.autoLanes.size() * (autoLaneH() + kLaneGap)
                        + (clipEditor != nullptr ? 120.0f : 0.0f)
                        + (colourKeyPicker != nullptr ? (float) RampColorPickerPanel::kPreferredHeight + 4.0f : 0.0f);
    return juce::roundToInt (chrome + 6.0f + lanes + 4.0f);
}

const SharedColors& Spec3DRampTimelineComponent::colors() const noexcept
{
    return theme != nullptr ? theme->sharedColors : resources.sharedColors;
}

void Spec3DRampTimelineComponent::styleChrome()
{
    const auto& c = colors();
    // Flat square tool face — dark idle, gold active (Sequencer / Premiere).
    const auto face = c.pluginButtonBackground.brighter (0.06f);
    const auto faceOn = c.pluginButtonAccent;
    auto styleBtn = [&] (juce::Button* b)
    {
        if (b == nullptr) return;
        b->setColour (juce::TextButton::buttonColourId, face);
        b->setColour (juce::TextButton::buttonOnColourId, faceOn);
        b->setColour (juce::TextButton::textColourOffId, c.pluginButtonText.withAlpha (0.92f));
        b->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };
    juce::Button* all[] = {
        enableButton.get(), selectToolButton.get(), razorToolButton.get(),
        undoToolButton.get(), redoToolButton.get(), dupButton.get(), deleteButton.get(),
        moveLeftButton.get(), moveRightButton.get(), addButton.get(),
        addLaneButton.get(), expandButton.get()
    };
    for (auto* b : all)
        styleBtn (b);
    lengthLabel.setColour (juce::Label::textColourId, c.menuLabelTextColor1.withAlpha (0.85f));
    lengthSlider.setColour (juce::Slider::thumbColourId, c.menuSliderFillColor);
}

void Spec3DRampTimelineComponent::notifyChanged()
{
    rebuildCaches();
    if (onSequenceChanged)
        onSequenceChanged();
    if (getParentComponent() != nullptr)
        getParentComponent()->resized(); // window may need height
    resized();
}

void Spec3DRampTimelineComponent::rebuildCaches()
{
    sequence.buildLayout (layout);
    clipCaches.resize (sequence.clips.size());
}

int Spec3DRampTimelineComponent::findClipIndexAtTime (float timeSec) const noexcept
{
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    float t = timeSec;
    // Wrap into sequence length like evaluate does for looping playhead.
    if (len > 0.0f)
    {
        t = std::fmod (t, len);
        if (t < 0.0f)
            t += len;
    }
    for (const auto& L : layout)
    {
        if (t >= L.startSec && t < L.endSec)
            return L.index;
        // Last clip: include endSec.
        if (L.index == (int) layout.size() - 1 && t >= L.startSec && t <= L.endSec + 1.0e-4f)
            return L.index;
    }
    return -1;
}

void Spec3DRampTimelineComponent::setUndoManager (juce::UndoManager* um) noexcept
{
    undoManager = um != nullptr ? um : &ownedUndo;
    updateEditToolEnabled();
}

juce::UndoManager& Spec3DRampTimelineComponent::getUndoManager() noexcept
{
    return undoManager != nullptr ? *undoManager : ownedUndo;
}

void Spec3DRampTimelineComponent::beginEditSnapshot()
{
    editSnapshotBefore = sequence.toValueTree();
    editSnapshotValid = true;
}

void Spec3DRampTimelineComponent::commitEdit (const juce::String& name)
{
    if (! editSnapshotValid)
    {
        notifyChanged();
        updateEditToolEnabled();
        return;
    }
    editSnapshotValid = false;
    auto after = sequence.toValueTree();
    if (after.isEquivalentTo (editSnapshotBefore))
    {
        notifyChanged();
        updateEditToolEnabled();
        return;
    }

    auto& um = getUndoManager();
    um.beginNewTransaction (name);
    // perform() re-applies `after` (idempotent) and registers undo → `before`.
    um.perform (new RampSequenceUndoAction (
        sequence,
        editSnapshotBefore,
        after,
        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this)]
        {
            if (safe != nullptr)
                safe->refreshAfterUndoRedo();
        }));
    updateEditToolEnabled();
}

void Spec3DRampTimelineComponent::commitDragEdit (const juce::String& name)
{
    commitEdit (name);
}

void Spec3DRampTimelineComponent::refreshAfterUndoRedo()
{
    closeClipRampEditor (false);
    selectedClip = juce::jlimit (-1, (int) sequence.clips.size() - 1, selectedClip);
    if (enableButton != nullptr)
        enableButton->setToggleState (sequence.enabled, juce::dontSendNotification);
    lengthSlider.setValue (sequence.lengthSec, juce::dontSendNotification);
    rebuildCaches();
    if (onEnabledChanged)
        onEnabledChanged();
    if (onSequenceChanged)
        onSequenceChanged();
    updateEditToolEnabled();
    resized();
    repaint();
}

void Spec3DRampTimelineComponent::setEditTool (EditTool t)
{
    editTool = t;
    updateToolButtonToggles();
    updateEditToolEnabled();
    // Razor uses a crosshair over the track; selection uses normal cursor.
    if (editTool == EditTool::razor)
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
    repaint();
}

void Spec3DRampTimelineComponent::updateToolButtonToggles()
{
    if (selectToolButton != nullptr)
        selectToolButton->setToggleState (editTool == EditTool::select, juce::dontSendNotification);
    if (razorToolButton != nullptr)
        razorToolButton->setToggleState (editTool == EditTool::razor, juce::dontSendNotification);
    if (enableButton != nullptr)
        enableButton->setToggleState (sequence.enabled, juce::dontSendNotification);
}

bool Spec3DRampTimelineComponent::canSplitAtTime (float timeSec) const noexcept
{
    if (! sequence.canAddClip() || layout.empty())
        return false;
    const int i = findClipIndexAtTime (timeSec);
    if (i < 0 || i >= (int) layout.size())
        return false;
    const auto& L = layout[(size_t) i];
    const float body = L.endSec - L.startSec;
    if (body < 1.0e-3f)
        return false;
    const float t = juce::jlimit (0.0f, sequence.lengthSec, timeSec);
    // Industry: refuse cuts too close to edges (Premiere leaves a tiny residual otherwise).
    const float edge = juce::jmax (0.05f, body * 0.02f);
    if (t <= L.startSec + edge || t >= L.endSec - edge)
        return false;
    const float f = (t - L.startSec) / body;
    const float w = juce::jmax (Spec3DRampSequence::kMinWeight, sequence.clips[(size_t) i].weight);
    return (w * f) >= Spec3DRampSequence::kMinWeight
        && (w * (1.0f - f)) >= Spec3DRampSequence::kMinWeight;
}

bool Spec3DRampTimelineComponent::splitClipAtTime (float timeSec)
{
    if (! canSplitAtTime (timeSec))
        return false;
    const int i = findClipIndexAtTime (timeSec);
    if (i < 0 || i >= (int) sequence.clips.size() || i >= (int) layout.size())
        return false;

    const auto& L = layout[(size_t) i];
    const float body = juce::jmax (1.0e-4f, L.endSec - L.startSec);
    const float t = juce::jlimit (0.0f, sequence.lengthSec, timeSec);
    const float f = juce::jlimit (0.01f, 0.99f, (t - L.startSec) / body);
    const float w = juce::jmax (Spec3DRampSequence::kMinWeight, sequence.clips[(size_t) i].weight);

    closeClipRampEditor (true);

    Spec3DRampClip right = sequence.clips[(size_t) i];
    sequence.clips[(size_t) i].weight = juce::jmax (Spec3DRampSequence::kMinWeight, w * f);
    right.weight = juce::jmax (Spec3DRampSequence::kMinWeight, w * (1.0f - f));
    sequence.clips.insert (sequence.clips.begin() + i + 1, std::move (right));
    sequence.clamp();
    selectedClip = i + 1; // Premiere: right half selected after razor
    rebuildCaches();
    return true;
}

void Spec3DRampTimelineComponent::duplicateSelectedClip()
{
    if (selectedClip < 0 || selectedClip >= (int) sequence.clips.size() || ! sequence.canAddClip())
        return;
    beginEditSnapshot();
    closeClipRampEditor (true);
    Spec3DRampClip copy = sequence.clips[(size_t) selectedClip];
    if (! copy.displayName.endsWithIgnoreCase (" copy"))
        copy.displayName = copy.displayName + " copy";
    const int insertAt = selectedClip + 1;
    sequence.clips.insert (sequence.clips.begin() + insertAt, std::move (copy));
    sequence.clamp();
    selectedClip = insertAt;
    commitEdit ("Duplicate clip");
    repaint();
}

void Spec3DRampTimelineComponent::moveSelectedClip (int delta)
{
    if (delta == 0 || selectedClip < 0 || selectedClip >= (int) sequence.clips.size())
        return;
    const int n = (int) sequence.clips.size();
    const int target = selectedClip + delta;
    if (target < 0 || target >= n)
        return;
    beginEditSnapshot();
    closeClipRampEditor (true);
    std::swap (sequence.clips[(size_t) selectedClip], sequence.clips[(size_t) target]);
    selectedClip = target;
    commitEdit (delta < 0 ? "Nudge clip earlier" : "Nudge clip later");
    repaint();
}

void Spec3DRampTimelineComponent::deleteSelectedClip()
{
    if (selectedClip < 0 || selectedClip >= (int) sequence.clips.size())
        return;
    beginEditSnapshot();
    closeClipRampEditor (false);
    sequence.clips.erase (sequence.clips.begin() + selectedClip);
    selectedClip = -1;
    sequence.clamp();
    commitEdit ("Delete clip");
    repaint();
}

void Spec3DRampTimelineComponent::updateEditToolEnabled()
{
    const bool exp = expandedLayout;
    auto setVis = [exp] (std::unique_ptr<juce::Button>& b, bool show)
    {
        if (b != nullptr) b->setVisible (exp && show);
    };
    setVis (selectToolButton, true);
    setVis (razorToolButton, true);
    setVis (undoToolButton, true);
    setVis (redoToolButton, true);
    setVis (dupButton, true);
    setVis (moveLeftButton, true);
    setVis (moveRightButton, true);
    // delete / add / addLane / enable stay visible in compact too (layoutChrome).

    const int n = (int) sequence.clips.size();
    auto en = [] (std::unique_ptr<juce::Button>& b, bool e)
    {
        if (b != nullptr) b->setEnabled (e);
    };
    en (undoToolButton, getUndoManager().canUndo());
    en (redoToolButton, getUndoManager().canRedo());
    en (razorToolButton, sequence.canAddClip() && n > 0);
    en (dupButton, selectedClip >= 0 && selectedClip < n && sequence.canAddClip());
    en (deleteButton, selectedClip >= 0 && selectedClip < n);
    en (moveLeftButton, selectedClip > 0);
    en (moveRightButton, selectedClip >= 0 && selectedClip < n - 1);
    en (addButton, sequence.canAddClip());
    updateToolButtonToggles();
}

void Spec3DRampTimelineComponent::timerCallback()
{
    // Skip all work when not on-screen (expanded window closed / menu dismissed).
    if (! isShowing())
        return;

    if (playheadProvider)
        playheadSec = playheadProvider();
    if (sequence.enabled)
        repaint();
}

juce::Rectangle<float> Spec3DRampTimelineComponent::getTracksArea() const noexcept
{
    auto r = getLocalBounds().toFloat();
    // Match layoutChrome: single chrome row.
    const float chromeH = expandedLayout ? 28.0f : 24.0f;
    r.removeFromTop (chromeH + 4.0f);
    if (clipEditor != nullptr)
        r.removeFromBottom (120.0f);
    if (colourKeyPicker != nullptr)
        r.removeFromBottom ((float) RampColorPickerPanel::kPreferredHeight + 4.0f);
    return r.reduced (4.0f, 2.0f);
}

juce::Rectangle<float> Spec3DRampTimelineComponent::getRampLaneBounds() const noexcept
{
    auto area = getTracksArea();
    area.removeFromLeft (labelColW() + 4.0f);
    return area.removeFromTop (rampLaneH());
}

juce::Rectangle<float> Spec3DRampTimelineComponent::getAutoLaneBounds (int autoIdx) const noexcept
{
    auto area = getTracksArea();
    area.removeFromLeft (labelColW() + 4.0f);
    constexpr float kLaneGap = 5.0f;
    area.removeFromTop (rampLaneH() + kLaneGap);
    for (int i = 0; i < autoIdx; ++i)
        area.removeFromTop (autoLaneH() + kLaneGap);
    return area.removeFromTop (autoLaneH());
}

void Spec3DRampTimelineComponent::layoutChrome()
{
    // Single row of square icon buttons (UE5 Sequencer-style tool strip).
    const int chromeH = expandedLayout ? 28 : 24;
    auto row = getLocalBounds().removeFromTop (chromeH);
    const int icon = chromeH - 2; // perfect square hit target
    const int gap = 1;

    auto placeSq = [&] (std::unique_ptr<juce::Button>& b, bool fromLeft, bool visible)
    {
        if (b == nullptr) return;
        b->setVisible (visible);
        if (! visible) return;
        auto cell = fromLeft ? row.removeFromLeft (icon) : row.removeFromRight (icon);
        b->setBounds (cell.reduced (gap));
        if (fromLeft)
            row.removeFromLeft (1);
        else
            row.removeFromRight (1);
    };

    // Left: Enable | Select Razor | Undo Redo | Dup Delete | Nudge
    placeSq (enableButton, true, true);

    const bool tools = expandedLayout;
    placeSq (selectToolButton, true, tools);
    placeSq (razorToolButton, true, tools);
    if (tools) row.removeFromLeft (3); // group spacer
    placeSq (undoToolButton, true, tools);
    placeSq (redoToolButton, true, tools);
    if (tools) row.removeFromLeft (3);
    placeSq (dupButton, true, tools);
    placeSq (deleteButton, true, true); // compact + expanded
    if (tools) row.removeFromLeft (3);
    placeSq (moveLeftButton, true, tools);
    placeSq (moveRightButton, true, tools);

    // Right: Expand | +Lane | +clip
    placeSq (expandButton, false, showExpandButton);
    placeSq (addLaneButton, false, true);
    placeSq (addButton, false, true);


    const int lengthLabelW = juce::jmax (
        48, juce::roundToInt (juce::GlyphArrangement::getStringWidth (
                                  juce::Font (juce::FontOptions (12.0f)), "Length"))
                + 10);
    const int sliderW = expandedLayout ? 118 : 96;
    lengthSlider.setBounds (row.removeFromRight (sliderW).reduced (1, 3));
    lengthLabel.setBounds (row.removeFromRight (lengthLabelW).reduced (1, 2));

    if (enableButton != nullptr)
        enableButton->setToggleState (sequence.enabled, juce::dontSendNotification);
    lengthSlider.setValue (sequence.lengthSec, juce::dontSendNotification);
    updateEditToolEnabled();

    auto bottom = getLocalBounds();
    if (colourKeyPicker != nullptr)
    {
        auto pick = bottom.removeFromBottom (RampColorPickerPanel::kPreferredHeight + 4);
        colourKeyPicker->setBounds (pick.reduced (6, 2));
    }
    if (clipEditor != nullptr)
    {
        auto ed = bottom.removeFromBottom (120);
        clipEditor->setBounds (ed.reduced (6, 2));
    }
}

void Spec3DRampTimelineComponent::resized()
{
    layoutChrome();
    const auto lane = getRampLaneBounds();
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    for (size_t i = 0; i < layout.size() && i < clipCaches.size(); ++i)
    {
        const float x0 = lane.getX() + lane.getWidth() * (layout[i].startSec / len);
        const float x1 = lane.getX() + lane.getWidth() * (layout[i].endSec / len);
        clipCaches[i].bounds = { x0, lane.getY(), juce::jmax (2.0f, x1 - x0), lane.getHeight() };
    }
}

void Spec3DRampTimelineComponent::paintRampInClip (juce::Graphics& g, juce::Rectangle<float> r,
                                                   const GradientRamp& ramp) const
{
    paintRampSwatch (g, r, ramp, 4.0f);
}

void Spec3DRampTimelineComponent::paintRampLane (juce::Graphics& g)
{
    const auto& c = colors();
    const bool laneOn = sequence.rampLaneEnabled;
    auto full = getTracksArea();
    auto labelR = full.removeFromLeft (labelColW());
    labelR = labelR.withHeight (rampLaneH());
    g.setColour (c.menuBackgroundGradientColor1.withAlpha (laneOn ? 0.55f : 0.28f));
    g.fillRoundedRectangle (labelR, 4.0f);
    g.setColour (c.menuLabelTextColor1.withAlpha (laneOn ? 0.88f : 0.35f));
    g.setFont (juce::FontOptions (expandedLayout ? 12.0f : 11.0f).withStyle ("Bold"));
    g.drawFittedText ("Ramp", labelR.toNearestInt(), juce::Justification::centred, 1);
    if (! laneOn)
    {
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawFittedText ("off", labelR.reduced (2.0f).removeFromBottom (12).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    const auto lane = getRampLaneBounds();
    g.setColour (c.oscBackground.darker (0.15f).withAlpha (laneOn ? 0.92f : 0.45f));
    g.fillRoundedRectangle (lane, 7.0f);
    g.setColour (juce::Colours::white.withAlpha (laneOn ? 0.10f : 0.05f));
    g.drawRoundedRectangle (lane.reduced (0.5f), 7.0f, 1.0f);

    if (sequence.clips.empty())
    {
        const float d = expandedLayout ? 34.0f : 30.0f;
        auto circle = juce::Rectangle<float> (d, d).withCentre (lane.getCentre());
        const bool hot = circle.expanded (4.0f).contains (getMouseXYRelative().toFloat());
        g.setColour (c.pluginButtonAccent.withAlpha (hot ? 0.35f : 0.18f));
        g.fillEllipse (circle);
        g.setColour (c.pluginButtonAccent.withAlpha (hot ? 0.95f : 0.7f));
        g.drawEllipse (circle, 1.6f);
        g.setFont (juce::FontOptions (d * 0.55f));
        g.drawText ("+", circle.toNearestInt(), juce::Justification::centred, false);
        g.setColour (c.menuLabelTextColor1.withAlpha (0.4f));
        g.setFont (juce::FontOptions (10.5f));
        g.drawText ("Add ramp clip",
                    lane.withTrimmedTop (lane.getHeight() * 0.55f).toNearestInt(),
                    juce::Justification::centred, false);
        return;
    }

    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    for (size_t i = 0; i < sequence.clips.size(); ++i)
    {
        auto r = clipCaches[i].bounds;
        if (r.getWidth() < 1.0f) continue;

        if (sequence.clips[i].ramp.stops.size() >= 2)
            paintRampInClip (g, r.reduced (1.0f), sequence.clips[i].ramp);
        else
        {
            g.setColour (juce::Colours::darkgrey.withAlpha (0.5f));
            g.fillRoundedRectangle (r.reduced (1.0f), 4.0f);
        }

        if (r.getWidth() > 36.0f)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (r.reduced (1.0f).removeFromTop (expandedLayout ? 15.0f : 12.0f), 3.0f);
            g.setColour (juce::Colours::whitesmoke.withAlpha (0.92f));
            g.setFont (juce::FontOptions (expandedLayout ? 11.5f : 10.0f));
            g.drawFittedText (sequence.clips[i].displayName.isNotEmpty()
                                  ? sequence.clips[i].displayName : "Custom",
                              r.reduced (5.0f, 2.0f).toNearestInt(),
                              juce::Justification::centredLeft, 1);
        }

        g.setColour ((int) i == selectedClip ? c.scopeDropOutline.withAlpha (0.95f)
                                             : juce::Colours::white.withAlpha (0.18f));
        g.drawRoundedRectangle (r.reduced (0.5f), 4.0f, (int) i == selectedClip ? 1.6f : 1.0f);

        // Crossfade zone at clip end: washed overlay + Ableton-style X, drag to resize.
        if (i < layout.size() && layout[i].fadeOutSec > 1.0e-4f)
        {
            const float fadePx = juce::jmax (4.0f, lane.getWidth() * (layout[i].fadeOutSec / len));
            auto fadeR = juce::Rectangle<float> (r.getRight() - fadePx, r.getY() + 1.0f,
                                                fadePx, r.getHeight() - 2.0f);
            // Slightly lighter / washed so the fade region is obvious on the ramp art.
            g.setColour (juce::Colours::white.withAlpha (0.22f));
            g.fillRect (fadeR);
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.fillRect (fadeR);

            const float inset = 3.0f;
            const float x0 = fadeR.getX() + inset;
            const float x1 = fadeR.getRight() - inset;
            const float y0 = fadeR.getY() + inset;
            const float y1 = fadeR.getBottom() - inset;
            g.setColour (c.scopeDropOutline.withAlpha (0.92f));
            g.drawLine (x0, y0, x1, y1, 1.6f); // \
            g.drawLine (x0, y1, x1, y0, 1.6f); // /
            // Centre hit cue
            g.setColour (c.scopeDropOutline.withAlpha (0.75f));
            g.fillEllipse (fadeR.getCentreX() - 2.5f, fadeR.getCentreY() - 2.5f, 5.0f, 5.0f);
        }
    }

    if (dragMode == DragMode::reorder && dropInsertAt >= 0)
    {
        float x = lane.getX();
        if (dropInsertAt < (int) layout.size())
            x = lane.getX() + lane.getWidth() * (layout[(size_t) dropInsertAt].startSec / len);
        else if (! layout.empty())
            x = lane.getRight();
        g.setColour (c.scopeDropOutline);
        g.fillRect (x - 1.5f, lane.getY(), 3.0f, lane.getHeight());
    }

    if (dragMode == DragMode::reorder && dragClip >= 0 && dragClip < (int) clipCaches.size())
    {
        auto ghost = clipCaches[(size_t) dragClip].bounds;
        ghost.setX (getMouseXYRelative().x - dragGrabX);
        g.setColour (c.scopeDropOutline.withAlpha (0.2f));
        g.fillRoundedRectangle (ghost, 4.0f);
        g.setColour (c.scopeDropOutline.withAlpha (0.85f));
        g.drawRoundedRectangle (ghost, 4.0f, 1.5f);
    }

    if (! laneOn)
    {
        // Veil after clip art so muted lane reads grey but stays editable.
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.fillRoundedRectangle (lane, 7.0f);
    }
}

void Spec3DRampTimelineComponent::paintAutoLane (juce::Graphics& g, int autoIdx)
{
    if (! juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
        return;

    const auto& c = colors();
    const auto& lane = sequence.autoLanes[(size_t) autoIdx];
    const bool laneOn = lane.enabled;
    auto full = getTracksArea();
    // Separator above each auto lane (and under the ramp lane).
    const float gap = 5.0f;
    float y = full.getY() + rampLaneH() + gap + (float) autoIdx * (autoLaneH() + gap);
    auto labelR = juce::Rectangle<float> (full.getX(), y, labelColW(), autoLaneH());
    auto track = juce::Rectangle<float> (full.getX() + labelColW() + 4.0f, y,
                                         full.getWidth() - labelColW() - 4.0f, autoLaneH());

    // Lane separator line
    {
        const float sepY = y - gap * 0.5f;
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawLine (full.getX(), sepY, full.getRight(), sepY, 1.0f);
        g.setColour (juce::Colours::black.withAlpha (0.35f));
        g.drawLine (full.getX(), sepY + 1.0f, full.getRight(), sepY + 1.0f, 1.0f);
    }

    g.setColour (c.menuBackgroundGradientColor1.darker (0.25f).withAlpha (laneOn ? 0.75f : 0.35f));
    g.fillRoundedRectangle (labelR, 4.0f);
    g.setColour (c.menuLabelTextColor1.withAlpha (laneOn ? 0.85f : 0.32f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText (lane.label, labelR.reduced (2.0f).toNearestInt(), juce::Justification::centred, 2);
    if (! laneOn)
    {
        g.setColour (juce::Colours::white.withAlpha (0.14f));
        g.setFont (juce::FontOptions (9.0f));
        g.drawFittedText ("off", labelR.reduced (2.0f).removeFromBottom (11).toNearestInt(),
                          juce::Justification::centred, 1);
    }

    // Darker well so the envelope line reads clearly.
    g.setColour (juce::Colour::fromRGB (8, 8, 10).withAlpha (laneOn ? 0.94f : 0.55f));
    g.fillRoundedRectangle (track, 6.0f);
    g.setColour (juce::Colours::white.withAlpha (laneOn ? 0.07f : 0.04f));
    g.drawRoundedRectangle (track.reduced (0.5f), 6.0f, 1.0f);

    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);

    if (lane.isColourLane())
    {
        const auto& env = lane.colourEnv;
        // Default flat line at mid (RGB baseline); user adds coloured keys.
        g.setColour (juce::Colours::white.withAlpha (0.28f));
        g.drawLine (track.getX() + 2.0f, track.getCentreY(),
                    track.getRight() - 2.0f, track.getCentreY(), 1.35f);

        for (size_t i = 0; i < env.keys.size(); ++i)
        {
            const float x = track.getX() + track.getWidth() * (env.keys[i].timeSec / len);
            if (i + 1 < env.keys.size())
            {
                const float x2 = track.getX() + track.getWidth() * (env.keys[i + 1].timeSec / len);
                juce::ColourGradient grad (env.keys[i].colour, x, track.getCentreY(),
                                           env.keys[i + 1].colour, x2, track.getCentreY(), false);
                g.setGradientFill (grad);
                g.drawLine (x, track.getCentreY(), x2, track.getCentreY(), 2.5f);
            }
            const float r = (selectedAutoLane == autoIdx && selectedKey == (int) i) ? 5.5f : 4.5f;
            g.setColour (env.keys[i].colour);
            g.fillEllipse (x - r, track.getCentreY() - r, r * 2.0f, r * 2.0f);
            g.setColour (juce::Colours::white.withAlpha (0.75f));
            g.drawEllipse (x - r, track.getCentreY() - r, r * 2.0f, r * 2.0f, 1.0f);
        }
    }
    else
    {
        const auto& env = lane.floatEnv;
        const float span = juce::jmax (1.0e-4f, env.maxV - env.minV);
        auto yAt = [&] (float v)
        {
            const float n = (v - env.minV) / span;
            return track.getBottom() - 4.0f - n * (track.getHeight() - 8.0f);
        };

        // Always show a default envelope line at value 0 when in range, else at min.
        const float baseV = (env.minV <= 0.0f && 0.0f <= env.maxV) ? 0.0f : env.minV;
        const float baseY = yAt (baseV);
        g.setColour (juce::Colours::white.withAlpha (0.30f));
        g.drawLine (track.getX() + 2.0f, baseY, track.getRight() - 2.0f, baseY, 1.35f);

        if (! env.keys.empty())
        {
            juce::Path path;
            for (size_t i = 0; i < env.keys.size(); ++i)
            {
                const float x = track.getX() + track.getWidth() * (env.keys[i].timeSec / len);
                const float yy = yAt (env.keys[i].value);
                if (i == 0) path.startNewSubPath (x, yy);
                else path.lineTo (x, yy);

                if (env.keys[i].interp == Spec3DKeyInterp::bezier
                    && (selectedAutoLane == autoIdx && selectedKey == (int) i))
                {
                    g.setColour (c.scopeDropOutline.withAlpha (0.55f));
                    const float hx = 18.0f;
                    g.drawLine (x, yy, x + hx, yy - env.keys[i].outTangent * 0.05f * track.getHeight(), 1.0f);
                    g.drawLine (x, yy, x - hx, yy - env.keys[i].inTangent * 0.05f * track.getHeight(), 1.0f);
                    g.fillEllipse (x + hx - 2.5f, yy - env.keys[i].outTangent * 0.05f * track.getHeight() - 2.5f, 5, 5);
                    g.fillEllipse (x - hx - 2.5f, yy - env.keys[i].inTangent * 0.05f * track.getHeight() - 2.5f, 5, 5);
                }
            }
            g.setColour (c.pluginButtonAccent.withAlpha (0.95f));
            g.strokePath (path, juce::PathStrokeType (1.8f));
            for (size_t i = 0; i < env.keys.size(); ++i)
            {
                const float x = track.getX() + track.getWidth() * (env.keys[i].timeSec / len);
                const float yy = yAt (env.keys[i].value);
                const bool sel = selectedAutoLane == autoIdx && selectedKey == (int) i;
                g.setColour (sel ? c.scopeDropOutline : c.pluginButtonAccent.brighter (0.2f));
                g.fillEllipse (x - 4.0f, yy - 4.0f, 8.0f, 8.0f);
                g.setColour (juce::Colours::black.withAlpha (0.35f));
                g.drawEllipse (x - 4.0f, yy - 4.0f, 8.0f, 8.0f, 1.0f);
            }
        }
    }

    if (! laneOn)
    {
        g.setColour (juce::Colours::black.withAlpha (0.42f));
        g.fillRoundedRectangle (track, 6.0f);
    }
}

void Spec3DRampTimelineComponent::paint (juce::Graphics& g)
{
    styleChrome();
    paintRampLane (g);
    for (int i = 0; i < (int) sequence.autoLanes.size(); ++i)
        paintAutoLane (g, i);

    paintRegionOverlay (g);

    // Playhead across all tracks
    if (sequence.enabled)
    {
        const auto& c = colors();
        auto area = getTrackContentBounds();
        const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
        float t = playheadSec;
        t = std::fmod (t, len);
        if (t < 0.0f) t += len;
        const float x = area.getX() + area.getWidth() * (t / len);
        g.setColour (c.pluginButtonAccent.withAlpha (0.9f));
        g.fillRect (x - 0.75f, area.getY(), 1.5f, area.getHeight());
    }
}

bool Spec3DRampTimelineComponent::hitEmptyAdd (juce::Point<float> p) const noexcept
{
    if (! sequence.clips.empty())
        return false;
    const auto lane = getRampLaneBounds();
    const float d = expandedLayout ? 34.0f : 30.0f;
    return juce::Rectangle<float> (d, d).withCentre (lane.getCentre()).expanded (6.0f).contains (p);
}

int Spec3DRampTimelineComponent::hitClipBody (juce::Point<float> p) const noexcept
{
    for (int i = (int) clipCaches.size(); --i >= 0;)
        if (clipCaches[(size_t) i].bounds.reduced (kEdgeHit, 0.0f).contains (p))
            return i;
    return -1;
}

int Spec3DRampTimelineComponent::hitClipEdge (juce::Point<float> p, bool& leftEdge) const noexcept
{
    for (int i = 0; i < (int) clipCaches.size(); ++i)
    {
        const auto& r = clipCaches[(size_t) i].bounds;
        if (p.y < r.getY() || p.y > r.getBottom()) continue;
        if (std::abs (p.x - r.getX()) <= kEdgeHit) { leftEdge = true; return i; }
        if (std::abs (p.x - r.getRight()) <= kEdgeHit) { leftEdge = false; return i; }
    }
    return -1;
}

int Spec3DRampTimelineComponent::hitFadeHandle (juce::Point<float> p) const noexcept
{
    const auto lane = getRampLaneBounds();
    if (! lane.expanded (0.0f, 2.0f).contains (p))
        return -1;

    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    for (int i = 0; i < (int) layout.size(); ++i)
    {
        const float xRight = lane.getX() + lane.getWidth() * (layout[(size_t) i].endSec / len);
        const float xLeft  = lane.getX() + lane.getWidth() * (layout[(size_t) i].startSec / len);
        const float fadeSec = layout[(size_t) i].fadeOutSec;
        const float fadePx = juce::jmax (0.0f, lane.getWidth() * (fadeSec / len));

        // Left edge of the fade zone (or just inside the clip end when fade is 0)
        // — drag this to resize. Always hittable so fades can be created from zero.
        const float fadeStartX = (fadeSec > 1.0e-4f)
                                     ? (xRight - fadePx)
                                     : (xRight - kFadeHit * 1.5f);
        const float cx = (fadeSec > 1.0e-4f) ? (xRight - fadePx * 0.5f) : fadeStartX;

        if (p.x < xLeft - 1.0f || p.x > xRight + 1.0f)
            continue;
        if (p.y < lane.getY() || p.y > lane.getBottom())
            continue;

        if (std::abs (p.x - fadeStartX) <= kFadeHit || std::abs (p.x - cx) <= kFadeHit)
            return i;

        // Anywhere inside an existing fade region is also a grab target.
        if (fadeSec > 1.0e-4f && p.x >= fadeStartX - 1.0f && p.x <= xRight + 1.0f)
            return i;
    }
    return -1;
}

juce::Rectangle<float> Spec3DRampTimelineComponent::getRampLabelBounds() const noexcept
{
    auto full = getTracksArea();
    return { full.getX(), full.getY(), labelColW(), rampLaneH() };
}

juce::Rectangle<float> Spec3DRampTimelineComponent::getAutoLaneLabelBounds (int autoIdx) const noexcept
{
    const auto track = getAutoLaneBounds (autoIdx);
    return { getTracksArea().getX(), track.getY(), labelColW(), autoLaneH() };
}

bool Spec3DRampTimelineComponent::hitRampLabel (juce::Point<float> p) const noexcept
{
    return getRampLabelBounds().contains (p);
}

bool Spec3DRampTimelineComponent::hitAutoLaneLabel (int autoIdx, juce::Point<float> p) const noexcept
{
    if (! juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
        return false;
    return getAutoLaneLabelBounds (autoIdx).contains (p);
}

int Spec3DRampTimelineComponent::hitAutoLane (juce::Point<float> p) const noexcept
{
    for (int i = 0; i < (int) sequence.autoLanes.size(); ++i)
        if (getAutoLaneBounds (i).expanded (0, 1).contains (p)
            || getAutoLaneLabelBounds (i).contains (p))
            return i;
    return -1;
}

int Spec3DRampTimelineComponent::hitFloatKey (int autoIdx, juce::Point<float> p) const noexcept
{
    if (! juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
        return -1;
    const auto& lane = sequence.autoLanes[(size_t) autoIdx];
    if (lane.isColourLane()) return -1;
    const auto track = getAutoLaneBounds (autoIdx);
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    const float span = juce::jmax (1.0e-4f, lane.floatEnv.maxV - lane.floatEnv.minV);
    for (int i = 0; i < (int) lane.floatEnv.keys.size(); ++i)
    {
        const float x = track.getX() + track.getWidth() * (lane.floatEnv.keys[(size_t) i].timeSec / len);
        const float n = (lane.floatEnv.keys[(size_t) i].value - lane.floatEnv.minV) / span;
        const float yy = track.getBottom() - 4.0f - n * (track.getHeight() - 8.0f);
        if (juce::Point<float> (x, yy).getDistanceFrom (p) <= kKeyHit)
            return i;
    }
    return -1;
}

int Spec3DRampTimelineComponent::hitColourKey (int autoIdx, juce::Point<float> p) const noexcept
{
    if (! juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
        return -1;
    const auto& lane = sequence.autoLanes[(size_t) autoIdx];
    if (! lane.isColourLane()) return -1;
    const auto track = getAutoLaneBounds (autoIdx);
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    for (int i = 0; i < (int) lane.colourEnv.keys.size(); ++i)
    {
        const float x = track.getX() + track.getWidth() * (lane.colourEnv.keys[(size_t) i].timeSec / len);
        if (juce::Point<float> (x, track.getCentreY()).getDistanceFrom (p) <= kKeyHit)
            return i;
    }
    return -1;
}

void Spec3DRampTimelineComponent::toggleLaneEnabled (int autoIdx)
{
    beginEditSnapshot();
    if (autoIdx < 0)
    {
        sequence.rampLaneEnabled = ! sequence.rampLaneEnabled;
    }
    else if (juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
    {
        auto& lane = sequence.autoLanes[(size_t) autoIdx];
        lane.enabled = ! lane.enabled;
    }
    else
    {
        editSnapshotValid = false;
        return;
    }
    commitEdit ("Toggle lane");
    repaint();
}

bool Spec3DRampTimelineComponent::keyPressed (const juce::KeyPress& key)
{
    // Premiere / Resolve-style hotkeys (expanded timeline).
    if (key == juce::KeyPress ('v') || key == juce::KeyPress ('V'))
    {
        setEditTool (EditTool::select);
        return true;
    }
    if (key == juce::KeyPress ('c') || key == juce::KeyPress ('C'))
    {
        // C alone = Razor tool. Ctrl+C is not used (no system clipboard yet).
        if (! key.getModifiers().isCommandDown() && ! key.getModifiers().isCtrlDown())
        {
            setEditTool (EditTool::razor);
            return true;
        }
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        deleteSelectedClip();
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (getUndoManager().canUndo())
            getUndoManager().undo();
        updateEditToolEnabled();
        return true;
    }
    if (key == juce::KeyPress ('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('z', juce::ModifierKeys::ctrlModifier | juce::ModifierKeys::shiftModifier, 0)
        || key == juce::KeyPress ('y', juce::ModifierKeys::commandModifier, 0)
        || key == juce::KeyPress ('y', juce::ModifierKeys::ctrlModifier, 0))
    {
        if (getUndoManager().canRedo())
            getUndoManager().redo();
        updateEditToolEnabled();
        return true;
    }
    return false;
}

void Spec3DRampTimelineComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;
    grabKeyboardFocus();

    // Lane labels: LMB toggle enable, RMB menu (enable/disable + remove for auto)
    if (e.mods.isPopupMenu())
    {
        // Prefer export on a selected region (NLE-style right-click range).
        if (hitRegion (p) || (regionValid && getTrackContentBounds().contains (p)
                              && hitClipBody (p) < 0 && hitAutoLane (p) < 0))
        {
            showRegionContextMenu (e.getScreenPosition());
            return;
        }
        if (hitRampLabel (p))
        {
            showLaneLabelMenu (-1, e.getScreenPosition());
            return;
        }
        const int al = hitAutoLane (p);
        if (al >= 0)
        {
            if (hitAutoLaneLabel (al, p))
            {
                showLaneLabelMenu (al, e.getScreenPosition());
                return;
            }
            const int ck = hitColourKey (al, p);
            const int fk = hitFloatKey (al, p);
            if (ck >= 0) { showKeyInterpMenu (al, ck, true, e.getScreenPosition()); return; }
            if (fk >= 0) { showKeyInterpMenu (al, fk, false, e.getScreenPosition()); return; }
        }
        const int body = hitClipBody (p);
        if (body >= 0)
        {
            showClipContextMenu (body, e.getScreenPosition());
            return;
        }
        // Empty track: allow export menu if a region exists, else start nothing.
        if (regionValid && getTrackContentBounds().contains (p))
        {
            showRegionContextMenu (e.getScreenPosition());
            return;
        }
        return;
    }

    if (hitRampLabel (p))
    {
        toggleLaneEnabled (-1);
        return;
    }

    if (hitEmptyAdd (p))
    {
        showAddPresetPicker (this);
        return;
    }

    // ── Razor tool (C): cut at mouse X — industry standard blade behaviour ──
    if (editTool == EditTool::razor && expandedLayout)
    {
        // Only cut on the ramp clip lane (not automation keys).
        if (getRampLaneBounds().contains (p) || hitClipBody (p) >= 0)
        {
            const float t = xToTimeFull (p.x);
            if (canSplitAtTime (t))
            {
                beginEditSnapshot();
                if (splitClipAtTime (t))
                    commitEdit ("Razor cut");
                else
                    editSnapshotValid = false;
            }
            repaint();
            return;
        }
        // Click elsewhere with razor: no-op (don't start selection drag).
        return;
    }

    bool left = false;
    int edge = hitClipEdge (p, left);
    int fade = hitFadeHandle (p);
    int body = hitClipBody (p);
    int al = hitAutoLane (p);

    // LMB on auto-lane label -> mute/unmute (don't start key drag)
    if (al >= 0 && hitAutoLaneLabel (al, p))
    {
        toggleLaneEnabled (al);
        return;
    }

    if (fade >= 0)
    {
        beginEditSnapshot();
        dragMode = DragMode::fade;
        dragClip = fade;
        dragStartFade = sequence.clips[(size_t) fade].crossfadeOutSec;
        selectedClip = fade;
    }
    else if (edge >= 0)
    {
        beginEditSnapshot();
        dragMode = left ? DragMode::resizeLeft : DragMode::resizeRight;
        dragClip = edge;
        dragStartWeight = sequence.clips[(size_t) edge].weight;
        if (left && edge > 0)
            dragStartNeighbourWeight = sequence.clips[(size_t) edge - 1].weight;
        else if (! left && edge + 1 < (int) sequence.clips.size())
            dragStartNeighbourWeight = sequence.clips[(size_t) edge + 1].weight;
        else
            dragStartNeighbourWeight = dragStartWeight;
        selectedClip = edge;
    }
    else if (body >= 0)
    {
        beginEditSnapshot();
        dragMode = DragMode::reorder;
        dragClip = body;
        dragGrabX = p.x - clipCaches[(size_t) body].bounds.getX();
        selectedClip = body;
        dropInsertAt = body;
        closeClipRampEditor (true);
    }
    else if (al >= 0)
    {
        selectedAutoLane = al;
        int k = hitFloatKey (al, p);
        if (k < 0) k = hitColourKey (al, p);
        if (k >= 0)
        {
            beginEditSnapshot();
            selectedKey = k;
            dragMode = sequence.autoLanes[(size_t) al].isColourLane() ? DragMode::colourKey : DragMode::floatKey;
            dragAutoLane = al;
            dragKey = k;
            if (sequence.autoLanes[(size_t) al].isColourLane())
            {
                dragStartKeyT = sequence.autoLanes[(size_t) al].colourEnv.keys[(size_t) k].timeSec;
            }
            else
            {
                dragStartKeyT = sequence.autoLanes[(size_t) al].floatEnv.keys[(size_t) k].timeSec;
                dragStartKeyV = sequence.autoLanes[(size_t) al].floatEnv.keys[(size_t) k].value;
            }
        }
        else
        {
            selectedKey = -1;
            selectedClip = -1;
        }
    }
    else if (getTrackContentBounds().contains (p)
             && hitClipBody (p) < 0 && hitAutoLane (p) < 0 && ! hitEmptyAdd (p))
    {
        // Drag empty track content to select an export region (NLE range).
        selectedClip = -1;
        selectedKey = -1;
        selectedAutoLane = -1;
        closeClipRampEditor (true);
        dragMode = DragMode::selectRegion;
        regionInSec = regionOutSec = xToTimeFull (p.x);
        regionValid = false;
    }
    else
    {
        selectedClip = -1;
        selectedKey = -1;
        selectedAutoLane = -1;
        closeClipRampEditor (true);
    }
    updateEditToolEnabled();
    repaint();
}

void Spec3DRampTimelineComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none) return;
    const auto lane = getRampLaneBounds();
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    const float dx = (float) e.getDistanceFromDragStartX();
    const float dSec = (dx / juce::jmax (1.0f, lane.getWidth())) * len;

    if (dragMode == DragMode::selectRegion)
    {
        regionOutSec = xToTimeFull (e.position.x);
        regionValid = std::abs (regionOutSec - regionInSec) > 1.0e-3f;
        repaint();
        return;
    }

    if (dragMode == DragMode::fade && dragClip >= 0
        && dragClip < (int) sequence.clips.size())
    {
        // Drag left (negative dSec) lengthens the fade from the clip end; right shortens.
        // Cap at full clip body so a fade can span the entire clip.
        float body = sequence.lengthSec;
        if (dragClip < (int) layout.size())
            body = juce::jmax (1.0e-4f,
                               layout[(size_t) dragClip].endSec - layout[(size_t) dragClip].startSec);
        const float next = juce::jmax (0.0f, dragStartFade - dSec);
        sequence.clips[(size_t) dragClip].crossfadeOutSec = juce::jlimit (0.0f, body, next);
        sequence.clamp();
        rebuildCaches();
        resized();
        repaint();
        return;
    }
    if (dragMode == DragMode::resizeRight && dragClip + 1 < (int) sequence.clips.size())
    {
        const float total = dragStartWeight + dragStartNeighbourWeight;
        float w = juce::jlimit (Spec3DRampSequence::kMinWeight, total - Spec3DRampSequence::kMinWeight,
                                dragStartWeight + dSec * 0.12f);
        sequence.clips[(size_t) dragClip].weight = w;
        sequence.clips[(size_t) dragClip + 1].weight = total - w;
        rebuildCaches();
        resized();
        repaint();
        return;
    }
    if (dragMode == DragMode::resizeLeft && dragClip > 0)
    {
        const float total = dragStartNeighbourWeight + dragStartWeight;
        float w = juce::jlimit (Spec3DRampSequence::kMinWeight, total - Spec3DRampSequence::kMinWeight,
                                dragStartWeight - dSec * 0.12f);
        sequence.clips[(size_t) dragClip].weight = w;
        sequence.clips[(size_t) dragClip - 1].weight = total - w;
        rebuildCaches();
        resized();
        repaint();
        return;
    }
    if (dragMode == DragMode::reorder)
    {
        dropInsertAt = (int) sequence.clips.size();
        for (int i = 0; i < (int) clipCaches.size(); ++i)
            if (e.position.x < clipCaches[(size_t) i].bounds.getCentreX())
            { dropInsertAt = i; break; }
        repaint();
        return;
    }
    if ((dragMode == DragMode::floatKey || dragMode == DragMode::colourKey)
        && juce::isPositiveAndBelow (dragAutoLane, (int) sequence.autoLanes.size())
        && dragKey >= 0)
    {
        auto track = getAutoLaneBounds (dragAutoLane);
        float t = juce::jlimit (0.0f, len,
                                (e.position.x - track.getX()) / juce::jmax (1.0f, track.getWidth()) * len);
        auto& al = sequence.autoLanes[(size_t) dragAutoLane];
        if (dragMode == DragMode::colourKey && dragKey < (int) al.colourEnv.keys.size())
        {
            al.colourEnv.keys[(size_t) dragKey].timeSec = t;
            al.colourEnv.sortKeys();
        }
        else if (dragMode == DragMode::floatKey && dragKey < (int) al.floatEnv.keys.size())
        {
            al.floatEnv.keys[(size_t) dragKey].timeSec = t;
            const float span = juce::jmax (1.0e-4f, al.floatEnv.maxV - al.floatEnv.minV);
            const float n = 1.0f - juce::jlimit (0.0f, 1.0f,
                (e.position.y - (track.getY() + 4.0f)) / juce::jmax (1.0f, track.getHeight() - 8.0f));
            al.floatEnv.keys[(size_t) dragKey].value = al.floatEnv.minV + n * span;
            al.floatEnv.sortKeys();
        }
        repaint();
    }
}

void Spec3DRampTimelineComponent::mouseUp (const juce::MouseEvent&)
{
    if (dragMode == DragMode::selectRegion)
    {
        if (std::abs (regionOutSec - regionInSec) <= 1.0e-3f)
            clearRegionSelection();
        else
            regionValid = true;
        dragMode = DragMode::none;
        repaint();
        return;
    }

    if (dragMode == DragMode::reorder && dragClip >= 0 && dropInsertAt >= 0)
    {
        int from = dragClip, insertAt = dropInsertAt;
        if (insertAt != from && insertAt != from + 1)
        {
            auto clip = sequence.clips[(size_t) from];
            sequence.clips.erase (sequence.clips.begin() + from);
            if (insertAt > from) --insertAt;
            insertAt = juce::jlimit (0, (int) sequence.clips.size(), insertAt);
            sequence.clips.insert (sequence.clips.begin() + insertAt, std::move (clip));
            selectedClip = insertAt;
            commitDragEdit ("Reorder clip");
        }
        else
            editSnapshotValid = false; // click without move
    }
    else if (dragMode == DragMode::fade)
        commitDragEdit ("Crossfade");
    else if (dragMode == DragMode::resizeLeft || dragMode == DragMode::resizeRight)
        commitDragEdit ("Trim clip");
    else if (dragMode == DragMode::floatKey || dragMode == DragMode::colourKey)
        commitDragEdit ("Move keyframe");
    else
        editSnapshotValid = false;

    dragMode = DragMode::none;
    dragClip = -1;
    dropInsertAt = -1;
    rebuildCaches();
    updateEditToolEnabled();
    resized();
    repaint();
}

void Spec3DRampTimelineComponent::mouseMove (const juce::MouseEvent& e)
{
    if (editTool == EditTool::razor && expandedLayout)
    {
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
        return;
    }

    bool left = false;
    if (hitEmptyAdd (e.position) || hitClipBody (e.position) >= 0)
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    else if (hitFadeHandle (e.position) >= 0 || hitClipEdge (e.position, left) >= 0)
        setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
    else
        setMouseCursor (juce::MouseCursor::NormalCursor);
    if (sequence.clips.empty())
        repaint();
}

void Spec3DRampTimelineComponent::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int body = hitClipBody (e.position);
    if (body >= 0)
    {
        openClipRampEditor (body);
        return;
    }

    const int al = hitAutoLane (e.position);
    if (al < 0) return;
    auto& lane = sequence.autoLanes[(size_t) al];
    auto track = getAutoLaneBounds (al);
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    const float t = juce::jlimit (0.0f, len,
                                  (e.position.x - track.getX()) / juce::jmax (1.0f, track.getWidth()) * len);

    if (lane.isColourLane())
    {
        if (hitColourKey (al, e.position) >= 0)
            return;
        Spec3DColourKey k;
        k.timeSec = t;
        k.colour = juce::Colours::white;
        lane.colourEnv.keys.push_back (k);
        lane.colourEnv.sortKeys();
        notifyChanged();
        repaint();
    }
    else
    {
        if (hitFloatKey (al, e.position) >= 0)
            return;
        const float span = juce::jmax (1.0e-4f, lane.floatEnv.maxV - lane.floatEnv.minV);
        const float n = 1.0f - juce::jlimit (0.0f, 1.0f,
            (e.position.y - (track.getY() + 4.0f)) / juce::jmax (1.0f, track.getHeight() - 8.0f));
        Spec3DFloatKey k;
        k.timeSec = t;
        k.value = lane.floatEnv.minV + n * span;
        lane.floatEnv.keys.push_back (k);
        lane.floatEnv.sortKeys();
        notifyChanged();
        repaint();
    }
}

void Spec3DRampTimelineComponent::showAddPresetPicker (juce::Component* anchor)
{
    if (! sequence.canAddClip()) return;
    showRampPresetPickerCallOut (
        bank.getPresets(),
        anchor != nullptr ? anchor : this,
        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this)] (int index)
        {
            if (safe == nullptr) return;
            const auto& presets = safe->bank.getPresets().getPresets();
            if (! juce::isPositiveAndBelow (index, presets.size())) return;
            safe->beginEditSnapshot();
            Spec3DRampClip c;
            c.displayName = presets.getReference (index).name;
            c.ramp = presets.getReference (index).ramp;
            c.ramp.mapMode = GradientRamp::MapMode::intensityLowToHigh;
            c.ramp.enabled = c.ramp.stops.size() >= 2;
            ++c.ramp.revision;
            c.weight = 1.0f;
            c.crossfadeOutSec = safe->sequence.defaultCrossfadeSec;
            safe->sequence.clips.push_back (std::move (c));
            safe->sequence.clamp();
            safe->selectedClip = (int) safe->sequence.clips.size() - 1;
            safe->commitEdit ("Add clip");
            safe->repaint();
        });
}

void Spec3DRampTimelineComponent::showChangePresetPicker (int clipIndex, juce::Component* anchor)
{
    if (clipIndex < 0 || clipIndex >= (int) sequence.clips.size()) return;
    showRampPresetPickerCallOut (
        bank.getPresets(),
        anchor != nullptr ? anchor : this,
        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this), clipIndex] (int index)
        {
            if (safe == nullptr) return;
            const auto& presets = safe->bank.getPresets().getPresets();
            if (! juce::isPositiveAndBelow (index, presets.size())) return;
            if (clipIndex >= (int) safe->sequence.clips.size()) return;
            safe->beginEditSnapshot();
            auto& c = safe->sequence.clips[(size_t) clipIndex];
            c.displayName = presets.getReference (index).name;
            c.ramp = presets.getReference (index).ramp;
            c.ramp.mapMode = GradientRamp::MapMode::intensityLowToHigh;
            c.ramp.enabled = true;
            ++c.ramp.revision;
            safe->commitEdit ("Change ramp");
            safe->repaint();
        });
}

void Spec3DRampTimelineComponent::showRegionContextMenu (juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const bool ok = regionValid && (getRegionEndSec() - getRegionStartSec()) > 1.0e-3f;
    menu.addItem (1, "Export region offline...", ok);
    menu.addSeparator();
    menu.addItem (2, "Clear region", ok);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this)] (int r)
                        {
                            if (safe == nullptr || r == 0) return;
                            if (r == 1)
                                safe->showExportRegionDialog();
                            else if (r == 2)
                                safe->clearRegionSelection();
                        });
}

void Spec3DRampTimelineComponent::showExportRegionDialog()
{
    if (! regionValid)
        return;
    const float t0 = getRegionStartSec();
    const float t1 = getRegionEndSec();
    if (t1 - t0 < 1.0e-3f)
        return;

    // Settings dialog (NLE-style). Audio (DAW/plugin) is always included.
    auto* aw = new juce::AlertWindow ("Export region offline",
                                      "Region " + juce::String (t0, 2) + "s - " + juce::String (t1, 2)
                                          + "s (" + juce::String (t1 - t0, 2) + "s)\n"
                                          + "Video: offline Spec3D re-render. Audio: DAW/plugin stereo (required).\n"
                                          + "Choose resolution and frame rate, then Export.",
                                      juce::AlertWindow::NoIcon,
                                      this);
    aw->addComboBox ("resolution", { "1280 x 720", "1920 x 1080", "2560 x 1440", "3840 x 2160" },
                     "Resolution");
    if (auto* cb = aw->getComboBoxComponent ("resolution"))
        cb->setSelectedItemIndex (1, juce::dontSendNotification);
    aw->addComboBox ("fps", { "24", "25", "30", "60" }, "Frame rate");
    if (auto* cb = aw->getComboBoxComponent ("fps"))
        cb->setSelectedItemIndex (2, juce::dontSendNotification);
    aw->addComboBox ("quality", { "Draft", "High", "Maximum" }, "Quality");
    if (auto* cb = aw->getComboBoxComponent ("quality"))
        cb->setSelectedItemIndex (1, juce::dontSendNotification);
    aw->addButton ("Export", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true,
                         juce::ModalCallbackFunction::create (
                             [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this),
                              aw, t0, t1] (int result)
                             {
                                 std::unique_ptr<juce::AlertWindow> cleanup (aw);
                                 if (safe == nullptr || result != 1)
                                     return;

                                 int resId = 2, fpsId = 3, qualityId = 2;
                                 if (auto* cb = aw->getComboBoxComponent ("resolution"))
                                     resId = cb->getSelectedItemIndex() + 1;
                                 if (auto* cb = aw->getComboBoxComponent ("fps"))
                                     fpsId = cb->getSelectedItemIndex() + 1;
                                 if (auto* cb = aw->getComboBoxComponent ("quality"))
                                     qualityId = cb->getSelectedItemIndex() + 1;

                                 Spec3DExportSettings settings;
                                 settings.startSec = t0;
                                 settings.endSec = t1;
                                 settings.width = Spec3DExportSettings::widthForPreset (resId);
                                 settings.height = Spec3DExportSettings::heightForPreset (resId);
                                 settings.fps = Spec3DExportSettings::fpsForPreset (fpsId);
                                 settings.quality = juce::jlimit (0, 2, qualityId - 1);
                                 settings.includeAudio = true;

                                 auto chooser = std::make_shared<juce::FileChooser> (
                                     "Export video",
                                     juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                         .getChildFile ("Spec3D_export.mp4"),
                                     "*.mp4");
                                 chooser->launchAsync (
                                     juce::FileBrowserComponent::saveMode
                                         | juce::FileBrowserComponent::canSelectFiles
                                         | juce::FileBrowserComponent::warnAboutOverwriting,
                                     [safe, chooser, settings] (const juce::FileChooser& fc) mutable
                                     {
                                         juce::ignoreUnused (chooser);
                                         if (safe == nullptr)
                                             return;
                                         auto file = fc.getResult();
                                         if (file == juce::File())
                                             return;
                                         if (! file.hasFileExtension (".mp4"))
                                             file = file.withFileExtension (".mp4");
                                         settings.outputFile = file;
                                         if (safe->onExportRegionOffline)
                                             safe->onExportRegionOffline (settings);
                                         else
                                             juce::AlertWindow::showMessageBoxAsync (
                                                 juce::AlertWindow::WarningIcon,
                                                 "Export region offline",
                                                 "Export is not connected to the host.");
                                     });
                             }),
                         true);
}

void Spec3DRampTimelineComponent::showClipContextMenu (int clipIndex, juce::Point<int> screenPos)
{
    selectedClip = clipIndex;
    updateEditToolEnabled();
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Delete", true);
    menu.addItem (5, "Duplicate", sequence.canAddClip());
    menu.addItem (6, "Razor at playhead",
                  canSplitAtTime (playheadSec) && findClipIndexAtTime (playheadSec) == clipIndex);
    menu.addSeparator();
    menu.addItem (7, "Selection tool (V)");
    menu.addItem (8, "Razor tool (C)");
    menu.addSeparator();
    menu.addItem (2, "Change ramp...");
    menu.addItem (3, "Edit ramp...");
    if (regionValid)
    {
        menu.addSeparator();
        menu.addItem (4, "Export region offline...");
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this),
                         clipIndex] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            if (r == 4)
                            {
                                safe->showExportRegionDialog();
                                return;
                            }
                            if (r == 5)
                            {
                                safe->selectedClip = clipIndex;
                                safe->duplicateSelectedClip();
                                return;
                            }
                            if (r == 6)
                            {
                                safe->beginEditSnapshot();
                                if (safe->splitClipAtTime (safe->playheadSec))
                                    safe->commitEdit ("Razor cut");
                                else
                                    safe->editSnapshotValid = false;
                                safe->repaint();
                                return;
                            }
                            if (r == 7) { safe->setEditTool (EditTool::select); return; }
                            if (r == 8) { safe->setEditTool (EditTool::razor); return; }
                            if (r == 1)
                            {
                                safe->selectedClip = clipIndex;
                                safe->deleteSelectedClip();
                                return;
                            }
                            if (r == 2)
                                safe->showChangePresetPicker (clipIndex, safe.getComponent());
                            else if (r == 3)
                                safe->openClipRampEditor (clipIndex);
                        });
}

void Spec3DRampTimelineComponent::openClipRampEditor (int clipIndex)
{
    if (clipIndex < 0 || clipIndex >= (int) sequence.clips.size())
        return;
    closeClipRampEditor (true);
    clipEditorIndex = clipIndex;
    auto& ramp = sequence.clips[(size_t) clipIndex].ramp;
    if (ramp.stops.size() < 2)
        ramp = makeDefaultRamp();

    clipEditor = std::make_unique<GradientStripEditor> (
        resources, GradientStripEditor::ModeFamily::intensity, &bank.getPresets());
    clipEditor->setRamp (&ramp);
    clipEditor->setCompact (true);
    clipEditor->setUiScale (0.92f);
    clipEditor->onRampChanged = [this]
    {
        if (clipEditorIndex >= 0 && clipEditorIndex < (int) sequence.clips.size())
        {
            sequence.clips[(size_t) clipEditorIndex].displayName = "Custom";
            ++sequence.clips[(size_t) clipEditorIndex].ramp.revision;
            sequence.clips[(size_t) clipEditorIndex].ramp.enabled = true;
            notifyChanged();
            repaint();
        }
    };
    clipEditor->onRampPreview = clipEditor->onRampChanged;
    addAndMakeVisible (*clipEditor);
    if (getParentComponent() != nullptr)
        getParentComponent()->resized();
    resized();
    repaint();
}

void Spec3DRampTimelineComponent::closeClipRampEditor (bool)
{
    if (clipEditor == nullptr)
        return;
    clipEditor.reset();
    clipEditorIndex = -1;
    if (getParentComponent() != nullptr)
        getParentComponent()->resized();
    resized();
}

void Spec3DRampTimelineComponent::showAddLaneMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const Spec3DSeqLaneType types[] = {
        Spec3DSeqLaneType::lightAmount, Spec3DSeqLaneType::lightAzimuth,
        Spec3DSeqLaneType::lightElevation, Spec3DSeqLaneType::lightColour,
        Spec3DSeqLaneType::rimAmount, Spec3DSeqLaneType::rimColour
    };
    for (int i = 0; i < 6; ++i)
    {
        const auto t = types[i];
        menu.addItem (i + 1, Spec3DSeqLane::defaultLabel (t), ! sequence.hasLaneType (t), false);
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (addLaneButton.get()),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this)] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            const Spec3DSeqLaneType types[] = {
                                Spec3DSeqLaneType::lightAmount, Spec3DSeqLaneType::lightAzimuth,
                                Spec3DSeqLaneType::lightElevation, Spec3DSeqLaneType::lightColour,
                                Spec3DSeqLaneType::rimAmount, Spec3DSeqLaneType::rimColour
                            };
                            safe->beginEditSnapshot();
                            if (safe->sequence.addLane (types[r - 1]))
                            {
                                safe->commitEdit ("Add lane");
                                safe->repaint();
                            }
                            else
                                safe->editSnapshotValid = false;
                        });
}

void Spec3DRampTimelineComponent::showLaneLabelMenu (int autoIdx, juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());

    const bool isRamp = autoIdx < 0;
    const bool on = isRamp
                        ? sequence.rampLaneEnabled
                        : (juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size())
                               && sequence.autoLanes[(size_t) autoIdx].enabled);

    menu.addItem (2, "Enabled", true, on);
    menu.addItem (3, "Disabled", true, ! on);
    if (! isRamp)
    {
        menu.addSeparator();
        menu.addItem (1, "Remove lane");
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this), autoIdx] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            if (r == 1)
                            {
                                if (autoIdx >= 0)
                                {
                                    safe->beginEditSnapshot();
                                    safe->sequence.removeLaneAt (autoIdx);
                                    safe->selectedAutoLane = -1;
                                    safe->selectedKey = -1;
                                    safe->commitEdit ("Remove lane");
                                    safe->repaint();
                                }
                                return;
                            }
                            if (r == 2 || r == 3)
                            {
                                const bool wantOn = (r == 2);
                                if (autoIdx < 0)
                                {
                                    if (safe->sequence.rampLaneEnabled != wantOn)
                                        safe->toggleLaneEnabled (-1);
                                }
                                else if (juce::isPositiveAndBelow (autoIdx, (int) safe->sequence.autoLanes.size()))
                                {
                                    if (safe->sequence.autoLanes[(size_t) autoIdx].enabled != wantOn)
                                        safe->toggleLaneEnabled (autoIdx);
                                }
                            }
                        });
}

void Spec3DRampTimelineComponent::showKeyInterpMenu (int autoIdx, int keyIdx, bool isColour,
                                                     juce::Point<int> screenPos)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    if (! isColour)
        menu.addItem (2, "Edit value...");
    menu.addItem (1, "Delete key");
    menu.addSeparator();
    menu.addItem (10, "Step");
    menu.addItem (11, "Linear");
    menu.addItem (12, "Smooth");
    menu.addItem (13, "Bezier");
    if (isColour)
    {
        menu.addSeparator();
        menu.addItem (20, "Set colour...");
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this),
                         autoIdx, keyIdx, isColour] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            if (! juce::isPositiveAndBelow (autoIdx, (int) safe->sequence.autoLanes.size()))
                                return;
                            auto& lane = safe->sequence.autoLanes[(size_t) autoIdx];
                            if (r == 2 && ! isColour)
                            {
                                safe->beginEditFloatKeyValue (autoIdx, keyIdx);
                                return;
                            }
                            if (r == 1)
                            {
                                if (isColour && keyIdx < (int) lane.colourEnv.keys.size())
                                    lane.colourEnv.keys.erase (lane.colourEnv.keys.begin() + keyIdx);
                                else if (! isColour && keyIdx < (int) lane.floatEnv.keys.size())
                                    lane.floatEnv.keys.erase (lane.floatEnv.keys.begin() + keyIdx);
                                safe->notifyChanged();
                                safe->repaint();
                                return;
                            }
                            if (r >= 10 && r <= 13)
                            {
                                const auto interp = static_cast<Spec3DKeyInterp> (r - 10);
                                if (isColour && keyIdx < (int) lane.colourEnv.keys.size())
                                    lane.colourEnv.keys[(size_t) keyIdx].interp = interp;
                                else if (! isColour && keyIdx < (int) lane.floatEnv.keys.size())
                                    lane.floatEnv.keys[(size_t) keyIdx].interp = interp;
                                safe->notifyChanged();
                                safe->repaint();
                                return;
                            }
                            if (r == 20 && isColour)
                                safe->openColourKeyPicker (autoIdx, keyIdx);
                        });
}

void Spec3DRampTimelineComponent::beginEditFloatKeyValue (int autoIdx, int keyIdx)
{
    if (! juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
        return;
    auto& lane = sequence.autoLanes[(size_t) autoIdx];
    if (lane.isColourLane() || keyIdx < 0 || keyIdx >= (int) lane.floatEnv.keys.size())
        return;

    auto& env = lane.floatEnv;
    const float cur = env.keys[(size_t) keyIdx].value;
    const bool wideRange = (env.maxV - env.minV) > 2.0f; // azimuth / elevation vs 0-1 amounts
    const juce::String initial = juce::String (cur, wideRange ? 1 : 3);
    const juce::String rangeText = lane.label + " (" + juce::String (env.minV, wideRange ? 0 : 2)
                                   + " - " + juce::String (env.maxV, wideRange ? 0 : 2) + ")";

    auto* aw = new juce::AlertWindow ("Edit key value", rangeText, juce::AlertWindow::NoIcon);
    aw->addTextEditor ("value", initial, "Value");
    aw->addButton ("OK", 1, juce::KeyPress (juce::KeyPress::returnKey));
    aw->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

    aw->enterModalState (true, juce::ModalCallbackFunction::create (
        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this),
         aw, autoIdx, keyIdx] (int r)
        {
            if (r == 1 && safe != nullptr
                && juce::isPositiveAndBelow (autoIdx, (int) safe->sequence.autoLanes.size()))
            {
                auto& ln = safe->sequence.autoLanes[(size_t) autoIdx];
                if (! ln.isColourLane()
                    && keyIdx >= 0 && keyIdx < (int) ln.floatEnv.keys.size())
                {
                    const auto text = aw->getTextEditorContents ("value").trim();
                    // Require at least one digit so "abc" is rejected (getDoubleValue -> 0).
                    bool hasDigit = false;
                    for (int i = 0; i < text.length(); ++i)
                    {
                        if (juce::CharacterFunctions::isDigit (text[i]))
                        {
                            hasDigit = true;
                            break;
                        }
                    }
                    if (hasDigit)
                    {
                        const float v = juce::jlimit (ln.floatEnv.minV, ln.floatEnv.maxV,
                                                      (float) text.getDoubleValue());
                        ln.floatEnv.keys[(size_t) keyIdx].value = v;
                        safe->notifyChanged();
                        safe->repaint();
                    }
                }
            }
            delete aw;
        }));
}

void Spec3DRampTimelineComponent::openColourKeyPicker (int autoIdx, int keyIdx)
{
    if (! juce::isPositiveAndBelow (autoIdx, (int) sequence.autoLanes.size()))
        return;
    auto& lane = sequence.autoLanes[(size_t) autoIdx];
    if (! lane.isColourLane() || keyIdx < 0 || keyIdx >= (int) lane.colourEnv.keys.size())
        return;

    colourPickerLane = autoIdx;
    colourPickerKey = keyIdx;
    const auto col = lane.colourEnv.keys[(size_t) keyIdx].colour;
    colourKeyPicker = std::make_unique<RampColorPickerPanel> (col);
    colourKeyPicker->onColourChanged = [this] (juce::Colour c)
    {
        if (colourPickerLane >= 0 && colourPickerLane < (int) sequence.autoLanes.size()
            && colourPickerKey >= 0
            && colourPickerKey < (int) sequence.autoLanes[(size_t) colourPickerLane].colourEnv.keys.size())
        {
            sequence.autoLanes[(size_t) colourPickerLane].colourEnv.keys[(size_t) colourPickerKey].colour = c;
            repaint();
        }
    };
    colourKeyPicker->onDone = [this]
    {
        colourKeyPicker.reset();
        colourPickerLane = colourPickerKey = -1;
        notifyChanged();
        if (getParentComponent()) getParentComponent()->resized();
        resized();
    };
    addAndMakeVisible (*colourKeyPicker);
    if (getParentComponent()) getParentComponent()->resized();
    resized();
}
