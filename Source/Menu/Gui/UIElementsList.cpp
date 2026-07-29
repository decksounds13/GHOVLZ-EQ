#include "UIElementsList.h"
#include "AppearanceComponent.h"
#include "../ThemeColorRegistry.h"
#include <algorithm>

UIElementsList::UIElementsList(SharedResources& resources)
    : sharedResources(resources), selectedRow(-1)
{
    customHeader = std::make_unique<CustomHeader2>(sharedResources);
    customHeader->addListener (this);

    addAndMakeVisible(listBox);

    listBox.setModel(this);
    needsRepaint = true;

    customScrollBar = std::make_unique<CustomScrollBar>(listBox);

    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    listBox.updateContent();
    listBox.repaint();
    listBox.setVisible(true);
     listBox.setRepaintsOnMouseActivity(false);
    listBox.setMultipleSelectionEnabled(true);


    addAndMakeVisible(*customScrollBar);

    addAndMakeVisible(*customHeader);

    juce::Colour menuScrollBarTrackColor1 = sharedResources.sharedColors.menuScrollBarTrackColor1;
    juce::Colour menuScrollBarThumbColor1 = sharedResources.sharedColors.menuScrollBarThumbColor1;
    juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;

    customScrollBar->setTrackBackgroundColour(menuScrollBarTrackColor1);
    customScrollBar->setThumbBackgroundColour(menuScrollBarThumbColor1);
    customScrollBar->setThumbOutlineColour(menuThinBorderColor);
    customScrollBar->repaint();

    selectedRow = 0;
    notifyListenersOfSelection();

    juce::Colour gradientColor1 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor1;
    juce::Colour gradientColor2 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor2;

    setColor1(gradientColor1);
    setColor2(gradientColor2);
    gradientNeedsUpdate = true;
    updateGradient();
    repaint();
}

void UIElementsList::paint(juce::Graphics& g)
{
    //if (!needsRepaint) {
    //    return; // Skip painting if no new elements and no hover
    //}

    juce::Colour gradientColor1 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor1;
    juce::Colour gradientColor2 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor2;

    float cornerSize = 14.0f;
    setColor1(gradientColor1);
    setColor2(gradientColor2);
    gradientNeedsUpdate = true;
    updateGradient();
    g.setGradientFill(cachedGradient);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0), cornerSize);
}

void UIElementsList::paintOverChildren(juce::Graphics& g)
{
    //if (!needsRepaint) {
    //    return; // Skip painting if no new elements and no hover
    //}

    // Define the radius for the rounded corners
    float cornerSize = 14.0f;

    // Create a path for the rounded rectangle
    juce::Path roundedRectPath;
    juce::Rectangle<float> bounds = getLocalBounds().toFloat();
    roundedRectPath.addRoundedRectangle(bounds, cornerSize);

    // Create a path for the outline with the same corner size
    juce::Path outlinePath;
    outlinePath.addRoundedRectangle(bounds, cornerSize);

    // Save the current graphics state
    g.saveState();

    // Set the clipping region to the path
    g.reduceClipRegion(shadowPath);

    //shadow.render(g, outlinePath);

    // Call the base class method to paint child components
    // This will clip the child components to the rounded corners
    Component::paintOverChildren(g);

 

    // Restore the graphics state
    g.restoreState();

    //innerShadow1.render(g, roundedRectPath);

    // Draw the outline over all child components
    //juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;
    //g.setColour(menuThinBorderColor);
    //g.strokePath(outlinePath, juce::PathStrokeType(4.0f)); // Adjust the stroke thickness as needed
}



