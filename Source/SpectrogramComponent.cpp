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
    // Compact strip uses rounded chrome like the oscilloscope (corners must not be forced opaque).
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

juce::StringArray SpectrogramComponent::getEnhancedLfDetailNames()
{
    return { "Off", "2x", "4x" };
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

    const bool dual = loadBoolParam ("SPEC_ENHANCED_FREQ_ID", false)
                   != loadBoolParam ("SPEC_ENHANCED_FREQ_3D_ID", false);
    if (dual)
    {
        if (historyDb3D.size() != n)
            historyDb3D = historyDb; // seed so 3D isn't blank on first diverge
    }
    else if (! historyDb3D.empty())
    {
        historyDb3D.clear();
    }
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
    historyDb3D.clear();
    historyColumnSerial.store (0, std::memory_order_relaxed);
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
    historyDb3D.clear();
    historyColumnSerial.store (0, std::memory_order_relaxed);
    havePrevPhase = false;
    havePrevPhaseLf = false;
    havePrevPhaseMid = false;
    enhancedLfFrameCounter = 0;
    lastLookFingerprint = lookFingerprint();
    imageDirty = true;
    screenSoftDirty = true;
    srCachedForBins = 0.0;
    fftSizeCachedForBins = 0;
    lfFftSizeCachedForBins = 0;
    midFftSizeCachedForBins = 0;
}

