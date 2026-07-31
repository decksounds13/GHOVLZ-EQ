#include "SpectrogramComponent.h"

namespace
{
    float freqForNormY (float yNorm, double sampleRate, bool logFreq)
    {
        const float nyquist = (float) (sampleRate * 0.5);
        const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);
        const float minHz = SpectrogramComponent::kMinDisplayHz;
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, yNorm);

        if (logFreq)
            return minHz * std::pow (maxHz / minHz, t);

        return minHz + t * (maxHz - minHz);
    }

    /** Inverse of freqForNormY — yNorm 0 = top (high Hz), 1 = bottom (low Hz). */
    float normYForFreq (float hz, double sampleRate, bool logFreq)
    {
        const float nyquist = (float) (sampleRate * 0.5);
        const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);
        const float minHz = SpectrogramComponent::kMinDisplayHz;
        hz = juce::jlimit (minHz, maxHz, hz);

        float t = 0.0f;
        if (logFreq)
            t = std::log (hz / minHz) / std::log (maxHz / minHz);
        else
            t = (hz - minHz) / juce::jmax (1.0f, maxHz - minHz);

        return 1.0f - juce::jlimit (0.0f, 1.0f, t);
    }

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

SpectrogramComponent::SpectrogramComponent()
{
    setOpaque (false);
    setVisible (false);
    resolveDisplaySize (internalW, internalH);
    binForRow.assign ((size_t) internalH, 0.0f);
    ensureScratchImage();
    rebuildColourLut();
}

SpectrogramComponent::~SpectrogramComponent()
{
    stopTimer();
}

juce::StringArray SpectrogramComponent::getColourSchemeNames()
{
    return { "Classic", "Inferno", "Magma", "Viridis", "Ice", "Greyscale", "Heat" };
}