void UIElementsList::resized()
{
    DBG("UIElementsList::resized Called");

    needsRepaint = true;
    gradientNeedsUpdate = true;

    const int headerHeight = 20; // Adjust this height as needed for your header
    const int padding = 2; // Listbox outline width
    const int scrollBarWidth = 11;

    // Position the custom header at the top
    customHeader->setBounds(0, 0, getWidth(), headerHeight);

    // Calculate the bounds for the listbox, positioned below the header
    juce::Rectangle<int> listBoxBounds(0, headerHeight, getWidth() - scrollBarWidth, getHeight() - headerHeight);

    listBox.setBounds(listBoxBounds);
    //DBG("ListBox resized. Calling loadPresetsFromXML again.");

    // Ensure updateContent is called after loading presets
    listBox.updateContent();

    //DBG("ListBox bounds: " << listBoxBounds.toString());
    //DBG("ListBox bounds after resize: " << listBox.getBounds().toString());


    // Adjust the scrollbar bounds to align with the listbox
    int scrollBarStartX = getWidth() - scrollBarWidth;
    int scrollBarHeight = listBoxBounds.getHeight();
    customScrollBar->setBounds(scrollBarStartX, headerHeight, scrollBarWidth, scrollBarHeight);

    float cornerSize = 5.0f;
    shadowPath.clear();  // Clear the existing path
    shadowPath.addRoundedRectangle(getLocalBounds().toFloat(), cornerSize);
}

int UIElementsList::getNumRows()
{
    return uiElements.size();
}

void UIElementsList::paintListBoxItem(int rowNumber, juce::Graphics& g,
    int width, int height, bool rowIsSelected)
{    gradientNeedsUpdate = true;
    auto mousePosition = getMouseXYRelative();
    hoveredRow = listBox.getRowContainingPosition(mousePosition.x, mousePosition.y) - 1;

        

    //if (!needsRepaint) {
    //    return; // Skip painting if no new elements and no hover
    //}

    DBG("UIElementsList::paintListBoxItem Called");

    if (rowNumber < uiElements.size())
    {
        auto element = uiElements[rowNumber];  // Use a copy instead of a reference

        // Fetch the live colour from SharedResources by palette index / name
        if (element.paletteIndex >= 0 && element.paletteIndex < sharedResources.sharedColors.getNumColors())
            element.color = sharedResources.sharedColors.colourAt (element.paletteIndex);
        else if (auto* c = sharedResources.sharedColors.findByDisplayName (element.name))
            element.color = *c;

        float cornerSize = 0.0f;
        const int innerMargin = 1;

        // Get the parent's bounds
        juce::Rectangle<int> parentBounds = getParentComponent()->getLocalBounds();
       
        juce::Rectangle<float> bounds(
            innerMargin, // X position
            innerMargin, // Y position
            width - 2 * innerMargin, // Width
            height - innerMargin * 2 // Height
        );

        juce::Path roundedRectPath;

        roundedRectPath.addRoundedRectangle(bounds, 2);

        innerShadow.render(g, roundedRectPath);

        juce::Colour menuListBoxSelectionColor1 = sharedResources.sharedColors.menuListBoxSelectionColor1;
       
      

        // Check if the mouse is hovering over this row
        if (hoveredRow == rowNumber && !rowIsSelected) {
            g.setColour(menuListBoxSelectionColor1.withAlpha(0.75f)); // Semi-transparent hover colour
        }
        else if (rowIsSelected) {
            g.setColour(menuListBoxSelectionColor1); // Solid colour for selected row
        }
        else {
            g.setColour(juce::Colours::transparentBlack); // Default background
        }
        g.fillRect(0, 0, width, height);

        juce::Colour listTextColor = sharedResources.sharedColors.menuListBoxTextColor1;

        // Draw a small swatch of the corresponding color with padding
        int swatchSize = height - 6;
        int swatchX = width - swatchSize - 15;  // Positioning it on the far right with 5 pixels padding
        int swatchY = (height - swatchSize) / 2;
        g.setColour(element.color);  // Use the updated color
        g.fillRect(swatchX, swatchY, swatchSize, swatchSize);
       

       
        // Draw an outline around the swatch
        juce::Colour outlineColour2 = juce::Colour::fromRGB(25, 30, 25); // Set the color for the outline
        juce::Path outlinePath;
        // Define the rectangle for the outline path, slightly smaller to fit within the component
        outlinePath.addRectangle(juce::Rectangle<float>(swatchX, swatchY, swatchSize, swatchSize));
        g.setColour(outlineColour2); // Set the color for the outline
        g.strokePath(outlinePath, juce::PathStrokeType(2.0f)); // Stroke the path with 2-pixel thickness


        // Now, add the outlines at the end of the function, after the existing code
        const int outlineThickness = 1;  // Define the outline thickness
      

        // Draw an outline around each list box item with the specified thickness and inner margin
        juce::Colour outlineColour = juce::Colours::grey;  // Choose a suitable colour for the outline
        g.setColour(outlineColour.withAlpha(0.65f));  // Set the colour with some transparency
        // Draw the outline inside the bounds, respecting the inner margin
        g.drawRect(innerMargin, innerMargin,
            width - 2 * innerMargin, height - 2 * innerMargin + 2, outlineThickness);

        g.setColour(listTextColor);

        // Set the font to Lato Black, size 16
        g.setFont(juce::Font("Lato Black", 16.0f, juce::Font::plain));

        const int textIndent = 10;  // Add an indent for the text
        const int textWidth = width - swatchSize - 15 - textIndent;  // Adjust text width for the indent
        // Draw the item name with the indent
        g.drawText(element.name, textIndent, 0, textWidth, height, juce::Justification::centredLeft);
      
     
        needsRepaint = false;
    }
}


