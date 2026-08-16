#include "ThemeList.h"
#include <JuceHeader.h>
#include "../SharedResources.h"
#include "../../EqProcessor.h"
#include "../../FactoryDefaultsState.h"
#include "UIElementsList.h"

ThemeList::ThemeList(SharedResources& resources)
    : sharedResources(resources), selectedRow(-1), newCol1Width(100), newCol2Width(100), newCol3Width(100) {
    DBG("ThemeList constructor called");

    // Initialize column widths
    newCol1Width = 100; // Default width for column 1
    newCol2Width = 100; // Default width for column 2
    newCol3Width = 100; // Default width for column 3

    loadPresetsFromXML();

    customHeader = std::make_unique<CustomHeader>(this, &sharedResources);
    addAndMakeVisible(customHeader.get());
    
    needsRepainting = true;
    listBox.setModel(this);
    listBox.setMultipleSelectionEnabled(false);
    listBox.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    listBox.updateContent();
    addAndMakeVisible(listBox);
    startTimer(50);  // Start a timer to defer focus grabbing
    listBox.setWantsKeyboardFocus(true);
    listBox.addKeyListener(this);
    listBox.setRepaintsOnMouseActivity(false);
    listBox.repaint();
    listBox.setVisible(true);  // This line is typically not necessary

    textEditor.addListener(this);

    customScrollBar = std::make_unique<CustomScrollBar>(listBox);
    addAndMakeVisible(*customScrollBar);

    juce::Colour menuScrollBarTrackColor1 = sharedResources.sharedColors.menuScrollBarTrackColor1;
    juce::Colour menuScrollBarThumbColor1 = sharedResources.sharedColors.menuScrollBarThumbColor1;
    juce::Colour menuScrollBarOutlineColor = sharedResources.sharedColors.menuScrollBarOutlineColor1;

    customScrollBar->setTrackBackgroundColour(menuScrollBarTrackColor1);  // Track color
    customScrollBar->setThumbBackgroundColour(menuScrollBarThumbColor1);  // Thumb color
    customScrollBar->setThumbOutlineColour(menuScrollBarOutlineColor);     // Thumb outline color
    customScrollBar->repaint();

    if (presets.isEmpty()) {
        DBG("No presets found. Adding default preset.");
        saveCurrentPreset("Default Preset");
        // It's important to update the content and repaint after adding a preset
        listBox.updateContent();
        listBox.repaint();
        selectedRow = 0;  // Set the first row as selected
    }

    // If there are presets but none is selected, select the default if it exists
    if (selectedRow == -1) {
        for (int i = 0; i < presetNames.size(); ++i) {
            if (presetNames[i] == "Default Preset") {
                selectedRow = i;
                break;
            }
        }
    }

    // Select Default Preset row for the list only — do NOT apply colours here.
    // Host setState / MainComponent restore session UI after construction; applying
    // Default here wiped project colours on every Ableton reopen.
    if (selectedRow < 0)
        selectedRow = 0;
    if (selectedRow >= 0 && selectedRow < getNumRows())
        listBox.selectRow (selectedRow, false, false);

    DBG("[ThemeList] Calling updateContent on listBox");
    listBox.updateContent();
    listBox.repaint();
}


ThemeList::~ThemeList() 
{
    listBox.setLookAndFeel(nullptr); // Reset LookAndFeel before destruction
    // Any other necessary cleanup code
}



int ThemeList::getNumRows() {
    DBG("Getting number of rows: " << presetNames.size());
    return presetNames.size();
}

void ThemeList::paint(juce::Graphics& g)
{
    float cornerSize = 14.0f;

    juce::Colour menuListBoxBackgroundGradientColor1 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor1;
    juce::Colour menuListBoxBackgroundGradientColor2 = sharedResources.sharedColors.menuListBoxBackgroundGradientColor2;
    juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;
    juce::Colour color1 = juce::Colour::fromRGB(60, 47, 39);
    juce::Colour color2 = juce::Colour::fromRGB(0, 0, 0);
    juce::Colour listBoxOutlineColor = juce::Colour::fromRGB(25, 30, 25);

    juce::ColourGradient gradient(
        menuListBoxBackgroundGradientColor1,
        juce::Point<float>(static_cast<float>(getWidth()) / 2.0f, static_cast<float>(getHeight() * - 1.5)),
        menuListBoxBackgroundGradientColor2,
        juce::Point<float>(static_cast<float>(getWidth()) / 2, static_cast<float>(getHeight() * 1.5)),
        false
    );

    g.setGradientFill(gradient);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(0), cornerSize);
}

