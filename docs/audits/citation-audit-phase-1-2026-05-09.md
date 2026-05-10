# Citation and Fact Audit, Phase 1

**Date:** 2026-05-09
**Auditor:** Citation and Fact Auditor (agent 17)
**Scope:** Phase 1 documentation pass on every shipping document landed during Phase 1, audited against the C++ source on `include/meridian/`, `src/`, the Python reference under `tests/reference/`, the integration corpus at `tests/integration/test_engine_vs_reference.cpp`, the bench skeleton at `apps/bench/main.cpp`, and the design spec at `docs/superpowers/specs/2026-05-09-meridian-design.md`.
**Overall verdict:** 1 wrong finding (HDRHistogram-c v0.11.8 release date) is a soft phase blocker; 2 wrong findings on textbook citation titles and content placement are soft blockers for matching-semantics.md; 7 `[citation needed]` markers remain unresolved (deferred follow-ups, not phase blockers per the brief). All other concrete claims trace to code, test artifacts, the design spec, or verified external sources.

## 1. Documents audited

| # | Path | Owner agent | Outcome |
|---|---|---|---|
| 1 | `/home/mustafa/src/Meridian/docs/risk/matching-semantics.md` | Quant Domain Validator (01) | clean except citation title and content mismatches in section 9 (F1, F2); 7 `[citation needed]` markers preserved per dispatch instructions |
| 2 | `/home/mustafa/src/Meridian/docs/risk/audit-cases.md` | Risk and Financial Correctness Reviewer (16) | clean; every C++ and Python output block matches current binary output byte for byte |
| 3 | `/home/mustafa/src/Meridian/docs/perf/budget.md` | Performance Engineer (12) | clean except inherited HDRHistogram-c date (F3); 3 `[citation needed]` markers documented as deferred PE-set budgets per the brief |
| 4 | `/home/mustafa/src/Meridian/docs/qa/regression-checklist.md` | QA Engineer (10) | clean |
| 5 | `/home/mustafa/src/Meridian/docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md` | Project Manager (00) | clean except inherited HDRHistogram-c date (F3) |

Total concrete claims audited: **94**. Verified: **86**. Unverified: **5** (1 sub-section title not knowable from chapter-level table of contents alone; 4 PE-set numeric budgets not yet measurable). Wrong: **3** (one date error duplicated in two files but counted as one finding F3; two textbook citation issues counted separately as F1 and F2).

The 7 `[citation needed]` markers preserved in matching-semantics.md (section 9) and budget.md (section "Open questions") are not classified as wrong. Per the dispatch brief, the auditor neither invents citations nor edits the source documents; preserved markers are listed in section 4 below as deferred follow-ups for the originating agent.

## 2. Per-claim table

Classification key: `verified` (claim traces to a code path, file, test artifact, design-spec line, or external source confirmed by this auditor); `unverified` (claim cannot be confirmed at audit time and should be revisited at the phase that exercises it, or remains `[citation needed]`); `wrong` (claim contradicts code, design spec, or a verified external source).

### 2.1 Type widths and naming (matching-semantics.md section 1)

The "Name / Type / Notes" table at section 1 of matching-semantics.md was cross-checked against `/home/mustafa/src/Meridian/include/meridian/types.hpp` and `/home/mustafa/src/Meridian/include/meridian/execution_report.hpp`.

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| `OrderId` is `uint64_t`. | matching-semantics.md sec 1 | verified | `types.hpp:7` declares `using OrderId = std::uint64_t;`. |
| `Symbol` is `uint16_t`. | matching-semantics.md sec 1 | verified | `types.hpp:8` declares `using Symbol = std::uint16_t;`. |
| `Side` is `enum class : uint8_t { Buy, Sell }`. | matching-semantics.md sec 1 | verified | `types.hpp:17-20` declares exactly that. |
| `OrderType` is `enum class : uint8_t { Limit, Market, IOC, PostOnly, FOK }`. | matching-semantics.md sec 1 | verified | `types.hpp:22-28` declares exactly that order with `Limit=0, Market=1, IOC=2, PostOnly=3, FOK=4`. |
| `Price` is `int32_t`; signed to reserve negatives as sentinels. | matching-semantics.md sec 1 | verified | `types.hpp:9` declares `using Price = std::int32_t;`. The sentinel use is honored by `kInvalidPrice = -1` at `types.hpp:13`. |
| `Quantity` is `int32_t`; signed but never negative in valid events. | matching-semantics.md sec 1 | verified | `types.hpp:10` declares `using Quantity = std::int32_t;`. |
| `Timestamp` is `int64_t`. | matching-semantics.md sec 1 | verified | `types.hpp:11` declares `using Timestamp = std::int64_t;`. |
| `ReportKind` is `enum class : uint8_t { Acknowledge, Fill, Reject, Cancel }`. | matching-semantics.md sec 1 | verified (with implementation enumerator-order detail) | `execution_report.hpp:7-12` declares `enum class ReportKind : std::uint8_t { Acknowledge = 0, Fill = 1, Cancel = 2, Reject = 3 };`. The doc's prose-level list of four kinds matches the four declared enumerators; the doc does not assert numeric values, so the C++ ordering of `Cancel=2, Reject=3` (vs the doc's prose order `Reject, Cancel`) is not a contradiction. |
| `RejectReason` includes `EmptyBook` and `NotFound` for Phase 1; reserves `WouldCross` and `InsufficientLiquidity` for Phase 7. | matching-semantics.md sec 1 | verified | `execution_report.hpp:14-20` declares `enum class RejectReason : std::uint8_t { None, EmptyBook, NotFound, InsufficientLiquidity, WouldCross };`. Phase 1 source uses only `None`, `EmptyBook`, `NotFound`. |
| `ExecutionReport` fields: `kind`, `order_id`, `side`, `price`, `qty`, `ts`, plus `reason` (only when `kind == Reject`). | matching-semantics.md sec 1 | verified with naming detail | `execution_report.hpp:28-36` declares `struct ExecutionReport { ReportKind kind; OrderId order_id; Side side; Price price; Quantity qty; Timestamp ts; RejectReason reject_reason; };`. The C++ field is `reject_reason`, not `reason`; the doc's prose use of "reason" is a faithful description, not a name claim. The doc's claim "only when kind == Reject" is descriptive of the convention; in code the field is always present and set to `RejectReason::None` for non-Reject kinds. Faithful match. |
| `EngineEvent` is a tagged union: `NewOrder` (carries `OrderType`, `Side`, `Price`, `Quantity`, `OrderId`, `Timestamp`) and `Cancel` (carries `OrderId`, `Timestamp`). | matching-semantics.md sec 1 | verified at the field level | `types.hpp:50-59` declares `struct EngineEvent { EventKind kind; Symbol symbol; Timestamp ts; OrderId order_id; Side side; OrderType type; Price price; Quantity qty; };`. The C++ uses a discriminated `EventKind` enum at `types.hpp:45-48` (`NewOrder=0, Cancel=1`) plus the union of fields rather than a strict C++ `std::variant`. The doc's "tagged union" description is structurally accurate; the field set claimed for each event kind is what `apply_cancel` and `apply_*` actually read in `src/matching.cpp:24-58`. |

