#pragma once

#include <JuceHeader.h>
#include "Menu/SharedResources.h"
#include "ScopeModules.h"

/**
    Shared visual chrome for docked Scope-mode module panes (strip + tiled).
    Cards / headers / content insets — no density setting; one polished look.
*/
namespace ScopePaneChrome
{
    struct Metrics
    {
        static constexpr int kHeaderH = 18;
        static constexpr int kContentInset = 3;
        static constexpr float kRadiusStrip = 5.0f;
        static constexpr float kRadiusTiled = 7.0f;
        static constexpr int kTileGapDesign = 4; // design px; caller scales
        static constexpr int kAccentRailW = 3;
    };

    inline float radiusFor (bool stripMode) noexcept
    {
        return stripMode ? Metrics::kRadiusStrip : Metrics::kRadiusTiled;
    }

    /** Full pane → content rect for the module (below header, inset). */
    inline juce::Rectangle<int> contentBounds (juce::Rectangle<int> pane) noexcept
    {
        auto r = pane;
        r.removeFromTop (Metrics::kHeaderH);
        return r.reduced (Metrics::kContentInset);
    }

    /** Header band within a full slot rect. */
    inline juce::Rectangle<float> headerBand (juce::Rectangle<float> pane) noexcept
    {
        return pane.removeFromTop ((float) Metrics::kHeaderH);
    }

    /** Pick a title that fits without ellipsis (full label, else short key). */
    inline juce::String titleThatFits (ScopeModuleId id, float maxWidth, const juce::Font& font)
    {
        const auto full = ScopeModules::idToLabel (id);
        if (juce::GlyphArrangement::getStringWidth (font, full) <= maxWidth)
            return full;

        // Shorter friendly names for tight panes.
        juce::String shortName;
        switch (id)
        {
            case ScopeModuleId::levelIn:       shortName = "Level In"; break;
            case ScopeModuleId::levelOut:      shortName = "Level Out"; break;
            case ScopeModuleId::spectrogram:   shortName = "Spectro"; break;
            case ScopeModuleId::spectrum:      shortName = "Analyzer"; break;
            case ScopeModuleId::goniometer:    shortName = "Gonio"; break;
            case ScopeModuleId::oscilloscope:  shortName = "Osc"; break;
            case ScopeModuleId::stereogram:    shortName = "Stereo"; break;
            case ScopeModuleId::loudness:      shortName = "Loudness"; break;
            case ScopeModuleId::histogram:     shortName = "Histo"; break;
            case ScopeModuleId::spectrogram3D: shortName = "Spec 3D"; break;
            default:                           shortName = ScopeModules::idToKey (id); break;
        }
        if (juce::GlyphArrangement::getStringWidth (font, shortName) <= maxWidth)
            return shortName;

        // Last resort: key only (still never draw "...").
        return ScopeModules::idToKey (id);
    }

    /** Card body fill — paint BEHIND modules (e.g. MainComponent::paint). */
    inline void paintCardFill (juce::Graphics& g,
                               juce::Rectangle<float> outer,
                               const SharedColors& c,
                               bool stripMode)
    {
        if (outer.getWidth() < 4.0f || outer.getHeight() < 4.0f)
            return;

        const float rad = radiusFor (stripMode);
        juce::Path card;
        card.addRoundedRectangle (outer, rad);

        juce::ColourGradient grad (c.oscBackground.brighter (0.06f).withAlpha (0.96f),
                                   outer.getX(), outer.getY(),
                                   c.oscBackground2.darker (0.14f).withAlpha (0.98f),
                                   outer.getX(), outer.getBottom(), false);
        g.setGradientFill (grad);
        g.fillPath (card);

        // Top sheen (local copy so we don't shrink the stroke path)
        auto sheenR = outer;
        sheenR.setHeight (juce::jmin (24.0f, outer.getHeight() * 0.28f));
        juce::Path sheenPath;
        sheenPath.addRoundedRectangle (sheenR, rad);
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.fillPath (sheenPath);
    }

    /** Card edge — paint ABOVE modules (overlay) so rims stay crisp. */
    inline void paintCardStroke (juce::Graphics& g,
                                 juce::Rectangle<float> outer,
                                 const SharedColors& c,
                                 bool hover,
                                 bool stripMode)
    {
        if (outer.getWidth() < 4.0f || outer.getHeight() < 4.0f)
            return;

        const float rad = radiusFor (stripMode);
        juce::Path card;
        card.addRoundedRectangle (outer, rad);

        g.setColour (juce::Colours::white.withAlpha (hover ? 0.20f : 0.12f));
        g.strokePath (card, juce::PathStrokeType (1.0f));

        g.setColour (c.scopeDropOutline.withAlpha (hover ? 0.50f : 0.20f));
        g.strokePath (card, juce::PathStrokeType (hover ? 1.5f : 1.1f));

        if (hover)
        {
            g.setColour (c.scopeDropOutline.withAlpha (0.14f));
            g.strokePath (card, juce::PathStrokeType (3.5f));
        }
    }

    inline void paintHeader (juce::Graphics& g,
                             juce::Rectangle<float> band,
                             ScopeModuleId id,
                             const SharedColors& c,
                             bool hover)
    {
        if (band.getWidth() < 8.0f || band.getHeight() < 4.0f)
            return;

        // Header wash
        g.setColour (juce::Colours::black.withAlpha (hover ? 0.32f : 0.22f));
        g.fillRect (band);

        // Accent rail
        auto rail = juce::Rectangle<float> (band.getX(), band.getY() + 3.0f,
                                            (float) Metrics::kAccentRailW, band.getHeight() - 6.0f);
        g.setColour (c.scopeDropOutline.withAlpha (hover ? 0.98f : 0.75f));
        g.fillRoundedRectangle (rail, 1.0f);

        // Hairline under header
        g.setColour (c.scopeDropOutline.withAlpha (0.28f));
        g.fillRect (band.getX(), band.getBottom() - 1.0f, band.getWidth(), 1.0f);

        const float fontH = juce::jlimit (10.0f, 12.5f, band.getHeight() - 4.0f);
        const auto font = juce::Font (juce::FontOptions (fontH).withStyle ("Bold"));
        g.setFont (font);

        auto textArea = band.withTrimmedLeft ((float) Metrics::kAccentRailW + 6.0f).reduced (4.0f, 0.0f);
        const auto title = titleThatFits (id, textArea.getWidth(), font);
        // Header wash is near-black — resolve ink for legibility (default on globally).
        const auto headerBg = juce::Colours::black;
        const auto seed = hover ? c.scopeDropOutline.brighter (0.18f) : juce::Colours::whitesmoke;
        g.setColour (c.legibleTextOn (seed, headerBg).withAlpha (hover ? 0.98f : 0.92f));
        g.drawText (title, textArea.toNearestInt(), juce::Justification::centredLeft, false);
    }
} // namespace ScopePaneChrome
