#include "OptionBoxMenu.h"
#include "OnOffButton1.h"
#include "FilterType.h"

OptionBoxMenu::OptionBoxMenu(juce::AudioProcessorValueTreeState& state)
    : treeState(state) // Initialize treeState with the passed state parameter
{
    onOffButton1 = std::make_unique<OnOffButton1>(treeState, "highpassOnOff");

    // Initialize custom colors
    backgroundColor = juce::Colour::fromRGBA(46, 44, 40, 245.0f);
    borderColor = juce::Colour::fromRGBA(30, 25, 25, 255.0f);  
    titleColor = juce::Colours::whitesmoke.withAlpha(0.8f);
    labelTextColor = juce::Colours::whitesmoke.withAlpha(0.8f);
    separatorColor = juce::Colours::whitesmoke.withAlpha(0.5f);
    juce::Colour comboBoxBackgroundColor = juce::Colour::fromRGBA(97, 96, 44, 255.0f);
    juce::Colour popupMenuBackgroundColor = juce::Colour::fromRGBA(22, 22, 22, 245.0f);
    juce::Colour arrowColor = juce::Colour::fromRGBA(10, 10, 10, 255.0);
    juce::Colour comboBoxOutlineColor = juce::Colour::fromRGBA(10, 10, 10, 255.0);
    juce::Colour popupMenuTextColor = juce::Colours::whitesmoke.withAlpha(0.8f);
    juce::Colour popupMenuSelectionColor = juce::Colour::fromRGBA(85, 77, 22, 255.0f);
    juce::Colour comboBoxTextColor = juce::Colour::fromRGBA(10, 10, 10, 255.0);;
 
    juce::Font myFont("Lato Black", 16.0f, juce::Font::plain);

    setSize(150, 150);  // Setting the size of the component

    // ComboBox is populated per-band in setupFilterModelMenu().
    // Same pattern as working Level Meters / Spectrum combos (stock ComboBox + colour LAF).
    customComboBox.setLookAndFeel (&customLookAndFeel);
    customComboBox.addListener (this);
    addAndMakeVisible(customComboBox);
    customComboBox.setColour(juce::ComboBox::backgroundColourId, comboBoxBackgroundColor);
    customComboBox.setColour(juce::ComboBox::textColourId, PluginMenuTheme::text());
    customComboBox.setColour(juce::ComboBox::outlineColourId, comboBoxOutlineColor);
    customComboBox.setColour(juce::ComboBox::arrowColourId, PluginMenuTheme::text());
    customComboBox.setColour(juce::ComboBox::buttonColourId, comboBoxBackgroundColor);

    // Per-band saturation — right of filter model dropdown.
    satButton.setClickingTogglesState (true);
    satButton.setTooltip ("Saturation - raising band gain adds harmonics in that region (Q sets how wide). Right-click to choose a model or oversampling.");
    satButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    satButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    satButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    satButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    satButton.setLookAndFeel (&myTextButtonLookAndFeel);
    satButton.addListener (this);
    satButton.addMouseListener (this, false);
    addChildComponent (satButton);

    satPrePostButton.setClickingTogglesState (true);
    satPrePostButton.setTooltip ("Pre: EQ then saturate. Post: add saturated harmonics from the EQ boost only, blended with the dry signal.");
    satPrePostButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    satPrePostButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    satPrePostButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    satPrePostButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    satPrePostButton.setLookAndFeel (&myTextButtonLookAndFeel);
    satPrePostButton.addListener (this);
    addChildComponent (satPrePostButton);

    // Stage 2 - post-Spectral sat (global). Plain ">" so UI fonts don't show missing-glyph boxes.
    spectralSatButton.setButtonText (">");
    spectralSatButton.setClickingTogglesState (true);
    spectralSatButton.setTooltip (
        "Post-spectral saturation - after Spectral, so Expand and resonance peaks can drive the grit. Right-click for model or oversampling.");
    spectralSatButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    spectralSatButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    spectralSatButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    spectralSatButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    spectralSatButton.setLookAndFeel (&myTextButtonLookAndFeel);
    spectralSatButton.addListener (this);
    spectralSatButton.addMouseListener (this, false);
    addChildComponent (spectralSatButton);

    spectralSatDriveKnob.setCompactNoValueBox (true);
    spectralSatDriveKnob.setTooltip ("Drive - how hard post-spectral saturation is pushed");
    spectralSatDriveKnob.addListener (this);
    addChildComponent (spectralSatDriveKnob);

    // Initialize the label for the current band name
    bandNameLabel.setFont(juce::Font("Lato Black", 18.0f, juce::Font::plain));
    bandNameLabel.setJustificationType(juce::Justification::centredLeft);
    bandNameLabel.setColour(juce::Label::textColourId, titleColor);  // Set the label color
    bandNameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible(bandNameLabel);

    auto styleBandNav = [this] (juce::TextButton& b, const juce::String& tip)
    {
        b.setClickingTogglesState (false);
        b.setTooltip (tip);
        // Stock LAF + gold fill so these stay visible on the dark OptionBox header.
        b.setLookAndFeel (nullptr);
        b.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
        b.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (220, 190, 80, 255));
        b.setColour (juce::TextButton::textColourOffId, juce::Colours::black);
        b.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        b.addListener (this);
        addAndMakeVisible (b);
    };
    styleBandNav (prevBandButton, "Previous band");
    styleBandNav (nextBandButton, "Next band");

    addAndMakeVisible(rotaryImageKnobForOptionBox1);
    addAndMakeVisible(rotaryImageKnobForOptionBox2);
    addAndMakeVisible(rotaryImageKnobForOptionBox3);

    rotaryImageKnobForOptionBox1.addListener (this);
    rotaryImageKnobForOptionBox2.addListener (this);
    rotaryImageKnobForOptionBox3.addListener (this);

    addAndMakeVisible(*onOffButton1);

    // Initialize channel-mode buttons (radio-style; none selected = stereo)
    midSelectorButton.setButtonText("M");
    sideSelectorButton.setButtonText("S");
    leftSelectorButton.setButtonText("L");
    rightSelectorButton.setButtonText("R");

    for (auto* button : { &midSelectorButton, &sideSelectorButton, &leftSelectorButton, &rightSelectorButton })
    {
        button->setClickingTogglesState (false);
        button->setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
        button->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
        button->setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
        button->setColour (juce::TextButton::textColourOnId, juce::Colours::black);
        addAndMakeVisible (*button);
        button->addListener (this);
        button->setLookAndFeel (&myTextButtonLookAndFeel);
    }

    sidechainButton.setClickingTogglesState (true);
    sidechainButton.setTooltip ("Sidechain - duck this band from the Sidechain input (audio) or MIDI");
    // Greyed / inactive by default; yellow on-colour (existing) when enabled.
    sidechainButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (42, 40, 38, 160));
    sidechainButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    sidechainButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.38f));
    sidechainButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    sidechainButton.setLookAndFeel (&sidechainButtonLookAndFeel);
    sidechainButton.addListener (this);
    addChildComponent (sidechainButton);

    sidechainMidiButton.setClickingTogglesState (true);
    sidechainMidiButton.setTooltip ("MIDI - any note triggers ducking to the band gain (uses A/R)");
    sidechainMidiButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    sidechainMidiButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (120, 160, 200, 255));
    sidechainMidiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    sidechainMidiButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    sidechainMidiButton.setLookAndFeel (&myTextButtonLookAndFeel);
    sidechainMidiButton.addListener (this);
    addChildComponent (sidechainMidiButton);

    // Dynamic EQ "D" — same visual language as M/S/L/R; toggle + compact threshold under freq knob.
    dynamicButton.setButtonText ("D");
    dynamicButton.setClickingTogglesState (true);
    dynamicButton.setTooltip ("Dynamic EQ - band gain responds to level in this band");
    dynamicButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    dynamicButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    dynamicButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    dynamicButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    dynamicButton.setLookAndFeel (&myTextButtonLookAndFeel);
    dynamicButton.addListener (this);
    addChildComponent (dynamicButton);

    // Spectral "S" — same size as D; sits below threshold (not the channel Side "S").
    spectralButton.setButtonText ("S");
    spectralButton.setClickingTogglesState (true);
    spectralButton.setTooltip ("Spectral - process resonances inside the band Q; Amount + Res + Expand + A/R");
    spectralButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    spectralButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    spectralButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    spectralButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    spectralButton.setLookAndFeel (&myTextButtonLookAndFeel);
    spectralButton.addListener (this);
    addChildComponent (spectralButton);

    // Expand invert — directly below Amount when S is on; on = boost resonances.
    spectralExpandButton.setButtonText ("E");
    spectralExpandButton.setClickingTogglesState (true);
    spectralExpandButton.setTooltip ("Expand - exaggerate resonances instead of suppressing them");
    spectralExpandButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (45, 55, 50, 255));
    spectralExpandButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (70, 170, 130, 255));
    spectralExpandButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.85f));
    spectralExpandButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    spectralExpandButton.setLookAndFeel (&myTextButtonLookAndFeel);
    spectralExpandButton.addListener (this);
    addChildComponent (spectralExpandButton);

    // Lattice pack — global Flat / LF / HF under Res+Amt (shared by all S bands).
    spectralPackButton.setClickingTogglesState (false);
    spectralPackButton.setTooltip (
        "Pack (global) - FL is an even lattice, LP packs more slices toward the lows, HP toward the highs. Applies to all spectral bands.");
    spectralPackButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromRGBA (60, 50, 35, 255));
    spectralPackButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromRGBA (180, 150, 55, 255));
    spectralPackButton.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    spectralPackButton.setColour (juce::TextButton::textColourOnId, juce::Colours::black);
    spectralPackButton.setLookAndFeel (&myTextButtonLookAndFeel);
    spectralPackButton.addListener (this);
    addChildComponent (spectralPackButton);

    dynThresholdSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    dynThresholdSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    dynThresholdSlider.setTooltip ("Threshold - level where dynamic EQ starts engaging");
    dynThresholdSlider.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGBA (40, 35, 28, 255));
    dynThresholdSlider.setColour (juce::Slider::trackColourId, juce::Colour::fromRGBA (180, 150, 55, 220));
    dynThresholdSlider.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGBA (220, 200, 120, 255));
    dynThresholdSlider.addListener (this);
    addChildComponent (dynThresholdSlider);

    // Res: vertical (same param mapping as horizontal — value is orientation-agnostic).
    spectralBandwidthSlider.setSliderStyle (juce::Slider::LinearVertical);
    spectralBandwidthSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    spectralBandwidthSlider.setTooltip ("Res - how narrow the spectral slices are (finer = more selective, more CPU)");
    spectralBandwidthSlider.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGBA (40, 35, 28, 255));
    spectralBandwidthSlider.setColour (juce::Slider::trackColourId, juce::Colour::fromRGBA (180, 150, 55, 220));
    spectralBandwidthSlider.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGBA (220, 200, 120, 255));
    spectralBandwidthSlider.addListener (this);
    addChildComponent (spectralBandwidthSlider);

    // Amount: vertical. Param Amount 0–2 (higher = more); slider is inverted so
    // bottom = more processing (sliderValue = 1 − amount/max).
    spectralAmountSlider.setSliderStyle (juce::Slider::LinearVertical);
    spectralAmountSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    spectralAmountSlider.setRange (0.0, 1.0, 0.01);
    spectralAmountSlider.setTooltip ("Amount - pull down for more spectral resonance attenuation");
    spectralAmountSlider.setColour (juce::Slider::backgroundColourId, juce::Colour::fromRGBA (40, 35, 28, 255));
    spectralAmountSlider.setColour (juce::Slider::trackColourId, juce::Colour::fromRGBA (180, 150, 55, 220));
    spectralAmountSlider.setColour (juce::Slider::thumbColourId, juce::Colour::fromRGBA (220, 200, 120, 255));
    spectralAmountSlider.addListener (this);
    addChildComponent (spectralAmountSlider);

    auto setupSpectralSliderLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::NotificationType::dontSendNotification);
        label.setFont (juce::Font ("Lato Black", 10.0f, juce::Font::plain));
        label.setJustificationType (juce::Justification::centred);
        label.setColour (juce::Label::textColourId, titleColor);
        label.setInterceptsMouseClicks (false, false);
        addChildComponent (label);
    };
    setupSpectralSliderLabel (spectralResLabel, "Res");
    setupSpectralSliderLabel (spectralAmountLabel, "Amt");

    // A / R image knobs — hover shows value in ms (shared envelope params).
    auto setupTinyTimeKnob = [this] (RotaryImageKnobForOptionBox& knob, const juce::String& tip)
    {
        knob.setCompactNoValueBox (false);
        knob.setTooltip (tip);
        knob.setTextValueSuffix (" ms");
        knob.setNumDecimalPlacesToDisplay (1);
        knob.addListener (this);
        addChildComponent (knob);
    };
    setupTinyTimeKnob (attackKnob, "Attack - how fast dynamics / spectral / sidechain engage");
    setupTinyTimeKnob (releaseKnob, "Release - how fast dynamics / spectral / sidechain recover");

    attackLabel.setText ("A", juce::NotificationType::dontSendNotification);
    releaseLabel.setText ("R", juce::NotificationType::dontSendNotification);
    for (auto* label : { &attackLabel, &releaseLabel })
    {
        label->setFont (juce::Font ("Lato Black", 12.0f, juce::Font::plain));
        label->setJustificationType (juce::Justification::centred);
        label->setColour (juce::Label::textColourId, titleColor);
        label->setInterceptsMouseClicks (false, false);
        addChildComponent (*label);
    }

    frequencyLabel.setText("Frequency", juce::NotificationType::dontSendNotification);
    gainLabel.setText("Gain", juce::NotificationType::dontSendNotification);
    qLabel.setText("Q", juce::NotificationType::dontSendNotification);
    gainLabel.setJustificationType(juce::Justification::centred);
    qLabel.setJustificationType(juce::Justification::centred);

    bool isDraggable = true;

    resized();
}

