#include "HueSelector.h"
#include "../SharedResources.h"

HueSelector::HueSelector(SharedResources& resources)
    : sharedResources(resources)
{
    juce::Colour arrowSliderThumbColor = juce::Colour::fromRGBA(70, 70, 65, 255.0f);
    juce::Colour arrowBackgroundColor = sharedResources.sharedColors.menuScrollBarThumbColor1;
    juce::Colour arrowOutlineColor = sharedResources.sharedColors.menuScrollBarOutlineColor1;

    hueSlider.setSliderStyle(juce::Slider::LinearVertical);
    hueSlider.setRange(0.0, 1.0, 0.01);
    hueSlider.setValue(0.0);
    hueSlider.addListener(this);
    hueSlider.setSliderSnapsToMousePosition(true);
    hueSlider.setMouseDragSensitivity(1);
    hueSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    hueSlider.setAlwaysOnTop(true);
    hueSlider.toFront(true);
   // hueSlider.setArrowProperties
    // Set background and track colors to be transparent
    hueSlider.setColour(juce::Slider::backgroundColourId, juce::Colours::transparentBlack);
    hueSlider.setColour(juce::Slider::trackColourId, juce::Colours::transparentBlack);

    addAndMakeVisible(hueSlider);

  

    DBG("HueSelector initialized...");
}





HueSelector::~HueSelector()
{
    hueSlider.setLookAndFeel(nullptr);  
}

void HueSelector::paint(juce::Graphics& g) {
    if (gradientImage.isValid()) {
        int padding = 5;
        int extraPadding = 2;
        int sliderWidth = 30;
        int gradientX = (getWidth() + padding) / 2 + extraPadding;
        juce::Rectangle<int> gradientRect(gradientX, extraPadding, gradientImage.getWidth(), gradientImage.getHeight());

        float cornerSize = 5.0f;
        juce::Path roundedClipPath;
        roundedClipPath.addRoundedRectangle(gradientRect.toFloat(), cornerSize);

        shadow.render(g, roundedClipPath); 

        g.saveState();
        g.reduceClipRegion(roundedClipPath);
        g.drawImage(gradientImage, gradientRect.getX(), gradientRect.getY(), gradientRect.getWidth(), gradientRect.getHeight(), 0, 0, gradientImage.getWidth(), gradientImage.getHeight());
        g.restoreState();


        drawWhiteLine(g);
    }
}

void HueSelector::resized()
{
    int sliderWidth = 35;
    int padding = 20;
    int totalWidth = sliderWidth + padding;
    int startX = (getWidth() - totalWidth) / 2;

    // Set bounds for slider
    hueSlider.setBounds (startX, 0, sliderWidth, getHeight());

    // Rebuild hue strip whenever the settings menu resizes this column.
    if (getWidth() > 0 && getHeight() > 0)
        generateGradientImage();
}

void HueSelector::sliderValueChanged(juce::Slider* slider)
{
    if (slider == &hueSlider)
    {
        float selectedHue = hueSlider.getValue();
        selectedHue = selectedHue;  // Invert the hue value

        // Invoke the onHueChanged callback with the selected hue
        if (onHueChanged) onHueChanged(selectedHue);

        // Notify listeners about the hue change
        listeners.call([selectedHue](Listener& l) { l.hueChanged(selectedHue); });
    }
}




void HueSelector::addListener(Listener* newListener)
{
    listeners.add(newListener);
}

void HueSelector::setColor(const juce::Colour& newColor)
{
    DBG("HueSelector::setColor called with color: " + newColor.toString());  // Debug statement

    float hue, saturation, brightness, alpha;
    newColor.getHSB(hue, saturation, brightness);
    alpha = newColor.getAlpha();

    DBG("Hue: " + juce::String(hue) + ", Alpha: " + juce::String(alpha));

    // Ensure hue is within a valid range before using it
    if (hue >= 0.0f && hue <= 1.0f)
    {
        hueSlider.setValue(hue, juce::dontSendNotification); // Set the slider value to the new hue
    }
    else
    {
        // Handle the case where hue is out of range, if needed
    }

    repaint(); // Repaint to reflect the new hue value

    // Store alpha value for later use when notifying listeners or updating colors
    // Note: You need to add a member variable to store this alpha value
    this->alphaValue = alpha;
}




void HueSelector::onElementSelected(const juce::String& name, const juce::Colour& color)
{
    setColor(color);  // Update the color of HueSelector
}

void HueSelector::updateHueSliderLookAndFeel(ArrowSliderLookAndFeel::Direction direction, juce::Colour arrowColor, juce::Colour arrowOutlineColor, float size) 
{
    // Update ArrowSliderLookAndFeel with new colors
    hueSlider.setArrowProperties(direction, arrowColor, arrowOutlineColor, size);
    hueSlider.repaint();
}

void HueSelector:: generateGradientImage() {
    int padding = 5;
    int extraPadding = 2;
    int sliderWidth = 30;
    int gradientWidth = juce::jmax (1, getWidth() - sliderWidth - padding * 3 - extraPadding * 2);
    int gradientHeight = juce::jmax (1, getHeight() - padding / 2 - extraPadding * 2);
    juce::Rectangle<int> imageBounds (0, 0, gradientWidth, gradientHeight);

    gradientImage = juce::Image(juce::Image::PixelFormat::ARGB, imageBounds.getWidth(), imageBounds.getHeight(), true);
    juce::Graphics g(gradientImage);

    float cornerSize = 5.0f;
    juce::Path roundedClipPath;
    roundedClipPath.addRoundedRectangle(imageBounds.toFloat(), cornerSize);
    g.reduceClipRegion(roundedClipPath);

    juce::ColourGradient gradient(juce::Colours::red, 0, 0, juce::Colours::red, 0, imageBounds.getHeight(), false);
    gradient.addColour(1.0 / 6.0, juce::Colours::magenta);
    gradient.addColour(2.0 / 6.0, juce::Colours::blue);
    gradient.addColour(3.0 / 6.0, juce::Colours::cyan);
    gradient.addColour(4.0 / 6.0, juce::Colours::green);
    gradient.addColour(5.0 / 6.0, juce::Colours::yellow);
    g.setGradientFill(gradient);
    g.fillRect(imageBounds);


}

juce::Rectangle<int> HueSelector::getGradientBounds() const {
    int padding = 5;
    int extraPadding = 2;
    int gradientX = (getWidth() + padding) / 2 + extraPadding;
    return juce::Rectangle<int>(gradientX, extraPadding, gradientImage.getWidth(), gradientImage.getHeight());
}

void HueSelector::drawWhiteLine(juce::Graphics& g) {
    float sliderPos = hueSlider.getVerticalThumbPosition();
    float lineHeight = 3.0f; // Line height
    juce::Rectangle<int> gradientBounds = getGradientBounds();

    // Calculate the position of the line
    float lineX = gradientBounds.getX();
    float lineWidth = gradientBounds.getWidth();
    float lineY = sliderPos - lineHeight / 2;

    // Create a rectangle path for the shadow2 based on the line's position
    juce::Path shadowPath;
    shadowPath.addRoundedRectangle(lineX, lineY, lineWidth, lineHeight, 2.0f); // You can adjust the corner radius as needed

    // Render the shadow2 using the path
    shadow3.render(g, shadowPath);

    // Draw the white line
    g.setColour(juce::Colours::white);
    g.fillRect(juce::Rectangle<float>(lineX, lineY, lineWidth, lineHeight));
}