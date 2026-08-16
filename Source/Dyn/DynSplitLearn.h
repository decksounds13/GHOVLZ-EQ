#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>
#include "DynParams.h"
#include "DynCompressor.h"
#include "../Visualizer/Analyser.h"

/** Ozone-style Learn: hunt splits and/or tickle Down/Up thresholds for ~5 s. */
class DynSplitLearn : private juce::Timer
{
public:
    bool wantSplits = true;
    bool wantDownThr = true;
    bool wantUpThr = true;
    bool wantClipThr = true;
    std::array<bool, DynParams::kMaxBands> wantBandDown {
        true, true, true, true, true, true
    };
    std::array<bool, DynParams::kMaxBands> wantBandUp {
        true, true, true, true, true, true
    };
    std::array<bool, DynParams::kMaxBands> wantBandClip {
        true, true, true, true, true, true
    };

    DynSplitLearn (juce::AudioProcessorValueTreeState& s, Analyser& a, DynCompressor& e)
        : state (s), analyser (a), engine (e) {}

    ~DynSplitLearn() override { stopTimer(); }

    bool isLearning() const noexcept { return learning; }
    bool canStart() const noexcept
    {
        const bool splits = wantSplits && bandCount() >= 2 && ! analyser.isEcoMode()
                            && (int) analyser.getScopeSize() >= 16;
        return splits || wantDownThr || wantUpThr || wantClipThr;
    }

    bool canStartBand (int band) const noexcept
    {
        band = juce::jlimit (0, DynParams::kMaxBands - 1, band);
        return wantBandDown[(size_t) band] || wantBandUp[(size_t) band]
            || wantBandClip[(size_t) band];
    }

    int activeBand() const noexcept { return soloBand; }

    std::function<void()> onFinished;

    bool start (int bandOrAll = -1)
    {
        if (learning)
            return false;

        soloBand = bandOrAll;
        const int n = bandCount();
        if (bandOrAll >= 0)
        {
            const int b = juce::jlimit (0, DynParams::kMaxBands - 1, bandOrAll);
            soloBand = b;
            doSplitsNow = false;
            doDownNow = wantBandDown[(size_t) b];
            doUpNow = wantBandUp[(size_t) b];
            doClipNow = wantBandClip[(size_t) b];
        }
        else
        {
            doSplitsNow = wantSplits && n >= 2 && ! analyser.isEcoMode()
                          && (int) analyser.getScopeSize() >= 16;
            doDownNow = wantDownThr;
            doUpNow = wantUpThr;
            doClipNow = wantClipThr;
        }
        if (! doSplitsNow && ! doDownNow && ! doUpNow && ! doClipNow)
            return false;

        frames = 0;
        haveAudio = false;
        haveEnv = false;

        if (doSplitsNow)
        {
            const int bins = (int) analyser.getScopeSize();
            sumLin.assign ((size_t) juce::jmax (16, bins), 0.0);
            binHz.resize (sumLin.size());
            for (int i = 0; i < (int) binHz.size(); ++i)
                binHz[(size_t) i] = analyser.getBinFrequencyHz ((size_t) i);
            current.assign ((size_t) (n - 1), 0.0f);
            for (int i = 0; i < n - 1; ++i)
            {
                float hz = 1000.0f;
                if (auto* p = state.getRawParameterValue (DynParams::splitId (i)))
                    hz = p->load();
                current[(size_t) i] = juce::jlimit (kMinHz, kMaxHz, hz);
            }
        }

        for (int b = 0; b < DynParams::kMaxBands; ++b)
        {
            loudDb[(size_t) b] = -80.0f;
            quietDb[(size_t) b] = -80.0f;
            clipPeak[(size_t) b] = -80.0f;
            envSeen[(size_t) b] = false;
            if (auto* p = state.getRawParameterValue (DynParams::thresholdId (b)))
                curDown[(size_t) b] = p->load();
            else
                curDown[(size_t) b] = -18.0f;
            if (auto* p = state.getRawParameterValue (DynParams::upThresholdId (b)))
                curUp[(size_t) b] = p->load();
            else
                curUp[(size_t) b] = -36.0f;
            if (auto* p = state.getRawParameterValue (DynParams::clipThrId (b)))
                curClip[(size_t) b] = p->load();
            else
                curClip[(size_t) b] = 0.0f;
        }

        learning = true;
        t0 = juce::Time::getMillisecondCounterHiRes();
        startTimerHz (20);
        return true;
    }

