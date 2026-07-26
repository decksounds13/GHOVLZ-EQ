#include "Analyser.h"
#include <JuceHeader.h>

namespace
{
    constexpr size_t kRingMask = 16384u - 1u;
}

Analyser::Analyser (juce::AudioProcessorValueTreeState& audioProcessorValueTreeState)
    : juce::Thread ("SpectrumAnalyser"),
      mr_valueTree (audioProcessorValueTreeState),
      m_forwardFFT_9 (9),
      m_forwardFFT_10 (10),
      m_forwardFFT_11 (11),
      m_forwardFFT_12 (12),
      m_forwardFFT_13 (13),
      m_forwardFFT_14 (14),
      m_window_512 (512, juce::dsp::WindowingFunction<float>::hann),
      m_window_1024 (1024, juce::dsp::WindowingFunction<float>::hann),
      m_window_2048 (2048, juce::dsp::WindowingFunction<float>::hann),
      m_window_4096 (4096, juce::dsp::WindowingFunction<float>::hann),
      m_window_8192 (8192, juce::dsp::WindowingFunction<float>::hann),
      m_window_16384 (16384, juce::dsp::WindowingFunction<float>::hann)
{
    int blockIndex = 0;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (mr_valueTree.getParameter ("BLOCK_ID")))
        blockIndex = choice->getIndex();
    else if (auto* raw = mr_valueTree.getRawParameterValue ("BLOCK_ID"))
        blockIndex = juce::jlimit (0, 3, (int) std::lround (raw->load()));

    setFFTBlockSize (blockIndex);

    if (auto* p = mr_valueTree.getRawParameterValue ("REFRESH_ID"))
        setRefreshMs ((int) std::lround (p->load()));

    mr_valueTree.addParameterListener ("BLOCK_ID", this);
    mr_valueTree.addParameterListener ("LEFT_ID", this);
    mr_valueTree.addParameterListener ("RIGHT_ID", this);
    mr_valueTree.addParameterListener ("BOTH_ID", this);
    mr_valueTree.addParameterListener ("AVG_ID", this);
    mr_valueTree.addParameterListener ("RANGE_ID", this);
    mr_valueTree.addParameterListener ("MAXIMUM_ID", this);
    mr_valueTree.addParameterListener ("MINIMUM_ID", this);
    mr_valueTree.addParameterListener ("REFRESH_ID", this);

    startThread (juce::Thread::Priority::low);
}

Analyser::~Analyser()
{
    stopThread (2000);

    mr_valueTree.removeParameterListener ("BLOCK_ID", this);
    mr_valueTree.removeParameterListener ("LEFT_ID", this);
    mr_valueTree.removeParameterListener ("RIGHT_ID", this);
    mr_valueTree.removeParameterListener ("BOTH_ID", this);
    mr_valueTree.removeParameterListener ("AVG_ID", this);
    mr_valueTree.removeParameterListener ("RANGE_ID", this);
    mr_valueTree.removeParameterListener ("MAXIMUM_ID", this);
    mr_valueTree.removeParameterListener ("MINIMUM_ID", this);
    mr_valueTree.removeParameterListener ("REFRESH_ID", this);
}

// ============================================================================
void Analyser::resetScopeMaximumsData()
{
    const juce::ScopedLock analysisLock (m_analysisLock);
    const juce::ScopedLock lock (m_volumeRangeChange);

    const auto minimum = m_minimumVolumeInDecibels;

    for (auto& data : m_volumsMaximumData)
        data = minimum;

    {
        const juce::ScopedLock scopeLock (m_scopeLock);
        for (auto& data : m_scopeMaximumData)
            data = 0.0f;
    }

    m_currentMaximumVolumeInDecibels.store (minimum);
    m_currentAverageVolumeInDecibels.store (minimum);
    m_lastMaxHoldUpdateMs = 0.0;
}

// ============================================================================
void Analyser::setNextFFTBlockStatus (const bool nextFFTBlockStatus)
{
    m_nextFFTBlockReady.store (nextFFTBlockStatus);
}

bool Analyser::getNextFFTBlockStatus()
{
    return m_nextFFTBlockReady.load() && m_blockSizeDefined.load();
}

