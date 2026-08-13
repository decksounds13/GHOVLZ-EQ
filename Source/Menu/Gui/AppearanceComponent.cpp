#include "AppearanceComponent.h"
#include "../ThemeColorRegistry.h"
#include <algorithm>
#include <cstdint>
#include "../../TextButtonLookAndFeel.h"
#include "../../ComboBoxLookAndFeel.h"
#include "QuadPickerOverlayComponent.h"
#include "../Menu.h"


namespace
{
    juce::Font typefaceForCatalogueName (const juce::String& name, float height)
    {
        return SharedColors::makeNamedUiFont (name, height, false);
    }

    constexpr int kFontRowH = 22;

    /** Desktop list so the catalogue can be taller than the Settings panel. */
    class FontCatalogueWindow : public juce::Component,
                                public juce::ListBoxModel
    {
    public:
        FontCatalogueWindow (juce::StringArray fontsIn,
                             int selectedIdIn,
                             juce::Component* comboIn,
                             std::function<void (const juce::String&)> onHoverIn,
                             std::function<void (int)> onPickIn,
                             std::function<void()> onClickAwayIn)
            : fonts (std::move (fontsIn)),
              selectedId (selectedIdIn),
              combo (comboIn),
              onHover (std::move (onHoverIn)),
              onPick (std::move (onPickIn)),
              onClickAway (std::move (onClickAwayIn))
        {
            list.setModel (this);
            list.setRowHeight (kFontRowH);
            list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
            list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
            list.setMouseMoveSelectsRows (true);
            addAndMakeVisible (list);
            if (auto* vp = list.getViewport())
            {
                vp->setScrollBarsShown (true, false);
                vp->setScrollBarThickness (11);
            }
            list.updateContent();
            const int sel = juce::jlimit (0, juce::jmax (0, fonts.size() - 1), selectedId - 1);
            list.selectRow (sel);
            list.scrollToEnsureRowIsOnscreen (sel);

            juce::Desktop::getInstance().addGlobalMouseListener (this);
            juce::Timer::callAfterDelay (80, [safe = juce::Component::SafePointer<FontCatalogueWindow> (this)]
            {
                if (safe != nullptr)
                    safe->armed = true;
            });
        }

        ~FontCatalogueWindow() override
        {
            juce::Desktop::getInstance().removeGlobalMouseListener (this);
            list.setModel (nullptr);
        }

        int getNumRows() override { return fonts.size(); }

        void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
        {
            if (! juce::isPositiveAndBelow (row, fonts.size()))
                return;
            auto bounds = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height);
            if (rowIsSelected)
            {
                ComboBoxLookAndFeel::fillMenuGradient (g, bounds.reduced (1.0f, 1.0f),
                                                       PluginMenuTheme::highlight(),
                                                       juce::jmin (3.0f, PluginMenuTheme::popupCorner() * 0.75f));
                g.setColour (PluginMenuTheme::textOnHighlight());
            }
            else
            {
                g.setColour (PluginMenuTheme::text());
            }
            if (row + 1 == selectedId)
            {
                auto tick = bounds.removeFromLeft (16.0f).reduced (3.0f, 5.0f);
                juce::Path p;
                p.startNewSubPath (tick.getX(), tick.getCentreY());
                p.lineTo (tick.getX() + tick.getWidth() * 0.35f, tick.getBottom());
                p.lineTo (tick.getRight(), tick.getY());
                g.strokePath (p, juce::PathStrokeType (1.6f));
            }
            else
            {
                bounds.removeFromLeft (16.0f);
            }
            g.setFont (typefaceForCatalogueName (fonts[row], 13.0f));
            g.drawText (fonts[row], bounds.toNearestInt(), juce::Justification::centredLeft, false);
        }

        void listBoxItemClicked (int row, const juce::MouseEvent&) override
        {
            if (! juce::isPositiveAndBelow (row, fonts.size()) || onPick == nullptr)
                return;
            const int id = row + 1;
            // Leave the ListBox click stack before the window is torn down / theme applied.
            juce::MessageManager::callAsync ([fn = onPick, id] { fn (id); });
        }

        void selectedRowsChanged (int lastRow) override
        {
            if (juce::isPositiveAndBelow (lastRow, fonts.size()) && onHover != nullptr)
                onHover (fonts[lastRow]);
        }

        void paint (juce::Graphics& g) override
        {
            const float corner = PluginMenuTheme::popupCorner();
            ComboBoxLookAndFeel::fillMenuGradient (g, getLocalBounds().toFloat(),
                                                   PluginMenuTheme::background(), corner);
            if (PluginMenuTheme::popupDrawOutline())
            {
                g.setColour (PluginMenuTheme::outline().withAlpha (0.85f));
                if (corner > 0.5f)
                    g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), corner, 1.0f);
                else
                    g.drawRect (getLocalBounds().toFloat().reduced (0.5f), 1.0f);
            }
        }

        void resized() override
        {
            list.setBounds (getLocalBounds().reduced (4));
            list.updateContent();
        }

        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& wheel) override
        {
            if (auto* vp = list.getViewport())
            {
                const float rowH = (float) juce::jmax (18, list.getRowHeight());
                float axis = std::abs (wheel.deltaX) > std::abs (wheel.deltaY) ? wheel.deltaX : wheel.deltaY;
                if (wheel.isReversed)
                    axis = -axis;
                const auto pos = vp->getViewPosition();
                vp->setViewPosition (pos.x, pos.y + juce::roundToInt (-axis * rowH * (wheel.isSmooth ? 1.0f : 3.5f)));
            }
        }

    private:
        void mouseDown (const juce::MouseEvent& e) override
        {
            if (! armed)
                return;
            auto* c = e.eventComponent;
            if (c == nullptr)
                return;
            if (c == this || isParentOf (c))
                return;
            if (combo != nullptr && (c == combo || combo->isParentOf (c)))
                return;
            if (onClickAway != nullptr)
                onClickAway();
        }

        juce::StringArray fonts;
        int selectedId = 1;
        juce::Component* combo = nullptr;
        juce::ListBox list;
        std::function<void (const juce::String&)> onHover;
        std::function<void (int)> onPick;
        std::function<void()> onClickAway;
        bool armed = false;
    };
}

