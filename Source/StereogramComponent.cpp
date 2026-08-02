#include "StereogramComponent.h"

namespace
{
    juce::String formatGridHz (float hz)
    {
        if (hz >= 1000.0f)
        {
            const float k = hz / 1000.0f;
            if (std::abs (k - std::round (k)) < 0.05f)
                return juce::String ((int) std::round (k)) + "k";
            return juce::String (k, 1) + "k";
        }
        return juce::String ((int) std::round (hz));
    }

}

StereogramComponent::StereogramComponent()
{
    mainWorkL.assign ((size_t) kMainSize * 2, 0.0f);
    mainWorkR.assign ((size_t) kMainSize * 2, 0.0f);
    mainMagL.assign ((size_t) kMainSize / 2 + 1, 0.0f);
    mainMagR.assign ((size_t) kMainSize / 2 + 1, 0.0f);
    balanceByBin.assign ((size_t) kMainSize / 2 + 1, 0.0f);
    energyByBin.assign ((size_t) kMainSize / 2 + 1, 0.0f);
    ensureLfFft();
    ensureRing (lfSize > 0 ? lfSize : kMainSize);
    startTimerHz (kTimerHz);
}

StereogramComponent::~StereogramComponent()
{
    stopTimer();
}

void StereogramComponent::prepare (double sr)
{
    sampleRate = sr > 0.0 ? sr : 48000.0;
    ensureLfFft();
    ensureRing (juce::jmax (kMainSize, lfSize));
}

void StereogramComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
    if (! shouldEnable && trailImage.isValid())
        trailImage.clear (trailImage.getBounds(), juce::Colours::transparentBlack);
}

void StereogramComponent::setColourRamp (const GradientRamp& ramp)
{
    colourRamp = ramp;
    hasCustomRamp = ramp.isUsable();
    repaint();
}

void StereogramComponent::clearColourRamp()
{
    hasCustomRamp = false;
    repaint();
}

void StereogramComponent::ensureRing (int minCapacity)
{
    const int need = juce::jmax (kMainSize, minCapacity);
    if (capacity.load (std::memory_order_acquire) >= need
        && (int) ringL.size() == capacity.load (std::memory_order_relaxed))
        return;

    ringL.assign ((size_t) need, 0.0f);
    ringR.assign ((size_t) need, 0.0f);
    writePos.store (0, std::memory_order_relaxed);
    capacity.store (need, std::memory_order_release);
}

void StereogramComponent::ensureLfFft()
{
    const int order = kMainOrder + kLfBoost; // 4× → 8192
    if (lfFft != nullptr && lfOrder == order)
        return;

    lfOrder = order;
    lfSize = 1 << lfOrder;
    lfFft = std::make_unique<juce::dsp::FFT> (lfOrder);
    lfWindow = std::make_unique<juce::dsp::WindowingFunction<float>> (
        (size_t) lfSize, juce::dsp::WindowingFunction<float>::hann);
    lfWorkL.assign ((size_t) lfSize * 2, 0.0f);
    lfWorkR.assign ((size_t) lfSize * 2, 0.0f);
    lfMagL.assign ((size_t) lfSize / 2 + 1, 0.0f);
    lfMagR.assign ((size_t) lfSize / 2 + 1, 0.0f);
    balanceLf.assign ((size_t) lfSize / 2 + 1, 0.0f);
    energyLf.assign ((size_t) lfSize / 2 + 1, 0.0f);
}

void StereogramComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
{
    if (! enabled.load (std::memory_order_relaxed) || left == nullptr || numSamples <= 0)
        return;

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0 || (int) ringL.size() != cap)
        return;

    const float* r = right != nullptr ? right : left;
    int pos = writePos.load (std::memory_order_relaxed);

    for (int i = 0; i < numSamples; ++i)
    {
        ringL[(size_t) pos] = left[i];
        ringR[(size_t) pos] = r[i];
        pos = (pos + 1) % cap;
    }

    writePos.store (pos, std::memory_order_release);
}

