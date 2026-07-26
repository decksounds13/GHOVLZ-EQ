#include "Visualizer.h"

Visualizer::Visualizer(
    juce::AudioProcessorValueTreeState &audioProcessorValueTreeState,
    Analyser &analyser
) :
    m_grid( audioProcessorValueTreeState ),
    m_graph( audioProcessorValueTreeState, analyser )
{
    addAndMakeVisible( m_grid );
    m_grid.setGridColour( juce::Colour( 0xff464646 ) );
    m_grid.setTextColour( juce::Colour( 0xff848484 ) );
    
    addAndMakeVisible( m_graph );
    m_graph.setGraphColour( juce::Colours::whitesmoke.withAlpha(0.6f) );
    m_graph.setGraphMaximumsColour(juce::Colours::whitesmoke.withAlpha(0.6f) );
}



Visualizer::~Visualizer() {}


// ============================================================================
void Visualizer::paint( juce::Graphics &g )
{
    juce::Path path;
    path.addRoundedRectangle( getLocalBounds(), m_marginInPixels );
 
    auto area = getLocalBounds();
    auto w = area.getWidth();
    auto h = area.getHeight();
    
    // Define your custom colors for the gradient
    juce::Colour color1 = juce::Colour(10, 10, 10); // Custom color 1 (e.g., red)
    juce::Colour color2 = juce::Colour(60, 55, 50); // Custom color 2 (e.g., blue)

    // Create a horizontal linear gradient between two X coordinates
    juce::ColourGradient gradient = juce::ColourGradient::horizontal(color1, 0.0f, color2, static_cast<float>(getWidth()));


    // Fill the component with the gradient
    g.setGradientFill(gradient);
    g.fillAll();
   
    
    
    g.fillPath( path );
}



void Visualizer::resized()
{
    auto area = getLocalBounds().reduced( m_marginInPixels );
    
    m_grid.setBounds( area );
    m_graph.setBounds( area );
}


// ============================================================================
void Visualizer::setBackgroundColour( const juce::Colour &colour )
{
    m_backgroundColour = colour;
}


// ========================================================================
void Visualizer::setMarginInPixels( const int margin )
{
    m_marginInPixels = margin;
}