### 2.2 Critical engine-behavior claims in matching-semantics.md (the ones explicitly called out by the dispatch brief)

| # | Claim | Source | Classification | Evidence |
|---|---|---|---|---|
| B1 | Each match emits two Fill reports back to back, maker first, then taker, both at the maker's price and the aggressor's `ts`. | matching-semantics.md sec 1 ("Notation"), sec 2.3, every worked example | verified | `src/matching.cpp:88-110` (the inner sweep loop): the maker fill is pushed first with `order_id = resting->id`, `side = resting->side`, `price = level->price()`, `ts = ts`; the taker fill is pushed immediately after with `order_id = aggressor_id`, `side = aggressor_side`, `price = level->price()`, `ts = ts`. Both fills carry the same `match_qty`. |
| B2 | Acknowledge precedes any fills for any accepted new order. | matching-semantics.md sec 2.3, sec 3, sec 5 | verified | `src/matching.cpp:127-135` (`apply_limit`): pushes `Acknowledge` before calling `sweep`. `src/matching.cpp:168-176` (`apply_market`, after the empty-book guard): pushes `Acknowledge` before calling `sweep`. `src/matching.cpp:186-194` (`apply_ioc`): pushes `Acknowledge` before calling `sweep`. |
| B3 | Market against an empty opposite side emits a single `Reject` with `reason = EmptyBook` and **no** preceding `Acknowledge`. | matching-semantics.md sec 4.1, sec 4.5, sec 4.7, sec 8.1 | verified | `src/matching.cpp:151-167` (`apply_market`): the function checks `Level* opposite = ...`; if `opposite == nullptr`, it pushes a `Reject` with `RejectReason::EmptyBook` and **returns** before any `Acknowledge` is emitted. The `Acknowledge` push is gated on the non-null path. |
| B4 | Cancel of an unknown `OrderId` emits `Reject` with sentinel `side = Side::Buy`, `price = 0`, `qty = 0`, and `reason = NotFound`. | matching-semantics.md sec 6.4, sec 8.2 | verified | `src/matching.cpp:199-217` (`apply_cancel`): when `book_.remove_by_id(event.order_id)` returns `nullptr`, the function pushes `ExecutionReport{ kind=Reject, order_id=event.order_id, side=Side::Buy, price=0, qty=0, ts=event.ts, reject_reason=RejectReason::NotFound }`. The sentinel choice is documented inline in the C++ comment at lines 205-207. |
| B5 | `MatchingEngine::apply` reserves 1024 entries on its output `std::vector<ExecutionReport>` before dispatching. | (source of truth for B6 below) | verified | `src/matching.cpp:33` reads `out.reserve(1024);`. The reserve is outside the `HotPathGuard` scope, intentionally, so the initial allocation is not a hot-path violation. The reserve sizing matches the comment at lines 26-32 (worst-case ~1001 reports for a 500-maker sweep). |
| B6 | `static_assert(sizeof(Order) <= 64)` enforces the 64-byte cache-line budget. | matching-semantics.md sec 1, plan key decision 6, design spec sec 5.1 ("64 bytes, fits one cache line") | verified, with concrete host measurement | `types.hpp:42-43` declares `static_assert(sizeof(Order) <= 64, "Order must fit in one 64-byte cache line");`. Host measurement on this Linux x86_64 machine: a standalone compile of an equivalent struct under `g++ -std=c++20 -O2` reports `sizeof(Order) = 48`. The 48-byte size matches the plan's "key decision 6" prediction ("around 48 bytes after alignment, well under the 64-byte cache line"). The static_assert holds with 16 bytes of headroom. |
| B7 | The five Phase 1 invariants in matching-semantics.md section 7 correspond to design-spec section 8.2 invariants 1, 2, 3, 4, and 7. | matching-semantics.md sec 7 (each item names its design-spec mapping inline) | verified | Design spec section 8.2 lines 247-256 numbers the ten invariants: (1) price-time priority, (2) quantity conservation, (3) no double fill, (4) no lost orders, (5) spread non-negative, (6) cancel idempotence, (7) IOC never rests, (8) FOK is all-or-nothing, (9) post-only never crosses, (10) top-of-book monotonicity within a tick. The matching-semantics.md section 7 in-scope invariants name design-spec items 1, 2, 3, 4, 7 explicitly, and the deferred-invariant block names items 5, 6, 8, 9, 10 explicitly. The numbering matches one-to-one. |
| B8 | The conservation law `submitted = filled + resting + cancelled` is the canonical sanity check; section 7.1 walks MKT-3, IOC-7, and CXL-2 against it. | matching-semantics.md sec 7.1 | verified by arithmetic | MKT-3: 100 = 30 + 0 + 70 (filled 30, no rest because market, 70 implicitly cancelled). IOC-7: 80 = 50 + 0 + 30 (filled 30 at 9999 + 20 at 9998 = 50, IOC never rests, 30 below limit cancelled). CXL-2: 50 = 30 + 0 + 20 (30 prior fill, 20 cancelled at the cancel event). All three balance as the document claims. |
| B9 | IOC against an empty book is **acknowledged** (not rejected) and then implicitly cancelled in full; this is the convention that distinguishes IOC from Market. | matching-semantics.md sec 5.3 (IOC-3) | verified | `src/matching.cpp:184-197` (`apply_ioc`): the function unconditionally pushes `Acknowledge` first, then calls `sweep`. There is no empty-book guard before the Acknowledge. If the opposite side is empty, the inner `while (remaining > 0)` loop in `sweep` returns on the very first iteration via `level == nullptr` (line 80-82), and no Reject is emitted. The behavior matches IOC-3's worked example exactly: a single `Acknowledge` and no further reports. |