void StereogramComponent::runChannelFft (const float* srcRing, int cap, int endPos, int size,
                                        juce::dsp::FFT& fftEng,
                                        juce::dsp::WindowingFunction<float>& win,
                                        std::vector<float>& work,
                                        std::vector<float>& magOut)
{
    if (srcRing == nullptr || cap <= 0 || size <= 0 || (int) work.size() < size * 2)
        return;

    const int start = ((endPos - size) % cap + cap) % cap;
    std::fill (work.begin(), work.end(), 0.0f);
    for (int i = 0; i < size; ++i)
        work[(size_t) i] = srcRing[(size_t) ((start + i) % cap)];

    win.multiplyWithWindowingTable (work.data(), (size_t) size);
    // Non-negative spectrum only — mag at bin i is |X[i]| (same layout as spectrogram).
    fftEng.performFrequencyOnlyForwardTransform (work.data(), true);

    const int bins = size / 2 + 1;
    if ((int) magOut.size() != bins)
        magOut.assign ((size_t) bins, 0.0f);

    constexpr float smooth = 0.35f;
    // Divide by size so main/LF bins are level-matched at the crossover.
    const float norm = 1.0f / (float) size;
    for (int bin = 1; bin < bins; ++bin)
    {
        const float mag = work[(size_t) bin] * norm;
        magOut[(size_t) bin] += smooth * (mag - magOut[(size_t) bin]);
    }
}

void StereogramComponent::processFromRing()
{
    // Ring is sized in prepare()/ctor — never resize here (audio thread writes it).
    ensureLfFft();

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap < juce::jmax (kMainSize, lfSize) || lfFft == nullptr || lfWindow == nullptr)
        return;

    const int endPos = writePos.load (std::memory_order_acquire);
    const juce::ScopedLock sl (lock);

    runChannelFft (ringL.data(), cap, endPos, kMainSize, mainFft, mainWindow, mainWorkL, mainMagL);
    runChannelFft (ringR.data(), cap, endPos, kMainSize, mainFft, mainWindow, mainWorkR, mainMagR);

    constexpr float smooth = 0.35f;
    const int mainBins = (int) mainMagL.size();
    if ((int) balanceByBin.size() != mainBins)
    {
        balanceByBin.assign ((size_t) mainBins, 0.0f);
        energyByBin.assign ((size_t) mainBins, 0.0f);
    }

    // Display energy: match pre-LF stereogram sensitivity (raw*0.02 ≈ norm*size*0.02).
    // Sqrt compresses the range so quieter HF bins aren't wiped by the density gate.
    auto toDisplayEnergy = [] (float l, float r) noexcept
    {
        const float sum = juce::jmax (0.0f, l + r);
        return juce::jlimit (0.0f, 1.0f, std::sqrt (sum * 96.0f));
    };

    for (int bin = 1; bin < mainBins; ++bin)
    {
        const float l = mainMagL[(size_t) bin];
        const float r = mainMagR[(size_t) bin];
        const float sum = l + r;
        const float balance = sum > 1.0e-12f ? juce::jlimit (-1.0f, 1.0f, (r - l) / sum) : 0.0f;
        const float energy = toDisplayEnergy (l, r);
        balanceByBin[(size_t) bin] += smooth * (balance - balanceByBin[(size_t) bin]);
        energyByBin[(size_t) bin] += smooth * (energy - energyByBin[(size_t) bin]);
    }

    // Longer LF FFT every other frame (spectrogram Enhanced style).
    ++lfFrameCounter;
    if ((lfFrameCounter & 1) == 1)
    {
        runChannelFft (ringL.data(), cap, endPos, lfSize, *lfFft, *lfWindow, lfWorkL, lfMagL);
        runChannelFft (ringR.data(), cap, endPos, lfSize, *lfFft, *lfWindow, lfWorkR, lfMagR);

        const int lfBins = (int) lfMagL.size();
        if ((int) balanceLf.size() != lfBins)
        {
            balanceLf.assign ((size_t) lfBins, 0.0f);
            energyLf.assign ((size_t) lfBins, 0.0f);
        }

        for (int bin = 1; bin < lfBins; ++bin)
        {
            const float l = lfMagL[(size_t) bin];
            const float r = lfMagR[(size_t) bin];
            const float sum = l + r;
            const float balance = sum > 1.0e-12f ? juce::jlimit (-1.0f, 1.0f, (r - l) / sum) : 0.0f;
            const float energy = toDisplayEnergy (l, r);
            balanceLf[(size_t) bin] += smooth * (balance - balanceLf[(size_t) bin]);
            energyLf[(size_t) bin] += smooth * (energy - energyLf[(size_t) bin]);
        }
    }
}

float StereogramComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree == nullptr)
        return fallback;
    if (auto* p = valueTree->getRawParameterValue (id))
        return p->load();
    return fallback;
}

bool StereogramComponent::loadBoolParam (const char* id, bool fallback) const
{
    return loadFloatParam (id, fallback ? 1.0f : 0.0f) >= 0.5f;
}

float StereogramComponent::freqForDisplayT (float t01) const noexcept
{
    const float nyquist = (float) (sampleRate * 0.5);
    const float maxHz = juce::jmin (kMaxDisplayHz, nyquist * 0.999f);
    const float minHz = kMinDisplayHz;
    t01 = juce::jlimit (0.0f, 1.0f, t01);
    return minHz * std::pow (maxHz / minHz, t01);
}

float StereogramComponent::displayTForFreq (float hz) const noexcept
{
    const float nyquist = (float) (sampleRate * 0.5);
    const float maxHz = juce::jmin (kMaxDisplayHz, nyquist * 0.999f);
    const float minHz = kMinDisplayHz;
    hz = juce::jlimit (minHz, maxHz, hz);
    return std::log (hz / minHz) / std::log (maxHz / minHz);
}

void StereogramComponent::sampleBalanceEnergyAtFreq (float hz, float& balanceOut, float& energyOut) const noexcept
{
    auto samplePair = [&] (const std::vector<float>& bal,
                           const std::vector<float>& eng,
                           int fftSize,
                           float& bOut, float& eOut)
    {
        const int n = (int) bal.size();
        if (n < 2 || fftSize <= 0)
        {
            bOut = 0.0f;
            eOut = 0.0f;
            return;
        }

        // bins cover DC..Nyquist (size/2 + 1); keep clear of DC.
        const float maxBin = (float) juce::jmax (1, n - 1);
        float binF = hz * (float) fftSize / (float) juce::jmax (1.0, sampleRate);
        binF = juce::jlimit (1.0f, maxBin, binF);
        const int i0 = (int) std::floor (binF);
        const int i1 = juce::jmin (n - 1, i0 + 1);
        const float u = binF - (float) i0;
        bOut = bal[(size_t) i0] + (bal[(size_t) i1] - bal[(size_t) i0]) * u;
        eOut = eng[(size_t) i0] + (eng[(size_t) i1] - eng[(size_t) i0]) * u;
    };

    float balMain = 0.0f, engMain = 0.0f;
    float balLf = 0.0f, engLf = 0.0f;
    samplePair (balanceByBin, energyByBin, kMainSize, balMain, engMain);

    if (lfSize > 0 && ! balanceLf.empty())
        samplePair (balanceLf, energyLf, lfSize, balLf, engLf);
    else
    {
        balanceOut = balMain;
        energyOut = engMain;
        return;
    }

    // Soft crossover around 350 Hz (spectrogram Enhanced LF band edges).
    const float lfLo = kCrossoverHz * 0.75f;
    const float lfHi = kCrossoverHz * 1.25f;

    if (hz <= lfLo)
    {
        balanceOut = balLf;
        energyOut = engLf;
    }
    else if (hz < lfHi)
    {
        const float t = (hz - lfLo) / juce::jmax (1.0e-3f, lfHi - lfLo);
        balanceOut = balLf * (1.0f - t) + balMain * t;
        energyOut = engLf * (1.0f - t) + engMain * t;
    }
    else
    {
        balanceOut = balMain;
        energyOut = engMain;
    }
}

