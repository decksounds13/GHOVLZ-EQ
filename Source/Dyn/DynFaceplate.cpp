#include "DynFaceplate.h"
#include "../KnobThemeHelpers.h"

namespace
{
    int textW (const juce::Font& font, const juce::String& s)
    {
        juce::GlyphArrangement ga;
        ga.addLineOfText (font, s, 0.0f, 0.0f);
        return (int) std::ceil (ga.getBoundingBox (0, ga.getNumGlyphs(), true).getWidth()) + 8;
    }

    void styleLabel (juce::Label& l, float h)
    {
        l.setFont (SharedResources::uiFont (h));
        l.setJustificationType (juce::Justification::centred);
        l.setMinimumHorizontalScale (1.0f);
        l.setInterceptsMouseClicks (false, false);
    }

    void hideBox (juce::Slider& s)
    {
        s.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    }

    juce::Colour bandTint (int b)
    {
        static const juce::Colour k[] = {
            juce::Colour::fromRGB (100, 149, 237),
            juce::Colour::fromRGB (192, 96, 224),
            juce::Colour::fromRGB (32, 224, 224),
            juce::Colour::fromRGB (80, 96, 255),
            juce::Colour::fromRGB (40, 170, 70),
            juce::Colour::fromRGB (230, 50, 50)
        };
        return k[juce::jlimit (0, 5, b)];
    }
}

DynFaceplate::DynFaceplate (juce::AudioProcessorValueTreeState& s, DynCompressor& e, Analyser& analyser)
    : state (s), engine (e), splitLearn (s, analyser, e), globalPanel (s)
{
    auto setupChrome = [this] (juce::TextButton& b)
    {
        b.setLookAndFeel (&chromeLaf);
        b.setClickingTogglesState (true);
        GraphOverlayButtonLookAndFeel::setCaptionFontDelta (b, 4);
        addAndMakeVisible (b);
    };
    setupChrome (focusButton);
    setupChrome (allButton);
    setupChrome (addBandButton);
    setupChrome (focusLearnButton);
    GraphOverlayButtonLookAndFeel::setCaptionFontDelta (focusLearnButton, 6);
    GraphOverlayButtonLookAndFeel::setCaptionBold (focusLearnButton, true);
    addBandButton.setClickingTogglesState (false);
    addBandButton.setTooltip ("Add a band. Splits the selected band in half.");
    focusLearnButton.setButtonText ("Learn");
    focusLearnButton.setClickingTogglesState (true);
    focusLearnButton.setTooltip ("Listen for about 5 seconds and park this band's Down, Up, and Clip thresholds so they just tickle the signal. Right click chooses which.");
    focusLearnButton.onPopupMenu = [this] { showBandLearnMenu (getSelectedBand(), focusLearnButton); };
    focusLearnButton.onClick = [this] { toggleBandLearn (getSelectedBand()); };
    splitLearn.onFinished = [this] { refreshLearnButtons(); };
    addBandButton.onClick = [this]
    {
        const int n = bandCount();
        if (n >= DynParams::kMaxBands)
            return;
        auto getSplit = [this] (int b, float fb) -> float
        {
            if (auto* p = state.getRawParameterValue (DynParams::splitId (b)))
                return p->load();
            return fb;
        };
        const int sel = getSelectedBand();
        float lo = 20.0f;
        for (int b = 0; b < sel; ++b)
            lo = getSplit (b, 1000.0f);
        const float hi = (sel < n - 1) ? getSplit (sel, 20000.0f) : 20000.0f;
        DynParams::insertSplitAtHz (state, std::sqrt (lo * hi));
        refresh();
        if (onModeChanged)
            onModeChanged();
    };
    setupChrome (onButton);
    setupChrome (soloButton);
    onButton.setTooltip ("Turn this band on or off.");
    soloButton.setTooltip ("Hear only this band.");
    focusButton.setTooltip ("Show one band's knobs and transfer curve.");
    allButton.setTooltip ("Show every band as a card.");
    focusButton.setClickingTogglesState (false);
    allButton.setClickingTogglesState (false);
    focusButton.onClick = [this]
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (DynParams::faceAllId())))
            *p = false;
        refresh();
        if (onModeChanged)
            onModeChanged();
    };
    allButton.onClick = [this]
    {
        if (auto* p = dynamic_cast<juce::AudioParameterBool*> (state.getParameter (DynParams::faceAllId())))
            *p = true;
        refresh();
        if (onModeChanged)
            onModeChanged();
    };

    selLabel.setMinimumHorizontalScale (1.0f);
    addAndMakeVisible (selLabel);

    auto addKnob = [this] (juce::Slider& k, juce::Label& cap, juce::Label& val, const juce::String& word)
    {
        hideBox (k);
        addAndMakeVisible (k);
        cap.setText (word, juce::dontSendNotification);
        styleLabel (cap, 11.5f);
        styleLabel (val, 10.5f);
        addAndMakeVisible (cap);
        addAndMakeVisible (val);
    };
    addKnob (thrKnob, thrLab, thrVal, "Down");
    addKnob (upThrKnob, upThrLab, upThrVal, "Up");
    addKnob (ratioKnob, ratioLab, ratioVal, "Ratio");
    addKnob (attackKnob, attackLab, attackVal, "Attack");
    addKnob (releaseKnob, releaseLab, releaseVal, "Release");
    addKnob (kneeKnob, kneeLab, kneeVal, "Knee");
    addKnob (makeupKnob, makeupLab, makeupVal, "Makeup");
    addKnob (mixKnob, mixLab, mixVal, "Mix");
    addKnob (clipKnob, clipLab, clipVal, "Clip");
    thrKnob.setTooltip ("Down threshold. Sounds louder than this get turned down.");
    upThrKnob.setTooltip ("Up threshold. Sounds quieter than this get turned up.");
    ratioKnob.setTooltip ("How hard this band squeezes. Higher is more compression. Below 1:1 is lift only.");
    attackKnob.setTooltip ("How fast the compressor grabs a loud sound.");
    releaseKnob.setTooltip ("How fast the compressor lets go after the sound drops.");
    kneeKnob.setTooltip ("How softly compression eases in around the threshold. Higher is smoother.");
    makeupKnob.setTooltip ("Volume after compression. Use this to make up what was turned down.");
    mixKnob.setTooltip ("Blend between the compressed sound and the dry original.");
    clipKnob.setTooltip ("Pushes this band into a clipper before compression. Right click chooses Soft or Hard.");
    clipKnob.onPopupMenu = [this] { showClipModeMenu (getSelectedBand(), clipKnob); };

    auto hookReadout = [this]
    {
        if (! showingAll)
            updateFocusReadouts();
    };
    thrKnob.onValueChange = [this, hookReadout]
    {
        DynParams::writeDownThr (state, getSelectedBand(), (float) thrKnob.getValue());
        hookReadout();
    };
    upThrKnob.onValueChange = [this, hookReadout]
    {
        DynParams::writeUpThr (state, getSelectedBand(), (float) upThrKnob.getValue());
        hookReadout();
    };
    ratioKnob.onValueChange = hookReadout;
    attackKnob.onValueChange = hookReadout;
    releaseKnob.onValueChange = hookReadout;
    kneeKnob.onValueChange = hookReadout;
    makeupKnob.onValueChange = hookReadout;
    mixKnob.onValueChange = hookReadout;
    clipKnob.onValueChange = hookReadout;

    focusGr = std::make_unique<GainReductionMeter> (state, engine, [this]
    {
        return getSelectedBand();
    });
    addAndMakeVisible (*focusGr);

    addAndMakeVisible (globalPanel);
    globalPanel.timeKnob.onValueChange = [this] { globalPanel.updateReadouts(); };
    globalPanel.amountKnob.onValueChange = [this] { globalPanel.updateReadouts(); };
    globalPanel.downKnob.onValueChange = [this] { globalPanel.updateReadouts(); };
    globalPanel.upKnob.onValueChange = [this] { globalPanel.updateReadouts(); };

    bindFocus (0);
    rebuildAll();
    state.addParameterListener (DynParams::countId(), this);
    state.addParameterListener (DynParams::selectedId(), this);
    state.addParameterListener (DynParams::faceAllId(), this);
    refresh();
}

