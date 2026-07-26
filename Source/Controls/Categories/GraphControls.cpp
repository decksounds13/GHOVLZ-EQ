#include "GraphControls.h"

GraphControls::GraphControls(
    juce::AudioProcessorValueTreeState &audioProcessorValueTreeState
) :
    mr_audioProcessorValueTreeState( audioProcessorValueTreeState )
{
    juce::Colour customButtonColorOn = juce::Colour::fromRGBA(36, 40, 26, 100);
    juce::Colour customButtonColorOff = juce::Colour::fromRGBA(28, 32, 18, 100);
    juce::Colour customTextColorOn = juce::Colours::whitesmoke.withAlpha(0.9f);
    juce::Colour customTextColorOff = juce::Colours::whitesmoke.withAlpha(0.6f);

    addAndMakeVisible( m_lineTextButton );
    m_lineTextButton.setButtonText( "Line" );
    m_lineTextButton.setClickingTogglesState( true );
    m_lineTextButton.setColour(juce::TextButton::textColourOnId, customTextColorOn);
    m_lineTextButton.setColour(juce::TextButton::textColourOffId, customTextColorOff);
    m_lineTextButton.setColour(juce::TextButton::buttonOnColourId, customButtonColorOn);
    m_lineTextButton.setColour(juce::TextButton::buttonColourId, customButtonColorOff);
    m_lineTextButton.onClick = [ this ]

   

    {
        if ( m_lineTextButton.getToggleState() == true )
        {
            m_lineTextButton.setButtonText( "Line" );
        }
        else
        {
            m_lineTextButton.setButtonText( "Bins" );
        }
    };
    
    m_lineTextButtonAttachment =
        std::make_unique<Attachment>(
            mr_audioProcessorValueTreeState,
            "BINS_ID",
            m_lineTextButton
        );
    
    addAndMakeVisible( m_maxTextButton );
    m_maxTextButton.setButtonText( "Max" );
    m_maxTextButton.setClickingTogglesState( true );
    m_maxTextButton.setColour(juce::TextButton::textColourOnId, customTextColorOn);
    m_maxTextButton.setColour(juce::TextButton::textColourOffId, customTextColorOff);
    m_maxTextButton.setColour(juce::TextButton::buttonOnColourId, customButtonColorOn);
    m_maxTextButton.setColour(juce::TextButton::buttonColourId, customButtonColorOff);

    
    m_maxTextButtonAttachment =
        std::make_unique<Attachment>(
            mr_audioProcessorValueTreeState,
            "MAX_ID",
            m_maxTextButton
        );


}



GraphControls::~GraphControls() {}


// ============================================================================
void GraphControls::resized()
{
    auto indent { 3 };
    auto area { getLocalBounds() };
    auto buttonWidth
    {
        ( area.getWidth() - indent ) / 2
    };
    
    m_lineTextButton.setBounds( area.removeFromLeft( buttonWidth ) );
    m_maxTextButton.setBounds( area.removeFromRight( buttonWidth ) );
}