juce::Colour SpectrogramComponent::colourForScheme (ColourScheme scheme, float t) noexcept
{
    auto sampleStops = [] (const juce::Colour* stops, int numStops, float tt) noexcept -> juce::Colour
    {
        if (numStops <= 1)
            return stops[0];

        tt = juce::jlimit (0.0f, 1.0f, tt);
        const float scaled = tt * (float) (numStops - 1);
        const int i0 = (int) scaled;
        const int i1 = juce::jmin (numStops - 1, i0 + 1);
        const float frac = scaled - (float) i0;
        const auto a = stops[i0], b = stops[i1];
        return juce::Colour::fromFloatRGBA (
            a.getFloatRed()   + (b.getFloatRed()   - a.getFloatRed())   * frac,
            a.getFloatGreen() + (b.getFloatGreen() - a.getFloatGreen()) * frac,
            a.getFloatBlue()  + (b.getFloatBlue()  - a.getFloatBlue())  * frac,
            1.0f);
    };

    switch (scheme)
    {
        case ColourScheme::inferno:
        {
            const juce::Colour stops[] = {
                juce::Colour::fromRGB (0, 0, 4),
                juce::Colour::fromRGB (40, 11, 84),
                juce::Colour::fromRGB (101, 21, 110),
                juce::Colour::fromRGB (159, 42, 99),
                juce::Colour::fromRGB (212, 72, 66),
                juce::Colour::fromRGB (245, 125, 21),
                juce::Colour::fromRGB (252, 255, 164)
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
        }
        case ColourScheme::magma:
        {
            const juce::Colour stops[] = {
                juce::Colour::fromRGB (0, 0, 4),
                juce::Colour::fromRGB (28, 16, 68),
                juce::Colour::fromRGB (79, 18, 123),
                juce::Colour::fromRGB (129, 37, 129),
                juce::Colour::fromRGB (181, 54, 122),
                juce::Colour::fromRGB (229, 92, 110),
                juce::Colour::fromRGB (252, 253, 191)
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
        }
        case ColourScheme::viridis:
        {
            const juce::Colour stops[] = {
                juce::Colour::fromRGB (68, 1, 84),
                juce::Colour::fromRGB (59, 82, 139),
                juce::Colour::fromRGB (33, 145, 140),
                juce::Colour::fromRGB (94, 201, 98),
                juce::Colour::fromRGB (253, 231, 37)
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
        }
        case ColourScheme::ice:
        {
            const juce::Colour stops[] = {
                juce::Colour::fromRGB (0, 0, 0),
                juce::Colour::fromRGB (0, 20, 80),
                juce::Colour::fromRGB (0, 80, 180),
                juce::Colour::fromRGB (40, 180, 255),
                juce::Colour::fromRGB (220, 250, 255)
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
        }
        case ColourScheme::greyscale:
        {
            const juce::Colour stops[] = { juce::Colours::black, juce::Colours::white };
            return sampleStops (stops, 2, t);
        }
        case ColourScheme::heat:
        {
            const juce::Colour stops[] = {
                juce::Colours::black,
                juce::Colour::fromRGB (120, 0, 0),
                juce::Colour::fromRGB (220, 40, 0),
                juce::Colour::fromRGB (255, 180, 0),
                juce::Colours::white
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
        }
        case ColourScheme::classic:
        default:
        {
            const juce::Colour stops[] = {
                juce::Colours::black,
                juce::Colour::fromRGB (20, 0, 80),
                juce::Colour::fromRGB (0, 40, 200),
                juce::Colour::fromRGB (0, 200, 200),
                juce::Colour::fromRGB (40, 220, 40),
                juce::Colour::fromRGB (240, 240, 0),
                juce::Colours::white
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
        }
    }
}

juce::StringArray SpectrogramComponent::getFftSizeNames()
{
    return { "2048", "4096", "8192", "16384" };
}

juce::StringArray SpectrogramComponent::getDisplayResNames()
{
    return { "Draft", "Normal", "High", "Ultra" };
}

void SpectrogramComponent::resolveDisplaySize (int& outW, int& outH) const
{
    // Pixel density of the scrolling image (not FFT size). Upscaled with filtering in paint.
    switch (loadChoiceIndex ("SPEC_DISPLAY_RES_ID", 2)) // default High
    {
        case 0:  outW = 720;  outH = 360;  break; // Draft
        case 1:  outW = 960;  outH = 540;  break; // Normal
        case 3:  outW = 1920; outH = 1080; break; // Ultra
        case 2:
        default: outW = 1440; outH = 810;  break; // High
    }
}

const SharedColors& SpectrogramComponent::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void SpectrogramComponent::prepare (double sampleRate)
{
    const double sr = sampleRate > 0.0 ? sampleRate : 48000.0;
    sampleRateHz.store (sr, std::memory_order_relaxed);

    const int newCap = juce::jmax (16384, (int) std::ceil (sr * (double) kMaxBufferSeconds));
    ringL.assign ((size_t) newCap, 0.0f);
    ringR.assign ((size_t) newCap, 0.0f);
    writePos.store (0, std::memory_order_relaxed);
    capacity.store (newCap, std::memory_order_release);
    resetDisplay();
}

void SpectrogramComponent::ensureHistoryBuffer()
{
    const size_t n = (size_t) juce::jmax (0, internalW) * (size_t) juce::jmax (0, internalH);
    if (historyDb.size() != n)
        historyDb.assign (n, -120.0f);
}

void SpectrogramComponent::ensureScratchImage()
{
    int wantW = 0, wantH = 0;
    resolveDisplaySize (wantW, wantH);

    if (scrollImage.isValid()
        && scrollImage.getWidth() == wantW
        && scrollImage.getHeight() == wantH
        && internalW == wantW
        && internalH == wantH
        && historyDb.size() == (size_t) wantW * (size_t) wantH)
        return;

    const bool sizeChanged = (internalW != wantW || internalH != wantH);
    internalW = wantW;
    internalH = wantH;
    binForRow.assign ((size_t) internalH, 0.0f);
    scrollImage = juce::Image (juce::Image::ARGB, internalW, internalH, true);
    scrollImage.clear (scrollImage.getBounds(), juce::Colours::black);
    historyDb.assign ((size_t) internalW * (size_t) internalH, -120.0f);
    if (sizeChanged)
    {
        srCachedForBins = 0.0;
        fftSizeCachedForBins = 0;
        displayResCached = loadChoiceIndex ("SPEC_DISPLAY_RES_ID", 2);
    }
}

void SpectrogramComponent::resetDisplay()
{
    ringReadPos = writePos.load (std::memory_order_acquire);
    ensureScratchImage();
    if (scrollImage.isValid())
        scrollImage.clear (scrollImage.getBounds(), juce::Colours::black);
    historyDb.assign ((size_t) internalW * (size_t) internalH, -120.0f);
    havePrevPhase = false;
    lastLookFingerprint = lookFingerprint();
    imageDirty = true;
    screenSoftDirty = true;
    srCachedForBins = 0.0;
    fftSizeCachedForBins = 0;
}

void SpectrogramComponent::setEnabled (bool shouldEnable) noexcept
{
    enabled.store (shouldEnable, std::memory_order_relaxed);
    setVisible (shouldEnable);

    if (shouldEnable)
    {
        resetDisplay();
        startTimerHz (60);
    }
    else
    {
        stopTimer();
    }
}

void SpectrogramComponent::setExpanded (bool shouldExpand) noexcept
{
    if (expanded == shouldExpand)
        return;

    expanded = shouldExpand;
    setInterceptsMouseClicks (! expanded, ! expanded);
    // Do not wipe history — pane / fullscreen is only a paint stretch of the same image.
    repaint();
}

float SpectrogramComponent::getSpeed() const
{
    return juce::jlimit (1.0f, 100.0f, loadFloatParam ("SPEC_SPEED_ID", 70.0f));
}

void SpectrogramComponent::speedUp()
{
    if (valueTree == nullptr)
        return;

    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (valueTree->getParameter ("SPEC_SPEED_ID")))
    {
        const float next = juce::jmin (100.0f, p->get() + 12.0f);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (next));
        p->endChangeGesture();
    }
}

void SpectrogramComponent::speedDown()
{
    if (valueTree == nullptr)
        return;

    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (valueTree->getParameter ("SPEC_SPEED_ID")))
    {
        const float next = juce::jmax (1.0f, p->get() - 12.0f);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (next));
        p->endChangeGesture();
    }
}

float SpectrogramComponent::loadFloatParam (const char* id, float fallback) const
{
    if (valueTree == nullptr)
        return fallback;

    if (auto* v = valueTree->getRawParameterValue (id))
        return v->load();

    return fallback;
}

int SpectrogramComponent::loadChoiceIndex (const char* id, int fallback) const
{
    if (valueTree == nullptr)
        return fallback;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (valueTree->getParameter (id)))
        return choice->getIndex();

    return fallback;
}

bool SpectrogramComponent::loadBoolParam (const char* id, bool fallback) const
{
    if (valueTree == nullptr)
        return fallback;

    if (auto* v = valueTree->getRawParameterValue (id))
        return v->load() >= 0.5f;

    return fallback;
}

SpectrogramComponent::ColourScheme SpectrogramComponent::currentScheme() const
{
    const int idx = juce::jlimit (0, (int) ColourScheme::numSchemes - 1,
                                  loadChoiceIndex ("SPEC_COLOUR_SCHEME_ID", (int) ColourScheme::heat));
    return static_cast<ColourScheme> (idx);
}

SpectrogramComponent::ChannelMode SpectrogramComponent::currentChannelMode() const
{
    const int idx = juce::jlimit (0, 2, loadChoiceIndex ("SPEC_CHANNEL_ID", 0));
    return static_cast<ChannelMode> (idx);
}

int SpectrogramComponent::currentFftOrder() const
{
    // 0=2048 (2^11), 1=4096, 2=8192 (default), 3=16384
    const int idx = juce::jlimit (0, 3, loadChoiceIndex ("SPEC_FFT_SIZE_ID", 2));
    return 11 + idx;
}

void SpectrogramComponent::setCustomColourRamp (const GradientRamp* ramp) noexcept
{
    customColourRamp = ramp;
    customRampRevision = ramp != nullptr ? ramp->revision : 0;
    rebuildColourLut();
    screenSoftDirty = true;
    imageDirty = true;
    repaint();
}

void SpectrogramComponent::rebuildColourLut()
{
    const auto scheme = currentScheme();
    lutScheme = scheme;

    if (customColourRamp != nullptr && customColourRamp->isUsable())
    {
        customRampRevision = customColourRamp->revision;
        customColourRamp->fillLut (colourLut.data(), kLutSize);
        return;
    }

    for (int i = 0; i < kLutSize; ++i)
    {
        const float t = (float) i / (float) (kLutSize - 1);
        colourLut[(size_t) i] = colourForScheme (scheme, t).getPixelARGB();
    }
}

void SpectrogramComponent::ensureFft (int order)
{
    if (fft != nullptr && fftOrder == order)
        return;

    fftOrder = order;
    fftSize = 1 << fftOrder;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);
    window = std::make_unique<juce::dsp::WindowingFunction<float>> (
        (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann);
    fftWork.assign ((size_t) (fftSize * 2), 0.0f);
    windowed.assign ((size_t) fftSize, 0.0f);
    columnDb.assign ((size_t) (fftSize / 2 + 1), -120.0f);
    prevPhase.assign ((size_t) (fftSize / 2 + 1), 0.0f);
    havePrevPhase = false;
    fftSizeCachedForBins = 0; // force bin map rebuild
}

void SpectrogramComponent::pushSamples (const float* left, const float* right, int numSamples) noexcept
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

void SpectrogramComponent::advanceFromRing()
{
    freeze = loadBoolParam ("SPEC_FREEZE_ID", false);
    if (freeze)
        return;

    const int cap = capacity.load (std::memory_order_acquire);
    if (cap <= 0)
        return;

    // Recreate scroll buffer if the user changed Display Resolution (wipes history once).
    const int displayRes = loadChoiceIndex ("SPEC_DISPLAY_RES_ID", 2);
    if (displayRes != displayResCached)
    {
        displayResCached = displayRes;
        ensureScratchImage();
    }

    ensureScratchImage();
    ensureFft (currentFftOrder());
    if (fft == nullptr || fftSize <= 0 || internalW <= 0 || internalH <= 0)
        return;

    const auto scheme = currentScheme();
    if (scheme != lutScheme)
        rebuildColourLut();

    const float speed = juce::jlimit (1.0f, 100.0f, loadFloatParam ("SPEC_SPEED_ID", 70.0f));
    const double sr = juce::jmax (1.0, sampleRateHz.load (std::memory_order_relaxed));
    // Speed → columns/sec (independent of FFT size). Hop keeps overlap in [fft/32 .. fft/2].
    const float colsPerSec = juce::jmap (speed, 1.0f, 100.0f, 12.0f, 96.0f);
    int hop = (int) std::round (sr / (double) colsPerSec);
    const int hopMin = juce::jmax (32, fftSize / 32);
    const int hopMax = juce::jmax (hopMin, fftSize / 2);
    hop = juce::jlimit (hopMin, hopMax, hop);

    const int write = writePos.load (std::memory_order_acquire);
    int available = write - ringReadPos;
    if (available < 0)
        available += cap;

    // Drop backlog so we stay realtime — keep one window + a couple hops, not a FFT storm.
    const int maxHold = fftSize + hop * kMaxColumnsPerTick;
    if (available > maxHold)
    {
        ringReadPos = (write - fftSize + cap) % cap;
        available = fftSize;
    }

    // Brightness / floor / ceiling applied at colourise so history can be re-mapped live.
    const float smooth = juce::jlimit (0.0f, 0.95f, loadFloatParam ("SPEC_SMOOTH_ID", 35.0f) / 100.0f);
    const auto channel = currentChannelMode();
    const int numBins = fftSize / 2 + 1;
    const bool enhanced = loadBoolParam ("SPEC_ENHANCED_FREQ_ID", false);
    const bool logFreq = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
    lastHopSamples = hop;

    if (enhanced != lastEnhancedMode)
    {
        lastEnhancedMode = enhanced;
        havePrevPhase = false;
    }

    if (enhanced && (int) prevPhase.size() != numBins)
    {
        prevPhase.assign ((size_t) numBins, 0.0f);
        havePrevPhase = false;
    }

    int columnsWritten = 0;
    while (available >= fftSize && columnsWritten < kMaxColumnsPerTick)
    {
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = (ringReadPos + i) % cap;
            const float l = ringL[(size_t) idx];
            const float rCh = ringR[(size_t) idx];
            float s = 0.0f;
            switch (channel)
            {
                case ChannelMode::left:  s = l; break;
                case ChannelMode::right: s = rCh; break;
                default:                 s = 0.5f * (l + rCh); break;
            }
            windowed[(size_t) i] = s;
        }

        window->multiplyWithWindowingTable (windowed.data(), (size_t) fftSize);
        std::fill (fftWork.begin(), fftWork.end(), 0.0f);
        std::copy (windowed.begin(), windowed.end(), fftWork.begin());

        if (enhanced)
        {
            // Complex STFT + bin-relative IF (Wave Candy / MiniMeters Sharp family).
            // Absolute phase→Hz wraps when hop is large; refine around each bin instead.
            fft->performRealOnlyForwardTransform (fftWork.data(), true);

            constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
            const float binHz = (float) sr / (float) fftSize;
            const float expectedPerBin = kTwoPi * (float) hop / (float) fftSize;

            for (int bin = 0; bin < numBins; ++bin)
            {
                const float re = fftWork[(size_t) (bin * 2)];
                const float im = fftWork[(size_t) (bin * 2 + 1)];
                const float mag = std::sqrt (re * re + im * im) / (float) fftSize;
                const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-12f), -120.0f);
                columnDb[(size_t) bin] = columnDb[(size_t) bin] * smooth + db * (1.0f - smooth);
            }

            // Classic continuum first — keeps LF texture matching normal mode.
            ensureBinForRowMap();
            columnScratch.assign ((size_t) internalH, -120.0f);
            for (int y = 0; y < internalH; ++y)
            {
                const float bf = binForRow[(size_t) y];
                const int b0 = juce::jlimit (0, numBins - 1, (int) std::floor (bf));
                const int b1 = juce::jmin (numBins - 1, b0 + 1);
                const float t = juce::jlimit (0.0f, 1.0f, bf - (float) b0);
                columnScratch[(size_t) y] = columnDb[(size_t) b0] * (1.0f - t)
                                          + columnDb[(size_t) b1] * t;
            }

            // Sharpen tones: relocate strong bin energy to the IF row (bin + phase residual).
            for (int bin = 1; bin < numBins - 1; ++bin)
            {
                const float smoothedDb = columnDb[(size_t) bin];
                if (smoothedDb < -95.0f || internalH <= 1)
                {
                    const float re = fftWork[(size_t) (bin * 2)];
                    const float im = fftWork[(size_t) (bin * 2 + 1)];
                    prevPhase[(size_t) bin] = std::atan2 (im, re);
                    continue;
                }

                // Local peak preference — broadband noise stays on the classic continuum.
                const float leftDb = columnDb[(size_t) (bin - 1)];
                const float rightDb = columnDb[(size_t) (bin + 1)];
                const bool isPeak = smoothedDb >= leftDb - 0.5f && smoothedDb >= rightDb - 0.5f
                                    && smoothedDb > juce::jmax (leftDb, rightDb) - 6.0f;

                const float re = fftWork[(size_t) (bin * 2)];
                const float im = fftWork[(size_t) (bin * 2 + 1)];
                const float phase = std::atan2 (im, re);
                float ifHz = (float) bin * binHz;

                if (havePrevPhase && isPeak)
                {
                    float dPhase = phase - prevPhase[(size_t) bin];
                    while (dPhase > juce::MathConstants<float>::pi)  dPhase -= kTwoPi;
                    while (dPhase < -juce::MathConstants<float>::pi) dPhase += kTwoPi;

                    float residual = dPhase - expectedPerBin * (float) bin;
                    while (residual > juce::MathConstants<float>::pi)  residual -= kTwoPi;
                    while (residual < -juce::MathConstants<float>::pi) residual += kTwoPi;

                    // Clamp to ~±1.25 bins so hop wrapping can't fling energy across decades.
                    const float binOffset = juce::jlimit (-1.25f, 1.25f,
                                                          residual / expectedPerBin);
                    ifHz = ((float) bin + binOffset) * binHz;
                    if (! std::isfinite (ifHz) || ifHz < kMinDisplayHz * 0.25f)
                        ifHz = (float) bin * binHz;
                }

                prevPhase[(size_t) bin] = phase;

                if (! isPeak)
                    continue;

                // Boost IF row on top of the classic continuum (no carve — carving
                // left sparse "lines only" below ~150 Hz where many bins share rows).
                const float yNorm = normYForFreq (ifHz, sr, logFreq);
                const float yf = yNorm * (float) (internalH - 1);
                const int y0 = juce::jlimit (0, internalH - 1, (int) std::floor (yf));
                const int y1 = juce::jmin (internalH - 1, y0 + 1);
                const float frac = juce::jlimit (0.0f, 1.0f, yf - (float) y0);

                columnScratch[(size_t) y0] = juce::jmax (columnScratch[(size_t) y0],
                                                         smoothedDb - 1.5f * frac);
                columnScratch[(size_t) y1] = juce::jmax (columnScratch[(size_t) y1],
                                                         smoothedDb - 1.5f * (1.0f - frac));
            }

            // Keep DC/Nyquist phase state coherent.
            for (int bin : { 0, numBins - 1 })
            {
                if (bin < 0 || bin >= numBins)
                    continue;
                const float re = fftWork[(size_t) (bin * 2)];
                const float im = fftWork[(size_t) (bin * 2 + 1)];
                prevPhase[(size_t) bin] = std::atan2 (im, re);
            }

            havePrevPhase = true;
            // Mild soften only — enough to hide row gaps without undoing ridges.
            const float soften = juce::jlimit (0.0f, 0.55f, loadFloatParam ("SPEC_SOFTEN_ID", 55.0f) / 100.0f * 0.55f);
            if (soften > 0.001f && internalH > 2)
            {
                columnSoftTmp = columnScratch;
                constexpr float kKernel[3] = { 0.20f, 0.60f, 0.20f };
                for (int y = 0; y < internalH; ++y)
                {
                    float acc = 0.0f, wSum = 0.0f;
                    for (int k = -1; k <= 1; ++k)
                    {
                        const int yy = y + k;
                        if (yy < 0 || yy >= internalH)
                            continue;
                        const float w = kKernel[k + 1];
                        acc += columnSoftTmp[(size_t) yy] * w;
                        wSum += w;
                    }
                    columnScratch[(size_t) y] = columnSoftTmp[(size_t) y] * (1.0f - soften)
                                              + (acc / juce::jmax (1.0e-6f, wSum)) * soften;
                }
            }

            appendDisplayColumn (columnScratch.data());
        }
        else
        {
            havePrevPhase = false;
            fft->performFrequencyOnlyForwardTransform (fftWork.data());

            for (int bin = 0; bin < numBins; ++bin)
            {
                const float mag = fftWork[(size_t) bin] / (float) fftSize;
                const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-12f), -120.0f);
                columnDb[(size_t) bin] = columnDb[(size_t) bin] * smooth + db * (1.0f - smooth);
            }

            appendColumn (columnDb.data(), numBins);
        }

        ringReadPos = (ringReadPos + hop) % cap;
        available -= hop;
        ++columnsWritten;
    }
}

