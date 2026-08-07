#include "Spectrogram3DSettingsComponent.h"

#include "../../MainComponent.h"
#include "../../ModuleLookPresets.h"
#include "../../ScopeModules.h"
#include "../../SpectrogramComponent.h"
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
    // Dual-thumb range: arrow grips (same family as Appearance range bars).
    particleRangeLnF.setThumbStyle (CustomTwoValueSliderLookAndFeel::Arrow);
    particleRangeLnF.setArrowOrientation (CustomTwoValueSliderLookAndFeel::Up);
    particleRangeLnF.applyThemeColors (juce::Colours::darkgoldenrod.withAlpha (0.65f),
                                       juce::Colours::black.withAlpha (0.40f),
                                       juce::Colours::goldenrod);

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

    fftSizeLabel.setText ("FFT Size", juce::dontSendNotification);
    styleCombo (fftSizeCombo);
    {
        const auto names = SpectrogramComponent::getFftSizeNames();
        for (int i = 0; i < names.size(); ++i)
            fftSizeCombo.addItem (names[i], i + 1);
    }
    fftSizeCombo.setTooltip (
        "Analysis FFT block size for the spectrogram (shared with 2D Spec). "
        "Larger = finer frequency resolution, more CPU / latency.");
    addAndMakeVisible (fftSizeLabel);
    addAndMakeVisible (fftSizeCombo);
    fftSizeAttachment = std::make_unique<ComboBoxAttachment> (treeState, "SPEC_FFT_SIZE_ID", fftSizeCombo);

    meshQualityLabel.setText ("Mesh Quality", juce::dontSendNotification);
    styleCombo (meshQualityCombo);
    meshQualityCombo.addItem ("Low", 1);
    meshQualityCombo.addItem ("Medium", 2);
    meshQualityCombo.addItem ("High", 3);
    meshQualityCombo.addItem ("Ultra", 4);
    meshQualityCombo.setTooltip ("Mesh density along time x frequency. Ultra is 288x240 base rows.");
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
        "How strongly to pack extra frequency quads above the density start. "
        "0 = uniform; higher = denser highs. Row count also depends on HF Density Start.");
    addAndMakeVisible (freqMeshBiasLabel);
    addAndMakeVisible (freqMeshBiasSlider);

    freqMeshBiasPivotLabel.setText ("HF Density Start", juce::dontSendNotification);
    styleSlider (freqMeshBiasPivotSlider);
    freqMeshBiasPivotSlider.setRange (0.0, 0.95, 0.01);
    freqMeshBiasPivotSlider.setValue (Spectrogram3DComponent::kFreqMeshBiasPivotDefault,
                                      juce::dontSendNotification);
    freqMeshBiasPivotSlider.onValueChange = [this] { applyStructureControlsToMain(); };
    freqMeshBiasPivotSlider.setTooltip (
        "Inflection on the frequency axis where HF packing begins. "
        "0 = boost from the lows (old behaviour); higher = keep mids uniform and only densify the top.");
    addAndMakeVisible (freqMeshBiasPivotLabel);
    addAndMakeVisible (freqMeshBiasPivotSlider);

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
        "Off by default. Independent of SSS - when SSS is on it uses volume thickness if enabled.");
    closedMeshToggle.onClick = [this]
    {
        updateLookDevVisibility();
        applyStructureControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (closedMeshToggle);

    styleLabel (softAngleLabel);
    softAngleLabel.setText ("Soft Angle (deg)", juce::dontSendNotification);
    styleSlider (softAngleSlider);
    softAngleSlider.setRange (Spectrogram3DComponent::kNormalCuspMinDeg,
                              Spectrogram3DComponent::kNormalCuspMaxDeg,
                              1.0);
    softAngleSlider.setValue (Spectrogram3DComponent::kNormalCuspDefaultDeg,
                              juce::dontSendNotification);
    softAngleSlider.setTooltip (
        "Labs Soften Normals-style soft angle (cusp). 180 = fully soft organic shading; "
        "lower values keep harder ridges. Weighting stays Angle+Area under the hood.");
    softAngleSlider.onValueChange = [this] { applyStructureControlsToMain(); };
    addAndMakeVisible (softAngleLabel);
    addAndMakeVisible (softAngleSlider);

    styleSaveDefaultButton (resetCameraButton);
    resetCameraButton.setButtonText ("Reset 3D Camera");
    resetCameraButton.onClick = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->resetSpec3DCamera();
    };
    addAndMakeVisible (resetCameraButton);

    styleSaveDefaultButton (saveDefaultButton);
    saveDefaultButton.setTooltip ("Save this module's current look as default for Spectrogram 3D.");
    saveDefaultButton.onClick = [this] { saveModuleLookDefault(); };
    addAndMakeVisible (saveDefaultButton);

    styleSaveDefaultButton (savePresetButton);
    savePresetButton.setTooltip ("Save current look as a named preset for this module.");
    savePresetButton.onClick = [this] { saveModuleLookPreset(); };
    addAndMakeVisible (savePresetButton);

    lookLabel.setText ("Look (SSR on by default)", juce::dontSendNotification);
    styleLabel (lookLabel);
    lookLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (lookLabel);

    styleToggle (audioLevelToggle);
    audioLevelToggle.setTooltip (
        "Sidechain the pre-EQ input level (HP/LP filtered) into Look targets so the mesh "
        "pulses with the beat. Off by default.");
    audioLevelToggle.onClick = [this]
    {
        updateLookDevVisibility();
        applyLookControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (audioLevelToggle);

    styleLabel (audioLevelTargetLabel);
    audioLevelTargetLabel.setText ("Affects", juce::dontSendNotification);
    styleCombo (audioLevelTargetCombo);
    audioLevelTargetCombo.addItem ("Ramp brightness", 1);
    audioLevelTargetCombo.addItem ("Lighting amount", 2);
    audioLevelTargetCombo.addItem ("Specular", 3);
    audioLevelTargetCombo.addItem ("Rim light", 4);
    audioLevelTargetCombo.addItem ("Dome fill", 5);
    audioLevelTargetCombo.addItem ("All lights", 6);
    audioLevelTargetCombo.addItem ("Brightness + lights", 7);
    audioLevelTargetCombo.setSelectedId (1, juce::dontSendNotification);
    audioLevelTargetCombo.setTooltip (
        "Which Look parameter the filtered audio level modulates. "
        "Ramp brightness pulses colours only - Lighting amount / All lights are required to pulse lighting.");
    audioLevelTargetCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (audioLevelTargetLabel);
    addAndMakeVisible (audioLevelTargetCombo);

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
                                   double minV, double maxV, double step, const juce::String& tip,
                                   bool uncappedText = false)
    {
        styleLabel (lab);
        styleSlider (s);
        s.setRange (minV, maxV, step);
        s.setTooltip (tip);
        s.onValueChange = [this] { applyLookControlsToMain(); };
        if (uncappedText)
        {
            wireUncappedTextEntry (s);
            s.setTooltip (tip + " (type beyond the slider range for extreme values.)");
        }
        addAndMakeVisible (lab);
        addAndMakeVisible (s);
    };
    auto setupParticleSlider = [setupLookSlider] (juce::Label& lab, juce::Slider& s,
                                                  double minV, double maxV, double step,
                                                  const juce::String& tip)
    {
        setupLookSlider (lab, s, minV, maxV, step, tip, true);
    };

    audioLevelMinPctLabel.setText ("At Silence (%)", juce::dontSendNotification);
    setupLookSlider (audioLevelMinPctLabel, audioLevelMinPctSlider,
                     Spectrogram3DComponent::kAudioLevelPercentMin,
                     Spectrogram3DComponent::kAudioLevelPercentMax,
                     1.0,
                     "Modulation % when the sidechain is below/at threshold (level 0). "
                     "Full +/-100 range - swap with At Peak to invert.");
    audioLevelMinPctSlider.setValue (Spectrogram3DComponent::kAudioLevelMinPercentDefault,
                                     juce::dontSendNotification);
    audioLevelMaxPctLabel.setText ("At Peak (%)", juce::dontSendNotification);
    setupLookSlider (audioLevelMaxPctLabel, audioLevelMaxPctSlider,
                     Spectrogram3DComponent::kAudioLevelPercentMin,
                     Spectrogram3DComponent::kAudioLevelPercentMax,
                     1.0,
                     "Modulation % at full sidechain level (1). Independent of At Silence - "
                     "set e.g. +20 / -20 to invert the pulse.");
    audioLevelMaxPctSlider.setValue (Spectrogram3DComponent::kAudioLevelMaxPercentDefault,
                                     juce::dontSendNotification);

    audioLevelHpLabel.setText ("Sidechain HP (Hz)", juce::dontSendNotification);
    setupLookSlider (audioLevelHpLabel, audioLevelHpSlider, 20.0, 2000.0, 1.0,
                     "High-pass the visual sidechain (raise to ignore rumble).");
    audioLevelHpSlider.setValue (Spectrogram3DComponent::kAudioLevelHpDefaultHz,
                                 juce::dontSendNotification);
    audioLevelLpLabel.setText ("Sidechain LP (Hz)", juce::dontSendNotification);
    setupLookSlider (audioLevelLpLabel, audioLevelLpSlider, 40.0, 8000.0, 1.0,
                     "Low-pass the visual sidechain (lower to isolate kick).");
    audioLevelLpSlider.setValue (Spectrogram3DComponent::kAudioLevelLpDefaultHz,
                                 juce::dontSendNotification);
    audioLevelThresholdLabel.setText ("Threshold (dB)", juce::dontSendNotification);
    setupLookSlider (audioLevelThresholdLabel, audioLevelThresholdSlider,
                     Spectrogram3DComponent::kAudioLevelThresholdMinDb,
                     Spectrogram3DComponent::kAudioLevelThresholdMaxDb,
                     0.5,
                     "Sidechain level must exceed this before the pulse rises (0...1 over the next 24 dB).");
    audioLevelThresholdSlider.setValue (Spectrogram3DComponent::kAudioLevelThresholdDefaultDb,
                                        juce::dontSendNotification);
    styleLabel (audioLevelSpeedLabel);
    audioLevelSpeedLabel.setText ("Envelope Speed", juce::dontSendNotification);
    styleCombo (audioLevelSpeedCombo);
    audioLevelSpeedCombo.addItem ("Fast", 1);
    audioLevelSpeedCombo.addItem ("Med", 2);
    audioLevelSpeedCombo.addItem ("Slow", 3);
    audioLevelSpeedCombo.setSelectedId (1, juce::dontSendNotification);
    audioLevelSpeedCombo.setTooltip (
        "Hard-wired attack/release: Fast 8/80 ms, Med 40/300 ms, Slow 120/900 ms.");
    audioLevelSpeedCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (audioLevelSpeedLabel);
    addAndMakeVisible (audioLevelSpeedCombo);
    setupLookToggle (audioAffectPlayheadToggle,
                     "Also pulse the closed-mesh playhead (+X) wall. Only when Audio level affects is on.");
    setupLookToggle (audioAffectAntiPlayheadToggle,
                     "Also pulse the closed-mesh anti-playhead (-X) wall. Only when Audio level affects is on.");

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
                     "PBR microfacet roughness - low = sharp highlight, high = broad/dull.");
    roughnessSlider.setValue (0.45, juce::dontSendNotification);
    metalnessLabel.setText ("Metalness", juce::dontSendNotification);
    setupLookSlider (metalnessLabel, metalnessSlider, 0.0, 1.0, 0.01,
                     "PBR metalness - 0 = dielectric, 1 = metal (tinted specular, no diffuse).");
    metalnessSlider.setValue (0.0, juce::dontSendNotification);
    setupLookToggle (energyConserveToggle,
                     "Multiply diffuse/dome by (1-Fresnel). Off by default (legacy Look).");
    rimLabel.setText ("Rim Light", juce::dontSendNotification);
    setupLookSlider (rimLabel, rimSlider, 0.0, 1.0, 0.01, "View-dependent edge lift.");
    rimSlider.setValue (0.22, juce::dontSendNotification);
    wireColourEditor (lightColourEditor);
    lightColourEditor.setColour (juce::Colours::white);
    wireColourEditor (rimColourEditor);
    rimColourEditor.setColour (juce::Colours::white);

    setupLookToggle (domeFillToggle,
                     "Hemisphere dome fill - sky/ground ambient into shadows (needs Lighting). "
                     "Off by default.");
    domeFillStrengthLabel.setText ("Dome Fill Strength", juce::dontSendNotification);
    setupLookSlider (domeFillStrengthLabel, domeFillStrengthSlider, 0.0, 1.0, 0.01,
                     "How strongly sky/ground ambient fills shadowed regions.");
    domeFillStrengthSlider.setValue (0.35, juce::dontSendNotification);
    setupLookToggle (domeTextureToggle,
                     "Sample an equirectangular dome texture (HDRI) instead of solid sky/ground colours. "
                     "Includes Poly Haven Venice Sunset (CC0).");
    domeTextureLabel.setText ("Dome Texture", juce::dontSendNotification);
    styleCombo (domeTextureCombo);
    domeTextureCombo.addItem ("Venice Sunset (Poly Haven)", 1);
    domeTextureCombo.addItem ("Load custom...", 2);
    domeTextureCombo.setSelectedId (1, juce::dontSendNotification);
    domeTextureCombo.setTooltip (
        "Built-in Venice Sunset is a free CC0 equirectangular from Poly Haven. "
        "Load custom accepts JPG/PNG equirectangular maps.");
    domeTextureCombo.onChange = [this]
    {
        if (domeTextureCombo.getSelectedId() == 2)
            browseDomeTextureFile();
        else
            applyLookControlsToMain();
    };
    addAndMakeVisible (domeTextureLabel);
    addAndMakeVisible (domeTextureCombo);
    wireColourEditor (domeSkyEditor);
    domeSkyEditor.setColour (juce::Colour (0xff7390bf));
    wireColourEditor (domeGroundEditor);
    domeGroundEditor.setColour (juce::Colour (0xff403328));

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
    ssgiQualityCombo.addItem ("Ultra", 4);
    ssgiQualityCombo.setSelectedId (2, juce::dontSendNotification);
    ssgiQualityCombo.setTooltip (
        "SSGI ray/step density: Low 6x4, Medium 10x6, High 14x8, Ultra 20x12. "
        "Ultra is the preferred quality step when GI needs more stability.");
    ssgiQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (ssgiQualityLabel);
    addAndMakeVisible (ssgiQualityCombo);

    setupLookToggle (ssrToggle,
                     "Screen-space reflections of the lit colour buffer (key specular, "
                     "dome-lit surfaces, bright bands). Soft FBO path. On by default.");
    ssrToggle.setToggleState (true, juce::dontSendNotification);
    ssrStrengthLabel.setText ("SSR Strength", juce::dontSendNotification);
    setupLookSlider (ssrStrengthLabel, ssrStrengthSlider, 0.0, 1.0, 0.01, "Reflection mix into the scene.");
    ssrStrengthSlider.setValue (0.55, juce::dontSendNotification);
    ssrDistanceLabel.setText ("SSR Distance", juce::dontSendNotification);
    setupLookSlider (ssrDistanceLabel, ssrDistanceSlider, 0.0, 1.0, 0.01,
                     "How far reflection rays march in view space.");
    ssrDistanceSlider.setValue (0.55, juce::dontSendNotification);
    ssrThicknessLabel.setText ("SSR Thickness", juce::dontSendNotification);
    setupLookSlider (ssrThicknessLabel, ssrThicknessSlider, 0.0, 1.0, 0.01,
                     "Depth hit acceptance slab - higher catches more, softer contacts.");
    ssrThicknessSlider.setValue (0.40, juce::dontSendNotification);
    ssrQualityLabel.setText ("SSR Quality", juce::dontSendNotification);
    styleCombo (ssrQualityCombo);
    ssrQualityCombo.addItem ("Low", 1);
    ssrQualityCombo.addItem ("Medium", 2);
    ssrQualityCombo.addItem ("High", 3);
    ssrQualityCombo.addItem ("Ultra", 4);
    ssrQualityCombo.setSelectedId (2, juce::dontSendNotification);
    ssrQualityCombo.setTooltip (
        "SSR march steps + hit soft-sample taps: Low 10/4, Medium 18/6, High 28/8, Ultra 40/10. "
        "Uses mesh/sphere normals (not depth derivatives) to avoid blocky reflections.");
    ssrQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (ssrQualityLabel);
    addAndMakeVisible (ssrQualityCombo);
    ssrFresnelLabel.setText ("SSR Fresnel", juce::dontSendNotification);
    setupLookSlider (ssrFresnelLabel, ssrFresnelSlider, 0.0, 1.0, 0.01,
                     "View-angle fresnel - stronger at glancing angles.");
    ssrFresnelSlider.setValue (0.75, juce::dontSendNotification);
    ssrRoughInfLabel.setText ("SSR Roughness Infl.", juce::dontSendNotification);
    setupLookSlider (ssrRoughInfLabel, ssrRoughInfSlider, 0.0, 1.0, 0.01,
                     "How strongly Look Roughness kills / softens SSR.");
    ssrRoughInfSlider.setValue (0.85, juce::dontSendNotification);
    ssrIntensityLabel.setText ("SSR Intensity", juce::dontSendNotification);
    setupLookSlider (ssrIntensityLabel, ssrIntensitySlider, 0.0, 2.0, 0.01,
                     "Reflection brightness gain (can push past 1 for lookdev).");
    ssrIntensitySlider.setValue (1.0, juce::dontSendNotification);
    ssrEdgeFadeLabel.setText ("SSR Edge Fade", juce::dontSendNotification);
    setupLookSlider (ssrEdgeFadeLabel, ssrEdgeFadeSlider, 0.0, 1.0, 0.01,
                     "Fade reflections near screen borders (hides SSR cutoff).");
    ssrEdgeFadeSlider.setValue (0.15, juce::dontSendNotification);
    ssrMetalBiasLabel.setText ("SSR Metallic Bias", juce::dontSendNotification);
    setupLookSlider (ssrMetalBiasLabel, ssrMetalBiasSlider, 0.0, 1.0, 0.01,
                     "Extra SSR boost when Look Metalness is high.");
    ssrMetalBiasSlider.setValue (0.35, juce::dontSendNotification);
    ssrDomeFbLabel.setText ("SSR Dome Fallback", juce::dontSendNotification);
    setupLookSlider (ssrDomeFbLabel, ssrDomeFbSlider, 0.0, 1.0, 0.01,
                     "On ray miss, blend Dome Sky colour so off-screen / sky still reads.");
    ssrDomeFbSlider.setValue (0.65, juce::dontSendNotification);

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
                     "How strongly occluded ridges darken (0-2).");
    selfShadowStrengthSlider.setValue (0.85, juce::dontSendNotification);
    selfShadowBiasLabel.setText ("Shadow Bias", juce::dontSendNotification);
    setupLookSlider (selfShadowBiasLabel, selfShadowBiasSlider, 0.0, 1.0, 0.01,
                     "Shared by Self-Shadow and Cast Shadows - raises the sample origin to fight acne.");
    selfShadowBiasSlider.setValue (0.35, juce::dontSendNotification);
    selfShadowSoftnessLabel.setText ("Shadow Softness", juce::dontSendNotification);
    setupLookSlider (selfShadowSoftnessLabel, selfShadowSoftnessSlider, 0.0, 1.0, 0.01,
                     "Shared by Self-Shadow and Cast Shadows - widens the penumbra (higher = softer).");
    selfShadowSoftnessSlider.setValue (0.85, juce::dontSendNotification);
    selfShadowQualityLabel.setText ("Shadow Quality", juce::dontSendNotification);
    styleCombo (selfShadowQualityCombo);
    selfShadowQualityCombo.addItem ("Low", 1);
    selfShadowQualityCombo.addItem ("Medium", 2);
    selfShadowQualityCombo.addItem ("High", 3);
    selfShadowQualityCombo.setSelectedId (2, juce::dontSendNotification);
    selfShadowQualityCombo.setTooltip ("Sample density for horizon + ray-march self-shadows.");
    selfShadowQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (selfShadowQualityLabel);
    addAndMakeVisible (selfShadowQualityCombo);

    setupLookToggle (castShadowsToggle,
                     "Directional light-depth atlas (true cast occlusion). Mesh + debug sphere "
                     "cast and receive. Needs Lighting. Bias/Softness above are shared.");
    shadowResLabel.setText ("Shadow Map Resolution", juce::dontSendNotification);
    styleCombo (shadowResCombo);
    shadowResCombo.addItem ("512", 1);
    shadowResCombo.addItem ("1024", 2);
    shadowResCombo.addItem ("2048", 3);
    shadowResCombo.addItem ("4096", 4);
    shadowResCombo.setSelectedId (2, juce::dontSendNotification);
    shadowResCombo.setTooltip ("Per-cascade tile resolution (UE-style).");
    shadowResCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (shadowResLabel);
    addAndMakeVisible (shadowResCombo);

    cascadeCountLabel.setText ("Dynamic Shadow Cascades", juce::dontSendNotification);
    styleCombo (cascadeCountCombo);
    cascadeCountCombo.addItem ("1", 1);
    cascadeCountCombo.addItem ("2", 2);
    cascadeCountCombo.addItem ("3", 3);
    cascadeCountCombo.addItem ("4", 4);
    cascadeCountCombo.setSelectedId (1, juce::dontSendNotification);
    cascadeCountCombo.setTooltip (
        "Splits the same shadow distance into N cascade tiles (near = higher res). "
        "Does not shorten draw distance.");
    cascadeCountCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (cascadeCountLabel);
    addAndMakeVisible (cascadeCountCombo);

    cascadeDistLabel.setText ("Cascade Distribution Exponent", juce::dontSendNotification);
    setupLookSlider (cascadeDistLabel, cascadeDistSlider, 1.0, 4.0, 0.05,
                     "UE-style blend of linear/log cascade splits (higher = more logarithmic).");
    cascadeDistSlider.setValue (3.0, juce::dontSendNotification);

    cascadeTransLabel.setText ("Cascade Transition Fraction", juce::dontSendNotification);
    setupLookSlider (cascadeTransLabel, cascadeTransSlider, 0.0, 0.3, 0.01,
                     "Blend width between cascade splits (hides the hard split line).");
    cascadeTransSlider.setValue (0.10, juce::dontSendNotification);

    setupLookToggle (ssaoToggle,
                     "Heightfield ambient occlusion in crevices (mesh shader). Off by default.");
    ssaoStrengthLabel.setText ("AO Strength", juce::dontSendNotification);
    setupLookSlider (ssaoStrengthLabel, ssaoStrengthSlider, 0.0, 2.0, 0.01,
                     "How dark occluded crevices become (0-2).");
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

    setupLookToggle (motionBlurToggle,
                     "UE-style camera motion blur (soft path): depth reproject with previous "
                     "view-projection → screen velocity → reconstruction gather. "
                     "Blurs orbit / freecam / zoom motion. Off by default.");
    motionBlurAmountLabel.setText ("Motion blur amount", juce::dontSendNotification);
    setupLookSlider (motionBlurAmountLabel, motionBlurAmountSlider, 0.0, 1.0, 0.01,
                     "Shutter / exposure fraction (0 = none, 1 = full frame velocity). "
                     "Like Unreal motion blur amount.");
    motionBlurAmountSlider.setValue (Spectrogram3DComponent::kMotionBlurAmountDefault,
                                     juce::dontSendNotification);
    motionBlurMaxLabel.setText ("Motion blur max (px)", juce::dontSendNotification);
    setupLookSlider (motionBlurMaxLabel, motionBlurMaxSlider,
                     (double) Spectrogram3DComponent::kMotionBlurMaxMin,
                     (double) Spectrogram3DComponent::kMotionBlurMaxMax,
                     1.0,
                     "Clamp maximum streak length in screen pixels (UE max velocity clamp).");
    motionBlurMaxSlider.setValue (Spectrogram3DComponent::kMotionBlurMaxDefault,
                                  juce::dontSendNotification);
    motionBlurQualityLabel.setText ("Motion blur quality", juce::dontSendNotification);
    styleCombo (motionBlurQualityCombo);
    motionBlurQualityCombo.addItem ("Low (8 samples)", 1);
    motionBlurQualityCombo.addItem ("Medium (16 samples)", 2);
    motionBlurQualityCombo.addItem ("High (24 samples)", 3);
    motionBlurQualityCombo.setSelectedId (2, juce::dontSendNotification);
    motionBlurQualityCombo.setTooltip ("Samples along the velocity vector (more = smoother streaks, costlier).");
    motionBlurQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (motionBlurQualityLabel);
    addAndMakeVisible (motionBlurQualityCombo);

    setupLookToggle (dofToggle,
                     "Realtime post DOF (EEVEE / Marmoset Post Effect style): thin-lens "
                     "CoC + disc gather. Soft FBO path. Off by default.");
    // World unit = 1 m for photographic DOF (F-Stop / focal mm). Mesh footprint is 2x2 m.
    dofFocusLabel.setText ("Focus Distance (m)", juce::dontSendNotification);
    setupLookSlider (dofFocusLabel, dofFocusSlider,
                     Spectrogram3DComponent::kDofFocusMin,
                     Spectrogram3DComponent::kDofFocusMax,
                     0.01,
                     "Sharp plane distance from the camera (metres; 1 world unit = 1 m). "
                     "Mesh footprint is 2x2 m (time x frequency); default height about  0.55 m; "
                     "default camera distance about  3.4 m. "
                     "Ctrl+LMB (Cmd+LMB on Mac) on the mesh to set focus under the cursor.");
    dofFocusSlider.setValue (Spectrogram3DComponent::kDofFocusDefault, juce::dontSendNotification);
    // Focus must not be rewritten by other Look sliders (would stomp Ctrl+click picks).
    dofFocusSlider.onValueChange = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
        {
            main->setSpec3DDofFocusDistance ((float) dofFocusSlider.getValue(), false);
            main->requestUiPrefsSave();
        }
    };
    dofFStopLabel.setText ("F-Stop", juce::dontSendNotification);
    setupLookSlider (dofFStopLabel, dofFStopSlider,
                     (double) Spectrogram3DComponent::kDofFStopMin,
                     (double) Spectrogram3DComponent::kDofFStopMax,
                     0.1,
                     "Lens aperture (f-number). Lower = shallower DOF / more bokeh "
                     "(f/1.4 wide open -> f/22 deep focus).");
    dofFStopSlider.setValue (Spectrogram3DComponent::kDofFStopDefault, juce::dontSendNotification);
    dofFocalLengthLabel.setText ("Focal Length (mm)", juce::dontSendNotification);
    setupLookSlider (dofFocalLengthLabel, dofFocalLengthSlider,
                     (double) Spectrogram3DComponent::kDofFocalLengthMinMm,
                     (double) Spectrogram3DComponent::kDofFocalLengthMaxMm,
                     1.0,
                     "Effective focal length for thin-lens CoC (scene metres). "
                     "Longer = shallower DOF at the same focus distance (18-85 mm, default 35).");
    dofFocalLengthSlider.setValue (Spectrogram3DComponent::kDofFocalLengthDefaultMm,
                                   juce::dontSendNotification);
    dofQualityLabel.setText ("DOF Quality", juce::dontSendNotification);
    styleCombo (dofQualityCombo);
    dofQualityCombo.addItem ("Low", 1);
    dofQualityCombo.addItem ("Medium", 2);
    dofQualityCombo.addItem ("High", 3);
    dofQualityCombo.setSelectedId (2, juce::dontSendNotification);
    dofQualityCombo.setTooltip ("Disc sample count / base max bokeh size (8 / 16 / 24 taps).");
    dofQualityCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (dofQualityLabel);
    addAndMakeVisible (dofQualityCombo);
    dofCocDilateLabel.setText ("DOF Edge Dilate", juce::dontSendNotification);
    setupLookSlider (dofCocDilateLabel, dofCocDilateSlider, 0.0, 1.0, 0.01,
                     "How far defocus CoC spreads into Soft BG void (radius only). "
                     "0 = no spread. Does not brighten edges by itself — pairs with Edge Spill "
                     "for mesh colour bleed onto sky. Keep moderate to avoid halos.");
    dofCocDilateSlider.setValue (Spectrogram3DComponent::kDofCocDilateDefault, juce::dontSendNotification);
    dofEdgeSpillLabel.setText ("DOF Edge Spill", juce::dontSendNotification);
    setupLookSlider (dofEdgeSpillLabel, dofEdgeSpillSlider, 0.0, 1.0, 0.01,
                     "How strongly out-of-focus mesh colour bleeds onto Soft BG at silhouettes "
                     "(weight only, 0–1). Independent of Dilate — do not crank both to max "
                     "or you get a bright outline.");
    dofEdgeSpillSlider.setValue (Spectrogram3DComponent::kDofEdgeSpillDefault, juce::dontSendNotification);

    setupLookToggle (tonemapToggle,
                     "Display transform + color grade (soft path). Off by default - "
                     "preserves the current LDR look until enabled.");
    exposureLabel.setText ("Exposure (stops)", juce::dontSendNotification);
    setupLookSlider (exposureLabel, exposureSlider, -4.0, 4.0, 0.01,
                     "Exposure in stops (0 = unchanged). Applied when Tonemap is on.");
    exposureSlider.setValue (-0.3, juce::dontSendNotification);
    gradeLabel.setText ("Color Grade", juce::dontSendNotification);
    styleCombo (gradeCombo);
    gradeCombo.addItem ("ACES (neutral)", 1);
    gradeCombo.addItem ("Filmic", 2);
    gradeCombo.addItem ("Warm Cinema", 3);
    gradeCombo.addItem ("Cool Cinema", 4);
    gradeCombo.addItem ("Teal Orange", 5);
    gradeCombo.addItem ("Bleach Bypass", 6);
    gradeCombo.setSelectedId (3, juce::dontSendNotification); // Warm Cinema
    gradeCombo.setTooltip ("Display transform look. Default: Warm Cinema at -0.3 EV.");
    gradeCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (gradeLabel);
    addAndMakeVisible (gradeCombo);

    setupLookToggle (particleToggle,
                     "Replace the mesh with a playhead particle field. Off by default "
                     "(no cost when disabled).");
    setupLookToggle (particleGpuSimToggle,
                     "Run force / age integration on the GPU (OpenGL 4.3 compute). "
                     "CPU is the default and always works. GPU needs a 4.3+ context; "
                     "if compute fails to start, motion falls back to CPU automatically. "
                     "Spawn and colour matrix stay on the CPU either way.");
    particleGpuSimToggle.setToggleState (false, juce::dontSendNotification);
    particleMaxAliveLabel.setText ("Max particles", juce::dontSendNotification);
    // Drag range to 100k; type up to 1M for stress tests (absolute hard cap).
    setupParticleSlider (particleMaxAliveLabel, particleMaxAliveSlider, 256.0, 100000.0, 256.0,
                         "Live particle budget. Default 8192. Drag up to 100k; type up to 1,000,000 "
                         "for stress tests (~200MB RAM at 1M — can hitch or OOM the host).\n"
                         "This is the total ceiling. Emission only controls how fast you fill it.\n"
                         "At 100k+: free-list spawn, heavy colour throttling; hybrid GPU integrate "
                         "auto-falls back to CPU (readback can't scale). Full GPU sim still TODO.");
    setSliderActual (particleMaxAliveSlider, 8192.0);
    setupLookToggle (particleDebugOverlayToggle,
                     "Show live particle stats on the 3D view (alive / budget / pool / "
                     "spawned / culled). Useful when tuning emission without crashing.");
    particleDebugOverlayToggle.setToggleState (false, juce::dontSendNotification);
    styleSaveDefaultButton (particleClearButton);
    particleClearButton.setButtonText ("Clear particles");
    particleClearButton.setTooltip ("Kill all live particles immediately (escape hatch).");
    particleClearButton.onClick = [this]
    {
        if (auto* main = findParentComponentOfClass<MainComponent>())
            main->clearSpec3DParticles();
    };
    addAndMakeVisible (particleClearButton);
    styleLabel (particleBindingLabel);
    particleBindingLabel.setText ("Binding", juce::dontSendNotification);
    styleCombo (particleBindingCombo);
    particleBindingCombo.addItem ("Spectrogram trail", 1);
    particleBindingCombo.addItem ("Free visualizer", 2);
    particleBindingCombo.setSelectedId (1, juce::dontSendNotification);
    particleBindingCombo.setTooltip (
        "Trail: particles follow the waterfall. Free: motion owned by forces; spectrum is sources only.");
    particleBindingCombo.onChange = [this]
    {
        applyLookControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (particleBindingLabel);
    addAndMakeVisible (particleBindingCombo);
    styleLabel (particleEmitModeLabel);
    particleEmitModeLabel.setText ("Emit mode", juce::dontSendNotification);
    styleCombo (particleEmitModeCombo);
    particleEmitModeCombo.addItem ("Slice (per-bin rates)", 1);
    particleEmitModeCombo.addItem ("Continuous (random playhead)", 2);
    particleEmitModeCombo.setSelectedId (2, juce::dontSendNotification); // continuous default
    particleEmitModeCombo.setTooltip (
        "Continuous (default): energy-weighted random samples along the playhead with "
        "sub-bin positions - particles don't stack on exact grid points.\n"
        "Slice: each frequency bin emits on its own clock (more grid-like columns), "
        "still with within-band scatter + spawn jitter.");
    particleEmitModeCombo.onChange = [this]
    {
        applyLookControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (particleEmitModeLabel);
    addAndMakeVisible (particleEmitModeCombo);
    particleEmissionLabel.setText ("Emission rate (particles/s)", juce::dontSendNotification);
    setupParticleSlider (particleEmissionLabel, particleEmissionSlider, 0.0, 5000.0, 1.0,
                         "Particles spawned per second (total across the field). "
                         "Drag 0–5000; type higher for denser tests. "
                         "Live count ≈ rate × lifespan (until Max particles). "
                         "Continuous and slice modes share this same particles/s budget.");
    setSliderActual (particleEmissionSlider, 200.0);
    particleSpawnJitterLabel.setText ("Spawn jitter", juce::dontSendNotification);
    setupParticleSlider (particleSpawnJitterLabel, particleSpawnJitterSlider, 0.0, 0.5, 0.001,
                         "Randomize each particle's spawn offset (world units). "
                         "0 = exact surface sample; default scatters so particles don't stack. "
                         "Offsets are kept under waterfall lock.");
    setSliderActual (particleSpawnJitterSlider, 0.035);
    particleInitVelXLabel.setText ("Init vel X", juce::dontSendNotification);
    setupParticleSlider (particleInitVelXLabel, particleInitVelXSlider, -10.0, 10.0, 0.01,
                         "Initial velocity X (world units / second).");
    setSliderActual (particleInitVelXSlider, 0.0);
    particleInitVelYLabel.setText ("Init vel Y", juce::dontSendNotification);
    setupParticleSlider (particleInitVelYLabel, particleInitVelYSlider, -10.0, 10.0, 0.01,
                         "Initial velocity Y (world units / second). Default +1 rises up the mesh.");
    setSliderActual (particleInitVelYSlider, 1.0);
    particleInitVelZLabel.setText ("Init vel Z", juce::dontSendNotification);
    setupParticleSlider (particleInitVelZLabel, particleInitVelZSlider, -10.0, 10.0, 0.01,
                         "Initial velocity Z (world units / second).");
    setSliderActual (particleInitVelZSlider, 0.0);
    particleVelRandomLabel.setText ("Velocity random", juce::dontSendNotification);
    setupParticleSlider (particleVelRandomLabel, particleVelRandomSlider, 0.0, 1.0, 0.01,
                         "Per-axis random scale at spawn (+/- fraction of each Init vel component).");
    setSliderActual (particleVelRandomSlider, 0.0);
    particleLifespanLabel.setText ("Lifespan (s)", juce::dontSendNotification);
    setupParticleSlider (particleLifespanLabel, particleLifespanSlider, 0.0, 30.0, 0.05,
                         "Particle lifetime in seconds. 0 = indefinite (until scrolled off history).");
    setSliderActual (particleLifespanSlider, 0.0);
    // Relayout when lifespan crosses 0 so Lifespan random shows/hides.
    {
        const auto prev = particleLifespanSlider.onValueChange;
        particleLifespanSlider.onValueChange = [this, prev]
        {
            if (particleLifespanSlider.isMouseButtonDown())
                particleLifespanSlider.getProperties().set ("particleSliderActual",
                                                            particleLifespanSlider.getValue());
            updateLookDevVisibility();
            applyLookControlsToMain();
            requestParentRelayout();
            juce::ignoreUnused (prev);
        };
    }
    particleLifespanRandomLabel.setText ("Lifespan random", juce::dontSendNotification);
    setupParticleSlider (particleLifespanRandomLabel, particleLifespanRandomSlider, 0.0, 1.0, 0.01,
                         "Randomize lifetime at spawn (+/- fraction of Lifespan).");
    setSliderActual (particleLifespanRandomSlider, 0.0);
    particleSizeLabel.setText ("Particle size", juce::dontSendNotification);
    setupParticleSlider (particleSizeLabel, particleSizeSlider, 0.001, 0.08, 0.0005,
                         "Particle radius / mesh scale in world units.");
    setSliderActual (particleSizeSlider, 0.008);
    setupLookToggle (particleEmissiveToggle,
                     "Unlit mode: particles are pure self-lit colour x emissive strength "
                     "(skips lighting / PBR). Off = same GGX material as the waterfall mesh, "
                     "with emissive added on top. Uses global Lighting / Energy conserving.");
    particleRoughLabel.setText ("Roughness", juce::dontSendNotification);
    setupParticleSlider (particleRoughLabel, particleRoughSlider, 0.04, 1.0, 0.01,
                         "PBR microfacet roughness - same lobe as Spec3D waterfall "
                         "(low = sharp highlight, high = broad/dull).");
    setSliderActual (particleRoughSlider, 0.45);
    particleMetalLabel.setText ("Metalness", juce::dontSendNotification);
    setupParticleSlider (particleMetalLabel, particleMetalSlider, 0.0, 1.0, 0.01,
                         "PBR metalness - same as waterfall (0 = dielectric, 1 = metal).");
    setSliderActual (particleMetalSlider, 0.0);
    particleSpecLabel.setText ("Specular", juce::dontSendNotification);
    setupParticleSlider (particleSpecLabel, particleSpecSlider, 0.0, 1.0, 0.01,
                         "GGX specular lobe intensity - same as waterfall Specular.");
    setSliderActual (particleSpecSlider, 0.35);
    particleEmissiveStrLabel.setText ("Emissive", juce::dontSendNotification);
    setupParticleSlider (particleEmissiveStrLabel, particleEmissiveStrSlider, 0.0, 4.0, 0.01,
                         "Emissive amount. Additive on lit PBR (albedo x amount x matrix Emissive dest). "
                         "In unlit mode this is the only brightness scale.");
    setSliderActual (particleEmissiveStrSlider, 0.0);

    styleLabel (particleMeshLabel);
    particleMeshLabel.setText ("Mesh shape", juce::dontSendNotification);
    styleCombo (particleMeshCombo);
    particleMeshCombo.addItem ("Sphere (instanced)", 1);
    particleMeshCombo.addItem ("Cube (instanced)", 2);
    particleMeshCombo.addItem ("Billboard sprite", 3);
    particleMeshCombo.setSelectedId (1, juce::dontSendNotification);
    particleMeshCombo.setTooltip ("GPU-instanced low-poly mesh (default) or soft billboard sprites.");
    particleMeshCombo.onChange = [this]
    {
        updateLookDevVisibility();
        applyLookControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (particleMeshLabel);
    addAndMakeVisible (particleMeshCombo);
    particleInitRotXLabel.setText ("Init rot X°", juce::dontSendNotification);
    setupParticleSlider (particleInitRotXLabel, particleInitRotXSlider, -180.0, 180.0, 0.1, "Initial mesh rotation X (degrees).");
    setSliderActual (particleInitRotXSlider, 0.0);
    particleInitRotYLabel.setText ("Init rot Y°", juce::dontSendNotification);
    setupParticleSlider (particleInitRotYLabel, particleInitRotYSlider, -180.0, 180.0, 0.1, "Initial mesh rotation Y (degrees).");
    setSliderActual (particleInitRotYSlider, 0.0);
    particleInitRotZLabel.setText ("Init rot Z°", juce::dontSendNotification);
    setupParticleSlider (particleInitRotZLabel, particleInitRotZSlider, -180.0, 180.0, 0.1, "Initial mesh rotation Z (degrees).");
    setSliderActual (particleInitRotZSlider, 0.0);
    particleInitRotRndLabel.setText ("Init rot random", juce::dontSendNotification);
    setupParticleSlider (particleInitRotRndLabel, particleInitRotRndSlider, 0.0, 1.0, 0.01,
                         "Randomize initial rotation (fraction of full turn). Also matrix-routable.");
    setSliderActual (particleInitRotRndSlider, 0.0);

    setupLookToggle (particleForcesToggle,
                     "Evaluate the ordered force stack each frame. Drive force params from the mod matrix.");
    setupLookToggle (particleWaterfallLockToggle,
                     "In spectrogram trail mode, lock particle X to history columns.");
    particleWaterfallLockToggle.setToggleState (true, juce::dontSendNotification);
    particleForceStack = std::make_unique<ParticleForceStackComponent> (sharedResources);
    particleForceStack->onRequestUid = [] { return (uint32_t) juce::Random::getSystemRandom().nextInt(); };
    particleForceStack->onChanged = [this]
    {
        applyLookControlsToMain();
        requestParentRelayout();
    };
    addAndMakeVisible (*particleForceStack);

    styleLabel (particleModLabel);
    particleModLabel.setText ("Particle mod matrix", juce::dontSendNotification);
    particleModLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (particleModLabel);
    styleLabel (particleModHintLabel);
    particleModHintLabel.setText (
        "Route sources into particle or force params. Curve + range map the source; force dests affect all particles.",
        juce::dontSendNotification);
    particleModHintLabel.setColour (juce::Label::textColourId, juce::Colours::grey);
    addAndMakeVisible (particleModHintLabel);

    auto styleHdr = [this] (juce::Label& l, const juce::String& t)
    {
        styleLabel (l);
        l.setText (t, juce::dontSendNotification);
        l.setFont (juce::FontOptions().withName ("Lato Black").withHeight (12.0f));
        l.setColour (juce::Label::textColourId, juce::Colours::grey);
        addAndMakeVisible (l);
    };
    styleHdr (particleModHdrOn, "On");
    styleHdr (particleModHdrSrc, "Source");
    styleHdr (particleModHdrThr, "Thr");
    styleHdr (particleModHdrDst, "Dest");
    styleHdr (particleModHdrOp, "Op");
    styleHdr (particleModHdrCurve, "Curve");
    styleHdr (particleModHdrRange, "Range");
    styleHdr (particleModHdrInv, "Inv");
    styleHdr (particleModHdrAmt, "Amt");

    auto fillModSource = [] (juce::ComboBox& c)
    {
        c.clear (juce::dontSendNotification);
        // Item id = enum + 1 (stable for prefs).
        const ParticleModSource sources[] = {
            ParticleModSource::none,
            ParticleModSource::amplitude,
            ParticleModSource::binDb,
            ParticleModSource::binFreq,
            ParticleModSource::ageNorm,
            ParticleModSource::history,
            ParticleModSource::constant,
            ParticleModSource::random1,
            ParticleModSource::random2,
            ParticleModSource::random3,
            ParticleModSource::initVel,
            ParticleModSource::particleId,
        };
        for (auto s : sources)
            c.addItem (particleModSourceMenuLabel (s), (int) s + 1);
        c.setSelectedId (1, juce::dontSendNotification);
    };
    auto fillModDest = [] (juce::ComboBox& c)
    {
        c.clear (juce::dontSendNotification);
        const ParticleModDest dests[] = {
            ParticleModDest::emission,
            ParticleModDest::initVel,
            ParticleModDest::riseSpeed, // legacy: Init vel Y only
            ParticleModDest::lifespan,
            ParticleModDest::size,
            ParticleModDest::colourGain,
            ParticleModDest::colourHue,
            ParticleModDest::emissive,
            ParticleModDest::alpha,
            ParticleModDest::spawnJitter,
            ParticleModDest::sizeScale,
            ParticleModDest::initRot,
            ParticleModDest::forceGravity,
            ParticleModDest::forceDrag,
            ParticleModDest::forceWindX,
            ParticleModDest::forceWindY,
            ParticleModDest::forceWindZ,
            ParticleModDest::forceCurlStrength,
            ParticleModDest::forceCurlScale,
            ParticleModDest::forceCurlSpeed,
            ParticleModDest::forceTurbulence,
        };
        for (auto d : dests)
            c.addItem (particleModDestMenuLabel (d), (int) d + 1);
        c.setSelectedId (1, juce::dontSendNotification);
    };
    auto fillModOp = [] (juce::ComboBox& c)
    {
        c.clear (juce::dontSendNotification);
        c.addItem ("Set", 1);
        c.addItem ("Multiply", 2);
        c.addItem ("Add", 3);
        c.setSelectedId (2, juce::dontSendNotification);
    };

    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        auto& row = particleModRows[(size_t) i];
        styleToggle (row.enable);
        row.enable.setButtonText ("R" + juce::String (i + 1));
        row.enable.setTooltip ("Enable mod slot " + juce::String (i + 1));
        row.enable.onClick = [this]
        {
            updateLookDevVisibility();
            applyLookControlsToMain();
            requestParentRelayout();
        };
        addAndMakeVisible (row.enable);

        styleCombo (row.source);
        fillModSource (row.source);
        row.source.setTooltip ("Modulation source");
        row.source.onChange = [this]
        {
            updateLookDevVisibility();
            applyLookControlsToMain();
            requestParentRelayout();
        };
        addAndMakeVisible (row.source);

        styleToggle (row.thresholdToggle);
        row.thresholdToggle.setButtonText ("Thr");
        row.thresholdToggle.setTooltip (
            "Threshold gate + envelope. Off = no follower CPU for this slot.");
        row.thresholdToggle.onClick = [this]
        {
            updateLookDevVisibility();
            applyLookControlsToMain();
            requestParentRelayout();
        };
        addAndMakeVisible (row.thresholdToggle);

        styleSlider (row.thresholdSlider);
        row.thresholdSlider.setSliderStyle (juce::Slider::LinearVertical);
        row.thresholdSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row.thresholdSlider.setRange (0.0, 1.0, 0.01);
        row.thresholdSlider.setValue (0.25, juce::dontSendNotification);
        row.thresholdSlider.setTooltip ("Gate threshold (0-1). Signal below is cut.");
        row.thresholdSlider.onValueChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.thresholdSlider);

        row.attackKnob.setCustomRange (0.1, 500.0, 0.1);
        row.attackKnob.setValue (10.0, juce::dontSendNotification);
        row.attackKnob.setTooltip ("Envelope attack (ms)");
        row.attackKnob.setThemeColors (&sharedResources);
        row.attackKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row.attackKnob.onValueChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.attackKnob);

        row.releaseKnob.setCustomRange (0.1, 2000.0, 0.1);
        row.releaseKnob.setValue (80.0, juce::dontSendNotification);
        row.releaseKnob.setTooltip ("Envelope release (ms)");
        row.releaseKnob.setThemeColors (&sharedResources);
        row.releaseKnob.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        row.releaseKnob.onValueChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.releaseKnob);

        styleCombo (row.dest);
        fillModDest (row.dest);
        row.dest.setTooltip ("Modulation destination");
        row.dest.onChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.dest);

        styleCombo (row.op);
        fillModOp (row.op);
        row.op.setTooltip ("Set = lerp to source, Multiply = scale by source, Add = offset");
        row.op.onChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.op);

        row.curve.onShapeChanged = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.curve);

        // Matrix numeric fields: wide enough for "-12.345" at 3 d.p. - never "...".
        // Wide enough for "-12.345" / "100.000" at 3 d.p. (never ellipsis-only).
        constexpr int kMatrixNumBoxW = 80;
        constexpr int kMatrixNumBoxH = 18;
        auto styleMatrixNumeric = [this, kMatrixNumBoxW, kMatrixNumBoxH] (juce::Slider& s,
                                                                          double minV, double maxV,
                                                                          const juce::String& tip)
        {
            styleSlider (s);
            s.setSliderStyle (juce::Slider::LinearHorizontal);
            s.setRange (minV, maxV, 0.001);
            s.setNumDecimalPlacesToDisplay (3);
            s.setTextBoxStyle (juce::Slider::TextBoxRight, false, kMatrixNumBoxW, kMatrixNumBoxH);
            s.setTooltip (tip);
            s.onValueChange = [this] { applyLookControlsToMain(); };
            wireUncappedTextEntry (s);
        };

        // Dual-thumb range: one bar, two arrows (min / max). Closer = narrower map.
        row.rangeSlider.setSliderStyle (juce::Slider::TwoValueHorizontal);
        row.rangeSlider.setLookAndFeel (&particleRangeLnF);
        row.rangeSlider.setRange (0.0, 1.0, 0.001);
        row.rangeSlider.setMinValue (0.0, juce::dontSendNotification);
        row.rangeSlider.setMaxValue (1.0, juce::dontSendNotification);
        row.rangeSlider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
        row.rangeSlider.setSliderSnapsToMousePosition (false);
        row.rangeSlider.setTooltip (
            "Map range: drag the two arrows. Left = source 0, right = source 1. "
            "Pull apart for wider mapping, push together for a narrow band.");
        row.rangeSlider.onValueChange = [this, i]
        {
            auto& r = particleModRows[(size_t) i];
            r.rangeMinReadout.setText (juce::String (r.rangeSlider.getMinValue(), 3),
                                       juce::dontSendNotification);
            r.rangeMaxReadout.setText (juce::String (r.rangeSlider.getMaxValue(), 3),
                                       juce::dontSendNotification);
            applyLookControlsToMain();
        };
        addAndMakeVisible (row.rangeSlider);

        auto styleRangeReadout = [this] (juce::Label& lab)
        {
            lab.setFont (juce::FontOptions().withName ("Lato").withHeight (12.0f));
            lab.setJustificationType (juce::Justification::centred);
            lab.setMinimumHorizontalScale (0.55f); // prefer shrink over "..."
            lab.setColour (juce::Label::textColourId, juce::Colours::whitesmoke.withAlpha (0.88f));
            lab.setColour (juce::Label::backgroundColourId, juce::Colours::black.withAlpha (0.30f));
            lab.setInterceptsMouseClicks (false, false);
            addAndMakeVisible (lab);
        };
        styleRangeReadout (row.rangeMinReadout);
        styleRangeReadout (row.rangeMaxReadout);
        row.rangeMinReadout.setText ("0.000", juce::dontSendNotification);
        row.rangeMaxReadout.setText ("1.000", juce::dontSendNotification);

        styleToggle (row.invertToggle);
        row.invertToggle.setButtonText ("Inv");
        row.invertToggle.setTooltip ("Invert shaped source (1 - s). High source drives low destination.");
        row.invertToggle.onClick = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.invertToggle);

        styleMatrixNumeric (row.amount, 0.0, 4.0,
                            "Modulation depth / amount. Type beyond 0-4 for extremes.");
        setSliderActual (row.amount, 1.0);
        addAndMakeVisible (row.amount);

        styleMatrixNumeric (row.constant, 0.0, 1.0,
                            "Constant source value (type beyond 0-1 if needed).");
        setSliderActual (row.constant, 0.5);
        addAndMakeVisible (row.constant);

        wireUncappedTextEntry (row.thresholdSlider);
    }

    styleLabel (particleSourcesLabel);
    particleSourcesLabel.setText ("Sources", juce::dontSendNotification);
    particleSourcesLabel.setColour (juce::Label::textColourId, juce::Colours::goldenrod.withAlpha (0.95f));
    addAndMakeVisible (particleSourcesLabel);
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
    {
        auto& row = particleRandomRows[(size_t) i];
        styleLabel (row.title);
        row.title.setText ("Random " + juce::String (i + 1), juce::dontSendNotification);
        addAndMakeVisible (row.title);
        styleLabel (row.dimLabel);
        row.dimLabel.setText ("Dim", juce::dontSendNotification);
        styleCombo (row.dimCombo);
        row.dimCombo.addItem ("Float", 1);
        row.dimCombo.addItem ("Vec2", 2);
        row.dimCombo.addItem ("Vec3", 3);
        row.dimCombo.setSelectedId (1, juce::dontSendNotification);
        row.dimCombo.onChange = [this] { applyLookControlsToMain(); };
        addAndMakeVisible (row.dimLabel);
        addAndMakeVisible (row.dimCombo);
        styleLabel (row.modeLabel);
        row.modeLabel.setText ("Mode", juce::dontSendNotification);
        styleCombo (row.modeCombo);
        row.modeCombo.addItem ("Per particle", 1);
        row.modeCombo.addItem ("Per frame", 2);
        row.modeCombo.addItem ("Smoothed", 3);
        row.modeCombo.setSelectedId (1, juce::dontSendNotification);
        row.modeCombo.onChange = [this]
        {
            updateLookDevVisibility();
            applyLookControlsToMain();
            requestParentRelayout();
        };
        addAndMakeVisible (row.modeLabel);
        addAndMakeVisible (row.modeCombo);
        setupParticleSlider (row.minLabel, row.minSlider, 0.0, 1.0, 0.01, "Random min");
        row.minLabel.setText ("Min", juce::dontSendNotification);
        setSliderActual (row.minSlider, 0.0);
        setupParticleSlider (row.maxLabel, row.maxSlider, 0.0, 1.0, 0.01, "Random max");
        row.maxLabel.setText ("Max", juce::dontSendNotification);
        setSliderActual (row.maxSlider, 1.0);
        setupParticleSlider (row.smoothLabel, row.smoothSlider, 1.0, 500.0, 1.0, "Smooth ms");
        row.smoothLabel.setText ("Smooth ms", juce::dontSendNotification);
        setSliderActual (row.smoothSlider, 50.0);
    }

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
    styleLabel (fftSizeLabel);
    styleLabel (meshQualityLabel);
    styleLabel (meshHeightLabel);
    styleLabel (freqMeshBiasLabel);
    styleLabel (freqMeshBiasPivotLabel);
    styleLabel (msaaLabel);
    styleLabel (selfShadowQualityLabel);
    styleLabel (shadowResLabel);
    styleLabel (cascadeCountLabel);
    styleLabel (sssQualityLabel);
    styleLabel (dofQualityLabel);
    styleLabel (ssgiQualityLabel);
    styleLabel (ssrQualityLabel);
    styleLabel (exposureLabel);
    styleLabel (gradeLabel);

    updateLookDevVisibility();
}