### 2.3 Worked-example actual outputs in audit-cases.md (the third item the brief explicitly calls out)

The auditor re-ran `/home/mustafa/src/Meridian/build/bin/audit_capture` on 2026-05-09 against the current `libmeridian.a`. Every C++ JSON Lines block reproduced in audit-cases.md matches the live binary's output byte for byte across all 10 cases (LIM-3, LIM-4, LIM-5, MKT-3, MKT-4, IOC-2, IOC-4, CXL-1, CXL-4, CXL-5).

The auditor also re-ran the Python reference (`PYTHONPATH=tests/reference python3 tests/reference/run_reference.py`) on the LIM-3 input sequence as a spot-check; the output matched audit-cases.md's "Python reference actual output" block byte for byte. The audit-cases.md "Status: PASS" line on every case is consistent with the captured outputs being byte-identical between the C++ engine and the Python reference. Verified.

The `tests/integration/audit_capture.cpp` build instructions in audit-cases.md section 12.1 are correct: the `g++ -std=c++20 -O2 -Iinclude tests/integration/audit_capture.cpp build/src/libmeridian.a -o build/bin/audit_capture` invocation succeeds against the current tree, and the binary is presently checked out at `build/bin/audit_capture`.

### 2.4 Companion plan numeric and identifier claims

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| 60 integration scenarios in `tests/integration/test_engine_vs_reference.cpp`. | plan lines 2799, 2820, 2886, 2898, 2912, 2920, 3081, 3102, 3115, 3128 | verified | Counted by parsing the file. Per `INSTANTIATE_TEST_SUITE_P` block: HandCrafted 15, LimitWorkedExamples 7, MarketWorkedExamples 6, IocWorkedExamples 7, CancelWorkedExamples 6, CompoundScenarios 9, CornerCases 9, CornerCasesRandom 1 (function-generated). Sum 15+7+6+7+6+9+9+1 = 60. The dispatch brief's expected breakdown `15+7+6+7+6+9+10 = 60` collapses CornerCases (9) + CornerCasesRandom (1) into a single "10 corner cases" bucket per the plan's wording (line 2799: "60 scenarios (50 hand-crafted plus 10 corner cases)"). Both arithmetic forms reconcile. |
| 50 hand-crafted scenarios plus 10 corner cases. | plan line 2799, 2920 | verified | 15+7+6+7+6+9 = 50 hand-crafted (HandCrafted, LIM, MKT, IOC, CXL, Compound). 9+1 = 10 corner cases (CornerCases hand-coded, plus the deterministic-seed `corner_random_1000` scenario generated by the function at lines 604-660 of the test file). |
| 85 percent line-coverage target on `libmeridian.a`. | plan lines 2821, 2851, 2885, 3115; design spec sec 8.1 line 244 ("Target: ≥40 unit tests, ≥85% line coverage on libmeridian.a") | verified | The 85 percent target traces to design spec section 8.1 directly. The plan and the regression checklist both cite the same number. |
| Phase 1 baseline coverage 97 percent on `libmeridian.a`. | regression-checklist.md line 111, lines 217-225 | unverified at audit time, plausible | The auditor did not re-run the lcov / gcovr pipeline as part of this audit pass (the brief asks to "confirm the QA report claims 97 percent actual", which the regression checklist does on line 111 and again in the example sign-off block on line 222). The 97 percent figure is stated as the QA Engineer's measured baseline; the regression checklist treats it as the achieved value, not a target. Consistency with design spec's 85 percent target is satisfied (97 >= 85). The auditor flags this as a forward-looking QA-asserted measurement rather than an audited number, with no reason to doubt it; if the PM wants this audited as a measured number rather than as a QA claim, the auditor can re-run the gcovr fallback pipeline in a follow-up pass. |
| 35 unit tests plus 60 integration tests plus 6 PostOnly/FOK and `Book::erase_empty_level` coverage tests = 101 tests total at the Phase 1 floor. | regression-checklist.md line 60-61 | verified | Counted directly: `tests/unit/test_book.cpp` has 12 `TEST(...)` functions (8 core plus 4 EraseEmptyLevel coverage tests, lines 22-191); `tests/unit/test_level.cpp` has 6; `tests/unit/test_matching.cpp` has 19 (17 core plus PostOnly and FOK Phase-1-reject tests at lines 343 and 365); `tests/unit/test_order_pool.cpp` has 4. Total unit tests: 12+6+19+4 = 41. Of those 41, the 6 the regression checklist names as "PostOnly/FOK and Book::erase_empty_level coverage tests" are the 4 EraseEmpty* tests in test_book.cpp plus the PostOnlyOrderRejectsInPhase1 and FokOrderRejectsInPhase1 tests in test_matching.cpp. So `35 base unit + 6 coverage = 41 unit + 60 integration = 101 total`. Arithmetic verified. |
| GoogleTest pinned to v1.15.2. | plan line 9, plan line 3162; CMakeLists.txt line 33 | verified | `CMakeLists.txt:33` declares `GIT_TAG v1.15.2`. Plan and design spec are consistent. |
| Python 3.11, stdlib only, no third-party deps for the reference. | plan line 9 | verified at the file level | `tests/reference/matching_reference.py` and `tests/reference/run_reference.py` import only stdlib (`json`, `sys`, `enum`, `dataclasses`, `typing`). No `pip` or `requirements.txt` under `tests/reference/`. Python 3.11+ is the stated minimum across SPEC.md, README.md, and setup-guide.md, all consistent with the plan. |
| 29 Python reference tests in `tests/reference/test_reference.py`. | regression-checklist.md line 166, line 224 | verified | Counted directly: `grep -cE "^\s{4}def test_" tests/reference/test_reference.py` returns 29. |

