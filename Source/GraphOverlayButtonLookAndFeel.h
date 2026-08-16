#pragma once

#include <JuceHeader.h>
#include "MelatoninBlur/melatonin/shadows.h"
#include "Menu/SharedResources.h"

/**
    Single chrome paint path for every plugin button.

    Drop shadow is always drawn when the global glow/shadow master is on.
    Hover outline-glow is separate (Appearance button-glow / hover-only).

    New chrome must call paintChromeFace / paintChromeButton / drawButtonBackground
    on this LAF. Do not add one-off shadow lists for individual buttons.
*/
class GraphOverlayButtonLookAndFeel : public juce::LookAndFeel_V4
{
public:
    /** Corner radius from Appearance (SharedColors::buttonCornerRadius), clamped. */
    static float cornerRadius() noexcept
    {
        float r = 6.0f;
        if (auto* active = SharedResources::getActive())
            r = active->sharedColors.buttonCornerRadius;
        return juce::jlimit (2.0f, 16.0f, r);
    }

    /** Melatonin outline-blur radius (px); scales with corner radius. */
    static int outlineBlurRadius() noexcept
    {
        return juce::jlimit (1, 14, juce::roundToInt (cornerRadius() * 0.9f) + 1);
    }

    /**
        True when Melatonin button-edge glow should paint.
        Requires global glow master + Appearance button glow, and hover if
        "Glow on hover only" is enabled.
    */
    static bool shouldShowButtonGlow (bool highlightedOrDown) noexcept
    {
        if (! SharedResources::glowShadowEffectsEnabled())
            return false;

        bool glowOn = true;
        bool hoverOnly = true;
        if (auto* active = SharedResources::getActive())
        {
            glowOn = active->sharedColors.buttonGlowEnabled;
            hoverOnly = active->sharedColors.buttonGlowOnlyOnHover;
        }

        if (! glowOn)
            return false;
        if (hoverOnly && ! highlightedOrDown)
            return false;
        return true;
    }

    /** WCAG-ish relative luminance of a colour (0 = black, 1 = white). */
    static float relativeLuminance (juce::Colour c) noexcept
    {
        auto lin = [] (float channel) noexcept
        {
            channel = juce::jlimit (0.0f, 1.0f, channel);
            return channel <= 0.04045f ? channel / 12.92f
                                       : std::pow ((channel + 0.055f) / 1.055f, 2.4f);
        };
        return 0.2126f * lin (c.getFloatRed())
             + 0.7152f * lin (c.getFloatGreen())
             + 0.0722f * lin (c.getFloatBlue());
    }

    /**
        Hover/down adjust: dark faces go lighter, light faces go darker.
        Same idea as legible-text direction (push away from mid-grey for contrast).
    */
    static juce::Colour adjustForInteraction (juce::Colour base,
                                              bool highlighted,
                                              bool down) noexcept
    {
        if (! highlighted && ! down)
            return base;

        const float y = relativeLuminance (base);
        const bool darkBase = y < 0.48f;
        const float amt = down ? 0.22f : 0.12f;

        if (darkBase)
            return base.brighter (amt);
        return base.darker (amt);
    }

