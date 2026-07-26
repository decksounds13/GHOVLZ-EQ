#include "OscilloscopeComponent.h"
#include "ComboBoxLookAndFeel.h"

OscilloscopeComponent::OscilloscopeComponent()
{
    setOpaque (false);
    setVisible (false);
}

OscilloscopeComponent::~OscilloscopeComponent()
{
    stopTimer();
}

void OscilloscopeComponent::prepare (double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    sampleRateHz.store (sr, std::memory_order_relaxed);

    const int newCap = juce::jmax (1024, (int) std::ceil (sr * (double) kMaxBufferSeconds));
    ringL.assign ((size_t) newCap, 0.0f);
    ringR.assign ((size_t) newCap, 0.0f);
    writePos.store (0, std::memory_order_relaxed);
    capacity.store (newCap, std::memory_order_release);
    resetDisplay();
}

void OscilloscopeComponent::setHostBpm (double bpm) noexcept
{
    if (bpm > 1.0 && bpm < 999.0)
        hostBpm.store (bpm, std::memory_order_relaxed);
}

void OscilloscopeComponent::resetDisplay()
{
    columns.clear();
    ringReadPos = writePos.load (std::memory_order_acquire);
    samplesInColumn = 0;
    colMinSum = 1.0f; colMaxSum = -1.0f;
    colMinL = 1.0f; colMaxL = -1.0f;
    colMinR = 1.0f; colMaxR = -1.0f;
    lastZoomIndex = -1;
    lastWindowSamples = -1;
    lastColumnCount = -1;
    samplesPerColumn = 1;
    writeColumn = 0;
}

void OscilloscopeComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
    setVisible (shouldEnable);

    if (shouldEnable)
    {
        zoomIndex.store (0, std::memory_order_relaxed);
        resetDisplay();
        if (getWidth() > 4)
            ensureDisplayWidth (getWidth() - 2);
        updateSamplesPerColumn();
        startTimerHz (30);
    }
    else
    {
        stopTimer();
    }
}

void OscilloscopeComponent::setScrollMode (bool shouldScroll) noexcept
{
    if (scrollMode == shouldScroll)
        return;

    scrollMode = shouldScroll;
    resetDisplay();
    ensureDisplayWidth (juce::jmax (2, getWidth() - 2));
    updateSamplesPerColumn();
    repaint();
}

void OscilloscopeComponent::setChannelMode (ChannelMode mode) noexcept
{
    if (channelMode == mode)
        return;

    channelMode = mode;
    repaint();
}

void OscilloscopeComponent::toggleChannelMode() noexcept
{
    setChannelMode (channelMode == ChannelMode::summedStereo ? ChannelMode::splitStereo
                                                             : ChannelMode::summedStereo);
}

void OscilloscopeComponent::setExpanded (bool shouldExpand) noexcept
{
    if (expanded == shouldExpand)
        return;

    expanded = shouldExpand;
    setInterceptsMouseClicks (! expanded, ! expanded);
    resetDisplay();
    ensureDisplayWidth (juce::jmax (2, getWidth() - 2));
    updateSamplesPerColumn();
    repaint();
}

void OscilloscopeComponent::setZoomIndex (int index) noexcept
{
    const int z = juce::jlimit (0, kMaxZoomIndex, index);
    const bool changed = z != zoomIndex.load (std::memory_order_relaxed);
    zoomIndex.store (z, std::memory_order_relaxed);

    if (changed)
        resetDisplay();

    ensureDisplayWidth (juce::jmax (2, getWidth() - 2));
    updateSamplesPerColumn();
    repaint();
}

void OscilloscopeComponent::zoomIn()
{
    setZoomIndex (zoomIndex.load (std::memory_order_relaxed) - 1);
}

void OscilloscopeComponent::zoomOut()
{
    setZoomIndex (zoomIndex.load (std::memory_order_relaxed) + 1);
}

juce::String OscilloscopeComponent::getZoomLabel() const
{
    switch (zoomIndex.load (std::memory_order_relaxed))
    {
        case 0:  return "1 beat";
        case 1:  return "2 beats";
        case 2:  return "4 beats";
        case 3:  return "8 beats";
        case 4:  return "16 beats";
        default: return "32 beats";
    }
}

int OscilloscopeComponent::getWindowLengthInSamples() const noexcept
{
    static constexpr int beatsForZoom[] = { 1, 2, 4, 8, 16, 32 };
    const int z = juce::jlimit (0, kMaxZoomIndex, zoomIndex.load (std::memory_order_relaxed));
    const double bpm = juce::jmax (1.0, hostBpm.load (std::memory_order_relaxed));
    const double sr = juce::jmax (1.0, sampleRateHz.load (std::memory_order_relaxed));
    const double seconds = (double) beatsForZoom[z] * (60.0 / bpm);
    const int samples = (int) std::ceil (seconds * sr);
    const int cap = capacity.load (std::memory_order_acquire);
    return juce::jlimit (1, juce::jmax (1, cap), samples);
}