uint32_t SpectrogramComponent::lookFingerprint() const
{
    const auto scheme = (uint32_t) currentScheme();
    const uint32_t bright = (uint32_t) juce::roundToInt (loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f) * 10.0f);
    const uint32_t minDb = (uint32_t) juce::roundToInt (loadFloatParam ("SPEC_MIN_DB_ID", -90.0f) * 10.0f);
    const uint32_t maxDb = (uint32_t) juce::roundToInt (loadFloatParam ("SPEC_MAX_DB_ID", -6.0f) * 10.0f);
    const uint32_t soften = (uint32_t) juce::roundToInt (loadFloatParam ("SPEC_SOFTEN_ID", 55.0f) * 10.0f);
    const uint32_t logF = loadBoolParam ("SPEC_LOG_FREQ_ID", true) ? 1u : 0u;
    const uint32_t rampRev = (customColourRamp != nullptr && customColourRamp->isUsable())
                                 ? customColourRamp->revision : 0u;
    const uint32_t rampOn = (customColourRamp != nullptr && customColourRamp->isUsable()) ? 1u : 0u;
    const uint32_t rampMap = (customColourRamp != nullptr)
                                 ? (uint32_t) customColourRamp->mapMode : 0u;
    return scheme ^ (bright << 3) ^ (minDb << 7) ^ (maxDb << 13) ^ (soften << 19) ^ (logF << 29)
           ^ (rampOn << 1) ^ (rampMap << 22) ^ (rampRev * 2654435761u);
}