void SpectrogramComponent::setEnabled (bool shouldEnable) noexcept
{
    const bool wasEnabled = enabled.load (std::memory_order_relaxed);
    enabled.store (shouldEnable, std::memory_order_relaxed);
    setVisible (shouldEnable);

    if (shouldEnable)
    {
        // Only wipe history on a true off→on transition. Layout/menu sync calls
        // setEnabled(true) repeatedly while Spec3D is up — resetting would restart
        // the waterfall every time the Settings panel moves or resizes.
        if (! wasEnabled)
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
    setOpaque (expanded); // pane fills the rect; compact uses rounded transparent corners
    setInterceptsMouseClicks (! expanded, ! expanded);
    // Compact vs pane share history, but compact needs a brightness re-colourise
    // so the strip doesn't read dimmer than the expanded overlay.
    rerenderScrollFromHistory();
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
    // Recolour the whole scroll history immediately — don't wait for new columns
    // (deposit used to stamp lastLookFingerprint and skip the full re-tint).
    rerenderScrollFromHistory();
    repaint();
}

void SpectrogramComponent::setCustomColourRamp3D (const GradientRamp* ramp) noexcept
{
    customColourRamp3D = ramp;
    customRampRevision3D = ramp != nullptr ? ramp->revision : 0;
    rebuildColourLut3D();
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

void SpectrogramComponent::rebuildColourLut3D()
{
    if (customColourRamp3D != nullptr && customColourRamp3D->isUsable())
    {
        customRampRevision3D = customColourRamp3D->revision;
        customColourRamp3D->fillLut (colourLut3D.data(), kLutSize);
        return;
    }

    // No custom 3D ramp: fall back to the same built-in scheme as 2D Spec.
    const auto scheme = currentScheme();
    for (int i = 0; i < kLutSize; ++i)
    {
        const float t = (float) i / (float) (kLutSize - 1);
        colourLut3D[(size_t) i] = colourForScheme (scheme, t).getPixelARGB();
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

void SpectrogramComponent::ensureAuxFft (int mainOrder, int orderBoost,
                                         std::unique_ptr<juce::dsp::FFT>& auxFft,
                                         std::unique_ptr<juce::dsp::WindowingFunction<float>>& auxWindow,
                                         int& auxOrder, int& auxSize,
                                         std::vector<float>& auxWork, std::vector<float>& auxWindowed,
                                         std::vector<float>& auxColumnDb, std::vector<float>& auxPrevPhase,
                                         bool& auxHavePrev, int& auxBinsCached)
{
    const int order = juce::jlimit (mainOrder, kMaxAuxFftOrder, mainOrder + orderBoost);
    if (auxFft != nullptr && auxOrder == order)
        return;

    auxOrder = order;
    auxSize = 1 << auxOrder;
    auxFft = std::make_unique<juce::dsp::FFT> (auxOrder);
    auxWindow = std::make_unique<juce::dsp::WindowingFunction<float>> (
        (size_t) auxSize, juce::dsp::WindowingFunction<float>::hann);
    auxWork.assign ((size_t) (auxSize * 2), 0.0f);
    auxWindowed.assign ((size_t) auxSize, 0.0f);
    auxColumnDb.assign ((size_t) (auxSize / 2 + 1), -120.0f);
    auxPrevPhase.assign ((size_t) (auxSize / 2 + 1), 0.0f);
    auxHavePrev = false;
    auxBinsCached = 0;
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

    const bool enhanced2D = loadBoolParam ("SPEC_ENHANCED_FREQ_ID", false);
    const bool enhanced3D = loadBoolParam ("SPEC_ENHANCED_FREQ_3D_ID", false);
    const bool needEnhanced = enhanced2D || enhanced3D;
    const bool needClassic = ! enhanced2D || ! enhanced3D;
    const bool dualHistory = (enhanced2D != enhanced3D);
    const int lfDetail = needEnhanced ? juce::jlimit (0, 2, loadChoiceIndex ("SPEC_ENHANCED_LF_DETAIL_ID", 2)) : 0;
    const int lfBoost = lfDetail;                 // 0=off, 1=2×, 2=4×
    const bool useMid = (lfDetail >= 2);          // mid 2× only with 4× LF
    const float enhancedStrength = needEnhanced
        ? juce::jlimit (0.0f, 1.0f, loadFloatParam ("SPEC_ENHANCED_STRENGTH_ID", 100.0f) / 100.0f)
        : 0.0f;
    const float crossoverHz = juce::jlimit (200.0f, 600.0f,
                                            loadFloatParam ("SPEC_ENHANCED_CROSSOVER_ID", 350.0f));

    if (needEnhanced && lfBoost > 0)
        ensureAuxFft (fftOrder, lfBoost, lfFft, lfWindow, lfFftOrder, lfFftSize,
                      lfFftWork, lfWindowed, columnDbLf, prevPhaseLf,
                      havePrevPhaseLf, lfFftSizeCachedForBins);
    if (needEnhanced && useMid)
        ensureAuxFft (fftOrder, 1, midFft, midWindow, midFftOrder, midFftSize,
                      midFftWork, midWindowed, columnDbMid, prevPhaseMid,
                      havePrevPhaseMid, midFftSizeCachedForBins);

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

    // Enhanced needs the longest aux window available before the first column.
    const int analysisWindow = needEnhanced
        ? juce::jmax (fftSize, juce::jmax (lfFftSize, midFftSize))
        : fftSize;

    // Drop backlog so we stay realtime — keep one window + a couple hops, not a FFT storm.
    const int maxHold = analysisWindow + hop * kMaxColumnsPerTick;
    if (available > maxHold)
    {
        ringReadPos = (write - analysisWindow + cap) % cap;
        available = analysisWindow;
    }

    // Brightness / floor / ceiling applied at colourise so history can be re-mapped live.
    const float smooth = juce::jlimit (0.0f, 0.95f, loadFloatParam ("SPEC_SMOOTH_ID", 35.0f) / 100.0f);
    const auto channel = currentChannelMode();
    const int numBins = fftSize / 2 + 1;
    const bool logFreq = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
    lastHopSamples = hop;

    if (needEnhanced != lastEnhancedMode
        || enhanced2D != lastEnhanced2D
        || enhanced3D != lastEnhanced3D)
    {
        lastEnhancedMode = needEnhanced;
        lastEnhanced2D = enhanced2D;
        lastEnhanced3D = enhanced3D;
        havePrevPhase = false;
        havePrevPhaseLf = false;
        havePrevPhaseMid = false;
        enhancedLfFrameCounter = 0;
        if (dualHistory)
            ensureHistoryBuffer();
        else
            historyDb3D.clear();
        // Force 3D mesh to reseed — history content source may have switched.
        historyColumnSerial.store (0, std::memory_order_relaxed);
    }

    if (needEnhanced && (int) prevPhase.size() != numBins)
    {
        prevPhase.assign ((size_t) numBins, 0.0f);
        havePrevPhase = false;
    }

    auto readChannelSample = [&] (int absIndex) -> float
    {
        const int idx = ((absIndex % cap) + cap) % cap;
        const float l = ringL[(size_t) idx];
        const float rCh = ringR[(size_t) idx];
        switch (channel)
        {
            case ChannelMode::left:  return l;
            case ChannelMode::right: return rCh;
            default:                 return 0.5f * (l + rCh);
        }
    };

    auto runComplexFft = [&] (juce::dsp::FFT& transform,
                              juce::dsp::WindowingFunction<float>& winFn,
                              int thisSize, int windowEnd,
                              std::vector<float>& winBuf, std::vector<float>& work,
                              std::vector<float>& colDb)
    {
        const int start = windowEnd - thisSize;
        for (int i = 0; i < thisSize; ++i)
            winBuf[(size_t) i] = readChannelSample (start + i);
        winFn.multiplyWithWindowingTable (winBuf.data(), (size_t) thisSize);
        std::fill (work.begin(), work.end(), 0.0f);
        std::copy (winBuf.begin(), winBuf.begin() + thisSize, work.begin());
        transform.performRealOnlyForwardTransform (work.data(), true);

        const int bins = thisSize / 2 + 1;
        if ((int) colDb.size() < bins)
            colDb.assign ((size_t) bins, -120.0f);

        for (int bin = 0; bin < bins; ++bin)
        {
            const float re = work[(size_t) (bin * 2)];
            const float im = work[(size_t) (bin * 2 + 1)];
            const float mag = std::sqrt (re * re + im * im) / (float) thisSize;
            const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-12f), -120.0f);
            colDb[(size_t) bin] = colDb[(size_t) bin] * smooth + db * (1.0f - smooth);
        }
    };

    int columnsWritten = 0;
    while (available >= analysisWindow && columnsWritten < kMaxColumnsPerTick)
    {
        // Main window ends at ringReadPos + analysisWindow (same time origin as aux windows).
        const int windowEnd = ringReadPos + analysisWindow;
        const int mainStart = windowEnd - fftSize;

        for (int i = 0; i < fftSize; ++i)
            windowed[(size_t) i] = readChannelSample (mainStart + i);

        window->multiplyWithWindowingTable (windowed.data(), (size_t) fftSize);
        std::fill (fftWork.begin(), fftWork.end(), 0.0f);
        std::copy (windowed.begin(), windowed.end(), fftWork.begin());

        if (needEnhanced)
        {
            constexpr float kTwoPi = juce::MathConstants<float>::twoPi;
            ++enhancedLfFrameCounter;

            // LF FFT every other column when detail > Off (reuse spectrum on skip frames).
            const bool computeLf = lfBoost > 0 && lfFft != nullptr && lfFftSize > 0
                                   && ((enhancedLfFrameCounter & 1) == 1 || ! havePrevPhaseLf);
            const bool computeMid = useMid && midFft != nullptr && midFftSize > 0;

            if (computeLf)
                runComplexFft (*lfFft, *lfWindow, lfFftSize, windowEnd,
                               lfWindowed, lfFftWork, columnDbLf);
            if (computeMid)
                runComplexFft (*midFft, *midWindow, midFftSize, windowEnd,
                               midWindowed, midFftWork, columnDbMid);

            // ---- Main STFT (always) ----
            fft->performRealOnlyForwardTransform (fftWork.data(), true);
            for (int bin = 0; bin < numBins; ++bin)
            {
                const float re = fftWork[(size_t) (bin * 2)];
                const float im = fftWork[(size_t) (bin * 2 + 1)];
                const float mag = std::sqrt (re * re + im * im) / (float) fftSize;
                const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-12f), -120.0f);
                columnDb[(size_t) bin] = columnDb[(size_t) bin] * smooth + db * (1.0f - smooth);
            }

            ensureBinForRowMap();
            if (lfBoost > 0 && lfFftSize > 0)
                ensureAuxBinForRowMap (lfFftSize, lfFftSizeCachedForBins, binForRowLf);
            if (computeMid || (useMid && midFftSize > 0))
                ensureAuxBinForRowMap (midFftSize, midFftSizeCachedForBins, binForRowMid);

            const int lfBins = lfFftSize > 0 ? lfFftSize / 2 + 1 : 0;
            const int midBins = midFftSize > 0 ? midFftSize / 2 + 1 : 0;

            auto sampleCol = [] (const std::vector<float>& bins,
                                 const std::vector<float>& map,
                                 int yIdx, int nBins) -> float
            {
                if (nBins <= 0 || map.empty() || bins.empty())
                    return -120.0f;
                const float bf = map[(size_t) yIdx];
                const int b0 = juce::jlimit (0, nBins - 1, (int) std::floor (bf));
                const int b1 = juce::jmin (nBins - 1, b0 + 1);
                const float t = juce::jlimit (0.0f, 1.0f, bf - (float) b0);
                return bins[(size_t) b0] * (1.0f - t) + bins[(size_t) b1] * t;
            };

            // Soft band edges: LF↔mid around crossover, mid↔HF around ~2 kHz (or crossover×5).
            const float lfLo = crossoverHz * 0.75f;
            const float lfHi = crossoverHz * 1.25f;
            const float midHiCentre = useMid
                ? juce::jmax (kEnhancedMidHiHz, crossoverHz * 5.0f)
                : crossoverHz;
            const float midHi = useMid ? midHiCentre * 1.25f : lfHi;
            const float hfLo = useMid ? midHiCentre * 0.75f : lfLo;

            columnScratch.assign ((size_t) internalH, -120.0f);
            for (int y = 0; y < internalH; ++y)
            {
                const float yNorm = internalH > 1 ? (float) y / (float) (internalH - 1) : 0.0f;
                const float freq = freqForNormY (yNorm, sr, logFreq);
                const float hfDb = sampleCol (columnDb, binForRow, y, numBins);
                float db = hfDb;

                if (lfBoost > 0 && lfBins > 0)
                {
                    const float lfDb = sampleCol (columnDbLf, binForRowLf, y, lfBins);
                    if (useMid && midBins > 0)
                    {
                        const float midDb = sampleCol (columnDbMid, binForRowMid, y, midBins);

                        if (freq <= lfLo)
                            db = lfDb;
                        else if (freq < lfHi)
                        {
                            const float t = (freq - lfLo) / juce::jmax (1.0e-3f, lfHi - lfLo);
                            db = lfDb * (1.0f - t) + midDb * t;
                        }
                        else if (freq <= hfLo)
                            db = midDb;
                        else if (freq < midHi)
                        {
                            const float t = (freq - hfLo) / juce::jmax (1.0e-3f, midHi - hfLo);
                            db = midDb * (1.0f - t) + hfDb * t;
                        }
                        else
                            db = hfDb;
                    }
                    else
                    {
                        if (freq <= lfLo)
                            db = lfDb;
                        else if (freq < lfHi)
                        {
                            const float t = (freq - lfLo) / juce::jmax (1.0e-3f, lfHi - lfLo);
                            db = lfDb * (1.0f - t) + hfDb * t;
                        }
                        else
                            db = hfDb;
                    }
                }

                columnScratch[(size_t) y] = db;
            }

            // Classic display rows from the same main STFT (for dual 2D/3D modes).
            if (needClassic)
                buildClassicDisplayColumn (columnDb.data(), numBins, columnClassic);

            // Reassigned column starts as continuum; IF deposits sharpen on top.
            columnSoftTmp = columnScratch;
            ensureHistoryBuffer();
            // Time-reassignment writes into whichever history stores the enhanced view.
            std::vector<float>* enhancedHistory = enhanced2D ? &historyDb
                                                : (enhanced3D ? &historyDb3D : nullptr);
            float* prevHistoryCol = (enhancedHistory != nullptr
                                     && internalW > 1
                                     && enhancedHistory->size() == historyDb.size())
                ? enhancedHistory->data() + (size_t) (internalW - 1) * (size_t) internalH
                : nullptr;
            bool touchedPrevColumn = false;

            auto wrapPi = [] (float x)
            {
                while (x > juce::MathConstants<float>::pi)  x -= kTwoPi;
                while (x < -juce::MathConstants<float>::pi) x += kTwoPi;
                return x;
            };

            auto reassignRange = [&] (const float* work, std::vector<float>& colDb,
                                      std::vector<float>& prevPh, bool& havePrev,
                                      int thisFftSize, int thisBins,
                                      float fMinHz, float fMaxHz)
            {
                if (work == nullptr || thisFftSize <= 0 || thisBins < 3)
                    return;

                if ((int) prevPh.size() != thisBins)
                {
                    prevPh.assign ((size_t) thisBins, 0.0f);
                    havePrev = false;
                }

                const float binHz = (float) sr / (float) thisFftSize;
                const float expectedPerBin = kTwoPi * (float) hop / (float) thisFftSize;

                for (int bin = 1; bin < thisBins - 1; ++bin)
                {
                    const float centreHz = (float) bin * binHz;
                    const float re = work[(size_t) (bin * 2)];
                    const float im = work[(size_t) (bin * 2 + 1)];
                    const float phase = std::atan2 (im, re);
                    const float smoothedDb = colDb[(size_t) bin];

                    if (centreHz < fMinHz || centreHz > fMaxHz || smoothedDb < -112.0f)
                    {
                        prevPh[(size_t) bin] = phase;
                        continue;
                    }

                    float ifHz = centreHz;
                    float timeOffset = 0.0f;

                    if (havePrev)
                    {
                        float dPhase = wrapPi (phase - prevPh[(size_t) bin]);
                        float residual = wrapPi (dPhase - expectedPerBin * (float) bin);
                        const float binOffset = juce::jlimit (-1.25f, 1.25f,
                                                              residual / juce::jmax (1.0e-6f, expectedPerBin));
                        ifHz = ((float) bin + binOffset) * binHz;
                        if (! std::isfinite (ifHz) || ifHz < kMinDisplayHz * 0.25f)
                            ifHz = centreHz;
                    }

                    // Group delay from adjacent-bin phase (no extra FFT): τ ≈ −∂φ/∂ω.
                    {
                        const float reM = work[(size_t) ((bin - 1) * 2)];
                        const float imM = work[(size_t) ((bin - 1) * 2 + 1)];
                        const float reP = work[(size_t) ((bin + 1) * 2)];
                        const float imP = work[(size_t) ((bin + 1) * 2 + 1)];
                        const float phM = std::atan2 (imM, reM);
                        const float phP = std::atan2 (imP, reP);
                        const float dPhi = wrapPi (phP - phM) * 0.5f; // per bin
                        timeOffset = juce::jlimit (-(float) hop, (float) hop,
                                                   -dPhi * (float) thisFftSize / kTwoPi);
                    }

                    prevPh[(size_t) bin] = phase;

                    if (ifHz < fMinHz * 0.85f || ifHz > fMaxHz * 1.15f)
                        ifHz = juce::jlimit (fMinHz, fMaxHz, ifHz);

                    if (depositEnhanced (columnSoftTmp.data(), prevHistoryCol,
                                         ifHz, smoothedDb, sr, logFreq,
                                         timeOffset, (float) hop))
                        touchedPrevColumn = true;
                }

                for (int bin : { 0, thisBins - 1 })
                {
                    if (bin < 0 || bin >= thisBins)
                        continue;
                    const float re = work[(size_t) (bin * 2)];
                    const float im = work[(size_t) (bin * 2 + 1)];
                    prevPh[(size_t) bin] = std::atan2 (im, re);
                }

                havePrev = true;
            };

            // Skip LF reassignment on reuse frames — stale complex spectrum would corrupt IF.
            if (computeLf && lfBoost > 0 && lfFftSize > 0 && ! lfFftWork.empty())
            {
                reassignRange (lfFftWork.data(), columnDbLf, prevPhaseLf, havePrevPhaseLf,
                               lfFftSize, lfBins, kMinDisplayHz * 0.5f, lfHi);
            }

            if (useMid && midFftSize > 0 && ! midFftWork.empty())
            {
                reassignRange (midFftWork.data(), columnDbMid, prevPhaseMid, havePrevPhaseMid,
                               midFftSize, midBins, lfLo, midHi);
            }

            reassignRange (fftWork.data(), columnDb, prevPhase, havePrevPhase,
                           fftSize, numBins, hfLo, (float) (sr * 0.49));

            // Strength: 0 = continuum, 100 = fully reassigned.
            if (enhancedStrength < 0.999f)
            {
                for (int y = 0; y < internalH; ++y)
                    columnScratch[(size_t) y] = columnScratch[(size_t) y] * (1.0f - enhancedStrength)
                                              + columnSoftTmp[(size_t) y] * enhancedStrength;
            }
            else
            {
                columnScratch.swap (columnSoftTmp);
            }

            // Recolour 2D scroll only when the enhanced history is the 2D buffer.
            if (touchedPrevColumn && prevHistoryCol != nullptr && enhanced2D)
            {
                const float brightness = juce::jlimit (10.0f, 200.0f,
                                                       loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f)) / 100.0f;
                colouriseColumnIntoImage (internalW - 1, prevHistoryCol, brightness,
                                          loadFloatParam ("SPEC_MIN_DB_ID", -90.0f),
                                          loadFloatParam ("SPEC_MAX_DB_ID", -6.0f));
            }

            // Mild soften — hide row gaps without undoing ridges.
            const float soften = juce::jlimit (0.0f, 0.45f,
                                               loadFloatParam ("SPEC_SOFTEN_ID", 55.0f) / 100.0f * 0.45f);
            if (soften > 0.001f && internalH > 2)
            {
                columnSoftTmp = columnScratch;
                constexpr float kKernel[3] = { 0.18f, 0.64f, 0.18f };
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

            const float* col2D = enhanced2D ? columnScratch.data() : columnClassic.data();
            const float* col3D = enhanced3D ? columnScratch.data() : columnClassic.data();
            appendDisplayColumn (col2D);
            if (dualHistory)
                appendHistory3DColumn (col3D);
        }
        else
        {
            havePrevPhase = false;
            havePrevPhaseLf = false;
            havePrevPhaseMid = false;
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

void SpectrogramComponent::getHistorySnapshot (std::vector<float>& outColumnMajorDb,
                                               int& outW, int& outH,
                                               float& outBrightness, float& outMinDb, float& outMaxDb) const
{
    outW = internalW;
    outH = internalH;
    outBrightness = juce::jlimit (10.0f, 200.0f, loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f)) / 100.0f;
    outMinDb = loadFloatParam ("SPEC_MIN_DB_ID", -90.0f);
    outMaxDb = juce::jmax (outMinDb + 6.0f, loadFloatParam ("SPEC_MAX_DB_ID", -6.0f));

    const size_t n = (size_t) juce::jmax (1, outW * outH);
    const bool dual = loadBoolParam ("SPEC_ENHANCED_FREQ_ID", false)
                   != loadBoolParam ("SPEC_ENHANCED_FREQ_3D_ID", false);
    const auto& src = (dual && historyDb3D.size() == historyDb.size() && ! historyDb3D.empty())
                          ? historyDb3D
                          : historyDb;

    if (src.size() != (size_t) internalW * (size_t) internalH)
    {
        outColumnMajorDb.assign (n, -120.0f);
        return;
    }

    outColumnMajorDb = src;
}

juce::Colour SpectrogramComponent::colourFromHistoryDb (float db, float brightness,
                                                        float minDb, float maxDb) const
{
    // Assumes colourLut is current (caller should rebuildColourLut / warm once per mesh).
    const float dbGain = juce::Decibels::gainToDecibels (juce::jmax (brightness, 1.0e-3f), -100.0f);
    const float denom = juce::jmax (1.0f, maxDb - minDb);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + dbGain - minDb) / denom);
    const int lutIdx = juce::jlimit (0, kLutSize - 1,
                                     (int) std::lround (norm * (float) (kLutSize - 1)));
    return juce::Colour (colourLut[(size_t) lutIdx]);
}