void OscilloscopeComponent::ensureDisplayWidth (int widthPx)
{
    const int w = juce::jmax (2, widthPx);
    if ((int) columns.size() == w)
        return;

    std::vector<Column> next ((size_t) w);
    if (! columns.empty())
    {
        if (scrollMode)
        {
            const int copy = juce::jmin (w, (int) columns.size());
            std::copy (columns.end() - copy, columns.end(), next.end() - copy);
        }
        else
        {
            const int copy = juce::jmin (w, (int) columns.size());
            std::copy (columns.begin(), columns.begin() + copy, next.begin());
            writeColumn = juce::jlimit (0, w - 1, writeColumn);
        }
    }
    columns.swap (next);
}

void OscilloscopeComponent::updateSamplesPerColumn()
{
    const int w = juce::jmax (2, (int) columns.size());
    const int windowSamples = getWindowLengthInSamples();
    const int z = zoomIndex.load (std::memory_order_relaxed);

    if (z == lastZoomIndex
        && windowSamples == lastWindowSamples
        && w == lastColumnCount
        && samplesPerColumn > 0)
        return;

    lastZoomIndex = z;
    lastWindowSamples = windowSamples;
    lastColumnCount = w;
    samplesPerColumn = juce::jmax (1, windowSamples / w);
}

void OscilloscopeComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || left == nullptr || numSamples <= 0)
        return;

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0 || (int) ringL.size() < cap || (int) ringR.size() < cap)
        return;

    int pos = writePos.load (std::memory_order_relaxed);
    const float* r = right != nullptr ? right : left;

    for (int i = 0; i < numSamples; ++i)
    {
        ringL[(size_t) pos] = left[i];
        ringR[(size_t) pos] = r[i];
        ++pos;
        if (pos >= cap)
            pos = 0;
    }

    writePos.store (pos, std::memory_order_release);
}

void OscilloscopeComponent::advanceDisplayFromRing()
{
    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0 || (int) ringL.size() < cap || (int) ringR.size() < cap || columns.empty())
        return;

    updateSamplesPerColumn();

    const int wp = writePos.load (std::memory_order_acquire);
    int available = wp - ringReadPos;
    if (available < 0)
        available += cap;

    const int maxConsume = samplesPerColumn * (int) columns.size();
    available = juce::jmin (available, maxConsume);

    while (available > 0)
    {
        const float l = ringL[(size_t) ringReadPos];
        const float r = ringR[(size_t) ringReadPos];
        const float sum = 0.5f * (l + r);

        colMinSum = juce::jmin (colMinSum, sum);
        colMaxSum = juce::jmax (colMaxSum, sum);
        colMinL = juce::jmin (colMinL, l);
        colMaxL = juce::jmax (colMaxL, l);
        colMinR = juce::jmin (colMinR, r);
        colMaxR = juce::jmax (colMaxR, r);
        ++samplesInColumn;

        ++ringReadPos;
        if (ringReadPos >= cap)
            ringReadPos = 0;
        --available;

        if (samplesInColumn >= samplesPerColumn)
        {
            Column baked;
            baked.minSum = juce::jlimit (-1.0f, 1.0f, colMinSum);
            baked.maxSum = juce::jlimit (-1.0f, 1.0f, colMaxSum);
            baked.minL = juce::jlimit (-1.0f, 1.0f, colMinL);
            baked.maxL = juce::jlimit (-1.0f, 1.0f, colMaxL);
            baked.minR = juce::jlimit (-1.0f, 1.0f, colMinR);
            baked.maxR = juce::jlimit (-1.0f, 1.0f, colMaxR);
            baked.valid = true;

            if (scrollMode)
            {
                if (columns.size() > 1)
                    std::move (columns.begin() + 1, columns.end(), columns.begin());
                columns.back() = baked;
            }
            else
            {
                writeColumn = juce::jlimit (0, (int) columns.size() - 1, writeColumn);
                columns[(size_t) writeColumn] = baked;
                ++writeColumn;
                if (writeColumn >= (int) columns.size())
                    writeColumn = 0;
            }

            samplesInColumn = 0;
            colMinSum = 1.0f; colMaxSum = -1.0f;
            colMinL = 1.0f; colMaxL = -1.0f;
            colMinR = 1.0f; colMaxR = -1.0f;
        }
    }
}

void OscilloscopeComponent::timerCallback()
{
    if (! isVisible())
        return;

    ensureDisplayWidth (juce::jmax (2, getWidth() - 2));
    advanceDisplayFromRing();
    repaint();
}