void SpectrogramComponent::colouriseColumnIntoImage (int x, const float* dbRows,
                                                    float brightness, float minDb, float maxDb)
{
    if (! scrollImage.isValid() || dbRows == nullptr || x < 0 || x >= internalW)
        return;

    const float dbGain = juce::Decibels::gainToDecibels (juce::jmax (brightness, 1.0e-3f), -100.0f);
    const float denom = juce::jmax (1.0f, maxDb - minDb);

    juce::Image::BitmapData pixels (scrollImage, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < internalH; ++y)
    {
        const float dbAdj = dbRows[y] + dbGain;
        const float norm = juce::jlimit (0.0f, 1.0f, (dbAdj - minDb) / denom);
        const int lutIdx = juce::jlimit (0, kLutSize - 1,
                                         (int) std::lround (norm * (float) (kLutSize - 1)));
        pixels.setPixelColour (x, y, juce::Colour (colourLut[(size_t) lutIdx]));
    }
}

void SpectrogramComponent::rerenderScrollFromHistory()
{
    ensureScratchImage();
    ensureHistoryBuffer();
    if (! scrollImage.isValid() || historyDb.empty())
        return;

    rebuildColourLut();

    const float brightness = juce::jlimit (10.0f, 200.0f, loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f)) / 100.0f;
    const float minDb = loadFloatParam ("SPEC_MIN_DB_ID", -90.0f);
    const float maxDb = juce::jmax (minDb + 6.0f, loadFloatParam ("SPEC_MAX_DB_ID", -6.0f));

    for (int x = 0; x < internalW; ++x)
        colouriseColumnIntoImage (x, historyDb.data() + (size_t) x * (size_t) internalH,
                                  brightness, minDb, maxDb);

    lastLookFingerprint = lookFingerprint();
    imageDirty = true;
    screenSoftDirty = true;
}