    void cancel()
    {
        if (! learning)
            return;
        stopTimer();
        learning = false;
        if (onFinished)
            onFinished();
    }

private:
    static constexpr float kMinHz = 40.0f;
    static constexpr float kMaxHz = 16000.0f;
    static constexpr float kCaptureSec = 5.0f;
    static constexpr float kMinOct = 0.55f;
    static constexpr int kLogN = 96;

    struct Valley
    {
        float hz = 1000.0f;
        float score = 0.0f;
    };

    int bandCount() const
    {
        if (auto* p = state.getRawParameterValue (DynParams::countId()))
            return juce::jlimit (1, DynParams::kMaxBands, (int) std::lround (p->load()));
        return 1;
    }

    void timerCallback() override
    {
        if (! learning)
        {
            stopTimer();
            return;
        }

        accumulate();
        const double elapsed = (juce::Time::getMillisecondCounterHiRes() - t0) * 0.001;
        accumulateEnv();
        const float alpha = elapsed < 1.2 ? 0.38f
                          : (elapsed < 3.5f ? 0.22f : 0.14f);
        if (doSplitsNow && haveAudio && frames >= 4)
            huntSplits (alpha);
        if ((doDownNow || doUpNow || doClipNow) && haveEnv)
            huntThresholds (alpha);

        if (elapsed >= (double) kCaptureSec)
        {
            if (doSplitsNow && haveAudio && frames >= 4)
                huntSplits (1.0f);
            if ((doDownNow || doUpNow || doClipNow) && haveEnv)
                huntThresholds (1.0f);
            stopTimer();
            learning = false;
            if (onFinished)
                onFinished();
        }
    }

    void accumulate()
    {
        const int n = (int) sumLin.size();
        if (n < 8)
            return;
        std::vector<float> db ((size_t) n);
        const int got = analyser.copyScopeDataDb (false, db.data(), n);
        const float peak = analyser.getScopePeakDb (false);
        if (got < 8 || peak < -70.0f)
            return;

        const int use = juce::jmin (got, n);
        for (int i = 0; i < use; ++i)
        {
            const float lin = juce::Decibels::decibelsToGain (
                juce::jlimit (-120.0f, 12.0f, db[(size_t) i]), -120.0f);
            sumLin[(size_t) i] += (double) lin;
        }
        ++frames;
        haveAudio = true;
    }

    float avgDbAtHz (float hz) const
    {
        if (frames < 1 || binHz.empty())
            return -80.0f;
        const int n = (int) binHz.size();
        if (hz <= binHz.front())
            return juce::Decibels::gainToDecibels ((float) (sumLin.front() / (double) frames), -120.0f);
        if (hz >= binHz.back())
            return juce::Decibels::gainToDecibels ((float) (sumLin.back() / (double) frames), -120.0f);

        int lo = 0;
        for (int i = 1; i < n; ++i)
        {
            if (binHz[(size_t) i] >= hz)
            {
                lo = i - 1;
                break;
            }
        }
        const int hi = juce::jmin (n - 1, lo + 1);
        const float a = binHz[(size_t) lo];
        const float b = juce::jmax (a + 0.01f, binHz[(size_t) hi]);
        const float t = juce::jlimit (0.0f, 1.0f, (hz - a) / (b - a));
        const float linA = (float) (sumLin[(size_t) lo] / (double) frames);
        const float linB = (float) (sumLin[(size_t) hi] / (double) frames);
        return juce::Decibels::gainToDecibels (linA + (linB - linA) * t, -120.0f);
    }