OptionBoxMenu::~OptionBoxMenu()
{
    listenToCurrentBandOnOff (false);
    listenToCurrentBandDynamic (false);
    listenToCurrentBandSpectral (false);
    clearAttachments();

    rotaryImageKnobForOptionBox1.removeListener (this);
    rotaryImageKnobForOptionBox2.removeListener (this);
    rotaryImageKnobForOptionBox3.removeListener (this);
    attackKnob.removeListener (this);
    releaseKnob.removeListener (this);

    customComboBox.removeListener (this);
    customComboBox.setLookAndFeel (nullptr);

    // Remove the listener from each button
    midSelectorButton.removeListener(this);
    sideSelectorButton.removeListener(this);
    leftSelectorButton.removeListener(this);
    rightSelectorButton.removeListener(this);
    sidechainButton.removeListener (this);
    sidechainMidiButton.removeListener (this);
    dynamicButton.removeListener (this);
    spectralButton.removeListener (this);
    spectralExpandButton.removeListener (this);
    spectralPackButton.removeListener (this);
    satButton.removeListener (this);
    satButton.removeMouseListener (this);
    satPrePostButton.removeListener (this);
    spectralSatButton.removeListener (this);
    spectralSatButton.removeMouseListener (this);
    spectralSatDriveKnob.removeListener (this);
    dynThresholdSlider.removeListener (this);
    spectralBandwidthSlider.removeListener (this);
    spectralAmountSlider.removeListener (this);

    // Reset the LookAndFeel for each button to the default
    midSelectorButton.setLookAndFeel(nullptr);
    sideSelectorButton.setLookAndFeel(nullptr);
    leftSelectorButton.setLookAndFeel(nullptr);
    rightSelectorButton.setLookAndFeel(nullptr);
    sidechainButton.setLookAndFeel (nullptr);
    sidechainMidiButton.setLookAndFeel (nullptr);
    dynamicButton.setLookAndFeel (nullptr);
    spectralButton.setLookAndFeel (nullptr);
    spectralExpandButton.setLookAndFeel (nullptr);
    spectralPackButton.setLookAndFeel (nullptr);
    satButton.setLookAndFeel (nullptr);
    satPrePostButton.setLookAndFeel (nullptr);
    spectralSatButton.setLookAndFeel (nullptr);
}


