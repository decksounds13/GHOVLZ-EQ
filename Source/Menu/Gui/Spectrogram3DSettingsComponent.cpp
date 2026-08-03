#include "Spectrogram3DSettingsComponent.h"

#include "../../MainComponent.h"
#include "../../ScopeModules.h"

namespace
{
    constexpr int kPadX = 16;
    constexpr int kPadY = 10;
    constexpr int kLabelH = 18;
    constexpr int kSliderH = 24;
    constexpr int kRowGap = 6;
    constexpr int kLabelGap = 2;
    constexpr int kSectionGap = 10;
}

Spectrogram3DSettingsComponent::Content::Content (SharedResources& resources,
                                                  juce::AudioProcessorValueTreeState& state,
                                                  ColourRampBank& ramps)
    : sharedResources (resources),
      treeState (state),
      colourRamps (ramps),
      gradientEditor (resources, GradientStripEditor::ModeFamily::intensity, &ramps.getPresets())
{
    comboLookAndFeel.setThemeColors (&sharedResources);

    titleLabel.setText ("Spectrogram 3D", juce::dontSendNotification);
    titleLabel.setFont (juce::FontOptions().withName ("Lato Black").withHeight (20.0f));
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (titleLabel);

    styleToggle (enable3DToggle);
    enable3DToggle.setButtonText ("Enable 3D (expanded / Scope module)");
    enable3DToggle.setTooltip (
        "Expanded: show 3D over Spec. Scope: toggles the independent Spectrogram 3D module "
        "(2D Spectrograph stays available separately).");
    enable3DToggle.onClick = [this] { applyStructureControlsToMain(); };
    addAndMakeVisible (enable3DToggle);

    styleToggle (enhancedFreq3DToggle);
    enhancedFreq3DToggle.setTooltip (
        "3D heightfield: same enhanced-frequency analysis as the 2D toggle, but independent. "
        "When 2D and 3D disagree, both classic and enhanced columns are computed (extra CPU). "
        "Strength / LF Detail / Crossover are shared.");
    addAndMakeVisible (enhancedFreq3DToggle);
    enhancedFreq3DAttachment = std::make_unique<ButtonAttachment> (
        treeState, "SPEC_ENHANCED_FREQ_3D_ID", enhancedFreq3DToggle);

    meshQualityLabel.setText ("Mesh Quality", juce::dontSendNotification);
    styleCombo (meshQualityCombo);
    meshQualityCombo.addItem ("Low", 1);
    meshQualityCombo.addItem ("Medium", 2);
    meshQualityCombo.addItem ("High", 3);
    meshQualityCombo.addItem ("Ultra", 4);
    meshQualityCombo.onChange = [this] { applyStructureControlsToMain(); };
    addAndMakeVisible (meshQualityLabel);
    addAndMakeVisible (meshQualityCombo);

    meshHeightLabel.setText ("Mesh Height", juce::dontSendNotification);
    styleSlider (meshHeightSlider);
    meshHeightSlider.setRange (Spectrogram3DComponent::kMinMeshHeight,
                               Spectrogram3DComponent::kMaxMeshHeight,
                               0.01);
    meshHeightSlider.setValue (Spectrogram3DComponent::kDefaultMeshHeight, juce::dontSendNotification);
    meshHeightSlider.onValueChange = [this] { applyStructureControlsToMain(); };
    meshHeightSlider.setTooltip ("Peak height of the 3D spectrogram mesh (world units).");
    addAndMakeVisible (meshHeightLabel);
    addAndMakeVisible (meshHeightSlider);

    msaaLabel.setText ("Antialiasing (MSAA)", juce::dontSendNotification);
    styleCombo (msaaCombo);
    msaaCombo.addItem ("Off", 1);
    msaaCombo.addItem ("4x", 2);
    msaaCombo.addItem ("8x", 3);
    msaaCombo.addItem ("16x", 4);
    msaaCombo.setSelectedId (2, juce::dontSendNotification); // 4x default
    msaaCombo.setTooltip ("Multisample antialiasing for the 3D spectrogram (4x default). "
                          "Higher levels are clamped to what the GPU supports.");
    msaaCombo.onChange = [this] { applyStructureControlsToMain(); };
    addAndMakeVisible (msaaLabel);
    addAndMakeVisible (msaaCombo);

    styleToggle (transparentBgToggle);
    transparentBgToggle.setTooltip (
        "Composites the 3D view over the EQ via offscreen OpenGL (FBO to image to paint). "
        "Nested OpenGL HWND transparency is not available on Windows/Direct2D.");
    transparentBgToggle.onClick = [this] { applyStructureControlsToMain(); };
    addAndMakeVisible (transparentBgToggle);

    styleToggle (reverseFreqAxisToggle);
    reverseFreqAxisToggle.setTooltip (
        "Flip the frequency axis on the 3D floor grid (bass at front vs back).");
    reverseFreqAxisToggle.onClick = [this] { applyStructureControlsToMain(); };
    addAndMakeVisible (reverseFreqAxisToggle);

    styleSaveDefaultButton (resetCameraButton);
    resetCameraButton.setButtonText ("Reset 3D Camera");
    resetCameraButton.onClick = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->resetSpec3DCamera();
    };
    addAndMakeVisible (resetCameraButton);

    lookLabel.setText ("Look (all off by default)", juce::dontSendNotification);
    styleLabel (lookLabel);
    lookLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (lookLabel);

    auto setupLookToggle = [this] (juce::ToggleButton& t, const juce::String& tip)
    {
        styleToggle (t);
        t.setTooltip (tip);
        t.onClick = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (t);
    };
    auto setupLookSlider = [this] (juce::Label& lab, juce::Slider& s,
                                   double minV, double maxV, double step, const juce::String& tip)
    {
        styleLabel (lab);
        styleSlider (s);
        s.setRange (minV, maxV, step);
        s.setTooltip (tip);
        s.onValueChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (lab);
        addAndMakeVisible (s);
    };

    setupLookToggle (lightingToggle, "Directional lighting with normals / specular / rim. Off = flat colours.");
    lightingAmountLabel.setText ("Lighting Amount", juce::dontSendNotification);
    setupLookSlider (lightingAmountLabel, lightingAmountSlider, 0.0, 1.0, 0.01, "How strongly lighting reshapes the ramp colours.");
    lightingAmountSlider.setValue (0.70, juce::dontSendNotification);
    lightAzimuthLabel.setText ("Light Azimuth (deg)", juce::dontSendNotification);
    setupLookSlider (lightAzimuthLabel, lightAzimuthSlider, -180.0, 180.0, 1.0, "Orbit the key light around the mesh (yaw).");
    lightAzimuthSlider.setValue (-40.0, juce::dontSendNotification);
    lightElevationLabel.setText ("Light Elevation (deg)", juce::dontSendNotification);
    setupLookSlider (lightElevationLabel, lightElevationSlider, 5.0, 89.0, 1.0, "Raise / lower the key light above the floor.");
    lightElevationSlider.setValue (55.0, juce::dontSendNotification);
    specularLabel.setText ("Specular", juce::dontSendNotification);
    setupLookSlider (specularLabel, specularSlider, 0.0, 1.0, 0.01,
                     "Specular intensity / F0 blend for the GGX highlight.");
    specularSlider.setValue (0.35, juce::dontSendNotification);
    roughnessLabel.setText ("Roughness", juce::dontSendNotification);
    setupLookSlider (roughnessLabel, roughnessSlider, 0.04, 1.0, 0.01,
                     "PBR microfacet roughness — low = sharp highlight, high = broad/dull.");
    roughnessSlider.setValue (0.45, juce::dontSendNotification);
    rimLabel.setText ("Rim Light", juce::dontSendNotification);
    setupLookSlider (rimLabel, rimSlider, 0.0, 1.0, 0.01, "View-dependent edge lift.");
    rimSlider.setValue (0.22, juce::dontSendNotification);

    setupLookToggle (contactShadowToggle, "Soft dark disc under the mesh on the floor plane.");
    contactShadowStrengthLabel.setText ("Contact Shadow Strength", juce::dontSendNotification);
    setupLookSlider (contactShadowStrengthLabel, contactShadowStrengthSlider, 0.0, 1.0, 0.01, "Darkness of the floor contact shadow.");
    contactShadowStrengthSlider.setValue (0.45, juce::dontSendNotification);

    setupLookToggle (selfShadowToggle,
                     "Heightfield self-shadowing toward the key light. Off by default "
                     "(useful when the floor is too dark for contact shadows).");
    selfShadowStrengthLabel.setText ("Self-Shadow Strength", juce::dontSendNotification);
    setupLookSlider (selfShadowStrengthLabel, selfShadowStrengthSlider, 0.0, 1.0, 0.01,
                     "How strongly occluded ridges darken.");
    selfShadowStrengthSlider.setValue (0.65, juce::dontSendNotification);

    setupLookToggle (ssaoToggle,
                     "Heightfield ambient occlusion in crevices (mesh shader). Off by default.");
    ssaoStrengthLabel.setText ("AO Strength", juce::dontSendNotification);
    setupLookSlider (ssaoStrengthLabel, ssaoStrengthSlider, 0.0, 2.0, 0.01,
                     "How dark occluded crevices become (0–2).");
    ssaoStrengthSlider.setValue (0.55, juce::dontSendNotification);
    ssaoRadiusLabel.setText ("AO Radius", juce::dontSendNotification);
    setupLookSlider (ssaoRadiusLabel, ssaoRadiusSlider, 0.25, 3.0, 0.05, "Sample radius for occlusion taps.");
    ssaoRadiusSlider.setValue (1.0, juce::dontSendNotification);

    setupLookToggle (bloomToggle, "Glow on hot peaks (soft FBO path). Off by default.");
    bloomStrengthLabel.setText ("Bloom Strength", juce::dontSendNotification);
    setupLookSlider (bloomStrengthLabel, bloomStrengthSlider, 0.0, 1.0, 0.01, "Additive glow intensity.");
    bloomStrengthSlider.setValue (0.45, juce::dontSendNotification);
    bloomThresholdLabel.setText ("Bloom Threshold", juce::dontSendNotification);
    setupLookSlider (bloomThresholdLabel, bloomThresholdSlider, 0.0, 1.0, 0.01, "Luminance gate before glow.");
    bloomThresholdSlider.setValue (0.62, juce::dontSendNotification);

    gradientLabel.setText ("Custom Gradient (3D)", juce::dontSendNotification);
    styleLabel (gradientLabel);
    gradientLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (gradientLabel);

    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrogram3D));
    gradientEditor.onRampChanged = [this] { colourRamps.notifyEdited(); };
    gradientEditor.onRampPreview = [this] { colourRamps.notifyPreview(); };
    gradientEditor.onSamplePath = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->beginRampSamplingForTarget (ColourRampBank::Target::spectrogram3D);
    };
    gradientEditor.onPreferredHeightChanged = [this]
    {
        if (auto* parent = findParentComponentOfClass<Spectrogram3DSettingsComponent>())
            parent->resized();
        else
            resized();
    };
    addAndMakeVisible (gradientEditor);

    juce::Timer::callAfterDelay (0, [safe = juce::Component::SafePointer<Content> (this)]
    {
        if (safe != nullptr)
            safe->syncControlsFromMain();
    });

    styleLabel (titleLabel);
    styleLabel (meshQualityLabel);
    styleLabel (meshHeightLabel);
    styleLabel (msaaLabel);
}

