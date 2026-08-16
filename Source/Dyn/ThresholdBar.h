#pragma once

#include <JuceHeader.h>
#include "../Menu/SharedResources.h"

/** Vertical threshold fader: bar on the right, arrow handle on the left. 0 dB at top. */
class ThresholdBar : public juce::Slider
{
public:
    ThresholdBar()
    {
        setSliderStyle (juce::Slider::LinearVertical);
        setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        setRange (-60.0, 0.0, 0.1);
        setValue (-18.0, juce::dontSendNotification);
        setDoubleClickReturnValue (true, -18.0);
        setSliderSnapsToMousePosition (true);
        setMouseDragSensitivity (200);
        setOpaque (false);
        setLookAndFeel (&barLaf);
    }

    ~ThresholdBar() override { setLookAndFeel (nullptr); }

    void setThemeColors (SharedResources* r) noexcept { theme = r; }

    static int getReadoutHeight() noexcept { return 14; }
    static int getBestWidth() noexcept { return 52; }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        if (r.getWidth() < 8.0f || r.getHeight() < 16.0f)
            return;

        auto head = r.removeFromTop ((float) getReadoutHeight());
        const float v = (float) getValue();
        g.setFont (SharedResources::uiFont (10.0f));
        g.setColour (juce::Colour::fromRGB (218, 165, 32));
        g.drawText (juce::String (v, 1) + " dB", head.toNearestInt(), juce::Justification::centred, false);

        const float arrowW = 11.0f;
        const float gap = 2.0f;
        auto bar = r.withTrimmedLeft (arrowW + gap).reduced (0.5f, 1.5f);
        if (bar.getWidth() < 4.0f || bar.getHeight() < 8.0f)
            return;

        g.setColour (juce::Colour::fromRGB (12, 11, 9));
        g.fillRoundedRectangle (bar, 2.0f);
        g.setColour (juce::Colours::black);
        g.drawRoundedRectangle (bar, 2.0f, 1.0f);

        const float t01 = (float) valueToProportionOfLength (getValue());
        const float y = bar.getBottom() - t01 * bar.getHeight();
        const float yClamped = juce::jlimit (bar.getY(), bar.getBottom(), y);

        auto above = juce::Rectangle<float> (bar.getX() + 1.5f, bar.getY() + 1.0f,
                                             bar.getWidth() - 3.0f,
                                             juce::jmax (0.0f, yClamped - bar.getY() - 1.0f));
        auto below = juce::Rectangle<float> (bar.getX() + 1.5f, yClamped,
                                             bar.getWidth() - 3.0f,
                                             juce::jmax (0.0f, bar.getBottom() - yClamped - 1.0f));

        juce::Colour downC = juce::Colour::fromRGB (90, 138, 48);
        juce::Colour upC   = juce::Colour::fromRGB (196, 64, 40);
        if (theme != nullptr)
        {
            downC = theme->sharedColors.graphBand1.interpolatedWith (juce::Colour::fromRGB (90, 138, 48), 0.35f);
            upC   = theme->sharedColors.graphBand6.interpolatedWith (juce::Colour::fromRGB (196, 64, 40), 0.25f);
        }

        if (above.getHeight() > 0.5f)
        {
            g.setColour (downC.withAlpha (0.52f));
            g.fillRect (above);
        }
        if (below.getHeight() > 0.5f)
        {
            g.setColour (upC.withAlpha (0.42f));
            g.fillRect (below);
        }

        g.setColour (juce::Colours::white.withAlpha (0.88f));
        g.fillRect (bar.getX() + 0.5f, yClamped - 0.75f, bar.getWidth() - 1.0f, 1.5f);

        juce::Path arrow;
        const float ah = 9.0f;
        const float ax = r.getX();
        arrow.addTriangle (ax, yClamped - ah * 0.5f,
                           ax, yClamped + ah * 0.5f,
                           ax + arrowW, yClamped);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillPath (arrow);
        juce::Path inner;
        inner.addTriangle (ax + 1.0f, yClamped - ah * 0.38f,
                           ax + 1.0f, yClamped + ah * 0.38f,
                           ax + arrowW - 1.2f, yClamped);
        g.setColour (juce::Colours::white.withAlpha (0.94f));
        g.fillPath (inner);
    }

private:
    struct BarLaf : public juce::LookAndFeel_V4
    {
        juce::Slider::SliderLayout getSliderLayout (juce::Slider& s) override
        {
            juce::Slider::SliderLayout layout;
            layout.sliderBounds = s.getLocalBounds().withTrimmedTop (ThresholdBar::getReadoutHeight());
            layout.textBoxBounds = {};
            return layout;
        }

        void drawLinearSlider (juce::Graphics&, int, int, int, int, float, float, float,
                               juce::Slider::SliderStyle, juce::Slider&) override {}
    };

    BarLaf barLaf;
    SharedResources* theme = nullptr;
};
