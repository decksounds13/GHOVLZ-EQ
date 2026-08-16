# GHOVLZ! DYN — first cloud pass

Open `Builds/VisualStudio2022/GhovlzDyn.sln` and build **VST3 | x64 | Debug or Release**.

The host will list **Decksounds - GHOVLZ! DYN** (plugin code `Zgld`, not the EQ).

User data: `Documents/Decksounds/GhovlzDyn/`

Authority: `docs/PLAN.md` **r3, approved in full (2026-08-15)**. Layout, Dual/OTT, linear-phase splits, and lookahead delay are all in. No mid-row oscilloscope.

## What is in this pass

- EQ chrome language: Settings, Look, dice, A–D, presets, Appearance
- `setDynShellMode`: Osc / Gon / Spec / Scope chrome hidden
- `dynScope` constructed but disabled and hidden
- Focus: large square transfer left, packed knobs + GR right; graph and that row same height
- All: cards fill the lower half; mini knee per band
- Full-word labels: Threshold, Ratio, Attack, Release, Knee, Makeup, Mix, Lookahead, Output
- Wide vertical GR meters (0 at top)
- Double-click add band (max 6), drag splits, Thr / Make bars
- Broadband compressor DSP (Linkwitz-Riley splits when you add bands)
- Linear Phase combo still in the trim (crossovers are min-phase this pass)
- Lookahead knob is wired as a parameter; delay not implemented yet

## Approved, not built

- Finish r3 polish (top-trim chrome move, All-mode module scale, hover arrows, ghost-add, band level wash)
- Lookahead delay
- OTT + OTT Xfer factory presets
- Upward / Dual path
- Linear-phase crossovers

Do not commit until you have played it and said it is close.
