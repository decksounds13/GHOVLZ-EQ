#include "GraphBins.h"



GraphBins::GraphBins( Analyser &analyser ) : GraphLine::GraphLine( analyser ) {}

GraphBins::~GraphBins() {}



// ============================================================================

void GraphBins::drawFrame( juce::Graphics &g )

{

    auto width  = static_cast<float> ( getLocalBounds().getWidth() );

    auto height = static_cast<float> ( getLocalBounds().getHeight() );

    const float lineOpacity = getSpectrumOpacity();

    const float binWidth = juce::jmax (0.5f, getSpectrumPathWidth() * 0.5f);

    const int lastBin = mr_analyser.getHighestDisplayBinIndex();



    if (lastBin < 1 || width < 2.0f)

        return;



    g.setColour (m_colour.withMultipliedAlpha (lineOpacity));



    for ( auto x { 0 }; x <= lastBin; ++x )

    {

        const float xNorm = normalizeValue (x);

        if (xNorm < 0.0f || xNorm > 1.0f)

            continue;



        const auto xPosition = xNorm * width;



        g.drawLine(

            xPosition,

            juce::jmap(

                getScopeDataFromAnalyser( x ),

                1.0f,

                0.0f,

                0.0f,

                height

            ),

            xPosition,

            height,

            binWidth

        );

    }

}