Spectrogram3DSettingsComponent::Content::~Content()
{
    fftSizeCombo.setLookAndFeel (nullptr);
    meshQualityCombo.setLookAndFeel (nullptr);
    msaaCombo.setLookAndFeel (nullptr);
    audioLevelTargetCombo.setLookAndFeel (nullptr);
    audioLevelSpeedCombo.setLookAndFeel (nullptr);
    selfShadowQualityCombo.setLookAndFeel (nullptr);
    shadowResCombo.setLookAndFeel (nullptr);
    cascadeCountCombo.setLookAndFeel (nullptr);
    sssQualityCombo.setLookAndFeel (nullptr);
    dofQualityCombo.setLookAndFeel (nullptr);
    ssgiQualityCombo.setLookAndFeel (nullptr);
    ssrQualityCombo.setLookAndFeel (nullptr);
    gradeCombo.setLookAndFeel (nullptr);
    for (auto& row : particleModRows)
        row.rangeSlider.setLookAndFeel (nullptr);
}

void Spectrogram3DSettingsComponent::Content::setLookChildVisible (juce::Component& c, bool vis)
{
    c.setVisible (vis);
}

void Spectrogram3DSettingsComponent::Content::updateLookDevVisibility()
{
    const bool audioOn = audioLevelToggle.getToggleState();
    setLookChildVisible (audioLevelTargetLabel, audioOn);
    setLookChildVisible (audioLevelTargetCombo, audioOn);
    setLookChildVisible (audioLevelMinPctLabel, audioOn);
    setLookChildVisible (audioLevelMinPctSlider, audioOn);
    setLookChildVisible (audioLevelMaxPctLabel, audioOn);
    setLookChildVisible (audioLevelMaxPctSlider, audioOn);
    setLookChildVisible (audioLevelHpLabel, audioOn);
    setLookChildVisible (audioLevelHpSlider, audioOn);
    setLookChildVisible (audioLevelLpLabel, audioOn);
    setLookChildVisible (audioLevelLpSlider, audioOn);
    setLookChildVisible (audioLevelThresholdLabel, audioOn);
    setLookChildVisible (audioLevelThresholdSlider, audioOn);
    setLookChildVisible (audioLevelSpeedLabel, audioOn);
    setLookChildVisible (audioLevelSpeedCombo, audioOn);
    setLookChildVisible (audioAffectPlayheadToggle, audioOn);
    setLookChildVisible (audioAffectAntiPlayheadToggle, audioOn);

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
    setLookChildVisible (energyConserveToggle, lit);
    setLookChildVisible (rimLabel, lit);
    setLookChildVisible (rimSlider, lit);
    setLookChildVisible (lightColourEditor, lit);
    setLookChildVisible (rimColourEditor, lit);

    const bool dome = lit && domeFillToggle.getToggleState();
    setLookChildVisible (domeFillStrengthLabel, dome);
    setLookChildVisible (domeFillStrengthSlider, dome);
    setLookChildVisible (domeTextureToggle, dome);
    const bool domeTex = dome && domeTextureToggle.getToggleState();
    setLookChildVisible (domeTextureLabel, domeTex);
    setLookChildVisible (domeTextureCombo, domeTex);
    setLookChildVisible (domeSkyEditor, dome && ! domeTex);
    setLookChildVisible (domeGroundEditor, dome && ! domeTex);

    const bool ssgi = ssgiToggle.getToggleState();
    setLookChildVisible (ssgiStrengthLabel, ssgi);
    setLookChildVisible (ssgiStrengthSlider, ssgi);
    setLookChildVisible (ssgiRadiusLabel, ssgi);
    setLookChildVisible (ssgiRadiusSlider, ssgi);
    setLookChildVisible (ssgiQualityLabel, ssgi);
    setLookChildVisible (ssgiQualityCombo, ssgi);

    const bool ssr = ssrToggle.getToggleState();
    setLookChildVisible (ssrStrengthLabel, ssr);
    setLookChildVisible (ssrStrengthSlider, ssr);
    setLookChildVisible (ssrDistanceLabel, ssr);
    setLookChildVisible (ssrDistanceSlider, ssr);
    setLookChildVisible (ssrThicknessLabel, ssr);
    setLookChildVisible (ssrThicknessSlider, ssr);
    setLookChildVisible (ssrQualityLabel, ssr);
    setLookChildVisible (ssrQualityCombo, ssr);
    setLookChildVisible (ssrFresnelLabel, ssr);
    setLookChildVisible (ssrFresnelSlider, ssr);
    setLookChildVisible (ssrRoughInfLabel, ssr);
    setLookChildVisible (ssrRoughInfSlider, ssr);
    setLookChildVisible (ssrIntensityLabel, ssr);
    setLookChildVisible (ssrIntensitySlider, ssr);
    setLookChildVisible (ssrEdgeFadeLabel, ssr);
    setLookChildVisible (ssrEdgeFadeSlider, ssr);
    setLookChildVisible (ssrMetalBiasLabel, ssr);
    setLookChildVisible (ssrMetalBiasSlider, ssr);
    setLookChildVisible (ssrDomeFbLabel, ssr);
    setLookChildVisible (ssrDomeFbSlider, ssr);

    const bool contact = contactShadowToggle.getToggleState();
    setLookChildVisible (contactShadowStrengthLabel, contact);
    setLookChildVisible (contactShadowStrengthSlider, contact);

    const bool selfSh = selfShadowToggle.getToggleState();
    const bool castSh = castShadowsToggle.getToggleState();
    setLookChildVisible (selfShadowStrengthLabel, selfSh);
    setLookChildVisible (selfShadowStrengthSlider, selfSh);
    // Bias/Softness are shared with the cast-shadow map.
    setLookChildVisible (selfShadowBiasLabel, selfSh || castSh);
    setLookChildVisible (selfShadowBiasSlider, selfSh || castSh);
    setLookChildVisible (selfShadowSoftnessLabel, selfSh || castSh);
    setLookChildVisible (selfShadowSoftnessSlider, selfSh || castSh);
    setLookChildVisible (selfShadowQualityLabel, selfSh);
    setLookChildVisible (selfShadowQualityCombo, selfSh);

    setLookChildVisible (shadowResLabel, castSh);
    setLookChildVisible (shadowResCombo, castSh);
    setLookChildVisible (cascadeCountLabel, castSh);
    setLookChildVisible (cascadeCountCombo, castSh);
    setLookChildVisible (cascadeDistLabel, castSh);
    setLookChildVisible (cascadeDistSlider, castSh);
    setLookChildVisible (cascadeTransLabel, castSh);
    setLookChildVisible (cascadeTransSlider, castSh);

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

    const bool motionBlur = motionBlurToggle.getToggleState();
    setLookChildVisible (motionBlurAmountLabel, motionBlur);
    setLookChildVisible (motionBlurAmountSlider, motionBlur);
    setLookChildVisible (motionBlurMaxLabel, motionBlur);
    setLookChildVisible (motionBlurMaxSlider, motionBlur);
    setLookChildVisible (motionBlurQualityLabel, motionBlur);
    setLookChildVisible (motionBlurQualityCombo, motionBlur);

    const bool dof = dofToggle.getToggleState();
    setLookChildVisible (dofFocusLabel, dof);
    setLookChildVisible (dofFocusSlider, dof);
    setLookChildVisible (dofFStopLabel, dof);
    setLookChildVisible (dofFStopSlider, dof);
    setLookChildVisible (dofFocalLengthLabel, dof);
    setLookChildVisible (dofFocalLengthSlider, dof);
    setLookChildVisible (dofQualityLabel, dof);
    setLookChildVisible (dofQualityCombo, dof);
    setLookChildVisible (dofCocDilateLabel, dof);
    setLookChildVisible (dofCocDilateSlider, dof);
    setLookChildVisible (dofEdgeSpillLabel, dof);
    setLookChildVisible (dofEdgeSpillSlider, dof);

    const bool tonemap = tonemapToggle.getToggleState();
    setLookChildVisible (exposureLabel, tonemap);
    setLookChildVisible (exposureSlider, tonemap);
    setLookChildVisible (gradeLabel, tonemap);
    setLookChildVisible (gradeCombo, tonemap);

    const bool particleOn = particleToggle.getToggleState();
    setLookChildVisible (particleGpuSimToggle, particleOn);
    setLookChildVisible (particleMaxAliveLabel, particleOn);
    setLookChildVisible (particleMaxAliveSlider, particleOn);
    setLookChildVisible (particleDebugOverlayToggle, particleOn);
    setLookChildVisible (particleClearButton, particleOn);
    setLookChildVisible (particleBindingLabel, particleOn);
    setLookChildVisible (particleBindingCombo, particleOn);
    setLookChildVisible (particleEmitModeLabel, particleOn);
    setLookChildVisible (particleEmitModeCombo, particleOn);
    setLookChildVisible (particleEmissionLabel, particleOn);
    setLookChildVisible (particleEmissionSlider, particleOn);
    setLookChildVisible (particleSpawnJitterLabel, particleOn);
    setLookChildVisible (particleSpawnJitterSlider, particleOn);
    setLookChildVisible (particleInitVelXLabel, particleOn);
    setLookChildVisible (particleInitVelXSlider, particleOn);
    setLookChildVisible (particleInitVelYLabel, particleOn);
    setLookChildVisible (particleInitVelYSlider, particleOn);
    setLookChildVisible (particleInitVelZLabel, particleOn);
    setLookChildVisible (particleInitVelZSlider, particleOn);
    setLookChildVisible (particleVelRandomLabel, particleOn);
    setLookChildVisible (particleVelRandomSlider, particleOn);
    setLookChildVisible (particleLifespanLabel, particleOn);
    setLookChildVisible (particleLifespanSlider, particleOn);
    const bool lifeOn = particleOn && getSliderActual (particleLifespanSlider) > 1.0e-4;
    setLookChildVisible (particleLifespanRandomLabel, lifeOn);
    setLookChildVisible (particleLifespanRandomSlider, lifeOn);
    setLookChildVisible (particleSizeLabel, particleOn);
    setLookChildVisible (particleSizeSlider, particleOn);
    // Material: always show PBR + emissive together (waterfall parity). Unlit is optional.
    setLookChildVisible (particleEmissiveToggle, particleOn);
    setLookChildVisible (particleRoughLabel, particleOn);
    setLookChildVisible (particleRoughSlider, particleOn);
    setLookChildVisible (particleMetalLabel, particleOn);
    setLookChildVisible (particleMetalSlider, particleOn);
    setLookChildVisible (particleSpecLabel, particleOn);
    setLookChildVisible (particleSpecSlider, particleOn);
    setLookChildVisible (particleEmissiveStrLabel, particleOn);
    setLookChildVisible (particleEmissiveStrSlider, particleOn);
    setLookChildVisible (particleMeshLabel, particleOn);
    setLookChildVisible (particleMeshCombo, particleOn);
    const bool meshRotOn = particleOn && particleMeshCombo.getSelectedId() != 3; // not billboard
    setLookChildVisible (particleInitRotXLabel, meshRotOn);
    setLookChildVisible (particleInitRotXSlider, meshRotOn);
    setLookChildVisible (particleInitRotYLabel, meshRotOn);
    setLookChildVisible (particleInitRotYSlider, meshRotOn);
    setLookChildVisible (particleInitRotZLabel, meshRotOn);
    setLookChildVisible (particleInitRotZSlider, meshRotOn);
    setLookChildVisible (particleInitRotRndLabel, meshRotOn);
    setLookChildVisible (particleInitRotRndSlider, meshRotOn);
    setLookChildVisible (particleForcesToggle, particleOn);
    setLookChildVisible (particleWaterfallLockToggle, particleOn);
    const bool forcesOn = particleOn && particleForcesToggle.getToggleState();
    if (particleForceStack != nullptr)
        particleForceStack->setVisible (forcesOn);

    setLookChildVisible (particleModLabel, particleOn);
    setLookChildVisible (particleModHintLabel, particleOn);
    setLookChildVisible (particleModHdrOn, particleOn);
    setLookChildVisible (particleModHdrSrc, particleOn);
    setLookChildVisible (particleModHdrThr, particleOn);
    setLookChildVisible (particleModHdrDst, particleOn);
    setLookChildVisible (particleModHdrOp, particleOn);
    setLookChildVisible (particleModHdrCurve, particleOn);
    setLookChildVisible (particleModHdrRange, particleOn);
    setLookChildVisible (particleModHdrInv, particleOn);
    setLookChildVisible (particleModHdrAmt, particleOn);
    bool anyRandom = false;
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        auto& row = particleModRows[(size_t) i];
        setLookChildVisible (row.enable, particleOn);
        setLookChildVisible (row.source, particleOn);
        setLookChildVisible (row.thresholdToggle, particleOn);
        const bool thrOn = particleOn && row.thresholdToggle.getToggleState();
        setLookChildVisible (row.thresholdSlider, thrOn);
        setLookChildVisible (row.attackKnob, thrOn);
        setLookChildVisible (row.releaseKnob, thrOn);
        setLookChildVisible (row.dest, particleOn);
        setLookChildVisible (row.op, particleOn);
        setLookChildVisible (row.curve, particleOn);
        setLookChildVisible (row.rangeSlider, particleOn);
        setLookChildVisible (row.rangeMinReadout, particleOn);
        setLookChildVisible (row.rangeMaxReadout, particleOn);
        setLookChildVisible (row.invertToggle, particleOn);
        setLookChildVisible (row.amount, particleOn);
        const int srcId = row.source.getSelectedId();
        const bool showConst = particleOn && srcId == 7;
        setLookChildVisible (row.constant, showConst);
        if (particleOn && srcId >= 8 && srcId <= 10)
            anyRandom = true;
    }
    setLookChildVisible (particleSourcesLabel, anyRandom);
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
    {
        auto& row = particleRandomRows[(size_t) i];
        bool used = false;
        if (anyRandom)
            for (int r = 0; r < kParticleModSlotCount; ++r)
                if (particleModRows[(size_t) r].source.getSelectedId() == 8 + i)
                    used = true;
        setLookChildVisible (row.title, used);
        setLookChildVisible (row.dimLabel, used);
        setLookChildVisible (row.dimCombo, used);
        setLookChildVisible (row.modeLabel, used);
        setLookChildVisible (row.modeCombo, used);
        setLookChildVisible (row.minLabel, used);
        setLookChildVisible (row.minSlider, used);
        setLookChildVisible (row.maxLabel, used);
        setLookChildVisible (row.maxSlider, used);
        const bool smooth = used && row.modeCombo.getSelectedId() == 3;
        setLookChildVisible (row.smoothLabel, smooth);
        setLookChildVisible (row.smoothSlider, smooth);
    }

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

    // Viewport scroll range lives on Menu::contentPanel - refresh it when Look rows grow/shrink.
    if (auto* menu = findParentComponentOfClass<Menu>())
        menu->notifyContentHeightChanged();
}