Spectrogram3DSettingsComponent::Content::~Content()
{
    meshQualityCombo.setLookAndFeel (nullptr);
    msaaCombo.setLookAndFeel (nullptr);
}

void Spectrogram3DSettingsComponent::Content::styleSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 20);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha (0.55f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::goldenrod);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::whitesmoke.withAlpha (0.2f));
}

void Spectrogram3DSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setColour (juce::Label::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
}

void Spectrogram3DSettingsComponent::Content::styleToggle (juce::ToggleButton& toggle)
{
    toggle.setClickingTogglesState (true);
    toggle.setColour (juce::ToggleButton::textColourId, sharedResources.sharedColors.menuLabelTextColor1);
    toggle.setColour (juce::ToggleButton::tickColourId, juce::Colours::goldenrod);
    toggle.setColour (juce::ToggleButton::tickDisabledColourId, juce::Colours::darkgrey);
}

void Spectrogram3DSettingsComponent::Content::styleCombo (juce::ComboBox& combo)
{
    comboLookAndFeel.setThemeColors (&sharedResources);
    combo.setLookAndFeel (&comboLookAndFeel);
    combo.setColour (juce::ComboBox::textColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
}

void Spectrogram3DSettingsComponent::Content::styleSaveDefaultButton (juce::TextButton& button)
{
    button.setClickingTogglesState (false);
    button.setColour (juce::TextButton::buttonColourId, juce::Colours::black.withAlpha (0.35f));
    button.setColour (juce::TextButton::buttonOnColourId, juce::Colours::darkgoldenrod.withAlpha (0.75f));
    button.setColour (juce::TextButton::textColourOffId, juce::Colours::whitesmoke.withAlpha (0.9f));
    button.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
}

void Spectrogram3DSettingsComponent::Content::layoutSliderRow (juce::Rectangle<int>& area,
                                                               juce::Label& label,
                                                               juce::Slider& slider)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    slider.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (420, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void Spectrogram3DSettingsComponent::Content::layoutComboRow (juce::Rectangle<int>& area,
                                                              juce::Label& label,
                                                              juce::ComboBox& combo)
{
    label.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    combo.setBounds (area.removeFromTop (kSliderH).removeFromLeft (juce::jmin (220, area.getWidth())));
    area.removeFromTop (kRowGap);
}

void Spectrogram3DSettingsComponent::Content::layoutToggle (juce::Rectangle<int>& area,
                                                            juce::ToggleButton& toggle)
{
    toggle.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (320, area.getWidth())));
    area.removeFromTop (6);
}

