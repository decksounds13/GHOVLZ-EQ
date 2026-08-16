#include "ThdMeterComponent.h"
#include "ComboBoxLookAndFeel.h"
#include <cmath>

namespace
{
// Log bands covering 20 Hz – Nyquist (capped at 20 kHz for labels)
constexpr float kBandEdgesHz[ThdMeterComponent::kNumBands + 1] = {
    20.0f, 80.0f, 250.0f, 800.0f, 2500.0f, 8000.0f, 20000.0f
};
} // namespace

const char* ThdMeterComponent::bandLabel (int index) noexcept
{
    static constexpr const char* kLabels[kNumBands] = {
        "Sub", "Low", "LMid", "Mid", "High", "Air"
    };
    if (index < 0 || index >= kNumBands)
        return "?";
    return kLabels[index];
}

void ThdMeterComponent::bandFreqRange (int index, float& loHz, float& hiHz) noexcept
{
    index = juce::jlimit (0, kNumBands - 1, index);
    loHz = kBandEdgesHz[index];
    hiHz = kBandEdgesHz[index + 1];
}

ThdMeterComponent::ThdMeterComponent()
{
    setOpaque (true);
    ring.assign ((size_t) kFftSize * 4, 0.0f);
    workTime.assign ((size_t) kFftSize, 0.0f);
    workFft.assign ((size_t) kFftSize * 2, 0.0f);
    mag.assign ((size_t) kFftSize / 2 + 1, 0.0f);
    for (auto& h : harmonicNorm)
        h.store (0.0f, std::memory_order_relaxed);
    for (auto& b : bandResidual)
        b.store (0.0f, std::memory_order_relaxed);
    for (auto& b : bandPeakNorm)
        b.store (0.0f, std::memory_order_relaxed);
    startTimerHz (24);
}

ThdMeterComponent::~ThdMeterComponent()
{
    stopTimer();
}

void ThdMeterComponent::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    fifo.reset();
    std::fill (ring.begin(), ring.end(), 0.0f);
    smoothThdPct = 0.0f;
    smoothThdDb = -120.0f;
    smoothF0 = 0.0f;
    smoothLock = 0.0f;
    smoothHarm.fill (0.0f);
    smoothBandRes.fill (0.0f);
    smoothBandPeak.fill (0.0f);
}

void ThdMeterComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
}

void ThdMeterComponent::setUiTimerRunning (bool shouldRun) noexcept
{
    if (shouldRun)
        startTimerHz (24);
    else
        stopTimer();
}

void ThdMeterComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || left == nullptr || numSamples <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    fifo.prepareToWrite (numSamples, start1, size1, start2, size2);
    auto write = [&] (int start, int size, int srcOff)
    {
        for (int i = 0; i < size; ++i)
        {
            const float l = left[srcOff + i];
            const float r = right != nullptr ? right[srcOff + i] : l;
            ring[(size_t) (start + i)] = 0.5f * (l + r);
        }
    };
    write (start1, size1, 0);
    write (start2, size2, size1);
    fifo.finishedWrite (size1 + size2);
}

float ThdMeterComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree == nullptr)
        return fallback;
    if (auto* p = valueTree->getRawParameterValue (id))
        return p->load();
    return fallback;
}

int ThdMeterComponent::loadIntParam (const char* id, int fallback) const
{
    return (int) std::lround (loadFloatParam (id, (float) fallback));
}

bool ThdMeterComponent::loadBoolParam (const char* id, bool fallback) const
{
    if (valueTree == nullptr)
        return fallback;
    if (auto* p = valueTree->getRawParameterValue (id))
        return p->load() >= 0.5f;
    return fallback;
}

ThdMeterComponent::DisplayMode ThdMeterComponent::currentMode() const
{
    const int m = loadIntParam ("THD_MODE_ID", 0);
    return m >= 1 ? DisplayMode::multiband : DisplayMode::broadband;
}

void ThdMeterComponent::setMode (DisplayMode mode)
{
    if (valueTree == nullptr)
        return;
    if (auto* p = valueTree->getParameter ("THD_MODE_ID"))
        p->setValueNotifyingHost (p->convertTo0to1 ((float) (int) mode));
    repaint();
}

