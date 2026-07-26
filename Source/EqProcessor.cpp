#include "EqProcessor.h"
#include "EqEditor.h"
#include "BinaryData.h"
#include "FrequencyResponseComponent.h"
#include "OscilloscopeComponent.h"
#include <juce_dsp/juce_dsp.h>
#include "Visualizer/Analyser.h"
#include "FilterType.h"
#include "BandChannel.h"
#include "DynamicEq.h"
#include "Menu/AnalyserDefaults.h"
#include <JuceHeader.h>

juce::Image EqProcessor::darkKnob4_StitchedImage;

namespace
{
    /** Mid = 0.5*(L+R), Side = 0.5*(L-R) — matches Side Check. Mono → Mid=L, Side=silence. */
    void measureMidSideLevels (const juce::AudioBuffer<float>& buffer,
                               int numSamples,
                               std::atomic<float>& peakMid,
                               std::atomic<float>& rmsMid,
                               std::atomic<float>& peakSide,
                               std::atomic<float>& rmsSide)
    {
        constexpr float silenceDb = -100.0f;

        if (numSamples <= 0 || buffer.getNumChannels() <= 0)
        {
            peakMid.store (silenceDb);
            rmsMid.store (silenceDb);
            peakSide.store (silenceDb);
            rmsSide.store (silenceDb);
            return;
        }

        const float* left = buffer.getReadPointer (0);
        const float* right = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : nullptr;

        float peakM = 0.0f;
        float peakS = 0.0f;
        double sumSqM = 0.0;
        double sumSqS = 0.0;

        if (right == nullptr)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float mid = left[i];
                peakM = juce::jmax (peakM, std::abs (mid));
                sumSqM += (double) mid * (double) mid;
            }
        }
        else
        {
            for (int i = 0; i < numSamples; ++i)
            {
                const float mid = 0.5f * (left[i] + right[i]);
                const float side = 0.5f * (left[i] - right[i]);
                peakM = juce::jmax (peakM, std::abs (mid));
                peakS = juce::jmax (peakS, std::abs (side));
                sumSqM += (double) mid * (double) mid;
                sumSqS += (double) side * (double) side;
            }
        }

        const float invN = 1.0f / (float) numSamples;
        peakMid.store (juce::Decibels::gainToDecibels (peakM, silenceDb));
        rmsMid.store (juce::Decibels::gainToDecibels ((float) std::sqrt (sumSqM * (double) invN), silenceDb));
        peakSide.store (juce::Decibels::gainToDecibels (peakS, silenceDb));
        rmsSide.store (juce::Decibels::gainToDecibels ((float) std::sqrt (sumSqS * (double) invN), silenceDb));
    }
}

//==============================================================================
EqProcessor::EqProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if ! JucePlugin_IsMidiEffect
#if ! JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withInput("Sidechain", juce::AudioChannelSet::stereo(), false)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    treeState(*this, &undoManager, "PARAMETERS", createParameterLayout()),
    m_analyser(treeState)
#endif
{
    initializeSharedImages();

    treeState.addParameterListener("highpassOnOff", this);
    treeState.addParameterListener("lowpassOnOff", this);
    treeState.addParameterListener("highShelfOnOff", this);
    treeState.addParameterListener("lowShelfOnOff", this);
    treeState.addParameterListener("band1OnOff", this);
    treeState.addParameterListener("band2OnOff", this);
    treeState.addParameterListener("band3OnOff", this);
    treeState.addParameterListener("band4OnOff", this);

    treeState.addParameterListener("highpassCutoff", this);
    treeState.addParameterListener("highpassQ", this);
   
    treeState.addParameterListener("highShelfGain", this);
    treeState.addParameterListener("highShelfFrequency", this);
    treeState.addParameterListener("highShelfQ", this);
   
    treeState.addParameterListener("lowShelfGain", this);
    treeState.addParameterListener("lowShelfFrequency", this);
    treeState.addParameterListener("lowShelfQ", this);
   
    treeState.addParameterListener("lowpassCutoff", this);
    treeState.addParameterListener("lowpassQ", this);
    
    treeState.addParameterListener("band1Q", this);
    treeState.addParameterListener("band1Gain", this);
    treeState.addParameterListener("band1Frequency", this);
   
    treeState.addParameterListener("band2Q", this);
    treeState.addParameterListener("band2Gain", this);
    treeState.addParameterListener("band2Frequency", this);
   
    treeState.addParameterListener("band3Q", this);
    treeState.addParameterListener("band3Gain", this);
    treeState.addParameterListener("band3Frequency", this);
   
    treeState.addParameterListener("band4Q", this);
    treeState.addParameterListener("band4Gain", this);
    treeState.addParameterListener("band4Frequency", this);

    treeState.addParameterListener("outputGain", this);

    treeState.addParameterListener("menuBackgroundColor", this);

    treeState.addParameterListener("BLOCK_ID", this);
    treeState.addParameterListener("LEFT_ID", this);
    treeState.addParameterListener("RIGHT_ID", this);
    treeState.addParameterListener("BOTH_ID", this);
    treeState.addParameterListener("REFRESH_ID", this);
    treeState.addParameterListener("AVG_ID", this);
    treeState.addParameterListener("BINS_ID", this);
    treeState.addParameterListener("MAX_ID", this);
    treeState.addParameterListener("LIN_ID", this);
    treeState.addParameterListener("LOG_ID", this);
    treeState.addParameterListener("ST_ID", this);
    treeState.addParameterListener("RANGE_ID", this);
    treeState.addParameterListener("MAXIMUM_ID", this);
    treeState.addParameterListener("MINIMUM_ID", this);

    bypassParam = dynamic_cast<juce::AudioParameterBool*> (treeState.getParameter ("bypass"));
    initialiseAbSnapshotsFromCurrentState();
}

void EqProcessor::setOscilloscopeTarget (OscilloscopeComponent* target) noexcept
{
    oscilloscopeTarget.store (target, std::memory_order_release);
    if (target != nullptr && sampleRate > 0.0)
        target->prepare (sampleRate);
}

OscilloscopeComponent* EqProcessor::getOscilloscopeTarget() const noexcept
{
    return oscilloscopeTarget.load (std::memory_order_acquire);
}

void EqProcessor::setEcoMode (bool shouldEnable) noexcept
{
    ecoMode.store (shouldEnable);
    m_analyser.setEcoMode (shouldEnable);
}

bool EqProcessor::isSpectrumAnalyserActive() const noexcept
{
    if (ecoMode.load())
        return false;

    if (auto* p = treeState.getRawParameterValue ("SPECTRUM_ANALYSER_ID"))
        return p->load() > 0.5f;

    return true;
}

EqProcessor::~EqProcessor()
{
    treeState.removeParameterListener("highpassCutoff", this);
    treeState.removeParameterListener("highpassQ", this);
    
    treeState.removeParameterListener("highShelfGain", this);
    treeState.removeParameterListener("highShelfFrequency", this);
    treeState.removeParameterListener("highShelfQ", this);
    
    treeState.removeParameterListener("lowShelfGain", this);
    treeState.removeParameterListener("lowShelfFrequency", this);
    treeState.removeParameterListener("lowShelfQ", this);
   
    treeState.removeParameterListener("lowpassCutoff", this);
    treeState.removeParameterListener("lowpassQ", this);
   
    treeState.removeParameterListener("band1Q", this);
    treeState.removeParameterListener("band1Gain", this);
    treeState.removeParameterListener("band1Frequency", this);
   
    treeState.removeParameterListener("band2Q", this);
    treeState.removeParameterListener("band2Gain", this);
    treeState.removeParameterListener("band2Frequency", this);
   
    treeState.removeParameterListener("band3Q", this);
    treeState.removeParameterListener("band3Gain", this);
    treeState.removeParameterListener("band3Frequency", this);
   
    treeState.removeParameterListener("band4Q", this);
    treeState.removeParameterListener("band4Gain", this);
    treeState.removeParameterListener("band4Frequency", this);

    treeState.removeParameterListener("outputGain", this);

    treeState.removeParameterListener("menuBackgroundColor", this);

    treeState.removeParameterListener("BLOCK_ID", this);
    treeState.removeParameterListener("LEFT_ID", this);
    treeState.removeParameterListener("RIGHT_ID", this);
    treeState.removeParameterListener("BOTH_ID", this);
    treeState.removeParameterListener("REFRESH_ID", this);
    treeState.removeParameterListener("AVG_ID", this);
    treeState.removeParameterListener("BINS_ID", this);
    treeState.removeParameterListener("MAX_ID", this);
    treeState.removeParameterListener("LIN_ID", this);
    treeState.removeParameterListener("LOG_ID", this);
    treeState.removeParameterListener("ST_ID", this);
    treeState.removeParameterListener("RANGE_ID", this);
    treeState.removeParameterListener("MAXIMUM_ID", this);
    treeState.removeParameterListener("MINIMUM_ID", this);

    treeState.removeParameterListener("highpassOnOff", this);
    treeState.removeParameterListener("lowpassOnOff", this);
    treeState.removeParameterListener("highShelfOnOff", this);
    treeState.removeParameterListener("lowShelfOnOff", this);
    treeState.removeParameterListener("band1OnOff", this);
    treeState.removeParameterListener("band2OnOff", this);
    treeState.removeParameterListener("band3OnOff", this);
    treeState.removeParameterListener("band4OnOff", this);

}