AppearanceComponent::AppearanceComponent(SharedResources& resources, juce::AudioProcessorValueTreeState& state)
	: sharedResources(resources),  // Initialize the member variable
	uiElementsList(resources),
	themeList(resources),
	quadPicker(resources),
	hueSelector(resources),
	colorSwatch(*this),
	textButtonLookAndFeel(14.0f),
	overlayComponent(quadPicker, brightnessRangeSlider, saturationRangeSlider),
	overlayComponent2(hueSelector, hueRangeSlider),
	chromeSection (resources, "appearance.chrome", "Chrome", false)
{
	//juce::Colour menuThinBorderColor = juce::Colours::transparentBlack;
	initializeComponents();  // Set up the callbacks
	hueSelector.addListener(&quadPicker);
	uiElementsList.addListener(this);

	setupLabels();
	setRangeSliderColors();
	setSliderLookAndFeels();
	setButtonLookAndFeels();

	// Apply theme colors to textButtonLookAndFeel
	textButtonLookAndFeel.applyThemeColors(
	sharedResources.sharedColors.menuButtonGradientColor1,
	sharedResources.sharedColors.menuButtonGradientColor2,
	sharedResources.sharedColors.menuThinBorderColor,
	sharedResources.sharedColors.menuButtonTextColor1);

	// Apply theme colors to textButtonLookAndFeel2
	textButtonLookAndFeel2.applyThemeColors(
	sharedResources.sharedColors.menuButtonGradientColor1,
	sharedResources.sharedColors.menuButtonGradientColor2,
	sharedResources.sharedColors.menuThinBorderColor,
	sharedResources.sharedColors.menuLabelTextColor1);

	// Apply theme colors to customTwoValueSliderLookAndFeel
	customTwoValueSliderLookAndFeel.applyThemeColors(
		sharedResources.sharedColors.menuScrollBarTrackColor1,
		sharedResources.sharedColors.menuSliderFillColor,
		sharedResources.sharedColors.menuBackgroundGradientColor1);

	// Link the ThemeList to the UIElementsList
	themeList.setUIElementsList(&uiElementsList);
	themeList.addListener(this);

	// Add buttons to buttonsUsingCustomLookAndFeel
	buttonsUsingCustomLookAndFeel.add(&newPresetButton);
	buttonsUsingCustomLookAndFeel.add(&deletePresetButton);
	buttonsUsingCustomLookAndFeel.add(&overwritePresetButton);
	buttonsUsingCustomLookAndFeel.add(&randomizeColorsButton);
	buttonsUsingCustomLookAndFeel.add(&randomizeSelectedColorsButton);

	// Add buttons to buttonsUsingCustomLookAndFeel2
	buttonsUsingCustomLookAndFeel2.add(&randomizeHueToggleButton);
	buttonsUsingCustomLookAndFeel2.add(&randomizeSaturationToggleButton);
	buttonsUsingCustomLookAndFeel2.add(&randomizeBrightnessToggleButton);
	buttonsUsingCustomLookAndFeel2.add(&randomizeAlphaToggleButton);

	// Add sliders to UsingCustomLookAndFeel
	slidersUsingCustomLookAndFeel.add(&hueRangeSlider);
	slidersUsingCustomLookAndFeel.add(&saturationRangeSlider);
	slidersUsingCustomLookAndFeel.add(&hueRangeSlider);
	
	customTwoValueSliderLookAndFeel.setThumbStyle(CustomTwoValueSliderLookAndFeel::Round);

	// Add UI elements to the main component and make them visible
	addAndMakeVisible(hueSelector);
	addAndMakeVisible(quadPicker);
	addAndMakeVisible(colorSwatch);
	addAndMakeVisible(colorValuesInput);
	addAndMakeVisible(uiElementsList);
	addAndMakeVisible(themeList);
	addAndMakeVisible(overlayComponent);
	addAndMakeVisible(overlayComponent2);
	overlayComponent.setInterceptsMouseClicks(false, false);
	overlayComponent2.setInterceptsMouseClicks(false, false);

	saturationRangeSlider.setSliderStyle(juce::Slider::TwoValueHorizontal);
	saturationRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
	saturationRangeSlider.setRange(0.0f, 1.0f, 0.01f);  
	saturationRangeSlider.setMinValue(0.05f); 
	saturationRangeSlider.setMaxValue(0.3f);
	saturationRangeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
	saturationRangeSlider.setSliderSnapsToMousePosition(true);
	saturationRangeSlider.setMouseDragSensitivity(1);
	saturationRangeSlider.addListener(this);
	addAndMakeVisible(saturationRangeSlider);
	saturationRangeSlider.repaint();

	hueRangeSlider.setSliderStyle(juce::Slider::TwoValueVertical);
	hueRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
	hueRangeSlider.setRange(0.0f, 1.0f, 0.01f);
	hueRangeSlider.setMinValue(0.2f);
	hueRangeSlider.setMaxValue(0.8f);
	hueRangeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
	hueRangeSlider.setSliderSnapsToMousePosition(true);
	hueRangeSlider.setMouseDragSensitivity(1);
	addAndMakeVisible(hueRangeSlider);
	hueRangeSlider.repaint();

	brightnessRangeSlider.setSliderStyle(juce::Slider::TwoValueVertical);
	brightnessRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
	brightnessRangeSlider.setRange(0.0f, 1.0f, 0.001f);
	brightnessRangeSlider.setMinValue(0.05f);
	brightnessRangeSlider.setMaxValue(1.0f);
	brightnessRangeSlider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
	brightnessRangeSlider.setSliderSnapsToMousePosition(true);
	brightnessRangeSlider.setMouseDragSensitivity(1);
	brightnessRangeSlider.addListener(this);
	addAndMakeVisible(brightnessRangeSlider);
	brightnessRangeSlider.repaint();
	//DBG("Brightness Range Slider added with range: Start = " << brightnessRangeSlider.getRange().getStart() << ", End = " << brightnessRangeSlider.getRange().getEnd());
	//DBG("Min Value: " << brightnessRangeSlider.getMinValue() << ", Max Value: " << brightnessRangeSlider.getMaxValue());

	juce::Colour sliderTrackColor = juce::Colour::fromRGBA(30, 20, 10, 255);
	juce::Colour sliderThumbColor = sharedResources.sharedColors.menuScrollBarThumbColor1;
	juce::Colour arrowOutlineColor = sharedResources.sharedColors.menuScrollBarOutlineColor1;

	// Using the correct names from sharedResources to set button colors
	juce::Colour menuThinBorderColor = sharedResources.sharedColors.menuThinBorderColor;
	juce::Colour menuButtonGradientColor1 = sharedResources.sharedColors.menuButtonGradientColor1;
	juce::Colour menuButtonGradientColor2 = sharedResources.sharedColors.menuButtonGradientColor2;
	juce::Colour menuButtonTextColor1 = sharedResources.sharedColors.menuButtonTextColor1;
	juce::Colour menuButtonLabelColor1 = sharedResources.sharedColors.menuLabelTextColor1;

	//DBG("[AppearanceComponent] Setting up custom LookAndFeel for buttons.");

	textButtonLookAndFeel.setButtonTextColor(menuButtonTextColor1);
	textButtonLookAndFeel.setGradientColor1(menuButtonGradientColor1);
	textButtonLookAndFeel.setGradientColor2(menuButtonGradientColor2);
	textButtonLookAndFeel.setButtonOutlineColor(menuThinBorderColor); 

	textButtonLookAndFeel2.setButtonTextColor(menuButtonLabelColor1);
	textButtonLookAndFeel2.setGradientColor1(menuButtonGradientColor1);
	textButtonLookAndFeel2.setGradientColor2(menuButtonGradientColor2);
	textButtonLookAndFeel2.setButtonOutlineColor(menuThinBorderColor); 

	textButtonLookAndFeel.setTextSize(14.0f); 
	textButtonLookAndFeel2.setTextSize(14.0f); 

	// New Preset Button setup
	addAndMakeVisible(newPresetButton);
	newPresetButton.setLookAndFeel(&textButtonLookAndFeel);
	newPresetButton.repaint();
	newPresetButton.onClick = [this] {
		//DBG("New Preset Button Clicked - Inside onClick handler");
		themeList.addPreset("Preset Created");
		popup.updateBackgroundBlur();
		popup.setVisible(true);
		showPopup("Preset Created");
		};


	addAndMakeVisible(overwritePresetButton);
	overwritePresetButton.setLookAndFeel(&textButtonLookAndFeel);
	overwritePresetButton.repaint();
	overwritePresetButton.onClick = [this] {
		int selectedPresetIndex = themeList.getSelectedRow();
		if (selectedPresetIndex != -1) {
			juce::String selectedPresetName = themeList.getPresetName(selectedPresetIndex);
			themeList.overwritePreset(selectedPresetIndex, selectedPresetName);
			popup.updateBackgroundBlur();
			popup.setVisible(true);
			showPopup("Preset Overwritten");
		}
		else {
			DBG("No preset selected to overwrite");
		}
		};

	addAndMakeVisible(deletePresetButton);
	deletePresetButton.setLookAndFeel(&textButtonLookAndFeel);
	deletePresetButton.repaint();
	deletePresetButton.onClick = [this] {
		int selectedPresetIndex = themeList.getSelectedRow();
		if (selectedPresetIndex != -1) {
			themeList.deletePreset(selectedPresetIndex);
			popup.updateBackgroundBlur();
			popup.setVisible(true);
			showPopup("Preset Deleted");
		}
		else {
			DBG("No preset selected to delete");
		}
		};

	// Randomize Hue Toggle Button
	addAndMakeVisible(randomizeHueToggleButton);
	randomizeHueToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeHueToggleButton.setClickingTogglesState(true);  // Make the button toggle	
	randomizeHueToggleButton.onClick = [this] {
		bool isToggled = randomizeHueToggleButton.getToggleState();
		sharedResources.sharedColors.setRandomizeHue(isToggled);
		randomizeHueToggleButton.repaint();
		// Additional logic if needed
		};

	// Randomize Saturation Toggle Button
	addAndMakeVisible(randomizeSaturationToggleButton);
	randomizeSaturationToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeSaturationToggleButton.setClickingTogglesState(true);  // Make the button toggle
	randomizeSaturationToggleButton.repaint();
	randomizeSaturationToggleButton.onClick = [this] {
		bool isToggled = randomizeSaturationToggleButton.getToggleState();
		sharedResources.sharedColors.setRandomizeSaturation(isToggled);
		overlayComponent.setSaturationRandomizationEnabled(isToggled);
		// Additional logic if needed
		};

	// Randomize Brightness Toggle Button
	addAndMakeVisible(randomizeBrightnessToggleButton);
	randomizeBrightnessToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeBrightnessToggleButton.setClickingTogglesState(true);  // Make the button toggle
	randomizeBrightnessToggleButton.repaint();
	randomizeBrightnessToggleButton.onClick = [this] {
		bool isToggled = randomizeBrightnessToggleButton.getToggleState();
		sharedResources.sharedColors.setRandomizeBrightness(isToggled);
		overlayComponent.setBrightnessRandomizationEnabled(isToggled);
		// Additional logic if needed
		};

	// Randomize Alpha Toggle Button
	addAndMakeVisible(randomizeAlphaToggleButton);
	randomizeAlphaToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeAlphaToggleButton.setClickingTogglesState(true);  // Make the button toggle
	randomizeAlphaToggleButton.repaint();
	randomizeAlphaToggleButton.onClick = [this] {
		bool isToggled = randomizeAlphaToggleButton.getToggleState();
		sharedResources.sharedColors.setRandomizeAlpha(isToggled);
		// Additional logic if needed
		};

	// Section: Randomize (header only - buttons laid out in resized)
	randomizeSectionLabel.setText ("Randomize", juce::dontSendNotification);
	randomizeSectionLabel.setJustificationType (juce::Justification::centredLeft);
	randomizeSectionLabel.setFont (SharedResources::uiFont (14.0f));
	randomizeSectionLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (randomizeSectionLabel);

	addAndMakeVisible (chromeSection);
	chromeSection.onChanged = [this]
	{
		resized();
		if (auto* menu = findParentComponentOfClass<Menu>())
			menu->notifyContentHeightChanged();
	};

	enforceLegibleTextToggle.setClickingTogglesState (true);
	enforceLegibleTextToggle.setToggleState (sharedResources.sharedColors.enforceLegibleText,
	                                         juce::dontSendNotification);
	enforceLegibleTextToggle.setTooltip (
	    "On by default. Keeps labels readable on the faceplate, mod panel, option box, "
	    "graph-top UI menus, PopupMenus, Scope modules, meters, graph handles, and settings menus. "
	    "After randomize (and on theme load), pushes text value away from its background. "
	    "Adjusts text only (value), not backgrounds.");
	enforceLegibleTextToggle.onClick = [this]
	{
		sharedResources.sharedColors.enforceLegibleText = enforceLegibleTextToggle.getToggleState();
		syncAccessibilityControls();
		if (sharedResources.sharedColors.enforceLegibleText)
			applyAccessibilityTextContrast();
	};
	addAndMakeVisible (enforceLegibleTextToggle);

	textContrastLabel.setText ("Contrast", juce::dontSendNotification);
	textContrastLabel.setJustificationType (juce::Justification::centredLeft);
	textContrastLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (textContrastLabel);

	textContrastSlider.setRange (0.0, 1.0, 0.01);
	textContrastSlider.setValue (sharedResources.sharedColors.textContrastAmount, juce::dontSendNotification);
	textContrastSlider.setSliderStyle (juce::Slider::LinearHorizontal);
	textContrastSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
	textContrastSlider.setTooltip ("How strongly text is separated from background value (text only).");
	textContrastSlider.addListener (this);
	addAndMakeVisible (textContrastSlider);

	optionBoxOpacityLabel.setText ("Option box opacity", juce::dontSendNotification);
	optionBoxOpacityLabel.setJustificationType (juce::Justification::centredLeft);
	optionBoxOpacityLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (optionBoxOpacityLabel);

	optionBoxOpacitySlider.setRange (0.30, 1.0, 0.01);
	optionBoxOpacitySlider.setValue (sharedResources.sharedColors.optionBoxOpacity, juce::dontSendNotification);
	optionBoxOpacitySlider.setSliderStyle (juce::Slider::LinearHorizontal);
	optionBoxOpacitySlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
	optionBoxOpacitySlider.setTooltip (
	    "How solid the floating band editor (Option box) is over the spectrum. "
	    "Default 90%. Open the Option box on the graph to preview. "
	    "Does not change Spectrum analyser Opacity.");
	optionBoxOpacitySlider.addListener (this);
	addAndMakeVisible (optionBoxOpacitySlider);

	optionBoxOpacityPercentLabel.setJustificationType (juce::Justification::centredRight);
	optionBoxOpacityPercentLabel.setMinimumHorizontalScale (1.0f);
	optionBoxOpacityPercentLabel.setInterceptsMouseClicks (false, false);
	addAndMakeVisible (optionBoxOpacityPercentLabel);

	buttonCornerRadiusLabel.setText ("Button radius", juce::dontSendNotification);
	buttonCornerRadiusLabel.setJustificationType (juce::Justification::centredLeft);
	buttonCornerRadiusLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (buttonCornerRadiusLabel);

	buttonCornerRadiusSlider.setRange (2.0, 16.0, 0.5);
	buttonCornerRadiusSlider.setValue (sharedResources.sharedColors.buttonCornerRadius,
	                                   juce::dontSendNotification);
	buttonCornerRadiusSlider.setSliderStyle (juce::Slider::LinearHorizontal);
	buttonCornerRadiusSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
	buttonCornerRadiusSlider.setTooltip (
	    "Corner roundness for plugin chrome buttons. Also softens the Melatonin "
	    "outline blur so the rim sits against the background (interior is filled over).");
	buttonCornerRadiusSlider.addListener (this);
	addAndMakeVisible (buttonCornerRadiusSlider);

	buttonCornerRadiusValueLabel.setJustificationType (juce::Justification::centredRight);
	buttonCornerRadiusValueLabel.setMinimumHorizontalScale (1.0f);
	buttonCornerRadiusValueLabel.setInterceptsMouseClicks (false, false);
	addAndMakeVisible (buttonCornerRadiusValueLabel);

	menuPopupRadiusLabel.setText ("Menu radius", juce::dontSendNotification);
	menuPopupRadiusLabel.setJustificationType (juce::Justification::centredLeft);
	menuPopupRadiusLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (menuPopupRadiusLabel);

	menuPopupRadiusSlider.setRange (0.0, 16.0, 0.5);
	menuPopupRadiusSlider.setValue (sharedResources.sharedColors.menuPopupCornerRadius,
	                                juce::dontSendNotification);
	menuPopupRadiusSlider.setSliderStyle (juce::Slider::LinearHorizontal);
	menuPopupRadiusSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
	menuPopupRadiusSlider.setTooltip (
	    "Corner roundness for dropdown and popup menus. Separate from button radius.");
	menuPopupRadiusSlider.addListener (this);
	addAndMakeVisible (menuPopupRadiusSlider);

	menuPopupRadiusValueLabel.setJustificationType (juce::Justification::centredRight);
	menuPopupRadiusValueLabel.setMinimumHorizontalScale (1.0f);
	menuPopupRadiusValueLabel.setInterceptsMouseClicks (false, false);
	menuPopupRadiusValueLabel.setText (
	    juce::String (juce::roundToInt (sharedResources.sharedColors.menuPopupCornerRadius)) + " px",
	    juce::dontSendNotification);
	addAndMakeVisible (menuPopupRadiusValueLabel);

	menuPopupOutlineToggle.setClickingTogglesState (true);
	menuPopupOutlineToggle.setToggleState (sharedResources.sharedColors.menuPopupOutline,
	                                       juce::dontSendNotification);
	menuPopupOutlineToggle.setTooltip ("Draw a 1 px outline around dropdown and popup menus.");
	menuPopupOutlineToggle.onClick = [this]
	{
		sharedResources.sharedColors.menuPopupOutline = menuPopupOutlineToggle.getToggleState();
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	};
	addAndMakeVisible (menuPopupOutlineToggle);

	uiFontLabel.setText ("UI font", juce::dontSendNotification);
	uiFontLabel.setJustificationType (juce::Justification::centredLeft);
	uiFontLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (uiFontLabel);

	uiFontCombo.setTooltip (
	    "Global typeface for buttons, labels, and chrome text. "
	    "Hover a name to preview; click to keep it. "
	    "Pirulen is bundled; other names use system fonts when installed.");
	uiFontCombo.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
	const auto fonts = SharedColors::getUiFontCatalogue();
	for (int i = 0; i < fonts.size(); ++i)
		uiFontCombo.addItem (fonts[i], i + 1);
	{
		const int idx = fonts.indexOf (sharedResources.sharedColors.uiFontName);
		uiFontCombo.setSelectedId (idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);
	}
	committedUiFontName = sharedResources.sharedColors.uiFontName;
	uiFontCombo.onHoverPreview = [this] (const juce::String& name)
	{
		// Preview on this page only. Walking the editor (and ComboBox::lookAndFeelChanged)
		// while the CallOutBox is open dismissed the menu and crashed.
		if (name.isNotEmpty())
			sharedResources.sharedColors.uiFontName = name;
		sharedResources.makeActive();
		SharedResources::applyUiFontsRecursively (*this);
		themeList.repaint();
		uiElementsList.repaint();
		repaint();
	};
	uiFontCombo.onMenuDismissedWithoutChoice = [this]
	{
		applyUiFontPreview (committedUiFontName, false);
	};
	uiFontCombo.onChange = [this]
	{
		const int id = uiFontCombo.getSelectedId();
		if (id <= 0)
			return;
		const auto fonts = SharedColors::getUiFontCatalogue();
		const int i = id - 1;
		if (! juce::isPositiveAndBelow (i, fonts.size()))
			return;
		committedUiFontName = fonts[i];
		applyUiFontPreview (fonts[i], true);
	};
	addAndMakeVisible (uiFontCombo);

	uiFontBoldToggle.setClickingTogglesState (true);
	uiFontBoldToggle.setToggleState (sharedResources.sharedColors.uiFontBold, juce::dontSendNotification);
	uiFontBoldToggle.setTooltip ("Use a bold weight for the global UI font plugin-wide.");
	uiFontBoldToggle.onClick = [this]
	{
		sharedResources.sharedColors.uiFontBold = uiFontBoldToggle.getToggleState();
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	};
	addAndMakeVisible (uiFontBoldToggle);

	cursorInfoSizeLabel.setText ("Cursor info size", juce::dontSendNotification);
	cursorInfoSizeLabel.setJustificationType (juce::Justification::centredLeft);
	cursorInfoSizeLabel.setMinimumHorizontalScale (1.0f);
	addAndMakeVisible (cursorInfoSizeLabel);

	cursorInfoSizeSlider.setRange (8.0, 24.0, 0.5);
	cursorInfoSizeSlider.setValue (sharedResources.sharedColors.graphCursorInfoFontSize,
	                               juce::dontSendNotification);
	cursorInfoSizeSlider.setSliderStyle (juce::Slider::LinearHorizontal);
	cursorInfoSizeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
	cursorInfoSizeSlider.setTooltip (
	    "Font size for the floating dB / Hz / Q label next to the cursor and crosshair.");
	cursorInfoSizeSlider.addListener (this);
	addAndMakeVisible (cursorInfoSizeSlider);

	cursorInfoSizeValueLabel.setJustificationType (juce::Justification::centredRight);
	cursorInfoSizeValueLabel.setMinimumHorizontalScale (1.0f);
	cursorInfoSizeValueLabel.setInterceptsMouseClicks (false, false);
	cursorInfoSizeValueLabel.setText (
	    juce::String (sharedResources.sharedColors.graphCursorInfoFontSize, 1) + " pt",
	    juce::dontSendNotification);
	addAndMakeVisible (cursorInfoSizeValueLabel);

	buttonGlowToggle.setClickingTogglesState (true);
	buttonGlowToggle.setToggleState (sharedResources.sharedColors.buttonGlowEnabled,
	                                 juce::dontSendNotification);
	buttonGlowToggle.setTooltip (
	    "Soft Melatonin edge glow on chrome buttons (also needs global glow/shadow on).");
	buttonGlowToggle.onClick = [this]
	{
		sharedResources.sharedColors.buttonGlowEnabled = buttonGlowToggle.getToggleState();
		buttonGlowHoverOnlyToggle.setEnabled (sharedResources.sharedColors.buttonGlowEnabled);
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	};
	addAndMakeVisible (buttonGlowToggle);

	buttonGlowHoverOnlyToggle.setClickingTogglesState (true);
	buttonGlowHoverOnlyToggle.setToggleState (sharedResources.sharedColors.buttonGlowOnlyOnHover,
	                                          juce::dontSendNotification);
	buttonGlowHoverOnlyToggle.setTooltip (
	    "When on, button glow only appears while the pointer is over the button (or pressed).");
	buttonGlowHoverOnlyToggle.setEnabled (sharedResources.sharedColors.buttonGlowEnabled);
	buttonGlowHoverOnlyToggle.onClick = [this]
	{
		sharedResources.sharedColors.buttonGlowOnlyOnHover = buttonGlowHoverOnlyToggle.getToggleState();
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	};
	addAndMakeVisible (buttonGlowHoverOnlyToggle);

	syncAccessibilityControls();

	randomizeSelectedColorsButton.onClick = [this] {
		auto selectedIndices = uiElementsList.getSelectedPaletteIndices();
		DBG("Selected palette indices for randomization: " + juce::String(selectedIndices.size()));
		auto& sharedColors = sharedResources.sharedColors;

		// Reset all flags to false (skip) - then enable selected palette slots.
		std::fill(sharedColors.colorRandomizationFlags.begin(), sharedColors.colorRandomizationFlags.end(), (uint8_t) 1);

		// Set flags for the selected palette indices (0 = randomize; inverted historical semantics)
		for (auto index : selectedIndices) {
			if (index >= 0 && index < sharedColors.colorRandomizationFlags.size()) {
				sharedColors.colorRandomizationFlags[(size_t) index] = 0;
			}
		}

		float lowerHue = hueRangeSlider.getMinValue();
		float upperHue = hueRangeSlider.getMaxValue();

		sharedResources.sharedColors.setHueRange(lowerHue, upperHue);

		saturationRangeSlider.setMinValue(saturationRangeSlider.getMinValue());
		saturationRangeSlider.setMaxValue(saturationRangeSlider.getMaxValue());

		brightnessRangeSlider.setMinValue(brightnessRangeSlider.getMinValue());
		brightnessRangeSlider.setMaxValue(brightnessRangeSlider.getMaxValue());

		// Randomize the selected colors and get the randomized color
		juce::Colour randomizedColor = sharedResources.sharedColors.randomizeSelectedColorsWithinRange();
		// Enforce last: never re-sync faceplate/mod after (that undoes mod-panel text fixes).
		if (sharedResources.sharedColors.enforceLegibleText)
			sharedResources.sharedColors.enforceLegibleTextContrast();

		std::fill(sharedColors.colorRandomizationFlags.begin(), sharedColors.colorRandomizationFlags.end(), (uint8_t) 1);

		colorValuesInput.updateSaturationSlider(currentColor.getSaturation() * 255.0f);
		colorValuesInput.updateBrightnessSlider(currentColor.getBrightness() * 255.0f);
		colorValuesInput.updateHueSlider(currentColor.getHue() * 255.0f);

		setupLabels();
		updateComponents(randomizedColor);
		updateAllComponents();

		setRangeSliderColors();
		setSliderLookAndFeels();
		setButtonLookAndFeels();

		juce::Colour trackColor = sharedResources.sharedColors.menuScrollBarTrackColor1;
		juce::Colour thumbColor = sharedResources.sharedColors.menuScrollBarThumbColor1;
		juce::Colour outlineColor = sharedResources.sharedColors.menuScrollBarOutlineColor1;

		themeList.updateScrollBarColors(trackColor, thumbColor, outlineColor);
		uiElementsList.updateScrollBarColors(trackColor, thumbColor, outlineColor);

		repaintComponents();

		// Trigger a repaint of the Menu background with the updated colors
		repaintParentComponent();
		notifyThemeLiveChanged();
		};


	randomizeColorsButton.onClick = [this] {

		float lowerHue = hueRangeSlider.getMinValue();
		float upperHue = hueRangeSlider.getMaxValue();

		sharedResources.sharedColors.setHueRange(lowerHue, upperHue);

		saturationRangeSlider.setMinValue(saturationRangeSlider.getMinValue());
		saturationRangeSlider.setMaxValue(saturationRangeSlider.getMaxValue());

		brightnessRangeSlider.setMinValue(brightnessRangeSlider.getMinValue());
		brightnessRangeSlider.setMaxValue(brightnessRangeSlider.getMaxValue());

		sharedResources.sharedColors.randomizeColors();
		// Enforce last: never re-sync faceplate/mod after (that undoes mod-panel text fixes).
		if (sharedResources.sharedColors.enforceLegibleText)
			sharedResources.sharedColors.enforceLegibleTextContrast();
		refreshAfterRandomize();
		};

	randomizeColorsButton.onPopupMenu = [this] { showRandomizeScopeMenu(); };

	addAndMakeVisible(randomizeSelectedColorsButton);
	randomizeSelectedColorsButton.setButtonText ("Rand sel");
	randomizeSelectedColorsButton.setLookAndFeel(&textButtonLookAndFeel);

	addAndMakeVisible(randomizeColorsButton);
	randomizeColorsButton.setButtonText ("Rand all");
	randomizeColorsButton.setLookAndFeel(&textButtonLookAndFeel); // Applying the custom LookAndFeel

	addAndMakeVisible(randomizeHueToggleButton);
	randomizeHueToggleButton.setButtonText("H"); // Setting the button text
	randomizeHueToggleButton.setLookAndFeel(&textButtonLookAndFeel2); // Applying the custom LookAndFeel
	randomizeHueToggleButton.setClickingTogglesState(true);

	addAndMakeVisible(randomizeSaturationToggleButton);
	randomizeSaturationToggleButton.setButtonText("S"); // Setting the button text
	randomizeSaturationToggleButton.setLookAndFeel(&textButtonLookAndFeel2); // Applying the custom LookAndFeel
	randomizeSaturationToggleButton.setClickingTogglesState(true);

	addAndMakeVisible(randomizeBrightnessToggleButton);
	randomizeBrightnessToggleButton.setButtonText("B"); // Setting the button text
	randomizeBrightnessToggleButton.setLookAndFeel(&textButtonLookAndFeel2); // Applying the custom LookAndFeel
	randomizeBrightnessToggleButton.setClickingTogglesState(true);

	addAndMakeVisible(randomizeAlphaToggleButton);
	randomizeAlphaToggleButton.setButtonText("A"); // Setting the button text
	randomizeAlphaToggleButton.setLookAndFeel(&textButtonLookAndFeel2); // Applying the custom LookAndFeel
	randomizeAlphaToggleButton.setClickingTogglesState(true);

	// Populate UI Elements from the theme colour registry (sorted A-Z on header click).
	uiElementsList.populateFromRegistry();

	// Get color of the first item in the list
	juce::Colour firstItemColor = sharedResources.sharedColors.menuBackgroundGradientColor1;
	auto arrowColor = sharedResources.sharedColors.menuScrollBarThumbColor1;
	auto fillBarColor = sharedResources.sharedColors.menuSliderFillColor;

	/*

	eyedropper = std::make_unique<Eyedropper>();
	addAndMakeVisible(eyedropper.get());

	eyedropper = std::make_unique<Eyedropper>();
	addAndMakeVisible(eyedropper.get());

	eyedropperButton = std::make_unique<juce::TextButton>("Eyedropper"); // Initialize the button
	addAndMakeVisible(eyedropperButton.get());
	eyedropperButton->onClick = [this] {
		eyedropper->beginColorSelection([this](juce::Colour selectedColor) {
			updateSelectedColor(selectedColor);
			});
		};

		*/

	// Initialize QuadPicker, HueSelector, and ColorSwatch with the first item color
	quadPicker.setColor(firstItemColor);
	hueSelector.setColor(firstItemColor);
	colorValuesInput.setColor(firstItemColor);
	colorSwatch.setColor(firstItemColor);
};

