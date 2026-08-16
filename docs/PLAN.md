# GHOVLZ! DYN — adaptation plan (r3)

**Approved in full — 2026-08-15.** Layout, graph gestures, chrome, DSP, OTT, linear-phase splits, and lookahead delay are all in. This is the product spec, not a sketch.

Working title: **GHOVLZ! DYN**. Ship under that name unless we rename later.

New product, not EQ `main`, not Analyzer Suite.
Tree: `Desktop/DecksoundsParametricEq/GhovlzDyn` (parent rename to `Decksounds Plugin Projects` is housekeeping, not a product hold).
Own VST3 UID (`Zgld`), own `Documents/Decksounds/GhovlzDyn/`.

Do not revive the r2 mid-row oscilloscope, Osc / Gon / Spec chrome, or Scope-mode quad view unless asked.

---

## Product

A compressor that starts broadband. Double-click the spectrum to add bands (max 6).
Threshold and makeup are solid paths on the analyzer. One large square transfer
plot (knee + ratio + live ball). Faceplate has Focus and All. Per-band Auto and
global Auto. Same Look, dice, themes, and chrome language as GHOVLZ! EQ — without
the EQ meters that do not belong on a compressor.

---

## UI (r3) — supersedes r2 mid-row

r2 put a 220 px transfer + oscilloscope strip under the analyser. That is out.

### Hide (approved)

| Hide | Why |
|---|---|
| Mid-row `dynScope` / `OscilloscopeComponent` | Transfer + knobs own that space |
| Spectrogram | User: get rid of it for now |
| Osc / Gon / Spec buttons | EQ meter chrome |
| Scope-mode button | Door back into spectrogram / oscilloscope |
| Piano / keyboard | EQ graph leftover |
| Mini-meter buttons | Hide chrome; keep the logic |

Implementation hook: `MainComponent::setDynShellMode(true)` hides Osc / Gon / Spec
and the compact overlays. `dynScope` stays in the tree but stays disabled and
hidden. Do not `addAndMakeVisible` it again.

### Focus

Lower half of the window (same height as the analyser for now):

- **Left** — large **square** transfer. Centered with the knobs as one instrument.
  Not a tiny Reaktor tile. Ozone / FabFilter polish.
- **Right** — selected-band compressor knobs + vertical GR, **packed**, not
  stretched across the window. Default knob filmstrip colour (no band tint).
- Knobs 1.5× larger when a single band is selected (broadband / Focus).
- Mix glow / fill follows the real 0–1 Mix position, not Q mapping.

### All

- Hide the big transfer (and the oscilloscope — already gone).
- Band cards fill that same lower half.
- Each card gets its own **mini knee**, knobs, and GR rail.
- Scaling closer to `Show and Tell/module.png`: larger knobs, taller GR.
- Tasteful, packed, not a functional grid dump.

### Stack (top → bottom)

1. **Top trim** — expand, Bypass, A–D, UI, dice, preset bar, save, undo/redo,
   settings hamburger. These live in the trim, not on the graph.
2. **Spectrum** — the only large plot. IN / OUT meters taller, up to the trim,
   5 px pad.
3. **Focus row** — large transfer | packed knobs + GR. Same height as the graph.
   In All, this row is the card grid instead.
4. **Bottom trim** — `?`, Minimum / Linear Phase, wordmark, Lookahead, global A, Out.

---

## Knobs

Same `DarkKnob4` filmstrip + `KnobTheme` glow arcs as EQ (`RotaryImageKnob1` family).
**Default filmstrip colour by default — no tinting or adjustments.**
No CSS discs. No generic SVG knobs in the product.

## Linear phase

Same combo as EQ, same place: left of the bottom trim, beside `?`.

| Mode | Crossovers | Latency |
|---|---|---|
| **Minimum Phase** | IIR Linkwitz-Riley (default) | ~lookahead only |
| **Linear Phase** | FIR / FFT crossovers, flat sum, constant group delay | reports host delay (same order as EQ linear, plus lookahead) |

Linear phase is for the **splits**, not a different compressor. Pre-ring on transients is the tradeoff — same honesty as EQ.

## Lookahead

Lives in the **bottom trim**, not the faceplate.

- Small image knob + `Look` / `2.0 ms` readout
- Range ~0–20 ms (skewed toward 0–5)
- Adds to reported latency with linear phase
- Default 0 on Default preset; OTT can stay at 0 (Ableton MD has no lookahead)

## Bottom trim (match EQ)

30 px bronze strip, same paint as EQ expanded trim:

- Left: `?` help, **Minimum Phase / Linear Phase** combo
- Center: `GHOVLZ! DYN` wordmark + version
- Right: **Look** knob, global **A** (auto-gain), **Out** label + output image knob