juce::AudioProcessorValueTreeState::ParameterLayout EqProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;
    const auto analyserDefaults = AnalyserDefaults::load();

    //Highpass
    auto pHighpassCutoff = std::make_unique<juce::AudioParameterFloat>("highpassCutoff", "HighpassCutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 20.0f);
    auto pHighpassQ = std::make_unique<juce::AudioParameterFloat>("highpassQ", "HighpassQ", juce::NormalisableRange < float>(0.15f, 10.0f, 0.01f, 0.1f), 0.5f);
    auto pHighpassOnOff = std::make_unique<juce::AudioParameterBool>("highpassOnOff", "HighpassOnOff", false);
    auto pHighpassSlope = std::make_unique<juce::AudioParameterChoice>("highpassSlope", "HighpassSlope", FilterSlope::getChoiceNames(), FilterSlope::db12);
    auto pHighpassChannel = std::make_unique<juce::AudioParameterChoice>("highpassChannel", "HighpassChannel", BandChannel::getChoiceNames(), BandChannel::stereo);

    //Lowpass
    auto pLowpassCutoff = std::make_unique<juce::AudioParameterFloat>("lowpassCutoff", "LowpassCutoff", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 20000.0f);
    auto pLowpassQ = std::make_unique<juce::AudioParameterFloat>("lowpassQ", "LowpassQ", juce::NormalisableRange < float>(0.15f, 10.0f, 0.01f, 0.1f), 0.5f);
    auto pLowpassOnOff = std::make_unique<juce::AudioParameterBool>("lowpassOnOff", "LowpassOnOff", false);
    auto pLowpassSlope = std::make_unique<juce::AudioParameterChoice>("lowpassSlope", "LowpassSlope", FilterSlope::getChoiceNames(), FilterSlope::db12);
    auto pLowpassChannel = std::make_unique<juce::AudioParameterChoice>("lowpassChannel", "LowpassChannel", BandChannel::getChoiceNames(), BandChannel::stereo);

    //LowShelf
    auto pLowShelfFrequency = std::make_unique<juce::AudioParameterFloat>("lowShelfFrequency", "LowShelfFrequency", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 100.0f);
    auto pLowShelfQ = std::make_unique<juce::AudioParameterFloat>("lowShelfQ", "LowShelfQ", juce::NormalisableRange<float>(0.15f, 10.0f, 0.01f, 0.25f), 0.5f);
    auto pLowShelfGain = std::make_unique<juce::AudioParameterFloat>("lowShelfGain", "LowShelfGain", -24.0f, 24.0f, 0.0f);
    auto pLowShelfOnOff = std::make_unique<juce::AudioParameterBool>("lowShelfOnOff", "LowShelfOnOff", false);
    auto pLowShelfType = std::make_unique<juce::AudioParameterChoice>("lowShelfType", "LowShelfType", FilterType::getChoiceNames(), FilterType::lowShelf);
    auto pLowShelfChannel = std::make_unique<juce::AudioParameterChoice>("lowShelfChannel", "LowShelfChannel", BandChannel::getChoiceNames(), BandChannel::stereo);
    auto pLowShelfDynamic = std::make_unique<juce::AudioParameterBool>("lowShelfDynamic", "LowShelfDynamic", false);
    auto pLowShelfSidechain = std::make_unique<juce::AudioParameterBool>("lowShelfSidechain", "LowShelfSidechain", false);
    auto pLowShelfSidechainMidi = std::make_unique<juce::AudioParameterBool>("lowShelfSidechainMidi", "LowShelfSidechainMidi", false);
    auto pLowShelfSpectral = std::make_unique<juce::AudioParameterBool>("lowShelfSpectral", "LowShelfSpectral", false);
    auto pLowShelfDynThreshold = std::make_unique<juce::AudioParameterFloat>(
        "lowShelfDynThreshold", "LowShelfDynThreshold",
        juce::NormalisableRange<float> (-120.0f, 0.0f, 0.1f), -24.0f);
    auto pLowShelfAttackMs = std::make_unique<juce::AudioParameterFloat>(
        "lowShelfAttackMs", "LowShelfAttackMs",
        DynamicEq::attackMsRange(), DynamicEq::attackMs);
    auto pLowShelfReleaseMs = std::make_unique<juce::AudioParameterFloat>(
        "lowShelfReleaseMs", "LowShelfReleaseMs",
        DynamicEq::releaseMsRange(), DynamicEq::releaseMs);
    auto pLowShelfSpectralResHz = std::make_unique<juce::AudioParameterFloat>(
        "lowShelfSpectralResHz", "LowShelfSpectralResHz",
        juce::NormalisableRange<float> (SpectralBinning::kMinBandwidthHz, SpectralBinning::kTargetBandwidthHz, 0.05f),
        SpectralBinning::kDefaultBandwidthHz);
    auto pLowShelfSpectralDepth = std::make_unique<juce::AudioParameterFloat>(
        "lowShelfSpectralDepth", "LowShelfSpectralAmount",
        juce::NormalisableRange<float> (SpectralDynamics::kMinSpectralAmount, SpectralDynamics::kMaxSpectralAmount, 0.01f),
        SpectralDynamics::kDefaultSpectralAmount);
    auto pLowShelfSpectralExpand = std::make_unique<juce::AudioParameterBool>(
        "lowShelfSpectralExpand", "LowShelfSpectralExpand", false);
    auto pLowShelfSat = std::make_unique<juce::AudioParameterBool>(
        "lowShelfSat", "LowShelfSat", false);
    auto pLowShelfSatModel = std::make_unique<juce::AudioParameterChoice>(
        "lowShelfSatModel", "LowShelfSatModel", BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pLowShelfSatPost = std::make_unique<juce::AudioParameterBool>(
        "lowShelfSatPost", "LowShelfSatPost", false);

    //HighShelf
    auto pHighShelfFrequency = std::make_unique<juce::AudioParameterFloat>("highShelfFrequency", "HighShelfFrequency", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 5500.0f);
    auto pHighShelfQ = std::make_unique<juce::AudioParameterFloat>("highShelfQ", "HighShelfQ", juce::NormalisableRange<float>(0.15f, 10.0f, 0.01f, 0.25f), 0.5f);
    auto pHighShelfGain = std::make_unique<juce::AudioParameterFloat>("highShelfGain", "HighShelfGain", -24.0f, 24.0f, 0.0f);
    auto pHighShelfOnOff = std::make_unique<juce::AudioParameterBool>("highShelfOnOff", "HighShelfOnOff", false);
    auto pHighShelfType = std::make_unique<juce::AudioParameterChoice>("highShelfType", "HighShelfType", FilterType::getChoiceNames(), FilterType::highShelf);
    auto pHighShelfChannel = std::make_unique<juce::AudioParameterChoice>("highShelfChannel", "HighShelfChannel", BandChannel::getChoiceNames(), BandChannel::stereo);
    auto pHighShelfDynamic = std::make_unique<juce::AudioParameterBool>("highShelfDynamic", "HighShelfDynamic", false);
    auto pHighShelfSidechain = std::make_unique<juce::AudioParameterBool>("highShelfSidechain", "HighShelfSidechain", false);
    auto pHighShelfSidechainMidi = std::make_unique<juce::AudioParameterBool>("highShelfSidechainMidi", "HighShelfSidechainMidi", false);
    auto pHighShelfSpectral = std::make_unique<juce::AudioParameterBool>("highShelfSpectral", "HighShelfSpectral", false);
    auto pHighShelfDynThreshold = std::make_unique<juce::AudioParameterFloat>(
        "highShelfDynThreshold", "HighShelfDynThreshold",
        juce::NormalisableRange<float> (-120.0f, 0.0f, 0.1f), -24.0f);
    auto pHighShelfAttackMs = std::make_unique<juce::AudioParameterFloat>(
        "highShelfAttackMs", "HighShelfAttackMs",
        DynamicEq::attackMsRange(), DynamicEq::attackMs);
    auto pHighShelfReleaseMs = std::make_unique<juce::AudioParameterFloat>(
        "highShelfReleaseMs", "HighShelfReleaseMs",
        DynamicEq::releaseMsRange(), DynamicEq::releaseMs);
    auto pHighShelfSpectralResHz = std::make_unique<juce::AudioParameterFloat>(
        "highShelfSpectralResHz", "HighShelfSpectralResHz",
        juce::NormalisableRange<float> (SpectralBinning::kMinBandwidthHz, SpectralBinning::kTargetBandwidthHz, 0.05f),
        SpectralBinning::kDefaultBandwidthHz);
    auto pHighShelfSpectralDepth = std::make_unique<juce::AudioParameterFloat>(
        "highShelfSpectralDepth", "HighShelfSpectralAmount",
        juce::NormalisableRange<float> (SpectralDynamics::kMinSpectralAmount, SpectralDynamics::kMaxSpectralAmount, 0.01f),
        SpectralDynamics::kDefaultSpectralAmount);
    auto pHighShelfSpectralExpand = std::make_unique<juce::AudioParameterBool>(
        "highShelfSpectralExpand", "HighShelfSpectralExpand", false);
    auto pHighShelfSat = std::make_unique<juce::AudioParameterBool>(
        "highShelfSat", "HighShelfSat", false);
    auto pHighShelfSatModel = std::make_unique<juce::AudioParameterChoice>(
        "highShelfSatModel", "HighShelfSatModel", BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pHighShelfSatPost = std::make_unique<juce::AudioParameterBool>(
        "highShelfSatPost", "HighShelfSatPost", false);

    //Band1
    auto pBand1Frequency = std::make_unique<juce::AudioParameterFloat>("band1Frequency", "Band1Frequency", juce::NormalisableRange<float>( 20.0f, 20000.0f, 1.0f, 0.2f), 300.0f);
    auto pBand1Gain = std::make_unique<juce::AudioParameterFloat>("band1Gain", "Band1Gain", -24.0f, 24.0f, 0.0f);
    // Default bell Q ~1/3 tighter than 0.5 (×4/3 → narrower bandwidth).
    constexpr float defaultBellQ = 0.67f;
    auto pBand1Q = std::make_unique<juce::AudioParameterFloat>("band1Q", "Band1Q", juce::NormalisableRange < float>(0.15f, 10.0f, 0.01f, 0.25f), defaultBellQ);
    auto pBand1OnOff = std::make_unique<juce::AudioParameterBool>("band1OnOff", "Band1OnOff", false);
    auto pBand1Type = std::make_unique<juce::AudioParameterChoice>("band1Type", "Band1Type", FilterType::getChoiceNames(), FilterType::bell);
    auto pBand1Channel = std::make_unique<juce::AudioParameterChoice>("band1Channel", "Band1Channel", BandChannel::getChoiceNames(), BandChannel::stereo);
    auto pBand1Dynamic = std::make_unique<juce::AudioParameterBool>("band1Dynamic", "Band1Dynamic", false);
    auto pBand1Sidechain = std::make_unique<juce::AudioParameterBool>("band1Sidechain", "Band1Sidechain", false);
    auto pBand1SidechainMidi = std::make_unique<juce::AudioParameterBool>("band1SidechainMidi", "Band1SidechainMidi", false);
    auto pBand1Spectral = std::make_unique<juce::AudioParameterBool>("band1Spectral", "Band1Spectral", false);
    auto pBand1DynThreshold = std::make_unique<juce::AudioParameterFloat>(
        "band1DynThreshold", "Band1DynThreshold",
        juce::NormalisableRange<float> (-120.0f, 0.0f, 0.1f), -24.0f);
    auto pBand1AttackMs = std::make_unique<juce::AudioParameterFloat>(
        "band1AttackMs", "Band1AttackMs",
        DynamicEq::attackMsRange(), DynamicEq::attackMs);
    auto pBand1ReleaseMs = std::make_unique<juce::AudioParameterFloat>(
        "band1ReleaseMs", "Band1ReleaseMs",
        DynamicEq::releaseMsRange(), DynamicEq::releaseMs);
    auto pBand1SpectralResHz = std::make_unique<juce::AudioParameterFloat>(
        "band1SpectralResHz", "Band1SpectralResHz",
        juce::NormalisableRange<float> (SpectralBinning::kMinBandwidthHz, SpectralBinning::kTargetBandwidthHz, 0.05f),
        SpectralBinning::kDefaultBandwidthHz);
    auto pBand1SpectralDepth = std::make_unique<juce::AudioParameterFloat>(
        "band1SpectralDepth", "Band1SpectralAmount",
        juce::NormalisableRange<float> (SpectralDynamics::kMinSpectralAmount, SpectralDynamics::kMaxSpectralAmount, 0.01f),
        SpectralDynamics::kDefaultSpectralAmount);
    auto pBand1SpectralExpand = std::make_unique<juce::AudioParameterBool>(
        "band1SpectralExpand", "Band1SpectralExpand", false);
    auto pBand1Sat = std::make_unique<juce::AudioParameterBool>(
        "band1Sat", "Band1Sat", false);
    auto pBand1SatModel = std::make_unique<juce::AudioParameterChoice>(
        "band1SatModel", "Band1SatModel", BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pBand1SatPost = std::make_unique<juce::AudioParameterBool>(
        "band1SatPost", "Band1SatPost", false);

    //Band2
    auto pBand2Frequency = std::make_unique<juce::AudioParameterFloat>("band2Frequency", "Band2Frequency", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 1000.0f);
    auto pBand2Gain = std::make_unique<juce::AudioParameterFloat>("band2Gain", "band2Gain", -24.0f, 24.0f , 0.0f);
    auto pBand2Q = std::make_unique<juce::AudioParameterFloat>("band2Q", "Band2Q", juce::NormalisableRange < float>(0.15f, 10.0f, 0.01f, 0.25f), defaultBellQ);
    auto pBand2OnOff = std::make_unique<juce::AudioParameterBool>("band2OnOff", "Band2OnOff", false);
    auto pBand2Type = std::make_unique<juce::AudioParameterChoice>("band2Type", "Band2Type", FilterType::getChoiceNames(), FilterType::bell);
    auto pBand2Channel = std::make_unique<juce::AudioParameterChoice>("band2Channel", "Band2Channel", BandChannel::getChoiceNames(), BandChannel::stereo);
    auto pBand2Dynamic = std::make_unique<juce::AudioParameterBool>("band2Dynamic", "Band2Dynamic", false);
    auto pBand2Sidechain = std::make_unique<juce::AudioParameterBool>("band2Sidechain", "Band2Sidechain", false);
    auto pBand2SidechainMidi = std::make_unique<juce::AudioParameterBool>("band2SidechainMidi", "Band2SidechainMidi", false);
    auto pBand2Spectral = std::make_unique<juce::AudioParameterBool>("band2Spectral", "Band2Spectral", false);
    auto pBand2DynThreshold = std::make_unique<juce::AudioParameterFloat>(
        "band2DynThreshold", "Band2DynThreshold",
        juce::NormalisableRange<float> (-120.0f, 0.0f, 0.1f), -24.0f);
    auto pBand2AttackMs = std::make_unique<juce::AudioParameterFloat>(
        "band2AttackMs", "Band2AttackMs",
        DynamicEq::attackMsRange(), DynamicEq::attackMs);
    auto pBand2ReleaseMs = std::make_unique<juce::AudioParameterFloat>(
        "band2ReleaseMs", "Band2ReleaseMs",
        DynamicEq::releaseMsRange(), DynamicEq::releaseMs);
    auto pBand2SpectralResHz = std::make_unique<juce::AudioParameterFloat>(
        "band2SpectralResHz", "Band2SpectralResHz",
        juce::NormalisableRange<float> (SpectralBinning::kMinBandwidthHz, SpectralBinning::kTargetBandwidthHz, 0.05f),
        SpectralBinning::kDefaultBandwidthHz);
    auto pBand2SpectralDepth = std::make_unique<juce::AudioParameterFloat>(
        "band2SpectralDepth", "Band2SpectralAmount",
        juce::NormalisableRange<float> (SpectralDynamics::kMinSpectralAmount, SpectralDynamics::kMaxSpectralAmount, 0.01f),
        SpectralDynamics::kDefaultSpectralAmount);
    auto pBand2SpectralExpand = std::make_unique<juce::AudioParameterBool>(
        "band2SpectralExpand", "Band2SpectralExpand", false);
    auto pBand2Sat = std::make_unique<juce::AudioParameterBool>(
        "band2Sat", "Band2Sat", false);
    auto pBand2SatModel = std::make_unique<juce::AudioParameterChoice>(
        "band2SatModel", "Band2SatModel", BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pBand2SatPost = std::make_unique<juce::AudioParameterBool>(
        "band2SatPost", "Band2SatPost", false);

    //Band3
    auto pBand3Frequency = std::make_unique<juce::AudioParameterFloat>("band3Frequency", "Band3Frequency", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 4000.0f);
    auto pBand3Gain = std::make_unique<juce::AudioParameterFloat>("band3Gain", "Band3Gain", -24.0f, 24.0f, 0.0f);
    auto pBand3Q = std::make_unique<juce::AudioParameterFloat>("band3Q", "Band3Q", juce::NormalisableRange < float>(0.15f, 10.0f, 0.01f, 0.25f), defaultBellQ);
    auto pBand3OnOff = std::make_unique<juce::AudioParameterBool>("band3OnOff", "Band3OnOff", false);
    auto pBand3Type = std::make_unique<juce::AudioParameterChoice>("band3Type", "Band3Type", FilterType::getChoiceNames(), FilterType::bell);
    auto pBand3Channel = std::make_unique<juce::AudioParameterChoice>("band3Channel", "Band3Channel", BandChannel::getChoiceNames(), BandChannel::stereo);
    auto pBand3Dynamic = std::make_unique<juce::AudioParameterBool>("band3Dynamic", "Band3Dynamic", false);
    auto pBand3Sidechain = std::make_unique<juce::AudioParameterBool>("band3Sidechain", "Band3Sidechain", false);
    auto pBand3SidechainMidi = std::make_unique<juce::AudioParameterBool>("band3SidechainMidi", "Band3SidechainMidi", false);
    auto pBand3Spectral = std::make_unique<juce::AudioParameterBool>("band3Spectral", "Band3Spectral", false);
    auto pBand3DynThreshold = std::make_unique<juce::AudioParameterFloat>(
        "band3DynThreshold", "Band3DynThreshold",
        juce::NormalisableRange<float> (-120.0f, 0.0f, 0.1f), -24.0f);
    auto pBand3AttackMs = std::make_unique<juce::AudioParameterFloat>(
        "band3AttackMs", "Band3AttackMs",
        DynamicEq::attackMsRange(), DynamicEq::attackMs);
    auto pBand3ReleaseMs = std::make_unique<juce::AudioParameterFloat>(
        "band3ReleaseMs", "Band3ReleaseMs",
        DynamicEq::releaseMsRange(), DynamicEq::releaseMs);
    auto pBand3SpectralResHz = std::make_unique<juce::AudioParameterFloat>(
        "band3SpectralResHz", "Band3SpectralResHz",
        juce::NormalisableRange<float> (SpectralBinning::kMinBandwidthHz, SpectralBinning::kTargetBandwidthHz, 0.05f),
        SpectralBinning::kDefaultBandwidthHz);
    auto pBand3SpectralDepth = std::make_unique<juce::AudioParameterFloat>(
        "band3SpectralDepth", "Band3SpectralAmount",
        juce::NormalisableRange<float> (SpectralDynamics::kMinSpectralAmount, SpectralDynamics::kMaxSpectralAmount, 0.01f),
        SpectralDynamics::kDefaultSpectralAmount);
    auto pBand3SpectralExpand = std::make_unique<juce::AudioParameterBool>(
        "band3SpectralExpand", "Band3SpectralExpand", false);
    auto pBand3Sat = std::make_unique<juce::AudioParameterBool>(
        "band3Sat", "Band3Sat", false);
    auto pBand3SatModel = std::make_unique<juce::AudioParameterChoice>(
        "band3SatModel", "Band3SatModel", BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pBand3SatPost = std::make_unique<juce::AudioParameterBool>(
        "band3SatPost", "Band3SatPost", false);

    //Band4
    auto pBand4Frequency = std::make_unique<juce::AudioParameterFloat>("band4Frequency", "Band4Frequency", juce::NormalisableRange<float>(20.0f, 20000.0f, 1.0f, 0.2f), 8000.0f);
    auto pBand4Gain = std::make_unique<juce::AudioParameterFloat>("band4Gain", "Band4Gain", -24.0f, 24.0f, 0.0f);
    auto pBand4Q = std::make_unique<juce::AudioParameterFloat>("band4Q", "Band4Q", juce::NormalisableRange < float>(0.15f, 10.0f, 0.01f, 0.25f), defaultBellQ);
    auto pBand4OnOff = std::make_unique<juce::AudioParameterBool>("band4OnOff", "Band4OnOff", false);
    auto pBand4Type = std::make_unique<juce::AudioParameterChoice>("band4Type", "Band4Type", FilterType::getChoiceNames(), FilterType::bell);
    auto pBand4Channel = std::make_unique<juce::AudioParameterChoice>("band4Channel", "Band4Channel", BandChannel::getChoiceNames(), BandChannel::stereo);
    auto pBand4Dynamic = std::make_unique<juce::AudioParameterBool>("band4Dynamic", "Band4Dynamic", false);
    auto pBand4Sidechain = std::make_unique<juce::AudioParameterBool>("band4Sidechain", "Band4Sidechain", false);
    auto pBand4SidechainMidi = std::make_unique<juce::AudioParameterBool>("band4SidechainMidi", "Band4SidechainMidi", false);
    auto pBand4Spectral = std::make_unique<juce::AudioParameterBool>("band4Spectral", "Band4Spectral", false);
    auto pBand4DynThreshold = std::make_unique<juce::AudioParameterFloat>(
        "band4DynThreshold", "Band4DynThreshold",
        juce::NormalisableRange<float> (-120.0f, 0.0f, 0.1f), -24.0f);
    auto pBand4AttackMs = std::make_unique<juce::AudioParameterFloat>(
        "band4AttackMs", "Band4AttackMs",
        DynamicEq::attackMsRange(), DynamicEq::attackMs);
    auto pBand4ReleaseMs = std::make_unique<juce::AudioParameterFloat>(
        "band4ReleaseMs", "Band4ReleaseMs",
        DynamicEq::releaseMsRange(), DynamicEq::releaseMs);
    auto pBand4SpectralResHz = std::make_unique<juce::AudioParameterFloat>(
        "band4SpectralResHz", "Band4SpectralResHz",
        juce::NormalisableRange<float> (SpectralBinning::kMinBandwidthHz, SpectralBinning::kTargetBandwidthHz, 0.05f),
        SpectralBinning::kDefaultBandwidthHz);
    auto pBand4SpectralDepth = std::make_unique<juce::AudioParameterFloat>(
        "band4SpectralDepth", "Band4SpectralAmount",
        juce::NormalisableRange<float> (SpectralDynamics::kMinSpectralAmount, SpectralDynamics::kMaxSpectralAmount, 0.01f),
        SpectralDynamics::kDefaultSpectralAmount);
    auto pBand4SpectralExpand = std::make_unique<juce::AudioParameterBool>(
        "band4SpectralExpand", "Band4SpectralExpand", false);
    auto pBand4Sat = std::make_unique<juce::AudioParameterBool>(
        "band4Sat", "Band4Sat", false);
    auto pBand4SatModel = std::make_unique<juce::AudioParameterChoice>(
        "band4SatModel", "Band4SatModel", BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pBand4SatPost = std::make_unique<juce::AudioParameterBool>(
        "band4SatPost", "Band4SatPost", false);

    // Global sat oversampling for Stage 1 per-band (juce::dsp::Oversampling).
    auto pSatOversample = std::make_unique<juce::AudioParameterChoice>(
        BandSaturation::oversampleParamId(), "SatOversample",
        BandSaturation::getOversampleChoiceNames(), BandSaturation::osOff);

    // Stage 2 — post-Spectral bus sat (Expand can drive grit).
    auto pSpectralSat = std::make_unique<juce::AudioParameterBool>(
        BandSaturation::spectralSatParamId(), "SpectralSat", false);
    auto pSpectralSatModel = std::make_unique<juce::AudioParameterChoice>(
        BandSaturation::spectralSatModelParamId(), "SpectralSatModel",
        BandSaturation::getModelChoiceNames(), BandSaturation::tube);
    auto pSpectralSatDrive = std::make_unique<juce::AudioParameterFloat>(
        BandSaturation::spectralSatDriveParamId(), "SpectralSatDrive",
        juce::NormalisableRange<float> (BandSaturation::kMinSpectralSatDrive,
                                        BandSaturation::kMaxSpectralSatDrive, 0.01f),
        BandSaturation::kDefaultSpectralSatDrive);
    auto pSpectralSatOversample = std::make_unique<juce::AudioParameterChoice>(
        BandSaturation::spectralSatOversampleParamId(), "SpectralSatOversample",
        BandSaturation::getOversampleChoiceNames(), BandSaturation::osOff);

    // Global spectral lattice pack (shared by all S bands).
    auto pSpectralPack = std::make_unique<juce::AudioParameterChoice>(
        SpectralDynamics::spectralPackParamId(), "SpectralPack",
        SpectralDynamics::getPackModeChoiceNames(), 0);

    // Built-in analyser defaults match Documents/.../analyser_defaults.xml;
    // AnalyserDefaults::load() still overrides when that file is present.
    juce::StringArray choices = AnalyserDefaults::getBlockSizeNames();
    params.push_back(std::make_unique<juce::AudioParameterChoice>(
        "BLOCK_ID", "Block", choices, analyserDefaults.getBlockIndex (0))); // 2048

    // Boolean parameters
    params.push_back(std::make_unique<juce::AudioParameterBool>("LEFT_ID", "Left", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("RIGHT_ID", "Right", false));
    params.push_back(std::make_unique<juce::AudioParameterBool>("BOTH_ID", "Both", true));
    params.push_back(std::make_unique<juce::AudioParameterBool>("BINS_ID", "Bins", analyserDefaults.getBool ("BINS_ID", true)));
    params.push_back(std::make_unique<juce::AudioParameterBool>("MAX_ID", "Max", analyserDefaults.getBool ("MAX_ID", true)));
    params.push_back(std::make_unique<juce::AudioParameterBool>("LIN_ID", "Lin", analyserDefaults.getBool ("LIN_ID", false)));
    params.push_back(std::make_unique<juce::AudioParameterBool>("LOG_ID", "Log", analyserDefaults.getBool ("LOG_ID", true)));
    params.push_back(std::make_unique<juce::AudioParameterBool>("ST_ID", "ST", analyserDefaults.getBool ("ST_ID", false)));
    params.push_back(std::make_unique<juce::AudioParameterBool>("RANGE_ID", "Range", true));

    // Integer parameters
    // Analysis + UI refresh interval in ms (Ableton Spectrum default ~60 ms).
    // Analysis thread FFTs the latest Block samples on this cadence (overlapping).
    params.push_back(std::make_unique<juce::AudioParameterInt>("REFRESH_ID", "Refresh", 16, 200, 60));
    params.push_back(std::make_unique<juce::AudioParameterInt>("AVG_ID", "Avg", 1, 8, 1));
    params.push_back(std::make_unique<juce::AudioParameterInt>("MAXIMUM_ID", "Maximum", -200, 40, 12));
    params.push_back(std::make_unique<juce::AudioParameterInt>("MINIMUM_ID", "Minimum", -380, 30, -120));
    // How long spectrum peak-hold takes to fade from a peak down to the noise floor.
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        "MAX_HOLD_ID", "MaxHold",
        juce::NormalisableRange<float> (0.25f, 30.0f, 0.01f, 0.45f),
        analyserDefaults.getFloat ("MAX_HOLD_ID", 30.0f)));

    // Spectrum appearance (opacity / stroke) — no colour controls yet.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_ANALYSER_ID", "SpectrumAnalyser", analyserDefaults.getBool ("SPECTRUM_ANALYSER_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_PRE_CURVE_ID", "SpectrumPreCurve", analyserDefaults.getBool ("SPECTRUM_PRE_CURVE_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_PRE_FILL_ID", "SpectrumPreFill", analyserDefaults.getBool ("SPECTRUM_PRE_FILL_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_POST_CURVE_ID", "SpectrumPostCurve", analyserDefaults.getBool ("SPECTRUM_POST_CURVE_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_POST_FILL_ID", "SpectrumPostFill", analyserDefaults.getBool ("SPECTRUM_POST_FILL_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_HOLD_FILL_ID", "SpectrumHoldFill", analyserDefaults.getBool ("SPECTRUM_HOLD_FILL_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_OPACITY_ID", "SpectrumOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("SPECTRUM_OPACITY_ID", 100.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_FILL_OPACITY_ID", "SpectrumFillOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("SPECTRUM_FILL_OPACITY_ID", 100.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_PATH_WIDTH_ID", "SpectrumPathWidth",
        juce::NormalisableRange<float> (0.5f, 8.0f, 0.05f),
        analyserDefaults.getFloat ("SPECTRUM_PATH_WIDTH_ID", 2.0f)));
    // Spectrum curve on/off density gate (Show Bins sets 0 or 100).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_RESOLUTION_ID", "SpectrumResolution",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("SPECTRUM_RESOLUTION_ID", 96.8f)));
    // Spectrum curve display smoothness (Off/Low/Med/High). Cosmetic only —
    // true LF FFT resolution comes from BLOCK_ID (2048–16384).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "SPECTRUM_CURVE_RES_ID", "SpectrumCurveSmoothness",
        juce::StringArray { "Off", "Low", "Med", "High" },
        juce::jlimit (0, 3, analyserDefaults.getInt ("SPECTRUM_CURVE_RES_ID", 2)))); // Med
    // Show Bars — FFT bin overlay on/off (density is FFT_RESOLUTION_ID).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_FFT_BINS_ID", "SpectrumFftBins", analyserDefaults.getBool ("SPECTRUM_FFT_BINS_ID", true)));

    // EQ curve display vertical range (±6 / ±12 / ±24 dB). Default ±24.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "EQ_DISPLAY_RANGE_ID", "EqDisplayRange",
        juce::StringArray { "6", "12", "24" },
        juce::jlimit (0, 2, analyserDefaults.getInt ("EQ_DISPLAY_RANGE_ID", 2))));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "EQ_BAND_PATH_WIDTH_ID", "EqBandPathWidth",
        juce::NormalisableRange<float> (0.5f, 8.0f, 0.05f),
        analyserDefaults.getFloat ("EQ_BAND_PATH_WIDTH_ID", 3.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "EQ_SUM_PATH_WIDTH_ID", "EqSumPathWidth",
        juce::NormalisableRange<float> (0.5f, 8.0f, 0.05f),
        analyserDefaults.getFloat ("EQ_SUM_PATH_WIDTH_ID", 3.0f)));
    // Sum (cumulative EQ) glow — settings kept, enable off by default.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "EQ_SUM_GLOW_ENABLE_ID", "EqSumGlowEnable",
        analyserDefaults.getBool ("EQ_SUM_GLOW_ENABLE_ID", false)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "EQ_SUM_GLOW_RADIUS_ID", "EqSumGlowRadius",
        juce::NormalisableRange<float> (0.0f, 80.0f, 0.1f),
        analyserDefaults.getFloat ("EQ_SUM_GLOW_RADIUS_ID", 14.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "EQ_SUM_GLOW_SPREAD_ID", "EqSumGlowSpread",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f),
        analyserDefaults.getFloat ("EQ_SUM_GLOW_SPREAD_ID", 2.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "EQ_SUM_GLOW_OPACITY_ID", "EqSumGlowOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("EQ_SUM_GLOW_OPACITY_ID", 70.0f)));
    // Spectrum post-curve glow — on by default.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "SPECTRUM_GLOW_ENABLE_ID", "SpectrumGlowEnable",
        analyserDefaults.getBool ("SPECTRUM_GLOW_ENABLE_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_GLOW_RADIUS_ID", "SpectrumGlowRadius",
        juce::NormalisableRange<float> (0.0f, 80.0f, 0.1f),
        analyserDefaults.getFloat ("SPECTRUM_GLOW_RADIUS_ID", 12.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_GLOW_SPREAD_ID", "SpectrumGlowSpread",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f),
        analyserDefaults.getFloat ("SPECTRUM_GLOW_SPREAD_ID", 2.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "SPECTRUM_GLOW_OPACITY_ID", "SpectrumGlowOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("SPECTRUM_GLOW_OPACITY_ID", 70.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_LINE_OPACITY_ID", "OscLineOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_LINE_OPACITY_ID", 100.0f)));
    // Compact strip (minimized scope).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_LINE_WIDTH_ID", "OscLineWidth",
        juce::NormalisableRange<float> (0.5f, 8.0f, 0.05f),
        analyserDefaults.getFloat ("OSC_LINE_WIDTH_ID", 1.0f)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "OSC_GLOW_ENABLE_ID", "OscGlowEnable",
        analyserDefaults.getBool ("OSC_GLOW_ENABLE_ID", false)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_GLOW_RADIUS_ID", "OscGlowRadius",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_GLOW_RADIUS_ID", 6.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_GLOW_SPREAD_ID", "OscGlowSpread",
        juce::NormalisableRange<float> (0.0f, 20.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_GLOW_SPREAD_ID", 1.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_GLOW_OPACITY_ID", "OscGlowOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_GLOW_OPACITY_ID", 75.0f)));
    // Expanded (fullscreen) scope — independent line/glow tuning.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_EXPANDED_LINE_WIDTH_ID", "OscExpandedLineWidth",
        juce::NormalisableRange<float> (0.5f, 8.0f, 0.05f),
        analyserDefaults.getFloat ("OSC_EXPANDED_LINE_WIDTH_ID", 2.5f)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "OSC_EXPANDED_GLOW_ENABLE_ID", "OscExpandedGlowEnable",
        analyserDefaults.getBool ("OSC_EXPANDED_GLOW_ENABLE_ID", false)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_EXPANDED_GLOW_RADIUS_ID", "OscExpandedGlowRadius",
        juce::NormalisableRange<float> (0.0f, 80.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_EXPANDED_GLOW_RADIUS_ID", 18.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_EXPANDED_GLOW_SPREAD_ID", "OscExpandedGlowSpread",
        juce::NormalisableRange<float> (0.0f, 40.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_EXPANDED_GLOW_SPREAD_ID", 2.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "OSC_EXPANDED_GLOW_OPACITY_ID", "OscExpandedGlowOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("OSC_EXPANDED_GLOW_OPACITY_ID", 75.0f)));
    // 0 = Draft (direct stroke + cheap glow), 1 = High (2x supersampled stroke + full glow).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "OSC_QUALITY_ID", "OscQuality",
        juce::StringArray { "Draft", "High" },
        juce::jlimit (0, 1, analyserDefaults.getInt ("OSC_QUALITY_ID", 1))));
    // Multicolor band fills (default on). Off → neutral golden-yellow boost/cut fills.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "EQ_MULTICOLOR_BAND_FILL_ID", "MulticolorBandFill",
        analyserDefaults.getBool ("EQ_MULTICOLOR_BAND_FILL_ID", true)));
    // Graph cursor crosshair (default on).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "EQ_SHOW_CROSSHAIR_ID", "ShowCrosshair",
        analyserDefaults.getBool ("EQ_SHOW_CROSSHAIR_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "FFT_FULL_HEIGHT_ID", "FftFullHeight", analyserDefaults.getBool ("FFT_FULL_HEIGHT_ID", false)));
    // FFT bar draw density: 0 = use default when bars on, 100 = densest / per-bin.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_RESOLUTION_ID", "FftResolution",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_RESOLUTION_ID", 32.9f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_OPACITY_ID", "FftOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_OPACITY_ID", 66.6f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_BAR_WIDTH_ID", "FftBarWidth",
        juce::NormalisableRange<float> (10.0f, 150.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_BAR_WIDTH_ID", 112.9f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_INTENSITY_ID", "FftIntensity",
        juce::NormalisableRange<float> (25.0f, 300.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_INTENSITY_ID", 222.1f)));
    // Soft gate on quiet bins for colour mapping (0 = linear-ish, 100 = strong presence focus).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_THRESHOLD_ID", "FftThreshold",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_THRESHOLD_ID", 95.8f)));
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "FFT_GLOW_ENABLE_ID", "FftGlowEnable",
        analyserDefaults.getBool ("FFT_GLOW_ENABLE_ID", true)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_GLOW_RADIUS_ID", "FftGlowRadius",
        juce::NormalisableRange<float> (0.0f, 250.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_GLOW_RADIUS_ID", 94.1f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_GLOW_SPREAD_ID", "FftGlowSpread",
        juce::NormalisableRange<float> (0.0f, 250.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_GLOW_SPREAD_ID", 15.1f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_GLOW_OPACITY_ID", "FftGlowOpacity",
        juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_GLOW_OPACITY_ID", 85.5f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_GLOW_OFFSET_X_ID", "FftGlowOffsetX",
        juce::NormalisableRange<float> (-40.0f, 40.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_GLOW_OFFSET_X_ID", 0.0f)));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "FFT_GLOW_OFFSET_Y_ID", "FftGlowOffsetY",
        juce::NormalisableRange<float> (-40.0f, 40.0f, 0.1f),
        analyserDefaults.getFloat ("FFT_GLOW_OFFSET_Y_ID", 0.0f)));

    // Level meters (output metering / readouts). Defaults favour readable averages
    // and peak-aware clip indication (Ableton-style sample peak).
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "METER_MODE_ID", "MeterMode",
        juce::StringArray { "Peak", "RMS", "Peak+RMS" },
        juce::jlimit (0, 2, analyserDefaults.getInt ("METER_MODE_ID", 2)))); // Peak+RMS
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        "METER_CHANNEL_MODE_ID", "MeterChannelMode",
        juce::StringArray { "L/R", "M/S" },
        juce::jlimit (0, 1, analyserDefaults.getInt ("METER_CHANNEL_MODE_ID", 0)))); // default L/R
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "METER_READOUT_INTEGRATION_ID", "MeterReadoutIntegration",
        juce::NormalisableRange<float> (50.0f, 2000.0f, 1.0f, 0.45f),
        analyserDefaults.getFloat ("METER_READOUT_INTEGRATION_ID", 450.0f))); // ms
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "METER_FALL_ID", "MeterFall",
        juce::NormalisableRange<float> (0.15f, 2.0f, 0.01f, 0.5f),
        analyserDefaults.getFloat ("METER_FALL_ID", 0.75f))); // seconds
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "METER_PEAK_HOLD_ID", "MeterPeakHold",
        juce::NormalisableRange<float> (0.0f, 5.0f, 0.01f, 0.5f),
        analyserDefaults.getFloat ("METER_PEAK_HOLD_ID", 1.5f))); // seconds
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "METER_CLIP_HOLD_ID", "MeterClipHold",
        juce::NormalisableRange<float> (0.5f, 10.0f, 0.01f, 0.5f),
        analyserDefaults.getFloat ("METER_CLIP_HOLD_ID", 3.0f))); // seconds
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "METER_CLIP_THRESHOLD_ID", "MeterClipThreshold",
        juce::NormalisableRange<float> (-3.0f, 0.0f, 0.1f),
        analyserDefaults.getFloat ("METER_CLIP_THRESHOLD_ID", 0.0f))); // dBFS

    // Master output gain after all EQ bands (matches band gain range).
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        "outputGain", "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f),
        0.0f));

    // Autogain: compensates EQ loudness change via an internal offset (not the Out knob).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "autoGain", "Auto Gain", false));

    // Side Check (S<=M): global post-EQ BP-lattice bus — tuck Side when louder than Mid per slice.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        SideCheck::enabledParamId(), "Side Check", false));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        SideCheck::amountParamId(), "Side Check Amount",
        juce::NormalisableRange<float> (SideCheck::kMinAmount, SideCheck::kMaxAmount, 0.01f),
        SideCheck::kDefaultAmount));
    // HP/LP — Side Check only ducks slices whose centre falls between these freqs.
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        SideCheck::hpHzParamId(), "Side Check HP",
        juce::NormalisableRange<float> (SideCheck::kMinHpLpHz, SideCheck::kMaxFreqHz, 1.0f, 0.2f),
        SideCheck::kDefaultHpHz));
    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        SideCheck::lpHzParamId(), "Side Check LP",
        juce::NormalisableRange<float> (SideCheck::kMinHpLpHz, SideCheck::kMaxFreqHz, 1.0f, 0.2f),
        SideCheck::kDefaultLpHz));
    // Fast / Med / Slow envelope ballistics (no exposed A/R knobs). Default Fast for harshness tuck.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        SideCheck::modeParamId(), "Side Check Speed",
        SideCheck::getModeChoiceNames(),
        SideCheck::fast));
    // HQ on = full BP lattice (default); off = 3-band shelf/bell eco path.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        SideCheck::hqParamId(), "Side Check HQ", SideCheck::kDefaultHq));

    // Proportional Q (Ableton-style): tighten peaking Q as |gain| rises. Default off.
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "PROPORTIONAL_Q_ID", "Proportional Q", false));

    // Processing / phase mode (Pro-Q-style). Default = classic IIR minimum phase.
    params.push_back (std::make_unique<juce::AudioParameterChoice> (
        PhaseMode::paramId(), "Phase Mode",
        PhaseMode::getChoiceNames(),
        PhaseMode::minimumPhase));

    // LFO + Shape + mod matrix (3 voices + custom curve → band freq/gain/Q).
    LfoMod::addParameters (params);
    ShapeMod::addParameters (params);

    // In-UI / host-linked bypass — dry pass-through (excluded from A/B snapshots).
    params.push_back (std::make_unique<juce::AudioParameterBool> (
        "bypass", "Bypass", false));

    
    params.push_back(std::move(pHighpassCutoff));
    params.push_back(std::move(pHighpassQ));
    params.push_back(std::move(pHighpassSlope));
    params.push_back(std::move(pHighpassChannel));
    
    params.push_back(std::move(pLowpassCutoff));
    params.push_back(std::move(pLowpassQ));
    params.push_back(std::move(pLowpassSlope));
    params.push_back(std::move(pLowpassChannel));
    
    params.push_back(std::move(pHighShelfGain));
    params.push_back(std::move(pHighShelfFrequency));
    params.push_back(std::move(pHighShelfQ));
    params.push_back(std::move(pHighShelfType));
    params.push_back(std::move(pHighShelfChannel));
    params.push_back(std::move(pHighShelfDynamic));
    params.push_back(std::move(pHighShelfSidechain));
    params.push_back(std::move(pHighShelfSidechainMidi));
    params.push_back(std::move(pHighShelfSpectral));
    params.push_back(std::move(pHighShelfDynThreshold));
    params.push_back(std::move(pHighShelfAttackMs));
    params.push_back(std::move(pHighShelfReleaseMs));
    params.push_back(std::move(pHighShelfSpectralResHz));
    params.push_back(std::move(pHighShelfSpectralDepth));
    params.push_back(std::move(pHighShelfSpectralExpand));
    params.push_back(std::move(pHighShelfSat));
    params.push_back(std::move(pHighShelfSatModel));
    params.push_back(std::move(pHighShelfSatPost));
   
    params.push_back(std::move(pLowShelfGain));
    params.push_back(std::move(pLowShelfFrequency));
    params.push_back(std::move(pLowShelfQ));
    params.push_back(std::move(pLowShelfType));
    params.push_back(std::move(pLowShelfChannel));
    params.push_back(std::move(pLowShelfDynamic));
    params.push_back(std::move(pLowShelfSidechain));
    params.push_back(std::move(pLowShelfSidechainMidi));
    params.push_back(std::move(pLowShelfSpectral));
    params.push_back(std::move(pLowShelfDynThreshold));
    params.push_back(std::move(pLowShelfAttackMs));
    params.push_back(std::move(pLowShelfReleaseMs));
    params.push_back(std::move(pLowShelfSpectralResHz));
    params.push_back(std::move(pLowShelfSpectralDepth));
    params.push_back(std::move(pLowShelfSpectralExpand));
    params.push_back(std::move(pLowShelfSat));
    params.push_back(std::move(pLowShelfSatModel));
    params.push_back(std::move(pLowShelfSatPost));

    params.push_back(std::move(pBand1Gain));
    params.push_back(std::move(pBand1Frequency));
    params.push_back(std::move(pBand1Q));
    params.push_back(std::move(pBand1Type));
    params.push_back(std::move(pBand1Channel));
    params.push_back(std::move(pBand1Dynamic));
    params.push_back(std::move(pBand1Sidechain));
    params.push_back(std::move(pBand1SidechainMidi));
    params.push_back(std::move(pBand1Spectral));
    params.push_back(std::move(pBand1DynThreshold));
    params.push_back(std::move(pBand1AttackMs));
    params.push_back(std::move(pBand1ReleaseMs));
    params.push_back(std::move(pBand1SpectralResHz));
    params.push_back(std::move(pBand1SpectralDepth));
    params.push_back(std::move(pBand1SpectralExpand));
    params.push_back(std::move(pBand1Sat));
    params.push_back(std::move(pBand1SatModel));
    params.push_back(std::move(pBand1SatPost));

    params.push_back(std::move(pBand2Gain));
    params.push_back(std::move(pBand2Frequency));
    params.push_back(std::move(pBand2Q));
    params.push_back(std::move(pBand2Type));
    params.push_back(std::move(pBand2Channel));
    params.push_back(std::move(pBand2Dynamic));
    params.push_back(std::move(pBand2Sidechain));
    params.push_back(std::move(pBand2SidechainMidi));
    params.push_back(std::move(pBand2Spectral));
    params.push_back(std::move(pBand2DynThreshold));
    params.push_back(std::move(pBand2AttackMs));
    params.push_back(std::move(pBand2ReleaseMs));
    params.push_back(std::move(pBand2SpectralResHz));
    params.push_back(std::move(pBand2SpectralDepth));
    params.push_back(std::move(pBand2SpectralExpand));
    params.push_back(std::move(pBand2Sat));
    params.push_back(std::move(pBand2SatModel));
    params.push_back(std::move(pBand2SatPost));

    params.push_back(std::move(pBand3Gain));
    params.push_back(std::move(pBand3Frequency));
    params.push_back(std::move(pBand3Q));
    params.push_back(std::move(pBand3Type));
    params.push_back(std::move(pBand3Channel));
    params.push_back(std::move(pBand3Dynamic));
    params.push_back(std::move(pBand3Sidechain));
    params.push_back(std::move(pBand3SidechainMidi));
    params.push_back(std::move(pBand3Spectral));
    params.push_back(std::move(pBand3DynThreshold));
    params.push_back(std::move(pBand3AttackMs));
    params.push_back(std::move(pBand3ReleaseMs));
    params.push_back(std::move(pBand3SpectralResHz));
    params.push_back(std::move(pBand3SpectralDepth));
    params.push_back(std::move(pBand3SpectralExpand));
    params.push_back(std::move(pBand3Sat));
    params.push_back(std::move(pBand3SatModel));
    params.push_back(std::move(pBand3SatPost));

    params.push_back(std::move(pBand4Gain));
    params.push_back(std::move(pBand4Frequency));
    params.push_back(std::move(pBand4Q));
    params.push_back(std::move(pBand4Type));
    params.push_back(std::move(pBand4Channel));
    params.push_back(std::move(pBand4Dynamic));
    params.push_back(std::move(pBand4Sidechain));
    params.push_back(std::move(pBand4SidechainMidi));
    params.push_back(std::move(pBand4Spectral));
    params.push_back(std::move(pBand4DynThreshold));
    params.push_back(std::move(pBand4AttackMs));
    params.push_back(std::move(pBand4ReleaseMs));
    params.push_back(std::move(pBand4SpectralResHz));
    params.push_back(std::move(pBand4SpectralDepth));
    params.push_back(std::move(pBand4SpectralExpand));
    params.push_back(std::move(pBand4Sat));
    params.push_back(std::move(pBand4SatModel));
    params.push_back(std::move(pBand4SatPost));

    params.push_back(std::move(pSatOversample));
    params.push_back(std::move(pSpectralSat));
    params.push_back(std::move(pSpectralSatModel));
    params.push_back(std::move(pSpectralSatDrive));
    params.push_back(std::move(pSpectralSatOversample));
    params.push_back(std::move(pSpectralPack));

    params.push_back(std::move(pHighpassOnOff));
    params.push_back(std::move(pLowpassOnOff));
    params.push_back(std::move(pHighShelfOnOff));
    params.push_back(std::move(pLowShelfOnOff));
    params.push_back(std::move(pBand1OnOff));
    params.push_back(std::move(pBand2OnOff));
    params.push_back(std::move(pBand3OnOff));
    params.push_back(std::move(pBand4OnOff));


    return { params.begin(), params.end() };
}



void EqProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    DBG("parameterChanged called. Parameter ID: " << parameterID << ", New Value: " << newValue);

    if (parameterID == "highpassCutoff") {
        smoothknob1.setTargetValue(newValue);

    }
    else if (parameterID == "highpassQ") {
        smoothknob2.setTargetValue(newValue);
    }

    if (parameterID == "lowpassCutoff") {
        smoothknob3.setTargetValue(newValue);
    }
    else if (parameterID == "lowpassQ") {
        smoothknob4.setTargetValue(newValue);
    }

    if (parameterID == "highShelfFrequency") {
        smoothknob5.setTargetValue(newValue);
    }
    else if (parameterID == "highShelfQ") {
        smoothknob6.setTargetValue(newValue);
    }
    else if (parameterID == "highShelfGain") {
        smoothknob7.setTargetValue(newValue);
    }

    if (parameterID == "lowShelfFrequency") {
        smoothknob8.setTargetValue(newValue);
    }
    else if (parameterID == "lowShelfQ") {
        smoothknob9.setTargetValue(newValue);
    }
    else if (parameterID == "lowShelfGain") {
        smoothknob10.setTargetValue(newValue);
    }

    if (parameterID == "band1Frequency") {
        smoothknob11.setTargetValue(newValue);
    }
    else if (parameterID == "band1Q") {
        smoothknob12.setTargetValue(newValue);
    }
    else if (parameterID == "band1Gain") {
        smoothknob13.setTargetValue(newValue);
    }

    if (parameterID == "band2Frequency") {
        smoothknob14.setTargetValue(newValue);
    }
    else if (parameterID == "band2Q") {
        smoothknob15.setTargetValue(newValue);
    }
    else if (parameterID == "band2Gain") {
        smoothknob16.setTargetValue(newValue);
    }

    if (parameterID == "band3Frequency") {
        smoothknob17.setTargetValue(newValue);
    }
    else if (parameterID == "band3Q") {
        smoothknob18.setTargetValue(newValue);
    }
    else if (parameterID == "band3Gain") {
        smoothknob19.setTargetValue(newValue);
    }

    if (parameterID == "band4Frequency") {
        smoothknob20.setTargetValue(newValue);
    }
    else if (parameterID == "band4Q") {
        smoothknob21.setTargetValue(newValue);
    }
    else if (parameterID == "band4Gain") {
        smoothknob22.setTargetValue(newValue);
    }

    if (parameterID == "outputGain") {
        smoothOutputGain.setTargetValue(newValue);
    }



