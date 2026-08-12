#pragma once

#include "Menu/SharedResources.h"
#include "KnobBandHighlight.h"

namespace KnobTheme
{
    inline const SharedColors& colors (SharedResources* themeColors) noexcept
    {
        if (themeColors != nullptr)
            return themeColors->sharedColors;

        // Prefer the live plugin theme over a frozen static default — otherwise
        // knobs keep factory orange while chrome/randomize update sharedResources.
        if (auto* active = SharedResources::getActive())
            return active->sharedColors;

        static const SharedColors defaultColors;
        return defaultColors;
    }

    /** Optional per-knob arc colour (ARGB int on Slider properties). Empty = theme knobArc. */
    inline constexpr const char* arcColourPropertyID = "arcColourArgb";

    inline juce::Colour resolveArcBase (const SharedColors& c, const juce::Slider* slider) noexcept
    {
        if (slider != nullptr)
        {
            const auto& props = slider->getProperties();
            if (props.contains (arcColourPropertyID))
                return juce::Colour ((juce::uint32) (int) props[arcColourPropertyID]);
        }
        return c.knobArc;
    }

    inline void setArcColour (juce::Slider& slider, juce::Colour colour) noexcept
    {
        slider.getProperties().set (arcColourPropertyID, (int) colour.getARGB());
        slider.repaint();
    }

    inline void clearArcColour (juce::Slider& slider) noexcept
    {
        slider.getProperties().remove (arcColourPropertyID);
        slider.repaint();
    }

    /**
        Active (band on) chrome ink — shared by power rings and knob glow arcs.
        Preserves H/S/V of the base (no sat/bright boost) so "match handle colours"
        stays identical to graph handles. Only forces full alpha.
    */
    inline juce::Colour chromeActive (juce::Colour base) noexcept
    {
        return base.withAlpha (1.0f);
    }

    /** Inactive (band off) chrome ink — dimmed active ink. */
    inline juce::Colour chromeInactive (juce::Colour base) noexcept
    {
        return chromeActive (base).darker (0.55f);
    }

    /**
        Same solid colour the graph draws for a band handle disk:
        full alpha + optional legibleHandleFill (does not raise sat for dice limits).
    */
    inline juce::Colour chromeFromHandleFill (const SharedColors& c,
                                              juce::Colour handleFill,
                                              juce::Colour graphBackground) noexcept
    {
        return c.legibleHandleFill (handleFill.withAlpha (1.0f), graphBackground);
    }

    inline juce::Colour arcBright (const SharedColors& c, bool bandHighlight,
                                   const juce::Slider* slider = nullptr) noexcept
    {
        // Exact base colour (handle match or Knob Arc) — only hover/selection intensifies.
        const auto base = chromeActive (resolveArcBase (c, slider));
        return KnobBandHighlight::intensify (base, bandHighlight);
    }

    inline juce::Colour arcDark (const SharedColors& c, bool bandHighlight,
                                 const juce::Slider* slider = nullptr) noexcept
    {
        // Mild gradient tail only; bright end of the arc stays the true band/theme colour.
        const auto base = chromeActive (resolveArcBase (c, slider)).darker (0.18f);
        return KnobBandHighlight::intensify (base, bandHighlight);
    }

    /** Multiply filter: never full black; white ≈ identity. */
    inline juce::Colour clampMultiply (juce::Colour c) noexcept
    {
        auto chan = [] (float v) { return juce::jlimit (0.50f, 1.0f, v); };
        return juce::Colour::fromFloatRGBA (chan (c.getFloatRed()),
                                            chan (c.getFloatGreen()),
                                            chan (c.getFloatBlue()),
                                            1.0f);
    }

    /** Tint wash: limited alpha + mid brightness so it can't blow out or crush. */
    inline juce::Colour clampTint (juce::Colour c) noexcept
    {
        const float h = c.getHue();
        const float s = juce::jlimit (0.0f, 0.85f, c.getSaturation());
        const float b = juce::jlimit (0.30f, 0.85f, c.getBrightness());
        const float a = juce::jlimit (0.0f, 0.40f, c.getFloatAlpha());
        return juce::Colour::fromHSV (h, s, b, a);
    }

