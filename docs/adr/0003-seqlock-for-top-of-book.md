# ADR 0003: Seqlock-protected top-of-book snapshot, per-field std::atomic layout

**Date**: 2026-05-10
**Status**: Accepted

## Context

The matching engine runs single-threaded on a pinned core (core 4 in the live demo topology, see `docs/architecture.md` section 5). Two other threads need to read the top of book without ever blocking the matching thread:

* The sampler thread (core 6) wakes at 30 Hz, snapshots the top of book for every symbol, and hands a delta to the WebSocket event loop.
* The WebSocket event loop (core 5) accepts new client connections and serves the snapshot picture immediately on connect (the `snapshot` message that precedes the delta stream).

The matching loop must not take a mutex on the publish path: any blocking primitive that the sampler or the WebSocket thread might also touch can preempt the matching loop and blow the latency budget (`docs/perf/budget.md` lists p50 < 500 ns, p99 < 2 us). The publish must be lock free, allocation free, and finite-time bounded on the writer side.

The published picture is small: best bid price and quantity, best ask price and quantity, and the event timestamp at which this picture was taken. Total payload is six 32-or-64-bit values: trivially fits in a cache line.

## Decision

Publish the top of book through a per-`Book` **seqlock** with **per-field `std::atomic<T>`** representation:

```
class SeqlockSnapshot {
    alignas(64) std::atomic<uint64_t> seq_{0};
    std::atomic<Price>     bid_px_, ask_px_;
    std::atomic<Quantity>  bid_qty_, ask_qty_;
    std::atomic<Timestamp> ts_;
};
```

* The writer (matching thread) bumps `seq_` to the next odd value with `memory_order_release`, stores each data field with `memory_order_relaxed`, then bumps `seq_` to the next even value with `memory_order_release`.
* The reader (any other thread) loads `seq_` with `memory_order_acquire`, retries if odd, reads each data field with `memory_order_relaxed`, loads `seq_` again with `memory_order_acquire`, and retries if the two loads disagree.

`SeqlockSnapshot` lives at `include/meridian/seqlock_snapshot.hpp`. `Book` owns one snapshot per symbol; `MatchingEngine::apply` calls `book->publish_top_of_book(event.ts)` after every accepted event (limit, market, IOC, cancel, post-only / FOK reject). The reader-side accessor is `Book::top_of_book()`.

## Memory model

The C++ memory model (ISO/IEC 14882:2020 [intro.races]) defines that two operations conflict only if at least one is a non-atomic write to the same location. Every read and write in this design is an atomic operation, so by the standard there is no data race: this is true regardless of memory ordering. ThreadSanitizer respects the standard and stays silent on the per-field accesses.

The release/acquire pairing on `seq_` provides the happens-before edges that make the seqlock protocol correct in the abstract machine:

* Writer's first `seq_.store(odd, release)` ensures any prior writes by the matching thread are visible before the seqlock window opens.
* Writer's data stores happen between the odd and even seq stores; they are themselves race-free against any reader (atomic).
* Writer's second `seq_.store(even, release)` ensures the data stores happen-before the readable-window edge.
* Reader's `seq_.load(acquire)` (the s1 load) synchronises-with the writer's most recent even store, so the data fields read after s1 are already-visible at least up to that publish.
* Reader's second `seq_.load(acquire)` (the s2 load) detects whether a new write started between s1 and the data reads: if `s1 == s2 && (s1 & 1) == 0`, the data is from a single consistent publish.

Livelock is bounded in practice: the matching thread publishes once per accepted event, on the order of microseconds apart at the 6M-events/sec target; the reader loop spins for at most a small handful of iterations before it lands on a stable value. A `try_read(out, max_retries)` form is provided for callers that want a hard bound on tail latency, but is not used by the sampler or the WebSocket server today.

## Alternatives considered and rejected

### Raw POD fields plus `std::atomic_thread_fence`

This is the canonical Lamport / Linux-kernel seqlock pattern: declare the data as plain non-atomic fields, use `std::atomic_thread_fence(memory_order_release)` around the writer's data stores, and `std::atomic_thread_fence(memory_order_acquire)` around the reader's data loads. The advantage is one fewer atomic operation per data field at the codegen level.

Rejected because:

* The C++ memory model in 2020 does not give a clean answer for a non-atomic data race that the seqlock makes "benign by retry": the racy reader's load is technically undefined behaviour even though the seqlock invariant guarantees the value will be discarded if the seq counter changed. Hans Boehm's analysis ("Can Seqlocks Get Along With Programming Language Memory Models?", MSPC 2012) shows the per-field `std::atomic` form sidesteps this entirely.
* ThreadSanitizer flags the racy reader's load even though it is benign. The workaround is `__tsan_acquire` / `__tsan_release` annotations or a per-file TSAN suppression, both of which add maintenance surface.
* The codegen difference is irrelevant on the matching loop's publish path: at most one publish per event, and the data stores are five 32-or-64-bit relaxed atomic stores which compile to plain `MOV` on x86-64.

The per-field `std::atomic` form costs nothing measurable in the publish path and trades a microscopic extra codegen instruction per field for first-class TSAN cleanliness and a portable C++20 memory model proof. That is a good trade for this engine.

### A reader-writer lock (`std::shared_mutex` or equivalent)

Trivially correct but blocks the matching thread on any reader contention. The whole point of the seqlock is to keep the matching thread off any blocking primitive that another thread can hold; a shared mutex defeats that purpose. Rejected on first principles.

### Read-Copy-Update (RCU)

Powerful for larger published structures, but the deferred-reclamation machinery (per-thread quiescent state tracking, grace periods, RCU reader epochs) is heavyweight for a 24-byte payload. The seqlock's whole-snapshot copy on read is fine here because the snapshot is small enough to fit in a cache line. Reserved for future work that publishes larger structures (e.g., L10 depth, the recent-trades ring) if those move out of the matching thread.

### Hazard pointers

Same answer as RCU: too much machinery for a 24-byte payload. Reserved for the same future use case.

## Consequences

* The matching thread's publish path is bounded: five relaxed atomic stores plus two release-ordered seq stores per event. No allocation, no syscall, no blocking primitive. Confirmed by the existing `HotPathGuard` debug allocator: the 1000 random sequences per property test (`MatchingInvariants.SpreadNonNegative`, `MatchingInvariants.SnapshotMatchesBook`) all pass under `-DCMAKE_BUILD_TYPE=Debug` with the heap-allocation guard active.
* Readers see a consistent picture: the property tests cover the structural correctness (`SpreadNonNegative`, `SnapshotMatchesBook`), and the multi-threaded TSAN test (`tests/concurrency/test_seqlock_concurrent.cpp`) covers the publish atomicity under real preemption. The TSAN test runs three seconds in CI by default; an `MERIDIAN_TSAN_DURATION_SEC` environment variable lets a developer or a manual acceptance run extend that to the documented 60-second pass in `docs/qa/regression-checklist.md`.
* The matching engine now has a stable boundary at `Book::top_of_book()` that the live-demo server, the sampler, and any future read-side consumer can take. The `BookRegistry` exposes per-symbol books, so the sampler iterates `registry.book(s)->top_of_book()` per symbol with no other coupling to engine internals.
* The cost on the engine's hot path is two release seq stores and five relaxed atomic stores per accepted event, plus the deterministic recompute of best-bid / best-ask from the bid/ask `std::map`s. Both maps are already touched by the matching loop, so the marginal recompute is two `begin()` calls. Measured impact on `meridian-bench` will be reported in the bench-push milestone.