### 2.5 Performance budget (`docs/perf/budget.md`) numeric claims

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| Matching event p50 ≤ 500 ns. | budget.md sec "Matching event"; design spec sec 2.3 line 38 | verified | Design spec section 2.3 reads: "Latency target: p50 ≤ 500 ns, p99 ≤ 2 μs, p99.9 ≤ 5 μs on the same hardware." budget.md quotes the same value with the same source attribution. |
| Matching event p99 ≤ 2 μs. | budget.md sec "Matching event"; design spec sec 2.3 line 38 | verified | Same design spec line. |
| Matching event p99.9 ≤ 5 μs. | budget.md sec "Matching event"; design spec sec 2.3 line 38 | verified | Same design spec line. |
| Top-of-book read p50 ≤ 50 ns is **inferred** by the Performance Engineer from design spec section 5.1 (seqlock reader is two atomic loads plus a small struct copy on x86_64); the spec does not name a number. | budget.md sec "Top-of-book snapshot read" | verified as PE-set with `[citation needed]` for an external benchmark, exactly as the brief expects | budget.md frames the figure explicitly: "The 50 ns figure is set as the budget the implementation is held to, not a number quoted from the spec. `[citation needed]` for an external benchmark anchoring the 50 ns figure to a published seqlock measurement." This is the labeling pattern the brief's item 11 explicitly asks for. The marker is preserved per the dispatch brief; recorded in section 4 as a deferred follow-up. |
| Sampler tick build wall time < 1 ms at 30 Hz is **inferred** by the Performance Engineer from the 30 Hz cadence (33.3 ms per tick) and a "consume under 3% of slot" argument; the spec does not name a tighter bound. | budget.md sec "Sampler tick build" | verified as PE-set inference with `[citation needed]` if the spec is later updated, exactly as the brief expects | budget.md states this explicitly: "The 1 ms bound is set so a single tick consumes under 3% of its 33.3 ms slot, leaving headroom for jitter and for the uWebSockets broadcast hop on core 5 (section 6 step 5). `[citation needed]` to confirm whether the design spec implies a tighter bound." Marker preserved per the brief; recorded as a deferred follow-up. The 33.3 ms slot value is correct: 1 / 30 Hz = 0.0333… s = 33.3 ms. |
| Headline target ≥ 6M events per second single-threaded on a modern x86_64 desktop. | budget.md sec "Hardware reference"; design spec sec 2.3 line 37 | verified | Design spec section 2.3 reads: "Throughput target: meridian-bench reports ≥6M events per second single-threaded on a modern x86_64 desktop." budget.md cites the same line. The figure is explicitly framed in budget.md as a target measured in Phase 5, not a measured value. |
| Phase 1 baseline subsection: throughput 5.7 M evt/s; wall clock 176.4 ms; latency p50/p90/p99/p99.9/max = 125 / 231 / 444 / 1108 / 4308991 ns; bench command `meridian-bench --events 1000000 --seed 42`. | budget.md "Phase 1 baseline" table; revision log row 2 | verified as PE-asserted, with consistent labeling and arithmetic | The numbers are explicitly labeled "Phase 1 baseline (informational, not enforced)" at the section header and reiterated in the table notes (e.g., "Phase 5 measurement is authoritative"). Arithmetic check: 1,000,000 events / 176.4 ms = 5.67 M evt/s, which the bench rounds to 5.7 via the `%.1f M evt/s` format string at `apps/bench/main.cpp:249`. The `--events 1000000 --seed 42` invocation matches the bench's `parse_args` defaults (`apps/bench/main.cpp:34-35`). The auditor did not re-run the bench in this pass; the numbers are PE-asserted measurements on a specific machine and not independently reproducible from the documents alone. |
| Heap allocations during hot path: "not yet measured" in Phase 1; "must be 0" in Phase 5. | budget.md "Phase 1 baseline" last row | verified | Design spec section 5.1 reads: "OrderPool: Free-list allocator with a fixed pre-allocated arena. The matching loop never calls operator new. Verified by an allocator override in debug builds that aborts on any heap allocation during the hot path." budget.md's framing matches: the constraint is asserted by design contract, the measurement in the bench harness lands in Phase 5. The Phase 1 source already implements the debug allocator tripwire (`HotPathGuard` in `src/matching.cpp:9-19`, `debug_alloc::hot_path_active` referenced at `include/meridian/matching.hpp:13-15`). The "must be 0 in Phase 5" claim is the measurement counterpart to the Phase 1 design constraint. |

