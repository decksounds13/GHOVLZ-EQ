#include "ColorValuesInput.h"
#include <JuceHeader.h>
#include "AppearanceComponent.h"
#include "../SharedResources.h"

ColorValuesInput::ColorValuesInput()
{
    createSliderAndLabel("R", "Red", 0, 255);
    createSliderAndLabel("G", "Green", 0, 255);
    createSliderAndLabel("B", "Blue", 0, 255);
    createSliderAndLabel("A", "Alpha", 0, 255);
    createSliderAndLabel("H", "Hue", 0, 255);
    createSliderAndLabel("S", "Saturation", 0, 255);
    createSliderAndLabel("B", "Brightness", 0, 255);
}

void ColorValuesInput::paint(juce::Graphics& g)
{
    // Optional: paint labels, backgrounds, etc.
}

void ColorValuesInput::resized()
{
    int padding = 5;
    int labelHeight = 10;
    int labelXOffset = 12;
    int sliderHeight = 26;
    int labelYOffset = 0; // Adjust this value to set the Y-offset for the labels
    int spacing = getHeight() / 33; // Reduced spacing

    // Calculate the available width and height based on local bounds
    int availableWidth = getWidth() - labelXOffset - padding;
    int availableHeight = getHeight() - padding;

    int yPosition = padding;

    for (int i = 0; i < colorSliders.size(); ++i)
    {
        colorLabels[i]->setBounds(labelXOffset + 10 , yPosition, availableWidth, labelHeight);
        yPosition += labelHeight;

        // Adjust the position of the sliders closer to the labels
        colorSliders[i]->setBounds(labelXOffset, yPosition + labelYOffset, availableWidth, sliderHeight);

        yPosition += sliderHeight / 2 + spacing;
    }
}

void ColorValuesInput::sliderValueChanged(juce::Slider* slider)
{
    juce::Colour newColor;

    // Check if the slider is one of the HSB sliders
    if (slider == colorSliders[4] || slider == colorSliders[5] || slider == colorSliders[6]) {
        float hue = (float) colorSliders[4]->getValue() / 255.0f;
        float saturation = (float) colorSliders[5]->getValue() / 255.0f;
        float brightness = (float) colorSliders[6]->getValue() / 255.0f;
        float alpha = (float) colorSliders[3]->getValue() / 255.0f;
        newColor = juce::Colour::fromHSV(hue, saturation, brightness, alpha);

        // Update AppearanceComponent's selectors if the hue slider has changed
        if (slider == colorSliders[4]) {
            if (auto* appearanceComponent = findParentComponentOfClass<AppearanceComponent>()) {
                appearanceComponent->updateColorSelectors(newColor);
            }
        }
    }
    else {
        // Handling RGBA sliders
        newColor = juce::Colour::fromRGBA(
            static_cast<uint8_t>(colorSliders[0]->getValue()),
            static_cast<uint8_t>(colorSliders[1]->getValue()),
            static_cast<uint8_t>(colorSliders[2]->getValue()),
            static_cast<uint8_t>(colorSliders[3]->getValue())
        );
    }

    // Notify AppearanceComponent of the color change (A slider / RGBA may change alpha)
    if (auto* appearanceComponent = findParentComponentOfClass<AppearanceComponent>()) {
        appearanceComponent->directColorUpdate(newColor, true);
    }

    // Notify via callback and listeners
    if (colorChangedCallback) {
        colorChangedCallback(newColor);
    }
    listeners.call([newColor](Listener& l) { l.colorValuesInputChanged(newColor); });
}

void ColorValuesInput::setColor(const juce::Colour& color)
{
    colorSliders[0]->setValue(color.getRed());
    colorSliders[1]->setValue(color.getGreen());
    colorSliders[2]->setValue(color.getBlue());
    colorSliders[3]->setValue(color.getAlpha());
}

