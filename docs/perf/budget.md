# Latency budget

**Owner:** Performance Engineer (agent 12)
**Status:** Phase 1 initial publication. Targets are not enforced in Phase 1; the bench harness is a skeleton, not a regression gate. Enforcement schedule is documented under "How this budget is enforced" below.
**Source spec:** `/home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md` (referenced as the design spec throughout this document).

This document is the single source of truth for Meridian's latency targets. Every number below is traced to a specific section of the design spec. Any number that cannot be traced is marked `[citation needed]` and will be resolved by the Citation and Fact Auditor before Phase 1 closes.

## Targets

### Matching event (engine hot path, single-threaded)

| Percentile | Target | Source | Measured in |
|---|---|---|---|
| p50  | <= 500 ns | Design spec section 2.3, "Latency target" line | Phase 5 (`meridian-bench`) |
| p99  | <= 2 us   | Design spec section 2.3, "Latency target" line | Phase 5 (`meridian-bench`) |
| p99.9| <= 5 us   | Design spec section 2.3, "Latency target" line | Phase 5 (`meridian-bench`) |

A "matching event" is one trip through `MatchingEngine::apply` for a single `EngineEvent` (NewOrder or Cancel), including any matching, level updates, intrusive list mutations, and the seqlock writer protocol on `TopOfBookSnapshot`. Defined by design spec section 5.1 (engine and snapshot anatomy) and section 6 (data flow steps 2 and 3).

The matching event budget is the headline latency claim that backs the resume bullet, so it is the most heavily defended number in this document. Phase 5 is the first phase where these numbers are measured against a baseline; Phase 1 ships the bench skeleton only.

### Top-of-book snapshot read (consumer side, e.g., the sampler thread)

| Percentile | Target | Source | Measured in |
|---|---|---|---|
| p50  | <= 50 ns | Inferred from design spec section 5.1 (`TopOfBookSnapshot` is "seqlock-protected" with a "writer (matching thread) bumps seq before and after each update; readers retry on torn read"). The seqlock reader in the steady, untorn case is two atomic loads with `acquire` ordering plus a small struct copy, which on modern x86_64 is in the tens-of-nanoseconds range. The 50 ns figure is set as the budget the implementation is held to, not a number quoted from the spec. `[citation needed]` for an external benchmark anchoring the 50 ns figure to a published seqlock measurement. | Phase 4 (after seqlock lands and TSAN tests are wired) |

The top-of-book read budget exists so the Phase 4 sampler can hit its 30 Hz cadence with substantial headroom while reading every active symbol. With the demo's five-symbol set (AAPL, SPY, NVDA, TSLA, GOOG, design spec section 13), five reads per tick at 50 ns each costs 250 ns per tick on the read path, which is negligible relative to the 33.3 ms tick interval at 30 Hz.

### Sampler tick build (producer side, runs at 30 Hz on its own core)

| Bound | Target | Source | Measured in |
|---|---|---|---|
| Tick build wall time | < 1 ms at 30 Hz | Inferred from design spec section 5.4 (the sampler "starts on core 6 (30 Hz)", which fixes the cadence at one tick every 33.3 ms) and section 6 step 4 (the sampler "wakes at 30 Hz, reads all snapshots (seqlock reader protocol), builds a JSON delta"). The 1 ms bound is set so a single tick consumes under 3% of its 33.3 ms slot, leaving headroom for jitter and for the uWebSockets broadcast hop on core 5 (section 6 step 5). `[citation needed]` to confirm whether the design spec implies a tighter bound. | Phase 4 (after sampler lands) |

A "sampler tick build" is the wall time from the sampler thread waking up at the 30 Hz tick boundary through finishing the JSON delta payload, exclusive of the WebSocket broadcast (which runs on core 5 per design spec section 5.4). The hand-off from the sampler to uWebSockets is bounded separately by the WebSocket broadcast budget, which is not set in Phase 1.

## How this budget is enforced

This budget exists in three states across the project's phase plan (design spec section 9):

1. **Phase 1 (current): document only, no enforcement.** This file ships, plus the `meridian-bench` skeleton (per the Phase 1 plan, Task 4). The bench harness compiles and runs end to end against `libmeridian.a`, but its throughput and latency numbers are recorded as informational baselines, not as targets. No CI job fails on regression in Phase 1. The Phase 1 baseline subsection at the bottom of this document is filled in by the bench scaffold dispatch that runs after `libmeridian.a` is linkable.

2. **Phase 4: top-of-book and sampler enforcement via TSAN.** The seqlock writer/reader protocol lands in Phase 4 (design spec section 9, Phase 4 deliverable). The Concurrency Reviewer's TSAN test suite enforces the seqlock correctness guarantees (no torn reads observed by the reader, writer never blocks). The sampler tick wall time is profiled in Phase 4 and signed off against the < 1 ms target. The 50 ns top-of-book read budget is profiled in the same pass.

