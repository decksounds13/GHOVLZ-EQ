#pragma once

#include <JuceHeader.h>
#include <vector>

namespace BandChannel
{
    enum Mode : int
    {
        stereo = 0,
        mid,
        side,
        left,
        right,
        numModes
    };

    inline juce::StringArray getChoiceNames()
    {
        return { "Stereo", "Mid", "Side", "Left", "Right" };
    }

    inline juce::String paramIDForBandIndex (int bandIndex)
    {
        switch (bandIndex)
        {
            case 0: return "band1Channel";
            case 1: return "band2Channel";
            case 2: return "band3Channel";
            case 3: return "band4Channel";
            case 4: return "highpassChannel";
            case 5: return "lowpassChannel";
            case 6: return "highShelfChannel";
            case 7: return "lowShelfChannel";
            default: return {};
        }
    }

    /** Safely read an AudioParameterChoice as its integer index. */
    inline int readChoiceIndex (juce::AudioProcessorValueTreeState& treeState, const juce::String& paramID, int fallback = 0)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (paramID)))
            return choice->getIndex();

        if (auto* raw = treeState.getRawParameterValue (paramID))
            return (int) std::lround (raw->load());

        return fallback;
    }

    /**
        Run a stereo ProcessorDuplicator-friendly process under the selected channel mode.

        Left/Right/Mid/Side must not feed a 1-channel block into ProcessorDuplicator —
        that always uses processors[0]. Instead we process the full stereo block and
        restore the dry channel that should remain unprocessed.
    */
    template <typename ProcessFn>
    void process (juce::dsp::AudioBlock<float>& block, int mode, ProcessFn&& processFn)
    {
        if (block.getNumChannels() == 0 || block.getNumSamples() == 0)
            return;

        if (mode == stereo || block.getNumChannels() < 2)
        {
            processFn (block);
            return;
        }

        auto* leftData = block.getChannelPointer (0);
        auto* rightData = block.getChannelPointer (1);
        const int numSamples = (int) block.getNumSamples();

        thread_local std::vector<float> dry;
        dry.resize ((size_t) numSamples);

        auto restoreChannel = [&] (float* dest)
        {
            juce::FloatVectorOperations::copy (dest, dry.data(), numSamples);
        };

        if (mode == Mode::left)
        {
            juce::FloatVectorOperations::copy (dry.data(), rightData, numSamples);
            processFn (block);
            restoreChannel (rightData);
            return;
        }

        if (mode == Mode::right)
        {
            juce::FloatVectorOperations::copy (dry.data(), leftData, numSamples);
            processFn (block);
            restoreChannel (leftData);
            return;
        }

        // Encode L/R -> Mid/Side (mid in ch0, side in ch1).
        for (int i = 0; i < numSamples; ++i)
        {
            const float midSample = 0.5f * (leftData[i] + rightData[i]);
            const float sideSample = 0.5f * (leftData[i] - rightData[i]);
            leftData[i] = midSample;
            rightData[i] = sideSample;
        }

        if (mode == Mode::mid)
        {
            juce::FloatVectorOperations::copy (dry.data(), rightData, numSamples); // dry side
            processFn (block);
            restoreChannel (rightData);
        }
        else // Mode::side
        {
            juce::FloatVectorOperations::copy (dry.data(), leftData, numSamples); // dry mid
            processFn (block);
            restoreChannel (leftData);
        }

        // Decode Mid/Side -> L/R
        for (int i = 0; i < numSamples; ++i)
        {
            const float midSample = leftData[i];
            const float sideSample = rightData[i];
            leftData[i] = midSample + sideSample;
            rightData[i] = midSample - sideSample;
        }
    }
}
