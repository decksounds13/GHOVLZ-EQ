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
#include "StructuralSplit/StructuralSplitSettings.h"
#include "StructuralSplit/StructuralSplitEngine.h"
#include "PhaseMode.h"
#include "LinearPhaseEqEngine.h"
#include "SideCheck.h"
#include "Match/MatchProcessor.h"
#include "BandSaturation.h"
#include "LfoMod.h"
#include "BandSidechain.h"
#include "ShapeMod.h"
#include "EqBand.h"
#include "Menu/SharedResources.h"
#include "Export/Spec3DExportSandbox.h"
#if SPEC3D_EXPORT_ENABLED
#include "Export/ExportAudioRingBuffer.h"
#endif

class FrequencyResponseComponent;
class OscilloscopeComponent;
class GoniometerComponent;
class SpectrogramComponent;
class LoudnessComponent;
class StereogramComponent;
class HistogramComponent;
class ThdMeterComponent;

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

    /** Optional goniometer (UI). Audio thread pushes when non-null and enabled. */
    void setGoniometerTarget (class GoniometerComponent* target) noexcept;
    GoniometerComponent* getGoniometerTarget() const noexcept;

    /** Optional spectrogram strip (UI). Audio thread pushes when non-null and enabled. */
    void setSpectrogramTarget (class SpectrogramComponent* target) noexcept;
    SpectrogramComponent* getSpectrogramTarget() const noexcept;

    /** Optional loudness meter (Scope UI). Audio thread pushes when non-null and enabled. */
    void setLoudnessTarget (class LoudnessComponent* target) noexcept;
    LoudnessComponent* getLoudnessTarget() const noexcept;

    /** Optional stereogram (Scope UI). Audio thread pushes when non-null and enabled. */
    void setStereogramTarget (class StereogramComponent* target) noexcept;
    StereogramComponent* getStereogramTarget() const noexcept;

    /** Optional level histogram (Scope UI). Audio thread pushes when non-null and enabled. */
    void setHistogramTarget (class HistogramComponent* target) noexcept;
    HistogramComponent* getHistogramTarget() const noexcept;

    /** Optional broadband THD meter (Scope UI). Audio thread pushes when non-null and enabled. */
    void setThdTarget (class ThdMeterComponent* target) noexcept;
    ThdMeterComponent* getThdTarget() const noexcept;

#if SPEC3D_EXPORT_ENABLED
    /** Continuous plugin output capture for Spec3D offline video export (DAW audio). */
    ExportAudioRingBuffer& getExportAudioRing() noexcept { return exportAudioRing; }
    const ExportAudioRingBuffer& getExportAudioRing() const noexcept { return exportAudioRing; }