void Spectrogram3DSettingsComponent::Content::styleSlider (juce::Slider& slider)
{
    slider.setSliderStyle (juce::Slider::LinearHorizontal);
    // Wide enough for "-12.345" / "100.000" at 3 d.p. without ellipsis.
    slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 80, 20);
    slider.setNumDecimalPlacesToDisplay (3);
    slider.setColour (juce::Slider::trackColourId, juce::Colours::darkgoldenrod.withAlpha (0.55f));
    slider.setColour (juce::Slider::thumbColourId, juce::Colours::goldenrod);
    slider.setColour (juce::Slider::backgroundColourId, juce::Colours::black.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::whitesmoke.withAlpha (0.9f));
    slider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::black.withAlpha (0.35f));
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::whitesmoke.withAlpha (0.2f));
}

namespace
{
    const juce::Identifier kParticleSliderActual ("particleSliderActual");
}

void Spectrogram3DSettingsComponent::Content::wireUncappedTextEntry (juce::Slider& slider)
{
    // Drag stays within setRange; typed values outside range are stored and applied.
    slider.setTextBoxIsEditable (true);
    slider.setNumDecimalPlacesToDisplay (3);
    slider.valueFromTextFunction = [&slider] (const juce::String& text)
    {
        const double typed = text.getDoubleValue();
        if (std::isfinite (typed))
            slider.getProperties().set (kParticleSliderActual, typed);
        // Thumb position still within drag range.
        return juce::jlimit (slider.getMinimum(), slider.getMaximum(), typed);
    };
    slider.textFromValueFunction = [&slider] (double v)
    {
        const double shown = slider.getProperties().contains (kParticleSliderActual)
                                 ? (double) slider.getProperties()[kParticleSliderActual]
                                 : v;
        // Integers for large rates (emission) stay readable; else 3 d.p.
        if (std::abs (shown) >= 100.0 && std::abs (shown - std::round (shown)) < 1.0e-6)
            return juce::String ((int) std::round (shown));
        return juce::String (shown, 3);
    };
    // Always sync actual on any value change (drag or text). valueFromTextFunction
    // already stored typed out-of-range values before this runs.
    const auto prev = slider.onValueChange;
    slider.onValueChange = [&slider, prev]
    {
        if (slider.isMouseButtonDown())
        {
            // Drag: thumb value is the actual.
            slider.getProperties().set (kParticleSliderActual, slider.getValue());
        }
        else if (! slider.getProperties().contains (kParticleSliderActual))
        {
            slider.getProperties().set (kParticleSliderActual, slider.getValue());
        }
        // else: keep typed actual from valueFromTextFunction
        if (prev)
            prev();
    };
}

