#include "PathSampleOverlay.h"

PathSampleOverlay::PathSampleOverlay()
{
    setOpaque (false);
    setAlwaysOnTop (true);
    setVisible (false);
    setWantsKeyboardFocus (true);
}

void PathSampleOverlay::beginSession (juce::Component& sampleRoot)
{
    root = &sampleRoot;
    pathPoints.clear();
    dragging = false;
    setBounds (sampleRoot.getLocalBounds());
    setVisible (true);
    toFront (true);
    grabKeyboardFocus();
    repaint();
}

void PathSampleOverlay::endSession()
{
    setVisible (false);
    pathPoints.clear();
    dragging = false;
    root = nullptr;
}

void PathSampleOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black.withAlpha (0.18f));

    g.setColour (juce::Colours::whitesmoke.withAlpha (0.85f));
    g.setFont (juce::FontOptions().withName ("Lato Black").withHeight (14.0f));
    g.drawText ("Drag to sample UI colours - Esc to cancel",
                getLocalBounds().removeFromTop (28).reduced (10, 4),
                juce::Justification::centredLeft, false);

    if (pathPoints.size() < 2)
        return;

    juce::Path p;
    p.startNewSubPath (pathPoints.front());
    for (size_t i = 1; i < pathPoints.size(); ++i)
        p.lineTo (pathPoints[i]);

    g.setColour (juce::Colours::goldenrod.withAlpha (0.95f));
    g.strokePath (p, juce::PathStrokeType (2.0f));
}

void PathSampleOverlay::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        endSession();
        if (onCancelled != nullptr)
            onCancelled();
        return;
    }

    pathPoints.clear();
    pathPoints.push_back (e.position);
    dragging = true;
    repaint();
}

void PathSampleOverlay::mouseDrag (const juce::MouseEvent& e)
{
    if (! dragging)
        return;

    if (pathPoints.empty() || e.position.getDistanceFrom (pathPoints.back()) >= 2.0f)
        pathPoints.push_back (e.position);

    repaint();
}

void PathSampleOverlay::mouseUp (const juce::MouseEvent& e)
{
    juce::ignoreUnused (e);
    if (! dragging)
        return;

    dragging = false;
    auto ramp = buildRampFromPath();
    endSession();

    if (ramp.stops.size() >= 2 && onSampled != nullptr)
        onSampled (std::move (ramp));
    else if (onCancelled != nullptr)
        onCancelled();
}

bool PathSampleOverlay::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey)
    {
        endSession();
        if (onCancelled != nullptr)
            onCancelled();
        return true;
    }
    return false;
}

GradientRamp PathSampleOverlay::buildRampFromPath() const
{
    GradientRamp ramp;
    if (root == nullptr || pathPoints.size() < 2)
        return ramp;

    // Snapshot the plugin UI without this overlay (caller ends the session next).
    auto* self = const_cast<PathSampleOverlay*> (this);
    self->setVisible (false);
    auto snap = root->createComponentSnapshot (root->getLocalBounds(), true, 1.0f);

    if (! snap.isValid())
        return ramp;

    struct Sample
    {
        float dist = 0.0f;
        juce::Colour colour;
    };

    std::vector<Sample> samples;
    samples.reserve (pathPoints.size());

    float total = 0.0f;
    juce::Point<float> prev = pathPoints.front();
    for (const auto& pt : pathPoints)
    {
        total += pt.getDistanceFrom (prev);
        prev = pt;

        const int x = juce::jlimit (0, snap.getWidth() - 1, juce::roundToInt (pt.x));
        const int y = juce::jlimit (0, snap.getHeight() - 1, juce::roundToInt (pt.y));
        auto c = snap.getPixelAt (x, y);
        // Skip near-transparent / dimmer veil samples.
        if (c.getAlpha() < 8)
            continue;
        samples.push_back ({ total, c.withAlpha (1.0f) });
    }

    if (samples.size() < 2)
        return ramp;

    const float pathLen = juce::jmax (1.0f, samples.back().dist);
    // Light default — user can Densify for smoother poles.
    const int numStops = juce::jlimit (2, GradientRamp::kDefaultStops,
                                       juce::jmin (GradientRamp::kDefaultStops, (int) samples.size()));

    std::vector<GradientRamp::Stop> stops;
    stops.reserve ((size_t) numStops);

    for (int i = 0; i < numStops; ++i)
    {
        const float targetDist = pathLen * ((float) i / (float) (numStops - 1));
        // Nearest sample by distance along path.
        size_t best = 0;
        float bestErr = std::abs (samples[0].dist - targetDist);
        for (size_t s = 1; s < samples.size(); ++s)
        {
            const float err = std::abs (samples[s].dist - targetDist);
            if (err < bestErr)
            {
                bestErr = err;
                best = s;
            }
        }

        GradientRamp::Stop stop;
        stop.position = (float) i / (float) (numStops - 1);
        stop.colour = samples[best].colour;
        stops.push_back (stop);
    }

    ramp.setStops (std::move (stops));
    return ramp;
}
