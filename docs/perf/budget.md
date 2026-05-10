# Latency budget

**Status:** initial publication alongside the single-symbol matching milestone. Targets are not enforced yet; the bench harness is a skeleton, not a regression gate. Enforcement schedule is documented under "How this budget is enforced" below.

This document is the single source of truth for Meridian's latency targets. Any number that cannot be traced to code or a stated source is marked `[citation needed]` and should be resolved on the next pass.

## Targets

### Matching event (engine hot path, single-threaded)

| Percentile | Target | Notes | Measured in |
|---|---|---|---|
| p50  | <= 500 ns | Headline latency claim. | Bench milestone (`meridian-bench`) |
| p99  | <= 2 us   | Headline latency claim. | Bench milestone (`meridian-bench`) |
| p99.9| <= 5 us   | Headline latency claim. | Bench milestone (`meridian-bench`) |

A "matching event" is one trip through `MatchingEngine::apply` for a single `EngineEvent` (NewOrder or Cancel), including any matching, level updates, intrusive list mutations, and the seqlock writer protocol on `TopOfBookSnapshot`. See `docs/architecture.md` sections 3.1 and 4.

The matching event budget is the headline latency claim that backs the resume bullet, so it is the most heavily defended number in this document. The bench milestone is the first time these numbers are measured against a baseline.

### Top-of-book snapshot read (consumer side, e.g., the sampler thread)

| Percentile | Target | Notes | Measured in |
|---|---|---|---|
| p50  | <= 50 ns | The seqlock reader in the steady, untorn case is two atomic loads with `acquire` ordering plus a small struct copy, which on modern x86_64 is in the tens-of-nanoseconds range. The 50 ns figure is set as the budget the implementation is held to. `[citation needed]` for an external benchmark anchoring the 50 ns figure to a published seqlock measurement. | After seqlock lands and TSAN tests are wired |

The top-of-book read budget exists so the sampler can hit its 30 Hz cadence with substantial headroom while reading every active symbol. With the demo's five-symbol set (AAPL, SPY, NVDA, TSLA, GOOG), five reads per tick at 50 ns each costs 250 ns per tick on the read path, which is negligible relative to the 33.3 ms tick interval at 30 Hz.

### Sampler tick build (producer side, runs at 30 Hz on its own core)

| Bound | Target | Notes | Measured in |
|---|---|---|---|
| Tick build wall time | < 1 ms at 30 Hz | The sampler runs on core 6 at 30 Hz (one tick every 33.3 ms): wakes, reads all snapshots, builds a JSON delta. The 1 ms bound is set so a single tick consumes under 3% of its slot, leaving headroom for jitter and for the uWebSockets broadcast hop on core 5. `[citation needed]` to confirm whether a tighter bound is appropriate. | After sampler lands |

A "sampler tick build" is the wall time from the sampler thread waking up at the 30 Hz tick boundary through finishing the JSON delta payload, exclusive of the WebSocket broadcast (which runs on core 5). The hand-off from the sampler to uWebSockets is bounded separately by the WebSocket broadcast budget, which is not set yet.

## How this budget is enforced

This budget exists in three states across the build:

1. **Now: document only, no enforcement.** This file ships, plus the `meridian-bench` skeleton. The bench harness compiles and runs end to end against `libmeridian.a`, but its throughput and latency numbers are recorded as informational baselines, not as targets. No CI job fails on regression yet. The baseline subsection at the bottom of this document is filled in by the bench scaffold once `libmeridian.a` is linkable.

2. **Top-of-book and sampler enforcement via TSAN.** The seqlock writer/reader protocol lands with the concurrency milestone. A TSAN test suite enforces the seqlock correctness guarantees (no torn reads observed by the reader, writer never blocks). The sampler tick wall time is profiled then and signed off against the < 1 ms target. The 50 ns top-of-book read budget is profiled in the same pass.

3. **Matching event enforcement via `bench/baseline.json` regression workflow.** `meridian-bench` runs in CI on every PR and compares against `bench/baseline.json` checked into the repo. CI fails the build if throughput drops more than 5% relative to baseline, or if any of p50, p99, p99.9 latency percentiles increase more than 10% relative to baseline. The baseline is updated only by an explicit "regenerate baseline" workflow on `main`, never silently. The first headline benchmark report at `docs/perf/benchmark-report.md` is generated alongside the bench milestone, with histograms, methodology, and reproducibility instructions.

The pacing rule: a number in this document does not become a CI-enforced target until the milestone named in the "Measured in" column closes. Until then, it is a publicly stated design intent that the implementation is on the hook to defend when that milestone opens.

## Hardware reference

The headline target of >= 6M events per second single-threaded is for a modern x86_64 desktop. The exact reference machine specs (CPU model, core count, base and boost clock, cache sizes, RAM speed, kernel version, scheduler governor) land in `/home/mustafa/src/Meridian/docs/perf/benchmark-report.md` when the bench milestone closes.