void SpectrogramComponent::ensureBinForRowMap()
{
    const double sr = sampleRateHz.load (std::memory_order_relaxed);
    const bool logFreq = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
    const int numBinsSafe = juce::jmax (1, fftSize / 2 + 1);

    if (logFreq == logFreqCached && fftSize == fftSizeCachedForBins && sr == srCachedForBins
        && (int) binForRow.size() == internalH)
        return;

    // Axis / FFT remap invalidates display-row history — wipe then rebuild map.
    const bool axisChanged = (logFreq != logFreqCached) && fftSizeCachedForBins != 0;
    logFreqCached = logFreq;
    fftSizeCachedForBins = fftSize;
    srCachedForBins = sr;
    binForRow.resize ((size_t) internalH);

    for (int y = 0; y < internalH; ++y)
    {
        const float yNorm = internalH > 1 ? (float) y / (float) (internalH - 1) : 0.0f;
        const float freq = freqForNormY (yNorm, sr, logFreq);
        const float binF = (float) (freq * (double) fftSize / sr);
        binForRow[(size_t) y] = juce::jlimit (0.0f, (float) (numBinsSafe - 1), binF);
    }

    if (axisChanged)
    {
        historyDb.assign ((size_t) internalW * (size_t) internalH, -120.0f);
        if (scrollImage.isValid())
            scrollImage.clear (scrollImage.getBounds(), juce::Colours::black);
    }
}

