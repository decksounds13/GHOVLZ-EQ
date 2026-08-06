#include "Spec3DRampTimelineComponent.h"
#include "ColourRampBank.h"
#include "RampPresetPicker.h"
#include "../ComboBoxLookAndFeel.h"

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
}

Spec3DRampTimelineComponent::Spec3DRampTimelineComponent (SharedResources& resourcesIn,
                                                          ColourRampBank& bankIn,
                                                          Spec3DRampSequence& sequenceIn)
    : resources (resourcesIn), theme (&resourcesIn), bank (bankIn), sequence (sequenceIn)
{
    setOpaque (false);
    sequence.hydrateFromStore (bank.getPresets());

    enableButton.setClickingTogglesState (true);
    enableButton.setTooltip ("Enable sequencer (ramp morph + automation)");
    enableButton.onClick = [this]
    {
        sequence.enabled = enableButton.getToggleState();
        if (onEnabledChanged) onEnabledChanged();
        notifyChanged();
        repaint();
    };
    addAndMakeVisible (enableButton);

    lengthLabel.setText ("Length", juce::dontSendNotification);
    lengthLabel.setJustificationType (juce::Justification::centredRight);
    lengthLabel.setMinimumHorizontalScale (1.0f); // never ellipsize "Length"
    lengthLabel.setTooltip ("Timeline length (0.5 s – 5 min)");
    addAndMakeVisible (lengthLabel);

    lengthSlider.setRange (Spec3DRampSequence::kMinLengthSec, Spec3DRampSequence::kMaxLengthSec, 0.01);
    lengthSlider.setSkewFactorFromMidPoint (8.0);
    lengthSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    lengthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 16);
    lengthSlider.setTooltip ("Timeline length (0.5 s – 5 min). Crossfades: drag the X between clips.");
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
    lengthSlider.onValueChange = [this]
    {
        sequence.lengthSec = (float) lengthSlider.getValue();
        sequence.clamp();
        notifyChanged();
        repaint();
    };
    addAndMakeVisible (lengthSlider);

    addButton.setTooltip ("Add ramp clip");
    addButton.onClick = [this] { showAddPresetPicker (&addButton); };
    addAndMakeVisible (addButton);

    removeButton.setTooltip ("Remove selected clip");
    removeButton.onClick = [this]
    {
        if (selectedClip < 0 || selectedClip >= (int) sequence.clips.size())
            return;
        closeClipRampEditor (false);
        sequence.clips.erase (sequence.clips.begin() + selectedClip);
        selectedClip = -1;
        sequence.clamp();
        notifyChanged();
        repaint();
    };
    addAndMakeVisible (removeButton);

    addLaneButton.setTooltip ("Add lighting automation lane");
    addLaneButton.onClick = [this] { showAddLaneMenu(); };
    addAndMakeVisible (addLaneButton);

    expandButton.setTooltip ("Expand sequencer window");
    expandButton.onClick = [this] { if (onRequestExpand) onRequestExpand(); };
    addAndMakeVisible (expandButton);

    styleChrome();
    enableButton.setToggleState (sequence.enabled, juce::dontSendNotification);
    lengthSlider.setValue (sequence.lengthSec, juce::dontSendNotification);
    rebuildCaches();
    // Playhead paint only while visible — do not thrash the message thread when hidden.
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
    resized();
    repaint();
}

void Spec3DRampTimelineComponent::setShowExpandButton (bool shouldShow) noexcept
{
    showExpandButton = shouldShow;
    expandButton.setVisible (shouldShow);
    resized();
}

void Spec3DRampTimelineComponent::setPlayheadSec (float sec) noexcept
{
    if (std::abs (playheadSec - sec) < 1.0e-4f)
        return;
    playheadSec = sec;
    if (isShowing())
        repaint();
}