void ThemeList::paintOverChildren(juce::Graphics& g)
{
    // Define the radius for the rounded corners
    float cornerSize = 14.0f;
    float cornerSize2 = 11.0f;

    // Create a path for the rounded rectangle (clipping region)
    juce::Path roundedRectPath;
    roundedRectPath.addRoundedRectangle(getLocalBounds().toFloat().reduced(4, 4), cornerSize);


    // Save the current graphics state
    g.saveState();

    // Set the clipping region to the path
    g.reduceClipRegion(shadowPath);

    // Call the base class method to paint child components
    // This will clip the child components to the rounded corners
    Component::paintOverChildren(g);

    // Restore the graphics state
    //g.restoreState();

    // Create a separate path for the outline
    juce::Path outlinePath;
    juce::Rectangle<float> outlineBounds = getLocalBounds().toFloat().reduced(0); // Reduce the size by 4 pixels
    outlinePath.addRoundedRectangle(outlineBounds, cornerSize2);

    // Draw the outline over all child components
    juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;
    g.setColour(menuThinBorderColor);
   // g.strokePath(outlinePath, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved));

    g.restoreState();
}


void ThemeList::paintListBoxItem(int rowNumber, juce::Graphics& g,
    int width, int height, bool rowIsSelected) {

    auto mousePosition = getMouseXYRelative();
    hoveredRow = listBox.getRowContainingPosition(mousePosition.x, mousePosition.y) - 1;

    //if (!needsRepainting) {
    //   return; // Skip painting if no new elements and no hover
    //}

    DBG("ThemeList::paintListBoxItem Called");

    //DBG("Painting Row: " << rowNumber << ", Hovered Row: " << hoveredRow); // Debug statement

    // Define constants for layout
    const int textIndent = 5;
    const int separatorThickness = 1; // Thickness of the separator lines
    const int outlineThickness = 1; // Thickness of the outline
    const int innerMargin = 1; // Inner margin for outline

    // Define colours
    juce::Colour outlineColour = juce::Colours::grey; // Colour for the outline
    juce::Colour textColour = sharedResources.sharedColors.menuListBoxTextColor1; // Text colour
    juce::Colour selectionColour = sharedResources.sharedColors.menuListBoxSelectionColor1; // Selection background colour

    // Set the font for the listbox items
    g.setFont(SharedResources::uiFont (12.0f));
   

 
    // Check if the mouse is hovering over this row
    if (hoveredRow == rowNumber && !rowIsSelected) {
        g.setColour(selectionColour.withAlpha(0.75f)); // Semi-transparent hover colour
    }
    else if (rowIsSelected) {
        g.setColour(selectionColour); // Solid colour for selected row
    }
    else {
        g.setColour(juce::Colours::transparentBlack); // Default background
    }
    g.fillRect(0, 0, width, height);

    int dynamicCol1Width = customHeader->getColumnWidths()[0];
    int dynamicCol2Width = customHeader->getColumnWidths()[1];
    int dynamicCol3Width = customHeader->getColumnWidths()[2];

    // Ensure dynamicCol3Width is not less than the minimum width
    const int minColWidth = 50; // Define the minimum column width
    if (dynamicCol3Width < minColWidth) {
        dynamicCol3Width = minColWidth;
    }

   
    // Draw vertical separators
    g.setColour(outlineColour.withAlpha(0.65f));
    g.fillRect(dynamicCol1Width - separatorThickness, 0, separatorThickness, height);
    g.fillRect(dynamicCol1Width + dynamicCol2Width - separatorThickness, 0, separatorThickness, height);

    // Draw the outline around each list box item
    g.drawRect(innerMargin, innerMargin, width - 2 * innerMargin, height - innerMargin, outlineThickness);
 
    juce::Rectangle<float> bounds(
        innerMargin, // X position
        innerMargin, // Y position
        width - 2 * innerMargin, // Width
        height - innerMargin * 2 // Height
    );

    juce::Path roundedRectPath;

    roundedRectPath.addRoundedRectangle(bounds, 2);

    innerShadow.render(g, roundedRectPath);

    if (rowNumber < presetNames.size()) {
        // Access the data directly from the arrays
        const juce::String& presetName = presetNames[rowNumber];
        const juce::Time& createdTime = themeCreatedTimes[rowNumber];
        const juce::Time& lastModifiedTime = themeModifiedTimes[rowNumber];

        // Format the timestamps with date and time
        juce::String formattedCreatedTime = createdTime.formatted("%m-%d-%Y %I:%M:%S %p");
        juce::String formattedLastModifiedTime = lastModifiedTime.formatted("%m-%d-%Y %I:%M:%S %p");

        // Debugging
        //DBG("Drawing row " << rowNumber << ": " << formattedCreatedTime << ", " << formattedLastModifiedTime);

        // Draw Preset Name column
        g.setColour(textColour);
        g.drawText(presetName, innerMargin + textIndent, 0, dynamicCol1Width - textIndent - separatorThickness, height, juce::Justification::centredLeft, true);

        // Draw Created column
        g.setColour(textColour);
        g.drawText(formattedCreatedTime, dynamicCol1Width + textIndent, 0, dynamicCol2Width - textIndent - separatorThickness, height, juce::Justification::centredLeft, true);

        // Draw Last Modified column
        g.setColour(textColour);
        g.drawText(formattedLastModifiedTime, dynamicCol1Width + dynamicCol2Width + textIndent, 0, dynamicCol3Width - textIndent - separatorThickness, height, juce::Justification::centredLeft, true);

       
    }
     needsRepainting = false;
}

