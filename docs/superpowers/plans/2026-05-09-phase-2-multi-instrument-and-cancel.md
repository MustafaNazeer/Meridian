# Phase 2: Multi-Instrument and Cancel-by-Id (Cross-Symbol Layer)

**Goal:** Ship `BookRegistry` (per-symbol `Book` dispatch) and `OrderIndex` (cross-symbol `OrderId` lookup) so the matching engine handles multiple symbols and supports cancel without knowing the order's symbol upfront.

**Architecture:** `MatchingEngine` switches from holding one `Book` to holding a `BookRegistry` (a fixed-capacity hash map from `Symbol` to `Book`) plus an `OrderIndex` (a hash map from `OrderId` to `Order*` across all symbols). On `NewOrder`, the engine dispatches by `event.symbol` to the right `Book`. On `Cancel`, the engine looks up the order in `OrderIndex`, retrieves its `Symbol`, and calls `remove_by_id` on the matching `Book`. Each successful insert and removal updates `OrderIndex`. The order's own `symbol` field carries the routing tag, so a single `Order*` is sufficient for the cancel path.

**Tech stack:** Same as Phase 1. C++20, GoogleTest, Python 3.11 stdlib for the reference. No new dependencies.

---

## Context

Phase 1 closed at commit `bc5e9ee`. The matching engine ships `Order`, `Level`, `Book`, `OrderPool`, `MatchingEngine` with limit, market, IOC, and per-`Book` cancel. Each `Book` already has its own `unordered_map<OrderId, Order*>` for cancel within the symbol; Phase 2 adds the cross-symbol layer that the multi-symbol `MatchingEngine::apply(Cancel)` dispatch needs.

Phase 2 is sized at ~50 percent of one window. The PM session is at 32 percent usage with 2.5 hours of headroom; this fits comfortably without bundling Phase 3.

## Key decisions

1. **`BookRegistry` is the engine's primary owner of `Book` objects.** The Phase 1 `MatchingEngine{pool, book}` constructor signature changes to `MatchingEngine{pool, registry, index}`. Every Phase 1 unit test that constructed a single `Book` directly is updated to construct a one-symbol `BookRegistry`. The Phase 1 tests still drive symbol id 1, so the migration is mechanical.

2. **`OrderIndex` maps `OrderId` to `Order*`** (not to `Symbol`). The order's own `symbol` field carries the routing info; a single dereference in the cancel path tells us which `Book` to call. This keeps `OrderIndex` simple and avoids storing the symbol redundantly.

3. **`BookRegistry` is fixed-capacity at construction.** Symbols are pre-registered; the matching loop never inserts a new symbol on the hot path. For Phase 2 the demo set is `{AAPL=1, SPY=2, NVDA=3, TSLA=4, GOOG=5}`. The constructor accepts an `initializer_list<Symbol>`. Lookups for unregistered symbols return `nullptr` and `apply` emits a `Reject` with reason `UnknownSymbol` (added to `RejectReason`).

4. **`Book::add` and `Book::remove_by_id` keep their per-symbol id index.** Phase 1's per-`Book` lookup is redundant with the cross-symbol `OrderIndex`, but removing it would be Phase 1 churn. Leave both indexes in place; the cross-symbol `OrderIndex` is consulted by the matching engine, the per-`Book` index is consulted by `Book::find` and per-symbol tests. This is documented in the new ADR.

5. **No exception path on multi-instrument dispatch.** An unregistered symbol on `NewOrder` or `Cancel` is a `Reject`, not a throw.

6. **`BookRegistry::book(Symbol)` returns `Book*`.** Returns `nullptr` for unknown symbols. The matching loop checks before dispatching; the `[[unlikely]]` annotation goes on the unknown-symbol branch.

## File structure

| Path | Owner | Purpose |
|---|---|---|
| `include/meridian/book_registry.hpp` (create) | PM | `BookRegistry` API |
| `src/book_registry.cpp` (create) | PM | `BookRegistry` impl |
| `include/meridian/order_index.hpp` (create) | PM | `OrderIndex` API |
| `src/order_index.cpp` (create) | PM | `OrderIndex` impl |
| `include/meridian/matching.hpp` (modify) | PM | Constructor takes `BookRegistry` and `OrderIndex` |
| `src/matching.cpp` (modify) | PM | Dispatch by symbol; cancel via `OrderIndex` |
| `include/meridian/execution_report.hpp` (modify) | PM | Add `RejectReason::UnknownSymbol` |
| `tests/unit/test_book_registry.cpp` (create) | PM, QA | Lookup, dispatch, no cross-symbol leak |
| `tests/unit/test_order_index.cpp` (create) | PM, QA | Insert, lookup, remove |
| `tests/unit/test_matching.cpp` (modify) | PM | Migrate fixture to `BookRegistry` |
| `tests/unit/test_book.cpp`, `test_level.cpp`, `test_order_pool.cpp` | (no change) | Per-type tests stay |
| `tests/integration/test_engine_vs_reference.cpp` (modify) | QA | Migrate `run_cpp` to `BookRegistry`; Python harness already supports multi-instrument once Reference Impl Engineer extends it |
| `tests/integration/test_multi_instrument.cpp` (create) | QA | New scenarios driving 5 symbols, cross-symbol cancel safety, symbol-isolation invariants |
| `tests/reference/matching_reference.py` (modify) | Reference Impl Engineer | Multi-symbol dispatch |
| `tests/reference/run_reference.py` (modify) | Reference Impl Engineer | Read symbol field from event JSON |
| `tests/reference/test_reference.py` (modify) | Reference Impl Engineer | Add multi-symbol tests |
| `docs/risk/audit-cases.md` (modify) | Risk Reviewer | Add Phase 2 audit cases |
| `docs/adr/0002-cross-symbol-cancel.md` (create) | Documentation Engineer / PM | Records the dual-index design (per-`Book` plus cross-symbol) |

