#pragma once

#include <atomic>
#include <array>
#include <cmath>
#include <JuceHeader.h>
#include <juce_dsp/juce_dsp.h>
#include "Visualizer/Visualizer.h"
#include "Visualizer/Analyser.h"
#include "BinaryData.h"
#include "FilterSlope.h"
#include "DynamicEq.h"
#include "Spectral/SpectralDynamicsProcessor.h"
#include "Spectral/SpectralBandSettings.h"
#include "PhaseMode.h"
#include "LinearPhaseEqEngine.h"
#include "SideCheck.h"
#include "BandSaturation.h"
#include "LfoMod.h"
#include "BandSidechain.h"
#include "ShapeMod.h"

class FrequencyResponseComponent;
class OscilloscopeComponent;

class EqProcessor : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener//, public juce::Timer 


{
public:
    EqProcessor();
    ~EqProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    void initializeSharedImages();

    static const char* DarkKnob4_Stitched_png;
    static const char* DarkKnob4_Stitched_pngSize;

    static juce::Image darkKnob4_StitchedImage;

    //juce::AudioProcessorValueTreeState treeState;

    Analyser& getAnalyser() { return m_analyser; }

    /** Optional oscilloscope strip (UI). Audio thread pushes when non-null and enabled. */
    void setOscilloscopeTarget (class OscilloscopeComponent* target) noexcept;
    OscilloscopeComponent* getOscilloscopeTarget() const noexcept;

    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    const juce::UndoManager& getUndoManager() const noexcept { return undoManager; }

    /** Eco: skip FFT/analyser visual work. Does not affect Dynamic (D) or Spectral (S) DSP. */
    void setEcoMode (bool shouldEnable) noexcept;
    bool isEcoMode() const noexcept { return ecoMode.load(); }

    /** True when spectrum analyser should run (pref on and Eco off). */
    bool isSpectrumAnalyserActive() const noexcept;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

   // void timerCallback() override;

    float getRmsValue(const int channel);
    float getInputRmsValue(const int channel);
    float getPostProcessingRmsValue(const int channel);
    float getInputPeakValue(const int channel);
    float getPostProcessingPeakValue(const int channel);

    /** Mid/Side levels from the same tap as L/R meters. channel 0 = Mid, 1 = Side. */
    float getInputMsPeakValue (int channel) const;
    float getInputMsRmsValue (int channel) const;
    float getPostProcessingMsPeakValue (int channel) const;
    float getPostProcessingMsRmsValue (int channel) const;

    /** True when Level Meters channel mode is M/S (APVTS METER_CHANNEL_MODE_ID). */
    bool isMeterMsMode() const noexcept;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /** A/B/C/D referencing — four full APVTS snapshots (bypass excluded). */
    enum class AbSlot { A = 0, B = 1, C = 2, D = 3 };

    static constexpr int abSlotCount = 4;

    static juce::String abSlotName (AbSlot slot) noexcept
    {
        switch (slot)
        {
            case AbSlot::A: return "A";
            case AbSlot::B: return "B";
            case AbSlot::C: return "C";
            case AbSlot::D: return "D";
        }
        return "A";
    }

    AbSlot getActiveAbSlot() const noexcept { return activeAbSlot; }
    void switchToAbSlot (AbSlot slot);
    void saveCurrentToAbSlot (AbSlot slot);
    void copyAbSlot (AbSlot from, AbSlot to);
    void swapAbSlots (AbSlot a, AbSlot b);

    ShapeMod::Engine& getShapeEngine() noexcept { return shapeEngine; }
    const ShapeMod::Engine& getShapeEngine() const noexcept { return shapeEngine; }
    float getPublishedShapePhase() const noexcept
    {
        return publishedShapePhase.load (std::memory_order_relaxed);
    }

    float getPublishedLfoPhase (int lfoIndex) const noexcept
    {
        if (lfoIndex < 0 || lfoIndex >= LfoMod::kNumLfos)
            return 0.0f;
        return publishedLfoPhase[(size_t) lfoIndex].load (std::memory_order_relaxed);
    }

    bool getIsHighpassOn() const { return isHighpassOn; }
    bool getIsLowpassOn() const { return isLowpassOn; }
    bool getIsHighShelfOn() const { return isHighShelfOn; }
    bool getIsLowShelfOn() const { return isLowShelfOn; }
    bool getIsBand1On() const { return isBand1On; }
    bool getIsBand2On() const { return isBand2On; }
    bool getIsBand3On() const { return isBand3On; }
    bool getIsBand4On() const { return isBand4On; }


