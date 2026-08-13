#include "Analyser.h"
#include <JuceHeader.h>
#include <algorithm>
#include <cmath>

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
    mr_valueTree.addParameterListener (SpectrumAnalysis::channelParamId(), this);
    mr_valueTree.addParameterListener (SpectrumAnalysis::octaveSmoothParamId(), this);
    mr_valueTree.addParameterListener ("AVG_ID", this);
    mr_valueTree.addParameterListener ("RANGE_ID", this);
    mr_valueTree.addParameterListener ("MAXIMUM_ID", this);
    mr_valueTree.addParameterListener ("MINIMUM_ID", this);
    mr_valueTree.addParameterListener ("REFRESH_ID", this);

    readChannelParamsFromTree();
    readOctaveSmoothFromTree();

    startThread (juce::Thread::Priority::low);
}

Analyser::~Analyser()
{
    stopThread (2000);

    mr_valueTree.removeParameterListener ("BLOCK_ID", this);
    mr_valueTree.removeParameterListener ("LEFT_ID", this);
    mr_valueTree.removeParameterListener ("RIGHT_ID", this);
    mr_valueTree.removeParameterListener ("BOTH_ID", this);
    mr_valueTree.removeParameterListener (SpectrumAnalysis::channelParamId(), this);
    mr_valueTree.removeParameterListener (SpectrumAnalysis::octaveSmoothParamId(), this);
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

void Analyser::getVolumeRangeInDecibels (float& minDb, float& maxDb) noexcept
{
    const juce::ScopedLock lock (m_volumeRangeChange);
    minDb = m_minimumVolumeInDecibels;
    maxDb = m_maximumVolumeInDecibels;
}

float Analyser::getScopeDataDb (size_t index)
{
    float minDb = -120.0f, maxDb = 12.0f;
    getVolumeRangeInDecibels (minDb, maxDb);
    const float n = getScopeData (index);
    return juce::jmap (juce::jlimit (0.0f, 1.0f, n), 0.0f, 1.0f, minDb, maxDb);
}

float Analyser::getScopePreDataDb (size_t index)
{
    float minDb = -120.0f, maxDb = 12.0f;
    getVolumeRangeInDecibels (minDb, maxDb);
    const float n = getScopePreData (index);
    return juce::jmap (juce::jlimit (0.0f, 1.0f, n), 0.0f, 1.0f, minDb, maxDb);
}

int Analyser::copyScopeDataDb (bool pre, float* destDb, int maxBins)
{
    if (destDb == nullptr || maxBins <= 0)
        return 0;

    // Message-thread safe: only m_scopeLock (never m_analysisLock).
    // Taking analysisLock here deadlocks / freezes Ableton when Spec/UI paints
    // while the analyser thread holds analysis for a large FFT.
    float minDb = -120.0f, maxDb = 12.0f;
    getVolumeRangeInDecibels (minDb, maxDb);

    const juce::ScopedLock lock (m_scopeLock);
    const auto& src = pre ? m_scopePreData : m_scopeDynamicData;
    const int n = juce::jmin (maxBins, (int) src.size());
    for (int i = 0; i < n; ++i)
    {
        const float norm = juce::jlimit (0.0f, 1.0f, src[(size_t) i]);
        destDb[i] = juce::jmap (norm, 0.0f, 1.0f, minDb, maxDb);
    }
    return n;
}

float Analyser::getScopePeakDb (bool pre) noexcept
{
    float minDb = -120.0f, maxDb = 12.0f;
    getVolumeRangeInDecibels (minDb, maxDb);

    const juce::ScopedLock lock (m_scopeLock);
    const auto& src = pre ? m_scopePreData : m_scopeDynamicData;
    if (src.empty())
        return -200.0f;

    float peakN = 0.0f;
    for (float n : src)
        peakN = juce::jmax (peakN, n);
    return juce::jmap (juce::jlimit (0.0f, 1.0f, peakN), 0.0f, 1.0f, minDb, maxDb);
}

float Analyser::getScopeMaximumsData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopeMaximumData.size())
        return 0.0f;

    return m_scopeMaximumData[index];
}

float Analyser::getScopeSecondaryData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopeDynamicDataB.size())
        return 0.0f;

    return m_scopeDynamicDataB[index];
}

float Analyser::getScopeSecondaryPreData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopePreDataB.size())
        return 0.0f;

    return m_scopePreDataB[index];
}

