#include "ScopeLevelMeterModule.h"
#include "EqProcessor.h"

namespace
{
    constexpr int kTitleH = 14;
    constexpr int kChannelLabelH = 14;
    constexpr float kMeterFloorDb = -60.0f;
    constexpr float kMeterCeilDb = 6.0f;
    constexpr int kScaleW = 22;
    constexpr int kPeakHoldH = 16;
    constexpr int kMeterGap = 3;

    juce::String formatDbfs (float db)
    {
        if (db <= -99.0f)
            return juce::String ("-inf dBFS");
        return juce::String (juce::jmax (db, -99.9f), 1) + " dBFS";
    }

    float dbToNormY (float db) noexcept
    {
        return juce::jmap (juce::jlimit (kMeterFloorDb, kMeterCeilDb, db),
                           kMeterFloorDb, kMeterCeilDb, 0.0f, 1.0f);
    }

    void paintDbScale (juce::Graphics& g,
                       juce::Rectangle<float> area,
                       juce::Colour textCol)
    {
        static constexpr float ticks[] = { 6.0f, 0.0f, -6.0f, -12.0f, -24.0f, -36.0f, -48.0f, -60.0f };
        const float fontH = juce::jlimit (7.0f, 9.0f, area.getWidth() * 0.42f);
        g.setFont (juce::Font (juce::FontOptions (fontH)));
        g.setColour (textCol.withAlpha (0.55f));

        for (float db : ticks)
        {
            const float y = area.getBottom() - area.getHeight() * dbToNormY (db);
            const auto label = db > 0.0f ? ("+" + juce::String ((int) db))
                                         : juce::String ((int) db);
            g.drawText (label,
                        juce::Rectangle<float> (area.getX(), y - fontH * 0.5f, area.getWidth(), fontH),
                        juce::Justification::centred, false);
        }
    }
}

ScopeLevelMeterModule::ScopeLevelMeterModule (EqProcessor& p,
                                              juce::AudioProcessorValueTreeState& state,
                                              Tap defaultTap,
                                              const juce::String& moduleTitle)
    : processor (p),
      treeState (state),
      tap (defaultTap),
      title (moduleTitle),
      meterL ([this]() { return readPeakDb (0); },
             [this]() { return readRmsDb (0); },
             state, clipState, 0),
      meterR ([this]() { return readPeakDb (1); },
             [this]() { return readRmsDb (1); },
             state, clipState, 1)
{
    meterL.setTextChromeVisible (false);
    meterR.setTextChromeVisible (false);
    addAndMakeVisible (meterL);
    addAndMakeVisible (meterR);
    meterL.addMouseListener (this, false);
    meterR.addMouseListener (this, false);
    startTimerHz (30);
}

ScopeLevelMeterModule::~ScopeLevelMeterModule()
{
    stopTimer();
}

float ScopeLevelMeterModule::readPeakDb (int channel) const
{
    if (processor.isMeterMsMode())
        return tap == Tap::output ? processor.getPostProcessingMsPeakValue (channel)
                                  : processor.getInputMsPeakValue (channel);
    return tap == Tap::output ? processor.getPostProcessingPeakValue (channel)
                              : processor.getInputPeakValue (channel);
}

float ScopeLevelMeterModule::readRmsDb (int channel) const
{
    if (processor.isMeterMsMode())
        return tap == Tap::output ? processor.getPostProcessingMsRmsValue (channel)
                                  : processor.getInputMsRmsValue (channel);
    return tap == Tap::output ? processor.getPostProcessingRmsValue (channel)
                              : processor.getInputRmsValue (channel);
}

float ScopeLevelMeterModule::readTruePeakDb (int channel) const
{
    if (processor.isMeterMsMode())
        return tap == Tap::output ? processor.getPostProcessingMsTruePeakValue (channel)
                                  : processor.getInputMsTruePeakValue (channel);
    return tap == Tap::output ? processor.getPostProcessingTruePeakValue (channel)
                              : processor.getInputTruePeakValue (channel);
}

void ScopeLevelMeterModule::updateTruePeakBallistics()
{
    const float fallSec = [&]() -> float
    {
        if (auto* raw = treeState.getRawParameterValue ("METER_FALL_ID"))
            return juce::jmax (0.05f, raw->load());
        return 0.75f;
    }();

    const float dt = 1.0f / 30.0f;
    const float coeff = 1.0f - std::exp (-dt / fallSec);
    auto applyFall = [coeff] (float current, float target) -> float
    {
        if (target >= current)
            return target;
        return current + (target - current) * coeff;
    };

    displayedTruePeakL = applyFall (displayedTruePeakL, readTruePeakDb (0));
    displayedTruePeakR = applyFall (displayedTruePeakR, readTruePeakDb (1));
}

void ScopeLevelMeterModule::setTap (Tap t) noexcept
{
    tap = t;
    repaint();
}

void ScopeLevelMeterModule::setThemeColors (SharedResources* r) noexcept
{
    theme = r;
    meterL.setThemeColors (r);
    meterR.setThemeColors (r);
    repaint();
}