juce::Colour UIElementsList::getElementColor(int row)
{
    if (row >= 0 && row < uiElements.size())
    {
        const int pi = uiElements[row].paletteIndex;
        if (pi >= 0 && pi < sharedResources.sharedColors.getNumColors())
            return sharedResources.sharedColors.colourAt (pi);
        if (auto* c = sharedResources.sharedColors.findByDisplayName (uiElements[row].name))
            return *c;
        return uiElements[row].color;
    }

    return juce::Colours::rebeccapurple;
}

void UIElementsList::listBoxItemClicked(int row, const juce::MouseEvent&)
{
    DBG("UIElementsList: listBoxItemClicked - Row " << row);

    needsRepaint = true;
    if (row < uiElements.size())
    {
       // bool isSelected = listBox.isRowSelected(row);
        //listBox.selectRow(row, !isSelected, false); // Toggle the selection state
        selectAndNotify(row, true); // Notify listeners about the selection change

    }
}

void UIElementsList::selectAndNotify(int row, bool shouldNotifyListeners)
{
    DBG("UIElementsList: selectAndNotify - Row " << row);

    needsRepaint = true;
    if (row >= 0 && row < getNumRows())
    {
        listBox.selectRow(row, !listBox.isRowSelected(row), false);
        selectedRow = row;

        juce::Colour selectedColor = getElementColor(row);

        if (shouldNotifyListeners)
        {
            notifyListenersOfSelection();
        }

        // Assume updateColorSelectors will update QuadPicker and HueSelector
        if (auto* parentAppearanceComponent = findParentComponentOfClass<AppearanceComponent>())
        {
            parentAppearanceComponent->updateColorSelectors(selectedColor);
        }
    }
}

void UIElementsList::addElement(const juce::String& name, const juce::Colour& color)
{
    addElement (name, color, ThemeColorRegistry::indexForDisplayName (name));
}

void UIElementsList::addElement(const juce::String& name, const juce::Colour& color, int paletteIndex)
{
    needsRepaint = true;
    uiElements.add ({ name, color, paletteIndex });
    listBox.updateContent();

    if (selectedRow == -1)
    {
        selectedRow = 0;
        notifyListenersOfSelection();
    }
    listBox.updateContent();
}