void DynFaceplate::parameterChanged (const juce::String&, float)
{
    triggerAsyncUpdate();
}

void DynFaceplate::handleAsyncUpdate()
{
    refresh();
    if (onModeChanged)
        onModeChanged();
}

DynFaceplate::~DynFaceplate()
{
    state.removeParameterListener (DynParams::countId(), this);
    state.removeParameterListener (DynParams::selectedId(), this);
    state.removeParameterListener (DynParams::faceAllId(), this);
    focusButton.setLookAndFeel (nullptr);
    allButton.setLookAndFeel (nullptr);
    addBandButton.setLookAndFeel (nullptr);
    focusLearnButton.setLookAndFeel (nullptr);
    splitLearn.onFinished = nullptr;
    splitLearn.cancel();
    onButton.setLookAndFeel (nullptr);
    soloButton.setLookAndFeel (nullptr);
}

int DynFaceplate::bandCount() const
{
    if (auto* p = state.getRawParameterValue (DynParams::countId()))
        return juce::jlimit (1, DynParams::kMaxBands, (int) std::lround (p->load()));
    return 1;
}

int DynFaceplate::getSelectedBand() const
{
    if (auto* p = state.getRawParameterValue (DynParams::selectedId()))
        return juce::jlimit (0, juce::jmax (0, bandCount() - 1), (int) std::lround (p->load()));
    return 0;
}

void DynFaceplate::bindFocus (int band)
{
    band = juce::jlimit (0, DynParams::kMaxBands - 1, band);
    thrAt.reset(); upThrAt.reset(); ratioAt.reset(); attackAt.reset(); releaseAt.reset();
    kneeAt.reset(); makeupAt.reset(); mixAt.reset(); clipAt.reset();
    onAt.reset(); soloAt.reset();

    thrAt = std::make_unique<SliderAttachment> (state, DynParams::thresholdId (band), thrKnob);
    upThrAt = std::make_unique<SliderAttachment> (state, DynParams::upThresholdId (band), upThrKnob);
    ratioAt = std::make_unique<SliderAttachment> (state, DynParams::ratioId (band), ratioKnob);
    attackAt = std::make_unique<SliderAttachment> (state, DynParams::attackId (band), attackKnob);
    releaseAt = std::make_unique<SliderAttachment> (state, DynParams::releaseId (band), releaseKnob);
    kneeAt = std::make_unique<SliderAttachment> (state, DynParams::kneeId (band), kneeKnob);
    makeupAt = std::make_unique<SliderAttachment> (state, DynParams::makeupId (band), makeupKnob);
    mixAt = std::make_unique<SliderAttachment> (state, DynParams::mixId (band), mixKnob);
    clipAt = std::make_unique<SliderAttachment> (state, DynParams::clipId (band), clipKnob);
    onAt = std::make_unique<ButtonAttachment> (state, DynParams::onId (band), onButton);
    soloAt = std::make_unique<ButtonAttachment> (state, DynParams::soloId (band), soloButton);
}

void DynFaceplate::rebuildAll()
{
    columns.clear();
    const int n = bandCount();
    for (int b = 0; b < n; ++b)
    {
        auto* col = new AllColumn (state, engine, b, chromeLaf);
        col->setThemeColors (theme);
        columns.add (col);
        addChildComponent (col);
    }
}