The Fly.io free-tier shared-CPU machine where `meridian-server` runs is **not** the reference for the headline number. The shared-CPU machine has substantially lower steady-state throughput than a modern desktop, and treating its numbers as the headline would mislead. The Fly machine is profiled separately as part of hosting hardening, and the resulting numbers are reported as a "live demo throughput" data point in `docs/perf/findings.md`, distinct from the headline benchmark in `docs/perf/benchmark-report.md`.

When the project ships, the README's headline number cites the desktop benchmark; the demo page's footer may show the live Fly throughput separately so a recruiter clicking the demo is not surprised by the gap.

## Initial baseline (informational, not enforced)

These numbers are the output of `meridian-bench --events 1000000 --seed 42` against the single-symbol matching engine, with no PGO and no LTO, on a development machine. They are explicitly informational. The bench milestone is where these numbers must be defended against the headline targets above.

Reproduction:

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target meridian-bench
./build/apps/bench/meridian-bench --events 1000000 --seed 42
```

The bench also writes `bench/last-run.json` as a sidecar; the file is informational and is not yet the CI baseline (`bench/baseline.json` lands at the bench milestone).

| Metric | Initial baseline | Notes |
|---|---|---|
| Throughput | 5.7 M events/sec | Synthetic limit orders, deterministic seed=42, single symbol, single thread, no PGO, no LTO. Not the headline benchmark. The 6M events/sec target is the bench milestone deliverable, measured after the optimization passes. |
| Wall clock | 176.4 ms | For 1,000,000 synthetic events. |
| Matching event p50   | 125 ns  | Well within the <= 500 ns target, but this measurement includes the `std::vector<ExecutionReport>` allocation inside `MatchingEngine::apply` (a known cost in the current implementation, documented in code). The bench-milestone measurement is the one that defends the headline target. |
| Matching event p90   | 231 ns  | |
| Matching event p99   | 444 ns  | Within the <= 2 us target on this run; bench-milestone measurement is authoritative. |
| Matching event p99.9 | 1108 ns | Within the <= 5 us target on this run; bench-milestone measurement is authoritative. |
| Matching event max   | 4308991 ns | Single tail outlier, likely a kernel preempt or page fault. The bench milestone will use a pinned-CPU, isolated-core methodology that suppresses these; the current bench is unpinned and the tail is left in the histogram for honesty. |
| Heap allocations during hot path | not yet measured | Must be 0 (the OrderPool is the no-allocation guarantee). The current `apply()` returns a freshly constructed `std::vector<ExecutionReport>` per call, which is documented in code. The bench milestone is the one that will assert the zero-allocation invariant on the hot path. |

## Out-of-scope budgets (deferred to later milestones)

These budgets are not set yet because the components do not exist yet. Listed here so later work has a clear inventory of what to publish next:

* **WebSocket broadcast hop** (sampler thread to uWebSockets event loop on core 5): published when the WebSocket server lands.
* **End-to-end tick latency** (matching event applied to delta visible to a connected WebSocket client): published when the WebSocket server lands, validated against the 30 Hz cadence end to end.
* **Frontend re-render budget**: the depth chart re-renders in under 16 ms (60 fps headroom), trade tape append does not retain old DOM nodes, total bundle size stays under 200 KB gzipped. Published with the React frontend; out of scope for `docs/perf/budget.md` until then, may move into this file when the frontend ships.

## Open questions

The following items are flagged for the next fact-check pass:

1. The 50 ns top-of-book read p50 figure is a defensible budget given x86_64 seqlock reader cost, but no specific external benchmark anchors it. Confirm whether the figure should remain as a stated budget or be deferred until the seqlock lands and can be measured. `[citation needed]` for an external published seqlock reader benchmark.
2. The < 1 ms sampler tick build figure is inferred from the 30 Hz cadence. Confirm the inference is sound (a tick that consumes under 3% of its 33.3 ms slot is a reasonable budget) or push back if a tighter or looser bound is more appropriate.

## Revision log

| Date | Change |
|---|---|
| 2026-05-09 | Initial publication. Matching event, top-of-book, sampler tick budgets set. Initial baseline subsection placeholder added; populated by bench scaffold. |
| 2026-05-09 | Initial baseline subsection populated from `meridian-bench --events 1000000 --seed 42`. Throughput 5.7 M evt/s; latency p50/p90/p99/p99.9/max = 125 / 231 / 444 / 1108 / 4308991 ns. Numbers labeled informational, not targets. The bench skeleton (`apps/bench/main.cpp`) and the HDRHistogram-c FetchContent wiring (root `CMakeLists.txt`, pinned to v0.11.8) ship with this revision. |
