# GHOVLZ! EQ — Release Notes

**Checkpoint:** 2026-07-30  
**Status:** Pre-spectrogram / pre-metering-VST baseline. Working tree was clean at `1cd6fc4`; this file documents the shipped product so the next feature wave has a clear freeze point.

---

## Product snapshot

Compact parametric EQ VST3/AU with overlapping spectrum analysis, beat-synced oscilloscope, stereo goniometer, per-band saturation, spectral lattice dynamics, SideCheck, linear-phase FIR path, UI themes, and EQ presets — still a small Release binary with headroom for more metering chrome.

---

## Highlights since last notes pass (2026-07-26 → now)

### UI themes & chrome
- UI theme list (CallOutBox) stays open for Rename / Duplicate / Delete without dismissing on right-click.
- Option box paints with the faceplate radial wash (`pluginBackground2` → `pluginBackground`).
- Knob tint/multiply, glow disable, oscilloscope/goniometer colour unification, and settings-button theming wired through shared theme colours.
- Top chrome: Bypass, A–D reference slots, UI theme picker + dice randomize, centered EQ preset bar, Eco / OSC / Gon toggles, undo/redo, Settings.

### Meters & scopes
- Overlapping (Ableton-style) spectrum analyser with Spectrum / FFT settings tabs.
- Compact + expandable oscilloscope (beat zoom, ST / L-R, scroll vs overwrite, HQ envelopes).
- Compact + expandable goniometer with correlation meter and matching settings tab (quality, line opacity, compact/expanded glow).

### EQ / DSP
- Per-band HP/LP slopes; spectral per-band lattice.
- Factory defaults hard-coded from the “new default” preset (`FactoryDefaultsState.h`).
- Linear-phase highpass FIR restore fixed (Nyquist passband); min-phase HP/LP slope changes reset state to avoid clicks.
- Per-band saturation (Tape / Tube / Diode / Dual-Triode) with OS and Pre/Post.

### Build notes (ops)
- Prefer **Release** builds for UI/CPU (Debug is dramatically heavier).
- Windows: JUCE 9.0.0 + Gin modules layout; Mac AU path proven on MacinCloud; Mac VST3 still hardening (manifest / SDK paths).

---

## Explicitly not in this checkpoint

- Spectrogram strip (next: left of preset bar, right of UI dice; osc-like height, slightly wider; colour schemes + settings tab).
- Metering-only VST that hosts scopes alone (later).

---

## Earlier notes (2026-07-26)

### Spectrum analyser
- Ring-buffer write + background FFT on Refresh cadence (overlapping windows).
- Default Refresh tuned for smoother UI; Spectrum Average exposed in menus.

### Oscilloscope
- HQ continuous min/max envelopes with soft fill; connector artefact fixes.
- Separate line/glow for compact vs expanded; compact HQ avoids double-trace look.

### Factory default state
- Notable factory: **8192** FFT, **30 ms** refresh, Auto Gain, Linear Phase, Band 1 sat + post Tube, spectrum smoothness High.

### UI / HP fixes
- Level meters re-raised with chrome; input L/R meters made visible.
- Linear-phase HP FIR and min-phase slope click fixes as above.

---

*Next commit stream: spectrogram + settings; metering product remains deferred.*
