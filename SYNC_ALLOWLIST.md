# AnalyzerSuite ↔ EQ sync allowlist

Port Bot may only copy or adapt paths listed under **Allow** (or matching **Auto-allow patterns**).  
Review Bot fails the PR if any change falls outside Allow/patterns (unless Port documents a human-approved exception).

| | |
|--|--|
| EQ source | `decksounds13/GHOVLZ-EQ` branch **`main`** |
| Analyzer target | same repo branch **`analyzer/main`** (PRs from `sync/eq-*`) |
| This file | Lives on **`analyzer/main`**. Keep it current when adding meters. |

---

## Adding a new analyzer (required process)

When EQ `main` gains a new Scope/meter/analyser feature (e.g. **THD meter**):

1. **Implement on EQ `main`** as usual (code, jucer, UI wiring).
2. **Update this allowlist in the same effort** — either:
   - human / Grok Build commits the list change on `analyzer/main`, or
   - Port’s next run includes an **allowlist update** in the sync PR (preferred once draft-pr is on).
3. List **every new path** the Analyzer product needs:
   - component `.cpp` / `.h`
   - settings UI under `Source/Menu/…`
   - shared helpers only used by that meter
   - theme/resource assets if required
4. Do **not** add EqProcessor DSP, band faceplate, or mod-matrix files just because the meter “reads audio” — Analyzer keeps pass-through + analysis only.
5. Name new modules so auto-patterns match (see below), e.g. `ThdMeterComponent.cpp`, `Source/Menu/Gui/ThdMeterSettingsComponent.*`.

### Auto-allow patterns (new files can match without a manual bullet)

If a path is **new on EQ `main`** and matches any pattern below, Port treats it as **Allow-candidate** and **must** add an explicit bullet under Allow in the same PR (or report it as “allowlist update needed” in report-only):

| Pattern | Examples |
|---------|----------|
| `Source/*Meter*.{cpp,h}` | `ThdMeterComponent.cpp`, `ScopeLevelMeterModule.h` |
| `Source/*Analyser*.{cpp,h}` / `*Analyzer*.{cpp,h}` | `AnalyserDefaults.cpp` |
| `Source/*Goniometer*` `*Oscilloscope*` `*Spectrogram*` `*Stereogram*` `*Histogram*` `*Loudness*` | existing meters |
| `Source/Scope*.{cpp,h}` | `ScopeModules.h`, `ScopeLayoutPresets.cpp` |
| `Source/Visualizer/**` | graph layers, analyser core |
| `Source/ColourRamp/**` | Spec3D ramps |
| `Source/Menu/**/*{Meter,Scope,Spectro,Analyser,Analyzer,Gonio,Histogram,Loudness,Thd,THD}*` | settings panels |
| `Source/FramedFloatingScopeWindow.*` | floating scope chrome |

**Not** auto-allowed (even if name is cute): `*EqProcessor*`, `*EqEditor*`, `*EqBand*`, `*DynamicEq*`, `*ModSection*`, `*LinearPhase*`, `*SideCheck*` (DSP), filter design headers used only for EQ curves.

If a new analyzer needs a file outside patterns (e.g. a shared `AudioAnalysisFifo.h`), Port/human adds an **explicit Allow bullet** — patterns alone are not enough for gray shared code.

---

## Allow (safe to port)

### Visualizer core
- `Source/Visualizer/**`

### Metering / scope UI components
- `Source/GoniometerComponent.cpp` / `.h`
- `Source/OscilloscopeComponent.cpp` / `.h`
- `Source/SpectrogramComponent.cpp` / `.h`
- `Source/Spectrogram.cpp` / `.h` (if present)
- `Source/Spectrogram3DComponent.cpp` / `.h`
- `Source/StereogramComponent.cpp` / `.h`
- `Source/HistogramComponent.cpp` / `.h`
- `Source/LoudnessComponent.cpp` / `.h`
- `Source/Spec3DParticleSystem.cpp` / `.h`
- `Source/ScopeLevelMeterModule.cpp` / `.h`
- `Source/ScopeModules.h`
- `Source/ScopeLayoutPresets.cpp` / `.h`
- `Source/ScopePaneChrome.h`
- `Source/VerticalGradientMeter.cpp` / `.h`
- `Source/FramedFloatingScopeWindow.cpp` / `.h`
- `Source/ThdMeterComponent.cpp` / `.h` (broadband Scope THD)
- `Source/ColourRamp/**` (Spec3D ramps used by analyzers)

### Menu / analyzer prefs (narrow)
- `Source/Menu/AnalyserDefaults.cpp` / `.h`
- `Source/Menu/Gui/Spectrogram3DSettingsComponent.cpp` / `.h`
- Paths under `Source/Menu/` that only affect Scope / analyzer settings  
  (Port must justify each file in the PR body, or match Auto-allow patterns)

### Themes / assets used by meters
- `Themes/**` when meter chrome depends on them
- `Resources/**` only if analyzer visuals need the asset

### This policy file
- `SYNC_ALLOWLIST.md` (Port may update when promoting Allow-candidates)
- `SYNC_STATE.md` (SHA bookkeeping after Review PASS / human merge)

---


### Spectral / FFT (ported from EQ main)
- `Source/Spectral/SpectralFftEngine.cpp` / `.h`
- `Source/Spectral/SpectralMethod.h`
- `Source/Spectral/SpectralDynamicsProcessor.cpp` / `.h`
- `Source/SpectrogramReassignment.cpp` / `.h`
- `Source/Menu/Gui/SpectrumComponent.cpp` / `.h`
- `Source/Menu/Gui/SpectrogramSettingsComponent.cpp` / `.h` (if present)
- `Source/Menu/Gui/LevelMetersComponent.cpp` / `.h`
- `Source/ParticleEmitterTypes.h`
- `Source/ParticleForceModule.h`

## Deny (never port into AnalyzerSuite)

- EQ band DSP and channel processing beyond dry pass-through  
  (`DynamicEq`, band saturation as product features, SideCheck DSP, linear-phase FIR product path, etc.)
- Faceplate / band editor chrome that Analyzer is stripping  
  (`ModSection*`, band knobs layout for EQ product)
- Product identity for the EQ  
  `ParametricEqProject.jucer`, VST3-only targets, EQ marketing as Analyzer copy source
- Any push/commit to EQ **`main`** by Port

---

## Gray area (human or Review “needs-human”)

- `Source/EqProcessor.*` / `Source/EqEditor.*` / `Source/MainComponent.*`  
  Surgical diffs only for pass-through + Scope wiring — never wholesale overwrite.
- `Source/FrequencyResponseComponent.*` — cherry-pick only if Analyzer still uses that surface.
- `Source/ParticleNodeGraph/**` — only if Analyzer still ships Spec3D particles.
- `Source/Menu/SharedResources.*` — extract analyser/Spec3D keys only.
- Projucer / VS project files — AnalyzerSuite targets only; never restore `ParametricEqProject_*` VST3 projects.

---

## Decision rule

1. On allowlist or matching **Auto-allow pattern** → **Allow** (and update this file if pattern-only).  
2. Clearly EQ DSP/product → **Deny**.  
3. Shared shell / uncertain → **Gray** / skip whole-file; extract or NEEDS-HUMAN.  
4. Never guess a Deny path into Analyzer.