if (parameterID == "highpassOnOff")
{
    if (newValue > 0.5f)
    {
        isHighpassOn = true;
    }
    else
    {
        isHighpassOn = false;
    }
}

if (parameterID == "lowpassOnOff")
{
    if (newValue > 0.5f)
    {
        isLowpassOn = true;
    }
    else
    {
        isLowpassOn = false;
    }
}

if (parameterID == "highShelfOnOff")
{
    if (newValue > 0.5f)
    {
        isHighShelfOn = true;
    }
    else
    {
        isHighShelfOn = false;
    }
}

if (parameterID == "lowShelfOnOff")
{
    if (newValue > 0.5f)
    {
        isLowShelfOn = true;
    }
    else
    {
        isLowShelfOn = false;
    }
}

if (parameterID == "band1OnOff")
{
    if (newValue > 0.5f)
    {
        isBand1On = true;
    }
    else
    {
        isBand1On = false;
    }
}

if (parameterID == "band2OnOff")
{
    if (newValue > 0.5f)
    {
        isBand2On = true;
    }
    else
    {
        isBand2On = false;
    }
}

if (parameterID == "band3OnOff")
{
    if (newValue > 0.5f)
    {
        isBand3On = true;
    }
    else
    {
        isBand3On = false;
    }
}

if (parameterID == "band4OnOff")
{
    if (newValue > 0.5f)
    {
        isBand4On = true;
    }
    else
    {
        isBand4On = false;
    }
}


        
}

void EqProcessor::updateParameters()
{
    // For Initializing the Coefficents with Smoothed Values
    
    // Get the restored values
    float restoredHighpassCutoff = treeState.getRawParameterValue("highpassCutoff")->load();
    float restoredHighpassQ = treeState.getRawParameterValue("highpassQ")->load();
    
    float restoredLowpassCutoff = treeState.getRawParameterValue("lowpassCutoff")->load();
    float restoredLowpassQ = treeState.getRawParameterValue("lowpassQ")->load();
    
    float restoredHighShelfFrequency = treeState.getRawParameterValue("highShelfFrequency")->load();
    float restoredHighShelfQ = treeState.getRawParameterValue("highShelfQ")->load();
    float restoredHighShelfGain = treeState.getRawParameterValue("highShelfGain")->load();

    float restoredLowShelfFrequency = treeState.getRawParameterValue("lowShelfFrequency")->load();
    float restoredLowShelfQ = treeState.getRawParameterValue("lowShelfQ")->load();
    float restoredLowShelfGain = treeState.getRawParameterValue("lowShelfGain")->load();

    float restoredBand1Frequency = treeState.getRawParameterValue("band1Frequency")->load();
    float restoredBand1Q = treeState.getRawParameterValue("band1Q")->load();
    float restoredBand1Gain = treeState.getRawParameterValue("band1Gain")->load();

    float restoredBand2Frequency = treeState.getRawParameterValue("band2Frequency")->load();
    float restoredBand2Q = treeState.getRawParameterValue("band2Q")->load();
    float restoredBand2Gain = treeState.getRawParameterValue("band2Gain")->load();

    float restoredBand3Frequency = treeState.getRawParameterValue("band3Frequency")->load();
    float restoredBand3Q = treeState.getRawParameterValue("band3Q")->load();
    float restoredBand3Gain = treeState.getRawParameterValue("band3Gain")->load();

    float restoredBand4Frequency = treeState.getRawParameterValue("band4Frequency")->load();
    float restoredBand4Q = treeState.getRawParameterValue("band4Q")->load();
    float restoredBand4Gain = treeState.getRawParameterValue("band4Gain")->load();

    publishedBand1Freq.store (restoredBand1Frequency, std::memory_order_relaxed);
    publishedBand1Q.store (restoredBand1Q, std::memory_order_relaxed);
    publishedBand2Freq.store (restoredBand2Frequency, std::memory_order_relaxed);
    publishedBand2Q.store (restoredBand2Q, std::memory_order_relaxed);
    publishedBand3Freq.store (restoredBand3Frequency, std::memory_order_relaxed);
    publishedBand3Q.store (restoredBand3Q, std::memory_order_relaxed);
    publishedBand4Freq.store (restoredBand4Frequency, std::memory_order_relaxed);
    publishedBand4Q.store (restoredBand4Q, std::memory_order_relaxed);
    publishedHighShelfFreq.store (restoredHighShelfFrequency, std::memory_order_relaxed);
    publishedHighShelfQ.store (restoredHighShelfQ, std::memory_order_relaxed);
    publishedLowShelfFreq.store (restoredLowShelfFrequency, std::memory_order_relaxed);
    publishedLowShelfQ.store (restoredLowShelfQ, std::memory_order_relaxed);
    dynBand1.publishEffectiveGain (restoredBand1Gain);
    dynBand2.publishEffectiveGain (restoredBand2Gain);
    dynBand3.publishEffectiveGain (restoredBand3Gain);
    dynBand4.publishEffectiveGain (restoredBand4Gain);
    dynHighShelf.publishEffectiveGain (restoredHighShelfGain);
    dynLowShelf.publishEffectiveGain (restoredLowShelfGain);


    // Update the smoothed knobs to the restored values
    smoothknob1.setCurrentAndTargetValue(restoredHighpassCutoff);
    smoothknob2.setCurrentAndTargetValue(restoredHighpassQ);
   
    smoothknob3.setCurrentAndTargetValue(restoredLowpassCutoff);
    smoothknob4.setCurrentAndTargetValue(restoredLowpassQ);

    smoothknob5.setCurrentAndTargetValue(restoredHighShelfFrequency);
    smoothknob6.setCurrentAndTargetValue(restoredHighShelfQ);
    smoothknob7.setCurrentAndTargetValue(restoredHighShelfGain);

    smoothknob8.setCurrentAndTargetValue(restoredLowShelfFrequency);
    smoothknob9.setCurrentAndTargetValue(restoredLowShelfQ);
    smoothknob10.setCurrentAndTargetValue(restoredLowShelfGain);

    smoothknob11.setCurrentAndTargetValue(restoredBand1Frequency);
    smoothknob12.setCurrentAndTargetValue(restoredBand1Q);
    smoothknob13.setCurrentAndTargetValue(restoredBand1Gain);

    smoothknob14.setCurrentAndTargetValue(restoredBand2Frequency);
    smoothknob15.setCurrentAndTargetValue(restoredBand2Q);
    smoothknob16.setCurrentAndTargetValue(restoredBand2Gain);

    smoothknob17.setCurrentAndTargetValue(restoredBand3Frequency);
    smoothknob18.setCurrentAndTargetValue(restoredBand3Q);
    smoothknob19.setCurrentAndTargetValue(restoredBand3Gain);

    smoothknob20.setCurrentAndTargetValue(restoredBand4Frequency);
    smoothknob21.setCurrentAndTargetValue(restoredBand4Q);
    smoothknob22.setCurrentAndTargetValue(restoredBand4Gain);

    if (auto* v = treeState.getRawParameterValue ("outputGain"))
        smoothOutputGain.setCurrentAndTargetValue (v->load());

    smoothAutoGainOffset.setCurrentAndTargetValue (0.0f);

    // Update the filters
    const int hpSlope = (int) treeState.getRawParameterValue ("highpassSlope")->load();
    const int lpSlope = (int) treeState.getRawParameterValue ("lowpassSlope")->load();
    updateHighpass(restoredHighpassCutoff, restoredHighpassQ, hpSlope);
    updateLowpass(restoredLowpassCutoff, restoredLowpassQ, lpSlope);
    updateHighShelf(restoredHighShelfFrequency, restoredHighShelfQ, restoredHighShelfGain);
    updateLowShelf(restoredLowShelfFrequency, restoredLowShelfQ, restoredLowShelfGain);
    updateBand1(restoredBand1Frequency, restoredBand1Q, restoredBand1Gain);
    updateBand2(restoredBand2Frequency, restoredBand2Q, restoredBand2Gain);
    updateBand3(restoredBand3Frequency, restoredBand3Q, restoredBand3Gain);
    updateBand4(restoredBand4Frequency, restoredBand4Q, restoredBand4Gain);

    auto onOff = [this] (const char* id) -> bool
    {
        if (auto* v = treeState.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };

    isHighpassOn  = onOff ("highpassOnOff");
    isLowpassOn   = onOff ("lowpassOnOff");
    isHighShelfOn = onOff ("highShelfOnOff");
    isLowShelfOn  = onOff ("lowShelfOnOff");
    isBand1On     = onOff ("band1OnOff");
    isBand2On     = onOff ("band2OnOff");
    isBand3On     = onOff ("band3OnOff");
    isBand4On     = onOff ("band4OnOff");
}


//==============================================================================
const juce::String EqProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EqProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool EqProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool EqProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double EqProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EqProcessor::getNumPrograms()
{
    return 1;
}

int EqProcessor::getCurrentProgram()
{
    return 0;
}

void EqProcessor::setCurrentProgram(int index)
{
}

const juce::String EqProcessor::getProgramName(int index)
{
    return {};
}

void EqProcessor::changeProgramName(int index, const juce::String& newName)
{
}

void EqProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    //DBG("Preparing to play called ");

    this->sampleRate = sampleRate;
    m_analyser.setSampleRate (sampleRate);
    if (auto* osc = oscilloscopeTarget.load (std::memory_order_acquire))
        osc->prepare (sampleRate);

    // Initialize dsp modules
    juce::dsp::ProcessSpec spec;
    spec.maximumBlockSize = samplesPerBlock;
    spec.sampleRate = sampleRate;
    spec.numChannels = getTotalNumOutputChannels();

    // Prepare DSP modules
    for (auto& stage : highpassStages)
        stage.prepare(spec);
    for (auto& stage : lowpassStages)
        stage.prepare(spec);
    highShelf.prepare(spec);
    lowShelf.prepare(spec);
    band1.prepare(spec);
    band2.prepare(spec);
    band3.prepare(spec);
    band4.prepare(spec);

    // Dynamic-EQ detectors are mono (pre-EQ sidechain).
    juce::dsp::ProcessSpec monoSpec = spec;
    monoSpec.numChannels = 1;
    dynBand1.prepare (monoSpec, samplesPerBlock);
    dynBand2.prepare (monoSpec, samplesPerBlock);
    dynBand3.prepare (monoSpec, samplesPerBlock);
    dynBand4.prepare (monoSpec, samplesPerBlock);
    dynHighShelf.prepare (monoSpec, samplesPerBlock);
    dynLowShelf.prepare (monoSpec, samplesPerBlock);

    scDetectBand1.prepare (monoSpec, samplesPerBlock);
    scDetectBand2.prepare (monoSpec, samplesPerBlock);
    scDetectBand3.prepare (monoSpec, samplesPerBlock);
    scDetectBand4.prepare (monoSpec, samplesPerBlock);
    scDetectHighShelf.prepare (monoSpec, samplesPerBlock);
    scDetectLowShelf.prepare (monoSpec, samplesPerBlock);
    scGateBand1.prepare (sampleRate, samplesPerBlock);
    scGateBand2.prepare (sampleRate, samplesPerBlock);
    scGateBand3.prepare (sampleRate, samplesPerBlock);
    scGateBand4.prepare (sampleRate, samplesPerBlock);
    scGateHighShelf.prepare (sampleRate, samplesPerBlock);
    scGateLowShelf.prepare (sampleRate, samplesPerBlock);
    for (auto& v : lfoVoices)
        v.reset();
    modEnvFollower.reset();
    shapeEngine.prepare();
    midiNotesHeld = 0;

    spectralEngine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, (int) spec.numChannels));
    sideCheck.prepare (sampleRate, samplesPerBlock);
    for (auto& sat : bandSatEngines)
        sat.prepare (sampleRate, samplesPerBlock, juce::jmax (1, (int) spec.numChannels));
    spectralSatEngine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, (int) spec.numChannels));
    linearPhaseEngine.prepare (sampleRate, samplesPerBlock, juce::jmax (1, (int) spec.numChannels));
    spectralDetectBuffer.setSize (juce::jmax (1, (int) spec.numChannels), samplesPerBlock, false, false, true);
    updateReportedLatency (false);


    // Reset smoothed values with appropriate time constants

    // Highpass
    smoothknob1.reset(sampleRate, 0.00025);
    smoothknob2.reset(sampleRate, 0.0005);
   
    // Lowpass
    smoothknob3.reset(sampleRate, 0.00025);
    smoothknob4.reset(sampleRate, 0.0005);
    
    // High Shelf
    smoothknob5.reset(sampleRate, 0.00025);
    smoothknob6.reset(sampleRate, 0.001);
    smoothknob7.reset(sampleRate, 0.0002);
  
    // Low Shelf
    smoothknob8.reset(sampleRate, 0.00025);
    smoothknob9.reset(sampleRate, 0.001);
    smoothknob10.reset(sampleRate, 0.0002);
   
    // Band 1
    smoothknob11.reset(sampleRate, 0.00025);    // Freq
    smoothknob12.reset(sampleRate, 0.001);    // Q
    smoothknob13.reset(sampleRate, 0.0002);     // Gain
   
    // Band 2
    smoothknob14.reset(sampleRate, 0.00025);
    smoothknob15.reset(sampleRate, 0.001);
    smoothknob16.reset(sampleRate, 0.0002);
    
    // Band 3
    smoothknob17.reset(sampleRate, 0.00025);
    smoothknob18.reset(sampleRate, 0.001);
    smoothknob19.reset(sampleRate, 0.0002);
    
    // Band 4
    smoothknob20.reset(sampleRate, 0.00025);
    smoothknob21.reset(sampleRate, 0.001);
    smoothknob22.reset(sampleRate, 0.0002);

    smoothOutputGain.reset (sampleRate, 0.0002);
    if (auto* v = treeState.getRawParameterValue ("outputGain"))
        smoothOutputGain.setCurrentAndTargetValue (v->load());

    // Autogain offset slews slowly to avoid pumping (~250 ms).
    smoothAutoGainOffset.reset (sampleRate, 0.25);
    smoothAutoGainOffset.setCurrentAndTargetValue (0.0f);

    inputPeakDbLeft.store (-100.0f);
    inputPeakDbRight.store (-100.0f);
    inputRmsDbLeft.store (-100.0f);
    inputRmsDbRight.store (-100.0f);
    postPeakDbLeft.store (-100.0f);
    postPeakDbRight.store (-100.0f);
    postRmsDbLeft.store (-100.0f);
    postRmsDbRight.store (-100.0f);
    inputPeakDbMid.store (-100.0f);
    inputPeakDbSide.store (-100.0f);
    inputRmsDbMid.store (-100.0f);
    inputRmsDbSide.store (-100.0f);
    postPeakDbMid.store (-100.0f);
    postPeakDbSide.store (-100.0f);
    postRmsDbMid.store (-100.0f);
    postRmsDbSide.store (-100.0f);

    // Update coefficients based on initial values
    updateParameters();

   

}



