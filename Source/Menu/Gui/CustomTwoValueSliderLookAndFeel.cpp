#include "CustomTwoValueSliderLookAndFeel.h"

CustomTwoValueSliderLookAndFeel::CustomTwoValueSliderLookAndFeel()
    : thumbStyle(Round), arrowOrientation(Up),
    sliderTrackColor(juce::Colours::lightgrey),
    sliderBackgroundColor(juce::Colours::darkgrey),
    sliderThumbColor(juce::Colours::whitesmoke) // Default color values
{
}


CustomTwoValueSliderLookAndFeel::~CustomTwoValueSliderLookAndFeel()
{
  
}
void CustomTwoValueSliderLookAndFeel::drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
    float sliderPos, float minSliderPos, float maxSliderPos,
    const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    bool isVertical = (style == juce::Slider::TwoValueVertical);
    const int thumbWidth = 10;
    const int thumbHeight = 10;

    // Adjust the track position to account for the padding
    int paddedX = x ;
    int paddedY = y ;


    if (isVertical)
    {
        // Define offsets for the track
        int trackOffsetX = -10; // Horizontal offset for the track
        int trackOffsetY = -25; // Vertical offset for the track

        // Create and draw the track image with offsets
        juce::Image trackImage = createTrackImage(x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        g.drawImageAt(trackImage, x - 8 + trackOffsetX, y - 8 + trackOffsetY);
    }
    else
    {
        // Define offsets for the track
        int trackOffsetX = -25; // Horizontal offset for the track
        int trackOffsetY = -14; // Vertical offset for the track

        // Create and draw the track image with offsets
        juce::Image trackImage = createTrackImage(x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);
        g.drawImageAt(trackImage, x - 8 + trackOffsetX, y - 8 + trackOffsetY);
    }

    
    // Define offsets for the thumbs
    int verticalThumbOffsetX = -6; // Horizontal offset for the thumbs
    int verticalThumbOffsetY =  -10; // Vertical offset for the thumbs
   
    int horizontalThumbOffsetX = -10; // Horizontal offset for the thumbs
    int horizontalThumbOffsetY = -10; // Vertical offset for the thumbs

    // Create the thumb image
    juce::Image thumbImage = createThumbImage(thumbWidth, thumbHeight, slider);

    int thumbPosX, thumbPosY;

    // Calculate positions for drawing thumbs with offsets
    if (isVertical)
    {
        thumbPosX = x + width / 2 - thumbWidth / 2 + verticalThumbOffsetX; // Centered horizontally with offset
        thumbPosY = static_cast<int>(minSliderPos) - thumbHeight / 2 + verticalThumbOffsetY; // Position for the lower thumb with offset
        g.drawImageAt(thumbImage, thumbPosX, thumbPosY);
        thumbPosY = static_cast<int>(maxSliderPos) - thumbHeight / 2 + verticalThumbOffsetY; // Position for the upper thumb with offset
        g.drawImageAt(thumbImage, thumbPosX, thumbPosY);
    }
    else
    {
        thumbPosX = static_cast<int>(minSliderPos) - thumbWidth / 2 + horizontalThumbOffsetX; // Position for the left thumb with offset
        thumbPosY = y + height / 2 - thumbHeight / 2 + horizontalThumbOffsetY; // Centered vertically with offset
        g.drawImageAt(thumbImage, thumbPosX, thumbPosY);
        thumbPosX = static_cast<int>(maxSliderPos) - thumbWidth / 2 + horizontalThumbOffsetX; // Position for the right thumb with offset
        g.drawImageAt(thumbImage, thumbPosX, thumbPosY);
    }
}

void CustomTwoValueSliderLookAndFeel::createArrowShape(juce::Path& path, int width, int height)
{
    // Define the arrow shape here
    // ...
}

void CustomTwoValueSliderLookAndFeel::applyArrowTransformation(juce::Path& path, float thumbX, float thumbY)
{
    juce::AffineTransform transform;
    transform = transform.rotated(getRotationForOrientation()).translated(thumbX, thumbY);
    path.applyTransform(transform);
}

