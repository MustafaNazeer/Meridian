# Phase 1: Core Data Structures and Single-Symbol Matching Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship the C++20 matching engine's core data structures (`Order`, `Level`, `Book`, `OrderPool`) and a working single-symbol matching loop covering limit, market, and IOC order types with strict price-time priority (FIFO at each price level), driven test-first against a Python reference implementation.

**Architecture:** A single-symbol L3 order book per `Book` instance, holding two ordered structures (`std::map<Price, Level*>`, descending for bids and ascending for asks) of intrusive doubly linked `Level`s, each owning a FIFO queue of `Order`s. All `Order`s are allocated from a fixed-capacity `OrderPool` free-list arena so the matching loop performs zero heap allocations. A `MatchingEngine` consumes `EngineEvent`s (NewOrder for limit/market/IOC, Cancel) and emits `ExecutionReport`s (Acknowledge, Fill, Reject, Cancel). A Python reference implementation under `tests/reference/` mirrors the same semantics in obviously-correct form and is diffed against the C++ engine on every change.

**Tech Stack:** C++20 (`-std=c++20`, `-Wall -Wextra -Wpedantic -Werror`), CMake 3.25 with Ninja, GoogleTest 1.15.2 (already vendored via FetchContent in Phase 0), Python 3.11 (stdlib only, no third-party deps), HDRHistogram-c (for bench skeleton; vendored via FetchContent in this phase).

---

## Context

Phase 0 closed at commit `db0ccff` on `main`. The repo has the CMake skeleton, the Vite/React frontend scaffold, the Twilight design tokens, the Phase 0 threat model, and a green CI workflow. The `include/`, `src/`, `tests/unit/`, `tests/property/`, `tests/integration/`, and `tests/reference/` directories do not yet exist. The matching engine has zero source code on `main` today; this phase ships its first real lines.

This phase is sized at ~95 percent of one Claude Max 5x window. The PM session opened Phase 1 at 2026-05-09 with the user reporting 4 percent usage and 4.75 hours until reset.

## Key decisions captured for Phase 1

These resolutions consolidate ambiguities surfaced when the PM read `SPEC.md`, `docs/plan.md`, the design spec, and the agent briefs together at Phase 1 open. They are documented here in one place so the dispatched agents and the PM session itself work from a consistent contract.

1. **`Book` scope: full per-symbol L3 book containing both bid and ask sides.** The design spec section 5.1 sentence "Book: One side of one symbol's order book" is wrong; it contradicts the `BookRegistry: unordered_map<Symbol, Book>` signature in the same section. `docs/plan.md` Phase 1's "single-symbol L3 book" wording is correct. Task 1 in this plan files a small docs PR amending the design spec.

2. **`Book` ordering structure: `std::map<Price, Level*>` per side.** `docs/plan.md` Phase 1 task 6 reads "two heaps (bids descending, asks ascending) of Level pointers." `std::priority_queue` cannot do in-order traversal, which is required to sweep multiple price levels during matching. The design spec section 5.1's "sorted map of Levels by price (red-black tree initially via std::map)" wording is correct. Task 1 amends `docs/plan.md` to match.

3. **Header file extension: `.hpp`.** `docs/plan.md` Phase 1 uses `.h` ("`include/meridian/types.h`") in a few places; the design spec section 10 and every agent brief use `.hpp`. The project convention is `.hpp`. Task 1 amends `docs/plan.md` to match.

4. **Cancel-by-id is in Phase 1 at the per-`Book` level.** `STATUS.md` and `docs/plan.md` Phase 1 task list do not explicitly list cancel-by-id; the design spec section 5.1 says `Book` has the `unordered_map<OrderId, Order*>` for O(1) cancel; the Reference Implementation Engineer's Phase 1 brief lists "cancel-by-id" as part of the reference. Resolution: Phase 1's `Book` ships the lookup map and a `Book::cancel(OrderId)` method so matching scenarios involving cancel-while-resting can be tested. Phase 2 adds the cross-symbol `BookRegistry` and the cross-book `OrderIndex` that dispatches by `OrderId` without knowing which symbol the order belongs to.

5. **Primitive type widths.** Locked in this plan to remove ambiguity:
   * `OrderId`: `uint64_t`. Standard for HFT order IDs.
   * `Symbol`: `uint16_t`. 65,536 distinct symbols suffices for the five demo symbols in Phase 0 and for any realistic ITCH replay.
   * `Price`: `int32_t`. 32-bit signed fixed-point ticks; signed lets us reserve negative values as sentinels.
   * `Quantity`: `int32_t`. 32-bit signed; max ~2.1B is sufficient for any per-order share count.
   * `Timestamp`: `int64_t`. Nanoseconds since epoch.
   * `Side`: `enum class : uint8_t { Buy, Sell }`.
   * `OrderType`: `enum class : uint8_t { Limit, Market, IOC, PostOnly, FOK }` (PostOnly and FOK declared in Phase 1 but not implemented until Phase 7).

6. **`Order` size budget: target `sizeof(Order) <= 64`.** With the field set above plus two intrusive list pointers (`prev`, `next`), the natural layout is around 48 bytes after alignment, well under the 64-byte cache line. A `static_assert(sizeof(Order) <= 64, ...)` in `types.hpp` enforces this from day one.

7. **`OrderPool` default capacity: 1,048,576 slots (`1 << 20`).** Configurable at construction. 1M slots at ~48 bytes per `Order` is ~48 MB resident, well within the developer machine and the Fly.io free-tier machine size. A 5-minute ITCH replay across the five demo symbols is bounded well under this capacity.

8. **No exceptions on the matching hot path.** `MatchingEngine::apply` and every function it calls return result codes via `ExecutionReport`. Exceptions are reserved for setup paths (config parse, file open, port bind) which Phase 1 does not exercise. The Code Reviewer audits this on every PR.

9. **Engine Developer + Market Microstructure Engineer roles played by the PM session.** Per `CLAUDE.md` "When to dispatch vs play the role yourself" guidance, the matching engine's iterative test-first development is long-lived work that benefits from accumulated context. Tasks 5 through 14 in this plan are executed by the PM session in this conversation. The Quant Domain Validator, Reference Implementation Engineer, QA Engineer, Performance Engineer, Risk Reviewer, Code Reviewer, and Citation Auditor are dispatched as separate `Task` invocations.

## File structure

Files this plan creates or modifies:

| Path | Owner | Purpose |
|---|---|---|
| `docs/superpowers/specs/2026-05-09-meridian-design.md` (modify) | PM | Amend section 5.1 `Book` definition |
| `docs/plan.md` (modify) | PM | Amend Phase 1 wording: `.hpp` extension, "two `std::map`s" not "two heaps", cancel-by-id in Phase 1 scope |
| `docs/risk/matching-semantics.md` (create) | Quant Domain Validator (01) | Worked examples for limit, market, IOC; the contract every other agent builds against |
| `tests/reference/matching_reference.py` (create) | Reference Impl Engineer (18) | Python reference engine, ~300-500 lines, stdlib only |
| `tests/reference/test_reference.py` (create) | Reference Impl Engineer (18) | Worked-example tests for the Python reference |
| `tests/reference/README.md` (create) | Reference Impl Engineer (18) | How to run the reference standalone |
| `docs/perf/budget.md` (create) | Performance Engineer (12) | Latency budget for matching event, sampler tick |
| `include/meridian/types.hpp` (create) | PM as Engine Dev | `OrderId`, `Symbol`, `Price`, `Quantity`, `Timestamp`, `Side`, `OrderType`, `Order`, `EngineEvent` |
| `include/meridian/order_pool.hpp` (create) | PM as Engine Dev | `OrderPool` free-list allocator API |
| `src/order_pool.cpp` (create) | PM as Engine Dev | `OrderPool` impl plus debug-mode allocation tripwire |
| `include/meridian/level.hpp` (create) | PM as Engine Dev | `Level` FIFO queue at one price |
| `src/level.cpp` (create) | PM as Engine Dev | `Level` impl |
| `include/meridian/execution_report.hpp` (create) | PM as Engine Dev | `ExecutionReport` (Acknowledge, Fill, Reject, Cancel) |
| `include/meridian/book.hpp` (create) | PM as Engine Dev | Per-symbol L3 `Book` with bid and ask sides plus `OrderId` lookup |
| `src/book.cpp` (create) | PM as Engine Dev | `Book` impl |
| `include/meridian/matching.hpp` (create) | PM as Engine Dev | `MatchingEngine::apply(EngineEvent)` |
| `src/matching.cpp` (create) | PM as Engine Dev | Limit, market, IOC matching plus cancel |
| `src/CMakeLists.txt` (create) | PM as Engine Dev | Build target for `libmeridian.a` |
| `CMakeLists.txt` (modify) | PM as Engine Dev | Uncomment `add_subdirectory(src)` and the test subdirectory line |
| `tests/CMakeLists.txt` (modify) | PM as Engine Dev | Add `add_subdirectory(unit)` and `add_subdirectory(integration)` |
| `tests/unit/CMakeLists.txt` (create) | PM as Engine Dev / QA | Build targets for unit tests |
| `tests/unit/test_order_pool.cpp` (create) | QA Engineer (10) (paired) | Unit test for `OrderPool` |
| `tests/unit/test_level.cpp` (create) | QA Engineer (10) (paired) | Unit test for `Level` |
| `tests/unit/test_book.cpp` (create) | QA Engineer (10) (paired) | Unit test for `Book` |
| `tests/unit/test_matching.cpp` (create) | QA Engineer (10) (paired) | Unit tests for limit, market, IOC matching plus cancel |
| `tests/integration/CMakeLists.txt` (create) | QA Engineer (10) | Build target for integration test |
| `tests/integration/test_engine_vs_reference.cpp` (create) | QA Engineer (10) | C++ engine vs Python reference diff harness |
| `apps/bench/main.cpp` (create) | Performance Engineer (12) | Bench skeleton with HDRHistogram wiring |
| `apps/bench/CMakeLists.txt` (modify) | Performance Engineer (12) | Add `add_executable(meridian-bench main.cpp)` and link |
| `docs/qa/regression-checklist.md` (create) | QA Engineer (10) | Manual smoke test run before each phase closes |
| `docs/risk/audit-cases.md` (create) | Risk Reviewer (16) | 10 hand-audited cases with C++ output and Python reference output side by side |
| `docs/audits/citation-audit-phase-1-2026-05-09.md` (create) | Citation Auditor (17) | Audit of Phase 1 docs |

## Dispatch order

Phase 1 runs in three stages. Within a stage, tasks may run in parallel.

* **Stage A: Specification.** Quant Domain Validator first (Task 2). Reference Implementation Engineer (Task 3) and Performance Engineer (Task 4) follow once `matching-semantics.md` exists. Stage A produces the contract documents the C++ implementation builds against.
* **Stage B: C++ implementation, test-first.** PM session as Engine Developer + Market Microstructure Engineer drives Tasks 5 through 14. QA Engineer paired alongside (Task 15) writes unit tests test-first.
* **Stage C: Quality gates.** Risk Reviewer (Task 16), Citation Auditor (Task 17), Code Reviewer (Task 18). Phase close (Task 19).

---

## Task 1: PM amendments to design spec and `docs/plan.md`

**Files:**
- Modify: `docs/superpowers/specs/2026-05-09-meridian-design.md` (section 5.1 `Book` definition)
- Modify: `docs/plan.md` (Phase 1 outputs table, Phase 1 task 6 wording)

- [ ] **Step 1: Amend the design spec section 5.1 `Book` definition**

In `docs/superpowers/specs/2026-05-09-meridian-design.md`, replace the current `Book` paragraph:

```
**Book**
One side (bid or ask) of one symbol's order book. Sorted map of Levels by price (red-black tree initially via std::map; consider a flat skip list later). Plus an `unordered_map<OrderId, Order*>` for O(1) cancel.
```

with:

```
**Book**
Full L3 book for one symbol: holds both bid and ask sides. Each side is a `std::map<Price, Level*>` ordered by price (red-black tree initially via std::map; consider a flat skip list later). Bids are walked in descending price order; asks in ascending price order. Plus an `unordered_map<OrderId, Order*>` for O(1) cancel within this symbol.
```

This matches the `BookRegistry: unordered_map<Symbol, Book>` signature in the same section.

- [ ] **Step 2: Amend `docs/plan.md` Phase 1 outputs table to use `.hpp`**

In the Phase 1 outputs table at `docs/plan.md`, change every `.h` to `.hpp` for headers under `include/meridian/`:

* `include/meridian/types.h` to `include/meridian/types.hpp`
* `include/meridian/order_pool.h` to `include/meridian/order_pool.hpp`
* `include/meridian/level.h` to `include/meridian/level.hpp`
* `include/meridian/book.h` to `include/meridian/book.hpp`
* `include/meridian/matching.h` to `include/meridian/matching.hpp`
* `include/meridian/execution_report.h` to `include/meridian/execution_report.hpp`

- [ ] **Step 3: Amend `docs/plan.md` Phase 1 task 6 wording**

Replace task 6:

```
6. Implement `Book` with two heaps (bids descending, asks ascending) of `Level` pointers. Test top-of-book read and update.
```

with:

```
6. Implement `Book` as a per-symbol L3 book holding both bid and ask sides. Each side is a `std::map<Price, Level*>` (bids ordered descending, asks ascending). The book also owns an `unordered_map<OrderId, Order*>` for O(1) cancel within the symbol. Test top-of-book read and update plus cancel-by-id.
```

- [ ] **Step 4: Add the cancel-by-id resolution note to `docs/plan.md` working-notes section**

Append to `docs/plan.md` "Working notes and decisions log" section, in chronological order:

```
* **2026-05-09 (Phase 1 open)**: Book ships cancel-by-id at the per-symbol level
  in Phase 1. The cross-symbol BookRegistry plus OrderIndex layer is still Phase 2.
  Reference: docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
  key decision 4.
```

- [ ] **Step 5: Commit**

```bash
git checkout -b phase-1-companion-plan-and-amendments
git add docs/plan.md docs/superpowers/specs/2026-05-09-meridian-design.md \
        docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
git commit -m "phase 1: companion plan plus design-spec and plan.md amendments

Resolves Book scope ambiguity (full per-symbol L3 book, both sides), Book
ordering structure (std::map per side, not heap), header file extension
(.hpp), and cancel-by-id placement (per-Book in Phase 1; cross-symbol
OrderIndex still Phase 2)."
```

---

## Task 2: Dispatch Quant Domain Validator (01) for `matching-semantics.md`

**Files:**
- Create (by dispatched agent): `docs/risk/matching-semantics.md`

