#include "AppearanceComponent.h"
#include "../ThemeColorRegistry.h"
#include <algorithm>
#include <cstdint>
#include "../../TextButtonLookAndFeel.h"
#include "../../ComboBoxLookAndFeel.h"
#include "QuadPickerOverlayComponent.h"
#include "../Menu.h"  

AppearanceComponent::AppearanceComponent(SharedResources& resources, juce::AudioProcessorValueTreeState& state)
	: sharedResources(resources),  // Initialize the member variable
	uiElementsList(resources),
	themeList(resources),
	quadPicker(resources),
	hueSelector(resources),
	colorSwatch(*this),
	textButtonLookAndFeel(14.0f),
	overlayComponent(quadPicker, brightnessRangeSlider, saturationRangeSlider),
	overlayComponent2(hueSelector, hueRangeSlider)
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

	// Accessibility — legible text after randomize
	accessibilityLabel.setText ("Accessibility", juce::dontSendNotification);
	accessibilityLabel.setJustificationType (juce::Justification::centredLeft);
	accessibilityLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (14.0f));
	addAndMakeVisible (accessibilityLabel);

	enforceLegibleTextToggle.setClickingTogglesState (true);
	enforceLegibleTextToggle.setToggleState (sharedResources.sharedColors.enforceLegibleText,
	                                         juce::dontSendNotification);
	enforceLegibleTextToggle.setTooltip (
	    "After randomize, push text value away from its background so labels stay readable. "
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
	addAndMakeVisible (textContrastLabel);

	textContrastSlider.setRange (0.0, 1.0, 0.01);
	textContrastSlider.setValue (sharedResources.sharedColors.textContrastAmount, juce::dontSendNotification);
	textContrastSlider.setSliderStyle (juce::Slider::LinearHorizontal);
	textContrastSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
	textContrastSlider.setTooltip ("How strongly text is separated from background value (text only).");
	textContrastSlider.addListener (this);
	addAndMakeVisible (textContrastSlider);

	syncAccessibilityControls();

	randomizeSelectedColorsButton.onClick = [this] {
		auto selectedIndices = uiElementsList.getSelectedPaletteIndices();
		DBG("Selected palette indices for randomization: " + juce::String(selectedIndices.size()));
		auto& sharedColors = sharedResources.sharedColors;

		// Reset all flags to false (skip) — then enable selected palette slots.
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
		if (sharedResources.sharedColors.enforceLegibleText)
		{
			sharedResources.sharedColors.enforceLegibleTextContrast();
			if (sharedResources.sharedColors.randomizeFaceplateMod)
				sharedResources.sharedColors.syncFaceplateModScheme();
		}

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
		if (sharedResources.sharedColors.enforceLegibleText)
		{
			sharedResources.sharedColors.enforceLegibleTextContrast();
			if (sharedResources.sharedColors.randomizeFaceplateMod)
				sharedResources.sharedColors.syncFaceplateModScheme();
		}
		refreshAfterRandomize();
		};

	randomizeColorsButton.onPopupMenu = [this] { showRandomizeScopeMenu(); };

	addAndMakeVisible(randomizeSelectedColorsButton);
	randomizeSelectedColorsButton.setButtonText("Rand. Sel."); // Setting the button text
	randomizeSelectedColorsButton.setLookAndFeel(&textButtonLookAndFeel);

	addAndMakeVisible(randomizeColorsButton);
	randomizeColorsButton.setButtonText("Rand. All"); // Setting the button text
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

	// Populate UI Elements from the theme colour registry (sorted A–Z on header click).
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
	int padding = 20;
	int quadPickerScaleX = getWidth() / 3;
	int quadPickerScaleY = getHeight() * .9;
	int hueSelectorScaleX = padding * 3.5;
	int hueSelectorScaleY = quadPickerScaleY;
	int hueSelectorX = getWidth() - quadPickerScaleX - padding - hueSelectorScaleX;
	int hueSelectorY = padding;
	int quadPickerX = getWidth() - quadPickerScaleX - padding;
	int quadPickerY = padding;
	int saturationRangeSliderX = quadPickerX;
	int saturationRangeSliderY = quadPickerY - padding;
	int hueRangeSliderX = hueSelectorX + hueSelectorScaleX - 20;
	int hueRangeSliderY = quadPickerY + 2;
	int brightnessRangeSliderX = getWidth() - padding * 1.825;
	int rangeSliderWidth = 40;
	int swatchSize = 130;
	int miscellaneousColumnScaleX = swatchSize + padding;
	int listBoxWidth = getWidth() - quadPickerScaleX - hueSelectorScaleX - swatchSize * 1.6;
	int listHeight = hueSelectorScaleY / 2 - padding * 2;
	int colorValuesInputScaleX = getWidth() - listBoxWidth - hueSelectorScaleX - quadPickerScaleX - padding;
	int colorValuesInputScaleY = getHeight() - swatchSize / 2 - padding;
	int colorValueInputX = listBoxWidth + padding / 1.5;
	int colorValueInputY = swatchSize + padding * 1.3;
	int colorSwatchX = getWidth() - quadPickerScaleX - hueSelectorScaleX - padding * 1.2 - swatchSize;

	int themesListY = padding + listHeight + 2 * padding;
	int themesListX = padding;
	int elementsListY = padding * 1.5;
	int themesLabelY = themesListY - 21;


	// Shadows
	float cornerSize = 5.0f;
	quadPickerShadowPath.clear();
	quadPickerShadowPath.addRoundedRectangle(quadPickerX, quadPickerY, quadPickerScaleX, quadPickerScaleY, cornerSize);

	float cornerSize2 = 14.0f;
	uiElementsListShadowPath.clear();
	uiElementsListShadowPath.addRoundedRectangle(padding, elementsListY, listBoxWidth, listHeight, cornerSize2);

	float cornerSize3 = 14.0f;
	themesListShadowPath.clear();
	themesListShadowPath.addRoundedRectangle(padding, themesListY, listBoxWidth, listHeight, cornerSize3);




	quadPicker.setBounds(quadPickerX, quadPickerY, quadPickerScaleX, quadPickerScaleY);
	hueSelector.setBounds(hueSelectorX, hueSelectorY, hueSelectorScaleX, hueSelectorScaleY);
	colorSwatch.setBounds(colorSwatchX, padding * 1.5, swatchSize, swatchSize);
	colorValuesInput.setBounds(colorValueInputX, colorValueInputY, colorValuesInputScaleX, colorValuesInputScaleY);

	uiElementsList.setBounds(padding, elementsListY, listBoxWidth, listHeight);
	themeList.setBounds(themesListX, themesListY, listBoxWidth, listHeight);
	uiElementsLabel.setBounds(padding, padding * 1.5 - 21, listBoxWidth, 16); // 16 for height, 5 for space above
	themesLabel.setBounds(themesListX, themesLabelY, listBoxWidth, 16); // Same here

	saturationRangeSlider.setBounds(quadPickerX - 8, saturationRangeSliderY, quadPickerScaleX + 22, rangeSliderWidth);
	hueRangeSlider.setBounds(hueRangeSliderX - 8, hueSelectorY - 3, rangeSliderWidth, hueSelectorScaleY + 10);
	brightnessRangeSlider.setBounds(brightnessRangeSliderX, hueSelectorY - 7, rangeSliderWidth, hueSelectorScaleY + 22);

	// Calculate the widths and positions for the three new buttons
	int buttonHeight = 35;
	int buttonWidth = (listBoxWidth) / 3.5; // Subtract padding for two gaps and divide by 3 for three buttons
	int buttonSpacing = (listBoxWidth) / 3;
	int firstButtonX = padding;
	int secondButtonX = firstButtonX + buttonSpacing + 3;
	int thirdButtonX = secondButtonX + buttonSpacing + 3;
	int buttonY = themesLabel.getY() + listHeight + padding * 1.53;

	// Set the bounds for the three buttons
	newPresetButton.setBounds(firstButtonX, buttonY, buttonWidth, buttonHeight);
	overwritePresetButton.setBounds(secondButtonX, buttonY, buttonWidth, buttonHeight);
	deletePresetButton.setBounds(thirdButtonX, buttonY, buttonWidth, buttonHeight);

	// Button Shadows
	float cornerSize4 = 6.0f;
	newPresetButtonShadowPath.clear();
	newPresetButtonShadowPath.addRoundedRectangle(firstButtonX, buttonY, buttonWidth, buttonHeight, cornerSize4);

	overwritePresetButtonShadowPath.clear();
	overwritePresetButtonShadowPath.addRoundedRectangle(secondButtonX, buttonY, buttonWidth, buttonHeight, cornerSize4);

	deletePresetButtonShadowPath.clear();
	deletePresetButtonShadowPath.addRoundedRectangle(thirdButtonX, buttonY, buttonWidth, buttonHeight, cornerSize4);

	int randomButtonWidth = buttonWidth;
	int randomButtonHeight = 20;
	int randomSelectedButtonX = listBoxWidth / 3 + padding;
	int randomButtonX = randomSelectedButtonX + randomButtonWidth + padding / 2; // 20px padding from right
	int randomButtonY = themesLabel.getY() - padding / 5; // Align with the Themes label	
	int randomHueToggleButtonX = padding + listBoxWidth / 2 - padding / 2;
	int randomHueToggleButtonY = elementsListY - padding * 1.35;
	int randomToggleAvailableX = listBoxWidth / 2;
	int randomToggleButtonSpacing = randomToggleAvailableX / 4;

	int randomSaturationToggleButtonX = randomHueToggleButtonX + randomToggleButtonSpacing;
	int randomBrightnessToggleButtonX = randomHueToggleButtonX + randomToggleButtonSpacing * 2;
	int randomAlphaToggleButtonX = randomHueToggleButtonX + randomToggleButtonSpacing * 3;
	int popupCenteredLocationX = listBoxWidth / 2;
	int popupCenteredLocationY = themesListY + listHeight / 2;

	// Randomize Button Shadows
	randomColorsButtonShadowPath.clear();
	randomColorsButtonShadowPath.addRoundedRectangle(randomButtonX, randomButtonY, randomButtonWidth, randomButtonHeight, cornerSize4);

	randomSelectedColorsButtonShadowPath.clear();
	randomSelectedColorsButtonShadowPath.addRoundedRectangle(randomSelectedButtonX, randomButtonY, randomButtonWidth, randomButtonHeight, cornerSize4);


	randomizeColorsButton.setBounds(randomButtonX, randomButtonY, randomButtonWidth, randomButtonHeight * 1);
	randomizeSelectedColorsButton.setBounds(randomSelectedButtonX, randomButtonY, randomButtonWidth, randomButtonHeight * 1);

	randomizeHueToggleButton.setBounds(randomHueToggleButtonX, randomHueToggleButtonY, randomButtonWidth / 2.5, randomButtonHeight);
	randomizeSaturationToggleButton.setBounds(randomSaturationToggleButtonX, randomHueToggleButtonY, randomButtonWidth / 2.5, randomButtonHeight);
	randomizeBrightnessToggleButton.setBounds(randomBrightnessToggleButtonX, randomHueToggleButtonY, randomButtonWidth / 2.5, randomButtonHeight);
	randomizeAlphaToggleButton.setBounds(randomAlphaToggleButtonX, randomHueToggleButtonY, randomButtonWidth / 2.5, randomButtonHeight);

	// Accessibility row under theme New/Overwrite/Delete
	const int accessY = buttonY + buttonHeight + 10;
	const int accessH = 22;
	accessibilityLabel.setBounds (padding, accessY, 100, accessH);
	enforceLegibleTextToggle.setBounds (padding + 100, accessY, 120, accessH);
	textContrastLabel.setBounds (padding + 230, accessY, 60, accessH);
	textContrastSlider.setBounds (padding + 290, accessY, juce::jmax (80, listBoxWidth - 290), accessH);

	juce::Colour firstItemColor = sharedResources.sharedColors.menuBackgroundGradientColor1;
	quadPicker.setColor(firstItemColor);
	hueSelector.setColor(firstItemColor);
	colorSwatch.setColor(firstItemColor);

	overlayComponent.setBounds(quadPicker.getBounds());
	overlayComponent2.setBounds(hueSelector.getBounds());

	popup.setBounds(themesListX, themesListY, listBoxWidth, listHeight );
}

