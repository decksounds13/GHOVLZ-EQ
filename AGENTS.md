# Agent notes — Parametric EQ

## Two products: you edit EQ; bots update Analyzer

| Product | Tree | Branch | Who changes it |
|---------|------|--------|----------------|
| **GHOVLZ EQ** (this project) | `ParametricEqProject` | `main` | **You** |
| **Analyzer Suite** | sibling `../AnalyzerSuite` | `analyzer/main` | **Grok Port / Review bots** |

- Implement, fix, commit, and push **here** (EQ `main`).
- **Do not** hand-port the same fix into AnalyzerSuite; Port absorbs allowlisted EQ changes automatically.
- Global rule: `~/.grok/rules/eq-vs-analyzer-suite.md`.

## UI chrome text

Follow the global rule **no-ellipsis-labels** (`~/.grok/rules/no-ellipsis-labels.md`):

- Never ship `...` / ellipsis on buttons, labels, tabs, slider captions, or **numeric value readouts**.
- Chrome strings are **plain text** (ASCII words/abbreviations). Size bounds to the measured string; use `Label::setMinimumHorizontalScale (1.0f)`.
- OptionBox spectral labels are `"Res"` and `"Amt"` — full letters, never truncated.
- Knob value popups: use `KnobTheme::showValueTextBox` so widths follow the full number string.

## Builds

Do not run full builds unless the user asks or a change is highly likely not to compile.

## AnalyzerSuite sync allowlist (when adding meters / Scope features)

Analyzer product lives on branch **`analyzer/main`** (same GitHub repo). Port/Review bots only copy paths in **`SYNC_ALLOWLIST.md`** on that branch.

**When you add or substantially extend an analyzer, meter, or Scope module on EQ `main`** (examples: THD meter, new spectrogram mode, scope pane, level meter):

1. Prefer names that match allowlist auto-patterns (`*Meter*`, `*Spectrogram*`, `Scope*`, `Source/Visualizer/**`, Menu GUI with Meter/Scope/Spectro/Analyser in the name).
2. **Update the allowlist** so Port will pick up the new files:
   - Canonical file: `SYNC_ALLOWLIST.md` on branch `analyzer/main`  
     (sibling tree: `../AnalyzerSuite/SYNC_ALLOWLIST.md` when that checkout is on `analyzer/main`).
   - Add explicit bullets for every new `.cpp`/`.h`, settings panel, and assets the Analyzer app needs.
3. Do **not** put pure EQ DSP / faceplate / mod-matrix paths on the allowlist.
4. Shell wiring (`MainComponent`, `EqProcessor` feed hooks, `SharedResources`) stays **gray** — note surgical needs in the PR/commit message; do not wholesale-list them as Allow.
5. If you only touch EQ in this session, leave a short note in the commit message: `allowlist: add Source/ThdMeterComponent.*` so Port can promote the list on the next sync.

See also: `../AnalyzerSuite/docs/BOT_SYNC_SETUP.md` and `docs/analyzer-sync-allowlist.md` in this tree.