double Spectrogram3DSettingsComponent::Content::getSliderActual (const juce::Slider& slider)
{
    if (slider.getProperties().contains (kParticleSliderActual))
        return (double) slider.getProperties()[kParticleSliderActual];
    return slider.getValue();
}

void Spectrogram3DSettingsComponent::Content::setSliderActual (juce::Slider& slider, double actual)
{
    if (! std::isfinite (actual))
        actual = slider.getValue();
    slider.getProperties().set (kParticleSliderActual, actual);
    const double thumb = juce::jlimit (slider.getMinimum(), slider.getMaximum(), actual);
    slider.setValue (thumb, juce::dontSendNotification);
    slider.updateText();
}

void Spectrogram3DSettingsComponent::Content::styleLabel (juce::Label& label)
{
    label.setFont (juce::FontOptions().withName ("Lato Black").withHeight (15.0f));
    label.setJustificationType (juce::Justification::centredLeft);
    label.setMinimumHorizontalScale (0.55f); // shrink before "..." on narrow panels
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

void Spectrogram3DSettingsComponent::Content::saveModuleLookDefault()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    const bool ok = main != nullptr
                        && main->saveModuleLookDefault (ModuleLookPresets::Kind::spectrogram3D);
    saveDefaultButton.setButtonText (ok ? "Saved!" : "Failed");
    juce::Timer::callAfterDelay (1400, [safe = juce::Component::SafePointer<juce::TextButton> (&saveDefaultButton)]
    {
        if (safe != nullptr)
            safe->setButtonText ("Save Default");
    });
}