void OptionBoxMenu::paint(juce::Graphics& g)
{


    // Define custom RGB colors
    juce::Colour color1 = juce::Colour::fromRGB(55, 35, 25);  // Inner color
    juce::Colour color2 = juce::Colour::fromRGB(0, 0, 0);      // Outer color
    juce::Colour color3 = juce::Colour::fromRGB(135, 90, 40);  // Inner color
    juce::Colour color4 = juce::Colour::fromRGB(0, 0, 0);      // Outer color
    juce::Colour thinOutlineColor = juce::Colour::fromRGB(0, 0, 0); // Thin outline color

    // Create a ColourGradient object for the background
    juce::ColourGradient optionBoxGradient(color1,                           // Inner color
        juce::Point<float>(getWidth() / 2, 0.0f),                                      // Start position (top-left corner)
        color2,                                                              // Outer color
        juce::Point<float>(static_cast<float>(getWidth() / 2), static_cast<float>(getHeight() * 3)), // End position (bottom-right corner)
        true);                                // Radial gradient

    juce::ColourGradient optionBoxGradient2(color3,                           // Inner color
        juce::Point<float>(getWidth() / 2, 0.0f),                                      // Start position (top-left corner)
        color4,                                                              // Outer color
        juce::Point<float>(static_cast<float>(getWidth() / 2), static_cast<float>(getHeight() * 2)), // End position (bottom-right corner)
        true);                                // Radial gradient

    // Define the dimensions and corner radius
    float x = 0.0f, y = 0.0f, width = (float) designWidth, height = (float) designHeight;
    float cornerRadius = 20.0f;
    float borderWidth = 8.0f;
    float thinOutlineWidth = 2.0f;


    // Draw the background as a rounded rectangle filled with gradient
    g.setGradientFill(optionBoxGradient);
    g.fillRoundedRectangle(x, y, width, height, cornerRadius);

    // Create a Path object for the main border outline
    juce::Path borderPath;
    float offset = borderWidth / 2.0f; // Center the border on the edge
    borderPath.addRoundedRectangle(x + offset, y + offset, width - borderWidth, height - borderWidth, cornerRadius - offset);

    // Set the color for the main border outline
    g.setColour(borderColor);

    // Use strokePath to draw only the outline of the border
    g.strokePath(borderPath, juce::PathStrokeType(borderWidth));

    // Create a Path object for the first thin inner outline
    juce::Path innerThinOutlinePath;
    float innerOffset = borderWidth - thinOutlineWidth / 2.0f; // Position it inside the main border
    innerThinOutlinePath.addRoundedRectangle(x + innerOffset, y + innerOffset, width - 2 * innerOffset, height - 2 * innerOffset, cornerRadius - innerOffset);

    // Set the gradient fill for the first thin inner outline
    g.setColour(juce::Colours::black);

    // Use strokePath to draw only the outline of the first thin inner border
    g.strokePath(innerThinOutlinePath, juce::PathStrokeType(thinOutlineWidth));

    // Create a Path object for the second thin outer outline
    juce::Path outerThinOutlinePath;
    outerThinOutlinePath.addRoundedRectangle(0, 0, getWidth(), getHeight(), cornerRadius);

    // Temporarily set a distinct color to debug the outline
    g.setGradientFill(optionBoxGradient2); // Debugging color

    // Use strokePath to draw only the outline of the second thin outer border
    g.strokePath(outerThinOutlinePath, juce::PathStrokeType(thinOutlineWidth));

}