    void buildLogSpectrum (std::vector<float>& hz, std::vector<float>& db) const
    {
        hz.resize ((size_t) kLogN);
        db.resize ((size_t) kLogN);
        const float log0 = std::log (kMinHz);
        const float log1 = std::log (kMaxHz);
        for (int i = 0; i < kLogN; ++i)
        {
            const float t = (float) i / (float) (kLogN - 1);
            hz[(size_t) i] = std::exp (log0 + t * (log1 - log0));
            db[(size_t) i] = avgDbAtHz (hz[(size_t) i]);
        }

        std::vector<float> sm = db;
        constexpr int w = 2;
        for (int i = 0; i < kLogN; ++i)
        {
            float s = 0.0f;
            int c = 0;
            for (int k = i - w; k <= i + w; ++k)
            {
                if (k >= 0 && k < kLogN)
                {
                    s += db[(size_t) k];
                    ++c;
                }
            }
            sm[(size_t) i] = s / (float) juce::jmax (1, c);
        }
        db.swap (sm);
    }

    void energyQuantiles (const std::vector<float>& hz, const std::vector<float>& db,
                          int need, std::vector<float>& out) const
    {
        out.clear();
        if (need <= 0)
            return;
        std::vector<float> cume ((size_t) kLogN, 0.0f);
        float acc = 0.0f;
        for (int i = 0; i < kLogN; ++i)
        {
            acc += juce::Decibels::decibelsToGain (db[(size_t) i], -120.0f);
            cume[(size_t) i] = acc;
        }
        if (acc < 1.0e-12f)
            return;
        for (int k = 1; k <= need; ++k)
        {
            const float target = acc * ((float) k / (float) (need + 1));
            int idx = 0;
            while (idx < kLogN - 1 && cume[(size_t) idx] < target)
                ++idx;
            out.push_back (hz[(size_t) idx]);
        }
    }

    void findValleys (const std::vector<float>& hz, const std::vector<float>& db,
                      const std::vector<float>& quant, std::vector<Valley>& out) const
    {
        out.clear();
        for (int i = 3; i < kLogN - 3; ++i)
        {
            const float v = db[(size_t) i];
            if (v >= db[(size_t) i - 1] || v >= db[(size_t) i + 1]
                || v >= db[(size_t) i - 2] || v >= db[(size_t) i + 2])
                continue;

            int L = i;
            while (L > 1 && db[(size_t) L - 1] >= db[(size_t) L])
                --L;
            int R = i;
            while (R < kLogN - 2 && db[(size_t) R + 1] >= db[(size_t) R])
                ++R;
            const float depth = juce::jmin (db[(size_t) L], db[(size_t) R]) - v;
            if (depth < 1.2f)
                continue;

            const float f = hz[(size_t) i];
            float near = 8.0f;
            for (float q : quant)
                near = juce::jmin (near, std::abs (std::log2 (juce::jmax (20.0f, f) / juce::jmax (20.0f, q))));
            const float bonus = 2.4f * std::exp (-near * near / 0.28f);
            out.push_back ({ f, depth + bonus });
        }
    }

