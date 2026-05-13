# Latency budget

**Status:** the matching event budget is enforced today by the bench regression workflow (`bench/check_regression.py` against `bench/baseline.json`, run on every PR). The top-of-book read budget and the sampler tick budget remain informational; they land alongside the WebSocket server milestone, when the sampler thread itself ships. Enforcement schedule is documented under "How this budget is enforced" below.

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
| p50  | <= 50 ns | The seqlock reader in the steady, untorn case is two acquire seq loads plus five relaxed atomic data loads plus a small struct copy, which on modern x86_64 is in the tens-of-nanoseconds range. The 50 ns figure is set as the budget the implementation is held to. `[citation needed]` for an external benchmark anchoring the 50 ns figure to a published seqlock measurement. | Seqlock landed (ADR 0003); read p50 measured alongside the WebSocket server milestone |

The top-of-book read budget exists so the sampler can hit its 30 Hz cadence with substantial headroom while reading every active symbol. With the demo's five-symbol set (AAPL, SPY, NVDA, TSLA, GOOG), five reads per tick at 50 ns each costs 250 ns per tick on the read path, which is negligible relative to the 33.3 ms tick interval at 30 Hz.

### Sampler tick build (producer side, runs at 30 Hz on its own core)

| Bound | Target | Notes | Measured in |
|---|---|---|---|
| Tick build wall time | < 1 ms at 30 Hz | The sampler runs on core 6 at 30 Hz (one tick every 33.3 ms): wakes, reads all snapshots, builds a JSON delta. The 1 ms bound is set so a single tick consumes under 3% of its slot, leaving headroom for jitter and for the uWebSockets broadcast hop on core 5. `[citation needed]` to confirm whether a tighter bound is appropriate. | After sampler lands |

A "sampler tick build" is the wall time from the sampler thread waking up at the 30 Hz tick boundary through finishing the JSON delta payload, exclusive of the WebSocket broadcast (which runs on core 5). The hand-off from the sampler to uWebSockets is bounded separately by the WebSocket broadcast budget, which is not set yet.

## How this budget is enforced

This budget exists in three states across the build:

1. **Foundations: document only, no enforcement.** Closed at the start of the bench-push milestone. This file shipped alongside a skeleton `meridian-bench`; numbers were informational, no CI job failed on regression.

2. **Top-of-book and sampler enforcement via TSAN.** The seqlock writer/reader protocol landed with the concurrency milestone (see ADR 0003 and `tests/concurrency/test_seqlock_concurrent.cpp`). The TSAN test enforces no torn reads under one writer plus two readers spinning for at least three seconds (60 seconds for the documented acceptance pass via `MERIDIAN_TSAN_DURATION_SEC`). The sampler tick wall time is profiled and signed off against the < 1 ms target alongside the WebSocket server milestone, when the sampler thread itself ships. The 50 ns top-of-book read budget is profiled at the same time.

3. **Matching event enforcement via `bench/baseline.json` regression workflow** (in force today). `meridian-bench` runs in CI on every PR (the clang job in `.github/workflows/ci.yml`) and `bench/check_regression.py` compares the resulting `bench/last-run.json` against `bench/baseline.json` checked into the repo. CI fails the build if throughput drops more than 5 percent relative to baseline, or if any of p50, p99, p99.9 latency percentiles rise more than 10 percent relative to baseline. The baseline values are captured on the GitHub Actions `ubuntu-24.04` runner so the comparison is apples-to-apples; baselines captured on a developer machine would be artificially tight against shared-CI hardware noise. To regenerate the baseline (e.g., after an optimization pass closes that legitimately raises the floor), run the bench locally or in CI, paste the resulting numbers into `bench/baseline.json`, and open a PR. The change is reviewed like any other regression.

The pacing rule: a number in this document does not become a CI-enforced target until the milestone named in the "Measured in" column closes. Until then, it is a publicly stated design intent that the implementation is on the hook to defend when that milestone opens.

## Hardware reference

