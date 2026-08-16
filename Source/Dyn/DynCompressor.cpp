#include "DynCompressor.h"
#include "../PhaseMode.h"
#include <cmath>

namespace
{
    void designLinPhaseLowpass (float* h, int n, float fcHz, double fs)
    {
        const int mid = n / 2;
        const float wc = (float) (2.0 * juce::MathConstants<double>::pi
                                  * (double) juce::jlimit (10.0f, (float) fs * 0.45f, fcHz) / fs);
        double sum = 0.0;
        for (int i = 0; i < n; ++i)
        {
            const int k = i - mid;
            const float sinc = (k == 0) ? (wc / juce::MathConstants<float>::pi)
                                        : std::sin (wc * (float) k) / (juce::MathConstants<float>::pi * (float) k);
            const float w = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi
                                                    * (float) i / (float) (n - 1));
            h[i] = sinc * w;
            sum += (double) h[i];
        }
        if (std::abs (sum) > 1.0e-12)
        {
            const float inv = (float) (1.0 / sum);
            for (int i = 0; i < n; ++i)
                h[i] *= inv;
        }
    }

    float rmsDbOf (const juce::AudioBuffer<float>& buf, int chans, int n) noexcept
    {
        double acc = 0.0;
        const int useCh = juce::jmax (1, chans);
        for (int c = 0; c < useCh; ++c)
        {
            const auto* s = buf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                acc += (double) s[i] * (double) s[i];
        }
        const float rms = (float) std::sqrt (acc / (double) juce::jmax (1, n * useCh));
        return juce::Decibels::gainToDecibels (rms, -80.0f);
    }
}

void DynCompressor::prepare (double sampleRate, int samplesPerBlock)
{
    sr = sampleRate > 0.0 ? sampleRate : 44100.0;
    maxBlock = juce::jmax (1, samplesPerBlock);
    processSr = sr;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlock, 2 };
    dry.setSize (2, maxBlock * 16, false, false, true);
    delayedIn.setSize (2, maxBlock * 16, false, false, true);
    osWork.setSize (2, maxBlock * 16, false, false, true);
    for (auto& b : bands)
    {
        b.lp.prepare (spec);
        b.hp.prepare (spec);
        b.lp.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
        b.hp.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        b.work.setSize (2, maxBlock * 16, false, false, true);
        b.envDb = -140.0f;
        b.rmsLin = 0.0f;
        b.autoMakeupDb = 0.0f;
        b.lookWrite = 0;
    }
    for (int s = 0; s < DynParams::kMaxBands; ++s)
    {
        lastSplitHz[(size_t) s] = -1.0f;
        lpWork[s].setSize (2, maxBlock * 16, false, false, true);
    }
    preparedOsLog2 = -1;
    preparedLinPhase = false;
    oversampling.reset();
    globalAutoMakeupDb = 0.0f;
    reset();
}

void DynCompressor::reset() noexcept
{
    for (auto& b : bands)
    {
        b.lp.reset();
        b.hp.reset();
        b.envDb = -140.0f;
        b.rmsLin = 0.0f;
        b.autoMakeupDb = 0.0f;
        b.lookWrite = 0;
        for (auto& v : b.lookBuf)
            std::fill (v.begin(), v.end(), 0.0f);
    }
    globalAutoMakeupDb = 0.0f;
    dryLookWrite = 0;
    for (auto& v : dryLook)
        std::fill (v.begin(), v.end(), 0.0f);
    for (auto& g : grDb)
        g.store (0.0f, std::memory_order_relaxed);
    for (auto& u : upGrDb)
        u.store (0.0f, std::memory_order_relaxed);
    for (auto& v : inDb)
        v.store (-140.0f, std::memory_order_relaxed);
    for (auto& c : clipDb)
        c.store (0.0f, std::memory_order_relaxed);
    for (auto& ch : inDelay)
        std::fill (ch.begin(), ch.end(), 0.0f);
    inDelayWrite = 0;
    if (oversampling != nullptr)
        oversampling->reset();
    for (auto& pair : firLp)
        for (auto& f : pair)
            f.reset();
}