#endif

    juce::UndoManager& getUndoManager() noexcept { return undoManager; }
    const juce::UndoManager& getUndoManager() const noexcept { return undoManager; }

    /** Eco: skip FFT/analyser visual work. Does not affect Dynamic (D) or Spectral (S) DSP. */
    void setEcoMode (bool shouldEnable) noexcept;
    bool isEcoMode() const noexcept { return ecoMode.load(); }

    /**
        Spec3D “Bypass Other Analyzers”: freeze EQ spectrum + all scopes except the
        spectrogram history feed (needed for 3D mesh) and level meters.
        Does not affect EQ DSP. Audio-thread safe.
    */
    void setBypassOtherAnalyzers (bool shouldBypass) noexcept;
    bool isBypassOtherAnalyzers() const noexcept { return bypassOtherAnalyzers.load (std::memory_order_acquire); }

    /**
        Solo-monitor one band's full processing chain (Bank1 internal 0–7 or global 8–63).
        -1 = off. Works even when that band's OnOff is false (force-enables only that band).
    */
    void setBandListenIndex (int bandIndex) noexcept;
    int getBandListenIndex() const noexcept { return bandListenIndex.load (std::memory_order_acquire); }

    /**
        Temporary Alt+drag bandpass audition on the EQ graph (message thread → audio).
        Q should stay modest (e.g. ≤ ~8) to avoid whistling.
    */
    void setAuditionBandpass (bool active, float frequencyHz, float q) noexcept;
    bool isAuditionBandpassActive() const noexcept { return auditionBandpassActive.load (std::memory_order_acquire); }
    float getAuditionBandpassFreqHz() const noexcept { return auditionBandpassFreqHz.load (std::memory_order_relaxed); }
    float getAuditionBandpassQ() const noexcept { return auditionBandpassQ.load (std::memory_order_relaxed); }

    /** Scope mode: quad metering view. DSP bypass depends on Pre/Post tap (see setScopeTapPost). */
    void setScopeMode (bool shouldEnable) noexcept;
    bool isScopeMode() const noexcept { return scopeMode.load(); }

    /**
        Scope-mode audio tap (Scope UI only; compact chrome scopes stay post when processing).
        false = Pre: dry passthrough + scopes see input (analyzer).
        true  = Post: EQ DSP runs + scopes see wet output.
    */
    void setScopeTapPost (bool shouldTapPost) noexcept;
    bool isScopeTapPost() const noexcept { return scopeTapPost.load (std::memory_order_acquire); }

    /**
        When a Scope module is maximized / OS-fullscreen: -1 = no solo (all run),
        else ScopeModuleId cast to int. Spectrum analyser only runs if solo is Spectrum
        (or no solo). Audio-thread safe.
    */
    void setScopeSoloModule (int moduleIdOrNeg1) noexcept;
    int getScopeSoloModule() const noexcept { return scopeSoloModule.load (std::memory_order_acquire); }

    /** True when spectrum analyser should run (pref on and Eco off; Scope solo-aware). */
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
    float getInputTruePeakValue (int channel) const;
    float getPostProcessingTruePeakValue (int channel) const;

    /** Mid/Side levels from the same tap as L/R meters. channel 0 = Mid, 1 = Side. */
    float getInputMsPeakValue (int channel) const;
    float getInputMsRmsValue (int channel) const;
    float getPostProcessingMsPeakValue (int channel) const;
    float getPostProcessingMsRmsValue (int channel) const;
    float getInputMsTruePeakValue (int channel) const;
    float getPostProcessingMsTruePeakValue (int channel) const;

    /** True when Level Meters channel mode is M/S (APVTS METER_CHANNEL_MODE_ID). */
    bool isMeterMsMode() const noexcept;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    /** UI theme for the current plugin instance (survives editor close/reopen). */
    void storeSessionUiTheme (const SharedColors& colours, const juce::ValueTree& colourRamps);
    bool tryRestoreSessionUiTheme (SharedColors& colours, juce::ValueTree& colourRampsOut) const;
    bool hasSessionUiTheme() const noexcept { return sessionUiThemeValid; }

    /**
        Full non-DSP UI blob (prefs, theme colours, GlobalUi modules, live layout flags).
        Type "UiSession". Host-persisted via get/setStateInformation so project reopen
        restores the UI as last left.
    */
    void storeSessionUiState (const juce::ValueTree& state);
    bool tryGetSessionUiState (juce::ValueTree& out) const;
    bool hasSessionUiState() const noexcept;

    /** Attach / extract UiSession for host state (gzip+base64 property + child). */
    void attachSessionUiToState (juce::ValueTree& state) const;
    static juce::ValueTree extractSessionUiFromState (juce::ValueTree& state);
    /** Mirror sessionUiState onto the live APVTS tree so copyState always carries UI. */
    void syncSessionUiOntoLiveTree();

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

    /** Envelope follower level in dB (smoothed), for threshold meter UI. */
    float getPublishedEnvDb() const noexcept
    {
        return publishedEnvDb.load (std::memory_order_relaxed);
    }

    /**
        Spec3D visual sidechain: filtered input envelope → 0..1 for Look modulation.
        Call configure from the UI thread; process runs in processBlock when enabled.
    */
    void configureSpec3DVisualSidechain (bool enabled, float hpHz, float lpHz,
                                         float thresholdDb,
                                         float attackMs, float releaseMs) noexcept;
    float getSpec3DVisualLevel01() const noexcept
    {
        return spec3dVisScLevel01.load (std::memory_order_relaxed);
    }

    bool getIsHighpassOn() const { return isHighpassOn; }
    bool getIsLowpassOn() const { return isLowpassOn; }
    bool getIsHighShelfOn() const { return isHighShelfOn; }
    bool getIsLowShelfOn() const { return isLowShelfOn; }
    bool getIsBand1On() const { return isBand1On; }
    bool getIsBand2On() const { return isBand2On; }
    bool getIsBand3On() const { return isBand3On; }
    bool getIsBand4On() const { return isBand4On; }

    /** True when the given global display band (0 = Band 1) is enabled. */
    bool isGlobalBandOn (int globalDisplay) const noexcept;
    /** First off slot, preferring currentBank's column order then any bank. -1 if all on. */
    int findFreeGlobalBand (int preferredBank = 0) const noexcept;
    /** Highest bank index that has at least one band on (0 if none). */
    int highestBankWithActiveBand() const noexcept;
    /** Number of banks to show in the faceplate pager (at least 1, grows with use). */
    int getFaceplateBankCount() const noexcept;
    void ensureBankAvailable (int bankIndex) noexcept;


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

    /** Global Res is source of truth; stamp legacy per-band Res into saved state. */
    void stampLegacySpectralResHzInState (juce::ValueTree& state, float hzValue) const;
    void migrateSpectralResHzFromLegacyParams();

    using StereoIIR = juce::dsp::ProcessorDuplicator<juce::dsp::IIR::Filter<float>, juce::dsp::IIR::Coefficients<float>>;
    std::array<StereoIIR, FilterSlope::maxBiquadStages> highpassStages;
    std::array<StereoIIR, FilterSlope::maxBiquadStages> lowpassStages;
    int highpassActiveStages = 1;
    int lowpassActiveStages = 1;

    /**
        Cascade banks for mid/shelf slots when typed HP/LP.
        Index: 0–3 = band1–4, 4 = highShelf, 5 = lowShelf.
    */
    static constexpr int kFlexibleCascadeSlots = 6;
    std::array<std::array<StereoIIR, FilterSlope::maxBiquadStages>, kFlexibleCascadeSlots> flexibleHpLpStages {};
    std::array<int, kFlexibleCascadeSlots> flexibleHpLpActiveStages { 1, 1, 1, 1, 1, 1 };

    /** Load HP or LP cascade into Band 1/8 stage banks (type selects HP vs LP coeffs). */
    void updateSlotHpLpCascade (bool isHighpassSlot, float cutoff, float q, int slopeChoice, int filterType);
    /** Load HP/LP cascade into a flexible mid/shelf bank. */
    void updateFlexibleHpLpCascade (int flexIndex, float cutoff, float q, int slopeChoice, int filterType);

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

    /**
        Detector envelope (dB) for OptionBox threshold meter.
        Bank 1: internal DSP index 0–7. Extended: global display 8–63.
    */
    float getPublishedDynEnvelopeDb (int bandIndexOrGlobal) const noexcept;

    /** Effective gain after dynamic / sidechain (same indexing as getPublishedDynEnvelopeDb). */
    float getPublishedEffectiveGainDb (int bandIndexOrGlobal) const noexcept;

    /**
        Swap static ↔ dynamic-range gain when the user toggles D.
        First enable starts range at 0 dB; later toggles restore the last range/static values.
        Preset/session load is ignored until the first observed state is latched.
        Indexing matches getPublishedDynEnvelopeDb.
    */
    void applyDynamicModeGainSwap (int bandIndexOrGlobal, bool nowDynamicOn);
    /** Clear dual-mode gain memory (band delete / double-click reset). */
    void clearDynamicModeGainMemory (int bandIndexOrGlobal) noexcept;

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

    /** Sample Match target curve (dB, mean-normalized shape) for graph overlay. */
    void sampleMatchTargetDb (const float* frequenciesHz, float* destDb, int numPoints) const;
    /** Sample Match GR onto display freqs for the sum curve; zeros if idle. */
    void sampleMatchGrDb (const float* frequenciesHz, float* destDb, int numPoints) const;

    SpectralDynamicsProcessor& getSpectralEngine() noexcept { return spectralEngine; }
    const SpectralDynamicsProcessor& getSpectralEngine() const noexcept { return spectralEngine; }

    MatchEq::Processor& getMatchEngine() noexcept { return matchEngine; }
    const MatchEq::Processor& getMatchEngine() const noexcept { return matchEngine; }

    /** Last-block Side Check GR (dB, negative when pulling Side down). */
    float getSideCheckGrDb() const noexcept { return sideCheck.getPublishedGrDb(); }

    bool hasExternalSidechainSignal() const noexcept { return collisionHasSc.load (std::memory_order_relaxed); }
    /** Hz of strongest SC-vs-main excess (0 if none). */
    float getCollisionPeakHz() const noexcept { return collisionPeakHz.load (std::memory_order_relaxed); }
    void copyCollisionOctaves (float* mainDb3, float* scDb3) const noexcept
    {
        if (mainDb3 != nullptr)
        {
            mainDb3[0] = collisionMainDb[0].load (std::memory_order_relaxed);
            mainDb3[1] = collisionMainDb[1].load (std::memory_order_relaxed);
            mainDb3[2] = collisionMainDb[2].load (std::memory_order_relaxed);
        }
        if (scDb3 != nullptr)
        {
            scDb3[0] = collisionScDb[0].load (std::memory_order_relaxed);
            scDb3[1] = collisionScDb[1].load (std::memory_order_relaxed);
            scDb3[2] = collisionScDb[2].load (std::memory_order_relaxed);
        }
    }

    /** Global EQ depth scale 0..2 (default 1 = 100%). Scales band gains + Match amount. */
    float getEqScale() const noexcept
    {
        if (auto* v = treeState.getRawParameterValue ("eqScale"))
            return juce::jlimit (0.0f, 2.0f, v->load());
        return 1.0f;
    }

    /**
        Smoothed mean transient weight from StructuralSplitEngine (0 = sustain, 1 = transient).
        Updated only while Learn is capturing stats, or when Split DSP is already armed.
    */
    float getPublishedTransientRatio() const noexcept
    {
        return publishedTransientRatio.load (std::memory_order_relaxed);
    }

    /**
        Learn capture/detect: enable lightweight T/S gain compute for source detection.
        Off by default — must not leave StructuralSplit running every block.
    */
    void setLearnTransientStatsCapture (bool shouldEnable) noexcept
    {
        learnTransientStatsCapture.store (shouldEnable, std::memory_order_relaxed);
    }
    bool isLearnTransientStatsCapture() const noexcept
    {
        return learnTransientStatsCapture.load (std::memory_order_relaxed);
    }

    /**
        Message-thread helpers for Match activation.
        disableActiveBands: store OnOff map and turn all bands off (restored by restoreMatchBandDisable).
    */
    void applyMatchBandDisable();
    void restoreMatchBandDisable();
    bool hasMatchBandDisableSnapshot() const noexcept { return matchBandDisableActive; }
    void syncMatchFactoryTargetFromParam();
    void captureMatchSpectrumFromAnalyser();

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
    /** Slow ballistics on the RMS levels that drive autogain (not the UI meters). */
    float autoGainPreDbSmooth = -100.0f;
    float autoGainPostDbSmooth = -100.0f;

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

    // True-peak (4× cubic inter-sample) at input / post taps.
    std::atomic<float> inputTruePeakDbLeft { -100.0f };
    std::atomic<float> inputTruePeakDbRight { -100.0f };
    std::atomic<float> postTruePeakDbLeft { -100.0f };
    std::atomic<float> postTruePeakDbRight { -100.0f };
    std::atomic<float> inputTruePeakDbMid { -100.0f };
    std::atomic<float> inputTruePeakDbSide { -100.0f };
    std::atomic<float> postTruePeakDbMid { -100.0f };
    std::atomic<float> postTruePeakDbSide { -100.0f };
    float inputTruePeakHistL[3] {};
    float inputTruePeakHistR[3] {};
    float postTruePeakHistL[3] {};
    float postTruePeakHistR[3] {};
    float inputTruePeakHistMid[3] {};
    float inputTruePeakHistSide[3] {};
    float postTruePeakHistMid[3] {};
    float postTruePeakHistSide[3] {};

    Analyser m_analyser;

    std::atomic<OscilloscopeComponent*> oscilloscopeTarget { nullptr };
    std::atomic<GoniometerComponent*> goniometerTarget { nullptr };
    std::atomic<SpectrogramComponent*> spectrogramTarget { nullptr };
    std::atomic<LoudnessComponent*> loudnessTarget { nullptr };
    std::atomic<StereogramComponent*> stereogramTarget { nullptr };
    std::atomic<HistogramComponent*> histogramTarget { nullptr };
    std::atomic<ThdMeterComponent*> thdTarget { nullptr };

