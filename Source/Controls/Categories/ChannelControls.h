#pragma once

#include <JuceHeader.h>

// ****************************************************************************
// CHANNEL CONTROLS CLASS
// ****************************************************************************
class ChannelControls : public juce::Component, public juce::Button::Listener
{
public:
    enum RadioButtonIds { channels = 1001 };


    // ========================================================================
    ChannelControls(juce::AudioProcessorValueTreeState&);
    ~ChannelControls() override;

    // Overrides from juce::Button::Listener
    void buttonClicked(juce::Button* button) override;


    // ========================================================================
    void resized() override;

private:
    // ========================================================================
    juce::AudioProcessorValueTreeState& mr_audioProcessorValueTreeState;
    juce::Colour customButtonColor;
    juce::Colour customButtonColorOn;
    juce::Colour customButtonColorOff;
    juce::Colour customTextColorOn;
    juce::Colour customTextColorOff;

    juce::TextButton m_leftChannelTextButton;
    juce::TextButton m_rightChannelTextButton;
    juce::TextButton m_bothChannelsTextButton;

    using Attachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    std::unique_ptr<Attachment> m_leftChannelTextButtonAttachment;
    std::unique_ptr<Attachment> m_rightChannelTextButtonAttachment;
    std::unique_ptr<Attachment> m_bothChannelsButtonAttachment;

    std::atomic<int> m_activeChannels;

    // Place this somewhere accessible in your file, or include the header file where it is defined.
    enum Channels {
        left,
        right,
        both
    };


    // ========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ChannelControls)
};