    void setFrequencyResponseComponent (FrequencyResponseComponent* component);

   // void updateBand1Parameters(float newFrequency, float newGain);

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    // Declared before treeState so &undoManager is valid in the APVTS constructor.
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState treeState;


    juce::dsp::IIR::Coefficients<float> getHighpassCoefficients() const {
        return *highpassStages[0].state;
    }

    juce::dsp::IIR::Coefficients<float> getLowpassCoefficients() const {
        return *lowpassStages[0].state;
    }

    juce::dsp::IIR::Coefficients<float> getLowShelfCoefficients() const {
        return *lowShelf.state;
    }

    juce::dsp::IIR::Coefficients<float> getHighShelfCoefficients() const {
        return *highShelf.state;
    }

    juce::dsp::IIR::Coefficients<float> getBand1Coefficients() const {
        return *band1.state;
    }

    juce::dsp::IIR::Coefficients<float> getBand2Coefficients() const {
        return *band2.state;
    }

    juce::dsp::IIR::Coefficients<float> getBand3Coefficients() const {
        return *band3.state;
    }

    juce::dsp::IIR::Coefficients<float> getBand4Coefficients() const {
        return *band4.state;
    }

  



    void parameterChanged(const juce::String& parameterID, float newValue) override;

    void updateParameters();

    void updateHighpass(float frequency, float q, int slopeChoice);
    void updateLowpass(float frequency, float q, int slopeChoice);

    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<StereoIIR, FilterSlope::maxBiquadStages> highpassStages;
    std::array<StereoIIR, FilterSlope::maxBiquadStages> lowpassStages;
    int highpassActiveStages = 1;
    int lowpassActiveStages = 1;

    void updateHighShelf(float frequency, float q, float gain);
    StereoIIR highShelf;

    void updateLowShelf(float frequency, float q, float gain);
    StereoIIR lowShelf;

    void updateBand1(float frequency, float q, float gain);
    StereoIIR band1;

    void updateBand2(float frequency, float q, float gain);
    StereoIIR band2;

    void updateBand3(float frequency, float q, float gain);
    StereoIIR band3;

    void updateBand4(float frequency, float q, float gain);
    StereoIIR band4;

    /** Instantaneous effective gain (static target, or dynamic amount × target). */
    float getBand1EffectiveGainDb() const { return dynBand1.getPublishedEffectiveGain(); }
    float getBand2EffectiveGainDb() const { return dynBand2.getPublishedEffectiveGain(); }
    float getBand3EffectiveGainDb() const { return dynBand3.getPublishedEffectiveGain(); }
    float getBand4EffectiveGainDb() const { return dynBand4.getPublishedEffectiveGain(); }
    float getHighShelfEffectiveGainDb() const { return dynHighShelf.getPublishedEffectiveGain(); }
    float getLowShelfEffectiveGainDb() const { return dynLowShelf.getPublishedEffectiveGain(); }

    /** Last-block freq/Q after LFO (curve display). Gain uses get*EffectiveGainDb(). */
    float getPublishedBand1Freq() const noexcept { return publishedBand1Freq.load (std::memory_order_relaxed); }
    float getPublishedBand1Q() const noexcept { return publishedBand1Q.load (std::memory_order_relaxed); }
    float getPublishedBand2Freq() const noexcept { return publishedBand2Freq.load (std::memory_order_relaxed); }
    float getPublishedBand2Q() const noexcept { return publishedBand2Q.load (std::memory_order_relaxed); }
    float getPublishedBand3Freq() const noexcept { return publishedBand3Freq.load (std::memory_order_relaxed); }
    float getPublishedBand3Q() const noexcept { return publishedBand3Q.load (std::memory_order_relaxed); }
    float getPublishedBand4Freq() const noexcept { return publishedBand4Freq.load (std::memory_order_relaxed); }
    float getPublishedBand4Q() const noexcept { return publishedBand4Q.load (std::memory_order_relaxed); }
    float getPublishedHighShelfFreq() const noexcept { return publishedHighShelfFreq.load (std::memory_order_relaxed); }
    float getPublishedHighShelfQ() const noexcept { return publishedHighShelfQ.load (std::memory_order_relaxed); }
    float getPublishedLowShelfFreq() const noexcept { return publishedLowShelfFreq.load (std::memory_order_relaxed); }
    float getPublishedLowShelfQ() const noexcept { return publishedLowShelfQ.load (std::memory_order_relaxed); }