float CustomTwoValueSliderLookAndFeel::getRotationForOrientation() const
{
    switch (arrowOrientation) {
    case Up: return 0.0f;
    case Down: return juce::MathConstants<float>::pi;
    case Left: return -juce::MathConstants<float>::halfPi;
    case Right: return juce::MathConstants<float>::halfPi;
    default: return 0.0f;
    }
}


void CustomTwoValueSliderLookAndFeel::updateGlowShadowColor(const juce::Colour& newColor)
{
    glowShadow = { { newColor, 10, { 0, 0 }, 2 } };
}

void CustomTwoValueSliderLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
  /*  // Your drawLabel implementation here, as provided in your original code
    g.fillAll(label.findColour(juce::Label::backgroundColourId));

    if (!label.isBeingEdited()) {
        auto alpha = label.isEnabled() ? 1.0f : 0.5f;
        const juce::Font customFont("Lato Black", 16.0f, juce::Font::plain);

        g.setColour(label.findColour(juce::Label::textColourId).withMultipliedAlpha(alpha));
        g.setFont(customFont);

        juce::Rectangle<int> textArea(label.getBorderSize().subtractedFrom(label.getLocalBounds()));
        g.drawFittedText(label.getText(), textArea, label.getJustificationType(),
            juce::jmax(1, (int)(textArea.getHeight() / customFont.getHeight())),
            label.getMinimumHorizontalScale());

        g.setColour(label.findColour(juce::Label::outlineColourId).withMultipliedAlpha(alpha));
    }

    */
}

void CustomTwoValueSliderLookAndFeel::setSliderTrackColor(const juce::Colour& newColour)
{
    sliderTrackColor = newColour;
}

void CustomTwoValueSliderLookAndFeel::setSliderBackgroundColor(const juce::Colour& newColour)
{
    sliderBackgroundColor = newColour;
    updateGlowShadowColor(newColour);
}

void CustomTwoValueSliderLookAndFeel::setSliderThumbColor(const juce::Colour& newColour)
{
    sliderThumbColor = newColour;
}

void CustomTwoValueSliderLookAndFeel::applyThemeColors(const juce::Colour& trackColor, const juce::Colour& backgroundColor, const juce::Colour& thumbColor)
{
    setSliderTrackColor(trackColor);
    setSliderBackgroundColor(backgroundColor);
    setSliderThumbColor(thumbColor);


    // Now trigger a repaint on all buttons using this look and feel
    for (auto* button : slidersUsingCustomLookAndFeel) {
        button->setLookAndFeel(this); // Ensure the button is using the updated LookAndFeel
        button->repaint(); // Force the button to repaint with the new colors
    }
}

void CustomTwoValueSliderLookAndFeel::setArrowOrientation(ArrowOrientation newOrientation) {
    arrowOrientation = newOrientation;
}

void CustomTwoValueSliderLookAndFeel::setThumbStyle(ThumbStyle style) {
    thumbStyle = style;
}