juce::Rectangle<int> StereogramComponent::getPlotBounds() const noexcept
{
    // Top chrome: +1 (L) / +1 (R). Left chrome: frequency scale.
    constexpr int kTopChrome = 18;
    constexpr int kFreqScaleW = 30;
    return getLocalBounds().reduced (4, 4)
        .withTrimmedTop (kTopChrome)
        .withTrimmedLeft (kFreqScaleW);
}

void StereogramComponent::ensureTrailImage (int width, int height)
{
    const int w = juce::jmax (8, width);
    const int h = juce::jmax (8, height);
    if (trailImage.isValid()
        && trailImage.getWidth() == w
        && trailImage.getHeight() == h)
        return;

    trailImage = juce::Image (juce::Image::ARGB, w, h, true);
}

void StereogramComponent::buildGlowPath (juce::Path& outPath) const
{
    outPath.clear();
    auto plot = getPlotBounds().toFloat();
    if (plot.isEmpty())
        return;

    if ((int) balanceByBin.size() < 2)
        return;

    const float density = juce::jlimit (1.0f, 100.0f, loadFloatParam ("STEREOGRAM_DOT_DENSITY_ID", 100.0f));
    const int rowStep = juce::jmax (1, juce::roundToInt (juce::jmap (density, 1.0f, 100.0f, 4.0f, 1.0f)));
    const float gateRatio = juce::jmap (density, 1.0f, 100.0f, 0.12f, 0.02f);

    const float cx = plot.getCentreX();
    const float maxHalf = plot.getWidth() * 0.48f;
    const float h = juce::jmax (1.0f, plot.getHeight());
    const int rows = juce::jmax (1, (int) std::lround (h));

    float peakEnergy = 1.0e-6f;
    for (int row = 0; row < rows; row += rowStep)
    {
        float bal = 0.0f, energy = 0.0f;
        sampleBalanceEnergyAtFreq (freqForDisplayT (rows <= 1 ? 0.0f : 1.0f - (float) row / (float) (rows - 1)),
                                   bal, energy);
        peakEnergy = juce::jmax (peakEnergy, energy);
    }
    const float energyGate = peakEnergy * gateRatio;

    bool started = false;
    for (int row = 0; row < rows; row += rowStep)
    {
        const float tFreq = rows <= 1 ? 0.0f : 1.0f - (float) row / (float) (rows - 1);
        const float freq = freqForDisplayT (tFreq);
        float bal = 0.0f, energy = 0.0f;
        sampleBalanceEnergyAtFreq (freq, bal, energy);
        if (energy < energyGate)
            continue;

        const float y = plot.getY() + (float) row;
        const float x = cx + bal * maxHalf;
        if (! started)
        {
            outPath.startNewSubPath (x, y);
            started = true;
        }
        else
        {
            outPath.lineTo (x, y);
        }
    }
}