This task is a single dispatch via the `Task` tool with `subagent_type: "general-purpose"`. The dispatched agent reads its brief and produces the document.

- [ ] **Step 1: Create the `docs/risk/` directory placeholder**

```bash
mkdir -p /home/mustafa/src/Meridian/docs/risk
```

- [ ] **Step 2: Dispatch the agent**

Send a `Task` invocation with this prompt body:

```
You are the Quant Domain Validator agent for the Meridian project at
/home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/SPEC.md
- /home/mustafa/src/Meridian/agents/01-quant-domain-validator.md
- /home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md
  (especially section 5 Components, section 7 Error handling, section 8.2 the
  invariant list)
- /home/mustafa/src/Meridian/docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
  (this companion plan; its "Key decisions captured for Phase 1" section is the
  PM's resolution of cross-document ambiguities)

Execute the Phase 1 tasks listed in your agent file. Specifically:

1. Write /home/mustafa/src/Meridian/docs/risk/matching-semantics.md covering
   limit, market, and IOC behavior. Include at least 6 worked examples per
   order type, with arrival sequences and expected fills. Each worked example
   uses concrete OrderIds, prices (integer ticks), and quantities, and shows
   the resulting ExecutionReport stream and the post-event book state.

2. Document the price-time priority tie-breaking rule (FIFO at each price
   level: the order resting longer at a given price trades first against an
   incoming aggressor).

3. Document the conservation law: for any order, submitted_qty equals
   filled_qty plus resting_qty plus cancelled_qty.

4. Document IOC fill-or-cancel semantics: an IOC fills as much as it can
   against the resting book on the opposite side at prices favorable to it,
   and the residual is cancelled (never rests).

5. Document market order semantics: a market order takes liquidity at any
   available price on the opposite side. If the opposite side is empty, the
   order is rejected with reason `EmptyBook`. If the order partially fills,
   the residual is cancelled (never rests).

6. Document cancel-by-id semantics: cancelling a known resting order removes
   it from its Level and emits a Cancel report. Cancelling an unknown order
   ID emits a Reject with reason `NotFound`. Cancelling an order that has
   already been fully filled also emits Reject reason NotFound (the order is
   no longer in the book).

7. List the 5 Phase 1 invariants (price-time priority, quantity conservation,
   no double fill, no lost orders, IOC never rests) in a checklist that the
   QA Engineer will turn into property tests in Phase 3. Mark spread-non-
   negative, cancel-idempotence, FOK-all-or-nothing, post-only-never-crosses,
   and top-of-book-monotonicity as "deferred to later phases" so QA does not
   try to test them in Phase 1.

8. Cite specific sections of the textbook references in your agent brief
   (Harris "Trading and Exchanges", O'Hara "Market Microstructure Theory")
   for the price-time priority rule and the IOC semantics. Cite the NASDAQ
   ITCH 5.0 specification for any wording you adopt verbatim. Every citation
   must be specific enough that the Citation and Fact Auditor can verify it
   in Phase 1's audit pass.

Stay strictly in the role of Quant Domain Validator; do not write C++ code,
do not write the Python reference, do not write unit tests. Route those needs
back to the Project Manager session that dispatched you.

When you are done, report:
1. The file path you produced (docs/risk/matching-semantics.md).
2. Any decisions you made that the PM should review (especially any worked
   examples where the textbook references give multiple acceptable behaviors).
3. The next agent in the handoff chain per your agent file (Reference
   Implementation Engineer for the Python reference, then QA Engineer for
   property test interpretation).
```

- [ ] **Step 3: Review the agent's output**

Read `/home/mustafa/src/Meridian/docs/risk/matching-semantics.md` in full when the agent reports done. Verify:

* Six or more worked examples per order type.
* Each worked example has concrete numbers, an event sequence, and an expected output.
* The five Phase 1 invariants are listed as a checklist.
* Citations are specific (named book or specification, named section).
* The deferred invariants are explicitly marked deferred.

If anything is missing, re-prompt the same agent with a specific correction. Do not accept vague output.

- [ ] **Step 4: Commit**

```bash
git add docs/risk/matching-semantics.md
git commit -m "phase 1: matching-semantics conventions document

Quant Domain Validator pass. Limit, market, IOC worked examples plus the
five Phase 1 invariants. Reference Implementation Engineer and Engine
Developer build against this document."
```

---

## Task 3: Dispatch Reference Implementation Engineer (18) for the Python reference

**Files:**
- Create (by dispatched agent): `tests/reference/matching_reference.py`
- Create (by dispatched agent): `tests/reference/test_reference.py`
- Create (by dispatched agent): `tests/reference/README.md`

Run after Task 2 completes (the agent reads `matching-semantics.md`).

- [ ] **Step 1: Create the `tests/reference/` directory placeholder**

```bash
mkdir -p /home/mustafa/src/Meridian/tests/reference
```

- [ ] **Step 2: Dispatch the agent**

```
You are the Reference Implementation Engineer agent for the Meridian project
at /home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/SPEC.md
- /home/mustafa/src/Meridian/agents/18-reference-implementation-engineer.md
- /home/mustafa/src/Meridian/docs/risk/matching-semantics.md (the Quant Domain
  Validator's worked examples; the reference must reproduce every example)
- /home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md
  section 8.2 (the matching invariants)
- /home/mustafa/src/Meridian/docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
  (this companion plan)

Execute the Phase 1 tasks listed in your agent file. Specifically:

1. Implement /home/mustafa/src/Meridian/tests/reference/matching_reference.py:
   a Python single-symbol matching engine with limit, market, IOC, and
   cancel-by-id. FIFO at each price level. About 300 to 500 lines including
   docstrings.

   Constraints:
   - Python 3.11 stdlib only. No numpy, no pandas, no pydantic.
   - Use dataclasses and enum.
   - Integer prices and integer quantities (no floats).
   - Deterministic: same input always produces byte-identical output.
   - Optimize for obvious correctness, not performance.

   Public API:
       MatchingReference(symbol: int)
           .submit_limit(order_id, side, price, qty, ts) -> list[ExecutionReport]
           .submit_market(order_id, side, qty, ts) -> list[ExecutionReport]
           .submit_ioc(order_id, side, price, qty, ts) -> list[ExecutionReport]
           .cancel(order_id) -> list[ExecutionReport]
           .top_of_book() -> tuple[best_bid_px, best_bid_qty, best_ask_px, best_ask_qty]

   ExecutionReport is a dataclass with fields: kind (ack/fill/reject/cancel),
   order_id, side, price, qty, ts, plus optional reason for reject. Serialize
   to JSON via dataclasses.asdict for the integration-test diff later.

2. Implement /home/mustafa/src/Meridian/tests/reference/test_reference.py:
   pytest-style unit tests covering every worked example in
   docs/risk/matching-semantics.md. The reference passing every worked example
   is the bar.

   Use the standard library's unittest if you prefer (the project does not
   require pytest). The tests must be runnable as
   `python -m unittest discover tests/reference` from the repo root.

3. Write /home/mustafa/src/Meridian/tests/reference/README.md explaining:
   - How to run the reference standalone.
   - How to run the test suite.
   - Why the reference is intentionally slow and obviously correct (citing
     this companion plan's "obvious over fast" constraint).
   - Which spec section each rule cites (link to matching-semantics.md
     anchors).

Stay strictly in the role of Reference Implementation Engineer. Do not write
C++ code. Do not write the matching-semantics document (that is the Quant
Domain Validator's; you are reading from it). Do not invent semantics that
are not in matching-semantics.md; if a corner case is missing from the
conventions document, route the question back to the PM.

When you are done, report:
1. The file paths you produced (matching_reference.py, test_reference.py,
   README.md).
2. The test count (e.g., "31 tests, all pass").
3. Any decisions you made that the PM should review (especially any worked
   example you could not reproduce because the conventions document was
   ambiguous).
```

- [ ] **Step 3: Verify**

```bash
cd /home/mustafa/src/Meridian
python3 -m unittest discover tests/reference -v
```

Expected: every test passes.

- [ ] **Step 4: Commit**

```bash
git add tests/reference/matching_reference.py tests/reference/test_reference.py tests/reference/README.md
git commit -m "phase 1: Python reference matching engine

Single-symbol limit, market, IOC, cancel-by-id. Stdlib only. Optimized for
obvious correctness; the C++ engine will be diffed against this reference on
every change."
```

---

## Task 4: Dispatch Performance Engineer (12) for `docs/perf/budget.md` and bench scaffold

**Files:**
- Create (by dispatched agent): `docs/perf/budget.md`
- Create (by dispatched agent): `apps/bench/main.cpp`
- Modify (by dispatched agent): `apps/bench/CMakeLists.txt`
- Modify (by dispatched agent): `CMakeLists.txt` (uncomment `add_subdirectory(apps/bench)`)

May run in parallel with Task 3 once Task 2 ships. The budget document does not depend on the reference; the bench scaffold depends only on `libmeridian.a` linkability, which Task 5 onward provides. Sequence the dispatch so the bench scaffold work starts after Task 14 lands.

- [ ] **Step 1: Dispatch the agent for `budget.md` (no dependency on C++ code)**

```
You are the Performance Engineer agent for the Meridian project at
/home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/agents/12-performance-engineer.md
- /home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md
  section 2.3 (latency targets) and section 8.4 (benchmark regression)

Phase 1 task 1 in your agent file: write /home/mustafa/src/Meridian/docs/perf/budget.md
with the initial latency budget:

  Matching event:        p50 <= 500 ns, p99 <= 2 us, p99.9 <= 5 us
  Top-of-book read:      p50 <= 50 ns
  Sampler tick build:    < 1 ms at 30 Hz

For each entry, name the design-spec section it traces to and the phase that
will measure it (the matching event budget is measured in Phase 5; the top-
of-book read is measured in Phase 4 once the seqlock lands; the sampler tick
build is measured in Phase 4).

Add a "How this budget is enforced" section explaining that Phase 5 wires
the bench regression workflow and Phase 4 wires the TSAN tests; Phase 1
ships only the budget document and the bench skeleton (no targets enforced
yet).

Cite section numbers in the design spec for every number you publish. The
Citation and Fact Auditor will verify these citations in Phase 1's audit
pass.

Stay strictly in the role of Performance Engineer. Do not write the C++
matching engine. Do not write unit tests. Do not invent latency numbers that
are not in the design spec.

Report the file path you produced and any decisions the PM should review.
```

- [ ] **Step 2: Dispatch the agent for the bench scaffold (after Task 14)**

Once `libmeridian.a` is linkable from Tasks 5 through 14, send a follow-up
dispatch:

```
You are the Performance Engineer agent (continuation). Phase 1 task 2: pair
with the Engine Developer (the PM session) on /home/mustafa/src/Meridian/apps/bench/main.cpp.

The bench skeleton's job in Phase 1 is to compile and run end to end, not to
hit a throughput target. Tasks:

1. Write apps/bench/main.cpp that:
   a. Constructs an OrderPool with 1M slots.
   b. Constructs one Book for symbol id 1.
   c. Generates 1,000,000 synthetic limit orders alternating Buy and Sell at
      prices around a fixed midpoint (e.g., midpoint=10000, +- random offset
      in [0, 50] ticks, qty in [1, 100]). Use a deterministic seed.
   d. Calls MatchingEngine::apply on each event in a tight loop. Captures
      the per-event latency in nanoseconds via a steady_clock difference.
   e. Records the latencies in an HDRHistogram (vendor HDRHistogram-c via
      CMake FetchContent in this task; see step 3).
   f. At end of run, prints a Markdown summary table: throughput
      (events/sec), p50/p90/p99/p99.9/max latency (ns), max heap allocations
      observed (must be 0).

2. Modify /home/mustafa/src/Meridian/apps/bench/CMakeLists.txt to add an
   add_executable target plus the link to meridian (libmeridian.a) and to the
   HDRHistogram-c library.

3. Modify /home/mustafa/src/Meridian/CMakeLists.txt: uncomment
   add_subdirectory(apps/bench). Also add a FetchContent_Declare for
   HDRHistogram-c (https://github.com/HdrHistogram/HdrHistogram_c.git, git
   tag 0.11.8 released 2024-04-04). Lock the tag; update only with explicit
   DevOps and Code Reviewer sign-off, per the GoogleTest pinning policy.

4. Verify the bench compiles and runs:
   cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --target meridian-bench
   ./build/apps/bench/meridian-bench --events 1000000

   Expected: prints a table; the throughput number is informational and not
   yet a target.

5. Append the resulting throughput and percentiles to docs/perf/budget.md
   under a new "Phase 1 baseline (informational, not enforced)" subsection,
   labeled clearly as not-yet-targets so the Citation and Fact Auditor does
   not flag the numbers as drifting from the formal targets.

Do not introduce optimizations. Do not change the matching engine. The
bench's job in Phase 1 is to be a working harness, not to hit 6M events/sec.

Report file paths produced and any decisions the PM should review.
```

- [ ] **Step 3: Verify and commit**

After both dispatches complete:

```bash
cd /home/mustafa/src/Meridian
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target meridian-bench
./build/apps/bench/meridian-bench --events 1000000

git add docs/perf/budget.md apps/bench/main.cpp apps/bench/CMakeLists.txt CMakeLists.txt
git commit -m "phase 1: latency budget plus meridian-bench skeleton

Performance Engineer pass. budget.md publishes the matching event, top-of-
book, and sampler tick targets cited from the design spec section 2.3.
meridian-bench compiles and runs end to end against libmeridian.a; the
informational baseline is recorded in budget.md without being treated as a
target. The 6M events/sec headline target is enforced in Phase 5."
```

---

## Task 5: Implement `include/meridian/types.hpp`

**Files:**
- Create: `include/meridian/types.hpp`

The PM session executes this task. No tests yet for the type definitions alone; the unit tests in subsequent tasks exercise the types via the data structures that use them.

- [ ] **Step 1: Create the `include/meridian/` directory**

```bash
mkdir -p /home/mustafa/src/Meridian/include/meridian
```

- [ ] **Step 2: Write `include/meridian/types.hpp`**

