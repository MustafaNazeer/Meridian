# Meridian project status

Single source of truth for which phase is next. Read this file when the user says "work on the next phase" or any equivalent. Update this file when a phase changes state. The Project Manager session owns it.

**Last updated**: 2026-05-09 (Phase 2 opened. User confirmed at check-in: 32 percent usage, 2.5 hours until reset; Phase 2 alone fits the window, bundling Phase 3 would overspill. PM session has cross-repo cleanup on `gitignore-private-files` branches merged across 6 repos before resuming Phase 2.).

## Next phase

**Phase 2: Multi-instrument and cancel-by-id (cross-symbol layer).** Status: `in progress`. Owner: Project Manager session as Engine Developer + Market Microstructure Engineer, dispatching Reference Implementation Engineer (18), QA Engineer (10), Risk and Financial Correctness Reviewer (16), and Code Reviewer (11). Window cost: ~50 percent alone (Phase 3 bundle deferred to next window). See `docs/plan.md` Phase 2 section for the detailed deliverable list and `docs/superpowers/plans/2026-05-09-phase-2-multi-instrument-and-cancel.md` for the bite-sized step plan.

## Phase status table

Status values: `not started`, `in progress`, `completed`, `bundled with phase N` (when this phase shipped together with another phase in the same window), `paused` (mid phase, see Resume notes).

| # | Phase | Status | Completed on | Window cost | Notes |
|---|---|---|---|---|---|
| 0 | Foundations | completed | 2026-05-09 | ~95% | scaffolds, plan, threat model, design tokens, ADRs land here |
| 1 | Core data structures and single-symbol matching | completed | 2026-05-09 | ~95% | Order, Level, Book, OrderPool, limit + market + IOC + cancel matching, FIFO at price levels, Python reference, integration corpus, audit cases, citation audit clean |
| 2 | Multi-instrument and cancel-by-id | in progress | | ~50% | BookRegistry symbol dispatch, O(1) cancel via order ID map (per-Book cancel already shipped in Phase 1) |
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

### Phase 1 closeout (2026-05-09)

Phase 1 closed cleanly. PR #6 (`phase 1: core data structures and single-symbol matching`) squash-merged to `main` at `bc5e9ee`. CI green on both initial run and the post-review-fix run: engine (clang) ~40 s, engine (gcc) ~37 to 43 s, frontend ~12 to 14 s. 18 logical commits squashed into one merge commit.

Quality gates all green at close:

* **Quant Domain Validator (01)**: `docs/risk/matching-semantics.md` published, 26 worked examples (7 limit, 6 market, 7 IOC, 6 cancel) plus the 5 Phase 1 invariant checklist.
* **Reference Implementation Engineer (18)**: `tests/reference/matching_reference.py` (461 lines, stdlib only), `test_reference.py` (29 tests passing), `run_reference.py` (JSON Lines CLI driver), README.
* **Engine Developer + Market Microstructure Engineer (PM session)**: `include/meridian/{types,order_pool,level,book,execution_report,matching}.hpp` plus `src/{order_pool,level,book,matching}.cpp` plus `src/CMakeLists.txt`. Static_assert size of Order is 64 bytes or less (actual 48). HotPathGuard armed inside sweep only; debug allocator override aborts on heap allocation while armed.
* **QA Engineer (10)**: integration corpus expanded to 60 scenarios across six gtest groups, line coverage on `libmeridian.a` is 97 percent (over the 85 percent target), `docs/qa/regression-checklist.md` filed, plus 4 new `Book::erase_empty_level` tests and 2 new PostOnly/FOK reject tests.
* **Performance Engineer (12)**: `docs/perf/budget.md` published with matching event budget cited from design spec section 2.3, `apps/bench/main.cpp` working harness, HDRHistogram-c vendored at v0.11.8. Phase 1 baseline informational: 4.9 M evt/s throughput, p50 153 ns, p99 442 ns, p99.9 1224 ns. The 6 M evt/s headline target is enforced in Phase 5.
* **Risk and Financial Correctness Reviewer (16)**: `docs/risk/audit-cases.md` with 10 worked examples cross-validated; all PASS; Phase 1 matching correctness signed off.
* **Citation and Fact Auditor (17)**: `docs/audits/citation-audit-phase-1-2026-05-09.md` with 86 verified, 5 unverified (PE-set inferences and one Harris sub-section number deferred), 0 wrong (3 found, all resolved in PR before merge: F1 Harris chapter title, F2 O'Hara citation drop, F3 HDRHistogram release date).
* **Code Reviewer (11)**: PR review approved. 5 non-blocking nits left as comments; 4 fixed in commit `74cd4f6` before merge (portability includes, stale comment, tempdir cleanup, bench rerun); the fifth (defensive nullptr check on pool exhaustion in apply_limit) deferred to a Phase 2 hardening pass.

Outstanding deferred work (not Phase 1 blockers):

* The Phase 2 `BookRegistry` plus cross-symbol `OrderIndex` layer.
* Property-based tests with rapidcheck (Phase 3).
* The defensive nullptr check on `OrderPool::acquire` exhaustion (filed for Phase 2).
* External seqlock-reader benchmark anchoring the 50 ns top-of-book p50 figure (filed for Phase 4 close).
* Resolution of the Harris Chapter 6 sub-section number for the price-time priority citation (deferred follow-up; current chapter-level citation is verified).

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
