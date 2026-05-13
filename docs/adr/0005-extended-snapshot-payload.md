# ADR 0005: extended WebSocket snapshot payload (v1.1)

**Date**: 2026-05-11
**Status**: Accepted
**Supersedes**: nothing
**Superseded by**: nothing

## Context

The v1 WebSocket frame carried only top-of-book. Four panels on the live demo shipped with placeholder anatomy waiting for an extended payload: the Ladder's rows 2 through 8 on each side, the Tape (recent trade prints), the DepthChart cumulative step function, and the PerfPanel's engine latency histogram and percentile cells. The header bullet on the production README named the placeholders explicitly: "L1 today, L8 with the extended snapshot follow-up."

The constraints that shaped this decision:

* **Single matching thread, lock-free reads.** Every new payload section has to be readable from the sampler thread (and indirectly the WebSocket server) without blocking the matching loop. The existing top-of-book snapshot uses a seqlock under one writer many readers; new primitives need to follow the same idiom.
* **30 Hz wire cadence is the contract.** The sampler broadcasts at 30 Hz. Adding payload sections must not change the cadence, fragment the message kinds, or require the frontend to coordinate across multiple frame types.
* **Editorial throttle holds.** The frontend renders editorial cells (Hero stats, Ladder L1 row, DepthChart) at 4 Hz with a separate `displayedTop` field. The new sections should fit the same throttle pattern where appropriate.
* **No bench regression on the matching path.** The headline number in `bench/baseline.json` is the matching loop without instrumentation. Adding live observability must not cost throughput on the binary that ships the headline number.

## Decision

Extend the two existing WebSocket message kinds (`snapshot` and `delta`) with four nested sections per frame: `tob`, `depth` (up to L8 per side), `trades` (last 16 prints, oldest first), and `latency` (33-bucket histogram plus p50, p99, p99.9, max, samples). Keep the existing 30 Hz cadence and the existing JSON-over-text-frames protocol.

Engine-side, three new primitives carry the data:

1. **`SeqlockDepth`** (`include/meridian/depth_snapshot.hpp`): a second seqlock on `Book` covering the L8 depth array per side. Same one-writer-many-readers idiom as the top-of-book seqlock.
2. **`TradeRing`** (`include/meridian/trade_print.hpp`): a fixed-size ring of 16 prints on `Book` with a monotonic `seq` field. Readers compare a snapshot-time `seq` against the ring head to detect torn reads.
3. **`LatencyHistogram`** (`include/meridian/latency_histogram.hpp`): 33 log-spaced buckets covering 16 ns to 16 us, lock-free per-bucket atomic increments, totals readable from any thread.

The instrumented path is gated behind an `observability` flag on the `MatchingEngine` constructor (default `true`). The bench binary builds with `observability=false` so its measured throughput continues to reflect the v1 matching path; the live `meridian-server` builds with the default so the four panels render real data. See `docs/perf/budget.md` ("v1.1: engine observability overhead") for paired measurements.

Two new property invariants protect the depth array: **P11** (sorted, deduped) and **P12** (bounded by resting qty). They are tested by `MatchingInvariants.DepthOrderedAndDeduped` and `MatchingInvariants.DepthQuantityBoundedByResting` over 1000 cases per CI run, plus byte-for-byte diff against the Python reference's `l8_depth()` method inside `Differential.RandomSequencesAgreeWithReference`. The trade aggregator is verified end-to-end against the 26-case worked-example corpus in `tests/integration/test_trade_aggregation.cpp`.

## Alternatives considered

1. **Split message kinds (`snapshot`, `depth`, `trade`, `latency`).** Honest about per-data-type cadence (trades are push-on-fill, latency is fine at 1 Hz, top-of-book and depth want 30 Hz). Rejected for v1.1 because four parsers on the frontend and four state machines for connection / reconnect / backfill is meaningful added complexity for a single-viewer demo. The split is feasible without breaking the engine primitives if a later version wants to ship it; the seqlock, ring, and histogram are all addressable independently.
2. **Binary protocol (FlatBuffers).** Reduces wire size by roughly 3x and parse cost on the frontend. Rejected because v1.1's per-frame size at 30 Hz lands well under 1 KB and the bottleneck is not the wire format. JSON is also debuggable in browser DevTools, which keeps the demo legible to anyone who opens the network panel. Deferred to the `future-ideas.md` binary-protocol entry, to revisit if the demo ever needs to scale to many concurrent viewers.
3. **Rolling-reservoir histogram (last N samples).** More honest live picture that responds to rate changes. Rejected for v1.1 in favor of the cumulative histogram so the percentiles stabilise quickly (within seconds of a warm engine), so the reader does not need to recompute percentiles under rolling-window state, and so the histogram bucket increments stay lock-free (a rolling reservoir wants a mutex or a complicated lock-free deque). The cumulative form is also what a production exchange would publish on a status page.
4. **Don't gate behind an `observability` flag; eat the bench regression.** The instrumented path measured ~40 percent throughput cost on the desktop reference (see `docs/perf/budget.md`). Rejected because the headline benchmark number is the resume bullet that anchors the project. The flag adds one bool to one constructor and zero overhead in the false branch.

## Consequences

* The Ladder, Tape, DepthChart, and PerfPanel all render real data. Four placeholder surfaces become four real surfaces, and the public README's "L1 today, L8 with the extended snapshot follow-up" framing is retired in favor of "L8 depth, live."
* The matching engine has a second seqlock, a trade ring, and a histogram on the hot path when `observability` is on. Bench throughput is preserved by building bench with `observability=false`; the live server gets the instrumentation.
* Two new property invariants (P11, P12) protect the depth array; the Python reference grows an `l8_depth()` method and the differential test diffs the C++ depth against it on every random sequence.
* The wire protocol carries one nested envelope instead of one flat top-of-book object. The frontend Zustand store carries `depth`, `trades`, and `latency` alongside `top`; component re-renders are unchanged except where new fields are read.
* Future expansion (e.g. per-symbol depth in a multi-symbol view, or a per-bucket histogram per percentile) extends this design rather than replacing it. The split-message alternative remains available without rewiring the engine primitives.