AppearanceComponent::~AppearanceComponent()  // Destructor
{
	uiFontCombo.hidePopup();
	uiFontCombo.setLookAndFeel (nullptr);

	for (auto* button : buttonsUsingCustomLookAndFeel)
		if (button != nullptr)
			button->setLookAndFeel (nullptr);

	for (auto* button : buttonsUsingCustomLookAndFeel2)
		if (button != nullptr)
			button->setLookAndFeel (nullptr);

	for (auto* slider : slidersUsingCustomLookAndFeel)
		if (slider != nullptr)
			slider->setLookAndFeel (nullptr);

	newPresetButton.setLookAndFeel (nullptr);
	overwritePresetButton.setLookAndFeel (nullptr);
	deletePresetButton.setLookAndFeel (nullptr);
	randomizeColorsButton.setLookAndFeel (nullptr);
	randomizeSelectedColorsButton.setLookAndFeel (nullptr);
	randomizeHueToggleButton.setLookAndFeel (nullptr);
	randomizeSaturationToggleButton.setLookAndFeel (nullptr);
	randomizeBrightnessToggleButton.setLookAndFeel (nullptr);
	randomizeAlphaToggleButton.setLookAndFeel (nullptr);
	hueRangeSlider.setLookAndFeel (nullptr);
	saturationRangeSlider.setLookAndFeel (nullptr);
	brightnessRangeSlider.setLookAndFeel (nullptr);

	buttonsUsingCustomLookAndFeel.clear();
	buttonsUsingCustomLookAndFeel2.clear();
	slidersUsingCustomLookAndFeel.clear();
}