#if SPEC3D_EXPORT_ENABLED
    ExportAudioRingBuffer exportAudioRing;
#endif

    std::atomic<bool> ecoMode { false };
    /** Spec3D solo: skip EQ spectrum + scopes (not spectrogram history / meters / DSP). */
    std::atomic<bool> bypassOtherAnalyzers { false };
    std::atomic<bool> scopeMode { false };
    /** When Scope is on: true = Post (DSP on), false = Pre (analyzer / meteringOnly). */
    std::atomic<bool> scopeTapPost { false };
    /** -1 = all Scope modules process; else only that ScopeModuleId is live. */
    std::atomic<int> scopeSoloModule { -1 };

    /** -1 = off; otherwise Bank1 internal 0–7 or global display 8–63. */
    std::atomic<int> bandListenIndex { -1 };

    std::atomic<bool> auditionBandpassActive { false };
    std::atomic<float> auditionBandpassFreqHz { 1000.0f };
    std::atomic<float> auditionBandpassQ { 1.0f };
    juce::AudioBuffer<float> auditionDryBuffer;

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
    DynamicEq::BandState dynHighpass, dynLowpass;
    DynamicEq::BandState dynHighShelf, dynLowShelf;

    struct DynModeGainMemory
    {
        float staticGainDb = 0.0f;
        float dynamicRangeDb = 0.0f;
        bool hasDynamicMemory = false;
        bool latched = false;
        bool lastDynamicOn = false;
    };
    std::array<DynModeGainMemory, EqBand::kMaxBands> dynModeGainMemory {};

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
    DynamicEq::BandState scDetectHighpass, scDetectLowpass;
    DynamicEq::BandState scDetectHighShelf, scDetectLowShelf;
    BandSidechain::GateState scGateBand1, scGateBand2, scGateBand3, scGateBand4;
    BandSidechain::GateState scGateHighpass, scGateLowpass;
    BandSidechain::GateState scGateHighShelf, scGateLowShelf;

    std::array<LfoMod::Voice, LfoMod::kNumLfos> lfoVoices;
    LfoMod::EnvFollower modEnvFollower;
    ShapeMod::Engine shapeEngine;
    int midiNotesHeld = 0;
    std::atomic<float> publishedEnvAmount { 0.0f };
    std::atomic<float> publishedEnvDb { -140.0f };
    std::atomic<float> publishedShapeBipolar { 0.0f };
    std::atomic<float> publishedShapePhase { 0.0f };

    std::atomic<bool> spec3dVisScEnabled { false };
    std::atomic<float> spec3dVisScHpHz { 40.0f };
    std::atomic<float> spec3dVisScLpHz { 150.0f };
    std::atomic<float> spec3dVisScThresholdDb { -24.0f };
    std::atomic<float> spec3dVisScAttackMs { 8.0f };
    std::atomic<float> spec3dVisScReleaseMs { 80.0f };
    std::atomic<float> spec3dVisScLevel01 { 0.0f };
    juce::dsp::IIR::Filter<float> spec3dVisScHpFilter;
    juce::dsp::IIR::Filter<float> spec3dVisScLpFilter;
    float spec3dVisScEnvDb = -140.0f;
    float spec3dVisScAppliedHpHz = -1.0f;
    float spec3dVisScAppliedLpHz = -1.0f;
    void processSpec3DVisualSidechain (const juce::AudioBuffer<float>& buffer, int numSamples) noexcept;

    /** One shared coarse bandpass bank for all S bands (GR on post-EQ; detect pre-EQ). Zero cost when no S is on. */
    SpectralDynamicsProcessor spectralEngine;

    /** Complementary transient / sustain splitter (min-phase per-band delta mix). */
    StructuralSplitEngine structuralSplitEngine;
    juce::AudioBuffer<float> splitDryBuffer;
    juce::AudioBuffer<float> splitTGainBuffer; // 1 ch: transient gains
    juce::AudioBuffer<float> splitSGainBuffer; // 1 ch: sustain gains
    std::atomic<float> publishedTransientRatio { 0.3f };
    std::atomic<bool> learnTransientStatsCapture { false };

    /** Global Side Check (S<=M): post-Spectral BP-lattice Mid/Side balance. */
    SideCheck::Processor sideCheck;

    /** Spectral Match — isolated shape-match lattice (Pre or Post EQ). */
    MatchEq::Processor matchEngine;
    juce::AudioBuffer<float> matchDetectBuffer;
    bool matchBandDisableActive = false;
    std::array<bool, EqBand::kMaxBands> matchBandOnSnapshot {};

    /** Per-band sat engines — Bank 1 uses internal indices 0–7; extended use global 8–63. */
    std::array<BandSaturation::Engine, EqBand::kMaxBands> bandSatEngines;

    /**
        Agnostic IIR slots for banks 2–8 (global display 8…63).
        Index = globalDisplay - kBankSize.
    */
    struct ExtendedBandSlot
    {
        std::array<StereoIIR, FilterSlope::maxBiquadStages> cascade {};
        StereoIIR single;
        int activeStages = 1;
        bool useCascade = false;
        CoeffCache lastCoeffs {};
    };

    /** Cached APVTS atomics — never build juce::String on the audio thread. */
    struct ExtendedParamPtrs
    {
        std::atomic<float>* on = nullptr;
        std::atomic<float>* frequency = nullptr;
        std::atomic<float>* q = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* slope = nullptr;
        std::atomic<float>* channel = nullptr;
        std::atomic<float>* dynamic = nullptr;
        std::atomic<float>* dynThreshold = nullptr;
        std::atomic<float>* attackMs = nullptr;
        std::atomic<float>* releaseMs = nullptr;
    };

    static constexpr int kNumExtended = EqBand::kMaxBands - EqBand::kBankSize;
    std::array<ExtendedBandSlot, kNumExtended> extendedSlots {};
    std::array<DynamicEq::BandState, kNumExtended> extendedDyn {};
    std::array<ExtendedParamPtrs, kNumExtended> extendedParams {};
    std::array<juce::LinearSmoothedValue<float>, kNumExtended> smoothExtFreq {};
    std::array<juce::LinearSmoothedValue<float>, kNumExtended> smoothExtQ {};
    std::array<juce::LinearSmoothedValue<float>, kNumExtended> smoothExtGain {};
    /** UI: how many banks have been opened (1…kMaxBanks). Grows when creating past a full bank. */
    std::atomic<int> banksOpened { 1 };
    /** Fast audio-thread gate — skip extended DSP when zero. */
    std::atomic<int> extendedOnCount { 0 };

    void cacheExtendedParamPointers();
    void refreshExtendedOnCount() noexcept;
    void prepareExtendedSlots (const juce::dsp::ProcessSpec& spec, int blockSize);
    void processExtendedBands (juce::dsp::AudioBlock<float>& audioBlock,
                               const float* dryL, const float* dryR, int numSamples,
                               bool proportionalQOn);
    void appendExtendedLinearPhaseSpecs (LinearPhaseEqEngine::BandSpec* specs, int& count) const;

    /** Stage 2 — post-Spectral bus sat. */
    BandSaturation::Engine spectralSatEngine;

    /** Single-biquad path when Band 1 / Band 8 slots are not in HP/LP cascade mode. */
    StereoIIR highpassBandFilter;
    StereoIIR lowpassBandFilter;
    /** Temporary Alt+drag isolate BP (post-EQ audition). */
    StereoIIR auditionBandpassFilter;

    BandSaturation::Engine& satEngineForBandIndex (int bandIndex) noexcept
    {
        if (bandIndex >= 0 && bandIndex < (int) bandSatEngines.size())
            return bandSatEngines[(size_t) bandIndex];
        return bandSatEngines[0];
    }

    /** Linear-phase FIR EQ path (used when phaseMode == Linear Phase). */
    LinearPhaseEqEngine linearPhaseEngine;
    int lastPhaseMode = PhaseMode::minimumPhase;

    /** Pre-EQ copy for spectral sidechain detect (threshold must not track cut/boost). */
    juce::AudioBuffer<float> spectralDetectBuffer;

    /** Keep host PDC stable when Bypass is on (delay dry by reported latency). */
    static constexpr int kMaxBypassCompDelay = 8192;
    juce::AudioBuffer<float> bypassCompBuffer; // circular, sized in prepareToPlay
    int bypassCompWritePos = 0;
    int bypassCompDelaySamples = -1;

    void updateReportedLatency() noexcept;
    int computeProcessingLatencySamples() noexcept;
    void applyBypassLatencyCompensation (juce::AudioBuffer<float>& buffer, int delaySamples) noexcept;

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

    std::atomic<bool> collisionHasSc { false };
    std::array<std::atomic<float>, 3> collisionMainDb {};
    std::array<std::atomic<float>, 3> collisionScDb {};
    std::atomic<float> collisionPeakHz { 0.0f };

    FrequencyResponseComponent* frequencyResponseComponent = nullptr;

    juce::AudioParameterBool* bypassParam = nullptr;

    juce::ValueTree abSnapshots[abSlotCount];
    AbSlot activeAbSlot = AbSlot::A;

    bool sessionUiThemeValid = false;
    SharedColors sessionUiColors;
    juce::ValueTree sessionColourRamps;
    juce::ValueTree sessionUiState; // type "UiSession"

    juce::ValueTree captureStateForSnapshot();
    void applySnapshotState (const juce::ValueTree& snapshot);
    juce::ValueTree& getAbSnapshot (AbSlot slot) noexcept;
    const juce::ValueTree& getAbSnapshot (AbSlot slot) const noexcept;
    void initialiseAbSnapshotsFromCurrentState();
    void storeAbSnapshotsInState (juce::ValueTree& state) const;
    void restoreAbSnapshotsFromState (const juce::ValueTree& state);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(EqProcessor)
};