void OscilloscopeComponent::resized()
{
    ensureDisplayWidth (juce::jmax (2, getWidth() - 2));
    updateSamplesPerColumn();
}

void OscilloscopeComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void OscilloscopeComponent::showContextMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addItem (1, "Redraw in place", true, ! scrollMode);

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<OscilloscopeComponent> (this)] (int result)
                        {
                            if (safe == nullptr || result != 1)
                                return;
                            safe->setScrollMode (! safe->isScrollMode());
                        });
}

float OscilloscopeComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree == nullptr)
        return fallback;
    if (auto* p = valueTree->getRawParameterValue (id))
        return p->load();
    return fallback;
}

bool OscilloscopeComponent::isHighQuality() const
{
    if (valueTree == nullptr)
        return true;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (valueTree->getParameter ("OSC_QUALITY_ID")))
        return choice->getIndex() >= 1;

    return true;
}

void OscilloscopeComponent::appendColumnStub (juce::Path& path, float px, float yMax, float yMin) const
{
    if (std::abs (yMin - yMax) < 1.0f)
    {
        const float mid = 0.5f * (yMin + yMax);
        yMax = mid - 0.5f;
        yMin = mid + 0.5f;
    }

    path.startNewSubPath (px, yMax);
    path.lineTo (px, yMin);
}