void OptionBoxMenu::resized()
{
    // Group all positioning-related variables here
    int padding = 5;
    int comboBoxHeight = 20;
    int elementYSpacing = 5;  // Vertical spacing between elements
    int labelWidth = 100; // Set to the desired width of your label
    int labelHeight = 30; // Set to the desired height of your label
    int labelY = 13; // Set to the desired y-coordinate, can also be calculated similar to labelX
    int rotaryImageKnobLargeSize = 80;
    int rotaryImageKnobSmallSize = 55;
    int onOffButton1Size = 32;


    juce::Font myFont("Lato Black", 16.0f, juce::Font::plain);

    // Top-left gold < > cycle bands without moving the box.
    constexpr int navSize = 20;
    constexpr int navGap = 3;
    constexpr int navLeft = 14;
    const int navY = labelY + (onOffButton1Size - navSize) / 2;
    prevBandButton.setBounds (navLeft, navY, navSize, navSize);
    nextBandButton.setBounds (navLeft + navSize + navGap, navY, navSize, navSize);
    prevBandButton.setVisible (true);
    nextBandButton.setVisible (true);
    prevBandButton.toFront (false);
    nextBandButton.toFront (false);

    onOffButton1->setBounds (getWidth() * .92 - onOffButton1Size, labelY, onOffButton1Size, onOffButton1Size);

    const int nameLeft = nextBandButton.getRight() + 5;
    const int nameRight = onOffButton1->getX() - 2;
    bandNameLabel.setBounds (nameLeft, labelY, juce::jmax (20, nameRight - nameLeft), labelHeight);

    const int comboW = juce::roundToInt (getWidth() * 0.42);
    const int satBtnW = 28;
    const int prePostW = 32;
    const int satGap = 3;
    customComboBox.setBounds(padding * 3, bandNameLabel.getBottom() + elementYSpacing, comboW, comboBoxHeight);
    satButton.setBounds (customComboBox.getRight() + satGap, customComboBox.getY(), satBtnW, comboBoxHeight);
    satPrePostButton.setBounds (satButton.getRight() + satGap, customComboBox.getY(), prePostW, comboBoxHeight);

    // Former getHeight()/2 layout at height 340, shifted up by designTopCrop so MSLR (kept at top)
    // sits level with the Q label; box height/top crop move the chrome down on screen.
    rotaryImageKnobForOptionBox1.setBounds (padding * 2, 170 - designTopCrop, rotaryImageKnobLargeSize, rotaryImageKnobLargeSize);
    rotaryImageKnobForOptionBox2.setBounds (getWidth() * .55, 170 + 55 - designTopCrop, rotaryImageKnobSmallSize, rotaryImageKnobSmallSize);
    rotaryImageKnobForOptionBox3.setBounds (getWidth() * .55, 170 - 30 - designTopCrop, rotaryImageKnobSmallSize, rotaryImageKnobSmallSize);

    juce::Rectangle<int> knob1Bounds = rotaryImageKnobForOptionBox1.getBounds();
    juce::Rectangle<int> knob2Bounds = rotaryImageKnobForOptionBox2.getBounds();
    juce::Rectangle<int> knob3Bounds = rotaryImageKnobForOptionBox3.getBounds();

  // Calculate the x-positions for the "Gain" and "Q" labels to be centered above the knobs
    int gainLabelX = knob2Bounds.getCentreX() - labelWidth / 2;
    int qLabelX = knob3Bounds.getCentreX() - labelWidth / 2;

  // Set the bounds for the "Frequency" label (keep it as it was)
    frequencyLabel.setBounds(knob1Bounds.getX(), knob1Bounds.getY() - labelHeight, labelWidth, labelHeight);

  // Set the bounds for the "Gain" and "Q" labels (centered above the knobs)
    gainLabel.setBounds(gainLabelX, knob2Bounds.getY() - labelHeight, labelWidth, labelHeight);
    qLabel.setBounds(qLabelX, knob3Bounds.getY() - labelHeight, labelWidth, labelHeight);

    // Set the font and color for the labels
    frequencyLabel.setFont(myFont);
    frequencyLabel.setColour(juce::Label::textColourId, titleColor);

    gainLabel.setFont(myFont);
    gainLabel.setColour(juce::Label::textColourId, titleColor);

    qLabel.setFont(myFont);
    qLabel.setColour(juce::Label::textColourId, titleColor);

    // Add the labels to the view, if they haven't been added yet
    if (frequencyLabel.getParentComponent() == nullptr) {
        addAndMakeVisible(frequencyLabel);
    }
    if (gainLabel.getParentComponent() == nullptr) {
        addAndMakeVisible(gainLabel);
    }
    if (qLabel.getParentComponent() == nullptr) {
        addAndMakeVisible(qLabel);
    }

    // Calculate the position for the text buttons under the customComboBox
    int buttonWidth = 15;  // Width for each of M, S, L, R buttons
    int buttonHeight = 20; // Height for each of M, S, L, R buttons
    int comboBoxBottom = customComboBox.getBottom();
    int spacing = 5;  // Spacing between the buttons

    // Calculate x-positions for M, S, L, R buttons
    int midButtonX = customComboBox.getX();
    int sideButtonX = midButtonX + buttonWidth + spacing;
    int leftButtonX = sideButtonX + buttonWidth + spacing;
    int rightButtonX = leftButtonX + buttonWidth + spacing;

    // Set bounds for M, S, L, R buttons — level with the Q label after the top crop.
    midSelectorButton.setBounds(midButtonX, comboBoxBottom + spacing, buttonWidth, buttonHeight);
    sideSelectorButton.setBounds(sideButtonX, comboBoxBottom + spacing, buttonWidth, buttonHeight);
    leftSelectorButton.setBounds(leftButtonX, comboBoxBottom + spacing, buttonWidth, buttonHeight);
    rightSelectorButton.setBounds(rightButtonX, comboBoxBottom + spacing, buttonWidth, buttonHeight);

    // Sidechain under M/S/L/R (centered), above Frequency label; M appears when SC on.
    const int scBtnW = 64;
    const int scBtnH = 16;
    const int scMidiW = 15;
    const int scRowY = midSelectorButton.getBottom() + 3;
    const int mslrLeft = midSelectorButton.getX();
    const int mslrRight = rightSelectorButton.getRight();
    const int scCentreX = (mslrLeft + mslrRight) / 2;
    sidechainButton.setBounds (scCentreX - scBtnW / 2, scRowY, scBtnW, scBtnH);
    sidechainMidiButton.setBounds (sidechainButton.getRight() + 3, scRowY, scMidiW, scBtnH);

    // Add the buttons to the view, if they haven't been added yet
    if (midSelectorButton.getParentComponent() == nullptr) {
        addAndMakeVisible(midSelectorButton);
    }
    if (sideSelectorButton.getParentComponent() == nullptr) {
        addAndMakeVisible(sideSelectorButton);
    }
    if (leftSelectorButton.getParentComponent() == nullptr) {
        addAndMakeVisible(leftSelectorButton);
    }
    if (rightSelectorButton.getParentComponent() == nullptr) {
        addAndMakeVisible(rightSelectorButton);
    }

    // D + S + E column under the frequency knob.
    // S on: vertical Res (closest to D/S/E), then Amount; Expand under D/S.
    // D on (no S): horizontal threshold beside D. D+S: threshold below Expand.
    const int dynBtnW = 15;
    const int dynBtnH = 18;
    const int dynRowY = knob1Bounds.getBottom() + 2;
    const int optionBoxRight = getWidth() - padding * 2;
    const int dynRowX = knob1Bounds.getX() + 2;
    const bool sOn = spectralButton.isVisible() && spectralButton.getToggleState();

    dynamicButton.setBounds (dynRowX, dynRowY, dynBtnW, dynBtnH);

    const int spectralY = dynRowY + dynBtnH + 2;
    spectralButton.setBounds (dynRowX, spectralY, dynBtnW, dynBtnH);

    // Expand: same column as D/S, directly under S.
    const int expandY = spectralY + dynBtnH + 2;
    spectralExpandButton.setBounds (dynRowX, expandY, dynBtnW, dynBtnH);

    // Post-Spectral sat: identical footprint to D / S / E, directly under Expand.
    spectralSatButton.setBounds (dynRowX,
                                 spectralExpandButton.getBottom() + 2,
                                 spectralExpandButton.getWidth(),
                                 spectralExpandButton.getHeight());

    const int arSize = juce::roundToInt (32.0f * 0.8f); // 26 — same as A/R below
    const int ssDriveSize = juce::jmax (1, arSize / 2);

    const int sliderColW = 18;
    const int sliderLabelH = 12;
    const int sliderGap = 4;
    const int sliderH = 44;
    const int packBtnH = 14;
    const int packBtnW = sliderColW; // narrow — under one slider column
    const int sliderTop = dynRowY;
    const int sliderTrackY = sliderTop + sliderLabelH;
    const int resX = dynamicButton.getRight() + 4;
    const int amountX = resX + sliderColW + sliderGap;

    spectralResLabel.setBounds (resX - 2, sliderTop, sliderColW + 4, sliderLabelH);
    spectralBandwidthSlider.setBounds (resX, sliderTrackY, sliderColW, sliderH);

    spectralAmountLabel.setBounds (amountX - 2, sliderTop, sliderColW + 4, sliderLabelH);
    spectralAmountSlider.setBounds (amountX, sliderTrackY, sliderColW, sliderH);

    // Pack FL/LP/HP under Res by default; when → sat on, pack shifts under Amt
    // and Drive knob takes the Res slot.
    const int packY = sliderTrackY + sliderH + 2;
    const bool ssOn = spectralSatButton.isVisible() && spectralSatButton.getToggleState();
    if (ssOn)
    {
        spectralPackButton.setBounds (amountX, packY, packBtnW, packBtnH);
        spectralSatDriveKnob.setBounds (resX + (sliderColW - ssDriveSize) / 2,
                                        packY + (packBtnH - ssDriveSize) / 2,
                                        ssDriveSize,
                                        ssDriveSize);
    }
    else
    {
        spectralPackButton.setBounds (resX, packY, packBtnW, packBtnH);
        // Park off-layout; visibility gated in syncSpectralSatControls.
        spectralSatDriveKnob.setBounds (resX, packY, ssDriveSize, ssDriveSize);
    }

    // Threshold: beside D when S is off; below Expand when both D and S are on.
    const int threshH = 14;
    const int dynMaxRight = juce::jmin (knob2Bounds.getX() - 4, optionBoxRight);
    if (sOn)
    {
        const int threshY = juce::jmax (spectralExpandButton.getBottom(),
                                        juce::jmax (spectralPackButton.getBottom(),
                                                    spectralSatButton.getBottom())) + 3;
        const int threshX = dynRowX;
        dynThresholdSlider.setBounds (threshX, threshY, juce::jmax (0, dynMaxRight - threshX), threshH);
    }
    else
    {
        const int threshX = dynamicButton.getRight() + 4;
        const int threshW = juce::jmax (0, dynMaxRight - threshX);
        const int threshY = dynRowY + (dynBtnH - threshH) / 2;
        dynThresholdSlider.setBounds (threshX, threshY, threshW, threshH);
    }

    // A / R — face is arSize; extra height reserved for ms value readout under the knob.
    const int arLabelH = 12;
    const int arValueH = 14;
    const int arPairGap = 4;
    const int arPairW = arSize * 2 + arPairGap;
    const int amountRight = amountX + sliderColW;
    int arXMin = dynRowX;
    if (sOn)
        arXMin = amountRight + 4;
    const int arXIdeal = knob2Bounds.getCentreX() - arPairW / 2;
    const int arX = juce::jlimit (arXMin, juce::jmax (arXMin, optionBoxRight - arPairW), arXIdeal);
    const int arY = knob2Bounds.getBottom() + 2;
    attackKnob.setBounds (arX, arY, arSize, arSize + arValueH);
    releaseKnob.setBounds (arX + arSize + arPairGap, arY, arSize, arSize + arValueH);
    attackLabel.setBounds (arX, arY + arSize + arValueH, arSize, arLabelH);
    releaseLabel.setBounds (releaseKnob.getX(), arY + arSize + arValueH, arSize, arLabelH);

    updateDynamicControlsVisibility();
}

void OptionBoxMenu::mouseDown(const juce::MouseEvent& event)
{
    if (event.eventComponent == &satButton && event.mods.isPopupMenu())
    {
        showSatContextMenu();
        return;
    }

    if (event.eventComponent == &spectralSatButton && event.mods.isPopupMenu())
    {
        showSpectralSatContextMenu();
        return;
    }

    // Keep the menu open while interacting with its contents.
    // Outside-click closing is handled by FrequencyResponseComponent.
    if (! isDraggable)
        return;

    // Ignore drag starts that originated on child controls.
    if (event.eventComponent != this)
        return;

    juce::Rectangle<int> borderBounds (12, 12, getWidth() - 24, getHeight() - 24);
    if (! borderBounds.contains (event.getPosition()))
    {
        isBeingDragged = true;

        if (auto* parent = getParentComponent())
        {
            // Use visual (transformed) bounds so drag stays aligned under UI scale.
            const auto parentPos = event.getEventRelativeTo (parent).getPosition();
            const auto visual = getBoundsInParent();
            dragOffsetX = parentPos.x - visual.getX();
            dragOffsetY = parentPos.y - visual.getY();
        }
        else
        {
            dragOffsetX = juce::roundToInt (event.position.x);
            dragOffsetY = juce::roundToInt (event.position.y);
        }
    }
}

void OptionBoxMenu::mouseDrag(const juce::MouseEvent& event)
{
    if (! (isBeingDragged && isDraggable))
        return;

    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    const auto parentPos = event.getEventRelativeTo (parent).getPosition();
    const int padding = juce::jmax (0, juce::roundToInt (15.0f * getUiScale()));
    const int maxX = juce::jmax (padding, parent->getWidth() - getVisualWidth() - padding);
    const int maxY = juce::jmax (padding, parent->getHeight() - getVisualHeight() - padding);
    const int newX = juce::jlimit (padding, maxX, parentPos.x - dragOffsetX);
    const int newY = juce::jlimit (padding, maxY, parentPos.y - dragOffsetY);
    setTopLeftPosition (newX, newY);
    constrainVisualBoundsToParent();
}

