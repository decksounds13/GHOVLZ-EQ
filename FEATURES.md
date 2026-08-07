# Decksounds — GHOVLZ! EQ with SideCheck™

**A serious parametric EQ with Mid/Side control and metering built in.**

*v1.0.0-beta · VST3*

---

### One-liners (pick one)

- Shape the tone. Keep the Side under control. See what you’re doing.
- Eight bands, real dynamics, and Mid/Side that actually earns its place.
- An EQ you can mix on — and trust when the image gets messy.

---

## What it is

**GHOVLZ! EQ** is a parametric equalizer for mixing and mastering. You get eight flexible bands, two kinds of dynamics (classic Dynamic EQ and spectral resonance control), saturation, modulation, and a full set of analyzers so you don’t need a second plugin just to see the signal.

The signature feature is **SideCheck™**: when the Side gets louder than the Mid in a frequency range, SideCheck pulls that Side energy back. You see the reduction on the EQ curve while it happens.

---

## Equalizer

Eight bands. Each one can be a bell, low or high shelf, notch, band-pass, high-pass, low-pass, tilt shelf, flat tilt, all-pass, band shelf, Baxandall, brickwall cut, or vintage (Pultec-style) shelf — you’re not locked into a fixed layout.

Out of the box it’s set up like a practical channel strip: high-pass, low shelf, four bells, high shelf, low-pass. Change any of them as you like.

- Frequency from **20 Hz to 20 kHz**
- Gain **±24 dB**
- Q from **0.15 to 10**
- High-pass / low-pass slopes: **6, 12, 24, 48, or 96 dB per octave**
- **Proportional Q** on bells — boosts and cuts get more focused as you push them harder
- Process each band in **stereo, mid, side, left, or right**
- **Minimum phase** for zero EQ latency, or **linear phase** when you need matched timing (~11 ms at 48 kHz)
- Zoom the curve display to **±6, ±12, or ±24 dB** so fine moves stay readable

---

## Dynamics

### Dynamic EQ

Each band can move only when the signal needs it. Detection listens to the dry signal before the EQ, so the band doesn’t chase its own boosts and cuts. You set threshold, attack, and release the same way you would on a compressor — but the “gain” is the EQ move itself.

### Spectral dynamics

For harsh resonances and build-ups that a single bell can’t catch cleanly. It works like a dense bank of narrow filters (not a big FFT on the audio path), so you can tame or lift resonant energy with finer control. You can expand instead of cut, bias the focus toward lows or highs, and detect from the main input or an external sidechain.

### SideCheck™

After the EQ (and spectral processing), SideCheck watches Mid vs Side. If Side is louder than Mid in the ranges you care about, it turns Side down. Use the full-quality multi-band mode for detail, or a lighter three-band mode when you want less CPU. Speed, strength, and the frequency range it acts on are all adjustable. When it’s idle, it gets out of the way.

### External sidechain / MIDI

Bands can also duck from an external key input, or from MIDI, using the same timing controls as the dynamics above.

---

## Saturation

Add harmonics without leaving the EQ:

- Per-band flavors: **Tape, Tube, Diode, Dual-Triode**
- Run saturation **before** or **after** the band’s EQ move
- Optional oversampling up to **8×** for cleaner drive
- A second saturation stage after spectral processing for bus-style glue or grit

---

## Modulation

Three LFOs (sine, triangle, square, saws), a drawable custom shape, and an envelope follower can drive band frequency, gain, and Q through a 20-slot matrix. Rates can free-run or lock to the host tempo. Depth can swing both ways or only in one direction. MIDI can retrigger the modulators when you need them locked to performance.

---

## Metering and analysis

Meant to replace a pile of separate analyzer plugins:

- **Spectrum** — see the signal before and after the EQ, with large FFT sizes and averaging
- **FFT bars** — denser bar view with glow and custom colour ramps
- **Oscilloscope** — beat-synced waveform; zoom by bars; summed or split left/right
- **Goniometer** — stereo image plus correlation from −1 to +1
- **Spectrogram** — scrolling frequency view with several colour maps, optional sharper “enhanced frequency” mode, freeze, and custom gradients
- **Level meters** — input and output, peak and RMS, switchable between L/R and Mid/Side
- **Scope mode** — all four views at once (goniometer, spectrum, oscilloscope, spectrogram). Right-click Scope to choose **Pre** (dry / analyzer-only) or **Post** (EQ stays on and meters show the processed sound)

**Eco mode** turns the visual analyzers off to save CPU. Dynamic EQ, spectral processing, and SideCheck keep working.

---

## Everyday workflow

- Collapse to a compact graph view, or keep the full faceplate
- **A / B / C / D** slots to compare full settings (bypass isn’t part of the snapshot)
- Undo and redo
- Save and browse EQ presets
- Themes for the UI, plus a dice control to randomize colours and metering ramps
- Custom colour ramps for FFT bars, spectrogram, and spectrum fill (sample from a path or edit by hand)
- **Auto Gain** keeps loudness roughly matched to the pre-EQ level while you sculpt
- **Bypass** stays latency-aligned with the host so you don’t get a timing jump

---

## Specs (short)

- **Format:** VST3
- **I/O:** Stereo in/out, optional stereo sidechain
- **Latency:** Minimum-phase EQ adds none; linear phase adds about 512 samples; oversampling can add more; bypass matches reported delay
- **Built with:** JUCE
- **User data:** `Documents/Decksounds/ParametricEq/`

---

## Why people would care

1. **SideCheck™** — Mid/Side control with a clear job: keep Side from overpowering Mid
2. **Two dynamics tools** — broad Dynamic EQ and finer spectral control on the same bands
3. **Saturation in the EQ** — harmonics without a second plugin
4. **Modulation that moves the curve** — not just for show
5. **Real metering** — including a Scope view that can act as a pure analyzer or show the wet signal

---

## Quick list for a store page

- 8-band parametric EQ; full shape menu (tilt, all-pass, band shelf, Baxandall, brickwall, vintage…); slopes up to 96 dB/oct
- Minimum and linear phase; proportional Q; per-band stereo / mid / side / L / R
- Dynamic EQ, spectral dynamics, and SideCheck™
- Band and spectral saturation with up to 8× oversampling
- Three LFOs, custom shape, and a 20-slot modulation matrix
- Spectrum, FFT bars, oscilloscope, goniometer, spectrogram, and level meters
- Scope mode with Pre (analyze) or Post (process + meter)
- A–D compare, undo, themes, colour ramps, auto gain, eco mode

---

## Keep these honest

- Current project build is **VST3** (don’t advertise AU/AAX unless those targets are enabled)
- UI still says **beta**
- Linear phase matches the EQ shape in stereo; the per-band mid/side/left/right modes are for minimum-phase operation