    /** Bipolar LFO sample (−1..1) published each block for UI (handle pulse, etc.). */
    float getPublishedLfoBipolar (int lfoIndex) const noexcept
    {
        if (lfoIndex < 0 || lfoIndex >= LfoMod::kNumLfos)
            return 0.0f;
        return publishedLfoBipolar[(size_t) lfoIndex].load (std::memory_order_relaxed);
    }

    /**
        Handle size multiplier for a band (1.0..1.5) when an enabled matrix slot
        targets that band — follows the LFO unipolar wave so the handle grows +50%.
    */
    float getModHandlePulseScale (int bandIndex) const noexcept;

    /** Sample published spectral GR (dB, signed cut/boost) for curve display; zeros if inactive. */
    void sampleSpectralGrDb (int bandIndex, const float* frequenciesHz, float* destDb, int numPoints) const;

    /** Sample published Side Check GR (dB) onto display freqs for the sum curve; zeros if idle. */
    void sampleSideCheckGrDb (const float* frequenciesHz, float* destDb, int numPoints) const;

    SpectralDynamicsProcessor& getSpectralEngine() noexcept { return spectralEngine; }
    const SpectralDynamicsProcessor& getSpectralEngine() const noexcept { return spectralEngine; }

    /** Last-block Side Check GR (dB, negative when pulling Side down). */
    float getSideCheckGrDb() const noexcept { return sideCheck.getPublishedGrDb(); }

    /** Last-block spectral bank stats (armed / banked / gated / processing). */
    SpectralDynamicsProcessor::RuntimeStats getSpectralRuntimeStats() const noexcept
    {
        return spectralEngine.getRuntimeStats();
    }

    float getTargetValueForSmoothKnob1() const { return smoothknob1.getTargetValue(); }
    float getTargetValueForSmoothKnob2() const { return smoothknob2.getTargetValue(); }
    float getTargetValueForSmoothKnob3() const { return smoothknob3.getTargetValue(); }
    float getTargetValueForSmoothKnob4() const { return smoothknob4.getTargetValue(); }
    float getTargetValueForSmoothKnob5() const { return smoothknob5.getTargetValue(); }
    float getTargetValueForSmoothKnob6() const { return smoothknob6.getTargetValue(); }
    float getTargetValueForSmoothKnob7() const { return smoothknob7.getTargetValue(); }
    float getTargetValueForSmoothKnob8() const { return smoothknob8.getTargetValue(); }
    float getTargetValueForSmoothKnob9() const { return smoothknob9.getTargetValue(); }
    float getTargetValueForSmoothKnob10() const { return smoothknob10.getTargetValue(); }
    float getTargetValueForSmoothKnob11() const { return smoothknob11.getTargetValue(); }
    float getTargetValueForSmoothKnob12() const { return smoothknob12.getTargetValue(); }
    float getTargetValueForSmoothKnob13() const { return smoothknob13.getTargetValue(); }
    float getTargetValueForSmoothKnob14() const { return smoothknob14.getTargetValue(); }
    float getTargetValueForSmoothKnob15() const { return smoothknob15.getTargetValue(); }
    float getTargetValueForSmoothKnob16() const { return smoothknob16.getTargetValue(); }
    float getTargetValueForSmoothKnob17() const { return smoothknob17.getTargetValue(); }
    float getTargetValueForSmoothKnob18() const { return smoothknob18.getTargetValue(); }
    float getTargetValueForSmoothKnob19() const { return smoothknob19.getTargetValue(); }
    float getTargetValueForSmoothKnob20() const { return smoothknob20.getTargetValue(); }
    float getTargetValueForSmoothKnob21() const { return smoothknob21.getTargetValue(); }
    float getTargetValueForSmoothKnob22() const { return smoothknob22.getTargetValue(); }

