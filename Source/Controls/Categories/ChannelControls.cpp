#include "ChannelControls.h"

ChannelControls::ChannelControls(
    juce::AudioProcessorValueTreeState& audioProcessorValueTreeState
) :
    mr_audioProcessorValueTreeState(audioProcessorValueTreeState)
{
    m_activeChannels.store(Channels::both);

    juce::Colour customButtonColor = juce::Colour::fromRGBA(32, 36, 22, 100);
    juce::Colour customButtonColorOn = juce::Colour::fromRGBA(36, 40, 26, 100);
    juce::Colour customButtonColorOff = juce::Colour::fromRGBA(28, 32, 18, 100);
    juce::Colour customTextColorOn = juce::Colours::whitesmoke.withAlpha(0.9f);
    juce::Colour customTextColorOff = juce::Colours::whitesmoke.withAlpha(0.6f);

    m_activeChannels.store(Channels::both);
    m_bothChannelsTextButton.setToggleState(true, juce::dontSendNotification);



    addAndMakeVisible(m_leftChannelTextButton);
    m_leftChannelTextButton.setButtonText("Left");
    m_leftChannelTextButton.setClickingTogglesState(true);
    m_leftChannelTextButton.setRadioGroupId(channels);
    m_leftChannelTextButton.setColour(juce::TextButton::textColourOnId, customTextColorOn);
    m_leftChannelTextButton.setColour(juce::TextButton::textColourOffId, customTextColorOff);
    m_leftChannelTextButton.setColour(juce::TextButton::buttonOnColourId, customButtonColorOn);
    m_leftChannelTextButton.setColour(juce::TextButton::buttonColourId, customButtonColorOff);



    m_leftChannelTextButtonAttachment =
        std::make_unique<Attachment>(
            mr_audioProcessorValueTreeState,
            "LEFT_ID",
            m_leftChannelTextButton
        );

    addAndMakeVisible(m_rightChannelTextButton);
    m_rightChannelTextButton.setButtonText("Right");
    m_rightChannelTextButton.setClickingTogglesState(true);
    m_rightChannelTextButton.setRadioGroupId(channels);
    m_rightChannelTextButton.setColour(juce::TextButton::textColourOnId, customTextColorOn);
    m_rightChannelTextButton.setColour(juce::TextButton::textColourOffId, customTextColorOff);
    m_rightChannelTextButton.setColour(juce::TextButton::buttonOnColourId, customButtonColorOn);
    m_rightChannelTextButton.setColour(juce::TextButton::buttonColourId, customButtonColorOff);

    m_rightChannelTextButtonAttachment =
        std::make_unique<Attachment>(
            mr_audioProcessorValueTreeState,
            "RIGHT_ID",
            m_rightChannelTextButton
        );

    addAndMakeVisible(m_bothChannelsTextButton);
    m_bothChannelsTextButton.setButtonText("Both");
    m_bothChannelsTextButton.setClickingTogglesState(true);
    m_bothChannelsTextButton.setRadioGroupId(channels);
    m_bothChannelsTextButton.setColour(juce::TextButton::textColourOnId, customTextColorOn);
    m_bothChannelsTextButton.setColour(juce::TextButton::textColourOffId, customTextColorOff);
    m_bothChannelsTextButton.setColour(juce::TextButton::buttonOnColourId, customButtonColorOn);
    m_bothChannelsTextButton.setColour(juce::TextButton::buttonColourId, customButtonColorOff);

    m_bothChannelsButtonAttachment =
        std::make_unique<Attachment>(
            mr_audioProcessorValueTreeState,
            "BOTH_ID",
            m_bothChannelsTextButton
        );

    m_leftChannelTextButton.addListener(this);
    m_rightChannelTextButton.addListener(this);
    m_bothChannelsTextButton.addListener(this);
}

enum Channels {
    left,
    right,
    both
};

// Listener callback to handle button clicks
void ChannelControls::buttonClicked(juce::Button* button)
{
    if (button == &m_leftChannelTextButton)
    {
        m_activeChannels.store(Channels::left);
        m_rightChannelTextButton.setToggleState(false, juce::dontSendNotification);
        m_bothChannelsTextButton.setToggleState(false, juce::dontSendNotification);
    }
    else if (button == &m_rightChannelTextButton)
    {
        m_activeChannels.store(Channels::right);
        m_leftChannelTextButton.setToggleState(false, juce::dontSendNotification);
        m_bothChannelsTextButton.setToggleState(false, juce::dontSendNotification);
    }
    else if (button == &m_bothChannelsTextButton)
    {
        m_activeChannels.store(Channels::both);
        m_leftChannelTextButton.setToggleState(false, juce::dontSendNotification);
        m_rightChannelTextButton.setToggleState(false, juce::dontSendNotification);
    }


}



ChannelControls::~ChannelControls() {}


// ============================================================================
void ChannelControls::resized()
{
    auto indent{ 3 };
    auto area{ getLocalBounds() };
    auto buttonWidth{ (area.getWidth() - indent * 2) / 3 };

    m_leftChannelTextButton.setBounds(area.removeFromLeft(buttonWidth));
    area.removeFromLeft(indent);

    m_bothChannelsTextButton.setBounds(area.removeFromRight(buttonWidth));
    area.removeFromRight(indent);

    m_rightChannelTextButton.setBounds(area);
}