void Spectrogram3DSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrogram3D));
    gradientEditor.repaint();
}

void Spectrogram3DSettingsComponent::Content::syncControlsFromMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    enable3DToggle.setToggleState (
        main->isScopeMode() ? main->isScopeModuleEnabled (ScopeModuleId::spectrogram3D)
                            : main->isSpec3DMode(),
        juce::dontSendNotification);
    const auto q = main->getSpec3DMeshQuality();
    meshQualityCombo.setSelectedId (
        q == Spectrogram3DComponent::MeshQuality::low ? 1
            : (q == Spectrogram3DComponent::MeshQuality::high ? 3
               : (q == Spectrogram3DComponent::MeshQuality::ultra ? 4 : 2)),
        juce::dontSendNotification);
    {
        const auto msaa = main->getSpec3DMsaaLevel();
        const int id = msaa == Spectrogram3DComponent::MsaaLevel::off ? 1
                     : (msaa == Spectrogram3DComponent::MsaaLevel::x8 ? 3
                        : (msaa == Spectrogram3DComponent::MsaaLevel::x16 ? 4 : 2));
        msaaCombo.setSelectedId (id, juce::dontSendNotification);
    }
    transparentBgToggle.setToggleState (main->isSpec3DTransparentBackground(), juce::dontSendNotification);
    reverseFreqAxisToggle.setToggleState (main->isSpec3DReverseFrequencyAxis(), juce::dontSendNotification);
    meshHeightSlider.setValue (main->getSpec3DMeshHeight(), juce::dontSendNotification);

    lightingToggle.setToggleState (main->isSpec3DLightingEnabled(), juce::dontSendNotification);
    lightingAmountSlider.setValue (main->getSpec3DLightingAmount(), juce::dontSendNotification);
    lightAzimuthSlider.setValue (main->getSpec3DLightAzimuthDeg(), juce::dontSendNotification);
    lightElevationSlider.setValue (main->getSpec3DLightElevationDeg(), juce::dontSendNotification);
    specularSlider.setValue (main->getSpec3DSpecularAmount(), juce::dontSendNotification);
    roughnessSlider.setValue (main->getSpec3DRoughnessAmount(), juce::dontSendNotification);
    rimSlider.setValue (main->getSpec3DRimAmount(), juce::dontSendNotification);
    contactShadowToggle.setToggleState (main->isSpec3DContactShadowEnabled(), juce::dontSendNotification);
    contactShadowStrengthSlider.setValue (main->getSpec3DContactShadowStrength(), juce::dontSendNotification);
    selfShadowToggle.setToggleState (main->isSpec3DSelfShadowEnabled(), juce::dontSendNotification);
    selfShadowStrengthSlider.setValue (main->getSpec3DSelfShadowStrength(), juce::dontSendNotification);
    ssaoToggle.setToggleState (main->isSpec3DSsaoEnabled(), juce::dontSendNotification);
    ssaoStrengthSlider.setValue (main->getSpec3DSsaoStrength(), juce::dontSendNotification);
    ssaoRadiusSlider.setValue (main->getSpec3DSsaoRadius(), juce::dontSendNotification);
    bloomToggle.setToggleState (main->isSpec3DBloomEnabled(), juce::dontSendNotification);
    bloomStrengthSlider.setValue (main->getSpec3DBloomStrength(), juce::dontSendNotification);
    bloomThresholdSlider.setValue (main->getSpec3DBloomThreshold(), juce::dontSendNotification);
}

