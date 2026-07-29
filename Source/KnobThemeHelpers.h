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

    inline juce::Colour arcBright (const SharedColors& c, bool bandHighlight) noexcept
    {
        return KnobBandHighlight::intensify (c.knobArc, bandHighlight);
    }

    inline juce::Colour arcDark (const SharedColors& c, bool bandHighlight) noexcept
    {
        return KnobBandHighlight::intensify (c.knobArc.darker (0.55f), bandHighlight);
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
}