void AppearanceComponent::initializeComponents() {
	DBG("initializeComponents Called");

	// Initialize QuadPicker's color change callback
	quadPicker.onColorChanged = [this](juce::Colour newColor) {
		updateComponents(newColor);
		directColorUpdate(newColor);

		// Update ColorValuesInput Saturation and Brightness Sliders
		colorValuesInput.updateSaturationSlider(newColor.getSaturation() * 255.0f);
		colorValuesInput.updateBrightnessSlider(newColor.getBrightness() * 255.0f);
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
	uiElementsLabel.setColour(juce::Label::textColourId, menuLabelTextColor);
	themesLabel.setColour(juce::Label::textColourId, menuLabelTextColor);

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

void AppearanceComponent::directColorUpdate(juce::Colour newColor, bool applyAlpha) {
	DBG("directColorUpdate Called");
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

	colorSwatch.setColor(newColor);
	colorValuesInput.setColor(newColor);
	uiElementsList.updateSelectedElementColor(newColor);
	quadPicker.setColor(newColor);
	colorValuesInput.setColor(newColor);

	repaintComponents();
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
	// Do not re-assign theme.getColors() here — that would restore stale dice
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

	// Set the text color for the labels
	uiElementsLabel.setColour(juce::Label::textColourId, menuLabelTextColor);
	themesLabel.setColour(juce::Label::textColourId, menuLabelTextColor);

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
	accessibilityLabel.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
	textContrastLabel.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
	enforceLegibleTextToggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
	enforceLegibleTextToggle.setColour (juce::ToggleButton::tickColourId, sharedResources.sharedColors.menuSliderFillColor);
	// Set text for the labels
	uiElementsLabel.setText("UI Elements", juce::dontSendNotification);
	themesLabel.setText("Themes", juce::dontSendNotification);

	// Set the font for the labels
	uiElementsLabel.setFont(juce::Font("Lato Black", 16.0f, juce::Font::plain));
	themesLabel.setFont(juce::Font("Lato Black", 16.0f, juce::Font::plain));

	juce::Colour menuLabelTextColor = sharedResources.sharedColors.menuLabelTextColor1;

	// Set the text color for the labels
	uiElementsLabel.setColour(juce::Label::textColourId, menuLabelTextColor);
	themesLabel.setColour(juce::Label::textColourId, menuLabelTextColor);

	// Add the labels to the parent component
	addAndMakeVisible(uiElementsLabel);
	addAndMakeVisible(themesLabel);
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
		uiElementsLabel.setColour (juce::Label::textColourId, newColor);
		themesLabel.setColour (juce::Label::textColourId, newColor);
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
}

void AppearanceComponent::syncAccessibilityControls()
{
	const bool on = sharedResources.sharedColors.enforceLegibleText;
	enforceLegibleTextToggle.setToggleState (on, juce::dontSendNotification);
	textContrastSlider.setValue (sharedResources.sharedColors.textContrastAmount, juce::dontSendNotification);
	textContrastSlider.setVisible (on);
	textContrastLabel.setVisible (on);
	textContrastSlider.setEnabled (on);
}

void AppearanceComponent::applyAccessibilityTextContrast()
{
	sharedResources.sharedColors.enforceLegibleTextContrast();
	if (sharedResources.sharedColors.randomizeFaceplateMod)
		sharedResources.sharedColors.syncFaceplateModScheme();
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
	uiElementsLabel.repaint();
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