void ThemeList::listBoxItemClicked(int row, const juce::MouseEvent& e)
{
    needsRepainting = true;

    if (e.getNumberOfClicks() == 2) {
        DBG("Double-click detected on row " << row);

        // Check if the row index is within the valid range
        if (row < 0 || row >= getNumRows()) {
            DBG("[Error] Row index out of range: " << row);
            return;
        }

        isTextEditorActive = true;
        editedRow = row;
        const juce::String presetName = getPresetName(row); // Ensure this is declared in the correct scope

        if (!presetName.isEmpty()) {
            // Calculate the bounds of the row manually
            int headerHeight = customHeader->getHeight();
            int rowYPosition = row * listBox.getRowHeight() + headerHeight;
            const juce::Rectangle<int> rowBounds(0, rowYPosition, listBox.getWidth(), listBox.getRowHeight()); // Ensure this is declared in the correct scope
                   
            // Example RGB colors
            juce::Colour backgroundColour = juce::Colour::fromRGB(10, 10, 10); // White
            juce::Colour textColour = juce::Colours::whitesmoke.withAlpha(0.85f);  // Black
            juce::Colour highlightColour = juce::Colour::fromRGB(220, 160, 150);      // Blue
            juce::Colour highlightedTextColour = juce::Colour::fromRGB(50, 50, 0); // Yellow
            juce::Colour outlineColour = juce::Colour::fromRGB(128, 128, 128);    // Grey
            juce::Colour focusedOutlineColour = juce::Colour::fromRGB(80, 80, 80); // Green
            juce::Colour shadowColour = juce::Colour::fromRGB(20, 20, 20);     // Light Grey

            textEditor.setColour(juce::TextEditor::backgroundColourId, backgroundColour);
            textEditor.setColour(juce::TextEditor::textColourId, textColour);
            textEditor.setColour(juce::TextEditor::highlightColourId, highlightColour);
            textEditor.setColour(juce::TextEditor::highlightedTextColourId, highlightedTextColour);
            textEditor.setColour(juce::TextEditor::outlineColourId, outlineColour);
            textEditor.setColour(juce::TextEditor::focusedOutlineColourId, focusedOutlineColour);
            textEditor.setColour(juce::TextEditor::shadowColourId, shadowColour);

            textEditor.setFont(SharedResources::uiFont (12.0f, true));

            DBG("Adding and making text editor visible");
            addAndMakeVisible(&textEditor); // No need for the isParentComponent check

            textEditor.setBounds(rowBounds);
            textEditor.setText(presetName, false);

            // Check bounds before making visible
            if (textEditor.getBounds().isEmpty() || textEditor.getBounds().getWidth() * textEditor.getBounds().getHeight() == 0) {
                DBG("[Error] Text editor bounds are invalid: " << textEditor.getBounds().toString());
                return;
            }

            textEditor.setVisible(true);
            textEditor.grabKeyboardFocus();
            DBG("Text editor is now active for editing.");
        }
        else {
            DBG("[Warning] Preset name is empty for row " << row);
        }

        repaint();
    }



    else
    {
        // Single-click event, apply the preset if it's not already selected
        if (!isTextEditorActive && row != selectedRow)
        {
            applyPreset(row, false);
        }
    }

}




juce::ValueTree ThemeList::capturePluginState() const
{
    if (processor == nullptr)
        return {};

    juce::MemoryBlock block;
    processor->getStateInformation (block);

    if (auto xml = juce::AudioProcessor::getXmlFromBinary (block.getData(), (int) block.getSize()))
        return juce::ValueTree::fromXml (*xml);

    return {};
}