## Dispatch order

1. **Stage A:** Reference Implementation Engineer extends the Python reference for multi-symbol. Runs in parallel with Stage B's first task.
2. **Stage B:** PM session implements `BookRegistry`, `OrderIndex`, and the matching dispatch update test-first.
3. **Stage C:** QA Engineer writes the cross-symbol integration test; Risk Reviewer adds Phase 2 audit cases; Code Reviewer reviews the PR.

---

## Task 1: PM amendments to `RejectReason` enum

Add `UnknownSymbol = 5` to `RejectReason` in `include/meridian/execution_report.hpp`. Document in matching-semantics.md (a small append-only note; no Phase 1 worked example changes).

## Task 2: Dispatch Reference Implementation Engineer

Extend the Python reference to support multi-symbol. The dispatch prompt:

```
You are the Reference Implementation Engineer agent for the Meridian project.
Read agents/18-reference-implementation-engineer.md, the existing
tests/reference/matching_reference.py, and
docs/superpowers/plans/2026-05-09-phase-2-multi-instrument-and-cancel.md.

Phase 2 task: extend the Python reference with multi-symbol support.

Specifically:

1. Add a MultiSymbolMatchingReference class (or extend the existing
   MatchingReference to take a list of symbols at construction). The class
   owns a dict {symbol_id: MatchingReference}; submit_* and cancel_at
   dispatch by symbol.

2. Cancel-by-id is cross-symbol: cancel(order_id) looks up the order's
   symbol in a per-class id_to_symbol map; calls cancel_at on the right
   per-symbol engine. Cancel for a symbol X never touches symbol Y's book.

3. New events: cancel for an order_id that was never submitted in any symbol
   emits Reject NotFound (existing semantics, applied across all symbols).
   New order with an unregistered symbol emits Reject with reason
   "unknown_symbol".

4. Update tests/reference/run_reference.py to read event["symbol"] from the
   wire format. Default to symbol=1 if absent so Phase 1 scenarios still
   work.

5. Add tests in tests/reference/test_reference.py for: 5-symbol dispatch
   (one event per symbol), cross-symbol cancel isolation (cancel for symbol
   X does not affect symbol Y), unknown-symbol reject.

Stay strictly in the Reference Implementation Engineer role. Do not modify
C++ code. Use stdlib only. Maintain byte-identical JSON output via
json.dumps(sort_keys=True, separators=(",", ":")).

Report file paths produced or modified, the test count, and any decisions
for the PM to review.
```

## Task 3: Implement `OrderIndex` (PM, test-first)

`include/meridian/order_index.hpp` and `src/order_index.cpp`. API:

```cpp
class OrderIndex {
public:
    OrderIndex();
    void insert(OrderId id, Order* order);
    Order* find(OrderId id) const noexcept;  // nullptr if absent
    void erase(OrderId id);
    bool contains(OrderId id) const noexcept;
    std::size_t size() const noexcept;
private:
    std::unordered_map<OrderId, Order*> map_;
};
```

`tests/unit/test_order_index.cpp`: insert returns identity on find; erase removes; double-erase is a no-op; lookup of unknown id returns nullptr.

## Task 4: Implement `BookRegistry` (PM, test-first)

`include/meridian/book_registry.hpp` and `src/book_registry.cpp`. API:

```cpp
class BookRegistry {
public:
    BookRegistry();
    explicit BookRegistry(std::initializer_list<Symbol> symbols);
    Book* book(Symbol s) noexcept;  // nullptr if unregistered
    const Book* book(Symbol s) const noexcept;
    bool contains(Symbol s) const noexcept;
    std::size_t size() const noexcept;
private:
    std::unordered_map<Symbol, Book> books_;
};
```

`tests/unit/test_book_registry.cpp`: empty registry returns nullptr; constructed-with-symbols registry returns valid Book pointers; unknown symbol returns nullptr; books for different symbols are distinct objects.

