#include "EqLearnController.h"
#include "EqSourceTemplates.h"
#include "../EqProcessor.h"
#include "../EqBand.h"
#include "../FilterType.h"
#include "../Visualizer/Analyser.h"
#include <cmath>

namespace EqLearn
{

Controller::Controller (EqProcessor& p)
    : processor (p)
{
}

Controller::~Controller()
{
    stopTimer();
    processor.setLearnTransientStatsCapture (false);
}

void Controller::notifyChanged()
{
    if (onStateChanged)
        onStateChanged();
}

float Controller::sampleCrestDb() const
{
    const float peakL = processor.getInputPeakValue (0);
    const float peakR = processor.getInputPeakValue (1);
    const float rmsL = processor.getInputRmsValue (0);
    const float rmsR = processor.getInputRmsValue (1);
    const float peak = juce::jmax (peakL, peakR);
    const float rms = 0.5f * (rmsL + rmsR);
    // Meters are dB; crest ~ peak − RMS
    return juce::jlimit (0.0f, 40.0f, peak - rms);
}

bool Controller::startLearn()
{
    if (state == State::capturing)
        return false;

    lastResult = {};
    lastClassification = {};
    framesAccumulated = 0;
    binCount = 0;
    sumLinear.clear();
    binHz.clear();
    crestSum = 0.0;
    crestSamples = 0;
    transientSum = 0.0;
    transientSamples = 0;
    sessionPeakDb = -200.0f;
    capturePathLocked = false;
    captureUsePre = settings.usePreEq;

    auto& analyser = processor.getAnalyser();
    const int n = (int) analyser.getScopeSize();
    if (n < 8)
    {
        lastResult.ok = false;
        lastResult.message = "Spectrum not ready - play audio and try again";
        notifyChanged();
        return false;
    }

    if (analyser.isEcoMode())
    {
        lastResult.ok = false;
        lastResult.message = "Turn off Eco mode to Learn";
        notifyChanged();
        return false;
    }

    sumLinear.assign ((size_t) n, 0.0f);
    binHz.resize ((size_t) n);
    for (int i = 0; i < n; ++i)
        binHz[(size_t) i] = analyser.getBinFrequencyHz ((size_t) i);
    binCount = n;

    // Prefer pre-EQ so re-Learn is stable after previous bake.
    // Lock path on first good frame (see accumulateFrame).
    processor.setLearnTransientStatsCapture (true);

    state = State::capturing;
    captureStartMs = juce::Time::getMillisecondCounterHiRes();
    startTimerHz (30);
    notifyChanged();
    return true;
}

void Controller::cancelLearn()
{
    if (state != State::capturing)
        return;

    stopTimer();
    processor.setLearnTransientStatsCapture (false);
    state = State::idle;
    lastResult.ok = false;
    lastResult.message = "Learn cancelled";
    notifyChanged();
}

float Controller::getCaptureProgress() const noexcept
{
    if (state != State::capturing)
        return 0.0f;

    const float dur = juce::jlimit (kMinCaptureSec, kMaxCaptureSec, settings.captureSec);
    const double elapsed = (juce::Time::getMillisecondCounterHiRes() - captureStartMs) * 0.001;
    return juce::jlimit (0.0f, 1.0f, (float) (elapsed / (double) dur));
}

float Controller::getCaptureSecondsRemaining() const noexcept
{
    if (state != State::capturing)
        return 0.0f;

    const float dur = juce::jlimit (kMinCaptureSec, kMaxCaptureSec, settings.captureSec);
    const double elapsed = (juce::Time::getMillisecondCounterHiRes() - captureStartMs) * 0.001;
    return juce::jmax (0.0f, dur - (float) elapsed);
}

void Controller::timerCallback()
{
    if (state != State::capturing)
    {
        stopTimer();
        return;
    }

    accumulateFrame();

    const float dur = juce::jlimit (kMinCaptureSec, kMaxCaptureSec, settings.captureSec);
    const double elapsed = (juce::Time::getMillisecondCounterHiRes() - captureStartMs) * 0.001;
    notifyChanged();

    if (elapsed >= (double) dur)
        finishAndApply();
}

void Controller::accumulateFrame()
{
    auto& analyser = processor.getAnalyser();
    const int n = juce::jmin (binCount, (int) analyser.getScopeSize());
    if (n <= 0)
        return;

    if ((int) sumLinear.size() < n)
        sumLinear.resize ((size_t) n, 0.0f);

    std::vector<float> frame ((size_t) n);
    int got = 0;
    float peak = -200.0f;

    // Lock pre/post for the whole capture so we never mix dry + wet frames
    // (wet-after-bake was a major source of "different every press").
    if (! capturePathLocked)
    {
        captureUsePre = settings.usePreEq;
        got = analyser.copyScopeDataDb (captureUsePre, frame.data(), n);
        peak = analyser.getScopePeakDb (captureUsePre);
        if (got <= 0 || peak < kSilenceFloorDb)
        {
            if (captureUsePre)
            {
                captureUsePre = false;
                got = analyser.copyScopeDataDb (false, frame.data(), n);
                peak = analyser.getScopePeakDb (false);
            }
        }
        if (got <= 0 || peak < kSilenceFloorDb)
            return;
        capturePathLocked = true;
        sessionPeakDb = peak;
    }
    else
    {
        got = analyser.copyScopeDataDb (captureUsePre, frame.data(), n);
        peak = analyser.getScopePeakDb (captureUsePre);
        if (got <= 0 || peak < kSilenceFloorDb)
            return;

        // Dynamic gate: ignore quiet gaps between bass notes (Pro-Q-style stable avg)
        sessionPeakDb = juce::jmax (sessionPeakDb, peak);
        if (peak < sessionPeakDb - kCaptureDynamicGateDb)
            return;
    }

    const int useN = juce::jmin (got, n);
    for (int i = 0; i < useN; ++i)
    {
        // Linear-domain average - more stable than mean of dB frames
        const float lin = juce::Decibels::decibelsToGain (
            juce::jlimit (-120.0f, 12.0f, frame[(size_t) i]), -120.0f);
        sumLinear[(size_t) i] += lin;
    }

    crestSum += (double) sampleCrestDb();
    ++crestSamples;

    transientSum += (double) processor.getPublishedTransientRatio();
    ++transientSamples;

    if (useN != binCount && useN > 0)
    {
        binCount = useN;
        binHz.resize ((size_t) useN);
        for (int i = 0; i < useN; ++i)
            binHz[(size_t) i] = analyser.getBinFrequencyHz ((size_t) i);
    }

    ++framesAccumulated;
}

bool Controller::isSignalPresent() const
{
    return framesAccumulated > 0;
}

Classification Controller::detectNow()
{
    auto& analyser = processor.getAnalyser();
    const int n = (int) analyser.getScopeSize();
    if (n < 8 || analyser.isEcoMode())
    {
        lastClassification = {};
        lastClassification.summary = "Spectrum not ready";
        return lastClassification;
    }

    std::vector<float> db ((size_t) n);
    std::vector<float> hz ((size_t) n);
    const int got = analyser.copyScopeDataDb (settings.usePreEq, db.data(), n);
    for (int i = 0; i < got; ++i)
        hz[(size_t) i] = analyser.getBinFrequencyHz ((size_t) i);

    // Snapshot only - no arming of T/S compute (that runs during Learn capture only).
    // If Split DSP is already armed for bands, the published ratio is live; otherwise
    // classify without T/S terms so we don't force StructuralSplit every block.
    const float crest = sampleCrestDb();
    const float tRatio = processor.getPublishedTransientRatio();
    const bool hasTs = processor.isLearnTransientStatsCapture(); // normally false here

    lastClassification = SourceClassifier::classify (
        db.data(), hz.data(), got, crest, settings.minDetectConfidence, tRatio, hasTs);
    return lastClassification;
}

void Controller::buildTarget (std::vector<float>& targetDb,
                              std::vector<float>& targetHz,
                              int& targetN,
                              SourceClass templateClass) const
{
    targetN = 0;
    targetDb.clear();
    targetHz.clear();

    if (settings.target == Target::matchCurve)
    {
        constexpr int n = 96;
        targetDb.resize ((size_t) n);
        targetHz.resize ((size_t) n);
        SourceTemplates::makeLogGrid (targetHz.data(), n);
        processor.sampleMatchTargetDb (targetHz.data(), targetDb.data(), n);
        targetN = n;
        return;
    }

    if (templateClass != SourceClass::unknown
        || isSourceTemplateTarget (settings.target))
    {
        SourceClass cls = templateClass;
        if (cls == SourceClass::unknown)
            cls = sourceClassForTarget (settings.target);
        if (cls == SourceClass::unknown)
            cls = SourceClass::mix;

        constexpr int n = 96;
        targetDb.resize ((size_t) n);
        targetHz.resize ((size_t) n);
        SourceTemplates::makeLogGrid (targetHz.data(), n);
        SourceTemplates::fillSourceTargetDb (cls, targetHz.data(), targetDb.data(), n);
        targetN = n;
        return;
    }

    // Factory pink/flat - leave empty so fitter builds factory target
    targetN = 0;
}

void Controller::setParamFloat (const juce::String& id, float value)
{
    if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (processor.treeState.getParameter (id)))
    {
        const float v = juce::jlimit (p->getNormalisableRange().start,
                                      p->getNormalisableRange().end,
                                      value);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (v));
        p->endChangeGesture();
    }
}

