#pragma once

#include <JuceHeader.h>
#include <cmath>

/** GHOVLZ DYN parameter IDs and ranges. Max 6 bands. */
namespace DynParams
{
    constexpr int kMaxBands = 6;

    inline juce::String bandId (int band, const char* suffix)
    {
        return "dynB" + juce::String (band + 1) + suffix;
    }

    inline const char* countId() noexcept { return "dynBandCount"; }
    inline const char* selectedId() noexcept { return "dynSelected"; }
    inline const char* faceAllId() noexcept { return "dynFaceAll"; }
    inline const char* lookaheadId() noexcept { return "dynLookaheadMs"; }
    inline const char* grAvgMsId() noexcept { return "DYN_GR_AVG_MS_ID"; }
    inline const char* grFallMsId() noexcept { return "DYN_GR_FALL_MS_ID"; }
    inline const char* autoGainModeId() noexcept { return "dynAutoGainMode"; }
    inline const char* timeId() noexcept { return "dynGlobalTime"; }
    inline const char* amountId() noexcept { return "dynGlobalAmount"; }
    inline const char* downAmtId() noexcept { return "dynDownAmt"; }
    inline const char* upAmtId() noexcept { return "dynUpAmt"; }
    inline const char* detectId() noexcept { return "dynDetect"; }
    inline const char* osRealtimeId() noexcept { return "dynOsRealtime"; }
    inline const char* osOfflineId() noexcept { return "dynOsOffline"; }

    enum OsChoice : int { osOff = 0, os2x, os4x, os8x, os16x, osNumChoices };

    inline juce::StringArray osRealtimeNames()
    {
        return { "OS Off", "OS 2x", "OS 4x", "OS 8x" };
    }

    inline juce::StringArray osOfflineNames()
    {
        return { "Off", "2x", "4x", "8x", "16x" };
    }

    /** log2 factor: Off=0, 2x=1, 4x=2, 8x=3, 16x=4 */
    inline int osLog2FromIndex (int index, bool offline) noexcept
    {
        const int maxI = offline ? 4 : 3;
        return juce::jlimit (0, maxI, index);
    }

    inline juce::String thresholdId    (int b) { return bandId (b, "Threshold"); }
    inline juce::String upThresholdId  (int b) { return bandId (b, "UpThreshold"); }
    inline juce::String ratioId        (int b) { return bandId (b, "Ratio"); }
    inline juce::String attackId    (int b) { return bandId (b, "Attack"); }
    inline juce::String releaseId   (int b) { return bandId (b, "Release"); }
    inline juce::String kneeId      (int b) { return bandId (b, "Knee"); }
    inline juce::String makeupId    (int b) { return bandId (b, "Makeup"); }
    inline juce::String mixId       (int b) { return bandId (b, "Mix"); }
    inline juce::String onId        (int b) { return bandId (b, "On"); }
    inline juce::String soloId      (int b) { return bandId (b, "Solo"); }
    inline juce::String autoId      (int b) { return bandId (b, "Auto"); }
    inline juce::String splitId        (int b) { return bandId (b, "SplitHz"); }
    inline juce::String clipId         (int b) { return bandId (b, "Clip"); }
    inline juce::String clipThrId      (int b) { return bandId (b, "ClipThr"); }
    inline juce::String clipModeId     (int b) { return bandId (b, "ClipMode"); }

    inline juce::NormalisableRange<float> thresholdRange() { return { -60.0f, 0.0f, 0.1f }; }
    constexpr float kMinThrGapDb = 1.0f;
    inline juce::NormalisableRange<float> ratioRange()
    {
        juce::NormalisableRange<float> r { 0.01f, 100.0f, 0.01f };
        r.setSkewForCentre (1.0f);
        return r;
    }
    inline juce::NormalisableRange<float> attackRange()    { return { 0.1f, 200.0f, 0.1f, 0.4f }; }
    inline juce::NormalisableRange<float> releaseRange()   { return { 10.0f, 2000.0f, 1.0f, 0.4f }; }
    inline juce::NormalisableRange<float> kneeRange()      { return { 0.0f, 24.0f, 0.1f }; }
    inline juce::NormalisableRange<float> makeupRange()    { return { -24.0f, 24.0f, 0.1f }; }
    inline juce::NormalisableRange<float> mixRange()       { return { 0.0f, 100.0f, 0.1f }; }
    inline juce::NormalisableRange<float> splitRange()     { return { 20.0f, 20000.0f, 1.0f, 0.3f }; }
    inline juce::NormalisableRange<float> lookaheadRange() { return { 0.0f, 20.0f, 0.01f, 0.4f }; }
    inline juce::NormalisableRange<float> grAvgRange()      { return { 20.0f, 2000.0f, 1.0f, 0.45f }; }
    inline juce::NormalisableRange<float> grFallRange()     { return { 20.0f, 4000.0f, 1.0f, 0.45f }; }
    inline juce::NormalisableRange<float> timeRange()       { return { 10.0f, 400.0f, 1.0f, 0.45f }; }
    inline juce::NormalisableRange<float> amountRange()     { return { 0.0f, 200.0f, 1.0f }; }
    inline juce::NormalisableRange<float> dirAmtRange()     { return { 0.0f, 200.0f, 1.0f }; }
    inline juce::NormalisableRange<float> clipRange()       { return { 0.0f, 24.0f, 0.1f }; }
    enum DetectChoice : int { detectPeak = 0, detectRms = 1 };
    inline juce::StringArray detectNames() { return { "Peak", "RMS" }; }
    constexpr int kBandCardW = 252;
    constexpr int kGlobalPanelW = 126;