```cpp
// include/meridian/types.hpp
#pragma once

#include <cstdint>

namespace meridian {

using OrderId = std::uint64_t;
using Symbol = std::uint16_t;
using Price = std::int32_t;
using Quantity = std::int32_t;
using Timestamp = std::int64_t;

constexpr Price kInvalidPrice = -1;
constexpr Quantity kInvalidQuantity = -1;
constexpr OrderId kInvalidOrderId = 0;

enum class Side : std::uint8_t {
    Buy = 0,
    Sell = 1,
};

enum class OrderType : std::uint8_t {
    Limit = 0,
    Market = 1,
    IOC = 2,
    PostOnly = 3,
    FOK = 4,
};

struct Order {
    OrderId id;
    Symbol symbol;
    Side side;
    OrderType type;
    Price price;
    Quantity qty_remaining;
    Timestamp ts_arrival;
    Order* prev;
    Order* next;
};

static_assert(sizeof(Order) <= 64,
              "Order must fit in one 64-byte cache line");

enum class EventKind : std::uint8_t {
    NewOrder = 0,
    Cancel = 1,
};

struct EngineEvent {
    EventKind kind;
    Symbol symbol;
    Timestamp ts;
    OrderId order_id;
    Side side;
    OrderType type;
    Price price;
    Quantity qty;
};

}  // namespace meridian
```

- [ ] **Step 3: Verify the file compiles standalone**

Add a temporary smoke check to confirm `static_assert` holds. The proper test will arrive when `OrderPool` lands. For now:

```bash
cd /home/mustafa/src/Meridian
echo '#include "meridian/types.hpp"
int main() { return 0; }' | g++ -x c++ -std=c++20 -Iinclude -o /tmp/types_smoke -
echo "exit=$?"
rm -f /tmp/types_smoke
```

Expected: `exit=0`.

- [ ] **Step 4: Commit**

```bash
git add include/meridian/types.hpp
git commit -m "phase 1: primitive types plus Order and EngineEvent

OrderId, Symbol, Price, Quantity, Timestamp aliases. Side and OrderType
enums. Order struct with intrusive list pointers, static_assert size <= 64
bytes. EngineEvent with kind tag for NewOrder and Cancel."
```

---

## Task 6: Implement `OrderPool` with debug-mode allocation tripwire

**Files:**
- Create: `include/meridian/order_pool.hpp`
- Create: `src/order_pool.cpp`
- Create: `src/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Create: `tests/unit/CMakeLists.txt`
- Create: `tests/unit/test_order_pool.cpp`
- Modify: `tests/CMakeLists.txt`

This task wires up the build system end to end. After this task the test framework runs.

- [ ] **Step 1: Write the failing test `tests/unit/test_order_pool.cpp`**

```cpp
// tests/unit/test_order_pool.cpp
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

TEST(OrderPoolTest, AcquireReturnsNonNullSlotsUpToCapacity) {
    OrderPool pool(4);
    Order* a = pool.acquire();
    Order* b = pool.acquire();
    Order* c = pool.acquire();
    Order* d = pool.acquire();
    EXPECT_NE(a, nullptr);
    EXPECT_NE(b, nullptr);
    EXPECT_NE(c, nullptr);
    EXPECT_NE(d, nullptr);
    EXPECT_NE(a, b);
    EXPECT_NE(b, c);
    EXPECT_NE(c, d);
}

TEST(OrderPoolTest, AcquireBeyondCapacityReturnsNullptr) {
    OrderPool pool(2);
    EXPECT_NE(pool.acquire(), nullptr);
    EXPECT_NE(pool.acquire(), nullptr);
    EXPECT_EQ(pool.acquire(), nullptr);
}

TEST(OrderPoolTest, ReleaseReturnsSlotToPool) {
    OrderPool pool(1);
    Order* a = pool.acquire();
    ASSERT_NE(a, nullptr);
    EXPECT_EQ(pool.acquire(), nullptr);
    pool.release(a);
    Order* b = pool.acquire();
    EXPECT_NE(b, nullptr);
    EXPECT_EQ(b, a);
}

TEST(OrderPoolTest, InUseCountTracksAcquiresAndReleases) {
    OrderPool pool(8);
    EXPECT_EQ(pool.in_use(), 0u);
    Order* a = pool.acquire();
    Order* b = pool.acquire();
    EXPECT_EQ(pool.in_use(), 2u);
    pool.release(a);
    EXPECT_EQ(pool.in_use(), 1u);
    pool.release(b);
    EXPECT_EQ(pool.in_use(), 0u);
}

}  // namespace
}  // namespace meridian
```

- [ ] **Step 2: Wire up `src/CMakeLists.txt`**

```cmake
# src/CMakeLists.txt
add_library(meridian STATIC
    order_pool.cpp
)

target_include_directories(meridian PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_compile_features(meridian PUBLIC cxx_std_20)

target_compile_options(meridian PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)
```

- [ ] **Step 3: Wire up `tests/unit/CMakeLists.txt`**

```cmake
# tests/unit/CMakeLists.txt
add_executable(test_order_pool test_order_pool.cpp)

target_link_libraries(test_order_pool PRIVATE
    meridian
    GTest::gtest_main
)

target_compile_options(test_order_pool PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

gtest_discover_tests(test_order_pool)
```

- [ ] **Step 4: Update `tests/CMakeLists.txt`**

Replace the stub content of `tests/CMakeLists.txt` with:

```cmake
# Meridian test suite.
add_subdirectory(unit)
# add_subdirectory(integration)  # uncomment when integration test lands in Task 15
```

- [ ] **Step 5: Update root `CMakeLists.txt`**

In `CMakeLists.txt`, uncomment `add_subdirectory(src)` and the test subdirectory block:

```cmake
# Replace:
# add_subdirectory(src)
# with:
add_subdirectory(src)

# Replace:
# if(MERIDIAN_BUILD_TESTS)
#     add_subdirectory(tests)
# endif()
# with:
if(MERIDIAN_BUILD_TESTS)
    add_subdirectory(tests)
endif()
```

- [ ] **Step 6: Run the test to verify it fails (no impl yet)**

```bash
cd /home/mustafa/src/Meridian
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_order_pool 2>&1 | tail -20
```

Expected: build fails because `meridian/order_pool.hpp` does not exist yet.

- [ ] **Step 7: Write the OrderPool header**

```cpp
// include/meridian/order_pool.hpp
#pragma once

#include "meridian/types.hpp"

#include <cstddef>
#include <memory>
#include <vector>

namespace meridian {

class OrderPool {
public:
    explicit OrderPool(std::size_t capacity);
    ~OrderPool();

    OrderPool(const OrderPool&) = delete;
    OrderPool& operator=(const OrderPool&) = delete;

    // Returns a pointer to a zeroed Order slot, or nullptr if the pool is
    // exhausted. Hot path; never throws, never allocates.
    Order* acquire() noexcept;

    // Returns the slot to the free list. Requires that order was previously
    // returned from this pool's acquire(). Hot path; never throws, never
    // allocates.
    void release(Order* order) noexcept;

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }
    [[nodiscard]] std::size_t in_use() const noexcept { return in_use_; }

private:
    std::size_t capacity_;
    std::size_t in_use_;
    std::unique_ptr<Order[]> storage_;
    Order* free_head_;
};

}  // namespace meridian
```

- [ ] **Step 8: Write the OrderPool implementation**

```cpp
// src/order_pool.cpp
#include "meridian/order_pool.hpp"

namespace meridian {

OrderPool::OrderPool(std::size_t capacity)
    : capacity_(capacity),
      in_use_(0),
      storage_(std::make_unique<Order[]>(capacity)),
      free_head_(nullptr) {
    for (std::size_t i = 0; i < capacity_; ++i) {
        storage_[i].next = free_head_;
        free_head_ = &storage_[i];
    }
}

OrderPool::~OrderPool() = default;

Order* OrderPool::acquire() noexcept {
    if (free_head_ == nullptr) {
        return nullptr;
    }
    Order* slot = free_head_;
    free_head_ = slot->next;
    slot->prev = nullptr;
    slot->next = nullptr;
    ++in_use_;
    return slot;
}

void OrderPool::release(Order* order) noexcept {
    order->prev = nullptr;
    order->next = free_head_;
    free_head_ = order;
    --in_use_;
}

}  // namespace meridian
```

- [ ] **Step 9: Build and run tests; expect pass**

```bash
cd /home/mustafa/src/Meridian
cmake --build build --target test_order_pool
ctest --test-dir build --output-on-failure -R OrderPool
```

Expected: 4 tests, all pass.

- [ ] **Step 10: Add the debug-mode allocation tripwire**

Append to `src/order_pool.cpp`:

```cpp
#ifndef NDEBUG
#include <atomic>
#include <cstdio>
#include <cstdlib>

namespace meridian::debug_alloc {

// When the matching loop is active, set this flag to abort on any heap
// allocation. The Engine Developer wraps the matching loop with a
// HotPathGuard RAII helper (added in Task 10) that flips the flag on entry
// and off on exit.
std::atomic<bool> hot_path_active{false};

}  // namespace meridian::debug_alloc

void* operator new(std::size_t size) {
    if (meridian::debug_alloc::hot_path_active.load(std::memory_order_relaxed)) {
        std::fprintf(stderr,
                     "FATAL: heap allocation of %zu bytes during matching hot path\n",
                     size);
        std::abort();
    }
    void* p = std::malloc(size);
    if (p == nullptr) {
        throw std::bad_alloc{};
    }
    return p;
}

void operator delete(void* p) noexcept {
    std::free(p);
}

void operator delete(void* p, std::size_t) noexcept {
    std::free(p);
}
#endif
```

The release build does not include the override (NDEBUG is defined). Debug builds abort on any heap allocation between the matching loop's entry and exit. The `HotPathGuard` helper is added in Task 10.

- [ ] **Step 11: Build and re-run tests; expect pass**

```bash
cd /home/mustafa/src/Meridian
cmake --build build --target test_order_pool
ctest --test-dir build --output-on-failure -R OrderPool
```

Expected: 4 tests, all pass. The override is dormant (the flag is false outside the matching loop), so unit tests are unaffected.

- [ ] **Step 12: Commit**

```bash
git add include/meridian/order_pool.hpp src/order_pool.cpp src/CMakeLists.txt \
        tests/unit/CMakeLists.txt tests/unit/test_order_pool.cpp \
        tests/CMakeLists.txt CMakeLists.txt
git commit -m "phase 1: OrderPool free-list arena plus debug allocation tripwire

Fixed-capacity arena, O(1) acquire/release, no heap allocation on the hot
path. Debug-build operator new override aborts on any malloc/new while
debug_alloc::hot_path_active is true. The hot-path guard wrapping the
matching loop arrives in Task 10."
```

---

## Task 7: Implement `Level` (FIFO queue at one price)

**Files:**
- Create: `include/meridian/level.hpp`
- Create: `src/level.cpp`
- Modify: `src/CMakeLists.txt` (add `level.cpp`)
- Create: `tests/unit/test_level.cpp`
- Modify: `tests/unit/CMakeLists.txt` (add `test_level` target)

- [ ] **Step 1: Write the failing test `tests/unit/test_level.cpp`**

```cpp
// tests/unit/test_level.cpp
#include "meridian/level.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

Order* make_order(OrderPool& pool, OrderId id, Quantity qty, Price price = 100) {
    Order* o = pool.acquire();
    o->id = id;
    o->symbol = 1;
    o->side = Side::Buy;
    o->type = OrderType::Limit;
    o->price = price;
    o->qty_remaining = qty;
    o->ts_arrival = 0;
    return o;
}

TEST(LevelTest, EmptyLevelHasZeroTotalsAndNoHead) {
    Level level{100};
    EXPECT_EQ(level.price(), 100);
    EXPECT_EQ(level.total_qty(), 0);
    EXPECT_EQ(level.order_count(), 0u);
    EXPECT_EQ(level.head(), nullptr);
}

TEST(LevelTest, AppendInsertsAtTailAndUpdatesTotals) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    Order* b = make_order(pool, 2, 25);
    level.append(a);
    level.append(b);

    EXPECT_EQ(level.head(), a);
    EXPECT_EQ(a->next, b);
    EXPECT_EQ(b->prev, a);
    EXPECT_EQ(b->next, nullptr);
    EXPECT_EQ(level.total_qty(), 35);
    EXPECT_EQ(level.order_count(), 2u);
}

TEST(LevelTest, RemoveHeadAdvancesAndDecrementsTotals) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    Order* b = make_order(pool, 2, 25);
    level.append(a);
    level.append(b);

    level.remove(a);

    EXPECT_EQ(level.head(), b);
    EXPECT_EQ(b->prev, nullptr);
    EXPECT_EQ(level.total_qty(), 25);
    EXPECT_EQ(level.order_count(), 1u);
}

TEST(LevelTest, RemoveMiddleSplicesAndDecrementsTotals) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    Order* b = make_order(pool, 2, 25);
    Order* c = make_order(pool, 3, 5);
    level.append(a);
    level.append(b);
    level.append(c);

    level.remove(b);

    EXPECT_EQ(level.head(), a);
    EXPECT_EQ(a->next, c);
    EXPECT_EQ(c->prev, a);
    EXPECT_EQ(level.total_qty(), 15);
    EXPECT_EQ(level.order_count(), 2u);
}

TEST(LevelTest, AdjustQuantityUpdatesTotalQty) {
    OrderPool pool(4);
    Level level{100};

    Order* a = make_order(pool, 1, 10);
    level.append(a);

    level.adjust_qty(a, /*new_remaining=*/3);

    EXPECT_EQ(a->qty_remaining, 3);
    EXPECT_EQ(level.total_qty(), 3);
}

}  // namespace
}  // namespace meridian
```

- [ ] **Step 2: Add `test_level` to `tests/unit/CMakeLists.txt`**

```cmake
# Append:
add_executable(test_level test_level.cpp)
target_link_libraries(test_level PRIVATE meridian GTest::gtest_main)
target_compile_options(test_level PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_level)
```

- [ ] **Step 3: Run; expect fail**

```bash
cmake --build build --target test_level 2>&1 | tail -10
```

Expected: build fails because `meridian/level.hpp` does not exist.

- [ ] **Step 4: Write `include/meridian/level.hpp`**

```cpp
// include/meridian/level.hpp
#pragma once

#include "meridian/types.hpp"

#include <cstddef>

namespace meridian {

class Level {
public:
    explicit Level(Price price) noexcept;