void DynFaceplate::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    thrKnob.setThemeColors (r);
    upThrKnob.setThemeColors (r);
    ratioKnob.setThemeColors (r);
    attackKnob.setThemeColors (r);
    releaseKnob.setThemeColors (r);
    kneeKnob.setThemeColors (r);
    makeupKnob.setThemeColors (r);
    mixKnob.setThemeColors (r);
    clipKnob.setThemeColors (r);
    if (focusGr != nullptr)
        focusGr->setThemeColors (r);
    globalPanel.setThemeColors (r);
    for (auto* col : columns)
        col->setThemeColors (r);
}

int DynFaceplate::getFocusClusterWidth (int forHeight) const noexcept
{
    const int nTop = 6;
    const int h = forHeight > 0 ? forHeight : getHeight();
    const int bodyH = juce::jmax (80, h - 26 - 12);
    const int capH = 15;
    const int valH = 14;
    const int rowGap = 8;
    const int knobD = juce::jlimit (48, 80, (bodyH - 28 - 2 * (capH + valH) - rowGap) / 2);
    const auto capFont = SharedResources::uiFont (12.0f);
    const int slotGap = 10;
    const char* words[] = { "Down", "Up", "Attack", "Knee", "Mix", "Clip" };
    int knobsW = 0;
    for (int i = 0; i < nTop; ++i)
        knobsW += juce::jmax (knobD + 12, textW (capFont, words[i]));
    knobsW += slotGap * (nTop - 1);
    return knobsW + 16 + 150 + 36;
}

void DynFaceplate::updateFocusReadouts()
{
    thrVal.setText (juce::String (thrKnob.getValue(), 1) + " dB", juce::dontSendNotification);
    upThrVal.setText (juce::String (upThrKnob.getValue(), 1) + " dB", juce::dontSendNotification);
    attackVal.setText (juce::String (attackKnob.getValue(), 1) + " ms", juce::dontSendNotification);
    releaseVal.setText (juce::String (releaseKnob.getValue(), 0) + " ms", juce::dontSendNotification);
    kneeVal.setText (juce::String (kneeKnob.getValue(), 1) + " dB", juce::dontSendNotification);
    makeupVal.setText (juce::String (makeupKnob.getValue(), 1) + " dB", juce::dontSendNotification);
    mixVal.setText (juce::String (mixKnob.getValue(), 0) + " %", juce::dontSendNotification);
    ratioVal.setText (DynParams::formatRatio ((float) ratioKnob.getValue()), juce::dontSendNotification);
    const float cl = (float) clipKnob.getValue();
    int clipMode = 0;
    if (auto* p = state.getRawParameterValue (DynParams::clipModeId (getSelectedBand())))
        clipMode = (int) std::lround (p->load());
    clipVal.setText (cl <= 0.05f ? juce::String ("Off")
                                 : juce::String (cl, 1) + (clipMode != 0 ? " Hard" : " Soft"),
                     juce::dontSendNotification);
}

void DynFaceplate::refresh()
{
    showingAll = false;
    if (auto* p = state.getRawParameterValue (DynParams::faceAllId()))
        showingAll = p->load() > 0.5f;

    if (columns.size() != bandCount())
        rebuildAll();

    bindFocus (getSelectedBand());
    selLabel.setText ("Band " + juce::String (getSelectedBand() + 1), juce::dontSendNotification);
    resized();
    updateFocusReadouts();
    globalPanel.updateReadouts();
    repaint();
}

void DynFaceplate::drawModuleShadow (juce::Graphics& g, juce::Rectangle<float> r)
{
    if (r.getWidth() < 8.0f || r.getHeight() < 8.0f)
        return;

    juce::Path p;
    p.addRoundedRectangle (r, 6.5f);
    if (SharedResources::glowShadowEffectsEnabled())
    {
        moduleShadow.setRadius (14.0, 0);
        moduleShadow.setSpread (1.0, 0);
        moduleShadow.setOffset (0, 5, 0);
        moduleShadow.setColor (juce::Colours::black.withAlpha (0.55f), 0);
        moduleShadow.setRadius (5.0, 1);
        moduleShadow.setSpread (0.0, 1);
        moduleShadow.setOffset (0, 2, 1);
        moduleShadow.setColor (juce::Colours::black.withAlpha (0.32f), 1);
        moduleShadow.render (g, p);
    }
    else
    {
        g.setColour (juce::Colours::black.withAlpha (0.38f));
        g.fillRoundedRectangle (r.translated (0.0f, 3.0f).expanded (1.5f, 2.5f), 6.5f);
    }
}

void DynFaceplate::paint (juce::Graphics& g)
{
    const auto& c = theme != nullptr ? theme->sharedColors : SharedColors {};
    g.setGradientFill (juce::ColourGradient (c.pluginBackground2, 0, 0,
                                             c.pluginBackground, 0, (float) getHeight(), false));
    g.fillAll();

    if (! showingAll && ! focusWell.isEmpty())
    {
        auto well = focusWell.toFloat();
        drawModuleShadow (g, well);
        g.setColour (c.pluginBackground.brighter (0.07f));
        g.fillRoundedRectangle (well, 7.0f);
        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawRoundedRectangle (well, 7.0f, 1.0f);
    }
    else if (showingAll)
    {
        for (auto* col : columns)
            if (col != nullptr && col->isVisible())
                drawModuleShadow (g, col->getBounds().toFloat().reduced (0.5f));
    }

    if (globalPanel.isVisible())
        drawModuleShadow (g, globalPanel.getBounds().toFloat().reduced (0.5f));
}