int DynCompressor::getBandCount (juce::AudioProcessorValueTreeState& state) const
{
    if (auto* p = state.getRawParameterValue (DynParams::countId()))
        return juce::jlimit (1, DynParams::kMaxBands, (int) std::lround (p->load()));
    return 1;
}

void DynCompressor::ensureRatePath (int osLog2, bool linearPhase, int maxN, int chans)
{
    osLog2 = juce::jlimit (0, 4, osLog2);
    if (osLog2 == preparedOsLog2 && linearPhase == preparedLinPhase)
    {
        int lat = 0;
        if (oversampling != nullptr)
            lat += (int) std::ceil (oversampling->getLatencyInSamples());
        if (linearPhase)
            lat += (osLog2 > 0) ? (kFirDelay >> osLog2) : kFirDelay;
        reportedLatency = juce::jmax (0, lat);
        return;
    }

    if (osLog2 != preparedOsLog2)
    {
        oversampling.reset();
        if (osLog2 > 0)
        {
            oversampling = std::make_unique<juce::dsp::Oversampling<float>> (
                (size_t) juce::jmax (1, chans),
                (size_t) osLog2,
                juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
                true, true);
            oversampling->initProcessing ((size_t) juce::jmax (1, maxBlock));
            oversampling->reset();
        }
        preparedOsLog2 = osLog2;
        lastSplitHz.fill (-1.0f);
    }

    processSr = sr * (double) (1 << juce::jmax (0, osLog2));
    const int need = juce::jmax (maxN * (1 << osLog2), maxBlock);
    dry.setSize (2, need, false, false, true);
    delayedIn.setSize (2, need, false, false, true);
    {
        juce::dsp::ProcessSpec iirSpec { processSr, (juce::uint32) juce::jmax (1, need), 2 };
        for (auto& b : bands)
        {
            b.work.setSize (2, need, false, false, true);
            b.lp.prepare (iirSpec);
            b.hp.prepare (iirSpec);
            b.lp.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
            b.hp.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
        }
    }
    for (int s = 0; s < DynParams::kMaxBands; ++s)
        lpWork[s].setSize (2, need, false, false, true);

    const int maxLook = juce::jmax (8, (int) std::ceil (0.022 * processSr) + 4);
    for (auto& b : bands)
    {
        for (auto& v : b.lookBuf)
            v.assign ((size_t) maxLook, 0.0f);
        b.lookWrite = 0;
        b.rmsLin = 0.0f;
    }
    for (auto& v : dryLook)
        v.assign ((size_t) maxLook, 0.0f);
    dryLookWrite = 0;

    if (linearPhase)
    {
        juce::dsp::ProcessSpec firSpec { processSr, (juce::uint32) juce::jmax (1, need), 1 };
        for (auto& pair : firLp)
            for (auto& f : pair)
                f.prepare (firSpec);
        for (int c = 0; c < 2; ++c)
        {
            inDelay[(size_t) c].assign ((size_t) kFirDelay, 0.0f);
        }
        inDelayWrite = 0;
    }

    preparedLinPhase = linearPhase;

    int lat = 0;
    if (oversampling != nullptr)
        lat += (int) std::ceil (oversampling->getLatencyInSamples());
    if (linearPhase)
        lat += (osLog2 > 0) ? (kFirDelay >> osLog2) : kFirDelay;
    reportedLatency = juce::jmax (0, lat);
}