void Controller::setParamChoice (const juce::String& id, int index)
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (processor.treeState.getParameter (id)))
    {
        const int maxIdx = juce::jmax (0, p->choices.size() - 1);
        const int idx = juce::jlimit (0, maxIdx, index);
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 ((float) idx));
        p->endChangeGesture();
    }
}

void Controller::setParamBool (const juce::String& id, bool on)
{
    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (processor.treeState.getParameter (id)))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (on ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void Controller::assignSlots (juce::Array<BandProposal>& proposals) const
{
    // Clear-bank bake slots (display global indices):
    //   Bank 1 tonal: LS=1, peaks=2..5, HS=6  (leave HP=0, LP=7 alone)
    //   Spill peaks into Bank 2 slots 8..15 until total ≤ 10
    const int lowShelfSlot = 1;
    const int highShelfSlot = 6;

    juce::Array<int> peakSlots;
    for (int g = 2; g <= 5; ++g)
        peakSlots.add (g);
    for (int g = 8; g <= 15; ++g) // Bank 2 type-agnostic
        peakSlots.add (g);

    // Sort: shelves first (LS then HS), then peaking by frequency (mix-habit readability)
    struct Order
    {
        static int compareElements (const BandProposal& a, const BandProposal& b) noexcept
        {
            if (a.isShelf != b.isShelf)
                return a.isShelf ? -1 : 1;
            if (a.isShelf && b.isShelf)
            {
                // low shelf before high shelf
                const bool aLo = a.filterType == FilterType::lowShelf;
                const bool bLo = b.filterType == FilterType::lowShelf;
                if (aLo != bLo)
                    return aLo ? -1 : 1;
            }
            if (a.frequencyHz < b.frequencyHz) return -1;
            if (a.frequencyHz > b.frequencyHz) return 1;
            return 0;
        }
    };
    Order comp;
    proposals.sort (comp);

    int peakIdx = 0;
    bool usedLS = false, usedHS = false;

    for (auto& p : proposals)
    {
        p.globalDisplay = -1;

        if (p.isShelf && p.filterType == FilterType::lowShelf && ! usedLS)
        {
            p.globalDisplay = lowShelfSlot;
            usedLS = true;
            continue;
        }
        if (p.isShelf && p.filterType == FilterType::highShelf && ! usedHS)
        {
            p.globalDisplay = highShelfSlot;
            usedHS = true;
            continue;
        }

        // Peaking (or overflow shelf as bell)
        if (peakIdx < peakSlots.size())
        {
            p.globalDisplay = peakSlots[peakIdx++];
            p.isShelf = false;
            p.filterType = FilterType::bell;
        }
    }
}

void Controller::applyProposals (const juce::Array<BandProposal>& proposals)
{
    {
        auto state = processor.treeState.copyState();
        revertSnapshot.reset();
        juce::MemoryOutputStream mo (revertSnapshot, false);
        state.writeToStream (mo);
        hasRevertSnapshot = revertSnapshot.getSize() > 0;
    }

    processor.getUndoManager().beginNewTransaction ("Learn EQ");

    // Always clear Bank 1 tonal + Bank 2 spill range so leftover settings don't stack.
    juce::Array<int> clearSlots;
    clearSlots.add (1); // LS
    for (int g = 2; g <= 5; ++g)
        clearSlots.add (g);
    clearSlots.add (6); // HS
    for (int g = 8; g <= 15; ++g)
        clearSlots.add (g);

    for (int g : clearSlots)
    {
        setParamFloat (EqBand::gainParamIDForGlobal (g), 0.0f);
        setParamBool (EqBand::onOffParamIDForGlobal (g), false);
    }

    juce::Array<int> usedSlots;
    int applied = 0;
    for (const auto& p : proposals)
    {
        if (p.globalDisplay < 0)
            continue;

        const int g = p.globalDisplay;
        setParamChoice (EqBand::typeParamIDForGlobal (g), p.filterType);
        setParamFloat (EqBand::frequencyParamIDForGlobal (g), p.frequencyHz);
        setParamFloat (EqBand::gainParamIDForGlobal (g), p.gainDb);
        setParamFloat (EqBand::qParamIDForGlobal (g), p.q);
        setParamBool (EqBand::onOffParamIDForGlobal (g), true);
        usedSlots.addIfNotAlreadyThere (g);
        ++applied;
    }

    juce::ignoreUnused (usedSlots);
    lastResult.bandsApplied = applied;
    lastResult.applySerial = ++applySerialCounter;
}

bool Controller::revertLastLearn()
{
    if (! hasRevertSnapshot || revertSnapshot.getSize() == 0)
        return false;

    juce::MemoryInputStream mi (revertSnapshot, false);
    auto state = juce::ValueTree::readFromStream (mi);
    if (! state.isValid())
        return false;

    processor.getUndoManager().beginNewTransaction ("Revert Learn");
    processor.treeState.replaceState (state);
    hasRevertSnapshot = false;
    lastResult.message = "Reverted last Learn";
    notifyChanged();
    return true;
}

void Controller::finishAndApply()
{
    stopTimer();
    processor.setLearnTransientStatsCapture (false);
    state = State::idle;

    if (! isSignalPresent() || framesAccumulated <= 0 || binCount <= 0)
    {
        lastResult.ok = false;
        lastResult.message = "Play audio to Learn";
        lastResult.bandsApplied = 0;
        notifyChanged();
        return;
    }

    if (framesAccumulated < kMinCaptureFrames)
    {
        lastResult.ok = false;
        lastResult.message = "Not enough solid frames - play louder / longer ("
                             + juce::String (framesAccumulated) + "/"
                             + juce::String (kMinCaptureFrames) + ")";
        lastResult.bandsApplied = 0;
        notifyChanged();
        return;
    }

    // Linear average -> dB (stable spectral shape across note gaps)
    std::vector<float> avgDb ((size_t) binCount);
    const float inv = 1.0f / (float) framesAccumulated;
    for (int i = 0; i < binCount; ++i)
    {
        const float lin = sumLinear[(size_t) i] * inv;
        avgDb[(size_t) i] = juce::Decibels::gainToDecibels (juce::jmax (1.0e-12f, lin), -120.0f);
    }

    float peakDb = -200.0f;
    for (int i = 0; i < binCount; ++i)
        peakDb = juce::jmax (peakDb, avgDb[(size_t) i]);
    if (peakDb < kSilenceFloorDb)
    {
        lastResult.ok = false;
        lastResult.message = "Signal too quiet - raise level and try again (peak "
                             + juce::String (peakDb, 1) + " dB)";
        notifyChanged();
        return;
    }

    const float crest = (crestSamples > 0)
        ? (float) (crestSum / (double) crestSamples)
        : sampleCrestDb();
    const bool hasTs = transientSamples > 0;
    const float tRatio = hasTs
        ? (float) (transientSum / (double) transientSamples)
        : processor.getPublishedTransientRatio();

    // Always run classifier for reporting; use result when Auto-detect is selected.
    lastClassification = SourceClassifier::classify (
        avgDb.data(), binHz.data(), binCount, crest, settings.minDetectConfidence,
        tRatio, hasTs);

    lastResult.features = lastClassification.features;
    lastResult.detectedClass = lastClassification.label;
    lastResult.detectConfidence = lastClassification.confidence;
    lastResult.usedDetection = (settings.target == Target::autoSource);

    SourceClass templateClass = SourceClass::unknown;
    Target effectiveTarget = settings.target;

    if (settings.target == Target::autoSource)
    {
        if (lastClassification.label != SourceClass::unknown)
        {
            templateClass = lastClassification.label;
            lastResult.appliedSourceClass = templateClass;
        }
        else
        {
            // Low confidence -> pink fallback (honest UI)
            effectiveTarget = Target::pink;
            templateClass = SourceClass::unknown;
            lastResult.appliedSourceClass = SourceClass::unknown;
        }
    }
    else if (isSourceTemplateTarget (settings.target))
    {
        templateClass = sourceClassForTarget (settings.target);
        lastResult.appliedSourceClass = templateClass;
    }

    // Temporarily use effective pink when falling back from auto
    const Target savedTarget = settings.target;
    if (effectiveTarget == Target::pink && settings.target == Target::autoSource)
    {
        // buildTarget with unknown + non-source -> pink factory
    }

    std::vector<float> targetDb, targetHz;
    int targetN = 0;

    auto runFitAndApply = [this] (const float* avg, const float* hz, int n,
                                  const float* tDb, const float* tHz, int tN,
                                  Settings fitSettings,
                                  const juce::String& baseMsg) -> bool
    {
        juce::Array<BandProposal> proposals;
        float mae = 0.0f, resMae = 0.0f;
        BandFitter::fit (avg, hz, n, tDb, tHz, tN, fitSettings, proposals, &mae, &resMae);
        assignSlots (proposals);
        for (int i = proposals.size(); --i >= 0;)
            if (proposals.getReference (i).globalDisplay < 0)
                proposals.remove (i);

        lastResult.fitMaeDb = mae;
        lastResult.residualMaeDb = resMae;

        if (proposals.isEmpty())
        {
            lastResult.ok = false;
            lastResult.bandsApplied = 0;
            lastResult.message = baseMsg.isNotEmpty()
                ? baseMsg + " - no significant spectral error"
                : juce::String ("No significant spectral error to correct");
            return false;
        }

        applyProposals (proposals);
        lastResult.ok = true;
        lastResult.target = settings.target;
        lastResult.strength = settings.strength;
        lastResult.proposals = proposals;
        lastResult.message = baseMsg
            + " | " + juce::String (lastResult.bandsApplied) + " bands"
            + " | MAE " + juce::String (mae, 1) + " dB"
            + (settings.usePreEq ? " | pre-EQ" : " | post-EQ");
        return true;
    };

    if (settings.target == Target::autoSource && templateClass == SourceClass::unknown)
    {
        Settings fitSettings = settings;
        fitSettings.target = Target::pink;
        const juce::String base = lastClassification.summary + " -> Pink fallback";
        if (! runFitAndApply (avgDb.data(), binHz.data(), binCount,
                              nullptr, nullptr, 0, fitSettings, base))
        {
            notifyChanged();
            return;
        }
        juce::ignoreUnused (savedTarget);
        notifyChanged();
        return;
    }

    buildTarget (targetDb, targetHz, targetN, templateClass);

    Settings fitSettings = settings;
    if (isSourceTemplateTarget (settings.target) || templateClass != SourceClass::unknown)
        fitSettings.target = Target::pink; // ignored when targetDb provided
    else
        fitSettings.target = settings.target;

    juce::String baseMsg;
    if (settings.target == Target::autoSource)
    {
        baseMsg = lastClassification.summary
            + " -> " + sourceClassName (lastResult.appliedSourceClass) + " template";
    }
    else if (isSourceTemplateTarget (settings.target))
    {
        baseMsg = "target " + sourceClassName (lastResult.appliedSourceClass)
            + " | detect " + lastClassification.summary;
    }
    else
    {
        baseMsg = "target " + targetShortName (settings.target)
            + " | detect " + lastClassification.summary;
    }

    if (! runFitAndApply (avgDb.data(), binHz.data(), binCount,
                          targetN > 0 ? targetDb.data() : nullptr,
                          targetN > 0 ? targetHz.data() : nullptr,
                          targetN,
                          fitSettings, baseMsg))
    {
        notifyChanged();
        return;
    }

    juce::ignoreUnused (savedTarget);
    notifyChanged();
}

} // namespace EqLearn