void DynFaceplate::resized()
{
    auto r = getLocalBounds();
    auto head = r.removeFromTop (26);
    const auto font = SharedResources::uiFont (12.5f, true);
    selLabel.setFont (font);
    selLabel.setMinimumHorizontalScale (1.0f);
    const int selW = juce::jmax (80, textW (font, selLabel.getText()));
    selLabel.setBounds (head.removeFromLeft (selW).reduced (6, 2));

    auto placeRight = [&head] (juce::TextButton& b, const juce::String& word, int minW)
    {
        b.setButtonText (word);
        const int w = juce::jmax (minW, (int) std::ceil (b.getBestWidthForHeight (22)));
        b.setBounds (head.removeFromRight (w).reduced (2, 2));
    };
    placeRight (addBandButton, "+", 28);
    placeRight (allButton, "All", 40);
    placeRight (focusButton, "Single", 56);
    head.removeFromRight (8);
    placeRight (focusLearnButton, "Learn", 60);
    placeRight (soloButton, "Solo", 36);
    placeRight (onButton, "On", 36);

    focusLearnButton.setVisible (! showingAll);
    refreshLearnButtons();

    const bool all = showingAll;
    focusButton.setToggleState (! all, juce::dontSendNotification);
    allButton.setToggleState (all, juce::dontSendNotification);
    onButton.setVisible (! all);
    soloButton.setVisible (! all);
    selLabel.setVisible (! all);

    auto body = r.reduced (10, 6);
    focusWell = {};

    const int panelW = DynParams::kGlobalPanelW;
    auto panelSlot = body.removeFromRight (panelW);
    body.removeFromRight (8);
    globalPanel.setVisible (true);
    globalPanel.setBounds (panelSlot);
    globalPanel.updateReadouts();

    if (! all)
    {
        for (auto* col : columns)
            col->setVisible (false);

        juce::Slider* topS[] = { &thrKnob, &upThrKnob, &attackKnob, &kneeKnob, &mixKnob, &clipKnob };
        juce::Label*  topC[] = { &thrLab, &upThrLab, &attackLab, &kneeLab, &mixLab, &clipLab };
        juce::Label*  topV[] = { &thrVal, &upThrVal, &attackVal, &kneeVal, &mixVal, &clipVal };
        const char* topW[] = { "Down", "Up", "Attack", "Knee", "Mix", "Clip" };

        juce::Slider* botS[] = { &ratioKnob, &releaseKnob, &makeupKnob };
        juce::Label*  botC[] = { &ratioLab, &releaseLab, &makeupLab };
        juce::Label*  botV[] = { &ratioVal, &releaseVal, &makeupVal };
        const char* botW[] = { "Ratio", "Release", "Makeup" };

        const int nTop = 6;
        const int nBot = 3;
        const int capH = 15;
        const int valH = 14;
        const int rowGap = 8;
        const int wellPadX = 18;
        const int wellPadY = 14;
        const int knobD = juce::jlimit (48, 80,
            (body.getHeight() - wellPadY * 2 - 2 * (capH + valH) - rowGap) / 2);
        const auto capFont = SharedResources::uiFont (12.0f);
        const int slotGap = 10;

        int slotW[6] {};
        int knobsW = 0;
        for (int i = 0; i < nTop; ++i)
        {
            slotW[i] = juce::jmax (knobD + 12, textW (capFont, topW[i]));
            knobsW += slotW[i];
        }
        knobsW += slotGap * (nTop - 1);

        const int grGap = 10;
        const int grW = 150;
        const int rowH = knobD + capH + valH;
        const int innerH = rowH * 2 + rowGap;
        const int innerW = knobsW + grGap + grW;
        const int wellW = juce::jmin (body.getWidth(), innerW + wellPadX * 2);
        const int wellH = innerH + wellPadY * 2;
        const int wellX = body.getX() + juce::jmax (0, (body.getWidth() - wellW) / 2);
        const int wellY = body.getY() + juce::jmax (0, (body.getHeight() - wellH) / 2);
        focusWell = { wellX, wellY, wellW, wellH };

        auto inner = focusWell.reduced (wellPadX, wellPadY);
        int x = inner.getX();
        const int ky = inner.getY();

        auto placeKnob = [&] (juce::Slider& k, juce::Label& cap, juce::Label& val,
                              const char* word, juce::Rectangle<int> slot)
        {
            k.setVisible (true);
            cap.setVisible (true);
            val.setVisible (true);
            cap.setText (word, juce::dontSendNotification);
            cap.setMinimumHorizontalScale (1.0f);
            val.setMinimumHorizontalScale (1.0f);
            k.setBounds (slot.removeFromTop (knobD).withSizeKeepingCentre (knobD, knobD));
            cap.setBounds (slot.removeFromTop (capH));
            val.setBounds (slot.removeFromTop (valH));
        };

        for (int i = 0; i < nTop; ++i)
        {
            auto slot = juce::Rectangle<int> (x, ky, slotW[i], rowH);
            placeKnob (*topS[i], *topC[i], *topV[i], topW[i], slot);
            x += slotW[i] + slotGap;
        }

        x = inner.getX();
        const int ky2 = ky + rowH + rowGap;
        for (int i = 0; i < nBot; ++i)
        {
            const int w = (i < nTop) ? slotW[i] : slotW[0];
            auto slot = juce::Rectangle<int> (x, ky2, w, rowH);
            placeKnob (*botS[i], *botC[i], *botV[i], botW[i], slot);
            x += w + slotGap;
        }

        if (focusGr != nullptr)
        {
            focusGr->setVisible (true);
            const int grX = inner.getX() + knobsW + grGap;
            focusGr->setBounds (grX, ky, grW, innerH);
        }

        updateFocusReadouts();
    }
    else
    {
        thrKnob.setVisible (false); upThrKnob.setVisible (false);
        ratioKnob.setVisible (false); attackKnob.setVisible (false);
        releaseKnob.setVisible (false); kneeKnob.setVisible (false); makeupKnob.setVisible (false);
        mixKnob.setVisible (false);
        clipKnob.setVisible (false);
        clipLab.setVisible (false);
        clipVal.setVisible (false);
        thrLab.setVisible (false); upThrLab.setVisible (false);
        ratioLab.setVisible (false); attackLab.setVisible (false);
        releaseLab.setVisible (false); kneeLab.setVisible (false); makeupLab.setVisible (false);
        mixLab.setVisible (false);
        thrVal.setVisible (false); upThrVal.setVisible (false);
        ratioVal.setVisible (false); attackVal.setVisible (false);
        releaseVal.setVisible (false); kneeVal.setVisible (false); makeupVal.setVisible (false);
        mixVal.setVisible (false);
        if (focusGr != nullptr)
            focusGr->setVisible (false);

        const int n = columns.size();
        if (n > 0)
        {
            const int gap = 8;
            const int avail = juce::jmax (1, body.getWidth());
            int cardW = DynParams::kBandCardW;
            const int need = n * cardW + (n - 1) * gap;
            if (need > avail)
                cardW = juce::jmax (160, (avail - gap * (n - 1)) / n);

            int x = body.getX();
            for (auto* col : columns)
            {
                col->setVisible (true);
                col->setBounds (x, body.getY(), cardW, body.getHeight());
                x += cardW + gap;
            }
        }
    }

    globalPanel.toFront (false);
}