void ThemeList::applyPluginState (const juce::ValueTree& state)
{
    if (processor == nullptr || ! state.isValid())
        return;

    if (auto xml = state.createXml())
    {
        juce::MemoryBlock block;
        juce::AudioProcessor::copyXmlToBinary (*xml, block);
        processor->setStateInformation (block.getData(), (int) block.getSize());
    }
}

void ThemeList::persistPresetsToXml()
{
    if (presets.isEmpty())
        return;

    presets.getReference (0).savePresetsToXML (presets, presetNames, themeCreatedTimes, themeModifiedTimes);
}

void ThemeList::saveCurrentPreset (const juce::String& name)
{
    addPreset (name, true);
}

void ThemeList::addPreset (const juce::String& name, bool appendDateSuffix)
{
    needsRepainting = true;

    const juce::Time currentTime = juce::Time::getCurrentTime();

    Theme newPreset (sharedResources.sharedColors);
    newPreset.setCreated (currentTime);
    newPreset.setModified (currentTime);
    // Global UI: colours + modular snapshot. Never embed EQ DSP.
    newPreset.clearPluginState();
    newPreset.setGlobalUi (captureCurrentGlobalUi());

    juce::String uniqueName = name.trim();
    if (uniqueName.isEmpty())
        uniqueName = "Global UI Preset";

    if (appendDateSuffix)
        uniqueName = uniqueName + "_" + currentTime.formatted ("%m-%d-%Y");

    // Avoid colliding with an existing name (including Default).
    if (presetNames.contains (uniqueName))
    {
        int suffix = 2;
        const juce::String base = uniqueName;
        while (presetNames.contains (base + " (" + juce::String (suffix) + ")"))
            ++suffix;
        uniqueName = base + " (" + juce::String (suffix) + ")";
    }

    presets.add (newPreset);
    presetNames.add (uniqueName);
    themeCreatedTimes.add (currentTime);
    themeModifiedTimes.add (currentTime);

    persistPresetsToXml();

    listBox.updateContent();
    listBox.repaint();

    listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
}

juce::ValueTree ThemeList::captureCurrentGlobalUi() const
{
    if (captureGlobalUi)
        return captureGlobalUi();
    return {};
}