void ThdMeterComponent::analyseIfReady()
{
    if (fifo.getNumReady() < kFftSize)
        return;

    const int ready = fifo.getNumReady();
    if (ready > kFftSize)
    {
        int skip = ready - kFftSize;
        int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
        fifo.prepareToRead (skip, s1, n1, s2, n2);
        fifo.finishedRead (n1 + n2);
    }

    int s1 = 0, n1 = 0, s2 = 0, n2 = 0;
    fifo.prepareToRead (kFftSize, s1, n1, s2, n2);
    int out = 0;
    for (int i = 0; i < n1; ++i)
        workTime[(size_t) out++] = ring[(size_t) (s1 + i)];
    for (int i = 0; i < n2; ++i)
        workTime[(size_t) out++] = ring[(size_t) (s2 + i)];
    fifo.finishedRead (n1 + n2);

    if (out < kFftSize)
        return;

    window.multiplyWithWindowingTable (workTime.data(), (size_t) kFftSize);
    std::fill (workFft.begin(), workFft.end(), 0.0f);
    std::copy (workTime.begin(), workTime.begin() + kFftSize, workFft.begin());
    fft.performFrequencyOnlyForwardTransform (workFft.data(), true);

    const int nBins = kFftSize / 2;
    for (int i = 0; i <= nBins; ++i)
        mag[(size_t) i] = workFft[(size_t) i];

    const float binHz = (float) (sampleRate / (double) kFftSize);
    const float nyquist = (float) (sampleRate * 0.5);
    const int maxHarm = juce::jlimit (2, kMaxHarmonics, loadIntParam ("THD_MAX_HARMONICS_ID", 8));
    const float minLockDb = loadFloatParam ("THD_LOCK_DB_ID", 18.0f);

    // --- Broadband fundamental lock ---
    const float fMaxFund = juce::jmin (5000.0f, nyquist / (float) maxHarm - binHz);
    const int binLo = juce::jmax (1, (int) std::floor (40.0f / binHz));
    const int binHi = juce::jmax (binLo + 2, (int) std::floor (fMaxFund / binHz));
    const int binHiClamped = juce::jmin (binHi, nBins - 1);

    double sumMag = 0.0;
    int countMag = 0;
    int peakBin = binLo;
    float peakMag = 0.0f;
    for (int b = binLo; b <= binHiClamped; ++b)
    {
        const float m = mag[(size_t) b];
        sumMag += (double) m;
        ++countMag;
        if (m > peakMag)
        {
            peakMag = m;
            peakBin = b;
        }
    }

    const float meanMag = countMag > 0 ? (float) (sumMag / (double) countMag) : 1.0e-12f;
    const float floorMag = juce::jmax (meanMag, 1.0e-12f);
    const float peakDb = 20.0f * std::log10 (juce::jmax (peakMag, 1.0e-12f) / floorMag);
    const float lock = juce::jlimit (0.0f, 1.0f, (peakDb - minLockDb * 0.35f) / juce::jmax (6.0f, minLockDb));

    float f0 = (float) peakBin * binHz;
    if (peakBin > 1 && peakBin < nBins - 1)
    {
        const float a = mag[(size_t) (peakBin - 1)];
        const float b = mag[(size_t) peakBin];
        const float c = mag[(size_t) (peakBin + 1)];
        const float denom = a - 2.0f * b + c;
        if (std::abs (denom) > 1.0e-12f)
        {
            const float delta = 0.5f * (a - c) / denom;
            f0 = ((float) peakBin + juce::jlimit (-0.5f, 0.5f, delta)) * binHz;
        }
    }

    auto binEnergy = [&] (float freqHz) -> float
    {
        if (freqHz <= 0.0f || freqHz >= nyquist)
            return 0.0f;
        const float binF = freqHz / binHz;
        const int b0 = juce::jlimit (1, nBins - 1, (int) std::lround (binF));
        float e = mag[(size_t) b0];
        if (b0 > 1)
            e = juce::jmax (e, mag[(size_t) (b0 - 1)]);
        if (b0 + 1 <= nBins)
            e = juce::jmax (e, mag[(size_t) (b0 + 1)]);
        return e * e;
    };

    const float p1 = juce::jmax (binEnergy (f0), 1.0e-24f);
    float harmPower = 0.0f;
    std::array<float, kMaxHarmonics> hNorm {};
    hNorm[0] = 1.0f;
    for (int h = 2; h <= maxHarm; ++h)
    {
        const float ph = binEnergy (f0 * (float) h);
        harmPower += ph;
        hNorm[(size_t) (h - 1)] = std::sqrt (ph / p1);
    }
    for (int h = maxHarm; h < kMaxHarmonics; ++h)
        hNorm[(size_t) h] = 0.0f;

    float thdPct = 0.0f;
    float thdDbVal = -120.0f;
    if (lock > 0.25f && p1 > 1.0e-20f)
    {
        const float thdRatio = std::sqrt (harmPower / p1);
        thdPct = thdRatio * 100.0f;
        thdDbVal = thdRatio > 1.0e-8f ? 20.0f * std::log10 (thdRatio) : -160.0f;
    }

    // --- Multiband residual (works on program material) ---
    std::array<float, kNumBands> bandRes {};
    std::array<float, kNumBands> bandPk {};
    float globalPeak = 1.0e-12f;

    for (int bi = 0; bi < kNumBands; ++bi)
    {
        float loHz = 0, hiHz = 0;
        bandFreqRange (bi, loHz, hiHz);
        hiHz = juce::jmin (hiHz, nyquist - binHz);

        const int b0 = juce::jmax (1, (int) std::floor (loHz / binHz));
        const int b1 = juce::jmax (b0 + 1, juce::jmin (nBins - 1, (int) std::ceil (hiHz / binHz)));

        double total = 0.0;
        float peak = 0.0f;
        for (int b = b0; b <= b1; ++b)
        {
            const float m = mag[(size_t) b];
            const float e = m * m;
            total += (double) e;
            peak = juce::jmax (peak, m);
        }

        globalPeak = juce::jmax (globalPeak, peak);

        // Residual = energy not concentrated in the strongest peak bin (3-bin peak region)
        float peakRegion = 0.0f;
        int peakB = b0;
        float peakM = 0.0f;
        for (int b = b0; b <= b1; ++b)
        {
            if (mag[(size_t) b] > peakM)
            {
                peakM = mag[(size_t) b];
                peakB = b;
            }
        }
        for (int b = juce::jmax (b0, peakB - 1); b <= juce::jmin (b1, peakB + 1); ++b)
            peakRegion += mag[(size_t) b] * mag[(size_t) b];

        const float tot = (float) juce::jmax (total, 1.0e-24);
        bandRes[(size_t) bi] = juce::jlimit (0.0f, 1.0f, 1.0f - peakRegion / tot);
        bandPk[(size_t) bi] = peak;
    }

    for (int bi = 0; bi < kNumBands; ++bi)
        bandPk[(size_t) bi] = bandPk[(size_t) bi] / globalPeak;

    // Smooth
    const float a = 0.28f;
    const float aSlow = 0.18f;
    if (lock > 0.25f)
    {
        smoothThdPct += a * (thdPct - smoothThdPct);
        smoothThdDb += a * (thdDbVal - smoothThdDb);
        smoothF0 += aSlow * (f0 - smoothF0);
    }
    else
    {
        smoothThdPct += 0.15f * (0.0f - smoothThdPct);
        smoothThdDb += 0.15f * (-120.0f - smoothThdDb);
    }
    smoothLock += 0.25f * (lock - smoothLock);
    for (int i = 0; i < kMaxHarmonics; ++i)
        smoothHarm[(size_t) i] += a * (hNorm[(size_t) i] - smoothHarm[(size_t) i]);
    for (int i = 0; i < kNumBands; ++i)
    {
        smoothBandRes[(size_t) i] += a * (bandRes[(size_t) i] - smoothBandRes[(size_t) i]);
        smoothBandPeak[(size_t) i] += a * (bandPk[(size_t) i] - smoothBandPeak[(size_t) i]);
    }

    thdPercent.store (smoothThdPct, std::memory_order_relaxed);
    thdDb.store (smoothThdDb, std::memory_order_relaxed);
    fundamentalHz.store (smoothF0, std::memory_order_relaxed);
    lockQuality.store (smoothLock, std::memory_order_relaxed);
    for (int i = 0; i < kMaxHarmonics; ++i)
        harmonicNorm[(size_t) i].store (smoothHarm[(size_t) i], std::memory_order_relaxed);
    for (int i = 0; i < kNumBands; ++i)
    {
        bandResidual[(size_t) i].store (smoothBandRes[(size_t) i], std::memory_order_relaxed);
        bandPeakNorm[(size_t) i].store (smoothBandPeak[(size_t) i], std::memory_order_relaxed);
    }
}