void DynCompressor::rebuildFirCoeffs (double fs, const float* splits, int numSplits)
{
    float taps[kFirTaps];
    juce::dsp::ProcessSpec firSpec { fs, (juce::uint32) juce::jmax (1, maxBlock * 16), 1 };
    for (int s = 0; s < numSplits; ++s)
    {
        if (std::abs (splits[s] - lastSplitHz[(size_t) s]) < 0.25f && lastSplitHz[(size_t) s] > 0.0f)
            continue;
        lastSplitHz[(size_t) s] = splits[s];
        designLinPhaseLowpass (taps, kFirTaps, splits[s], fs);
        auto coeffs = juce::dsp::FIR::Coefficients<float>::Ptr (
            new juce::dsp::FIR::Coefficients<float> (taps, (size_t) kFirTaps));
        for (int c = 0; c < 2; ++c)
        {
            firLp[(size_t) s][(size_t) c].prepare (firSpec);
            firLp[(size_t) s][(size_t) c].coefficients = coeffs;
            firLp[(size_t) s][(size_t) c].reset();
        }
    }
    for (int s = numSplits; s < DynParams::kMaxBands; ++s)
        lastSplitHz[(size_t) s] = -1.0f;
}

void DynCompressor::splitMinPhase (int count, int n, int chans, juce::AudioProcessorValueTreeState& state)
{
    auto readF = [&state] (const juce::String& id, float fb) -> float
    {
        if (auto* p = state.getRawParameterValue (id))
            return p->load();
        return fb;
    };

    const float nyq = (float) processSr * 0.45f;
    for (int b = 0; b < count; ++b)
    {
        auto& band = bands[(size_t) b];
        const float loHz = (b == 0 ? 20.0f : readF (DynParams::splitId (b - 1), 120.0f));
        const float hiHz = (b == count - 1 ? 20000.0f : readF (DynParams::splitId (b), 20000.0f));
        const float lo = juce::jlimit (20.0f, nyq, loHz);
        const float hi = juce::jlimit (lo + 10.0f, nyq, hiHz);
        if (count > 1)
        {
            if (b > 0)
            {
                band.hp.setCutoffFrequency (lo);
                juce::dsp::AudioBlock<float> block (band.work);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                band.hp.process (ctx);
            }
            if (b < count - 1)
            {
                band.lp.setCutoffFrequency (hi);
                juce::dsp::AudioBlock<float> block (band.work);
                juce::dsp::ProcessContextReplacing<float> ctx (block);
                band.lp.process (ctx);
            }
        }
    }
}

void DynCompressor::splitLinearPhase (int count, int n, int chans, const float* splits)
{
    const int numSplits = juce::jmax (0, count - 1);
    if (numSplits <= 0)
        return;

    rebuildFirCoeffs (processSr, splits, numSplits);

    const int w0 = inDelayWrite;
    int wEnd = w0;
    for (int c = 0; c < chans; ++c)
    {
        auto* in = dry.getWritePointer (c);
        auto* del = delayedIn.getWritePointer (c);
        auto& ring = inDelay[(size_t) c];
        if ((int) ring.size() != kFirDelay)
            ring.assign ((size_t) kFirDelay, 0.0f);
        int w = w0;
        for (int i = 0; i < n; ++i)
        {
            const float x = in[i];
            del[i] = ring[(size_t) w];
            ring[(size_t) w] = x;
            w = (w + 1) % kFirDelay;
        }
        wEnd = w;
    }
    inDelayWrite = wEnd;

    for (int s = 0; s < numSplits; ++s)
    {
        lpWork[s].makeCopyOf (dry, true);
        for (int c = 0; c < chans; ++c)
        {
            auto* d = lpWork[s].getWritePointer (c);
            auto& fir = firLp[(size_t) s][(size_t) c];
            for (int i = 0; i < n; ++i)
                d[i] = fir.processSample (d[i]);
        }
    }

    for (int b = 0; b < count; ++b)
    {
        auto& band = bands[(size_t) b];
        if (b == 0)
        {
            band.work.makeCopyOf (lpWork[0], true);
        }
        else if (b == count - 1)
        {
            band.work.makeCopyOf (delayedIn, true);
            for (int c = 0; c < chans; ++c)
                band.work.addFrom (c, 0, lpWork[numSplits - 1], c, 0, n, -1.0f);
        }
        else
        {
            band.work.makeCopyOf (lpWork[b], true);
            for (int c = 0; c < chans; ++c)
                band.work.addFrom (c, 0, lpWork[b - 1], c, 0, n, -1.0f);
        }
    }

    // Align dry to the FIR group delay so Mix does not comb.
    dry.makeCopyOf (delayedIn, true);
}

