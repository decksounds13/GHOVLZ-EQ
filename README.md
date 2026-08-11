# Decksounds Analyzer Suite

**Fork of GHOVLZ EQ for analyzer / Scope development.**  
The EQ product lives in `../ParametricEqProject` and must not be modified from this tree.

## Product (v0 target)

- Standalone **.exe** (primary)
- Scope mode modules (meters, spectrum, spectrogram, Spec3D, osc, gonio, …)
- Spectrum analyzer graph (no EQ band curves / faceplate / mod section)
- **No EQ DSP** — pass-through + analysis only (in progress)

## Build

1. Open `Builds/VisualStudio2022/AnalyzerSuite.sln`
2. Build configuration **Debug | x64**, project **AnalyzerSuite - Standalone Plugin**
3. Run the generated Standalone `.exe` (audio device settings in the app menu)

Or resave from Projucer: `AnalyzerSuite.jucer` with format **Standalone**.

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
Do not push Analyzer-only work onto EQ `main` until git topology is split (separate remote or `analyzer/main`).
