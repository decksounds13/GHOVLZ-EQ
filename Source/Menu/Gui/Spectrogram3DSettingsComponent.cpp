#include "Spectrogram3DSettingsComponent.h"

#include "../../MainComponent.h"
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
    meshQualityCombo.setTooltip ("Mesh density along time × frequency. Ultra is 288×240 base rows.");
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
        "Off by default. Independent of SSS — when SSS is on it uses volume thickness if enabled.");
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
        "Labs Soften Normals–style soft angle (cusp). 180 = fully soft organic shading; "
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
        "Ramp brightness pulses colours only — Lighting amount / All lights are required to pulse lighting.");
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

    audioLevelMinPctLabel.setText ("At Silence (%)", juce::dontSendNotification);
    setupLookSlider (audioLevelMinPctLabel, audioLevelMinPctSlider,
                     Spectrogram3DComponent::kAudioLevelPercentMin,
                     Spectrogram3DComponent::kAudioLevelPercentMax,
                     1.0,
                     "Modulation % when the sidechain is below/at threshold (level 0). "
                     "Full ±100 range — swap with At Peak to invert.");
    audioLevelMinPctSlider.setValue (Spectrogram3DComponent::kAudioLevelMinPercentDefault,
                                     juce::dontSendNotification);
    audioLevelMaxPctLabel.setText ("At Peak (%)", juce::dontSendNotification);
    setupLookSlider (audioLevelMaxPctLabel, audioLevelMaxPctSlider,
                     Spectrogram3DComponent::kAudioLevelPercentMin,
                     Spectrogram3DComponent::kAudioLevelPercentMax,
                     1.0,
                     "Modulation % at full sidechain level (1). Independent of At Silence — "
                     "set e.g. +20 / −20 to invert the pulse.");
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
                     "Sidechain level must exceed this before the pulse rises (0…1 over the next 24 dB).");
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
                     "Also pulse the closed-mesh anti-playhead (−X) wall. Only when Audio level affects is on.");

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
    setupLookToggle (energyConserveToggle,
                     "Multiply diffuse/dome by (1−Fresnel). Off by default (legacy Look).");
    rimLabel.setText ("Rim Light", juce::dontSendNotification);
    setupLookSlider (rimLabel, rimSlider, 0.0, 1.0, 0.01, "View-dependent edge lift.");
    rimSlider.setValue (0.22, juce::dontSendNotification);
    wireColourEditor (lightColourEditor);
    lightColourEditor.setColour (juce::Colours::white);
    wireColourEditor (rimColourEditor);
    rimColourEditor.setColour (juce::Colours::white);

    setupLookToggle (domeFillToggle,
                     "Hemisphere dome fill — sky/ground ambient into shadows (needs Lighting). "
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
    domeTextureCombo.addItem ("Load custom…", 2);
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
        "SSGI ray/step density: Low 6×4, Medium 10×6, High 14×8, Ultra 20×12. "
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
                     "Depth hit acceptance slab — higher catches more, softer contacts.");
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
                     "View-angle fresnel — stronger at glancing angles.");
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
                     "How strongly occluded ridges darken (0–2).");
    selfShadowStrengthSlider.setValue (0.85, juce::dontSendNotification);
    selfShadowBiasLabel.setText ("Shadow Bias", juce::dontSendNotification);
    setupLookSlider (selfShadowBiasLabel, selfShadowBiasSlider, 0.0, 1.0, 0.01,
                     "Shared by Self-Shadow and Cast Shadows — raises the sample origin to fight acne.");
    selfShadowBiasSlider.setValue (0.35, juce::dontSendNotification);
    selfShadowSoftnessLabel.setText ("Shadow Softness", juce::dontSendNotification);
    setupLookSlider (selfShadowSoftnessLabel, selfShadowSoftnessSlider, 0.0, 1.0, 0.01,
                     "Shared by Self-Shadow and Cast Shadows — widens the penumbra (higher = softer).");
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
    // World unit = 1 m for photographic DOF (F-Stop / focal mm). Mesh footprint is 2×2 m.
    dofFocusLabel.setText ("Focus Distance (m)", juce::dontSendNotification);
    setupLookSlider (dofFocusLabel, dofFocusSlider,
                     Spectrogram3DComponent::kDofFocusMin,
                     Spectrogram3DComponent::kDofFocusMax,
                     0.01,
                     "Sharp plane distance from the camera (metres; 1 world unit = 1 m). "
                     "Mesh footprint is 2×2 m (time × frequency); default height ≈ 0.55 m; "
                     "default camera distance ≈ 3.4 m. "
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
                     "(f/1.4 wide open → f/22 deep focus).");
    dofFStopSlider.setValue (Spectrogram3DComponent::kDofFStopDefault, juce::dontSendNotification);
    dofFocalLengthLabel.setText ("Focal Length (mm)", juce::dontSendNotification);
    setupLookSlider (dofFocalLengthLabel, dofFocalLengthSlider,
                     (double) Spectrogram3DComponent::kDofFocalLengthMinMm,
                     (double) Spectrogram3DComponent::kDofFocalLengthMaxMm,
                     1.0,
                     "Effective focal length for thin-lens CoC (scene metres). "
                     "Longer = shallower DOF at the same focus distance (18–85 mm, default 35).");
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
                     "Soft BG edge tune: pulls neighbour CoC into silhouette pixels so thin "
                     "far edges still gather against the background.");
    dofCocDilateSlider.setValue (Spectrogram3DComponent::kDofCocDilateDefault, juce::dontSendNotification);
    dofEdgeSpillLabel.setText ("DOF Edge Spill", juce::dontSendNotification);
    setupLookSlider (dofEdgeSpillLabel, dofEdgeSpillSlider, 0.0, 1.0, 0.01,
                     "Soft BG edge tune: how strongly out-of-focus mesh bleeds onto sky / "
                     "background (silhouette soften). Use with Edge Dilate.");
    dofEdgeSpillSlider.setValue (Spectrogram3DComponent::kDofEdgeSpillDefault, juce::dontSendNotification);

    setupLookToggle (tonemapToggle,
                     "Display transform + color grade (soft path). Off by default — "
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
    gradeCombo.setTooltip ("Display transform look. Default: Warm Cinema at −0.3 EV.");
    gradeCombo.onChange = [this] { applyLookControlsToMain(); };
    addAndMakeVisible (gradeLabel);
    addAndMakeVisible (gradeCombo);

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
            // Keep "Load custom…" selected so the choice stays visible.
            domeTextureCombo.setSelectedId (2, juce::dontSendNotification);
            updateLookDevVisibility();
            requestParentRelayout();
        }
        else
        {
            // Cancelled — restore previous source in the combo.
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
    // Soften path: Soft Angle + Angle+Area (Labs Soften ≈ cusp 180; organic default).
    main->setSpec3DNormalWeighting (Spectrogram3DComponent::NormalWeighting::angleAndArea, kSave);
    main->setSpec3DNormalCuspAngleDeg ((float) softAngleSlider.getValue(), kSave);
    main->requestUiPrefsSave();
}

void Spectrogram3DSettingsComponent::Content::applyLookControlsToMain()
{
    auto* main = findParentComponentOfClass<MainComponent>();
    if (main == nullptr)
        return;

    // Look-only: re-shade / re-light existing mesh history — never mode/quality/MSAA.
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
    main->setSpec3DDofEnabled (dofToggle.getToggleState(), kSave);
    // Focus distance: only via dofFocusSlider.onValueChange / Ctrl+click — never stomp here.
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
    int toggles = 5 + 13; // includes tonemap + audio + SSR + cast; energy counted when lighting on
    if (lightingToggle.getToggleState())
        toggles += 1; // energy conserving
    if (audioLevelToggle.getToggleState())
        toggles += 2; // playhead / anti-playhead
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
        lookRows += 10; // strength…dome fallback
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

    return kPadY * 2
           + 24 + 8
           + comboRows * rowH
           + baseSliderRows * rowH
           + lookRows * rowH
           + colourEditorH
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
        // Bias/Softness shared with self-shadow — show here if self-shadow UI is collapsed.
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