void ThdMeterComponent::timerCallback()
{
    if (! enabled.load (std::memory_order_relaxed))
        return;
    analyseIfReady();
    repaint();
}

void ThdMeterComponent::resized() {}

void ThdMeterComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
    {
        showContextMenu();
        return;
    }

    if (broadbandChip.contains (e.getPosition()))
    {
        setMode (DisplayMode::broadband);
        return;
    }
    if (multibandChip.contains (e.getPosition()))
    {
        setMode (DisplayMode::multiband);
        return;
    }
}

void ThdMeterComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick)
        onDoubleClick();
}

void ThdMeterComponent::showContextMenu()
{
    juce::PopupMenu m;
    m.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const auto mode = currentMode();
    m.addItem (1, "Broadband", true, mode == DisplayMode::broadband);
    m.addItem (2, "Multiband", true, mode == DisplayMode::multiband);
    m.addSeparator();
    m.addItem (10, "Scope Menu...");

    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe = juce::Component::SafePointer<ThdMeterComponent> (this)] (int result)
                     {
                         if (safe == nullptr || result == 0)
                             return;
                         if (result == 1)
                             safe->setMode (DisplayMode::broadband);
                         else if (result == 2)
                             safe->setMode (DisplayMode::multiband);
                         else if (result == 10 && safe->onShowContextMenu)
                             safe->onShowContextMenu();
                     });
}