    void pickTargets (int need, std::vector<float>& dest) const
    {
        dest.clear();
        if (need <= 0)
            return;

        std::vector<float> hz, db, quant;
        buildLogSpectrum (hz, db);
        energyQuantiles (hz, db, need, quant);

        std::vector<Valley> valleys;
        findValleys (hz, db, quant, valleys);
        std::sort (valleys.begin(), valleys.end(),
                   [] (const Valley& a, const Valley& b) { return a.score > b.score; });

        auto farEnough = [&dest] (float f)
        {
            for (float p : dest)
            {
                const float oct = std::abs (std::log2 (juce::jmax (20.0f, f) / juce::jmax (20.0f, p)));
                if (oct < kMinOct)
                    return false;
            }
            return true;
        };

        for (const auto& v : valleys)
        {
            if ((int) dest.size() >= need)
                break;
            if (farEnough (v.hz))
                dest.push_back (v.hz);
        }
        for (float q : quant)
        {
            if ((int) dest.size() >= need)
                break;
            if (farEnough (q))
                dest.push_back (q);
        }

        std::sort (dest.begin(), dest.end());
        while ((int) dest.size() > need)
            dest.pop_back();
        while ((int) dest.size() < need)
        {
            const float t = ((float) dest.size() + 1.0f) / ((float) need + 1.0f);
            dest.push_back (kMinHz * std::pow (kMaxHz / kMinHz, t));
            std::sort (dest.begin(), dest.end());
        }

        for (int i = 0; i < (int) dest.size(); ++i)
        {
            dest[(size_t) i] = juce::jlimit (kMinHz, kMaxHz, dest[(size_t) i]);
            if (i > 0)
                dest[(size_t) i] = juce::jmax (dest[(size_t) i], dest[(size_t) i - 1] * 1.22f);
        }
    }

    void accumulateEnv()
    {
        const int n = bandCount();
        bool any = false;
        for (int b = 0; b < n; ++b)
        {
            if (soloBand >= 0 && b != soloBand)
                continue;
            const float env = engine.getInputEnvelopeDb (b);
            if (env < -75.0f)
                continue;
            auto& loud = loudDb[(size_t) b];
            auto& quiet = quietDb[(size_t) b];
            if (! envSeen[(size_t) b])
            {
                loud = env;
                quiet = env;
                clipPeak[(size_t) b] = env;
                envSeen[(size_t) b] = true;
            }
            else
            {
                if (env > loud) loud = env;
                else            loud += 0.06f * (env - loud);
                if (env < quiet) quiet = env;
                else             quiet += 0.04f * (env - quiet);
                if (env > clipPeak[(size_t) b])
                    clipPeak[(size_t) b] = env;
            }
            any = true;
        }
        if (any)
            haveEnv = true;
    }

    float tickleDistDb (int band, bool forUp) const
    {
        float ratio = 4.0f;
        if (auto* p = state.getRawParameterValue (DynParams::ratioId (band)))
            ratio = p->load();
        const float amt = [&]()
        {
            float a = 100.0f;
            if (auto* p = state.getRawParameterValue (DynParams::amountId()))
                a = p->load();
            return a;
        }();
        const float dir = [&]()
        {
            float a = 100.0f;
            if (auto* p = state.getRawParameterValue (forUp ? DynParams::upAmtId()
                                                           : DynParams::downAmtId()))
                a = p->load();
            return a;
        }();
        float r = DynParams::scaleRatio (ratio, amt);
        if (r < 0.999f)
            r = 1.0f / juce::jmax (0.01f, r);
        else if (r <= 1.001f)
            r = 4.0f;
        r = DynParams::scaleRatio (r, dir);
        r = juce::jmax (1.2f, r);
        constexpr float kWantGr = 4.0f;
        if (r >= 99.0f)
            return kWantGr;
        return kWantGr / juce::jmax (0.18f, 1.0f - 1.0f / r);
    }