int Spec3DRampTimelineComponent::getPreferredHeight() const noexcept
{
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
    auto styleBtn = [&] (juce::TextButton& b)
    {
        b.setColour (juce::TextButton::buttonColourId, c.pluginButtonBackground);
        b.setColour (juce::TextButton::buttonOnColourId, c.pluginButtonAccent);
        b.setColour (juce::TextButton::textColourOffId, c.pluginButtonText.withAlpha (0.9f));
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    };
    styleBtn (enableButton);
    styleBtn (addButton);
    styleBtn (removeButton);
    styleBtn (addLaneButton);
    styleBtn (expandButton);
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
    r.removeFromTop ((expandedLayout ? 28.0f : 24.0f) + 4.0f);
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
    auto row = getLocalBounds().removeFromTop (expandedLayout ? 28 : 24);
    enableButton.setBounds (row.removeFromLeft (40).reduced (1));
    expandButton.setVisible (showExpandButton);
    if (showExpandButton)
        expandButton.setBounds (row.removeFromRight (26).reduced (1));
    addLaneButton.setBounds (row.removeFromRight (expandedLayout ? 48 : 40).reduced (1));
    removeButton.setBounds (row.removeFromRight (24).reduced (1));
    addButton.setBounds (row.removeFromRight (24).reduced (1));
    // "Length" needs room — never squeeze into ellipsis.
    lengthSlider.setBounds (row.removeFromRight (expandedLayout ? 128 : 112).reduced (1, 3));
    lengthLabel.setBounds (row.removeFromRight (expandedLayout ? 48 : 44).reduced (1, 2));

    enableButton.setToggleState (sequence.enabled, juce::dontSendNotification);
    lengthSlider.setValue (sequence.lengthSec, juce::dontSendNotification);
    addButton.setEnabled (sequence.canAddClip());
    removeButton.setEnabled (selectedClip >= 0);

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

    // Playhead across all tracks
    if (sequence.enabled)
    {
        const auto& c = colors();
        auto area = getTracksArea();
        area.removeFromLeft (labelColW() + 4.0f);
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
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    for (int i = 0; i < (int) layout.size(); ++i)
    {
        if (layout[(size_t) i].fadeOutSec <= 1.0e-4f) continue;
        const float xRight = lane.getX() + lane.getWidth() * (layout[(size_t) i].endSec / len);
        const float fadePx = lane.getWidth() * (layout[(size_t) i].fadeOutSec / len);
        const float cx = xRight - fadePx * 0.5f;
        if (std::abs (p.x - cx) <= kFadeHit && p.y >= lane.getY() && p.y <= lane.getBottom())
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
        return;
    notifyChanged();
    repaint();
}

void Spec3DRampTimelineComponent::mouseDown (const juce::MouseEvent& e)
{
    const auto p = e.position;

    // Lane labels: LMB toggle enable, RMB menu (enable/disable + remove for auto)
    if (e.mods.isPopupMenu())
    {
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

    bool left = false;
    int edge = hitClipEdge (p, left);
    int fade = hitFadeHandle (p);
    int body = hitClipBody (p);
    int al = hitAutoLane (p);

    // LMB on auto-lane label → mute/unmute (don't start key drag)
    if (al >= 0 && hitAutoLaneLabel (al, p))
    {
        toggleLaneEnabled (al);
        return;
    }

    if (fade >= 0)
    {
        dragMode = DragMode::fade;
        dragClip = fade;
        dragStartFade = sequence.clips[(size_t) fade].crossfadeOutSec;
        selectedClip = fade;
    }
    else if (edge >= 0)
    {
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
    else
    {
        selectedClip = -1;
        selectedKey = -1;
        selectedAutoLane = -1;
        closeClipRampEditor (true);
    }
    removeButton.setEnabled (selectedClip >= 0);
    repaint();
}

void Spec3DRampTimelineComponent::mouseDrag (const juce::MouseEvent& e)
{
    if (dragMode == DragMode::none) return;
    const auto lane = getRampLaneBounds();
    const float len = juce::jmax (1.0e-4f, sequence.lengthSec);
    const float dx = (float) e.getDistanceFromDragStartX();
    const float dSec = (dx / juce::jmax (1.0f, lane.getWidth())) * len;

    if (dragMode == DragMode::fade && dragClip >= 0)
    {
        sequence.clips[(size_t) dragClip].crossfadeOutSec = juce::jmax (0.0f, dragStartFade + dSec);
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
            notifyChanged();
        }
    }
    else if (dragMode == DragMode::fade || dragMode == DragMode::resizeLeft
             || dragMode == DragMode::resizeRight
             || dragMode == DragMode::floatKey || dragMode == DragMode::colourKey)
    {
        notifyChanged();
    }
    dragMode = DragMode::none;
    dragClip = -1;
    dropInsertAt = -1;
    rebuildCaches();
    resized();
    repaint();
}

void Spec3DRampTimelineComponent::mouseMove (const juce::MouseEvent& e)
{
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
            safe->notifyChanged();
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
            auto& c = safe->sequence.clips[(size_t) clipIndex];
            c.displayName = presets.getReference (index).name;
            c.ramp = presets.getReference (index).ramp;
            c.ramp.mapMode = GradientRamp::MapMode::intensityLowToHigh;
            c.ramp.enabled = true;
            ++c.ramp.revision;
            safe->notifyChanged();
            safe->repaint();
        });
}

void Spec3DRampTimelineComponent::showClipContextMenu (int clipIndex, juce::Point<int> screenPos)
{
    selectedClip = clipIndex;
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Delete");
    menu.addSeparator();
    menu.addItem (2, "Change ramp…");
    menu.addItem (3, "Edit ramp…");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this),
                         clipIndex] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            if (r == 1 && clipIndex < (int) safe->sequence.clips.size())
                            {
                                safe->closeClipRampEditor (false);
                                safe->sequence.clips.erase (safe->sequence.clips.begin() + clipIndex);
                                safe->selectedClip = -1;
                                safe->notifyChanged();
                                safe->repaint();
                            }
                            else if (r == 2)
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
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&addLaneButton),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this)] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            const Spec3DSeqLaneType types[] = {
                                Spec3DSeqLaneType::lightAmount, Spec3DSeqLaneType::lightAzimuth,
                                Spec3DSeqLaneType::lightElevation, Spec3DSeqLaneType::lightColour,
                                Spec3DSeqLaneType::rimAmount, Spec3DSeqLaneType::rimColour
                            };
                            if (safe->sequence.addLane (types[r - 1]))
                            {
                                safe->notifyChanged();
                                safe->repaint();
                            }
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
                                    safe->sequence.removeLaneAt (autoIdx);
                                    safe->selectedAutoLane = -1;
                                    safe->selectedKey = -1;
                                    safe->notifyChanged();
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
    menu.addItem (1, "Delete key");
    menu.addSeparator();
    menu.addItem (10, "Step");
    menu.addItem (11, "Linear");
    menu.addItem (12, "Smooth");
    menu.addItem (13, "Bezier");
    if (isColour)
    {
        menu.addSeparator();
        menu.addItem (20, "Set colour…");
    }
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetScreenArea ({ screenPos.x, screenPos.y, 1, 1 }),
                        [safe = juce::Component::SafePointer<Spec3DRampTimelineComponent> (this),
                         autoIdx, keyIdx, isColour] (int r)
                        {
                            if (safe == nullptr || r <= 0) return;
                            if (! juce::isPositiveAndBelow (autoIdx, (int) safe->sequence.autoLanes.size()))
                                return;
                            auto& lane = safe->sequence.autoLanes[(size_t) autoIdx];
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