    float getSmoothKnob1Value() const { return smoothknob1.getCurrentValue(); }
    float getSmoothKnob2Value() const { return smoothknob2.getCurrentValue(); }
    float getSmoothKnob3Value() const { return smoothknob3.getCurrentValue(); }
    float getSmoothKnob4Value() const { return smoothknob4.getCurrentValue(); }
    float getSmoothKnob5Value() const { return smoothknob5.getCurrentValue(); }
    float getSmoothKnob6Value() const { return smoothknob6.getCurrentValue(); }
    float getSmoothKnob7Value() const { return smoothknob7.getCurrentValue(); }
    float getSmoothKnob8Value() const { return smoothknob8.getCurrentValue(); }
    float getSmoothKnob9Value() const { return smoothknob9.getCurrentValue(); }
    float getSmoothKnob10Value() const { return smoothknob10.getCurrentValue(); }
    float getSmoothKnob11Value() const { return smoothknob11.getCurrentValue(); }
    float getSmoothKnob12Value() const { return smoothknob12.getCurrentValue(); }
    float getSmoothKnob13Value() const { return smoothknob13.getCurrentValue(); }
    float getSmoothKnob14Value() const { return smoothknob14.getCurrentValue(); }
    float getSmoothKnob15Value() const { return smoothknob15.getCurrentValue(); }
    float getSmoothKnob16Value() const { return smoothknob16.getCurrentValue(); }
    float getSmoothKnob17Value() const { return smoothknob17.getCurrentValue(); }
    float getSmoothKnob18Value() const { return smoothknob18.getCurrentValue(); }
    float getSmoothKnob19Value() const { return smoothknob19.getCurrentValue(); }
    float getSmoothKnob20Value() const { return smoothknob20.getCurrentValue(); }
    float getSmoothKnob21Value() const { return smoothknob21.getCurrentValue(); }
    float getSmoothKnob22Value() const { return smoothknob22.getCurrentValue(); }


private:

    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();