void OptionBoxMenu::mouseUp(const juce::MouseEvent& event)
{
    isBeingDragged = false;
}

void OptionBoxMenu::mouseExit(const juce::MouseEvent& event)
{
    // Mouse event's position is already in local coordinates of this component (OptionBoxMenu)
    juce::Point<int> localPosition = event.position.toInt();
    ;


}

void OptionBoxMenu::mouseMove(const juce::MouseEvent& event)
{
    juce::Rectangle<int> borderBounds(12, 12, getWidth() - 24, getHeight() - 24);
    if (!borderBounds.contains(event.getPosition()))
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }
    else
    {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    }
}


bool OptionBoxMenu::isVisible() const
{
    return Component::isVisible();  // Call the parent class's isVisible method
}

float OptionBoxMenu::getUiScale() const
{
    // Grow never: stay at design size when the plugin is larger than 100%.
    // Shrink 1:1 when the plugin is smaller than the design width.
    if (auto* parent = getParentComponent())
        if (parent->getWidth() > 0)
            return juce::jmin (1.0f, (float) parent->getWidth() / designParentWidth);

    return 1.0f;
}

void OptionBoxMenu::applyUiScale()
{
    const float scale = getUiScale();
    setSize (designWidth, designHeight);
    setTransform (juce::AffineTransform::scale (scale));
}

int OptionBoxMenu::getVisualWidth() const
{
    return juce::roundToInt ((float) designWidth * getUiScale());
}

int OptionBoxMenu::getVisualHeight() const
{
    return juce::roundToInt ((float) designHeight * getUiScale());
}

void OptionBoxMenu::constrainVisualBoundsToParent()
{
    auto* parent = getParentComponent();
    if (parent == nullptr)
        return;

    const int parentW = parent->getWidth();
    const int parentH = parent->getHeight();
    if (parentW <= 0 || parentH <= 0)
        return;

    const float scale = getUiScale();
    const int padding = juce::jmax (0, juce::roundToInt (15.0f * scale));
    const int visualW = getVisualWidth();
    const int visualH = getVisualHeight();

    // Max top-left so the *transformed* box stays inside the graph.
    const int maxX = juce::jmax (padding, parentW - visualW - padding);
    const int maxY = juce::jmax (padding, parentH - visualH - padding);

    int x = juce::jlimit (padding, maxX, getX());
    int y = juce::jlimit (padding, maxY, getY());

    // Second pass using actual transformed bounds (AffineTransform-safe).
    setTopLeftPosition (x, y);
    auto visual = getBoundsInParent();

    int dx = 0;
    int dy = 0;

    if (visual.getRight() > parentW - padding)
        dx = (parentW - padding) - visual.getRight();
    if (visual.getX() + dx < padding)
        dx = padding - visual.getX();
    if (visual.getBottom() > parentH - padding)
        dy = (parentH - padding) - visual.getBottom();
    if (visual.getY() + dy < padding)
        dy = padding - visual.getY();

    if (dx != 0 || dy != 0)
        setTopLeftPosition (getX() + dx, getY() + dy);
}

void OptionBoxMenu::updateUiScaleFromParent()
{
    if (! Component::isVisible())
        return;

    // Keep the user's placed position; only refresh scale + clamp to the graph.
    applyUiScale();
    constrainVisualBoundsToParent();
}

void OptionBoxMenu::setInitialPosition(int x, int y)
{
    anchorHandleX = x;
    anchorHandleY = y;

    applyUiScale();

    const float scale = getUiScale();
    // Design-space offsets (20px gap, 15px padding at 1.0 scale), then scaled once.
    const int padding = juce::roundToInt (15.0f * scale);
    const int gap = juce::roundToInt (20.0f * scale);
    const int boxWidth = getVisualWidth();
    const int boxHeight = getVisualHeight();
    const int parentW = getParentWidth();
    const int parentH = getParentHeight();

    // Prefer right of handle; flip left when the scaled box (+ gap) would leave the graph.
    int boxX = x + gap;
    if (boxX + boxWidth + padding > parentW)
        boxX = x - boxWidth - gap;

    // Drop the chrome by the cropped top so MSLR/Q stay put vs the handle while the box top comes down.
    int boxY = y + juce::roundToInt ((float) designTopCrop * scale);

    const int maxX = juce::jmax (padding, parentW - boxWidth - padding);
    const int maxY = juce::jmax (padding, parentH - boxHeight - padding);
    boxX = juce::jlimit (padding, maxX, boxX);
    boxY = juce::jlimit (padding, maxY, boxY);

    setTopLeftPosition (boxX, boxY);
    constrainVisualBoundsToParent();

    interactionFaded = false;
    updateDisplayAlpha();
    setVisible (true);
}


void OptionBoxMenu::setDraggable(bool shouldBeDraggable)
{
    isDraggable = shouldBeDraggable;
}

void OptionBoxMenu::setInteractionFaded (bool shouldFade)
{
    interactionFaded = shouldFade;
    updateDisplayAlpha();
}

juce::String OptionBoxMenu::getOnOffParamIDForBand (int bandIndex) const
{
    switch (bandIndex)
    {
        case 0: return "band1OnOff";
        case 1: return "band2OnOff";
        case 2: return "band3OnOff";
        case 3: return "band4OnOff";
        case 4: return "highpassOnOff";
        case 5: return "lowpassOnOff";
        case 6: return "highShelfOnOff";
        case 7: return "lowShelfOnOff";
        default: return {};
    }
}

bool OptionBoxMenu::isCurrentBandEnabled() const
{
    const auto paramID = getOnOffParamIDForBand (currentBandIndex);
    if (paramID.isEmpty())
        return true;

    if (auto* v = treeState.getRawParameterValue (paramID))
        return v->load() > 0.5f;

    return true;
}

void OptionBoxMenu::updateDisplayAlpha()
{
    // Only dim while dragging a handle/knob. Band on/off is shown by the On button —
    // do not fade the whole box when < > switches to a disabled band.
    setAlpha (interactionFaded ? 0.5f : 1.0f);
}

void OptionBoxMenu::cycleBand (int delta)
{
    if (currentBandIndex < 0 || delta == 0)
        return;

    // Switching bands should only reload that band's settings — clear any drag-fade.
    interactionFaded = false;

    const int next = (currentBandIndex + delta + 8 * 8) % 8;
    setCurrentBandIndex (next, cachedBandNames.data());

    // Stay put — do not call setInitialPosition.
    if (onBandCycled != nullptr)
        onBandCycled (next);
}

void OptionBoxMenu::listenToCurrentBandOnOff (bool shouldListen)
{
    if (currentOnOffParamID.isNotEmpty())
        treeState.removeParameterListener (currentOnOffParamID, this);

    currentOnOffParamID = {};

    if (! shouldListen)
        return;

    currentOnOffParamID = getOnOffParamIDForBand (currentBandIndex);
    if (currentOnOffParamID.isNotEmpty())
        treeState.addParameterListener (currentOnOffParamID, this);
}

void OptionBoxMenu::listenToCurrentBandDynamic (bool shouldListen)
{
    if (currentDynamicParamID.isNotEmpty())
        treeState.removeParameterListener (currentDynamicParamID, this);

    currentDynamicParamID = {};

    if (! shouldListen)
        return;

    currentDynamicParamID = DynamicEq::dynamicParamIDForBandIndex (currentBandIndex);
    if (currentDynamicParamID.isNotEmpty())
        treeState.addParameterListener (currentDynamicParamID, this);
}

void OptionBoxMenu::listenToCurrentBandSpectral (bool shouldListen)
{
    if (currentSpectralParamID.isNotEmpty())
        treeState.removeParameterListener (currentSpectralParamID, this);
    if (currentSpectralPackParamID.isNotEmpty())
        treeState.removeParameterListener (currentSpectralPackParamID, this);

    currentSpectralParamID = {};
    currentSpectralPackParamID = {};

    if (! shouldListen)
        return;

    currentSpectralParamID = SpectralDynamics::spectralParamIDForBandIndex (currentBandIndex);
    if (currentSpectralParamID.isNotEmpty())
        treeState.addParameterListener (currentSpectralParamID, this);

    currentSpectralPackParamID = SpectralDynamics::spectralPackParamId();
    treeState.addParameterListener (currentSpectralPackParamID, this);
}

