#include "Visualizer.h"

Visualizer::Visualizer(
    juce::AudioProcessorValueTreeState &audioProcessorValueTreeState,
    Analyser &analyser
) :
    m_grid( audioProcessorValueTreeState ),
    m_graph( audioProcessorValueTreeState, analyser )
{
    addAndMakeVisible( m_grid );
    addAndMakeVisible( m_graph );
    m_grid.addMouseListener (this, true);
    m_graph.addMouseListener (this, true);
    applyThemeColours();
}



Visualizer::~Visualizer() {}

const SharedColors& Visualizer::colors() const noexcept
{
    static const SharedColors defaultColors;
    return themeColors != nullptr ? themeColors->sharedColors : defaultColors;
}

void Visualizer::setThemeColors (SharedResources* r) noexcept
{
    themeColors = r;
    applyThemeColours();
    repaint();
}

void Visualizer::applyThemeColours()
{
    const auto& c = colors();
    m_grid.setGridColour (c.spectrumGrid);
    m_grid.setTextColour (c.spectrumText);
    m_graph.setGraphColour (c.spectrumLine.withAlpha (0.6f));
    m_graph.setGraphFillColour (c.spectrumFill.withAlpha (0.55f));
    m_graph.setGraphMaximumsColour (c.spectrumLine.brighter (0.2f).withAlpha (0.65f));
    m_graph.setBinOverlayColour (c.spectrumFill);
}

void Visualizer::setBinOverlayColourRamp (const GradientRamp* ramp)
{
    m_graph.setBinOverlayColourRamp (ramp);
    repaint();
}

void Visualizer::setSpectrumFillRamp (const GradientRamp* ramp)
{
    m_graph.setSpectrumFillRamp (ramp);
    repaint();
}


// ============================================================================
void Visualizer::paint( juce::Graphics &g )
{
    juce::Path path;
    path.addRoundedRectangle( getLocalBounds(), m_marginInPixels );
 
    const auto& c = colors();

    juce::Colour color1 = c.spectrumBackground;
    juce::Colour color2 = c.spectrumBackground2;

    juce::ColourGradient gradient = juce::ColourGradient::horizontal(color1, 0.0f, color2, static_cast<float>(getWidth()));

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

void Visualizer::mouseDown (juce::MouseEvent const& e)
{
    if (e.mods.isPopupMenu() && onShowContextMenu != nullptr)
        onShowContextMenu();
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
