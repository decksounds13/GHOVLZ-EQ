#include "LoudnessSettingsComponent.h"

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
      treeState (state)
{
    comboLookAndFeel.setThemeColors (&sharedResources);

    titleLabel.setText ("Loudness", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

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
    styleLabel (targetLabel);
}

LoudnessSettingsComponent::Content::~Content()
{
    targetCombo.setLookAndFeel (nullptr);
}

void LoudnessSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void LoudnessSettingsComponent::Content::styleCombo (juce::ComboBox& combo)
{
    combo.setLookAndFeel (&comboLookAndFeel);
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
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

int LoudnessSettingsComponent::Content::getPreferredHeight() const
{
    return kPadY * 2 + 24 + 8 + kLabelH + kLabelGap + kRowH + kRowGap + 44;
}

void LoudnessSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);
    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);
    targetLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    targetCombo.setBounds (area.removeFromTop (kRowH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kRowGap);
    resetNoteLabel.setBounds (area.removeFromTop (44));
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