void DynCompressor::processDynamics (int count, int n, int chans, double procSr,
                                     juce::AudioProcessorValueTreeState& state,
                                     juce::AudioBuffer<float>& dryBuf,
                                     juce::AudioBuffer<float>& outBuf)
{
    auto readF = [&state] (const juce::String& id, float fb) -> float
    {
        if (auto* p = state.getRawParameterValue (id))
            return p->load();
        return fb;
    };
    auto readB = [&state] (const juce::String& id) -> bool
    {
        if (auto* p = state.getRawParameterValue (id))
            return p->load() > 0.5f;
        return false;
    };

    bool anySolo = false;
    for (int b = 0; b < count; ++b)
        if (auto* s = state.getRawParameterValue (DynParams::soloId (b)))
            if (s->load() > 0.5f)
                anySolo = true;

    outBuf.clear();
    float bandNeedDb[DynParams::kMaxBands] {};
    float bandMix01[DynParams::kMaxBands] {};
    float bandMakeup[DynParams::kMaxBands] {};

    const float lookMs = juce::jmax (0.0f, readF (DynParams::lookaheadId(), 0.0f));
    const int dryCap = dryLook[0].empty() ? 0 : (int) dryLook[0].size();
    const int lookS = (dryCap > 1)
        ? juce::jlimit (0, dryCap - 1, (int) std::lround (lookMs * 0.001 * procSr))
        : 0;

    for (int b = 0; b < count; ++b)
    {
        auto& band = bands[(size_t) b];
        const bool on = readB (DynParams::onId (b));
        const bool solo = readB (DynParams::soloId (b));
        if (! on || (anySolo && ! solo))
        {
            grDb[(size_t) b].store (0.0f, std::memory_order_relaxed);
            upGrDb[(size_t) b].store (0.0f, std::memory_order_relaxed);
            inDb[(size_t) b].store (-140.0f, std::memory_order_relaxed);
            clipDb[(size_t) b].store (0.0f, std::memory_order_relaxed);
            continue;
        }

        const float downThr = readF (DynParams::thresholdId (b), -18.0f);
        const float upThr   = readF (DynParams::upThresholdId (b), -36.0f);
        const float timePct = readF (DynParams::timeId(), 100.0f);
        const float amtPct  = readF (DynParams::amountId(), 100.0f);
        const float downAmt = readF (DynParams::downAmtId(), 100.0f);
        const float upAmt   = readF (DynParams::upAmtId(), 100.0f);
        const float ratio = DynParams::scaleRatio (readF (DynParams::ratioId (b), 4.0f), amtPct);
        const float atkMs = DynParams::scaleTimeMs (readF (DynParams::attackId (b), 12.0f), timePct);
        const float relMs = DynParams::scaleTimeMs (readF (DynParams::releaseId (b), 180.0f), timePct);
        const float knee = juce::jmax (0.0f, readF (DynParams::kneeId (b), 8.0f));
        const float makeup = readF (DynParams::makeupId (b), 0.0f);
        const float mix01 = readF (DynParams::mixId (b), 100.0f) * 0.01f;
        const float clipDrive = juce::jmax (0.0f, readF (DynParams::clipId (b), 0.0f));
        const float clipThr = juce::jlimit (-60.0f, 0.0f, readF (DynParams::clipThrId (b), 0.0f));
        const int clipMode = (int) std::lround (readF (DynParams::clipModeId (b), 0.0f));
        const bool clipOn = clipDrive > 0.001f || clipThr < -0.05f;
        const bool rmsMode = (int) std::lround (readF (DynParams::detectId(), 0.0f)) == DynParams::detectRms;

        const float atkCoeff = std::exp (-1.0f / (juce::jmax (0.001f, atkMs * 0.001f) * (float) procSr));
        const float relCoeff = std::exp (-1.0f / (juce::jmax (0.001f, relMs * 0.001f) * (float) procSr));
        const float rmsCoeff = std::exp (-1.0f / (0.005f * (float) procSr));
        const int lookCap = band.lookBuf[0].empty() ? 0 : (int) band.lookBuf[0].size();

        float peakDown = 0.0f;
        float peakUp = 0.0f;
        float peakClip = 0.0f;
        double preAcc = 0.0;
        double postAcc = 0.0;
        const bool useLook = lookS > 0 && lookCap > 1
            && (chans <= 1 || (int) band.lookBuf[1].size() >= lookCap);

        for (int i = 0; i < n; ++i)
        {
            if (clipOn)
            {
                for (int c = 0; c < chans; ++c)
                {
                    const float x = band.work.getSample (c, i);
                    const float driveG = clipDrive > 0.001f
                        ? juce::Decibels::decibelsToGain (clipDrive) : 1.0f;
                    const float driven = std::abs (x) * driveG;
                    const float y = DynParams::clipSample (x, clipDrive, clipMode, clipThr);
                    const float outA = std::abs (y);
                    const float drivenDb = juce::Decibels::gainToDecibels (driven, -80.0f);
                    const float outDb = juce::Decibels::gainToDecibels (outA, -80.0f);
                    const float shaved = juce::jmax (0.0f, drivenDb - outDb);
                    const float over = juce::jmax (0.0f, drivenDb - clipThr);
                    peakClip = juce::jmax (peakClip, juce::jmax (shaved, over));
                    band.work.setSample (c, i, y);
                }
            }

            float det = 0.0f;
            for (int c = 0; c < chans; ++c)
                det = juce::jmax (det, std::abs (band.work.getSample (c, i)));
            float inDb = juce::Decibels::gainToDecibels (det, -140.0f);
            if (rmsMode)
            {
                band.rmsLin = rmsCoeff * band.rmsLin + (1.0f - rmsCoeff) * det * det;
                inDb = juce::Decibels::gainToDecibels (std::sqrt (juce::jmax (1.0e-20f, band.rmsLin)), -140.0f);
            }

            if (inDb > band.envDb)
                band.envDb = atkCoeff * band.envDb + (1.0f - atkCoeff) * inDb;
            else
                band.envDb = relCoeff * band.envDb + (1.0f - relCoeff) * inDb;

            const float gd = DynParams::gainDbDual (band.envDb, downThr, upThr, ratio, knee,
                                                    downAmt, upAmt);
            const float g = juce::Decibels::decibelsToGain (gd);

            if (! useLook)
            {
                for (int c = 0; c < chans; ++c)
                {
                    const float xIn = band.work.getSample (c, i);
                    const float xOut = xIn * g;
                    preAcc += (double) xIn * (double) xIn;
                    postAcc += (double) xOut * (double) xOut;
                    band.work.setSample (c, i, xOut);
                }
            }
            else
            {
                int w = band.lookWrite;
                int r = w - lookS;
                if (r < 0)
                    r += lookCap;
                for (int c = 0; c < chans; ++c)
                {
                    band.lookBuf[(size_t) c][(size_t) w] = band.work.getSample (c, i);
                    const float delayed = band.lookBuf[(size_t) c][(size_t) r];
                    const float xOut = delayed * g;
                    preAcc += (double) delayed * (double) delayed;
                    postAcc += (double) xOut * (double) xOut;
                    band.work.setSample (c, i, xOut);
                }
                band.lookWrite = (w + 1) % lookCap;
            }

            if (gd < 0.0f)
                peakDown = juce::jmax (peakDown, -gd);
            else if (gd > 0.0f)
                peakUp = juce::jmax (peakUp, gd);
        }

        {
            const float denom = (float) juce::jmax (1, n * chans);
            const float preDb = juce::Decibels::gainToDecibels (
                (float) std::sqrt (preAcc / (double) denom), -80.0f);
            const float postDb = juce::Decibels::gainToDecibels (
                (float) std::sqrt (postAcc / (double) denom), -80.0f);
            bandNeedDb[(size_t) b] = (preDb > -60.0f)
                ? juce::jlimit (-24.0f, 24.0f, preDb - postDb)
                : 0.0f;
        }
        bandMix01[(size_t) b] = mix01;
        bandMakeup[(size_t) b] = makeup;
        grDb[(size_t) b].store (peakDown, std::memory_order_relaxed);
        upGrDb[(size_t) b].store (peakUp, std::memory_order_relaxed);
        inDb[(size_t) b].store (band.envDb, std::memory_order_relaxed);
        clipDb[(size_t) b].store (peakClip, std::memory_order_relaxed);
    }

    const bool autoOn = readB ("autoGain");
    const int autoMode = (int) std::lround (readF (DynParams::autoGainModeId(), 0.0f));
    const bool multiAuto = autoOn && autoMode == 0;
    const bool globalAuto = autoOn && autoMode != 0;

    if (lookS > 0 && dryCap > 1
        && (chans <= 1 || (int) dryLook[1].size() >= dryCap))
    {
        int w = dryLookWrite;
        for (int i = 0; i < n; ++i)
        {
            int r = w - lookS;
            if (r < 0)
                r += dryCap;
            for (int c = 0; c < chans; ++c)
            {
                dryLook[(size_t) c][(size_t) w] = dryBuf.getSample (c, i);
                dryBuf.setSample (c, i, dryLook[(size_t) c][(size_t) r]);
            }
            w = (w + 1) % dryCap;
        }
        dryLookWrite = w;
    }

    for (int b = 0; b < count; ++b)
    {
        auto& band = bands[(size_t) b];
        const bool on = readB (DynParams::onId (b));
        const bool solo = readB (DynParams::soloId (b));
        if (! on || (anySolo && ! solo))
        {
            band.autoMakeupDb = 0.0f;
            continue;
        }

        if (multiAuto)
            band.autoMakeupDb = 0.85f * band.autoMakeupDb + 0.15f * bandNeedDb[(size_t) b];
        else
            band.autoMakeupDb = 0.0f;

        const float outDb = bandMakeup[(size_t) b] + (multiAuto ? band.autoMakeupDb : 0.0f);
        const float outG = juce::Decibels::decibelsToGain (outDb);
        const float mix01 = bandMix01[(size_t) b];
        for (int c = 0; c < chans; ++c)
        {
            auto* dst = outBuf.getWritePointer (c);
            const auto* wet = band.work.getReadPointer (c);
            const auto* d = dryBuf.getReadPointer (c);
            for (int i = 0; i < n; ++i)
                dst[i] += d[i] * (1.0f - mix01) + wet[i] * outG * mix01;
        }
    }

    if (globalAuto)
    {
        const float dryDb = rmsDbOf (dryBuf, chans, n);
        const float wetDb = rmsDbOf (outBuf, chans, n);
        const float need = (dryDb > -60.0f)
            ? juce::jlimit (-24.0f, 24.0f, dryDb - wetDb)
            : 0.0f;
        globalAutoMakeupDb = 0.85f * globalAutoMakeupDb + 0.15f * need;
        const float g = juce::Decibels::decibelsToGain (globalAutoMakeupDb);
        for (int c = 0; c < chans; ++c)
            outBuf.applyGain (c, 0, n, g);
    }
    else
    {
        globalAutoMakeupDb = 0.0f;
    }

    for (int b = count; b < DynParams::kMaxBands; ++b)
    {
        grDb[(size_t) b].store (0.0f, std::memory_order_relaxed);
        upGrDb[(size_t) b].store (0.0f, std::memory_order_relaxed);
        inDb[(size_t) b].store (-140.0f, std::memory_order_relaxed);
        clipDb[(size_t) b].store (0.0f, std::memory_order_relaxed);
    }
}

