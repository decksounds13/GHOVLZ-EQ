#pragma once

#include <JuceHeader.h>

/** Pro-Q-style processing / phase modes (V1: Minimum + Linear). */
namespace PhaseMode
{
    enum Choice : int
    {
        minimumPhase = 0, // classic IIR EQ, zero algorithmic latency
        linearPhase = 1,  // linear-phase FIR / FFT EQ, reports host latency
        // naturalPhase reserved — mild-phase / low-latency compromise
        numChoices
    };

    inline juce::StringArray getChoiceNames()
    {
        return { "Minimum Phase", "Linear Phase" };
    }

    inline const char* paramId() noexcept { return "phaseMode"; }

    inline int readChoiceIndex (juce::AudioProcessorValueTreeState& treeState, int fallback = minimumPhase)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (paramId())))
            return juce::jlimit (0, numChoices - 1, choice->getIndex());

        if (auto* raw = treeState.getRawParameterValue (paramId()))
            return juce::jlimit (0, numChoices - 1, (int) std::lround (raw->load()));

        return fallback;
    }
}
