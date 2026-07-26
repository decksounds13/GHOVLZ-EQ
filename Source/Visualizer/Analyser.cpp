


#include "Analyser.h"
#include <JuceHeader.h>

Analyser::Analyser(juce::AudioProcessorValueTreeState& audioProcessorValueTreeState)
    : mr_valueTree(audioProcessorValueTreeState),
    m_forwardFFT_9(9),
    m_forwardFFT_10(10),
    m_forwardFFT_11(11),
    m_forwardFFT_12(12),
    m_forwardFFT_13(13),
    m_forwardFFT_14(14),
    m_window_512(512, juce::dsp::WindowingFunction<float>::hann),
    m_window_1024(1024, juce::dsp::WindowingFunction<float>::hann),
    m_window_2048(2048, juce::dsp::WindowingFunction<float>::hann),
    m_window_4096(4096, juce::dsp::WindowingFunction<float>::hann),
    m_window_8192(8192, juce::dsp::WindowingFunction<float>::hann),
    m_window_16384(16384, juce::dsp::WindowingFunction<float>::hann)
{
    // AudioParameterChoice raw value is the choice index (0=2048 … 3=16384).
    // Prefer getIndex() so we never treat a 0–1 normalised host value as an index.
    int blockIndex = 0;
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (mr_valueTree.getParameter ("BLOCK_ID")))
        blockIndex = choice->getIndex();
    else if (auto* raw = mr_valueTree.getRawParameterValue ("BLOCK_ID"))
        blockIndex = juce::jlimit (0, 3, (int) std::lround (raw->load()));

    setFFTBlockSize (blockIndex);
    
    mr_valueTree.addParameterListener( "BLOCK_ID", this );
    mr_valueTree.addParameterListener( "LEFT_ID", this );
    mr_valueTree.addParameterListener( "RIGHT_ID", this );
    mr_valueTree.addParameterListener( "BOTH_ID", this );
    mr_valueTree.addParameterListener( "AVG_ID", this );
    mr_valueTree.addParameterListener( "RANGE_ID", this );
    mr_valueTree.addParameterListener( "MAXIMUM_ID", this );
    mr_valueTree.addParameterListener( "MINIMUM_ID", this );

    
}

Analyser::~Analyser()
{
    mr_valueTree.removeParameterListener ("BLOCK_ID", this);
    mr_valueTree.removeParameterListener ("LEFT_ID", this);
    mr_valueTree.removeParameterListener ("RIGHT_ID", this);
    mr_valueTree.removeParameterListener ("BOTH_ID", this);
    mr_valueTree.removeParameterListener ("AVG_ID", this);
    mr_valueTree.removeParameterListener ("RANGE_ID", this);
    mr_valueTree.removeParameterListener ("MAXIMUM_ID", this);
    mr_valueTree.removeParameterListener ("MINIMUM_ID", this);
}


// ============================================================================
void Analyser::resetScopeMaximumsData()
{
    const juce::ScopedLock lock( m_volumeRangeChange );
    
    auto minimum { m_minimumVolumeInDecibels };
    
    for ( auto &data : m_volumsMaximumData )
    {
        data = minimum;
    }
    
    m_currentMaximumVolumeInDecibels.store( minimum );
    m_currentAverageVolumeInDecibels.store( minimum );
    m_lastMaxHoldUpdateMs = 0.0;
}


