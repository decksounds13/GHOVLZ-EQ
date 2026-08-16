#include "LoudnessSettingsComponent.h"
#include "../../ModuleLookPresets.h"
#include "../Menu.h"

namespace
{
    constexpr int kPadX = 16;
    constexpr int kPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kRowH = 24;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;

    constexpr std::array<float, 4> kTargetLufs { -9.0f, -14.0f, -16.0f, -23.0f };
}

LoudnessSettingsComponent::Content::Content (SharedResources& resources,
                                             juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      treeState (state),
      meterSection (resources, "loudness.meter", "Meter", false)
{
    comboLookAndFeel.setThemeColors (&sharedResources);

    titleLabel.setText ("Loudness", juce::dontSendNotification);
    titleLabel.setFont (SharedResources::uiFont (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.onClick = [this] { saveAnalyserDefaults(); };
    addAndMakeVisible (saveDefaultButton);

    autoGainModeLabel.setText ("Auto Gain", juce::dontSendNotification);
    styleCombo (autoGainModeCombo);
    autoGainModeCombo.addItem ("RMS (match input)", 1);
    autoGainModeCombo.addItem ("LUFS (target)", 2);
    autoGainModeCombo.setTooltip (
        "RMS matches output loudness to the pre-EQ level. "
        "LUFS aims the output at the Target LUFS value below. "
        "Same choice as right-click on the Auto Gain button.");
    autoGainModeCombo.onChange = [this] { applyAutoGainModeFromCombo(); };
    addAndMakeVisible (autoGainModeLabel);
    addAndMakeVisible (autoGainModeCombo);
    syncAutoGainModeFromParam();

    targetLabel.setText ("Target LUFS", juce::dontSendNotification);
    styleCombo (targetCombo);
    targetCombo.addItem ("-9 LUFS", 1);
    targetCombo.addItem ("-14 LUFS", 2);
    targetCombo.addItem ("-16 LUFS", 3);
    targetCombo.addItem ("-23 LUFS", 4);
    targetCombo.onChange = [this] { applyTargetFromCombo(); };
    addAndMakeVisible (targetLabel);
    addAndMakeVisible (targetCombo);
    syncTargetComboFromParam();

    resetNoteLabel.setText ("Integrated loudness resets when playback stops or from the meter context menu.",
                            juce::dontSendNotification);
    resetNoteLabel.setFont (juce::FontOptions().withHeight (12.5f));
    resetNoteLabel.setJustificationType (juce::Justification::topLeft);
    resetNoteLabel.setColour (juce::Label::textColourId,
                              sharedResources.sharedColors.menuLabelTextColor1.withAlpha (0.75f));
    addAndMakeVisible (resetNoteLabel);

    styleLabel (titleLabel);
    styleLabel (autoGainModeLabel);
    styleLabel (targetLabel);

    wireSection (meterSection);
}

void LoudnessSettingsComponent::Content::wireSection (SettingsSection& section)
{
    addAndMakeVisible (section);
    section.onChanged = [this]
    {
        resized();
        if (auto* menu = findParentComponentOfClass<Menu>())
            menu->notifyContentHeightChanged();
    };
}

LoudnessSettingsComponent::Content::~Content()
{
    autoGainModeCombo.setLookAndFeel (nullptr);
    targetCombo.setLookAndFeel (nullptr);
}

void LoudnessSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (SharedResources::uiFont (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void LoudnessSettingsComponent::Content::styleCombo (juce::ComboBox& combo)
{
    combo.setLookAndFeel (&comboLookAndFeel);
}

void LoudnessSettingsComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void LoudnessSettingsComponent::Content::saveAnalyserDefaults()
{
    const bool ok = ModuleLookPresets::saveDefaultFromApvts (ModuleLookPresets::Kind::loudness, treeState);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void LoudnessSettingsComponent::Content::syncTargetComboFromParam()
{
    const float target = treeState.getRawParameterValue ("LOUDNESS_TARGET_ID") != nullptr
                             ? treeState.getRawParameterValue ("LOUDNESS_TARGET_ID")->load()
                             : -14.0f;
    int best = 2;
    float bestDist = 1000.0f;
    for (int i = 0; i < (int) kTargetLufs.size(); ++i)
    {
        const float d = std::abs (kTargetLufs[(size_t) i] - target);
        if (d < bestDist)
        {
            bestDist = d;
            best = i + 1;
        }
    }
    targetCombo.setSelectedId (best, juce::dontSendNotification);
}

void LoudnessSettingsComponent::Content::applyTargetFromCombo()
{
    const int id = targetCombo.getSelectedId();
    if (id <= 0 || id > (int) kTargetLufs.size())
        return;

    if (auto* p = dynamic_cast<juce::AudioParameterFloat*> (treeState.getParameter ("LOUDNESS_TARGET_ID")))
        p->setValueNotifyingHost (p->convertTo0to1 (kTargetLufs[(size_t) (id - 1)]));
}

void LoudnessSettingsComponent::Content::syncAutoGainModeFromParam()
{
    const bool aimLufs = treeState.getRawParameterValue ("targetLufsEnable") != nullptr
                         && treeState.getRawParameterValue ("targetLufsEnable")->load() > 0.5f;
    autoGainModeCombo.setSelectedId (aimLufs ? 2 : 1, juce::dontSendNotification);
}

void LoudnessSettingsComponent::Content::applyAutoGainModeFromCombo()
{
    const bool aimLufs = autoGainModeCombo.getSelectedId() == 2;
    if (auto* p = dynamic_cast<juce::AudioParameterBool*> (treeState.getParameter ("targetLufsEnable")))
    {
        if (p->get() != aimLufs)
            p->setValueNotifyingHost (aimLufs ? 1.0f : 0.0f);
    }
}

int LoudnessSettingsComponent::Content::getPreferredHeight() const
{
    const int comboBlock = kLabelH + kLabelGap + kRowH + kRowGap;
    return kPadY * 2 + 24 + 8
           + meterSection.heightFor (comboBlock * 2 + 44);
}

void LoudnessSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);
    auto titleRow = area.removeFromTop (24);
    saveDefaultButton.setBounds (titleRow.removeFromRight (108).withHeight (22).withY (titleRow.getY() + 1));
    titleLabel.setBounds (titleRow);
    area.removeFromTop (8);

    meterSection.applyVisible ({
        &autoGainModeLabel, &autoGainModeCombo, &targetLabel, &targetCombo, &resetNoteLabel });
    meterSection.placeHeader (area);
    if (meterSection.isOpen())
    {
        autoGainModeLabel.setBounds (area.removeFromTop (kLabelH));
        area.removeFromTop (kLabelGap);
        autoGainModeCombo.setBounds (area.removeFromTop (kRowH).removeFromLeft (juce::jmin (220, area.getWidth())));
        area.removeFromTop (kRowGap);
        targetLabel.setBounds (area.removeFromTop (kLabelH));
        area.removeFromTop (kLabelGap);
        targetCombo.setBounds (area.removeFromTop (kRowH).removeFromLeft (juce::jmin (220, area.getWidth())));
        area.removeFromTop (kRowGap);
        resetNoteLabel.setBounds (area.removeFromTop (44));
    }
}

LoudnessSettingsComponent::LoudnessSettingsComponent (SharedResources& resources,
                                                      juce::AudioProcessorValueTreeState& state)
    : sharedResources (resources),
      content (resources, state)
{
    addAndMakeVisible (content);
}

LoudnessSettingsComponent::~LoudnessSettingsComponent() = default;

void LoudnessSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void LoudnessSettingsComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