    [[nodiscard]] Price price() const noexcept { return price_; }
    [[nodiscard]] Quantity total_qty() const noexcept { return total_qty_; }
    [[nodiscard]] std::size_t order_count() const noexcept { return order_count_; }
    [[nodiscard]] Order* head() const noexcept { return head_; }
    [[nodiscard]] Order* tail() const noexcept { return tail_; }
    [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

    // Appends order at the FIFO tail; updates totals.
    void append(Order* order) noexcept;

    // Splices order out of the list; updates totals. order must currently
    // be in this level.
    void remove(Order* order) noexcept;

    // Sets order->qty_remaining to new_remaining and updates total_qty.
    // order must currently be in this level.
    void adjust_qty(Order* order, Quantity new_remaining) noexcept;

private:
    Price price_;
    Quantity total_qty_;
    std::size_t order_count_;
    Order* head_;
    Order* tail_;
};

}  // namespace meridian
```

- [ ] **Step 5: Write `src/level.cpp`**

```cpp
// src/level.cpp
#include "meridian/level.hpp"

namespace meridian {

Level::Level(Price price) noexcept
    : price_(price),
      total_qty_(0),
      order_count_(0),
      head_(nullptr),
      tail_(nullptr) {}

void Level::append(Order* order) noexcept {
    order->prev = tail_;
    order->next = nullptr;
    if (tail_ != nullptr) {
        tail_->next = order;
    } else {
        head_ = order;
    }
    tail_ = order;
    total_qty_ += order->qty_remaining;
    ++order_count_;
}

void Level::remove(Order* order) noexcept {
    if (order->prev != nullptr) {
        order->prev->next = order->next;
    } else {
        head_ = order->next;
    }
    if (order->next != nullptr) {
        order->next->prev = order->prev;
    } else {
        tail_ = order->prev;
    }
    total_qty_ -= order->qty_remaining;
    --order_count_;
    order->prev = nullptr;
    order->next = nullptr;
}

void Level::adjust_qty(Order* order, Quantity new_remaining) noexcept {
    total_qty_ += (new_remaining - order->qty_remaining);
    order->qty_remaining = new_remaining;
}

}  // namespace meridian
```

- [ ] **Step 6: Add `level.cpp` to `src/CMakeLists.txt`**

```cmake
# src/CMakeLists.txt: change
add_library(meridian STATIC
    order_pool.cpp
    level.cpp
)
```

- [ ] **Step 7: Build and run; expect pass**

```bash
cmake --build build --target test_level
ctest --test-dir build --output-on-failure -R Level
```

Expected: 5 tests, all pass.

- [ ] **Step 8: Commit**

```bash
git add include/meridian/level.hpp src/level.cpp src/CMakeLists.txt \
        tests/unit/test_level.cpp tests/unit/CMakeLists.txt
git commit -m "phase 1: Level FIFO queue at one price

Doubly linked list of Orders with O(1) append-at-tail (FIFO discipline),
O(1) remove (intrusive list pointer fix-up), O(1) qty adjust. Owns
total_qty and order_count, both updated on every mutation."
```

---

## Task 8: Implement `ExecutionReport`

**Files:**
- Create: `include/meridian/execution_report.hpp`

This is a pure data type. No `.cpp`; no test file of its own (consumed by the matching tests).

- [ ] **Step 1: Write `include/meridian/execution_report.hpp`**

```cpp
// include/meridian/execution_report.hpp
#pragma once

#include "meridian/types.hpp"

namespace meridian {

enum class ReportKind : std::uint8_t {
    Acknowledge = 0,
    Fill = 1,
    Cancel = 2,
    Reject = 3,
};

enum class RejectReason : std::uint8_t {
    None = 0,
    EmptyBook = 1,         // market order against empty side
    NotFound = 2,          // cancel for unknown order id
    InsufficientLiquidity = 3,  // FOK that cannot fully fill (Phase 7)
    WouldCross = 4,        // post-only that would trade (Phase 7)
};

struct ExecutionReport {
    ReportKind kind;
    OrderId order_id;        // for fills, the aggressor's id
    OrderId matched_id;      // for fills, the resting counterparty's id; 0 otherwise
    Side side;
    Price price;
    Quantity qty;
    Timestamp ts;
    RejectReason reject_reason;
};

}  // namespace meridian
```

- [ ] **Step 2: Commit**

```bash
git add include/meridian/execution_report.hpp
git commit -m "phase 1: ExecutionReport plus report kinds and reject reasons

Pure data type consumed by the matching engine. Phase 7 reject reasons
(InsufficientLiquidity, WouldCross) declared but unused until Phase 7."
```

---

## Task 9: Implement `Book` (per-symbol L3 book)

**Files:**
- Create: `include/meridian/book.hpp`
- Create: `src/book.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/unit/test_book.cpp`
- Modify: `tests/unit/CMakeLists.txt`

`Book` holds both bid and ask sides for one symbol, plus the `OrderId` lookup map for cancel.

- [ ] **Step 1: Write the failing test `tests/unit/test_book.cpp`**

```cpp
// tests/unit/test_book.cpp
#include "meridian/book.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>

namespace meridian {
namespace {

Order* make(OrderPool& pool, OrderId id, Side side, Price px, Quantity qty) {
    Order* o = pool.acquire();
    o->id = id;
    o->symbol = 1;
    o->side = side;
    o->type = OrderType::Limit;
    o->price = px;
    o->qty_remaining = qty;
    o->ts_arrival = 0;
    return o;
}

TEST(BookTest, EmptyBookHasNoTopOfBook) {
    Book book{1};
    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST(BookTest, AddRestingOrderUpdatesTopOfBook) {
    OrderPool pool(8);
    Book book{1};

    book.add(make(&pool, 1, Side::Buy,  100, 10));
    book.add(make(&pool, 2, Side::Sell, 105, 20));

    ASSERT_NE(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_bid()->price(), 100);
    EXPECT_EQ(book.best_bid()->total_qty(), 10);

    ASSERT_NE(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_ask()->price(), 105);
    EXPECT_EQ(book.best_ask()->total_qty(), 20);
}

TEST(BookTest, BidsOrderedDescendingAsksAscending) {
    OrderPool pool(8);
    Book book{1};

    book.add(make(&pool, 1, Side::Buy, 100, 10));
    book.add(make(&pool, 2, Side::Buy,  99, 10));
    book.add(make(&pool, 3, Side::Buy, 101, 10));

    book.add(make(&pool, 4, Side::Sell, 110, 10));
    book.add(make(&pool, 5, Side::Sell, 109, 10));

    EXPECT_EQ(book.best_bid()->price(), 101);
    EXPECT_EQ(book.best_ask()->price(), 109);
}

TEST(BookTest, FifoAtSamePrice) {
    OrderPool pool(8);
    Book book{1};

    book.add(make(&pool, 1, Side::Buy, 100, 10));
    book.add(make(&pool, 2, Side::Buy, 100, 25));

    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->order_count(), 2u);
    EXPECT_EQ(level->head()->id, 1u);
    EXPECT_EQ(level->head()->next->id, 2u);
}

TEST(BookTest, FindByIdReturnsRestingOrder) {
    OrderPool pool(8);
    Book book{1};

    Order* a = make(&pool, 42, Side::Buy, 100, 10);
    book.add(a);

    EXPECT_EQ(book.find(42), a);
    EXPECT_EQ(book.find(999), nullptr);
}

TEST(BookTest, RemoveByIdSplicesAndForgetsId) {
    OrderPool pool(8);
    Book book{1};

    Order* a = make(&pool, 1, Side::Buy, 100, 10);
    Order* b = make(&pool, 2, Side::Buy, 100, 25);
    book.add(a);
    book.add(b);

    Order* removed = book.remove_by_id(1);
    EXPECT_EQ(removed, a);
    EXPECT_EQ(book.find(1), nullptr);

    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->order_count(), 1u);
    EXPECT_EQ(level->head(), b);
}

TEST(BookTest, RemoveLastOrderAtPriceErasesLevel) {
    OrderPool pool(4);
    Book book{1};

    Order* a = make(&pool, 1, Side::Buy, 100, 10);
    book.add(a);

    book.remove_by_id(1);

    EXPECT_EQ(book.best_bid(), nullptr);
}

}  // namespace
}  // namespace meridian
```

- [ ] **Step 2: Add `test_book` to `tests/unit/CMakeLists.txt`**

```cmake
add_executable(test_book test_book.cpp)
target_link_libraries(test_book PRIVATE meridian GTest::gtest_main)
target_compile_options(test_book PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_book)
```

- [ ] **Step 3: Run; expect fail (no `book.hpp` yet)**

```bash
cmake --build build --target test_book 2>&1 | tail -10
```

- [ ] **Step 4: Write `include/meridian/book.hpp`**

```cpp
// include/meridian/book.hpp
#pragma once

#include "meridian/level.hpp"
#include "meridian/types.hpp"

#include <functional>
#include <map>
#include <memory>
#include <unordered_map>

namespace meridian {

class Book {
public:
    explicit Book(Symbol symbol);

    [[nodiscard]] Symbol symbol() const noexcept { return symbol_; }

    // Best level (highest bid, lowest ask) or nullptr if empty.
    [[nodiscard]] Level* best_bid() const noexcept;
    [[nodiscard]] Level* best_ask() const noexcept;

    // Adds order at its price level (creates Level if needed). order->side
    // determines which side it lands on. The book takes ownership of the
    // pointer's bookkeeping (intrusive list links); the OrderPool owns the
    // memory.
    void add(Order* order);

    // O(1) lookup by id; returns nullptr if id is not in the book.
    [[nodiscard]] Order* find(OrderId id) const noexcept;

    // Splices order out of its Level, removes it from the id map, and
    // returns the now-detached Order pointer (caller releases to OrderPool).
    // Returns nullptr if id not found.
    Order* remove_by_id(OrderId id);

    // Iterate ask levels in ascending price order, calling fn(level) for
    // each. fn returns true to continue, false to stop. Used by matching to
    // sweep prices for an aggressor.
    void for_each_ask_level(const std::function<bool(Level&)>& fn);
    void for_each_bid_level(const std::function<bool(Level&)>& fn);

    // Erase a level if it is empty; the matching loop calls this after
    // exhausting a level during a sweep.
    void erase_empty_level(Side side, Price price);

private:
    using BidLevelMap = std::map<Price, std::unique_ptr<Level>, std::greater<>>;
    using AskLevelMap = std::map<Price, std::unique_ptr<Level>>;

    Symbol symbol_;
    BidLevelMap bids_;
    AskLevelMap asks_;
    std::unordered_map<OrderId, Order*> id_index_;
};

}  // namespace meridian
```

Note: `for_each_*_level` takes `std::function`, which heap-allocates for non-trivial captures. The matching loop in Task 10 uses raw iterators directly to stay allocation-free; `for_each_*_level` is a convenience for tests and the bench harness. The Code Reviewer will catch any hot-path use of `std::function` in Phase 1 review.

- [ ] **Step 5: Write `src/book.cpp`**

```cpp
// src/book.cpp
#include "meridian/book.hpp"

namespace meridian {

Book::Book(Symbol symbol) : symbol_(symbol) {}

Level* Book::best_bid() const noexcept {
    return bids_.empty() ? nullptr : bids_.begin()->second.get();
}

Level* Book::best_ask() const noexcept {
    return asks_.empty() ? nullptr : asks_.begin()->second.get();
}

void Book::add(Order* order) {
    Level* level = nullptr;
    if (order->side == Side::Buy) {
        auto it = bids_.find(order->price);
        if (it == bids_.end()) {
            auto inserted = bids_.emplace(order->price,
                                          std::make_unique<Level>(order->price));
            level = inserted.first->second.get();
        } else {
            level = it->second.get();
        }
    } else {
        auto it = asks_.find(order->price);
        if (it == asks_.end()) {
            auto inserted = asks_.emplace(order->price,
                                          std::make_unique<Level>(order->price));
            level = inserted.first->second.get();
        } else {
            level = it->second.get();
        }
    }
    level->append(order);
    id_index_[order->id] = order;
}

Order* Book::find(OrderId id) const noexcept {
    auto it = id_index_.find(id);
    return it == id_index_.end() ? nullptr : it->second;
}

Order* Book::remove_by_id(OrderId id) {
    auto it = id_index_.find(id);
    if (it == id_index_.end()) {
        return nullptr;
    }
    Order* order = it->second;
    Side side = order->side;
    Price price = order->price;

    if (side == Side::Buy) {
        auto level_it = bids_.find(price);
        if (level_it != bids_.end()) {
            level_it->second->remove(order);
            if (level_it->second->empty()) {
                bids_.erase(level_it);
            }
        }
    } else {
        auto level_it = asks_.find(price);
        if (level_it != asks_.end()) {
            level_it->second->remove(order);
            if (level_it->second->empty()) {
                asks_.erase(level_it);
            }
        }
    }

    id_index_.erase(it);
    return order;
}

void Book::for_each_ask_level(const std::function<bool(Level&)>& fn) {
    for (auto& [price, level_ptr] : asks_) {
        if (!fn(*level_ptr)) break;
    }
}

void Book::for_each_bid_level(const std::function<bool(Level&)>& fn) {
    for (auto& [price, level_ptr] : bids_) {
        if (!fn(*level_ptr)) break;
    }
}

void Book::erase_empty_level(Side side, Price price) {
    if (side == Side::Buy) {
        auto it = bids_.find(price);
        if (it != bids_.end() && it->second->empty()) {
            bids_.erase(it);
        }
    } else {
        auto it = asks_.find(price);
        if (it != asks_.end() && it->second->empty()) {
            asks_.erase(it);
        }
    }
}

}  // namespace meridian
```

- [ ] **Step 6: Add `book.cpp` to `src/CMakeLists.txt`**

```cmake
add_library(meridian STATIC
    order_pool.cpp
    level.cpp
    book.cpp
)
```

- [ ] **Step 7: Build and run; expect pass**

```bash
cmake --build build --target test_book
ctest --test-dir build --output-on-failure -R Book
```

Expected: 7 tests, all pass.

- [ ] **Step 8: Commit**

```bash
git add include/meridian/book.hpp src/book.cpp src/CMakeLists.txt \
        tests/unit/test_book.cpp tests/unit/CMakeLists.txt
git commit -m "phase 1: Book per-symbol L3 with both sides plus id index

std::map<Price, Level*> per side (bids descending, asks ascending) plus an
unordered_map<OrderId, Order*> for O(1) cancel within the symbol. Empty
levels are erased lazily (after each remove). The cross-symbol BookRegistry
plus OrderIndex layer is still Phase 2."
```

---

## Task 10: Implement `MatchingEngine` for limit orders

**Files:**
- Create: `include/meridian/matching.hpp`
- Create: `src/matching.cpp`
- Modify: `src/CMakeLists.txt`
- Create: `tests/unit/test_matching.cpp`
- Modify: `tests/unit/CMakeLists.txt`

This task introduces the matching loop and the `HotPathGuard` RAII helper that arms the debug allocator tripwire.

- [ ] **Step 1: Write the failing test `tests/unit/test_matching.cpp`**

```cpp
// tests/unit/test_matching.cpp
#include "meridian/book.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <gtest/gtest.h>
#include <vector>

namespace meridian {
namespace {

class MatchingTest : public ::testing::Test {
protected:
    OrderPool pool{1024};
    Book book{1};
    MatchingEngine engine{pool, book};

