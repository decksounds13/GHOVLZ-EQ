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
| Last Review pass | plan PASS for `314f5c32` → `df4e49c` (report-only; not yet ported) |
| Mode | `report-only` → change to `draft-pr` after first successful dry runs |

## Hard rules

1. **Never push Analyzer product work to `main`.** `main` is GHOVLZ EQ only.
2. Port Bot writes only to `sync/eq-*` branches and opens draft PRs **into `analyzer/main`**.
3. Review Bot never merges.
4. Human merges `sync/eq-*` → `analyzer/main` after Review PASS.

## Notes

- AnalyzerSuite working copy should stay on branch `analyzer/main` (or a `sync/eq-*` branch).
- This checkout has `remote.origin.push` restricted so a bare `git push` cannot update `main`.
- ParametricEqProject remains a separate working tree on `main` for EQ development.
