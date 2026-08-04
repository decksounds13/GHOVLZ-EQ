#include "Spectrogram3DSettingsComponent.h"

#include "../../MainComponent.h"
#include "../../ScopeModules.h"
#include "../Menu.h"

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

    titleLabel.setText ("3D Spectrogram", juce::dontSendNotification);
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
    meshQualityCombo.addItem ("Overkill", 5);
    meshQualityCombo.setTooltip ("Mesh density along time × frequency. Overkill is 512×448 (~230k verts).");
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

    freqMeshBiasLabel.setText ("HF Mesh Density", juce::dontSendNotification);
    styleSlider (freqMeshBiasSlider);
    freqMeshBiasSlider.setRange (0.0, 1.0, 0.01);
    freqMeshBiasSlider.setValue (0.0, juce::dontSendNotification);
    freqMeshBiasSlider.onValueChange = [this] { applyStructureControlsToMain(); };
    freqMeshBiasSlider.setTooltip (
        "Packs extra frequency quads toward the highs without thinning the lows. "
        "0 = uniform grid; 1 ≈ 2.7× frequency rows (biased to HF).");
    addAndMakeVisible (freqMeshBiasLabel);
    addAndMakeVisible (freqMeshBiasSlider);

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

    styleToggle (closedMeshToggle);
    closedMeshToggle.setTooltip (
        "Extrude border edges just under the 0-intensity floor and cap the bottom (solid volume). "
        "Off by default. Independent of SSS — when SSS is on it uses volume thickness if enabled.");
    closedMeshToggle.onClick = [this]
    {
        updateLookDevVisibility();
        applyStructureControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (closedMeshToggle);

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
        t.onClick = [this]
        {
            updateLookDevVisibility();
            applyLookControlsToMain();
            requestParentRelayout();
        };
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
                     "GGX specular lobe intensity (highlight strength).");
    specularSlider.setValue (0.35, juce::dontSendNotification);
    roughnessLabel.setText ("Roughness", juce::dontSendNotification);
    setupLookSlider (roughnessLabel, roughnessSlider, 0.04, 1.0, 0.01,
                     "PBR microfacet roughness — low = sharp highlight, high = broad/dull.");
    roughnessSlider.setValue (0.45, juce::dontSendNotification);
    metalnessLabel.setText ("Metalness", juce::dontSendNotification);
    setupLookSlider (metalnessLabel, metalnessSlider, 0.0, 1.0, 0.01,
                     "PBR metalness — 0 = dielectric, 1 = metal (tinted specular, no diffuse).");
    metalnessSlider.setValue (0.0, juce::dontSendNotification);
    rimLabel.setText ("Rim Light", juce::dontSendNotification);
    setupLookSlider (rimLabel, rimSlider, 0.0, 1.0, 0.01, "View-dependent edge lift.");
    rimSlider.setValue (0.22, juce::dontSendNotification);
    lightColRLabel.setText ("Light Color R", juce::dontSendNotification);
    setupLookSlider (lightColRLabel, lightColRSlider, 0.0, 1.0, 0.01, "Key light tint red.");
    lightColRSlider.setValue (1.0, juce::dontSendNotification);
    lightColGLabel.setText ("Light Color G", juce::dontSendNotification);
    setupLookSlider (lightColGLabel, lightColGSlider, 0.0, 1.0, 0.01, "Key light tint green.");
    lightColGSlider.setValue (1.0, juce::dontSendNotification);
    lightColBLabel.setText ("Light Color B", juce::dontSendNotification);
    setupLookSlider (lightColBLabel, lightColBSlider, 0.0, 1.0, 0.01, "Key light tint blue.");
    lightColBSlider.setValue (1.0, juce::dontSendNotification);
    rimColRLabel.setText ("Rim Color R", juce::dontSendNotification);
    setupLookSlider (rimColRLabel, rimColRSlider, 0.0, 1.0, 0.01, "Rim light tint red.");
    rimColRSlider.setValue (1.0, juce::dontSendNotification);
    rimColGLabel.setText ("Rim Color G", juce::dontSendNotification);
    setupLookSlider (rimColGLabel, rimColGSlider, 0.0, 1.0, 0.01, "Rim light tint green.");
    rimColGSlider.setValue (1.0, juce::dontSendNotification);
    rimColBLabel.setText ("Rim Color B", juce::dontSendNotification);
    setupLookSlider (rimColBLabel, rimColBSlider, 0.0, 1.0, 0.01, "Rim light tint blue.");
    rimColBSlider.setValue (1.0, juce::dontSendNotification);

    setupLookToggle (domeFillToggle,
                     "Hemisphere dome fill — sky/ground ambient into shadows (needs Lighting). "
                     "Off by default.");
    domeFillStrengthLabel.setText ("Dome Fill Strength", juce::dontSendNotification);
    setupLookSlider (domeFillStrengthLabel, domeFillStrengthSlider, 0.0, 1.0, 0.01,
                     "How strongly sky/ground ambient fills shadowed regions.");
    domeFillStrengthSlider.setValue (0.35, juce::dontSendNotification);
    domeSkyRLabel.setText ("Dome Sky R", juce::dontSendNotification);
    setupLookSlider (domeSkyRLabel, domeSkyRSlider, 0.0, 1.0, 0.01, "Sky hemisphere tint red.");
    domeSkyRSlider.setValue (juce::Colour (0xff7390bf).getFloatRed(), juce::dontSendNotification);
    domeSkyGLabel.setText ("Dome Sky G", juce::dontSendNotification);
    setupLookSlider (domeSkyGLabel, domeSkyGSlider, 0.0, 1.0, 0.01, "Sky hemisphere tint green.");
    domeSkyGSlider.setValue (juce::Colour (0xff7390bf).getFloatGreen(), juce::dontSendNotification);
    domeSkyBLabel.setText ("Dome Sky B", juce::dontSendNotification);
    setupLookSlider (domeSkyBLabel, domeSkyBSlider, 0.0, 1.0, 0.01, "Sky hemisphere tint blue.");
    domeSkyBSlider.setValue (juce::Colour (0xff7390bf).getFloatBlue(), juce::dontSendNotification);
    domeGroundRLabel.setText ("Dome Ground R", juce::dontSendNotification);
    setupLookSlider (domeGroundRLabel, domeGroundRSlider, 0.0, 1.0, 0.01, "Ground hemisphere tint red.");
    domeGroundRSlider.setValue (juce::Colour (0xff403328).getFloatRed(), juce::dontSendNotification);
    domeGroundGLabel.setText ("Dome Ground G", juce::dontSendNotification);
    setupLookSlider (domeGroundGLabel, domeGroundGSlider, 0.0, 1.0, 0.01, "Ground hemisphere tint green.");
    domeGroundGSlider.setValue (juce::Colour (0xff403328).getFloatGreen(), juce::dontSendNotification);
    domeGroundBLabel.setText ("Dome Ground B", juce::dontSendNotification);
    setupLookSlider (domeGroundBLabel, domeGroundBSlider, 0.0, 1.0, 0.01, "Ground hemisphere tint blue.");
    domeGroundBSlider.setValue (juce::Colour (0xff403328).getFloatBlue(), juce::dontSendNotification);

    setupLookToggle (ssgiToggle,
                     "Screen-space GI: short ray-march color bleed into shadows "
                     "(soft FBO path). Works best with Lighting. Off by default.");
    ssgiStrengthLabel.setText ("SSGI Strength", juce::dontSendNotification);
    setupLookSlider (ssgiStrengthLabel, ssgiStrengthSlider, 0.0, 1.0, 0.01, "Indirect bounce mix.");
    ssgiStrengthSlider.setValue (0.40, juce::dontSendNotification);
    ssgiRadiusLabel.setText ("SSGI Radius", juce::dontSendNotification);
    setupLookSlider (ssgiRadiusLabel, ssgiRadiusSlider, 0.0, 1.0, 0.01, "Screen-space gather distance.");
    ssgiRadiusSlider.setValue (0.45, juce::dontSendNotification);
    ssgiQualityLabel.setText ("SSGI Quality", juce::dontSendNotification);
    styleCombo (ssgiQualityCombo);
    ssgiQualityCombo.addItem ("Low", 1);
    ssgiQualityCombo.addItem ("Medium", 2);
    ssgiQualityCombo.addItem ("High", 3);
    ssgiQualityCombo.setSelectedId (2, juce::dontSendNotification);
    ssgiQualityCombo.setTooltip ("Sample density for screen-space GI gather.");
    ssgiQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (ssgiQualityLabel);
    addAndMakeVisible (ssgiQualityCombo);

    setupLookToggle (contactShadowToggle,
                     "Darkens low mesh regions nestled beside taller neighbours "
                     "(heightfield covers the floor, so a ground disc cannot show).");
    contactShadowStrengthLabel.setText ("Contact Shadow Strength", juce::dontSendNotification);
    setupLookSlider (contactShadowStrengthLabel, contactShadowStrengthSlider, 0.0, 1.0, 0.01,
                     "Strength of base / nestled contact darkening.");
    contactShadowStrengthSlider.setValue (0.45, juce::dontSendNotification);

    setupLookToggle (selfShadowToggle,
                     "Directional heightfield shadows toward the key light "
                     "(horizon + soft ray march). Uses light azimuth/elevation.");
    selfShadowStrengthLabel.setText ("Self-Shadow Strength", juce::dontSendNotification);
    setupLookSlider (selfShadowStrengthLabel, selfShadowStrengthSlider, 0.0, 2.0, 0.01,
                     "How strongly occluded ridges darken (0–2).");
    selfShadowStrengthSlider.setValue (0.85, juce::dontSendNotification);
    selfShadowBiasLabel.setText ("Shadow Bias", juce::dontSendNotification);
    setupLookSlider (selfShadowBiasLabel, selfShadowBiasSlider, 0.0, 1.0, 0.01,
                     "Raises the shadow ray origin to fight acne / self-intersection.");
    selfShadowBiasSlider.setValue (0.35, juce::dontSendNotification);
    selfShadowSoftnessLabel.setText ("Shadow Softness", juce::dontSendNotification);
    setupLookSlider (selfShadowSoftnessLabel, selfShadowSoftnessSlider, 0.0, 1.0, 0.01,
                     "Widens the terminator / penumbra (higher = softer edge).");
    selfShadowSoftnessSlider.setValue (0.85, juce::dontSendNotification);
    selfShadowQualityLabel.setText ("Shadow Quality", juce::dontSendNotification);
    styleCombo (selfShadowQualityCombo);
    selfShadowQualityCombo.addItem ("Low", 1);
    selfShadowQualityCombo.addItem ("Medium", 2);
    selfShadowQualityCombo.addItem ("High", 3);
    selfShadowQualityCombo.setSelectedId (2, juce::dontSendNotification);
    selfShadowQualityCombo.setTooltip ("Sample density for horizon + ray-march shadows.");
    selfShadowQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (selfShadowQualityLabel);
    addAndMakeVisible (selfShadowQualityCombo);

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

    setupLookToggle (dofToggle,
                     "Realtime post DOF (EEVEE / Marmoset Post Effect style): thin-lens "
                     "CoC + disc gather. Soft FBO path. Off by default.");
    dofFocusLabel.setText ("Focus Distance", juce::dontSendNotification);
    setupLookSlider (dofFocusLabel, dofFocusSlider,
                     Spectrogram3DComponent::kDofFocusMin,
                     Spectrogram3DComponent::kDofFocusMax,
                     0.01,
                     "Sharp plane distance from the camera (view units). "
                     "Ctrl+LMB (Cmd+LMB on Mac) on the mesh to set focus under the cursor.");
    dofFocusSlider.setValue (Spectrogram3DComponent::kDofFocusDefault, juce::dontSendNotification);
    dofApertureLabel.setText ("Aperture", juce::dontSendNotification);
    setupLookSlider (dofApertureLabel, dofApertureSlider, 0.0, 1.0, 0.01,
                     "Substance-style aperture openness — higher = shallower DOF (more blur). "
                     "Low values stay nearly sharp.");
    dofApertureSlider.setValue (Spectrogram3DComponent::kDofApertureDefault, juce::dontSendNotification);
    dofQualityLabel.setText ("DOF Quality", juce::dontSendNotification);
    styleCombo (dofQualityCombo);
    dofQualityCombo.addItem ("Low", 1);
    dofQualityCombo.addItem ("Medium", 2);
    dofQualityCombo.addItem ("High", 3);
    dofQualityCombo.setSelectedId (2, juce::dontSendNotification);
    dofQualityCombo.setTooltip ("Disc sample count / max bokeh size (8 / 16 / 24 taps).");
    dofQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (dofQualityLabel);
    addAndMakeVisible (dofQualityCombo);

    setupLookToggle (sssToggle,
                     "Subsurface scatter approx (needs Lighting). Uses volume thickness when "
                     "Closed mesh is on; otherwise heightfield ridge taps. Off by default.");
    sssStrengthLabel.setText ("SSS Strength", juce::dontSendNotification);
    setupLookSlider (sssStrengthLabel, sssStrengthSlider, 0.0, 1.0, 0.01, "Overall subsurface mix.");
    sssStrengthSlider.setValue (0.45, juce::dontSendNotification);
    sssWrapLabel.setText ("SSS Wrap", juce::dontSendNotification);
    setupLookSlider (sssWrapLabel, sssWrapSlider, 0.0, 1.0, 0.01, "Terminator bleed / scatter wrap.");
    sssWrapSlider.setValue (0.55, juce::dontSendNotification);
    sssTransmissionLabel.setText ("SSS Transmission", juce::dontSendNotification);
    setupLookSlider (sssTransmissionLabel, sssTransmissionSlider, 0.0, 1.0, 0.01,
                     "View-dependent backscatter through thin features.");
    sssTransmissionSlider.setValue (0.65, juce::dontSendNotification);
    sssTintRLabel.setText ("SSS Tint R", juce::dontSendNotification);
    setupLookSlider (sssTintRLabel, sssTintRSlider, 0.0, 1.0, 0.01, "Scatter tint red.");
    sssTintRSlider.setValue (0.91, juce::dontSendNotification);
    sssTintGLabel.setText ("SSS Tint G", juce::dontSendNotification);
    setupLookSlider (sssTintGLabel, sssTintGSlider, 0.0, 1.0, 0.01, "Scatter tint green.");
    sssTintGSlider.setValue (0.69, juce::dontSendNotification);
    sssTintBLabel.setText ("SSS Tint B", juce::dontSendNotification);
    setupLookSlider (sssTintBLabel, sssTintBSlider, 0.0, 1.0, 0.01, "Scatter tint blue.");
    sssTintBSlider.setValue (0.56, juce::dontSendNotification);

    sssRadiusLabel.setText ("SSS Radius", juce::dontSendNotification);
    setupLookSlider (sssRadiusLabel, sssRadiusSlider, 0.0, 1.0, 0.01,
                     "Open mesh: height-map tap distance for thin ridges.");
    sssRadiusSlider.setValue (0.40, juce::dontSendNotification);
    sssContrastLabel.setText ("SSS Contrast", juce::dontSendNotification);
    setupLookSlider (sssContrastLabel, sssContrastSlider, 0.0, 1.0, 0.01,
                     "Open mesh: how sharply only thin ridges transmit.");
    sssContrastSlider.setValue (0.50, juce::dontSendNotification);

    sssQualityLabel.setText ("SSS Quality", juce::dontSendNotification);
    styleCombo (sssQualityCombo);
    sssQualityCombo.addItem ("Low", 1);
    sssQualityCombo.addItem ("Medium", 2);
    sssQualityCombo.addItem ("High", 3);
    sssQualityCombo.setSelectedId (2, juce::dontSendNotification);
    sssQualityCombo.setTooltip ("Sample density for thickness taps.");
    sssQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (sssQualityLabel);
    addAndMakeVisible (sssQualityCombo);

    sssThickScaleLabel.setText ("SSS Thickness Scale", juce::dontSendNotification);
    setupLookSlider (sssThickScaleLabel, sssThickScaleSlider, 0.0, 1.0, 0.01,
                     "Closed mesh: maps volume depth to transmission.");
    sssThickScaleSlider.setValue (0.50, juce::dontSendNotification);
    sssMaxThickLabel.setText ("SSS Max Thickness", juce::dontSendNotification);
    setupLookSlider (sssMaxThickLabel, sssMaxThickSlider, 0.0, 1.0, 0.01,
                     "Closed mesh: above this optical depth, treat as opaque.");
    sssMaxThickSlider.setValue (0.70, juce::dontSendNotification);

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
        requestParentRelayout();
    };
    addAndMakeVisible (gradientEditor);

    juce::Timer::callAfterDelay (0, [safe = juce::Component::SafePointer<Content> (this)]
    {
        if (safe == nullptr)
            return;
        safe->syncControlsFromMain();
        safe->requestParentRelayout();
    });

    styleLabel (titleLabel);
    styleLabel (meshQualityLabel);
    styleLabel (meshHeightLabel);
    styleLabel (freqMeshBiasLabel);
    styleLabel (msaaLabel);
    styleLabel (selfShadowQualityLabel);
    styleLabel (sssQualityLabel);
    styleLabel (dofQualityLabel);
    styleLabel (ssgiQualityLabel);

    updateLookDevVisibility();
}