void UIElementsList::populateFromRegistry()
{
    uiElements.clearQuick();
    const auto* entries = ThemeColorRegistry::getEntries();
    const int n = ThemeColorRegistry::getNumEntries();
    for (int i = 0; i < n; ++i)
        uiElements.add ({ entries[i].displayName, sharedResources.sharedColors.colourAt (i), i });

    nameSortActive = false;
    nameSortAscending = true;
    if (customHeader)
        customHeader->setSortIndicator (false, true);

    selectedRow = uiElements.isEmpty() ? -1 : 0;
    listBox.updateContent();
    listBox.deselectAllRows();
    if (selectedRow >= 0)
        listBox.selectRow (selectedRow);
    notifyListenersOfSelection();
}

void UIElementsList::sortByElementName()
{
    if (uiElements.isEmpty())
        return;

    // Preserve selection by palette index across reordering.
    juce::Array<int> selectedPalette;
    for (auto row : getSelectedRows())
        if (auto pi = getPaletteIndexForRow (row); pi >= 0)
            selectedPalette.add (pi);

    const int anchorPalette = getPaletteIndexForRow (selectedRow);

    if (! nameSortActive)
    {
        nameSortActive = true;
        nameSortAscending = true;
    }
    else
    {
        nameSortAscending = ! nameSortAscending;
    }

    std::sort (uiElements.begin(), uiElements.end(),
               [this] (const UIElement& a, const UIElement& b)
               {
                   const int cmp = a.name.compareIgnoreCase (b.name);
                   return nameSortAscending ? (cmp < 0) : (cmp > 0);
               });

    if (customHeader)
        customHeader->setSortIndicator (true, nameSortAscending);

    listBox.deselectAllRows();
    selectedRow = -1;
    for (int i = 0; i < uiElements.size(); ++i)
    {
        if (selectedPalette.contains (uiElements[i].paletteIndex))
            listBox.selectRow (i, true, false);
        if (uiElements[i].paletteIndex == anchorPalette)
            selectedRow = i;
    }
    if (selectedRow < 0 && ! uiElements.isEmpty())
        selectedRow = 0;

    listBox.updateContent();
    listBox.repaint();
    notifyListenersOfSelection();
}

void UIElementsList::elementNameHeaderClicked()
{
    sortByElementName();
}

int UIElementsList::getPaletteIndexForRow (int row) const
{
    if (row >= 0 && row < uiElements.size())
        return uiElements[row].paletteIndex;
    return -1;
}

juce::Array<int> UIElementsList::getSelectedPaletteIndices()
{
    juce::Array<int> result;
    for (auto row : getSelectedRows())
    {
        const int pi = getPaletteIndexForRow (row);
        if (pi >= 0)
            result.add (pi);
    }
    return result;
}

void UIElementsList::updateSelectedElementColor(const juce::Colour& newColor)
{
    needsRepaint = true;
    if (selectedRow >= 0 && selectedRow < uiElements.size())
    {
        DBG("Updating Element at row: " + juce::String(selectedRow) + " from Color: " + uiElements[selectedRow].color.toString() + " to Color: " + newColor.toString());
        uiElements[selectedRow].color = newColor;
        listBox.updateContent();
    }
}

void UIElementsList::updateColorsForSelectedElements(const juce::Colour& newColor) {
    needsRepaint = true;
    for (auto index : getSelectedRows()) {
        setElementColor(index, newColor);
    }
}

void UIElementsList::setElementColor(int index, const juce::Colour& color) {
    needsRepaint = true;
    if (index >= 0 && index < uiElements.size()) {
        uiElements[index].color = color;
        listBox.repaintRow(index);
    }
}


juce::String UIElementsList::getSelectedElementName()
{
    if (selectedRow >= 0 && selectedRow < uiElements.size())
    {
        return uiElements[selectedRow].name;
    }
    return {};
}

juce::String UIElementsList::getSelectedElementNameForIndex(int index) {
    if (index >= 0 && index < uiElements.size()) {
        return uiElements[index].name; // Assuming each element has a 'name' attribute
    }
    else {
        return {}; // Return an empty string if the index is out of bounds
    }
}