void StereogramComponent::fadeAndPlotTrail()
{
    auto plot = getPlotBounds();
    if (plot.isEmpty())
        return;

    ensureTrailImage (plot.getWidth(), plot.getHeight());
    if (! trailImage.isValid())
        return;

    const float fadeMs = juce::jlimit (50.0f, 2000.0f, loadFloatParam ("STEREOGRAM_FADE_MS_ID", 400.0f));
    const float frameMs = 1000.0f / (float) kTimerHz;
    const float fadeAlpha = juce::jlimit (0.02f, 0.40f, frameMs / fadeMs);

    juce::Graphics tg (trailImage);
    tg.setColour (juce::Colours::black.withAlpha (fadeAlpha));
    tg.fillAll();
    tg.setOpacity (1.0f);

    const float dotSize = juce::jmax (1.0f, loadFloatParam ("STEREOGRAM_DOT_SIZE_ID", 3.0f));
    const float density = juce::jlimit (1.0f, 100.0f, loadFloatParam ("STEREOGRAM_DOT_DENSITY_ID", 100.0f));
    const int rowStep = juce::jmax (1, juce::roundToInt (juce::jmap (density, 1.0f, 100.0f, 4.0f, 1.0f)));
    // Gate is relative to the frame peak so HF isn't wiped on typical music rolloff.
    const float gateRatio = juce::jmap (density, 1.0f, 100.0f, 0.12f, 0.02f);

    const bool useRamp = loadBoolParam ("STEREOGRAM_USE_RAMP_ID", true) && hasCustomRamp
                         && colourRamp.isUsable();
    const auto solid = themeColors != nullptr ? themeColors->sharedColors.spectrumLine
                                              : juce::Colour::fromRGB (120, 200, 255);

    const juce::ScopedLock sl (lock);
    if ((int) balanceByBin.size() < 2)
        return;

    const float imgW = (float) trailImage.getWidth();
    const float imgH = (float) trailImage.getHeight();
    const float cx = imgW * 0.5f;
    const float maxHalf = imgW * 0.48f;
    const int rows = juce::jmax (1, (int) std::lround (imgH));

    float peakEnergy = 1.0e-6f;
    for (int row = 0; row < rows; row += rowStep)
    {
        const float tFreq = rows <= 1 ? 0.0f : 1.0f - (float) row / (float) (rows - 1);
        float bal = 0.0f, energy = 0.0f;
        sampleBalanceEnergyAtFreq (freqForDisplayT (tFreq), bal, energy);
        peakEnergy = juce::jmax (peakEnergy, energy);
    }
    const float energyGate = peakEnergy * gateRatio;

    for (int row = 0; row < rows; row += rowStep)
    {
        const float tFreq = rows <= 1 ? 0.0f : 1.0f - (float) row / (float) (rows - 1);
        const float freq = freqForDisplayT (tFreq);
        float bal = 0.0f, energy = 0.0f;
        sampleBalanceEnergyAtFreq (freq, bal, energy);
        if (energy < energyGate)
            continue;

        const float y = (float) row;
        const float x = cx + bal * maxHalf;
        const float alpha = juce::jlimit (0.2f, 1.0f, energy * 4.0f);

        if (useRamp)
            tg.setColour (colourRamp.colourForDriver (tFreq).withMultipliedAlpha (alpha));
        else
            tg.setColour (solid.withMultipliedAlpha (alpha));

        tg.fillEllipse (x - dotSize * 0.5f, y - dotSize * 0.5f, dotSize, dotSize);
    }

    buildGlowPath (lastGlowPath);
}

void StereogramComponent::timerCallback()
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    processFromRing();
    fadeAndPlotTrail();
    repaint();
}