#ifndef JucePlugin_PreferredChannelConfigurations
bool EqProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    // Optional sidechain: disabled, mono, or stereo.
    if (layouts.inputBuses.size() > 1)
    {
        const auto sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled()
            && sc != juce::AudioChannelSet::mono()
            && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
#endif
}
#endif



//==============================================================================
void EqProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();;

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    // Main bus only — sidechain channels must not be EQ'd / metered as program audio.
    auto mainBuffer = getBusBuffer (buffer, true, 0);
    const float* scL = nullptr;
    const float* scR = nullptr;
    if (getBusCount (true) > 1)
    {
        auto* scBus = getBus (true, 1);
        if (scBus != nullptr && scBus->isEnabled())
        {
            auto scBuffer = getBusBuffer (buffer, true, 1);
            if (scBuffer.getNumChannels() > 0 && scBuffer.getNumSamples() > 0)
            {
                scL = scBuffer.getReadPointer (0);
                scR = scBuffer.getNumChannels() > 1 ? scBuffer.getReadPointer (1) : scL;
            }
        }
    }

    // MIDI note-ons routed into this plugin (Ableton: MIDI To this device).
    bool midiNoteOnEdge = false;
    for (const auto metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();
        if (msg.isNoteOn (false))
        {
            ++midiNotesHeld;
            midiNoteOnEdge = true;
        }
        else if (msg.isNoteOff())
        {
            midiNotesHeld = juce::jmax (0, midiNotesHeld - 1);
        }
        else if (msg.isAllNotesOff() || msg.isAllSoundOff())
        {
            midiNotesHeld = 0;
        }
    }

    const bool bypassed = bypassParam != nullptr && bypassParam->get();

    // Advance smoothers even while bypassed so un-bypass doesn't zipper.
    auto take = [numSamples] (juce::LinearSmoothedValue<float>& s) -> float
    {
        s.skip (numSamples);
        return s.getCurrentValue();
    };

    if (bypassed)
    {
        smoothAutoGainOffset.skip (numSamples);
        take (smoothOutputGain);
        take (smoothknob1);  take (smoothknob2);  take (smoothknob3);  take (smoothknob4);
        take (smoothknob5);  take (smoothknob6);  take (smoothknob7);  take (smoothknob8);
        take (smoothknob9);  take (smoothknob10); take (smoothknob11); take (smoothknob12);
        take (smoothknob13); take (smoothknob14); take (smoothknob15); take (smoothknob16);
        take (smoothknob17); take (smoothknob18); take (smoothknob19); take (smoothknob20);
        take (smoothknob21); take (smoothknob22);

        // Bypass: input == output. Measure sample-peak + RMS on the buffer leaving the plugin.
        auto storeChannelMeters = [&] (int channel,
                                       std::atomic<float>& peakOut,
                                       std::atomic<float>& rmsOut,
                                       std::atomic<float>& peakPost,
                                       std::atomic<float>& rmsPost)
        {
            if (channel < 0 || channel >= mainBuffer.getNumChannels() || numSamples <= 0)
            {
                peakOut.store (-100.0f);
                rmsOut.store (-100.0f);
                peakPost.store (-100.0f);
                rmsPost.store (-100.0f);
                return;
            }

            const float peakDb = juce::Decibels::gainToDecibels (mainBuffer.getMagnitude (channel, 0, numSamples), -100.0f);
            const float rmsDb = juce::Decibels::gainToDecibels (mainBuffer.getRMSLevel (channel, 0, numSamples), -100.0f);
            peakOut.store (peakDb);
            rmsOut.store (rmsDb);
            peakPost.store (peakDb);
            rmsPost.store (rmsDb);
        };

        storeChannelMeters (0, inputPeakDbLeft, inputRmsDbLeft, postPeakDbLeft, postRmsDbLeft);
        if (mainBuffer.getNumChannels() > 1)
            storeChannelMeters (1, inputPeakDbRight, inputRmsDbRight, postPeakDbRight, postRmsDbRight);
        else
        {
            inputPeakDbRight.store (inputPeakDbLeft.load());
            inputRmsDbRight.store (inputRmsDbLeft.load());
            postPeakDbRight.store (postPeakDbLeft.load());
            postRmsDbRight.store (postRmsDbLeft.load());
        }

        // Bypass: input == output, so Mid/Side taps match at both meter points.
        measureMidSideLevels (mainBuffer, numSamples,
                              inputPeakDbMid, inputRmsDbMid, inputPeakDbSide, inputRmsDbSide);
        postPeakDbMid.store (inputPeakDbMid.load());
        postRmsDbMid.store (inputRmsDbMid.load());
        postPeakDbSide.store (inputPeakDbSide.load());
        postRmsDbSide.store (inputRmsDbSide.load());

        const bool analyserOn = isSpectrumAnalyserActive();
        if (analyserOn && mainBuffer.getNumChannels() > 0)
        {
            const auto* left = mainBuffer.getReadPointer (0);
            const auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getReadPointer (1) : left;
            m_analyser.pushPreSamplesIntoFifo (left, right, numSamples);
            m_analyser.pushSamplesIntoFifo (left, right, numSamples);
        }

        if (auto* osc = oscilloscopeTarget.load (std::memory_order_acquire))
        {
            if (osc->isScopeEnabled() && mainBuffer.getNumChannels() > 0)
            {
                const auto* left = mainBuffer.getReadPointer (0);
                const auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getReadPointer (1) : left;
                if (auto* playHead = getPlayHead())
                    if (auto pos = playHead->getPosition())
                        if (auto bpmOpt = pos->getBpm())
                            osc->setHostBpm (*bpmOpt);
                osc->pushSamples (left, right, numSamples);
            }
        }

        updateReportedLatency (true);
        return;
    }

    juce::dsp::AudioBlock<float> audioBlock (mainBuffer);
    const int phaseMode = PhaseMode::readChoiceIndex (treeState);
    const bool useLinearPhase = (phaseMode == PhaseMode::linearPhase);

    if (phaseMode != lastPhaseMode)
    {
        if (useLinearPhase)
            linearPhaseEngine.reset();
        lastPhaseMode = phaseMode;
    }

    const float smoothedHighpassCutoff = take (smoothknob1);
    const float smoothedHighpassQ = take (smoothknob2);
    const float smoothedLowpassCutoff = take (smoothknob3);
    const float smoothedLowpassQ = take (smoothknob4);
    float smoothedHighShelfFrequency = take (smoothknob5);
    float smoothedHighShelfQ = take (smoothknob6);
    float smoothedHighShelfGain = take (smoothknob7);
    float smoothedLowShelfFrequency = take (smoothknob8);
    float smoothedLowShelfQ = take (smoothknob9);
    float smoothedLowShelfGain = take (smoothknob10);
    float smoothedBand1Frequency = take (smoothknob11);
    float smoothedBand1Q = take (smoothknob12);
    float smoothedBand1Gain = take (smoothknob13);
    float smoothedBand2Frequency = take (smoothknob14);
    float smoothedBand2Q = take (smoothknob15);
    float smoothedBand2Gain = take (smoothknob16);
    float smoothedBand3Frequency = take (smoothknob17);
    float smoothedBand3Q = take (smoothknob18);
    float smoothedBand3Gain = take (smoothknob19);
    float smoothedBand4Frequency = take (smoothknob20);
    float smoothedBand4Q = take (smoothknob21);
    float smoothedBand4Gain = take (smoothknob22);

    // LFO mod matrix → working freq/gain/Q (APVTS knobs stay unmodulated).
    {
        double bpm = 120.0;
        if (auto* playHead = getPlayHead())
            if (auto pos = playHead->getPosition())
                if (auto bpmOpt = pos->getBpm())
                    bpm = *bpmOpt;

        if (auto* osc = oscilloscopeTarget.load (std::memory_order_acquire))
            osc->setHostBpm (bpm);

        int shapes[LfoMod::kNumLfos] {};
        float rates[LfoMod::kNumLfos] {}, phases[LfoMod::kNumLfos] {};
        int retrigModes[LfoMod::kNumLfos] {};
        for (int i = 0; i < LfoMod::kNumLfos; ++i)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::shapeParamId (i))))
                shapes[i] = p->getIndex();
            rates[i] = LfoMod::resolveRateHz (treeState, i, bpm);
            if (auto* v = treeState.getRawParameterValue (LfoMod::phaseParamId (i)))
                phases[i] = v->load();
            retrigModes[i] = LfoMod::retrigOff;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::retriggerParamId (i))))
                retrigModes[i] = p->getIndex();
        }

        float envThresh = -24.0f, envAtt = 20.0f, envRel = 200.0f;
        if (auto* v = treeState.getRawParameterValue (LfoMod::envThresholdParamId()))
            envThresh = v->load();
        if (auto* v = treeState.getRawParameterValue (LfoMod::envAttackParamId()))
            envAtt = v->load();
        if (auto* v = treeState.getRawParameterValue (LfoMod::envReleaseParamId()))
            envRel = v->load();

        modEnvFollower.updateCoeffs (envAtt, envRel, sampleRate, numSamples);
        const float* envL = mainBuffer.getNumChannels() > 0 ? mainBuffer.getReadPointer (0) : nullptr;
        const float* envR = mainBuffer.getNumChannels() > 1 ? mainBuffer.getReadPointer (1) : envL;
        const float envAmount = modEnvFollower.process (envL, envR, numSamples, envThresh);
        publishedEnvAmount.store (envAmount, std::memory_order_relaxed);
        const float envBipolar = LfoMod::envAmountToBipolar (envAmount);

        // Shape modulator — same rate/sync/phase/retrig model as LFOs.
        {
            int shapeRetrigMode = LfoMod::retrigOff;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    treeState.getParameter (ShapeMod::retriggerParamId())))
                shapeRetrigMode = p->getIndex();
            if (LfoMod::shouldRetrigger (shapeRetrigMode, midiNoteOnEdge))
                shapeEngine.voice.reset();

            float shapePhaseDeg = 0.0f;
            if (auto* v = treeState.getRawParameterValue (ShapeMod::phaseParamId()))
                shapePhaseDeg = v->load();

            const float shapeRate = ShapeMod::resolveRateHz (treeState, bpm);
            const float shapePhase01 = shapeEngine.voice.processBlock (
                sampleRate, numSamples, shapeRate, shapePhaseDeg);
            const float shapeDrive = shapeEngine.evaluatePublished (shapePhase01);
            publishedShapePhase.store (shapePhase01, std::memory_order_relaxed);
            publishedShapeBipolar.store (shapeDrive, std::memory_order_relaxed);
        }

        const float shapeBipolar = publishedShapeBipolar.load (std::memory_order_relaxed);

        int slotSources[LfoMod::kNumMatrixSlots] {}, slotDests[LfoMod::kNumMatrixSlots] {};
        float slotAmounts[LfoMod::kNumMatrixSlots] {};
        bool slotEnabled[LfoMod::kNumMatrixSlots] {}, slotPolar[LfoMod::kNumMatrixSlots] {};
        for (int s = 0; s < LfoMod::kNumMatrixSlots; ++s)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::slotSourceParamId (s))))
                slotSources[s] = p->getIndex();
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::slotDestParamId (s))))
                slotDests[s] = p->getIndex();
            slotAmounts[s] = 50.0f;
            if (auto* v = treeState.getRawParameterValue (LfoMod::slotAmountParamId (s)))
                slotAmounts[s] = v->load();
            slotEnabled[s] = true;
            if (auto* v = treeState.getRawParameterValue (LfoMod::slotEnabledParamId (s)))
                slotEnabled[s] = v->load() > 0.5f;
            slotPolar[s] = treeState.getRawParameterValue (LfoMod::slotPolarParamId (s)) != nullptr
                           && treeState.getRawParameterValue (LfoMod::slotPolarParamId (s))->load() > 0.5f;
        }

        float lfoBipolarOut[LfoMod::kNumLfos] {};
        float lfoPhaseOut[LfoMod::kNumLfos] {};
        LfoMod::applyMatrix (lfoVoices, sampleRate, numSamples,
                             shapes, rates, phases, retrigModes,
                             midiNoteOnEdge,
                             envBipolar, shapeBipolar,
                             slotSources, slotDests, slotAmounts, slotEnabled, slotPolar,
                             lfoBipolarOut, lfoPhaseOut,
                             smoothedBand1Frequency, smoothedBand1Gain, smoothedBand1Q,
                             smoothedBand2Frequency, smoothedBand2Gain, smoothedBand2Q,
                             smoothedBand3Frequency, smoothedBand3Gain, smoothedBand3Q,
                             smoothedBand4Frequency, smoothedBand4Gain, smoothedBand4Q,
                             smoothedHighShelfFrequency, smoothedHighShelfGain, smoothedHighShelfQ,
                             smoothedLowShelfFrequency, smoothedLowShelfGain, smoothedLowShelfQ);

        for (int i = 0; i < LfoMod::kNumLfos; ++i)
        {
            publishedLfoBipolar[(size_t) i].store (lfoBipolarOut[i], std::memory_order_relaxed);
            publishedLfoPhase[(size_t) i].store (lfoPhaseOut[i], std::memory_order_relaxed);
        }

        publishedBand1Freq.store (smoothedBand1Frequency, std::memory_order_relaxed);
        publishedBand1Q.store (smoothedBand1Q, std::memory_order_relaxed);
        publishedBand2Freq.store (smoothedBand2Frequency, std::memory_order_relaxed);
        publishedBand2Q.store (smoothedBand2Q, std::memory_order_relaxed);
        publishedBand3Freq.store (smoothedBand3Frequency, std::memory_order_relaxed);
        publishedBand3Q.store (smoothedBand3Q, std::memory_order_relaxed);
        publishedBand4Freq.store (smoothedBand4Frequency, std::memory_order_relaxed);
        publishedBand4Q.store (smoothedBand4Q, std::memory_order_relaxed);
        publishedHighShelfFreq.store (smoothedHighShelfFrequency, std::memory_order_relaxed);
        publishedHighShelfQ.store (smoothedHighShelfQ, std::memory_order_relaxed);
        publishedLowShelfFreq.store (smoothedLowShelfFrequency, std::memory_order_relaxed);
        publishedLowShelfQ.store (smoothedLowShelfQ, std::memory_order_relaxed);
    }


    //DBG("smoothedHighpassCutoff: " << smoothedHighpassCutoff);
    //DBG("smoothedHighpassQ: " << smoothedHighpassQ);

    // Process Filter Bands

    // Pre-EQ meters (input): sample-peak + RMS of the buffer entering processing.
    {
        auto storeInput = [&] (int channel, std::atomic<float>& peakOut, std::atomic<float>& rmsOut)
        {
            if (channel < 0 || channel >= mainBuffer.getNumChannels() || numSamples <= 0)
            {
                peakOut.store (-100.0f);
                rmsOut.store (-100.0f);
                return;
            }

            peakOut.store (juce::Decibels::gainToDecibels (mainBuffer.getMagnitude (channel, 0, numSamples), -100.0f));
            rmsOut.store (juce::Decibels::gainToDecibels (mainBuffer.getRMSLevel (channel, 0, numSamples), -100.0f));
        };

        storeInput (0, inputPeakDbLeft, inputRmsDbLeft);
        if (mainBuffer.getNumChannels() > 1)
        {
            storeInput (1, inputPeakDbRight, inputRmsDbRight);
        }
        else
        {
            inputPeakDbRight.store (inputPeakDbLeft.load());
            inputRmsDbRight.store (inputRmsDbLeft.load());
        }

        measureMidSideLevels (mainBuffer, numSamples,
                              inputPeakDbMid, inputRmsDbMid, inputPeakDbSide, inputRmsDbSide);
    }

    const float preLeftDb = inputRmsDbLeft.load();
    const float preRightDb = inputRmsDbRight.load();

    // Pre-EQ spectrum capture (before any band processing).
    {
        const bool analyserOn = isSpectrumAnalyserActive();
        const bool wantPre = analyserOn
                             && (treeState.getRawParameterValue ("SPECTRUM_PRE_CURVE_ID") == nullptr
                                 || treeState.getRawParameterValue ("SPECTRUM_PRE_CURVE_ID")->load() > 0.5f
                                 || treeState.getRawParameterValue ("SPECTRUM_PRE_FILL_ID") == nullptr
                                 || treeState.getRawParameterValue ("SPECTRUM_PRE_FILL_ID")->load() > 0.5f);

        if (wantPre)
        {
            if (mainBuffer.getNumChannels() > 1)
            {
                m_analyser.pushPreSamplesIntoFifo (mainBuffer.getReadPointer (0),
                                                   mainBuffer.getReadPointer (1),
                                                   mainBuffer.getNumSamples());
            }
            else if (mainBuffer.getNumChannels() > 0)
            {
                const auto* mono = mainBuffer.getReadPointer (0);
                m_analyser.pushPreSamplesIntoFifo (mono, mono, mainBuffer.getNumSamples());
            }
        }
    }

    // Dynamic EQ / external sidechain: detect from pre-EQ main or SC bus / MIDI.
    // Snapshot dry for spectral S only when any S toggle is on (S runs post-EQ; must
    // not detect on cut/boost wet). Avoids a stereo buffer copy when S is fully off.
    const float* preL = mainBuffer.getNumChannels() > 0 ? mainBuffer.getReadPointer (0) : nullptr;
    const float* preR = mainBuffer.getNumChannels() > 1 ? mainBuffer.getReadPointer (1) : nullptr;

    auto rawBool = [this] (const char* id) -> bool
    {
        if (auto* v = treeState.getRawParameterValue (id))
            return v->load() > 0.5f;
        return false;
    };
    auto rawFloat = [this] (const char* id, float fallback = 0.0f) -> float
    {
        if (auto* v = treeState.getRawParameterValue (id))
            return v->load();
        return fallback;
    };

    const bool anySpectralArmed =
        (isBand1On && rawBool ("band1Spectral"))
        || (isBand2On && rawBool ("band2Spectral"))
        || (isBand3On && rawBool ("band3Spectral"))
        || (isBand4On && rawBool ("band4Spectral"))
        || (isHighShelfOn && rawBool ("highShelfSpectral"))
        || (isLowShelfOn && rawBool ("lowShelfSpectral"));

    if (anySpectralArmed)
    {
        const int nCh = juce::jmin (2, mainBuffer.getNumChannels());
        if (nCh > 0 && numSamples > 0)
        {
            spectralDetectBuffer.setSize (nCh, numSamples, false, false, true);
            for (int ch = 0; ch < nCh; ++ch)
                spectralDetectBuffer.copyFrom (ch, 0, mainBuffer, ch, 0, numSamples);
        }
    }

    auto resolveDynamicGain = [&] (bool bandOn,
                                   bool typeUsesGain,
                                   bool dynamicOn,
                                   bool sidechainOn,
                                   bool sidechainMidi,
                                   DynamicEq::BandState& dynState,
                                   DynamicEq::BandState& scDetect,
                                   BandSidechain::GateState& scGate,
                                   float targetGainDb,
                                   float frequency,
                                   float q,
                                   float thresholdDb,
                                   float attackTimeMs,
                                   float releaseTimeMs) -> float
    {
        if (! typeUsesGain)
        {
            dynState.publishEffectiveGain (0.0f);
            return 0.0f;
        }

        if (! bandOn)
        {
            dynState.publishEffectiveGain (targetGainDb);
            return targetGainDb;
        }

        // External sidechain (audio or MIDI) takes priority over internal Dynamic EQ.
        if (sidechainOn)
        {
            scGate.updateCoeffs (attackTimeMs, releaseTimeMs, sampleRate, numSamples);
            float amount = 0.0f;

            if (sidechainMidi)
            {
                amount = scGate.stepToward (midiNotesHeld > 0 ? 1.0f : 0.0f);
            }
            else
            {
                scDetect.updateEnvelopeCoeffs (attackTimeMs, releaseTimeMs, sampleRate, numSamples);
                if (scL != nullptr)
                    amount = scDetect.detectAmount (scL, scR, numSamples, sampleRate,
                                                    frequency, q, thresholdDb);
                else
                    amount = scGate.stepToward (0.0f);
            }

            const float eg = DynamicEq::effectiveGainDb (true, targetGainDb, amount);
            dynState.publishEffectiveGain (eg);
            return eg;
        }

        if (! dynamicOn)
        {
            dynState.reset();
            dynState.publishEffectiveGain (targetGainDb);
            return targetGainDb;
        }

        dynState.updateEnvelopeCoeffs (attackTimeMs, releaseTimeMs, sampleRate, numSamples);
        const float amount = dynState.detectAmount (preL, preR, numSamples, sampleRate,
                                                    frequency, q, thresholdDb);
        const float eg = DynamicEq::effectiveGainDb (true, targetGainDb, amount);
        dynState.publishEffectiveGain (eg);
        return eg;
    };

    const int hsType = (int) rawFloat ("highShelfType");
    const float effHighShelfGain = resolveDynamicGain (
        isHighShelfOn, FilterType::usesGain (hsType), rawBool ("highShelfDynamic"),
        rawBool ("highShelfSidechain"), rawBool ("highShelfSidechainMidi"),
        dynHighShelf, scDetectHighShelf, scGateHighShelf,
        smoothedHighShelfGain, smoothedHighShelfFrequency, smoothedHighShelfQ,
        rawFloat ("highShelfDynThreshold", -24.0f),
        rawFloat ("highShelfAttackMs", DynamicEq::attackMs),
        rawFloat ("highShelfReleaseMs", DynamicEq::releaseMs));

    const int lsType = (int) rawFloat ("lowShelfType");
    const float effLowShelfGain = resolveDynamicGain (
        isLowShelfOn, FilterType::usesGain (lsType), rawBool ("lowShelfDynamic"),
        rawBool ("lowShelfSidechain"), rawBool ("lowShelfSidechainMidi"),
        dynLowShelf, scDetectLowShelf, scGateLowShelf,
        smoothedLowShelfGain, smoothedLowShelfFrequency, smoothedLowShelfQ,
        rawFloat ("lowShelfDynThreshold", -24.0f),
        rawFloat ("lowShelfAttackMs", DynamicEq::attackMs),
        rawFloat ("lowShelfReleaseMs", DynamicEq::releaseMs));

    const int b1Type = (int) rawFloat ("band1Type");
    const float effBand1Gain = resolveDynamicGain (
        isBand1On, FilterType::usesGain (b1Type), rawBool ("band1Dynamic"),
        rawBool ("band1Sidechain"), rawBool ("band1SidechainMidi"),
        dynBand1, scDetectBand1, scGateBand1,
        smoothedBand1Gain, smoothedBand1Frequency, smoothedBand1Q,
        rawFloat ("band1DynThreshold", -24.0f),
        rawFloat ("band1AttackMs", DynamicEq::attackMs),
        rawFloat ("band1ReleaseMs", DynamicEq::releaseMs));

    const int b2Type = (int) rawFloat ("band2Type");
    const float effBand2Gain = resolveDynamicGain (
        isBand2On, FilterType::usesGain (b2Type), rawBool ("band2Dynamic"),
        rawBool ("band2Sidechain"), rawBool ("band2SidechainMidi"),
        dynBand2, scDetectBand2, scGateBand2,
        smoothedBand2Gain, smoothedBand2Frequency, smoothedBand2Q,
        rawFloat ("band2DynThreshold", -24.0f),
        rawFloat ("band2AttackMs", DynamicEq::attackMs),
        rawFloat ("band2ReleaseMs", DynamicEq::releaseMs));

    const int b3Type = (int) rawFloat ("band3Type");
    const float effBand3Gain = resolveDynamicGain (
        isBand3On, FilterType::usesGain (b3Type), rawBool ("band3Dynamic"),
        rawBool ("band3Sidechain"), rawBool ("band3SidechainMidi"),
        dynBand3, scDetectBand3, scGateBand3,
        smoothedBand3Gain, smoothedBand3Frequency, smoothedBand3Q,
        rawFloat ("band3DynThreshold", -24.0f),
        rawFloat ("band3AttackMs", DynamicEq::attackMs),
        rawFloat ("band3ReleaseMs", DynamicEq::releaseMs));

    const int b4Type = (int) rawFloat ("band4Type");
    const float effBand4Gain = resolveDynamicGain (
        isBand4On, FilterType::usesGain (b4Type), rawBool ("band4Dynamic"),
        rawBool ("band4Sidechain"), rawBool ("band4SidechainMidi"),
        dynBand4, scDetectBand4, scGateBand4,
        smoothedBand4Gain, smoothedBand4Frequency, smoothedBand4Q,
        rawFloat ("band4DynThreshold", -24.0f),
        rawFloat ("band4AttackMs", DynamicEq::attackMs),
        rawFloat ("band4ReleaseMs", DynamicEq::releaseMs));

    const bool proportionalQOn = treeState.getRawParameterValue ("PROPORTIONAL_Q_ID") != nullptr
                                 && treeState.getRawParameterValue ("PROPORTIONAL_Q_ID")->load() > 0.5f;

    // Static / dynamic EQ: Minimum Phase = cascaded IIR (current path).
    // Linear Phase = single FIR matching the same magnitude response, then S as today.
    if (useLinearPhase)
    {
        LinearPhaseEqEngine::BandSpec specs[8] {};

        specs[0].enabled = isHighpassOn;
        specs[0].isHighpass = true;
        specs[0].frequency = smoothedHighpassCutoff;
        specs[0].q = smoothedHighpassQ;
        specs[0].slope = BandChannel::readChoiceIndex (treeState, "highpassSlope");

        specs[1].enabled = isLowpassOn;
        specs[1].isLowpass = true;
        specs[1].frequency = smoothedLowpassCutoff;
        specs[1].q = smoothedLowpassQ;
        specs[1].slope = BandChannel::readChoiceIndex (treeState, "lowpassSlope");

        specs[2].enabled = isHighShelfOn;
        specs[2].type = hsType;
        specs[2].frequency = smoothedHighShelfFrequency;
        specs[2].q = smoothedHighShelfQ;
        specs[2].gainDb = effHighShelfGain;

        specs[3].enabled = isLowShelfOn;
        specs[3].type = lsType;
        specs[3].frequency = smoothedLowShelfFrequency;
        specs[3].q = smoothedLowShelfQ;
        specs[3].gainDb = effLowShelfGain;

        specs[4].enabled = isBand1On;
        specs[4].type = b1Type;
        specs[4].frequency = smoothedBand1Frequency;
        specs[4].q = FilterType::effectiveBellQ (b1Type, smoothedBand1Q, effBand1Gain, proportionalQOn);
        specs[4].gainDb = effBand1Gain;

        specs[5].enabled = isBand2On;
        specs[5].type = b2Type;
        specs[5].frequency = smoothedBand2Frequency;
        specs[5].q = FilterType::effectiveBellQ (b2Type, smoothedBand2Q, effBand2Gain, proportionalQOn);
        specs[5].gainDb = effBand2Gain;

        specs[6].enabled = isBand3On;
        specs[6].type = b3Type;
        specs[6].frequency = smoothedBand3Frequency;
        specs[6].q = FilterType::effectiveBellQ (b3Type, smoothedBand3Q, effBand3Gain, proportionalQOn);
        specs[6].gainDb = effBand3Gain;

        specs[7].enabled = isBand4On;
        specs[7].type = b4Type;
        specs[7].frequency = smoothedBand4Frequency;
        specs[7].q = FilterType::effectiveBellQ (b4Type, smoothedBand4Q, effBand4Gain, proportionalQOn);
        specs[7].gainDb = effBand4Gain;

        linearPhaseEngine.setBands (specs, 8);
        linearPhaseEngine.process (audioBlock);
    }
    else
    {
        // Apply the filters conditionally based on the boolean flags
        if (isHighpassOn)
        {
            const int hpSlope = BandChannel::readChoiceIndex (treeState, "highpassSlope");
            const int hpChannel = BandChannel::readChoiceIndex (treeState, "highpassChannel");
            if (coeffsNeedUpdate (lastHighpass, smoothedHighpassCutoff, smoothedHighpassQ, 0.0f, hpSlope))
            {
                updateHighpass (smoothedHighpassCutoff, smoothedHighpassQ, hpSlope);
                storeCoeffs (lastHighpass, smoothedHighpassCutoff, smoothedHighpassQ, 0.0f, hpSlope);
            }
            BandChannel::process (audioBlock, hpChannel, [this] (juce::dsp::AudioBlock<float>& block)
            {
                for (int i = 0; i < highpassActiveStages; ++i)
                    highpassStages[(size_t) i].process (juce::dsp::ProcessContextReplacing<float> (block));
            });
        }

        if (isLowpassOn)
        {
            const int lpSlope = BandChannel::readChoiceIndex (treeState, "lowpassSlope");
            const int lpChannel = BandChannel::readChoiceIndex (treeState, "lowpassChannel");
            if (coeffsNeedUpdate (lastLowpass, smoothedLowpassCutoff, smoothedLowpassQ, 0.0f, lpSlope))
            {
                updateLowpass (smoothedLowpassCutoff, smoothedLowpassQ, lpSlope);
                storeCoeffs (lastLowpass, smoothedLowpassCutoff, smoothedLowpassQ, 0.0f, lpSlope);
            }
            BandChannel::process (audioBlock, lpChannel, [this] (juce::dsp::AudioBlock<float>& block)
            {
                for (int i = 0; i < lowpassActiveStages; ++i)
                    lowpassStages[(size_t) i].process (juce::dsp::ProcessContextReplacing<float> (block));
            });
        }

        // Sync juce oversampling factor on all sat engines (min-phase path).
        {
            const int osIdx = BandChannel::readChoiceIndex (treeState, BandSaturation::oversampleParamId());
            for (auto& sat : bandSatEngines)
                sat.setOversampleIndex (osIdx);
        }

        auto applyBandEqAndSat = [this] (StereoIIR& filter,
                                         juce::dsp::AudioBlock<float>& block,
                                         bool satOn,
                                         bool satPost,
                                         int satModel,
                                         int bandIndex)
        {
            auto& sat = satEngineForBandIndex (bandIndex);
            if (! satOn)
            {
                filter.process (juce::dsp::ProcessContextReplacing<float> (block));
                return;
            }

            sat.setModel (satModel);
            if (satPost)
            {
                sat.captureDry (block);
                filter.process (juce::dsp::ProcessContextReplacing<float> (block));
                sat.processPost (block);
            }
            else
            {
                filter.process (juce::dsp::ProcessContextReplacing<float> (block));
                sat.processPre (block);
            }
        };

        auto rawSatModel = [&] (const char* id) -> int
        {
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (id)))
                return choice->getIndex();
            return BandSaturation::tube;
        };

        if (isHighShelfOn)
        {
            const int channelMode = BandChannel::readChoiceIndex (treeState, "highShelfChannel");
            if (coeffsNeedUpdate (lastHighShelf, smoothedHighShelfFrequency, smoothedHighShelfQ, effHighShelfGain, hsType))
            {
                updateHighShelf (smoothedHighShelfFrequency, smoothedHighShelfQ, effHighShelfGain);
                storeCoeffs (lastHighShelf, smoothedHighShelfFrequency, smoothedHighShelfQ, effHighShelfGain, hsType);
            }
            const bool satOn = FilterType::usesGain (hsType) && rawBool ("highShelfSat") && effHighShelfGain > 0.05f;
            const bool satPost = rawBool ("highShelfSatPost");
            const int satModel = rawSatModel ("highShelfSatModel");
            BandChannel::process (audioBlock, channelMode, [&] (juce::dsp::AudioBlock<float>& block)
            {
                applyBandEqAndSat (highShelf, block, satOn, satPost, satModel, 6);
            });
        }

        if (isLowShelfOn)
        {
            const int channelMode = BandChannel::readChoiceIndex (treeState, "lowShelfChannel");
            if (coeffsNeedUpdate (lastLowShelf, smoothedLowShelfFrequency, smoothedLowShelfQ, effLowShelfGain, lsType))
            {
                updateLowShelf (smoothedLowShelfFrequency, smoothedLowShelfQ, effLowShelfGain);
                storeCoeffs (lastLowShelf, smoothedLowShelfFrequency, smoothedLowShelfQ, effLowShelfGain, lsType);
            }
            const bool satOn = FilterType::usesGain (lsType) && rawBool ("lowShelfSat") && effLowShelfGain > 0.05f;
            const bool satPost = rawBool ("lowShelfSatPost");
            const int satModel = rawSatModel ("lowShelfSatModel");
            BandChannel::process (audioBlock, channelMode, [&] (juce::dsp::AudioBlock<float>& block)
            {
                applyBandEqAndSat (lowShelf, block, satOn, satPost, satModel, 7);
            });
        }

        if (isBand1On)
        {
            const int channelMode = BandChannel::readChoiceIndex (treeState, "band1Channel");
            const float qEff = FilterType::effectiveBellQ (b1Type, smoothedBand1Q, effBand1Gain, proportionalQOn);
            const int cacheKey = b1Type + (proportionalQOn ? 100 : 0);
            if (coeffsNeedUpdate (lastBand1, smoothedBand1Frequency, qEff, effBand1Gain, cacheKey))
            {
                updateBand1 (smoothedBand1Frequency, smoothedBand1Q, effBand1Gain);
                storeCoeffs (lastBand1, smoothedBand1Frequency, qEff, effBand1Gain, cacheKey);
            }
            const bool satOn = FilterType::usesGain (b1Type) && rawBool ("band1Sat") && effBand1Gain > 0.05f;
            const bool satPost = rawBool ("band1SatPost");
            const int satModel = rawSatModel ("band1SatModel");
            BandChannel::process (audioBlock, channelMode, [&] (juce::dsp::AudioBlock<float>& block)
            {
                applyBandEqAndSat (band1, block, satOn, satPost, satModel, 0);
            });
        }

        if (isBand2On)
        {
            const int channelMode = BandChannel::readChoiceIndex (treeState, "band2Channel");
            const float qEff = FilterType::effectiveBellQ (b2Type, smoothedBand2Q, effBand2Gain, proportionalQOn);
            const int cacheKey = b2Type + (proportionalQOn ? 100 : 0);
            if (coeffsNeedUpdate (lastBand2, smoothedBand2Frequency, qEff, effBand2Gain, cacheKey))
            {
                updateBand2 (smoothedBand2Frequency, smoothedBand2Q, effBand2Gain);
                storeCoeffs (lastBand2, smoothedBand2Frequency, qEff, effBand2Gain, cacheKey);
            }
            const bool satOn = FilterType::usesGain (b2Type) && rawBool ("band2Sat") && effBand2Gain > 0.05f;
            const bool satPost = rawBool ("band2SatPost");
            const int satModel = rawSatModel ("band2SatModel");
            BandChannel::process (audioBlock, channelMode, [&] (juce::dsp::AudioBlock<float>& block)
            {
                applyBandEqAndSat (band2, block, satOn, satPost, satModel, 1);
            });
        }

        if (isBand3On)
        {
            const int channelMode = BandChannel::readChoiceIndex (treeState, "band3Channel");
            const float qEff = FilterType::effectiveBellQ (b3Type, smoothedBand3Q, effBand3Gain, proportionalQOn);
            const int cacheKey = b3Type + (proportionalQOn ? 100 : 0);
            if (coeffsNeedUpdate (lastBand3, smoothedBand3Frequency, qEff, effBand3Gain, cacheKey))
            {
                updateBand3 (smoothedBand3Frequency, smoothedBand3Q, effBand3Gain);
                storeCoeffs (lastBand3, smoothedBand3Frequency, qEff, effBand3Gain, cacheKey);
            }
            const bool satOn = FilterType::usesGain (b3Type) && rawBool ("band3Sat") && effBand3Gain > 0.05f;
            const bool satPost = rawBool ("band3SatPost");
            const int satModel = rawSatModel ("band3SatModel");
            BandChannel::process (audioBlock, channelMode, [&] (juce::dsp::AudioBlock<float>& block)
            {
                applyBandEqAndSat (band3, block, satOn, satPost, satModel, 2);
            });
        }

        if (isBand4On)
        {
            const int channelMode = BandChannel::readChoiceIndex (treeState, "band4Channel");
            const float qEff = FilterType::effectiveBellQ (b4Type, smoothedBand4Q, effBand4Gain, proportionalQOn);
            const int cacheKey = b4Type + (proportionalQOn ? 100 : 0);
            if (coeffsNeedUpdate (lastBand4, smoothedBand4Frequency, qEff, effBand4Gain, cacheKey))
            {
                updateBand4 (smoothedBand4Frequency, smoothedBand4Q, effBand4Gain);
                storeCoeffs (lastBand4, smoothedBand4Frequency, qEff, effBand4Gain, cacheKey);
            }
            const bool satOn = FilterType::usesGain (b4Type) && rawBool ("band4Sat") && effBand4Gain > 0.05f;
            const bool satPost = rawBool ("band4SatPost");
            const int satModel = rawSatModel ("band4SatModel");
            BandChannel::process (audioBlock, channelMode, [&] (juce::dsp::AudioBlock<float>& block)
            {
                applyBandEqAndSat (band4, block, satOn, satPost, satModel, 3);
            });
        }
    }

    // Spectral dynamics (S): ONE shared coarse IIR bandpass bank after the EQ chain.
    // D = broadband dynamic on IIR/LP gain; S = per-slice resonance cuts/boosts.
    // Detect = pre-EQ dry (spectralDetectBuffer); apply GR to post-EQ wet.
    // Order: static EQ → Spectral (S) → global Side Check (BP lattice) → AutoGain/Out.
    // Hard-bypassed (no filters) when no S band is on; no FFT.
    auto armSpectral = [&] (bool bandOn,
                            bool typeUsesGain,
                            bool spectralOn,
                            bool sidechainOn,
                            int slot,
                            float frequency,
                            float q,
                            float bandwidthHz,
                            float amount,
                            bool expand,
                            int packChoiceIndex,
                            float attackTimeMs,
                            float releaseTimeMs,
                            SpectralDynamics::BandShape shape)
    {
        if (! (bandOn && spectralOn && typeUsesGain))
            return;

        const float amt = juce::jlimit (SpectralDynamics::kMinSpectralAmount,
                                        SpectralDynamics::kMaxSpectralAmount,
                                        amount);

        SpectralDynamics::BandSettings settings;
        settings.enabled = true;
        settings.frequencyHz = frequency;
        settings.q = q;
        settings.bandwidthHz = SpectralBinning::clampBandwidthHz (bandwidthHz);
        settings.amount = amt;
        settings.expand = expand;
        settings.pack = SpectralDynamics::packModeFromChoiceIndex (packChoiceIndex);
        settings.attackMs = DynamicEq::clampAttackMs (attackTimeMs);
        settings.releaseMs = DynamicEq::clampReleaseMs (releaseTimeMs);
        settings.shape = shape;
        // S + Sidechain → detect resonances on the Sidechain bus, not the main track.
        settings.detectFromSidechain = sidechainOn;
        spectralEngine.setBand (slot, settings);
    };

    auto rawPack = [&]() -> int
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                treeState.getParameter (SpectralDynamics::spectralPackParamId())))
            return choice->getIndex();
        return 0;
    };
    const int spectralPackChoice = rawPack();

    spectralEngine.clearBands();
    armSpectral (isHighShelfOn, FilterType::usesGain (hsType), rawBool ("highShelfSpectral"),
                 rawBool ("highShelfSidechain"),
                 SpectralDynamics::slotForBandIndex (6),
                 smoothedHighShelfFrequency, smoothedHighShelfQ,
                 rawFloat ("highShelfSpectralResHz", SpectralBinning::kDefaultBandwidthHz),
                 rawFloat ("highShelfSpectralDepth", SpectralDynamics::kDefaultSpectralAmount),
                 rawBool ("highShelfSpectralExpand"),
                 spectralPackChoice,
                 rawFloat ("highShelfAttackMs", DynamicEq::attackMs),
                 rawFloat ("highShelfReleaseMs", DynamicEq::releaseMs),
                 SpectralDynamics::BandShape::highShelf);
    armSpectral (isLowShelfOn, FilterType::usesGain (lsType), rawBool ("lowShelfSpectral"),
                 rawBool ("lowShelfSidechain"),
                 SpectralDynamics::slotForBandIndex (7),
                 smoothedLowShelfFrequency, smoothedLowShelfQ,
                 rawFloat ("lowShelfSpectralResHz", SpectralBinning::kDefaultBandwidthHz),
                 rawFloat ("lowShelfSpectralDepth", SpectralDynamics::kDefaultSpectralAmount),
                 rawBool ("lowShelfSpectralExpand"),
                 spectralPackChoice,
                 rawFloat ("lowShelfAttackMs", DynamicEq::attackMs),
                 rawFloat ("lowShelfReleaseMs", DynamicEq::releaseMs),
                 SpectralDynamics::BandShape::lowShelf);
    armSpectral (isBand1On, FilterType::usesGain (b1Type), rawBool ("band1Spectral"),
                 rawBool ("band1Sidechain"),
                 SpectralDynamics::slotForBandIndex (0),
                 smoothedBand1Frequency, smoothedBand1Q,
                 rawFloat ("band1SpectralResHz", SpectralBinning::kDefaultBandwidthHz),
                 rawFloat ("band1SpectralDepth", SpectralDynamics::kDefaultSpectralAmount),
                 rawBool ("band1SpectralExpand"),
                 spectralPackChoice,
                 rawFloat ("band1AttackMs", DynamicEq::attackMs),
                 rawFloat ("band1ReleaseMs", DynamicEq::releaseMs),
                 SpectralDynamics::BandShape::bell);
    armSpectral (isBand2On, FilterType::usesGain (b2Type), rawBool ("band2Spectral"),
                 rawBool ("band2Sidechain"),
                 SpectralDynamics::slotForBandIndex (1),
                 smoothedBand2Frequency, smoothedBand2Q,
                 rawFloat ("band2SpectralResHz", SpectralBinning::kDefaultBandwidthHz),
                 rawFloat ("band2SpectralDepth", SpectralDynamics::kDefaultSpectralAmount),
                 rawBool ("band2SpectralExpand"),
                 spectralPackChoice,
                 rawFloat ("band2AttackMs", DynamicEq::attackMs),
                 rawFloat ("band2ReleaseMs", DynamicEq::releaseMs),
                 SpectralDynamics::BandShape::bell);
    armSpectral (isBand3On, FilterType::usesGain (b3Type), rawBool ("band3Spectral"),
                 rawBool ("band3Sidechain"),
                 SpectralDynamics::slotForBandIndex (2),
                 smoothedBand3Frequency, smoothedBand3Q,
                 rawFloat ("band3SpectralResHz", SpectralBinning::kDefaultBandwidthHz),
                 rawFloat ("band3SpectralDepth", SpectralDynamics::kDefaultSpectralAmount),
                 rawBool ("band3SpectralExpand"),
                 spectralPackChoice,
                 rawFloat ("band3AttackMs", DynamicEq::attackMs),
                 rawFloat ("band3ReleaseMs", DynamicEq::releaseMs),
                 SpectralDynamics::BandShape::bell);
    armSpectral (isBand4On, FilterType::usesGain (b4Type), rawBool ("band4Spectral"),
                 rawBool ("band4Sidechain"),
                 SpectralDynamics::slotForBandIndex (3),
                 smoothedBand4Frequency, smoothedBand4Q,
                 rawFloat ("band4SpectralResHz", SpectralBinning::kDefaultBandwidthHz),
                 rawFloat ("band4SpectralDepth", SpectralDynamics::kDefaultSpectralAmount),
                 rawBool ("band4SpectralExpand"),
                 spectralPackChoice,
                 rawFloat ("band4AttackMs", DynamicEq::attackMs),
                 rawFloat ("band4ReleaseMs", DynamicEq::releaseMs),
                 SpectralDynamics::BandShape::bell);

    // Always call process: hard-bypasses (no filters) when idle, and clears UI GR on S→off.
    {
        const float* detL = nullptr;
        const float* detR = nullptr;
        if (anySpectralArmed && spectralDetectBuffer.getNumChannels() > 0)
        {
            detL = spectralDetectBuffer.getReadPointer (0);
            detR = spectralDetectBuffer.getNumChannels() > 1
                       ? spectralDetectBuffer.getReadPointer (1) : detL;
        }
        spectralEngine.process (audioBlock, detL, detR, scL, scR);
    }

    // Stage 2 — post-Spectral bus sat (Expand / S peaks drive the grit).
    // Only when SS is on and at least one S band is armed.
    {
        const bool ssOn = rawBool (BandSaturation::spectralSatParamId());
        if (ssOn && anySpectralArmed)
        {
            const int osIdx = BandChannel::readChoiceIndex (
                treeState, BandSaturation::spectralSatOversampleParamId());
            spectralSatEngine.setOversampleIndex (osIdx);

            int model = BandSaturation::tube;
            if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (
                    treeState.getParameter (BandSaturation::spectralSatModelParamId())))
                model = choice->getIndex();
            spectralSatEngine.setModel (model);

            const float drive = rawFloat (BandSaturation::spectralSatDriveParamId(),
                                         BandSaturation::kDefaultSpectralSatDrive);
            spectralSatEngine.processBusDriven (audioBlock, drive);
        }
    }

    updateReportedLatency (false);

    // Global Side Check (S<=M): after EQ + Spectral (+ optional Stage 2 sat), before AutoGain/Out.
    // Own BP lattice (never mutates spectralEngine). Needs stereo.
    {
        const bool scOn = treeState.getRawParameterValue (SideCheck::enabledParamId()) != nullptr
                          && treeState.getRawParameterValue (SideCheck::enabledParamId())->load() > 0.5f;
        float scAmount = SideCheck::kDefaultAmount;
        if (auto* amt = treeState.getRawParameterValue (SideCheck::amountParamId()))
            scAmount = amt->load();
        float scHp = SideCheck::kDefaultHpHz;
        float scLp = SideCheck::kDefaultLpHz;
        if (auto* hp = treeState.getRawParameterValue (SideCheck::hpHzParamId()))
            scHp = hp->load();
        if (auto* lp = treeState.getRawParameterValue (SideCheck::lpHzParamId()))
            scLp = lp->load();
        const int scMode = SideCheck::readModeIndex (treeState, SideCheck::fast);
        const bool scHq = treeState.getRawParameterValue (SideCheck::hqParamId()) == nullptr
                          || treeState.getRawParameterValue (SideCheck::hqParamId())->load() > 0.5f;
        sideCheck.process (mainBuffer, scOn, scAmount, scHp, scLp, scMode, scHq);
    }

    // Autogain: match post-EQ loudness to pre-EQ via an internal offset (Out knob stays manual).
    {
        const bool autoGainOn = treeState.getRawParameterValue ("autoGain") != nullptr
                                && treeState.getRawParameterValue ("autoGain")->load() > 0.5f;

        auto blockRmsDb = [&mainBuffer, numSamples] (int channel) -> float
        {
            if (channel < 0 || channel >= mainBuffer.getNumChannels())
                return -100.0f;
            return juce::Decibels::gainToDecibels (mainBuffer.getRMSLevel (channel, 0, numSamples), -100.0f);
        };

        const float preDb = juce::jmax (preLeftDb, preRightDb);
        const float postEqDb = juce::jmax (blockRmsDb (0),
                                           mainBuffer.getNumChannels() > 1 ? blockRmsDb (1) : blockRmsDb (0));

        constexpr float silenceFloorDb = -60.0f;

        if (! autoGainOn)
        {
            smoothAutoGainOffset.setTargetValue (0.0f);
        }
        else if (preDb > silenceFloorDb && postEqDb > silenceFloorDb)
        {
            // Hold last target during silence so the offset doesn't pump out/in.
            smoothAutoGainOffset.setTargetValue (juce::jlimit (-24.0f, 24.0f, preDb - postEqDb));
        }

        const float autoOffsetDb = take (smoothAutoGainOffset);
        const float gainDb = take (smoothOutputGain) + autoOffsetDb;
        mainBuffer.applyGain (juce::Decibels::decibelsToGain (gainDb));
    }

    // Post-EQ meters: final stereo output after EQ + output gain + autogain
    // (the samples leaving the plugin — compare these to Ableton insert-return peak).
    {
        auto storePost = [&] (int channel, std::atomic<float>& peakOut, std::atomic<float>& rmsOut)
        {
            if (channel < 0 || channel >= mainBuffer.getNumChannels() || numSamples <= 0)
            {
                peakOut.store (-100.0f);
                rmsOut.store (-100.0f);
                return;
            }

            peakOut.store (juce::Decibels::gainToDecibels (mainBuffer.getMagnitude (channel, 0, numSamples), -100.0f));
            rmsOut.store (juce::Decibels::gainToDecibels (mainBuffer.getRMSLevel (channel, 0, numSamples), -100.0f));
        };

        storePost (0, postPeakDbLeft, postRmsDbLeft);
        if (mainBuffer.getNumChannels() > 1)
            storePost (1, postPeakDbRight, postRmsDbRight);
        else
        {
            postPeakDbRight.store (postPeakDbLeft.load());
            postRmsDbRight.store (postRmsDbLeft.load());
        }

        measureMidSideLevels (mainBuffer, numSamples,
                              postPeakDbMid, postRmsDbMid, postPeakDbSide, postRmsDbSide);
    }



    if (mainBuffer.getNumChannels() > 1)
    {
        const bool analyserOn = isSpectrumAnalyserActive();
        const bool wantPost = analyserOn
                              && ((treeState.getRawParameterValue ("SPECTRUM_POST_CURVE_ID") == nullptr
                                   || treeState.getRawParameterValue ("SPECTRUM_POST_CURVE_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("SPECTRUM_POST_FILL_ID") == nullptr
                                      || treeState.getRawParameterValue ("SPECTRUM_POST_FILL_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("MAX_ID") == nullptr
                                      || treeState.getRawParameterValue ("MAX_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("SPECTRUM_HOLD_FILL_ID") == nullptr
                                      || treeState.getRawParameterValue ("SPECTRUM_HOLD_FILL_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("SPECTRUM_FFT_BINS_ID") != nullptr
                                      && treeState.getRawParameterValue ("SPECTRUM_FFT_BINS_ID")->load() > 0.5f));

        if (wantPost)
        {
            m_analyser.pushSamplesIntoFifo (mainBuffer.getReadPointer (0),
                                            mainBuffer.getReadPointer (1),
                                            mainBuffer.getNumSamples());
        }
    }
    else if (mainBuffer.getNumChannels() > 0)
    {
        const bool analyserOn = isSpectrumAnalyserActive();
        const bool wantPost = analyserOn
                              && ((treeState.getRawParameterValue ("SPECTRUM_POST_CURVE_ID") == nullptr
                                   || treeState.getRawParameterValue ("SPECTRUM_POST_CURVE_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("SPECTRUM_POST_FILL_ID") == nullptr
                                      || treeState.getRawParameterValue ("SPECTRUM_POST_FILL_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("MAX_ID") == nullptr
                                      || treeState.getRawParameterValue ("MAX_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("SPECTRUM_HOLD_FILL_ID") == nullptr
                                      || treeState.getRawParameterValue ("SPECTRUM_HOLD_FILL_ID")->load() > 0.5f)
                                  || (treeState.getRawParameterValue ("SPECTRUM_FFT_BINS_ID") != nullptr
                                      && treeState.getRawParameterValue ("SPECTRUM_FFT_BINS_ID")->load() > 0.5f));

        if (wantPost)
        {
            const auto* mono = mainBuffer.getReadPointer (0);
            m_analyser.pushSamplesIntoFifo (mono, mono, mainBuffer.getNumSamples());
        }
    }

    if (auto* osc = oscilloscopeTarget.load (std::memory_order_acquire))
    {
        if (osc->isScopeEnabled() && mainBuffer.getNumChannels() > 0)
        {
            const auto* left = mainBuffer.getReadPointer (0);
            const auto* right = mainBuffer.getNumChannels() > 1 ? mainBuffer.getReadPointer (1) : left;
            osc->pushSamples (left, right, mainBuffer.getNumSamples());
        }
    }
}


void EqProcessor::updateHighpass(float cutoff, float q, int slopeChoice)
{
    auto stages = FilterSlope::makeHighpassCoeffs (getSampleRate(), cutoff, q, slopeChoice);
    highpassActiveStages = juce::jlimit (1, FilterSlope::maxBiquadStages, stages.size());

    for (int i = 0; i < highpassActiveStages; ++i)
        *highpassStages[(size_t) i].state = *stages.getUnchecked (i);
}

void EqProcessor::updateLowpass(float cutoff, float q, int slopeChoice)
{
    auto stages = FilterSlope::makeLowpassCoeffs (getSampleRate(), cutoff, q, slopeChoice);
    lowpassActiveStages = juce::jlimit (1, FilterSlope::maxBiquadStages, stages.size());

    for (int i = 0; i < lowpassActiveStages; ++i)
        *lowpassStages[(size_t) i].state = *stages.getUnchecked (i);
}

void EqProcessor::updateHighShelf(float cutoff, float q, float gain)
{
    const int type = (int) treeState.getRawParameterValue ("highShelfType")->load();
    *highShelf.state = *FilterType::makeCoefficients (type, getSampleRate(), cutoff, q, gain);
}

void EqProcessor::updateLowShelf(float cutoff, float q, float gain)
{
    const int type = (int) treeState.getRawParameterValue ("lowShelfType")->load();
    *lowShelf.state = *FilterType::makeCoefficients (type, getSampleRate(), cutoff, q, gain);
}

void EqProcessor::updateBand1(float cutoff, float q, float gain)
{
    const int type = (int) treeState.getRawParameterValue ("band1Type")->load();
    const bool proportionalOn = treeState.getRawParameterValue ("PROPORTIONAL_Q_ID") != nullptr
                                && treeState.getRawParameterValue ("PROPORTIONAL_Q_ID")->load() > 0.5f;
    const float qEff = FilterType::effectiveBellQ (type, q, gain, proportionalOn);
    *band1.state = *FilterType::makeCoefficients (type, getSampleRate(), cutoff, qEff, gain);
}

void EqProcessor::updateBand2(float cutoff, float q, float gain)
{
    const int type = (int) treeState.getRawParameterValue ("band2Type")->load();
    const bool proportionalOn = treeState.getRawParameterValue ("PROPORTIONAL_Q_ID") != nullptr
                                && treeState.getRawParameterValue ("PROPORTIONAL_Q_ID")->load() > 0.5f;
    const float qEff = FilterType::effectiveBellQ (type, q, gain, proportionalOn);
    *band2.state = *FilterType::makeCoefficients (type, getSampleRate(), cutoff, qEff, gain);
}

void EqProcessor::updateBand3(float cutoff, float q, float gain)
{
    const int type = (int) treeState.getRawParameterValue ("band3Type")->load();
    const bool proportionalOn = treeState.getRawParameterValue ("PROPORTIONAL_Q_ID") != nullptr
                                && treeState.getRawParameterValue ("PROPORTIONAL_Q_ID")->load() > 0.5f;
    const float qEff = FilterType::effectiveBellQ (type, q, gain, proportionalOn);
    *band3.state = *FilterType::makeCoefficients (type, getSampleRate(), cutoff, qEff, gain);
}

void EqProcessor::updateBand4(float cutoff, float q, float gain)
{
    const int type = (int) treeState.getRawParameterValue ("band4Type")->load();
    const bool proportionalOn = treeState.getRawParameterValue ("PROPORTIONAL_Q_ID") != nullptr
                                && treeState.getRawParameterValue ("PROPORTIONAL_Q_ID")->load() > 0.5f;
    const float qEff = FilterType::effectiveBellQ (type, q, gain, proportionalOn);
    *band4.state = *FilterType::makeCoefficients (type, getSampleRate(), cutoff, qEff, gain);
}

//==============================================================================


bool EqProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* EqProcessor::createEditor()
{
    return new EqEditor(*this, treeState, m_analyser);  // Assuming `parameters` and `analyser` are member variables of EqProcessor
}



//==============================================================================
namespace
{
    void stripBypassParamFromState (juce::ValueTree& state)
    {
        for (int i = state.getNumChildren(); --i >= 0;)
        {
            auto child = state.getChild (i);
            if (child.hasType ("PARAM") && child.getProperty ("id").toString() == "bypass")
                state.removeChild (i, nullptr);
        }
    }

    void stripShapeCurveFromState (juce::ValueTree& state)
    {
        for (int i = state.getNumChildren(); --i >= 0;)
            if (state.getChild (i).hasType (ShapeMod::kStateType))
                state.removeChild (i, nullptr);
    }

    void attachShapeCurveToState (juce::ValueTree& state, const ShapeMod::Engine& engine)
    {
        stripShapeCurveFromState (state);
        state.appendChild (engine.toValueTree(), nullptr);
    }

    juce::ValueTree takeShapeCurveFromState (juce::ValueTree& state)
    {
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            if (state.getChild (i).hasType (ShapeMod::kStateType))
            {
                auto shape = state.getChild (i).createCopy();
                state.removeChild (i, nullptr);
                return shape;
            }
        }
        return {};
    }

    /** Pre-Shape projects had 5 sources (0..4). Remap so Env stays Env when Shape is appended. */
    void migrateModSourceChoicesIfNeeded (juce::ValueTree& state)
    {
        bool hasShapeRate = false;
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            const auto child = state.getChild (i);
            if (child.hasType ("PARAM") && child.getProperty ("id").toString() == ShapeMod::rateParamId())
            {
                hasShapeRate = true;
                break;
            }
        }

        if (hasShapeRate)
            return;

        constexpr int oldMaxIndex = 4; // Off..Env
        constexpr int newMaxIndex = LfoMod::numSources - 1;

        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);
            if (! child.hasType ("PARAM"))
                continue;

            const auto id = child.getProperty ("id").toString();
            if (! id.startsWith ("modSlot") || ! id.endsWith ("Source"))
                continue;

            const float norm = (float) child.getProperty ("value", 0.0);
            const int oldIdx = juce::jlimit (0, oldMaxIndex,
                                             (int) std::lround (norm * (float) oldMaxIndex));
            const float newNorm = (float) oldIdx / (float) newMaxIndex;
            child.setProperty ("value", newNorm, nullptr);
        }
    }

    void setBypassNormalized (juce::AudioProcessorValueTreeState& treeState, float normalized)
    {
        if (auto* p = treeState.getParameter ("bypass"))
            p->setValueNotifyingHost (normalized);
    }

    float getBypassNormalized (juce::AudioProcessorValueTreeState& treeState)
    {
        if (auto* p = treeState.getParameter ("bypass"))
            return p->getValue();
        return 0.0f;
    }
}

juce::ValueTree EqProcessor::captureStateForSnapshot()
{
    auto state = treeState.copyState();
    stripBypassParamFromState (state);
    attachShapeCurveToState (state, shapeEngine);
    return state;
}

void EqProcessor::applySnapshotState (const juce::ValueTree& snapshot)
{
    if (! snapshot.isValid())
        return;

    const float bypassNorm = getBypassNormalized (treeState);
    auto state = snapshot.createCopy();
    const auto shapeTree = takeShapeCurveFromState (state);
    stripBypassParamFromState (state);
    treeState.replaceState (state);
    setBypassNormalized (treeState, bypassNorm);
    updateParameters();

    if (shapeTree.isValid())
        shapeEngine.fromValueTree (shapeTree);
}

juce::ValueTree& EqProcessor::getAbSnapshot (AbSlot slot) noexcept
{
    const int i = juce::jlimit (0, abSlotCount - 1, (int) slot);
    return abSnapshots[i];
}

const juce::ValueTree& EqProcessor::getAbSnapshot (AbSlot slot) const noexcept
{
    const int i = juce::jlimit (0, abSlotCount - 1, (int) slot);
    return abSnapshots[i];
}

void EqProcessor::initialiseAbSnapshotsFromCurrentState()
{
    const auto seed = captureStateForSnapshot();
    for (int i = 0; i < abSlotCount; ++i)
        abSnapshots[i] = seed.createCopy();
    activeAbSlot = AbSlot::A;
}

void EqProcessor::saveCurrentToAbSlot (AbSlot slot)
{
    getAbSnapshot (slot) = captureStateForSnapshot();
}

void EqProcessor::switchToAbSlot (AbSlot slot)
{
    if (slot == activeAbSlot)
        return;

    // Persist live edits into the slot we're leaving, then load the target.
    saveCurrentToAbSlot (activeAbSlot);
    activeAbSlot = slot;
    applySnapshotState (getAbSnapshot (slot));
}

void EqProcessor::copyAbSlot (AbSlot from, AbSlot to)
{
    if (from == to)
        return;

    if (from == activeAbSlot)
        saveCurrentToAbSlot (from);

    getAbSnapshot (to) = getAbSnapshot (from).createCopy();

    if (to == activeAbSlot)
        applySnapshotState (getAbSnapshot (to));
}

void EqProcessor::swapAbSlots (AbSlot a, AbSlot b)
{
    if (a == b)
        return;

    saveCurrentToAbSlot (activeAbSlot);
    std::swap (getAbSnapshot (a), getAbSnapshot (b));
    applySnapshotState (getAbSnapshot (activeAbSlot));
}

void EqProcessor::storeAbSnapshotsInState (juce::ValueTree& state) const
{
    state.setProperty ("abActiveSlot", (int) activeAbSlot, nullptr);

    static constexpr const char* ids[abSlotCount] = {
        "abSnapshotA", "abSnapshotB", "abSnapshotC", "abSnapshotD"
    };

    for (int i = 0; i < abSlotCount; ++i)
        if (auto xml = abSnapshots[i].createXml())
            state.setProperty (ids[i], xml->toString(), nullptr);
}

void EqProcessor::restoreAbSnapshotsFromState (const juce::ValueTree& state)
{
    static constexpr const char* ids[abSlotCount] = {
        "abSnapshotA", "abSnapshotB", "abSnapshotC", "abSnapshotD"
    };

    juce::ValueTree loaded[abSlotCount];
    int validCount = 0;

    for (int i = 0; i < abSlotCount; ++i)
    {
        const auto xml = state.getProperty (ids[i]).toString();
        if (xml.isEmpty())
            continue;

        if (auto parsed = juce::parseXML (xml))
        {
            loaded[i] = juce::ValueTree::fromXml (*parsed);
            stripBypassParamFromState (loaded[i]);
            if (loaded[i].isValid())
                ++validCount;
        }
    }

    // Legacy projects only stored A/B — seed C/D from A when missing.
    if (loaded[0].isValid() && loaded[1].isValid()
        && (! loaded[2].isValid() || ! loaded[3].isValid()))
    {
        if (! loaded[2].isValid())
            loaded[2] = loaded[0].createCopy();
        if (! loaded[3].isValid())
            loaded[3] = loaded[0].createCopy();
        validCount = abSlotCount;
    }

    if (validCount < 2 || ! loaded[0].isValid() || ! loaded[1].isValid())
    {
        // No (or incomplete) snapshot data — seed all from the just-loaded live state.
        initialiseAbSnapshotsFromCurrentState();
        return;
    }

    for (int i = 0; i < abSlotCount; ++i)
        abSnapshots[i] = loaded[i].isValid() ? std::move (loaded[i])
                                             : abSnapshots[0].createCopy();

    const int slot = juce::jlimit (0, abSlotCount - 1, (int) state.getProperty ("abActiveSlot", 0));
    activeAbSlot = static_cast<AbSlot> (slot);
}

void EqProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    // Keep the active slot's snapshot in sync with live edits before persisting.
    saveCurrentToAbSlot (activeAbSlot);

    auto state = treeState.copyState();

    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter ("BLOCK_ID")))
        AnalyserDefaults::stampBlockIdInState (state, choice->getIndex());
    else
        state.setProperty ("deckStateSchema", 2, nullptr);

    storeAbSnapshotsInState (state);
    attachShapeCurveToState (state, shapeEngine);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    copyXmlToBinary (*xml, destData);
}

void EqProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState == nullptr || ! xmlState->hasTagName (treeState.state.getType()))
        return;

    auto state = juce::ValueTree::fromXml (*xmlState);
    AnalyserDefaults::migrateBlockIdInState (state);
    migrateModSourceChoicesIfNeeded (state);

    // Keep A/B/C/D + Shape curve aside, then load APVTS (without those extras as params).
    const auto abProps = state.createCopy();
    const auto shapeTree = takeShapeCurveFromState (state);
    state.removeProperty ("abActiveSlot", nullptr);
    state.removeProperty ("abSnapshotA", nullptr);
    state.removeProperty ("abSnapshotB", nullptr);
    state.removeProperty ("abSnapshotC", nullptr);
    state.removeProperty ("abSnapshotD", nullptr);
    stripShapeCurveFromState (state);

    treeState.replaceState (state);
    updateParameters();

    // Restore after replaceState so a missing snapshot set seeds from the new live state.
    restoreAbSnapshotsFromState (abProps);

    if (shapeTree.isValid())
        shapeEngine.fromValueTree (shapeTree);
}


//==============================================================================
// This creates new instances of the plugin.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EqProcessor();
}


void EqProcessor::updateReportedLatency (bool bypassed) noexcept
{
    if (bypassed)
    {
        setLatencySamples (0);
        return;
    }

    int latency = spectralEngine.getLatencySamples();
    if (PhaseMode::readChoiceIndex (treeState) == PhaseMode::linearPhase)
        latency += linearPhaseEngine.getLatencySamples();

    // Sat OS latency: report max across engines (same factor on all).
    int satLatency = 0;
    for (auto& sat : bandSatEngines)
        satLatency = juce::jmax (satLatency, sat.getLatencySamples());
    satLatency = juce::jmax (satLatency, spectralSatEngine.getLatencySamples());
    latency += satLatency;

    setLatencySamples (latency);
}