void Spectrogram3DSettingsComponent::Content::applyStructureControlsToMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    main->setSpec3DMode (enable3DToggle.getToggleState(), true);
    const int id = meshQualityCombo.getSelectedId();
    main->setSpec3DMeshQuality (
        id == 1 ? Spectrogram3DComponent::MeshQuality::low
                : (id == 3 ? Spectrogram3DComponent::MeshQuality::high
                           : (id == 4 ? Spectrogram3DComponent::MeshQuality::ultra
                                      : Spectrogram3DComponent::MeshQuality::medium)),
        true);
    {
        const int msaaId = msaaCombo.getSelectedId();
        const auto level = msaaId == 1 ? Spectrogram3DComponent::MsaaLevel::off
                         : (msaaId == 3 ? Spectrogram3DComponent::MsaaLevel::x8
                            : (msaaId == 4 ? Spectrogram3DComponent::MsaaLevel::x16
                                           : Spectrogram3DComponent::MsaaLevel::x4));
        main->setSpec3DMsaaLevel (level, true);
    }
    main->setSpec3DTransparentBackground (transparentBgToggle.getToggleState(), true);
    main->setSpec3DReverseFrequencyAxis (reverseFreqAxisToggle.getToggleState(), true);
    main->setSpec3DMeshHeight ((float) meshHeightSlider.getValue(), true);
}