void StereogramComponent::paintFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> plot) const
{
    if (plot.getHeight() < 24.0f)
        return;

    const auto& theme = themeColors != nullptr ? themeColors->sharedColors
                                               : SharedColors {};

    static constexpr float kMajorHz[] = {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };
    static constexpr float kMinorHz[] = {
        30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f,
        150.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
        1500.0f, 3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 15000.0f
    };

    const float nyquist = (float) (sampleRate * 0.5);
    const float maxHz = juce::jmin (kMaxDisplayHz, nyquist * 0.999f);
    const auto full = getLocalBounds().reduced (4, 4).toFloat();
    const float labelX = full.getX();
    const float labelW = plot.getX() - full.getX() - 2.0f;

    auto yForHz = [&] (float hz) -> float
    {
        const float t = displayTForFreq (hz); // 0 = low (bottom), 1 = high (top)
        return plot.getY() + (1.0f - t) * plot.getHeight();
    };

    auto drawLine = [&] (float hz, bool major)
    {
        if (hz < kMinDisplayHz || hz > maxHz)
            return;

        const float y = yForHz (hz);
        g.setColour (theme.graphGrid.withAlpha (major ? 0.42f : 0.18f));
        g.drawHorizontalLine (juce::roundToInt (y), plot.getX(), plot.getRight());

        if (major && labelW > 12.0f)
        {
            g.setColour (theme.graphAxisText.withAlpha (0.85f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (formatGridHz (hz),
                        juce::Rectangle<float> (labelX, y - 7.0f, labelW, 14.0f),
                        juce::Justification::centredRight,
                        false);
        }
    };

    for (float hz : kMinorHz)
        drawLine (hz, false);
    for (float hz : kMajorHz)
        drawLine (hz, true);
}

void StereogramComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& theme = themeColors != nullptr ? themeColors->sharedColors
                                               : SharedColors {};
    const auto bg = theme.oscBackground;
    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 3.0f);

    auto plot = getPlotBounds();
    const auto plotF = plot.toFloat();
    // +L / +R sit in the top chrome above the plot (module title from overlay).
    auto topChrome = juce::Rectangle<int> (plot.getX(), getLocalBounds().getY() + 4,
                                           plot.getWidth(), 18);

    // X extremes are ±1 balance: full Left / full Right.
    g.setFont (juce::Font (juce::FontOptions (10.0f).withStyle ("Bold")));
    g.setColour (theme.graphAxisText.withAlpha (0.75f));
    g.drawText ("+1 (L)", topChrome.removeFromLeft (topChrome.getWidth() / 2),
                juce::Justification::centredLeft, false);
    g.drawText ("+1 (R)", topChrome, juce::Justification::centredRight, false);

    const bool glowEnabled = loadBoolParam ("STEREOGRAM_GLOW_ENABLE_ID", true);
    const float glowOpacity = juce::jlimit (0.0f, 1.0f, loadFloatParam ("STEREOGRAM_GLOW_OPACITY_ID", 75.0f) * 0.01f);
    const float glowRadius = juce::jmax (0.0f, loadFloatParam ("STEREOGRAM_GLOW_RADIUS_ID", 8.0f));
    const float glowSpread = juce::jmax (0.0f, loadFloatParam ("STEREOGRAM_GLOW_SPREAD_ID", 1.5f));
    const float dotSize = juce::jmax (1.0f, loadFloatParam ("STEREOGRAM_DOT_SIZE_ID", 3.0f));

    if (glowEnabled && SharedResources::glowShadowEffectsEnabled()
        && glowOpacity > 0.05f && glowRadius > 0.5f && ! lastGlowPath.isEmpty())
    {
        const auto ink = theme.spectrumLine;
        const auto bloom = ink.withAlpha (glowOpacity * 0.45f);
        const auto core = ink.brighter (0.15f).withAlpha (glowOpacity * 0.75f);
        const juce::PathStrokeType glowStroke (juce::jmax (1.0f, dotSize + 0.5f),
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded);
        juce::Path glowShape;
        glowStroke.createStrokedPath (glowShape, lastGlowPath);

        plotGlow.setRadius ((double) glowRadius, 0);
        plotGlow.setSpread ((double) glowSpread, 0);
        plotGlow.setOffset (0, 0, 0);
        plotGlow.setColor (bloom, 0);

        plotGlow.setRadius (juce::jmax (2.0, (double) glowRadius * 0.35), 1);
        plotGlow.setSpread (0.0, 1);
        plotGlow.setOffset (0, 0, 1);
        plotGlow.setColor (core, 1);

        plotGlow.render (g, glowShape, true);
    }

    if (trailImage.isValid() && ! plot.isEmpty())
        g.drawImage (trailImage, plotF);

    paintFrequencyGrid (g, plotF);

    if (! plotF.isEmpty())
    {
        const float cx = plotF.getCentreX();
        g.setColour (juce::Colours::whitesmoke.withAlpha (0.45f));
        g.drawVerticalLine ((int) std::lround (cx), plotF.getY(), plotF.getBottom());
    }
}

void StereogramComponent::resized()
{
    auto plot = getPlotBounds();
    if (! plot.isEmpty())
        ensureTrailImage (plot.getWidth(), plot.getHeight());
}

void StereogramComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onShowContextMenu != nullptr)
        onShowContextMenu();
}