DynFaceplate::AllColumn::AllColumn (juce::AudioProcessorValueTreeState& s, DynCompressor& e, int bandIndex,
                                    GraphOverlayButtonLookAndFeel& chrome)
    : band (bandIndex), state (s)
{
    title.setText ("Band " + juce::String (band + 1), juce::dontSendNotification);
    title.setMinimumHorizontalScale (1.0f);
    title.setFont (SharedResources::uiFont (16.0f, true));
    title.setJustificationType (juce::Justification::centredLeft);
    title.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.92f));
    addAndMakeVisible (title);

    auto setup = [this, &chrome] (juce::TextButton& b)
    {
        b.setLookAndFeel (&chrome);
        b.setClickingTogglesState (true);
        GraphOverlayButtonLookAndFeel::setCaptionFontDelta (b, 4);
        addAndMakeVisible (b);
    };
    setup (on); setup (solo); setup (learn);
    on.setTooltip ("Turn this band on or off.");
    solo.setTooltip ("Hear only this band.");
    GraphOverlayButtonLookAndFeel::setCaptionFontDelta (learn, 6);
    GraphOverlayButtonLookAndFeel::setCaptionBold (learn, true);
    learn.setButtonText ("Learn");
    learn.setTooltip ("Listen for about 5 seconds and park this band's Down, Up, and Clip thresholds so they just tickle the signal. Right click chooses which.");
    learn.onPopupMenu = [this]
    {
        if (auto* fp = findParentComponentOfClass<DynFaceplate>())
            fp->showBandLearnMenu (band, learn);
    };
    learn.onClick = [this]
    {
        if (auto* fp = findParentComponentOfClass<DynFaceplate>())
            fp->toggleBandLearn (band);
    };

    auto addK = [this] (juce::Slider& k, juce::Label& lab, const juce::String& word)
    {
        hideBox (k);
        lab.setText (word, juce::dontSendNotification);
        styleLabel (lab, 10.5f);
        addAndMakeVisible (k);
        addAndMakeVisible (lab);
    };
    addK (thr, tL, "Down");
    addK (upThr, uL, "Up");
    addK (ratio, rL, "Ratio");
    addK (attack, aL, "Attack");
    addK (release, eL, "Release");
    addK (knee, kL, "Knee");
    addK (makeup, mL, "Makeup");
    addK (clip, cL, "Clip");
    thr.setTooltip ("Down threshold. Sounds louder than this get turned down.");
    upThr.setTooltip ("Up threshold. Sounds quieter than this get turned up.");
    ratio.setTooltip ("How hard this band squeezes. Higher is more compression. Below 1:1 is lift only.");
    attack.setTooltip ("How fast the compressor grabs a loud sound.");
    release.setTooltip ("How fast the compressor lets go after the sound drops.");
    knee.setTooltip ("How softly compression eases in around the threshold. Higher is smoother.");
    makeup.setTooltip ("Volume after compression. Use this to make up what was turned down.");
    clip.setTooltip ("Pushes this band into a clipper before compression. Right click chooses Soft or Hard.");
    clip.onPopupMenu = [this]
    {
        if (auto* fp = findParentComponentOfClass<DynFaceplate>())
            fp->showClipModeMenu (band, clip);
    };

    gr = std::make_unique<GainReductionMeter> (s, e, [this] { return band; });
    addAndMakeVisible (*gr);

    xfer = std::make_unique<TransferCurveComponent> (state, [this] { return band; });
    xfer->setEngine (&e);
    xfer->setCompact (true);
    addAndMakeVisible (*xfer);

    tA = std::make_unique<SliderAttachment> (state, DynParams::thresholdId (band), thr);
    thr.onValueChange = [this]
    {
        DynParams::writeDownThr (state, band, (float) thr.getValue());
    };
    uA = std::make_unique<SliderAttachment> (state, DynParams::upThresholdId (band), upThr);
    upThr.onValueChange = [this]
    {
        DynParams::writeUpThr (state, band, (float) upThr.getValue());
    };
    rA = std::make_unique<SliderAttachment> (state, DynParams::ratioId (band), ratio);
    aA = std::make_unique<SliderAttachment> (state, DynParams::attackId (band), attack);
    eA = std::make_unique<SliderAttachment> (state, DynParams::releaseId (band), release);
    kA = std::make_unique<SliderAttachment> (state, DynParams::kneeId (band), knee);
    mA = std::make_unique<SliderAttachment> (state, DynParams::makeupId (band), makeup);
    cA = std::make_unique<SliderAttachment> (state, DynParams::clipId (band), clip);
    onA = std::make_unique<ButtonAttachment> (state, DynParams::onId (band), on);
    soA = std::make_unique<ButtonAttachment> (state, DynParams::soloId (band), solo);
}