juce::Colour SpectrogramComponent::colourFromHistoryDb3D (float db, float brightness,
                                                          float minDb, float maxDb) const
{
    const float dbGain = juce::Decibels::gainToDecibels (juce::jmax (brightness, 1.0e-3f), -100.0f);
    const float denom = juce::jmax (1.0f, maxDb - minDb);
    const float norm = juce::jlimit (0.0f, 1.0f, (db + dbGain - minDb) / denom);
    const int lutIdx = juce::jlimit (0, kLutSize - 1,
                                     (int) std::lround (norm * (float) (kLutSize - 1)));
    return juce::Colour (colourLut3D[(size_t) lutIdx]);
}

void SpectrogramComponent::colouriseColumnIntoImage (int x, const float* dbRows,
                                                    float brightness, float minDb, float maxDb)
{
    if (! scrollImage.isValid() || dbRows == nullptr || x < 0 || x >= internalW)
        return;

    // Compact strip is heavily downscaled — lift gain hard so it matches the expanded pane.
    const float brightAdj = expanded ? brightness : brightness * 2.75f;
    const float dbGain = juce::Decibels::gainToDecibels (juce::jmax (brightAdj, 1.0e-3f), -100.0f);
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
    historyColumnSerial.store (0, std::memory_order_relaxed);
        if (scrollImage.isValid())
            scrollImage.clear (scrollImage.getBounds(), juce::Colours::black);
        lfFftSizeCachedForBins = 0;
        midFftSizeCachedForBins = 0;
    }
}

