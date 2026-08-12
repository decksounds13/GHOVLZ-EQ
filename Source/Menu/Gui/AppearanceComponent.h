#pragma once
#include <JuceHeader.h>
#include "../../EqProcessor.h"
#include "HueSelector.h"
#include "QuadPicker.h"
#include "ColorValuesInput.h"
#include "UIElementsList.h"
#include "../SharedResources.h"
#include "../Theme.h"
#include "ThemeList.h"
#include "../../TextButtonLookAndFeel.h"
#include "../../TextButtonLookAndFeel2.h"
#include "../../ArrowSliderLookAndFeel.h"
#include "../../TwoThumbSliderLookAndFeel.h"
#include "CustomTwoValueSliderLookAndFeel.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "QuadPickerOverlayComponent.h"
#include "HueSelectorOverlayComponent.h"
#include "CustomTextButton1.h"
#include "Eyedropper.h"
#include "../../CustomPopup.h"

class AppearanceComponent : public juce::Component,
    public UIElementsList::Listener,
    public HueSelector::Listener,
    public ThemeList::Listener,
    public juce::Slider::Listener

{
public:
    AppearanceComponent(SharedResources& resources, juce::AudioProcessorValueTreeState& state);
    ~AppearanceComponent() override;  // Destructor to remove listener
    void paint(juce::Graphics& g) override;
    void resized() override;

    void sliderValueChanged(juce::Slider* slider);

    void onPresetApplied(const Theme& theme);

    // UIElementsList::Listener implementation
    void onElementSelected(const juce::String& name, const juce::Colour& color) override;

    // HueSelector::Listener implementation
    void colorChanged(float newHue);

    void initializeComponents();

    void updateComponents(juce::Colour newColor);

    void directColorUpdate(juce::Colour newColor, bool applyAlpha = false);
    /**
        Lightweight SV-pad scrub: update selection colours + local readouts only.
        No full-plugin theme refresh (that made the ring lag). Commit via directColorUpdate on mouse-up.
    */
    void liveColourPreviewFromPad (juce::Colour newColor);

    void updateColorSelectors(const juce::Array<juce::Colour>& colors);

    void setupLabels();

    void repaintNewPresetButton();

    void updateAllComponents();
    
    void repaintParentComponent();

    void repaintComponents();

    void setRangeSliderColors();

    void applyColourSideEffects (const juce::String& elementName, juce::Colour newColor);

    void notifyThemeLiveChanged();

    void showRandomizeScopeMenu();
    void refreshAfterRandomize();
    void syncAccessibilityControls();
    void applyAccessibilityTextContrast();

    void setButtonLookAndFeels();

    void setSliderLookAndFeels();

    void showPopup(const juce::String& message);

    juce::Colour getColorForIndex(int index);
    juce::Rectangle<int> getThemeListBounds() const;

    ThemeList& getThemeList() noexcept { return themeList; }
    const ThemeList& getThemeList() const noexcept { return themeList; }

    /** Fired after palette edits / randomize so the host can refresh plugin chrome. */
    std::function<void()> onThemeLiveChanged;

private:

    bool isUpdatingDirectly = true;
    bool isUpdatingRandom = false;

    int newRectY = 0;
    int newRectHeight = 0;
    juce::Rectangle<int> quadPickerBounds;
    juce::Rectangle<int> hueSelectorBounds;


    juce::Colour currentColor{ juce::Colours::white };
    HueSelector hueSelector;
    QuadPicker quadPicker;
    ColorValuesInput colorValuesInput;
    UIElementsList uiElementsList;  // Initialize in the constructor

    SharedResources& sharedResources;

    ThemeList themeList;  // Declare the ThemePresetsList

    // Non-owning — these point at member buttons/sliders, not heap objects.
    juce::Array<juce::Button*> buttonsUsingCustomLookAndFeel;
    juce::Array<juce::Button*> buttonsUsingCustomLookAndFeel2;
    juce::Array<juce::Slider*> slidersUsingCustomLookAndFeel;

    TextButtonLookAndFeel textButtonLookAndFeel;
    TextButtonLookAndFeel2 textButtonLookAndFeel2;

    //TwoThumbSlider saturationRangeSlider;

    juce::Slider saturationRangeSlider;
    juce::Slider hueRangeSlider;
    juce::Slider brightnessRangeSlider;

    TwoThumbSliderLookAndFeel twoThumbLookAndFeel;
   
    CustomTwoValueSliderLookAndFeel customTwoValueSliderLookAndFeel;


    CustomTextButton newPresetButton{ "New" };
    CustomTextButton overwritePresetButton{ "Overwrite" };
    CustomTextButton deletePresetButton{ "Delete" };

    juce::TextButton randomizeHueToggleButton{ "H" };
    juce::TextButton randomizeSaturationToggleButton{ "S" };
    juce::TextButton randomizeBrightnessToggleButton{ "B" };
    juce::TextButton randomizeAlphaToggleButton{ "A" };


    /** Section headers (Phase 1 Appearance structure). */
    juce::Label coloursSectionLabel;
    juce::Label themesLabel;
    juce::Label randomizeSectionLabel;
    juce::Label chromeSectionLabel;

    juce::ToggleButton enforceLegibleTextToggle { "Legible text" };
    juce::Label textContrastLabel;
    juce::Slider textContrastSlider;
    juce::Label optionBoxOpacityLabel;
    juce::Slider optionBoxOpacitySlider;
    /** Live percent for Option box opacity (e.g. "90%"). */
    juce::Label optionBoxOpacityPercentLabel;

    std::unique_ptr<Eyedropper> eyedropper;
    std::unique_ptr<juce::TextButton> eyedropperButton; // Add this line

    CustomTextButton randomizeColorsButton{ "Randomize" };
    CustomTextButton randomizeSelectedColorsButton { "Rand sel" };

    CustomPopup popup;

    class ColorSwatch : public juce::Component
    {
    public:
        // Constructor that takes a reference to the parent component
        ColorSwatch(AppearanceComponent& parent) : parentComponent(parent) {}

        void paint(juce::Graphics& g) override
        {
            juce::Colour menuThinBorderColor = parentComponent.sharedResources.sharedColors.menuThinBorderColor;

            int padding = 2;
            float cornerSize = 5.0f;  // Radius for the rounded corners

            // Define the bounds for the rounded rectangle
            juce::Rectangle<float> bounds = getLocalBounds().reduced(10).toFloat();

            // Create a path for the rounded rectangle
            juce::Path roundedRectPath;
            roundedRectPath.addRoundedRectangle(bounds, cornerSize);


            // Render the drop shadow
            shadow.render(g, roundedRectPath);

            // Save the current graphics state
            g.saveState();

            // Set the clipping region to the rounded rectangle path
            g.reduceClipRegion(roundedRectPath);


            // Fill the color swatch within the clipped region
            g.setColour(currentColor);
            g.fillRect(bounds);

            // Restore the graphics state to remove clipping
            g.restoreState();

            // Optionally, draw an outline around the color swatch with the same rounded rectangle
            //g.setColour(menuThinBorderColor);
           // g.strokePath(roundedRectPath, juce::PathStrokeType(1.0f));  // Adjust the stroke thickness as needed
        }

        void setColor(juce::Colour newColor)
        {
            //DBG("ColorSwatch::setColor - New Color: " + newColor.toDisplayString(true));
            if (currentColor != newColor) // Only update if color has changed
            {
                currentColor = newColor;
                repaint();
            }
        }

    private:
        juce::Colour currentColor{ juce::Colours::white };
        AppearanceComponent& parentComponent;

        melatonin::DropShadow shadow = { { juce::Colours::black.withAlpha(0.75f), 12, { 0, 2 } } };
    };

    ColorSwatch colorSwatch;
    
    melatonin::DropShadow quadPickerShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::DropShadow uiElementsListShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::DropShadow themesListShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::InnerShadow shadow4 = { { juce::Colours::black.withAlpha(0.75f), 5, { 0, 0 } } };
    melatonin::DropShadow newPresetButtonShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::DropShadow overwritePresetButtonShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::DropShadow deletePresetButtonShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::DropShadow randomColorsButtonShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };
    melatonin::DropShadow randomSelectedColorsButtonShadow = { { juce::Colours::black.withAlpha(0.95f), 12, { 0, 2 } } };

    juce::Path quadPickerShadowPath;
    juce::Path uiElementsListShadowPath;
    juce::Path themesListShadowPath;
    juce::Path newPresetButtonShadowPath;
    juce::Path overwritePresetButtonShadowPath;
    juce::Path deletePresetButtonShadowPath;
    juce::Path randomColorsButtonShadowPath;
    juce::Path randomSelectedColorsButtonShadowPath;

    QuadPickerOverlayComponent overlayComponent;
    HueSelectorOverlayComponent overlayComponent2;

    //TextButtonLookAndFeel textButtonLookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppearanceComponent)
};