void OptionBoxMenu::parameterChanged (const juce::String& parameterID, float newValue)
{
    juce::ignoreUnused (newValue);

    if (parameterID == currentOnOffParamID)
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<OptionBoxMenu> (this)]
        {
            if (safe != nullptr)
                safe->updateDisplayAlpha();
        });
    }
    else if (parameterID == currentDynamicParamID
             || parameterID == currentSpectralParamID)
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<OptionBoxMenu> (this)]
        {
            if (safe != nullptr)
            {
                safe->updateDynamicControlsVisibility();
                safe->resized();
            }
        });
    }
    else if (parameterID == currentSpectralPackParamID)
    {
        juce::MessageManager::callAsync ([safe = juce::Component::SafePointer<OptionBoxMenu> (this)]
        {
            if (safe != nullptr)
                safe->syncSpectralPackButton();
        });
    }
}

void OptionBoxMenu::sliderDragStarted (juce::Slider* slider)
{
    juce::ignoreUnused (slider);
    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Band edit");
    setInteractionFaded (true);

    if (onBandKnobDragHighlight)
        onBandKnobDragHighlight (currentBandIndex);
}

void OptionBoxMenu::sliderDragEnded (juce::Slider* slider)
{
    juce::ignoreUnused (slider);
    setInteractionFaded (false);

    if (onBandKnobDragHighlight)
        onBandKnobDragHighlight (-1);
}

void OptionBoxMenu::clearAttachments()
{
    frequencyAttachment.reset();
    gainAttachment.reset();
    qAttachment.reset();
    dynThresholdAttachment.reset();
    attackMsAttachment.reset();
    releaseMsAttachment.reset();
    spectralBandwidthAttachment.reset();
    spectralAmountSlider.onDragStart = nullptr;
    spectralAmountSlider.onDragEnd = nullptr;
    spectralAmountSlider.onValueChange = nullptr;
    spectralAmountAttachment.reset();
    dynamicButtonAttachment.reset();
    spectralButtonAttachment.reset();
    spectralExpandButtonAttachment.reset();
    satButtonAttachment.reset();
    satPostButtonAttachment.reset();
    sidechainButtonAttachment.reset();
    sidechainMidiButtonAttachment.reset();
    spectralSatButtonAttachment.reset();
    spectralSatDriveAttachment.reset();
    filterModelAttachment.reset();
}

bool OptionBoxMenu::currentBandSupportsDynamic() const
{
    if (! DynamicEq::supportsDynamic (currentBandIndex))
        return false;

    // Notch / band-pass have no gain — dynamic EQ does not apply.
    if (currentBandIndex == 4 || currentBandIndex == 5)
        return false;

    const int type = customComboBox.getSelectedItemIndex();
    return FilterType::usesGain (type);
}

bool OptionBoxMenu::currentBandSupportsSpectral() const
{
    if (! SpectralDynamics::supportsSpectral (currentBandIndex))
        return false;

    if (currentBandIndex == 4 || currentBandIndex == 5)
        return false;

    const int type = customComboBox.getSelectedItemIndex();
    return FilterType::usesGain (type);
}

bool OptionBoxMenu::currentBandSupportsSidechain() const
{
    return currentBandSupportsDynamic();
}

bool OptionBoxMenu::currentBandSupportsSat() const
{
    if (! BandSaturation::supportsSat (currentBandIndex))
        return false;

    const int type = customComboBox.getSelectedItemIndex();
    return FilterType::usesGain (type);
}

void OptionBoxMenu::updateDynamicControlsVisibility()
{
    const bool showD = currentBandSupportsDynamic();
    const bool showS = currentBandSupportsSpectral();
    const bool showSc = currentBandSupportsSidechain();
    dynamicButton.setVisible (showD);
    spectralButton.setVisible (showS);
    sidechainButton.setVisible (showSc);

    const bool dOn = showD && dynamicButton.getToggleState();
    const bool sOn = showS && spectralButton.getToggleState();
    const bool scOn = showSc && sidechainButton.getToggleState();
    sidechainMidiButton.setVisible (scOn);

    // Threshold is D-only; A/R shared by D, S, and Sidechain.
    dynThresholdSlider.setVisible (dOn);
    const bool showAR = dOn || sOn || scOn;
    attackKnob.setVisible (showAR);
    releaseKnob.setVisible (showAR);
    attackLabel.setVisible (showAR);
    releaseLabel.setVisible (showAR);

    // S exposes Res + Amount + Expand invert.
    spectralBandwidthSlider.setVisible (sOn);
    spectralResLabel.setVisible (sOn);
    spectralAmountSlider.setVisible (sOn);
    spectralAmountLabel.setVisible (sOn);
    spectralExpandButton.setVisible (sOn);
    spectralPackButton.setVisible (sOn);

    if (sOn)
    {
        syncSpectralPackButton();
        spectralAmountSlider.setTooltip (spectralExpandButton.getToggleState()
            ? "Amount - pull down for more spectral resonance expansion"
            : "Amount - pull down for more spectral resonance attenuation");
    }

    syncSatControls();
    syncSpectralSatControls();
}

void OptionBoxMenu::bindDynamicControls (int bandIndex)
{
    dynamicButtonAttachment.reset();
    spectralButtonAttachment.reset();
    spectralExpandButtonAttachment.reset();
    satButtonAttachment.reset();
    satPostButtonAttachment.reset();
    sidechainButtonAttachment.reset();
    sidechainMidiButtonAttachment.reset();
    // Stage 2 is global — keep attachments alive across band switches; rebind if missing.
    dynThresholdAttachment.reset();
    attackMsAttachment.reset();
    releaseMsAttachment.reset();
    spectralBandwidthAttachment.reset();
    spectralAmountSlider.onDragStart = nullptr;
    spectralAmountSlider.onDragEnd = nullptr;
    spectralAmountSlider.onValueChange = nullptr;
    spectralAmountAttachment.reset();

    const auto dynID = DynamicEq::dynamicParamIDForBandIndex (bandIndex);
    const auto spectralID = SpectralDynamics::spectralParamIDForBandIndex (bandIndex);
    const auto threshID = DynamicEq::thresholdParamIDForBandIndex (bandIndex);
    const auto attackID = DynamicEq::attackMsParamIDForBandIndex (bandIndex);
    const auto releaseID = DynamicEq::releaseMsParamIDForBandIndex (bandIndex);
    const auto resHzID = SpectralDynamics::spectralResHzParamIDForBandIndex (bandIndex);
    const auto amountID = SpectralDynamics::spectralAmountParamIDForBandIndex (bandIndex);
    const auto expandID = SpectralDynamics::spectralExpandParamIDForBandIndex (bandIndex);
    const auto satID = BandSaturation::satParamIDForBandIndex (bandIndex);
    const auto satPostID = BandSaturation::satPostParamIDForBandIndex (bandIndex);
    const auto scID = BandSidechain::sidechainParamIDForBandIndex (bandIndex);
    const auto scMidiID = BandSidechain::midiParamIDForBandIndex (bandIndex);

    if (threshID.isEmpty())
    {
        updateDynamicControlsVisibility();
        return;
    }

    if (dynID.isNotEmpty())
        dynamicButtonAttachment = std::make_unique<ButtonAttachment> (treeState, dynID, dynamicButton);

    if (spectralID.isNotEmpty())
        spectralButtonAttachment = std::make_unique<ButtonAttachment> (treeState, spectralID, spectralButton);

    if (scID.isNotEmpty())
        sidechainButtonAttachment = std::make_unique<ButtonAttachment> (treeState, scID, sidechainButton);

    if (scMidiID.isNotEmpty())
        sidechainMidiButtonAttachment = std::make_unique<ButtonAttachment> (treeState, scMidiID, sidechainMidiButton);


    if (expandID.isNotEmpty())
        spectralExpandButtonAttachment = std::make_unique<ButtonAttachment> (
            treeState, expandID, spectralExpandButton);

    if (satID.isNotEmpty())
        satButtonAttachment = std::make_unique<ButtonAttachment> (treeState, satID, satButton);

    if (satPostID.isNotEmpty())
        satPostButtonAttachment = std::make_unique<ButtonAttachment> (
            treeState, satPostID, satPrePostButton);

    if (spectralSatButtonAttachment == nullptr)
        spectralSatButtonAttachment = std::make_unique<ButtonAttachment> (
            treeState, BandSaturation::spectralSatParamId(), spectralSatButton);

    if (spectralSatDriveAttachment == nullptr)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (
                treeState.getParameter (BandSaturation::spectralSatDriveParamId())))
        {
            const auto range = param->getNormalisableRange();
            spectralSatDriveKnob.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start,
                (double) range.end,
                (double) range.interval,
                (double) range.skew,
                range.symmetricSkew));
        }

        spectralSatDriveAttachment = std::make_unique<SliderAttachment> (
            treeState, BandSaturation::spectralSatDriveParamId(), spectralSatDriveKnob);
    }

    if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter (threshID)))
    {
        const auto range = param->getNormalisableRange();
        dynThresholdSlider.setNormalisableRange (juce::NormalisableRange<double> (
            (double) range.start,
            (double) range.end,
            (double) range.interval,
            (double) range.skew,
            range.symmetricSkew));
    }

    dynThresholdAttachment = std::make_unique<SliderAttachment> (treeState, threshID, dynThresholdSlider);

    auto bindTimeKnob = [this] (const juce::String& paramID, juce::Slider& knob,
                                std::unique_ptr<SliderAttachment>& attachment)
    {
        if (paramID.isEmpty())
            return;

        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter (paramID)))
        {
            const auto range = param->getNormalisableRange();
            knob.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start,
                (double) range.end,
                (double) range.interval,
                (double) range.skew,
                range.symmetricSkew));
        }

        attachment = std::make_unique<SliderAttachment> (treeState, paramID, knob);
    };

    bindTimeKnob (attackID, attackKnob, attackMsAttachment);
    bindTimeKnob (releaseID, releaseKnob, releaseMsAttachment);

    if (resHzID.isNotEmpty())
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter (resHzID)))
        {
            const auto range = param->getNormalisableRange();
            spectralBandwidthSlider.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start,
                (double) range.end,
                (double) range.interval,
                (double) range.skew,
                range.symmetricSkew));
        }

        spectralBandwidthAttachment = std::make_unique<SliderAttachment> (treeState, resHzID, spectralBandwidthSlider);
    }

    if (amountID.isNotEmpty())
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter (amountID)))
        {
            // Slider shows inverted travel: bottom (0) = max Amount, top (1) = Amount 0.
            // Important: Amount ≈ 0 hard-bypasses the BP bank (no S CPU) even if S is on.
            spectralAmountSlider.setRange (0.0, 1.0, 0.01);
            spectralAmountAttachment = std::make_unique<juce::ParameterAttachment> (
                *param,
                [this] (float amount)
                {
                    const double t = juce::jlimit (0.0, 1.0,
                        (double) amount / (double) SpectralDynamics::kMaxSpectralAmount);
                    spectralAmountSlider.setValue (1.0 - t, juce::dontSendNotification);
                },
                nullptr);

            spectralAmountAttachment->sendInitialUpdate();

            spectralAmountSlider.onDragStart = [this]
            {
                if (spectralAmountAttachment != nullptr)
                    spectralAmountAttachment->beginGesture();
            };
            spectralAmountSlider.onDragEnd = [this]
            {
                if (spectralAmountAttachment != nullptr)
                    spectralAmountAttachment->endGesture();
            };
            spectralAmountSlider.onValueChange = [this]
            {
                if (spectralAmountAttachment != nullptr)
                    spectralAmountAttachment->setValueAsPartOfGesture (
                        (float) ((1.0 - spectralAmountSlider.getValue())
                                 * (double) SpectralDynamics::kMaxSpectralAmount));
            };
        }
    }

    updateDynamicControlsVisibility();
}