namespace
{
    constexpr int kAppPad = 16;
    constexpr int kAppSectionH = 16;
    constexpr int kAppSectionGap = 8;
    constexpr int kAppRowH = 22;
    constexpr int kAppThemeBtnH = 30;
    constexpr int kAppRandomRowH = 24;
    constexpr int kAppMinListH = 140;

    constexpr int kChromeLabelH = 18;
    constexpr int kChromeSliderH = 24;
    constexpr int kChromeRowGap = 6;
    constexpr int kChromeLabelGap = 2;
    constexpr int kChromeTogH = 22;

    int appearanceChromeBodyH() noexcept
    {
        const int sliderRow = kChromeLabelH + kChromeLabelGap + kChromeSliderH + kChromeRowGap;
        const int togRow = kChromeTogH + kChromeRowGap;
        // sliders: contrast, opacity, button radius, menu radius, cursor size
        // combo: UI font (+ optional bold wrap)
        // toggles: legible, outline, glow
        return sliderRow * 5 + sliderRow + togRow * 3 + 8;
    }

    int appearancePreferredHeightForList (int listH) noexcept
    {
        int h = kAppPad;
        h += kAppSectionH + 4 + listH + kAppSectionGap;          // Colours list
        h += kAppSectionH + 4 + kAppRandomRowH + kAppSectionGap; // Randomize
        h += kAppSectionH + 4 + listH + 6;                       // Themes list
        h += kAppThemeBtnH + kAppSectionGap + 2;                 // New / Overwrite / Delete
        h += kAppPad;
        return h;
    }
}

int AppearanceComponent::getPreferredContentHeight() const
{
    // Same floor as Menu used before Appearance had a preferred-height API
    // (kContentHeight - tab bar), so the page still scrolls in a typical frame.
    const int fitted = appearancePreferredHeightForList (kAppMinListH)
                       + chromeSection.heightFor (appearanceChromeBodyH());
    return juce::jmax (Menu::kContentHeight - 36, fitted);
}

void AppearanceComponent::paint(juce::Graphics& g)
{
	quadPickerShadow.render(g, quadPickerShadowPath);
	uiElementsListShadow.render(g, uiElementsListShadowPath);
	themesListShadow.render(g, themesListShadowPath);
	newPresetButtonShadow.render(g, newPresetButtonShadowPath);
	overwritePresetButtonShadow.render(g, overwritePresetButtonShadowPath);
	deletePresetButtonShadow.render(g, deletePresetButtonShadowPath);
	randomColorsButtonShadow.render(g, randomColorsButtonShadowPath);
	randomSelectedColorsButtonShadow.render(g, randomSelectedColorsButtonShadowPath);
}

void AppearanceComponent::resized()
{
	DBG("AppearanceComponent::resized Called");
	const int padding = kAppPad;
	const int sectionHeaderH = kAppSectionH;
	const int sectionGap = kAppSectionGap;
	const int themeBtnH = kAppThemeBtnH;

	const int quadPickerScaleX = getWidth() / 3;
	const int hueSelectorScaleX = juce::roundToInt ((float) padding * 3.5f);
	const int swatchSize = 130;
	const int listBoxWidth = juce::jmax (160, getWidth() - quadPickerScaleX - hueSelectorScaleX - juce::roundToInt ((float) swatchSize * 1.6f));
	const int themesListX = padding;

	// Lists share the left column above Chrome — never steal Chrome's reserved
	// height or preferred-height (that clipped the page and hid the scrollbar).
	const int leftTop = padding;
	const int leftBottom = getHeight() - padding;
	const int leftBottomReserve = chromeSection.heightFor (appearanceChromeBodyH())
	                              + kAppRandomRowH + kAppThemeBtnH + sectionHeaderH
	                              + sectionGap * 4 + 12;
	const int leftUsable = juce::jmax (120, leftBottom - leftTop - leftBottomReserve);
	const int listHeight = juce::jmax (kAppMinListH,
	                                   (leftUsable - sectionHeaderH * 2 - sectionGap * 2) / 2);

	int y = padding;
	coloursSectionLabel.setBounds (padding, y, listBoxWidth, sectionHeaderH);
	y += sectionHeaderH + 4;
	const int elementsListY = y;
	uiElementsList.setBounds (padding, elementsListY, listBoxWidth, listHeight);
	y = elementsListY + listHeight + sectionGap;

	randomizeSectionLabel.setBounds (padding, y, listBoxWidth, sectionHeaderH);
	y += sectionHeaderH + 4;
	const int randomRowH = 24;
	const int chipW = 26;
	const int chipGap = 4;
	const int chipsBlockW = chipW * 4 + chipGap * 3;
	// Size randomize buttons to full plain captions (no "..." from a too-narrow box).
	const juce::Font randFont (SharedResources::uiFont (12.0f));
	const int randSelW = juce::jmax (56, (int) std::ceil (
	    juce::GlyphArrangement::getStringWidth (randFont, "Rand sel")) + 12);
	const int randAllW = juce::jmax (56, (int) std::ceil (
	    juce::GlyphArrangement::getStringWidth (randFont, "Rand all")) + 12);
	const int maxRandPair = listBoxWidth - chipsBlockW - 12;
	const int randBtnW = juce::jmax (randSelW, randAllW);
	const int usedRandW = juce::jmin (maxRandPair, randBtnW * 2 + 6);
	const int eachRandW = juce::jmax (randSelW, (usedRandW - 6) / 2);
	randomizeSelectedColorsButton.setBounds (padding, y, eachRandW, randomRowH);
	randomizeColorsButton.setBounds (padding + eachRandW + 6, y, eachRandW, randomRowH);
	int chipX = padding + listBoxWidth - chipsBlockW;
	randomizeHueToggleButton.setBounds (chipX, y, chipW, randomRowH);
	chipX += chipW + chipGap;
	randomizeSaturationToggleButton.setBounds (chipX, y, chipW, randomRowH);
	chipX += chipW + chipGap;
	randomizeBrightnessToggleButton.setBounds (chipX, y, chipW, randomRowH);
	chipX += chipW + chipGap;
	randomizeAlphaToggleButton.setBounds (chipX, y, chipW, randomRowH);
	y += randomRowH + sectionGap;

	themesLabel.setBounds (padding, y, listBoxWidth, sectionHeaderH);
	y += sectionHeaderH + 4;
	const int themesListY = y;
	themeList.setBounds (themesListX, themesListY, listBoxWidth, listHeight);
	y = themesListY + listHeight + 6;

	const int themeBtnW = juce::jmax (48, (listBoxWidth - 12) / 3);
	newPresetButton.setBounds (padding, y, themeBtnW, themeBtnH);
	overwritePresetButton.setBounds (padding + themeBtnW + 6, y, themeBtnW, themeBtnH);
	deletePresetButton.setBounds (padding + (themeBtnW + 6) * 2, y, themeBtnW, themeBtnH);
	y += themeBtnH + sectionGap + 2;

	const int chromeW = juce::jmax (160, getWidth() - padding * 2);
	auto chromeArea = juce::Rectangle<int> (padding, y, chromeW, 8000);
	chromeSection.applyVisible ({
		&enforceLegibleTextToggle, &textContrastLabel, &textContrastSlider,
		&optionBoxOpacityLabel, &optionBoxOpacitySlider, &optionBoxOpacityPercentLabel,
		&buttonCornerRadiusLabel, &buttonCornerRadiusSlider, &buttonCornerRadiusValueLabel,
		&menuPopupRadiusLabel, &menuPopupRadiusSlider, &menuPopupRadiusValueLabel,
		&menuPopupOutlineToggle, &uiFontLabel, &uiFontCombo, &uiFontBoldToggle,
		&cursorInfoSizeLabel, &cursorInfoSizeSlider, &cursorInfoSizeValueLabel,
		&buttonGlowToggle, &buttonGlowHoverOnlyToggle });
	chromeSection.placeHeader (chromeArea);
	y = chromeArea.getY();

	const juce::Font bodyFont (juce::FontOptions (12.0f));
	const int percentW = juce::jmax (36, (int) std::ceil (
	    juce::GlyphArrangement::getStringWidth (bodyFont, "100%")) + 6);
	const int radiusValW = juce::jmax (40, (int) std::ceil (
	    juce::GlyphArrangement::getStringWidth (bodyFont, "16 px")) + 6);
	const int cursorValW = juce::jmax (44, (int) std::ceil (
	    juce::GlyphArrangement::getStringWidth (bodyFont, "24 pt")) + 6);

	if (chromeSection.isOpen())
	{
		auto placeSlider = [&] (juce::Label& lab, juce::Slider& sl, juce::Label* valueLab, int valueW)
		{
			lab.setBounds (padding, y, chromeW, kChromeLabelH);
			y += kChromeLabelH + kChromeLabelGap;
			if (valueLab != nullptr)
			{
				sl.setBounds (padding, y, juce::jmax (50, chromeW - valueW - 8), kChromeSliderH);
				valueLab->setBounds (padding + chromeW - valueW, y, valueW, kChromeSliderH);
			}
			else
			{
				sl.setBounds (padding, y, chromeW, kChromeSliderH);
			}
			y += kChromeSliderH + kChromeRowGap;
		};

		enforceLegibleTextToggle.setBounds (padding, y, chromeW, kChromeTogH);
		y += kChromeTogH + kChromeRowGap;
		placeSlider (textContrastLabel, textContrastSlider, nullptr, 0);
		placeSlider (optionBoxOpacityLabel, optionBoxOpacitySlider, &optionBoxOpacityPercentLabel, percentW);
		placeSlider (buttonCornerRadiusLabel, buttonCornerRadiusSlider, &buttonCornerRadiusValueLabel, radiusValW);
		placeSlider (menuPopupRadiusLabel, menuPopupRadiusSlider, &menuPopupRadiusValueLabel, radiusValW);

		const int outlineW = juce::jmax (100, (int) std::ceil (
		    juce::GlyphArrangement::getStringWidth (bodyFont, "Menu outline")) + 28);
		menuPopupOutlineToggle.setBounds (padding, y, juce::jmin (outlineW, chromeW), kChromeTogH);
		y += kChromeTogH + kChromeRowGap;

		uiFontLabel.setBounds (padding, y, chromeW, kChromeLabelH);
		y += kChromeLabelH + kChromeLabelGap;
		const int boldW = juce::jmax (88, (int) std::ceil (
		    juce::GlyphArrangement::getStringWidth (bodyFont, "Bold text")) + 36);
		uiFontCombo.setBounds (padding, y, juce::jmax (80, chromeW - boldW - 10), kChromeSliderH);
		uiFontBoldToggle.setBounds (padding + chromeW - boldW, y, boldW, kChromeSliderH);
		uiFontBoldToggle.toFront (false);
		y += kChromeSliderH + kChromeRowGap;

		placeSlider (cursorInfoSizeLabel, cursorInfoSizeSlider, &cursorInfoSizeValueLabel, cursorValW);

		const int glowW = juce::jmax (90, (int) std::ceil (
		    juce::GlyphArrangement::getStringWidth (bodyFont, "Button glow")) + 28);
		const int glowHoverW = juce::jmax (120, (int) std::ceil (
		    juce::GlyphArrangement::getStringWidth (bodyFont, "Glow on hover only")) + 28);
		buttonGlowToggle.setBounds (padding, y, glowW, kChromeTogH);
		buttonGlowHoverOnlyToggle.setBounds (padding + glowW + 8, y,
		                                     juce::jmin (glowHoverW, chromeW - glowW - 8), kChromeTogH);
		y += kChromeTogH + kChromeRowGap;
	}

	// Right column: SV pad + hue strip + brightness slider (pre-accordion layout).
	// Pad and brightness strip run down to the themes-list bottom.
	const int rangeSliderWidth = 40;
	const int hueSelectorX = getWidth() - quadPickerScaleX - padding - hueSelectorScaleX;
	const int hueSelectorY = padding;
	const int quadPickerX = getWidth() - quadPickerScaleX - padding;
	const int quadPickerY = padding;
	const int themesListBottom = themesListY + listHeight;
	const int quadPickerScaleY = juce::jmax (180, themesListBottom - quadPickerY);
	const int hueSelectorScaleY = quadPickerScaleY;
	const int saturationRangeSliderY = juce::jmax (0, quadPickerY - rangeSliderWidth);
	const int hueRangeSliderX = hueSelectorX + hueSelectorScaleX - 20;
	const int brightnessRangeSliderX = juce::roundToInt ((float) getWidth() - (float) padding * 1.825f);
	const int colorValuesInputScaleX = juce::jmax (80, getWidth() - listBoxWidth - hueSelectorScaleX - quadPickerScaleX - padding);
	const int colorValuesInputScaleY = juce::jmax (80, getHeight() - swatchSize / 2 - padding);
	const int colorValueInputX = listBoxWidth + juce::roundToInt ((float) padding / 1.5f);
	const int colorValueInputY = swatchSize + juce::roundToInt ((float) padding * 1.3f);
	const int colorSwatchX = getWidth() - quadPickerScaleX - hueSelectorScaleX
	                         - juce::roundToInt ((float) padding * 1.2f) - swatchSize;

	constexpr float cornerSize = 5.0f;
	quadPickerShadowPath.clear();
	quadPickerShadowPath.addRoundedRectangle ((float) quadPickerX, (float) quadPickerY,
	                                          (float) quadPickerScaleX, (float) quadPickerScaleY, cornerSize);

	constexpr float cornerSize2 = 14.0f;
	uiElementsListShadowPath.clear();
	uiElementsListShadowPath.addRoundedRectangle ((float) padding, (float) elementsListY,
	                                              (float) listBoxWidth, (float) listHeight, cornerSize2);

	constexpr float cornerSize3 = 14.0f;
	themesListShadowPath.clear();
	themesListShadowPath.addRoundedRectangle ((float) padding, (float) themesListY,
	                                          (float) listBoxWidth, (float) listHeight, cornerSize3);

	constexpr float cornerSize4 = 6.0f;
	newPresetButtonShadowPath.clear();
	newPresetButtonShadowPath.addRoundedRectangle (newPresetButton.getBounds().toFloat(), cornerSize4);
	overwritePresetButtonShadowPath.clear();
	overwritePresetButtonShadowPath.addRoundedRectangle (overwritePresetButton.getBounds().toFloat(), cornerSize4);
	deletePresetButtonShadowPath.clear();
	deletePresetButtonShadowPath.addRoundedRectangle (deletePresetButton.getBounds().toFloat(), cornerSize4);
	randomColorsButtonShadowPath.clear();
	randomColorsButtonShadowPath.addRoundedRectangle (randomizeColorsButton.getBounds().toFloat(), cornerSize4);
	randomSelectedColorsButtonShadowPath.clear();
	randomSelectedColorsButtonShadowPath.addRoundedRectangle (randomizeSelectedColorsButton.getBounds().toFloat(), cornerSize4);

	quadPicker.setBounds (quadPickerX, quadPickerY, quadPickerScaleX, quadPickerScaleY);
	hueSelector.setBounds (hueSelectorX, hueSelectorY, hueSelectorScaleX, hueSelectorScaleY);
	colorSwatch.setBounds (colorSwatchX, juce::roundToInt ((float) padding * 1.5f), swatchSize, swatchSize);
	colorValuesInput.setBounds (colorValueInputX, colorValueInputY, colorValuesInputScaleX, colorValuesInputScaleY);

	saturationRangeSlider.setBounds (quadPickerX - 8, saturationRangeSliderY, quadPickerScaleX + 22, rangeSliderWidth);
	hueRangeSlider.setBounds (hueRangeSliderX - 8, hueSelectorY - 3, rangeSliderWidth, hueSelectorScaleY + 10);
	brightnessRangeSlider.setBounds (brightnessRangeSliderX, hueSelectorY - 7, rangeSliderWidth, hueSelectorScaleY + 22);

	overlayComponent.setInterceptsMouseClicks (false, false);
	overlayComponent2.setInterceptsMouseClicks (false, false);
	overlayComponent.setBounds (quadPicker.getBounds());
	overlayComponent2.setBounds (hueSelector.getBounds());
	quadPicker.toFront (false);
	overlayComponent.toFront (false);
	saturationRangeSlider.toFront (false);
	brightnessRangeSlider.toFront (false);
	hueRangeSlider.toFront (false);
	popup.setBounds (themesListX, themesListY, listBoxWidth, listHeight);
}
void AppearanceComponent::initializeComponents() {
	DBG("initializeComponents Called");

	// SV pad: light path while scrubbing (ring stays 1:1); full theme commit on mouse-up.
	quadPicker.onColorChanged = [this] (juce::Colour newColor)
	{
		currentColor = newColor;
		if (quadPicker.isDraggingColour())
			liveColourPreviewFromPad (newColor);
		else
			directColorUpdate (newColor, false);
	};

	// Initialize HueSelector's hue change callback
	hueSelector.onHueChanged = [this](float newHue) {
		juce::Colour currentColor = quadPicker.getSelectedColor();
		float currentSaturation = currentColor.getSaturation();
		float currentBrightness = currentColor.getBrightness();
		float currentAlpha = currentColor.getFloatAlpha();
		juce::Colour newColor = juce::Colour::fromHSV(newHue, currentSaturation, currentBrightness, currentAlpha);

		// Update ColorValuesInput Hue Slider
		colorValuesInput.updateHueSlider(newHue * 255.0f); // Convert hue to 0-255 range

		saturationRangeSlider.setMinValue(saturationRangeSlider.getMinValue());
		saturationRangeSlider.setMaxValue(saturationRangeSlider.getMaxValue());

		brightnessRangeSlider.setMinValue(brightnessRangeSlider.getMinValue());
		brightnessRangeSlider.setMaxValue(brightnessRangeSlider.getMaxValue());

		updateComponents(newColor);
		directColorUpdate(newColor);
		quadPicker.setColor(newColor);
		};

	// Ensure that the QuadPicker and HueSelector are initialized with the correct colors
	// Get the color of the first element in the UIElementsList
	juce::Colour initialColor = uiElementsList.getFirstElementColor();
	quadPicker.setColor(initialColor);
	hueSelector.setColor(initialColor);

	colorValuesInput.updateSaturationSlider(initialColor.getSaturation() * 255.0f);
	colorValuesInput.updateBrightnessSlider(initialColor.getBrightness() * 255.0f);

	setRangeSliderColors();
	repaintComponents();
}

