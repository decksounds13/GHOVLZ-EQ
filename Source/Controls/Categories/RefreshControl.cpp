#include "RefreshControl.h"

RefreshControl::RefreshControl(
    juce::AudioProcessorValueTreeState &audioProcessorValueTreeState
) :
    mr_audioProcessorValueTreeState( audioProcessorValueTreeState )
{
    addAndMakeVisible( m_refreshSlider );
    m_refreshSlider.setSliderStyle( juce::Slider::SliderStyle::LinearBar );
    m_refreshSlider.setRange( 16, 200, 1 );
    m_refreshSlider.setValue( 33, juce::dontSendNotification );
    m_refreshSlider.setTextValueSuffix( " ms" );
    m_refreshSlider.setColour(juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha(0.5f));
    m_refreshSlider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha(0.8f));
    
    m_refreshSliderAttachment =
        std::make_unique<Attachment>(
            mr_audioProcessorValueTreeState,
            "REFRESH_ID",
            m_refreshSlider
        );
}



RefreshControl::~RefreshControl() {}


// ============================================================================
void RefreshControl::resized()
{
    m_refreshSlider.setBounds( getLocalBounds() );
}