void EqProcessor::releaseResources()
{
    linearPhaseEngine.releaseResources();
    for (auto& sat : bandSatEngines)
        sat.releaseResources();
    spectralSatEngine.releaseResources();
}

void EqProcessor::sampleSpectralGrDb (int bandIndex, const float* frequenciesHz, float* destDb, int numPoints) const
{
    if (destDb == nullptr || numPoints <= 0)
        return;

    spectralEngine.samplePublishedGrDb (bandIndex, frequenciesHz, destDb, numPoints);
}

void EqProcessor::sampleSideCheckGrDb (const float* frequenciesHz, float* destDb, int numPoints) const
{
    if (destDb == nullptr || numPoints <= 0)
        return;

    sideCheck.samplePublishedGrDb (frequenciesHz, destDb, numPoints);
}

void EqProcessor::setFrequencyResponseComponent (FrequencyResponseComponent* component)
{
    frequencyResponseComponent = component;
}

float EqProcessor::getInputRmsValue(const int channel)
{
    if (channel == 0)
        return inputRmsDbLeft.load();
    if (channel == 1)
        return inputRmsDbRight.load();
    return 0.0f;
}

float EqProcessor::getPostProcessingRmsValue(const int channel)
{
    if (channel == 0)
        return postRmsDbLeft.load();
    if (channel == 1)
        return postRmsDbRight.load();
    return 0.0f;
}

