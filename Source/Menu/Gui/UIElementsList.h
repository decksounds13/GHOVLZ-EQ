#pragma once
#include "../JuceLibraryCode/JuceHeader.h"
#include "../SharedResources.h"
#include "ThemeList.h"
#include "CustomScrollBar.h"
#include "CustomHeader2.h"
#include "MelatoninBlur/melatonin/shadows.h"

class UIElementsList : public juce::Component,
    public juce::ListBoxModel
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void onElementSelected(const juce::String& name, const juce::Colour& color) = 0;
    };

    UIElementsList(SharedResources& resources);

    //void selectItem(int row);
    void paint(juce::Graphics& g) override;
    void paintOverChildren(juce::Graphics& g) override;
    void resized() override;
    int getNumRows() override;
    void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
    void listBoxItemClicked(int row, const juce::MouseEvent&) override;
    void addElement(const juce::String& name, const juce::Colour& color);
    void updateSelectedElementColor(const juce::Colour& newColor);
    juce::String getSelectedElementName();
    juce::String getSelectedElementNameForIndex(int index);
    void addListener(Listener* listener);
    juce::Colour getSelectedElementColor();
    juce::Colour getElementColor(int row);
    void updateScrollBarColors(const juce::Colour& trackColor, const juce::Colour& thumbColor, const juce::Colour& outlineColor);
    void selectAndNotify(int row, bool shouldNotifyListeners);
    void updateSelectedElementsColor(const juce::Colour& newColor);
    void updateColorsForSelectedElements(const juce::Colour& newColor);
    void setElementColor(int index, const juce::Colour& color);
    void updateListBoxSelection();
    void updateIfNeeded();

    void mouseMove(const juce::MouseEvent& event) override;

    void updateGradient();
    void setColor1(juce::Colour newColor);
    void setColor2(juce::Colour newColor);

    juce::Colour getColorForIndex(int index);
 
    juce::Array<int> getSelectedRows();

    juce::ListBox& getListBox();

    juce::Colour getFirstElementColor()
    {
        if (!uiElements.isEmpty())
        {
            auto firstElement = uiElements[0];
            return firstElement.color;
        }
        return juce::Colours::grey;  // default color if the list is empty
    }

    const juce::Path& getShadowPath() const {
        return shadowPath;
    }

    int getSelectedRowIndex() const;

private:
    juce::Path shadowPath;
    juce::ColourGradient cachedGradient;
    bool gradientNeedsUpdate = true;
    bool needsRepaint = false;

    bool cursorOverComponent = false;

    struct UIElement
    {
        juce::String name;
        juce::Colour color;
    };

    juce::Array<UIElement> uiElements;
    juce::ListenerList<Listener> listeners;
    juce::ListBox listBox;
    int selectedRow = -1;
    int hoveredRow = -1;
    std::unique_ptr<CustomScrollBar> customScrollBar;
  
    std::unique_ptr<CustomHeader2> customHeader;

    SharedResources& sharedResources;

    melatonin::DropShadow shadow = { { juce::Colours::black, 3, { -2, 0 } } };
    melatonin::InnerShadow innerShadow1 = { { juce::Colours::black, 3, { 2, 0 } } };

    melatonin::InnerShadow innerShadow = { { juce::Colours::black.withAlpha(0.8f), 14, {-2, -2} } };

    // Additional code to ensure that when an element is selected, the QuadPicker, HueSelector, and Swatch are updated
    void notifyListenersOfSelection();
};