No second chrome Auto button in the top bar — EQ keeps `A` next to Out.

## GR meters

That bottom glow bar was GR. Wrong orientation.

Hardware and most digital comps (1176 / LA-2A needle, Pro-C, SSL, Neutron):
**0 dB GR is at rest at the top. The meter grows downward as reduction increases.**
Level meters grow up from silence. GR meters grow down from zero.

Per band:

- Vertical strip to the **right** of that band’s knobs
- Full-height colour ramp (green / gold / orange / red), same family as EQ `VerticalGradientMeter`
- A mask covers the unused bottom so the ramp *reveals* from the top
- 1 px peak-hold tick
- Focus mode: one tall meter + `-6.2 dB` readout under it
- All mode: slim meter, no extra label (value is the bar)

Scale 0 to about −24 dB. Not a level meter.

## Labels

Full words only on chrome: Threshold, Ratio, Attack, Release, Knee, Makeup, Mix,
Lookahead, Output, Compress, Stereo. Size bounds to the measured string.
No Atk / Rel / Thr / Make. No ellipsis.

## OTT factory preset

OTT is **not** just heavy downward compression. Ableton’s Multiband Dynamics preset (and Xfer’s clone) does **downward above** and **upward below** on three bands. Without upward, it will not sound like OTT.

**Approved DSP:** every band has a downward path and an upward path (Dual). OTT is a factory preset on that engine, not a one-off topology.

Published Ableton / Xfer figures (verify on a Live install before we ship):

| | Low | Mid | High |
|---|---|---|---|
| Crossover | — / 220 Hz (Ableton) or 88.3 Hz (Xfer) | 220 Hz–3.4 kHz or 88.3 Hz–2.5 kHz | 3.4 kHz or 2.5 kHz / — |
| Down ratio (Above) | 66.7 : 1 | 66.7 : 1 | inf : 1 |
| Up ratio (Below) | 4 : 1 | 4 : 1 | 4 : 1 |
| Attack | ~30–50 ms | medium | ~1–5 ms |
| Release | slower | medium | fast |
| Knee | soft | soft | soft |

**Ableton factory splits** are the `OTT` preset. Ship a second preset `OTT Xfer` at 88.3 Hz / 2.5 kHz.

Also: 3 bands, Mix/Depth ~100%, Look 0, global Auto off, band Auto off (OTT’s makeup is baked into the up/down dance).

UI when OTT / Dual is on: extra Up Thr / Up Ratio either appear in Focus or live in OptionBox so the default Comp row stays seven knobs.

---

## Spectrum (the only large plot)

- Log frequency, analyzer dB scale on the left (0 to −60).
- Band tints + crossover handles. Band colours are randomizable (dice).
- **Thr path**: solid horizontal bar across that band, Y = threshold on the
  analyzer dB scale. Drag it to set threshold without a knob.
- **Make path**: second solid bar like threshold, **different randomizable colour**
  (`ThemeColorRegistry` graph makeup bar). Not a dashed rail.
- Gold polyline = live result (GR + makeup) vs frequency.
- Downward wash = live GR in that band.
- Per-band **level wash**: 40% transparent, full width of the band, so you can
  park the Thr bar on the actual level.
- IN / OUT meters on the sides, taller, 5 px pad under the top trim.

### Why this and not two graphs, and not an oscilloscope

Pro-MB keeps dynamics on the analyzer (threshold / range lines, result curve)
and puts knobs under the selected band. Knee is a property of the I/O law — it
belongs on the square transfer plot.

The transfer **is** square: both axes are the same dB range. A wide rectangle
lies about the law.

r2 borrowed Neutron’s transfer + tempo-synced oscilloscope pairing. That mid-row
is rejected: the compressor UI is analyser + transfer + knobs. Oscilloscope /
goniometer / spectrogram stay in the EQ / Analyzer products.

---

## Graph-native controls

| Control | Where | Gesture |
|---|---|---|
| Threshold | Solid path, Y = analyzer dB | Drag vertically. Hover / drag: up-down arrows, band colour |
| Makeup | Solid path, different randomizable colour | Drag vertically. Same arrow language |
| Crossover | Vertical split | Drag horizontally. Hover: left-right arrows, colours match the two bands |
| Add band | Double-click spectrum | Split at that Hz. Hover: ghost band + plus under the cursor |
| Select band | Click tint | Focus + transfer follow |
| Ratio | Wheel on band, or transfer slope | — |
| Remove | Right-click band | Delete / merge |

Threshold is on the **analyzer** scale, not a separate “dynamics dB” scale.
If the spectrum shows −18 dB at that frequency, the Thr path sits on that line.

Makeup is an offset, not a signal level. It is a **second solid bar** on the
same plot. No dedicated gain lane.

Split lines: a bit thicker than r2, with drop shadows.