void ThdMeterComponent::paintBroadband (juce::Graphics& g, juce::Rectangle<int> area,
                                        juce::Colour text, juce::Colour muted,
                                        juce::Colour accent, juce::Colour accentCool, bool locked)
{
    const float pct = thdPercent.load (std::memory_order_relaxed);
    const float db = thdDb.load (std::memory_order_relaxed);
    const float f0 = fundamentalHz.load (std::memory_order_relaxed);

    {
        const int mainH = juce::jmax (44, area.getHeight() * 2 / 5);
        auto main = area.removeFromTop (mainH);

        const float bigH = juce::jlimit (22.0f, 48.0f, (float) main.getHeight() * 0.55f);
        auto bigFont = juce::Font (juce::FontOptions (bigH).withStyle ("Bold"));

        juce::String pctStr = locked ? juce::String (pct, pct >= 10.0f ? 1 : 2) + " %" : "-- %";
        juce::String dbStr = locked ? (db <= -99.0f ? "<-99 dB" : juce::String (db, 1) + " dB") : "-- dB";

        g.setFont (bigFont);
        juce::Colour pctCol = text;
        if (locked)
        {
            const float t = juce::jlimit (0.0f, 1.0f, pct / 25.0f);
            pctCol = accentCool.interpolatedWith (accent, t);
        }
        g.setColour (pctCol);
        g.drawText (pctStr, main.removeFromTop ((int) (bigH + 4)), juce::Justification::centredLeft, false);

        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (11.0f, 16.0f, bigH * 0.38f))));
        g.setColour (text.withAlpha (0.8f));
        g.drawText (dbStr, main.removeFromTop ((int) (bigH * 0.42f)), juce::Justification::centredLeft, false);

        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (10.0f, 13.0f, bigH * 0.32f))));
        g.setColour (muted);
        const juce::String f0Str = locked && f0 > 1.0f
                                       ? ("f0  " + juce::String (f0, f0 >= 1000.0f ? 0 : 1) + " Hz")
                                       : "f0  -- Hz";
        g.drawText (f0Str, main, juce::Justification::centredLeft, false);
    }

    area.removeFromTop (6);

    auto barArea = area;
    const int labelH = juce::jmax (12, barArea.getHeight() / 8);
    auto labels = barArea.removeFromBottom (labelH);
    const int nBars = kMaxHarmonics;
    const float gap = 4.0f;
    const float barW = juce::jmax (6.0f, ((float) barArea.getWidth() - gap * (float) (nBars - 1)) / (float) nBars);

    float maxH = 0.001f;
    std::array<float, kMaxHarmonics> vals {};
    for (int i = 0; i < nBars; ++i)
    {
        vals[(size_t) i] = harmonicNorm[(size_t) i].load (std::memory_order_relaxed);
        maxH = juce::jmax (maxH, vals[(size_t) i]);
    }

    for (int i = 0; i < nBars; ++i)
    {
        const float x = (float) barArea.getX() + (float) i * (barW + gap);
        auto r = juce::Rectangle<float> (x, (float) barArea.getY(), barW, (float) barArea.getHeight());

        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.12f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        const float norm = locked ? juce::jlimit (0.0f, 1.0f, vals[(size_t) i] / maxH) : 0.0f;
        if (norm > 0.001f)
        {
            auto fill = r.withTop (r.getBottom() - r.getHeight() * norm);
            const auto col = (i == 0) ? accentCool
                                      : accent.interpolatedWith (juce::Colours::orangered, (float) i / 8.0f);
            juce::ColourGradient grad (col.brighter (0.15f), fill.getCentreX(), fill.getY(),
                                      col.darker (0.25f), fill.getCentreX(), fill.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 3.0f);
        }

        auto lab = labels.withX ((int) x).withWidth ((int) std::ceil (barW));
        g.setColour (i == 0 ? accentCool.withAlpha (0.9f) : text.withAlpha (0.65f));
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (8.0f, 11.0f, (float) labelH * 0.85f)).withStyle ("Bold")));
        g.drawText ("H" + juce::String (i + 1), lab, juce::Justification::centred, false);
    }
}

