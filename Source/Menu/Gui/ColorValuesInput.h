#pragma once

#include <JuceHeader.h>
#include "UIElementsList.h"
#include "../SharedResources.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "gin_graphics/gin_graphics.h"

class ColorValuesInput : public juce::Component,
    public juce::Slider::Listener,
    public UIElementsList::Listener
{
public:
    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void colorValuesInputChanged(const juce::Colour& newColor) = 0;
    };

    ColorValuesInput();
    void paint(juce::Graphics& g) override;
    void resized() override;
    void sliderValueChanged(juce::Slider* slider) override;
    void setColor(const juce::Colour& color);
    void addListener(Listener* newListener);
    void removeListener(Listener* listener);
    void onElementSelected(const juce::String& name, const juce::Colour& color) override;
    void setSliderTrackColor(const juce::Colour& color);
    void setSliderBackgroundColor(const juce::Colour& color);
    void setSliderThumbColor(const juce::Colour& color);
    void setLabelTextColor(const juce::Colour& color);
    void setTextBoxTextColor(const juce::Colour& color);
    void updateHueSlider(float hueValue);
    void updateSaturationSlider(float saturationValue);
    void updateBrightnessSlider(float brightnessValue);
    
juce::Image createThumbImage(int thumbWidth, int thumbHeight, juce::Slider& slider);

    juce::Image createTrackImage(int x, int y, int width, int height, float sliderPos, float minSliderPos, float maxSliderPos, const juce::Slider::SliderStyle style, juce::Slider& slider);

    std::function<void(juce::Colour)> onColorChanged;

private:
    class CustomLookAndFeel : public juce::LookAndFeel_V4 {
    public:


        void drawLinearSlider(juce::Graphics& g, int x, int y, int width, int height,
            float sliderPos, float minSliderPos, float maxSliderPos,
            const juce::Slider::SliderStyle style, juce::Slider& slider) override {
            LookAndFeel_V4::drawLinearSlider(g, x, y, width, height, sliderPos, minSliderPos, maxSliderPos, style, slider);

    

            // Calculate the track's width expansion
            float trackWidthExpansion = 8; // Adjust the expansion value as needed

            // Calculate the track's left and right positions with the width expansion
            float trackLeft = x - 4 - trackWidthExpansion / 2;
            float trackRight = x + width + 8 + trackWidthExpansion / 2;

            // Calculate the track's top and bottom positions
            float trackTop = y + height / 2 - 4; // Adjust as needed
            float trackBottom = y + height / 2 + 4; // Adjust as needed

            // Create a path for the rounded rectangle track shape
            juce::Path trackPerimeter;
            float cornerSize = 3.0f; // Adjust the corner size as needed
            trackPerimeter.addRoundedRectangle(juce::Rectangle<float>(trackLeft, trackTop, trackRight - trackLeft, trackBottom - trackTop), cornerSize);
          
            // Optionally, set the color for the track perimeter
            g.setColour(slider.findColour(juce::Slider::trackColourId));// Set this to your desired fill color
 
            // Draw the track perimeter
            g.fillPath(trackPerimeter); // Fill the rounded rectangle track shape

            innerShadow2.render(g, trackPerimeter);
            
            // Fill the portion of the track up to the slider position
            juce::Path filledTrack;
            float filledTrackRight = juce::jlimit(trackLeft, trackRight, trackLeft + (sliderPos - x));
            filledTrack.addRoundedRectangle(juce::Rectangle<float>(trackLeft, trackTop, filledTrackRight - trackLeft +5, trackBottom - trackTop), cornerSize);

            glowShadow.render(g, filledTrack);
       
            g.setColour(slider.findColour(juce::Slider::backgroundColourId)); // Adjust to match the track color
            g.fillPath(filledTrack); // Fill the portion of the track

            innerShadow3.render(g, trackPerimeter);

            // Custom thumb path
            juce::Path customThumbPath;
            const int customThumbWidth = 14;
            const int customThumbHeight = 14;
            juce::Rectangle<float> thumbRect(sliderPos - customThumbWidth / 2.0f, y + (height - customThumbHeight) / 2.0f, customThumbWidth, customThumbHeight);
            customThumbPath.addEllipse(thumbRect);
            
            

            // Render the drop shadow
            shadow.render(g, customThumbPath);

            // Optionally draw the custom thumb
            g.setColour(slider.findColour(juce::Slider::thumbColourId));
            g.fillPath(customThumbPath);


            innerShadow.render(g, customThumbPath);
        }

        void updateGlowShadowColor(const juce::Colour& newColor) {
            // Soft local bloom — keep it close to the filled track.
            glowShadow = {
                { newColor.withMultipliedAlpha (0.40f), 4, { 0, 0 }, 0 },
                { newColor.withMultipliedAlpha (0.65f), 2, { 0, 0 }, 0 }
            };
        }
  



        void drawLabel(juce::Graphics& g, juce::Label& label) override {
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
        }

    private:
        melatonin::DropShadow shadow = { { juce::Colours::black, 10, { 0, 2 } } };
        melatonin::DropShadow glowShadow = {
            { juce::Colours::goldenrod.withAlpha (0.40f), 4, { 0, 0 }, 0 },
            { juce::Colours::goldenrod.withAlpha (0.65f), 2, { 0, 0 }, 0 }
        };
        melatonin::InnerShadow innerShadow = { { juce::Colours::black, 3, { 0, - 1 } } }; 
        melatonin::InnerShadow innerShadow2 = { { juce::Colours::black, 4, { 0, 2 } } };
        melatonin::InnerShadow innerShadow3 = { { juce::Colours::black.withAlpha(0.6f), 4, {0, 2}}};

        juce::Image trackImage;
        juce::Image thumbImage;
        bool trackImageIsValid = false;
        bool thumbImageIsValid = false;


    };

    juce::Colour labelTextColor;
    juce::Colour textBoxTextColor;

    void createSliderAndLabel(const juce::String& shortLabel, const juce::String& fullLabel, float minValue, float maxValue);

    juce::OwnedArray<juce::Slider> colorSliders;
    juce::OwnedArray<juce::Label> colorLabels;
    juce::OwnedArray<juce::Label> textBoxLabels;
    CustomLookAndFeel customLookAndFeel;

    std::function<void(const juce::Colour&)> colorChangedCallback;
    juce::ListenerList<Listener> listeners;



    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ColorValuesInput)
};
