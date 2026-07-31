# GHOVLZ! EQ — Release Notes

**Checkpoint:** 2026-07-30 (post–Scope mode / spectrogram)  
**Status:** Spectrogram + quad Scope metering view landed. Known follow-ups: spectrogram history clears on pane resize; fullscreen spectrogram CPU is heavy vs compact.

---

## Product snapshot

Compact parametric EQ VST3/AU with overlapping spectrum analysis, beat-synced oscilloscope, stereo goniometer, scrolling spectrogram, per-band saturation, spectral lattice dynamics, SideCheck, linear-phase FIR path, UI themes, EQ presets, and a **Scope** metering layout (dry passthrough) — still a small Release binary with headroom for a future metering-only product.

---

## This checkpoint

### Spectrogram
- Compact strip between UI dice and EQ preset bar (osc-like height, slightly wider).
- **Spec** toggle under Gon; tool column: speed + / − and expand (fullscreen).
- Colour schemes: Classic, **Inferno (default)**, Magma, Viridis, Ice, Greyscale, Heat.
- Settings tab (Look / Behaviour): scheme, brightness, FFT size, channel, scroll speed, floor/ceiling dB, smoothing, log freq, freeze; Save Default via analyser defaults.
- Wired through processor audio push like osc/gon.

### Scope mode (quad metering)
- Bottom-trim **Scope** button (SideCheck styling), right of SideCheck.
- Graph splits into four resizable panes (drag gold crosshair):
  - Top-left: Goniometer  
  - Top-right: Spectrum / FFT visualizer  
  - Bottom-left: Oscilloscope  
  - Bottom-right: Spectrogram  
- Minimized-state tools sit at the bottom of each pane; expand still maximizes that meter.
- While Scope is on: **EQ / spectral / SideCheck DSP off** (dry + latency-matched passthrough); meters, analyser, and scopes keep running.
- Persisted in `ui_prefs.xml` (`scopeModeEnabled`).

### Eco
- Eco now disables **all scopes** (OSC / Gon / Spec) as well as analyser/FFT, and exits Scope mode if active.
- Prior scope toggles restore when Eco turns off.
- Eco and Scope are mutually exclusive.

### Spectrogram perf follow-up (in tree after this checkpoint)
- Fixed internal resolution (480×160) + left-scrolling image — resize no longer wipes history.
- Colour LUT, ≤3 FFT columns/tick, 30 Hz timer, paint is a single stretched blit.

---

## Earlier highlights (themes → scopes)

### UI themes & chrome
- UI theme CallOutBox stays open for Rename / Duplicate / Delete.
- Option box faceplate wash; knob tint; shared theme colours for glow/settings/scopes.
- Top chrome: Bypass, A–D, UI + dice, preset bar, Eco / OSC / Gon / Spec, undo/redo, Settings.

### Meters & scopes (pre-spectrogram)
- Overlapping spectrum analyser; Spectrum / FFT settings.
- Oscilloscope + goniometer compact/expand with settings tabs.

### EQ / DSP
- Per-band HP/LP slopes; spectral lattice; factory defaults; LP FIR HP restore; min-phase slope click fixes; per-band sat.

### Build notes
- Prefer **Release** for UI/CPU.
- Windows JUCE 9; Mac AU proven; Mac VST3 still hardening.

---

*Next: spectrogram resize-history + fullscreen cost; metering-only VST remains deferred.*