DynFaceplate::AllColumn::~AllColumn()
{
    on.setLookAndFeel (nullptr);
    solo.setLookAndFeel (nullptr);
    learn.setLookAndFeel (nullptr);
}

void DynFaceplate::AllColumn::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    thr.setThemeColors (r);
    upThr.setThemeColors (r);
    ratio.setThemeColors (r);
    attack.setThemeColors (r);
    release.setThemeColors (r);
    knee.setThemeColors (r);
    makeup.setThemeColors (r);
    clip.setThemeColors (r);
    if (gr != nullptr)
        gr->setThemeColors (r);
    if (xfer != nullptr)
        xfer->setThemeColors (r);
}

void DynFaceplate::AllColumn::paint (juce::Graphics& g)
{
    auto box = getLocalBounds().toFloat().reduced (0.5f);
    bool selected = false;
    if (auto* p = state.getRawParameterValue (DynParams::selectedId()))
        selected = (int) std::lround (p->load()) == band;

    const auto& pal0 = theme != nullptr ? theme->sharedColors : SharedColors {};
    g.setColour (pal0.pluginBackground.brighter (selected ? 0.10f : 0.06f));
    g.fillRoundedRectangle (box, 6.0f);

    juce::Colour tint = bandTint (band);
    if (theme != nullptr)
    {
        const auto& pal = theme->sharedColors;
        switch (juce::jlimit (0, 5, band))
        {
            case 0: tint = pal.graphBand1; break;
            case 1: tint = pal.graphBand2; break;
            case 2: tint = pal.graphBand3; break;
            case 3: tint = pal.graphBand4; break;
            case 4: tint = pal.graphBand5; break;
            case 5: tint = pal.graphBand6; break;
            default: break;
        }
        tint = pal.applyGraphBandMinSaturation (tint.withAlpha (1.0f));
    }

    auto stroke = selected ? tint.withAlpha (0.55f)
                           : juce::Colours::white.withAlpha (0.075f);
    g.setColour (stroke);
    g.drawRoundedRectangle (box, 6.0f, selected ? 1.4f : 1.0f);
}

void DynFaceplate::AllColumn::mouseUp (const juce::MouseEvent&)
{
    if (auto* s = dynamic_cast<juce::AudioParameterInt*> (state.getParameter (DynParams::selectedId())))
        *s = band;
}

void DynFaceplate::AllColumn::resized()
{
    auto r = getLocalBounds().withTrimmedTop (6).withTrimmedBottom (6)
                             .withTrimmedLeft (7).withTrimmedRight (10);

    auto head = r.removeFromTop (24);
    const auto titleFont = SharedResources::uiFont (16.0f, true);
    title.setFont (titleFont);
    title.setJustificationType (juce::Justification::centredLeft);
    title.setMinimumHorizontalScale (1.0f);
    const int titleW = juce::jmax (56, textW (titleFont, title.getText()));
    title.setBounds (head.removeFromLeft (titleW));

    on.setButtonText ("On");
    solo.setButtonText ("Solo");
    const int bh = 20;
    const int onW = juce::jmax (32, (int) std::ceil (on.getBestWidthForHeight (bh)));
    const int soW = juce::jmax (36, (int) std::ceil (solo.getBestWidthForHeight (bh)));
    head.removeFromLeft (6);
    const int by = head.getY() + juce::jmax (0, (head.getHeight() - bh) / 2);
    learn.setButtonText ("Learn");
    const int learnW = juce::jmax (56, (int) std::ceil (learn.getBestWidthForHeight (bh)));
    on.setBounds (head.getX(), by, onW, bh);
    solo.setBounds (head.getX() + onW + 4, by, soW, bh);
    learn.setBounds (head.getX() + onW + 4 + soW + 4, by, learnW, bh);

    r.removeFromTop (3);

    const int xferH = juce::jlimit (96, 196, r.getHeight() * 50 / 100);
    auto xferRow = r.removeFromTop (xferH);
    if (xfer != nullptr)
    {
        const int side = juce::jmin (xferRow.getWidth(), xferRow.getHeight());
        xfer->setVisible (true);
        xfer->setBounds (xferRow.withSizeKeepingCentre (side, side));
    }

    r.removeFromTop (6);

    juce::Slider* ks[] = { &thr, &upThr, &ratio, &attack, &release, &knee, &makeup, &clip };
    juce::Label* ls[] = { &tL, &uL, &rL, &aL, &eL, &kL, &mL, &cL };
    const char* words[] = { "Down", "Up", "Ratio", "Attack", "Release", "Knee", "Makeup", "Clip" };

    const int labH = 13;
    const int cols = 2;
    const int rows = 4;
    const auto capFont = SharedResources::uiFont (10.5f);
    int labelW = 0;
    for (auto* w : words)
        labelW = juce::jmax (labelW, textW (capFont, w));

    const int cellH = r.getHeight() / rows;
    const int d = juce::jmax (36, juce::jmin (cellH - labH - 2, 64));
    const int cellW = juce::jmax (d + 8, labelW);
    const int knobsW = cellW * cols;

    if (gr != nullptr)
    {
        auto meter = r;
        const int meterMin = 150;
        const int knobsTake = juce::jmin (knobsW + 15, juce::jmax (80, r.getWidth() - meterMin));
        meter.removeFromLeft (knobsTake);
        gr->setBounds (meter);
    }

    for (int i = 0; i < 8; ++i)
    {
        auto cell = juce::Rectangle<int> (r.getX() + (i % cols) * cellW,
                                          r.getY() + (i / cols) * cellH,
                                          cellW, cellH).reduced (2, 1);
        ls[i]->setText (words[i], juce::dontSendNotification);
        ls[i]->setMinimumHorizontalScale (1.0f);
        auto lab = cell.removeFromBottom (labH);
        ks[i]->setBounds (cell.withSizeKeepingCentre (d, d));
        ls[i]->setBounds (lab);
    }
}

