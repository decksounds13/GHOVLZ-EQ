#pragma once

#include <JuceHeader.h>
#include <functional>
#include "Layers/Grid.h"
#include "Layers/Graph.h"
#include "Analyser.h"
#include "Menu/SharedResources.h"
#include "ColourRamp/GradientRamp.h"

/*
    TASKS
    
    Layer with interpretation of cursor position in note, frequency and volume.
*/

// ****************************************************************************
// VISUALISER CLASS
// ****************************************************************************
class Visualizer :
    public juce::Component
{
public:
    Visualizer( juce::AudioProcessorValueTreeState &, Analyser & );
    ~Visualizer() override;
    
    
    // ========================================================================
    void paint( juce::Graphics & ) override;
    void resized() override;
    void mouseDown (juce::MouseEvent const&) override;

    std::function<void()> onShowContextMenu;

    // ========================================================================
    void setBackgroundColour( const juce::Colour & );
    
    
    // ========================================================================
    void setMarginInPixels( const int );

    void setThemeColors (SharedResources* r) noexcept;
    void setBinOverlayColourRamp (const GradientRamp* ramp);
    void setSpectrumFillRamp (const GradientRamp* ramp);

private:
    // ========================================================================
    ::Grid m_grid;  // Use :: to indicate that Grid is in the global namespace
    Graph m_graph;
    
    juce::Colour m_backgroundColour { 0xff323232 };
    int m_marginInPixels { 10 };
    SharedResources* themeColors = nullptr;

    const SharedColors& colors() const noexcept;
    void applyThemeColours();
    
    
    // ========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( Visualizer )
};