// ============================================================================
void Analyser::setNextFFTBlockStatus( const bool nextFFTBlockStatus )
{
    m_nextFFTBlockReady.store( nextFFTBlockStatus );
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



float Analyser::getScopeData( size_t index )
{
    if (index >= m_scopeDynamicData.size())
        return 0.0f;

    return m_scopeDynamicData[index];
}

float Analyser::getScopePreData (size_t index)
{
    if (index >= m_scopePreData.size())
        return 0.0f;

    return m_scopePreData[index];
}



float Analyser::getScopeMaximumsData( size_t index )
{
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
    const double sr = m_sampleRate.load();
    const size_t fftSize = m_fftSize.load();

    if (sr <= 0.0 || fftSize == 0 || ! (freqHz > 0.0f))
        return 0.0f;

    return static_cast<float> (static_cast<double> (freqHz) * static_cast<double> (fftSize) / sr);
}

int Analyser::getHighestDisplayBinIndex() const
{
    const int scopeSize = static_cast<int> (m_scopeSize.load());
    if (scopeSize < 2)
        return 0;

    const double sr = m_sampleRate.load();
    const size_t fftSize = m_fftSize.load();
    if (sr <= 0.0 || fftSize == 0)
        return scopeSize - 1;

    const double maxFreq = static_cast<double> (getDisplayMaxFrequencyHz());
    const int bin = static_cast<int> (std::floor (maxFreq * static_cast<double> (fftSize) / sr));
    return juce::jlimit (1, scopeSize - 1, bin);
}

float Analyser::getModifiedScopeData(size_t index)
{
    float dynamicData = m_scopeDynamicData.at(index);
    float maximumsData = m_scopeMaximumData.at(index);
    float resultData = dynamicData - maximumsData; // Exclude maximums
    return resultData;
}

// ============================================================================
void Analyser::pushSamplesIntoFifo (
    const float leftChannel,
    const float rightChannel
) noexcept
{
    pushSamplesIntoFifo (&leftChannel, &rightChannel, 1);
}

void Analyser::pushSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept
{
    if (! m_blockSizeDefined.load() || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const size_t fftSize = m_fftSize.load();
    if (fftSize == 0 || m_fifo.size() < fftSize || m_fftData.size() < fftSize * 2)
        return;

    const auto channels = m_activeChannels.load();
    size_t index = m_fifoIndex.load();

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

        if (index >= fftSize)
        {
            if (! m_nextFFTBlockReady.load())
            {
                std::copy_n (m_fifo.data(), fftSize, m_fftData.data());
                std::fill_n (m_fftData.data() + fftSize, fftSize, 0.0f);
                m_nextFFTBlockReady.store (true);
            }

            index = 0;
        }

        m_fifo[index++] = dataForAnalysis;
    }

    m_fifoIndex.store (index);
}

void Analyser::pushPreSamplesIntoFifo (const float* left, const float* right, int numSamples) noexcept
{
    if (! m_blockSizeDefined.load() || left == nullptr || right == nullptr || numSamples <= 0)
        return;

    const size_t fftSize = m_fftSize.load();
    if (fftSize == 0 || m_preFifo.size() < fftSize || m_preFftData.size() < fftSize * 2)
        return;

    const auto channels = m_activeChannels.load();
    size_t index = m_preFifoIndex.load();

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

        if (index >= fftSize)
        {
            if (! m_preNextFFTBlockReady.load())
            {
                std::copy_n (m_preFifo.data(), fftSize, m_preFftData.data());
                std::fill_n (m_preFftData.data() + fftSize, fftSize, 0.0f);
                m_preNextFFTBlockReady.store (true);
            }

            index = 0;
        }

        m_preFifo[index++] = dataForAnalysis;
    }

    m_preFifoIndex.store (index);
}