    EngineEvent new_limit(OrderId id, Side side, Price px, Quantity qty,
                          Timestamp ts = 0) {
        return EngineEvent{
            .kind = EventKind::NewOrder,
            .symbol = 1,
            .ts = ts,
            .order_id = id,
            .side = side,
            .type = OrderType::Limit,
            .price = px,
            .qty = qty,
        };
    }
};

TEST_F(MatchingTest, LimitWithoutCounterpartyRestsAndAcknowledges) {
    auto reports = engine.apply(new_limit(1, Side::Buy, 100, 10));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].order_id, 1u);

    Level* level = book.best_bid();
    ASSERT_NE(level, nullptr);
    EXPECT_EQ(level->total_qty(), 10);
}

TEST_F(MatchingTest, LimitFullyCrossesSingleRestingOrderEmitsFill) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    auto reports = engine.apply(new_limit(2, Side::Buy, 100, 10));

    ASSERT_EQ(reports.size(), 2u);
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].order_id, 2u);
    EXPECT_EQ(reports[0].matched_id, 1u);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[0].price, 100);

    EXPECT_EQ(reports[1].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].order_id, 2u);

    EXPECT_EQ(book.best_ask(), nullptr);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, LimitPartiallyFillsAndRestsResidual) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    auto reports = engine.apply(new_limit(2, Side::Buy, 100, 25));

    ASSERT_EQ(reports.size(), 2u);
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[1].kind, ReportKind::Acknowledge);

    Level* bid = book.best_bid();
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->total_qty(), 15);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST_F(MatchingTest, LimitSweepsMultipleRestingOrdersFifoAtSamePrice) {
    engine.apply(new_limit(1, Side::Sell, 100, 10, /*ts=*/1));
    engine.apply(new_limit(2, Side::Sell, 100, 25, /*ts=*/2));
    auto reports = engine.apply(new_limit(3, Side::Buy, 100, 30));

    // Aggressor 3 should fill 10 from id=1 first (older), then 20 from id=2
    // (newer), leaving id=2 with 5 remaining.
    ASSERT_EQ(reports.size(), 3u);  // 2 fills + 1 ack
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].matched_id, 1u);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].matched_id, 2u);
    EXPECT_EQ(reports[1].qty, 20);
    EXPECT_EQ(reports[2].kind, ReportKind::Acknowledge);

    Level* ask = book.best_ask();
    ASSERT_NE(ask, nullptr);
    EXPECT_EQ(ask->total_qty(), 5);
}

TEST_F(MatchingTest, LimitSweepsMultiplePriceLevels) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 101, 10));
    auto reports = engine.apply(new_limit(3, Side::Buy, 102, 20));

    // Aggressor takes 5 at 100, then 10 at 101, leaves 5 resting at 102.
    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].price, 100);
    EXPECT_EQ(reports[0].qty, 5);
    EXPECT_EQ(reports[1].price, 101);
    EXPECT_EQ(reports[1].qty, 10);
    EXPECT_EQ(reports[2].kind, ReportKind::Acknowledge);

    Level* bid = book.best_bid();
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->price(), 102);
    EXPECT_EQ(bid->total_qty(), 5);
}

TEST_F(MatchingTest, LimitDoesNotCrossWhenPriceUnfavorable) {
    engine.apply(new_limit(1, Side::Sell, 105, 10));
    auto reports = engine.apply(new_limit(2, Side::Buy, 100, 10));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);

    Level* bid = book.best_bid();
    ASSERT_NE(bid, nullptr);
    EXPECT_EQ(bid->price(), 100);
    EXPECT_EQ(book.best_ask()->price(), 105);
}

}  // namespace
}  // namespace meridian
```

- [ ] **Step 2: Add `test_matching` to `tests/unit/CMakeLists.txt`**

```cmake
add_executable(test_matching test_matching.cpp)
target_link_libraries(test_matching PRIVATE meridian GTest::gtest_main)
target_compile_options(test_matching PRIVATE -Wall -Wextra -Wpedantic -Werror)
gtest_discover_tests(test_matching)
```

- [ ] **Step 3: Run; expect fail**

```bash
cmake --build build --target test_matching 2>&1 | tail -10
```

- [ ] **Step 4: Write `include/meridian/matching.hpp`**

```cpp
// include/meridian/matching.hpp
#pragma once

#include "meridian/book.hpp"
#include "meridian/execution_report.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <vector>

namespace meridian {

namespace debug_alloc {
extern std::atomic<bool> hot_path_active;
}

class HotPathGuard {
public:
    HotPathGuard() noexcept;
    ~HotPathGuard() noexcept;
    HotPathGuard(const HotPathGuard&) = delete;
    HotPathGuard& operator=(const HotPathGuard&) = delete;
};

class MatchingEngine {
public:
    MatchingEngine(OrderPool& pool, Book& book) noexcept;

    // Apply a single event. Returns the chronological list of execution
    // reports produced. The vector is constructed and returned by value
    // (single allocation; outside the hot-path guarded region) so the
    // matching loop itself remains allocation-free.
    std::vector<ExecutionReport> apply(const EngineEvent& event);

private:
    void apply_limit(const EngineEvent& event,
                     std::vector<ExecutionReport>& out);
    void apply_market(const EngineEvent& event,
                      std::vector<ExecutionReport>& out);
    void apply_ioc(const EngineEvent& event,
                   std::vector<ExecutionReport>& out);
    void apply_cancel(const EngineEvent& event,
                      std::vector<ExecutionReport>& out);

    // Cross the aggressor against the opposite side until either the
    // aggressor is exhausted or the next opposite-side level is at a price
    // worse than limit_price. Returns the quantity remaining on the
    // aggressor after sweeping.
    Quantity sweep(Side aggressor_side, Price limit_price,
                   OrderId aggressor_id, Quantity remaining,
                   Timestamp ts, std::vector<ExecutionReport>& out);

    OrderPool& pool_;
    Book& book_;
};

}  // namespace meridian
```

- [ ] **Step 5: Write `src/matching.cpp`**

```cpp
// src/matching.cpp
#include "meridian/matching.hpp"

#include <algorithm>
#include <atomic>

namespace meridian {

HotPathGuard::HotPathGuard() noexcept {
#ifndef NDEBUG
    debug_alloc::hot_path_active.store(true, std::memory_order_relaxed);
#endif
}

HotPathGuard::~HotPathGuard() noexcept {
#ifndef NDEBUG
    debug_alloc::hot_path_active.store(false, std::memory_order_relaxed);
#endif
}

MatchingEngine::MatchingEngine(OrderPool& pool, Book& book) noexcept
    : pool_(pool), book_(book) {}

std::vector<ExecutionReport> MatchingEngine::apply(const EngineEvent& event) {
    std::vector<ExecutionReport> out;
    out.reserve(8);  // headroom; resize past this is allowed (out of guard)
    {
        HotPathGuard guard;
        if (event.kind == EventKind::NewOrder) {
            switch (event.type) {
                case OrderType::Limit:  apply_limit(event,  out); break;
                case OrderType::Market: apply_market(event, out); break;
                case OrderType::IOC:    apply_ioc(event,    out); break;
                case OrderType::PostOnly:
                case OrderType::FOK:
                    // Phase 7. For Phase 1 we reject explicitly so a
                    // stray PostOnly/FOK in a test does not silently fall
                    // through to undefined behavior.
                    out.push_back(ExecutionReport{
                        .kind = ReportKind::Reject,
                        .order_id = event.order_id,
                        .matched_id = 0,
                        .side = event.side,
                        .price = event.price,
                        .qty = event.qty,
                        .ts = event.ts,
                        .reject_reason = RejectReason::None,
                    });
                    break;
            }
        } else {
            apply_cancel(event, out);
        }
    }
    return out;
}

namespace {

inline bool crosses(Side aggressor_side, Price limit_price, Price resting_price) {
    if (aggressor_side == Side::Buy) {
        return resting_price <= limit_price;
    }
    return resting_price >= limit_price;
}

}  // namespace

Quantity MatchingEngine::sweep(Side aggressor_side, Price limit_price,
                               OrderId aggressor_id, Quantity remaining,
                               Timestamp ts,
                               std::vector<ExecutionReport>& out) {
    while (remaining > 0) {
        Level* level = (aggressor_side == Side::Buy)
                           ? book_.best_ask()
                           : book_.best_bid();
        if (level == nullptr) [[unlikely]] {
            break;
        }
        if (!crosses(aggressor_side, limit_price, level->price())) {
            break;
        }

        Order* resting = level->head();
        while (resting != nullptr && remaining > 0) {
            Quantity match_qty = std::min(remaining, resting->qty_remaining);
            out.push_back(ExecutionReport{
                .kind = ReportKind::Fill,
                .order_id = aggressor_id,
                .matched_id = resting->id,
                .side = aggressor_side,
                .price = level->price(),
                .qty = match_qty,
                .ts = ts,
                .reject_reason = RejectReason::None,
            });
            remaining -= match_qty;
            Order* next = resting->next;
            if (match_qty == resting->qty_remaining) {
                Order* removed = book_.remove_by_id(resting->id);
                pool_.release(removed);
            } else {
                level->adjust_qty(resting, resting->qty_remaining - match_qty);
            }
            resting = next;
        }

        Side opposite = (aggressor_side == Side::Buy) ? Side::Sell : Side::Buy;
        book_.erase_empty_level(opposite,
                                (aggressor_side == Side::Buy)
                                    ? (book_.best_ask() ? book_.best_ask()->price() : 0)
                                    : (book_.best_bid() ? book_.best_bid()->price() : 0));
    }
    return remaining;
}

void MatchingEngine::apply_limit(const EngineEvent& event,
                                 std::vector<ExecutionReport>& out) {
    Quantity remaining = sweep(event.side, event.price, event.order_id,
                               event.qty, event.ts, out);
    if (remaining > 0) {
        Order* resting = pool_.acquire();
        resting->id = event.order_id;
        resting->symbol = event.symbol;
        resting->side = event.side;
        resting->type = event.type;
        resting->price = event.price;
        resting->qty_remaining = remaining;
        resting->ts_arrival = event.ts;
        book_.add(resting);
    }
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .matched_id = 0,
        .side = event.side,
        .price = event.price,
        .qty = event.qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
}

void MatchingEngine::apply_market(const EngineEvent& event,
                                  std::vector<ExecutionReport>& out) {
    Level* opposite = (event.side == Side::Buy) ? book_.best_ask() : book_.best_bid();
    if (opposite == nullptr) [[unlikely]] {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .matched_id = 0,
            .side = event.side,
            .price = 0,
            .qty = event.qty,
            .ts = event.ts,
            .reject_reason = RejectReason::EmptyBook,
        });
        return;
    }
    Price worst_price = (event.side == Side::Buy) ? std::numeric_limits<Price>::max()
                                                  : std::numeric_limits<Price>::min();
    Quantity remaining = sweep(event.side, worst_price, event.order_id,
                               event.qty, event.ts, out);
    // Market orders never rest; any residual is implicitly cancelled.
    if (remaining < event.qty) {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Acknowledge,
            .order_id = event.order_id,
            .matched_id = 0,
            .side = event.side,
            .price = 0,
            .qty = event.qty - remaining,
            .ts = event.ts,
            .reject_reason = RejectReason::None,
        });
    } else {
        // No fills at all (sweep ran but opposite went empty). Reject.
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .matched_id = 0,
            .side = event.side,
            .price = 0,
            .qty = event.qty,
            .ts = event.ts,
            .reject_reason = RejectReason::EmptyBook,
        });
    }
}

void MatchingEngine::apply_ioc(const EngineEvent& event,
                               std::vector<ExecutionReport>& out) {
    Quantity remaining = sweep(event.side, event.price, event.order_id,
                               event.qty, event.ts, out);
    out.push_back(ExecutionReport{
        .kind = ReportKind::Acknowledge,
        .order_id = event.order_id,
        .matched_id = 0,
        .side = event.side,
        .price = event.price,
        .qty = event.qty - remaining,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
    // Residual is implicitly cancelled (no Cancel report needed; the ack's
    // qty field already reports what filled).
}

void MatchingEngine::apply_cancel(const EngineEvent& event,
                                  std::vector<ExecutionReport>& out) {
    Order* removed = book_.remove_by_id(event.order_id);
    if (removed == nullptr) [[unlikely]] {
        out.push_back(ExecutionReport{
            .kind = ReportKind::Reject,
            .order_id = event.order_id,
            .matched_id = 0,
            .side = event.side,
            .price = 0,
            .qty = 0,
            .ts = event.ts,
            .reject_reason = RejectReason::NotFound,
        });
        return;
    }
    Quantity cancelled_qty = removed->qty_remaining;
    Side cancelled_side = removed->side;
    Price cancelled_price = removed->price;
    pool_.release(removed);
    out.push_back(ExecutionReport{
        .kind = ReportKind::Cancel,
        .order_id = event.order_id,
        .matched_id = 0,
        .side = cancelled_side,
        .price = cancelled_price,
        .qty = cancelled_qty,
        .ts = event.ts,
        .reject_reason = RejectReason::None,
    });
}

}  // namespace meridian
```

- [ ] **Step 6: Add `matching.cpp` to `src/CMakeLists.txt`**

```cmake
add_library(meridian STATIC
    order_pool.cpp
    level.cpp
    book.cpp
    matching.cpp
)
```

- [ ] **Step 7: Add the missing `<atomic>` include to `order_pool.hpp` extern declaration**

`matching.hpp` references `meridian::debug_alloc::hot_path_active`. Confirm `order_pool.cpp` defines it (Task 6 step 10) and that the `extern` declaration in `matching.hpp` matches. Build to verify the linker resolves it.

- [ ] **Step 8: Build and run the limit-order tests**

```bash
cmake --build build --target test_matching
ctest --test-dir build --output-on-failure -R Matching.Limit
```

Expected: 6 limit-order tests pass.

- [ ] **Step 9: Commit**

```bash
git add include/meridian/matching.hpp src/matching.cpp src/CMakeLists.txt \
        tests/unit/test_matching.cpp tests/unit/CMakeLists.txt
git commit -m "phase 1: MatchingEngine for limit orders plus HotPathGuard

apply(NewOrder Limit) sweeps the opposite side from the best price outward,
matching FIFO at each level until the aggressor is exhausted or the next
level is at a price worse than the limit. Residual rests at the limit
price. HotPathGuard arms the debug allocator tripwire for the duration of
apply()."
```

---

## Task 11: Implement market and IOC order matching

**Files:**
- Modify: `tests/unit/test_matching.cpp` (already created in Task 10; append the new tests)

The matching code for market and IOC was already written in Task 10's `src/matching.cpp` so the implementation passes when the tests arrive. This task focuses on the test cases.

- [ ] **Step 1: Append market-order tests to `tests/unit/test_matching.cpp`**

Add inside the same `MatchingTest` fixture:

```cpp
EngineEvent new_market(OrderId id, Side side, Quantity qty,
                       Timestamp ts = 0) {
    return EngineEvent{
        .kind = EventKind::NewOrder,
        .symbol = 1,
        .ts = ts,
        .order_id = id,
        .side = side,
        .type = OrderType::Market,
        .price = 0,
        .qty = qty,
    };
}

TEST_F(MatchingTest, MarketOnEmptyBookRejectsWithEmptyBook) {
    auto reports = engine.apply(new_market(1, Side::Buy, 10));
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::EmptyBook);
}