    inline void addParameters (std::vector<std::unique_ptr<juce::RangedAudioParameter>>& params)
    {
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            countId(), "Band Count", 1, kMaxBands, 1));
        params.push_back (std::make_unique<juce::AudioParameterInt> (
            selectedId(), "Selected Band", 0, kMaxBands - 1, 0));
        params.push_back (std::make_unique<juce::AudioParameterBool> (
            faceAllId(), "Face All", false));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            lookaheadId(), "Lookahead", lookaheadRange(), 0.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            grAvgMsId(), "GR Averaging", grAvgRange(), 180.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            grFallMsId(), "GR Fall Time", grFallRange(), 400.0f));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            autoGainModeId(), "Auto Gain Mode",
            juce::StringArray { "Multiband", "Global" }, 0));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            timeId(), "Time", timeRange(), 100.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            amountId(), "Amount", amountRange(), 100.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            downAmtId(), "Down", dirAmtRange(), 100.0f));
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            upAmtId(), "Up", dirAmtRange(), 100.0f));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            detectId(), "Detector", detectNames(), detectPeak));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            osRealtimeId(), "Oversample", osRealtimeNames(), 0));
        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            osOfflineId(), "Offline Oversample", osOfflineNames(), 4));

        const float defaultSplits[kMaxBands] = { 120.0f, 400.0f, 1200.0f, 3500.0f, 8000.0f, 20000.0f };

        for (int b = 0; b < kMaxBands; ++b)
        {
            const auto n = juce::String (b + 1);
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                thresholdId (b), "Band " + n + " Threshold", thresholdRange(), -18.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                upThresholdId (b), "Band " + n + " Up Threshold", thresholdRange(), -36.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                ratioId (b), "Band " + n + " Ratio", ratioRange(), 4.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                attackId (b), "Band " + n + " Attack", attackRange(), 12.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                releaseId (b), "Band " + n + " Release", releaseRange(), 180.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                kneeId (b), "Band " + n + " Knee", kneeRange(), 8.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                makeupId (b), "Band " + n + " Makeup", makeupRange(), 0.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                mixId (b), "Band " + n + " Mix", mixRange(), 100.0f));
            params.push_back (std::make_unique<juce::AudioParameterBool> (
                onId (b), "Band " + n + " On", true));
            params.push_back (std::make_unique<juce::AudioParameterBool> (
                soloId (b), "Band " + n + " Solo", false));
            params.push_back (std::make_unique<juce::AudioParameterBool> (
                autoId (b), "Band " + n + " Auto", false));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                splitId (b), "Band " + n + " Split", splitRange(), defaultSplits[b]));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                clipId (b), "Band " + n + " Clip", clipRange(), 0.0f));
            params.push_back (std::make_unique<juce::AudioParameterFloat> (
                clipThrId (b), "Band " + n + " Clip Thr", thresholdRange(), 0.0f));
            params.push_back (std::make_unique<juce::AudioParameterChoice> (
                clipModeId (b), "Band " + n + " Clip Mode",
                juce::StringArray { "Soft", "Hard" }, 0));
        }
    }

    /** Ableton Time: scale every band attack / release. 100 = as written. */
    inline float scaleTimeMs (float ms, float timePct) noexcept
    {
        return juce::jmax (0.05f, ms * juce::jmax (0.01f, timePct * 0.01f));
    }

    /** Ableton Amount: blend every ratio toward 1:1. 0 = off, 100 = as written, 200 = twice. */
    inline float scaleRatio (float ratio, float amountPct) noexcept
    {
        const float a = juce::jmax (0.0f, amountPct * 0.01f);
        return 1.0f + (ratio - 1.0f) * a;
    }

    /** Drive (optional) then clip to ceilingDb. mode 0 = tanh (Soft), else hard. */
    inline float clipSample (float x, float driveDb, int mode, float ceilingDb = 0.0f) noexcept
    {
        const bool driveOn = driveDb > 0.001f;
        const bool ceilOn  = ceilingDb < -0.05f;
        if (! driveOn && ! ceilOn)
            return x;
        const float driven = driveOn ? x * juce::Decibels::decibelsToGain (driveDb) : x;
        const float ceilLin = juce::Decibels::decibelsToGain (juce::jlimit (-60.0f, 0.0f, ceilingDb));
        if (mode != 0)
            return juce::jlimit (-ceilLin, ceilLin, driven);
        const float n = driven / juce::jmax (1.0e-6f, ceilLin);
        return std::tanh (n) * ceilLin;
    }

    inline juce::String formatRatio (float ratio) noexcept
    {
        if (ratio >= 99.0f)
            return "inf : 1";
        if (ratio > 1.001f)
            return juce::String (ratio, 1) + " : 1";
        if (ratio < 0.0105f)
            return "1 : inf";
        if (ratio < 0.999f)
        {
            const float inv = 1.0f / juce::jmax (0.01f, ratio);
            if (inv >= 99.0f)
                return "1 : inf";
            return "1 : " + juce::String (inv, 1);
        }
        return "1 : 1";
    }

    /** Pull-back in dB for a distance past a threshold. Ratio 1 = none. */
    inline float compressAmountDb (float dist, float ratio, float knee) noexcept
    {
        const float r = juce::jmax (1.0f, ratio);
        const float half = 0.5f * juce::jmax (0.0f, knee);
        float soft = 0.0f;
        if (knee <= 0.001f)
            soft = dist > 0.0f ? dist : 0.0f;
        else if (dist > half)
            soft = dist;
        else if (dist > -half)
        {
            const float x = dist + half;
            soft = (x * x) / (2.0f * knee);
        }
        return r >= 99.0f ? soft : (soft - soft / r);
    }

    inline void orderThresholds (float& downThr, float& upThr) noexcept
    {
        downThr = juce::jlimit (-60.0f, 0.0f, downThr);
        upThr   = juce::jlimit (-60.0f, 0.0f, upThr);
        if (downThr < upThr + kMinThrGapDb)
            downThr = juce::jmin (0.0f, upThr + kMinThrGapDb);
        if (upThr > downThr - kMinThrGapDb)
            upThr = juce::jmax (-60.0f, downThr - kMinThrGapDb);
    }

    /** R >= 1: down R:1 above Down Thr and 1:R up below Up Thr. R < 1: upward only.
        downAmt / upAmt (0-200, 100 = as written) scale each direction toward 1:1. */
    inline float gainDbDual (float envDb, float downThr, float upThr, float ratio, float knee,
                             float downAmtPct = 100.0f, float upAmtPct = 100.0f) noexcept
    {
        orderThresholds (downThr, upThr);
        float rDown = ratio > 1.001f ? ratio : 1.0f;
        float rUp   = ratio > 1.001f ? ratio
                    : (ratio < 0.999f ? 1.0f / juce::jmax (0.01f, ratio) : 1.0f);
        rDown = scaleRatio (rDown, downAmtPct);
        rUp   = scaleRatio (rUp, upAmtPct);
        float g = 0.0f;
        if (rDown > 1.001f && envDb > downThr)
            g -= compressAmountDb (envDb - downThr, rDown, knee);
        // Gate upward on silence: below -60 the distance to Up Thr is huge and
        // would slam +tens of dB, filling the green meters when audio stops.
        if (rUp > 1.001f && envDb < upThr && envDb > -72.0f)
        {
            const float effective = juce::jmax (envDb, -60.0f);
            float amt = compressAmountDb (upThr - effective, rUp, knee);
            if (envDb < -60.0f)
                amt *= juce::jlimit (0.0f, 1.0f, (envDb + 72.0f) / 12.0f);
            g += amt;
        }
        return g;
    }

    inline float transferOutDb (float inDb, float downThr, float upThr, float ratio, float knee,
                                float downAmtPct = 100.0f, float upAmtPct = 100.0f) noexcept
    {
        return inDb + gainDbDual (inDb, downThr, upThr, ratio, knee, downAmtPct, upAmtPct);
    }

    inline void writeDownThr (juce::AudioProcessorValueTreeState& state, int band, float v)
    {
        float down = thresholdRange().snapToLegalValue (v);
        float up = -36.0f;
        if (auto* p = state.getRawParameterValue (upThresholdId (band)))
            up = p->load();
        if (down < up + kMinThrGapDb)
            down = juce::jmin (0.0f, up + kMinThrGapDb);
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (thresholdId (band))))
            *p = down;
    }

    inline void writeUpThr (juce::AudioProcessorValueTreeState& state, int band, float v)
    {
        float up = thresholdRange().snapToLegalValue (v);
        float down = -18.0f;
        if (auto* p = state.getRawParameterValue (thresholdId (band)))
            down = p->load();
        if (up > down - kMinThrGapDb)
            up = juce::jmax (-60.0f, down - kMinThrGapDb);
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (upThresholdId (band))))
            *p = up;
    }

    inline void writeClipThr (juce::AudioProcessorValueTreeState& state, int band, float v)
    {
        if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (clipThrId (band))))
            *p = thresholdRange().snapToLegalValue (v);
    }

    inline int readCount (juce::AudioProcessorValueTreeState& state)
    {
        if (auto* p = state.getRawParameterValue (countId()))
            return juce::jlimit (1, kMaxBands, (int) std::lround (p->load()));
        return 1;
    }

    inline void copyBand (juce::AudioProcessorValueTreeState& state, int from, int to)
    {
        auto setF = [&state] (const juce::String& id, float v)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (id)))
                *p = v;
        };
        auto setB = [&state] (const juce::String& id, bool v)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (id)))
                *p = v;
        };
        auto getF = [&state] (const juce::String& id, float fb) -> float
        {
            if (auto* p = state.getRawParameterValue (id))
                return p->load();
            return fb;
        };
        auto getB = [&state] (const juce::String& id) -> bool
        {
            if (auto* p = state.getRawParameterValue (id))
                return p->load() > 0.5f;
            return false;
        };

        setF (thresholdId (to),   getF (thresholdId (from), -18.0f));
        setF (upThresholdId (to), getF (upThresholdId (from), -36.0f));
        setF (ratioId (to),       getF (ratioId (from), 4.0f));
        setF (attackId (to),    getF (attackId (from), 12.0f));
        setF (releaseId (to),   getF (releaseId (from), 180.0f));
        setF (kneeId (to),      getF (kneeId (from), 8.0f));
        setF (makeupId (to),    getF (makeupId (from), 0.0f));
        setF (mixId (to),       getF (mixId (from), 100.0f));
        setF (splitId (to),        getF (splitId (from), 1000.0f));
        setF (clipId (to),         getF (clipId (from), 0.0f));
        setF (clipThrId (to),      getF (clipThrId (from), 0.0f));
        setB (onId (to),           getB (onId (from)));
        setB (soloId (to),         getB (soloId (from)));
        if (auto* dst = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (clipModeId (to))))
            if (auto* src = dynamic_cast<juce::AudioParameterChoice*> (state.getParameter (clipModeId (from))))
                *dst = src->getIndex();
    }

    /** Insert a crossover at hz. Returns the new band index, or -1. */
    inline int insertSplitAtHz (juce::AudioProcessorValueTreeState& state, float hz)
    {
        const int n = readCount (state);
        if (n >= kMaxBands)
            return -1;

        hz = splitRange().snapToLegalValue (hz);

        auto getSplit = [&state] (int b, float fb) -> float
        {
            if (auto* p = state.getRawParameterValue (splitId (b)))
                return p->load();
            return fb;
        };

        int host = n - 1;
        float lo = 20.0f;
        for (int b = 0; b < n; ++b)
        {
            const float hi = (b < n - 1) ? getSplit (b, 1000.0f) : 20000.0f;
            if (hz > lo && hz < hi)
            {
                host = b;
                break;
            }
            lo = hi;
        }

        for (int b = n - 1; b > host; --b)
            copyBand (state, b, b + 1);

        if (auto* sp = dynamic_cast<juce::AudioParameterFloat*> (state.getParameter (splitId (host))))
            *sp = hz;
        if (auto* c = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (countId())))
            *c = n + 1;
        if (auto* s = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (selectedId())))
            *s = host + 1;
        return host + 1;
    }

    inline bool removeBandAt (juce::AudioProcessorValueTreeState& state, int band)
    {
        const int n = readCount (state);
        if (n <= 1 || band < 0 || band >= n)
            return false;

        for (int b = band; b < n - 1; ++b)
            copyBand (state, b + 1, b);

        if (auto* c = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (countId())))
            *c = n - 1;
        if (auto* s = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (selectedId())))
            *s = juce::jlimit (0, n - 2, band > 0 ? band - 1 : 0);
        return true;
    }
}