DynFaceplate::GlobalPanel::GlobalPanel (juce::AudioProcessorValueTreeState& s)
    : state (s)
{
    title.setText ("Global", juce::dontSendNotification);
    title.setMinimumHorizontalScale (1.0f);
    title.setFont (SharedResources::uiFont (16.0f));
    title.setJustificationType (juce::Justification::centred);
    title.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.92f));
    addAndMakeVisible (title);

    hideBox (timeKnob);
    hideBox (amountKnob);
    hideBox (downKnob);
    hideBox (upKnob);
    addAndMakeVisible (timeKnob);
    addAndMakeVisible (amountKnob);
    addAndMakeVisible (downKnob);
    addAndMakeVisible (upKnob);

    timeLab.setText ("Time", juce::dontSendNotification);
    amountLab.setText ("Amount", juce::dontSendNotification);
    downLab.setText ("Down", juce::dontSendNotification);
    upLab.setText ("Up", juce::dontSendNotification);
    styleLabel (timeLab, 13.5f);
    styleLabel (amountLab, 13.5f);
    styleLabel (downLab, 13.5f);
    styleLabel (upLab, 13.5f);
    styleLabel (timeVal, 12.5f);
    styleLabel (amountVal, 12.5f);
    styleLabel (downVal, 12.5f);
    styleLabel (upVal, 12.5f);
    addAndMakeVisible (timeLab);
    addAndMakeVisible (amountLab);
    addAndMakeVisible (downLab);
    addAndMakeVisible (upLab);
    addAndMakeVisible (timeVal);
    addAndMakeVisible (amountVal);
    addAndMakeVisible (downVal);
    addAndMakeVisible (upVal);

    timeKnob.setTooltip ("Time. Speeds up or slows down every band's Attack and Release. 100 is as written.");
    amountKnob.setTooltip ("Amount. How much of each band's Ratio to use. 100 is as written. 0 is no compression.");
    downKnob.setTooltip ("Down amount. How much downward squeeze to use. 100 is the band Ratio. 0 is none.");
    upKnob.setTooltip ("Up amount. How much quiet sounds get lifted. 100 is full lift. 0 is none.");

    timeAt = std::make_unique<SliderAttachment> (state, DynParams::timeId(), timeKnob);
    amountAt = std::make_unique<SliderAttachment> (state, DynParams::amountId(), amountKnob);
    downAt = std::make_unique<SliderAttachment> (state, DynParams::downAmtId(), downKnob);
    upAt = std::make_unique<SliderAttachment> (state, DynParams::upAmtId(), upKnob);
    updateReadouts();
}

void DynFaceplate::GlobalPanel::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    timeKnob.setThemeColors (r);
    amountKnob.setThemeColors (r);
    downKnob.setThemeColors (r);
    upKnob.setThemeColors (r);
}

void DynFaceplate::GlobalPanel::updateReadouts()
{
    timeVal.setText (juce::String (timeKnob.getValue(), 0) + " %", juce::dontSendNotification);
    amountVal.setText (juce::String (amountKnob.getValue(), 0) + " %", juce::dontSendNotification);
    downVal.setText (juce::String (downKnob.getValue(), 0) + " %", juce::dontSendNotification);
    upVal.setText (juce::String (upKnob.getValue(), 0) + " %", juce::dontSendNotification);
}

void DynFaceplate::GlobalPanel::paint (juce::Graphics& g)
{
    auto box = getLocalBounds().toFloat().reduced (0.5f);
    const auto& pal = theme != nullptr ? theme->sharedColors : SharedColors {};
    g.setColour (pal.pluginBackground.brighter (0.08f));
    g.fillRoundedRectangle (box, 6.0f);
    g.setColour (juce::Colour::fromRGB (212, 176, 86).withAlpha (0.34f));
    g.drawRoundedRectangle (box, 6.0f, 1.2f);
}

void DynFaceplate::refreshLearnButtons()
{
    focusLearnButton.setToggleState (isBandLearnLit (getSelectedBand()),
                                     juce::dontSendNotification);
    focusLearnButton.setEnabled (splitLearn.canStartBand (getSelectedBand())
                                 || splitLearn.isLearning());
    for (auto* col : columns)
    {
        if (col == nullptr)
            continue;
        col->learn.setToggleState (isBandLearnLit (col->band), juce::dontSendNotification);
        col->learn.setEnabled (splitLearn.canStartBand (col->band) || splitLearn.isLearning());
    }
    if (onLearnChanged)
        onLearnChanged();
}

void DynFaceplate::toggleGlobalLearn()
{
    if (splitLearn.isLearning())
    {
        splitLearn.cancel();
        return;
    }
    splitLearn.start();
    refreshLearnButtons();
}