The headline target of >= 6M events per second single-threaded is for a modern x86_64 desktop. The exact reference machine specs (CPU model, core count, base and boost clock, cache sizes, RAM speed, kernel version, scheduler governor) land in `/home/mustafa/src/Meridian/docs/perf/benchmark-report.md` when the bench milestone closes.

The Fly.io free-tier shared-CPU machine where `meridian-server` runs is **not** the reference for the headline number. The shared-CPU machine has substantially lower steady-state throughput than a modern desktop, and treating its numbers as the headline would mislead. The Fly machine is profiled separately as part of hosting hardening, and the resulting numbers are reported as a "live demo throughput" data point in `docs/perf/findings.md`, distinct from the headline benchmark in `docs/perf/benchmark-report.md`.

When the project ships, the README's headline number cites the desktop benchmark; the demo page's footer may show the live Fly throughput separately so a recruiter clicking the demo is not surprised by the gap.

## Current baseline (CI-enforced)

The numbers in `bench/baseline.json` are captured on the GitHub Actions `ubuntu-24.04` runner with `meridian-bench --events 1000000 --seed 42 --warmup 100000`, single thread, single symbol, mixed event stream (70 percent limit, 20 percent market, 10 percent cancel), no PGO, no LTO. Every PR builds the bench, runs it once, and `bench/check_regression.py` fails CI if the run regresses past the published tolerances. The regression check is also run locally with the same command (the script reads the same JSON sidecar shape).

Reproduction (local):

```bash
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target meridian-bench
./build/apps/bench/meridian-bench --events 1000000 --seed 42 --json-out bench/last-run.json
python3 bench/check_regression.py bench/baseline.json bench/last-run.json
```

The CI-enforced baseline lives at `bench/baseline.json`. A snapshot of a representative GitHub Actions run is reproduced below as documentation; the live numbers in CI are the source of truth.

| Metric | Headline target | Notes |
|---|---|---|
| Throughput | >= 6.0 M events/sec on the desktop reference; lower on shared-CI runners (informational) | Synthetic mix (70 percent limit, 20 percent market, 10 percent cancel), deterministic seed=42, single symbol, single thread, no PGO, no LTO. The headline 6M figure is for a modern x86_64 desktop; the GitHub Actions shared-CPU runner runs at a fraction of that and is used only for regression detection, not for the headline number. |
| Matching event p50   | <= 500 ns | Headline. The bench measurement includes `std::chrono::steady_clock::now()` overhead on each event (~25 ns on Linux x86_64) plus the matching work plus the seqlock publish. |
| Matching event p99   | <= 2 us | Headline. |
| Matching event p99.9 | <= 5 us | Headline. |
| Matching event max   | informational | Single tail outliers (multi-millisecond) are typically kernel preempts or page faults; the bench is unpinned and the tail is left in the histogram for honesty. The headline number is read at p99.9, not max. |
| Heap allocations on the inner matching loop | 0 | Enforced structurally by `HotPathGuard` inside `sweep()`: any heap allocation while the guard is active aborts the Debug build. Every property test (1000 random sequences each) and every integration scenario passes under Debug. The new `apply()` out-param overload eliminated the per-call `std::vector<ExecutionReport>` allocation that the previous returning overload incurred; the convenience returning overload is preserved for tests and ad-hoc callers. |

## Out-of-scope budgets (deferred to later milestones)

These budgets are not set yet because the components do not exist yet. Listed here so later work has a clear inventory of what to publish next:

* **WebSocket broadcast hop** (sampler thread to uWebSockets event loop on core 5): published when the WebSocket server lands.
* **End-to-end tick latency** (matching event applied to delta visible to a connected WebSocket client): published when the WebSocket server lands, validated against the 30 Hz cadence end to end.
* **Frontend re-render budget**: the depth chart re-renders in under 16 ms (60 fps headroom), trade tape append does not retain old DOM nodes, total bundle size stays under 200 KB gzipped. Published with the React frontend; out of scope for `docs/perf/budget.md` until then, may move into this file when the frontend ships.

## Open questions

The following items are flagged for the next fact-check pass:

1. The 50 ns top-of-book read p50 figure is a defensible budget given x86_64 seqlock reader cost, but no specific external benchmark anchors it. Confirm whether the figure should remain as a stated budget or be deferred until the seqlock lands and can be measured. `[citation needed]` for an external published seqlock reader benchmark.
2. The < 1 ms sampler tick build figure is inferred from the 30 Hz cadence. Confirm the inference is sound (a tick that consumes under 3% of its 33.3 ms slot is a reasonable budget) or push back if a tighter or looser bound is more appropriate.

## v1.1: engine observability overhead

The v1.1 `MatchingEngine` constructor takes an `observability` flag (default `true`). When set, `apply` brackets each event with two `std::chrono::steady_clock::now()` reads, records the delta into a 33-bucket log-spaced histogram via one relaxed atomic increment, walks the top of each side's price map to publish an L8 depth snapshot under the depth seqlock, and pushes a print into the trade ring for each match leg. When set to `false`, all four side effects are skipped and the matching path collapses back to its v1 shape; the top-of-book publish is unconditional in both modes since it is part of the core engine contract.

Paired measurements on the desktop reference, same seed (42), same 1M measured events, same 100k warmup:

| Mode | Throughput | p50 | p99 | p99.9 |
|---|---|---|---|---|
| `observability=false` (matching only) | 4.79 M evt/s | 149 ns | 618 ns | 1332 ns |
| `observability=true` (matching + depth + trades + histogram) | 2.79 M evt/s | 288 ns | 903 ns | 2128 ns |

The instrumented path costs roughly 40 percent of the bench-mode throughput at full unbounded rate. That cost is acceptable for the live demo, which runs the server at 400 events per second (four orders of magnitude below the instrumented ceiling), so the PerfPanel histogram and the Tape tile both have real data to render with multi-thousand-x headroom. It is not acceptable for the headline benchmark.

The bench binary (`apps/bench/main.cpp`) is therefore built with `observability=false`. The headline throughput is the v1 matching path; `bench/baseline.json` continues to track the matching loop in isolation. Future work that wants to defend an instrumented-path budget should publish a second baseline with `observability=true` rather than blending the two.

## Revision log

| Date | Change |
|---|---|
| 2026-05-09 | Initial publication. Matching event, top-of-book, sampler tick budgets set. Initial baseline subsection placeholder added; populated by bench scaffold. |
| 2026-05-09 | Initial baseline subsection populated from `meridian-bench --events 1000000 --seed 42`. Throughput 5.7 M evt/s; latency p50/p90/p99/p99.9/max = 125 / 231 / 444 / 1108 / 4308991 ns. Numbers labeled informational, not targets. The bench skeleton (`apps/bench/main.cpp`) and the HDRHistogram-c FetchContent wiring (root `CMakeLists.txt`, pinned to v0.11.8) ship with this revision. |
| 2026-05-10 | Bench-push milestone closes. `MatchingEngine::apply` gains an out-param overload that eliminates the per-call `std::vector<ExecutionReport>` allocation; the returning overload is preserved as a convenience. Bench rewritten to reuse a single report buffer across the timed window, add a 100k-event warmup phase, and emit a `schema_version: 2` JSON sidecar. `bench/check_regression.py` lands; `bench/baseline.json` lands with numbers captured on the GitHub Actions ubuntu-24.04 runner. CI (`.github/workflows/ci.yml`) runs the bench plus the regression check on every PR (clang job only). 5 percent throughput tolerance, 10 percent per-percentile latency tolerance. |
| 2026-05-11 | v1.1 extended snapshot lands. `MatchingEngine` gains a live latency histogram (33 log-spaced buckets, lock-free atomic increments), an L8 depth publisher under a second seqlock, and a per-event trade aggregator that pushes prints into `Book::trades()`. An opt-in `observability` constructor flag (default true) lets the bench skip all four paths; the bench is built with `observability=false`, so its measured throughput reflects the matching loop without the v1.1 instrumentation. Paired-run measurement of the instrumented path published above ("v1.1: engine observability overhead"). |