void AppearanceComponent::updateComponents(juce::Colour newColor)
{
	DBG("updateComponents Called");
	quadPicker.setColor(newColor);

	if (isUpdatingDirectly) {
		return;
	}

	juce::String selectedElementName = uiElementsList.getSelectedElementName();

	juce::Colour menuLabelTextColor = sharedResources.sharedColors.menuLabelTextColor1;
	coloursSectionLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	themesLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	randomizeSectionLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	chromeSection.repaint();

	// Enable write for selected palette slots, then apply.
	auto paletteIndices = uiElementsList.getSelectedPaletteIndices();
	if (paletteIndices.isEmpty())
	{
		const int pi = ThemeColorRegistry::indexForDisplayName (selectedElementName);
		if (pi >= 0)
			paletteIndices.add (pi);
	}
	sharedResources.sharedColors.toggleColorRandomizationFlags (paletteIndices);
	{
		juce::Colour applied = newColor;
		const int pi = ThemeColorRegistry::indexForDisplayName (selectedElementName);
		if (pi >= 0)
			applied = newColor.withAlpha (sharedResources.sharedColors.colourAt (pi).getFloatAlpha());
		sharedResources.sharedColors.setColourByDisplayName (selectedElementName, applied, true);
		applyColourSideEffects (selectedElementName, applied);
	}

	repaintParentComponent();

	colorSwatch.setColor(newColor);
	colorValuesInput.setColor(newColor);
	uiElementsList.updateSelectedElementColor(newColor);

	newPresetButton.setLookAndFeel(&textButtonLookAndFeel);
	overwritePresetButton.setLookAndFeel(&textButtonLookAndFeel);
	deletePresetButton.setLookAndFeel(&textButtonLookAndFeel);
	randomizeColorsButton.setLookAndFeel(&textButtonLookAndFeel);

	repaintComponents();
	repaint();
	notifyThemeLiveChanged();
}

void AppearanceComponent::liveColourPreviewFromPad (juce::Colour newColor)
{
	// Pad paints the ring itself. Only touch the local readouts here —
	// palette writes, list repaints, applyColourSideEffects, and notifyThemeLiveChanged
	// run on mouse-up via directColorUpdate so scrubbing stays frame-smooth.
	colorSwatch.setColor (newColor);
	colorValuesInput.setColor (newColor);
	colorValuesInput.updateHueSlider (newColor.getHue() * 255.0f);
	colorValuesInput.updateSaturationSlider (newColor.getSaturation() * 255.0f);
	colorValuesInput.updateBrightnessSlider (newColor.getBrightness() * 255.0f);
}

void AppearanceComponent::directColorUpdate(juce::Colour newColor, bool applyAlpha) {
	auto& sharedColors = sharedResources.sharedColors;
	auto originalFlags = sharedColors.colorRandomizationFlags;
	std::fill (sharedColors.colorRandomizationFlags.begin(), sharedColors.colorRandomizationFlags.end(), (uint8_t) 1);

	auto selectedRows = uiElementsList.getSelectedRows();
	for (auto row : selectedRows)
	{
		juce::String selectedElementName = uiElementsList.getSelectedElementNameForIndex (row);
		const int pi = uiElementsList.getPaletteIndexForRow (row);
		juce::Colour applied = newColor;
		if (! applyAlpha && pi >= 0)
			applied = newColor.withAlpha (sharedColors.colourAt (pi).getFloatAlpha());
		else if (! applyAlpha)
		{
			if (auto* existing = sharedColors.findByDisplayName (selectedElementName))
				applied = newColor.withAlpha (existing->getFloatAlpha());
		}

		if (pi >= 0)
			sharedColors.setColourAt (pi, applied, true);
		else
			sharedColors.setColourByDisplayName (selectedElementName, applied, true);
		applyColourSideEffects (selectedElementName, applied);
	}

	sharedColors.colorRandomizationFlags = std::move (originalFlags);

	colorSwatch.setColor (newColor);
	colorValuesInput.setColor (newColor);
	colorValuesInput.updateHueSlider (newColor.getHue() * 255.0f);
	colorValuesInput.updateSaturationSlider (newColor.getSaturation() * 255.0f);
	colorValuesInput.updateBrightnessSlider (newColor.getBrightness() * 255.0f);
	uiElementsList.updateSelectedElementColor (newColor);

	// Never push setColor back into the pad while scrubbing.
	if (! quadPicker.isDraggingColour())
		quadPicker.setColor (newColor);

	repaintComponents();

	if (! quadPicker.isDraggingColour())
		updateAllComponents();

	notifyThemeLiveChanged();
}

