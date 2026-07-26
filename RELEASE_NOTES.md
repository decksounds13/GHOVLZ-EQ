# GHOVLZ! EQ — Progress Update

**Date:** 2026-07-26  
**Scope:** General day summary (not a commit-by-commit changelog)

---

## Spectrum analyser

- Switched analysis to an Ableton-style overlapping model: audio writes into fixed ring buffers; a background thread FFTs the latest Block window on the Refresh cadence instead of waiting for non-overlapping full blocks.
- Default Refresh set for smoother UI updates; Spectrum/FFT menus expose Refresh (and Spectrum Average) controls.
- Improves feel at larger FFT sizes without the heavy/choppy wait of block-aligned analysis.

## Oscilloscope

- Restored High-quality continuous min/max envelopes with soft fill; fixed cross-window connector artefacts (path breaks at gaps and at the in-place write seam).
- Separate line/glow tuning for compact strip vs expanded overlay.
- Compact High-quality path no longer strokes max and min as two full traces (that read as a double waveform in the short strip); soft fill + stubs in compact, dual-envelope stroke kept for expanded.

## Factory default state

- Hard-coded factory plugin state from the user preset **“new default”** (`FactoryDefaultsState.h`), applied on processor construction and attached to the built-in **Default** theme.
- Notable factory settings: **8192** FFT block, **30 ms** refresh, Auto Gain on, Linear Phase, Band 1 sat + post on (Tube), spectrum curve smoothness High, compact/expanded osc line widths and glow as tuned.
- Built-in Default theme text colour aligned with that preset.

## UI fixes

- Level meters were getting buried under the EQ graph after z-order refreshes; meters are raised with chrome again. Input L/R meters are also shown (they were laid out but never made visible).

## Highpass / linear phase

- **Linear Phase:** FIR post-window gain restore used DC for all designs. Highpass target DC ≈ 0 crushed the impulse response (lowpass was unaffected). Restore now uses **Nyquist** when that is the stronger passband; stopband magnitude can be true zero.
- **Minimum Phase clicks:** Steeper HP/LP slopes hard-swapped cascaded IIR coeffs without resetting filter state. Stages now reset on slope/stage-count changes; HP/LP cutoff/Q smoothing lengthened to reduce zipper pops.

---

*Later releases can track notes commit-to-commit from this point forward.*