float EqProcessor::getInputPeakValue(const int channel)
{
    if (channel == 0)
        return inputPeakDbLeft.load();
    if (channel == 1)
        return inputPeakDbRight.load();
    return 0.0f;
}

float EqProcessor::getPostProcessingPeakValue(const int channel)
{
    if (channel == 0)
        return postPeakDbLeft.load();
    if (channel == 1)
        return postPeakDbRight.load();
    return 0.0f;
}

float EqProcessor::getInputMsPeakValue (int channel) const
{
    if (channel == 0)
        return inputPeakDbMid.load();
    if (channel == 1)
        return inputPeakDbSide.load();
    return -100.0f;
}

float EqProcessor::getInputMsRmsValue (int channel) const
{
    if (channel == 0)
        return inputRmsDbMid.load();
    if (channel == 1)
        return inputRmsDbSide.load();
    return -100.0f;
}

float EqProcessor::getPostProcessingMsPeakValue (int channel) const
{
    if (channel == 0)
        return postPeakDbMid.load();
    if (channel == 1)
        return postPeakDbSide.load();
    return -100.0f;
}

float EqProcessor::getPostProcessingMsRmsValue (int channel) const
{
    if (channel == 0)
        return postRmsDbMid.load();
    if (channel == 1)
        return postRmsDbSide.load();
    return -100.0f;
}

bool EqProcessor::isMeterMsMode() const noexcept
{
    if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter ("METER_CHANNEL_MODE_ID")))
        return choice->getIndex() == 1;

    if (auto* raw = treeState.getRawParameterValue ("METER_CHANNEL_MODE_ID"))
        return raw->load() >= 0.5f;

    return false;
}

float EqProcessor::getModHandlePulseScale (int bandIndex) const noexcept
{
    float peakUnipolar = 0.0f;
    bool any = false;

    for (int s = 0; s < LfoMod::kNumMatrixSlots; ++s)
    {
        if (auto* onV = treeState.getRawParameterValue (LfoMod::slotEnabledParamId (s)))
            if (onV->load() <= 0.5f)
                continue;

        int src = LfoMod::srcOff, dest = LfoMod::destOff;
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::slotSourceParamId (s))))
            src = p->getIndex();
        if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (LfoMod::slotDestParamId (s))))
            dest = p->getIndex();

        if (src <= LfoMod::srcOff || ! LfoMod::destinationTargetsBand (dest, bandIndex))
            continue;

        const int lfo = LfoMod::sourceToLfoIndex (src);
        if (lfo >= 0)
        {
            const float bip = publishedLfoBipolar[(size_t) lfo].load (std::memory_order_relaxed);
            peakUnipolar = juce::jmax (peakUnipolar, (bip + 1.0f) * 0.5f);
            any = true;
        }
        else if (LfoMod::sourceIsEnvFollower (src))
        {
            peakUnipolar = juce::jmax (peakUnipolar, publishedEnvAmount.load (std::memory_order_relaxed));
            any = true;
        }
        else if (LfoMod::sourceIsShape (src))
        {
            const float bip = publishedShapeBipolar.load (std::memory_order_relaxed);
            peakUnipolar = juce::jmax (peakUnipolar, (bip + 1.0f) * 0.5f);
            any = true;
        }
    }

    if (! any)
        return 1.0f;

    // Grow up to +50% with the LFO wave (1.0 → 1.5).
    return 1.0f + 0.5f * peakUnipolar;
}

float EqProcessor::getRmsValue(const int channel)
{
    jassert(channel == 0 || channel == 1);
    if (channel == 0)
        return inputRmsDbLeft.load();
    if (channel == 1)
        return inputRmsDbRight.load();
    return 0.f;
}

void EqProcessor::initializeSharedImages()
{
    DBG("initializeSharedImages called");

    // Attempt to load the image from binary data
    darkKnob4_StitchedImage = juce::ImageCache::getFromMemory(BinaryData::DarkKnob4_Stitched_png, BinaryData::DarkKnob4_Stitched_pngSize);

    // DBG("InitializeSharedImages Called");

     // Check if the image loaded successfully   
    if (!darkKnob4_StitchedImage.isValid())
    {
        // Handle the error gracefully or provide a fallback image
        DBG("Failed to load DarkKnob4_Stitched_png");
        // You can set a fallback image here if needed.
    }

    else
    {
        //DBG("DarkKnob4_Stitched_png loaded successfully");
    }

}