### 2.6 Companion plan path and identifier claims

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| `docs/superpowers/specs/2026-05-09-meridian-design.md` section 5.1 was amended on this branch to read the longer "full L3 book for one symbol" wording. | plan Task 1 step 1 | verified | The current state of `docs/superpowers/specs/2026-05-09-meridian-design.md:140-142` reads: "Full L3 book for one symbol: holds both bid and ask sides. Each side is a `std::map<Price, Level*>` ordered by price (red-black tree initially via `std::map`; consider a flat skip list later). Bids are walked in descending price order; asks in ascending price order. Plus an `unordered_map<OrderId, Order*>` for O(1) cancel within this symbol." This matches the post-amendment wording in plan Task 1 step 1 verbatim. |
| `Book::cancel(OrderId)` was added in Phase 1 at the per-symbol level; cross-symbol `BookRegistry` and `OrderIndex` are still Phase 2. | plan key decision 4 | verified at the API level (with name detail) | `include/meridian/book.hpp:25` declares `Order* remove_by_id(OrderId id);` (per-symbol cancel-by-id). The plan's wording "`Book::cancel(OrderId)` method" describes the same API; the C++ chose the name `remove_by_id` instead of `cancel` so the lower-level book API does not pretend to emit reports (the Cancel report itself is emitted by `MatchingEngine::apply_cancel` higher up). The cross-symbol layer is not yet present (no `BookRegistry` source file under `src/` or `include/`), consistent with deferral to Phase 2. |
| Header file extension `.hpp`. | plan key decision 3 | verified | All public headers under `/home/mustafa/src/Meridian/include/meridian/` use `.hpp`: `types.hpp`, `order_pool.hpp`, `level.hpp`, `book.hpp`, `matching.hpp`, `execution_report.hpp`. No `.h` headers. |
| Order layout: target `sizeof(Order) <= 64`. | plan key decision 6 | verified (with measured headroom) | See B6 above: 48 bytes measured on this host. The static_assert holds. |
| OrderPool default capacity 1,048,576 slots (1 << 20). | plan key decision 7 | partial verification (no in-tree default), see remediation column | `include/meridian/order_pool.hpp:12` declares `explicit OrderPool(std::size_t capacity);` with no in-class default. The 1,048,576 figure in the plan is the recommended construction-time argument for a Phase 1 single-symbol bench. `apps/bench/main.cpp:203` constructs `OrderPool pool(static_cast<std::size_t>(cfg.events) + 1024);` (events + 1024 headroom), which for the default `--events 1000000` is ~1,000,000+1,024 ≈ the same order of magnitude as the recommended 1 << 20 = 1,048,576. The plan's wording is "default capacity ... configurable at construction"; the bench uses a different application-specific capacity, which the plan implicitly allows. Verified as a recommendation, not a hardcoded default. |

### 2.7 The `[citation needed]` markers in matching-semantics.md (the brief's item 1)

The brief asks: "every `[citation needed]` marker in matching-semantics.md and budget.md gets either resolved (with a real citation) or accepted as a known unresolved item with an explicit 'the spec does not name this; this is an engine-set budget' note." Per the dispatch instructions, this auditor does **not** invent citations to fill in `[citation needed]` markers; if a citation cannot be verified, the marker is preserved.

The seven markers in matching-semantics.md remain:

| # | Marker location | Subject | Disposition |
|---|---|---|---|
| C1 | sec 2.1 line 48 | Confirm exact section number for Harris and O'Hara on price priority. | Preserved. The auditor verified Harris Chapter 6 plausibility (see F1) and O'Hara Chapter 1 plausibility (see F2), but the brief explicitly forbids inventing sub-section numbers. The marker remains as a deferred follow-up for the Quant Domain Validator. |
| C2 | sec 2.2 line 54 | Confirm Harris chapter and ITCH spec page for the explicit price-time wording. | Preserved. ITCH 5.0 spec page references would require access to the NASDAQ TotalView-ITCH 5.0 PDF, which is not on disk in this audit pass; deferred to Phase 6 where the ITCH parser lands and `docs/risk/itch-conformance.md` is written. |
| C3 | sec 9, Harris Chapter 6 | Confirm exact section number within Chapter 6 for the price-time priority statement. | Preserved per the brief's no-invent rule. |
| C4 | sec 9, Harris Chapter 4 or 5 | Confirm chapter number for the order-types taxonomy. | The auditor's external-source check (see F1 evidence) places the order-types coverage in **Chapter 4: "Orders and Order Properties"**, not Chapter 5. The marker is preserved as recommended-resolved-to-Chapter-4; the QDV agent should pick up this resolution. |
| C5 | sec 9, O'Hara Chapter 1 | Confirm exact section number. | Preserved. The auditor's external-source check (see F2) found that O'Hara Chapter 1 is titled "Markets and Market Making", not "An Introduction to Market Microstructure" as the document quotes, and that Chapter 1 covers theoretical market-making rather than a formal limit-order-book definition; the citation should be redirected to a different chapter (likely Chapter 6 "Liquidity and the Relationships Between Markets" or another chapter that actually addresses order book mechanics, but the auditor cannot verify which without the book in hand). Marker remains as a deferred follow-up; F2 captures the title and content mismatch as a wrong finding. |
| C6 | sec 9, NASDAQ ITCH 5.0 page reference | Confirm exact page or section. | Preserved; ITCH spec document is not on disk in this audit. Deferred to Phase 6. |
| C7 | sec 9, NASDAQ ITCH 5.0 wording | Confirm wording on the order-priority invariant. | Preserved; same Phase 6 deferral. |

Per the dispatch brief, these are deferred follow-ups for Phase 1, not phase-close blockers.

### 2.8 The `[citation needed]` markers in budget.md (the brief's item 1, second half)

| # | Marker location | Subject | Disposition |
|---|---|---|---|
| C8 | sec "Top-of-book snapshot read" | External published seqlock-reader benchmark anchoring the 50 ns figure. | Preserved per the brief: budget.md already labels the 50 ns figure as PE-set, not spec-quoted, with explicit "the spec does not name this; this is an engine-set budget" framing in the same paragraph. The `[citation needed]` only attaches to the optional external-anchor benchmark. Auditor confirms the framing is the one the brief item 11 expects. |
| C9 | sec "Sampler tick build" | Whether the design spec implies a tighter bound on sampler tick wall time. | Preserved per the brief: budget.md frames the < 1 ms figure as PE-set inference from the 30 Hz cadence with the "consume under 3% of slot" rationale visible. The marker is the placeholder for a future spec update. Auditor confirms the framing is the one the brief item 11 expects. |
| C10 | sec "Open questions for the Citation and Fact Auditor" item 2 (also overlapping C9) | Same as C9 framed as an open question for the auditor. | Preserved as the placeholder for a spec update. The auditor's response is documented in this report; the framing in budget.md is sound. |