void ThdMeterComponent::paintMultiband (juce::Graphics& g, juce::Rectangle<int> area,
                                        juce::Colour text, juce::Colour muted,
                                        juce::Colour accent, juce::Colour accentCool, bool locked)
{
    // Compact broadband summary strip
    {
        auto strip = area.removeFromTop (juce::jmax (22, area.getHeight() / 7));
        const float pct = thdPercent.load (std::memory_order_relaxed);
        const float f0 = fundamentalHz.load (std::memory_order_relaxed);
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (11.0f, 14.0f, (float) strip.getHeight() * 0.7f)).withStyle ("Bold")));
        g.setColour (locked ? accentCool.interpolatedWith (accent, juce::jlimit (0.0f, 1.0f, pct / 25.0f))
                            : muted);
        const juce::String left = locked
                                      ? ("THD  " + juce::String (pct, pct >= 10.0f ? 1 : 2) + " %")
                                      : "THD  -- %";
        g.drawText (left, strip.removeFromLeft (strip.getWidth() / 2), juce::Justification::centredLeft, false);
        g.setColour (muted);
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (10.0f, 12.0f, (float) strip.getHeight() * 0.6f))));
        const juce::String right = locked && f0 > 1.0f
                                       ? ("f0  " + juce::String (f0, f0 >= 1000.0f ? 0 : 1) + " Hz")
                                       : "Band residual";
        g.drawText (right, strip, juce::Justification::centredRight, false);
    }

    area.removeFromTop (4);

    g.setFont (juce::Font (juce::FontOptions (9.0f)));
    g.setColour (muted.withAlpha (0.75f));
    g.drawText ("Residual (non-peak energy in band)", area.removeFromTop (12),
                juce::Justification::centredLeft, false);

    area.removeFromTop (2);

    const int labelH = juce::jmax (12, area.getHeight() / 9);
    auto labels = area.removeFromBottom (labelH);
    const float gap = 5.0f;
    const float barW = juce::jmax (10.0f, ((float) area.getWidth() - gap * (float) (kNumBands - 1)) / (float) kNumBands);

    for (int i = 0; i < kNumBands; ++i)
    {
        const float res = bandResidual[(size_t) i].load (std::memory_order_relaxed);
        const float pk = bandPeakNorm[(size_t) i].load (std::memory_order_relaxed);
        const float x = (float) area.getX() + (float) i * (barW + gap);
        auto r = juce::Rectangle<float> (x, (float) area.getY(), barW, (float) area.getHeight());

        // Track
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.fillRoundedRectangle (r, 3.0f);
        g.setColour (juce::Colours::white.withAlpha (0.10f));
        g.drawRoundedRectangle (r, 3.0f, 1.0f);

        // Level ghost (peak energy in band) as thin side mark
        const float ghostH = r.getHeight() * juce::jlimit (0.0f, 1.0f, pk);
        if (ghostH > 1.0f)
        {
            g.setColour (accentCool.withAlpha (0.18f));
            g.fillRoundedRectangle (r.withTop (r.getBottom() - ghostH), 3.0f);
        }

        // Residual fill (dirt)
        const float fillH = r.getHeight() * juce::jlimit (0.0f, 1.0f, res);
        if (fillH > 1.0f)
        {
            auto fill = r.withTop (r.getBottom() - fillH);
            const auto col = accentCool.interpolatedWith (accent, res)
                                 .interpolatedWith (juce::Colours::orangered, juce::jmax (0.0f, res - 0.45f) * 1.6f);
            juce::ColourGradient grad (col.brighter (0.2f), fill.getCentreX(), fill.getY(),
                                      col.darker (0.3f), fill.getCentreX(), fill.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (fill, 3.0f);
        }

        // % readout on bar if tall enough
        if (r.getHeight() > 36.0f)
        {
            g.setFont (juce::Font (juce::FontOptions (juce::jlimit (8.0f, 10.0f, barW * 0.35f)).withStyle ("Bold")));
            g.setColour (text.withAlpha (0.85f));
            g.drawText (juce::String ((int) std::lround (res * 100.0f)),
                        r.reduced (1.0f, 2.0f), juce::Justification::centredTop, false);
        }

        auto lab = labels.withX ((int) x).withWidth ((int) std::ceil (barW));
        g.setColour (text.withAlpha (0.7f));
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (8.0f, 11.0f, (float) labelH * 0.8f)).withStyle ("Bold")));
        g.drawText (bandLabel (i), lab, juce::Justification::centred, false);
    }
}