void SpectrogramComponent::ensureAuxBinForRowMap (int auxFftSize, int& auxBinsCached,
                                                 std::vector<float>& auxBinForRow)
{
    const double sr = sampleRateHz.load (std::memory_order_relaxed);
    const bool logFreq = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
    const int numBinsSafe = juce::jmax (1, auxFftSize / 2 + 1);

    if (auxFftSize == auxBinsCached && logFreq == logFreqCached && sr == srCachedForBins
        && (int) auxBinForRow.size() == internalH)
        return;

    auxBinsCached = auxFftSize;
    auxBinForRow.resize ((size_t) internalH);

    for (int y = 0; y < internalH; ++y)
    {
        const float yNorm = internalH > 1 ? (float) y / (float) (internalH - 1) : 0.0f;
        const float freq = freqForNormY (yNorm, sr, logFreq);
        const float binF = (float) (freq * (double) auxFftSize / sr);
        auxBinForRow[(size_t) y] = juce::jlimit (0.0f, (float) (numBinsSafe - 1), binF);
    }
}

bool SpectrogramComponent::depositEnhanced (float* columnRows, float* prevColumnRows,
                                            float ifHz, float db, double sr, bool logFreq,
                                            float timeOffsetSamples, float hopSamples) const
{
    if (columnRows == nullptr || internalH <= 1 || ! std::isfinite (ifHz) || ! std::isfinite (db))
        return false;

    auto depositInto = [&] (float* target)
    {
        if (target == nullptr)
            return;

        // Log-constant width (~1/40 octave) so LF ridges aren't paper-thin while HF stays sharp.
        constexpr float kHalfOctave = 1.0f / 40.0f;
        const float hzLo = ifHz * std::pow (2.0f, -kHalfOctave);
        const float hzHi = ifHz * std::pow (2.0f, kHalfOctave);
        const float yCentre = normYForFreq (ifHz, sr, logFreq) * (float) (internalH - 1);
        const float yA = normYForFreq (hzHi, sr, logFreq) * (float) (internalH - 1);
        const float yB = normYForFreq (hzLo, sr, logFreq) * (float) (internalH - 1);
        const float y0f = juce::jmin (yA, yB);
        const float y1f = juce::jmax (yA, yB);
        const float halfW = juce::jmax (0.75f, 0.5f * (y1f - y0f));

        const int yStart = juce::jlimit (0, internalH - 1, (int) std::floor (yCentre - halfW));
        const int yEnd = juce::jlimit (0, internalH - 1, (int) std::ceil (yCentre + halfW));

        for (int y = yStart; y <= yEnd; ++y)
        {
            const float dist = std::abs ((float) y - yCentre) / halfW;
            const float atten = juce::jlimit (0.0f, 12.0f, dist * dist * 12.0f);
            target[y] = juce::jmax (target[y], db - atten);
        }
    };

    // Time reassignment: early group delay paints into the previous history column.
    const bool toPrev = prevColumnRows != nullptr
                        && hopSamples > 1.0f
                        && timeOffsetSamples < -0.35f * hopSamples;
    const bool toBoth = prevColumnRows != nullptr
                        && hopSamples > 1.0f
                        && ! toPrev
                        && timeOffsetSamples < -0.12f * hopSamples;

    if (toPrev)
    {
        depositInto (prevColumnRows);
        return true;
    }

    if (toBoth)
    {
        depositInto (columnRows);
        depositInto (prevColumnRows);
        return true;
    }

    depositInto (columnRows);
    return false;
}