Spectrogram3DSettingsComponent::Content::~Content()
{
    meshQualityCombo.setLookAndFeel (nullptr);
    msaaCombo.setLookAndFeel (nullptr);
    selfShadowQualityCombo.setLookAndFeel (nullptr);
    sssQualityCombo.setLookAndFeel (nullptr);
    dofQualityCombo.setLookAndFeel (nullptr);
    ssgiQualityCombo.setLookAndFeel (nullptr);
}

void Spectrogram3DSettingsComponent::Content::setLookChildVisible (juce::Component& c, bool vis)
{
    c.setVisible (vis);
}

void Spectrogram3DSettingsComponent::Content::updateLookDevVisibility()
{
    const bool lit = lightingToggle.getToggleState();
    setLookChildVisible (lightingAmountLabel, lit);
    setLookChildVisible (lightingAmountSlider, lit);
    setLookChildVisible (lightAzimuthLabel, lit);
    setLookChildVisible (lightAzimuthSlider, lit);
    setLookChildVisible (lightElevationLabel, lit);
    setLookChildVisible (lightElevationSlider, lit);
    setLookChildVisible (specularLabel, lit);
    setLookChildVisible (specularSlider, lit);
    setLookChildVisible (roughnessLabel, lit);
    setLookChildVisible (roughnessSlider, lit);
    setLookChildVisible (metalnessLabel, lit);
    setLookChildVisible (metalnessSlider, lit);
    setLookChildVisible (rimLabel, lit);
    setLookChildVisible (rimSlider, lit);
    setLookChildVisible (lightColRLabel, lit);
    setLookChildVisible (lightColRSlider, lit);
    setLookChildVisible (lightColGLabel, lit);
    setLookChildVisible (lightColGSlider, lit);
    setLookChildVisible (lightColBLabel, lit);
    setLookChildVisible (lightColBSlider, lit);
    setLookChildVisible (rimColRLabel, lit);
    setLookChildVisible (rimColRSlider, lit);
    setLookChildVisible (rimColGLabel, lit);
    setLookChildVisible (rimColGSlider, lit);
    setLookChildVisible (rimColBLabel, lit);
    setLookChildVisible (rimColBSlider, lit);

    const bool dome = lit && domeFillToggle.getToggleState();
    setLookChildVisible (domeFillStrengthLabel, dome);
    setLookChildVisible (domeFillStrengthSlider, dome);
    setLookChildVisible (domeSkyRLabel, dome);
    setLookChildVisible (domeSkyRSlider, dome);
    setLookChildVisible (domeSkyGLabel, dome);
    setLookChildVisible (domeSkyGSlider, dome);
    setLookChildVisible (domeSkyBLabel, dome);
    setLookChildVisible (domeSkyBSlider, dome);
    setLookChildVisible (domeGroundRLabel, dome);
    setLookChildVisible (domeGroundRSlider, dome);
    setLookChildVisible (domeGroundGLabel, dome);
    setLookChildVisible (domeGroundGSlider, dome);
    setLookChildVisible (domeGroundBLabel, dome);
    setLookChildVisible (domeGroundBSlider, dome);

    const bool ssgi = ssgiToggle.getToggleState();
    setLookChildVisible (ssgiStrengthLabel, ssgi);
    setLookChildVisible (ssgiStrengthSlider, ssgi);
    setLookChildVisible (ssgiRadiusLabel, ssgi);
    setLookChildVisible (ssgiRadiusSlider, ssgi);
    setLookChildVisible (ssgiQualityLabel, ssgi);
    setLookChildVisible (ssgiQualityCombo, ssgi);

    const bool contact = contactShadowToggle.getToggleState();
    setLookChildVisible (contactShadowStrengthLabel, contact);
    setLookChildVisible (contactShadowStrengthSlider, contact);

    const bool selfSh = selfShadowToggle.getToggleState();
    setLookChildVisible (selfShadowStrengthLabel, selfSh);
    setLookChildVisible (selfShadowStrengthSlider, selfSh);
    setLookChildVisible (selfShadowBiasLabel, selfSh);
    setLookChildVisible (selfShadowBiasSlider, selfSh);
    setLookChildVisible (selfShadowSoftnessLabel, selfSh);
    setLookChildVisible (selfShadowSoftnessSlider, selfSh);
    setLookChildVisible (selfShadowQualityLabel, selfSh);
    setLookChildVisible (selfShadowQualityCombo, selfSh);

    const bool ao = ssaoToggle.getToggleState();
    setLookChildVisible (ssaoStrengthLabel, ao);
    setLookChildVisible (ssaoStrengthSlider, ao);
    setLookChildVisible (ssaoRadiusLabel, ao);
    setLookChildVisible (ssaoRadiusSlider, ao);

    const bool bloom = bloomToggle.getToggleState();
    setLookChildVisible (bloomStrengthLabel, bloom);
    setLookChildVisible (bloomStrengthSlider, bloom);
    setLookChildVisible (bloomThresholdLabel, bloom);
    setLookChildVisible (bloomThresholdSlider, bloom);

    const bool dof = dofToggle.getToggleState();
    setLookChildVisible (dofFocusLabel, dof);
    setLookChildVisible (dofFocusSlider, dof);
    setLookChildVisible (dofApertureLabel, dof);
    setLookChildVisible (dofApertureSlider, dof);
    setLookChildVisible (dofQualityLabel, dof);
    setLookChildVisible (dofQualityCombo, dof);

    const bool closed = closedMeshToggle.getToggleState();
    const bool sssOn = sssToggle.getToggleState();
    setLookChildVisible (sssStrengthLabel, sssOn);
    setLookChildVisible (sssStrengthSlider, sssOn);
    setLookChildVisible (sssWrapLabel, sssOn);
    setLookChildVisible (sssWrapSlider, sssOn);
    setLookChildVisible (sssTransmissionLabel, sssOn);
    setLookChildVisible (sssTransmissionSlider, sssOn);
    setLookChildVisible (sssTintRLabel, sssOn);
    setLookChildVisible (sssTintRSlider, sssOn);
    setLookChildVisible (sssTintGLabel, sssOn);
    setLookChildVisible (sssTintGSlider, sssOn);
    setLookChildVisible (sssTintBLabel, sssOn);
    setLookChildVisible (sssTintBSlider, sssOn);
    setLookChildVisible (sssQualityLabel, sssOn);
    setLookChildVisible (sssQualityCombo, sssOn);
    // Open-mesh SSS taps vs closed-mesh volume thickness controls.
    setLookChildVisible (sssRadiusLabel, sssOn && ! closed);
    setLookChildVisible (sssRadiusSlider, sssOn && ! closed);
    setLookChildVisible (sssContrastLabel, sssOn && ! closed);
    setLookChildVisible (sssContrastSlider, sssOn && ! closed);
    setLookChildVisible (sssThickScaleLabel, sssOn && closed);
    setLookChildVisible (sssThickScaleSlider, sssOn && closed);
    setLookChildVisible (sssMaxThickLabel, sssOn && closed);
    setLookChildVisible (sssMaxThickSlider, sssOn && closed);
}