void OptionBoxMenu::setupFilterModelMenu (int bandIndex)
{
    filterModelAttachment.reset();
    customComboBox.clear (juce::dontSendNotification);

    if (bandIndex == 4 || bandIndex == 5)
    {
        const auto names = FilterSlope::getChoiceNames();
        for (int i = 0; i < names.size(); ++i)
            customComboBox.addItem (names[i], i + 1); // ComboBox IDs are 1-based

        const juce::String slopeID = (bandIndex == 4) ? "highpassSlope" : "lowpassSlope";
        filterModelAttachment = std::make_unique<ComboBoxAttachment> (treeState, slopeID, customComboBox);
        customComboBox.setEnabled (true);
    }
    else
    {
        const auto names = FilterType::getChoiceNames();
        for (int i = 0; i < names.size(); ++i)
            customComboBox.addItem (names[i], i + 1);

        const auto typeID = FilterType::paramIDForBandIndex (bandIndex);
        if (typeID.isNotEmpty())
            filterModelAttachment = std::make_unique<ComboBoxAttachment> (treeState, typeID, customComboBox);

        customComboBox.setEnabled (typeID.isNotEmpty());
    }
}

void OptionBoxMenu::comboBoxChanged (juce::ComboBox* comboBoxThatHasChanged)
{
    if (comboBoxThatHasChanged != &customComboBox)
        return;

    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Filter type");

    if (currentBandIndex == 4 || currentBandIndex == 5)
    {
        // Hide Q for 6 dB/oct (1st-order has no Q).
        const bool showQ = customComboBox.getSelectedItemIndex() != FilterSlope::db6;
        rotaryImageKnobForOptionBox3.setVisible (showQ);
        qLabel.setVisible (showQ);
        return;
    }

    // Notch / band-pass have no gain control.
    const int type = customComboBox.getSelectedItemIndex();
    const bool showGain = FilterType::usesGain (type);
    rotaryImageKnobForOptionBox2.setVisible (showGain);
    gainLabel.setVisible (showGain);
    rotaryImageKnobForOptionBox3.setVisible (true);
    qLabel.setVisible (true);
    updateDynamicControlsVisibility();
}