void ColorValuesInput::createSliderAndLabel(const juce::String& shortLabel, const juce::String& fullLabel, float minValue, float maxValue)
{
    juce::Colour menuTextColor1 = juce::Colour::fromRGB(30, 25, 10);
    juce::Colour menuTextColor2 = juce::Colours::whitesmoke.withAlpha(0.85f);


    // Create and setup the slider
    auto* slider = new juce::Slider(shortLabel);
    slider->setRange(minValue, maxValue, 1);
    slider->setSliderStyle(juce::Slider::IncDecButtons);
    slider->setSliderStyle(juce::Slider::LinearHorizontal);
    slider->setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30, 20);

    // Get the background color from the slider
   // juce::Colour backgroundColor = slider->findColour(juce::Slider::backgroundColourId);

    // Update the glowShadow color with the background color
   // customLookAndFeel.updateGlowShadowColor(backgroundColor);

    slider->setColour(juce::Slider::backgroundColourId, juce::Colour::fromRGBA(30, 20, 10, 255));
    slider->setColour(juce::Slider::trackColourId, juce::Colour::fromRGBA(80, 75, 70, 255));
    slider->setColour(juce::Slider::thumbColourId, juce::Colour::fromRGBA(100, 96, 90, 255));
    slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha(0.85f));

    slider->setLookAndFeel(&customLookAndFeel);  // Apply the custom LookAndFeel
    slider->addListener(this);

    // Create and setup the label
    auto* label = new juce::Label();
    label->setText(fullLabel, juce::NotificationType::dontSendNotification);
    label->setFont(juce::Font("Lato Black", 16.0f, juce::Font::plain));
    label->setColour(juce::Label::textColourId, labelTextColor);

    // Calculate the bounds based on the text and font size
    juce::Rectangle<int> labelBounds = label->getBounds().withSizeKeepingCentre(200, 30); // Increase the height as needed
    label->setBounds(labelBounds);

    // Set the justification type to center the text vertically
    label->setJustificationType(juce::Justification::centred | juce::Justification::verticallyCentred);

    colorLabels.add(label);
    addAndMakeVisible(label);

    colorSliders.add(slider);
    addAndMakeVisible(slider);

}

//void ColorValuesInput::onColorChanged(std::function<void(const juce::Colour&)> callback)
//{
//    this->colorChangedCallback = callback;
//}

void ColorValuesInput::addListener(Listener* newListener)
{
    listeners.add(newListener);
}

void ColorValuesInput::removeListener(Listener* listener)
{
    listeners.remove(listener);
}

void ColorValuesInput::onElementSelected(const juce::String& name, const juce::Colour& color)
{
    setColor(color);  // Update the color of ColorValuesInput
}

void ColorValuesInput::setSliderTrackColor(const juce::Colour& color)
{
    for (int i = 0; i < colorSliders.size(); ++i)
    {
        colorSliders[i]->setColour(juce::Slider::trackColourId, color);
     
       // customLookAndFeel.updateGlowShadowColor(color);
    }
}

void ColorValuesInput::setSliderBackgroundColor(const juce::Colour& color)
{
    for (int i = 0; i < colorSliders.size(); ++i)
    {
        colorSliders[i]->setColour(juce::Slider::backgroundColourId, color);

        // Update the glowShadow color in the CustomLookAndFeel
        customLookAndFeel.updateGlowShadowColor(color);
    }
}

void ColorValuesInput::setSliderThumbColor(const juce::Colour& color)
{
    for (int i = 0; i < colorSliders.size(); ++i)
    {
        colorSliders[i]->setColour(juce::Slider::thumbColourId, color);
    }
}

void ColorValuesInput::setTextBoxTextColor(const juce::Colour& color)
{
    textBoxTextColor = color;

    for (int i = 0; i < colorSliders.size(); ++i)
    {
        colorSliders[i]->setColour(juce::Slider::textBoxTextColourId, textBoxTextColor);
    }
}

void ColorValuesInput:: setLabelTextColor(const juce::Colour& color)
{
    labelTextColor = color;

    for (int i = 0; i < colorLabels.size(); ++i)
    {
        colorLabels[i]->setColour(juce::Label::textColourId, labelTextColor);
    }
}




void ColorValuesInput::updateHueSlider(float hueValue) {
    // Assuming hue slider is at a specific index, e.g., 4
    colorSliders[4]->setValue(hueValue, juce::dontSendNotification);
}

void ColorValuesInput::updateSaturationSlider(float saturationValue) {
    // Assuming saturation slider is at a specific index, e.g., 5
    colorSliders[5]->setValue(saturationValue, juce::dontSendNotification);
}

void ColorValuesInput::updateBrightnessSlider(float brightnessValue) {
    // Assuming brightness slider is at a specific index, e.g., 6
    colorSliders[6]->setValue(brightnessValue, juce::dontSendNotification);
}
