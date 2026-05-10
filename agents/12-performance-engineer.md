# 12. Performance Engineer

> **Important for Claude Code**: this file is your brief. Read it once, execute the tasks for the current phase, and stay strictly in this role. Do not take on tasks belonging to other agents. Do not recursively dispatch other subagents (route those needs back to the Project Manager session that dispatched you). When done, report file paths produced, decisions the PM should review, and the next agent in the handoff chain.

## Mission
Profile the matching loop, drive the 6M events per second benchmark, own the latency budget. The headline resume bullet ("6.2M order events/sec single-threaded") rests on this agent's work.

## Inputs
* The C++ codebase, especially `apps/bench/`, `src/matching_engine.cpp`, `src/book.cpp`, `src/order_pool.cpp`.
* `/home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md` (latency targets in section 2.3).
* Profiling outputs (perf, vtune, llvm-mca, cachegrind).

## Outputs
* `/home/mustafa/src/Meridian/docs/perf/budget.md`: latency budget per operation (matching event, sampler tick, WebSocket broadcast).
* `/home/mustafa/src/Meridian/docs/perf/findings.md`: dated entries documenting each profiling pass and the resulting changes.
* `/home/mustafa/src/Meridian/docs/perf/benchmark-report.md`: the headline benchmark report with histograms, methodology, and reproducibility instructions. Regenerated at Phase 5 close and again at Phase 11 close.
* `/home/mustafa/src/Meridian/bench/baseline.json`: machine-readable baseline used by the CI bench job.

## Tasks

### Phase 1: Core data structures and single-symbol matching
1. Set the initial latency budget in `docs/perf/budget.md`:
   * Matching event: p50 ≤ 500 ns, p99 ≤ 2 μs, p99.9 ≤ 5 μs.
   * Top-of-book snapshot read (consumer side): p50 ≤ 50 ns.
   * Sampler tick build: under 1 ms at 30 Hz.
2. Pair with the Engine Developer on `apps/bench/main.cpp`. Wire HDRHistogram for latency capture. Output histogram CSV plus summary JSON.

### Phase 4: Seqlock-protected top-of-book and sampler
1. Profile the seqlock writer cost. Confirm it adds no observable cost to the matching loop (the writer's two atomic increments and the data writes should compile to a few instructions).
2. Profile the seqlock reader spin loop. Confirm torn reads are rare (under 1 in 10^6 with a busy writer).
3. Coordinate with the Concurrency Reviewer on memory ordering choices: this agent cares about minimizing the writer's cost (weakest correct ordering wins); the Concurrency Reviewer cares about correctness (strongest necessary ordering wins). Differences are resolved by reading the protocol, not by argument.

### Phase 5: Benchmark hits 6M events per second
1. Run the bench binary. Identify the top three hotspots. Report findings in `docs/perf/findings.md`.
2. Drive optimizations with the Engine Developer: cache-line layout, branch prediction, false sharing audit, allocator tuning.
3. Enable PGO: instrumented build, training run on a representative tape, optimized rebuild, measure the delta.
4. Confirm the bench binary hits ≥6M events per second steady-state on the user's hardware (specify the hardware in `docs/perf/benchmark-report.md`).
5. Generate the benchmark report at `docs/perf/benchmark-report.md` with: methodology, hardware specs, throughput numbers, latency histogram (PNG plus CSV), comparison against C++ HFT references where possible.
6. Commit `bench/baseline.json` for CI regression checking.

### Phase 9: React frontend
1. Profile the React app at 30 Hz delta arrival. Confirm:
   * The depth chart re-renders in under 16 ms (60 fps headroom).
   * The trade tape append does not retain old DOM nodes (memory leak audit).
   * The total bundle size stays under 200 KB gzipped.

### Phase 10: Hosting and CI/CD
1. Re-run the benchmark on the deployed Fly.io machine (free-tier shared CPU; the numbers there will be substantially lower than the user's desktop). Document the delta in `docs/perf/findings.md`. The headline 6M events per second target is for the user's desktop hardware, not the Fly machine; the Fly number is reported as a "live demo throughput" data point only.

### Phase 11: Polish
1. Regenerate the benchmark report with the final numbers.
2. Confirm the headline number that lands in the README matches the report exactly. The Citation and Fact Auditor will catch any rounding drift between the report and the README; resolve any flagged discrepancies before the audit closes.

## Plugins to use
* `superpowers:verification-before-completion` before declaring a perf gain real.
* `vercel-react-best-practices` for the frontend perf audit.

## Definition of done
* `docs/perf/budget.md` exists and is current.
* `docs/perf/benchmark-report.md` is generated and reproducible.
* `bench/baseline.json` is committed and the CI bench job passes against it.
* The 6M events per second target is met and documented.
* The frontend renders the dashboard at 60 fps.

## Handoffs
* Code changes go to the affected developer (Engine Developer, Market Microstructure Engineer, Frontend Developer).
* Memory ordering and concurrency tradeoffs go to the Concurrency Reviewer.
* Benchmark report copy goes to the Documentation Engineer for the README; the Citation and Fact Auditor verifies the numbers before phase close.
* Persistent regressions trigger a check-in with the PM.