void SpectrogramComponent::appendColumn (const float* magnitudesDb, int numBins)
{
    juce::ignoreUnused (numBins);
    if (! scrollImage.isValid() || magnitudesDb == nullptr)
        return;

    ensureHistoryBuffer();
    ensureBinForRowMap();

    const int numBinsSafe = fftSize / 2 + 1;

    // Interpolate bins → rows, then vertical Gaussian before storing / colourising.
    columnScratch.resize ((size_t) internalH);
    for (int y = 0; y < internalH; ++y)
    {
        const float binF = binForRow[(size_t) y];
        const int b0 = (int) binF;
        const int b1 = juce::jmin (numBinsSafe - 1, b0 + 1);
        const float frac = binF - (float) b0;
        columnScratch[(size_t) y] = magnitudesDb[(size_t) b0] * (1.0f - frac)
                                  + magnitudesDb[(size_t) b1] * frac;
    }
    softenColumnVertical (columnScratch, internalH);
    appendDisplayColumn (columnScratch.data());
}

void SpectrogramComponent::appendDisplayColumn (const float* displayDbRows)
{
    if (! scrollImage.isValid() || displayDbRows == nullptr || internalH <= 0)
        return;

    ensureHistoryBuffer();

    // Scroll history + pixels one column left, write new column on the right.
    if (internalW > 1 && ! historyDb.empty())
    {
        std::memmove (historyDb.data(),
                      historyDb.data() + (size_t) internalH,
                      (size_t) (internalW - 1) * (size_t) internalH * sizeof (float));
    }

    {
        juce::Image::BitmapData pixels (scrollImage, juce::Image::BitmapData::readWrite);
        const int pixelStride = pixels.pixelStride;

        for (int y = 0; y < internalH; ++y)
        {
            auto* row = pixels.getLinePointer (y);
            if (pixelStride == 4)
                std::memmove (row, row + 4, (size_t) (internalW - 1) * 4u);
            else
                for (int x = 0; x < internalW - 1; ++x)
                    pixels.setPixelColour (x, y, pixels.getPixelColour (x + 1, y));
        }
    }

    const int x = internalW - 1;
    std::copy (displayDbRows, displayDbRows + internalH,
               historyDb.begin() + (size_t) x * (size_t) internalH);

    const float brightness = juce::jlimit (10.0f, 200.0f, loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f)) / 100.0f;
    const float minDb = loadFloatParam ("SPEC_MIN_DB_ID", -90.0f);
    const float maxDb = juce::jmax (minDb + 6.0f, loadFloatParam ("SPEC_MAX_DB_ID", -6.0f));

    if (lutScheme != currentScheme())
        rebuildColourLut();

    colouriseColumnIntoImage (x, displayDbRows, brightness, minDb, maxDb);
    lastLookFingerprint = lookFingerprint();

    imageDirty = true;
    screenSoftDirty = true;
}