## Task 5: Migrate `MatchingEngine` to `BookRegistry` and `OrderIndex` (PM, test-first)

Constructor signature changes from `MatchingEngine(OrderPool&, Book&)` to `MatchingEngine(OrderPool&, BookRegistry&, OrderIndex&)`.

`apply` flow:
- `NewOrder`: lookup `book = registry.book(event.symbol)`. If null, emit `Reject UnknownSymbol`. Else dispatch (limit/market/IOC) using that `Book`. On insert into the book, also `index.insert(order_id, order_ptr)`.
- `Cancel`: lookup `order = index.find(event.order_id)`. If null, emit `Reject NotFound`. Else, `book = registry.book(order->symbol)`; call `book.remove_by_id(event.order_id)`; release to pool; `index.erase(event.order_id)`; emit `Cancel`.
- Existing `[[unlikely]]` annotations carry over.

Migration of all existing matching tests: `tests/unit/test_matching.cpp` fixture changes to construct `BookRegistry{1}` and pass with `OrderIndex`. `book.best_bid()` and friends become `registry.book(1)->best_bid()`.

## Task 6: Multi-symbol integration test (PM, test-first)

`tests/integration/test_multi_instrument.cpp` drives the 5 demo symbols (id=1..5) through both engines. Scenarios:

1. Independent activity per symbol: each of 5 symbols receives 3 limits + 1 market; verify both engines agree on per-symbol audit logs.
2. Cross-symbol cancel isolation: place a buy on symbol 1 and a buy on symbol 2; cancel the symbol-1 buy; verify symbol 2's book is unchanged.
3. Cross-symbol cancel by id: cancel an order on symbol 5 without naming the symbol; both engines route correctly.
4. Unknown-symbol reject: submit a NewOrder with symbol=99 (unregistered); both engines emit Reject UnknownSymbol.
5. Mixed: 50 events across the 5 symbols in random-but-deterministic order; byte-diff the report streams.

## Task 7: Update existing integration corpus

`tests/integration/test_engine_vs_reference.cpp` fixture migrates `run_cpp` to construct `BookRegistry{1}` plus `OrderIndex`. The 60 Phase 1 scenarios remain unchanged; they all use symbol id 1 implicitly.

## Task 8: Dispatch QA Engineer

QA expands coverage on `BookRegistry` and `OrderIndex` (>=85 percent line coverage), adds the multi-instrument tests to the regression checklist, and confirms ctest stays at 100 percent across Debug and Release.

## Task 9: Dispatch Risk Reviewer

Risk Reviewer adds 5 Phase 2 audit cases to `docs/risk/audit-cases.md`:
1. Cancel resting order on symbol X (success, no effect on Y).
2. Cancel after fully filled (Reject NotFound).
3. NewOrder for unknown symbol (Reject UnknownSymbol).
4. Multi-symbol concurrent matching (5 symbols, 5 audit logs, all match reference).
5. Cancel-by-id where id was placed on Y but cancel arrives without naming Y; engine routes correctly.

Signs off Phase 2 only if every case passes in both implementations.

## Task 10: Documentation Engineer files ADR 0002

`docs/adr/0002-cross-symbol-cancel.md` documents the dual-index choice (per-`Book` id_index plus cross-symbol `OrderIndex`), the rationale (avoid Phase 1 churn; both indexes are O(1)), and the future direction (consolidate to a single index in a later cleanup phase).

## Task 11: Citation Auditor pass

Audits new docs (matching-semantics.md UnknownSymbol note; audit-cases.md Phase 2 entries; ADR 0002). Verifies any concrete numbers or references trace to code.

## Task 12: Code Reviewer at PR time

Reviews the PR for hot-path discipline (no allocations in cancel path; `OrderIndex::find` is `noexcept`; `[[unlikely]]` on the unknown-symbol branch); type discipline; CMake hygiene; test discipline.

## Task 13: Phase 2 closeout

Standard push protocol: `gh pr create`, watch CI, admin merge once green. Flip `STATUS.md` to Phase 2 completed; set Next phase to Phase 3 (property-based tests with rapidcheck). Run mandatory check-in.

---

## Self-review

* Spec coverage: every output in `docs/plan.md` Phase 2 section maps to a task above. Five Phase 2 invariants implicit in the worked examples (no cross-symbol leak; cancel idempotence; reject NotFound; reject UnknownSymbol; FIFO preservation per symbol) are exercised by the multi-instrument integration test and the Phase 2 audit cases.
* Placeholder scan: no TBD or "fill in" left.
* Type consistency: `BookRegistry::book(Symbol) -> Book*`, `OrderIndex::find(OrderId) -> Order*`, `RejectReason::UnknownSymbol = 5` consistent across header, implementation, tests, and dispatch prompts.

## Execution Handoff

The PM session executes Tasks 1, 3, 4, 5, 6, 7 inline (test-first). Tasks 2, 8, 9, 10, 11, 12 are subagent dispatches. Task 13 is the closeout.
