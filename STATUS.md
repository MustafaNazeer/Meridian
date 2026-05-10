# Meridian project status

Single source of truth for which phase is next. Read this file when the user says "work on the next phase" or any equivalent. Update this file when a phase changes state. The Project Manager session owns it.

**Last updated**: 2026-05-09 (Phase 0 fully closed: PR #3 squash-merged to `main` at `3c744ed`. Awaiting user check-in answer on whether to open Phase 1 in this window, pause, or stop.).

## Next phase

**Phase 1: Core data structures and single-symbol matching.** Status: `not started`. Owner: Project Manager session, dispatching Quant Domain Validator (01), Reference Implementation Engineer (18), Engine Developer (03), Market Microstructure Engineer (02), QA Engineer (10), Performance Engineer (12), Risk and Financial Correctness Reviewer (16), and Code Reviewer (11). Window cost: ~95 percent. See `docs/plan.md` Phase 1 section for the detailed deliverable list.

## Phase status table

Status values: `not started`, `in progress`, `completed`, `bundled with phase N` (when this phase shipped together with another phase in the same window), `paused` (mid phase, see Resume notes).

| # | Phase | Status | Completed on | Window cost | Notes |
|---|---|---|---|---|---|
| 0 | Foundations | completed | 2026-05-09 | ~95% | scaffolds, plan, threat model, design tokens, ADRs land here |
| 1 | Core data structures and single-symbol matching | not started | | ~95% | Order, Level, Book, OrderPool, limit + market + IOC matching, FIFO at price levels |
| 2 | Multi-instrument and cancel-by-id | not started | | ~50% alone or bundled with Phase 3 | BookRegistry symbol dispatch, O(1) cancel via order ID map |
| 3 | Property-based tests for matching invariants | not started | | ~50% alone or bundled with Phase 2 | rapidcheck wired up, 10 invariants verified |
| 4 | Seqlock-protected top-of-book and sampler | not started | | ~85% | seqlock writer/reader protocol, 30 Hz sampler, TSAN clean |
| 5 | Benchmark hits 6M events per second | not started | | ~95% | PGO, cache layout audit, target throughput, latency report |
| 6 | NASDAQ ITCH 5.0 replay | not started | | ~95% | parser, replayer, integration test against reference Python |
| 7 | Post-only and FOK order types | not started | | ~30% alone or bundled with Phase 8 | two more order types, property tests updated |
| 8 | WebSocket server and protocol | not started | | ~60% alone or bundled with Phase 7 | uWebSockets wired, snapshot + delta protocol, meridian-server binary |
| 9 | React frontend with Twilight visual | not started | | ~95-99% | React + Vite + TS + Tailwind app, Twilight tokens, all panels |
| 10 | Hosting and CI/CD | not started | | ~75% | Cloudflare Pages, Fly.io machine (Dockerfile + `fly.toml`), hardening checklist |
| 11 | Polish, README, and benchmark report | not started | | ~80% | headline README, benchmark report PDF, final test pass |

## Resume notes

### Phase 0 closeout (2026-05-09)

Phase 0 closed cleanly. PR #3 (`phase 0: brainstorm, plan, scaffold, and audit`) squash-merged to `main` at `3c744ed`. First CI run was green: engine (clang) 42 s, engine (gcc) 34 s, frontend 16 s. Five logical commits squashed into one merge commit. Citation auditor's follow-ups F1 through F4 are resolved on `main`.

**Outstanding deferred work** (not Phase 0 blockers, but worth tracking):

* No Code Reviewer dispatch ran on Phase 0; PR review by a dispatched Code Reviewer agent was deferred since Phase 0 is mostly docs and scaffolding. Phase 1 onward will dispatch Code Reviewer per the PM brief.

**Branch protection on `main`**: applied 2026-05-09 via `gh api`. Required checks `engine (clang)`, `engine (gcc)`, `frontend`; 1 review required; linear history; conversation resolution; no force pushes; no deletions. `enforce_admins` is `false` so `gh pr merge --admin` continues to work for the review-bypass case per `CLAUDE.md`'s standing push protocol. Details in `docs/setup-guide.md` section 12.1.

## Design sync log

The user can change the design at any time, including after Phase 11 ships, via either flow in `CLAUDE.md` ("Design change workflow"):

* **Flow A** (direct from terminal): user describes the change in plain English, the session edits React, Tailwind, and the HTML directly.
* **Flow B** (Claude Design round trip): user replaces `docs/design/canonical.html`, the session diffs and dispatches.

Both flows write one line here per change, prefixed with the flow letter.

Format: `YYYY-MM-DD [A or B]: <one line summary of what changed and which components were updated>`.

(none yet)

## How to update this file

The Project Manager session must update this file at three moments:

1. **Starting a phase**: change status from `not started` to `in progress`, set the date in the "Last updated" line, and update the **Next phase** field to name the phase being started.
2. **Pausing mid phase**: change status to `paused`, write a Resume notes entry naming the commit ref and the next concrete task.
3. **Completing a phase**: change status to `completed`, fill in the "Completed on" date, and update the **Next phase** field to the next not-started phase. If the next phase is bundled with the one just completed, mark that phase `bundled with phase N` and skip to the one after.

Never edit any other field unless explicitly asked. Never reorder rows.