float Analyser::getScopeSecondaryMaximumsData (size_t index)
{
    const juce::ScopedLock lock (m_scopeLock);

    if (index >= m_scopeMaximumDataB.size())
        return 0.0f;

    return m_scopeMaximumDataB[index];
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
void Analyser::pushIntoStereoRings (std::array<float, kRingCapacity>& ringL,
                                    std::array<float, kRingCapacity>& ringR,
                                    std::atomic<size_t>& writePosAtom,
                                    std::atomic<size_t>& filledAtom,
                                    const float* left,
                                    const float* right,
                                    int numSamples) noexcept
{
    size_t writePos = writePosAtom.load (std::memory_order_relaxed);
    size_t filled = filledAtom.load (std::memory_order_relaxed);

    for (int n = 0; n < numSamples; ++n)
    {
        ringL[writePos] = left[n];
        ringR[writePos] = right[n];
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
    if (! m_blockSizeDefined.load() || ecoMode.load()
        || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    pushIntoStereoRings (m_ringL, m_ringR, m_ringWritePos, m_ringFilled, left, right, numSamples);
}

void Analyser::pushPreSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept
{
    if (! m_blockSizeDefined.load() || ecoMode.load()
        || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    pushIntoStereoRings (m_preRingL, m_preRingR, m_preRingWritePos, m_preRingFilled, left, right, numSamples);
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
    if (fftSize == 0
        || m_fftData.size() < fftSize * 2
        || m_timeL.size() < fftSize * 2
        || m_timeR.size() < fftSize * 2)
        return;

    const auto channel = m_activeChannels.load();
    const bool overlay = SpectrumAnalysis::isOverlay (channel);
    const auto primaryMix = primaryMixFor (channel);
    const auto secondaryMix = secondaryMixFor (channel);

    bool posted = false;

    const size_t writePos = m_ringWritePos.load (std::memory_order_acquire);
    const size_t filled = m_ringFilled.load (std::memory_order_acquire);

    const bool haveL = copyLatestFromRing (m_ringL, writePos, filled, m_timeL.data(), fftSize);
    const bool haveR = copyLatestFromRing (m_ringR, writePos, filled, m_timeR.data(), fftSize);

    if (haveL && haveR)
    {
        mixTimeDomain (m_timeL.data(), m_timeR.data(), m_fftData.data(), fftSize, primaryMix);
        performFrequencyTransform (m_fftData.data());
        calculateVolumesFromFft (m_fftData, m_avgData);

        if (overlay && m_fftDataB.size() >= fftSize * 2)
        {
            mixTimeDomain (m_timeL.data(), m_timeR.data(), m_fftDataB.data(), fftSize, secondaryMix);
            performFrequencyTransform (m_fftDataB.data());
            calculateVolumesFromFft (m_fftDataB, m_avgDataB);
        }

        if (m_avgFifoSize.load() != m_avgFactor.load())
            ++m_avgFifoSize;

        {
            const auto writePtr = m_avgFifoWritePointer.load();
            m_avgFifoReadPointer = writePtr;
            m_avgFifoWritePointer.store ((writePtr + 1) % m_avgFifoMaximumSize);
        }

        calculateAveragedVolumes (m_avgData, m_volumsDynamicData);
        applyOctaveSmoothing (m_volumsDynamicData);
        calculateHoldVolumes (m_volumsDynamicData, m_volumsMaximumData, &m_currentMaximumVolumeInDecibels);

        if (overlay)
        {
            calculateAveragedVolumes (m_avgDataB, m_volumsDynamicDataB);
            applyOctaveSmoothing (m_volumsDynamicDataB);
            calculateHoldVolumes (m_volumsDynamicDataB, m_volumsMaximumDataB, nullptr);
        }

        if (m_volumeRangeIsDynamamic.load())
        {
            calculateCurrentAverageVolume();
            m_dynamicRangeNeedsHostUpdate.store (true);
        }

        publishAdaptedScopes();
        m_nextFFTBlockReady.store (true);
        posted = true;
    }

    if (m_preFftData.size() >= fftSize * 2
        && m_timeL.size() >= fftSize * 2
        && m_timeR.size() >= fftSize * 2)
    {
        const size_t preWrite = m_preRingWritePos.load (std::memory_order_acquire);
        const size_t preFilled = m_preRingFilled.load (std::memory_order_acquire);

        const bool havePreL = copyLatestFromRing (m_preRingL, preWrite, preFilled, m_timeL.data(), fftSize);
        const bool havePreR = copyLatestFromRing (m_preRingR, preWrite, preFilled, m_timeR.data(), fftSize);

        if (havePreL && havePreR)
        {
            mixTimeDomain (m_timeL.data(), m_timeR.data(), m_preFftData.data(), fftSize, primaryMix);
            performFrequencyTransform (m_preFftData.data());

            if (overlay && m_preFftDataB.size() >= fftSize * 2)
            {
                mixTimeDomain (m_timeL.data(), m_timeR.data(), m_preFftDataB.data(), fftSize, secondaryMix);
                performFrequencyTransform (m_preFftDataB.data());
            }

            calculatePreScopeFromFft();
            m_preNextFFTBlockReady.store (true);
            posted = true;
        }
    }

    juce::ignoreUnused (posted);
}

void Analyser::publishAdaptedPair (const std::vector<float>& srcDb, std::vector<float>& destNorm,
                                   float minDb, float maxDb)
{
    const size_t n = srcDb.size();
    if (destNorm.size() != n)
        destNorm.resize (n);

    for (size_t i = 0; i < n; ++i)
        destNorm[i] = adaptData (srcDb[i], minDb, maxDb);
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

    publishAdaptedPair (m_volumsDynamicData, m_scopeDynamicData, minDb, maxDb);
    publishAdaptedPair (m_volumsMaximumData, m_scopeMaximumData, minDb, maxDb);

    if (SpectrumAnalysis::isOverlay (m_activeChannels.load())
        && m_volumsDynamicDataB.size() == n
        && m_volumsMaximumDataB.size() == n)
    {
        publishAdaptedPair (m_volumsDynamicDataB, m_scopeDynamicDataB, minDb, maxDb);
        publishAdaptedPair (m_volumsMaximumDataB, m_scopeMaximumDataB, minDb, maxDb);
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
void Analyser::performFrequencyTransform (float* data)
{
    if (data == nullptr)
        return;

    const auto fftSize = m_fftSize.load();

    switch (fftSize)
    {
        case 512:
            m_window_512.multiplyWithWindowingTable (data, fftSize);
            m_forwardFFT_9.performFrequencyOnlyForwardTransform (data);
            break;
        case 1024:
            m_window_1024.multiplyWithWindowingTable (data, fftSize);
            m_forwardFFT_10.performFrequencyOnlyForwardTransform (data);
            break;
        case 2048:
            m_window_2048.multiplyWithWindowingTable (data, fftSize);
            m_forwardFFT_11.performFrequencyOnlyForwardTransform (data);
            break;
        case 4096:
            m_window_4096.multiplyWithWindowingTable (data, fftSize);
            m_forwardFFT_12.performFrequencyOnlyForwardTransform (data);
            break;
        case 8192:
            m_window_8192.multiplyWithWindowingTable (data, fftSize);
            m_forwardFFT_13.performFrequencyOnlyForwardTransform (data);
            break;
        case 16384:
            m_window_16384.multiplyWithWindowingTable (data, fftSize);
            m_forwardFFT_14.performFrequencyOnlyForwardTransform (data);
            break;
        default:
            DBG ("FFT Size erorr!");
            break;
    }
}

void Analyser::processFFT()
{
    performFrequencyTransform (m_fftData.data());
}

void Analyser::processPreFFT()
{
    performFrequencyTransform (m_preFftData.data());
}

void Analyser::calculatePreScopeFromFft()
{
    const auto scopeSize = m_scopeSize.load();
    const auto offset = juce::Decibels::gainToDecibels (static_cast<float> (scopeSize));

    if (scopeSize == 0)
        return;

    auto fillPreDb = [scopeSize, offset, this] (const std::vector<float>& fft, std::vector<float>& destDb)
    {
        if (destDb.size() != scopeSize)
            destDb.resize (scopeSize, m_minimumVolumeInDecibels);

        const auto bins = juce::jmin (scopeSize, fft.size());
        for (size_t i = 0; i < bins; ++i)
            destDb[i] = juce::Decibels::gainToDecibels (fft[i]) - offset;

        for (size_t i = bins; i < scopeSize; ++i)
            destDb[i] = m_minimumVolumeInDecibels;

        applyOctaveSmoothing (destDb);
    };

    fillPreDb (m_preFftData, m_volumsPreData);

    const bool overlay = SpectrumAnalysis::isOverlay (m_activeChannels.load());
    if (overlay)
        fillPreDb (m_preFftDataB, m_volumsPreDataB);

    float minDb = -120.0f;
    float maxDb = 12.0f;
    {
        const juce::ScopedLock lock (m_volumeRangeChange);
        minDb = m_minimumVolumeInDecibels;
        maxDb = m_maximumVolumeInDecibels;
    }

    const juce::ScopedLock scopeLock (m_scopeLock);
    publishAdaptedPair (m_volumsPreData, m_scopePreData, minDb, maxDb);

    if (overlay)
        publishAdaptedPair (m_volumsPreDataB, m_scopePreDataB, minDb, maxDb);
}

// ============================================================================
void Analyser::calculateVolumesFromFft (const std::vector<float>& fftData,
                                        std::array<std::vector<float>, m_avgFifoMaximumSize>& avgData)
{
    const auto scopeSize = m_scopeSize.load();
    const auto offset = juce::Decibels::gainToDecibels (static_cast<float> (scopeSize));
    const auto writePtr = m_avgFifoWritePointer.load();
    auto& avgSlot = avgData[writePtr];

    if (avgSlot.size() != scopeSize)
        avgSlot.resize (scopeSize);

    const auto bins = juce::jmin (scopeSize, fftData.size());
    for (size_t i = 0; i < bins; ++i)
        avgSlot[i] = juce::Decibels::gainToDecibels (fftData[i]) - offset;

    for (size_t i = bins; i < scopeSize; ++i)
        avgSlot[i] = m_minimumVolumeInDecibels;
}

void Analyser::calculateCurrentVolumes()
{
    calculateVolumesFromFft (m_fftData, m_avgData);

    if (m_avgFifoSize.load() != m_avgFactor.load())
        ++m_avgFifoSize;

    const auto writePtr = m_avgFifoWritePointer.load();
    m_avgFifoReadPointer = writePtr;
    m_avgFifoWritePointer.store ((writePtr + 1) % m_avgFifoMaximumSize);
}

void Analyser::calculateAveragedVolumes (const std::array<std::vector<float>, m_avgFifoMaximumSize>& avgData,
                                         std::vector<float>& destDb)
{
    const auto scopeSize = m_scopeSize.load();
    auto avgSize = m_avgFifoSize.load();

    if (avgSize == 0 || scopeSize == 0)
        return;

    if (destDb.size() != scopeSize)
        destDb.resize (scopeSize, m_minimumVolumeInDecibels);

    for (size_t data = 0; data < scopeSize; ++data)
    {
        float sum = 0.0f;
        auto tempReadPointer = m_avgFifoReadPointer;
        size_t counted = 0;

        for (size_t counter = avgSize; counter > 0; --counter)
        {
            const auto& slot = avgData[tempReadPointer];
            if (data < slot.size())
            {
                sum += slot[data];
                ++counted;
            }

            tempReadPointer = (tempReadPointer == 0) ? m_avgFifoMaximumSize - 1 : tempReadPointer - 1;
        }

        destDb[data] = (counted > 0) ? (sum / static_cast<float> (counted))
                                     : m_minimumVolumeInDecibels;
    }
}

void Analyser::calculateDynamicVolumes()
{
    calculateAveragedVolumes (m_avgData, m_volumsDynamicData);
}

void Analyser::calculateHoldVolumes (const std::vector<float>& dynamicDb,
                                     std::vector<float>& holdDb,
                                     std::atomic<float>* peakOut)
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

    // Shared dt for both overlay layers — only the first (primary) call advances time.
    if (peakOut != nullptr)
        m_lastMaxHoldUpdateMs = nowMs;

    const float decayDbPerSecond = (m_maximumVolumeInDecibels - floorDb) / holdSeconds;
    const float decayThisFrame = decayDbPerSecond * dtSeconds;

    float framePeak = floorDb;
    const auto scopeSize = m_scopeSize.load();

    if (dynamicDb.size() != scopeSize)
        return;

    if (holdDb.size() != scopeSize)
        holdDb.assign (scopeSize, floorDb);

    for (size_t i = 0; i < scopeSize; ++i)
    {
        const float dyn = dynamicDb[i];
        float& maxDb = holdDb[i];

        if (dyn > maxDb)
            maxDb = dyn;
        else
            maxDb = juce::jmax (floorDb, maxDb - decayThisFrame);

        framePeak = juce::jmax (framePeak, maxDb);
    }

    if (peakOut != nullptr)
        peakOut->store (framePeak);
}

void Analyser::calculateMaximumVolumes()
{
    calculateHoldVolumes (m_volumsDynamicData, m_volumsMaximumData, &m_currentMaximumVolumeInDecibels);
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
    else if (parameterID == SpectrumAnalysis::channelParamId())
    {
        if (! m_syncingChannelParams.load())
        {
            int index = (int) std::lround (newValue);
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    mr_valueTree.getParameter (SpectrumAnalysis::channelParamId())))
                index = choice->getIndex();
            applyChannelFromChoiceIndex (index);
        }
    }
    else if (parameterID == SpectrumAnalysis::octaveSmoothParamId())
    {
        int index = (int) std::lround (newValue);
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                mr_valueTree.getParameter (SpectrumAnalysis::octaveSmoothParamId())))
            index = choice->getIndex();
        m_octaveSmoothOctaves.store (SpectrumAnalysis::octaveWidthFromIndex (index));
    }
    else if (parameterID == "LEFT_ID")
    {
        if (newValue > 0.5f && ! m_syncingChannelParams.load())
            applyChannelFromChoiceIndex ((int) Channels::left);
    }
    else if (parameterID == "RIGHT_ID")
    {
        if (newValue > 0.5f && ! m_syncingChannelParams.load())
            applyChannelFromChoiceIndex ((int) Channels::right);
    }
    else if (parameterID == "BOTH_ID")
    {
        if (newValue > 0.5f && ! m_syncingChannelParams.load())
            applyChannelFromChoiceIndex ((int) Channels::both);
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
    m_ringL.fill (0.0f);
    m_ringR.fill (0.0f);
    m_preRingL.fill (0.0f);
    m_preRingR.fill (0.0f);

    const auto fftBytes = m_fftSize.load() * 2;
    m_timeL.assign (fftBytes, 0.0f);
    m_timeR.assign (fftBytes, 0.0f);
    m_fftData.assign (fftBytes, 0.0f);
    m_fftDataB.assign (fftBytes, 0.0f);
    m_preFftData.assign (fftBytes, 0.0f);
    m_preFftDataB.assign (fftBytes, 0.0f);
    m_nextFFTBlockReady.store (false);
    m_preNextFFTBlockReady.store (false);
    m_dynamicRangeNeedsHostUpdate.store (false);

    {
        const juce::ScopedLock rangeLock (m_volumeRangeChange);
        m_volumsDynamicData.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsDynamicDataB.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsMaximumData.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsMaximumDataB.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsPreData.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_volumsPreDataB.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_octaveSmoothScratch.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        m_octavePrefix.assign (m_scopeSize.load() + 1, 0.0);
        for (auto& vector : m_avgData)
            vector.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
        for (auto& vector : m_avgDataB)
            vector.assign (m_scopeSize.load(), m_minimumVolumeInDecibels);
    }

    {
        const juce::ScopedLock scopeLock (m_scopeLock);
        m_scopeDynamicData.assign (m_scopeSize.load(), 0.0f);
        m_scopeDynamicDataB.assign (m_scopeSize.load(), 0.0f);
        m_scopeMaximumData.assign (m_scopeSize.load(), 0.0f);
        m_scopeMaximumDataB.assign (m_scopeSize.load(), 0.0f);
        m_scopePreData.assign (m_scopeSize.load(), 0.0f);
        m_scopePreDataB.assign (m_scopeSize.load(), 0.0f);
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

Analyser::Channels Analyser::primaryMixFor (Channels channel) const noexcept
{
    switch (channel)
    {
        case Channels::left:
        case Channels::leftAndRight: return Channels::left;
        case Channels::right:        return Channels::right;
        case Channels::side:         return Channels::side;
        case Channels::mid:
        case Channels::midAndSide:
        case Channels::both:
        default:                     return Channels::mid;
    }
}

Analyser::Channels Analyser::secondaryMixFor (Channels channel) const noexcept
{
    if (channel == Channels::leftAndRight)
        return Channels::right;

    return Channels::side;
}

void Analyser::mixTimeDomain (const float* left,
                              const float* right,
                              float* dest,
                              size_t numSamples,
                              Channels mix) const noexcept
{
    if (dest == nullptr || numSamples == 0)
        return;

    switch (mix)
    {
        case Channels::left:
            if (left != nullptr)
                std::copy_n (left, numSamples, dest);
            else
                std::fill_n (dest, numSamples, 0.0f);
            break;

        case Channels::right:
            if (right != nullptr)
                std::copy_n (right, numSamples, dest);
            else
                std::fill_n (dest, numSamples, 0.0f);
            break;

        case Channels::side:
            if (left != nullptr && right != nullptr)
            {
                for (size_t i = 0; i < numSamples; ++i)
                    dest[i] = (left[i] - right[i]) * 0.5f;
            }
            else
            {
                std::fill_n (dest, numSamples, 0.0f);
            }
            break;

        case Channels::mid:
        case Channels::both:
        case Channels::midAndSide:
        case Channels::leftAndRight:
        default:
            if (left != nullptr && right != nullptr)
            {
                for (size_t i = 0; i < numSamples; ++i)
                    dest[i] = (left[i] + right[i]) * 0.5f;
            }
            else if (left != nullptr)
            {
                std::copy_n (left, numSamples, dest);
            }
            else
            {
                std::fill_n (dest, numSamples, 0.0f);
            }
            break;
    }

    std::fill_n (dest + numSamples, numSamples, 0.0f);
}

void Analyser::applyOctaveSmoothing (std::vector<float>& db)
{
    const float octaves = m_octaveSmoothOctaves.load();
    const int n = (int) db.size();
    if (n < 3 || octaves <= 0.01f)
        return;

    const float halfOct = octaves * 0.5f;
    const float ratio = std::pow (2.0f, halfOct);

    if ((int) m_octavePrefix.size() != n + 1)
        m_octavePrefix.assign ((size_t) n + 1, 0.0);
    if ((int) m_octaveSmoothScratch.size() != n)
        m_octaveSmoothScratch.resize ((size_t) n);

    m_octavePrefix[0] = 0.0;
    for (int i = 0; i < n; ++i)
        m_octavePrefix[(size_t) i + 1] = m_octavePrefix[(size_t) i] + (double) db[(size_t) i];

    m_octaveSmoothScratch[0] = db[0];

    for (int i = 1; i < n; ++i)
    {
        const float iF = (float) i;
        const int lo = juce::jlimit (1, n - 1, (int) std::floor ((double) (iF / ratio)));
        const int hi = juce::jlimit (1, n - 1, (int) std::ceil  ((double) (iF * ratio)));
        const int a = juce::jmin (lo, hi);
        const int b = juce::jmax (lo, hi);
        const double sum = m_octavePrefix[(size_t) b + 1] - m_octavePrefix[(size_t) a];
        const int count = b - a + 1;
        m_octaveSmoothScratch[(size_t) i] = (count > 0) ? (float) (sum / (double) count) : db[(size_t) i];
    }

    db.swap (m_octaveSmoothScratch);
}

void Analyser::applyChannelFromChoiceIndex (int index)
{
    const auto channel = SpectrumAnalysis::channelFromIndex (index);
    m_activeChannels.store (channel);

    if (m_syncingChannelParams.exchange (true))
        return;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            mr_valueTree.getParameter (SpectrumAnalysis::channelParamId())))
    {
        if (choice->getIndex() != (int) channel)
            choice->setValueNotifyingHost (choice->convertTo0to1 ((float) (int) channel));
    }

    auto setBool = [this] (const char* id, bool on)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (mr_valueTree.getParameter (id)))
            if (p->get() != on)
                p->setValueNotifyingHost (on ? 1.0f : 0.0f);
    };

    setBool ("LEFT_ID",  channel == Channels::left);
    setBool ("RIGHT_ID", channel == Channels::right);
    setBool ("BOTH_ID",  channel == Channels::both);

    m_syncingChannelParams.store (false);
}

void Analyser::readChannelParamsFromTree()
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            mr_valueTree.getParameter (SpectrumAnalysis::channelParamId())))
    {
        m_activeChannels.store (SpectrumAnalysis::channelFromIndex (choice->getIndex()));
        return;
    }

    if (auto* left = mr_valueTree.getRawParameterValue ("LEFT_ID");
        left != nullptr && left->load() > 0.5f)
    {
        m_activeChannels.store (Channels::left);
        return;
    }

    if (auto* right = mr_valueTree.getRawParameterValue ("RIGHT_ID");
        right != nullptr && right->load() > 0.5f)
    {
        m_activeChannels.store (Channels::right);
        return;
    }

    m_activeChannels.store (Channels::both);
}

void Analyser::readOctaveSmoothFromTree()
{
    int index = (int) SpectrumAnalysis::OctaveSmooth::off;

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
            mr_valueTree.getParameter (SpectrumAnalysis::octaveSmoothParamId())))
        index = choice->getIndex();
    else if (auto* raw = mr_valueTree.getRawParameterValue (SpectrumAnalysis::octaveSmoothParamId()))
        index = (int) std::lround (raw->load());

    m_octaveSmoothOctaves.store (SpectrumAnalysis::octaveWidthFromIndex (index));
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
