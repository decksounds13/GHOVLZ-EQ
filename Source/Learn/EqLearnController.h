#pragma once

#include "EqLearnSettings.h"
#include "EqBandFitter.h"
#include "EqSourceClassifier.h"
#include <JuceHeader.h>
#include <vector>

class EqProcessor;

namespace EqLearn
{
    /**
        Message-thread Learn session: capture analyser frames → (optional detect) → fit → apply.
        Never runs on the audio thread.
    */
    class Controller : private juce::Timer
    {
    public:
        explicit Controller (EqProcessor& processor);
        ~Controller() override;

        Settings& getSettings() noexcept { return settings; }
        const Settings& getSettings() const noexcept { return settings; }

        bool isLearning() const noexcept { return state == State::capturing; }
        bool canRevert() const noexcept { return hasRevertSnapshot; }

        /** Start capture + fit with current settings. Returns false if already learning. */
        bool startLearn();
        void cancelLearn();

        /** Restore pre-Learn APVTS snapshot if available. */
        bool revertLastLearn();

        /** Progress 0..1 while capturing; 1 when idle after finish. */
        float getCaptureProgress() const noexcept;
        /** Seconds remaining in capture window (0 when idle). */
        float getCaptureSecondsRemaining() const noexcept;

        const Result& getLastResult() const noexcept { return lastResult; }
        const Classification& getLastClassification() const noexcept { return lastClassification; }

        /**
            Classify current averaged capture (or live analyser snapshot) without applying EQ.
            Useful for UI preview; does not start a full Learn.
        */
        Classification detectNow();

        /** Optional: UI repaint / status hook after state changes. */
        std::function<void()> onStateChanged;

    private:
        enum class State { idle, capturing };

        void timerCallback() override;
        void accumulateFrame();
        void finishAndApply();
        bool isSignalPresent() const;
        float sampleCrestDb() const;
        void buildTarget (std::vector<float>& targetDb,
                          std::vector<float>& targetHz,
                          int& targetN,
                          SourceClass templateClass) const;
        void applyProposals (const juce::Array<BandProposal>& proposals);
        void assignSlots (juce::Array<BandProposal>& proposals) const;
        void setParamFloat (const juce::String& id, float value);
        void setParamChoice (const juce::String& id, int index);
        void setParamBool (const juce::String& id, bool on);
        void notifyChanged();

        EqProcessor& processor;
        Settings settings;
        State state = State::idle;
        Result lastResult;
        Classification lastClassification;

        double captureStartMs = 0.0;
        int framesAccumulated = 0;
        /** Linear-magnitude sum per bin (converted to dB only after capture). */
        std::vector<float> sumLinear;
        std::vector<float> binHz;
        int binCount = 0;
        double crestSum = 0.0;
        int crestSamples = 0;
        double transientSum = 0.0;
        int transientSamples = 0;
        /** Peak dB of loudest frame in this session (dynamic gate). */
        float sessionPeakDb = -200.0f;
        /** Lock pre/post path for whole capture so frames stay comparable. */
        bool captureUsePre = true;
        bool capturePathLocked = false;

        juce::MemoryBlock revertSnapshot;
        bool hasRevertSnapshot = false;
        /** Never reset — UI detects new applies via Result::applySerial. */
        int applySerialCounter = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Controller)
    };
}