void ThemeList::applyPreset (int index, bool shouldApplyPluginState, bool shouldApplyGlobalUiModules)
{
    needsRepainting = true;

    if (index < 0 || index >= presets.size())
        return;

    const auto selectedTheme = presets[index];

    // Theme snapshots copy full SharedColors (including dice scope flags).
    // Applying a theme must restore palette colours only — never disable
    // Faceplate/Graph/Menu randomize, or plugin buttons/menus stop following the dice.
    auto& live = sharedResources.sharedColors;
    const bool keepFace = live.randomizeFaceplateMod;
    const bool keepGraph = live.randomizeGraphModule;
    const bool keepMenu = live.randomizeMenuModule;
    const bool keepCursorInfo = live.randomizeGraphCursorInfo;
    const bool keepRampFft = live.randomizeRampFftBars;
    const bool keepRampSpec = live.randomizeRampSpectrogram;
    const bool keepRampSpec3D = live.randomizeRampSpectrogram3D;
    const bool keepRampFill = live.randomizeRampSpectrumFill;
    const bool keepRampCurve = live.randomizeRampSpectrumCurve;
    const bool keepRampEqCurve = live.randomizeRampEqCurve;
    const bool keepRampPreFill = live.randomizeRampSpectrumPreFill;
    const bool keepRampPreCurve = live.randomizeRampSpectrumPreCurve;
    const bool keepRampHoldFill = live.randomizeRampSpectrumHoldFill;
    const bool keepRampHoldCurve = live.randomizeRampSpectrumHoldCurve;
    const bool keepRampEqSumFill = live.randomizeRampEqSumFill;
    const bool keepRampEqBandCurve = live.randomizeRampEqBandCurve;
    const bool keepRampEqBandFill = live.randomizeRampEqBandFill;
    const bool keepRampMeters = live.randomizeRampLevelMeters;
    const bool keepOrdered = live.orderedRampGradation;
    const bool keepLegible = live.enforceLegibleText;
    const float keepContrast = live.textContrastAmount;
    const float keepOptionBoxOpacity = live.optionBoxOpacity;
    const float keepButtonCornerRadius = live.buttonCornerRadius;
    const float keepMenuPopupRadius = live.menuPopupCornerRadius;
    const bool keepMenuPopupOutline = live.menuPopupOutline;
    const bool keepButtonGlow = live.buttonGlowEnabled;
    const bool keepButtonGlowHover = live.buttonGlowOnlyOnHover;
    const juce::String keepUiFontName = live.uiFontName;
    const bool keepUiFontBold = live.uiFontBold;
    const float keepCursorInfoSize = live.graphCursorInfoFontSize;
    const bool keepGraphBandMinSatEn = live.graphBandRandomMinSatEnabled;
    const float keepGraphBandMinSat = live.graphBandRandomMinSaturation;
    const bool keepH = live.randomizeHue, keepS = live.randomizeSaturation;
    const bool keepB = live.randomizeBrightness, keepA = live.randomizeAlpha;
    const float hueL = live.hueLowerLimit, hueU = live.hueUpperLimit;
    const float satL = live.saturationLowerLimit, satU = live.saturationUpperLimit;
    const float briL = live.brightnessLowerLimit, briU = live.brightnessUpperLimit;

    live = selectedTheme.getColors();

    live.randomizeFaceplateMod = keepFace;
    live.randomizeGraphModule = keepGraph;
    live.randomizeMenuModule = keepMenu;
    live.randomizeGraphCursorInfo = keepCursorInfo;
    live.randomizeRampFftBars = keepRampFft;
    live.randomizeRampSpectrogram = keepRampSpec;
    live.randomizeRampSpectrogram3D = keepRampSpec3D;
    live.randomizeRampSpectrumFill = keepRampFill;
    live.randomizeRampSpectrumCurve = keepRampCurve;
    live.randomizeRampEqCurve = keepRampEqCurve;
    live.randomizeRampSpectrumPreFill = keepRampPreFill;
    live.randomizeRampSpectrumPreCurve = keepRampPreCurve;
    live.randomizeRampSpectrumHoldFill = keepRampHoldFill;
    live.randomizeRampSpectrumHoldCurve = keepRampHoldCurve;
    live.randomizeRampEqSumFill = keepRampEqSumFill;
    live.randomizeRampEqBandCurve = keepRampEqBandCurve;
    live.randomizeRampEqBandFill = keepRampEqBandFill;
    live.randomizeRampLevelMeters = keepRampMeters;
    live.orderedRampGradation = keepOrdered;
    live.enforceLegibleText = keepLegible;
    live.textContrastAmount = keepContrast;
    live.optionBoxOpacity = keepOptionBoxOpacity;
    live.buttonCornerRadius = keepButtonCornerRadius;
    live.menuPopupCornerRadius = keepMenuPopupRadius;
    live.menuPopupOutline = keepMenuPopupOutline;
    live.buttonGlowEnabled = keepButtonGlow;
    live.buttonGlowOnlyOnHover = keepButtonGlowHover;
    live.uiFontName = keepUiFontName;
    live.uiFontBold = keepUiFontBold;
    live.graphCursorInfoFontSize = keepCursorInfoSize;
    live.graphBandRandomMinSatEnabled = keepGraphBandMinSatEn;
    live.graphBandRandomMinSaturation = keepGraphBandMinSat;
    live.randomizeHue = keepH;
    live.randomizeSaturation = keepS;
    live.randomizeBrightness = keepB;
    live.randomizeAlpha = keepA;
    live.hueLowerLimit = hueL;
    live.hueUpperLimit = hueU;
    live.saturationLowerLimit = satL;
    live.saturationUpperLimit = satU;
    live.brightnessLowerLimit = briL;
    live.brightnessUpperLimit = briU;

    // Legible text stays global (default on) — re-enforce after palette swap.
    if (live.enforceLegibleText)
        live.enforceLegibleTextContrast();

    listBox.selectRow (index);
    selectedRow = index;

    if (shouldApplyGlobalUiModules && selectedTheme.hasGlobalUi() && applyGlobalUi)
        applyGlobalUi (selectedTheme.getGlobalUi());

    // Legacy themes only — chrome UI list passes false for plugin state.
    if (shouldApplyPluginState && selectedTheme.hasPluginState())
        applyPluginState (selectedTheme.getPluginState());

    listeners.call ([&] (Listener& listener)
    {
        listener.onPresetApplied (selectedTheme);
        listener.onPresetListChanged();
    });

    if (uiElementsList != nullptr)
    {
        uiElementsList->getListBox().updateContent();
        uiElementsList->getListBox().repaint();
    }
}



void ThemeList::addListener(Listener* newListener) {
    listeners.add(newListener);
}