void Spectrogram3DSettingsComponent::Content::applyLookControlsToMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    // Look-only: re-shade / re-light existing mesh history — never mode/quality/MSAA.
    main->setSpec3DLightingEnabled (lightingToggle.getToggleState(), true);
    main->setSpec3DLightingAmount ((float) lightingAmountSlider.getValue(), true);
    main->setSpec3DLightAzimuthDeg ((float) lightAzimuthSlider.getValue(), true);
    main->setSpec3DLightElevationDeg ((float) lightElevationSlider.getValue(), true);
    main->setSpec3DSpecularAmount ((float) specularSlider.getValue(), true);
    main->setSpec3DRoughnessAmount ((float) roughnessSlider.getValue(), true);
    main->setSpec3DRimAmount ((float) rimSlider.getValue(), true);
    main->setSpec3DContactShadowEnabled (contactShadowToggle.getToggleState(), true);
    main->setSpec3DContactShadowStrength ((float) contactShadowStrengthSlider.getValue(), true);
    main->setSpec3DSelfShadowEnabled (selfShadowToggle.getToggleState(), true);
    main->setSpec3DSelfShadowStrength ((float) selfShadowStrengthSlider.getValue(), true);
    main->setSpec3DSsaoEnabled (ssaoToggle.getToggleState(), true);
    main->setSpec3DSsaoStrength ((float) ssaoStrengthSlider.getValue(), true);
    main->setSpec3DSsaoRadius ((float) ssaoRadiusSlider.getValue(), true);
    main->setSpec3DBloomEnabled (bloomToggle.getToggleState(), true);
    main->setSpec3DBloomStrength ((float) bloomStrengthSlider.getValue(), true);
    main->setSpec3DBloomThreshold ((float) bloomThresholdSlider.getValue(), true);
}