    juce::LinearSmoothedValue<float> smoothknob1{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob2{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob3{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob4{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob5{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob6{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob7{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob8{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob9{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob10{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob11{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob12{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob13{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob14{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob15{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob16{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob17{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob18{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob19{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob20{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob21{ 0.0f };
    juce::LinearSmoothedValue<float> smoothknob22{ 0.0f };
    juce::LinearSmoothedValue<float> smoothOutputGain{ 0.0f };
    juce::LinearSmoothedValue<float> smoothAutoGainOffset{ 0.0f };

    float highpassCutoffValue;
    float highpassQValue;

    bool isStateRestored;

    // Raw per-block levels in dBFS (sample-peak + RMS). UI applies ballistics.
    std::atomic<float> inputPeakDbLeft { -100.0f };
    std::atomic<float> inputPeakDbRight { -100.0f };
    std::atomic<float> inputRmsDbLeft { -100.0f };
    std::atomic<float> inputRmsDbRight { -100.0f };
    std::atomic<float> postPeakDbLeft { -100.0f };
    std::atomic<float> postPeakDbRight { -100.0f };
    std::atomic<float> postRmsDbLeft { -100.0f };
    std::atomic<float> postRmsDbRight { -100.0f };

    // Mid/Side (0.5*(L±R), same convention as Side Check) at input / post taps.
    std::atomic<float> inputPeakDbMid { -100.0f };
    std::atomic<float> inputPeakDbSide { -100.0f };
    std::atomic<float> inputRmsDbMid { -100.0f };
    std::atomic<float> inputRmsDbSide { -100.0f };
    std::atomic<float> postPeakDbMid { -100.0f };
    std::atomic<float> postPeakDbSide { -100.0f };
    std::atomic<float> postRmsDbMid { -100.0f };
    std::atomic<float> postRmsDbSide { -100.0f };

    Analyser m_analyser;

    std::atomic<OscilloscopeComponent*> oscilloscopeTarget { nullptr };

    std::atomic<bool> ecoMode { false };

    double sampleRate = 48000;

    bool timerActive = false;
    int timerCounter = 0;

    float rawBand1Gain;

    bool isHighpassOn = false;
    bool isLowpassOn = false;
    bool isHighShelfOn = false;
    bool isLowShelfOn = false;
    bool isBand1On = false;
    bool isBand2On = false;
    bool isBand3On = false;
    bool isBand4On = false;

    // Skip redundant IIR coeff rebuilds when smoothed params haven't moved.
    struct CoeffCache
    {
        float a = -1.0e9f, b = -1.0e9f, c = -1.0e9f;
        int i = -999;
    };

    CoeffCache lastHighpass, lastLowpass, lastHighShelf, lastLowShelf;
    CoeffCache lastBand1, lastBand2, lastBand3, lastBand4;

    DynamicEq::BandState dynBand1, dynBand2, dynBand3, dynBand4;
    DynamicEq::BandState dynHighShelf, dynLowShelf;

    /** Curve-display snapshot of post-LFO freq/Q (gain via dyn publishedEffectiveGain). */
    std::atomic<float> publishedBand1Freq { 300.0f }, publishedBand1Q { 0.67f };
    std::atomic<float> publishedBand2Freq { 1000.0f }, publishedBand2Q { 0.67f };
    std::atomic<float> publishedBand3Freq { 4000.0f }, publishedBand3Q { 0.67f };
    std::atomic<float> publishedBand4Freq { 8000.0f }, publishedBand4Q { 0.67f };
    std::atomic<float> publishedHighShelfFreq { 5500.0f }, publishedHighShelfQ { 0.5f };
    std::atomic<float> publishedLowShelfFreq { 100.0f }, publishedLowShelfQ { 0.5f };
    std::array<std::atomic<float>, LfoMod::kNumLfos> publishedLfoBipolar {};
    std::array<std::atomic<float>, LfoMod::kNumLfos> publishedLfoPhase {};

    /** External sidechain audio detectors (separate from internal Dynamic EQ). */
    DynamicEq::BandState scDetectBand1, scDetectBand2, scDetectBand3, scDetectBand4;
    DynamicEq::BandState scDetectHighShelf, scDetectLowShelf;
    BandSidechain::GateState scGateBand1, scGateBand2, scGateBand3, scGateBand4;
    BandSidechain::GateState scGateHighShelf, scGateLowShelf;

    std::array<LfoMod::Voice, LfoMod::kNumLfos> lfoVoices;
    LfoMod::EnvFollower modEnvFollower;
    ShapeMod::Engine shapeEngine;
    int midiNotesHeld = 0;
    std::atomic<float> publishedEnvAmount { 0.0f };
    std::atomic<float> publishedShapeBipolar { 0.0f };
    std::atomic<float> publishedShapePhase { 0.0f };

    /** One shared coarse bandpass bank for all S bands (GR on post-EQ; detect pre-EQ). Zero cost when no S is on. */
    SpectralDynamicsProcessor spectralEngine;

    /** Global Side Check (S<=M): post-Spectral BP-lattice Mid/Side balance. */
    SideCheck::Processor sideCheck;

    /** Per-band sat engines (own juce::dsp::Oversampling state each). */
    std::array<BandSaturation::Engine, 6> bandSatEngines;

    /** Stage 2 — post-Spectral bus sat. */
    BandSaturation::Engine spectralSatEngine;

    BandSaturation::Engine& satEngineForBandIndex (int bandIndex) noexcept
    {
        switch (bandIndex)
        {
            case 0: return bandSatEngines[0];
            case 1: return bandSatEngines[1];
            case 2: return bandSatEngines[2];
            case 3: return bandSatEngines[3];
            case 6: return bandSatEngines[4];
            case 7: return bandSatEngines[5];
            default: return bandSatEngines[0];
        }
    }

    /** Linear-phase FIR EQ path (used when phaseMode == Linear Phase). */
    LinearPhaseEqEngine linearPhaseEngine;
    int lastPhaseMode = PhaseMode::minimumPhase;

    /** Pre-EQ copy for spectral sidechain detect (threshold must not track cut/boost). */
    juce::AudioBuffer<float> spectralDetectBuffer;

    void updateReportedLatency (bool bypassed) noexcept;

    static bool coeffsNeedUpdate (const CoeffCache& cache, float a, float b, float c = 0.0f, int i = 0)
    {
        constexpr float eps = 1.0e-5f;
        return cache.i != i
               || std::abs (cache.a - a) > eps
               || std::abs (cache.b - b) > eps
               || std::abs (cache.c - c) > eps;
    }

    static void storeCoeffs (CoeffCache& cache, float a, float b, float c = 0.0f, int i = 0)
    {
        cache = { a, b, c, i };
    }

    FrequencyResponseComponent* frequencyResponseComponent = nullptr;

    juce::AudioParameterBool* bypassParam = nullptr;

    juce::ValueTree abSnapshots[abSlotCount];
    AbSlot activeAbSlot = AbSlot::A;

    juce::ValueTree captureStateForSnapshot();
    void applySnapshotState (const juce::ValueTree& snapshot);
    juce::ValueTree& getAbSnapshot (AbSlot slot) noexcept;
    const juce::ValueTree& getAbSnapshot (AbSlot slot) const noexcept;
    void initialiseAbSnapshotsFromCurrentState();
    void storeAbSnapshotsInState (juce::ValueTree& state) const;
    void restoreAbSnapshotsFromState (const juce::ValueTree& state);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqProcessor)
};