void AppearanceComponent::onElementSelected(const juce::String& name, const juce::Colour& color)
{
	DBG("onElemenetSelected Called");
	juce::Colour colorToSet = color;
	if (auto* c = sharedResources.sharedColors.findByDisplayName (name))
		colorToSet = *c;

	currentColor = colorToSet;
	colorValuesInput.updateSaturationSlider(currentColor.getSaturation() * 255.0f);
	colorValuesInput.updateBrightnessSlider(currentColor.getBrightness() * 255.0f);
	colorValuesInput.updateHueSlider(currentColor.getHue() * 255.0f);
	colorValuesInput.setColor(colorToSet);
	quadPicker.setColor(colorToSet);
	hueSelector.setColor(colorToSet);
}
void AppearanceComponent::colorChanged(float newHue)
{
	DBG("colorChanged Called");
	// Get the current color from the QuadPicker
	juce::Colour currentColor = quadPicker.getSelectedColor();

	// Adjust the hue of the current color while preserving its alpha value
	juce::Colour newColor = juce::Colour::fromHSV(
		newHue,
		currentColor.getSaturation(),
		currentColor.getBrightness(),
		currentColor.getFloatAlpha()
	);

	//DBG("New Color Saturation: " + juce::String(newColor.getSaturation()));

	// Update QuadPicker
	quadPicker.setColor(newColor);

	// Update Swatch and ColorValuesInput
	colorSwatch.setColor(newColor);
	colorValuesInput.setColor(newColor);

	uiElementsList.updateSelectedElementColor(newColor);

	const auto selectedName = uiElementsList.getSelectedElementName();
	{
		juce::Colour applied = newColor;
		const int pi = ThemeColorRegistry::indexForDisplayName (selectedName);
		if (pi >= 0)
			applied = newColor.withAlpha (sharedResources.sharedColors.colourAt (pi).getFloatAlpha());
		sharedResources.sharedColors.setColourByDisplayName (selectedName, applied, true);
		applyColourSideEffects (selectedName, applied);
	}

	// Ensure the onColorChanged callback is invoked to update the GUI
	if (hueSelector.onColorChanged)
	{
		hueSelector.onColorChanged(newColor);
	}

	if (colorValuesInput.onColorChanged)
	{
		colorValuesInput.onColorChanged(newColor);
	}
	setRangeSliderColors();
	notifyThemeLiveChanged();
}

void AppearanceComponent::updateColorSelectors(const juce::Array<juce::Colour>& colors)
{
	DBG("updateComponents Called");
	for (auto& color : colors)
	{
		//eyedropper.setColor(color);
		quadPicker.setColor(color);
		hueSelector.setColor(color);
		colorSwatch.setColor(color);
		colorValuesInput.setColor(color);
	}
}

// In AppearanceComponent.cpp
void AppearanceComponent::onPresetApplied(const Theme& theme)
{
	DBG("onPresetApplied Called");
	// ThemeList already applied palette colours and preserved randomize flags.
	// Do not re-assign theme.getColors() here - that would restore stale dice
	// scope flags from the Theme snapshot and kill button/menu randomize.
	juce::ignoreUnused (theme);

	// Get the currently selected row index
	int selectedRowIndex = uiElementsList.getSelectedRowIndex();

	// Ensure the UIElementsList selects the current row
	if (selectedRowIndex >= 0 && uiElementsList.getNumRows() > selectedRowIndex) {
		uiElementsList.selectAndNotify(selectedRowIndex, true);
		// Update other components as necessary
	}

	juce::Colour currentColor = quadPicker.getSelectedColor();
	updateColorSelectors(currentColor);

	// Update the look and feel colors with the ones from the sharedResources
	textButtonLookAndFeel.setGradientColor1(sharedResources.sharedColors.menuButtonGradientColor1);
	textButtonLookAndFeel.setGradientColor2(sharedResources.sharedColors.menuButtonGradientColor2);
	textButtonLookAndFeel.setButtonOutlineColor(sharedResources.sharedColors.menuThinBorderColor);
	textButtonLookAndFeel.setButtonTextColor(sharedResources.sharedColors.menuButtonTextColor1);

	textButtonLookAndFeel2.setGradientColor1(sharedResources.sharedColors.menuButtonGradientColor1);
	textButtonLookAndFeel2.setGradientColor2(sharedResources.sharedColors.menuButtonGradientColor2);
	textButtonLookAndFeel2.setButtonOutlineColor(sharedResources.sharedColors.menuThinBorderColor);
	textButtonLookAndFeel2.setButtonTextColor(sharedResources.sharedColors.menuButtonTextColor1);

	// Apply theme colors to customTwoValueSliderLookAndFeel
	customTwoValueSliderLookAndFeel.applyThemeColors(
		sharedResources.sharedColors.menuScrollBarTrackColor1,
		sharedResources.sharedColors.menuSliderFillColor,
		sharedResources.sharedColors.menuBackgroundGradientColor1);

	// Trigger a repaint on all components using the TextButtonLookAndFeel
	for (auto* button : buttonsUsingCustomLookAndFeel) {
		button->setLookAndFeel(&textButtonLookAndFeel);
		button->repaint();
	}

	// Trigger a repaint on all components using the TextButtonLookAndFeel
	for (auto* button : buttonsUsingCustomLookAndFeel2) {
		button->setLookAndFeel(&textButtonLookAndFeel2);
		button->repaint();
	}

	// Trigger a repaint on all components using the customTwoThumbLookAndFeel
	for (auto* slider : slidersUsingCustomLookAndFeel) {
		slider->setLookAndFeel(&customTwoValueSliderLookAndFeel);
		slider->repaint();
	}

	juce::Colour menuLabelTextColor = sharedResources.sharedColors.menuLabelTextColor1;

	// Set the text color for the section labels
	coloursSectionLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	themesLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	randomizeSectionLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	chromeSection.repaint();
	optionBoxOpacityLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	optionBoxOpacityPercentLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	buttonCornerRadiusLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	buttonCornerRadiusValueLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	menuPopupRadiusLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	menuPopupRadiusValueLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	uiFontLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	cursorInfoSizeLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	cursorInfoSizeValueLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	textContrastLabel.setColour (juce::Label::textColourId, menuLabelTextColor);

	juce::Colour trackColor = sharedResources.sharedColors.menuScrollBarTrackColor1;
	juce::Colour thumbColor = sharedResources.sharedColors.menuScrollBarThumbColor1;
	juce::Colour outlineColor = sharedResources.sharedColors.menuScrollBarOutlineColor1;

	themeList.updateScrollBarColors(trackColor, thumbColor, outlineColor);
	uiElementsList.updateScrollBarColors(trackColor, thumbColor, outlineColor);

	colorValuesInput.updateSaturationSlider(currentColor.getSaturation() * 255.0f);
	colorValuesInput.updateBrightnessSlider(currentColor.getBrightness() * 255.0f);
	colorValuesInput.updateHueSlider(currentColor.getHue() * 255.0f);

	saturationRangeSlider.setMinValue(saturationRangeSlider.getMinValue());
	saturationRangeSlider.setMaxValue(saturationRangeSlider.getMaxValue());

	brightnessRangeSlider.setMinValue(brightnessRangeSlider.getMinValue());
	brightnessRangeSlider.setMaxValue(brightnessRangeSlider.getMaxValue());

	float lowerSaturation = saturationRangeSlider.getMinValue();
	float upperSaturation = saturationRangeSlider.getMaxValue();
	sharedResources.sharedColors.setSaturationRange(lowerSaturation, upperSaturation);

	float lowerBrightness = brightnessRangeSlider.getMinValue();
	float upperBrightness = brightnessRangeSlider.getMaxValue();
	sharedResources.sharedColors.setBrightnessRange(lowerBrightness, upperBrightness);

	addAndMakeVisible(popup);
	popup.setVisible(false);


	setRangeSliderColors();
	//initializeComponents();
	repaintComponents();
	repaintParentComponent();
}

void AppearanceComponent::showPopup(const juce::String& message) {
	popup.setMessage(message);
	popup.setMode(CustomPopup::TransparentNotify); // or CustomPopup::Normal
	popup.setColors(juce::Colours::transparentBlack, juce::Colours::transparentBlack, juce::Colours::white); // Customize colors
	popup.setVisible(true);
	popup.toFront(false);
}

void AppearanceComponent::setRangeSliderColors() {
	DBG("setRangeSliderColors Called");

	colorValuesInput.setSliderBackgroundColor(sharedResources.sharedColors.menuSliderFillColor);
	colorValuesInput.setSliderTrackColor(sharedResources.sharedColors.menuScrollBarTrackColor1);
	colorValuesInput.setSliderThumbColor(sharedResources.sharedColors.menuBackgroundGradientColor1);
	colorValuesInput.setTextBoxTextColor(sharedResources.sharedColors.menuTextBoxTextColor1);
	colorValuesInput.setLabelTextColor(sharedResources.sharedColors.menuLabelTextColor1);
}

void AppearanceComponent::repaintNewPresetButton() {
	DBG("repaintNewPresetButton Called");
	newPresetButton.repaint();
}

void AppearanceComponent::setupLabels() {
	DBG("setupLabels Called");
	const auto menuLabelTextColor = sharedResources.sharedColors.menuLabelTextColor1;
	const auto sectionFont = sharedResources.sharedColors.makeUiFont (14.0f, true);
	const auto bodyFont = sharedResources.sharedColors.makeUiFont (12.0f);

	auto styleSection = [&] (juce::Label& lab, const juce::String& text)
	{
		lab.setText (text, juce::dontSendNotification);
		lab.setFont (sectionFont);
		lab.setColour (juce::Label::textColourId, menuLabelTextColor);
		lab.setMinimumHorizontalScale (1.0f);
		lab.setJustificationType (juce::Justification::centredLeft);
		addAndMakeVisible (lab);
	};

	styleSection (coloursSectionLabel, "Colours");
	styleSection (themesLabel, "Themes");
	styleSection (randomizeSectionLabel, "Randomize");

	textContrastLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	textContrastLabel.setFont (bodyFont);
	textContrastLabel.setMinimumHorizontalScale (1.0f);

	optionBoxOpacityLabel.setText ("Option box opacity", juce::dontSendNotification);
	optionBoxOpacityLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	optionBoxOpacityLabel.setFont (bodyFont);
	optionBoxOpacityLabel.setMinimumHorizontalScale (1.0f);

	optionBoxOpacityPercentLabel.setFont (bodyFont);
	optionBoxOpacityPercentLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	optionBoxOpacityPercentLabel.setMinimumHorizontalScale (1.0f);

	buttonCornerRadiusLabel.setText ("Button radius", juce::dontSendNotification);
	buttonCornerRadiusLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	buttonCornerRadiusLabel.setFont (bodyFont);
	buttonCornerRadiusLabel.setMinimumHorizontalScale (1.0f);

	buttonCornerRadiusValueLabel.setFont (bodyFont);
	buttonCornerRadiusValueLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	buttonCornerRadiusValueLabel.setMinimumHorizontalScale (1.0f);

	menuPopupRadiusLabel.setText ("Menu radius", juce::dontSendNotification);
	menuPopupRadiusLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	menuPopupRadiusLabel.setFont (bodyFont);
	menuPopupRadiusLabel.setMinimumHorizontalScale (1.0f);

	menuPopupRadiusValueLabel.setFont (bodyFont);
	menuPopupRadiusValueLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	menuPopupRadiusValueLabel.setMinimumHorizontalScale (1.0f);

	menuPopupOutlineToggle.setColour (juce::ToggleButton::textColourId, menuLabelTextColor);
	menuPopupOutlineToggle.setColour (juce::ToggleButton::tickColourId, sharedResources.sharedColors.menuSliderFillColor);

	uiFontLabel.setText ("UI font", juce::dontSendNotification);
	uiFontLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	uiFontLabel.setFont (bodyFont);
	uiFontLabel.setMinimumHorizontalScale (1.0f);

	cursorInfoSizeLabel.setText ("Cursor info size", juce::dontSendNotification);
	cursorInfoSizeLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	cursorInfoSizeLabel.setFont (bodyFont);
	cursorInfoSizeLabel.setMinimumHorizontalScale (1.0f);

	cursorInfoSizeValueLabel.setFont (bodyFont);
	cursorInfoSizeValueLabel.setColour (juce::Label::textColourId, menuLabelTextColor);
	cursorInfoSizeValueLabel.setMinimumHorizontalScale (1.0f);

	uiFontBoldToggle.setColour (juce::ToggleButton::textColourId, menuLabelTextColor);
	uiFontBoldToggle.setColour (juce::ToggleButton::tickColourId, sharedResources.sharedColors.menuSliderFillColor);
	buttonGlowToggle.setColour (juce::ToggleButton::textColourId, menuLabelTextColor);
	buttonGlowToggle.setColour (juce::ToggleButton::tickColourId, sharedResources.sharedColors.menuSliderFillColor);
	buttonGlowHoverOnlyToggle.setColour (juce::ToggleButton::textColourId, menuLabelTextColor);
	buttonGlowHoverOnlyToggle.setColour (juce::ToggleButton::tickColourId, sharedResources.sharedColors.menuSliderFillColor);

	enforceLegibleTextToggle.setColour (juce::ToggleButton::textColourId, menuLabelTextColor);
	enforceLegibleTextToggle.setColour (juce::ToggleButton::tickColourId, sharedResources.sharedColors.menuSliderFillColor);
	optionBoxOpacitySlider.setColour (juce::Slider::trackColourId, sharedResources.sharedColors.menuSliderFillColor);
	optionBoxOpacitySlider.setColour (juce::Slider::thumbColourId, sharedResources.sharedColors.menuSliderFillColor.brighter (0.15f));
	buttonCornerRadiusSlider.setColour (juce::Slider::trackColourId, sharedResources.sharedColors.menuSliderFillColor);
	buttonCornerRadiusSlider.setColour (juce::Slider::thumbColourId, sharedResources.sharedColors.menuSliderFillColor.brighter (0.15f));
	menuPopupRadiusSlider.setColour (juce::Slider::trackColourId, sharedResources.sharedColors.menuSliderFillColor);
	menuPopupRadiusSlider.setColour (juce::Slider::thumbColourId, sharedResources.sharedColors.menuSliderFillColor.brighter (0.15f));
	textContrastSlider.setColour (juce::Slider::trackColourId, sharedResources.sharedColors.menuSliderFillColor);
	textContrastSlider.setColour (juce::Slider::thumbColourId, sharedResources.sharedColors.menuSliderFillColor.brighter (0.15f));
	cursorInfoSizeSlider.setColour (juce::Slider::trackColourId, sharedResources.sharedColors.menuSliderFillColor);
	cursorInfoSizeSlider.setColour (juce::Slider::thumbColourId, sharedResources.sharedColors.menuSliderFillColor.brighter (0.15f));
}