void ThemeList::removeListener(Listener* listener) {
    listeners.remove(listener);
}

void ThemeList::loadPresetsFromXML() {
    needsRepainting = true;

    DBG("Loading presets from XML...");

    // Clear any existing presets to start fresh
    presets.clear();
    presetNames.clear();
    themeCreatedTimes.clear();
    themeModifiedTimes.clear();

    // Create and insert the default preset at index 0
    createDefaultPreset();

    // Specify the desired path to load the presets file
    juce::File presetDirectory = juce::File::getSpecialLocation(juce::File::userDocumentsDirectory)
        .getChildFile("Decksounds")
        .getChildFile("ParametricEq")
        .getChildFile("Themes");

    DBG("Looking for presets XML at: " + presetDirectory.getFullPathName());

    if (!presetDirectory.exists()) {
        DBG("Presets directory NOT found.");
        return; // Exit if the directory isn't found
    }

    juce::File presetFile = presetDirectory.getChildFile("presets.xml");

    if (!presetFile.existsAsFile()) {
        DBG("Presets XML file not found.");
        return; // Exit if the presets file isn't found
    }

    auto xml = juce::parseXML(presetFile);
    if (xml == nullptr) {
        DBG("Failed to parse XML file.");
        return; // Exit if the XML file isn't parsed successfully
    }

    DBG("XML file parsed successfully.");

    if (!xml->hasTagName("Presets")) {
        DBG("XML is not null but root tag is not 'Presets'.");
        return; // Exit if the root tag is not 'Presets'
    }

    DBG("Root tag is 'Presets'.");
    for (auto* themeElement : xml->getChildIterator()) {
        auto presetName = themeElement->getStringAttribute("name");
        if (presetName.isEmpty())
            presetName = themeElement->getStringAttribute ("Name");

        if (presetName.isNotEmpty()) {
            // Avoid overwriting the default preset
            if (presetName != "Default") {
                DBG("Loaded preset: " + presetName);
                Theme theme;
                theme.fromXml(*themeElement);
                presets.add(theme);
                presetNames.add(presetName);

                // Prefer timestamps embedded in the Theme XML
                themeCreatedTimes.add (theme.getCreated());
                themeModifiedTimes.add (theme.getModified());
            }
            else {
                DBG("Skipped loading 'Default' preset from XML to preserve the hardcoded default.");
            }
        }
        else {
            DBG("Skipped loading preset with empty name.");
        }
    }

    // After loading all other presets, ensure the default preset is not overwritten
    for (int i = 1; i < presetNames.size(); ++i) {
        if (presetNames[i] == "Default") {
            // Handle the case where a loaded preset has the name "Default"
            DBG("A preset with the name 'Default' was loaded from XML, it will be renamed.");
            presetNames.set(i, "Default (1)"); // Rename to avoid confusion
        }
    }

    DBG("Total presets loaded: " + juce::String(presets.size()));

    // Ensure updateContent is called after loading presets
    DBG("[loadPresetsFromXML] Calling updateContent on listBox");
    listBox.updateContent();
    listBox.repaint();
}



void ThemeList::createDefaultPreset() {
    needsRepainting = true;
    Theme defaultTheme (SharedResources::getDefaultTheme());
    const auto now = juce::Time::getCurrentTime();
    defaultTheme.setCreated (now);
    defaultTheme.setModified (now);
    // Factory DSP/UI state from user preset "new default".
    defaultTheme.setPluginState (FactoryDefaults::createPluginState());

    presets.insert (0, defaultTheme);
    presetNames.insert (0, "Default");
    themeCreatedTimes.insert (0, now);
    themeModifiedTimes.insert (0, now);
}

// Assuming the default preset is always at index 0
const int defaultPresetIndex = 0;

void ThemeList::deletePreset(int index) {
    needsRepainting = true;

    if (index == defaultPresetIndex) {
        DBG("Attempted to delete the default preset, operation not allowed.");
        return;
    }

    if (index > 0 && index < presets.size()) {
        presets.remove (index);
        presetNames.remove (index);

        if (index < themeCreatedTimes.size())
            themeCreatedTimes.remove (index);
        if (index < themeModifiedTimes.size())
            themeModifiedTimes.remove (index);

        persistPresetsToXml();

        listBox.updateContent();
        listBox.repaint();

        if (selectedRow == index)
            selectedRow = -1;
        else if (selectedRow > index)
            --selectedRow;

        listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
    }
}

