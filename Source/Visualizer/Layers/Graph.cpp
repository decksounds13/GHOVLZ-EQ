#include "Graph.h"

Graph::Graph(
    juce::AudioProcessorValueTreeState &audioProcessorValueTreeState,
    Analyser &analyser
) :
    mr_audioProcessorValueTreeState( audioProcessorValueTreeState ),
    mr_analyser( analyser ),
    m_graphMaximumsLine( analyser ),
    m_graphLine( analyser ),
    m_graphBins( analyser ),
    m_infoLabel( audioProcessorValueTreeState )
{


  //  Component::setRepaintsOnMouseActivity(false);

  //  Component::setAlwaysOnTop(false);

  //  toBack();

    // REFRESH_ID drives UI paint polling; analysis thread uses the same interval
    // to FFT the latest Block window (Ableton-style overlapping refresh).
    {
        int refreshMs = 33; // ~30 Hz default
        if (auto* p = mr_audioProcessorValueTreeState.getRawParameterValue ("REFRESH_ID"))
            refreshMs = juce::jlimit (16, 200, (int) std::lround (p->load()));
        startTimer (refreshMs);
    }

    // Max curve is drawn inside GraphLine; this helper layer stays available for MAX_ID.
    m_graphMaximumsLine.setColour( m_volumeMaximumsGraphColour );
    
    addAndMakeVisible (m_graphLine);
    m_graphLine.setColour( m_volumeGraphColour );
    m_graphLine.setAudioProcessorValueTreeState (&mr_audioProcessorValueTreeState);
    
    addChildComponent( m_graphBins );
    m_graphBins.setColour( m_volumeGraphColour );
    m_graphBins.setAudioProcessorValueTreeState (&mr_audioProcessorValueTreeState);

    m_graphMaximumsLine.setAudioProcessorValueTreeState (&mr_audioProcessorValueTreeState);

    // Don't wait for the first FFT block — keep the line graph visible immediately.
    const bool analyserOn = isAnalyserEnabled();
    const bool lineMode = mr_audioProcessorValueTreeState.getRawParameterValue ("BINS_ID") == nullptr
                          || mr_audioProcessorValueTreeState.getRawParameterValue ("BINS_ID")->load() > 0.5f;
    m_graphLine.setVisible (analyserOn && lineMode);
    m_graphBins.setVisible (analyserOn && ! lineMode);
    
    mr_audioProcessorValueTreeState.addParameterListener( "REFRESH_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "BINS_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "MAX_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "LIN_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "LOG_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "ST_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_ANALYSER_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_PRE_CURVE_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_PRE_FILL_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_POST_CURVE_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_POST_FILL_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_HOLD_FILL_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_FILL_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_PATH_WIDTH_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_RESOLUTION_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_CURVE_RES_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_FFT_BINS_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_FULL_HEIGHT_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_RESOLUTION_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_BAR_WIDTH_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_INTENSITY_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_THRESHOLD_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_GLOW_RADIUS_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_GLOW_SPREAD_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_GLOW_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_GLOW_OFFSET_X_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_GLOW_OFFSET_Y_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_GLOW_ENABLE_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_GLOW_RADIUS_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_GLOW_SPREAD_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "SPECTRUM_GLOW_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "FFT_GLOW_ENABLE_ID", this );
    mr_audioProcessorValueTreeState.addParameterListener( "MAX_HOLD_ID", this );

}



Graph::~Graph()
{
    mr_audioProcessorValueTreeState.removeParameterListener( "REFRESH_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "BINS_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "MAX_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "LIN_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "LOG_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "ST_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_ANALYSER_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_PRE_CURVE_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_PRE_FILL_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_POST_CURVE_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_POST_FILL_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_HOLD_FILL_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_FILL_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_PATH_WIDTH_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_RESOLUTION_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_CURVE_RES_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_FFT_BINS_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_FULL_HEIGHT_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_RESOLUTION_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_BAR_WIDTH_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_INTENSITY_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_THRESHOLD_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_GLOW_RADIUS_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_GLOW_SPREAD_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_GLOW_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_GLOW_OFFSET_X_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_GLOW_OFFSET_Y_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_GLOW_ENABLE_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_GLOW_RADIUS_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_GLOW_SPREAD_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "SPECTRUM_GLOW_OPACITY_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "FFT_GLOW_ENABLE_ID", this );
    mr_audioProcessorValueTreeState.removeParameterListener( "MAX_HOLD_ID", this );

    stopTimer();
}


// ============================================================================
void Graph::resized()
{
    m_graphMaximumsLine.setBounds( getLocalBounds() );
    m_graphLine.setBounds( getLocalBounds() );
    m_graphBins.setBounds( getLocalBounds() );
    m_infoLabel.setBounds( getLocalBounds() );
}


// ============================================================================
void Graph::mouseDown( const juce::MouseEvent &event )
{
    mr_analyser.resetScopeMaximumsData();
}



void Graph::mouseEnter( const juce::MouseEvent &event )
{
    m_infoLabel.setVisible( true );
}



void Graph::mouseExit( const juce::MouseEvent &event )
{
    m_infoLabel.setVisible( false );
}



void Graph::mouseMove( const juce::MouseEvent &event )
{
    auto position = event.getMouseDownPosition();
    
    m_infoLabel.setData(
        position.getX(),
        position.getY(),
        m_scaleTypeIsLogarithmic.load()
    );
}


// ============================================================================
void Graph::timerCallback()
{
    const bool analyserOn = isAnalyserEnabled();

    if (! analyserOn)
    {
        // Discard pending FFT work and hide spectrum layers while disabled.
        if (mr_analyser.getNextFFTBlockStatus())
            mr_analyser.setNextFFTBlockStatus (false);
        if (mr_analyser.getNextPreFFTBlockStatus())
            mr_analyser.setNextPreFFTBlockStatus (false);

        m_graphLine.setVisible (false);
        m_graphBins.setVisible (false);
        m_graphMaximumsLine.setVisible (false);
        return;
    }

    // Analyser is on — always keep layers visible, even before the first FFT frame.
    // Gating visibility on getNextFFTBlockStatus() left the graph blank after project
    // load / BLOCK_ID resets (flags cleared) until the user toggled Spectrum in Settings.
    m_graphMaximumsLine.setVisible (m_maximumVolumesIsVisible.load());
    m_graphLine.setVisible (m_graphStyleIsLine.load());
    m_graphBins.setVisible (! m_graphStyleIsLine.load());

    if (mr_analyser.getNextFFTBlockStatus() || mr_analyser.getNextPreFFTBlockStatus())
    {
        mr_analyser.calculateNextFrameOfSpectrum();

        m_graphMaximumsLine.setScaleType (m_scaleTypeIsLogarithmic.load());
        m_graphLine.setScaleType (m_scaleTypeIsLogarithmic.load());
        m_graphBins.setScaleType (m_scaleTypeIsLogarithmic.load());

        repaint();
    }
}


// ============================================================================
void Graph::setGraphColour( juce::Colour colour )
{
    m_volumeGraphColour = colour;
    m_graphLine.setColour (colour);
    m_graphBins.setColour (colour);
}

void Graph::setGraphFillColour (juce::Colour colour)
{
    m_graphLine.setFillColour (colour);
    m_graphMaximumsLine.setFillColour (colour);
}

void Graph::setBinOverlayColour (juce::Colour colour)
{
    m_graphLine.setBinOverlayColour (colour);
}

void Graph::setBinOverlayColourRamp (const GradientRamp* ramp)
{
    m_graphLine.setBinOverlayColourRamp (ramp);
}

void Graph::setSpectrumFillRamp (const GradientRamp* ramp)
{
    m_graphLine.setSpectrumFillRamp (ramp);
}

void Graph::setGraphMaximumsColour( juce::Colour colour )
{
    m_volumeMaximumsGraphColour = colour;
    m_graphMaximumsLine.setColour (colour);
    m_graphLine.setHoldColour (colour);
    m_graphMaximumsLine.setHoldColour (colour);
}


// ============================================================================
void Graph::setTimerInterval( const int milliseconds )
{
    stopTimer();
    startTimer( milliseconds );
}



void Graph::setGraphStyleAsLine( const bool isLine )
{
    m_graphStyleIsLine.store( isLine );
}



void Graph::setMaximumVolumesVisible( const bool isVisible )
{
    m_maximumVolumesIsVisible.store( isVisible );
}



void Graph::setScaleTypeAsLogarithmic( const bool isLogarithmic )
{
    m_scaleTypeIsLogarithmic.store( isLogarithmic );
}

bool Graph::isAnalyserEnabled() const
{
    if (mr_analyser.isEcoMode())
        return false;

    if (auto* p = mr_audioProcessorValueTreeState.getRawParameterValue ("SPECTRUM_ANALYSER_ID"))
        return p->load() > 0.5f;

    return true;
}


// ============================================================================
void Graph::parameterChanged(
    const juce::String &parameterID,
    float newValue
) {
    if ( parameterID == "REFRESH_ID" )
    {
        setTimerInterval( static_cast<int>( newValue ) );
    }
    if ( parameterID == "BINS_ID" )
    {
        setGraphStyleAsLine( static_cast<bool>( newValue ) );
    }
    else if ( parameterID == "MAX_ID" )
    {
        setMaximumVolumesVisible( static_cast<bool>( newValue ) );
    }
    else if ( parameterID == "LIN_ID" )
    {
        setScaleTypeAsLogarithmic( false );
    }
    else if ( parameterID == "LOG_ID" || parameterID == "ST_ID" )
    {
        setScaleTypeAsLogarithmic( true );
    }
    else if ( parameterID == "SPECTRUM_ANALYSER_ID" )
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Graph> (this)]
        {
            if (safe == nullptr)
                return;

            if (! safe->isAnalyserEnabled())
            {
                safe->m_graphLine.setVisible (false);
                safe->m_graphBins.setVisible (false);
                safe->m_graphMaximumsLine.setVisible (false);
            }
            else
            {
                safe->m_graphMaximumsLine.setVisible (safe->m_maximumVolumesIsVisible.load());
                safe->m_graphLine.setVisible (safe->m_graphStyleIsLine.load());
                safe->m_graphBins.setVisible (! safe->m_graphStyleIsLine.load());
            }

            safe->repaint();
        });
    }
    else if (parameterID == "SPECTRUM_OPACITY_ID"
             || parameterID == "SPECTRUM_FILL_OPACITY_ID"
             || parameterID == "SPECTRUM_PATH_WIDTH_ID"
             || parameterID == "SPECTRUM_RESOLUTION_ID"
             || parameterID == "SPECTRUM_CURVE_RES_ID"
             || parameterID == "SPECTRUM_PRE_CURVE_ID"
             || parameterID == "SPECTRUM_PRE_FILL_ID"
             || parameterID == "SPECTRUM_POST_CURVE_ID"
             || parameterID == "SPECTRUM_POST_FILL_ID"
             || parameterID == "SPECTRUM_HOLD_FILL_ID"
             || parameterID == "SPECTRUM_FFT_BINS_ID"
             || parameterID == "FFT_FULL_HEIGHT_ID"
             || parameterID == "FFT_RESOLUTION_ID"
             || parameterID == "FFT_OPACITY_ID"
             || parameterID == "FFT_BAR_WIDTH_ID"
             || parameterID == "FFT_INTENSITY_ID"
             || parameterID == "FFT_THRESHOLD_ID"
             || parameterID == "FFT_GLOW_RADIUS_ID"
             || parameterID == "FFT_GLOW_SPREAD_ID"
             || parameterID == "FFT_GLOW_OPACITY_ID"
             || parameterID == "FFT_GLOW_OFFSET_X_ID"
             || parameterID == "FFT_GLOW_OFFSET_Y_ID"
             || parameterID == "SPECTRUM_GLOW_ENABLE_ID"
             || parameterID == "SPECTRUM_GLOW_RADIUS_ID"
             || parameterID == "SPECTRUM_GLOW_SPREAD_ID"
             || parameterID == "SPECTRUM_GLOW_OPACITY_ID"
             || parameterID == "FFT_GLOW_ENABLE_ID"
             || parameterID == "MAX_HOLD_ID")
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<Graph> (this)]
        {
            if (safe != nullptr)
                safe->repaint();
        });
    }
}