void AppearanceComponent::updateAllComponents() {
	DBG("updateAllComponents Called");
	auto selectedIndices = uiElementsList.getSelectedRows();
	for (auto index : selectedIndices) {
		// Assuming each index corresponds to a row and color
		auto colorToUpdate = getColorForIndex(index); // Implement this method to get the color for a given index
		updateComponents(colorToUpdate);

		//quadPicker.setColor(colorToUpdate);
		hueSelector.setColor(colorToUpdate);
		colorSwatch.setColor(colorToUpdate);
	}
}

juce::Colour AppearanceComponent::getColorForIndex(int index) {
	DBG("getColorForIndex Called");
	return uiElementsList.getElementColor (index);
}

void AppearanceComponent::applyColourSideEffects (const juce::String& elementName, juce::Colour newColor)
{
	if (elementName == "Menu Background 2")
	{
		customTwoValueSliderLookAndFeel.setSliderThumbColor (sharedResources.sharedColors.menuBackgroundGradientColor2);
		saturationRangeSlider.setColour (juce::Slider::thumbColourId, newColor);
		hueRangeSlider.setColour (juce::Slider::thumbColourId, newColor);
		brightnessRangeSlider.setColour (juce::Slider::thumbColourId, newColor);
	}
	else if (elementName == "Menu Thin Border")
	{
		textButtonLookAndFeel.setButtonOutlineColor (newColor);
	}
	else if (elementName == "Menu Button Background")
	{
		textButtonLookAndFeel.setGradientColor1 (newColor);
		saturationRangeSlider.setColour (juce::Slider::thumbColourId, newColor);
		hueRangeSlider.setColour (juce::Slider::thumbColourId, newColor);
		brightnessRangeSlider.setColour (juce::Slider::thumbColourId, newColor);
	}
	else if (elementName == "Menu Button Background 2")
	{
		textButtonLookAndFeel.setGradientColor2 (newColor);
	}
	else if (elementName == "Menu Button Text")
	{
		textButtonLookAndFeel.setButtonTextColor (newColor);
	}
	else if (elementName == "Menu Label Text")
	{
		colorValuesInput.setLabelTextColor (newColor);
		coloursSectionLabel.setColour (juce::Label::textColourId, newColor);
		themesLabel.setColour (juce::Label::textColourId, newColor);
		randomizeSectionLabel.setColour (juce::Label::textColourId, newColor);
		chromeSection.repaint();
		textContrastLabel.setColour (juce::Label::textColourId, newColor);
		optionBoxOpacityLabel.setColour (juce::Label::textColourId, newColor);
		optionBoxOpacityPercentLabel.setColour (juce::Label::textColourId, newColor);
		buttonCornerRadiusLabel.setColour (juce::Label::textColourId, newColor);
		buttonCornerRadiusValueLabel.setColour (juce::Label::textColourId, newColor);
		uiFontLabel.setColour (juce::Label::textColourId, newColor);
	}
	else if (elementName == "Menu Scroll Track")
	{
		customTwoValueSliderLookAndFeel.setSliderTrackColor (newColor);
		colorValuesInput.setSliderTrackColor (newColor);
		saturationRangeSlider.setColour (juce::Slider::backgroundColourId, newColor);
		hueRangeSlider.setColour (juce::Slider::backgroundColourId, newColor);
		brightnessRangeSlider.setColour (juce::Slider::backgroundColourId, newColor);
		themeList.updateScrollBarColors (newColor, sharedResources.sharedColors.menuScrollBarThumbColor1, sharedResources.sharedColors.menuScrollBarOutlineColor1);
		uiElementsList.updateScrollBarColors (newColor, sharedResources.sharedColors.menuScrollBarThumbColor1, sharedResources.sharedColors.menuScrollBarOutlineColor1);
	}
	else if (elementName == "Menu Scroll Thumb")
	{
		themeList.updateScrollBarColors (sharedResources.sharedColors.menuScrollBarTrackColor1, newColor, sharedResources.sharedColors.menuScrollBarOutlineColor1);
		uiElementsList.updateScrollBarColors (sharedResources.sharedColors.menuScrollBarTrackColor1, newColor, sharedResources.sharedColors.menuScrollBarOutlineColor1);
	}
	else if (elementName == "Menu Scroll Outline")
	{
		themeList.updateScrollBarColors (sharedResources.sharedColors.menuScrollBarTrackColor1, sharedResources.sharedColors.menuScrollBarThumbColor1, newColor);
		uiElementsList.updateScrollBarColors (sharedResources.sharedColors.menuScrollBarTrackColor1, sharedResources.sharedColors.menuScrollBarThumbColor1, newColor);
	}
	else if (elementName == "Menu Slider Fill")
	{
		customTwoValueSliderLookAndFeel.setSliderBackgroundColor (newColor);
		colorValuesInput.setSliderBackgroundColor (newColor);
		saturationRangeSlider.setColour (juce::Slider::trackColourId, newColor);
		brightnessRangeSlider.setColour (juce::Slider::trackColourId, newColor);
		hueRangeSlider.setColour (juce::Slider::trackColourId, newColor);
	}
	else if (elementName == "Menu TextBox Text")
	{
		colorValuesInput.setTextBoxTextColor (newColor);
	}

	if (auto* top = getTopLevelComponent())
		top->repaint();
}

void AppearanceComponent::notifyThemeLiveChanged()
{
	if (onThemeLiveChanged)
		onThemeLiveChanged();
}

void AppearanceComponent::applyUiFontPreview (const juce::String& fontName, bool persist)
{
	if (fontName.isNotEmpty())
		sharedResources.sharedColors.uiFontName = fontName;

	sharedResources.makeActive();
	SharedResources::applyUiFontsRecursively (*this);

	if (auto* menu = findParentComponentOfClass<Menu>())
		menu->refreshUiFonts();

	if (auto* top = getTopLevelComponent())
		if (top != this)
			SharedResources::applyUiFontsRecursively (*top);

	themeList.repaint();
	uiElementsList.repaint();

	if (persist)
		notifyThemeLiveChanged();
	else
		repaint();
}

void AppearanceComponent::refreshAfterRandomize()
{
	sharedResources.makeActive();

	textButtonLookAndFeel.setButtonTextColor(sharedResources.sharedColors.menuButtonTextColor1);
	textButtonLookAndFeel.setGradientColor1(sharedResources.sharedColors.menuButtonGradientColor1);
	textButtonLookAndFeel.setGradientColor2(sharedResources.sharedColors.menuButtonGradientColor2);
	textButtonLookAndFeel.setButtonOutlineColor(sharedResources.sharedColors.menuThinBorderColor);

	textButtonLookAndFeel2.setButtonTextColor(sharedResources.sharedColors.menuButtonTextColor1);
	textButtonLookAndFeel2.setGradientColor1(sharedResources.sharedColors.menuButtonGradientColor1);
	textButtonLookAndFeel2.setGradientColor2(sharedResources.sharedColors.menuButtonGradientColor2);
	textButtonLookAndFeel2.setButtonOutlineColor(sharedResources.sharedColors.menuThinBorderColor);

	customTwoValueSliderLookAndFeel.applyThemeColors(
		sharedResources.sharedColors.menuScrollBarTrackColor1,
		sharedResources.sharedColors.menuSliderFillColor,
		sharedResources.sharedColors.menuBackgroundGradientColor1);

	colorValuesInput.updateSaturationSlider(currentColor.getSaturation() * 255.0f);
	colorValuesInput.updateBrightnessSlider(currentColor.getBrightness() * 255.0f);
	colorValuesInput.updateHueSlider(currentColor.getHue() * 255.0f);

	setRangeSliderColors();
	setupLabels();
	setButtonLookAndFeels();
	setSliderLookAndFeels();

	juce::Colour trackColor = sharedResources.sharedColors.menuScrollBarTrackColor1;
	juce::Colour thumbColor = sharedResources.sharedColors.menuScrollBarThumbColor1;
	juce::Colour outlineColor = sharedResources.sharedColors.menuScrollBarOutlineColor1;

	themeList.updateScrollBarColors(trackColor, thumbColor, outlineColor);
	uiElementsList.updateScrollBarColors(trackColor, thumbColor, outlineColor);

	updateAllComponents();
	repaintComponents();
	repaintParentComponent();
	notifyThemeLiveChanged();
}

void AppearanceComponent::showRandomizeScopeMenu()
{
	auto& scopes = sharedResources.sharedColors;
	juce::PopupMenu menu;
	menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
	menu.addItem (1, "Faceplate/Mod", true, scopes.randomizeFaceplateMod);
	menu.addItem (2, "Graph", true, scopes.randomizeGraphModule);
	menu.addItem (3, "Menu", true, scopes.randomizeMenuModule);

	menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&randomizeColorsButton),
	                    [this] (int result)
	                    {
		                    if (result <= 0)
			                    return;

		                    auto& s = sharedResources.sharedColors;
		                    if (result == 1)
			                    s.randomizeFaceplateMod = ! s.randomizeFaceplateMod;
		                    else if (result == 2)
			                    s.randomizeGraphModule = ! s.randomizeGraphModule;
		                    else if (result == 3)
			                    s.randomizeMenuModule = ! s.randomizeMenuModule;
	                    });
}

void AppearanceComponent::setButtonLookAndFeels()
{
	DBG("setButtonLookAndFeels Called");
	newPresetButton.setLookAndFeel(&textButtonLookAndFeel);
	overwritePresetButton.setLookAndFeel(&textButtonLookAndFeel);
	deletePresetButton.setLookAndFeel(&textButtonLookAndFeel);
	randomizeColorsButton.setLookAndFeel(&textButtonLookAndFeel);
	randomizeSelectedColorsButton.setLookAndFeel(&textButtonLookAndFeel);

	randomizeHueToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeSaturationToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeBrightnessToggleButton.setLookAndFeel(&textButtonLookAndFeel2);
	randomizeAlphaToggleButton.setLookAndFeel(&textButtonLookAndFeel2);

}

void AppearanceComponent::setSliderLookAndFeels()
{
	DBG("setSliderLookAndFeels Called");
	
	hueRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
	saturationRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
	brightnessRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
}