void Spectrogram3DSettingsComponent::Content::requestParentRelayout()
{
    if (auto* parent = findParentComponentOfClass<Spectrogram3DSettingsComponent>())
        parent->resized();
    else
        resized();

    // Viewport scroll range lives on Menu::contentPanel — refresh it when Look rows grow/shrink.
    if (auto* menu = findParentComponentOfClass<Menu>())
        menu->notifyContentHeightChanged();
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
               : (q == Spectrogram3DComponent::MeshQuality::ultra ? 4
                  : (q == Spectrogram3DComponent::MeshQuality::overkill ? 5 : 2))),
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
    closedMeshToggle.setToggleState (main->isSpec3DClosedMeshEnabled(), juce::dontSendNotification);
    meshHeightSlider.setValue (main->getSpec3DMeshHeight(), juce::dontSendNotification);
    freqMeshBiasSlider.setValue (main->getSpec3DFreqMeshBias(), juce::dontSendNotification);

    lightingToggle.setToggleState (main->isSpec3DLightingEnabled(), juce::dontSendNotification);
    lightingAmountSlider.setValue (main->getSpec3DLightingAmount(), juce::dontSendNotification);
    lightAzimuthSlider.setValue (main->getSpec3DLightAzimuthDeg(), juce::dontSendNotification);
    lightElevationSlider.setValue (main->getSpec3DLightElevationDeg(), juce::dontSendNotification);
    specularSlider.setValue (main->getSpec3DSpecularAmount(), juce::dontSendNotification);
    roughnessSlider.setValue (main->getSpec3DRoughnessAmount(), juce::dontSendNotification);
    metalnessSlider.setValue (main->getSpec3DMetalnessAmount(), juce::dontSendNotification);
    rimSlider.setValue (main->getSpec3DRimAmount(), juce::dontSendNotification);
    {
        const auto lc = main->getSpec3DLightColour();
        lightColRSlider.setValue (lc.getFloatRed(), juce::dontSendNotification);
        lightColGSlider.setValue (lc.getFloatGreen(), juce::dontSendNotification);
        lightColBSlider.setValue (lc.getFloatBlue(), juce::dontSendNotification);
    }
    {
        const auto rc = main->getSpec3DRimColour();
        rimColRSlider.setValue (rc.getFloatRed(), juce::dontSendNotification);
        rimColGSlider.setValue (rc.getFloatGreen(), juce::dontSendNotification);
        rimColBSlider.setValue (rc.getFloatBlue(), juce::dontSendNotification);
    }
    domeFillToggle.setToggleState (main->isSpec3DDomeFillEnabled(), juce::dontSendNotification);
    domeFillStrengthSlider.setValue (main->getSpec3DDomeFillStrength(), juce::dontSendNotification);
    {
        const auto sky = main->getSpec3DDomeSkyColour();
        domeSkyRSlider.setValue (sky.getFloatRed(), juce::dontSendNotification);
        domeSkyGSlider.setValue (sky.getFloatGreen(), juce::dontSendNotification);
        domeSkyBSlider.setValue (sky.getFloatBlue(), juce::dontSendNotification);
    }
    {
        const auto ground = main->getSpec3DDomeGroundColour();
        domeGroundRSlider.setValue (ground.getFloatRed(), juce::dontSendNotification);
        domeGroundGSlider.setValue (ground.getFloatGreen(), juce::dontSendNotification);
        domeGroundBSlider.setValue (ground.getFloatBlue(), juce::dontSendNotification);
    }
    ssgiToggle.setToggleState (main->isSpec3DSsgiEnabled(), juce::dontSendNotification);
    ssgiStrengthSlider.setValue (main->getSpec3DSsgiStrength(), juce::dontSendNotification);
    ssgiRadiusSlider.setValue (main->getSpec3DSsgiRadius(), juce::dontSendNotification);
    {
        const auto sq = main->getSpec3DSsgiQuality();
        ssgiQualityCombo.setSelectedId (
            sq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (sq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 2),
            juce::dontSendNotification);
    }
    contactShadowToggle.setToggleState (main->isSpec3DContactShadowEnabled(), juce::dontSendNotification);
    contactShadowStrengthSlider.setValue (main->getSpec3DContactShadowStrength(), juce::dontSendNotification);
    selfShadowToggle.setToggleState (main->isSpec3DSelfShadowEnabled(), juce::dontSendNotification);
    selfShadowStrengthSlider.setValue (main->getSpec3DSelfShadowStrength(), juce::dontSendNotification);
    selfShadowBiasSlider.setValue (main->getSpec3DSelfShadowBias(), juce::dontSendNotification);
    selfShadowSoftnessSlider.setValue (main->getSpec3DSelfShadowSoftness(), juce::dontSendNotification);
    {
        const auto sq = main->getSpec3DSelfShadowQuality();
        selfShadowQualityCombo.setSelectedId (
            sq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (sq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 2),
            juce::dontSendNotification);
    }
    ssaoToggle.setToggleState (main->isSpec3DSsaoEnabled(), juce::dontSendNotification);
    ssaoStrengthSlider.setValue (main->getSpec3DSsaoStrength(), juce::dontSendNotification);
    ssaoRadiusSlider.setValue (main->getSpec3DSsaoRadius(), juce::dontSendNotification);
    bloomToggle.setToggleState (main->isSpec3DBloomEnabled(), juce::dontSendNotification);
    bloomStrengthSlider.setValue (main->getSpec3DBloomStrength(), juce::dontSendNotification);
    bloomThresholdSlider.setValue (main->getSpec3DBloomThreshold(), juce::dontSendNotification);
    dofToggle.setToggleState (main->isSpec3DDofEnabled(), juce::dontSendNotification);
    dofFocusSlider.setValue (main->getSpec3DDofFocusDistance(), juce::dontSendNotification);
    dofApertureSlider.setValue (main->getSpec3DDofAperture(), juce::dontSendNotification);
    {
        const auto dq = main->getSpec3DDofQuality();
        dofQualityCombo.setSelectedId (
            dq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (dq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 2),
            juce::dontSendNotification);
    }
    sssToggle.setToggleState (main->isSpec3DSssEnabled(), juce::dontSendNotification);
    sssStrengthSlider.setValue (main->getSpec3DSssStrength(), juce::dontSendNotification);
    sssWrapSlider.setValue (main->getSpec3DSssWrap(), juce::dontSendNotification);
    sssTransmissionSlider.setValue (main->getSpec3DSssTransmission(), juce::dontSendNotification);
    {
        const auto t = main->getSpec3DSssTint();
        sssTintRSlider.setValue (t.getFloatRed(), juce::dontSendNotification);
        sssTintGSlider.setValue (t.getFloatGreen(), juce::dontSendNotification);
        sssTintBSlider.setValue (t.getFloatBlue(), juce::dontSendNotification);
    }
    sssRadiusSlider.setValue (main->getSpec3DSssRadius(), juce::dontSendNotification);
    sssContrastSlider.setValue (main->getSpec3DSssContrast(), juce::dontSendNotification);
    {
        const auto sq = main->getSpec3DSssQuality();
        sssQualityCombo.setSelectedId (
            sq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (sq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 2),
            juce::dontSendNotification);
    }
    sssThickScaleSlider.setValue (main->getSpec3DSssThicknessScale(), juce::dontSendNotification);
    sssMaxThickSlider.setValue (main->getSpec3DSssMaxThickness(), juce::dontSendNotification);

    updateLookDevVisibility();
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
                                      : (id == 5 ? Spectrogram3DComponent::MeshQuality::overkill
                                                 : Spectrogram3DComponent::MeshQuality::medium))),
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
    main->setSpec3DClosedMeshEnabled (closedMeshToggle.getToggleState(), true);
    main->setSpec3DMeshHeight ((float) meshHeightSlider.getValue(), true);
    main->setSpec3DFreqMeshBias ((float) freqMeshBiasSlider.getValue(), true);
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
    main->setSpec3DMetalnessAmount ((float) metalnessSlider.getValue(), true);
    main->setSpec3DRimAmount ((float) rimSlider.getValue(), true);
    main->setSpec3DLightColour (juce::Colour::fromFloatRGBA ((float) lightColRSlider.getValue(),
                                                             (float) lightColGSlider.getValue(),
                                                             (float) lightColBSlider.getValue(),
                                                             1.0f),
                                true);
    main->setSpec3DRimColour (juce::Colour::fromFloatRGBA ((float) rimColRSlider.getValue(),
                                                           (float) rimColGSlider.getValue(),
                                                           (float) rimColBSlider.getValue(),
                                                           1.0f),
                              true);
    main->setSpec3DDomeFillEnabled (domeFillToggle.getToggleState(), true);
    main->setSpec3DDomeFillStrength ((float) domeFillStrengthSlider.getValue(), true);
    main->setSpec3DDomeSkyColour (juce::Colour::fromFloatRGBA ((float) domeSkyRSlider.getValue(),
                                                              (float) domeSkyGSlider.getValue(),
                                                              (float) domeSkyBSlider.getValue(),
                                                              1.0f),
                                  true);
    main->setSpec3DDomeGroundColour (juce::Colour::fromFloatRGBA ((float) domeGroundRSlider.getValue(),
                                                                 (float) domeGroundGSlider.getValue(),
                                                                 (float) domeGroundBSlider.getValue(),
                                                                 1.0f),
                                     true);
    main->setSpec3DSsgiEnabled (ssgiToggle.getToggleState(), true);
    main->setSpec3DSsgiStrength ((float) ssgiStrengthSlider.getValue(), true);
    main->setSpec3DSsgiRadius ((float) ssgiRadiusSlider.getValue(), true);
    {
        const int id = ssgiQualityCombo.getSelectedId();
        main->setSpec3DSsgiQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            true);
    }
    main->setSpec3DContactShadowEnabled (contactShadowToggle.getToggleState(), true);
    main->setSpec3DContactShadowStrength ((float) contactShadowStrengthSlider.getValue(), true);
    main->setSpec3DSelfShadowEnabled (selfShadowToggle.getToggleState(), true);
    main->setSpec3DSelfShadowStrength ((float) selfShadowStrengthSlider.getValue(), true);
    main->setSpec3DSelfShadowBias ((float) selfShadowBiasSlider.getValue(), true);
    main->setSpec3DSelfShadowSoftness ((float) selfShadowSoftnessSlider.getValue(), true);
    {
        const int id = selfShadowQualityCombo.getSelectedId();
        main->setSpec3DSelfShadowQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            true);
    }
    main->setSpec3DSsaoEnabled (ssaoToggle.getToggleState(), true);
    main->setSpec3DSsaoStrength ((float) ssaoStrengthSlider.getValue(), true);
    main->setSpec3DSsaoRadius ((float) ssaoRadiusSlider.getValue(), true);
    main->setSpec3DBloomEnabled (bloomToggle.getToggleState(), true);
    main->setSpec3DBloomStrength ((float) bloomStrengthSlider.getValue(), true);
    main->setSpec3DBloomThreshold ((float) bloomThresholdSlider.getValue(), true);
    main->setSpec3DDofEnabled (dofToggle.getToggleState(), true);
    main->setSpec3DDofFocusDistance ((float) dofFocusSlider.getValue(), true);
    main->setSpec3DDofAperture ((float) dofApertureSlider.getValue(), true);
    {
        const int id = dofQualityCombo.getSelectedId();
        main->setSpec3DDofQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            true);
    }
    main->setSpec3DSssEnabled (sssToggle.getToggleState(), true);
    main->setSpec3DSssStrength ((float) sssStrengthSlider.getValue(), true);
    main->setSpec3DSssWrap ((float) sssWrapSlider.getValue(), true);
    main->setSpec3DSssTransmission ((float) sssTransmissionSlider.getValue(), true);
    main->setSpec3DSssTint (juce::Colour::fromFloatRGBA ((float) sssTintRSlider.getValue(),
                                                         (float) sssTintGSlider.getValue(),
                                                         (float) sssTintBSlider.getValue(),
                                                         1.0f),
                            true);
    main->setSpec3DSssRadius ((float) sssRadiusSlider.getValue(), true);
    main->setSpec3DSssContrast ((float) sssContrastSlider.getValue(), true);
    {
        const int id = sssQualityCombo.getSelectedId();
        main->setSpec3DSssQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            true);
    }
    main->setSpec3DSssThicknessScale ((float) sssThickScaleSlider.getValue(), true);
    main->setSpec3DSssMaxThickness ((float) sssMaxThickSlider.getValue(), true);
}

