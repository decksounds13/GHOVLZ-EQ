# Setup: Port Bot + Review Bot (EQ → AnalyzerSuite)

This is the runbook to keep **AnalyzerSuite** current with analyzer-relevant **GHOVLZ EQ** changes, using two Grok Bots, **without** loading work into your interactive Grok Build coding session.

## Current topology (Option B — done)

| Tree | Path | Branch | Remote |
|------|------|--------|--------|
| EQ | `…/ParametricEqProject` | `main` | `decksounds13/GHOVLZ-EQ` |
| Analyzer | `…/AnalyzerSuite` | `analyzer/main` | same repo |

- **EQ product** = `main` only.
- **Analyzer product** = `analyzer/main` (and `sync/eq-*` working branches).
- AnalyzerSuite checkout is configured so a bare `git push` only updates `analyzer/main` (not `main`).
- Port draft PRs must target **`analyzer/main`**, never `main`.

Optional later upgrade: split into a separate `GHOVLZ-AnalyzerSuite` repo (Option A).

---

## Step 0 — Prerequisites

1. **Grok Bot** app with GitHub connected (token with access to `GHOVLZ-EQ`).
2. Keep **Grok Build** for human EQ coding only — do not run Analyzer sync inside that session.

---

## Step 1 — Git topology

**Done:** `analyzer/main` on `https://github.com/decksounds13/GHOVLZ-EQ` holds the AnalyzerSuite fork + bot policy files.

---

## Step 2 — Commit bot policy files on Analyzer

These files are already scaffolded in the AnalyzerSuite tree:

| File | Purpose |
|------|---------|
| `SYNC_ALLOWLIST.md` | What may be ported |
| `SYNC_STATE.md` | Last absorbed EQ SHA + mode |
| `docs/bot-prompts/PORT_BOT.md` | Port description |
| `docs/bot-prompts/REVIEW_BOT.md` | Review description |
| `docs/BOT_SYNC_SETUP.md` | This runbook |

Commit them on the Analyzer remote/branch (not onto EQ `main` by accident).

Optional: add a one-line pointer in `README.md` under Relation to EQ.

---

## Step 3 — Create the two Bots in Grok Bot

1. Open Grok Bot → create Bot **Port** (or “EQ→Analyzer Port”).
2. Paste the body of `docs/bot-prompts/PORT_BOT.md` into the Bot description / standing instructions.
3. Attach GitHub (and `@` the connector on first task). Ensure it can read EQ and write Analyzer (after Step 1).
4. Create Bot **Review** the same way with `docs/bot-prompts/REVIEW_BOT.md`.
5. Put both + you in a **group chat** (e.g. “Analyzer sync”) so handoffs are visible.
6. In group: “Port is the only writer. Review only comments / verdicts. Human merges.”

---

## Step 4 — First run (report-only)

In the group chat (or Port DM):

> Read SYNC_ALLOWLIST.md and SYNC_STATE.md from the AnalyzerSuite repo.  
> Diff EQ `main` from Last EQ SHA to HEAD.  
> Produce a port plan only (mode report-only). Do not commit.  
> @Review: sanity-check the plan against the allowlist.

Then:

> @Review: verify Port’s plan. Verdict PASS/FAIL on the *plan*, not a PR yet.

Tune allowlist if Review and Port disagree on gray paths.

---

## Step 5 — Routine (monitor without you babysitting)

When report-only looks right for a few runs, ask Port:

> Every weekday at 09:00 America/Chicago (adjust TZ), run the EQ→Analyzer delta report against SYNC_STATE.  
> Post the report in this group. Do not open PRs while mode is report-only.  
> If EQ did not move, post “no changes” and stop.

Optional event (if your account supports GitHub triggers):

> When `main` on GHOVLZ-EQ receives a push, run the same report.

Use **Test run** on the routine before leaving it enabled.  
Pause routines if you will be away a long time (Bot may auto-pause anyway).

---

## Step 6 — Enable draft-PR mode

1. Set `Mode` to `draft-pr` in `SYNC_STATE.md` and commit.
2. Tell Port:

> Mode is draft-pr. On the next non-empty Allow-list delta, open branch sync/eq-\<shortsha\>, push to Analyzer remote, open a **draft** PR, then @Review.

3. Tell Review:

> When Port links a draft sync PR, run the full checklist. Reply with Verdict PASS/FAIL. Never merge.

4. You merge only on PASS (or override consciously).  
5. After merge, update **Last EQ SHA** in `SYNC_STATE.md` (human or Port after you say “state update allowed”).

---

## Step 7 — Keep Grok Build out of the loop

| Task | Tool |
|------|------|
| EQ features / UI / DSP | Grok **Build** in `ParametricEqProject` |
| Automated port attempts | **Port** Bot |
| Check Port | **Review** Bot |
| Merge + local Standalone verify | You (VS2022) or a separate Build session on AnalyzerSuite when you choose |

Do not schedule Analyzer sync inside the EQ coding session.

---

## Step 8 — Success criteria

- Port posts reports (and later draft PRs) without you prompting every time.
- Review fails illegal ports more than once in early weeks (proves the checker works).
- EQ `main` never receives Analyzer renames or Standalone-only commits.
- Your interactive Build session does not run sync subagents.

---

## Abort / pause

- Pause Port’s routine in Bot → conversation → Routines.
- Set `Mode` back to `report-only` in `SYNC_STATE.md`.
- If a bad branch was pushed: delete `sync/eq-*` remote branch; do not reset shared EQ `main`.

---

## What we cannot do from Grok Build alone

Creating named Grok Bots, cloud computer logins, and GitHub plugin OAuth happen in the **Grok Bot app**, not in this TUI.  
This repo scaffolding + your Step 0–3 in the app is the full setup path.