void SpectrogramComponent::buildClassicDisplayColumn (const float* magnitudesDb, int numBins,
                                                      std::vector<float>& outRows)
{
    if (magnitudesDb == nullptr || internalH <= 0)
        return;

    ensureBinForRowMap();
    const int numBinsSafe = juce::jmax (1, numBins > 0 ? numBins : (fftSize / 2 + 1));
    outRows.resize ((size_t) internalH);
    for (int y = 0; y < internalH; ++y)
    {
        const float binF = binForRow[(size_t) y];
        const int b0 = juce::jlimit (0, numBinsSafe - 1, (int) binF);
        const int b1 = juce::jmin (numBinsSafe - 1, b0 + 1);
        const float frac = juce::jlimit (0.0f, 1.0f, binF - (float) b0);
        outRows[(size_t) y] = magnitudesDb[(size_t) b0] * (1.0f - frac)
                            + magnitudesDb[(size_t) b1] * frac;
    }
    softenColumnVertical (outRows, internalH);
}

void SpectrogramComponent::appendColumn (const float* magnitudesDb, int numBins)
{
    if (! scrollImage.isValid() || magnitudesDb == nullptr)
        return;

    ensureHistoryBuffer();
    buildClassicDisplayColumn (magnitudesDb, numBins, columnScratch);
    appendDisplayColumn (columnScratch.data());
}

