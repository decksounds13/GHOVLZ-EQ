#pragma once

#include <JuceHeader.h>
#include "../SharedResources.h"
#include "ThemeList.h"
#include "shadows-main/shadows.h"

class ThemeList;  

class CustomHeader : public juce::Component
{
public:
    CustomHeader(ThemeList* owner, SharedResources* sharedResources);
    ~CustomHeader() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    // Methods to set/get column widths, etc.
    void setColumnWidths(int col1Width, int col2Width, int col3Width);
    
    juce::Array<int> getColumnWidths() const;

private:
    float col1Ratio;
    float col2Ratio;
    float col3Ratio;

    int col1Width;
    int col2Width;
    int col3Width;

    bool isDraggingFirstSeparator = false;
    bool isDraggingSecondSeparator = false;

    SharedResources* sharedResources;
  
    ThemeList* themeList;
  
    std::unique_ptr<shadows::StackShadow> customShadow;
    // More member variables as needed
};
