#include "SpectrogramComponent.h"

namespace
{
    float freqForNormY (float yNorm, double sampleRate)
    {
        const float nyquist = (float) (sampleRate * 0.5);
        const float maxHz = juce::jmin (SpectrogramComponent::kMaxDisplayHz, nyquist * 0.999f);
        const float minHz = SpectrogramComponent::kMinDisplayHz;
        // yNorm 0 = top = high freq, 1 = bottom = low freq
        const float t = 1.0f - juce::jlimit (0.0f, 1.0f, yNorm);
        return minHz * std::pow (maxHz / minHz, t);
    }

    float lerpColourChannel (float a, float b, float t) noexcept
    {
        return a + (b - a) * t;
    }

    juce::Colour lerpColour (juce::Colour a, juce::Colour b, float t) noexcept
    {
        t = juce::jlimit (0.0f, 1.0f, t);
        return juce::Colour::fromFloatRGBA (
            lerpColourChannel (a.getFloatRed(), b.getFloatRed(), t),
            lerpColourChannel (a.getFloatGreen(), b.getFloatGreen(), t),
            lerpColourChannel (a.getFloatBlue(), b.getFloatBlue(), t),
            1.0f);
    }

    juce::Colour sampleStops (const juce::Colour* stops, int numStops, float t) noexcept
    {
        if (numStops <= 1)
            return stops[0];

        t = juce::jlimit (0.0f, 1.0f, t);
        const float scaled = t * (float) (numStops - 1);
        const int i0 = (int) scaled;
        const int i1 = juce::jmin (numStops - 1, i0 + 1);
        return lerpColour (stops[i0], stops[i1], scaled - (float) i0);
    }
}

SpectrogramComponent::SpectrogramComponent()
{
    setOpaque (false);
    setVisible (false);
}

SpectrogramComponent::~SpectrogramComponent()
{
    stopTimer();
}

juce::StringArray SpectrogramComponent::getColourSchemeNames()
{
    return { "Classic", "Inferno", "Magma", "Viridis", "Ice", "Greyscale", "Heat" };
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

    const int newCap = juce::jmax (2048, (int) std::ceil (sr * (double) kMaxBufferSeconds));
    ringL.assign ((size_t) newCap, 0.0f);
    ringR.assign ((size_t) newCap, 0.0f);
    writePos.store (0, std::memory_order_relaxed);
    capacity.store (newCap, std::memory_order_release);
    resetDisplay();
}

void SpectrogramComponent::resetDisplay()
{
    ringReadPos = writePos.load (std::memory_order_acquire);
    samplesUntilHop = 0;
    writeCol = 0;

    if (! history.empty())
        std::fill (history.begin(), history.end(), 0.0f);
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
    resetDisplay();
    ensureDisplaySize (juce::jmax (8, getWidth()), juce::jmax (8, getHeight()));
    repaint();
}

float SpectrogramComponent::getSpeed() const
{
    return juce::jlimit (1.0f, 100.0f, loadFloatParam ("SPEC_SPEED_ID", 55.0f));
}

void SpectrogramComponent::speedUp()
{
    if (valueTree == nullptr)
        return;

    if (auto* p = valueTree->getParameter ("SPEC_SPEED_ID"))
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jmin (100.0f, getSpeed() + 8.0f)));
}

void SpectrogramComponent::speedDown()
{
    if (valueTree == nullptr)
        return;

    if (auto* p = valueTree->getParameter ("SPEC_SPEED_ID"))
        p->setValueNotifyingHost (p->convertTo0to1 (juce::jmax (1.0f, getSpeed() - 8.0f)));
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
                                  loadChoiceIndex ("SPEC_COLOUR_SCHEME_ID", (int) ColourScheme::inferno));
    return static_cast<ColourScheme> (idx);
}

SpectrogramComponent::ChannelMode SpectrogramComponent::currentChannelMode() const
{
    const int idx = juce::jlimit (0, 2, loadChoiceIndex ("SPEC_CHANNEL_ID", 0));
    return static_cast<ChannelMode> (idx);
}

int SpectrogramComponent::currentFftOrder() const
{
    // 0=512, 1=1024, 2=2048, 3=4096
    const int idx = juce::jlimit (0, 3, loadChoiceIndex ("SPEC_FFT_SIZE_ID", 2));
    return 9 + idx;
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
    columnDb.assign ((size_t) (fftSize / 2 + 1), 0.0f);
}