TEST_F(MatchingTest, MarketFullyFillsAtBestPriceAcrossLevels) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 101, 5));
    auto reports = engine.apply(new_market(3, Side::Buy, 8));

    // 5 at 100, then 3 at 101.
    ASSERT_EQ(reports.size(), 3u);
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].price, 100);
    EXPECT_EQ(reports[0].qty, 5);
    EXPECT_EQ(reports[1].kind, ReportKind::Fill);
    EXPECT_EQ(reports[1].price, 101);
    EXPECT_EQ(reports[1].qty, 3);
    EXPECT_EQ(reports[2].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[2].qty, 8);
}

TEST_F(MatchingTest, MarketPartiallyFillsAndResidualIsImplicitlyCancelled) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    auto reports = engine.apply(new_market(2, Side::Buy, 10));

    // 5 fills, then book empties, residual 5 is implicitly cancelled.
    ASSERT_EQ(reports.size(), 2u);
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].qty, 5);
    EXPECT_EQ(reports[1].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].qty, 5);

    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}
```

- [ ] **Step 2: Append IOC-order tests**

```cpp
EngineEvent new_ioc(OrderId id, Side side, Price px, Quantity qty,
                    Timestamp ts = 0) {
    return EngineEvent{
        .kind = EventKind::NewOrder,
        .symbol = 1,
        .ts = ts,
        .order_id = id,
        .side = side,
        .type = OrderType::IOC,
        .price = px,
        .qty = qty,
    };
}

TEST_F(MatchingTest, IocOnEmptyBookAcksWithZeroFilled) {
    auto reports = engine.apply(new_ioc(1, Side::Buy, 100, 10));
    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[0].qty, 0);
    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, IocFillsWhatItCanAndDoesNotRest) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    auto reports = engine.apply(new_ioc(2, Side::Buy, 100, 20));

    // 5 fills, residual 15 is implicitly cancelled (never rests).
    ASSERT_EQ(reports.size(), 2u);
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].qty, 5);
    EXPECT_EQ(reports[1].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].qty, 5);

    EXPECT_EQ(book.best_bid(), nullptr);
    EXPECT_EQ(book.best_ask(), nullptr);
}

TEST_F(MatchingTest, IocPriceLimitedDoesNotCrossWorsePrices) {
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 105, 10));

    // IOC Buy at 102 only crosses the 100-priced level.
    auto reports = engine.apply(new_ioc(3, Side::Buy, 102, 20));

    ASSERT_EQ(reports.size(), 2u);
    EXPECT_EQ(reports[0].kind, ReportKind::Fill);
    EXPECT_EQ(reports[0].price, 100);
    EXPECT_EQ(reports[0].qty, 5);
    EXPECT_EQ(reports[1].kind, ReportKind::Acknowledge);
    EXPECT_EQ(reports[1].qty, 5);

    Level* ask = book.best_ask();
    ASSERT_NE(ask, nullptr);
    EXPECT_EQ(ask->price(), 105);
    EXPECT_EQ(ask->total_qty(), 10);
}
```

- [ ] **Step 3: Build and run; expect pass**

```bash
cmake --build build --target test_matching
ctest --test-dir build --output-on-failure -R Matching
```

Expected: all matching tests pass (6 limit + 3 market + 3 IOC = 12).

- [ ] **Step 4: Commit**

```bash
git add tests/unit/test_matching.cpp
git commit -m "phase 1: market and IOC matching tests

Market on empty book rejects with EmptyBook; market sweeps multiple price
levels; market residual is implicitly cancelled. IOC on empty book acks
with zero filled; IOC fills what it can and does not rest; IOC honors its
limit price."
```

---

## Task 12: Cancel-by-id integration tests

**Files:**
- Modify: `tests/unit/test_matching.cpp`

Cancel-by-id flows through the same `MatchingEngine::apply` entry point. The implementation in Task 10 already handles `EventKind::Cancel`. This task adds the tests.

- [ ] **Step 1: Append cancel tests to `tests/unit/test_matching.cpp`**

```cpp
EngineEvent new_cancel(OrderId id, Timestamp ts = 0) {
    return EngineEvent{
        .kind = EventKind::Cancel,
        .symbol = 1,
        .ts = ts,
        .order_id = id,
        .side = Side::Buy,  // unused for cancel
        .type = OrderType::Limit,
        .price = 0,
        .qty = 0,
    };
}

TEST_F(MatchingTest, CancelKnownOrderEmitsCancelReport) {
    engine.apply(new_limit(1, Side::Buy, 100, 10));
    auto reports = engine.apply(new_cancel(1));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Cancel);
    EXPECT_EQ(reports[0].order_id, 1u);
    EXPECT_EQ(reports[0].qty, 10);
    EXPECT_EQ(reports[0].price, 100);

    EXPECT_EQ(book.best_bid(), nullptr);
}

TEST_F(MatchingTest, CancelUnknownOrderEmitsRejectNotFound) {
    auto reports = engine.apply(new_cancel(99));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::NotFound);
}

TEST_F(MatchingTest, CancelAfterFullFillEmitsRejectNotFound) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    engine.apply(new_limit(2, Side::Buy, 100, 10));
    // id=1 is fully filled and removed.
    auto reports = engine.apply(new_cancel(1));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Reject);
    EXPECT_EQ(reports[0].reject_reason, RejectReason::NotFound);
}

TEST_F(MatchingTest, CancelAfterPartialFillRemovesResidual) {
    engine.apply(new_limit(1, Side::Sell, 100, 10));
    engine.apply(new_limit(2, Side::Buy, 100, 4));
    // id=1 partially filled; 6 remaining.
    auto reports = engine.apply(new_cancel(1));

    ASSERT_EQ(reports.size(), 1u);
    EXPECT_EQ(reports[0].kind, ReportKind::Cancel);
    EXPECT_EQ(reports[0].qty, 6);

    EXPECT_EQ(book.best_ask(), nullptr);
}
```

- [ ] **Step 2: Build and run; expect pass**

```bash
cmake --build build --target test_matching
ctest --test-dir build --output-on-failure -R Matching
```

Expected: all matching tests pass (16 total now).

- [ ] **Step 3: Commit**

```bash
git add tests/unit/test_matching.cpp
git commit -m "phase 1: cancel-by-id matching tests

Known order is cancelled and its remaining qty reported; unknown order
emits Reject NotFound; cancel after full fill emits Reject NotFound;
cancel after partial fill removes the residual."
```

---

## Task 13: Verify zero heap allocation on the matching hot path

**Files:**
- Modify: `tests/unit/test_matching.cpp`

The debug allocator override aborts on any heap allocation while `HotPathGuard` is armed. This task adds a focused test that exercises a non-trivial matching scenario in a debug build and confirms no abort.

- [ ] **Step 1: Confirm Debug build runs the existing matching tests cleanly**

```bash
cd /home/mustafa/src/Meridian
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build --target test_matching
ctest --test-dir build --output-on-failure -R Matching
```

Expected: all tests pass. If any test aborts with the "FATAL: heap allocation" message, the matching code is allocating on the hot path (likely a `std::vector` resize beyond its `reserve(8)` capacity, or a `Book::add` creating a new `Level` via `std::make_unique`).

- [ ] **Step 2: Document the known allocations**

The `Book::add` path creates a new `Level` when an order arrives at a price with no existing level. That allocation happens inside `HotPathGuard`'s scope today, which is a real correctness issue: the matching loop must not allocate.

Resolution for Phase 1: pre-warm the `bids_`/`asks_` maps by accepting prices the test scenarios use as a constructor argument is overkill. Instead, define the hot path narrowly: the **matching sweep** is the hot path; `Book::add` (which only runs after a sweep produces residual) is rest-of-the-engine. Move the `HotPathGuard` so it covers only the `sweep()` call, not the residual `Book::add`.

In `src/matching.cpp`, replace the body of `MatchingEngine::apply`:

```cpp
std::vector<ExecutionReport> MatchingEngine::apply(const EngineEvent& event) {
    std::vector<ExecutionReport> out;
    out.reserve(8);
    if (event.kind == EventKind::NewOrder) {
        switch (event.type) {
            case OrderType::Limit:  apply_limit(event,  out); break;
            case OrderType::Market: apply_market(event, out); break;
            case OrderType::IOC:    apply_ioc(event,    out); break;
            default:
                out.push_back(ExecutionReport{
                    .kind = ReportKind::Reject,
                    .order_id = event.order_id,
                    .matched_id = 0,
                    .side = event.side,
                    .price = event.price,
                    .qty = event.qty,
                    .ts = event.ts,
                    .reject_reason = RejectReason::None,
                });
                break;
        }
    } else {
        apply_cancel(event, out);
    }
    return out;
}
```

Then in `sweep()`, wrap only the inner FIFO walk with `HotPathGuard`:

```cpp
Quantity MatchingEngine::sweep(...) {
    HotPathGuard guard;
    while (remaining > 0) { ... }
    return remaining;
}
```

Re-build and re-run all matching tests.

- [ ] **Step 3: Add an explicit allocation-free assertion**

```cpp
TEST_F(MatchingTest, SweepLoopMakesNoHeapAllocation) {
    // Pre-populate the book so Book::add is not on the sweep path.
    engine.apply(new_limit(1, Side::Sell, 100, 5));
    engine.apply(new_limit(2, Side::Sell, 101, 5));
    engine.apply(new_limit(3, Side::Sell, 102, 5));
    // The sweep below should match all three resting orders without
    // allocating; if it does, the debug operator new override aborts the
    // test.
    auto reports = engine.apply(new_ioc(4, Side::Buy, 105, 15));
    EXPECT_EQ(reports.size(), 4u);  // 3 fills + 1 ack
}
```

- [ ] **Step 4: Build and verify in Debug**

```bash
cmake --build build --target test_matching
ctest --test-dir build --output-on-failure -R Matching
```

Expected: passes. If it aborts, investigate the allocation source.

- [ ] **Step 5: Commit**

```bash
git add src/matching.cpp tests/unit/test_matching.cpp
git commit -m "phase 1: narrow HotPathGuard to the sweep loop

The matching sweep is the hot path; Book::add (which creates a new Level
when an order rests at a fresh price) runs after the sweep and may safely
allocate. The HotPathGuard now covers only the sweep loop. Adds an
explicit allocation-free assertion test that exercises a multi-level
sweep without triggering the debug allocator override."
```

---

## Task 14: Wire the C++ engine to the Python reference for diff testing

**Files:**
- Create: `tests/integration/CMakeLists.txt`
- Create: `tests/integration/test_engine_vs_reference.cpp`
- Modify: `tests/CMakeLists.txt` (uncomment `add_subdirectory(integration)`)

The integration test writes a JSON event file, runs both the C++ engine and `tests/reference/run_reference.py` (which the dispatched Reference Implementation Engineer wrote in Task 3) on the same input, then byte-diffs the output JSON.

- [ ] **Step 1: Create `tests/reference/run_reference.py`**

Note: the Reference Implementation Engineer's Task 3 output ships `matching_reference.py` and `test_reference.py` but not `run_reference.py` (that file is Phase 6). Phase 1's diff harness is simpler: it loads the reference from Python directly via `subprocess` running an inline driver script. Add a minimal `run_reference.py` to the dispatch in Task 3, or write it here as part of Task 14:

```python
# tests/reference/run_reference.py
"""Drive the matching reference from a JSON Lines event stream on stdin.

Each input line is one JSON object with fields:
    {"kind": "new_order"|"cancel", "type": "limit"|"market"|"ioc",
     "order_id": int, "side": "buy"|"sell", "price": int, "qty": int,
     "ts": int}

Each output line is one JSON object describing one ExecutionReport.
"""

import json
import sys

from matching_reference import MatchingReference, Side, OrderType