void Spectrogram3DSettingsComponent::Content::applyControlsToMain()
{
    applyStructureControlsToMain();
    applyLookControlsToMain();
}

int Spectrogram3DSettingsComponent::Content::getPreferredHeight() const
{
    const int rowH = kLabelH + kLabelGap + kSliderH + kRowGap;
    const int toggleH = 22 + 6;
    const int comboRows = 2; // mesh quality + MSAA
    const int baseSliderRows = 2; // mesh height + HF density
    const int toggles = 5 + 9; // base (+ closed) + look master toggles (+ SSS + DOF + dome + SSGI)
    const int buttonRows = 1;

    int lookRows = 0;
    if (lightingToggle.getToggleState())
        lookRows += 13; // amount, az/el, specular, roughness, metalness, rim, light RGB, rim RGB
    if (lightingToggle.getToggleState() && domeFillToggle.getToggleState())
        lookRows += 7; // strength, sky RGB, ground RGB
    if (ssgiToggle.getToggleState()) lookRows += 3; // strength, radius, quality
    if (contactShadowToggle.getToggleState()) lookRows += 1;
    if (selfShadowToggle.getToggleState()) lookRows += 4; // strength, bias, softness, quality
    if (ssaoToggle.getToggleState()) lookRows += 2;
    if (bloomToggle.getToggleState()) lookRows += 2;
    if (dofToggle.getToggleState()) lookRows += 3; // focus, aperture, quality
    if (sssToggle.getToggleState())
    {
        lookRows += 7; // strength, wrap, transmission, tint RGB, quality
        if (closedMeshToggle.getToggleState())
            lookRows += 2; // thick scale, max thick
        else
            lookRows += 2; // radius, contrast
    }

    return kPadY * 2
           + 24 + 8
           + comboRows * rowH
           + baseSliderRows * rowH
           + lookRows * rowH
           + toggles * toggleH
           + buttonRows * toggleH
           + kLabelH + kSectionGap // look section header
           + kLabelH + kLabelGap + gradientEditor.getPreferredHeight() + kRowGap;
}