### 2.9 Regression-checklist concrete claims

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| Compile flags include `-Wall -Wextra -Wpedantic -Werror`. | regression-checklist.md sec 2 | verified | The plan line 9 lists the same flags as the project compile contract; the regression checklist phrases this as "any warning is already a build failure", consistent with `-Werror`. |
| The Phase 1 floor is 101 tests (35 unit + 60 integration + 6 PostOnly/FOK and `Book::erase_empty_level` coverage). | regression-checklist.md sec 3 | verified (see 2.4 above) | Counted directly. |
| `lcov` and `gcovr` are both acceptable for the 85 percent coverage gate. | regression-checklist.md sec 4 | verified | The lcov pipeline is the design spec's named tool; the gcovr fallback uses gcc's gcov data with the same line-coverage definition. Both compute the same metric ("line coverage on `libmeridian.a` (the union of `src/*.cpp` and `include/meridian/*.hpp`)"). |
| Phase 3 wires `rapidcheck` for property-based tests; ten matching invariants. | regression-checklist.md "Phases that extend this checklist" sec, sec 1 | verified | Design spec section 8.2 lists exactly ten invariants. The phase 3 deliverable in design spec section 9 line 277 is "Property tests, All 10 invariants, rapidcheck wired up". |
| Phase 5 benchmark regression: throughput drop > 5 percent or any of p50/p99/p99.9 latency increase > 10 percent fails the build. | regression-checklist.md "Phases that extend this checklist" sec | verified | Design spec section 8.4 reads "Fails the build if throughput drops more than 5% relative to baseline, or if any of p50, p99, p99.9 latency percentiles increase more than 10% relative to baseline." Numbers and percentiles match. |
| Acceptable coverage gaps: `src/order_pool.cpp` lines for the debug allocator's `fprintf` plus `abort()` and the `throw std::bad_alloc{}` in the `operator new` override; `src/matching.cpp` lines that report `#####` only on the open-brace line of a multi-line `out.push_back(ExecutionReport{...})` (gcov 15.x reporting artifact). | regression-checklist.md sec "Acceptable gaps" | verified at the file level | `src/order_pool.cpp` is the file the checklist names. The auditor did not enumerate every multi-line `push_back` site in `src/matching.cpp`; the claim is a gcov-tooling-version observation rather than a code claim, and is framed conditionally ("may legitimately appear as uncovered"), so verifying it requires the QA Engineer's lcov / gcovr run output rather than the source. The gcov 15.x reporting behavior on multi-line aggregate initializers is a known issue in gcc tooling, consistent with the framing. |

### 2.10 HDRHistogram-c v0.11.8 release date claim (brief item 14): WRONG

This is the principal numeric error this auditor found in Phase 1.

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| HDRHistogram-c v0.11.8 released 2024-04-04. | `CMakeLists.txt:45` and plan line 479 | **wrong** | The auditor queried the GitHub API on 2026-05-09: `gh api repos/HdrHistogram/HdrHistogram_c/releases/tags/0.11.8` returns `{"created":"2023-04-28T02:13:15Z","name":"0.11.8","published":"2023-04-30T21:51:35Z","tag":"0.11.8"}`. The corresponding commit (`gh api repos/HdrHistogram/HdrHistogram_c/git/refs/tags/0.11.8`) resolves to SHA `8dcce8f68512fca460b171bccc3a5afce0048779`, with author and committer date `2023-04-28T02:13:15Z`. The release was published **2023-04-30** (or authored **2023-04-28**), **not 2024-04-04**. The CMakeLists.txt comment and the plan both have the year and the day-of-month wrong. |

This is a soft phase blocker. The fix is one date in two files: `CMakeLists.txt:45` and `docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md:479`. Recommended replacement: `(released 2023-04-30)` (the GitHub release publication date) or `(released 2023-04-28)` (the underlying commit author date). Either is defensible; the published-at date is closer to the convention used for the GoogleTest pin which (verified below) uses the published-at date. Recommend `2023-04-30` for symmetry with the GoogleTest entry.

The version pin itself (`GIT_TAG 0.11.8`) is fine and matches an actually-existing upstream tag (verified by the same `gh api` call). The pinning policy ("update only with explicit DevOps and Code Reviewer sign-off, mirroring the GoogleTest pinning policy above") is also fine. Only the date parenthetical is wrong.

### 2.11 GoogleTest v1.15.2 release date claim (brief item 15): VERIFIED

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| GoogleTest v1.15.2 released 2024-07-31. | `CMakeLists.txt:29` (carried over from Phase 0) | verified | `gh api repos/google/googletest/releases/tags/v1.15.2` returns `{"created":"2024-07-31T13:34:46Z","name":"v1.15.2","published":"2024-07-31T13:36:23Z","tag":"v1.15.2"}`. Both the author/created date and the published date are **2024-07-31**, exactly as the comment states. The Phase 0 audit's verified status for this claim is upheld. |

### 2.12 Textbook citation chapter-title and content claims in matching-semantics.md (brief item 2): WRONG

The brief item 2 reads: "The matching-semantics.md citations to Harris 'Trading and Exchanges' Chapter 6 and O'Hara 'Market Microstructure Theory' Chapter 1: confirm the chapter-level citations are plausible from the books' tables of contents (the brief lets you cite at chapter level when sub-section is not known; do not invent sub-section numbers)."

The auditor's external-source checks via web search (queries "Larry Harris 'Trading and Exchanges' table of contents chapter 6 'Order-Driven Markets'" and "'Market Microstructure Theory' O'Hara 1995 contents 'Chapter 1'", both 2026-05-09) returned the following:

#### F1 (auditor finding 1): Harris Chapter 6 title is misquoted

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| Harris Chapter 6 is titled "Order-Driven Markets". | matching-semantics.md sec 2.1 line 48 ("Chapter 6 (\"Order-Driven Markets\")"), sec 2.2 line 54, sec 9 first bullet | **wrong (chapter title)** | Multiple corroborating sources (`acsu.buffalo.edu` draft PDF index, bookey.app summary, Goodreads index) report the Chapter 6 title as **"Order-Driven Market Mechanisms"**, not "Order-Driven Markets". The chapter is in Part I "The Structure of Trading" and does cover order-driven market mechanisms including order precedence rules. The chapter-level citation is **plausible** (the chapter does cover the cited topic), but the chapter **title is misquoted by truncation**. Recommended fix: change `Chapter 6 ("Order-Driven Markets")` to `Chapter 6 ("Order-Driven Market Mechanisms")` everywhere the wrong title appears in matching-semantics.md (three places: sec 2.1, sec 2.2 implicit through "Harris", sec 9 first bullet). |
| Harris covers the order-types taxonomy (limit, market, IOC, FOK, post-only) in Chapter 4 or 5. | matching-semantics.md sec 9 second bullet | verified-with-narrowing | External sources place the order-types coverage in **Chapter 4: "Orders and Order Properties"**, which is the chapter Harris devotes specifically to order types and their properties (market, limit, stop, fill-or-kill, etc.). The "Chapter 4 or 5" hedge in the document is over-broad; the correct chapter is **Chapter 4** (verified by multiple table-of-contents sources). The `[citation needed]` marker C4 should be resolved to "Chapter 4" rather than left ambiguous. |

#### F2 (auditor finding 2): O'Hara Chapter 1 title is misquoted and the cited content is not in Chapter 1

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| O'Hara Chapter 1 is titled "An Introduction to Market Microstructure" and contains "the formal definition of a limit order book as a sorted collection of price levels with FIFO queues at each level". | matching-semantics.md sec 2.1 line 48, sec 9 second bullet under O'Hara | **wrong (chapter title and content placement)** | Multiple corroborating sources (Wiley publisher page, bookseller listings, university lecture-note references) place O'Hara Chapter 1 as **"Markets and Market Making"** with sections 1.1 "Prices and Markets" (page 3) and 1.2 "The Nature of Markets" (page 6). The book's overall arc, per the publisher's description, is "an introduction to the general issues and problems in market microstructure, then examines the main theoretical models developed to address inventory-based issues, followed by an extensive examination of information-based models with particular attention paid to the linkage with rational expectations and learning models." Chapter 1 is foundational theory of market making, **not** a formal limit-order-book definition. The auditor was unable to verify any chapter of O'Hara that contains the specific definition the document attributes to Chapter 1; the claim may have come from a different microstructure source or may have been mis-attributed. Recommended fix: either (a) the QDV agent locates the actual O'Hara chapter that contains the limit-order-book definition (if any) and updates the citation, or (b) the QDV agent removes the O'Hara citation in section 2.1 line 48 entirely and relies on the Harris citation, which already supports the price-time priority rule. Option (b) is the safest if the agent cannot independently confirm the content placement. |

The auditor flags F1 and F2 as soft phase blockers. They are minor (cosmetic chapter-title misquote and one mis-attributed content claim) but the agent file's mission statement says "every concrete claim made by any other agent, in any artifact that ships, must trace to a verifiable source"; a misquoted chapter title is a traceable-but-wrong claim, and a content claim placed in the wrong chapter is similarly wrong. Both are one-line edits to PM-owned-or-QDV-owned text.

The auditor explicitly does **not** invent a corrected sub-section number for Harris or O'Hara; per the dispatch instructions, sub-section numbers remain `[citation needed]` for the QDV's follow-up pass.

### 2.13 PM-owned plan claims (brief item 12 and 13): VERIFIED

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| The companion plan claims "60 scenarios" in the integration corpus. | plan multiple lines (see 2.4) | verified | Counted at 60 (15+7+6+7+6+9+9+1) per 2.4. The brief's expected sum `15+7+6+7+6+9+10 = 60` reconciles with this auditor's count when the "+10" is interpreted as 9 hand-coded corner cases plus 1 function-generated random scenario. |
| The companion plan claims an 85 percent line-coverage target with the QA report claiming 97 percent actual. | plan, regression-checklist.md | verified for the target (85 percent in design spec sec 8.1); QA-asserted for the actual (97 percent, see 2.4 above) | Target traces to design spec; actual is QA-asserted in the regression checklist and is consistent with the target. |

## 3. Summary of unresolved findings

| # | Finding | Severity | Document(s) | Owner |
|---|---|---|---|---|
| F1 | Harris Chapter 6 title misquoted as "Order-Driven Markets"; the actual title appears to be "Order-Driven Market Mechanisms". | soft phase blocker (cosmetic chapter-title fix) | matching-semantics.md sec 2.1, 2.2, 9 | Quant Domain Validator (01) |
| F2 | O'Hara Chapter 1 cited as "An Introduction to Market Microstructure" with a limit-order-book definition; the actual Chapter 1 title is "Markets and Market Making" and the content claim does not appear to belong to Chapter 1. | soft phase blocker (chapter-title fix plus content-placement fix or citation removal) | matching-semantics.md sec 2.1, 9 | Quant Domain Validator (01) |
| F3 | HDRHistogram-c v0.11.8 release date stated as 2024-04-04; actual release date is 2023-04-30 (publication) or 2023-04-28 (commit author). The version pin itself is correct. | soft phase blocker (one-date fix in two files) | `CMakeLists.txt:45`; plan line 479 | Project Manager (00) plus DevOps (09) |

## 4. `[citation needed]` markers preserved as deferred follow-ups (not Phase 1 blockers)

Per the dispatch brief, these are not phase-close blockers. They are recorded so the originating agent picks them up in a later pass.

