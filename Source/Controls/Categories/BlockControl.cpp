#include "BlockControl.h"

#include "../../ComboBoxLookAndFeel.h"



// ****************************************************************************

// BLOCK CONTROL CLASS

// ****************************************************************************

BlockControl::BlockControl(

    juce::AudioProcessorValueTreeState& audioProcessorValueTreeState

) :

    mr_audioProcessorValueTreeState(audioProcessorValueTreeState)

{

    // Set the custom LookAndFeel

    m_blockComboBox.setLookAndFeel(&comboBoxLookAndFeel);





    addAndMakeVisible(m_blockComboBox);

 

    m_blockComboBox.addItem("2048", 1);

    m_blockComboBox.addItem("4096", 2);

    m_blockComboBox.addItem("8192", 3);

    m_blockComboBox.addItem("16384", 4);

    m_blockComboBox.setSelectedId(1, juce::dontSendNotification);

    m_blockComboBox.setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha(0.8f));



    m_blockComboBoxAttachment =

        std::make_unique<Attachment>(

            mr_audioProcessorValueTreeState,

            "BLOCK_ID",

            m_blockComboBox

        );





}









BlockControl::~BlockControl() 

{

    m_blockComboBox.setLookAndFeel(nullptr);

}





// ============================================================================

void BlockControl::resized()

{

    m_blockComboBox.setBounds( getLocalBounds() );

}


