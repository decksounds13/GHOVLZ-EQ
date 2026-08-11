# AnalyzerSuite ↔ EQ sync allowlist

Port Bot may only copy or adapt paths listed under **Allow**.  
Review Bot fails the PR if any change falls outside Allow (unless Port documents an explicit one-off exception you approved).

Source of truth for EQ: GitHub `decksounds13/GHOVLZ-EQ` (or the remote named in `SYNC_STATE.md`).  
Target: **this** AnalyzerSuite repository / `analyzer/*` branch only.

## Allow (safe to port)

### Visualizer core
- `Source/Visualizer/**`

### Metering / scope UI components
- `Source/GoniometerComponent.cpp` / `.h`
- `Source/OscilloscopeComponent.cpp` / `.h`
- `Source/SpectrogramComponent.cpp` / `.h`
- `Source/Spectrogram3DComponent.cpp` / `.h`
- `Source/StereogramComponent.cpp` / `.h`
- `Source/HistogramComponent.cpp` / `.h`
- `Source/LoudnessComponent.cpp` / `.h`
- `Source/Spec3DParticleSystem.cpp` / `.h`
- `Source/ColourRamp/**` (Spec3D ramps used by analyzers)

### Menu / analyzer prefs (narrow)
- `Source/Menu/AnalyserDefaults.cpp` / `.h`
- `Source/Menu/Gui/Spectrogram3DSettingsComponent.cpp` / `.h`
- Paths under `Source/Menu/` that only affect Scope / analyzer settings  
  (Port must justify each file in the PR body)

### Themes / assets used by meters
- `Themes/**` when meter chrome depends on them
- `Resources/**` only if analyzer visuals need the asset

## Deny (never port into AnalyzerSuite)

- EQ band DSP and channel processing beyond dry pass-through  
  (`DynamicEq`, band saturation as product features, SideCheck DSP, linear-phase FIR product path, etc.)
- Faceplate / band editor chrome that Analyzer is stripping  
  (`ModSection*`, band knobs layout for EQ product, FRC as EQ editor — port graph math only if needed for spectrum)
- Product identity for the EQ  
  `ParametricEqProject.jucer`, VST3-only targets, EQ `FEATURES.md` marketing as the source of Analyzer copy
- Any edit under a separate ParametricEq working tree (Port never touches EQ)

## Gray area (human or Review “needs-human”)

- `Source/EqProcessor.*` / `Source/EqEditor.*` / `Source/MainComponent.*`  
  Analyzer may need **surgical** diffs for pass-through + Scope-only UI — never wholesale overwrite from EQ.
- `Source/ParticleNodeGraph/**` — only if Analyzer still ships Spec3D particles; otherwise skip.
- Projucer / VS project files — prefer AnalyzerSuite’s own targets; do not restore `ParametricEqProject_*` VST3 projects.

## Decision rule

If a changed EQ file is not clearly in **Allow**, **skip it** and list it under “Skipped (EQ-only or gray)” in the PR.  
Do not guess.
