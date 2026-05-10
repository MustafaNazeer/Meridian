# ADR 0002: Cross-symbol cancel via a dedicated `OrderIndex`, with the per-`Book` id index left in place

**Status:** accepted on 2026-05-09 (Phase 2)

**Context:** Phase 1 shipped the matching engine for one symbol. Each `Book` maintained its own `unordered_map<OrderId, Order*>` so `Book::find` and `Book::remove_by_id` were O(1) within the symbol. The matching engine's `MatchingEngine{pool, book}` constructor took a single `Book` and dispatched `EngineEvent::Cancel` events directly to it. The order's symbol was implicit; events that named a different symbol were ignored.

Phase 2 adds multi-instrument support: the engine now holds a `BookRegistry` (`unordered_map<Symbol, Book>`) and routes `NewOrder` by `event.symbol` into the matching `Book`. For `Cancel`, the wire format does not name the symbol; the engine has only the `OrderId`. The engine must either (a) consult some cross-symbol structure to discover which `Book` owns the order, or (b) require the caller to name the symbol on every cancel.

Option (b) was rejected. Real exchanges and ITCH replays issue cancels by id alone; carrying the symbol on the wire would diverge from the canonical protocol and complicate the Phase 6 ITCH conformance work.

## Decision

Phase 2 introduces a project-level `OrderIndex` (`unordered_map<OrderId, Order*>`) that the matching engine consults on every `Cancel`. The order's own `symbol` field carries the routing tag, so a single dereference of the indexed `Order*` tells the engine which `Book` to call.

Each Phase 1 `Book` keeps its per-symbol id index. **Both indexes ship simultaneously.** Per-`Book` insert and remove update both indexes; the per-`Book` index continues to back `Book::find` and per-symbol unit tests, the cross-symbol `OrderIndex` backs the matching engine's cancel dispatch.

## Consequences

**Positive**

* Cancel-by-id retains O(1) lookup with at most one extra hash-map probe versus Phase 1 (the cross-symbol `OrderIndex` lookup, then a direct `Book::remove_by_id` call that hits the per-`Book` index).
* No churn against Phase 1's `Book` API surface or its 8 unit tests; everything keeps working.
* The matching engine's cancel path is auditable in one place (`MatchingEngine::apply_cancel` in `src/matching.cpp`); reviewers do not have to reason about `BookRegistry`-side cancel dispatch.
* Backward-compatible: a single-symbol `BookRegistry` plus `OrderIndex` is functionally equivalent to the Phase 1 setup, so Phase 1 tests migrate mechanically.

**Negative**

* The `Order*` is held in two hash maps. Each insert and erase touches both. The double-write is constant-time but doubles the hash-map work on the hot path's order-add and order-remove subroutines.
* The two indexes must never disagree. A bug that updates one but not the other surfaces as a phantom order or a leaked slot. The matching engine's `apply_limit` insert and `sweep` removal paths take care to keep them in sync; the unit tests `OrderPool` exhaustion and the multi-instrument integration test catch the visible failures, but a hand-audit of the dual-update sites in `matching.cpp` is recommended for any future change to the cancel path.
* `Book::find` (the per-`Book` lookup) returns within-symbol results; consumers that want cross-symbol lookup must use `OrderIndex::find`. The two have the same name shape but different scopes; documentation must be explicit.

## Future direction

Phase 5 (the bench push to 6M events per second) or Phase 7 (post-only and FOK) is a natural moment to consolidate the two indexes into one. Two paths:

* **Drop the per-`Book` id index and rely solely on `OrderIndex`.** `Book::find` would consult `OrderIndex` (passed in by reference at construction). Cleaner; one structure to keep coherent. Per-`Book` unit tests would need either dependency injection or a self-owned `OrderIndex` per `Book`.
* **Make `OrderIndex` a façade over per-`Book` indexes.** `OrderIndex::find(OrderId)` walks each `Book`'s index. This is O(n_symbols), which is fine when there are at most a few dozen symbols, but breaks once the engine handles thousands.

The first path is preferred; the consolidation is filed in `future-ideas.md` so it does not get lost. No deadline; the dual-index design is acceptable at the Phase 1 through Phase 6 cadence.

## Related

* Companion plan: `docs/superpowers/plans/2026-05-09-phase-2-multi-instrument-and-cancel.md`.
* Matching semantics: `docs/risk/matching-semantics.md` (Phase 1 conventions; section 8.2 describes the unknown-id sentinel choice that carries over to Phase 2's cross-symbol cancel).
* Audit cases: `docs/risk/audit-cases.md` is updated with Phase 2 cross-symbol cancel cases by the Risk and Financial Correctness Reviewer in this phase.