void ThemeList::duplicatePreset (int index)
{
    needsRepainting = true;

    if (index < 0 || index >= presets.size() || index >= presetNames.size())
        return;

    const juce::Time currentTime = juce::Time::getCurrentTime();

    Theme copy = presets[index];
    copy.setCreated (currentTime);
    copy.setModified (currentTime);
    copy.clearPluginState();

    juce::String base = presetNames[index].trim();
    if (base.isEmpty())
        base = "Theme";
    if (base.equalsIgnoreCase ("Default"))
        base = "Default copy";
    else if (! base.endsWithIgnoreCase (" copy"))
        base = base + " copy";

    juce::String uniqueName = base;
    if (presetNames.contains (uniqueName))
    {
        int suffix = 2;
        while (presetNames.contains (base + " (" + juce::String (suffix) + ")"))
            ++suffix;
        uniqueName = base + " (" + juce::String (suffix) + ")";
    }

    presets.add (copy);
    presetNames.add (uniqueName);
    themeCreatedTimes.add (currentTime);
    themeModifiedTimes.add (currentTime);

    persistPresetsToXml();

    listBox.updateContent();
    listBox.repaint();

    listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
}

void ThemeList::overwritePreset (int index, const juce::String& name)
{
    needsRepainting = true;

    if (index == defaultPresetIndex)
        return;

    if (index <= 0 || index >= presets.size())
        return;

    const juce::Time currentTime = juce::Time::getCurrentTime();

    Theme newPreset (sharedResources.sharedColors);
    newPreset.setCreated (index < themeCreatedTimes.size() ? themeCreatedTimes[index] : currentTime);
    newPreset.setModified (currentTime);
    // Global UI: colours + modular snapshot (not EQ).
    newPreset.clearPluginState();
    newPreset.setGlobalUi (captureCurrentGlobalUi());

    presets.set (index, newPreset);

    juce::String finalName = name.trim();
    if (finalName.isEmpty())
        finalName = presetNames[index];

    // Keep Default unique.
    if (finalName.equalsIgnoreCase ("Default"))
        finalName = presetNames[index];

    presetNames.set (index, finalName);

    if (index < themeModifiedTimes.size())
        themeModifiedTimes.set (index, currentTime);
    else
        themeModifiedTimes.add (currentTime);

    persistPresetsToXml();

    listBox.updateContent();
    listBox.repaint();

    listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
}

void ThemeList::renamePreset (int index, const juce::String& newName)
{
    if (index <= 0 || index >= presetNames.size())
        return;

    auto trimmed = newName.trim();
    if (trimmed.isEmpty() || trimmed.equalsIgnoreCase ("Default"))
        return;

    if (presetNames[index] == trimmed)
        return;

    // Avoid duplicate names.
    const int existing = presetNames.indexOf (trimmed);
    if (existing >= 0 && existing != index)
    {
        int suffix = 2;
        const auto base = trimmed;
        while (presetNames.contains (base + " (" + juce::String (suffix) + ")"))
            ++suffix;
        trimmed = base + " (" + juce::String (suffix) + ")";
    }

    presetNames.set (index, trimmed);

    if (index < themeModifiedTimes.size())
        themeModifiedTimes.set (index, juce::Time::getCurrentTime());

    persistPresetsToXml();
    listBox.updateContent();
    listBox.repaint();
    listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
}

void ThemeList::saveOrUpdateWithName (const juce::String& name)
{
    auto trimmed = name.trim();
    if (trimmed.isEmpty())
        trimmed = "Preset";

    // Never overwrite the built-in Default entry.
    if (trimmed.equalsIgnoreCase ("Default"))
        trimmed = "Preset";

    // Prefer updating the currently selected user preset.
    if (selectedRow > 0 && selectedRow < presets.size())
    {
        overwritePreset (selectedRow, trimmed);
        return;
    }

    // Otherwise overwrite an existing non-default with the same name, or create.
    const int existing = findPresetIndexByName (trimmed);
    if (existing > 0)
    {
        overwritePreset (existing, trimmed);
        listBox.selectRow (existing);
        selectedRow = existing;
        listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
        return;
    }

    addPreset (trimmed, false);
    const int newIndex = presets.size() - 1;
    if (newIndex >= 0)
    {
        listBox.selectRow (newIndex);
        selectedRow = newIndex;
        listeners.call ([] (Listener& listener) { listener.onPresetListChanged(); });
    }
}

int ThemeList::findPresetIndexByName (const juce::String& name) const
{
    return presetNames.indexOf (name);
}

