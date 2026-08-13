#include "LoudnessComponent.h"
#include "Menu/SharedResources.h"

namespace
{
float msToLufs (double meanSquare) noexcept
{
    if (meanSquare <= 1.0e-12)
        return -70.0f;
    return (float) (-0.691 + 10.0 * std::log10 (meanSquare));
}
} // namespace

LoudnessComponent::LoudnessComponent()
{
    setOpaque (false);
    startTimerHz (20);
}

LoudnessComponent::~LoudnessComponent()
{
    stopTimer();
}

void LoudnessComponent::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    momWindow = juce::jmax (1, (int) std::lround (0.400 * sampleRate));
    shortWindow = juce::jmax (1, (int) std::lround (3.0 * sampleRate));
    updateKCoeffs (sampleRate);
    const juce::ScopedLock sl (lock);
    stateL = {};
    stateR = {};
    momSum = shortSum = integSum = 0.0;
    momCount = shortCount = integCount = 0;
}

void LoudnessComponent::updateKCoeffs (double sr)
{
    const double f = sr;
    auto bilinearShelf = [&] (double f0, double Gdb, float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double A = std::pow (10.0, Gdb / 40.0);
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / f;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / 2.0 * std::sqrt (2.0);
        const double b0d =    A * ((A + 1) + (A - 1) * cosw + 2 * std::sqrt (A) * alpha);
        const double b1d = -2 * A * ((A - 1) + (A + 1) * cosw);
        const double b2d =    A * ((A + 1) + (A - 1) * cosw - 2 * std::sqrt (A) * alpha);
        const double a0d =        (A + 1) - (A - 1) * cosw + 2 * std::sqrt (A) * alpha;
        const double a1d =  2 * ((A - 1) - (A + 1) * cosw);
        const double a2d =        (A + 1) - (A - 1) * cosw - 2 * std::sqrt (A) * alpha;
        b0 = (float) (b0d / a0d); b1 = (float) (b1d / a0d); b2 = (float) (b2d / a0d);
        a1 = (float) (a1d / a0d); a2 = (float) (a2d / a0d);
    };

    auto bilinearHp = [&] (double f0, float& b0, float& b1, float& b2, float& a1, float& a2)
    {
        const double w0 = 2.0 * juce::MathConstants<double>::pi * f0 / f;
        const double cosw = std::cos (w0);
        const double sinw = std::sin (w0);
        const double alpha = sinw / 2.0 * std::sqrt (2.0);
        const double b0d =  (1 + cosw) / 2;
        const double b1d = -(1 + cosw);
        const double b2d =  (1 + cosw) / 2;
        const double a0d =   1 + alpha;
        const double a1d =  -2 * cosw;
        const double a2d =   1 - alpha;
        b0 = (float) (b0d / a0d); b1 = (float) (b1d / a0d); b2 = (float) (b2d / a0d);
        a1 = (float) (a1d / a0d); a2 = (float) (a2d / a0d);
    };

    bilinearShelf (1681.974450955533, 3.999843853973347, b0a, b1a, b2a, a1a, a2a);
    bilinearHp (38.13547087602444, b0b, b1b, b2b, a1b, a2b);
}

float LoudnessComponent::processKWeight (float x, KWeightState& s) const noexcept
{
    const float v1 = x - a1a * s.z1a - a2a * s.z2a;
    const float yShelf = b0a * v1 + b1a * s.z1a + b2a * s.z2a;
    s.z2a = s.z1a;
    s.z1a = v1;

    const float v2 = yShelf - a1b * s.z1b - a2b * s.z2b;
    const float y = b0b * v2 + b1b * s.z1b + b2b * s.z2b;
    s.z2b = s.z1b;
    s.z1b = v2;
    return y;
}

void LoudnessComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

void LoudnessComponent::setUiTimerRunning (bool shouldRun) noexcept
{
    if (shouldRun)
        startTimerHz (20);
    else
        stopTimer();
}

void LoudnessComponent::resetIntegrated() noexcept
{
    const juce::ScopedLock sl (lock);
    integSum = 0.0;
    integCount = 0;
    integratedLufs.store (-70.0f, std::memory_order_relaxed);
}

void LoudnessComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || left == nullptr || numSamples <= 0)
        return;

    const juce::ScopedTryLock sl (lock);
    if (! sl.isLocked())
        return;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = processKWeight (left[i], stateL);
        const float r = processKWeight (right != nullptr ? right[i] : left[i], stateR);
        const double ms = 0.5 * ((double) l * (double) l + (double) r * (double) r);

        momSum += ms;
        ++momCount;
        if (momCount >= momWindow)
        {
            momentaryLufs.store (msToLufs (momSum / (double) momCount), std::memory_order_relaxed);
            momSum = 0.0;
            momCount = 0;
        }

        shortSum += ms;
        ++shortCount;
        if (shortCount >= shortWindow)
        {
            shortTermLufs.store (msToLufs (shortSum / (double) shortCount), std::memory_order_relaxed);
            shortSum = 0.0;
            shortCount = 0;
        }

        integSum += ms;
        ++integCount;
        if ((integCount & 2047) == 0 && integCount > 0)
            integratedLufs.store (msToLufs (integSum / (double) integCount), std::memory_order_relaxed);
    }
}

float LoudnessComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree == nullptr)
        return fallback;
    if (auto* p = valueTree->getRawParameterValue (id))
        return p->load();
    return fallback;
}

int LoudnessComponent::loadIntParam (const char* id, int fallback) const
{
    return (int) std::lround (loadFloatParam (id, (float) fallback));
}

float LoudnessComponent::lufsToMeterNorm (float lufs, float minLufs, float maxLufs) noexcept
{
    if (lufs <= -69.5f)
        return 0.0f;
    return juce::jlimit (0.0f, 1.0f, (lufs - minLufs) / juce::jmax (0.001f, maxLufs - minLufs));
}

void LoudnessComponent::timerCallback()
{
    if (enabled.load (std::memory_order_relaxed))
        repaint();
}

void LoudnessComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ignoreUnused (bounds);
    // Opaque fill (maximize / F11 / Scope — no bleed from underneath).
    const auto bg = themeColors != nullptr ? themeColors->sharedColors.oscBackground.darker (0.08f)
                                           : juce::Colour::fromRGB (14, 12, 10);
    g.fillAll (bg);

    const float target = loadFloatParam ("LOUDNESS_TARGET_ID", -14.0f);
    const float m = momentaryLufs.load (std::memory_order_relaxed);
    const float s = shortTermLufs.load (std::memory_order_relaxed);
    const float integ = integratedLufs.load (std::memory_order_relaxed);

    auto fmt = [] (float v)
    {
        if (v <= -69.5f)
            return juce::String ("-inf LUFS");
        return juce::String (v, 1) + " LUFS";
    };

    auto area = getLocalBounds().reduced (4);
    const int footerH = juce::jmax (14, juce::roundToInt ((float) area.getHeight() * 0.12f));
    auto footer = area.removeFromBottom (footerH);

    const float numH = juce::jlimit (12.0f, 28.0f, (float) area.getHeight() * 0.22f);
    const float labelH = juce::jlimit (9.0f, 14.0f, numH * 0.62f);
    auto numFont = SharedResources::uiFont (numH);
    auto labelFont = juce::Font (juce::FontOptions (labelH));

    const juce::String labels[3] = { "Mom.", "Short", "Int." };
    const juce::String shortTags[3] = { "M", "S", "I" };
    const float values[3] = { m, s, integ };
    const juce::Colour colours[3] = {
        juce::Colour::fromRGB (240, 220, 120),
        juce::Colour::fromRGB (180, 220, 255),
        juce::Colour::fromRGB (180, 255, 180)
    };

    // Readout column keeps its natural width; meters fill the remaining right side.
    g.setFont (labelFont);
    int labelColW = 0;
    for (const auto& lab : labels)
        labelColW = juce::jmax (labelColW, (int) std::ceil (juce::GlyphArrangement::getStringWidth (labelFont, lab)));
    labelColW += 6;

    int valueColW = 0;
    g.setFont (numFont);
    for (float v : values)
        valueColW = juce::jmax (valueColW, (int) std::ceil (juce::GlyphArrangement::getStringWidth (numFont, fmt (v))));
    valueColW = juce::jmax (valueColW, (int) std::ceil (juce::GlyphArrangement::getStringWidth (numFont, "-00.0 LUFS")));
    valueColW += 8;

    constexpr int kMeterPad = 8;
    constexpr int kScaleW = 22;
    const int readoutW = labelColW + valueColW;
    // Give readouts their measured width first so "LUFS" suffixes never clip.
    constexpr int kMinMeterW = 36 + kScaleW;
    const int actualReadoutW = juce::jmin (readoutW, juce::jmax (80, area.getWidth() - kMinMeterW - kMeterPad));
    auto readoutCol = area.removeFromLeft (actualReadoutW);
    area.removeFromLeft (kMeterPad);
    auto meterCol = area;

    const auto readoutSeed = themeColors != nullptr ? themeColors->sharedColors.meterReadoutText
                                                    : juce::Colours::whitesmoke.withAlpha (0.92f);
    const auto readoutText = themeColors != nullptr
                                 ? themeColors->sharedColors.legibleTextOn (readoutSeed, bg)
                                 : readoutSeed;

    // Pack Mom./Short/Int. toward the vertical middle (less empty padding between rows).
    const int rowH = juce::jmax ((int) (numH + labelH * 0.15f) + 2, (int) (numH + 4.0f));
    const int packedH = rowH * 3;
    const int topPad = juce::jmax (0, (readoutCol.getHeight() - packedH) / 2);
    readoutCol.removeFromTop (topPad);

    for (int i = 0; i < 3; ++i)
    {
        auto r = readoutCol.removeFromTop (rowH);
        auto labelArea = r.removeFromLeft (labelColW);
        g.setColour (readoutText.withAlpha (0.7f));
        g.setFont (labelFont);
        g.drawText (labels[i], labelArea, juce::Justification::centredRight, false);
        g.setColour (readoutText);
        g.setFont (numFont);
        g.drawText (fmt (values[i]), r, juce::Justification::centredLeft, false);
    }

    // LUFS scale to the left of the bars, then three vertical meters.
    constexpr float minLufs = -70.0f;
    constexpr float maxLufs = 0.0f;
    const int barGap = 4;
    const int tagH = juce::jmax (10, juce::roundToInt (labelH));
    auto meterBody = meterCol;
    auto tagRow = meterBody.removeFromBottom (tagH);

    auto scaleArea = meterBody.removeFromLeft (kScaleW).toFloat().reduced (0.0f, 1.0f);
    meterBody.removeFromLeft (4);
    tagRow.removeFromLeft (kScaleW + 4);

    {
        static constexpr float ticks[] = { 0.0f, -10.0f, -20.0f, -30.0f, -40.0f, -50.0f, -60.0f, -70.0f };
        const float fontH = juce::jlimit (7.0f, 9.0f, scaleArea.getWidth() * 0.42f);
        g.setFont (juce::Font (juce::FontOptions (fontH)));
        g.setColour (readoutText.withAlpha (0.55f));
        for (float tick : ticks)
        {
            const float norm = lufsToMeterNorm (tick, minLufs, maxLufs);
            const float y = scaleArea.getBottom() - scaleArea.getHeight() * norm;
            g.drawText (juce::String ((int) tick),
                        juce::Rectangle<float> (scaleArea.getX(), y - fontH * 0.5f, scaleArea.getWidth(), fontH),
                        juce::Justification::centred, false);
        }
    }

    const int barW = juce::jmax (8, (meterBody.getWidth() - barGap * 2) / 3);
    const float targetNorm = lufsToMeterNorm (target, minLufs, maxLufs);

    for (int i = 0; i < 3; ++i)
    {
        auto barArea = meterBody.removeFromLeft (barW).toFloat().reduced (0.5f, 1.0f);
        if (i < 2)
            meterBody.removeFromLeft (barGap);

        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillRoundedRectangle (barArea, 2.0f);
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.18f));
        g.drawRoundedRectangle (barArea, 2.0f, 1.0f);

        const float norm = lufsToMeterNorm (values[i], minLufs, maxLufs);
        const float fillH = barArea.getHeight() * norm;
        auto fill = barArea.withTop (barArea.getBottom() - fillH);
        g.setColour (colours[i].withAlpha (0.9f));
        g.fillRoundedRectangle (fill, 2.0f);

        const float ty = barArea.getBottom() - barArea.getHeight() * targetNorm;
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.7f));
        g.drawLine (barArea.getX(), ty, barArea.getRight(), ty, 1.0f);

        auto tag = tagRow.removeFromLeft (barW);
        if (i < 2)
            tagRow.removeFromLeft (barGap);
        g.setColour (colours[i].withAlpha (0.9f));
        g.setFont (juce::Font (juce::FontOptions ((float) juce::jmax (8, tagH - 2)).withStyle ("Bold")));
        g.drawText (shortTags[i], tag, juce::Justification::centred, false);
    }

    // Scale unit under the scale column
    g.setFont (juce::Font (juce::FontOptions (7.0f)));
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.5f));
    g.drawText ("LUFS",
                juce::Rectangle<int> (meterCol.getX(), tagRow.getY(), kScaleW, tagH),
                juce::Justification::centred, false);

    g.setFont (labelFont);
    g.setColour (juce::Colours::whitesmoke.withAlpha (0.65f));
    g.drawText ("Target " + juce::String (target, 0) + " LUFS",
                footer, juce::Justification::centred, false);
}

void LoudnessComponent::resized() {}

void LoudnessComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showContextMenu();
}

void LoudnessComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick != nullptr)
        onDoubleClick();
}

void LoudnessComponent::showContextMenu()
{
    if (onShowContextMenu != nullptr)
    {
        onShowContextMenu();
        return;
    }

    juce::PopupMenu menu;
    menu.addItem (1, "Reset Integrated");
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<LoudnessComponent> (this)] (int r)
                        {
                            if (safe != nullptr && r == 1)
                                safe->resetIntegrated();
                        });
}