void Spectrogram3DSettingsComponent::Content::saveModuleLookPreset()
{
    if (auto* main = findParentComponentOfClass<MainComponent>())
        main->promptSaveModuleLookPreset (ModuleLookPresets::Kind::spectrogram3D);
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

void Spectrogram3DSettingsComponent::Content::layoutColourEditor (juce::Rectangle<int>& area,
                                                                  ColourSwatchEditor& editor)
{
    editor.setBounds (area.removeFromTop (editor.getPreferredHeight())
                          .removeFromLeft (juce::jmin (420, area.getWidth())));
}

void Spectrogram3DSettingsComponent::Content::wireColourEditor (ColourSwatchEditor& editor)
{
    editor.onColourChanged = [this] (juce::Colour) { applyLookControlsToMain(); };
    editor.onHeightChanged = [this]
    {
        requestParentRelayout();
        resized();
    };
    addAndMakeVisible (editor);
}

void Spectrogram3DSettingsComponent::Content::browseDomeTextureFile()
{
    domeTextureChooser = std::make_unique<juce::FileChooser> (
        "Load dome texture (equirectangular JPG/PNG)",
        juce::File(),
        "*.jpg;*.jpeg;*.png;*.webp");

    constexpr auto flags = juce::FileBrowserComponent::openMode
                         | juce::FileBrowserComponent::canSelectFiles;
    domeTextureChooser->launchAsync (flags, [this] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (f.existsAsFile())
        {
            if (auto* main = findParentComponentOfClass<MainComponent>())
            {
                main->setSpec3DDomeTextureCustomPath (f.getFullPathName(), false);
                main->setSpec3DDomeTextureSource (
                    Spectrogram3DComponent::DomeTextureSource::custom, false);
                main->setSpec3DDomeTextureEnabled (true, true);
            }
            domeTextureToggle.setToggleState (true, juce::dontSendNotification);
            // Keep "Load custom..." selected so the choice stays visible.
            domeTextureCombo.setSelectedId (2, juce::dontSendNotification);
            updateLookDevVisibility();
            requestParentRelayout();
        }
        else
        {
            // Cancelled - restore previous source in the combo.
            if (auto* main = findParentComponentOfClass<MainComponent>())
            {
                const bool custom = main->getSpec3DDomeTextureSource()
                    == Spectrogram3DComponent::DomeTextureSource::custom
                    && main->getSpec3DDomeTextureCustomPath().isNotEmpty();
                domeTextureCombo.setSelectedId (custom ? 2 : 1, juce::dontSendNotification);
            }
            else
            {
                domeTextureCombo.setSelectedId (1, juce::dontSendNotification);
            }
        }
    });
}

void Spectrogram3DSettingsComponent::Content::syncGradientFromBank()
{
    gradientEditor.setRamp (&colourRamps.get (ColourRampBank::Target::spectrogram3D));
    gradientEditor.repaint();
}

void Spectrogram3DSettingsComponent::Content::syncDofFocusFromMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;
    dofFocusSlider.setValue (main->getSpec3DDofFocusDistance(), juce::dontSendNotification);
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
               : (q == Spectrogram3DComponent::MeshQuality::ultra
                  || q == Spectrogram3DComponent::MeshQuality::overkill ? 4 : 2)),
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
    freqMeshBiasPivotSlider.setValue (main->getSpec3DFreqMeshBiasPivot(), juce::dontSendNotification);
    softAngleSlider.setValue (main->getSpec3DNormalCuspAngleDeg(), juce::dontSendNotification);

    audioLevelToggle.setToggleState (main->isSpec3DAudioLevelModEnabled(), juce::dontSendNotification);
    audioLevelTargetCombo.setSelectedId (
        (int) main->getSpec3DAudioLevelTarget() + 1, juce::dontSendNotification);
    audioLevelMinPctSlider.setValue (main->getSpec3DAudioLevelMinPercent(), juce::dontSendNotification);
    audioLevelMaxPctSlider.setValue (main->getSpec3DAudioLevelMaxPercent(), juce::dontSendNotification);
    audioLevelHpSlider.setValue (main->getSpec3DAudioLevelHpHz(), juce::dontSendNotification);
    audioLevelLpSlider.setValue (main->getSpec3DAudioLevelLpHz(), juce::dontSendNotification);
    audioLevelThresholdSlider.setValue (main->getSpec3DAudioLevelThresholdDb(),
                                        juce::dontSendNotification);
    {
        const auto spd = main->getSpec3DAudioLevelSpeed();
        audioLevelSpeedCombo.setSelectedId (
            spd == Spectrogram3DComponent::AudioLevelSpeed::slow ? 3
                : (spd == Spectrogram3DComponent::AudioLevelSpeed::med ? 2 : 1),
            juce::dontSendNotification);
    }
    audioAffectPlayheadToggle.setToggleState (main->getSpec3DAudioLevelAffectPlayhead(),
                                              juce::dontSendNotification);
    audioAffectAntiPlayheadToggle.setToggleState (main->getSpec3DAudioLevelAffectAntiPlayhead(),
                                                  juce::dontSendNotification);

    lightingToggle.setToggleState (main->isSpec3DLightingEnabled(), juce::dontSendNotification);
    lightingAmountSlider.setValue (main->getSpec3DLightingAmount(), juce::dontSendNotification);
    lightAzimuthSlider.setValue (main->getSpec3DLightAzimuthDeg(), juce::dontSendNotification);
    lightElevationSlider.setValue (main->getSpec3DLightElevationDeg(), juce::dontSendNotification);
    specularSlider.setValue (main->getSpec3DSpecularAmount(), juce::dontSendNotification);
    roughnessSlider.setValue (main->getSpec3DRoughnessAmount(), juce::dontSendNotification);
    metalnessSlider.setValue (main->getSpec3DMetalnessAmount(), juce::dontSendNotification);
    rimSlider.setValue (main->getSpec3DRimAmount(), juce::dontSendNotification);
    lightColourEditor.setColour (main->getSpec3DLightColour());
    rimColourEditor.setColour (main->getSpec3DRimColour());
    domeFillToggle.setToggleState (main->isSpec3DDomeFillEnabled(), juce::dontSendNotification);
    domeFillStrengthSlider.setValue (main->getSpec3DDomeFillStrength(), juce::dontSendNotification);
    domeTextureToggle.setToggleState (main->isSpec3DDomeTextureEnabled(), juce::dontSendNotification);
    {
        const bool custom = main->getSpec3DDomeTextureSource()
                                == Spectrogram3DComponent::DomeTextureSource::custom
                            && main->getSpec3DDomeTextureCustomPath().isNotEmpty();
        domeTextureCombo.setSelectedId (custom ? 2 : 1, juce::dontSendNotification);
    }
    domeSkyEditor.setColour (main->getSpec3DDomeSkyColour());
    domeGroundEditor.setColour (main->getSpec3DDomeGroundColour());
    ssgiToggle.setToggleState (main->isSpec3DSsgiEnabled(), juce::dontSendNotification);
    ssgiStrengthSlider.setValue (main->getSpec3DSsgiStrength(), juce::dontSendNotification);
    ssgiRadiusSlider.setValue (main->getSpec3DSsgiRadius(), juce::dontSendNotification);
    {
        const auto sq = main->getSpec3DSsgiQuality();
        ssgiQualityCombo.setSelectedId (
            sq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (sq == Spectrogram3DComponent::ShadowQuality::medium ? 2
                : (sq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 4)),
            juce::dontSendNotification);
    }
    ssrToggle.setToggleState (main->isSpec3DSsrEnabled(), juce::dontSendNotification);
    ssrStrengthSlider.setValue (main->getSpec3DSsrStrength(), juce::dontSendNotification);
    ssrDistanceSlider.setValue (main->getSpec3DSsrDistance(), juce::dontSendNotification);
    ssrThicknessSlider.setValue (main->getSpec3DSsrThickness(), juce::dontSendNotification);
    {
        const auto sq = main->getSpec3DSsrQuality();
        ssrQualityCombo.setSelectedId (
            sq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (sq == Spectrogram3DComponent::ShadowQuality::medium ? 2
                : (sq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 4)),
            juce::dontSendNotification);
    }
    ssrFresnelSlider.setValue (main->getSpec3DSsrFresnel(), juce::dontSendNotification);
    ssrRoughInfSlider.setValue (main->getSpec3DSsrRoughnessInfluence(), juce::dontSendNotification);
    ssrIntensitySlider.setValue (main->getSpec3DSsrIntensity(), juce::dontSendNotification);
    ssrEdgeFadeSlider.setValue (main->getSpec3DSsrEdgeFade(), juce::dontSendNotification);
    ssrMetalBiasSlider.setValue (main->getSpec3DSsrMetallicBias(), juce::dontSendNotification);
    ssrDomeFbSlider.setValue (main->getSpec3DSsrDomeFallback(), juce::dontSendNotification);
    energyConserveToggle.setToggleState (main->isSpec3DEnergyConservingEnabled(), juce::dontSendNotification);
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
    castShadowsToggle.setToggleState (main->isSpec3DCastShadowsEnabled(), juce::dontSendNotification);
    {
        const auto r = main->getSpec3DShadowMapResolution();
        const int id = r == Spectrogram3DComponent::ShadowMapResolution::r512 ? 1
                     : (r == Spectrogram3DComponent::ShadowMapResolution::r2048 ? 3
                        : (r == Spectrogram3DComponent::ShadowMapResolution::r4096 ? 4 : 2));
        shadowResCombo.setSelectedId (id, juce::dontSendNotification);
    }
    cascadeCountCombo.setSelectedId (juce::jlimit (1, 4, main->getSpec3DShadowCascadeCount()),
                                     juce::dontSendNotification);
    cascadeDistSlider.setValue (main->getSpec3DShadowCascadeDistributionExponent(),
                                juce::dontSendNotification);
    cascadeTransSlider.setValue (main->getSpec3DShadowCascadeTransitionFraction(),
                                 juce::dontSendNotification);
    ssaoToggle.setToggleState (main->isSpec3DSsaoEnabled(), juce::dontSendNotification);
    ssaoStrengthSlider.setValue (main->getSpec3DSsaoStrength(), juce::dontSendNotification);
    ssaoRadiusSlider.setValue (main->getSpec3DSsaoRadius(), juce::dontSendNotification);
    bloomToggle.setToggleState (main->isSpec3DBloomEnabled(), juce::dontSendNotification);
    bloomStrengthSlider.setValue (main->getSpec3DBloomStrength(), juce::dontSendNotification);
    bloomThresholdSlider.setValue (main->getSpec3DBloomThreshold(), juce::dontSendNotification);
    motionBlurToggle.setToggleState (main->isSpec3DMotionBlurEnabled(), juce::dontSendNotification);
    motionBlurAmountSlider.setValue (main->getSpec3DMotionBlurAmount(), juce::dontSendNotification);
    motionBlurMaxSlider.setValue (main->getSpec3DMotionBlurMax(), juce::dontSendNotification);
    {
        const auto mq = main->getSpec3DMotionBlurQuality();
        motionBlurQualityCombo.setSelectedId (
            mq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (mq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 2),
            juce::dontSendNotification);
    }
    dofToggle.setToggleState (main->isSpec3DDofEnabled(), juce::dontSendNotification);
    dofFocusSlider.setValue (main->getSpec3DDofFocusDistance(), juce::dontSendNotification);
    dofFStopSlider.setValue (main->getSpec3DDofFStop(), juce::dontSendNotification);
    dofFocalLengthSlider.setValue (main->getSpec3DDofFocalLengthMm(), juce::dontSendNotification);
    {
        const auto dq = main->getSpec3DDofQuality();
        dofQualityCombo.setSelectedId (
            dq == Spectrogram3DComponent::ShadowQuality::low ? 1
                : (dq == Spectrogram3DComponent::ShadowQuality::high ? 3 : 2),
            juce::dontSendNotification);
    }
    dofCocDilateSlider.setValue (main->getSpec3DDofCocDilate(), juce::dontSendNotification);
    dofEdgeSpillSlider.setValue (main->getSpec3DDofEdgeSpill(), juce::dontSendNotification);
    tonemapToggle.setToggleState (main->isSpec3DTonemapEnabled(), juce::dontSendNotification);
    exposureSlider.setValue (main->getSpec3DTonemapExposureStops(), juce::dontSendNotification);
    gradeCombo.setSelectedId ((int) main->getSpec3DColorGrade() + 1, juce::dontSendNotification);
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
    particleToggle.setToggleState (main->isSpec3DParticleModeEnabled(), juce::dontSendNotification);
    particleGpuSimToggle.setToggleState (main->isSpec3DParticleGpuSimEnabled(), juce::dontSendNotification);
    setSliderActual (particleMaxAliveSlider, (double) main->getSpec3DParticleMaxAlive());
    particleDebugOverlayToggle.setToggleState (main->isSpec3DParticleDebugOverlayEnabled(),
                                               juce::dontSendNotification);
    {
        const bool gpuOk = main->isSpec3DParticleGpuSimAvailable();
        particleGpuSimToggle.setTooltip (
            juce::String (
                "Run force / age integration on the GPU (OpenGL 4.3 compute). "
                "CPU is the default and always works. GPU needs a 4.3+ context; "
                "if compute fails to start, motion falls back to CPU automatically. "
                "Spawn and colour matrix stay on the CPU either way.")
            + (gpuOk ? "\n\nCompute is ready on this GPU."
                     : "\n\nCompute not ready yet (needs particle draw once, or GL 4.3 unavailable)."));
    }
    particleBindingCombo.setSelectedId (main->getSpec3DParticleBindingMode() + 1, juce::dontSendNotification);
    particleEmitModeCombo.setSelectedId (main->getSpec3DParticleEmitMode() + 1, juce::dontSendNotification);
    setSliderActual (particleEmissionSlider, main->getSpec3DParticleEmission());
    setSliderActual (particleSpawnJitterSlider, main->getSpec3DParticleSpawnJitter());
    setSliderActual (particleInitVelXSlider, main->getSpec3DParticleInitVelX());
    setSliderActual (particleInitVelYSlider, main->getSpec3DParticleInitVelY());
    setSliderActual (particleInitVelZSlider, main->getSpec3DParticleInitVelZ());
    setSliderActual (particleVelRandomSlider, main->getSpec3DParticleVelRandom());
    setSliderActual (particleLifespanSlider, main->getSpec3DParticleLifespan());
    setSliderActual (particleLifespanRandomSlider, main->getSpec3DParticleLifespanRandom());
    setSliderActual (particleSizeSlider, main->getSpec3DParticleSize());
    particleEmissiveToggle.setToggleState (main->isSpec3DParticleEmissiveEnabled(), juce::dontSendNotification);
    setSliderActual (particleEmissiveStrSlider, main->getSpec3DParticleEmissiveStrength());
    setSliderActual (particleRoughSlider, main->getSpec3DParticleRoughness());
    setSliderActual (particleMetalSlider, main->getSpec3DParticleMetalness());
    setSliderActual (particleSpecSlider, main->getSpec3DParticleSpecular());
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        const auto slot = main->getSpec3DParticleModSlot (i);
        auto& row = particleModRows[(size_t) i];
        row.enable.setToggleState (slot.enabled, juce::dontSendNotification);
        row.source.setSelectedId ((int) slot.source + 1, juce::dontSendNotification);
        row.thresholdToggle.setToggleState (slot.thresholdEnabled, juce::dontSendNotification);
        row.thresholdSlider.setValue (slot.threshold, juce::dontSendNotification);
        row.attackKnob.setValue (slot.attackMs, juce::dontSendNotification);
        row.releaseKnob.setValue (slot.releaseMs, juce::dontSendNotification);
        row.dest.setSelectedId ((int) slot.dest + 1, juce::dontSendNotification);
        row.op.setSelectedId ((int) slot.op + 1, juce::dontSendNotification);
        row.curve.setShape (slot.curveShape);
        {
            const double lo = juce::jmin ((double) slot.mapMin, (double) slot.mapMax);
            const double hi = juce::jmax ((double) slot.mapMin, (double) slot.mapMax);
            // Keep thumbs in drag range; values outside 0-1 still apply via clamp to bar ends
            // if needed (map stores full floats from prefs).
            const double r0 = juce::jlimit (row.rangeSlider.getMinimum(),
                                            row.rangeSlider.getMaximum(), lo);
            const double r1 = juce::jlimit (row.rangeSlider.getMinimum(),
                                            row.rangeSlider.getMaximum(), hi);
            row.rangeSlider.setMinValue (r0, juce::dontSendNotification);
            row.rangeSlider.setMaxValue (r1, juce::dontSendNotification);
            row.rangeMinReadout.setText (juce::String (lo, 3), juce::dontSendNotification);
            row.rangeMaxReadout.setText (juce::String (hi, 3), juce::dontSendNotification);
            // Preserve extremes beyond the bar in properties when they differ.
            row.rangeSlider.getProperties().set ("mapMinActual", (double) slot.mapMin);
            row.rangeSlider.getProperties().set ("mapMaxActual", (double) slot.mapMax);
        }
        row.invertToggle.setToggleState (slot.invert, juce::dontSendNotification);
        setSliderActual (row.amount, slot.amount);
        setSliderActual (row.constant, slot.constant);
        setSliderActual (row.thresholdSlider, slot.threshold);
        row.attackKnob.setValue (slot.attackMs, juce::dontSendNotification);
        row.releaseKnob.setValue (slot.releaseMs, juce::dontSendNotification);
    }
    particleMeshCombo.setSelectedId (main->getSpec3DParticleMeshShape() + 1, juce::dontSendNotification);
    setSliderActual (particleInitRotXSlider, main->getSpec3DParticleInitRotX());
    setSliderActual (particleInitRotYSlider, main->getSpec3DParticleInitRotY());
    setSliderActual (particleInitRotZSlider, main->getSpec3DParticleInitRotZ());
    setSliderActual (particleInitRotRndSlider, main->getSpec3DParticleInitRotRandom());
    particleForcesToggle.setToggleState (main->isSpec3DParticleForcesEnabled(), juce::dontSendNotification);
    particleWaterfallLockToggle.setToggleState (main->isSpec3DParticleWaterfallLock(), juce::dontSendNotification);
    if (particleForceStack != nullptr)
        particleForceStack->setModules (main->getSpec3DParticleForceStack());
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
    {
        const auto rs = main->getSpec3DParticleRandomSource (i);
        auto& row = particleRandomRows[(size_t) i];
        row.dimCombo.setSelectedId ((int) rs.dim + 1, juce::dontSendNotification);
        row.modeCombo.setSelectedId ((int) rs.mode + 1, juce::dontSendNotification);
        setSliderActual (row.minSlider, rs.minV);
        setSliderActual (row.maxSlider, rs.maxV);
        setSliderActual (row.smoothSlider, rs.smoothMs);
    }

    updateLookDevVisibility();
}