bool Analyser::getNextPreFFTBlockStatus()
{
    return m_preNextFFTBlockReady.load() && m_blockSizeDefined.load();
}

void Analyser::setNextPreFFTBlockStatus (const bool nextFFTBlockStatus)
{
    m_preNextFFTBlockReady.store (nextFFTBlockStatus);
}

// ============================================================================
size_t Analyser::getScopeSize()
{
    return m_scopeSize.load();
}

float Analyser::getScopeData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopeDynamicData.size())
        return 0.0f;

    return m_scopeDynamicData[index];
}

float Analyser::getScopePreData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopePreData.size())
        return 0.0f;

    return m_scopePreData[index];
}

float Analyser::getScopeMaximumsData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopeMaximumData.size())
        return 0.0f;

    return m_scopeMaximumData[index];
}

float Analyser::getOffset()
{
    return m_offset.load();
}

size_t Analyser::getFftSize() const
{
    return m_fftSize.load();
}

void Analyser::setSampleRate (const double sampleRate)
{
    if (sampleRate > 0.0)
        m_sampleRate.store (sampleRate);
}

double Analyser::getSampleRate() const
{
    return m_sampleRate.load();
}

float Analyser::getBinFrequencyHz (const size_t bin) const
{
    const double sr = m_sampleRate.load();
    const size_t fftSize = m_fftSize.load();

    if (sr <= 0.0 || fftSize == 0)
        return 0.0f;

    return static_cast<float> (static_cast<double> (bin) * sr / static_cast<double> (fftSize));
}

float Analyser::getDisplayMaxFrequencyHz() const
{
    const float nyquist = static_cast<float> (m_sampleRate.load() * 0.5);

    if (! (nyquist > 0.0f))
        return kMaxDisplayFrequencyHz;

    return juce::jmin (kMaxDisplayFrequencyHz, nyquist);
}

float Analyser::getBinDisplayXNorm (const int binIndex, const bool logarithmic) const
{
    if (binIndex <= 0)
        return 0.0f;

    const float freq = getBinFrequencyHz (static_cast<size_t> (binIndex));
    const float fMin = kMinDisplayFrequencyHz;
    const float fMax = getDisplayMaxFrequencyHz();

    if (! (fMax > fMin) || ! (freq > 0.0f))
        return 0.0f;

    if (logarithmic)
    {
        const float logMin = std::log10 (fMin);
        const float logMax = std::log10 (fMax);
        return (std::log10 (freq) - logMin) / (logMax - logMin);
    }

    return (freq - fMin) / (fMax - fMin);
}

float Analyser::getFrequencyForDisplayXNorm (const float xNorm, const bool logarithmic) const
{
    const float x = juce::jlimit (0.0f, 1.0f, xNorm);
    const float fMin = kMinDisplayFrequencyHz;
    const float fMax = getDisplayMaxFrequencyHz();

    if (! (fMax > fMin))
        return fMin;

    if (logarithmic)
    {
        const float logMin = std::log10 (fMin);
        const float logMax = std::log10 (fMax);
        return std::pow (10.0f, logMin + x * (logMax - logMin));
    }

    return fMin + x * (fMax - fMin);
}

float Analyser::getFractionalBinForFrequencyHz (const float freqHz) const
{
    if (! (freqHz > 0.0f))
        return 0.0f;

    const double sr = m_sampleRate.load();
    const size_t fftSize = m_fftSize.load();

    if (sr <= 0.0 || fftSize == 0)
        return 0.0f;

    return static_cast<float> (static_cast<double> (freqHz) * static_cast<double> (fftSize) / sr);
}

int Analyser::getHighestDisplayBinIndex() const
{
    const int scopeSize = static_cast<int> (m_scopeSize.load());
    if (scopeSize < 2)
        return 1;

    const double sr = m_sampleRate.load();
    const size_t fftSize = m_fftSize.load();
    if (sr <= 0.0 || fftSize == 0)
        return scopeSize - 1;

    const double maxFreq = static_cast<double> (getDisplayMaxFrequencyHz());
    const int bin = static_cast<int> (std::floor (maxFreq * static_cast<double> (fftSize) / sr));
    return juce::jlimit (1, scopeSize - 1, bin);
}