def main() -> int:
    engine = MatchingReference(symbol=1)
    side_map = {"buy": Side.BUY, "sell": Side.SELL}
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        event = json.loads(raw)
        if event["kind"] == "new_order":
            otype = event["type"]
            side = side_map[event["side"]]
            if otype == "limit":
                reports = engine.submit_limit(
                    event["order_id"], side, event["price"],
                    event["qty"], event["ts"])
            elif otype == "market":
                reports = engine.submit_market(
                    event["order_id"], side, event["qty"], event["ts"])
            elif otype == "ioc":
                reports = engine.submit_ioc(
                    event["order_id"], side, event["price"],
                    event["qty"], event["ts"])
            else:
                raise ValueError(f"unknown order type {otype!r}")
        elif event["kind"] == "cancel":
            reports = engine.cancel(event["order_id"])
        else:
            raise ValueError(f"unknown event kind {event['kind']!r}")
        for r in reports:
            sys.stdout.write(json.dumps(r.to_dict(), sort_keys=True) + "\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
```

(Confirm with the Reference Implementation Engineer in Task 3 that `MatchingReference.to_dict()` and the field names are stable.)

- [ ] **Step 2: Write `tests/integration/test_engine_vs_reference.cpp`**

This test is parameterized: each scenario is one event sequence. The harness writes the events to a temp JSON Lines file, runs the C++ engine writing JSON Lines output, runs `python3 tests/reference/run_reference.py` on the same input, and `EXPECT_EQ`s the two outputs line by line.

```cpp
// tests/integration/test_engine_vs_reference.cpp
#include "meridian/book.hpp"
#include "meridian/matching.hpp"
#include "meridian/order_pool.hpp"
#include "meridian/types.hpp"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
#include <string>
#include <vector>

namespace meridian {
namespace {

struct ScenarioEvent {
    std::string kind;
    std::string type;
    OrderId id;
    std::string side;
    Price price;
    Quantity qty;
    Timestamp ts;
};

std::string serialize_event_jsonl(const ScenarioEvent& e) {
    std::ostringstream ss;
    ss << "{"
       << "\"kind\":\"" << e.kind << "\","
       << "\"type\":\"" << e.type << "\","
       << "\"order_id\":" << e.id << ","
       << "\"side\":\"" << e.side << "\","
       << "\"price\":" << e.price << ","
       << "\"qty\":" << e.qty << ","
       << "\"ts\":" << e.ts
       << "}";
    return ss.str();
}

std::string report_to_jsonl(const ExecutionReport& r) {
    const char* kind = nullptr;
    switch (r.kind) {
        case ReportKind::Acknowledge: kind = "ack"; break;
        case ReportKind::Fill:        kind = "fill"; break;
        case ReportKind::Cancel:      kind = "cancel"; break;
        case ReportKind::Reject:      kind = "reject"; break;
    }
    const char* side = (r.side == Side::Buy) ? "buy" : "sell";
    const char* reason = "none";
    switch (r.reject_reason) {
        case RejectReason::None: reason = "none"; break;
        case RejectReason::EmptyBook: reason = "empty_book"; break;
        case RejectReason::NotFound: reason = "not_found"; break;
        case RejectReason::InsufficientLiquidity: reason = "insufficient_liquidity"; break;
        case RejectReason::WouldCross: reason = "would_cross"; break;
    }
    std::ostringstream ss;
    ss << "{"
       << "\"kind\":\"" << kind << "\","
       << "\"matched_id\":" << r.matched_id << ","
       << "\"order_id\":" << r.order_id << ","
       << "\"price\":" << r.price << ","
       << "\"qty\":" << r.qty << ","
       << "\"reject_reason\":\"" << reason << "\","
       << "\"side\":\"" << side << "\","
       << "\"ts\":" << r.ts
       << "}";
    return ss.str();
}

EngineEvent to_engine_event(const ScenarioEvent& e) {
    EngineEvent ev{};
    ev.symbol = 1;
    ev.ts = e.ts;
    ev.order_id = e.id;
    ev.side = (e.side == "buy") ? Side::Buy : Side::Sell;
    if (e.kind == "cancel") {
        ev.kind = EventKind::Cancel;
    } else {
        ev.kind = EventKind::NewOrder;
        if (e.type == "limit")       ev.type = OrderType::Limit;
        else if (e.type == "market") ev.type = OrderType::Market;
        else if (e.type == "ioc")    ev.type = OrderType::IOC;
        ev.price = e.price;
        ev.qty = e.qty;
    }
    return ev;
}

std::vector<std::string> run_cpp(const std::vector<ScenarioEvent>& events) {
    OrderPool pool(1024);
    Book book{1};
    MatchingEngine engine{pool, book};
    std::vector<std::string> lines;
    for (const auto& e : events) {
        auto reports = engine.apply(to_engine_event(e));
        for (const auto& r : reports) {
            lines.push_back(report_to_jsonl(r));
        }
    }
    return lines;
}

std::vector<std::string> run_python(const std::vector<ScenarioEvent>& events,
                                    const std::string& workdir) {
    std::string input_path = workdir + "/input.jsonl";
    std::string output_path = workdir + "/output.jsonl";
    {
        std::ofstream f(input_path);
        for (const auto& e : events) f << serialize_event_jsonl(e) << "\n";
    }
    std::string cmd = "cd " + workdir +
                      " && PYTHONPATH=" SOURCE_REFERENCE_DIR
                      " python3 " SOURCE_REFERENCE_DIR "/run_reference.py"
                      " < " + input_path + " > " + output_path;
    int rc = std::system(cmd.c_str());
    EXPECT_EQ(rc, 0);
    std::ifstream f(output_path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(f, line)) lines.push_back(line);
    return lines;
}

class EngineVsReferenceTest : public ::testing::TestWithParam<std::vector<ScenarioEvent>> {};

TEST_P(EngineVsReferenceTest, ProducesIdenticalReports) {
    char workdir[] = "/tmp/meridian_diff_XXXXXX";
    ASSERT_NE(mkdtemp(workdir), nullptr);
    auto cpp_lines = run_cpp(GetParam());
    auto py_lines = run_python(GetParam(), workdir);
    ASSERT_EQ(cpp_lines.size(), py_lines.size())
        << "C++ produced " << cpp_lines.size()
        << " reports, Python produced " << py_lines.size();
    for (std::size_t i = 0; i < cpp_lines.size(); ++i) {
        EXPECT_EQ(cpp_lines[i], py_lines[i]) << "report " << i;
    }
}

INSTANTIATE_TEST_SUITE_P(
    HandCrafted, EngineVsReferenceTest,
    ::testing::Values(
        // Scenario 1: trivial limit-rest, no cross.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "buy", 100, 10, 1},
            {"new_order", "limit", 2, "sell", 105, 10, 2},
        },
        // Scenario 2: limit fully crosses single resting order.
        std::vector<ScenarioEvent>{
            {"new_order", "limit", 1, "sell", 100, 10, 1},
            {"new_order", "limit", 2, "buy", 100, 10, 2},
        }
        // The remaining 48 hand-crafted scenarios are appended in Task 15
        // by the QA Engineer dispatch. The skeleton above proves the harness
        // works.
    )
);

}  // namespace
}  // namespace meridian
```

The `SOURCE_REFERENCE_DIR` macro is set in CMake.

- [ ] **Step 3: Write `tests/integration/CMakeLists.txt`**

```cmake
# tests/integration/CMakeLists.txt
add_executable(test_engine_vs_reference test_engine_vs_reference.cpp)

target_compile_definitions(test_engine_vs_reference PRIVATE
    SOURCE_REFERENCE_DIR="${CMAKE_SOURCE_DIR}/tests/reference"
)

target_link_libraries(test_engine_vs_reference PRIVATE
    meridian
    GTest::gtest_main
)

target_compile_options(test_engine_vs_reference PRIVATE
    -Wall -Wextra -Wpedantic -Werror
)

gtest_discover_tests(test_engine_vs_reference)
```

- [ ] **Step 4: Update `tests/CMakeLists.txt`**

```cmake
add_subdirectory(unit)
add_subdirectory(integration)
```

- [ ] **Step 5: Verify Python is available in CI**

`.github/workflows/ci.yml` already runs on `ubuntu-22.04` images that ship Python 3.10+. Add `python3 --version` to the engine job's setup step if not already present so CI fails clearly when Python is missing.

- [ ] **Step 6: Build and run; expect pass**

```bash
cmake --build build --target test_engine_vs_reference
ctest --test-dir build --output-on-failure -R EngineVsReference
```

Expected: 2 scenarios pass (the bootstrap pair). The QA dispatch in Task 15 expands the corpus to 60 scenarios (50 hand-crafted plus 10 corner cases) per `docs/plan.md` Phase 1.

- [ ] **Step 7: Commit**

```bash
git add tests/integration/CMakeLists.txt tests/integration/test_engine_vs_reference.cpp \
        tests/reference/run_reference.py tests/CMakeLists.txt
git commit -m "phase 1: integration test harness, C++ engine vs Python reference

Each scenario is a JSON Lines event sequence; the harness runs both
engines on the same input and byte-diffs the reports. Two bootstrap
scenarios prove the harness works; QA expands the corpus to 60 in the
next dispatch."
```

---

## Task 15: Dispatch QA Engineer (10) for full unit-test sweep, integration corpus, regression checklist

**Files:**
- Create (by dispatched agent): `docs/qa/regression-checklist.md`
- Modify (by dispatched agent): `tests/integration/test_engine_vs_reference.cpp` (expand to 60 scenarios)
- Modify (by dispatched agent): `tests/unit/test_*.cpp` (any coverage gaps to reach 85 percent line coverage on `libmeridian.a`)

- [ ] **Step 1: Dispatch the agent**

```
You are the QA Engineer agent for the Meridian project at
/home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/SPEC.md
- /home/mustafa/src/Meridian/agents/10-qa-engineer.md
- /home/mustafa/src/Meridian/docs/risk/matching-semantics.md
- /home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md
  (section 8.1 unit tests; section 8.3 integration tests)
- /home/mustafa/src/Meridian/docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
- The existing C++ test files under /home/mustafa/src/Meridian/tests/unit/
- The existing integration harness at
  /home/mustafa/src/Meridian/tests/integration/test_engine_vs_reference.cpp

Phase 1 tasks from your agent file:

1. Audit the C++ unit-test coverage. Run lcov against the existing tests:
     cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug \
       -DCMAKE_CXX_FLAGS="--coverage" -DCMAKE_EXE_LINKER_FLAGS="--coverage"
     cmake --build build
     ctest --test-dir build --output-on-failure
     lcov --capture --directory build --output-file coverage.info
     lcov --extract coverage.info '*/include/meridian/*' '*/src/*' \
       --output-file coverage_filtered.info
     genhtml coverage_filtered.info --output-directory coverage_html
   Target: >=85 percent line coverage on libmeridian.a (src/ plus
   include/meridian/). Add unit tests to fill any gaps. Coverage gaps in
   matching.cpp paths only triggered by Phase 7 PostOnly/FOK reject branches
   are acceptable for Phase 1 (mark with a /* PHASE7 */ comment).

2. Expand the integration corpus in
   tests/integration/test_engine_vs_reference.cpp from the 2 bootstrap
   scenarios to 60: 50 hand-crafted scenarios from
   docs/risk/matching-semantics.md worked examples (every worked example
   becomes one scenario), plus 10 corner cases:
     - Limit at exact best price (cross? no cross?).
     - Cancel on empty book.
     - Cancel of an id that was never added.
     - Market when opposite side has exactly the requested qty (full fill,
       book empties).
     - IOC with price outside any resting level (zero fill).
     - Sequence of 3 limits at the same price, different qty, then cross
       all three with one aggressor.
     - Self-trade (same trader id on both sides). Document the policy:
       Phase 1 allows self-trade and matches it like any other cross. A
       future agent file may revisit this.
     - Cancel then re-submit same order id. Document the policy: Phase 1
       allows reuse of an id after its prior order is cancelled or fully
       filled. A future agent file may add an "id reuse" reject.
     - Aggressor sweeps two price levels and partially fills the second
       level's first order.
     - 1000 random limits at random prices around 100, then a market on
       each side, then a cancel-everything sweep. Verify both engines agree.

3. Write the initial regression checklist at
   /home/mustafa/src/Meridian/docs/qa/regression-checklist.md. The checklist
   covers manual smoke tests run before each phase closes:
     - cmake --build build --target meridian succeeds in Debug and Release.
     - ctest --output-on-failure passes in both configurations.
     - lcov coverage on libmeridian.a is >=85 percent.
     - The integration corpus passes (60 scenarios in Phase 1; expands in
       later phases).
     - The bench skeleton (apps/bench/main.cpp) compiles and runs; the
       throughput is informational only in Phase 1 (target enforced in
       Phase 5).

Stay strictly in the role of QA Engineer. Do not modify the matching engine
implementation in src/. If a scenario reveals a bug in src/, escalate to
the Project Manager rather than fixing it yourself.

Report:
1. The final coverage percentage on libmeridian.a.
2. The integration test count (should be 60).
3. The file paths produced or modified.
4. Any bugs surfaced (route to PM for fix; do not fix in-flight).
```

- [ ] **Step 2: Verify**

```bash
cd /home/mustafa/src/Meridian
cmake -B build -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build build
ctest --test-dir build --output-on-failure
```

Expected: all unit tests plus 60 integration scenarios pass.

- [ ] **Step 3: Commit**

```bash
git add tests/unit tests/integration docs/qa/regression-checklist.md
git commit -m "phase 1: full QA pass

Expanded integration corpus to 60 scenarios (50 worked examples plus 10
corner cases). Coverage on libmeridian.a is now N percent (target 85
percent). Initial regression checklist filed."
```

---

## Task 16: Dispatch Risk and Financial Correctness Reviewer (16)

**Files:**
- Create (by dispatched agent): `docs/risk/audit-cases.md`

- [ ] **Step 1: Dispatch the agent**

```
You are the Risk and Financial Correctness Reviewer agent for the Meridian
project at /home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/agents/16-risk-correctness-reviewer.md
- /home/mustafa/src/Meridian/docs/risk/matching-semantics.md
- /home/mustafa/src/Meridian/tests/reference/matching_reference.py
- /home/mustafa/src/Meridian/src/matching.cpp
- /home/mustafa/src/Meridian/tests/integration/test_engine_vs_reference.cpp

Phase 1 task from your agent file:

1. Pick 10 worked examples from docs/risk/matching-semantics.md. Run them
   through both implementations:
   a. The C++ engine: write a temporary harness or use an existing
      integration scenario.
   b. The Python reference: run via tests/reference/test_reference.py or
      the run_reference.py CLI.

2. Write /home/mustafa/src/Meridian/docs/risk/audit-cases.md. For each of
   the 10 cases, record:
     - Case name (from matching-semantics.md heading).
     - Input event sequence.
     - Expected output (per matching-semantics.md).
     - C++ engine actual output.
     - Python reference actual output.
     - Status: PASS if both outputs match expected; FAIL otherwise.

3. Sign off Phase 1 only if every case passes in both implementations. If
   any case fails, escalate to the PM with the specific divergence; do not
   try to fix the divergence yourself.

Stay strictly in the role of Risk Reviewer. Do not write C++ code. Do not
write Python reference code. Your job is to verify, not to implement.

Report:
1. The path docs/risk/audit-cases.md.
2. The 10 case names and their status.
3. Sign-off statement: "Phase 1 matching correctness signed off." or the
   list of unresolved divergences blocking sign-off.
```

- [ ] **Step 2: Commit**

```bash
git add docs/risk/audit-cases.md
git commit -m "phase 1: risk and correctness audit cases

10 worked examples cross-verified against both the C++ engine and the
Python reference. All cases pass."
```

---

## Task 17: Dispatch Citation and Fact Auditor (17)

**Files:**
- Create (by dispatched agent): `docs/audits/citation-audit-phase-1-2026-05-09.md`

- [ ] **Step 1: Dispatch the agent**

```
You are the Citation and Fact Auditor agent for the Meridian project at
/home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/agents/17-citation-and-fact-auditor.md
- The Phase 0 audit at /home/mustafa/src/Meridian/docs/audits/citation-audit-2026-05-09.md
  (for tone, format, and the verified-vs-unverified-vs-wrong taxonomy).

Phase 1 task: audit every concrete claim in the documents that landed in
Phase 1:

  - /home/mustafa/src/Meridian/docs/risk/matching-semantics.md
  - /home/mustafa/src/Meridian/docs/risk/audit-cases.md
  - /home/mustafa/src/Meridian/docs/perf/budget.md
  - /home/mustafa/src/Meridian/docs/qa/regression-checklist.md
  - /home/mustafa/src/Meridian/docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md
    (this companion plan)