void OptionBoxMenu::bindKnobsToBand (int index)
{
    clearAttachments();
    setupFilterModelMenu (index);

    auto attachFloat = [this] (std::unique_ptr<SliderAttachment>& attachment,
                               const juce::String& paramID,
                               juce::Slider& slider)
    {
        if (auto* param = dynamic_cast<juce::RangedAudioParameter*> (treeState.getParameter (paramID)))
        {
            const auto range = param->getNormalisableRange();
            slider.setNormalisableRange (juce::NormalisableRange<double> (
                (double) range.start,
                (double) range.end,
                (double) range.interval,
                (double) range.skew,
                range.symmetricSkew));
        }

        attachment = std::make_unique<SliderAttachment> (treeState, paramID, slider);
    };

    switch (index)
    {
        case 0:
            attachFloat (frequencyAttachment, "band1Frequency", rotaryImageKnobForOptionBox1);
            attachFloat (gainAttachment, "band1Gain", rotaryImageKnobForOptionBox2);
            attachFloat (qAttachment, "band1Q", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("band1OnOff");
            break;
        case 1:
            attachFloat (frequencyAttachment, "band2Frequency", rotaryImageKnobForOptionBox1);
            attachFloat (gainAttachment, "band2Gain", rotaryImageKnobForOptionBox2);
            attachFloat (qAttachment, "band2Q", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("band2OnOff");
            break;
        case 2:
            attachFloat (frequencyAttachment, "band3Frequency", rotaryImageKnobForOptionBox1);
            attachFloat (gainAttachment, "band3Gain", rotaryImageKnobForOptionBox2);
            attachFloat (qAttachment, "band3Q", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("band3OnOff");
            break;
        case 3:
            attachFloat (frequencyAttachment, "band4Frequency", rotaryImageKnobForOptionBox1);
            attachFloat (gainAttachment, "band4Gain", rotaryImageKnobForOptionBox2);
            attachFloat (qAttachment, "band4Q", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("band4OnOff");
            break;
        case 4:
            attachFloat (frequencyAttachment, "highpassCutoff", rotaryImageKnobForOptionBox1);
            attachFloat (qAttachment, "highpassQ", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("highpassOnOff");
            break;
        case 5:
            attachFloat (frequencyAttachment, "lowpassCutoff", rotaryImageKnobForOptionBox1);
            attachFloat (qAttachment, "lowpassQ", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("lowpassOnOff");
            break;
        case 6:
            attachFloat (frequencyAttachment, "highShelfFrequency", rotaryImageKnobForOptionBox1);
            attachFloat (gainAttachment, "highShelfGain", rotaryImageKnobForOptionBox2);
            attachFloat (qAttachment, "highShelfQ", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("highShelfOnOff");
            break;
        case 7:
            attachFloat (frequencyAttachment, "lowShelfFrequency", rotaryImageKnobForOptionBox1);
            attachFloat (gainAttachment, "lowShelfGain", rotaryImageKnobForOptionBox2);
            attachFloat (qAttachment, "lowShelfQ", rotaryImageKnobForOptionBox3);
            onOffButton1->setParameterID ("lowShelfOnOff");
            break;
        default:
            break;
    }

    // HP/LP: no gain. Other bands: gain depends on selected filter model.
    if (index == 4 || index == 5)
    {
        rotaryImageKnobForOptionBox2.setVisible (false);
        gainLabel.setVisible (false);
        comboBoxChanged (&customComboBox);
    }
    else
    {
        comboBoxChanged (&customComboBox);
    }

    bindDynamicControls (index);
}

void OptionBoxMenu::setCurrentBandIndex(int index, const std::string bandNames[])
{
    if (index >= 0 && index < 8)
    {
        if (bandNames != nullptr)
        {
            for (int i = 0; i < 8; ++i)
                cachedBandNames[(size_t) i] = bandNames[i];
        }

        listenToCurrentBandOnOff (false);
        listenToCurrentBandDynamic (false);
        listenToCurrentBandSpectral (false);
        currentBandIndex = index;
        currentBandName = cachedBandNames[(size_t) index];
        bandNameLabel.setText(juce::String(currentBandName), juce::NotificationType::dontSendNotification);
        bindKnobsToBand (index);
        syncChannelModeButtons();
        listenToCurrentBandOnOff (true);
        listenToCurrentBandDynamic (true);
        listenToCurrentBandSpectral (true);
        updateDisplayAlpha();
        resized();
    }
}

void OptionBoxMenu::setCurrentBandName(const std::string& name) {
    currentBandName = name;
    bandNameLabel.setText(juce::String(currentBandName), juce::NotificationType::dontSendNotification);
}

void OptionBoxMenu::syncChannelModeButtons()
{
    const auto paramID = BandChannel::paramIDForBandIndex (currentBandIndex);
    const int mode = paramID.isNotEmpty()
                         ? BandChannel::readChoiceIndex (treeState, paramID, BandChannel::stereo)
                         : BandChannel::stereo;

    midSelectorButton.setToggleState (mode == BandChannel::mid, juce::dontSendNotification);
    sideSelectorButton.setToggleState (mode == BandChannel::side, juce::dontSendNotification);
    leftSelectorButton.setToggleState (mode == BandChannel::left, juce::dontSendNotification);
    rightSelectorButton.setToggleState (mode == BandChannel::right, juce::dontSendNotification);
}

void OptionBoxMenu::syncSpectralPackButton()
{
    int idx = 0;
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
            treeState.getParameter (SpectralDynamics::spectralPackParamId())))
        idx = p->getIndex();

    // Compact labels: Flat→FL, LF→LP (low pack), HF→HP (high pack).
    const char* label = "FL";
    switch (idx)
    {
        case 1:  label = "LP"; break;
        case 2:  label = "HP"; break;
        default: label = "FL"; break;
    }
    spectralPackButton.setButtonText (label);
    // Lit when packing LF or HF (not Flat).
    spectralPackButton.setToggleState (idx != 0, juce::dontSendNotification);
}

void OptionBoxMenu::cycleSpectralPackMode()
{
    if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
            treeState.getParameter (SpectralDynamics::spectralPackParamId())))
    {
        *p = SpectralDynamics::nextPackChoiceIndex (p->getIndex());
        syncSpectralPackButton();
    }
}

void OptionBoxMenu::syncSatControls()
{
    const bool show = currentBandSupportsSat();
    satButton.setVisible (show);

    const bool satOn = show && satButton.getToggleState();
    satPrePostButton.setVisible (satOn);
    satPrePostButton.setButtonText (satPrePostButton.getToggleState() ? "Post" : "Pre");
}

void OptionBoxMenu::syncSpectralSatControls()
{
    const bool showS = currentBandSupportsSpectral();
    const bool sOn = showS && spectralButton.getToggleState();
    spectralSatButton.setVisible (sOn);

    const bool ssOn = sOn && spectralSatButton.getToggleState();
    spectralSatDriveKnob.setVisible (ssOn);
}

void OptionBoxMenu::showSpectralSatContextMenu()
{
    auto* modelParam = dynamic_cast<juce::AudioParameterChoice*> (
        treeState.getParameter (BandSaturation::spectralSatModelParamId()));
    auto* osParam = dynamic_cast<juce::AudioParameterChoice*> (
        treeState.getParameter (BandSaturation::spectralSatOversampleParamId()));
    if (modelParam == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const auto modelNames = BandSaturation::getModelChoiceNames();
    for (int i = 0; i < modelNames.size(); ++i)
        menu.addItem (i + 1, modelNames[i], true, modelParam->getIndex() == i);

    if (osParam != nullptr)
    {
        menu.addSeparator();
        juce::PopupMenu osMenu;
        const auto osNames = BandSaturation::getOversampleChoiceNames();
        for (int i = 0; i < osNames.size(); ++i)
            osMenu.addItem (100 + i, osNames[i], true, osParam->getIndex() == i);
        menu.addSubMenu ("Oversample", osMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&spectralSatButton),
        [this, modelParam, osParam] (int result)
        {
            if (result <= 0)
                return;

            if (undoManager != nullptr)
                undoManager->beginNewTransaction ("SS model");

            if (result >= 1 && result <= BandSaturation::numModels)
                *modelParam = result - 1;
            else if (osParam != nullptr && result >= 100 && result < 100 + BandSaturation::numOversample)
                *osParam = result - 100;
        });
}

void OptionBoxMenu::showSatContextMenu()
{
    const auto modelID = BandSaturation::satModelParamIDForBandIndex (currentBandIndex);
    if (modelID.isEmpty())
        return;

    auto* modelParam = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (modelID));
    auto* osParam = dynamic_cast<juce::AudioParameterChoice*> (
        treeState.getParameter (BandSaturation::oversampleParamId()));
    if (modelParam == nullptr)
        return;

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    const auto modelNames = BandSaturation::getModelChoiceNames();
    for (int i = 0; i < modelNames.size(); ++i)
        menu.addItem (i + 1, modelNames[i], true, modelParam->getIndex() == i);

    if (osParam != nullptr)
    {
        menu.addSeparator();
        juce::PopupMenu osMenu;
        const auto osNames = BandSaturation::getOversampleChoiceNames();
        for (int i = 0; i < osNames.size(); ++i)
            osMenu.addItem (100 + i, osNames[i], true, osParam->getIndex() == i);
        menu.addSubMenu ("Oversample", osMenu);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&satButton),
        [this, modelParam, osParam] (int result)
        {
            if (result <= 0)
                return;

            if (undoManager != nullptr)
                undoManager->beginNewTransaction ("Sat model");

            if (result >= 1 && result <= BandSaturation::numModels)
            {
                *modelParam = result - 1;
            }
            else if (osParam != nullptr && result >= 100 && result < 100 + BandSaturation::numOversample)
            {
                *osParam = result - 100;
            }
        });
}

void OptionBoxMenu::setChannelMode (int mode)
{
    const auto paramID = BandChannel::paramIDForBandIndex (currentBandIndex);
    if (paramID.isEmpty())
        return;

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (treeState.getParameter (paramID)))
        *param = mode;

    syncChannelModeButtons();
}

void OptionBoxMenu::buttonClicked(juce::Button* button)
{
    if (button == &prevBandButton || button == &nextBandButton)
    {
        cycleBand (button == &nextBandButton ? 1 : -1);
        return;
    }

    if (undoManager != nullptr)
        undoManager->beginNewTransaction ("Band option");

    if (button == &dynamicButton || button == &spectralButton
        || button == &spectralExpandButton || button == &satButton
        || button == &satPrePostButton || button == &spectralSatButton
        || button == &sidechainButton || button == &sidechainMidiButton)
    {
        updateDynamicControlsVisibility();
        // Threshold / A-R anchors shift when S turns on (vertical Res+Amount cluster).
        resized();
        return;
    }

    if (button == &spectralPackButton)
    {
        cycleSpectralPackMode();
        return;
    }

    int requested = BandChannel::stereo;

    if (button == &midSelectorButton)        requested = BandChannel::mid;
    else if (button == &sideSelectorButton)  requested = BandChannel::side;
    else if (button == &leftSelectorButton)  requested = BandChannel::left;
    else if (button == &rightSelectorButton) requested = BandChannel::right;
    else
        return;

    const auto paramID = BandChannel::paramIDForBandIndex (currentBandIndex);
    const int current = paramID.isNotEmpty()
                            ? BandChannel::readChoiceIndex (treeState, paramID, BandChannel::stereo)
                            : BandChannel::stereo;

    // Clicking the active mode again returns to stereo (both channels).
    setChannelMode (current == requested ? BandChannel::stereo : requested);
}
