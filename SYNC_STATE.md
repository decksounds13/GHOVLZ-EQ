# Sync state (bots update this)

| Field | Value |
|-------|--------|
| Topology | **Option B** — same GitHub repo, isolated branch |
| EQ remote | `https://github.com/decksounds13/GHOVLZ-EQ.git` |
| EQ branch watched | `main` |
| Analyzer remote | same repo (`origin`) |
| Analyzer default branch | `analyzer/main` |
| Analyzer sync branch pattern | `sync/eq-<shortsha>` → PR into `analyzer/main` |
| Last EQ SHA Port claimed to absorb | `314f5c32fd05b075f9c95c9180b0e906305be125` |
| Last EQ SHA Port observed (any poll) | _(Port fills)_ |
| Last EQ change time (UTC) | _(Port fills when main tip moves)_ |
| Ports completed today (date + count) | _(Port fills, e.g. 2026-08-11: 0)_ |
| First-change-of-day port done | no |
| Dirty after last port | no |
| Mode | `report-only` (switch to `draft-pr` when ready to open PRs) |
| Timezone for “day” | `America/Chicago` (adjust if needed) |

## Cadence policy (max 2 ports per calendar day)

Port may produce at most **two** Analyzer updates per local calendar day (timezone above).  
An “update” = one report (report-only) **or** one draft PR (draft-pr mode) that advances **Last EQ SHA** absorbed.

### When EQ `main` moves

1. Record **Last EQ SHA observed** and **Last EQ change time**.
2. Set **Dirty after last port** = yes if tip ≠ Last absorbed SHA.

### Port #1 — first change of the day

- Eligible when: calendar day has **0** ports so far, and EQ `main` has advanced at least once today vs yesterday’s absorbed tip (or vs Last absorbed).
- **Do not** port on the first poll the instant a commit lands if you can wait for settle — prefer:  
  **quiet ≥ 60 minutes** since **Last EQ change time**, then run Port #1 for the full delta since Last absorbed.  
- If the day is almost over and quiet never hits 60m but there is dirty work, you may run Port #1 in the last poll window of the day (fallback).

### Port #2 — second batch (same day)

- Eligible only if: Port #1 already done today, **and** more EQ commits landed **after** Port #1 (Dirty), **and** **quiet ≥ 60 minutes** since Last EQ change time, **and** ports today &lt; 2.
- Port the delta since Last absorbed only.

### Hard caps

- **Never more than 2 ports per day.**
- If still dirty after Port #2: note “deferred to next day” in the report; do not open a third.
- Poll often enough to notice quiet windows (e.g. every **15–20 minutes** during work hours, or hourly 24/7). Polling is free; **updates** are capped.

### Empty polls

- If no new EQ commits and not eligible for a port: reply **no changes** (or silent if routine prefers) and stop.
- Do not burn a daily port slot on empty work.

## Hard rules

1. **Never push Analyzer product work to `main`.** `main` is GHOVLZ EQ only.
2. Port writes only to `sync/eq-*` and draft PRs **into `analyzer/main`**.
3. Review never merges.
4. Human merges after Review PASS (unless you later authorize auto-merge).

## Notes

- AnalyzerSuite working copy: branch `analyzer/main`.
- This tree’s `git push` is restricted to `analyzer/main` only.