void SpectrogramComponent::softenColumnVertical (std::vector<float>& column, int numRows)
{
    if (numRows < 3 || column.size() < (size_t) numRows)
        return;

    // 5-tap approx Gaussian [1,4,6,4,1] / 16 along frequency (display rows).
    columnSoftTmp.resize ((size_t) numRows);
    for (int y = 0; y < numRows; ++y)
    {
        const float a = column[(size_t) juce::jmax (0, y - 2)];
        const float b = column[(size_t) juce::jmax (0, y - 1)];
        const float c = column[(size_t) y];
        const float d = column[(size_t) juce::jmin (numRows - 1, y + 1)];
        const float e = column[(size_t) juce::jmin (numRows - 1, y + 2)];
        columnSoftTmp[(size_t) y] = (a + 4.0f * b + 6.0f * c + 4.0f * d + e) * (1.0f / 16.0f);
    }
    column.swap (columnSoftTmp);
}

void SpectrogramComponent::timerCallback()
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    advanceFromRing();

    // Look params (scheme / brightness / floor / ceiling / soften) recolour the whole strip.
    const auto fp = lookFingerprint();
    if (fp != lastLookFingerprint)
    {
        const bool logNow = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
        if (logNow != logFreqCached)
        {
            // Axis change: history rows are wrong — clear; new columns refill.
            historyDb.assign ((size_t) internalW * (size_t) internalH, -120.0f);
            if (scrollImage.isValid())
                scrollImage.clear (scrollImage.getBounds(), juce::Colours::black);
            logFreqCached = logNow;
            srCachedForBins = 0.0;
        }
        rebuildColourLut();
        rerenderScrollFromHistory();
    }

    if (imageDirty)
    {
        imageDirty = false;
        repaint();
    }
}