void Analyser::calculateNextFrameOfSpectrum()
{
    if (m_nextFFTBlockReady.load())
    {
        processFFT();
        calculateCurrentVolumes();
        calculateDynamicVolumes();
        calculateMaximumVolumes();

        if ( m_volumeRangeIsDynamamic )
        {
            auto maximum = mr_valueTree.getParameter( "MAXIMUM_ID" );
            maximum->
                setValueNotifyingHost(
                    juce::jmap(
                        m_currentMaximumVolumeInDecibels.load(),
                        -200.0f,
                        40.0f,
                        0.0f,
                        1.0f
                    )
                );

            calculateCurrentAverageVolume();

            auto minumum = mr_valueTree.getParameter( "MINIMUM_ID" );
            minumum->
                setValueNotifyingHost(
                    juce::jmap(
                        m_currentAverageVolumeInDecibels.load(),
                        -380.0f,
                        30.0f,
                        0.0f,
                        1.0f
                    )
                );
        }

        float minDb = -120.0f;
        float maxDb = 12.0f;
        {
            const juce::ScopedLock lock (m_volumeRangeChange);
            minDb = m_minimumVolumeInDecibels;
            maxDb = m_maximumVolumeInDecibels;
        }

        const size_t n = m_scopeSize.load();
        if (m_volumsDynamicData.size() == n && m_volumsMaximumData.size() == n)
        {
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

        m_nextFFTBlockReady.store (false);
    }

    if (m_preNextFFTBlockReady.load())
    {
        processPreFFT();
        calculatePreScopeFromFft();
        m_preNextFFTBlockReady.store (false);
    }
}


// ============================================================================
void Analyser::processFFT()
{
    switch ( m_fftSize.load() ) {
        case 512:
            m_window_512.multiplyWithWindowingTable(
                m_fftData.data(),
                m_fftSize.load()
            );
            m_forwardFFT_9.performFrequencyOnlyForwardTransform(
                m_fftData.data()
            );
            break;
        case 1024:
            m_window_1024.multiplyWithWindowingTable(
                m_fftData.data(),
                m_fftSize.load()
            );
            m_forwardFFT_10.performFrequencyOnlyForwardTransform(
                m_fftData.data()
            );
            break;
        case 2048:
            m_window_2048.multiplyWithWindowingTable(
                m_fftData.data(),
                m_fftSize.load()
            );
            m_forwardFFT_11.performFrequencyOnlyForwardTransform(
                m_fftData.data()
            );
            break;
        case 4096:
            m_window_4096.multiplyWithWindowingTable(
                m_fftData.data(),
                m_fftSize.load()
            );
            m_forwardFFT_12.performFrequencyOnlyForwardTransform(
                m_fftData.data()
            );
            break;
        case 8192:
            m_window_8192.multiplyWithWindowingTable(
                m_fftData.data(),
                m_fftSize.load()
            );
            m_forwardFFT_13.performFrequencyOnlyForwardTransform(
                m_fftData.data()
            );
            break;
        case 16384:
            m_window_16384.multiplyWithWindowingTable(
                m_fftData.data(),
                m_fftSize.load()
            );
            m_forwardFFT_14.performFrequencyOnlyForwardTransform(
                m_fftData.data()
            );
            break;
        default:
            DBG( "FFT Size erorr!" );
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
    if (m_scopePreData.size() != scopeSize)
        m_scopePreData.resize (scopeSize, 0.0f);

    float minDb = -120.0f;
    float maxDb = 12.0f;
    {
        const juce::ScopedLock lock (m_volumeRangeChange);
        minDb = m_minimumVolumeInDecibels;
        maxDb = m_maximumVolumeInDecibels;
    }

    const auto bins = juce::jmin (scopeSize, m_preFftData.size());
    for (size_t i = 0; i < bins; ++i)
    {
        m_volumsPreData[i] = juce::Decibels::gainToDecibels (m_preFftData[i]) - offset;
        m_scopePreData[i] = adaptData (m_volumsPreData[i], minDb, maxDb);
    }

    for (size_t i = bins; i < scopeSize; ++i)
    {
        m_volumsPreData[i] = m_minimumVolumeInDecibels;
        m_scopePreData[i] = 0.0f;
    }
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
    float dtSeconds = 0.033f;

    if (m_lastMaxHoldUpdateMs > 0.0)
    {
        dtSeconds = (float) ((nowMs - m_lastMaxHoldUpdateMs) * 0.001);
        // Clamp so hitch/pause doesn't wipe the whole max curve in one frame.
        dtSeconds = juce::jlimit (0.0f, 0.25f, dtSeconds);
    }

    m_lastMaxHoldUpdateMs = nowMs;

    // Fade from display top to floor over holdSeconds when a bin isn't being refreshed.
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
    m_currentAverageVolumeInDecibels.store(
        m_currentMaximumVolumeInDecibels.load() - 10.0f
    );
    
    for ( const auto &volume : m_volumsDynamicData )
    {
        m_currentAverageVolumeInDecibels.store(
            m_currentAverageVolumeInDecibels.load() + volume
        );
    }
    
    {
        const juce::ScopedLock lock( m_volumeRangeChange );
        
        m_currentAverageVolumeInDecibels.store( m_currentAverageVolumeInDecibels.load() /
            static_cast<float>( m_scopeSize.load() )
        );
    }
}



float Analyser::adaptData (const float volume, float minDb, float maxDb) const
{
    return juce::jmap (volume, minDb, maxDb, 0.0f, 1.0f);
}


// ============================================================================
void Analyser::parameterChanged(
    const juce::String &parameterID,
    float newValue )
{
    if ( parameterID == "BLOCK_ID" )
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
    else if ( parameterID == "LEFT_ID" )
    {
        setActiveChannel( Channels::left );
    }
    else if ( parameterID == "RIGHT_ID" )
    {
        setActiveChannel( Channels::right );
    }
    else if ( parameterID == "BOTH_ID" )
    {
        setActiveChannel( Channels::both );
    }
    else if ( parameterID == "AVG_ID" )
    {
        setAvg( static_cast<size_t>( newValue ) );
    }
    else if ( parameterID == "RANGE_ID" )
    {
        setVolumeScaleModeAsDynamic( ! static_cast<bool>( newValue ) );
    }
    else if ( parameterID == "MAXIMUM_ID" || parameterID == "MINIMUM_ID" )
    {
        auto maximum =
            mr_valueTree.getRawParameterValue( "MAXIMUM_ID" )->load();
        auto minimum =
            mr_valueTree.getRawParameterValue( "MINIMUM_ID" )->load();
        
        if ( maximum - 10.0f < minimum ) { minimum = maximum - 10.0f; }
        
        setVolumeRangeInDecibels( maximum, minimum );
    }
}


// ============================================================================
void Analyser::setFFTBlockSize( const int blockMenuIndex )
{
    m_blockSizeDefined.store( false );
    
    // FFT size 2048:  order 11, offset 1.0
    // FFT size 4096:  order 12, offset 2.0
    // FFT size 8192:  order 13, offset 4.0
    // FFT size 16384: order 14, offset 8.0

    const int index = juce::jlimit (0, 3, blockMenuIndex);

    switch ( index )
    {
        case 0: m_fftOrder.store( 11 ); m_offset.store( 1.0f ); break; // 2048
        case 1: m_fftOrder.store( 12 ); m_offset.store( 2.0f ); break; // 4096
        case 2: m_fftOrder.store( 13 ); m_offset.store( 4.0f ); break; // 8192
        case 3: m_fftOrder.store( 14 ); m_offset.store( 8.0f ); break; // 16384
        default:
            m_fftOrder.store( 11 );
            m_offset.store( 1.0f );
            break;
    }
    
    m_fftSize.store(int64_t{ 1 } << m_fftOrder);
    m_scopeSize.store( m_fftSize.load() / 2 );
    m_fifoIndex.store( 0 );
    
    if (m_fftOrder < 0 || m_fftOrder >= sizeof(int) * 8) {
        // Handle error: m_fftOrder is out of valid range
    }
    else {
        m_fftSize.store(int64_t{ 1 } << m_fftOrder);
    }



    m_fifo.resize( m_fftSize.load() );
    m_fftData.resize( m_fftSize.load() * 2 );
    m_preFifo.resize (m_fftSize.load());
    m_preFftData.resize (m_fftSize.load() * 2);
    m_preFifoIndex.store (0);
    m_preNextFFTBlockReady.store (false);
    
    {
        const juce::ScopedLock lock( m_volumeRangeChange );
        
        m_volumsDynamicData.resize(
            m_scopeSize.load(),
            m_minimumVolumeInDecibels
        );
        m_volumsMaximumData.resize(
            m_scopeSize.load(),
            m_minimumVolumeInDecibels
        );
        m_volumsPreData.resize (
            m_scopeSize.load(),
            m_minimumVolumeInDecibels
        );
    }
    
    m_scopeDynamicData.resize(
        m_scopeSize.load(),
        0
    );
    m_scopeMaximumData.resize(
        m_scopeSize.load(),
        0
    );
    m_scopePreData.resize (
        m_scopeSize.load(),
        0
    );
    
    {
        const juce::ScopedLock lock( m_volumeRangeChange );
        
        for ( auto &vector : m_avgData )
        {
            vector.resize( m_scopeSize.load(), m_minimumVolumeInDecibels );
        }
    }
    
    m_avgFifoSize.store( 0 );
    m_avgFifoWritePointer.store( 0 );
    
    resetScopeMaximumsData();
    
    m_blockSizeDefined.store( true );


}



void Analyser::setActiveChannel( Channels channels )
{
    m_activeChannels.store( channels );
}



void Analyser::setAvg( size_t avg )
{
    m_avgFactor.store( avg );
    m_avgFifoSize.store( avg );
}



void Analyser::setVolumeScaleModeAsDynamic( bool isDynamamic )
{
    m_volumeRangeIsDynamamic.store( isDynamamic );
    resetScopeMaximumsData();
}



void Analyser::setVolumeRangeInDecibels( float maximum, float minimum )
{
    const juce::ScopedLock lock( m_volumeRangeChange );
    
    m_maximumVolumeInDecibels = maximum;
    m_minimumVolumeInDecibels = minimum;
}