For each concrete claim (a number, a citation to a textbook section, a
file path, a function name, a class name, a CMake target, a behavior
statement), verify against:
  - the C++ source in /home/mustafa/src/Meridian/src and include/meridian
  - the Python reference in /home/mustafa/src/Meridian/tests/reference
  - the cited textbook or specification

File your report at
/home/mustafa/src/Meridian/docs/audits/citation-audit-phase-1-2026-05-09.md
following the same format as the Phase 0 audit. Each finding labeled
verified, unverified, or wrong. Wrong findings are phase-close blockers
unless explicitly waived by the PM with rationale captured in the audit
report itself.

Stay strictly in the role of Citation Auditor. Do not edit any document
to fix wrongs you find; report them to the PM. Do not write tests or
matching code.

Report:
1. The audit report path.
2. The verified/unverified/wrong counts.
3. The list of phase-close blockers (if any).
```

- [ ] **Step 2: Address findings**

For each `wrong` finding, fix the source document and re-run a tight audit pass on just the changed lines. Track resolutions in a follow-up commit.

- [ ] **Step 3: Commit**

```bash
git add docs/audits/citation-audit-phase-1-2026-05-09.md \
        <any documents fixed in response to findings>
git commit -m "phase 1: citation audit pass plus follow-up fixes

Citation Auditor reviewed matching-semantics.md, audit-cases.md, budget.md,
regression-checklist.md, and the companion plan. N verified, N unverified,
N wrong; wrongs resolved in this commit."
```

---

## Task 18: Dispatch Code Reviewer (11) for the Phase 1 PR

**Files:**
- (No files; the dispatched agent leaves PR review comments.)

- [ ] **Step 1: Open the Phase 1 PR**

Per `CLAUDE.md` "When the user says push" protocol:

```bash
cd /home/mustafa/src/Meridian
git push -u origin phase-1-companion-plan-and-amendments
gh pr create --title "phase 1: core data structures and single-symbol matching" \
             --body "$(cat <<'EOF'
## Summary

Phase 1 ships the matching engine's first real lines: `Order`, `Level`,
`Book`, `OrderPool`, plus `MatchingEngine::apply` for limit, market, IOC,
and cancel. Strict price-time priority (FIFO at each price level). Zero
heap allocation on the matching sweep, enforced by the debug allocator
tripwire in `OrderPool` plus `HotPathGuard`.

The Python reference under `tests/reference/` mirrors the same semantics
in obviously-correct form. The integration test diffs the C++ engine
against the reference on 60 scenarios.

## What shipped

- `include/meridian/types.hpp`: primitive types plus `Order` and
  `EngineEvent`. `static_assert(sizeof(Order) <= 64)`.
- `include/meridian/order_pool.hpp` and `src/order_pool.cpp`: free-list
  arena. Default capacity 1M slots. Debug-build operator new override
  aborts on heap allocation while `HotPathGuard` is armed.
- `include/meridian/level.hpp` and `src/level.cpp`: per-price FIFO queue.
- `include/meridian/book.hpp` and `src/book.cpp`: per-symbol L3 book with
  `std::map<Price, Level*>` per side plus `unordered_map<OrderId, Order*>`
  for cancel.
- `include/meridian/execution_report.hpp`: `ExecutionReport` plus report
  kinds and reject reasons.
- `include/meridian/matching.hpp` and `src/matching.cpp`: limit, market,
  IOC, cancel.
- `tests/reference/matching_reference.py` plus `tests/reference/test_reference.py`
  plus `tests/reference/README.md` plus `tests/reference/run_reference.py`.
- `tests/unit/test_order_pool.cpp`, `test_level.cpp`, `test_book.cpp`,
  `test_matching.cpp`.
- `tests/integration/test_engine_vs_reference.cpp` with 60 scenarios.
- `docs/risk/matching-semantics.md`, `docs/risk/audit-cases.md`,
  `docs/perf/budget.md`, `docs/qa/regression-checklist.md`,
  `docs/audits/citation-audit-phase-1-2026-05-09.md`.
- Design spec section 5.1 amended (Book is full L3 per symbol).
- `docs/plan.md` Phase 1 amended (`.hpp` extension, `std::map` per side
  not heap, cancel-by-id resolution noted).

## Sign-offs

- Quant Domain Validator: `docs/risk/matching-semantics.md` published.
- Reference Implementation Engineer: Python reference passes every worked
  example.
- QA Engineer: 60 integration scenarios pass; line coverage on libmeridian.a
  at N percent (>=85 percent target).
- Performance Engineer: `docs/perf/budget.md` published; bench skeleton
  compiles and runs.
- Risk and Financial Correctness Reviewer: 10 audit cases agree across
  both implementations; signed off.
- Citation and Fact Auditor: N verified, N unverified, no wrongs blocking
  phase close.

## Test plan

- [ ] `cmake --build build` succeeds in Debug and Release.
- [ ] `ctest --output-on-failure` passes in both configurations.
- [ ] Integration corpus (60 scenarios) passes.
- [ ] `meridian-bench --events 1000000` runs end to end.
EOF
)"
```

- [ ] **Step 2: Dispatch the Code Reviewer**

```
You are the Code Reviewer agent for the Meridian project at
/home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/agents/11-code-reviewer.md
- The PR diff for Phase 1 (use `gh pr diff <PR>` or `git diff main...HEAD`).

Review every C++ file under include/meridian/ and src/ for Phase 1.
Specifically audit:

1. Hot path discipline:
   - Zero heap allocations in the matching sweep (HotPathGuard scope).
   - No exceptions on the hot path (no try/catch, no functions that throw).
   - No virtual dispatch on the hot path.
   - [[likely]] / [[unlikely]] annotations used where the design spec
     suggests (e.g., empty book is unlikely; cancel on missing id is
     unlikely).

2. Type discipline:
   - sizeof(Order) <= 64 enforced via static_assert.
   - No GNU extensions; no `using namespace std`; no auto for return
     types unless deduction is genuinely the right call.

3. CMake hygiene:
   - -Wall -Wextra -Wpedantic -Werror on every target.
   - GoogleTest pinned to v1.15.2; HDRHistogram-c pinned to 0.11.8.

4. Test discipline:
   - Every implementation file has at least one paired unit test file.
   - The integration corpus diffs against the Python reference on 60
     scenarios (50 from matching-semantics.md plus 10 corner cases).

5. Documentation:
   - Inline doc comments only where the WHY is non-obvious (e.g.,
     [[unlikely]] reasoning, memory-ordering choices, why a particular
     branch is structured the way it is). Per CLAUDE.md, no comments
     describing what the code does.

Use the simplify skill (skills:simplify) for any file with obvious
simplification opportunities.

Leave PR review comments via `gh pr review --comment` for any concrete
finding. For each finding, name the file, the line range, and the
specific concern; do not comment generically.

If everything passes, leave a single approving review:
   gh pr review <PR> --approve --body "Phase 1 code review: approved.
   Hot path discipline OK. Coverage OK. CMake OK."

Report:
1. The number of comments left.
2. Approve or request changes.
3. Any unresolved concerns the PM should arbitrate.
```

- [ ] **Step 3: Address review comments**

If the Code Reviewer requests changes, the PM session addresses each comment with a follow-up commit on the same branch. Push and re-request review.

- [ ] **Step 4: Wait for required CI checks**

```bash
gh pr checks <PR> --watch --interval 10
```

Expected: every required check (`engine (clang)`, `engine (gcc)`, `frontend`) reports `pass`. The frontend job is a no-op for Phase 1 since no frontend changes shipped, but it still runs the smoke build.

---

## Task 19: Phase 1 close: STATUS.md flip, admin merge, mandatory check-in

**Files:**
- Modify: `STATUS.md`
- Modify: `docs/plan.md` (mark Phase 1 completed)

- [ ] **Step 1: Admin-merge the Phase 1 PR**

```bash
gh pr merge <PR> --squash --delete-branch --admin
git checkout main
git pull --ff-only
```

Verify the squash commit landed at the head of `main`.

- [ ] **Step 2: Update `STATUS.md`**

```markdown
**Last updated**: 2026-05-09 (Phase 1 closed: PR #N squash-merged to `main`
at <SHA>. Awaiting user check-in answer on whether to open Phase 2 in this
window, pause, or stop.).

## Next phase

**Phase 2: Multi-instrument and cancel-by-id (cross-symbol layer).**
Status: `not started`. Owner: Project Manager session, dispatching Engine
Developer (03), Reference Implementation Engineer (18), QA Engineer (10),
Risk and Financial Correctness Reviewer (16), and Code Reviewer (11).
Window cost: ~50 percent alone, or ~95 percent when bundled with Phase 3.
See `docs/plan.md` Phase 2 section for the detailed deliverable list.
```

In the phase status table, flip Phase 1's row to `completed` with the date.

Add a Resume notes entry capturing what shipped and where:

```markdown
### Phase 1 closeout (2026-05-09)

Phase 1 closed cleanly. PR #N (`phase 1: core data structures and
single-symbol matching`) squash-merged to `main` at <SHA>. The matching
engine now ships limit, market, IOC, and cancel for one symbol. The Python
reference agrees on 60 integration scenarios. Risk Reviewer signed off on
10 audit cases. Citation Auditor signed off on Phase 1 docs.

Outstanding deferred work:
- Per-symbol BookRegistry plus cross-symbol OrderIndex still pending
  (Phase 2).
- Property-based tests still pending (Phase 3).
- Bench is a working harness; the 6M events/sec target is enforced in
  Phase 5.
```

- [ ] **Step 3: Update `docs/plan.md` Phase 1 section**

Change the Phase 1 status line from `not started` to `completed 2026-05-09 (PR #N squash-merged at <SHA>)`. Append a `### Phase 1 close summary` paragraph.

- [ ] **Step 4: Commit on a small docs branch and admin-merge**

```bash
git checkout -b status-flip-phase-1-complete
git add STATUS.md docs/plan.md
git commit -m "phase 1: flip STATUS.md to completed; record next phase

Phase 1 closeout administrative commit. Updates STATUS.md table, sets
Next phase to Phase 2, files Resume notes for Phase 1, and marks
docs/plan.md Phase 1 section completed."
git push -u origin status-flip-phase-1-complete
gh pr create --title "phase 1: flip STATUS.md to completed; record next phase" \
             --body "Closeout admin commit. CI is a no-op for docs-only changes."
gh pr checks <PR> --watch --interval 10
gh pr merge <PR> --squash --delete-branch --admin
git checkout main && git pull --ff-only
```

- [ ] **Step 5: Run the mandatory check-in protocol from `CLAUDE.md`**

Send the user this message verbatim (substituting summary numbers):

```
Phase 1 is complete. Summary of what shipped: the matching engine's first
real lines under include/meridian/ and src/, with limit, market, IOC, and
cancel for one symbol; a Python reference under tests/reference/ that
agrees on 60 integration scenarios; 16 unit tests; the Performance
Engineer's latency budget; the Risk Reviewer's 10 audit cases; the
Citation Auditor's Phase 1 audit. Five logical commits squashed into one
merge commit. CI green.

Before I open Phase 2, please tell me:

1. What is your Claude Max usage at right now?
2. Roughly how long until your next reset window?
3. Do you want to continue into Phase 2 now, pause until the next window,
   or stop here for the day?

Phase 2 scope: multi-instrument support (BookRegistry symbol dispatch)
plus the cross-symbol O(1) cancel layer (OrderIndex). Estimated cost: ~50
percent of one window alone, or ~95 percent when bundled with Phase 3
(property-based tests for the matching invariants).
```

Wait for the user's explicit answer. Do not proceed to Phase 2 without it.

---

## Self-review

The PM ran a fresh-eyes pass over this plan after writing it.

**Spec coverage:**
- `docs/plan.md` Phase 1 outputs table: every row mapped to a task above (Tasks 5 through 14 plus the Reference Impl Engineer's Task 3 plus the QA dispatch in Task 15).
- `docs/plan.md` Phase 1 task list (1 through 9): each item is covered by an explicit task above.
- Phase 1 exit criteria: all six bullet points covered (unit tests pass, integration test against reference passes on the corpus, Risk Reviewer sign-off, Code Reviewer approval, no allocator override violations, bench compiles and runs).
- The five Phase 1 invariants from `matching-semantics.md`: deferred to Phase 3 property tests, captured as a checklist item the QA agent dispatch in Task 15 surfaces. The Quant Domain Validator's Task 2 dispatch instructs the agent to mark deferred invariants explicitly so QA does not try to test them in Phase 1.

**Placeholder scan:**
- No "TBD", "TODO", or "fill in" remain.
- The phrase "the dispatched agent in Task 15 expands the corpus" is concrete and intentional, not a placeholder.
- Sign-off counts and coverage percentages in the Task 18 PR body and Task 19 STATUS update are placeholders the PM fills in after Tasks 15-17 land. Marked with `N` so they are obvious to the agent that fills them in. This is intentional: the actual values depend on what the dispatched agents produce.

**Type consistency:**
- `Order` field names (`id`, `symbol`, `side`, `type`, `price`, `qty_remaining`, `ts_arrival`, `prev`, `next`) consistent across `types.hpp`, `order_pool.cpp`, `level.cpp`, `book.cpp`, `matching.cpp`, and tests.
- `ExecutionReport` field names (`kind`, `order_id`, `matched_id`, `side`, `price`, `qty`, `ts`, `reject_reason`) consistent across `execution_report.hpp`, `matching.cpp`, `report_to_jsonl` in the integration test, and the Reference Implementation Engineer's expected JSON shape.
- `Side` enum values (`Buy`, `Sell`) consistent. The Python reference uses `Side.BUY`/`Side.SELL` (uppercase enum convention in Python); the JSON shape uses lowercase string `"buy"`/`"sell"` everywhere.
- `OrderType` enum values (`Limit`, `Market`, `IOC`, `PostOnly`, `FOK`) consistent. JSON shape uses lowercase `"limit"`/`"market"`/`"ioc"`.
- `RejectReason` enum values (`None`, `EmptyBook`, `NotFound`, `InsufficientLiquidity`, `WouldCross`) consistent. JSON shape uses lowercase snake_case.

No issues found in the self-review.

---

## Execution Handoff

The PM session executes this plan inline rather than via subagent-driven-development. Tasks 5 through 14 (and any post-dispatch fixups) run in the current conversation. Task 2, 3, 4, 15, 16, 17, 18 are subagent dispatches per the project's PM dispatch model. Task 1 is a PM amendment and Task 19 is the closeout.

This matches the `CLAUDE.md` "When to dispatch vs play the role yourself" guidance: the matching engine implementation is long-lived iterative work that benefits from accumulated context.