void Spectrogram3DSettingsComponent::Content::applyControlsToMain()
{
    applyStructureControlsToMain();
    applyLookControlsToMain();
}

int Spectrogram3DSettingsComponent::Content::getPreferredHeight() const
{
    const int comboRows = 2; // mesh quality + MSAA
    const int baseSliderRows = 1;
    const int lookSliderRows = 13;
    const int toggles = 4 + 5; // base (no MSAA toggle) + look master toggles
    const int buttonRows = 1;

    return kPadY * 2
           + 24 + 8
           + comboRows * (kLabelH + kLabelGap + kSliderH + kRowGap)
           + baseSliderRows * (kLabelH + kLabelGap + kSliderH + kRowGap)
           + lookSliderRows * (kLabelH + kLabelGap + kSliderH + kRowGap)
           + toggles * (22 + 6)
           + buttonRows * (22 + 6)
           + kLabelH + kSectionGap // look section header
           + kLabelH + kLabelGap + gradientEditor.getPreferredHeight() + kRowGap;
}

void Spectrogram3DSettingsComponent::Content::resized()
{
    auto area = getLocalBounds().reduced (kPadX, kPadY);

    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    layoutToggle (area, enable3DToggle);
    layoutToggle (area, enhancedFreq3DToggle);
    layoutComboRow (area, meshQualityLabel, meshQualityCombo);
    layoutSliderRow (area, meshHeightLabel, meshHeightSlider);
    layoutComboRow (area, msaaLabel, msaaCombo);
    layoutToggle (area, transparentBgToggle);
    layoutToggle (area, reverseFreqAxisToggle);
    resetCameraButton.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (160, area.getWidth())));
    area.removeFromTop (kSectionGap);

    lookLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kRowGap);
    layoutToggle (area, lightingToggle);
    layoutSliderRow (area, lightingAmountLabel, lightingAmountSlider);
    layoutSliderRow (area, lightAzimuthLabel, lightAzimuthSlider);
    layoutSliderRow (area, lightElevationLabel, lightElevationSlider);
    layoutSliderRow (area, specularLabel, specularSlider);
    layoutSliderRow (area, roughnessLabel, roughnessSlider);
    layoutSliderRow (area, rimLabel, rimSlider);
    layoutToggle (area, contactShadowToggle);
    layoutSliderRow (area, contactShadowStrengthLabel, contactShadowStrengthSlider);
    layoutToggle (area, selfShadowToggle);
    layoutSliderRow (area, selfShadowStrengthLabel, selfShadowStrengthSlider);
    layoutToggle (area, ssaoToggle);
    layoutSliderRow (area, ssaoStrengthLabel, ssaoStrengthSlider);
    layoutSliderRow (area, ssaoRadiusLabel, ssaoRadiusSlider);
    layoutToggle (area, bloomToggle);
    layoutSliderRow (area, bloomStrengthLabel, bloomStrengthSlider);
    layoutSliderRow (area, bloomThresholdLabel, bloomThresholdSlider);
    area.removeFromTop (kSectionGap);

    gradientLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                  .removeFromLeft (juce::jmin (520, area.getWidth())));

    syncControlsFromMain();
}

Spectrogram3DSettingsComponent::Spectrogram3DSettingsComponent (SharedResources& resources,
                                                                  juce::AudioProcessorValueTreeState& state,
                                                                  ColourRampBank& ramps)
    : sharedResources (resources),
      colourRamps (ramps),
      content (resources, state, ramps)
{
    colourRamps.addChangeListener (this);
    addAndMakeVisible (content);
}

Spectrogram3DSettingsComponent::~Spectrogram3DSettingsComponent()
{
    colourRamps.removeChangeListener (this);
}

void Spectrogram3DSettingsComponent::changeListenerCallback (juce::ChangeBroadcaster*)
{
    content.syncGradientFromBank();
}

void Spectrogram3DSettingsComponent::paint (juce::Graphics& g)
{
    juce::ignoreUnused (g);
}

void Spectrogram3DSettingsComponent::resized()
{
    content.setBounds (0, 0, getWidth(), content.getPreferredHeight());
    content.resized();
}