    void huntThresholds (float alpha)
    {
        const int n = bandCount();
        alpha = juce::jlimit (0.0f, 1.0f, alpha);
        for (int b = 0; b < n; ++b)
        {
            if (soloBand >= 0 && b != soloBand)
                continue;
            if (! envSeen[(size_t) b])
                continue;
            const float loud = loudDb[(size_t) b];
            const float quiet = quietDb[(size_t) b];
            if (doDownNow)
            {
                const float tgt = juce::jlimit (-58.0f, -1.0f, loud - tickleDistDb (b, false));
                curDown[(size_t) b] += (tgt - curDown[(size_t) b]) * alpha;
            }
            if (doUpNow)
            {
                const float tgt = juce::jlimit (-59.0f, -2.0f, quiet + tickleDistDb (b, true));
                curUp[(size_t) b] += (tgt - curUp[(size_t) b]) * alpha;
            }
            if (doClipNow)
            {
                // 1-2 dB of actual clip. Peak-hold only (do not follow quiet),
                // then closed-loop on the clip meter so RMS detect cannot bury it.
                constexpr float kWant = 1.5f;
                float peak = clipPeak[(size_t) b];
                if (auto* det = state.getRawParameterValue (DynParams::detectId()))
                    if ((int) std::lround (det->load()) == DynParams::detectRms)
                        peak = juce::jmin (0.0f, peak + 8.0f);
                const float floorDb = juce::jmax (-36.0f, peak - 3.0f);
                const float have = engine.getClipDb (b);
                float tgt = curClip[(size_t) b];
                if (have < 0.25f)
                    tgt = juce::jmax (curClip[(size_t) b] - 1.5f, peak - kWant);
                else if (have > 2.2f)
                    tgt = curClip[(size_t) b] + juce::jmin (3.0f, have - kWant);
                else
                    tgt = curClip[(size_t) b] + (kWant - have) * 0.55f;
                tgt = juce::jlimit (floorDb, 0.0f, tgt);
                curClip[(size_t) b] += (tgt - curClip[(size_t) b]) * alpha;
            }
            if (doDownNow)
                DynParams::writeDownThr (state, b, curDown[(size_t) b]);
            if (doUpNow)
                DynParams::writeUpThr (state, b, curUp[(size_t) b]);
            if (doClipNow)
                DynParams::writeClipThr (state, b, curClip[(size_t) b]);
        }
    }

    void huntSplits (float alpha)
    {
        const int n = bandCount();
        const int need = n - 1;
        if (need < 1)
            return;

        if ((int) current.size() != need)
        {
            current.resize ((size_t) need);
            for (int i = 0; i < need; ++i)
            {
                float hz = 1000.0f;
                if (auto* p = state.getRawParameterValue (DynParams::splitId (i)))
                    hz = p->load();
                current[(size_t) i] = hz;
            }
        }

        std::vector<float> target;
        pickTargets (need, target);
        if ((int) target.size() != need)
            return;

        for (int i = 0; i < need; ++i)
        {
            const float a = std::log (juce::jmax (20.0f, current[(size_t) i]));
            const float b = std::log (juce::jmax (20.0f, target[(size_t) i]));
            current[(size_t) i] = std::exp (a + (b - a) * juce::jlimit (0.0f, 1.0f, alpha));
        }
        for (int i = 1; i < need; ++i)
            current[(size_t) i] = juce::jmax (current[(size_t) i], current[(size_t) i - 1] * 1.22f);

        for (int i = 0; i < need; ++i)
        {
            if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (
                    state.getParameter (DynParams::splitId (i))))
            {
                const float hz = DynParams::splitRange().snapToLegalValue (current[(size_t) i]);
                if (std::abs (p->get() - hz) > 0.5f)
                    *p = hz;
            }
        }
    }

    juce::AudioProcessorValueTreeState& state;
    Analyser& analyser;
    DynCompressor& engine;
    bool learning = false;
    bool haveAudio = false;
    bool haveEnv = false;
    bool doSplitsNow = false;
    bool doDownNow = false;
    bool doUpNow = false;
    bool doClipNow = false;
    int soloBand = -1;
    int frames = 0;
    double t0 = 0.0;
    std::vector<double> sumLin;
    std::vector<float> binHz;
    std::vector<float> current;
    std::array<float, DynParams::kMaxBands> loudDb {};
    std::array<float, DynParams::kMaxBands> quietDb {};
    std::array<float, DynParams::kMaxBands> curDown {};
    std::array<float, DynParams::kMaxBands> curUp {};
    std::array<float, DynParams::kMaxBands> curClip {};
    std::array<float, DynParams::kMaxBands> clipPeak {};
    std::array<bool, DynParams::kMaxBands> envSeen {};
};