void SpectrogramComponent::ensureDisplaySize (int widthPx, int heightPx)
{
    widthPx = juce::jmax (8, widthPx);
    heightPx = juce::jmax (8, heightPx);

    if (widthPx == historyW && heightPx == historyH && ! history.empty())
        return;

    historyW = widthPx;
    historyH = heightPx;
    history.assign ((size_t) historyW * (size_t) historyH, 0.0f);
    writeCol = 0;
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
    if (cap <= 0 || historyW <= 0 || historyH <= 0)
        return;

    ensureFft (currentFftOrder());
    if (fft == nullptr || fftSize <= 0)
        return;

    // Speed 1..100 → hop as fraction of FFT (smaller hop = faster scroll).
    const float speed = juce::jlimit (1.0f, 100.0f, loadFloatParam ("SPEC_SPEED_ID", 55.0f));
    const int hop = juce::jmax (32, (int) std::round ((float) fftSize * (1.05f - speed * 0.009f)));

    const int write = writePos.load (std::memory_order_acquire);
    int available = write - ringReadPos;
    if (available < 0)
        available += cap;

    while (available >= fftSize)
    {
        const auto channel = currentChannelMode();
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = (ringReadPos + i) % cap;
            const float l = ringL[(size_t) idx];
            const float r = ringR[(size_t) idx];
            float s = 0.0f;
            switch (channel)
            {
                case ChannelMode::left:  s = l; break;
                case ChannelMode::right: s = r; break;
                default:                 s = 0.5f * (l + r); break;
            }
            windowed[(size_t) i] = s;
        }

        window->multiplyWithWindowingTable (windowed.data(), (size_t) fftSize);
        std::fill (fftWork.begin(), fftWork.end(), 0.0f);
        std::copy (windowed.begin(), windowed.end(), fftWork.begin());
        fft->performFrequencyOnlyForwardTransform (fftWork.data());

        const int numBins = fftSize / 2 + 1;
        const float brightness = juce::jlimit (10.0f, 200.0f, loadFloatParam ("SPEC_BRIGHTNESS_ID", 100.0f)) / 100.0f;
        const float minDb = loadFloatParam ("SPEC_MIN_DB_ID", -90.0f);
        const float maxDb = juce::jmax (minDb + 6.0f, loadFloatParam ("SPEC_MAX_DB_ID", -6.0f));
        const float smooth = juce::jlimit (0.0f, 0.95f, loadFloatParam ("SPEC_SMOOTH_ID", 35.0f) / 100.0f);

        for (int bin = 0; bin < numBins; ++bin)
        {
            const float mag = fftWork[(size_t) bin] * brightness / (float) fftSize;
            const float db = juce::Decibels::gainToDecibels (juce::jmax (mag, 1.0e-12f), -120.0f);
            float norm = juce::jlimit (0.0f, 1.0f, (db - minDb) / (maxDb - minDb));
            columnDb[(size_t) bin] = columnDb[(size_t) bin] * smooth + norm * (1.0f - smooth);
        }

        writeColumn (columnDb.data(), numBins);

        ringReadPos = (ringReadPos + hop) % cap;
        available -= hop;
    }

    juce::ignoreUnused (samplesUntilHop);
}

void SpectrogramComponent::writeColumn (const float* magnitudesNorm, int numBins)
{
    juce::ignoreUnused (numBins);
    if (historyW <= 0 || historyH <= 0 || magnitudesNorm == nullptr)
        return;

    const double sr = sampleRateHz.load (std::memory_order_relaxed);
    const bool logFreq = loadBoolParam ("SPEC_LOG_FREQ_ID", true);
    const int numBinsSafe = fftSize / 2 + 1;

    for (int y = 0; y < historyH; ++y)
    {
        const float yNorm = historyH > 1 ? (float) y / (float) (historyH - 1) : 0.0f;
        float freq = 0.0f;
        if (logFreq)
        {
            freq = freqForNormY (yNorm, sr);
        }
        else
        {
            const float nyquist = (float) (sr * 0.5);
            const float maxHz = juce::jmin (kMaxDisplayHz, nyquist * 0.999f);
            freq = kMinDisplayHz + (1.0f - yNorm) * (maxHz - kMinDisplayHz);
        }

        const float binF = (float) (freq * (double) fftSize / sr);
        const int b0 = juce::jlimit (0, numBinsSafe - 1, (int) binF);
        const int b1 = juce::jmin (numBinsSafe - 1, b0 + 1);
        const float frac = binF - (float) b0;
        const float v = magnitudesNorm[(size_t) b0] * (1.0f - frac)
                      + magnitudesNorm[(size_t) b1] * frac;
        history[(size_t) writeCol * (size_t) historyH + (size_t) y] = juce::jlimit (0.0f, 1.0f, v);
    }

    writeCol = (writeCol + 1) % historyW;
}

juce::Colour SpectrogramComponent::mapMagnitude (float norm01) const
{
    const float t = juce::jlimit (0.0f, 1.0f, norm01);

    switch (currentScheme())
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
            const juce::Colour stops[] = {
                juce::Colours::black,
                juce::Colours::white
            };
            return sampleStops (stops, (int) (sizeof (stops) / sizeof (stops[0])), t);
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

void SpectrogramComponent::timerCallback()
{
    if (! enabled.load (std::memory_order_relaxed))
        return;

    advanceFromRing();
    repaint();
}

void SpectrogramComponent::resized()
{
    ensureDisplaySize (getWidth(), getHeight());
}

void SpectrogramComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto& theme = colors();

    if (! expanded)
    {
        juce::Path window;
        window.addRoundedRectangle (bounds.reduced (0.5f), 5.0f);
        g.setColour (theme.oscBackground.withAlpha (170.0f / 255.0f));
        g.fillPath (window);
    }

    if (historyW <= 0 || historyH <= 0 || history.empty())
        return;

    juce::Image img (juce::Image::ARGB, historyW, historyH, false);
    {
        juce::Image::BitmapData pixels (img, juce::Image::BitmapData::writeOnly);
        for (int x = 0; x < historyW; ++x)
        {
            const int col = (writeCol + x) % historyW;
            for (int y = 0; y < historyH; ++y)
            {
                const float v = history[(size_t) col * (size_t) historyH + (size_t) y];
                pixels.setPixelColour (x, y, mapMagnitude (v));
            }
        }
    }

    g.setImageResamplingQuality (juce::Graphics::lowResamplingQuality);
    g.drawImage (img, bounds.reduced (expanded ? 0.0f : 1.0f));
}
