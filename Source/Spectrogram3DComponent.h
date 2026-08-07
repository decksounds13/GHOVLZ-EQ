#pragma once

#include <JuceHeader.h>
#include "ColourRamp/Spec3DRampSequence.h"
#include "MelatoninBlur/melatonin/shadows.h"
#include "Spec3DParticleSystem.h"
#include "ParticleForceModule.h"
#include <atomic>
#include <vector>
#include <cstdint>
#include <functional>
#include <array>

class SpectrogramComponent;
class SharedResources;
class ColourRampBank;

/**
    OpenGL heightfield for the spectrogram (expanded / Scope).
    Framed Melatonin window + nested GL host (orbit / pan / zoom).
    Floating (expanded): move via frame chrome, resize via corner grip.
    Docked (Scope strip/tiled): fills the pane, no free move/resize.
*/
class Spectrogram3DComponent : public juce::Component,
                               private juce::Timer
{
public:
    friend class Spec3DParticleSystem;
    /** overkill kept for prefs migration only — clamped to ultra. */
    enum class MeshQuality { low = 0, medium, high, ultra, overkill };
    enum class ChromeMode { floating, docked };
    /** Window / soft-FBO multisample count. Off = 0. */
    enum class MsaaLevel { off = 0, x4 = 4, x8 = 8, x16 = 16 };
    /** Sample density for shadows / SSGI / DOF / SSS. Ultra is used by SSGI gather. */
    enum class ShadowQuality { low = 0, medium, high, ultra };

    Spectrogram3DComponent();
    ~Spectrogram3DComponent() override;

    void setDataSource (SpectrogramComponent* source) noexcept { dataSource = source; }
    void setThemeColors (SharedResources* r) noexcept;
    void setActive (bool shouldBeActive) noexcept;
    bool isActive() const noexcept { return active; }

    void setMeshQuality (MeshQuality q) noexcept;
    MeshQuality getMeshQuality() const noexcept { return meshQuality; }

    /**
        Extra frequency-row density toward the highs (0 = uniform).
        Never thins the lows vs the quality's base grid — only adds HF rows.
    */
    void setFreqMeshBias (float amount01) noexcept;
    float getFreqMeshBias() const noexcept { return freqMeshBias; }
    /**
        Inflection on the frequency axis (0=low…1=high) where HF density boost begins.
        Below this, rows stay uniform; above, w(u) ramps with the density slider.
    */
    void setFreqMeshBiasPivot (float pivot01) noexcept;
    float getFreqMeshBiasPivot() const noexcept { return freqMeshBiasPivot; }
    static constexpr float kFreqMeshBiasPivotDefault = 0.55f;

    void setMsaaLevel (MsaaLevel level) noexcept;
    MsaaLevel getMsaaLevel() const noexcept { return msaaLevel; }
    bool isMultisamplingEnabled() const noexcept { return msaaLevel != MsaaLevel::off; }

    /**
        Soft background: offscreen GL → Image → paint compositing over the EQ
        (nested OpenGL HWND transparency is not available on Windows/Direct2D).
    */
    void setTransparentBackground (bool shouldEnable) noexcept;
    bool isTransparentBackground() const noexcept { return transparentBackground; }

    /** When true, high frequencies map toward -Z (lows toward +Z). */
    void setReverseFrequencyAxis (bool shouldReverse) noexcept;
    bool isReverseFrequencyAxis() const noexcept { return reverseFrequencyAxis; }

    void setMeshHeight (float heightWorld) noexcept;
    float getMeshHeight() const noexcept { return meshHeight; }
    static constexpr float kDefaultMeshHeight = 0.55f;
    static constexpr float kMinMeshHeight = 0.15f;
    static constexpr float kMaxMeshHeight = 1.40f;

    /**
        Closed solid mesh: border extrude + bottom cap (DCC-style close).
        Independent of SSS — off by default.
    */
    void setClosedMeshEnabled (bool shouldEnable) noexcept;
    bool isClosedMeshEnabled() const noexcept { return closedMeshEnabled; }
    /** World-Y bias just under the 0-intensity floor for the closed bottom cap. */
    static constexpr float kClosedMeshFloorBias = 0.003f;
    /**
        Closed playhead wall sits at X = 1 + this (past “now”), so it doesn’t
        z-fight with history just behind the playhead. Scroll direction is -X.
    */
    static constexpr float kClosedPlayheadWallBias = 0.006f;
    /**
        Closed waterfall-end wall sits at X = -1 − this (past oldest history),
        mirroring the playhead bias so the last frames don’t z-fight the end face.
    */
    static constexpr float kClosedWaterfallEndWallBias = 0.006f;

    /** Visual polish — all effects default off (flat vertex colours). */
    void setLightingEnabled (bool shouldEnable) noexcept;
    bool isLightingEnabled() const noexcept { return lightingEnabled; }
    void setLightingAmount (float amount01) noexcept;
    float getLightingAmount() const noexcept { return lightingAmount; }
    void setLightAzimuthDeg (float deg) noexcept;
    float getLightAzimuthDeg() const noexcept { return lightAzimuthDeg; }
    void setLightElevationDeg (float deg) noexcept;
    float getLightElevationDeg() const noexcept { return lightElevationDeg; }
    void setSpecularAmount (float amount01) noexcept;
    float getSpecularAmount() const noexcept { return specularAmount; }
    void setRoughnessAmount (float amount01) noexcept;
    float getRoughnessAmount() const noexcept { return roughnessAmount; }
    /** PBR metalness 0–1 (dielectric → metal). Off/default = 0. */
    void setMetalnessAmount (float amount01) noexcept;
    float getMetalnessAmount() const noexcept { return metalnessAmount; }
    /** Multiply diffuse/dome by (1−F). Off by default (preserves current look). */
    void setEnergyConservingEnabled (bool shouldEnable) noexcept;
    bool isEnergyConservingEnabled() const noexcept { return energyConservingEnabled; }
    void setRimAmount (float amount01) noexcept;
    float getRimAmount() const noexcept { return rimAmount; }
    void setLightColour (juce::Colour c) noexcept;
    juce::Colour getLightColour() const noexcept { return lightColour; }
    void setRimColour (juce::Colour c) noexcept;
    juce::Colour getRimColour() const noexcept { return rimColour; }

    /** Hemisphere dome fill — sky/ground ambient into shadows (mesh shader). */
    void setDomeFillEnabled (bool shouldEnable) noexcept;
    bool isDomeFillEnabled() const noexcept { return domeFillEnabled; }
    void setDomeFillStrength (float amount01) noexcept;
    float getDomeFillStrength() const noexcept { return domeFillStrength; }
    void setDomeSkyColour (juce::Colour c) noexcept;
    juce::Colour getDomeSkyColour() const noexcept { return domeSkyColour; }
    void setDomeGroundColour (juce::Colour c) noexcept;
    juce::Colour getDomeGroundColour() const noexcept { return domeGroundColour; }

    /** Equirectangular dome irradiance map (off by default — uses sky/ground colours). */
    enum class DomeTextureSource { veniceSunset = 0, custom = 1 };
    void setDomeTextureEnabled (bool shouldEnable) noexcept;
    bool isDomeTextureEnabled() const noexcept { return domeTextureEnabled; }
    void setDomeTextureSource (DomeTextureSource source) noexcept;
    DomeTextureSource getDomeTextureSource() const noexcept { return domeTextureSource; }
    void setDomeTextureCustomPath (const juce::String& absolutePath) noexcept;
    juce::String getDomeTextureCustomPath() const noexcept { return domeTextureCustomPath; }
    /** Reload image from current source (built-in Venice Sunset or custom path). */
    void refreshDomeTextureImage();

    /**
        Screen-space GI (soft post path): short ray-march gather for color bleed
        into shadows. Off by default.
    */
    void setSsgiEnabled (bool shouldEnable) noexcept;
    bool isSsgiEnabled() const noexcept { return ssgiEnabled; }
    void setSsgiStrength (float amount01) noexcept;
    float getSsgiStrength() const noexcept { return ssgiStrength; }
    void setSsgiRadius (float amount01) noexcept;
    float getSsgiRadius() const noexcept { return ssgiRadius; }
    void setSsgiQuality (ShadowQuality q) noexcept;
    ShadowQuality getSsgiQuality() const noexcept { return ssgiQuality; }
    /** Temporal GI history. Off by default. */
    void setSsgiTemporalEnabled (bool shouldEnable) noexcept;
    bool isSsgiTemporalEnabled() const noexcept { return ssgiTemporalEnabled; }
    void setSsgiTemporalAmount (float amount01) noexcept;
    float getSsgiTemporalAmount() const noexcept { return ssgiTemporalAmount; }
    /** GI denoise. Off by default. */
    void setSsgiDenoiseEnabled (bool shouldEnable) noexcept;
    bool isSsgiDenoiseEnabled() const noexcept { return ssgiDenoiseEnabled; }
    void setSsgiDenoiseAmount (float amount01) noexcept;
    float getSsgiDenoiseAmount() const noexcept { return ssgiDenoiseAmount; }
    /** Simple = bilateral (default); Modern = SVGF-style temporal moments + à-trous. */
    enum class SsgiDenoiseMode { simple = 0, modern = 1 };
    void setSsgiDenoiseMode (SsgiDenoiseMode mode) noexcept;
    SsgiDenoiseMode getSsgiDenoiseMode() const noexcept { return ssgiDenoiseMode; }
    /** À-trous pass count for Modern denoise (low=3, medium=4, high=5). */
    void setSsgiAtrousQuality (ShadowQuality q) noexcept;
    ShadowQuality getSsgiAtrousQuality() const noexcept { return ssgiAtrousQuality; }
    /** Half-resolution SSGI gather + upsample. Off by default. */
    void setSsgiHalfResEnabled (bool shouldEnable) noexcept;
    bool isSsgiHalfResEnabled() const noexcept { return ssgiHalfResEnabled; }
    /** Use mesh view-normals for SSGI instead of depth derivatives. Off by default. */
    void setSsgiMeshNormalsEnabled (bool shouldEnable) noexcept;
    bool isSsgiMeshNormalsEnabled() const noexcept { return ssgiMeshNormalsEnabled; }

    /**
        Screen-space reflections (soft post path): march the reflection ray against
        depth and sample the colour buffer (key specular, dome-lit surfaces, etc.).
        On by default. Misses can fall back to dome sky colour.
    */
    void setSsrEnabled (bool shouldEnable) noexcept;
    bool isSsrEnabled() const noexcept { return ssrEnabled; }
    void setSsrStrength (float amount01) noexcept;
    float getSsrStrength() const noexcept { return ssrStrength; }
    void setSsrDistance (float amount01) noexcept;
    float getSsrDistance() const noexcept { return ssrDistance; }
    void setSsrThickness (float amount01) noexcept;
    float getSsrThickness() const noexcept { return ssrThickness; }
    void setSsrQuality (ShadowQuality q) noexcept;
    ShadowQuality getSsrQuality() const noexcept { return ssrQuality; }
    void setSsrFresnel (float amount01) noexcept;
    float getSsrFresnel() const noexcept { return ssrFresnel; }
    void setSsrRoughnessInfluence (float amount01) noexcept;
    float getSsrRoughnessInfluence() const noexcept { return ssrRoughnessInfluence; }
    void setSsrIntensity (float amount01) noexcept;
    float getSsrIntensity() const noexcept { return ssrIntensity; }
    void setSsrEdgeFade (float amount01) noexcept;
    float getSsrEdgeFade() const noexcept { return ssrEdgeFade; }
    void setSsrMetallicBias (float amount01) noexcept;
    float getSsrMetallicBias() const noexcept { return ssrMetallicBias; }
    void setSsrDomeFallback (float amount01) noexcept;
    float getSsrDomeFallback() const noexcept { return ssrDomeFallback; }

    void setContactShadowEnabled (bool shouldEnable) noexcept;
    bool isContactShadowEnabled() const noexcept { return contactShadowEnabled; }
    void setContactShadowStrength (float amount01) noexcept;
    float getContactShadowStrength() const noexcept { return contactShadowStrength; }

    /** Heightfield self-shadowing toward the key light (off by default). */
    void setSelfShadowEnabled (bool shouldEnable) noexcept;
    bool isSelfShadowEnabled() const noexcept { return selfShadowEnabled; }
    void setSelfShadowStrength (float amount01) noexcept;
    float getSelfShadowStrength() const noexcept { return selfShadowStrength; }
    void setSelfShadowBias (float bias01) noexcept;
    float getSelfShadowBias() const noexcept { return selfShadowBias; }
    void setSelfShadowSoftness (float amount01) noexcept;
    float getSelfShadowSoftness() const noexcept { return selfShadowSoftness; }
    void setSelfShadowQuality (ShadowQuality q) noexcept;
    ShadowQuality getSelfShadowQuality() const noexcept { return selfShadowQuality; }

    /**
        Directional cast-shadow map (CSM): mesh + debug sphere cast/receive via a
        light-depth atlas. Cascade count splits a fixed shadow distance into N
        tiles (does not shrink draw distance); each tile is rendered.
    */
    static constexpr int kMaxShadowCascades = 4;
    enum class ShadowMapResolution { r512 = 512, r1024 = 1024, r2048 = 2048, r4096 = 4096 };
    void setCastShadowsEnabled (bool shouldEnable) noexcept;
    bool isCastShadowsEnabled() const noexcept { return castShadowsEnabled; }
    void setShadowMapResolution (ShadowMapResolution res) noexcept;
    ShadowMapResolution getShadowMapResolution() const noexcept { return shadowMapResolution; }
    void setShadowCascadeCount (int count) noexcept;
    int getShadowCascadeCount() const noexcept { return shadowCascadeCount; }
    void setShadowCascadeDistributionExponent (float exponent) noexcept;
    float getShadowCascadeDistributionExponent() const noexcept { return shadowCascadeDistributionExponent; }
    void setShadowCascadeTransitionFraction (float amount01) noexcept;
    float getShadowCascadeTransitionFraction() const noexcept { return shadowCascadeTransitionFraction; }

    /** Debug lookdev sphere (soft normals) — off by default. */
    void setDebugSphereEnabled (bool shouldEnable) noexcept;
    bool isDebugSphereEnabled() const noexcept { return debugSphereEnabled; }
    void setDebugSphereDiameter (float metres) noexcept;
    float getDebugSphereDiameter() const noexcept { return debugSphereDiameter; }
    void setDebugSpherePosition (juce::Vector3D<float> worldPos) noexcept;
    juce::Vector3D<float> getDebugSpherePosition() const noexcept { return debugSpherePosition; }
    void setDebugSphereAlbedo (juce::Colour c) noexcept;
    juce::Colour getDebugSphereAlbedo() const noexcept { return debugSphereAlbedo; }
    void setDebugSphereRoughness (float amount01) noexcept;
    float getDebugSphereRoughness() const noexcept { return debugSphereRoughness; }
    void setDebugSphereMetalness (float amount01) noexcept;
    float getDebugSphereMetalness() const noexcept { return debugSphereMetalness; }
    void setDebugSphereSpecular (float amount01) noexcept;
    float getDebugSphereSpecular() const noexcept { return debugSphereSpecular; }
    /** Default diameter = 1/12 of the 2 m waterfall footprint. */
    static constexpr float kDebugSphereDefaultDiameter = 2.0f / 12.0f;
    static constexpr float kDebugSphereMinDiameter = 0.02f;
    static constexpr float kDebugSphereMaxDiameter = 1.5f;

    void setSsaoEnabled (bool shouldEnable) noexcept;
    bool isSsaoEnabled() const noexcept { return ssaoEnabled; }
    void setSsaoStrength (float amount01) noexcept;
    float getSsaoStrength() const noexcept { return ssaoStrength; }
    void setSsaoRadius (float radius) noexcept;
    float getSsaoRadius() const noexcept { return ssaoRadius; }

    void setBloomEnabled (bool shouldEnable) noexcept;
    bool isBloomEnabled() const noexcept { return bloomEnabled; }
    void setBloomStrength (float amount01) noexcept;
    float getBloomStrength() const noexcept { return bloomStrength; }
    void setBloomThreshold (float amount01) noexcept;
    float getBloomThreshold() const noexcept { return bloomThreshold; }

    /**
        Soft-path camera motion blur (UE5-style): reproject depth with previous
        view-projection → screen velocity → reconstruction gather along the vector.
        Amount ≈ shutter / exposure fraction; max blur clamps streak length in px.
    */
    void setMotionBlurEnabled (bool shouldEnable) noexcept;
    bool isMotionBlurEnabled() const noexcept { return motionBlurEnabled; }
    void setMotionBlurAmount (float amount01) noexcept;
    float getMotionBlurAmount() const noexcept { return motionBlurAmount; }
    void setMotionBlurMax (float maxPixels) noexcept;
    float getMotionBlurMax() const noexcept { return motionBlurMax; }
    void setMotionBlurQuality (ShadowQuality q) noexcept;
    ShadowQuality getMotionBlurQuality() const noexcept { return motionBlurQuality; }

    static constexpr float kMotionBlurAmountDefault = 0.55f;
    static constexpr float kMotionBlurMaxDefault = 32.0f;
    static constexpr float kMotionBlurMaxMin = 4.0f;
    static constexpr float kMotionBlurMaxMax = 96.0f;

    /**
        Soft-path post DOF (EEVEE / Marmoset Post Effect style): thin-lens CoC +
        disc gather. Focus distance (view Z) + aperture (higher = more blur) +
        sample quality.
    */
    void setDofEnabled (bool shouldEnable) noexcept;
    bool isDofEnabled() const noexcept { return dofEnabled; }
    void setDofFocusDistance (float distance, bool notifyPrefsCallback = false) noexcept;
    float getDofFocusDistance() const noexcept { return dofFocusDistance; }
    /**
        Photography-style F-Stop (Marmoset). Lower = shallower DOF / more blur;
        higher = wider sharp band. Typical range f/1.4–f/22.
    */
    void setDofFStop (float fStop) noexcept;
    float getDofFStop() const noexcept { return dofFStop; }
    /**
        Focal length in mm (DOF scale only — does not change orbit FOV).
        Longer = shallower feel at the same F-Stop.
    */
    void setDofFocalLengthMm (float mm) noexcept;
    float getDofFocalLengthMm() const noexcept { return dofFocalLengthMm; }
    /** Combined thin-lens power passed to the CoC shader: (f/35)² / (N/5.6). */
    float getDofLensPower() const noexcept;
    /** @deprecated Maps to F-Stop; prefer setDofFStop. */
    void setDofAperture (float amount) noexcept;
    float getDofAperture() const noexcept;
    /** @deprecated Prefer setDofFStop. */
    void setDofAmount (float amount) noexcept { setDofAperture (amount); }
    float getDofAmount() const noexcept { return getDofAperture(); }
    void setDofQuality (ShadowQuality q) noexcept;
    ShadowQuality getDofQuality() const noexcept { return dofQuality; }
    /** @deprecated No-op — blur scale removed. */
    void setDofBlurScale (float scale) noexcept;
    float getDofBlurScale() const noexcept { return 1.0f; }
    /** Neighbour CoC dilation for Soft BG silhouette gather (0 = off, 1 = full). */
    void setDofCocDilate (float amount01) noexcept;
    float getDofCocDilate() const noexcept { return dofCocDilate; }
    /** How strongly out-of-focus mesh spills onto Soft BG / sky edges. */
    void setDofEdgeSpill (float amount01) noexcept;
    float getDofEdgeSpill() const noexcept { return dofEdgeSpill; }

    /** Soft-path display transform. Off by default (current LDR look). */
    enum class ColorGrade
    {
        aces = 0,
        filmic,
        warmCinema,
        coolCinema,
        tealOrange,
        bleachBypass
    };
    void setTonemapEnabled (bool shouldEnable) noexcept;
    bool isTonemapEnabled() const noexcept { return tonemapEnabled; }
    /** Exposure in stops (0 = unchanged). Applied only when tonemap is on. */
    void setTonemapExposureStops (float stops) noexcept;
    float getTonemapExposureStops() const noexcept { return tonemapExposureStops; }
    void setColorGrade (ColorGrade grade) noexcept;
    ColorGrade getColorGrade() const noexcept { return colorGrade; }

    /**
        Focus distance in metres (1 world unit = 1 m for thin-lens DOF).
        Mesh footprint is 2×2 m (X time × Z frequency); default peak height ≈ 0.55 m.
    */
    static constexpr float kDofFocusMin = 0.5f;
    /** Match camera dolly max so far framing can still place focus on the mesh. */
    static constexpr float kDofFocusMax = 14.0f;
    /** Tuned for the default camera (~3.4 m) — sharp mid mesh, soft waterfall end. */
    static constexpr float kDofFocusDefault = 2.75f;
    static constexpr float kDofFStopMin = 1.4f;
    static constexpr float kDofFStopMax = 22.0f;
    static constexpr float kDofFStopDefault = 5.6f;
    static constexpr float kDofFocalLengthMinMm = 18.0f;
    static constexpr float kDofFocalLengthMaxMm = 85.0f;
    static constexpr float kDofFocalLengthDefaultMm = 35.0f;
    /** @deprecated Legacy 0–3 openness; migrated to F-Stop. */
    static constexpr float kDofApertureDefault = 0.35f;
    static constexpr float kDofApertureMax = 3.0f;
    static constexpr float kDofAmountDefault = kDofApertureDefault;
    /** @deprecated Blur scale removed; kept for prefs XML defaults only. */
    static constexpr float kDofBlurScaleDefault = 1.0f;
    /** Lower default — high dilate + spill was stacking into bright silhouette rings. */
    static constexpr float kDofCocDilateDefault = 0.35f;
    static constexpr float kDofEdgeSpillDefault = 0.30f;

    /**
        SSS Look toggle. Thickness path follows Closed Mesh:
        closed → volume thickness; open → heightfield taps. Off by default.
    */
    void setSssEnabled (bool shouldEnable) noexcept;
    bool isSssEnabled() const noexcept { return sssEnabled; }
    void setSssStrength (float amount01) noexcept;
    float getSssStrength() const noexcept { return sssStrength; }
    void setSssWrap (float amount01) noexcept;
    float getSssWrap() const noexcept { return sssWrap; }
    void setSssTransmission (float amount01) noexcept;
    float getSssTransmission() const noexcept { return sssTransmission; }
    void setSssTint (juce::Colour c) noexcept;
    juce::Colour getSssTint() const noexcept { return sssTint; }
    /** Open mesh SSS: height-map tap distance for thin ridges. */
    void setSssRadius (float amount01) noexcept;
    float getSssRadius() const noexcept { return sssRadius; }
    void setSssContrast (float amount01) noexcept;
    float getSssContrast() const noexcept { return sssContrast; }
    void setSssQuality (ShadowQuality q) noexcept;
    ShadowQuality getSssQuality() const noexcept { return sssQuality; }
    /** Closed-mesh SSS: maps volume depth → transmission. */
    void setSssThicknessScale (float amount01) noexcept;
    float getSssThicknessScale() const noexcept { return sssThicknessScale; }
    void setSssMaxThickness (float amount01) noexcept;
    float getSssMaxThickness() const noexcept { return sssMaxThickness; }

    void setChromeMode (ChromeMode mode) noexcept;
    ChromeMode getChromeMode() const noexcept { return chromeMode; }

    void resetCamera() noexcept;
    void saveAsDefaultView() noexcept;

    /**
        Freeze the waterfall (no new columns). Camera / look-dev still update.
        Toggle with right-click (no drag); Shift+right-click opens the view menu.
    */
    void setWaterfallFrozen (bool shouldFreeze) noexcept;
    bool isWaterfallFrozen() const noexcept { return waterfallFrozen; }

    /** Auto yaw orbit (turntable). Off by default; period = seconds per full revolution. */
    void setAutoRotateEnabled (bool shouldEnable, bool notifyPrefsCallback = true) noexcept;
    bool isAutoRotateEnabled() const noexcept { return autoRotateEnabled; }
    void setAutoRotatePeriodSec (float secondsPerRevolution, bool notifyPrefsCallback = true) noexcept;
    float getAutoRotatePeriodSec() const noexcept { return autoRotatePeriodSec; }
    static constexpr float kAutoRotatePeriodMinSec = 1.0f;
    static constexpr float kAutoRotatePeriodMaxSec = 60.0f;
    static constexpr float kAutoRotatePeriodDefaultSec = 10.0f;

    /** LFO on camera distance (zoom). Off by default. */
    void setZoomOscillateEnabled (bool shouldEnable, bool notifyPrefsCallback = true) noexcept;
    bool isZoomOscillateEnabled() const noexcept { return zoomOscillateEnabled; }
    void setZoomOscillateDepth (float amount01, bool notifyPrefsCallback = true) noexcept;
    float getZoomOscillateDepth() const noexcept { return zoomOscillateDepth; }
    void setZoomOscillatePeriodSec (float secondsPerCycle, bool notifyPrefsCallback = true) noexcept;
    float getZoomOscillatePeriodSec() const noexcept { return zoomOscillatePeriodSec; }
    static constexpr float kZoomOscillateDepthDefault = 0.35f;
    static constexpr float kZoomOscillatePeriodMinSec = 1.0f;
    static constexpr float kZoomOscillatePeriodMaxSec = 60.0f;
    static constexpr float kZoomOscillatePeriodDefaultSec = 8.0f;

    /**
        Audio-level visual mod matrix (off by default): sidechain envelope →
        ramp brightness and/or light intensities. Optional closed playhead /
        anti-playhead wall masks.
    */
    enum class AudioLevelTarget : int
    {
        brightness = 0,
        lightingAmount = 1,
        specular = 2,
        rim = 3,
        domeFill = 4,
        allLights = 5,
        brightnessAndLights = 6
    };
    void setAudioLevelModEnabled (bool shouldEnable) noexcept;
    bool isAudioLevelModEnabled() const noexcept { return audioLevelModEnabled; }
    void setAudioLevelTarget (AudioLevelTarget target) noexcept;
    AudioLevelTarget getAudioLevelTarget() const noexcept { return audioLevelTarget; }
    /**
        Pulse endpoints as % of the current target (independent; either may be anywhere
        in ±100). Silence→min, peak→max via mix — swap them to invert. Both 0 = no change.
    */
    void setAudioLevelMinPercent (float pct) noexcept;
    float getAudioLevelMinPercent() const noexcept { return audioLevelMinPercent; }
    void setAudioLevelMaxPercent (float pct) noexcept;
    float getAudioLevelMaxPercent() const noexcept { return audioLevelMaxPercent; }
    void setAudioLevelHpHz (float hz) noexcept;
    float getAudioLevelHpHz() const noexcept { return audioLevelHpHz; }
    void setAudioLevelLpHz (float hz) noexcept;
    float getAudioLevelLpHz() const noexcept { return audioLevelLpHz; }
    void setAudioLevelThresholdDb (float thresholdDb) noexcept;
    float getAudioLevelThresholdDb() const noexcept { return audioLevelThresholdDb; }
    /** Hard-wired A/R ballistics (Side Check–style Fast / Med / Slow). */
    enum class AudioLevelSpeed : int { fast = 0, med = 1, slow = 2 };
    void setAudioLevelSpeed (AudioLevelSpeed speed) noexcept;
    AudioLevelSpeed getAudioLevelSpeed() const noexcept { return audioLevelSpeed; }
    static void audioLevelBallisticsMs (AudioLevelSpeed speed,
                                        float& attackMs, float& releaseMs) noexcept;
    void setAudioLevelAffectPlayhead (bool shouldAffect) noexcept;
    bool getAudioLevelAffectPlayhead() const noexcept { return audioLevelAffectPlayhead; }
    void setAudioLevelAffectAntiPlayhead (bool shouldAffect) noexcept;
    bool getAudioLevelAffectAntiPlayhead() const noexcept { return audioLevelAffectAntiPlayhead; }
    /** Provider returns filtered sidechain level 0..1 (typically from EqProcessor). */
    void setAudioLevelProvider (std::function<float()> provider) noexcept;
    static constexpr float kAudioLevelPercentMin = -100.0f;
    static constexpr float kAudioLevelPercentMax = 100.0f;
    static constexpr float kAudioLevelMinPercentDefault = 0.0f;
    static constexpr float kAudioLevelMaxPercentDefault = 0.0f;
    static constexpr float kAudioLevelHpDefaultHz = 40.0f;
    static constexpr float kAudioLevelLpDefaultHz = 150.0f;
    static constexpr float kAudioLevelThresholdDefaultDb = -24.0f;
    static constexpr float kAudioLevelThresholdMinDb = -60.0f;
    static constexpr float kAudioLevelThresholdMaxDb = 0.0f;

    /**
        Top-surface normals (organic soften, Labs Soften Normals–style).
        Soft Angle = cusp; 180 (default) fully averages incident faces.
        Weighting methods stay available in code; default is Angle+Area
        (vertex angle × face area) for organic heightfields.
    */
    enum class NormalWeighting : int
    {
        equal = 0,
        vertexAngle = 1,
        faceArea = 2,
        angleAndArea = 3
    };
    void setNormalCuspAngleDeg (float deg) noexcept;
    float getNormalCuspAngleDeg() const noexcept { return normalCuspAngleDeg; }
    void setNormalWeighting (NormalWeighting method) noexcept;
    NormalWeighting getNormalWeighting() const noexcept { return normalWeighting; }
    /** Labs Soften Normals “Soft Angle” default — fully soft. */
    static constexpr float kNormalCuspDefaultDeg = 180.0f;
    static constexpr float kNormalCuspMinDeg = 0.0f;
    static constexpr float kNormalCuspMaxDeg = 180.0f;

    void invalidateMesh() noexcept;
    /** Rebuild vertex colours from the current mesh + 3D colour LUT (ramp edits). */
    void recolourMesh() noexcept;
    /** Ramp morph only: refresh vertex RGB from meshDb + current 3D LUT (no normals/positions). */
    void recolourVertexColoursOnly() noexcept;

    // ── Particle mode (modular; default off — zero cost until enabled) ─────────
    /** Slice = per-bin rates (grid-ish). Continuous = random playhead samples (default). */
    enum class ParticleEmitMode : int { slice = 0, continuous = 1 };

    void setParticleModeEnabled (bool shouldEnable) noexcept;
    bool isParticleModeEnabled() const noexcept { return particleModeEnabled; }
    void setParticleEmitMode (ParticleEmitMode mode) noexcept;
    ParticleEmitMode getParticleEmitMode() const noexcept { return particleEmitMode; }
    void setParticleBindingMode (ParticleBindingMode mode) noexcept;
    ParticleBindingMode getParticleBindingMode() const noexcept { return particleBindingMode; }
    void setParticleEmission (float amount) noexcept;
    float getParticleEmission() const noexcept { return particleEmission; }
    void setParticleSpawnJitter (float amount) noexcept;
    float getParticleSpawnJitter() const noexcept { return particleSpawnJitter; }

    static constexpr int getParticleModSlotCount() noexcept { return kParticleModSlotCount; }
    ParticleModSlot getParticleModSlot (int index) const noexcept;
    void setParticleModSlot (int index, const ParticleModSlot& slot) noexcept;
    ParticleRandomSource getParticleRandomSource (int index) const noexcept;
    void setParticleRandomSource (int index, const ParticleRandomSource& src) noexcept;

    /** Initial linear velocity (world units/s) at spawn — Vec3. */
    void setParticleInitVelX (float unitsPerSec) noexcept;
    void setParticleInitVelY (float unitsPerSec) noexcept;
    void setParticleInitVelZ (float unitsPerSec) noexcept;
    float getParticleInitVelX() const noexcept { return particleInitVelX; }
    float getParticleInitVelY() const noexcept { return particleInitVelY; }
    float getParticleInitVelZ() const noexcept { return particleInitVelZ; }
    /** @deprecated Prefer setParticleInitVelY — kept for prefs / older call sites. */
    void setParticleRiseSpeed (float unitsPerSec) noexcept { setParticleInitVelY (unitsPerSec); }
    float getParticleRiseSpeed() const noexcept { return particleInitVelY; }
    void setParticleVelRandom (float amount01) noexcept;
    float getParticleVelRandom() const noexcept { return particleVelRandom; }
    void setParticleLifespan (float seconds) noexcept;
    float getParticleLifespan() const noexcept { return particleLifespan; }
    void setParticleLifespanRandom (float amount01) noexcept;
    float getParticleLifespanRandom() const noexcept { return particleLifespanRandom; }
    void setParticleSize (float worldSize) noexcept;
    float getParticleSize() const noexcept { return particleSize; }
    void setParticleEmissiveEnabled (bool shouldEnable) noexcept;
    bool isParticleEmissiveEnabled() const noexcept { return particleEmissiveEnabled; }
    void setParticleEmissiveStrength (float amount) noexcept;
    float getParticleEmissiveStrength() const noexcept { return particleEmissiveStrength; }
    void setParticleRoughness (float amount01) noexcept;
    float getParticleRoughness() const noexcept { return particleRoughness; }
    void setParticleMetalness (float amount01) noexcept;
    float getParticleMetalness() const noexcept { return particleMetalness; }
    void setParticleSpecular (float amount01) noexcept;
    float getParticleSpecular() const noexcept { return particleSpecular; }

    // Forces stack (ordered modules; matrix can scale types)
    void setParticleForcesEnabled (bool e) noexcept;
    bool isParticleForcesEnabled() const noexcept { return particleForcesEnabled; }
    void setParticleWaterfallLock (bool e) noexcept;
    bool isParticleWaterfallLock() const noexcept { return particleWaterfallLock; }
    int getParticleForceModuleCount() const noexcept { return (int) particleForceStack.size(); }
    ParticleForceModule getParticleForceModule (int index) const noexcept;
    void setParticleForceModule (int index, const ParticleForceModule& m) noexcept;
    void addParticleForceModule (ParticleForceType type) noexcept;
    void removeParticleForceModule (int index) noexcept;
    void moveParticleForceModule (int from, int to) noexcept;
    void clearParticleForceModules() noexcept;
    void setParticleForceStack (std::vector<ParticleForceModule> stack) noexcept;
    uint32_t nextParticleForceUid() noexcept { return ++particleForceUidSerial; }

    void setParticleMeshShape (ParticleMeshShape s) noexcept;
    ParticleMeshShape getParticleMeshShape() const noexcept { return particleMeshShape; }
    /**
        Hybrid GPU particle integrate (compute). Default off = full CPU sim.
        When on and compute is available: CPU spawn/matrix, GPU force/age integrate.
        Falls back to CPU if the GL context has no compute support.
    */
    void setParticleGpuSimEnabled (bool shouldEnable) noexcept;
    bool isParticleGpuSimEnabled() const noexcept { return particleGpuSimEnabled; }
    /** True after GL init if a compute program linked successfully. */
    bool isParticleGpuSimAvailable() const noexcept;

    /** Live-particle budget — spawn freezes / culls at this (hard cap 16k). */
    void setParticleMaxAlive (int maxAlive) noexcept;
    int getParticleMaxAlive() const noexcept;
    int getParticleAliveCount() const noexcept;
    int getParticlePoolCapacity() const noexcept;
    int getParticleLastSpawnedCount() const noexcept;
    int getParticleLastCulledCount() const noexcept;
    /** Kill all live particles (escape hatch when the field is overloaded). */
    void clearParticles() noexcept;
    /** On-viewport stats: alive / budget / pool / spawned / culled. */
    void setParticleDebugOverlayEnabled (bool shouldShow) noexcept;
    bool isParticleDebugOverlayEnabled() const noexcept { return particleDebugOverlayEnabled; }

    void setParticleInitRotX (float deg) noexcept;
    float getParticleInitRotX() const noexcept { return particleInitRotX; }
    void setParticleInitRotY (float deg) noexcept;
    float getParticleInitRotY() const noexcept { return particleInitRotY; }
    void setParticleInitRotZ (float deg) noexcept;
    float getParticleInitRotZ() const noexcept { return particleInitRotZ; }
    void setParticleInitRotRandom (float amount01) noexcept;
    float getParticleInitRotRandom() const noexcept { return particleInitRotRandom; }

    /** @deprecated Use getMeshHeight() — kept as alias for call sites expecting a constant. */
    static constexpr float kMeshHeight = kDefaultMeshHeight;

    /**
        Y-up turntable camera: yaw around world Y, pitch = elevation.
        No roll — the ground plane always stays downward.
    */
    struct CameraState
    {
        float yawDeg = -40.0f;
        float pitchDeg = 35.0f;   // elevation above floor horizon (0=edge-on, 90=top-down)
        float distance = 3.0f;
        float panX = 0.0f;        // look-at X (time)
        float panY = kDefaultMeshHeight * 0.5f;
        float panZ = 0.0f;        // look-at Z (freq)
    };

    void setDefaultCameraState (const CameraState& state) noexcept;
    CameraState getDefaultCameraState() const noexcept { return defaultCamera; }
    CameraState getCameraState() const noexcept { return camera; }
    static CameraState getFactoryCameraState() noexcept;
    /** Vertical drag from the magnifying-glass control (wheel-equivalent zoom). */
    void applyUiZoomDrag (float deltaY) noexcept;

    /**
        UE freecam is 100% separate from the turntable/orbit camera while active
        (own eye + FPS yaw/pitch + view matrix). Settings below apply only to freecam.
    */
    /** Fly speed scale. 1.0 = full (max); lower = slower. Range [kFreecamSpeedMin, 1]. */
    void setFreecamSpeedScale (float scale01to1) noexcept;
    float getFreecamSpeedScale() const noexcept { return freecamSpeedScale; }
    /** UE viewport-style level 1 (slowest) … 8 (full). */
    void setFreecamSpeedLevel (float level1to8) noexcept;
    float getFreecamSpeedLevel() const noexcept;
    /** Mouse-look sensitivity (deg per pixel). Default ~0.22. */
    void setFreecamLookSensitivity (float degPerPixel) noexcept;
    float getFreecamLookSensitivity() const noexcept { return freecamLookSensitivity; }
    /** Invert vertical look (UE-style option). Off by default. */
    void setFreecamInvertY (bool shouldInvert) noexcept;
    bool isFreecamInvertY() const noexcept { return freecamInvertY; }
    bool isFreecamActive() const noexcept { return freecamActive; }
    static constexpr float kFreecamSpeedMin = 0.01f;
    static constexpr float kFreecamSpeedMax = 1.0f;
    /** Fired when freecam speed / look prefs change so UI prefs can persist. */
    std::function<void()> onFreecamSpeedChanged;

    std::function<void()> onEscape;
    std::function<void()> onUserResized;
    std::function<void()> onUserMoved;
    std::function<void()> onDefaultViewChanged;
    /** Toggle OS-level F11 fullscreen (borderless desktop window). */
    std::function<void()> onToggleFullscreen;
    /** True while Spec3D is in OS fullscreen (for menu tick / label). */
    std::function<bool()> isFullscreenQuery;
    /** Append look-preset items (ids >= 100). Fullscreen uses id 20. */
    std::function<void (juce::PopupMenu&)> onAugmentContextMenu;
    /** Return true if result was handled. */
    std::function<bool (int)> onContextMenuResult;
    static constexpr int kContextMenuFullscreenId = 20;
    /** Fired when turntable / zoom-oscillate settings change (persist prefs). */
    std::function<void()> onAutoRotateSettingsChanged;
    /** Fired when DOF focus is picked (Ctrl/Cmd+LMB) or otherwise changed with notify. */
    std::function<void()> onDofFocusChanged;
    /** Fired when the debug sphere is moved via gizmo (settings sync). */
    std::function<void()> onDebugSphereChanged;
    /** If set, double-click invokes this instead of resetCamera(). */
    std::function<void()> onDoubleClick;

    /** Colour ramp bank for ramp-timeline presets (set by MainComponent). */
    void setColourRampBank (class ColourRampBank* bank) noexcept { colourRampBank = bank; }
    ColourRampBank* getColourRampBank() const noexcept { return colourRampBank; }

    Spec3DRampSequence& getRampSequence() noexcept { return rampSequence; }
    const Spec3DRampSequence& getRampSequence() const noexcept { return rampSequence; }
    void setRampSequence (const Spec3DRampSequence& s) noexcept;
    float getRampTimelinePlayheadSec() const noexcept { return rampPlayheadSec; }
    /** Seek ramp playhead (sequence seconds) and apply morph / lighting for that time. */
    void setRampTimelinePlayheadSec (float sec) noexcept;
    void clearMorphRamp() noexcept;

    /**
        Thread-safe copy of the latest soft-composite image (empty if hard path or not ready).
        Live UI only — prefer captureExportFrame for offline export.
    */
    juce::Image copySoftCompositeImage() const;

    /**
        Offline export: force soft FBO render at width x height (blocks on the GL thread)
        and return an ARGB image. Invalid image if GL is unavailable.
        Temporarily disables temporal SSGI history for a clean frame.
    */
    juce::Image captureExportFrame (int width, int height);

    /** Persist when timeline model is edited (not per playhead tick). */
    std::function<void()> onRampSequenceChanged;
    /** Open expanded floating timeline editor. */
    std::function<void()> onRequestRampTimelineExpand;
    /** Offline export (region settings) — MainComponent runs Spec3DExportJob. */
    std::function<void (const struct Spec3DExportSettings&)> onExportRegionOffline;

    juce::Rectangle<int> getFrameBounds() const noexcept { return getBounds(); }
    int getShadowPad() const noexcept;
    static int getShadowPadForMode (ChromeMode mode) noexcept;
    void setResizeLimits (int maxW, int maxH) noexcept;
    void setMovementBounds (juce::Rectangle<int> parentLocalBounds) noexcept;

    void paint (juce::Graphics& g) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    bool keyStateChanged (bool isKeyDown) override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;

private:
    static constexpr int kShadowPadFloating = 14;
    static constexpr int kShadowPadDocked = 2;
    static constexpr float kCornerRadius = 12.0f;
    static constexpr int kGlInset = 3;

    struct Vertex { float x, y, z, r, g, b, nx, ny, nz; };

    struct FreqLabel
    {
        float hz = 0.0f;
        juce::String text;
        float u0 = 0, v0 = 0, u1 = 0, v1 = 0; // atlas UVs
        float worldZ = 0.0f;
    };

    class GlHost : public juce::Component,
                   public juce::OpenGLRenderer
    {
    public:
        explicit GlHost (Spectrogram3DComponent& owner);
        ~GlHost() override;

        void setActive (bool shouldBeActive) noexcept;
        void requestAttachAsync();
        void applyPixelFormat();
        void applyBackgroundTransparency() noexcept;
        void reattachWithCurrentFormat();
        void triggerRedraw() { if (openGLContext.isAttached()) openGLContext.triggerRepaint(); }
        juce::OpenGLContext& getOpenGLContext() noexcept { return openGLContext; }
        bool isGlReady() const noexcept { return glReady; }
        bool hasContextFailed() const noexcept { return contextFailed; }
        void markSoftContentDirty() noexcept { softContentDirty = true; }
        void markDebugSphereDirty() noexcept { sphereNeedsUpload = true; }
        void invalidateSsgiHistory() noexcept
        {
            ssgiHistoryValid = false;
            ssgiMomentsValid = false;
        }
        /** Drop previous-frame VP so motion blur does not smear a stale camera. */
        void invalidateMotionHistory() noexcept { motionPrevVpValid = false; }
        /**
            Force soft composite at exact pixel size and copy into outImage (ARGB).
            Must be called on the OpenGL thread (or via executeOnGLThread).
        */
        bool captureSoftFrameOnGlThread (int width, int height, juce::Image& outImage);
        bool projectWorldToNdc (float wx, float wy, float wz,
                                float& ndcX, float& ndcY, float& ndcZ) const;

        void newOpenGLContextCreated() override;
        void renderOpenGL() override;
        void openGLContextClosing() override;

        void mouseDown (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;
        bool keyStateChanged (bool isKeyDown) override;

    private:
        void attachNow();
        void createShaders();
        void destroyShaders();
        void uploadMeshIfNeeded();
        void ensureFloorGeometry();
        void rebuildFloorGeometry();
        void ensureLabelAtlas();
        void drawSoftTint();
        void drawGroundAndGrid();
        void drawPlayheadTicks();
        void drawContactShadow();
        void drawMesh();
        /** Mesh or particles depending on particleModeEnabled. */
        void drawSpectrogramSurface();
        void drawDebugSphere();
        void drawDebugGizmo();
        void setGizmoXrayUniforms (bool insideGhostPass) const;
        void drawFrequencyLabels();
        void ensureShadowAtlas();
        void releaseShadowAtlas();
        void updateCascadeMatrices();
        void renderShadowDepthPass();
        void drawMeshIntoShadow (const juce::Matrix3D<float>& lightVP) const;
        void drawSphereIntoShadow (const juce::Matrix3D<float>& lightVP) const;
        void ensureDebugSphereGeometry();
        void uploadDebugSphereWorldVerts();
        void bindShadowAtlasForMesh() const;
        void setCornerUniforms (juce::OpenGLShaderProgram& program) const;
        void setLightingUniforms (juce::OpenGLShaderProgram& program) const;
        void setCastShadowUniforms (juce::OpenGLShaderProgram& program) const;
        void setMaterialOverrideUniforms (bool enabled, juce::Colour albedo,
                                         float roughness, float metalness,
                                         float specular = 1.0f) const;
        juce::Matrix3D<float> getProjectionMatrix() const;
        juce::Matrix3D<float> getViewMatrix() const;
        juce::Vector3D<float> getLightDirectionWorld() const noexcept;
        juce::Rectangle<int> getViewPixelBounds() const noexcept;
        void ensureSoftFrameBuffer (int width, int height);
        void ensureSoftMsaaBuffers (int width, int height, int samples);
        void releaseSoftMsaaBuffers();
        int effectiveMsaaSamples() const noexcept;
        void ensurePostFrameBuffers (int width, int height);
        void ensureSsgiSupportBuffers (int width, int height, bool halfRes, bool needHistory,
                                       bool needNormals, bool needMoments);
        void releasePostFrameBuffers();
        void renderSoftComposite();
        void applySsaoAndBloom (int width, int height);
        void drawMeshNormalsPass (int width, int height);
        void readbackSoftImage (int width, int height);
        void uploadHeightMap (const std::vector<Vertex>& verts, int w, int h);
        void bindHeightMapForMesh() const;
        void uploadDomeTextureIfNeeded();
        void bindDomeTextureForMesh() const;
        void unbindDomeTexture() const;

        Spectrogram3DComponent& owner;
        juce::OpenGLContext openGLContext;
        bool attachPending = false;
        bool contextFailed = false;
        bool glReady = false;
        bool softContentDirty = true;

        std::unique_ptr<juce::OpenGLShaderProgram> colourShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> colourPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> colourColourAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> colourNormalAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourProjectionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourViewUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourResolutionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCornerUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourClearUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourLightDirUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourLightingAmountUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSpecularUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourRoughnessUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMetalnessUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourEnergyConserveUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourRimUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourLightColourUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourRimColourUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourDomeStrengthUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourDomeSkyUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourDomeGroundUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourDomeUseTexUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourDomeMapUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourHeightMapUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourLightDirWorldUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSelfShadowUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMeshHeightUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourReverseFreqUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourFreqBiasBUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourFreqBiasPivotUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAoAmountUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAoRadiusUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowDirXZUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowSunTanUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowBiasUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowSoftnessUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowQualityUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourContactUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssModeUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssStrengthUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssWrapUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssTransmissionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssTintUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssRadiusUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssContrastUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssQualityUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssBaseDepthUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssThickScaleUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourSssMaxThickUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioLevelUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioMinUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioMaxUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioModBrightUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioModLitAmtUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioModSpecUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioModRimUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioModDomeUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioAffectPlayheadUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAudioAffectAntiUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourPlayheadWallXUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourAntiPlayheadWallXUniform;

        std::unique_ptr<juce::OpenGLShaderProgram> labelShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> labelPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> labelTexAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelTexUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelProjectionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelViewUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelResolutionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelCornerUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> labelClearUniform;

        std::unique_ptr<juce::OpenGLShaderProgram> tintShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> tintPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> tintColourUniform;
        unsigned int tintVbo = 0;

        std::unique_ptr<juce::OpenGLShaderProgram> contactShadowShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> contactPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> contactProjectionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> contactViewUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> contactStrengthUniform;
        unsigned int contactVbo = 0;

        std::unique_ptr<juce::OpenGLShaderProgram> postShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> postPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postTexUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postDepthUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postAuxUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postModeUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postStrengthUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postRadiusUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postThresholdUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postParamUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postResolutionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postInvProjUniform;
        /** Mode 20 motion blur: current inv(view), previous viewProj. */
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postMotionInvViewUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> postMotionPrevVpUniform;

        std::unique_ptr<juce::OpenGLShaderProgram> normalsShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> normalsPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> normalsNormalAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> normalsProjectionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> normalsViewUniform;

        unsigned int meshVbo = 0, meshIbo = 0;
        unsigned int floorVbo = 0;
        unsigned int labelVbo = 0;
        unsigned int sphereVbo = 0, sphereIbo = 0;
        unsigned int gizmoVbo = 0, gizmoIbo = 0;
        int meshIndexCount = 0;
        int floorVertexCount = 0;
        int sphereIndexCount = 0;
        int gizmoIndexCount = 0;
        bool meshNeedsUpload = false;
        bool sphereNeedsUpload = true;

        unsigned int shadowFbo = 0;
        unsigned int shadowDepthTex = 0;
        int shadowAtlasW = 0;
        int shadowAtlasH = 0;
        int shadowTileRes = 0;
        int shadowAtlasCascades = 0;
        juce::Matrix3D<float> shadowMatrix[kMaxShadowCascades] {};
        float cascadeSplitFar[kMaxShadowCascades] {};
        int cascadesBuilt = 0;

        std::unique_ptr<juce::OpenGLShaderProgram> shadowDepthShader;
        std::unique_ptr<juce::OpenGLShaderProgram::Attribute> shadowDepthPositionAttrib;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> shadowDepthLightVpUniform;

        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowAtlasUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCastShadowUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowMatrix0Uniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowMatrix1Uniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowMatrix2Uniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowMatrix3Uniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCascadeSplitsUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCascadeCountUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCascadeTransitionUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourShadowAtlasTilesUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCastBiasUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourCastSoftnessUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourViewZUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMatOverrideUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMatAlbedoUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMatRoughUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMatMetalUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourMatSpecularUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourGizmoXrayUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourXrayCenterUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourXrayRadiusUniform;
        std::unique_ptr<juce::OpenGLShaderProgram::Uniform> colourXrayAlphaUniform;

        juce::OpenGLTexture labelAtlas;
        juce::OpenGLFrameBuffer softFbo;
        /** Reused each soft readback — avoid alloc hitch every frame. */
        std::vector<juce::PixelARGB> softReadbackPixels;
        juce::OpenGLFrameBuffer postFboA;
        juce::OpenGLFrameBuffer postFboB;
        juce::OpenGLFrameBuffer ssgiHalfFbo;
        juce::OpenGLFrameBuffer ssgiHistoryFbo;
        juce::OpenGLFrameBuffer ssgiMomentsFbo;
        juce::OpenGLFrameBuffer ssgiNormalsFbo;
        unsigned int softDepthTex = 0;
        unsigned int softMsaaFbo = 0;
        unsigned int softMsaaColorRb = 0;
        unsigned int softMsaaDepthRb = 0;
        int softMsaaW = 0;
        int softMsaaH = 0;
        int softMsaaSamples = 0;
        unsigned int heightMapTex = 0;
        int heightMapW = 0;
        int heightMapH = 0;
        juce::OpenGLTexture domeMapTex;
        bool domeMapReady = false;
        bool labelAtlasReady = false;
        int softFboW = 0;
        int softFboH = 0;
        int postFboW = 0;
        int postFboH = 0;
        int ssgiHalfW = 0;
        int ssgiHalfH = 0;
        int ssgiHistoryW = 0;
        int ssgiHistoryH = 0;
        int ssgiMomentsW = 0;
        int ssgiMomentsH = 0;
        int ssgiNormalsW = 0;
        int ssgiNormalsH = 0;
        bool ssgiHistoryValid = false;
        bool ssgiMomentsValid = false;
        /** When > 0, getViewPixelBounds uses these dims (export override). */
        int exportForceW = 0;
        int exportForceH = 0;
        /** Previous frame view-projection for UE-style camera motion blur. */
        float motionPrevVp[16] = {
            1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
        };
        bool motionPrevVpValid = false;
        unsigned int postFrameIndex = 0;
        double floorGridSr = 0.0;
        bool floorGridLog = true;
        bool floorGridSoftBg = false;
        bool floorGridReverseFreq = true;
        /** Bump when soft-floor ribbon layout changes (forces VBO rebuild). */
        int floorRibbonEpoch = 0;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GlHost)
    };

    /** Transparent hit target for orbit/pan/zoom when the GL HWND is hidden in soft mode. */
    class HitLayer : public juce::Component
    {
    public:
        explicit HitLayer (Spectrogram3DComponent& owner);
        void mouseDown (const juce::MouseEvent& e) override;
        void mouseMove (const juce::MouseEvent& e) override;
        void mouseExit (const juce::MouseEvent& e) override;
        void mouseDrag (const juce::MouseEvent& e) override;
        void mouseUp (const juce::MouseEvent& e) override;
        void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel) override;
        void mouseDoubleClick (const juce::MouseEvent& e) override;
        bool keyPressed (const juce::KeyPress& key) override;
        bool keyStateChanged (bool isKeyDown) override;

    private:
        Spectrogram3DComponent& owner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HitLayer)
    };

    void timerCallback() override;
    void seedDefaultOrientation() noexcept;
    void clampCamera() noexcept;
    /** Orbit pivot height — centre of the heightfield volume. */
    float lookAtY() const noexcept { return meshHeight * 0.5f; }
    /** Y-up turntable: orbit around (panX, panY, panZ). Orbit/LMB only. */
    juce::Matrix3D<float> getTurntableViewMatrix() const noexcept;
    /** FPS freecam view from freecam eye + freecam yaw/pitch (never uses orbit pan). */
    juce::Matrix3D<float> getFreecamViewMatrix() const noexcept;
    /** Active view: freecam matrix while freecamActive, else turntable. */
    juce::Matrix3D<float> getActiveViewMatrix() const noexcept;
    /** World-space eye (freecam eye when active, else turntable-derived). */
    juce::Vector3D<float> getCameraEyePosition() const noexcept;
    void enterFreecamFromTurntable() noexcept;
    void exitFreecamToTurntable() noexcept;
    void applyFreecamLookDelta (float dxPixels, float dyPixels) noexcept;
    void freecamBasis (juce::Vector3D<float>& outRight,
                       juce::Vector3D<float>& outUp,
                       juce::Vector3D<float>& outForward) const noexcept;
    /** Camera basis: freecam axes when active, else turntable. */
    void cameraBasis (juce::Vector3D<float>& outRight,
                      juce::Vector3D<float>& outUp,
                      juce::Vector3D<float>& outForward) const noexcept;
    juce::Colour getClearColour() const noexcept;
    void applyBackgroundTransparency() noexcept;
    void layoutPresentation() noexcept;
    /** Soft FBO→Image path — Soft BG, docked Scope, or when bloom/DOF needs a colour+depth target. */
    bool usesSoftComposite() const noexcept;
    bool needsPostEffects() const noexcept
    {
        return bloomEnabled || dofEnabled || ssgiEnabled || ssrEnabled
            || tonemapEnabled || motionBlurEnabled;
    }
    bool needsAdvancedSsgi() const noexcept
    {
        return ssgiTemporalEnabled || ssgiDenoiseEnabled
            || ssgiHalfResEnabled || ssgiMeshNormalsEnabled;
    }
    void markLookDirty() noexcept;
    juce::Rectangle<int> getInnerFrameLocal() const noexcept;
    juce::Rectangle<int> getGlViewLocal() const noexcept;
    void showContextMenu (juce::Point<int> screenPos);
    bool isInMoveChrome (juce::Point<int> localPos) const noexcept;
    void applyChromeMode() noexcept;
    void markSoftContentDirty() noexcept;

    void meshSizeForQuality (int& outW, int& outH) const noexcept;
    /** Frequency rows after HF bias expansion (base H from quality). */
    int effectiveFreqMeshRows (int baseH) const noexcept;
    /** Bias weight B for the HF density ramp (0 when slider is 0). */
    float freqMeshBiasB() const noexcept;
    /** Mesh-row param t∈[0,1] → frequency axis u∈[0,1] (0=low, 1=high). */
    static float freqAxisFromMeshT (float t, float B, float pivot) noexcept;
    /** Inverse: frequency u → mesh-row param t (CDF). */
    static float meshTFromFreqAxis (float u, float B, float pivot) noexcept;
    void fillMeshColumn (int meshCol, const float* histCol, int histH);
    void seedMeshFromHistory (const std::vector<float>& history, int histW, int histH);
    void appendMeshColumnsFromHistory (const std::vector<float>& history, int histW, int histH, int numNew);
    void ensureIndexBuffer (int w, int h);
    void rebuildVerticesFromMeshDb (float brightness, float minDb, float maxDb);
    /** @return true if mesh vertices were rebuilt this call. */
    bool updateMeshFromSource();
    void rebuildFreqLabels (double sampleRate, bool logFreq);
    float worldZForFreq (float hz, double sampleRate, bool logFreq) const noexcept;
    static juce::String formatGridHz (float hz);

    void handleMouseDown (const juce::MouseEvent& e);
    void handleMouseMove (const juce::MouseEvent& e);
    void handleMouseExit();
    void handleMouseDrag (const juce::MouseEvent& e);
    void handleMouseUp (const juce::MouseEvent& e);
    void handleMouseWheel (const juce::MouseWheelDetails& wheel);
    void handleDoubleClick();
    static bool isRightMouse (const juce::MouseEvent& e) noexcept;
    /** Heightfield sample at world XZ (top surface Y). */
    float heightAtWorldXZ (float wx, float wz) const noexcept;
    /**
        Ctrl/Cmd+LMB: ray-pick the mesh under the cursor and set DOF focus to the
        view-space distance of the hit (matches the post DOF focus plane).
    */
    bool pickDofFocusAtLocalPoint (juce::Point<float> localPos) noexcept;

    SpectrogramComponent* dataSource = nullptr;
    SharedResources* theme = nullptr;
    std::unique_ptr<GlHost> glHost;
    std::unique_ptr<HitLayer> hitLayer;
    bool active = false;
    MsaaLevel msaaLevel = MsaaLevel::x4;
    bool transparentBackground = true;
    bool reverseFrequencyAxis = true;
    ChromeMode chromeMode = ChromeMode::floating;
    MeshQuality meshQuality = MeshQuality::medium;
    float freqMeshBias = 0.0f; // 0..1 — HF density boost amount
    float freqMeshBiasPivot = kFreqMeshBiasPivotDefault; // where boost begins (freq axis)
    float meshHeight = kDefaultMeshHeight;
    static constexpr int kMaxFreqMeshRows = 768; // hard cap — bias must not explode GPU cost
    static constexpr float kFreqMeshBiasMaxB = 4.0f; // B at slider=1 (with pivot=0 → ~2.33× rows)

    // Particle mode (default off — Spec3DParticleSystem allocated lazily on enable)
    bool particleModeEnabled = false;
    ParticleEmitMode particleEmitMode = ParticleEmitMode::continuous;
    ParticleBindingMode particleBindingMode = ParticleBindingMode::spectrogramTrail;
    /** Particles spawned per second (field total). Not a 0–1 scale. */
    float particleEmission = 200.0f;
    /** World-space spawn scatter. Default > 0 so particles don't stack on exact mesh points. */
    float particleSpawnJitter = 0.035f;
    float particleInitVelX = 0.0f;
    float particleInitVelY = 1.0f; // was "rise speed"
    float particleInitVelZ = 0.0f;
    float particleVelRandom = 0.0f;
    float particleLifespan = 0.0f;
    float particleLifespanRandom = 0.0f;
    float particleSize = 0.008f;
    /** When true: unlit emissive-only (skip PBR). When false: lit PBR + additive emissive. */
    bool particleEmissiveEnabled = false;
    float particleEmissiveStrength = 0.0f;
    float particleRoughness = 0.45f;
    float particleMetalness = 0.0f;
    float particleSpecular = 0.35f;
    bool particleForcesEnabled = false;
    bool particleWaterfallLock = true;
    /** CPU default; GPU hybrid integrate when true and compute available. */
    bool particleGpuSimEnabled = false;
    int particleMaxAlive = Spec3DParticleSystem::kDefaultMaxAlive;
    bool particleDebugOverlayEnabled = false;
    std::vector<ParticleForceModule> particleForceStack;
    uint32_t particleForceUidSerial = 1;
    ParticleMeshShape particleMeshShape = ParticleMeshShape::sphere;
    float particleInitRotX = 0.0f, particleInitRotY = 0.0f, particleInitRotZ = 0.0f;
    float particleInitRotRandom = 0.0f; // 0..1 scales random euler at spawn
    std::array<ParticleModSlot, kParticleModSlotCount> particleModSlots {};
    std::array<ParticleRandomSource, kParticleRandomSourceCount> particleRandomSources {};
    std::unique_ptr<Spec3DParticleSystem> particleSystem;
    double particleLastUpdateSec = 0.0;
    void ensureParticleSystem();
    void initDefaultParticleModSlots() noexcept;
    void initDefaultParticleForceStack() noexcept;

    bool lightingEnabled = false;
    float lightingAmount = 0.70f;
    float lightAzimuthDeg = -40.0f;
    float lightElevationDeg = 55.0f;
    float specularAmount = 0.35f;
    float roughnessAmount = 0.45f;
    float metalnessAmount = 0.0f;
    bool energyConservingEnabled = false;
    float rimAmount = 0.22f;
    juce::Colour lightColour { juce::Colours::white };
    juce::Colour rimColour { juce::Colours::white };
    bool domeFillEnabled = false;
    float domeFillStrength = 0.35f;
    juce::Colour domeSkyColour { 0xff7390bf };    // soft sky blue-grey
    juce::Colour domeGroundColour { 0xff403328 }; // warm ground
    bool domeTextureEnabled = false;
    DomeTextureSource domeTextureSource = DomeTextureSource::veniceSunset;
    juce::String domeTextureCustomPath;
    juce::CriticalSection domeTextureLock;
    juce::Image domeTextureImage;
    std::atomic<bool> domeTextureDirty { false };
    bool ssgiEnabled = false;
    float ssgiStrength = 0.40f;
    float ssgiRadius = 0.45f;
    ShadowQuality ssgiQuality = ShadowQuality::medium;
    bool ssgiTemporalEnabled = false;
    float ssgiTemporalAmount = 0.85f;
    bool ssgiDenoiseEnabled = false;
    float ssgiDenoiseAmount = 0.50f;
    SsgiDenoiseMode ssgiDenoiseMode = SsgiDenoiseMode::simple;
    ShadowQuality ssgiAtrousQuality = ShadowQuality::medium;
    bool ssgiHalfResEnabled = false;
    bool ssgiMeshNormalsEnabled = false;
    bool ssrEnabled = true;
    float ssrStrength = 0.55f;
    float ssrDistance = 0.55f;
    float ssrThickness = 0.40f;
    ShadowQuality ssrQuality = ShadowQuality::medium;
    float ssrFresnel = 0.75f;
    float ssrRoughnessInfluence = 0.85f;
    float ssrIntensity = 1.0f;
    float ssrEdgeFade = 0.15f;
    float ssrMetallicBias = 0.35f;
    float ssrDomeFallback = 0.65f;
    bool contactShadowEnabled = false;
    float contactShadowStrength = 0.45f;
    bool selfShadowEnabled = false;
    float selfShadowStrength = 0.85f;
    float selfShadowBias = 0.35f;       // acne / self-intersection bias
    float selfShadowSoftness = 0.85f;   // terminator / penumbra width (higher = softer)
    ShadowQuality selfShadowQuality = ShadowQuality::medium;
    bool castShadowsEnabled = false;
    ShadowMapResolution shadowMapResolution = ShadowMapResolution::r1024;
    int shadowCascadeCount = 1;
    float shadowCascadeDistributionExponent = 3.0f;
    float shadowCascadeTransitionFraction = 0.10f;
    bool debugSphereEnabled = false;
    float debugSphereDiameter = kDebugSphereDefaultDiameter;
    juce::Vector3D<float> debugSpherePosition { 0.6f, kDebugSphereDefaultDiameter * 0.5f, 0.6f };
    juce::Colour debugSphereAlbedo { juce::Colours::white };
    float debugSphereRoughness = 0.70f;
    float debugSphereMetalness = 0.0f;
    float debugSphereSpecular = 1.0f;
    bool ssaoEnabled = false;
    float ssaoStrength = 0.55f;
    float ssaoRadius = 1.0f;
    bool bloomEnabled = false;
    float bloomStrength = 0.45f;
    float bloomThreshold = 0.62f;
    bool motionBlurEnabled = false;
    float motionBlurAmount = kMotionBlurAmountDefault;
    float motionBlurMax = kMotionBlurMaxDefault;
    ShadowQuality motionBlurQuality = ShadowQuality::medium;
    bool dofEnabled = false;
    float dofFocusDistance = kDofFocusDefault;
    float dofFStop = kDofFStopDefault;
    float dofFocalLengthMm = kDofFocalLengthDefaultMm;
    ShadowQuality dofQuality = ShadowQuality::medium;
    float dofCocDilate = kDofCocDilateDefault;
    float dofEdgeSpill = kDofEdgeSpillDefault;
    bool tonemapEnabled = false;
    float tonemapExposureStops = -0.3f;
    ColorGrade colorGrade = ColorGrade::warmCinema;

    bool closedMeshEnabled = false;
    bool sssEnabled = false;
    float sssStrength = 0.45f;
    float sssWrap = 0.55f;
    float sssTransmission = 0.65f;
    juce::Colour sssTint { 0xffe8b090 }; // warm peach
    float sssRadius = 0.40f;
    float sssContrast = 0.50f;
    ShadowQuality sssQuality = ShadowQuality::medium;
    float sssThicknessScale = 0.50f;
    float sssMaxThickness = 0.70f;
    bool meshClosed = false; // last built topology (open vs extrude+cap)
    bool meshIndexReverseFreq = false; // winding baked for reverse-freq Z mirror

    melatonin::DropShadow panelShadow {
        { juce::Colours::black.withAlpha (0.55f), 16, { 0, 6 }, 0 }
    };

    std::vector<Vertex> cpuVertices;
    std::vector<uint32_t> cpuIndices;
    juce::CriticalSection meshLock;
    std::vector<float> meshDb;
    int meshW = 0;
    int meshH = 0;
    uint64_t lastHistorySerial = 0;
    float lastBrightness = -1.0f;
    float lastMinDb = 0.0f;
    float lastMaxDb = 0.0f;
    bool indicesValid = false;
    bool meshNeedsUpload = false;

    std::vector<FreqLabel> freqLabels;
    juce::Image labelAtlasImage;
    bool labelAtlasDirty = true;

    juce::Image softCompositeImage;
    /** Soft readback write target; swapped with softCompositeImage under lock. */
    juce::Image softCompositeBack;
    juce::CriticalSection softImageLock;

    CameraState camera;
    CameraState defaultCamera;
    bool waterfallFrozen = false;
    bool autoRotateEnabled = false;
    float autoRotatePeriodSec = kAutoRotatePeriodDefaultSec;
    double autoRotateLastTimeSec = 0.0;
    bool zoomOscillateEnabled = false;
    float zoomOscillateDepth = kZoomOscillateDepthDefault;
    float zoomOscillatePeriodSec = kZoomOscillatePeriodDefaultSec;
    float zoomOscillateBaseDistance = 3.0f;
    float zoomOscillatePhaseRad = 0.0f;
    double zoomOscillateLastTimeSec = 0.0;

    Spec3DRampSequence rampSequence;
    ColourRampBank* colourRampBank = nullptr;
    GradientRamp morphRamp;
    float rampPlayheadSec = 0.0f;
    double rampTimelineLastTimeSec = 0.0;
    bool morphRampActive = false;
    /** -1 = none; last solid/fade clip index that updated the 3D LUT. */
    int lastMorphClipIndex = -1;
    /** -1 = solid; 1 while inside a crossfade (for leave-fade detection). */
    int lastMorphFadeStep = -1;
    /** Monotonic revision for morphRamp — lerpRamps always starts at rev 1, which
        made setCustomColourRamp3D early-out and freeze the LUT mid-crossfade. */
    uint32_t morphRampSerial = 1;
    /** Lighting automation changed uniforms; fold into next soft frame (no extra FBO). */
    bool lightingUniformsDirty = false;
    double lastLightingSoftRedrawSec = 0.0;
    /** Reused layout buffer for applyMorph (no heap thrash). */
    std::vector<Spec3DRampSequence::LaidOutClip> morphLayoutCache;
    /** Reused by full mesh rebuilds to avoid per-frame Vertex allocations. */
    std::vector<Vertex> meshBuildVerts;
    void tickRampTimeline (float dt) noexcept;
    /** Updates 3D colour LUT + mesh colours for ramp morph/crossfade. */
    void applyMorphRampIfNeeded() noexcept;
    /**
        Lighting envelopes: write shader uniforms only.
        Does not mark soft dirty — next mesh/soft frame picks them up (avoids DOF thrash).
    */
    void applyMorphLightingAutomation() noexcept;
    void invalidateMorphSchedule() noexcept;

    bool audioLevelModEnabled = false;
    AudioLevelTarget audioLevelTarget = AudioLevelTarget::brightness;
    float audioLevelMinPercent = kAudioLevelMinPercentDefault;
    float audioLevelMaxPercent = kAudioLevelMaxPercentDefault;
    float audioLevelHpHz = kAudioLevelHpDefaultHz;
    float audioLevelLpHz = kAudioLevelLpDefaultHz;
    float audioLevelThresholdDb = kAudioLevelThresholdDefaultDb;
    AudioLevelSpeed audioLevelSpeed = AudioLevelSpeed::fast;
    bool audioLevelAffectPlayhead = false;
    bool audioLevelAffectAntiPlayhead = false;
    float audioLevelLive01 = 0.0f;
    std::function<float()> audioLevelProvider;

    float normalCuspAngleDeg = kNormalCuspDefaultDeg;
    NormalWeighting normalWeighting = NormalWeighting::angleAndArea;
    void computeTopSurfaceNormals (std::vector<Vertex>& verts, int w, int h);
    // Reused across rebuilds — avoids heap churn in the hot mesh path.
    std::vector<float> normalAccumX, normalAccumY, normalAccumZ;
    std::vector<float> normalBestW, normalBestNx, normalBestNy, normalBestNz;

    void applyZoomOscillateDistance() noexcept;
    void captureZoomOscillateBaseFromCamera() noexcept;
    juce::Point<float> lastDrag {};
    juce::Point<float> rightClickStart {};
    bool rightClickCandidate = false;
    bool rightClickDragged = false;
    /**
        Freecam is a separate camera (not the orbit/turntable rig):
          freecamEye + freecamYawDeg + freecamPitchDeg (FPS: 0=horizon, + = look up).
        Active while RMB held or coasting. Wheel = speed. Settings: speed / look / invert Y.
    */
    bool freecamActive = false;
    bool freecamRmbHeld = false;
    juce::Vector3D<float> freecamEye { 0, 0.5f, 3.0f };
    float freecamYawDeg = 0.0f;
    float freecamPitchDeg = 0.0f; // FPS pitch: + look up, − look down
    juce::Vector3D<float> flyVel { 0, 0, 0 };
    double freecamLastTimeSec = 0.0;
    float freecamSpeedScale = kFreecamSpeedMax;
    float freecamLookSensitivity = 0.22f;
    bool freecamInvertY = false;
    void tickFreecam (float dt) noexcept;
    void nudgeFreecamSpeedFromWheel (float wheelDeltaY) noexcept;
    void notifyFreecamSpeedChanged() noexcept;
    static bool freecamKeyDown (juce::juce_wchar c) noexcept;
    static float freecamLevelToScale (float level1to8) noexcept;
    static float freecamScaleToLevel (float scale) noexcept;
    enum class DragMode { none, orbit, pan, screenPan, freecamLook, dolly, gizmoX, gizmoY, gizmoZ };
    DragMode dragMode = DragMode::none;
    DragMode gizmoHoverAxis = DragMode::none;
    DragMode hitTestDebugGizmo (juce::Point<float> localPos) const noexcept;
    bool tryPickDebugGizmo (juce::Point<float> localPos) noexcept;
    void updateGizmoHover (juce::Point<float> localPos) noexcept;
    bool isGizmoXrayActive() const noexcept;
    void dragDebugGizmo (juce::Point<float> localPos) noexcept;
    juce::Point<float> gizmoDragStartPos {};
    juce::Vector3D<float> gizmoDragStartSpherePos {};
    /** Elevation above the horizon (0 = edge-on to the floor, 90 = top-down). */
    static constexpr float kMinPitchDeg = 5.0f;
    static constexpr float kMaxPitchDeg = 89.0f;

    juce::ComponentDragger moveDragger;
    juce::ComponentBoundsConstrainer constrainer;
    bool movingByChrome = false;

    std::unique_ptr<juce::ResizableCornerComponent> resizer;
    std::unique_ptr<juce::Component> zoomHandle;
    /** UE-style camera speed chip (top-right); click opens speed slider. */
    std::unique_ptr<juce::Component> freecamSpeedHandle;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Spectrogram3DComponent)
};