juce::Image CustomTwoValueSliderLookAndFeel::createThumbImage(int thumbWidth, int thumbHeight, juce::Slider& slider)
{
    bool isVertical = (slider.getSliderStyle() == juce::Slider::LinearVertical ||
        slider.getSliderStyle() == juce::Slider::TwoValueVertical);

    int paddingX = 8;  // Horizontal padding
    int paddingY = 8;  // Vertical padding

    // Calculate the new width and height for the thumb image with padding
    int newWidth = thumbWidth + paddingX * 2;
    int newHeight = thumbHeight + paddingY * 2;

    juce::Image thumbImage(juce::Image::ARGB, newWidth, newHeight, true);
    juce::Graphics thumbGraphics(thumbImage);

    // Calculate the position to draw the thumb so it is centered within the new dimensions
    float thumbX = paddingX; // Add horizontal padding
    float thumbY = paddingY; // Add vertical padding

    if (isVertical) {
        thumbY = (newHeight - thumbHeight) / 2.0f; // Center vertically for vertical sliders
        thumbX -= 0; // Adjust for the offset
    }
    else {
        thumbX = (newWidth - thumbWidth) / 2.0f; // Center horizontally for horizontal sliders
        thumbY -= 0; // Adjust for the offset
    }

    juce::Path thumbPath;

    if (thumbStyle == Round) {
        thumbPath.addEllipse(thumbX, thumbY, thumbWidth, thumbHeight);
    }
    else if (thumbStyle == Arrow) {
        createArrowShape(thumbPath, thumbWidth, thumbHeight);
        applyArrowTransformation(thumbPath, thumbX, thumbY);
    }
  
    juce::Path thumbPath2;

    float rectWidth = thumbWidth / 5; // Adjust the width as needed
    float rectHeight = thumbHeight * 1.6; // Adjust the height as needed

    /*
    if (!isVertical) {
        // For a vertical slider, the rectangle should be horizontal
        thumbPath2.addRectangle(thumbX - rectWidth / 2 + thumbWidth / 2, thumbY - rectHeight / 2 + thumbWidth / 2, rectWidth, rectHeight);
    }
    else {
        // For a horizontal slider, the rectangle should be vertical
        thumbPath2.addRectangle(thumbX - rectHeight / 2 + thumbWidth / 2, thumbY - rectWidth / 2 + thumbWidth / 2, rectHeight, rectWidth);
    }
    
    thumbGraphics.setColour(juce::Colours::whitesmoke.withAlpha(0.85f));
    thumbGraphics.fillPath(thumbPath2);
    */


    // Render the thumb path as in your code
    shadow.render(thumbGraphics, thumbPath);
    thumbGraphics.setColour(sliderThumbColor);
    thumbGraphics.fillPath(thumbPath);
    innerShadow.render(thumbGraphics, thumbPath);

    return thumbImage;
}


juce::Image CustomTwoValueSliderLookAndFeel::createTrackImage(int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider)
{
    bool isVertical = (style == juce::Slider::TwoValueVertical);
    int padding = 20; // Padding size

    // Create an image with additional padding
    juce::Image trackImage(juce::Image::ARGB, width + padding * 2, height + padding * 2, true);
    juce::Graphics g(trackImage);

    // Adjust the drawing coordinates for the padding
    x += padding;
    y += padding;
    minSliderPos += padding;
    maxSliderPos += padding;

    // Dimensions for track and thumbs
    const int thumbWidth = 12;
    const int thumbHeight = 14;
    float trackWidthExpansion = 8;
    float cornerSize = 3.0f;

    // Define the trackWidth variable (default value is 8)
    float trackWidth = 4.0f;

    float trackLeft, trackRight, trackTop, trackBottom;
    if (isVertical)
    {
        trackLeft = x + width / 2 - trackWidth / 2;
        trackRight = trackLeft + trackWidth;
        trackTop = y - trackWidthExpansion / 2;
        trackBottom = y + height + trackWidthExpansion / 2;
    }
    else
    {
        trackLeft = x - trackWidthExpansion / 2;
        trackRight = x + width + trackWidthExpansion / 2;
        trackTop = y + height / 2 - trackWidth / 2;
        trackBottom = trackTop + trackWidth;
    }

    // Now you can use the trackWidth variable to control the width of the track when drawing it


    // Track Perimeter
    juce::Path trackPerimeter;
    trackPerimeter.addRoundedRectangle(juce::Rectangle<float>(trackLeft, trackTop, trackRight - trackLeft, trackBottom - trackTop), cornerSize);
    g.setColour(sliderTrackColor);
    g.fillPath(trackPerimeter);
    innerShadow2.render(g, trackPerimeter);

    if (isVertical)
    {
        innerShadowVertical.render(g, trackPerimeter);
    }
    else
    {
        innerShadow2.render(g, trackPerimeter);
    }


    // Filled Track
    juce::Path filledTrack;
    if (isVertical)
    {
        filledTrack.addRoundedRectangle(juce::Rectangle<float>(trackLeft, maxSliderPos, trackRight - trackLeft, minSliderPos - maxSliderPos), cornerSize);
    }
    else
    {
        filledTrack.addRoundedRectangle(juce::Rectangle<float>(minSliderPos, trackTop, maxSliderPos - minSliderPos, trackBottom - trackTop), cornerSize);
    }


    g.setColour(sliderBackgroundColor);
    g.fillPath(filledTrack);
    glowShadow.render(g, filledTrack);


    return trackImage;
}