float Analyser::getModifiedScopeData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);
    const float dynamicData = m_scopeDynamicData.at (index);
    const float maximumsData = m_scopeMaximumData.at (index);
    return dynamicData - maximumsData;
}

// ============================================================================
void Analyser::pushIntoRing (std::array<float, kRingCapacity>& ring,
                             std::atomic<size_t>& writePosAtom,
                             std::atomic<size_t>& filledAtom,
                             const float* left,
                             const float* right,
                             int numSamples) noexcept
{
    const auto channels = m_activeChannels.load();
    size_t writePos = writePosAtom.load (std::memory_order_relaxed);
    size_t filled = filledAtom.load (std::memory_order_relaxed);

    for (int n = 0; n < numSamples; ++n)
    {
        float dataForAnalysis = 0.0f;

        switch (channels)
        {
            case Channels::left:  dataForAnalysis = left[n]; break;
            case Channels::right: dataForAnalysis = right[n]; break;
            case Channels::both:  dataForAnalysis = (left[n] + right[n]) * 0.5f; break;
            default: break;
        }

        ring[writePos] = dataForAnalysis;
        writePos = (writePos + 1) & kRingMask;

        if (filled < kRingCapacity)
            ++filled;
    }

    writePosAtom.store (writePos, std::memory_order_release);
    filledAtom.store (filled, std::memory_order_release);
}

bool Analyser::copyLatestFromRing (const std::array<float, kRingCapacity>& ring,
                                   const size_t writePos,
                                   const size_t filled,
                                   float* dest,
                                   const size_t fftSize) const
{
    if (dest == nullptr || fftSize == 0 || filled < fftSize)
        return false;

    const size_t start = (writePos + kRingCapacity - fftSize) & kRingMask;

    if (start + fftSize <= kRingCapacity)
    {
        std::copy_n (ring.data() + start, fftSize, dest);
    }
    else
    {
        const size_t first = kRingCapacity - start;
        std::copy_n (ring.data() + start, first, dest);
        std::copy_n (ring.data(), fftSize - first, dest + first);
    }

    std::fill_n (dest + fftSize, fftSize, 0.0f);
    return true;
}

void Analyser::pushSamplesIntoFifo (const float leftChannel, const float rightChannel) noexcept
{
    pushSamplesIntoFifo (&leftChannel, &rightChannel, 1);
}