int ThemeList::findPresetIndexByNameIgnoreCase (const juce::String& name) const
{
    const auto trimmed = name.trim();
    if (trimmed.isEmpty())
        return -1;

    for (int i = 0; i < presetNames.size(); ++i)
        if (presetNames[i].equalsIgnoreCase (trimmed))
            return i;

    return -1;
}

void ThemeList::resized()
{
    needsRepainting = true;

    const int headerHeightLocal = 20;
    const int scrollBarWidth = 11;

    customHeader->setBounds (0, 0, getWidth(), headerHeightLocal);

    juce::Rectangle<int> listBoxBounds (0, headerHeightLocal, getWidth() - scrollBarWidth, getHeight() - headerHeightLocal);
    listBox.setBounds (listBoxBounds);
    listBox.updateContent();

    if (editingActive)
    {
        editingActive = false;
        repaint();
    }

    customScrollBar->setBounds (getWidth() - scrollBarWidth, headerHeightLocal, scrollBarWidth, listBoxBounds.getHeight());

    shadowPath.clear();
    shadowPath.addRoundedRectangle (getLocalBounds().toFloat(), 5.0f);
}

int ThemeList::getSelectedRow() const {
    return selectedRow; // Return the index of the selected row/preset
}

juce::String ThemeList::getPresetName(int index) const {
    if (index >= 0 && index < presetNames.size()) {
        return presetNames[index]; // Return the name of the preset at the given index
    }
    return {}; // Return an empty string if the index is out of range
}

juce::String ThemeList::getSelectedPresetName() const
{
    return getPresetName (selectedRow);
}

void ThemeList::textEditorReturnKeyPressed(juce::TextEditor& editor)
{
    needsRepainting = true;

    int editedRowIndex = -1;
    for (int i = 0; i < presetNames.size(); ++i)
    {
        if (editedRow == i)
        {
            // Update the preset name with the edited text
            presetNames.set(i, editor.getText());
            editedRowIndex = i;
            break;
        }
    }

    isTextEditorActive = false;

    // Save the updated preset names to XML
    persistPresetsToXml();

    // Reset the editedRow to -1 and repaint to exit editing mode
    editedRow = -1;
    textEditor.setVisible(false); // Hide the text editor
    repaint();

    listeners.call([] (Listener& listener) { listener.onPresetListChanged(); });
}

void ThemeList::setUIElementsList(UIElementsList* list)
{
    needsRepainting = true;
    uiElementsList = list;
}

void ThemeList::listBoxDataChanged()
{
    needsRepainting = true;
    customScrollBar->updateThumbPosition();
}

void ThemeList::updateScrollBarColors(const juce::Colour& trackColor, const juce::Colour& thumbColor, const juce::Colour& outlineColor)
{
    needsRepainting = true;

    if (customScrollBar)
    {
        customScrollBar->setTrackBackgroundColour(trackColor);
        customScrollBar->setThumbBackgroundColour(thumbColor);
        customScrollBar->setThumbOutlineColour(outlineColor);
        customScrollBar->repaint();
    }
}

void ThemeList::mouseDrag(const juce::MouseEvent& event) {
    // Handle mouse drag for column resizing here
    // Call the corresponding methods in CustomHeader
    customHeader->mouseDrag(event);


}

void ThemeList::mouseMove(const juce::MouseEvent& event) {
  
    auto mousePos = event.getPosition();
    cursorOverComponent = getLocalBounds().contains(mousePos) ||
        customHeader->getBounds().contains(mousePos) ||
        listBox.getBounds().contains(mousePos);
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



bool ThemeList::keyPressed(const juce::KeyPress& key, Component* originatingComponent) {
    needsRepainting = true;

    DBG("Key Pressed: " + key.getTextDescription());

    int maxRow = getNumRows() - 1;
    if (key == juce::KeyPress::upKey && selectedRow > 0) {
        selectedRow--;
        listBox.selectRow(selectedRow, false, true); // deselectOthers, notify
        applyPreset(selectedRow); // Apply the preset immediately
        return true;
    }
    else if (key == juce::KeyPress::downKey && selectedRow < maxRow) {
        selectedRow++;
        listBox.selectRow(selectedRow, false, true); // deselectOthers, notify
        applyPreset(selectedRow); // Apply the preset immediately
        return true;
    }
    else if (key == juce::KeyPress::returnKey) {
        applyPreset(selectedRow); // Apply the preset for the Enter key
        return true;
    }

    return false;
}





void ThemeList::timerCallback() {
    stopTimer(); // Stop the timer as we only need to do this once
    if (isShowing()) {
        listBox.grabKeyboardFocus();
    }
}