void DynCompressor::process (juce::AudioBuffer<float>& buffer, juce::AudioProcessorValueTreeState& state,
                             bool isOfflineRender)
{
    const int n = buffer.getNumSamples();
    const int chans = juce::jmin (2, buffer.getNumChannels());
    if (n <= 0 || chans <= 0)
        return;

    auto readI = [&state] (const char* id, int fb) -> int
    {
        if (auto* p = state.getRawParameterValue (id))
            return (int) std::lround (p->load());
        return fb;
    };

    const int rtOs = juce::jlimit (0, 3, readI (DynParams::osRealtimeId(), 0));
    const int offOs = juce::jlimit (0, 4, readI (DynParams::osOfflineId(), 4));
    const int osLog2 = isOfflineRender ? offOs : rtOs;
    const bool linPhase = PhaseMode::readChoiceIndex (state) == PhaseMode::linearPhase;
    const int count = getBandCount (state);

    ensureRatePath (osLog2, linPhase, n, chans);

    auto runAtRate = [&] (juce::AudioBuffer<float>& buf, int samples, double rate)
    {
        if (dry.getNumSamples() < samples)
            dry.setSize (2, samples, false, false, true);
        for (int c = 0; c < chans; ++c)
            dry.copyFrom (c, 0, buf, c, 0, samples);
        for (int b = 0; b < count; ++b)
        {
            if (bands[(size_t) b].work.getNumSamples() < samples)
                bands[(size_t) b].work.setSize (2, samples, false, false, true);
            for (int c = 0; c < chans; ++c)
                bands[(size_t) b].work.copyFrom (c, 0, dry, c, 0, samples);
            for (int c = chans; c < 2 && c < bands[(size_t) b].work.getNumChannels(); ++c)
                bands[(size_t) b].work.clear (c, 0, samples);
        }

        if (count > 1 && linPhase)
        {
            float splits[DynParams::kMaxBands] {};
            for (int s = 0; s < count - 1; ++s)
            {
                if (auto* p = state.getRawParameterValue (DynParams::splitId (s)))
                    splits[s] = p->load();
                else
                    splits[s] = 1000.0f;
            }
            splitLinearPhase (count, samples, chans, splits);
        }
        else
        {
            splitMinPhase (count, samples, chans, state);
        }

        processDynamics (count, samples, chans, rate, state, dry, buf);
    };

    if (osLog2 > 0 && oversampling != nullptr)
    {
        juce::dsp::AudioBlock<float> inBlock (buffer);
        auto osBlock = oversampling->processSamplesUp (inBlock);
        const int osN = (int) osBlock.getNumSamples();
        osWork.setSize ((int) osBlock.getNumChannels(), osN, false, false, true);
        for (int c = 0; c < (int) osBlock.getNumChannels(); ++c)
            osWork.copyFrom (c, 0, osBlock.getChannelPointer ((size_t) c), osN);
        runAtRate (osWork, osN, processSr);
        for (int c = 0; c < (int) osBlock.getNumChannels(); ++c)
            juce::FloatVectorOperations::copy (osBlock.getChannelPointer ((size_t) c),
                                               osWork.getReadPointer (c), osN);
        oversampling->processSamplesDown (inBlock);
    }
    else
    {
        runAtRate (buffer, n, sr);
    }

    float lookMs = 0.0f;
    if (auto* p = state.getRawParameterValue (DynParams::lookaheadId()))
        lookMs = juce::jmax (0.0f, p->load());
    const int lookLat = (int) std::lround (lookMs * 0.001 * sr);
    int lat = 0;
    if (oversampling != nullptr)
        lat += (int) std::ceil (oversampling->getLatencyInSamples());
    if (linPhase)
        lat += (osLog2 > 0) ? (kFirDelay >> osLog2) : kFirDelay;
    reportedLatency = juce::jmax (0, lat + lookLat);
}
