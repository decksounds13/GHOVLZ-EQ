# Sync state (bots update this)

| Field | Value |
|-------|--------|
| Topology | **Option B** — same GitHub repo, isolated branch |
| EQ remote | `https://github.com/decksounds13/GHOVLZ-EQ.git` |
| EQ branch watched | `main` |
| Analyzer remote | same repo (`origin`) |
| Analyzer default branch | `analyzer/main` |
| Analyzer sync branch pattern | `sync/eq-<shortsha>` → PR into `analyzer/main` |
| Last EQ SHA Port claimed to absorb | `df4e49c7011c507c8098a66a4e9c6c9e5ffd6f95` |
| Last EQ SHA Port observed (any poll) | `e4c1a4288c6ff0e4b3c3114c3351f8285826c8e8` |
| Last EQ change time (UTC) | `2026-08-12T19:27:53Z` |
| Ports completed today (date + count) | `2026-08-12: 0` |
| First-change-of-day port done | no |
| Dirty after last port | yes |
| **Mode** | **`auto`** (poll → port → Review → merge on PASS) |
| Timezone for “day” | `America/Chicago` |
| Human notify | **Always** — success **and** failure (app/OS notification + message in this group) |

## Automation level (`auto`)

Unattended path (no human click required for normal Allow-list ports):

1. Poll EQ `main` (cadence below).  
2. When a port slot fires → Port applies Allow / Allow-candidate files on `sync/eq-<shortsha>`.  
3. Port opens a PR into **`analyzer/main`** (draft or ready — prefer **ready to merge** if GitHub allows bot merge).  
4. Review runs checklist.  
5. **On Review PASS** → Review (or Port if Review cannot merge) **merges** the PR into `analyzer/main`, updates this file (Last absorbed, ports today, dirty).  
6. **Ping human: success** with PR URL + short file list.  
7. **On Review FAIL / NEEDS-HUMAN** → **do not merge**; **ping human: failure** with blockers.  
8. **Never** merge to EQ **`main`**. Never force-push `analyzer/main` history rewrites.

### Still escalate (no auto-merge)

- Any Deny-class file in the PR  
- Whole-file overwrite of `MainComponent` / `EqProcessor` / gray shell without extract-only justification  
- Conflict that cannot be resolved cleanly  
- Port #2 already used today and still dirty (defer message only — still ping “deferred”)  
- Missing GitHub write permission / merge blocked by branch protection  

If branch protection requires a human review on GitHub, configure **one** of: allow the bot account to merge, or lower protection on `analyzer/main` only (keep `main` protected).

## Cadence (max 2 ports per calendar day)

| Slot | When |
|------|------|
| **Port #1** | First EQ `main` change of the local day, after **≥ 60 min** quiet (no new main commits) |
| **Port #2** | More commits after Port #1, then **≥ 60 min** quiet again |
| **Cap** | **2** auto ports/day; leftover dirt → next day + success-style ping “deferred to tomorrow” |

**Poll** every **15–20 minutes** (detect change + quiet). Polling is not a port.

**Manual override:** human says “manual port now” → ignore quiet timer once; still counts toward daily cap; still Review + auto-merge + success ping.

## Hard rules

1. **Never** push Analyzer product work to **`main`**.  
2. Sync PRs **only** into **`analyzer/main`**.  
3. Human is notified on **every** completed port outcome (pass or fail), not only failures.  
4. Prefer small, Allow-list-only commits so auto-merge stays safe.