void SpectrogramComponent::resized()
{
    // Internal scroll buffer is resolution-setting sized; screen soften tracks component.
    screenSoftDirty = true;
}

void SpectrogramComponent::rebuildScreenSoftened()
{
    if (! scrollImage.isValid())
        return;

    auto area = getLocalBounds();
    if (area.isEmpty())
        return;

    if (! screenImage.isValid()
        || screenImage.getWidth() != area.getWidth()
        || screenImage.getHeight() != area.getHeight())
    {
        screenImage = juce::Image (juce::Image::ARGB, area.getWidth(), area.getHeight(), true);
    }

    screenImage.clear (screenImage.getBounds(), juce::Colours::transparentBlack);

    {
        juce::Graphics ig (screenImage);
        ig.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        ig.drawImage (scrollImage, screenImage.getBounds().toFloat());
    }

    // Soften 0..100 → blur radius 0..5 (screen pixels). 0 skips Melatonin path.
    const float soften = juce::jlimit (0.0f, 100.0f, loadFloatParam ("SPEC_SOFTEN_ID", 55.0f));
    const int radius = juce::roundToInt (soften * 0.05f); // 0..5

    if (radius > 0)
    {
        screenBlur.setRadius ((size_t) radius);
        screenBlur.update (screenImage);
    }

    screenSoftDirty = false;
}

void SpectrogramComponent::paintFrequencyGrid (juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    if (bounds.getHeight() < 40.0f)
        return;

    const double sr = sampleRateHz.load (std::memory_order_relaxed);
    const bool logFreq = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
    const auto& theme = colors();

    static constexpr float kMajorHz[] = {
        20.0f, 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f, 20000.0f
    };
    static constexpr float kMinorHz[] = {
        30.0f, 40.0f, 60.0f, 70.0f, 80.0f, 90.0f,
        150.0f, 300.0f, 400.0f, 600.0f, 700.0f, 800.0f, 900.0f,
        1500.0f, 3000.0f, 4000.0f, 6000.0f, 7000.0f, 8000.0f, 9000.0f, 15000.0f
    };

    const float nyquist = (float) (sr * 0.5);
    const float maxHz = juce::jmin (kMaxDisplayHz, nyquist * 0.999f);

    auto drawLine = [&] (float hz, bool major)
    {
        if (hz < kMinDisplayHz || hz > maxHz)
            return;

        const float y = bounds.getY() + normYForFreq (hz, sr, logFreq) * bounds.getHeight();
        g.setColour (theme.graphGrid.withAlpha (major ? 0.42f : 0.18f));
        g.drawHorizontalLine (juce::roundToInt (y), bounds.getX(), bounds.getRight());

        if (major && bounds.getWidth() > 120.0f)
        {
            g.setColour (theme.graphAxisText.withAlpha (0.85f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (formatGridHz (hz),
                        juce::Rectangle<float> (bounds.getX() + 6.0f, y - 8.0f, 44.0f, 16.0f),
                        juce::Justification::centredLeft,
                        false);
        }
    };

    for (float hz : kMinorHz)
        drawLine (hz, false);
    for (float hz : kMajorHz)
        drawLine (hz, true);
}

void SpectrogramComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& theme = colors();

    if (! expanded)
    {
        juce::Path chrome;
        chrome.addRoundedRectangle (bounds.reduced (0.5f), 5.0f);
        g.setColour (theme.oscBackground.withAlpha (170.0f / 255.0f));
        g.fillPath (chrome);
    }

    if (scrollImage.isValid())
    {
        const auto imageBounds = bounds.reduced (expanded ? 0.0f : 1.0f);
        const float soften = juce::jlimit (0.0f, 100.0f, loadFloatParam ("SPEC_SOFTEN_ID", 55.0f));
        const int radius = juce::roundToInt (soften * 0.05f);

        if (radius <= 0)
        {
            g.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
            g.drawImage (scrollImage, imageBounds);
        }
        else
        {
            if (radius != lastScreenBlurRadius)
            {
                lastScreenBlurRadius = radius;
                screenSoftDirty = true;
            }

            // Screen-space path: upscale first, then stack blur (Gaussian-like).
            if (screenSoftDirty
                || ! screenImage.isValid()
                || screenImage.getWidth() != getWidth()
                || screenImage.getHeight() != getHeight())
            {
                rebuildScreenSoftened();
            }

            if (screenBlur.isValid())
                g.drawImage (screenBlur.render(), imageBounds);
            else if (screenImage.isValid())
                g.drawImage (screenImage, imageBounds);
        }
    }

    if (expanded)
        paintFrequencyGrid (g, bounds);
}