void SpectrogramComponent::appendHistory3DColumn (const float* displayDbRows)
{
    if (displayDbRows == nullptr || internalH <= 0 || internalW <= 0)
        return;

    ensureHistoryBuffer();
    const size_t n = (size_t) internalW * (size_t) internalH;
    if (historyDb3D.size() != n)
        historyDb3D.assign (n, -120.0f);

    if (internalW > 1)
    {
        std::memmove (historyDb3D.data(),
                      historyDb3D.data() + (size_t) internalH,
                      (size_t) (internalW - 1) * (size_t) internalH * sizeof (float));
    }

    std::copy (displayDbRows, displayDbRows + internalH,
               historyDb3D.begin() + (size_t) (internalW - 1) * (size_t) internalH);
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
    historyColumnSerial.fetch_add (1, std::memory_order_relaxed);

    const float brightness = juce::jlimit (10.0f, 200.0f, loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f)) / 100.0f;
    const float minDb = loadFloatParam ("SPEC_MIN_DB_ID", -90.0f);
    const float maxDb = juce::jmax (minDb + 6.0f, loadFloatParam ("SPEC_MAX_DB_ID", -6.0f));

    if (lutScheme != currentScheme())
        rebuildColourLut();

    colouriseColumnIntoImage (x, displayDbRows, brightness, minDb, maxDb);
    // Do not stamp lastLookFingerprint here — a mid-frame LUT/ramp change would
    // hide the mismatch and leave already-drawn columns on the old palette.

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

    // Apply look/ramp changes before depositing new columns so history is
    // recoloured first (and fingerprint isn't stamped by a single new column).
    const auto fp = lookFingerprint();
    if (fp != lastLookFingerprint)
    {
        const bool logNow = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
        if (logNow != logFreqCached)
        {
            // Axis change: history rows are wrong — clear; new columns refill.
            historyDb.assign ((size_t) internalW * (size_t) internalH, -120.0f);
    historyColumnSerial.store (0, std::memory_order_relaxed);
            if (scrollImage.isValid())
                scrollImage.clear (scrollImage.getBounds(), juce::Colours::black);
            logFreqCached = logNow;
            srCachedForBins = 0.0;
        }
        rebuildColourLut();
        rerenderScrollFromHistory();
    }

    advanceFromRing();

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