void Analyser::pushSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept
{
    if (! m_blockSizeDefined.load() || ecoMode.load() || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    pushIntoRing (m_ring, m_ringWritePos, m_ringFilled, left, right, numSamples);
}

void Analyser::pushPreSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept
{
    if (! m_blockSizeDefined.load() || ecoMode.load() || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    pushIntoRing (m_preRing, m_preRingWritePos, m_preRingFilled, left, right, numSamples);
}

bool Analyser::isSpectrumAnalyserParamOn() const
{
    if (auto* p = mr_valueTree.getRawParameterValue ("SPECTRUM_ANALYSER_ID"))
        return p->load() > 0.5f;

    return true;
}

void Analyser::run()
{
    while (! threadShouldExit())
    {
        wait ((juce::uint32) juce::jlimit (16, 200, m_refreshMs.load()));

        if (threadShouldExit())
            break;

        analyseLatestWindow();
    }
}

void Analyser::analyseLatestWindow()
{
    if (ecoMode.load() || ! m_blockSizeDefined.load() || ! isSpectrumAnalyserParamOn())
        return;

    const juce::ScopedLock lock (m_analysisLock);

    if (! m_blockSizeDefined.load())
        return;

    const size_t fftSize = m_fftSize.load();
    if (fftSize == 0 || m_fftData.size() < fftSize * 2)
        return;

    bool posted = false;

    const size_t writePos = m_ringWritePos.load (std::memory_order_acquire);
    const size_t filled = m_ringFilled.load (std::memory_order_acquire);

    if (copyLatestFromRing (m_ring, writePos, filled, m_fftData.data(), fftSize))
    {
        processFFT();
        calculateCurrentVolumes();
        calculateDynamicVolumes();
        calculateMaximumVolumes();

        if (m_volumeRangeIsDynamamic.load())
        {
            calculateCurrentAverageVolume();
            m_dynamicRangeNeedsHostUpdate.store (true);
        }

        publishAdaptedScopes();
        m_nextFFTBlockReady.store (true);
        posted = true;
    }

    if (m_preFftData.size() >= fftSize * 2)
    {
        const size_t preWrite = m_preRingWritePos.load (std::memory_order_acquire);
        const size_t preFilled = m_preRingFilled.load (std::memory_order_acquire);

        if (copyLatestFromRing (m_preRing, preWrite, preFilled, m_preFftData.data(), fftSize))
        {
            processPreFFT();
            calculatePreScopeFromFft();
            m_preNextFFTBlockReady.store (true);
            posted = true;
        }
    }

    juce::ignoreUnused (posted);
}

void Analyser::publishAdaptedScopes()
{
    float minDb = -120.0f;
    float maxDb = 12.0f;
    {
        const juce::ScopedLock lock (m_volumeRangeChange);
        minDb = m_minimumVolumeInDecibels;
        maxDb = m_maximumVolumeInDecibels;
    }

    const size_t n = m_scopeSize.load();
    if (m_volumsDynamicData.size() != n || m_volumsMaximumData.size() != n)
        return;

    const juce::ScopedLock scopeLock (m_scopeLock);

    if (m_scopeDynamicData.size() != n)
        m_scopeDynamicData.resize (n);
    if (m_scopeMaximumData.size() != n)
        m_scopeMaximumData.resize (n);

    for (size_t i = 0; i < n; ++i)
    {
        m_scopeDynamicData[i] = adaptData (m_volumsDynamicData[i], minDb, maxDb);
        m_scopeMaximumData[i] = adaptData (m_volumsMaximumData[i], minDb, maxDb);
    }
}

void Analyser::applyDynamicRangeToHost()
{
    auto* maximum = mr_valueTree.getParameter ("MAXIMUM_ID");
    auto* minumum = mr_valueTree.getParameter ("MINIMUM_ID");

    if (maximum == nullptr || minumum == nullptr)
        return;

    maximum->setValueNotifyingHost (
        juce::jmap (m_currentMaximumVolumeInDecibels.load(), -200.0f, 40.0f, 0.0f, 1.0f));

    minumum->setValueNotifyingHost (
        juce::jmap (m_currentAverageVolumeInDecibels.load(), -380.0f, 30.0f, 0.0f, 1.0f));
}

void Analyser::calculateNextFrameOfSpectrum()
{
    // Analysis thread already produced the frame; message thread only syncs host params.
    const bool hadPost = m_nextFFTBlockReady.exchange (false);
    const bool hadPre = m_preNextFFTBlockReady.exchange (false);

    if ((hadPost || hadPre) && m_dynamicRangeNeedsHostUpdate.exchange (false))
        applyDynamicRangeToHost();
}

// ============================================================================
void Analyser::processFFT()
{
    switch (m_fftSize.load())
    {
        case 512:
            m_window_512.multiplyWithWindowingTable (m_fftData.data(), m_fftSize.load());
            m_forwardFFT_9.performFrequencyOnlyForwardTransform (m_fftData.data());
            break;
        case 1024:
            m_window_1024.multiplyWithWindowingTable (m_fftData.data(), m_fftSize.load());
            m_forwardFFT_10.performFrequencyOnlyForwardTransform (m_fftData.data());
            break;
        case 2048:
            m_window_2048.multiplyWithWindowingTable (m_fftData.data(), m_fftSize.load());
            m_forwardFFT_11.performFrequencyOnlyForwardTransform (m_fftData.data());
            break;
        case 4096:
            m_window_4096.multiplyWithWindowingTable (m_fftData.data(), m_fftSize.load());
            m_forwardFFT_12.performFrequencyOnlyForwardTransform (m_fftData.data());
            break;
        case 8192:
            m_window_8192.multiplyWithWindowingTable (m_fftData.data(), m_fftSize.load());
            m_forwardFFT_13.performFrequencyOnlyForwardTransform (m_fftData.data());
            break;
        case 16384:
            m_window_16384.multiplyWithWindowingTable (m_fftData.data(), m_fftSize.load());
            m_forwardFFT_14.performFrequencyOnlyForwardTransform (m_fftData.data());
            break;
        default:
            DBG ("FFT Size erorr!");
            break;
    }
}

void Analyser::processPreFFT()
{
    switch (m_fftSize.load())
    {
        case 512:
            m_window_512.multiplyWithWindowingTable (m_preFftData.data(), m_fftSize.load());
            m_forwardFFT_9.performFrequencyOnlyForwardTransform (m_preFftData.data());
            break;
        case 1024:
            m_window_1024.multiplyWithWindowingTable (m_preFftData.data(), m_fftSize.load());
            m_forwardFFT_10.performFrequencyOnlyForwardTransform (m_preFftData.data());
            break;
        case 2048:
            m_window_2048.multiplyWithWindowingTable (m_preFftData.data(), m_fftSize.load());
            m_forwardFFT_11.performFrequencyOnlyForwardTransform (m_preFftData.data());
            break;
        case 4096:
            m_window_4096.multiplyWithWindowingTable (m_preFftData.data(), m_fftSize.load());
            m_forwardFFT_12.performFrequencyOnlyForwardTransform (m_preFftData.data());
            break;
        case 8192:
            m_window_8192.multiplyWithWindowingTable (m_preFftData.data(), m_fftSize.load());
            m_forwardFFT_13.performFrequencyOnlyForwardTransform (m_preFftData.data());
            break;
        case 16384:
            m_window_16384.multiplyWithWindowingTable (m_preFftData.data(), m_fftSize.load());
            m_forwardFFT_14.performFrequencyOnlyForwardTransform (m_preFftData.data());
            break;
        default:
            break;
    }
}

void Analyser::calculatePreScopeFromFft()
{
    const auto scopeSize = m_scopeSize.load();
    const auto offset = juce::Decibels::gainToDecibels (static_cast<float> (scopeSize));

    if (scopeSize == 0)
        return;

    if (m_volumsPreData.size() != scopeSize)
        m_volumsPreData.resize (scopeSize, m_minimumVolumeInDecibels);

    float minDb = -120.0f;
    float maxDb = 12.0f;
    {
        const juce::ScopedLock lock (m_volumeRangeChange);
        minDb = m_minimumVolumeInDecibels;
        maxDb = m_maximumVolumeInDecibels;
    }

    const auto bins = juce::jmin (scopeSize, m_preFftData.size());
    for (size_t i = 0; i < bins; ++i)
        m_volumsPreData[i] = juce::Decibels::gainToDecibels (m_preFftData[i]) - offset;

    for (size_t i = bins; i < scopeSize; ++i)
        m_volumsPreData[i] = m_minimumVolumeInDecibels;

    const juce::ScopedLock scopeLock (m_scopeLock);

    if (m_scopePreData.size() != scopeSize)
        m_scopePreData.resize (scopeSize, 0.0f);

    for (size_t i = 0; i < scopeSize; ++i)
        m_scopePreData[i] = adaptData (m_volumsPreData[i], minDb, maxDb);
}

// ============================================================================
void Analyser::calculateCurrentVolumes()
{
    const auto scopeSize = m_scopeSize.load();
    const auto offset = juce::Decibels::gainToDecibels (static_cast<float> (scopeSize));
    const auto writePtr = m_avgFifoWritePointer.load();
    auto& avgSlot = m_avgData[writePtr];

    if (avgSlot.size() != scopeSize)
        avgSlot.resize (scopeSize);

    const auto bins = juce::jmin (scopeSize, m_fftData.size());
    for (size_t i = 0; i < bins; ++i)
        avgSlot[i] = juce::Decibels::gainToDecibels (m_fftData[i]) - offset;

    if (m_avgFifoSize.load() != m_avgFactor.load())
        ++m_avgFifoSize;

    m_avgFifoReadPointer = writePtr;
    m_avgFifoWritePointer.store ((writePtr + 1) % m_avgFifoMaximumSize);
}

void Analyser::calculateDynamicVolumes()
{
    const auto scopeSize = m_scopeSize.load();
    auto avgSize = m_avgFifoSize.load();

    if (avgSize == 0 || scopeSize == 0)
        return;

    if (m_volumsDynamicData.size() != scopeSize)
        m_volumsDynamicData.resize (scopeSize, m_minimumVolumeInDecibels);

    for (size_t data = 0; data < scopeSize; ++data)
    {
        float sum = 0.0f;
        auto tempReadPointer = m_avgFifoReadPointer;
        size_t counted = 0;

        for (size_t counter = avgSize; counter > 0; --counter)
        {
            const auto& slot = m_avgData[tempReadPointer];
            if (data < slot.size())
            {
                sum += slot[data];
                ++counted;
            }

            tempReadPointer = (tempReadPointer == 0) ? m_avgFifoMaximumSize - 1 : tempReadPointer - 1;
        }

        m_volumsDynamicData[data] = (counted > 0) ? (sum / static_cast<float> (counted))
                                                  : m_minimumVolumeInDecibels;
    }
}

void Analyser::calculateMaximumVolumes()
{
    const float floorDb = m_minimumVolumeInDecibels;
    const float holdSeconds = juce::jmax (
        0.05f,
        mr_valueTree.getRawParameterValue ("MAX_HOLD_ID") != nullptr
            ? mr_valueTree.getRawParameterValue ("MAX_HOLD_ID")->load()
            : 4.0f);

    const double nowMs = juce::Time::getMillisecondCounterHiRes();
    float dtSeconds = 0.06f;

    if (m_lastMaxHoldUpdateMs > 0.0)
    {
        dtSeconds = (float) ((nowMs - m_lastMaxHoldUpdateMs) * 0.001);
        dtSeconds = juce::jlimit (0.0f, 0.25f, dtSeconds);
    }

    m_lastMaxHoldUpdateMs = nowMs;

    const float decayDbPerSecond = (m_maximumVolumeInDecibels - floorDb) / holdSeconds;
    const float decayThisFrame = decayDbPerSecond * dtSeconds;

    float framePeak = floorDb;
    const auto scopeSize = m_scopeSize.load();

    if (m_volumsDynamicData.size() != scopeSize)
        return;

    if (m_volumsMaximumData.size() != scopeSize)
        m_volumsMaximumData.assign (scopeSize, floorDb);

    for (size_t i = 0; i < scopeSize; ++i)
    {
        const float dynamicDb = m_volumsDynamicData[i];
        float& maxDb = m_volumsMaximumData[i];

        if (dynamicDb > maxDb)
            maxDb = dynamicDb;
        else
            maxDb = juce::jmax (floorDb, maxDb - decayThisFrame);

        framePeak = juce::jmax (framePeak, maxDb);
    }

    m_currentMaximumVolumeInDecibels.store (framePeak);
}

void Analyser::calculateCurrentAverageVolume()
{
    m_currentAverageVolumeInDecibels.store (
        m_currentMaximumVolumeInDecibels.load() - 10.0f);

    for (const auto& volume : m_volumsDynamicData)
    {
        m_currentAverageVolumeInDecibels.store (
            m_currentAverageVolumeInDecibels.load() + volume);
    }

    {
        const juce::ScopedLock lock (m_volumeRangeChange);

        m_currentAverageVolumeInDecibels.store (
            m_currentAverageVolumeInDecibels.load() / static_cast<float> (m_scopeSize.load()));
    }
}

float Analyser::adaptData (const float volume, float minDb, float maxDb) const
{
    return juce::jmap (volume, minDb, maxDb, 0.0f, 1.0f);
}

// ============================================================================
void Analyser::parameterChanged (const juce::String& parameterID, float newValue)
{
    if (parameterID == "BLOCK_ID")
    {
        int blockIndex = 0;
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (mr_valueTree.getParameter ("BLOCK_ID")))
            blockIndex = choice->getIndex();
        else if (auto* p = mr_valueTree.getRawParameterValue ("BLOCK_ID"))
            blockIndex = juce::jlimit (0, 3, (int) std::lround (p->load()));
        else
            blockIndex = juce::jlimit (0, 3, (int) std::lround (newValue));

        setFFTBlockSize (blockIndex);
    }
    else if (parameterID == "REFRESH_ID")
    {
        setRefreshMs ((int) std::lround (newValue));
    }
    else if (parameterID == "LEFT_ID")
    {
        setActiveChannel (Channels::left);
    }
    else if (parameterID == "RIGHT_ID")
    {
        setActiveChannel (Channels::right);
    }
    else if (parameterID == "BOTH_ID")
    {
        setActiveChannel (Channels::both);
    }
    else if (parameterID == "AVG_ID")
    {
        setAvg (static_cast<size_t> (newValue));
    }
    else if (parameterID == "RANGE_ID")
    {
        setVolumeScaleModeAsDynamic (! static_cast<bool> (newValue));
    }
    else if (parameterID == "MAXIMUM_ID" || parameterID == "MINIMUM_ID")
    {
        auto maximum = mr_valueTree.getRawParameterValue ("MAXIMUM_ID")->load();
        auto minimum = mr_valueTree.getRawParameterValue ("MINIMUM_ID")->load();

        if (maximum - 10.0f < minimum)
            minimum = maximum - 10.0f;

        setVolumeRangeInDecibels (maximum, minimum);
    }
}

void Analyser::setRefreshMs (const int milliseconds)
{
    m_refreshMs.store (juce::jlimit (16, 200, milliseconds));
    notify(); // wake analysis wait so new rate applies promptly
}

// ============================================================================
void Analyser::setFFTBlockSize (const int blockMenuIndex)
{
    const juce::ScopedLock lock (m_analysisLock);

    m_blockSizeDefined.store (false);

    const int index = juce::jlimit (0, 3, blockMenuIndex);

    switch (index)
    {
        case 0: m_fftOrder.store (11); m_offset.store (1.0f); break; // 2048
        case 1: m_fftOrder.store (12); m_offset.store (2.0f); break; // 4096
        case 2: m_fftOrder.store (13); m_offset.store (4.0f); break; // 8192
        case 3: m_fftOrder.store (14); m_offset.store (8.0f); break; // 16384
        default:
            m_fftOrder.store (11);
            m_offset.store (1.0f);
            break;
    }

    m_fftSize.store (size_t { 1 } << m_fftOrder.load());
    m_scopeSize.store (m_fftSize.load() / 2);

    m_ringWritePos.store (0);
    m_ringFilled.store (0);
    m_preRingWritePos.store (0);
    m_preRingFilled.store (0);
    m_ring.fill (0.0f);
    m_preRing.fill (0.0f);

    m_fftData.resize (m_fftSize.load() * 2);
    m_preFftData.resize (m_fftSize.load() * 2);
    m_nextFFTBlockReady.store (false);
    m_preNextFFTBlockReady.store (false);
    m_dynamicRangeNeedsHostUpdate.store (false);

    {
        const juce::ScopedLock rangeLock (m_volumeRangeChange);
        m_volumsDynamicData.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsMaximumData.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsPreData.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        for (auto& vector : m_avgData)
            vector.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
    }

    {
        const juce::ScopedLock scopeLock (m_scopeLock);
        m_scopeDynamicData.assign (m_scopeSize.load(), 0.0f);
        m_scopeMaximumData.assign (m_scopeSize.load(), 0.0f);
        m_scopePreData.assign (m_scopeSize.load(), 0.0f);
    }

    m_avgFifoSize.store (0);
    m_avgFifoWritePointer.store (0);

    m_currentMaximumVolumeInDecibels.store (m_minimumVolumeInDecibels);
    m_currentAverageVolumeInDecibels.store (m_minimumVolumeInDecibels);
    m_lastMaxHoldUpdateMs = 0.0;

    m_blockSizeDefined.store (true);
}

void Analyser::setActiveChannel (Channels channels)
{
    m_activeChannels.store (channels);
}

void Analyser::setAvg (size_t avg)
{
    m_avgFactor.store (avg);
    m_avgFifoSize.store (avg);
}

void Analyser::setVolumeScaleModeAsDynamic (bool isDynamamic)
{
    m_volumeRangeIsDynamamic.store (isDynamamic);
    resetScopeMaximumsData();
}

void Analyser::setVolumeRangeInDecibels (float maximum, float minimum)
{
    const juce::ScopedLock lock (m_volumeRangeChange);

    m_maximumVolumeInDecibels = maximum;
    m_minimumVolumeInDecibels = minimum;
}