void UIElementsList::addListener(Listener* newListener)
{
    listeners.add(newListener);
}

juce::Colour UIElementsList::getSelectedElementColor()
{
    if (selectedRow >= 0 && selectedRow < uiElements.size())
    {
        juce::Colour color = uiElements[selectedRow].color;
        DBG("Fetched Color: " + color.toString());
        return color;
    }
    DBG("No selected element or invalid row");
    return {};
}

void UIElementsList::notifyListenersOfSelection()
{
    needsRepaint = true;

    if (selectedRow >= 0 && selectedRow < uiElements.size())
    {
        const auto& element = uiElements[selectedRow];
        juce::Colour selectedColor = element.color;
        DBG("Before Notifying Listeners in notifyListenersOfSelection: " + selectedColor.toString());

        listeners.call([&](Listener& listener) {
            listener.onElementSelected(element.name, element.color);
            });
    }
}

juce::Colour UIElementsList::getColorForIndex(int index)
{
    if (index >= 0 && index < uiElements.size())
    {
        return uiElements[index].color;
    }
    return {}; // Return an empty color if the index is out of bounds or invalid
}

juce::ListBox& UIElementsList::getListBox()
{
    return listBox;
}

// UIElementsList class



void UIElementsList::updateScrollBarColors(const juce::Colour& trackColor, const juce::Colour& thumbColor, const juce::Colour& outlineColor)
{
    needsRepaint = true;
    if (customScrollBar)
    {
        customScrollBar->setTrackBackgroundColour(trackColor);
        customScrollBar->setThumbBackgroundColour(thumbColor);
        customScrollBar->setThumbOutlineColour(outlineColor);
        customScrollBar->repaint();
    }
}

juce::Array<int> UIElementsList::getSelectedRows() {
    juce::Array<int> selectedRows;
    for (int i = 0; i < getNumRows(); ++i) { // Use getNumRows() from UIElementsList, not from ListBox
        if (listBox.isRowSelected(i)) {
            selectedRows.add(i);
        }
    }
    return selectedRows;
}

int UIElementsList::getSelectedRowIndex() const 
{
    return selectedRow; // Assuming 'selectedRow' holds the index of the selected row
}

void UIElementsList::updateGradient() {
    if (gradientNeedsUpdate) {
        cachedGradient = juce::ColourGradient(
            sharedResources.sharedColors.menuListBoxBackgroundGradientColor1,
            juce::Point<float>(static_cast<float>(getWidth()) / 2.0f, static_cast<float>(getHeight() * -1.5)),
            sharedResources.sharedColors.menuListBoxBackgroundGradientColor2, 
            juce::Point<float>(static_cast<float>(getWidth()) / 2, static_cast<float>(getHeight() * 1.5)),
            false
        );
        gradientNeedsUpdate = false;
    }
}

void UIElementsList::setColor1(juce::Colour newColor) {
    auto& color1 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor1;
    if (color1 != newColor) {
        color1 = newColor;
        gradientNeedsUpdate = true;
        repaint();
    }
}

void UIElementsList::setColor2(juce::Colour newColor) {
    auto& color2 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor2;
    if (color2 != newColor) {
        color2 = newColor;
        gradientNeedsUpdate = true;
        repaint();
    }
}

void UIElementsList::mouseMove(const juce::MouseEvent& event) {


    /*
      // Handle mouse move for changing cursor here
      // Call the corresponding methods in CustomHeader
      customHeader->mouseMove(event);

      DBG("Mouse Move Called");
      DBG("Mouse Move: X=" << event.x << " Y=" << event.y); // Add this debug statement

      hoveredRow = listBox.getRowContainingPosition(event.x, event.y);
      DBG("Updated Hovered Row: " << hoveredRow); // Add this debug statement

      */
      //repaint();
}