void Spectrogram3DSettingsComponent::Content::resized()
{
    // Don't sync from Main here — that changes Look row count mid-layout and
    // left the Menu scrollbar short until the user resized the panel. Sync is
    // done from the ctor delay + when the tab/page is shown.

    auto area = getLocalBounds().reduced (kPadX, kPadY);

    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (8);

    layoutToggle (area, enable3DToggle);
    layoutToggle (area, enhancedFreq3DToggle);
    layoutComboRow (area, meshQualityLabel, meshQualityCombo);
    layoutSliderRow (area, meshHeightLabel, meshHeightSlider);
    layoutSliderRow (area, freqMeshBiasLabel, freqMeshBiasSlider);
    layoutComboRow (area, msaaLabel, msaaCombo);
    layoutToggle (area, transparentBgToggle);
    layoutToggle (area, reverseFreqAxisToggle);
    layoutToggle (area, closedMeshToggle);
    resetCameraButton.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (160, area.getWidth())));
    area.removeFromTop (kSectionGap);

    lookLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kRowGap);
    layoutToggle (area, lightingToggle);
    if (lightingToggle.getToggleState())
    {
        layoutSliderRow (area, lightingAmountLabel, lightingAmountSlider);
        layoutSliderRow (area, lightAzimuthLabel, lightAzimuthSlider);
        layoutSliderRow (area, lightElevationLabel, lightElevationSlider);
        layoutSliderRow (area, specularLabel, specularSlider);
        layoutSliderRow (area, roughnessLabel, roughnessSlider);
        layoutSliderRow (area, metalnessLabel, metalnessSlider);
        layoutSliderRow (area, rimLabel, rimSlider);
        layoutSliderRow (area, lightColRLabel, lightColRSlider);
        layoutSliderRow (area, lightColGLabel, lightColGSlider);
        layoutSliderRow (area, lightColBLabel, lightColBSlider);
        layoutSliderRow (area, rimColRLabel, rimColRSlider);
        layoutSliderRow (area, rimColGLabel, rimColGSlider);
        layoutSliderRow (area, rimColBLabel, rimColBSlider);
    }
    layoutToggle (area, domeFillToggle);
    if (lightingToggle.getToggleState() && domeFillToggle.getToggleState())
    {
        layoutSliderRow (area, domeFillStrengthLabel, domeFillStrengthSlider);
        layoutSliderRow (area, domeSkyRLabel, domeSkyRSlider);
        layoutSliderRow (area, domeSkyGLabel, domeSkyGSlider);
        layoutSliderRow (area, domeSkyBLabel, domeSkyBSlider);
        layoutSliderRow (area, domeGroundRLabel, domeGroundRSlider);
        layoutSliderRow (area, domeGroundGLabel, domeGroundGSlider);
        layoutSliderRow (area, domeGroundBLabel, domeGroundBSlider);
    }
    layoutToggle (area, ssgiToggle);
    if (ssgiToggle.getToggleState())
    {
        layoutSliderRow (area, ssgiStrengthLabel, ssgiStrengthSlider);
        layoutSliderRow (area, ssgiRadiusLabel, ssgiRadiusSlider);
        layoutComboRow (area, ssgiQualityLabel, ssgiQualityCombo);
    }
    layoutToggle (area, contactShadowToggle);
    if (contactShadowToggle.getToggleState())
        layoutSliderRow (area, contactShadowStrengthLabel, contactShadowStrengthSlider);
    layoutToggle (area, selfShadowToggle);
    if (selfShadowToggle.getToggleState())
    {
        layoutSliderRow (area, selfShadowStrengthLabel, selfShadowStrengthSlider);
        layoutSliderRow (area, selfShadowBiasLabel, selfShadowBiasSlider);
        layoutSliderRow (area, selfShadowSoftnessLabel, selfShadowSoftnessSlider);
        layoutComboRow (area, selfShadowQualityLabel, selfShadowQualityCombo);
    }
    layoutToggle (area, ssaoToggle);
    if (ssaoToggle.getToggleState())
    {
        layoutSliderRow (area, ssaoStrengthLabel, ssaoStrengthSlider);
        layoutSliderRow (area, ssaoRadiusLabel, ssaoRadiusSlider);
    }
    layoutToggle (area, bloomToggle);
    if (bloomToggle.getToggleState())
    {
        layoutSliderRow (area, bloomStrengthLabel, bloomStrengthSlider);
        layoutSliderRow (area, bloomThresholdLabel, bloomThresholdSlider);
    }
    layoutToggle (area, dofToggle);
    if (dofToggle.getToggleState())
    {
        layoutSliderRow (area, dofFocusLabel, dofFocusSlider);
        layoutSliderRow (area, dofApertureLabel, dofApertureSlider);
        layoutComboRow (area, dofQualityLabel, dofQualityCombo);
    }
    layoutToggle (area, sssToggle);
    if (sssToggle.getToggleState())
    {
        layoutSliderRow (area, sssStrengthLabel, sssStrengthSlider);
        layoutSliderRow (area, sssWrapLabel, sssWrapSlider);
        layoutSliderRow (area, sssTransmissionLabel, sssTransmissionSlider);
        layoutSliderRow (area, sssTintRLabel, sssTintRSlider);
        layoutSliderRow (area, sssTintGLabel, sssTintGSlider);
        layoutSliderRow (area, sssTintBLabel, sssTintBSlider);
        layoutComboRow (area, sssQualityLabel, sssQualityCombo);
        if (closedMeshToggle.getToggleState())
        {
            layoutSliderRow (area, sssThickScaleLabel, sssThickScaleSlider);
            layoutSliderRow (area, sssMaxThickLabel, sssMaxThickSlider);
        }
        else
        {
            layoutSliderRow (area, sssRadiusLabel, sssRadiusSlider);
            layoutSliderRow (area, sssContrastLabel, sssContrastSlider);
        }
    }
    area.removeFromTop (kSectionGap);

    gradientLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kLabelGap);
    gradientEditor.setBounds (area.removeFromTop (gradientEditor.getPreferredHeight())
                                  .removeFromLeft (juce::jmin (520, area.getWidth())));
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