    inline void applyMultiplyAndTintInPlace (juce::Image& frame,
                                             juce::Colour multiplyIn,
                                             juce::Colour tintIn) noexcept
    {
        if (! frame.isValid())
            return;

        const auto multiply = clampMultiply (multiplyIn);
        const auto tint = clampTint (tintIn);

        const float mr = multiply.getFloatRed();
        const float mg = multiply.getFloatGreen();
        const float mb = multiply.getFloatBlue();
        const bool identityMultiply = mr > 0.98f && mg > 0.98f && mb > 0.98f;
        const float tintAmt = tint.getFloatAlpha();
        if (identityMultiply && tintAmt < 0.01f)
            return;

        const float tr = tint.getFloatRed();
        const float tg = tint.getFloatGreen();
        const float tb = tint.getFloatBlue();

        juce::Image::BitmapData bd (frame, juce::Image::BitmapData::readWrite);
        for (int y = 0; y < bd.height; ++y)
        {
            for (int x = 0; x < bd.width; ++x)
            {
                const auto src = bd.getPixelColour (x, y);
                const float a = src.getFloatAlpha();
                if (a <= 0.001f)
                    continue;

                float r = src.getFloatRed() * mr;
                float gch = src.getFloatGreen() * mg;
                float b = src.getFloatBlue() * mb;

                if (tintAmt > 0.001f)
                {
                    r = r * (1.0f - tintAmt) + tr * tintAmt;
                    gch = gch * (1.0f - tintAmt) + tg * tintAmt;
                    b = b * (1.0f - tintAmt) + tb * tintAmt;
                }

                bd.setPixelColour (x, y, juce::Colour::fromFloatRGBA (r, gch, b, a));
            }
        }
    }

    /** Draw stitched-knob artwork with theme multiply + tint. */
    inline void drawArtwork (juce::Graphics& g,
                             const juce::Image& knobImage,
                             int destX, int destY, int destW, int destH,
                             int srcX, int srcY, int srcW, int srcH,
                             const SharedColors& theme)
    {
        if (! knobImage.isValid() || destW <= 0 || destH <= 0 || srcW <= 0 || srcH <= 0)
            return;

        const auto multiply = clampMultiply (theme.knobMultiply);
        const auto tint = clampTint (theme.knobTint);
        const bool identityMultiply = multiply.getFloatRed() > 0.98f
                                      && multiply.getFloatGreen() > 0.98f
                                      && multiply.getFloatBlue() > 0.98f;

        if (identityMultiply && tint.getFloatAlpha() < 0.01f)
        {
            g.drawImage (knobImage, destX, destY, destW, destH, srcX, srcY, srcW, srcH, false);
            return;
        }

        auto frame = knobImage.getClippedImage ({ srcX, srcY, srcW, srcH })
                         .convertedToFormat (juce::Image::ARGB)
                         .createCopy();
        applyMultiplyAndTintInPlace (frame, multiply, tint);
        g.drawImage (frame, destX, destY, destW, destH, 0, 0, srcW, srcH, false);
    }

    inline void applyValuePopupColours (juce::Slider& slider, bool show, const SharedColors& c)
    {
        const auto textColour = show ? c.knobPopupText : juce::Colours::transparentBlack;
        const auto bgColour = show ? c.knobPopupBackground : juce::Colours::transparentBlack;
        const auto outlineColour = show ? c.knobPopupText.withAlpha (0.25f) : juce::Colours::transparentBlack;

        slider.setColour (juce::Slider::textBoxTextColourId, textColour);
        slider.setColour (juce::Slider::textBoxBackgroundColourId, bgColour);
        slider.setColour (juce::Slider::textBoxOutlineColourId, outlineColour);
    }

    /**
        Size the slider value text box to the full plain numeric string (never "...").
        Call whenever the value popup is shown or the value changes while shown.
    */
    inline void showValueTextBox (juce::Slider& slider, bool show, SharedResources* themeColors) noexcept
    {
        const auto& c = colors (themeColors);
        const auto text = slider.getTextFromValue (slider.getValue());
        // Match typical Slider text-box face; measure full string + padding.
        const juce::Font font (juce::FontOptions (12.5f));
        const float tw = juce::GlyphArrangement::getStringWidth (font, text);
        // Wide enough for e.g. "20000 Hz", "-24.0 dB", "1000.0 ms" — never clamp to tiny knob width.
        const int boxW = juce::jlimit (44, 100, (int) std::ceil (tw) + 14);
        const int boxH = 18;
        slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, boxW, boxH);
        applyValuePopupColours (slider, show, c);

        for (int i = 0; i < slider.getNumChildComponents(); ++i)
        {
            if (auto* lab = dynamic_cast<juce::Label*> (slider.getChildComponent (i)))
            {
                lab->setMinimumHorizontalScale (1.0f);
                lab->setJustificationType (juce::Justification::centred);
            }
        }
    }
}