void SpectrogramComponent::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu() && onShowContextMenu != nullptr)
        onShowContextMenu();
}

void SpectrogramComponent::mouseDoubleClick (const juce::MouseEvent&)
{
    if (onDoubleClick != nullptr)
        onDoubleClick();
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

    // Opaque clear — transparent holes + blur made the compact strip look washed/dim.
    screenImage.clear (screenImage.getBounds(), juce::Colours::black);

    {
        juce::Graphics ig (screenImage);
        ig.setImageResamplingQuality (juce::Graphics::highResamplingQuality);
        ig.drawImage (scrollImage, screenImage.getBounds().toFloat());
    }

    // Soften 0..100 → blur radius 0..5 (screen pixels). 0 skips Melatonin path.
    // Compact: no soften — blur + downscale was washing the strip dark.
    const float soften = expanded
                             ? juce::jlimit (0.0f, 100.0f, loadFloatParam ("SPEC_SOFTEN_ID", 55.0f))
                             : 0.0f;
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

    juce::Path window;
    if (! expanded)
    {
        // Match oscilloscope compact chrome: opaque fill + rounded outline.
        window.addRoundedRectangle (bounds.reduced (0.5f), 5.0f);
        g.setColour (theme.oscBackground);
        g.fillPath (window);
        g.setColour (theme.oscBackground2.withAlpha (140.0f / 255.0f));
        g.strokePath (window, juce::PathStrokeType (1.0f));
    }
    else
    {
        g.fillAll (theme.oscBackground);
    }

    if (scrollImage.isValid())
    {
        const auto imageBounds = bounds;
        // Compact: skip soften entirely (blur reads as dim on the tiny strip).
        const float soften = expanded
                                 ? juce::jlimit (0.0f, 100.0f, loadFloatParam ("SPEC_SOFTEN_ID", 55.0f))
                                 : 0.0f;
        const int radius = juce::roundToInt (soften * 0.05f);

        juce::Graphics::ScopedSaveState state (g);
        if (! expanded)
            g.reduceClipRegion (window);

        if (radius <= 0)
        {
            // lowResamplingQuality keeps bright peaks when shrinking the scroll buffer.
            g.setImageResamplingQuality (expanded ? juce::Graphics::highResamplingQuality
                                                  : juce::Graphics::lowResamplingQuality);
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
