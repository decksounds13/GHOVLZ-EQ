# Decksounds Analyzer Suite

**Fork of GHOVLZ EQ for analyzer / Scope development.**  
The EQ product lives in `../ParametricEqProject` and must not be modified from this tree.

## Product (v0 target)

- Standalone **.exe** (primary) — **always Scope** (tiled default)
- Scope modules (meters, spectrum, spectrogram, Spec3D, osc, gonio, …)
- **No faceplate / mod / EQ graph mode** (code paths stripped from product shell)
- **No EQ DSP / saturation** in the audio path — passthrough + analyse only
- **Bypass** = monitoring off (mute + no scope feed)
- **Eco** = lighter analyzer/visual cost (not DSP)

## Build

1. Open `Builds/VisualStudio2022/AnalyzerSuite.sln`
2. Build configuration **Debug | x64**, project **AnalyzerSuite - Standalone Plugin**
3. Run the generated Standalone `.exe` (audio device settings in the app menu)

Or resave from Projucer: `AnalyzerSuite.jucer` with format **Standalone**.

## Feed audio from a DAW

Use the sibling project **`../AnalyzerLink`** — VST3 plugin **Decksounds Analyzer Link**:

1. Build/install **Decksounds Analyzer Link** (VST3).  
2. Run this **Analyzer Suite** Standalone.  
3. In the DAW, put **Decksounds Analyzer Link** on the track (or bus) you want to analyze.  
4. Play — Standalone pulls that audio automatically (same machine, shared memory). The Link plugin is passthrough so the track still plays in the DAW.

No virtual cable required. Device input still works when Link is not streaming.

## Plan

See the Analyzer Suite fork plan in the Grok session / team notes:

1. Phase 0 — scaffold (this folder + Standalone)  
2. Phase 1 — gut `processBlock` to analyzer-only  
3. Phase 2 — strip faceplate / mod / FRC; force Scope UI  
4. Phase 3 — suite UX polish (prefs, defaults)  

## Relation to EQ

| | ParametricEqProject | AnalyzerSuite |
|--|---------------------|---------------|
| Role | Shipping EQ + scopes | Analyzer suite / lab |
| Formats | VST3 | Standalone (v0) |
| DSP | Full EQ | None (dry + analyzers) |

### Keeping Analyzer current (Port + Review bots)

See `docs/BOT_SYNC_SETUP.md`, `SYNC_ALLOWLIST.md`, and `SYNC_STATE.md`.

**Git:** this product lives on branch **`analyzer/main`** of `decksounds13/GHOVLZ-EQ`.  
EQ shipping product stays on **`main`**. Never merge Analyzer renames into `main`.  
Sync PRs: `sync/eq-*` → `analyzer/main` only.