void Spectrogram3DSettingsComponent::Content::applyStructureControlsToMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    constexpr bool kSave = false;
    main->setSpec3DMode (enable3DToggle.getToggleState(), kSave);
    const int id = meshQualityCombo.getSelectedId();
    main->setSpec3DMeshQuality (
        id == 1 ? Spectrogram3DComponent::MeshQuality::low
                : (id == 3 ? Spectrogram3DComponent::MeshQuality::high
                           : (id == 4 ? Spectrogram3DComponent::MeshQuality::ultra
                                      : Spectrogram3DComponent::MeshQuality::medium)),
        kSave);
    {
        const int msaaId = msaaCombo.getSelectedId();
        const auto level = msaaId == 1 ? Spectrogram3DComponent::MsaaLevel::off
                         : (msaaId == 3 ? Spectrogram3DComponent::MsaaLevel::x8
                            : (msaaId == 4 ? Spectrogram3DComponent::MsaaLevel::x16
                                           : Spectrogram3DComponent::MsaaLevel::x4));
        main->setSpec3DMsaaLevel (level, kSave);
    }
    main->setSpec3DTransparentBackground (transparentBgToggle.getToggleState(), kSave);
    main->setSpec3DReverseFrequencyAxis (reverseFreqAxisToggle.getToggleState(), kSave);
    main->setSpec3DClosedMeshEnabled (closedMeshToggle.getToggleState(), kSave);
    main->setSpec3DMeshHeight ((float) meshHeightSlider.getValue(), kSave);
    main->setSpec3DFreqMeshBias ((float) freqMeshBiasSlider.getValue(), kSave);
    main->setSpec3DFreqMeshBiasPivot ((float) freqMeshBiasPivotSlider.getValue(), kSave);
    // Soften path: Soft Angle + Angle+Area (Labs Soften about  cusp 180; organic default).
    main->setSpec3DNormalWeighting (Spectrogram3DComponent::NormalWeighting::angleAndArea, kSave);
    main->setSpec3DNormalCuspAngleDeg ((float) softAngleSlider.getValue(), kSave);
    main->requestUiPrefsSave();
}

