#pragma once

#include <JuceHeader.h>
#include <JucePluginDefines.h>

/** GHOVLZ! EQ wordmark - Bahnschrift brand type, fixed (not user-selectable). */
class BrandWordmark : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    /** Hero product name painted at full brand weight. */
    static juce::String getBrandText()
    {
        return "GHOVLZ! DYN";
    }

    /** Secondary product framing beside the hero brand (plain ASCII). */
    static juce::String getSideCheckTagText()
    {
        return "Multiband Dynamics";
    }

    /** Same as getSideCheckTagText() - kept for call sites that used the ASCII path. */
    static juce::String getSideCheckTagTextAscii()
    {
        return getSideCheckTagText();
    }

    /** UI label synced to JucePlugin_VersionString with a beta suffix. */
    static juce::String getBetaVersionString()
    {
        return juce::String ("v") + JucePlugin_VersionString + "-beta";
    }

    static juce::String getTypefaceName()
    {
        return "Bahnschrift";
    }

    BrandWordmark()
    {
        setInterceptsMouseClicks (false, false);
        setTooltip (getBrandText() + " " + getSideCheckTagTextAscii());
    }

    void setCompactLook (bool shouldBeCompact)
    {
        if (compactLook == shouldBeCompact)
            return;

        compactLook = shouldBeCompact;
        repaint();
    }

    void setBrandColour (juce::Colour colour) noexcept
    {
        brandColour = colour;
        repaint();
    }

    juce::Font makeFont (float height) const
    {
        return juce::Font (juce::FontOptions().withName (getTypefaceName()).withHeight (height));
    }

    static float measureTextWidth (const juce::Font& font, const juce::String& text)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, text, 0.0f, 0.0f);
        return ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth();
    }

    /** Plain-ASCII brand tag (no special trademark glyphs). */
    static juce::String resolveSideCheckTagText (const juce::Font& /*font*/)
    {
        return getSideCheckTagText();
    }

    void paint (juce::Graphics& g) override
    {
        const float h = (float) getHeight();
        const float brandH = juce::jmax (10.0f, h * (compactLook ? 0.62f : 0.70f));
        // SideCheck stays at full 2x tag size; beta is 75% of that.
        const float tagH = juce::jmax (8.0f, brandH * 1.00f);
        const float versionH = juce::jmax (8.0f, tagH * 0.75f);

        const auto brandFont = makeFont (brandH);
        const auto tagFont = juce::Font (juce::FontOptions()
                                             .withName ("Segoe UI")
                                             .withHeight (tagH)
                                             .withStyle ("bold"));
        const auto versionFont = juce::Font (juce::FontOptions()
                                                 .withName ("Segoe UI")
                                                 .withHeight (versionH));

        const juce::String brandText (getBrandText());
        const juce::String sideCheckText (resolveSideCheckTagText (tagFont));
        const juce::String versionText (getBetaVersionString());

        const float brandW = measureTextWidth (brandFont, brandText);
        const float gapBrandToTag = juce::jmax (5.0f, brandH * 0.28f);
        const float gapTagToVersion = juce::jmax (4.0f, brandH * 0.22f);
        const float sideCheckW = measureTextWidth (tagFont, sideCheckText);
        const float versionW = measureTextWidth (versionFont, versionText);
        const float totalW = brandW + gapBrandToTag + sideCheckW + gapTagToVersion + versionW;
        const float startX = juce::jmax (0.0f, ((float) getWidth() - totalW) * 0.5f);

        const float brandAlpha = compactLook ? 0.55f : 0.92f;
        // Beta keeps the previous subdued weight; SideCheck tracks brand colour.
        const float betaAlpha = compactLook ? 0.30f : 0.45f;
        const float sideCheckAlpha = brandAlpha;

        float x = startX;

        g.setFont (brandFont);
        g.setColour (brandColour.withAlpha (brandAlpha));
        g.drawText (brandText,
                    juce::Rectangle<float> (x, 0.0f, brandW + 1.0f, h),
                    juce::Justification::centredLeft,
                    false);
        x += brandW + gapBrandToTag;

        g.setFont (tagFont);
        g.setColour (brandColour.withAlpha (sideCheckAlpha));
        g.drawText (sideCheckText,
                    juce::Rectangle<float> (x, 0.0f, sideCheckW + 2.0f, h),
                    juce::Justification::centredLeft,
                    false);
        x += sideCheckW + gapTagToVersion;

        g.setFont (versionFont);
        g.setColour (brandColour.withAlpha (betaAlpha));
        g.drawText (versionText,
                    juce::Rectangle<float> (x, 0.0f, versionW + 2.0f, h),
                    juce::Justification::centredLeft,
                    false);
    }

private:
    bool compactLook = false;
    juce::Colour brandColour { juce::Colours::whitesmoke };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (BrandWordmark)
};