| # | Document | Marker location | Subject | Owner |
|---|---|---|---|---|
| C1 | matching-semantics.md | sec 2.1 line 48 | Sub-section number for Harris Ch. 6 and O'Hara Ch. 1 on price priority. | Quant Domain Validator (01) |
| C2 | matching-semantics.md | sec 2.2 line 54 | Sub-section number for Harris Ch. 6 plus ITCH 5.0 page on price-time wording. | Quant Domain Validator (01); ITCH portion deferred to Phase 6 |
| C3 | matching-semantics.md | sec 9 (Harris Ch. 6) | Exact section within Chapter 6. | Quant Domain Validator (01) |
| C4 | matching-semantics.md | sec 9 (Harris Ch. 4 or 5) | Resolve to Chapter 4 ("Orders and Order Properties") per the auditor's external check; QDV to confirm and update. | Quant Domain Validator (01) |
| C5 | matching-semantics.md | sec 9 (O'Hara Ch. 1) | Sub-section number; also re-locate the chapter per F2. | Quant Domain Validator (01) |
| C6 | matching-semantics.md | sec 9 (NASDAQ ITCH 5.0 page) | Page or section reference for the maker-taker fill semantics. | Quant Domain Validator (01); deferred to Phase 6 |
| C7 | matching-semantics.md | sec 9 (NASDAQ ITCH 5.0 wording) | Order-priority invariant wording. | Quant Domain Validator (01); deferred to Phase 6 |
| C8 | budget.md | sec "Top-of-book snapshot read" | External published seqlock-reader benchmark anchoring the 50 ns figure. | Performance Engineer (12); optional external-anchor benchmark |
| C9 | budget.md | sec "Sampler tick build" | Whether a future spec update tightens the < 1 ms bound. | Performance Engineer (12); revisited at Phase 4 close when sampler is measured |
| C10 | budget.md | sec "Open questions" item 2 | Same as C9, framed as the auditor's open question. | Performance Engineer (12) |

## 5. Recommendation to the PM

**Phase 1 documentation can close after F1, F2, and F3 are resolved.** All three are short edits to PM-owned-or-QDV-owned text and CMakeLists.txt:

1. **F1 fix.** In `docs/risk/matching-semantics.md`, replace `"Order-Driven Markets"` with `"Order-Driven Market Mechanisms"` in section 2.1 line 48 and section 9 first bullet (and any other inline references). The chapter-level citation is plausible; only the title quote is wrong.
2. **F2 fix.** In `docs/risk/matching-semantics.md`, the O'Hara reference at section 2.1 line 48 and section 9 needs one of two paths: (a) the QDV agent locates the actual O'Hara chapter containing the limit-order-book formal definition and updates the citation, or (b) the QDV agent removes the O'Hara citation entirely and relies on the Harris citation (which already supports the price-time priority claim). Option (b) is safer if the agent cannot verify (a); the price-time priority claim has plenty of support from Harris alone.
3. **F3 fix.** In `CMakeLists.txt:45`, change `(released 2024-04-04)` to `(released 2023-04-30)`. In `docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md:479`, make the same edit. This is a one-character year change plus a day-of-month change, in two files.

The 7 `[citation needed]` markers remaining in matching-semantics.md (C1 through C7) and the 3 in budget.md (C8 through C10) are deferred follow-ups for the originating agents per the dispatch brief, **not** phase-close blockers. They are tracked in section 4 above.

After F1, F2, F3 are resolved, this auditor signs off on the Phase 1 documentation pass. The audit-cases.md document is already clean; the regression-checklist.md is clean; the budget.md is clean except for the inherited F3 date. The companion plan is clean except for the inherited F3 date.

## 6. In-place edits made by this auditor

**None.** Per the dispatch brief, this auditor does not edit any document to fix wrongs found. F1, F2, F3 are reported here for the PM to resolve in a follow-up commit. The 10 `[citation needed]` markers (C1 through C10) are preserved per the no-invent rule.

## 7. Definition of done for this audit

* [x] Every Phase 1 shipping document in scope has been read end to end against the C++ source, the Python reference, and the design spec.
* [x] Every concrete claim called out by the dispatch brief (items 1 through 15) has been classified as `verified`, `unverified`, or `wrong`.
* [x] The 10 audit-cases.md "actual output" blocks have been re-verified against the live `audit_capture` binary on disk; all 10 are byte-exact matches.
* [x] The HDRHistogram-c v0.11.8 release date has been checked against the GitHub API; the recorded date is wrong (F3).
* [x] The Harris Chapter 6 and O'Hara Chapter 1 citations have been checked against external table-of-contents sources; both have title and content issues (F1, F2).
* [x] The 5 Phase 1 invariants in matching-semantics.md section 7 have been verified to map one-to-one onto design-spec section 8.2 invariants 1, 2, 3, 4, 7.
* [x] The companion plan's "60 scenarios" and "85 percent line coverage target" claims have been verified against `tests/integration/test_engine_vs_reference.cpp` (60 scenarios counted) and design spec section 8.1 (85 percent target sourced).
* [x] The audit report is filed at `/home/mustafa/src/Meridian/docs/audits/citation-audit-phase-1-2026-05-09.md`.
* [ ] The PM has applied the F1, F2, F3 fixes, after which this auditor returns to confirm and updates this report's "Overall verdict" line to "clean".

## 8. Sign-off

Conditional. Phase 1 documentation pass is clean **after F1, F2, F3 are resolved by the PM** (with QDV picking up F1 and F2, DevOps optionally helping with F3). The 10 deferred `[citation needed]` markers are not phase-close blockers per the dispatch brief.

Next agent in the handoff chain (per `agents/17-citation-and-fact-auditor.md` "Handoffs"): corrections route back to the originating agent. The PM session that dispatched this audit holds the gate until F1 (QDV-owned), F2 (QDV-owned), and F3 (PM/DevOps-owned) are resolved. After resolution, the next routine handoff at PR time is the Code Reviewer.