void Spectrogram3DSettingsComponent::Content::applyLookControlsToMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    // Look-only: re-shade / re-light existing mesh history - never mode/quality/MSAA.
    // notifyPrefs=false during scrub; one debounced write at the end (was hitching every tick).
    constexpr bool kSave = false;
    main->setSpec3DAudioLevelModEnabled (audioLevelToggle.getToggleState(), kSave);
    {
        const int id = juce::jlimit (1, 7, audioLevelTargetCombo.getSelectedId());
        main->setSpec3DAudioLevelTarget (
            static_cast<Spectrogram3DComponent::AudioLevelTarget> (id - 1), kSave);
    }
    main->setSpec3DAudioLevelMinPercent ((float) audioLevelMinPctSlider.getValue(), kSave);
    main->setSpec3DAudioLevelMaxPercent ((float) audioLevelMaxPctSlider.getValue(), kSave);
    main->setSpec3DAudioLevelHpHz ((float) audioLevelHpSlider.getValue(), kSave);
    main->setSpec3DAudioLevelLpHz ((float) audioLevelLpSlider.getValue(), kSave);
    main->setSpec3DAudioLevelThresholdDb ((float) audioLevelThresholdSlider.getValue(), kSave);
    {
        const int id = audioLevelSpeedCombo.getSelectedId();
        main->setSpec3DAudioLevelSpeed (
            id == 3 ? Spectrogram3DComponent::AudioLevelSpeed::slow
                    : (id == 2 ? Spectrogram3DComponent::AudioLevelSpeed::med
                               : Spectrogram3DComponent::AudioLevelSpeed::fast),
            kSave);
    }
    main->setSpec3DAudioLevelAffectPlayhead (audioAffectPlayheadToggle.getToggleState(), kSave);
    main->setSpec3DAudioLevelAffectAntiPlayhead (audioAffectAntiPlayheadToggle.getToggleState(), kSave);

    main->setSpec3DLightingEnabled (lightingToggle.getToggleState(), kSave);
    main->setSpec3DLightingAmount ((float) lightingAmountSlider.getValue(), kSave);
    main->setSpec3DLightAzimuthDeg ((float) lightAzimuthSlider.getValue(), kSave);
    main->setSpec3DLightElevationDeg ((float) lightElevationSlider.getValue(), kSave);
    main->setSpec3DSpecularAmount ((float) specularSlider.getValue(), kSave);
    main->setSpec3DRoughnessAmount ((float) roughnessSlider.getValue(), kSave);
    main->setSpec3DMetalnessAmount ((float) metalnessSlider.getValue(), kSave);
    main->setSpec3DRimAmount ((float) rimSlider.getValue(), kSave);
    main->setSpec3DLightColour (lightColourEditor.getColour(), kSave);
    main->setSpec3DRimColour (rimColourEditor.getColour(), kSave);
    main->setSpec3DDomeFillEnabled (domeFillToggle.getToggleState(), kSave);
    main->setSpec3DDomeFillStrength ((float) domeFillStrengthSlider.getValue(), kSave);
    main->setSpec3DDomeSkyColour (domeSkyEditor.getColour(), kSave);
    main->setSpec3DDomeGroundColour (domeGroundEditor.getColour(), kSave);
    {
        const int texId = domeTextureCombo.getSelectedId();
        if (texId == 1)
            main->setSpec3DDomeTextureSource (Spectrogram3DComponent::DomeTextureSource::veniceSunset, false);
        else if (texId == 2 && main->getSpec3DDomeTextureCustomPath().isNotEmpty())
            main->setSpec3DDomeTextureSource (Spectrogram3DComponent::DomeTextureSource::custom, false);
        main->setSpec3DDomeTextureEnabled (domeTextureToggle.getToggleState(), kSave);
    }
    main->setSpec3DSsgiEnabled (ssgiToggle.getToggleState(), kSave);
    main->setSpec3DSsgiStrength ((float) ssgiStrengthSlider.getValue(), kSave);
    main->setSpec3DSsgiRadius ((float) ssgiRadiusSlider.getValue(), kSave);
    {
        const int id = ssgiQualityCombo.getSelectedId();
        main->setSpec3DSsgiQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 2 ? Spectrogram3DComponent::ShadowQuality::medium
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::ultra)),
            kSave);
    }
    main->setSpec3DSsrEnabled (ssrToggle.getToggleState(), kSave);
    main->setSpec3DSsrStrength ((float) ssrStrengthSlider.getValue(), kSave);
    main->setSpec3DSsrDistance ((float) ssrDistanceSlider.getValue(), kSave);
    main->setSpec3DSsrThickness ((float) ssrThicknessSlider.getValue(), kSave);
    {
        const int id = ssrQualityCombo.getSelectedId();
        main->setSpec3DSsrQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 2 ? Spectrogram3DComponent::ShadowQuality::medium
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::ultra)),
            kSave);
    }
    main->setSpec3DSsrFresnel ((float) ssrFresnelSlider.getValue(), kSave);
    main->setSpec3DSsrRoughnessInfluence ((float) ssrRoughInfSlider.getValue(), kSave);
    main->setSpec3DSsrIntensity ((float) ssrIntensitySlider.getValue(), kSave);
    main->setSpec3DSsrEdgeFade ((float) ssrEdgeFadeSlider.getValue(), kSave);
    main->setSpec3DSsrMetallicBias ((float) ssrMetalBiasSlider.getValue(), kSave);
    main->setSpec3DSsrDomeFallback ((float) ssrDomeFbSlider.getValue(), kSave);
    main->setSpec3DEnergyConservingEnabled (energyConserveToggle.getToggleState(), kSave);
    main->setSpec3DContactShadowEnabled (contactShadowToggle.getToggleState(), kSave);
    main->setSpec3DContactShadowStrength ((float) contactShadowStrengthSlider.getValue(), kSave);
    main->setSpec3DSelfShadowEnabled (selfShadowToggle.getToggleState(), kSave);
    main->setSpec3DSelfShadowStrength ((float) selfShadowStrengthSlider.getValue(), kSave);
    main->setSpec3DSelfShadowBias ((float) selfShadowBiasSlider.getValue(), kSave);
    main->setSpec3DSelfShadowSoftness ((float) selfShadowSoftnessSlider.getValue(), kSave);
    {
        const int id = selfShadowQualityCombo.getSelectedId();
        main->setSpec3DSelfShadowQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            kSave);
    }
    main->setSpec3DCastShadowsEnabled (castShadowsToggle.getToggleState(), kSave);
    {
        const int id = shadowResCombo.getSelectedId();
        const auto res = id == 1 ? Spectrogram3DComponent::ShadowMapResolution::r512
                       : (id == 3 ? Spectrogram3DComponent::ShadowMapResolution::r2048
                          : (id == 4 ? Spectrogram3DComponent::ShadowMapResolution::r4096
                                     : Spectrogram3DComponent::ShadowMapResolution::r1024));
        main->setSpec3DShadowMapResolution (res, kSave);
    }
    main->setSpec3DShadowCascadeCount (cascadeCountCombo.getSelectedId(), kSave);
    main->setSpec3DShadowCascadeDistributionExponent ((float) cascadeDistSlider.getValue(), kSave);
    main->setSpec3DShadowCascadeTransitionFraction ((float) cascadeTransSlider.getValue(), kSave);
    main->setSpec3DSsaoEnabled (ssaoToggle.getToggleState(), kSave);
    main->setSpec3DSsaoStrength ((float) ssaoStrengthSlider.getValue(), kSave);
    main->setSpec3DSsaoRadius ((float) ssaoRadiusSlider.getValue(), kSave);
    main->setSpec3DBloomEnabled (bloomToggle.getToggleState(), kSave);
    main->setSpec3DBloomStrength ((float) bloomStrengthSlider.getValue(), kSave);
    main->setSpec3DBloomThreshold ((float) bloomThresholdSlider.getValue(), kSave);
    main->setSpec3DMotionBlurEnabled (motionBlurToggle.getToggleState(), kSave);
    main->setSpec3DMotionBlurAmount ((float) motionBlurAmountSlider.getValue(), kSave);
    main->setSpec3DMotionBlurMax ((float) motionBlurMaxSlider.getValue(), kSave);
    {
        const int id = motionBlurQualityCombo.getSelectedId();
        main->setSpec3DMotionBlurQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            kSave);
    }
    main->setSpec3DDofEnabled (dofToggle.getToggleState(), kSave);
    // Focus distance: only via dofFocusSlider.onValueChange / Ctrl+click - never stomp here.
    main->setSpec3DDofFStop ((float) dofFStopSlider.getValue(), kSave);
    main->setSpec3DDofFocalLengthMm ((float) dofFocalLengthSlider.getValue(), kSave);
    {
        const int id = dofQualityCombo.getSelectedId();
        main->setSpec3DDofQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            kSave);
    }
    main->setSpec3DDofCocDilate ((float) dofCocDilateSlider.getValue(), kSave);
    main->setSpec3DDofEdgeSpill ((float) dofEdgeSpillSlider.getValue(), kSave);
    main->setSpec3DTonemapEnabled (tonemapToggle.getToggleState(), kSave);
    main->setSpec3DTonemapExposureStops ((float) exposureSlider.getValue(), kSave);
    {
        const int id = juce::jlimit (1, 6, gradeCombo.getSelectedId());
        main->setSpec3DColorGrade (
            static_cast<Spectrogram3DComponent::ColorGrade> (id - 1), kSave);
    }
    main->setSpec3DSssEnabled (sssToggle.getToggleState(), kSave);
    main->setSpec3DSssStrength ((float) sssStrengthSlider.getValue(), kSave);
    main->setSpec3DSssWrap ((float) sssWrapSlider.getValue(), kSave);
    main->setSpec3DSssTransmission ((float) sssTransmissionSlider.getValue(), kSave);
    main->setSpec3DSssTint (juce::Colour::fromFloatRGBA ((float) sssTintRSlider.getValue(),
                                                         (float) sssTintGSlider.getValue(),
                                                         (float) sssTintBSlider.getValue(),
                                                         1.0f),
                            kSave);
    main->setSpec3DSssRadius ((float) sssRadiusSlider.getValue(), kSave);
    main->setSpec3DSssContrast ((float) sssContrastSlider.getValue(), kSave);
    {
        const int id = sssQualityCombo.getSelectedId();
        main->setSpec3DSssQuality (
            id == 1 ? Spectrogram3DComponent::ShadowQuality::low
                    : (id == 3 ? Spectrogram3DComponent::ShadowQuality::high
                               : Spectrogram3DComponent::ShadowQuality::medium),
            kSave);
    }
    main->setSpec3DSssThicknessScale ((float) sssThickScaleSlider.getValue(), kSave);
    main->setSpec3DSssMaxThickness ((float) sssMaxThickSlider.getValue(), kSave);
    main->setSpec3DParticleModeEnabled (particleToggle.getToggleState(), kSave);
    main->setSpec3DParticleGpuSimEnabled (particleGpuSimToggle.getToggleState(), kSave);
    main->setSpec3DParticleMaxAlive ((int) std::lround (getSliderActual (particleMaxAliveSlider)), kSave);
    main->setSpec3DParticleDebugOverlayEnabled (particleDebugOverlayToggle.getToggleState(), kSave);
    main->setSpec3DParticleBindingMode (juce::jmax (0, particleBindingCombo.getSelectedId() - 1), kSave);
    main->setSpec3DParticleEmitMode (juce::jmax (0, particleEmitModeCombo.getSelectedId() - 1), kSave);
    main->setSpec3DParticleEmission ((float) getSliderActual (particleEmissionSlider), kSave);
    main->setSpec3DParticleSpawnJitter ((float) getSliderActual (particleSpawnJitterSlider), kSave);
    main->setSpec3DParticleInitVelX ((float) getSliderActual (particleInitVelXSlider), kSave);
    main->setSpec3DParticleInitVelY ((float) getSliderActual (particleInitVelYSlider), kSave);
    main->setSpec3DParticleInitVelZ ((float) getSliderActual (particleInitVelZSlider), kSave);
    main->setSpec3DParticleVelRandom ((float) getSliderActual (particleVelRandomSlider), kSave);
    main->setSpec3DParticleLifespan ((float) getSliderActual (particleLifespanSlider), kSave);
    main->setSpec3DParticleLifespanRandom ((float) getSliderActual (particleLifespanRandomSlider), kSave);
    main->setSpec3DParticleSize ((float) getSliderActual (particleSizeSlider), kSave);
    main->setSpec3DParticleEmissiveEnabled (particleEmissiveToggle.getToggleState(), kSave);
    main->setSpec3DParticleEmissiveStrength ((float) getSliderActual (particleEmissiveStrSlider), kSave);
    main->setSpec3DParticleRoughness ((float) getSliderActual (particleRoughSlider), kSave);
    main->setSpec3DParticleMetalness ((float) getSliderActual (particleMetalSlider), kSave);
    main->setSpec3DParticleSpecular ((float) getSliderActual (particleSpecSlider), kSave);
    for (int i = 0; i < kParticleModSlotCount; ++i)
    {
        auto& row = particleModRows[(size_t) i];
        ParticleModSlot slot;
        slot.enabled = row.enable.getToggleState();
        slot.source = (ParticleModSource) juce::jmax (0, row.source.getSelectedId() - 1);
        slot.thresholdEnabled = row.thresholdToggle.getToggleState();
        slot.dest = (ParticleModDest) juce::jmax (0, row.dest.getSelectedId() - 1);
        slot.op = (ParticleModOp) juce::jmax (0, row.op.getSelectedId() - 1);
        slot.curveShape = row.curve.getShape();
        {
            double lo = row.rangeSlider.getMinValue();
            double hi = row.rangeSlider.getMaxValue();
            if (lo > hi)
                std::swap (lo, hi);
            slot.mapMin = (float) lo;
            slot.mapMax = (float) hi;
        }
        slot.invert = row.invertToggle.getToggleState();
        slot.amount = (float) getSliderActual (row.amount);
        slot.constant = (float) getSliderActual (row.constant);
        slot.threshold = (float) getSliderActual (row.thresholdSlider);
        slot.attackMs = (float) row.attackKnob.getValue();
        slot.releaseMs = (float) row.releaseKnob.getValue();
        main->setSpec3DParticleModSlot (i, slot, kSave);
    }
    main->setSpec3DParticleMeshShape (juce::jmax (0, particleMeshCombo.getSelectedId() - 1), kSave);
    main->setSpec3DParticleInitRotX ((float) getSliderActual (particleInitRotXSlider), kSave);
    main->setSpec3DParticleInitRotY ((float) getSliderActual (particleInitRotYSlider), kSave);
    main->setSpec3DParticleInitRotZ ((float) getSliderActual (particleInitRotZSlider), kSave);
    main->setSpec3DParticleInitRotRandom ((float) getSliderActual (particleInitRotRndSlider), kSave);
    main->setSpec3DParticleForcesEnabled (particleForcesToggle.getToggleState(), kSave);
    main->setSpec3DParticleWaterfallLock (particleWaterfallLockToggle.getToggleState(), kSave);
    if (particleForceStack != nullptr)
        main->setSpec3DParticleForceStack (particleForceStack->getModules(), kSave);
    for (int i = 0; i < kParticleRandomSourceCount; ++i)
    {
        const auto& row = particleRandomRows[(size_t) i];
        ParticleRandomSource rs;
        rs.dim = (ParticleRandomDim) juce::jmax (0, row.dimCombo.getSelectedId() - 1);
        rs.mode = (ParticleRandomMode) juce::jmax (0, row.modeCombo.getSelectedId() - 1);
        rs.minV = (float) getSliderActual (row.minSlider);
        rs.maxV = (float) getSliderActual (row.maxSlider);
        rs.smoothMs = (float) getSliderActual (row.smoothSlider);
        main->setSpec3DParticleRandomSource (i, rs, kSave);
    }
    main->requestUiPrefsSave();
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
    const int comboRows = 3; // FFT size + mesh quality + MSAA
    const int baseSliderRows = 4; // mesh height + HF density + density start + soft angle
    // base (+ closed) + look masters (+ SSS + DOF + dome + SSGI + SSR + tonemap
    // + cast shadows + energy when lit + audio level)
    int toggles = 5 + 14; // includes tonemap + audio + SSR + cast + particle; energy when lit
    if (lightingToggle.getToggleState())
        toggles += 1; // energy conserving
    if (audioLevelToggle.getToggleState())
        toggles += 2; // playhead / anti-playhead
    if (particleToggle.getToggleState())
        toggles += 1; // Unlit emissive only
    const int buttonRows = 1;

    int lookRows = 0;
    int colourEditorH = 0;
    if (audioLevelToggle.getToggleState())
        lookRows += 7; // target, silence%, peak%, HP, LP, threshold, speed
    if (lightingToggle.getToggleState())
    {
        lookRows += 7; // amount, az/el, specular, roughness, metalness, rim
        colourEditorH += lightColourEditor.getPreferredHeight()
                       + rimColourEditor.getPreferredHeight();
    }
    if (lightingToggle.getToggleState() && domeFillToggle.getToggleState())
    {
        lookRows += 1; // strength
        toggles += 1;  // dome texture
        if (domeTextureToggle.getToggleState())
            lookRows += 1; // texture combo
        else
            colourEditorH += domeSkyEditor.getPreferredHeight()
                           + domeGroundEditor.getPreferredHeight();
    }
    if (ssgiToggle.getToggleState())
        lookRows += 3; // strength, radius, quality
    if (ssrToggle.getToggleState())
        lookRows += 10; // strength...dome fallback
    if (contactShadowToggle.getToggleState()) lookRows += 1;
    {
        const bool selfSh = selfShadowToggle.getToggleState();
        const bool castSh = castShadowsToggle.getToggleState();
        if (selfSh) lookRows += 2; // strength + quality
        if (selfSh || castSh) lookRows += 2; // shared bias + softness
        if (castSh) lookRows += 4; // res, cascades, dist, transition
    }
    if (ssaoToggle.getToggleState()) lookRows += 2;
    if (bloomToggle.getToggleState()) lookRows += 2;
    if (motionBlurToggle.getToggleState()) lookRows += 3; // amount, max px, quality
    if (dofToggle.getToggleState()) lookRows += 6; // focus, f-stop, focal mm, quality, dilate, spill
    if (tonemapToggle.getToggleState()) lookRows += 2; // exposure, grade
    if (sssToggle.getToggleState())
    {
        lookRows += 7; // strength, wrap, transmission, tint RGB, quality
        if (closedMeshToggle.getToggleState())
            lookRows += 2; // thick scale, max thick
        else
            lookRows += 2; // radius, contrast
    }
    int particleModExtra = 0;
    if (particleToggle.getToggleState())
    {
        toggles += 1; // GPU particle integrate
        lookRows += 1; // max particles
        toggles += 1; // debug overlay
        lookRows += 1; // clear button (approx one row)
        lookRows += 2; // binding + emit mode
        lookRows += 7; // emission, jitter, init vel XYZ, vel random, lifespan
        if (particleLifespanSlider.getValue() > 1.0e-4)
            lookRows += 1;
        lookRows += 1; // size
        lookRows += 4; // roughness, metalness, specular, emissive (always)
        // unlit toggle counted above when particleToggle is on
        lookRows += 1; // mesh shape
        if (particleMeshCombo.getSelectedId() != 3)
            lookRows += 4; // init rot x/y/z + random
        toggles += 2; // forces enable + waterfall lock
        if (particleForcesToggle.getToggleState())
        {
            if (particleForceStack != nullptr)
                particleModExtra += particleForceStack->getPreferredHeight() + 8;
        }
        particleModExtra += 24 + 18 + 18; // matrix title, hint, headers
        // Each slot: control row + range row (full-width min/max sliders for readable numbers).
        for (int i = 0; i < kParticleModSlotCount; ++i)
            particleModExtra += (particleModRows[(size_t) i].thresholdToggle.getToggleState() ? 36 : 28) + 26 + 6;
        for (int i = 0; i < kParticleRandomSourceCount; ++i)
            for (int r = 0; r < kParticleModSlotCount; ++r)
                if (particleModRows[(size_t) r].source.getSelectedId() == 8 + i)
                {
                    particleModExtra += 120;
                    break;
                }
    }

    return kPadY * 2
           + 24 + 8
           + comboRows * rowH
           + baseSliderRows * rowH
           + lookRows * rowH
           + colourEditorH
           + particleModExtra
           + toggles * toggleH
           + buttonRows * toggleH
           + kLabelH + kSectionGap // look section header
           + kLabelH + kLabelGap + gradientEditor.getPreferredHeight() + kRowGap;
}

int Spectrogram3DSettingsComponent::Content::getPreferredWidth() const
{
    // Match Menu design width. Matrix / save chrome compress into the panel  - 
    // never request 900px+ (that shoved Settings off the right of the editor).
    return 533;
}