void OscilloscopeComponent::strokeWaveform (juce::Graphics& g, const juce::Path& waveform,
                                           float pathWidth, float lineOpacity, bool highQuality,
                                           bool glowEnabled, float glowOpacity, float glowRadius, float glowSpread)
{
    if (waveform.isEmpty())
        return;

    const auto lineColour = juce::Colour::fromRGBA (220, 190, 120, 220).withMultipliedAlpha (lineOpacity);
    const juce::PathStrokeType stroke (pathWidth,
                                       juce::PathStrokeType::curved,
                                       juce::PathStrokeType::rounded);

    const float radius = juce::jmax (0.0f, glowRadius);
    const float spread = juce::jmax (0.0f, glowSpread);

    if (glowEnabled && glowOpacity > 0.05f && radius > 0.5f)
    {
        const auto bloom = juce::Colour::fromRGBA (220, 190, 120, 130).withAlpha (glowOpacity * 0.45f);
        const auto core = juce::Colour::fromRGBA (255, 230, 160, 180).withAlpha (glowOpacity * 0.75f);

        // Melatonin ignores spread on stroked paths (only expands filled shapes).
        // Stroke the waveform into a filled outline first so spread thickens the glow.
        const juce::PathStrokeType glowStroke (juce::jmax (1.0f, pathWidth + 0.5f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded);
        juce::Path glowShape;
        glowStroke.createStrokedPath (glowShape, waveform);

        waveGlow.setRadius ((double) radius, 0);
        waveGlow.setSpread ((double) spread, 0);
        waveGlow.setOffset (0, 0, 0);
        waveGlow.setColor (bloom, 0);

        waveGlow.setRadius (juce::jmax (2.0, (double) radius * 0.35), 1);
        waveGlow.setSpread (0.0, 1);
        waveGlow.setOffset (0, 0, 1);
        waveGlow.setColor (core, 1);

        // Expanded: prefer Melatonin's fast path (full-graph blur is expensive).
        waveGlow.render (g, glowShape, expanded || ! highQuality);
    }

    // 2× supersampled stroke allocates a full ARGB image each paint — skip when expanded.
    if (highQuality && ! expanded)
    {
        constexpr int ss = 2;
        const auto bounds = getLocalBounds();
        const int w = juce::jmax (1, bounds.getWidth());
        const int h = juce::jmax (1, bounds.getHeight());

        juce::Image hiRes (juce::Image::ARGB, w * ss, h * ss, true);
        {
            juce::Graphics g2 (hiRes);
            g2.addTransform (juce::AffineTransform::scale ((float) ss));
            g2.setColour (lineColour);
            g2.strokePath (waveform, stroke);
        }

        g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        g.drawImage (hiRes, bounds.toFloat());
    }
    else
    {
        g.setColour (lineColour);
        g.strokePath (waveform, stroke);
    }
}

void OscilloscopeComponent::paint (juce::Graphics& g)
{
    auto waveArea = getLocalBounds().toFloat();

    if (! expanded)
    {
        juce::Path window;
        window.addRoundedRectangle (waveArea.reduced (0.5f), 5.0f);
        g.setColour (juce::Colour::fromRGBA (12, 10, 8, 170));
        g.fillPath (window);
        g.setColour (juce::Colour::fromRGBA (40, 32, 24, 140));
        g.strokePath (window, juce::PathStrokeType (1.0f));
    }

    auto drawZoomLabel = [this, &g]()
    {
        const float fontH = expanded ? 13.0f : 10.5f;
        g.setFont (juce::FontOptions().withHeight (fontH));
        g.setColour (juce::Colours::whitesmoke.withAlpha (expanded ? 0.70f : 0.55f));
        auto textArea = getLocalBounds().reduced (expanded ? 10 : 5, expanded ? 8 : 3);
        g.drawText (getZoomLabel(),
                    textArea.removeFromBottom (expanded ? 16 : 13),
                    juce::Justification::bottomRight,
                    false);
    };

    const float padY = expanded ? juce::jmax (8.0f, waveArea.getHeight() * 0.08f)
                                : (float) kWavePadPx;
    auto plot = waveArea.reduced (expanded ? 4.0f : 1.0f, padY);
    if (plot.getWidth() < 2.0f || plot.getHeight() < 2.0f || columns.empty())
    {
        drawZoomLabel();
        return;
    }

    const bool highQuality = isHighQuality();
    const float lineOpacity = juce::jlimit (0.0f, 1.0f, loadFloatParam ("OSC_LINE_OPACITY_ID", 100.0f) * 0.01f);

    // Compact strip and expanded overlay have independent line/glow settings.
    const float pathWidth = juce::jlimit (0.5f, 8.0f,
                                          loadFloatParam (expanded ? "OSC_EXPANDED_LINE_WIDTH_ID" : "OSC_LINE_WIDTH_ID",
                                                          expanded ? 1.0f : 1.5f));
    const bool glowEnabled = loadFloatParam (expanded ? "OSC_EXPANDED_GLOW_ENABLE_ID" : "OSC_GLOW_ENABLE_ID", 0.0f) > 0.5f;
    const float glowOpacity = juce::jlimit (0.0f, 1.0f,
                                            loadFloatParam (expanded ? "OSC_EXPANDED_GLOW_OPACITY_ID" : "OSC_GLOW_OPACITY_ID", 75.0f) * 0.01f);
    const float glowRadius = juce::jlimit (0.0f, expanded ? 80.0f : 40.0f,
                                           loadFloatParam (expanded ? "OSC_EXPANDED_GLOW_RADIUS_ID" : "OSC_GLOW_RADIUS_ID",
                                                           expanded ? 18.0f : 6.0f));
    const float glowSpread = juce::jlimit (0.0f, expanded ? 40.0f : 20.0f,
                                           loadFloatParam (expanded ? "OSC_EXPANDED_GLOW_SPREAD_ID" : "OSC_GLOW_SPREAD_ID",
                                                           expanded ? 2.0f : 1.0f));

    const int n = (int) columns.size();
    const float xScale = plot.getWidth() / (float) juce::jmax (1, n);
    juce::Path waveform;

    if (channelMode == ChannelMode::splitStereo)
    {
        const float midY = plot.getCentreY();
        const float halfH = plot.getHeight() * 0.5f;
        auto top = juce::Rectangle<float> (plot.getX(), plot.getY(), plot.getWidth(), halfH - 1.0f);
        auto bottom = juce::Rectangle<float> (plot.getX(), midY + 1.0f, plot.getWidth(), halfH - 1.0f);

        g.setColour (juce::Colour::fromRGBA (80, 70, 50, 90));
        g.drawHorizontalLine (juce::roundToInt (midY), plot.getX(), plot.getRight());

        for (int x = 0; x < n; ++x)
        {
            const auto& col = columns[(size_t) x];
            if (! col.valid)
                continue;

            const float px = plot.getX() + ((float) x + 0.5f) * xScale;
            appendColumnStub (waveform, px,
                              top.getY() + (0.5f - 0.5f * col.maxL) * top.getHeight(),
                              top.getY() + (0.5f - 0.5f * col.minL) * top.getHeight());
            appendColumnStub (waveform, px,
                              bottom.getY() + (0.5f - 0.5f * col.maxR) * bottom.getHeight(),
                              bottom.getY() + (0.5f - 0.5f * col.minR) * bottom.getHeight());
        }
    }
    else
    {
        for (int x = 0; x < n; ++x)
        {
            const auto& col = columns[(size_t) x];
            if (! col.valid)
                continue;

            const float px = plot.getX() + ((float) x + 0.5f) * xScale;
            appendColumnStub (waveform, px,
                              plot.getY() + (0.5f - 0.5f * col.maxSum) * plot.getHeight(),
                              plot.getY() + (0.5f - 0.5f * col.minSum) * plot.getHeight());
        }
    }

    strokeWaveform (g, waveform, pathWidth, lineOpacity, highQuality,
                    glowEnabled, glowOpacity, glowRadius, glowSpread);
    drawZoomLabel();
}