3. **Phase 5: matching event enforcement via `bench/baseline.json` regression workflow.** Per design spec section 8.4, `meridian-bench` runs in CI on every PR and compares against `bench/baseline.json` checked into the repo. CI fails the build if throughput drops more than 5% relative to baseline, or if any of p50, p99, p99.9 latency percentiles increase more than 10% relative to baseline. The baseline is updated only by an explicit "regenerate baseline" workflow on `main`, never silently. Phase 5 is also where the first headline benchmark report at `docs/perf/benchmark-report.md` is generated, with histograms, methodology, and reproducibility instructions.

The pacing rule: a number in this document does not become a CI-enforced target until the phase named in the "Measured in" column closes. Until then, it is a publicly stated design intent that the Performance Engineer is on the hook to defend when its phase opens.

## Hardware reference

The headline target of >= 6M events per second single-threaded is for a modern x86_64 desktop, per design spec section 2.3 ("meridian-bench reports >=6M events per second single-threaded on a modern x86_64 desktop"). The exact reference machine specs (CPU model, core count, base and boost clock, cache sizes, RAM speed, kernel version, scheduler governor) land in `/home/mustafa/src/Meridian/docs/perf/benchmark-report.md` when Phase 5 closes, per the Performance Engineer's Phase 5 task list (agent file `agents/12-performance-engineer.md`).

The Fly.io free-tier shared-CPU machine where `meridian-server` runs (design spec section 3, "Back-end host" row, and section 13 "Backend host") is **not** the reference for the headline number. The shared-CPU machine has substantially lower steady-state throughput than a modern desktop, and treating its numbers as the headline would mislead. The Fly machine is profiled separately in Phase 10 ("Hosting and CI/CD" in the agent file's Phase 10 task list), and the resulting numbers are reported as a "live demo throughput" data point in `docs/perf/findings.md`, distinct from the headline benchmark in `docs/perf/benchmark-report.md`.

When the project ships, the README's headline number cites the desktop benchmark; the demo page's footer (per design spec section 5.5, "Footer with build info") may show the live Fly throughput separately so a recruiter clicking the demo is not surprised by the gap.

## Phase 1 baseline (informational, not enforced)

This subsection is populated by the follow-up dispatch that scaffolds `apps/bench/main.cpp` after `libmeridian.a` is linkable (Phase 1 plan, Task 4 Step 2). The numbers recorded here are explicitly informational and are not treated as targets; the Citation and Fact Auditor is expected to verify the labeling of this section so the numbers are not later mistaken for the headline.

| Metric | Phase 1 baseline | Notes |
|---|---|---|
| Throughput (events/sec) | _to be filled by bench scaffold dispatch_ | Synthetic limit orders, deterministic seed, single symbol, single thread, no PGO, no LTO. Not the headline benchmark. |
| Matching event p50  | _to be filled_ | |
| Matching event p99  | _to be filled_ | |
| Matching event p99.9| _to be filled_ | |
| Heap allocations during hot path | _to be filled_ | Must be 0 (design spec section 5.1 OrderPool requirement and section 5.2 bench output). |

## Out-of-scope budgets (deferred to later phases)

These budgets are not set in Phase 1 because the components do not exist yet. Listed here so the Performance Engineer's later phase work has a clear inventory of what to publish next:

* **WebSocket broadcast hop** (sampler thread to uWebSockets event loop on core 5): published when Phase 8 wires the WebSocket server (design spec section 9, Phase 8 deliverable).
* **End-to-end tick latency** (matching event applied to delta visible to a connected WebSocket client): published when Phase 8 wires the WebSocket server, validated against the 30 Hz cadence (section 5.4) end to end.
* **Frontend re-render budget**: per the Performance Engineer's Phase 9 task list (agent file), the depth chart re-renders in under 16 ms (60 fps headroom), trade tape append does not retain old DOM nodes, total bundle size stays under 200 KB gzipped. These are published in Phase 9 alongside the React frontend and are out of scope for `docs/perf/budget.md` until then; they may move into this file when Phase 9 ships.

## Open questions for the Citation and Fact Auditor

The following items are flagged for the Phase 1 audit pass:

1. The 50 ns top-of-book read p50 figure is set by the Performance Engineer as a defensible budget given x86_64 seqlock reader cost, but the design spec does not name a specific number. The auditor is asked to confirm whether the figure should remain in this document as a Performance Engineer-set budget or be deferred to Phase 4 when it can be measured. `[citation needed]` for an external published seqlock reader benchmark, if the auditor wants one.
2. The < 1 ms sampler tick build figure is similarly inferred from the 30 Hz cadence rather than quoted from the spec. The auditor is asked to confirm the inference is sound (a tick that consumes under 3% of its 33.3 ms slot is a reasonable budget) or to push back if a tighter or looser bound is more appropriate. `[citation needed]` if the spec is updated to quote a number directly.
3. The matching event p50/p99/p99.9 figures are quoted directly from design spec section 2.3 and require no resolution.

## Revision log

| Date | Author | Change |
|---|---|---|
| 2026-05-09 | Performance Engineer (Phase 1, Task 4 Step 1) | Initial publication. Matching event, top-of-book, sampler tick budgets set. Phase 1 baseline subsection placeholder added; populated by bench scaffold dispatch. |