void Spectrogram3DSettingsComponent::Content::resized()
{
    // Don't sync from Main here - that changes Look row count mid-layout and
    // left the Menu scrollbar short until the user resized the panel. Sync is
    // done from the ctor delay + when the tab/page is shown.

    auto area = getLocalBounds().reduced (kPadX, kPadY);

    {
        // Title + save buttons share one row; buttons shrink to fit so they never
        // dictate panel width or leave a dead strip beside the matrix below.
        auto titleRow = area.removeFromTop (24);
        const int rowW = titleRow.getWidth();
        const int btnH = 22;
        const int btnY = titleRow.getY() + 1;
        // Prefer readable labels; compress when the panel is narrow.
        int btnW = 100;
        int gap = 6;
        const int titleMin = 120;
        if (rowW < titleMin + btnW * 2 + gap)
        {
            btnW = juce::jmax (52, (rowW - titleMin - gap) / 2);
            if (btnW < 52)
            {
                // Very narrow: stack-ish compact chips; keep title first.
                btnW = juce::jmax (40, (rowW - gap) / 4);
            }
        }
        savePresetButton.setBounds (titleRow.removeFromRight (btnW).withHeight (btnH).withY (btnY));
        titleRow.removeFromRight (gap);
        saveDefaultButton.setBounds (titleRow.removeFromRight (btnW).withHeight (btnH).withY (btnY));
        titleLabel.setBounds (titleRow);
    }
    area.removeFromTop (8);

    layoutToggle (area, enable3DToggle);
    layoutToggle (area, enhancedFreq3DToggle);
    layoutComboRow (area, fftSizeLabel, fftSizeCombo);
    layoutComboRow (area, meshQualityLabel, meshQualityCombo);
    layoutSliderRow (area, meshHeightLabel, meshHeightSlider);
    layoutSliderRow (area, freqMeshBiasLabel, freqMeshBiasSlider);
    layoutSliderRow (area, freqMeshBiasPivotLabel, freqMeshBiasPivotSlider);
    layoutComboRow (area, msaaLabel, msaaCombo);
    layoutToggle (area, transparentBgToggle);
    layoutToggle (area, reverseFreqAxisToggle);
    layoutToggle (area, closedMeshToggle);
    layoutSliderRow (area, softAngleLabel, softAngleSlider);
    resetCameraButton.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (160, area.getWidth())));
    area.removeFromTop (kSectionGap);

    lookLabel.setBounds (area.removeFromTop (kLabelH));
    area.removeFromTop (kRowGap);
    layoutToggle (area, audioLevelToggle);
    if (audioLevelToggle.getToggleState())
    {
        layoutComboRow (area, audioLevelTargetLabel, audioLevelTargetCombo);
        layoutSliderRow (area, audioLevelMinPctLabel, audioLevelMinPctSlider);
        layoutSliderRow (area, audioLevelMaxPctLabel, audioLevelMaxPctSlider);
        layoutSliderRow (area, audioLevelHpLabel, audioLevelHpSlider);
        layoutSliderRow (area, audioLevelLpLabel, audioLevelLpSlider);
        layoutSliderRow (area, audioLevelThresholdLabel, audioLevelThresholdSlider);
        layoutComboRow (area, audioLevelSpeedLabel, audioLevelSpeedCombo);
        layoutToggle (area, audioAffectPlayheadToggle);
        layoutToggle (area, audioAffectAntiPlayheadToggle);
    }
    layoutToggle (area, lightingToggle);
    if (lightingToggle.getToggleState())
    {
        layoutSliderRow (area, lightingAmountLabel, lightingAmountSlider);
        layoutSliderRow (area, lightAzimuthLabel, lightAzimuthSlider);
        layoutSliderRow (area, lightElevationLabel, lightElevationSlider);
        layoutSliderRow (area, specularLabel, specularSlider);
        layoutSliderRow (area, roughnessLabel, roughnessSlider);
        layoutSliderRow (area, metalnessLabel, metalnessSlider);
        layoutToggle (area, energyConserveToggle);
        layoutSliderRow (area, rimLabel, rimSlider);
        layoutColourEditor (area, lightColourEditor);
        layoutColourEditor (area, rimColourEditor);
    }
    layoutToggle (area, domeFillToggle);
    if (lightingToggle.getToggleState() && domeFillToggle.getToggleState())
    {
        layoutSliderRow (area, domeFillStrengthLabel, domeFillStrengthSlider);
        layoutToggle (area, domeTextureToggle);
        if (domeTextureToggle.getToggleState())
            layoutComboRow (area, domeTextureLabel, domeTextureCombo);
        else
        {
            layoutColourEditor (area, domeSkyEditor);
            layoutColourEditor (area, domeGroundEditor);
        }
    }
    layoutToggle (area, ssgiToggle);
    if (ssgiToggle.getToggleState())
    {
        layoutSliderRow (area, ssgiStrengthLabel, ssgiStrengthSlider);
        layoutSliderRow (area, ssgiRadiusLabel, ssgiRadiusSlider);
        layoutComboRow (area, ssgiQualityLabel, ssgiQualityCombo);
    }
    layoutToggle (area, ssrToggle);
    if (ssrToggle.getToggleState())
    {
        layoutSliderRow (area, ssrStrengthLabel, ssrStrengthSlider);
        layoutSliderRow (area, ssrDistanceLabel, ssrDistanceSlider);
        layoutSliderRow (area, ssrThicknessLabel, ssrThicknessSlider);
        layoutComboRow (area, ssrQualityLabel, ssrQualityCombo);
        layoutSliderRow (area, ssrFresnelLabel, ssrFresnelSlider);
        layoutSliderRow (area, ssrRoughInfLabel, ssrRoughInfSlider);
        layoutSliderRow (area, ssrIntensityLabel, ssrIntensitySlider);
        layoutSliderRow (area, ssrEdgeFadeLabel, ssrEdgeFadeSlider);
        layoutSliderRow (area, ssrMetalBiasLabel, ssrMetalBiasSlider);
        layoutSliderRow (area, ssrDomeFbLabel, ssrDomeFbSlider);
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
    layoutToggle (area, castShadowsToggle);
    if (castShadowsToggle.getToggleState())
    {
        // Bias/Softness shared with self-shadow - show here if self-shadow UI is collapsed.
        if (! selfShadowToggle.getToggleState())
        {
            layoutSliderRow (area, selfShadowBiasLabel, selfShadowBiasSlider);
            layoutSliderRow (area, selfShadowSoftnessLabel, selfShadowSoftnessSlider);
        }
        layoutComboRow (area, shadowResLabel, shadowResCombo);
        layoutComboRow (area, cascadeCountLabel, cascadeCountCombo);
        layoutSliderRow (area, cascadeDistLabel, cascadeDistSlider);
        layoutSliderRow (area, cascadeTransLabel, cascadeTransSlider);
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
    layoutToggle (area, motionBlurToggle);
    if (motionBlurToggle.getToggleState())
    {
        layoutSliderRow (area, motionBlurAmountLabel, motionBlurAmountSlider);
        layoutSliderRow (area, motionBlurMaxLabel, motionBlurMaxSlider);
        layoutComboRow (area, motionBlurQualityLabel, motionBlurQualityCombo);
    }
    layoutToggle (area, dofToggle);
    if (dofToggle.getToggleState())
    {
        layoutSliderRow (area, dofFocusLabel, dofFocusSlider);
        layoutSliderRow (area, dofFStopLabel, dofFStopSlider);
        layoutSliderRow (area, dofFocalLengthLabel, dofFocalLengthSlider);
        layoutComboRow (area, dofQualityLabel, dofQualityCombo);
        layoutSliderRow (area, dofCocDilateLabel, dofCocDilateSlider);
        layoutSliderRow (area, dofEdgeSpillLabel, dofEdgeSpillSlider);
    }
    layoutToggle (area, tonemapToggle);
    if (tonemapToggle.getToggleState())
    {
        layoutSliderRow (area, exposureLabel, exposureSlider);
        layoutComboRow (area, gradeLabel, gradeCombo);
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
    layoutToggle (area, particleToggle);
    if (particleToggle.getToggleState())
    {
        layoutToggle (area, particleGpuSimToggle);
        layoutSliderRow (area, particleMaxAliveLabel, particleMaxAliveSlider);
        layoutToggle (area, particleDebugOverlayToggle);
        particleClearButton.setBounds (area.removeFromTop (22).removeFromLeft (juce::jmin (160, area.getWidth())));
        area.removeFromTop (8);
        layoutComboRow (area, particleBindingLabel, particleBindingCombo);
        layoutComboRow (area, particleEmitModeLabel, particleEmitModeCombo);
        layoutSliderRow (area, particleEmissionLabel, particleEmissionSlider);
        layoutSliderRow (area, particleSpawnJitterLabel, particleSpawnJitterSlider);
        layoutSliderRow (area, particleInitVelXLabel, particleInitVelXSlider);
        layoutSliderRow (area, particleInitVelYLabel, particleInitVelYSlider);
        layoutSliderRow (area, particleInitVelZLabel, particleInitVelZSlider);
        layoutSliderRow (area, particleVelRandomLabel, particleVelRandomSlider);
        layoutSliderRow (area, particleLifespanLabel, particleLifespanSlider);
        if (particleLifespanSlider.getValue() > 1.0e-4)
            layoutSliderRow (area, particleLifespanRandomLabel, particleLifespanRandomSlider);
        layoutSliderRow (area, particleSizeLabel, particleSizeSlider);
        // Material (waterfall-matching PBR + additive emissive)
        layoutSliderRow (area, particleRoughLabel, particleRoughSlider);
        layoutSliderRow (area, particleMetalLabel, particleMetalSlider);
        layoutSliderRow (area, particleSpecLabel, particleSpecSlider);
        layoutSliderRow (area, particleEmissiveStrLabel, particleEmissiveStrSlider);
        layoutToggle (area, particleEmissiveToggle);

        layoutComboRow (area, particleMeshLabel, particleMeshCombo);
        if (particleMeshCombo.getSelectedId() != 3)
        {
            layoutSliderRow (area, particleInitRotXLabel, particleInitRotXSlider);
            layoutSliderRow (area, particleInitRotYLabel, particleInitRotYSlider);
            layoutSliderRow (area, particleInitRotZLabel, particleInitRotZSlider);
            layoutSliderRow (area, particleInitRotRndLabel, particleInitRotRndSlider);
        }

        layoutToggle (area, particleWaterfallLockToggle);
        layoutToggle (area, particleForcesToggle);
        if (particleForcesToggle.getToggleState() && particleForceStack != nullptr)
        {
            const int fh = particleForceStack->getPreferredHeight();
            particleForceStack->setBounds (area.removeFromTop (fh));
            area.removeFromTop (8);
        }

        particleModLabel.setBounds (area.removeFromTop (kLabelH));
        area.removeFromTop (2);
        particleModHintLabel.setBounds (area.removeFromTop (16));
        area.removeFromTop (4);

        // Matrix: control row + full-width range row.
        // Amount is the size reference; range min/max are each ≥ amount (prefer +25%).
        const int matrixW = juce::jmax (1, area.getWidth());
        constexpr int kGap = 3;
        constexpr int kAmtMinW = 110; // amount track + 56px text box (3 d.p. readable)
        constexpr int kRangeMinEach = (kAmtMinW * 5) / 4; // 25% larger than amount
        // Control-row columns (range lives on the next line - not squeezed here).
        constexpr int kGapsCtrl = 7; // 8 columns
        struct ColIdeal { int ideal; int minW; };
        const ColIdeal colsIdeal[] = {
            { 28, 24 },  // On
            { 86, 64 },  // Source
            { 30, 26 },  // Thr toggle
            { 70, 48 },  // thr widgets
            { 96, 70 },  // Dest
            { 58, 44 },  // Op
            { 26, 20 },  // Curve
            { 28, 24 },  // Inv
            // Amount takes remaining (and grows with free space)
        };
        constexpr int nFixed = 8;
        int fixedIdeal = kGapsCtrl * kGap;
        int fixedMin = kGapsCtrl * kGap;
        for (int i = 0; i < nFixed; ++i)
        {
            fixedIdeal += colsIdeal[i].ideal;
            fixedMin += colsIdeal[i].minW;
        }
        const int amountW = juce::jmax (kAmtMinW, matrixW - fixedMin);
        int colW[nFixed];
        {
            int roomForFixed = juce::jmax (fixedMin, matrixW - amountW);
            if (roomForFixed >= fixedIdeal)
            {
                int extra = roomForFixed - fixedIdeal;
                for (int i = 0; i < nFixed; ++i)
                    colW[i] = colsIdeal[i].ideal;
                // Prefer source + dest for extra chrome width
                colW[1] += extra / 2;
                colW[4] += extra - extra / 2;
            }
            else
            {
                int weightSum = 0;
                for (int i = 0; i < nFixed; ++i)
                    weightSum += (colsIdeal[i].ideal - colsIdeal[i].minW);
                int free = juce::jmax (0, roomForFixed - fixedMin);
                int used = 0;
                for (int i = 0; i < nFixed; ++i)
                {
                    const int span = colsIdeal[i].ideal - colsIdeal[i].minW;
                    const int add = (weightSum > 0 && i < nFixed - 1)
                                        ? (free * span) / weightSum
                                        : juce::jmax (0, free - used);
                    colW[i] = colsIdeal[i].minW + add;
                    used += add;
                }
            }
        }
        // Actual amount column = leftover after fixed columns
        int fixedUsed = kGapsCtrl * kGap;
        for (int i = 0; i < nFixed; ++i) fixedUsed += colW[i];
        const int amountColW = juce::jmax (kAmtMinW, matrixW - fixedUsed);

        auto takeCol = [&] (juce::Rectangle<int>& row, int idx) -> juce::Rectangle<int>
        {
            auto c = row.removeFromLeft (colW[idx]);
            if (idx < nFixed - 1)
                row.removeFromLeft (kGap);
            return c;
        };

        // Headers: control columns + Amt; Range header sits on the value row band
        {
            auto h = area.removeFromTop (16);
            particleModHdrOn.setBounds (takeCol (h, 0));
            particleModHdrSrc.setBounds (takeCol (h, 1));
            {
                auto thrHdr = takeCol (h, 2);
                thrHdr = thrHdr.getUnion (takeCol (h, 3));
                particleModHdrThr.setBounds (thrHdr);
            }
            particleModHdrDst.setBounds (takeCol (h, 4));
            particleModHdrOp.setBounds (takeCol (h, 5));
            particleModHdrCurve.setBounds (takeCol (h, 6));
            particleModHdrInv.setBounds (takeCol (h, 7));
            h.removeFromLeft (kGap);
            particleModHdrAmt.setBounds (h.removeFromLeft (amountColW));
            // Range header uses the full next-line band (laid out per row)
            particleModHdrRange.setBounds ({});
        }
        area.removeFromTop (2);
        // One shared "Range" label above the first slot's range pair
        {
            auto rh = area.removeFromTop (14);
            particleModHdrRange.setBounds (rh.removeFromLeft (juce::jmin (80, rh.getWidth())));
            particleModHdrRange.setText ("Range (min / max)", juce::dontSendNotification);
        }
        area.removeFromTop (2);

        for (int i = 0; i < kParticleModSlotCount; ++i)
        {
            auto& row = particleModRows[(size_t) i];
            const bool thrOn = row.thresholdToggle.getToggleState();
            const int rowH = thrOn ? 36 : 28;
            auto r = area.removeFromTop (rowH);

            {
                auto c = takeCol (r, 0);
                row.enable.setBounds (c.withSizeKeepingCentre (juce::jmin (32, c.getWidth()), 22));
            }
            {
                auto c = takeCol (r, 1);
                row.source.setBounds (c.withSizeKeepingCentre (c.getWidth(), 22));
            }
            {
                auto c = takeCol (r, 2);
                row.thresholdToggle.setBounds (c.withSizeKeepingCentre (juce::jmin (36, c.getWidth()), 22));
            }
            {
                auto thrBand = takeCol (r, 3);
                if (thrOn)
                {
                    const int knob = juce::jmin (26, thrBand.getHeight() - 2, thrBand.getWidth() / 3);
                    auto tb = thrBand;
                    row.thresholdSlider.setBounds (tb.removeFromLeft (juce::jmax (12, thrBand.getWidth() / 4)).reduced (0, 2));
                    tb.removeFromLeft (1);
                    row.attackKnob.setBounds (tb.removeFromLeft (knob).withSizeKeepingCentre (knob, knob));
                    tb.removeFromLeft (1);
                    row.releaseKnob.setBounds (tb.removeFromLeft (knob).withSizeKeepingCentre (knob, knob));
                }
            }
            {
                auto c = takeCol (r, 4);
                row.dest.setBounds (c.withSizeKeepingCentre (c.getWidth(), 22));
            }
            {
                auto c = takeCol (r, 5);
                row.op.setBounds (c.withSizeKeepingCentre (c.getWidth(), 22));
            }
            {
                auto c = takeCol (r, 6);
                const int curveSz = juce::jmin (rowH - 2, c.getWidth(), c.getHeight());
                row.curve.setBounds (c.withSizeKeepingCentre (curveSz, curveSz));
            }
            {
                auto c = takeCol (r, 7);
                row.invertToggle.setBounds (c.withSizeKeepingCentre (juce::jmin (32, c.getWidth()), 22));
            }
            {
                r.removeFromLeft (kGap);
                auto c = r.removeFromLeft (amountColW);
                const int sliderH = 22;
                if (row.source.getSelectedId() == 7)
                {
                    // Amount keeps full reference width; constant gets equal remaining if any
                    // (prefer amount ≥ kAmtMinW).
                    const int gap = 4;
                    const int constW = juce::jmax (kAmtMinW, (c.getWidth() - gap) / 2);
                    const int amtW = juce::jmax (kAmtMinW, c.getWidth() - gap - constW);
                    row.amount.setBounds (c.removeFromLeft (amtW).withSizeKeepingCentre (amtW, sliderH));
                    c.removeFromLeft (gap);
                    row.constant.setBounds (c.withSizeKeepingCentre (c.getWidth(), sliderH));
                    row.constant.setVisible (true);
                }
                else
                {
                    row.constant.setVisible (false);
                    row.amount.setBounds (c.withSizeKeepingCentre (c.getWidth(), sliderH));
                }
            }

            // Range row: [0.000] ═══════●═══════ [1.000]  dual-arrow bar
            area.removeFromTop (2);
            {
                auto rr = area.removeFromTop (26);
                constexpr int kReadoutW = 56; // fits "-0.000" / "1.000" at 12px Lato
                row.rangeMinReadout.setBounds (rr.removeFromLeft (kReadoutW).withSizeKeepingCentre (kReadoutW, 18));
                rr.removeFromLeft (4);
                row.rangeMaxReadout.setBounds (rr.removeFromRight (kReadoutW).withSizeKeepingCentre (kReadoutW, 18));
                rr.removeFromRight (4);
                row.rangeSlider.setBounds (rr.withSizeKeepingCentre (rr.getWidth(), 22));
            }
            area.removeFromTop (4);
        }

        // Sources (random gens in use)
        bool anyRnd = false;
        for (int i = 0; i < kParticleModSlotCount; ++i)
        {
            const int id = particleModRows[(size_t) i].source.getSelectedId();
            if (id >= 8 && id <= 10) anyRnd = true;
        }
        if (anyRnd)
        {
            area.removeFromTop (8);
            particleSourcesLabel.setBounds (area.removeFromTop (kLabelH));
            area.removeFromTop (4);
            for (int i = 0; i < kParticleRandomSourceCount; ++i)
            {
                bool used = false;
                for (int r = 0; r < kParticleModSlotCount; ++r)
                    if (particleModRows[(size_t) r].source.getSelectedId() == 8 + i)
                        used = true;
                if (! used) continue;
                auto& row = particleRandomRows[(size_t) i];
                row.title.setBounds (area.removeFromTop (18));
                area.removeFromTop (2);
                layoutComboRow (area, row.dimLabel, row.dimCombo);
                layoutComboRow (area, row.modeLabel, row.modeCombo);
                layoutSliderRow (area, row.minLabel, row.minSlider);
                layoutSliderRow (area, row.maxLabel, row.maxSlider);
                if (row.modeCombo.getSelectedId() == 3)
                    layoutSliderRow (area, row.smoothLabel, row.smoothSlider);
            }
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
