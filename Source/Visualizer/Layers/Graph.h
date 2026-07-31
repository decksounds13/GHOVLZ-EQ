#pragma once

#include <JuceHeader.h>
#include "../Analyser.h"
#include "GraphElements/GraphLine.h"
#include "GraphElements/GraphBins.h"
#include "GraphElements/GraphMaximumsLine.h"
#include "GraphElements/GraphInfoLabel.h"

// ****************************************************************************
// GRAPH CLASS
// ****************************************************************************
class Graph :
    public juce::Component,
    public juce::AudioProcessorValueTreeState::Listener,
    private juce::Timer
{
public:
    Graph( juce::AudioProcessorValueTreeState &, Analyser & );
    ~Graph() override;
    
    
    // ========================================================================
    void resized() override;
    
    
    // ========================================================================
    void mouseDown( const juce::MouseEvent & ) override;
    void mouseEnter( const juce::MouseEvent & ) override;
    void mouseExit( const juce::MouseEvent & ) override;
    void mouseMove( const juce::MouseEvent & ) override;
    
    
    // ========================================================================
    void timerCallback() override;
    
    
    // ========================================================================
    void setGraphColour( juce::Colour );
    void setGraphMaximumsColour( juce::Colour );
    void setBinOverlayColour (juce::Colour);
    void setBinOverlayColourRamp (const GradientRamp* ramp);
    void setSpectrumFillRamp (const GradientRamp* ramp);

private:
    // ========================================================================
    void setTimerInterval( const int );
    void setGraphStyleAsLine( const bool );
    void setMaximumVolumesVisible( const bool );
    void setScaleTypeAsLogarithmic( const bool );
    bool isAnalyserEnabled() const;
    
    
    // ========================================================================
    void parameterChanged( const juce::String &, float ) override;
    
    
    // ========================================================================
    juce::AudioProcessorValueTreeState &mr_audioProcessorValueTreeState;
    Analyser &mr_analyser;
    
    GraphMaximumsLine m_graphMaximumsLine;
    GraphLine m_graphLine;
    GraphBins m_graphBins;
    GraphInfoLabel m_infoLabel;
    
    std::atomic<bool> m_graphStyleIsLine { true };
    std::atomic<bool> m_maximumVolumesIsVisible { true };
    std::atomic<bool> m_scaleTypeIsLogarithmic { true };
    
    juce::Colour m_volumeGraphColour { juce::Colour::fromRGBA(120, 30, 37, 100)};
    juce::Colour m_volumeMaximumsGraphColour { juce::Colour::fromRGBA(120, 69, 66, 75 ) };
    
    // ========================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( Graph )
};