void ScopeLevelMeterModule::setTiledPresentation (bool shouldUseTiled) noexcept
{
    if (tiledPresentation == shouldUseTiled)
        return;
    tiledPresentation = shouldUseTiled;
    meterL.setTextChromeVisible (false);
    meterR.setTextChromeVisible (false);
    resized();
    repaint();
}

void ScopeLevelMeterModule::timerCallback()
{
    if (! isVisible())
        return;
    updateTruePeakBallistics();
    repaint();
}

int ScopeLevelMeterModule::computeReadoutWidth (int availableW) const
{
    const float numH = tiledPresentation ? 13.0f : 10.5f;
    auto numFont = juce::Font (juce::FontOptions (numH).withName ("Lato Black"));

    // Labels sit above values — only two value columns need horizontal room.
    const int valueColW = juce::jmax (54,
        (int) std::ceil (juce::GlyphArrangement::getStringWidth (numFont, "-00.0 dBFS")) + 4);
    const int natural = valueColW * 2 + 4;

    if (! tiledPresentation)
    {
        // Strip: leave ~3× the old meter block so bars can grow wide.
        constexpr int kMinMeterBlock = 120;
        return juce::jmin (natural, juce::jmax (64, availableW - kMinMeterBlock));
    }

    constexpr int kMinMeterBlock = 40;
    return juce::jmin (natural, juce::jmax (90, availableW - kMinMeterBlock));
}

void ScopeLevelMeterModule::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto bg = theme != nullptr ? theme->sharedColors.oscBackground
                                     : juce::Colour::fromRGB (18, 16, 14);
    g.setColour (bg);
    g.fillRoundedRectangle (bounds, 3.0f);

    const auto readoutColours = theme != nullptr ? theme->sharedColors.meterReadoutText
                                                 : juce::Colours::whitesmoke.withAlpha (0.92f);
    const auto textCol = readoutColours;
    juce::ignoreUnused (title);

    const float peakL = meterL.getDisplayedPeakDb();
    const float peakR = meterR.getDisplayedPeakDb();
    const float rmsL = meterL.getDisplayedRmsDb();
    const float rmsR = meterR.getDisplayedRmsDb();
    const float tpL = displayedTruePeakL;
    const float tpR = displayedTruePeakR;
    const float holdL = meterL.getPeakHoldDb();
    const float holdR = meterR.getPeakHoldDb();
    const juce::String chL = processor.isMeterMsMode() ? "M" : "L";
    const juce::String chR = processor.isMeterMsMode() ? "S" : "R";

    // Strip: hug the left edge; tiled keeps a small inset.
    const int edgePad = tiledPresentation ? 3 : 1;
    auto area = getLocalBounds().reduced (edgePad);
    const int titleBand = tiledPresentation ? kTitleH : 12;
    area.removeFromTop (titleBand);

    auto chLabelRow = area.removeFromBottom (kChannelLabelH);

    const int readoutW = computeReadoutWidth (area.getWidth());
    auto readoutCol = area.removeFromLeft (readoutW);
    area.removeFromLeft (tiledPresentation ? 4 : 2);
    auto meterCol = area;

    const float numH = juce::jlimit (9.0f, tiledPresentation ? 14.0f : 12.0f,
                                     (float) readoutCol.getHeight() * 0.10f);
    // Peak / RMS / T.P. name labels — 2× the previous size.
    const float labH = juce::jmax (15.0f, numH * 1.24f);
    auto numFont = juce::Font (juce::FontOptions (numH).withName ("Lato Black"));
    auto labFont = juce::Font (juce::FontOptions (labH));

    const int valueColW = juce::jmax (1, readoutCol.getWidth() / 2);
    const int chHeadH = juce::jmax (10, (int) labH + 1);
    const int nameH = juce::jmax (9, (int) labH);
    const int valueH = juce::jmax ((int) (numH + 2.0f), (int) numH + 1);
    const int metricBlockH = nameH + valueH;
    const int packedH = chHeadH + metricBlockH * 3;
    const int topPad = juce::jmax (0, (readoutCol.getHeight() - packedH) / 2);
    readoutCol.removeFromTop (topPad);

    // L / R above each value column.
    {
        auto header = readoutCol.removeFromTop (chHeadH);
        auto lHead = header.removeFromLeft (valueColW);
        auto rHead = header;
        g.setFont (labFont);
        g.setColour (textCol.withAlpha (0.75f));
        g.drawText (chL, lHead, juce::Justification::centred, false);
        g.drawText (chR, rHead, juce::Justification::centred, false);
    }

    // Peak / RMS / T.P. above the L/R value pair (no side label column — more X for meters).
    auto paintMetric = [&] (const juce::String& name, float vL, float vR)
    {
        auto nameRow = readoutCol.removeFromTop (nameH);
        g.setFont (labFont);
        g.setColour (textCol.withAlpha (0.65f));
        g.drawText (name, nameRow, juce::Justification::centred, false);

        auto valRow = readoutCol.removeFromTop (valueH);
        auto lVal = valRow.removeFromLeft (valueColW);
        auto rVal = valRow;
        g.setFont (numFont);
        g.setColour (textCol);
        g.drawText (formatDbfs (vL), lVal.reduced (1, 0), juce::Justification::centred, false);
        g.drawText (formatDbfs (vR), rVal.reduced (1, 0), juce::Justification::centred, false);
    };

    paintMetric ("Peak", peakL, peakR);
    paintMetric ("RMS", rmsL, rmsR);
    paintMetric ("T.P.", tpL, tpR);

    // Peak-hold above each meter
    auto holdRow = meterCol.removeFromTop (kPeakHoldH);
    const int meterPairW = meterCol.getWidth();
    const int barW = juce::jmax (1, (meterPairW - kScaleW - kMeterGap * 2) / 2);

    auto holdLArea = holdRow.removeFromLeft (barW);
    holdRow.removeFromLeft (kMeterGap);
    auto scaleHold = holdRow.removeFromLeft (kScaleW);
    holdRow.removeFromLeft (kMeterGap);
    auto holdRArea = holdRow.removeFromLeft (barW);

    const float holdFontH = juce::jlimit (8.0f, 11.0f, (float) kPeakHoldH - 2.0f);
    g.setFont (juce::Font (juce::FontOptions (holdFontH).withName ("Lato Black")));
    g.setColour (textCol.withAlpha (0.9f));
    g.drawText (formatDbfs (holdL),
                holdLArea.withSizeKeepingCentre (juce::jmax (holdLArea.getWidth(), 64), holdLArea.getHeight()),
                juce::Justification::centred, false);
    g.drawText (formatDbfs (holdR),
                holdRArea.withSizeKeepingCentre (juce::jmax (holdRArea.getWidth(), 64), holdRArea.getHeight()),
                juce::Justification::centred, false);

    g.setFont (juce::Font (juce::FontOptions (7.5f)));
    g.setColour (textCol.withAlpha (0.5f));
    g.drawText ("Hold", scaleHold, juce::Justification::centred, false);

    auto scaleArea = meterCol.withX (meterCol.getX() + barW + kMeterGap)
                             .withWidth (kScaleW)
                             .toFloat();
    paintDbScale (g, scaleArea, textCol);

    chLabelRow.removeFromLeft (readoutW + (tiledPresentation ? 4 : 2));
    auto chLArea = chLabelRow.removeFromLeft (barW);
    chLabelRow.removeFromLeft (kMeterGap);
    auto scaleTag = chLabelRow.removeFromLeft (kScaleW);
    chLabelRow.removeFromLeft (kMeterGap);
    auto chRArea = chLabelRow.removeFromLeft (barW);

    g.setFont (juce::Font (juce::FontOptions (juce::jmax (9.0f, (float) kChannelLabelH - 2.0f)).withStyle ("Bold")));
    g.setColour (textCol.withAlpha (0.75f));
    g.drawText (chL, chLArea, juce::Justification::centred, false);
    g.drawText (chR, chRArea, juce::Justification::centred, false);

    g.setFont (juce::Font (juce::FontOptions (7.0f)));
    g.setColour (textCol.withAlpha (0.5f));
    g.drawText ("dBFS", scaleTag, juce::Justification::centred, false);
}