void AppearanceComponent::sliderValueChanged(juce::Slider* slider) {
	DBG("sliderValueChanged Called");

	if (slider == &hueRangeSlider) {
		// Directly use the slider values
		hueRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
		float lowerHue = hueRangeSlider.getMinValue();
		float upperHue = hueRangeSlider.getMaxValue();
		sharedResources.sharedColors.setHueRange(lowerHue, upperHue);
		hueRangeSlider.lookAndFeelChanged();	
		hueRangeSlider.repaint();
	}

	if (slider == &saturationRangeSlider) {
		saturationRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
		float lowerSaturation = saturationRangeSlider.getMinValue();
		float upperSaturation = saturationRangeSlider.getMaxValue();
		sharedResources.sharedColors.setSaturationRange(lowerSaturation, upperSaturation);
		saturationRangeSlider.lookAndFeelChanged();
		saturationRangeSlider.repaint();
	}

	if (slider == &brightnessRangeSlider) {
		//DBG("Brightness Range Slider changed");
		brightnessRangeSlider.setLookAndFeel(&customTwoValueSliderLookAndFeel);
		float lowerBrightness = brightnessRangeSlider.getMinValue();
		float upperBrightness = brightnessRangeSlider.getMaxValue();
		sharedResources.sharedColors.setBrightnessRange(lowerBrightness, upperBrightness);
		brightnessRangeSlider.lookAndFeelChanged();
		brightnessRangeSlider.repaint();
	
		//DBG("Brightness Range Slider Value Changed: Min = " << slider->getMinValue() << ", Max = " << slider->getMaxValue());
	}

	if (slider == &textContrastSlider)
	{
		sharedResources.sharedColors.textContrastAmount = (float) textContrastSlider.getValue();
		if (sharedResources.sharedColors.enforceLegibleText)
			applyAccessibilityTextContrast();
	}

	if (slider == &optionBoxOpacitySlider)
	{
		sharedResources.sharedColors.optionBoxOpacity =
		    juce::jlimit (0.30f, 1.0f, (float) optionBoxOpacitySlider.getValue());
		const int pct = juce::roundToInt (sharedResources.sharedColors.optionBoxOpacity * 100.0f);
		optionBoxOpacityPercentLabel.setText (juce::String (pct) + "%", juce::dontSendNotification);
		// Live update floating OptionBox without full theme reload.
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	}

	if (slider == &buttonCornerRadiusSlider)
	{
		sharedResources.sharedColors.buttonCornerRadius =
		    juce::jlimit (2.0f, 16.0f, (float) buttonCornerRadiusSlider.getValue());
		const int px = juce::roundToInt (sharedResources.sharedColors.buttonCornerRadius);
		buttonCornerRadiusValueLabel.setText (juce::String (px) + " px", juce::dontSendNotification);
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	}

	if (slider == &menuPopupRadiusSlider)
	{
		sharedResources.sharedColors.menuPopupCornerRadius =
		    juce::jlimit (0.0f, 16.0f, (float) menuPopupRadiusSlider.getValue());
		const int px = juce::roundToInt (sharedResources.sharedColors.menuPopupCornerRadius);
		menuPopupRadiusValueLabel.setText (juce::String (px) + " px", juce::dontSendNotification);
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	}

	if (slider == &cursorInfoSizeSlider)
	{
		sharedResources.sharedColors.graphCursorInfoFontSize =
		    juce::jlimit (8.0f, 24.0f, (float) cursorInfoSizeSlider.getValue());
		const juce::String sizeText = juce::String (sharedResources.sharedColors.graphCursorInfoFontSize, 1)
		                            + " pt";
		cursorInfoSizeValueLabel.setText (sizeText, juce::dontSendNotification);
		if (onThemeLiveChanged)
			onThemeLiveChanged();
		else
			refreshAfterRandomize();
	}
}

void AppearanceComponent::syncAccessibilityControls()
{
	const bool on = sharedResources.sharedColors.enforceLegibleText;
	enforceLegibleTextToggle.setToggleState (on, juce::dontSendNotification);
	textContrastSlider.setValue (sharedResources.sharedColors.textContrastAmount, juce::dontSendNotification);
	textContrastSlider.setVisible (on);
	textContrastLabel.setVisible (on);
	textContrastSlider.setEnabled (on);
	optionBoxOpacitySlider.setValue (sharedResources.sharedColors.optionBoxOpacity, juce::dontSendNotification);
	const int pct = juce::roundToInt (sharedResources.sharedColors.optionBoxOpacity * 100.0f);
	optionBoxOpacityPercentLabel.setText (juce::String (pct) + "%", juce::dontSendNotification);
	buttonCornerRadiusSlider.setValue (sharedResources.sharedColors.buttonCornerRadius,
	                                   juce::dontSendNotification);
	const int px = juce::roundToInt (sharedResources.sharedColors.buttonCornerRadius);
	buttonCornerRadiusValueLabel.setText (juce::String (px) + " px", juce::dontSendNotification);
	menuPopupRadiusSlider.setValue (sharedResources.sharedColors.menuPopupCornerRadius,
	                                juce::dontSendNotification);
	menuPopupRadiusValueLabel.setText (
	    juce::String (juce::roundToInt (sharedResources.sharedColors.menuPopupCornerRadius)) + " px",
	    juce::dontSendNotification);
	menuPopupOutlineToggle.setToggleState (sharedResources.sharedColors.menuPopupOutline,
	                                       juce::dontSendNotification);
	{
		const auto fonts = SharedColors::getUiFontCatalogue();
		const int idx = fonts.indexOf (sharedResources.sharedColors.uiFontName);
		uiFontCombo.setSelectedId (idx >= 0 ? idx + 1 : 1, juce::dontSendNotification);
		committedUiFontName = sharedResources.sharedColors.uiFontName;
	}
	uiFontBoldToggle.setToggleState (sharedResources.sharedColors.uiFontBold, juce::dontSendNotification);
	cursorInfoSizeSlider.setValue (sharedResources.sharedColors.graphCursorInfoFontSize,
	                               juce::dontSendNotification);
	cursorInfoSizeValueLabel.setText (
	    juce::String (sharedResources.sharedColors.graphCursorInfoFontSize, 1) + " pt",
	    juce::dontSendNotification);
	buttonGlowToggle.setToggleState (sharedResources.sharedColors.buttonGlowEnabled,
	                                 juce::dontSendNotification);
	buttonGlowHoverOnlyToggle.setToggleState (sharedResources.sharedColors.buttonGlowOnlyOnHover,
	                                          juce::dontSendNotification);
	buttonGlowHoverOnlyToggle.setEnabled (sharedResources.sharedColors.buttonGlowEnabled);
}

void AppearanceComponent::applyAccessibilityTextContrast()
{
	// Enforce is the final pass - do not syncFaceplateModScheme afterward (rewrites
	// Mod/Option text from Plugin Button Text without re-checking panel backgrounds).
	sharedResources.sharedColors.enforceLegibleTextContrast();
	refreshAfterRandomize();
}

void AppearanceComponent::repaintParentComponent()
{
	DBG("repaintParentComponent Called");

	auto* parentMenu = findParentComponentOfClass<Menu>();
	if (parentMenu)
	{
		juce::Array<juce::Colour> menuColors;
		menuColors.add(sharedResources.sharedColors.menuBackgroundGradientColor1);
		menuColors.add(sharedResources.sharedColors.menuBackgroundGradientColor2);
		menuColors.add(sharedResources.sharedColors.menuListBoxBackgroundGradientColor1);
		menuColors.add(sharedResources.sharedColors.menuListBoxBackgroundGradientColor2);
		menuColors.add(sharedResources.sharedColors.menuTabBarBorderColor);
		menuColors.add(sharedResources.sharedColors.menuThinBorderColor);
		menuColors.add(sharedResources.sharedColors.menuButtonGradientColor1);
		menuColors.add(sharedResources.sharedColors.menuButtonGradientColor2);
		menuColors.add(sharedResources.sharedColors.menuButtonTextColor1);
		menuColors.add(sharedResources.sharedColors.menuLabelTextColor1);
		menuColors.add(sharedResources.sharedColors.menuScrollBarTrackColor1);
		menuColors.add(sharedResources.sharedColors.menuScrollBarThumbColor1);
		menuColors.add(sharedResources.sharedColors.menuScrollBarOutlineColor1);
		menuColors.add(sharedResources.sharedColors.menuListBoxTextColor1);
		menuColors.add(sharedResources.sharedColors.menuListBoxSelectionColor1);
		menuColors.add(sharedResources.sharedColors.menuTextBoxTextColor1);
		parentMenu->updateColors(menuColors);
	}
}

void AppearanceComponent::repaintComponents()
{
	DBG("repaintComponents Called");
	newPresetButton.repaint();
	deletePresetButton.repaint();
	overwritePresetButton.repaint();
	randomizeColorsButton.repaint();
	randomizeHueToggleButton.repaint();
	randomizeBrightnessToggleButton.repaint();
	randomizeSaturationToggleButton.repaint();
	randomizeAlphaToggleButton.repaint();
	quadPicker.repaint();
	hueSelector.repaint();
	colorValuesInput.repaint();
	themesLabel.repaint();
	coloursSectionLabel.repaint();
	randomizeSectionLabel.repaint();
	chromeSection.repaint();
	optionBoxOpacityLabel.repaint();
	optionBoxOpacityPercentLabel.repaint();
	optionBoxOpacitySlider.repaint();
	buttonCornerRadiusLabel.repaint();
	buttonCornerRadiusValueLabel.repaint();
	buttonCornerRadiusSlider.repaint();
	uiFontLabel.repaint();
	uiFontCombo.repaint();
	uiFontBoldToggle.repaint();
	buttonGlowToggle.repaint();
	buttonGlowHoverOnlyToggle.repaint();
	textContrastLabel.repaint();
	textContrastSlider.repaint();
	enforceLegibleTextToggle.repaint();
	uiElementsList.repaint();
	themeList.repaint();
	hueRangeSlider.repaint();
	saturationRangeSlider.repaint();
	brightnessRangeSlider.repaint();
	repaintParentComponent();
	
}

juce::Rectangle<int> AppearanceComponent::getThemeListBounds() const {
	return themeList.getBounds(); // Assuming themeList is a member
}

void AppearanceComponent::UiFontPreviewCombo::mouseDown (const juce::MouseEvent&)
{
	if (isEnabled())
		toggleFontList();
}

void AppearanceComponent::UiFontPreviewCombo::showPopup()
{
	toggleFontList();
}

void AppearanceComponent::UiFontPreviewCombo::hidePopup()
{
	dismissFontList (true);
}

void AppearanceComponent::UiFontPreviewCombo::toggleFontList()
{
	if (fontWindow != nullptr)
		dismissFontList (false);
	else
		launchFontList();
}

void AppearanceComponent::UiFontPreviewCombo::dismissFontList (bool committed)
{
	if (auto* w = fontWindow.getComponent())
	{
		fontWindow = nullptr;
		w->setVisible (false);
		if (w->isOnDesktop())
			w->removeFromDesktop();
		// Never delete from a list-click stack frame on this window.
		juce::MessageManager::callAsync ([w] { delete w; });
	}
	else
	{
		fontWindow = nullptr;
	}

	if (! committed && onMenuDismissedWithoutChoice != nullptr)
		onMenuDismissedWithoutChoice();
}

void AppearanceComponent::UiFontPreviewCombo::launchFontList()
{
	const auto fonts = SharedColors::getUiFontCatalogue();
	if (fonts.isEmpty())
		return;

	auto* win = new FontCatalogueWindow (
	    fonts,
	    getSelectedId(),
	    this,
	    onHoverPreview,
	    [safe = juce::Component::SafePointer<UiFontPreviewCombo> (this)] (int id)
	    {
	        if (safe == nullptr)
	            return;
	        safe->dismissFontList (true);
	        juce::MessageManager::callAsync ([safe, id]
	        {
	            if (safe != nullptr)
	                safe->setSelectedId (id, juce::sendNotificationSync);
	        });
	    },
	    [safe = juce::Component::SafePointer<UiFontPreviewCombo> (this)]
	    {
	        if (safe != nullptr)
	            safe->dismissFontList (false);
	    });

	const auto screen = localAreaToGlobal (getLocalBounds());
	constexpr int kVisibleRows = 16;
	const int h = juce::jmin (fonts.size(), kVisibleRows) * kFontRowH + 12;
	win->setBounds (screen.getX(), screen.getBottom(),
	                juce::jmax (screen.getWidth(), 280), juce::jmax (h, kFontRowH * 8 + 12));
	win->setAlwaysOnTop (true);
	win->addToDesktop (juce::ComponentPeer::windowIsTemporary
	                   | juce::ComponentPeer::windowHasDropShadow);
	win->setVisible (true);
	win->toFront (false);
	fontWindow = win;
}
