# Future notes (do not implement until asked)

## Product language (locked)
- **Image spill** — the problem: side energy exceeding the mid in a frequency region (harsh / weird / won’t sit).
- **SideCheck** — the solution: keeps Side from exceeding Mid (S≤M).
- One-liner: *SideCheck exists to solve image spill.*

## Free SideCheck (spitball — later)
- Standalone / lite funnel; eco only (no HQ).
- Cap instances (e.g. max 3) via process-wide lease.
- Upsell full GHOVLZ! EQ (HQ + spectral + pack + rest) ~$49.99.

---

## Per-band saturation — Stage 1 (SHIPPED — do not rip out)

### Status
**DSP + OptionBox UI landed (2026-07-24).** Polish / curve display / LP-path sat still open.

Models: Tape, Tube, Diode, Dual-Triode. OS via `juce::dsp::Oversampling` (Off/2x/4x/8x). Pre/Post per band.

**Chain position (Stage 1):** inside each tunable band’s min-phase EQ step (Pre/Post around that band IIR).  
Full block order today: **EQ(+per-band sat) → Spectral → SideCheck → Out**.

---

### How Wavesfactory Spectre actually works (research)

Source: [Spectre User Manual](https://www.wavesfactory.com/audio-plugins/manuals/Spectre-User-Manual.pdf) + reviews.

**It is not a normal EQ.** Boosting a band does not primarily turn that band up; it creates **new harmonics** in that region and blends them with dry.

**Signal chain (per band, parallel):**
1. Take dry input.
2. Run a **boost-only** parametric band (low shelf / peaks / high shelf) → “EQ’d” copy.
3. Take **difference** = EQ’d − dry (the “coloured area under the curve”).
4. Run that difference through a chosen **saturation algorithm**.
5. Sum all bands’ processed differences.
6. **Mix** that sum in parallel with the original dry (default mix often ~50%).

So gain = *how much difference you extract to distort*; **Q** = how wide/narrow that emphasis region is. Cuts aren’t the workflow (boost-oriented enhancer).

**De-Emphasis (on by default in Spectre):** after saturation, compensate the EQ boost so you mostly hear *harmonics*, not a loudness bump from the emphasis EQ. Key for “enhancer not EQ” feel.

**Global controls Spectre uses:**
- Color / algorithm (global originally; later versions also per-band sat)
- Mode: Subtle / Medium / Aggressive (drive character)
- Input ±12 dB extra drive
- Mix dry/wet
- Oversampling: **4× / 16×** (aliasing control when aggressive)
- Channel: stereo / L / R / M / S (per band in later versions)

**Algorithms in Spectre:**
| Model | Character (manual) |
|-------|--------------------|
| Tube | Soft clip, symmetric, versatile |
| Warm Tube | Darker tube; avoid highs |
| Solid | Transistor soft clip, asymmetric |
| Tape | Punchy, muffled; bass/kick; avoid highs |
| Class B | Crossover distortion; drums/transients |
| Diode | Soft clip like tube, more HF |
| Digital | Hard clip |
| Bit | Bitcrush (reacts more on low inputs) |
| Rectify / Half Rectify | Full/half wave rectify |
| Clean | No sat — parallel boost EQ only |

**Takeaway for GHOVLZ:** Spectre’s magic is **distort(EQ_boost − dry) → add to dry**, optionally with de-emphasis. Our Pre/Post modes can cover both Spectre-like parallel harmonics and classic “emphasis → drive band into sat” workflows.

---

### GHOVLZ! design (user spec — 2026-07-24)

#### Models (common set; expand later)
- Tape
- Tube
- Diode
- Dual-triode
- (solid / class B / warm tube as stretch goals)
- Optional: Clean (parallel boost only, Spectre-style)

#### Oversampling
- Off / 2× / 4× / 8× (or match Spectre 4×/16×) — global quality setting
- Needed when sat is bright or aggressive (aliasing)

#### OptionBox UX
- **Sat button** immediately **to the right of the filter-model dropdown**
- Left click: enable/disable sat for **this band**
- When on: glows the same orange as knob glow rings  
  - UI orange reference in codebase: ~`RGBA(180, 150, 55)` (buttonOn) / knob glow ~`RGBA(200, 120, 0)`–`RGBA(255, 200, 0)` — match active knob ring look
- **Right-click Sat** → popup list of distortion models (per-band or sticky last choice — decide at impl)
- When Sat enabled, a **Pre / Post** toggle appears:
  - Label shows **Pre**; click → **Post** (and vice versa)
  - Changes processing order for that band’s sat stage

#### Gain / Q behavior
- Raising band **gain** introduces harmonics, **shaped / restrained by Q** (narrow Q = surgical harmonics; wide Q = broad)
- Cut gain: little/no sat drive (boost-oriented), unless we later add cut-side behavior

#### Pre vs Post (two topologies)

**Pre (emphasis → drive):**  
Filter/boost the band first, then push that into the saturator (classic “EQ into distortion”). More smash, more interaction with the actual band level.

**Post (Spectre-like / parallel harmonics):**  
Form difference or band-limited wet, saturate, blend harmonics with dry (and optionally de-emphasize so loudness doesn’t jump). Closer to Spectre’s “phantom content.”

Both available via Pre/Post button when Sat is on.

#### DSP integration notes (when building)
- Eligible bands: same as gain-using types (bells + shelves); not HP/LP / notch / BP unless we decide otherwise
- Params (sketch): `*Sat`, `*SatModel` (choice), `*SatPrePost` (bool/choice), global `satOversample`
- Order vs rest of plugin: sit inside per-band EQ path; still **EQ(+sat) → Spectral → SideCheck → Out** at block level unless Pre/Post implies otherwise within the band
- Linear-phase path: sat likely **min-phase only** or disabled in LP mode (decide later)
- CPU: run sat only when band Sat on and gain > ~0
- Curve/analyzer: optional later (show sat activity / harmonic contribution)

#### References
- Wavesfactory Spectre manual PDF
- Knob glow orange: `RotaryImageKnobLookAndFeel1/2`, OptionBox `buttonOnColourId` `(180, 150, 55)`

---

## Post-Spectral saturation — Stage 2

### Status
**Landed (2026-07-24).** Chain: EQ(+Stage1) → Spectral → **SS** → SideCheck → Out.

- Global `spectralSat` / model / drive / oversample
- Engages only when SS on **and** at least one S band armed
- OptionBox: **SS** beside Expand when S is on; Drive slider; right-click model/OS
- Stage 1 per-band Sat unchanged

### Intent
A **second** sat stage **after Spectral**, so Expand (and spectral GR) can **drive the distortion**.

### One-liner
*Per-band Sat paints harmonics. SS (post-Spectral) lets Expand drive them.*

