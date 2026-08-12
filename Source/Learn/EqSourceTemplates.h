#pragma once

#include "EqLearnSettings.h"
#include <cmath>

/**
    Hand-authored source-class target shapes for Learn (mean-normalized dB vs log-f).
    Not ML — authored “healthy balance” curves the fitter aims toward.
*/
namespace EqLearn
{
    namespace SourceTemplates
    {
        /** Control-point shape: relative spectral balance (pre mean-norm). */
        inline void fillSourceTargetDb (SourceClass cls,
                                        const float* frequenciesHz,
                                        float* destDb,
                                        int numPoints) noexcept
        {
            if (frequenciesHz == nullptr || destDb == nullptr || numPoints <= 0)
                return;

            // Key bands (Hz) and relative levels (dB) per class — rough mix-engineer priors.
            // Values are relative; mean is removed after sampling.
            auto levelAt = [cls] (float f) noexcept -> float
            {
                f = juce::jmax (20.0f, f);
                const float oct = std::log2 (f / 1000.0f); // 0 at 1 kHz

                switch (cls)
                {
                    case SourceClass::bass:
                        // Strong sub/bass, gentle mid, soft air
                        if (f < 80.0f)  return 6.0f - 0.02f * (80.0f - f);
                        if (f < 250.0f) return 4.0f * (1.0f - (f - 80.0f) / 170.0f) + 1.5f;
                        if (f < 2000.0f) return 0.5f - 0.8f * ((f - 250.0f) / 1750.0f);
                        if (f < 8000.0f) return -1.5f - 1.0f * ((f - 2000.0f) / 6000.0f);
                        return -3.5f;

                    case SourceClass::vocals:
                        // Controlled sub, body 200–500, presence 2–5 k, gentle air
                        if (f < 100.0f)  return -4.0f;
                        if (f < 300.0f)  return -1.0f + 2.0f * ((f - 100.0f) / 200.0f);
                        if (f < 800.0f)  return 1.5f;
                        if (f < 2500.0f) return 2.0f + 1.0f * ((f - 800.0f) / 1700.0f);
                        if (f < 6000.0f) return 3.0f - 0.5f * ((f - 2500.0f) / 3500.0f);
                        if (f < 12000.0f) return 1.5f - 1.5f * ((f - 6000.0f) / 6000.0f);
                        return -1.0f;

                    case SourceClass::guitar:
                        // Mid-forward, modest sub, some bite 2–5 k
                        if (f < 80.0f)   return -3.5f;
                        if (f < 200.0f)  return -1.0f;
                        if (f < 800.0f)  return 1.5f + 1.0f * ((f - 200.0f) / 600.0f);
                        if (f < 3000.0f) return 2.5f;
                        if (f < 6000.0f) return 2.0f - 0.5f * ((f - 3000.0f) / 3000.0f);
                        return 0.0f - 1.5f * juce::jmin (1.0f, (f - 6000.0f) / 10000.0f);

                    case SourceClass::synth:
                        // Broader / flatter with optional air — higher flatness prior
                        if (f < 80.0f)   return -1.0f;
                        if (f < 400.0f)  return 0.5f;
                        if (f < 2000.0f) return 1.0f;
                        if (f < 8000.0f) return 1.5f;
                        return 2.0f - 0.5f * juce::jmin (1.0f, (f - 8000.0f) / 8000.0f);

                    case SourceClass::drums:
                        // Broadband punch: some sub, click mid-high, open air
                        if (f < 60.0f)   return 3.0f;
                        if (f < 150.0f)  return 2.0f;
                        if (f < 500.0f)  return 0.5f;
                        if (f < 2000.0f) return 0.0f;
                        if (f < 6000.0f) return 1.5f;
                        return 2.5f;

                    case SourceClass::mix:
                        // Mild pink-ish program balance (-3 dB/oct-ish)
                        return -3.0f * oct;

                    case SourceClass::unknown:
                    default:
                        return -3.0f * oct; // pink fallback
                }
            };

            for (int i = 0; i < numPoints; ++i)
                destDb[i] = levelAt (frequenciesHz[i]);

            // Mean-normalize like Match factory targets
            double sum = 0.0;
            for (int i = 0; i < numPoints; ++i)
                sum += (double) destDb[i];
            const float mean = (float) (sum / (double) numPoints);
            for (int i = 0; i < numPoints; ++i)
                destDb[i] -= mean;
        }

        inline void makeLogGrid (float* hzOut, int n) noexcept
        {
            if (hzOut == nullptr || n <= 0)
                return;
            const double logMin = std::log ((double) kMinFreqHz);
            const double logMax = std::log ((double) kMaxFreqHz);
            for (int i = 0; i < n; ++i)
            {
                const double t = (n == 1) ? 0.5 : (double) i / (double) (n - 1);
                hzOut[i] = (float) std::exp (logMin + t * (logMax - logMin));
            }
        }
    }
}