void ThdMeterComponent::paint (juce::Graphics& g)
{
    const auto bg = themeColors != nullptr ? themeColors->sharedColors.oscBackground.darker (0.08f)
                                           : juce::Colour::fromRGB (12, 14, 18);
    g.fillAll (bg);

    auto area = getLocalBounds().reduced (8);
    if (area.getWidth() < 40 || area.getHeight() < 40)
        return;

    const float lock = lockQuality.load (std::memory_order_relaxed);
    const bool locked = lock > 0.28f;
    const auto mode = currentMode();

    const auto accent = juce::Colour::fromRGB (255, 176, 72);
    const auto accentCool = juce::Colour::fromRGB (120, 200, 255);
    const auto muted = juce::Colours::whitesmoke.withAlpha (0.55f);
    const auto text = themeColors != nullptr
                          ? themeColors->sharedColors.legibleTextOn (
                                themeColors->sharedColors.meterReadoutText, bg)
                          : juce::Colours::whitesmoke.withAlpha (0.92f);

    // Header: title + mode chips + lock
    {
        auto header = area.removeFromTop (juce::jmax (18, area.getHeight() / 13));
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (10.0f, 13.0f, (float) header.getHeight() * 0.85f))
                                   .withStyle ("Bold")));
        g.setColour (text.withAlpha (0.75f));
        g.drawText ("THD", header.removeFromLeft (36), juce::Justification::centredLeft, false);

        auto pill = header.removeFromRight (juce::jmin (68, header.getWidth() / 4)).toFloat().reduced (0, 1);
        g.setColour (locked ? accent.withAlpha (0.22f) : juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (pill, 4.0f);
        g.setColour (locked ? accent : muted);
        g.setFont (juce::Font (juce::FontOptions (juce::jlimit (8.0f, 11.0f, pill.getHeight() * 0.7f)).withStyle ("Bold")));
        g.drawText (locked ? "Locked" : "Seeking", pill, juce::Justification::centred, false);

        header.removeFromRight (6);

        // Mode chips (measure full label width — no ellipsis)
        auto chipFont = juce::Font (juce::FontOptions (juce::jlimit (9.0f, 11.0f, (float) header.getHeight() * 0.65f)).withStyle ("Bold"));
        g.setFont (chipFont);
        const int chipH = header.getHeight();
        const int pad = 8;
        const int bbW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (chipFont, "Broadband")) + pad * 2;
        const int mbW = (int) std::ceil (juce::GlyphArrangement::getStringWidth (chipFont, "Multiband")) + pad * 2;

        multibandChip = header.removeFromRight (mbW).withHeight (chipH);
        header.removeFromRight (4);
        broadbandChip = header.removeFromRight (bbW).withHeight (chipH);

        auto drawChip = [&] (juce::Rectangle<int> r, const char* label, bool on)
        {
            auto rf = r.toFloat().reduced (0, 1);
            g.setColour (on ? accentCool.withAlpha (0.28f) : juce::Colours::white.withAlpha (0.06f));
            g.fillRoundedRectangle (rf, 4.0f);
            g.setColour (on ? accentCool : muted);
            g.setFont (chipFont);
            g.drawText (label, rf, juce::Justification::centred, false);
        };
        drawChip (broadbandChip, "Broadband", mode == DisplayMode::broadband);
        drawChip (multibandChip, "Multiband", mode == DisplayMode::multiband);
    }

    area.removeFromTop (4);

    if (mode == DisplayMode::multiband)
        paintMultiband (g, area, text, muted, accent, accentCool, locked);
    else
        paintBroadband (g, area, text, muted, accent, accentCool, locked);
}
