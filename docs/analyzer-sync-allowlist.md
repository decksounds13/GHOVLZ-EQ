# Analyzer sync allowlist (EQ developer note)

The Analyzer / Scope product is kept in step with EQ via **Port** + **Review** bots.

| | |
|--|--|
| EQ code | branch **`main`** (this tree) |
| Analyzer code | branch **`analyzer/main`** |
| Canonical allowlist | `SYNC_ALLOWLIST.md` on **`analyzer/main`** |
| Local copy when AnalyzerSuite is checked out | `../AnalyzerSuite/SYNC_ALLOWLIST.md` |

## Rule

**New analyzer on EQ ⇒ update the allowlist** so automation knows which files to port.

Example: adding a THD meter

1. Add `Source/ThdMeterComponent.cpp` / `.h` (and settings UI if any) on `main`.
2. On `analyzer/main`, add those paths under **Allow** in `SYNC_ALLOWLIST.md` (or rely on Port to add them if they match **Auto-allow patterns** and draft-pr is enabled).
3. Do not mark `EqProcessor` as Allow; only surgical feed hooks stay gray.

## Auto-allow patterns (summary)

Port treats matching **new** paths as allow-candidates (then records them explicitly on the list):

- `Source/*Meter*`
- `Source/Scope*`
- `Source/Visualizer/**`
- `Source/*Spectrogram*` / gonio / oscilloscope / histogram / loudness / stereogram
- Menu GUI names containing Meter, Scope, Spectro, Analyser, etc.

Full rules: `SYNC_ALLOWLIST.md` on `analyzer/main`.

## Grok Build

`AGENTS.md` in this project requires the allowlist update (or a commit note for Port) whenever you add Scope/meter features.