void DynFaceplate::showGlobalLearnMenu (juce::Component& target)
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addSectionHeader ("Learn");
    menu.addItem (1, "Splits", bandCount() >= 2, splitLearn.wantSplits);
    menu.addItem (2, "Down Thr", true, splitLearn.wantDownThr);
    menu.addItem (3, "Up Thr", true, splitLearn.wantUpThr);
    menu.addItem (4, "Clip Thr", true, splitLearn.wantClipThr);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
        [safe = juce::Component::SafePointer<DynFaceplate> (this)] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 1)
                safe->splitLearn.wantSplits = ! safe->splitLearn.wantSplits;
            else if (result == 2)
                safe->splitLearn.wantDownThr = ! safe->splitLearn.wantDownThr;
            else if (result == 3)
                safe->splitLearn.wantUpThr = ! safe->splitLearn.wantUpThr;
            else if (result == 4)
                safe->splitLearn.wantClipThr = ! safe->splitLearn.wantClipThr;
            if (! safe->splitLearn.wantSplits
                && ! safe->splitLearn.wantDownThr
                && ! safe->splitLearn.wantUpThr
                && ! safe->splitLearn.wantClipThr)
                safe->splitLearn.wantSplits = true;
            if (safe->onModeChanged)
                safe->onModeChanged();
        });
}

void DynFaceplate::toggleBandLearn (int band)
{
    band = juce::jlimit (0, DynParams::kMaxBands - 1, band);
    if (splitLearn.isLearning())
    {
        splitLearn.cancel();
        return;
    }
    splitLearn.start (band);
    refreshLearnButtons();
}

void DynFaceplate::showBandLearnMenu (int band, juce::Component& target)
{
    band = juce::jlimit (0, DynParams::kMaxBands - 1, band);
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addSectionHeader ("Learn Band " + juce::String (band + 1));
    menu.addItem (2, "Down Thr", true, splitLearn.wantBandDown[(size_t) band]);
    menu.addItem (3, "Up Thr", true, splitLearn.wantBandUp[(size_t) band]);
    menu.addItem (4, "Clip Thr", true, splitLearn.wantBandClip[(size_t) band]);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
        [safe = juce::Component::SafePointer<DynFaceplate> (this), band] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 2)
                safe->splitLearn.wantBandDown[(size_t) band] =
                    ! safe->splitLearn.wantBandDown[(size_t) band];
            else if (result == 3)
                safe->splitLearn.wantBandUp[(size_t) band] =
                    ! safe->splitLearn.wantBandUp[(size_t) band];
            else if (result == 4)
                safe->splitLearn.wantBandClip[(size_t) band] =
                    ! safe->splitLearn.wantBandClip[(size_t) band];
            if (! safe->splitLearn.wantBandDown[(size_t) band]
                && ! safe->splitLearn.wantBandUp[(size_t) band]
                && ! safe->splitLearn.wantBandClip[(size_t) band])
                safe->splitLearn.wantBandDown[(size_t) band] = true;
            safe->refreshLearnButtons();
        });
}

void DynFaceplate::showLearnMenu()
{
    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addSectionHeader ("Learn");
    menu.addItem (1, "Splits", bandCount() >= 2, splitLearn.wantSplits);
    menu.addItem (2, "Down Thr", true, splitLearn.wantDownThr);
    menu.addItem (3, "Up Thr", true, splitLearn.wantUpThr);
    menu.addItem (4, "Clip Thr", true, splitLearn.wantClipThr);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
        [safe = juce::Component::SafePointer<DynFaceplate> (this)] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (result == 1)
                safe->splitLearn.wantSplits = ! safe->splitLearn.wantSplits;
            else if (result == 2)
                safe->splitLearn.wantDownThr = ! safe->splitLearn.wantDownThr;
            else if (result == 3)
                safe->splitLearn.wantUpThr = ! safe->splitLearn.wantUpThr;
            else if (result == 4)
                safe->splitLearn.wantClipThr = ! safe->splitLearn.wantClipThr;
            if (! safe->splitLearn.wantSplits
                && ! safe->splitLearn.wantDownThr
                && ! safe->splitLearn.wantUpThr
                && ! safe->splitLearn.wantClipThr)
                safe->splitLearn.wantSplits = true;
            if (safe->onModeChanged)
                safe->onModeChanged();
        });
}

void DynFaceplate::showClipModeMenu (int band, juce::Component& target)
{
    band = juce::jlimit (0, DynParams::kMaxBands - 1, band);
    int mode = 0;
    if (auto* p = state.getRawParameterValue (DynParams::clipModeId (band)))
        mode = (int) std::lround (p->load());

    juce::PopupMenu menu;
    menu.setLookAndFeel (&ComboBoxLookAndFeel::sharedForPopupMenus());
    menu.addSectionHeader ("Clip");
    menu.addItem (1, "Soft", true, mode == 0);
    menu.addItem (2, "Hard", true, mode != 0);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&target),
        [safe = juce::Component::SafePointer<DynFaceplate> (this), band] (int result)
        {
            if (safe == nullptr || result <= 0)
                return;
            if (auto* p = dynamic_cast<juce::AudioParameterChoice*> (
                    safe->state.getParameter (DynParams::clipModeId (band))))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost (p->convertTo0to1 ((float) (result - 1)));
                p->endChangeGesture();
            }
            safe->updateFocusReadouts();
        });
}

void DynFaceplate::GlobalPanel::resized()
{
    auto r = getLocalBounds().reduced (8, 8);
    title.setBounds (r.removeFromTop (22));
    r.removeFromTop (6);

    const int capH = 16;
    const int valH = 15;
    const int slotH = r.getHeight() / 4;
    const int d = juce::jlimit (36, 64, slotH - capH - valH - 4);

    auto place = [&] (juce::Slider& k, juce::Label& cap, juce::Label& val)
    {
        auto slot = r.removeFromTop (slotH);
        k.setBounds (slot.removeFromTop (d).withSizeKeepingCentre (d, d));
        cap.setBounds (slot.removeFromTop (capH));
        val.setBounds (slot.removeFromTop (valH));
    };
    place (timeKnob, timeLab, timeVal);
    place (amountKnob, amountLab, amountVal);
    place (downKnob, downLab, downVal);
    place (upKnob, upLab, upVal);
}