void ScopeLevelMeterModule::resized()
{
    const int edgePad = tiledPresentation ? 3 : 1;
    auto area = getLocalBounds().reduced (edgePad);
    const int titleBand = tiledPresentation ? kTitleH : 12;
    area.removeFromTop (titleBand);
    area.removeFromBottom (kChannelLabelH);

    const int readoutW = computeReadoutWidth (area.getWidth());
    area.removeFromLeft (readoutW);
    area.removeFromLeft (tiledPresentation ? 4 : 2);
    area.removeFromTop (kPeakHoldH);

    const int barW = juce::jmax (1, (area.getWidth() - kScaleW - kMeterGap * 2) / 2);
    meterL.setBounds (area.removeFromLeft (barW));
    area.removeFromLeft (kMeterGap);
    area.removeFromLeft (kScaleW);
    area.removeFromLeft (kMeterGap);
    meterR.setBounds (area.removeFromLeft (barW));
}

void ScopeLevelMeterModule::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showTapMenu();
}

void ScopeLevelMeterModule::showTapMenu()
{
    if (onShowContextMenu != nullptr)
    {
        onShowContextMenu();
        return;
    }

    juce::PopupMenu menu;
    menu.addSectionHeader ("Level Meter Tap");
    menu.addItem (1, "Input", true, tap == Tap::input);
    menu.addItem (2, "Output", true, tap == Tap::output);
    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [safe = juce::Component::SafePointer<ScopeLevelMeterModule> (this)] (int r)
                        {
                            if (safe == nullptr || r <= 0)
                                return;
                            safe->setTap (r == 2 ? Tap::output : Tap::input);
                        });
}