Crossover slopes: **6 / 12 / 24 dB/oct**. Default **24 dB/oct Linkwitz-Riley**
(Pro-MB / Neutron / C6 convention). 6 = 1st-order IIR, 12 = Butterworth 2nd-order.

---

## Auto-gain

| Job | What it does | UI |
|---|---|---|
| **Band Auto** | Compensates that band’s average GR so compressing does not bury the band. Manual Make is trim on top of auto. | `Auto` on the band header / All-mode column |
| **Global Auto** | Same job as EQ’s `A`: match full-plugin output loudness to input. After the sum. | `Auto` in chrome |

They stack: band Auto keeps the split mix from tilting; global Auto keeps the
plugin from getting quieter or louder overall.

When band Auto is on, the Make path still draws (readout = auto + trim).
Dragging it while Auto is on sets trim (same rule as EQ’s auto-gain vs output).

---

## Interaction (approved)

| Action | Result |
|---|---|
| Fresh instance | 1 band, 20 Hz–20 kHz |
| Double-click spectrum | Insert a crossover (max 6 bands) |
| Hover empty spectrum | Ghost band + plus |
| Drag split | Move crossover (ordered, min gap). Hover arrows L/R, band colours |
| Drag Thr path | Set that band’s threshold. Hover arrows U/D |
| Drag Make path | Set that band’s makeup (or trim if Auto). Hover arrows U/D |
| Click band | Select — transfer + Focus knobs follow |
| Wheel on band | Ratio |
| Right-click | Solo, mute, listen, remove, copy, Auto |
| Focus / All | Faceplate mode (persisted in UI prefs) |

---

## Transfer polish

- Square plot, quiet grid, no heavy / red outline, no opaque rounded card
  (that caused white corner flicker).
- Live ball that travels the curve and reacts to the signal.
- Hard-knee ghost.
- Same polish language as the EQ graph (fills, glow, shadows) — not a 1987
  schematic.

---

## Parameters (per band, max 6)

Threshold, Ratio (1:1–inf), Attack, Release, Knee, Makeup, Mix,
On / Solo / Mute, Peak / RMS, Stereo / M / S / L / R,
**Auto** (bool), **slope** on each split (6 / 12 / 24).

Global: input, output, **Auto**, bypass, optional lookahead.

---

## What we copy vs drop

**Keep** — MainComponent chrome language, Menu / Appearance / Theme / dice / ramps,
LAFs, shadows, UI prefs, undo, preset-store shape, brand wordmark. Scope *module
code* may stay in the tree; it is not first-class UI here.

**Strip from the product UI** — Osc / Gon / Spec / Scope mode / piano / EQ
mini-meter buttons. Filter types / slopes / linear-phase EQ, Dynamic EQ, spectral
lattice, SideCheck, Match, structural split, saturation, EQ curve handles,
Freq/Gain/Q knobs.

**Retarget** — LFO matrix → threshold / ratio / makeup.
OptionBox → mode, detector, channel, sidechain, slope, lookahead.

---

## Copy steps (tree exists)

1. Duplicate `ParametricEqProject` → `GhovlzDyn` — **done**.
2. New `.jucer` / plugin code `Zgld` — **done**.
3. `processBlock` = dyn engine + analyser — **in**.
4. Band-split spectrum + Thr / Make paths — **in progress**.
5. Large square transfer + Focus / All faceplate — **in progress** (r3 layout).
6. Hide oscilloscope / Osc-Gon-Spec / Scope — **started** (`setDynShellMode`).
7. Trim: phase combo, lookahead, A, Out — **in**. Lookahead **delay** still to implement (approved).
8. Factory preset `OTT` (3-band up+down) + `OTT Xfer` — **approved, not built**.
9. Upward / Dual path per band — **approved, not built**.
10. Linear-phase crossovers — **approved, not built**.
11. Do **not** put this on EQ `main`. Analyzer bots stay on the EQ product.
12. Do **not** push GhovlzDyn’s copied `.git` at `decksounds13/GHOVLZ-EQ.git`.

---

## Approved but not built

These are in the product. They are unfinished work, not open questions.

- Finish r3 polish: top-trim chrome move, All-mode module scale (`Show and Tell/module.png`), hover arrows, ghost-add, thicker shadowed splits, 40% band level wash, traveling transfer ball
- Lookahead delay (knob exists)
- Linear-phase FIR / FFT crossovers
- Per-band upward path (Dual)
- Factory presets `OTT` (Ableton splits) and `OTT Xfer`
- All mode: **hide unused columns** (no empty “click to add” cards)

Housekeeping only: rename parent folder to `Decksounds Plugin Projects` when nothing has the tree locked.

UI concept (r2 mock, historical): `docs/ui-concept.html`
All-mode scale reference: `Show and Tell/module.png`