    /** Soft vertical 3D form from a solid theme colour. */
    static void fillRoundedGradient (juce::Graphics& g,
                                     juce::Rectangle<float> bounds,
                                     juce::Colour fill,
                                     float corner)
    {
        const float y = relativeLuminance (fill);
        // On light fills, "top brighter" would wash out — flip gradient direction.
        const bool lightFace = y >= 0.48f;
        auto top = lightFace ? fill.darker (0.06f) : fill.brighter (0.10f);
        float h = 0.0f, s = 0.0f, v = 0.0f;
        fill.getHSB (h, s, v);
        auto bottom = lightFace
                          ? fill.brighter (0.04f)
                          : juce::Colour::fromHSV (h,
                                                   juce::jlimit (0.0f, 1.0f, s * 0.88f),
                                                   juce::jlimit (0.0f, 1.0f, v * 0.82f),
                                                   fill.getFloatAlpha());
        juce::ColourGradient grad (top, bounds.getX(), bounds.getY(),
                                   bottom, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (bounds, corner);
    }

    /** Drop lift is independent of hover-glow. Master switch only. */
    static bool shouldShowDropShadow() noexcept
    {
        return SharedResources::glowShadowEffectsEnabled();
    }

    /**
        Soft drop under every chrome face. Call from paintChromeFace so new
        buttons inherit this automatically (do not add per-control shadow lists).
    */
    static void renderDropShadow (juce::Graphics& g,
                                  juce::Rectangle<float> bounds,
                                  float corner)
    {
        if (! shouldShowDropShadow())
            return;

        juce::Path p;
        p.addRoundedRectangle (bounds, corner);
        const int blur = juce::jmax (3, outlineBlurRadius());
        dropLift().setRadius ((double) blur);
        dropLift().setColor (juce::Colours::black.withAlpha (0.48f));
        dropLift().setOffset (0, 2);
        dropLift().render (g, p);
    }

    /**
        Soft melatonin outline against the background: blur the rounded path first,
        then paint the face so the interior of the blur is covered (edge-clipped).
        Glow only — drop lives in renderDropShadow so idle chrome still lifts.
    */
    static void renderOutlineBlur (juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   float corner,
                                   juce::Colour outlineHint)
    {
        juce::Path p;
        p.addRoundedRectangle (bounds, corner);

        const int blur = outlineBlurRadius();
        outlineGlow().setRadius ((double) blur);
        outlineGlow().setColor (outlineHint.withAlpha (0.55f));
        outlineGlow().setOffset (0, 0);
        outlineGlow().render (g, p);
    }

    /**
        @param highlightedOrDown  drives glow-on-hover; drop shadow is always on
        when the global glow/shadow master is enabled.
    */
    static void paintChromeFace (juce::Graphics& g,
                                 juce::Rectangle<float> bounds,
                                 juce::Colour fill,
                                 float corner,
                                 bool highlightedOrDown = false)
    {
        const float y = relativeLuminance (fill);
        renderDropShadow (g, bounds, corner);
        if (shouldShowButtonGlow (highlightedOrDown))
        {
            const auto hint = y < 0.48f ? fill.brighter (0.35f) : fill.darker (0.15f);
            renderOutlineBlur (g, bounds, corner, hint);
        }
        fillRoundedGradient (g, bounds, fill, corner);
        g.setColour (juce::Colours::black.withAlpha (y < 0.48f ? 0.38f : 0.22f));
        g.drawRoundedRectangle (bounds, corner, 1.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override
    {
        juce::ignoreUnused (backgroundColour);
        auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const float corner = cornerRadius();

        auto fill = button.getToggleState()
                        ? button.findColour (juce::TextButton::buttonOnColourId)
                        : button.findColour (juce::TextButton::buttonColourId);
        fill = adjustForInteraction (fill, shouldDrawButtonAsHighlighted, shouldDrawButtonAsDown);
        paintChromeFace (g, bounds, fill, corner,
                         shouldDrawButtonAsHighlighted || shouldDrawButtonAsDown);
    }

    static constexpr const char* kCaptionFontDeltaProp = "overlayFontDelta";
    static constexpr const char* kCaptionBoldProp = "overlayFontBold";

    static void setCaptionFontDelta (juce::Component& c, int extraPx) noexcept
    {
        c.getProperties().set (kCaptionFontDeltaProp, extraPx);
        c.repaint();
    }

    static int captionFontDelta (const juce::Component& c) noexcept
    {
        return (int) c.getProperties().getWithDefault (kCaptionFontDeltaProp, 0);
    }

    static void setCaptionBold (juce::Component& c, bool bold) noexcept
    {
        c.getProperties().set (kCaptionBoldProp, bold);
        c.repaint();
    }

    static bool captionBold (const juce::Component& c) noexcept
    {
        return (bool) c.getProperties().getWithDefault (kCaptionBoldProp, false);
    }

    juce::Font getTextButtonFont (juce::TextButton& button, int buttonHeight) override
    {
        const float h = juce::jmin (15.0f, (float) buttonHeight * 0.55f)
                        + (float) captionFontDelta (button);
        return SharedResources::uiFont (juce::jmax (9.0f, h), captionBold (button));
    }

    juce::Font getLabelFont (juce::Label& label) override
    {
        const float h = label.getFont().getHeight() > 1.0f ? label.getFont().getHeight() : 12.0f;
        return SharedResources::uiFont (h);
    }

    juce::Font getPopupMenuFont() override
    {
        return SharedResources::uiFont (13.0f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button,
                         bool, bool) override
    {
        auto fill = button.getToggleState()
                        ? button.findColour (juce::TextButton::buttonOnColourId)
                        : button.findColour (juce::TextButton::buttonColourId);
        auto ink = button.getToggleState()
                       ? button.findColour (juce::TextButton::textColourOnId)
                       : button.findColour (juce::TextButton::textColourOffId);
        if (auto* active = SharedResources::getActive())
            ink = active->sharedColors.legibleTextOn (ink, fill);
        g.setColour (ink.withAlpha (button.isEnabled() ? 0.95f : 0.40f));

        const int h = button.getHeight();
        const float fontH = juce::jlimit (9.0f, 22.0f,
                                          (float) h * 0.48f + (float) captionFontDelta (button));
        g.setFont (SharedResources::uiFont (fontH, captionBold (button)));
        // Never ellipsize chrome captions.
        g.drawText (button.getButtonText(),
                    button.getLocalBounds().reduced (2, 0),
                    juce::Justification::centred, false);
    }

    static void paintChevron (juce::Graphics& g, juce::Rectangle<float> r,
                              juce::Colour ink, bool pointDown)
    {
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float arm = s * 0.26f;
        const float thick = juce::jmax (1.6f, s * 0.11f);
        const float dir = pointDown ? 1.0f : -1.0f;
        juce::Path chevron;
        chevron.startNewSubPath (c.x - arm, c.y - dir * arm * 0.45f);
        chevron.lineTo (c.x, c.y + dir * arm * 0.5f);
        chevron.lineTo (c.x + arm, c.y - dir * arm * 0.45f);
        g.setColour (ink);
        g.strokePath (chevron, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));
    }

    static void paintUndoRedoArrow (juce::Graphics& g, juce::Rectangle<float> r,
                                    juce::Colour ink, bool redo)
    {
        const float s = juce::jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float rad = s * 0.28f;
        const float thick = juce::jmax (1.5f, s * 0.10f);
        const float dir = redo ? 1.0f : -1.0f;

        juce::Path arc;
        const float from = redo ? juce::MathConstants<float>::pi * 0.15f
                                : juce::MathConstants<float>::pi * 0.85f;
        const float to   = redo ? juce::MathConstants<float>::pi * 1.35f
                                : juce::MathConstants<float>::pi * 1.85f;
        arc.addCentredArc (c.x, c.y + s * 0.04f, rad, rad, 0.0f, from, to, true);
        g.setColour (ink);
        g.strokePath (arc, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));

        const float tipX = c.x + dir * rad * 0.15f;
        const float tipY = c.y - rad * 0.72f;
        juce::Path head;
        head.startNewSubPath (tipX - dir * rad * 0.42f, tipY);
        head.lineTo (tipX, tipY - rad * 0.38f);
        head.lineTo (tipX + dir * rad * 0.12f, tipY + rad * 0.18f);
        g.strokePath (head, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                  juce::PathStrokeType::rounded));
    }

    /** Shared shadow for non-button chrome (meters, knobs). Same lift as buttons. */
    static void renderRoundedDrop (juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   float corner = -1.0f,
                                   bool highlightedOrDown = false)
    {
        juce::ignoreUnused (highlightedOrDown);
        if (corner < 0.0f)
            corner = cornerRadius();
        renderDropShadow (g, bounds, corner);
    }

    /** Full chrome paint helper for glyph buttons (dice, zoom, bank arrows). */
    static void paintChromeButton (juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   juce::Colour fill,
                                   float corner = -1.0f)
    {
        if (corner < 0.0f)
            corner = cornerRadius();
        // No hover info — glow only if "always" (not hover-only).
        paintChromeFace (g, bounds, fill, corner, false);
    }

    /** Chrome + hover/down in one call (preferred for custom paintButton overrides). */
    static void paintChromeButton (juce::Graphics& g,
                                   juce::Rectangle<float> bounds,
                                   juce::Colour fill,
                                   bool highlighted,
                                   bool down,
                                   float corner = -1.0f)
    {
        if (corner < 0.0f)
            corner = cornerRadius();
        paintChromeFace (g, bounds,
                         adjustForInteraction (fill, highlighted, down),
                         corner,
                         highlighted || down);
    }

private:
    static melatonin::DropShadow& outlineGlow()
    {
        static melatonin::DropShadow s {
            { juce::Colours::white.withAlpha (0.35f), 4, { 0, 0 }, 0 }
        };
        return s;
    }

    static melatonin::DropShadow& dropLift()
    {
        static melatonin::DropShadow s {
            { juce::Colours::black.withAlpha (0.42f), 5, { 0, 2 }, 0 }
        };
        return s;
    }
};
