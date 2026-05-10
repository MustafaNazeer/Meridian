# Meridian implementation plan

> For agentic workers: this plan is the Project Manager session's rolling
> source of truth for which phase is in flight, what its scope is, who
> owns each task, and how a phase exits cleanly. `STATUS.md` is the
> simpler "which phase is next" pointer; this document is the per-phase
> detail.
>
> When a phase opens, the PM expands that phase's section here with
> concrete next steps, dispatches the listed specialist agents, and (for
> phases that warrant it) writes a bite-sized-step plan at
> `docs/superpowers/plans/YYYY-MM-DD-phase-N-<name>.md` for the
> dispatched agent to follow.

**Goal:** Ship a portfolio-grade C++ price-time priority limit order book
and matching engine plus a live web visualization, in 12 phases (0
through 11), driven by a PM session that dispatches specialist subagents
per phase.

**Architecture:** Single-process C++20 matching engine compiled to
`libmeridian.a` plus three driver binaries (`meridian-bench`,
`meridian-replay`, `meridian-server`). Frontend is a React 19 plus Vite
plus TypeScript plus Tailwind SPA hosted on Cloudflare Pages at
`meridian-demo.pages.dev`, talking to the server over WSS. The engine
deploys as a Fly.io machine described by `Dockerfile` plus `fly.toml`,
auto-stopping when idle and auto-starting on first request.

**Tech Stack:** C++20, CMake 3.25 or newer with Ninja, GoogleTest,
rapidcheck, Google Benchmark, uWebSockets v20 or newer, simdjson, glaze,
spdlog. React 19, Vite, TypeScript, Tailwind CSS, Zustand. CMake
FetchContent for vendoring C++ deps. Python 3.11 plus pytest for the
reference implementation under `tests/reference/`. Fly.io plus
Cloudflare Pages for hosting.

---

## How to use this plan

1. Open `STATUS.md` first to see which phase is current.
2. Open the matching `## Phase N` section below for the detailed scope,
   owner agents, tasks, and exit criteria.
3. The PM session is responsible for dispatching the listed agents,
   tracking task completion, and updating both `STATUS.md` and this
   document at every phase boundary (phase open, phase close).
4. Phases that involve substantial code (Phase 1, 4, 5, 6, 8, 9) get a
   companion bite-sized-step plan saved at
   `docs/superpowers/plans/YYYY-MM-DD-phase-N-<name>.md` written when
   the phase opens. That companion plan is what dispatched specialists
   actually execute against.
5. When a phase closes, mark it `completed` here, summarize what
   shipped, and update `STATUS.md`. Run the mandatory check-in
   protocol from `CLAUDE.md` before opening the next phase.

## Phase dependency graph

```
Phase 0  Foundations
   |
   v
Phase 1  Single-symbol matching ----+
   |                                |
   v                                |
Phase 2  Multi-instrument           |
   |                                |
   v                                |
Phase 3  Property tests             |   (Phases 2 and 3 may bundle)
   |                                |
   v                                v
Phase 4  Seqlock + 30 Hz sampler
   |
   v
Phase 5  Benchmark hits 6M evt/s
   |
   v
Phase 6  ITCH 5.0 replay
   |
   v
Phase 7  Post-only and FOK ----+
   |                            |
   v                            v
Phase 8  WebSocket server          (Phases 7 and 8 may bundle)
   |
   v
Phase 9  React frontend with Twilight
   |
   v
Phase 10  Hosting and CI/CD
   |
   v
Phase 11  Polish, README, benchmark report  ->  v0.1 ships
```

---

## Phase 0: Foundations

**Status:** completed 2026-05-09 (PR #3 squash-merged to `main` at `3c744ed`; first CI run was green).

**Window cost:** ~95 percent of one Claude Max window.

**Goal:** Project bootstrap. Resolve the four open design questions,
write this implementation plan, publish the threat model, scaffold the
CMake project and the React frontend, wire up CI, and ship an initial
audited README. After Phase 0, the project is ready for the first line
of engine code.

### Inputs

* The bootstrap-template-scaffolded project at `/home/mustafa/src/Meridian`:
  `CLAUDE.md`, `SPEC.md`, `STATUS.md`, `GETTING-STARTED.md`,
  `README.md`, `future-ideas.md`, `agents/` (18 briefs),
  `design-mockups/`, `docs/design/canonical.html`,
  `docs/superpowers/specs/2026-05-09-meridian-design.md`.
* Two PRs already merged on `main`: `#1` (initial agent roster) and
  `#2` (audit fixes for agent roster expansion). Verify with
  `git log --oneline -5`.
* GitHub remote `origin` already wired to
  `https://github.com/MustafaNazeer/Meridian.git`.
* The four open questions in design spec section 13, resolved on
  2026-05-09: hosting domain `meridian-demo.pages.dev`, backend host
  Fly.io free tier, demo symbol set AAPL, SPY, NVDA, TSLA, GOOG, audit
  log WebSocket-only.

### Outputs

| File or artifact | Owner agent |
|---|---|
| `docs/plan.md` (this document) | Project Manager |
| `docs/security/threat-model.md` (STRIDE) | Security Engineer (08) |
| `docs/security/checklist.md` (per-phase, stub) | Security Engineer (08) |
| `docs/security/secrets.md` (initial set) | Security Engineer (08) |
| `docs/design/wireframes.md` | UI/UX Designer (05) |
| `docs/design/tokens.md` (Tailwind tokens from `canonical.html`) | UI/UX Designer (05) |
| `.gitignore` (final form, supersedes any draft) | DevOps Engineer (09) |
| Root `CMakeLists.txt`, `apps/{bench,replay,server}/CMakeLists.txt`, `tests/CMakeLists.txt` | DevOps Engineer (09) |
| `frontend/` Vite scaffold (`pnpm create vite`) plus Tailwind config from tokens | DevOps Engineer (09) |
| `.github/workflows/ci.yml` | DevOps Engineer (09) |
| `README.md` (Phase 0 stub form) | Documentation Engineer (15) |
| `docs/architecture.md` (initial pass) | Documentation Engineer (15) |
| `docs/audits/citation-audit-2026-05-09.md` | Citation and Fact Auditor (17) |

### Phase 0 task list

1. **Done.** Brainstorm and resolve the four section-13 open questions.
   Resolved on 2026-05-09.
2. **Done.** Cascade the resolutions into `SPEC.md`, `STATUS.md`,
   `README.md`, `GETTING-STARTED.md`, the design spec, and the agent
   briefs for PM, Engine Developer, Security Engineer, DevOps Engineer,
   Performance Engineer, and Observability Engineer. Committed locally
   on branch `phase-0-brainstorm-cascade` at `1ad71f3`.
3. **Done.** Write this plan at `docs/plan.md`.
4. **Done.** Dispatched UI/UX Designer (05), Security Engineer (08),
   DevOps Engineer (09), Documentation Engineer (15) in parallel.
   Outputs landed in commit `23ed376`: `docs/design/{tokens,wireframes}.md`,
   `docs/security/{threat-model,checklist,secrets}.md`, root
   `CMakeLists.txt` plus stubs, `.github/workflows/ci.yml`, full Vite
   scaffold under `frontend/` with Twilight Tailwind config applied,
   `.gitignore` updated, `docs/setup-guide.md` skeleton with section
   12.1 documenting branch protection, `README.md` rewritten as a
   Phase 0 stub, `docs/architecture.md`, ADR `0001-cpp20-library-plus-drivers.md`.
   Plus PM reconciliation: React 18 became React 19 throughout and
   design spec section 5.4 was corrected (Fly serves only WSS plus
   `/healthz` plus `/metrics`, not the static React app).
5. **Done.** Verified `origin` points at
   `https://github.com/MustafaNazeer/Meridian.git`. CMake skeleton
   configures cleanly on the dispatch machine (DevOps reported `cmake
   -B build -S . -G Ninja` succeeded). `pnpm dev` boots Vite cleanly.
6. **Done.** Citation and Fact Auditor (17) ran and filed
   `docs/audits/citation-audit-2026-05-09.md` (53 verified, 2
   unverified, 2 wrong out of 57 claims audited). The auditor resolved
   the three `[citation needed]` markers in `docs/security/threat-model.md`
   in place (uWebSockets advisory history, Fly free-tier wake latency,
   Cloudflare Pages `_headers` mechanism). The PM resolved the four
   close-blocker findings (F1 through F4) in commit `60a9e89`:
   property-test case-count divergence, the "6.2M events per second"
   claim asserted as fact in `SPEC.md` and quoted in ADR 0001, the
   GoogleTest pinning policy, and the design spec section 10 repo
   layout that named the frontend `web/` instead of `frontend/`.
7. **Pending.** Phase 0 closeout merge gate: user authorizes "push",
   PM runs the standard push protocol (push branch, `gh pr create`,
   watch required CI checks for the first run of the workflow, admin
   merge once green, sync local `main`). Then flip Phase 0 to
   `completed` in `STATUS.md`, set Next phase to Phase 1, and run the
   mandatory check-in protocol. The check-in itself runs at the same
   time as this message; the user's continue / pause / stop answer
   is captured in `STATUS.md` Resume notes.

### Exit criteria

* All Phase 0 outputs in the table above exist on `main` (after PRs
  merge through the standard push protocol).
* Citation and Fact Auditor has filed an initial audit at
  `docs/audits/citation-audit-2026-05-09.md` with no open criticals.
* Security Engineer threat model is published with no open criticals.
* DevOps `cmake --build .` exits 0 with "no sources to compile" (or
  equivalent), and `pnpm dev` boots a blank Vite page in `frontend/`.
* CI workflow `.github/workflows/ci.yml` runs green on a trivial PR.
* `STATUS.md` flipped to `completed` with the completion date filled
  and "Next phase" updated to Phase 1.
* Mandatory check-in protocol run; user has explicitly answered
  continue, pause, or stop.

### Risks

* **DevOps frontend scaffold blocks on `pnpm` not being installed.**
  Mitigation: the DevOps brief instructs running `corepack enable`
  before `pnpm create vite` if `pnpm` is not on the PATH.
* **Citation auditor flags aspirational README claims.** Mitigation:
  the Phase 0 README explicitly labels every unachieved metric (6M
  evt/s, sub-microsecond p50, demo URL) as "target" rather than
  "achieved". Final README ships in Phase 11 once metrics are real.
* **Threat model references VPS or systemd surfaces that no longer
  exist.** Mitigation: cascading edits already applied to
  `agents/08-security-engineer.md` before dispatch; the Security
  Engineer reads the current brief, not the pre-cascade version.

### Phase 0 companion plan

No bite-sized-step companion plan needed for Phase 0 since each
specialist's brief already contains the per-task instruction list. The
PM dispatches per the brief.

---

## Phase 1: Core data structures and single-symbol matching

**Status:** completed 2026-05-09 (PR #6 squash-merged to `main` at `bc5e9ee`; CI green on both initial and post-review-fix runs).

**Window cost:** ~95 percent of one Claude Max window. Actual: opened at 4 percent usage with 4.75 hours of headroom; closed cleanly inside the same window.

### Phase 1 close summary

`libmeridian.a` ships its first real lines: `Order`, `Level`, `Book`,
`OrderPool` with the debug allocator tripwire, and a working
`MatchingEngine::apply` for limit, market, IOC, and cancel events.
The Python reference under `tests/reference/` mirrors the same
semantics; the integration test diffs the C++ engine against the
reference on 60 hand-crafted plus corner-case scenarios. Quality
gates all green at close: Quant Domain Validator, Reference
Implementation Engineer, QA Engineer (97 percent coverage on
libmeridian.a, 41 unit tests, 60 integration scenarios), Performance
Engineer (latency budget plus working bench skeleton, baseline
informational), Risk Reviewer (10 audit cases all PASS), Citation
Auditor (86 verified, 0 wrong remaining; 3 found and resolved before
merge), Code Reviewer (approved with 5 non-blocking nits, 4 fixed,
1 deferred).

**Goal:** Ship the matching engine's core types and a working
single-symbol matching loop. Limit, market, and IOC order types. FIFO
queueing at each price level. Cancel-by-id. All driven by GoogleTest
unit tests written first.

### Outputs

| File or artifact | Owner |
|---|---|
| `include/meridian/types.hpp` (Order, OrderId, Price, Quantity, Side) | Engine Developer (03) |
| `include/meridian/order_pool.hpp` and `src/order_pool.cpp` | Engine Developer (03) |
| `include/meridian/level.hpp` and `src/level.cpp` (FIFO queue) | Engine Developer (03) |
| `include/meridian/book.hpp` and `src/book.cpp` (per-symbol L3 book, both sides) | Engine Developer (03), Market Microstructure Engineer (02) |
| `include/meridian/matching.hpp` and `src/matching.cpp` (limit, market, IOC, cancel) | Engine Developer (03), Market Microstructure Engineer (02) |
| `include/meridian/execution_report.hpp` | Engine Developer (03) |
| `tests/unit/test_order_pool.cpp` | QA Engineer (10) |
| `tests/unit/test_level.cpp` | QA Engineer (10) |
| `tests/unit/test_book.cpp` | QA Engineer (10) |
| `tests/unit/test_matching.cpp` | QA Engineer (10), Risk Reviewer (16) |
| `tests/reference/run_reference.py` (Python reference for limit, market, IOC) | Reference Implementation Engineer (18) |
| `tests/integration/test_engine_vs_reference.cpp` | QA Engineer (10), Reference Implementation Engineer (18) |
| `apps/bench/main.cpp` (skeleton; full benchmark in Phase 5) | Engine Developer (03), Performance Engineer (12) |
| `docs/risk/matching-semantics.md` (textbook validation pass) | Quant Domain Validator (01) |

### Owners and dispatch order

1. **Quant Domain Validator (01)** writes `matching-semantics.md` from
   textbook references first. This is the contract every other agent
   builds against.
2. **Reference Implementation Engineer (18)** writes the Python
   reference (limit, market, IOC) before any C++ matching code lands.
3. **Engine Developer (03)** plus **Market Microstructure Engineer (02)**
   pair on the C++ implementation, test-first.
4. **QA Engineer (10)** lands the unit-test suite alongside the
   implementation; tests must exist before the code they cover.
5. **Performance Engineer (12)** scaffolds the bench binary so latency
   is measurable from day one.
6. **Risk and Financial Correctness Reviewer (16)** signs off on the
   matching invariants before the phase closes.
7. **Code Reviewer (11)** reviews every PR.

### High-level tasks

1. Write `docs/risk/matching-semantics.md` documenting price-time
   priority, the tie-breaking rules between resting orders at the same
   price, the IOC fill-or-cancel semantics, and the conservation law
   (every quantity unit either rests or trades, never disappears).
2. Write the Python reference implementation in `tests/reference/`
   (limit, market, IOC matching). This is a slow, simple, obviously
   correct version that the C++ engine will be compared against.
3. Define the C++ types: `Order`, `OrderId`, `Price`, `Quantity`,
   `Side`, `OrderType`, `ExecutionReport`. All trivially copyable, all
   in `include/meridian/types.h`.
4. Implement `OrderPool` with preallocated slots; no heap allocation
   on `acquire()` or `release()`. Test the slot-recycling invariant.
5. Implement `Level` (FIFO order queue at one price). Test FIFO
   discipline.
6. Implement `Book` as a per-symbol L3 book holding both bid and ask
   sides. Each side is a `std::map<Price, Level*>` (bids ordered
   descending, asks ascending). The book also owns an
   `unordered_map<OrderId, Order*>` for O(1) cancel within the symbol.
   Test top-of-book read and update plus cancel-by-id.
7. Implement `match_limit`, `match_market`, `match_ioc` in
   `matching.cpp`. Each function returns a list of `ExecutionReport`s.
   Test against the Python reference for at least 50 hand-crafted
   scenarios plus 10 corner cases (price improvement, partial fill, IOC
   on empty book, etc.).
8. Wire the C++ matching tests through GoogleTest and the integration
   test (C++ engine vs Python reference) through a custom GoogleTest
   harness that piped JSON event sequences through both and diffed.
9. Performance Engineer scaffolds `apps/bench/main.cpp` with a single
   "match 1M synthetic limits in a tight loop" benchmark. Reports
   nothing about throughput yet; just confirms the harness compiles
   and runs.

### Exit criteria

* All unit tests pass; integration test against the Python reference
  passes on 10,000 generated scenarios with zero diffs.
* Risk and Financial Correctness Reviewer signs off on
  `matching-semantics.md` and the implementation against it.
* Code Reviewer approves all PRs in the phase.
* No allocator override violations on the hot path (the debug allocator
  override aborts on any new or malloc inside the matching loop).
* `apps/bench/main.cpp` compiles and runs; throughput number is not
  yet a target.
* `STATUS.md` flipped, check-in run.

### Risks

* **Engine vs reference diff masked by float arithmetic.** Mitigation:
  prices are integer ticks, not floats; the reference uses the same
  integer-tick representation.
* **FIFO queue performance regresses against intuition.** Mitigation:
  Performance Engineer reviews the data structure choice before final
  PR; deque vs intrusive list trade-off documented in
  `docs/perf/findings.md`.

### Phase 1 companion plan

Required. To be written at
`docs/superpowers/plans/<date>-phase-1-single-symbol-matching.md` when
Phase 1 opens. The Engine Developer follows that document.

---

## Phase 2: Multi-instrument and cancel-by-id

**Status:** completed 2026-05-10 (PR #9 squash-merged to `main` at `8130599`; CI green; Risk Reviewer signed off all 5 Phase 2 audit cases).

**Window cost:** ~50 percent of one Claude Max window. Actual: opened at 32 percent usage; closed inside the same window without bundling Phase 3.

### Phase 2 close summary

`BookRegistry` and `OrderIndex` ship the cross-symbol layer. The
matching engine routes `NewOrder` by `event.symbol`, emitting
`Reject UnknownSymbol` for unregistered symbols. The cancel path
consults the cross-symbol `OrderIndex` and routes to the right `Book`
via the order's own `symbol` field; the per-`Book` id index from
Phase 1 stays in place per ADR 0002. Python reference extends to
multi-symbol with the same shape; the integration test suite ships 7
multi-symbol scenarios in addition to the 60 inherited from Phase 1.
118 ctest tests passing in Debug and Release; 40 Python reference
tests passing. Risk Reviewer signed off all 5 Phase 2 audit cases.

**Goal:** Ship multi-instrument support (`BookRegistry` dispatch by
symbol) plus O(1) cancel-by-id. Demonstrates the engine handles the
five demo symbols (AAPL, SPY, NVDA, TSLA, GOOG) chosen in Phase 0.

### Outputs

| File or artifact | Owner |
|---|---|
| `include/meridian/book_registry.h` and `src/book_registry.cpp` | Engine Developer (03) |
| `include/meridian/order_index.h` (id to slot lookup) | Engine Developer (03) |
| Updates to `book.cpp` for cancel-by-id | Engine Developer (03) |
| Python reference extended for multi-instrument and cancel | Reference Implementation Engineer (18) |
| `tests/integration/test_multi_instrument.cpp` | QA Engineer (10) |
| `tests/unit/test_cancel_by_id.cpp` | QA Engineer (10) |

### High-level tasks

1. Define `BookRegistry` as a fixed-capacity hash map from
   `SymbolId` (16-bit interned) to `Book`. No dynamic allocation after
   construction.
2. Add `OrderIndex` mapping `OrderId` to the `Order` slot in
   `OrderPool`. O(1) lookup, O(1) cancel.
3. Update `Book` to splice the cancelled order out of its `Level` in
   O(1) (intrusive list pointer fix-up).
4. Extend the Python reference to support multi-instrument and
   cancel-by-id with the same API shape as the C++ engine.
5. Write the integration test that drives 5 symbols concurrently
   through both engines and diffs the audit logs.

### Exit criteria

* Multi-instrument integration test passes for the five demo symbols.
* Cancel latency: p99 under 200 ns on the user's desktop (informal
  measurement; formal latency budget enforced in Phase 5).
* Risk Reviewer signs off on cancel semantics; specifically, that the
  cancelled order is removed from its Level FIFO position cleanly and
  the next match at that price level proceeds without disturbance.

### Risks

* **OrderIndex hash collisions degrade cancel latency.** Mitigation:
  size the table with load factor below 0.5; benchmark in Phase 5.

---

## Phase 3: Property-based tests for matching invariants

**Status:** not started.

**Window cost:** ~50 percent alone, or bundle with Phase 2.

**Goal:** Wire `rapidcheck` into the test suite and verify the 10 core
matching invariants over thousands of generated event sequences.

### Outputs

| File or artifact | Owner |
|---|---|
| CMake plumbing for `rapidcheck` via FetchContent | DevOps Engineer (09) |
| `tests/property/test_invariants.cpp` (10 invariants) | QA Engineer (10), Engine Developer (03) |
| `tests/reference/property_corpus_runner.py` | Reference Implementation Engineer (18) |
| Updates to `docs/risk/matching-semantics.md` listing the 10 invariants | Quant Domain Validator (01) |

### The 10 invariants

1. Quantity conservation: total quantity in equals total quantity in
   resting books plus total quantity traded.
2. No double-fill: each `OrderId` appears at most once per
   `ExecutionReport.matched_order_id` per event.
3. No lost orders: every accepted order eventually rests, trades, or
   is cancelled, never disappears.
4. Price-time priority: at each price level, an order resting longer
   trades before an order resting shorter against an aggressor.
5. Best-bid-best-ask invariant: bid top is always less than ask top
   (no crossed book) after every event.
6. IOC never rests: an IOC order's residual after matching is
   cancelled, not added to the book.
7. Market never rests: a market order's residual after matching is
   cancelled, not added to the book.
8. Cancel-by-id is idempotent: cancelling an unknown id is a no-op,
   not an error; cancelling twice has the same effect as cancelling
   once.
9. Audit log is monotonic: timestamps in the audit log are
   non-decreasing.
10. Engine equals reference: for every generated event sequence, the
    C++ engine's audit log matches the Python reference's audit log
    byte-for-byte after canonical serialization.

### Tasks

1. DevOps wires `rapidcheck` via FetchContent.
2. QA writes one property test per invariant; each generates at least
   10,000 sequences before the test is considered passing.
3. The Reference Implementation Engineer runs the same generated
   corpus through the Python reference and compares against the C++
   audit logs.
4. Risk Reviewer adjudicates any disagreements and either fixes the
   reference, fixes the engine, or amends the invariant statement.

### Exit criteria

* All 10 invariants hold over 10,000 generated sequences each.
* Engine vs reference diff is zero on the entire corpus.
* The 10 invariants are documented verbatim in
  `docs/risk/matching-semantics.md`.

---

## Phase 4: Seqlock-protected top-of-book and 30 Hz sampler

**Status:** not started.

**Window cost:** ~85 percent of one window.

**Goal:** Add the seqlock-protected `TopOfBookSnapshot` per symbol plus
the 30 Hz sampler thread. Pass under TSAN.

### Outputs

| File or artifact | Owner |
|---|---|
| `include/meridian/top_of_book.h` (seqlock writer protocol) | Market Microstructure Engineer (02), Concurrency Reviewer (19) |
| `include/meridian/sampler.h` and `src/sampler.cpp` | Engine Developer (03) |
| `tests/concurrency/test_seqlock.cpp` (TSAN run) | QA Engineer (10), Concurrency Reviewer (19) |
| `tests/concurrency/test_sampler_smoke.cpp` | QA Engineer (10) |
| `docs/adr/0001-seqlock-protocol.md` | Documentation Engineer (15) |
| Performance Engineer benchmark: sampler does not block matching loop | Performance Engineer (12) |

### High-level tasks

1. Concurrency Reviewer plus Market Microstructure Engineer co-design
   the seqlock writer and reader protocols, including memory ordering
   choices (`memory_order_release` on writer side, `memory_order_acquire`
   on reader side, `memory_order_seq_cst` reserved for the sequence
   counter increments).
2. Engine Developer implements `TopOfBookSnapshot::write(...)` (writer
   path) and `TopOfBookSnapshot::read_consistent(...)` (reader path
   with retry).
3. Sampler thread reads all symbols' snapshots at 30 Hz on a dedicated
   core, builds a JSON delta, and (in Phase 8) hands it off to the
   uWebSockets event loop. In Phase 4, the sampler writes deltas to
   stdout to prove the protocol works.
4. TSAN test under `clang -fsanitize=thread`. Must pass with zero
   warnings before Concurrency Reviewer signs off.
5. Performance Engineer measures the matching loop's throughput with
   and without the sampler attached; the seqlock writer cost must be
   under 5 ns p50 and must not perturb the matching loop's p99.
6. Documentation Engineer writes the ADR explaining the seqlock
   protocol choice over alternatives (RCU, double-buffering, mutex).

### Exit criteria

* TSAN clean across all concurrency tests.
* Seqlock writer overhead under 5 ns p50; matching loop p99 unchanged
  within measurement noise.
* Concurrency Reviewer signs off, including the ADR.
* Sampler thread runs stably for 60 seconds at 30 Hz with zero missed
  ticks.

### Risks

* **TSAN finds a real race in the seqlock.** Mitigation: this is
  exactly what TSAN is for; budget time to actually fix it rather than
  paper over with fences.
* **Sampler jitter exceeds 30 Hz target.** Mitigation: pin the sampler
  to its own core via `pthread_setaffinity_np`; document the pinning
  in the ADR.

---

## Phase 5: Benchmark hits 6M events per second

**Status:** not started.

**Window cost:** ~95 percent of one window.

**Goal:** Headline performance phase. PGO build, cache layout audit,
benchmark suite that reports throughput plus latency histogram, and
hits the 6M events per second target on the user's desktop.

### Outputs

| File or artifact | Owner |
|---|---|
| `apps/bench/main.cpp` final form (synthetic plus ITCH modes) | Performance Engineer (12), Engine Developer (03) |
| `apps/bench/hdr_histogram.h` (or vendored) | Performance Engineer (12) |
| `bench/baseline.json` (CI baseline) | Performance Engineer (12) |
| PGO build target in `CMakeLists.txt` | DevOps Engineer (09), Performance Engineer (12) |
| `.github/workflows/bench.yml` (manual trigger) | DevOps Engineer (09) |
| `docs/perf/budget.md` (latency budget) | Performance Engineer (12) |
| `docs/perf/findings.md` (cache layout audit, hot-path discipline) | Performance Engineer (12) |
| `docs/audits/citation-audit-phase-5.md` (audits the benchmark report) | Citation Auditor (17) |

### High-level tasks

1. Performance Engineer writes `docs/perf/budget.md` with concrete
   targets: throughput >=6M events per second single-threaded;
   p50 <=500 ns; p99 <=2 us; p99.9 <=5 us.
2. Implement the synthetic event generator (Poisson arrivals,
   configurable cancel ratio).
3. Implement the HDR histogram-based latency harness.
4. Add PGO build target: `cmake --build . --target meridian-bench-pgo`,
   which runs a profile collection pass, then a use pass.
5. Audit cache layout. Pack `Order` to fit in one cache line; verify
   with `static_assert(sizeof(Order) <= 64)`. Audit `Level` and
   `Book` for false sharing.
6. Iterate on the matching loop until 6M events per second target is
   met. Document each material change in `docs/perf/findings.md`.
7. Performance Engineer runs the benchmark; persists JSON baseline at
   `bench/baseline.json`.
8. DevOps adds `.github/workflows/bench.yml` (manual trigger; runs the
   benchmark, diffs against baseline, fails the build if throughput
   regresses more than 5 percent or any latency percentile regresses
   more than 10 percent, per design spec section 8.4).
9. Concurrency Reviewer pairs on cache layout changes that touch
   shared state (the seqlock writer's `TopOfBookSnapshot` field
   ordering matters here).
10. Citation Auditor audits the benchmark report for trustworthy
    methodology (machine description, kernel version, CPU governor
    setting, frequency pinning, sample size, warm-up, isolation).

### Exit criteria

* `meridian-bench --mode synthetic` reports >=6M events per second
  on the user's desktop with the targets met.
* `bench/baseline.json` exists and the bench workflow uses it.
* Cache layout audit complete; `static_assert` on `Order` size.
* Performance Engineer signs off on the latency budget.
* Citation Auditor signs off on the benchmark report.

### Risks

* **Target not hit on user's hardware.** Mitigation: per SPEC.md and
  the design spec section 11 risk register, the acceptance criterion
  is "matches the headline within 20 percent on equivalent hardware".
  PGO plus LTO before declaring failure.

---

## Phase 6: NASDAQ ITCH 5.0 replay

**Status:** not started.

**Window cost:** ~95 percent of one window.

**Goal:** Hand-roll a NASDAQ ITCH 5.0 parser plus a CLI replayer
(`meridian-replay`). Verify against the Python reference on a real
ITCH sample.

### Outputs

| File or artifact | Owner |
|---|---|
| `include/meridian/itch.h` (single-header parser) | Engine Developer (03) |
| `apps/replay/main.cpp` (`meridian-replay`) | Engine Developer (03) |
| `tests/reference/itch_parser.py` | Reference Implementation Engineer (18) |
| `tests/reference/run_reference.py` extended for ITCH input | Reference Implementation Engineer (18) |
| `tests/integration/test_itch_replay.cpp` (engine vs reference on real ITCH) | QA Engineer (10), Reference Implementation Engineer (18) |
| `docs/audits/itch-conformance.md` | Citation Auditor (17) |
| Sample ITCH tape committed at `tests/data/itch-sample.bin` (5 minutes, AAPL plus SPY plus NVDA plus TSLA plus GOOG) | Engine Developer (03) |

### High-level tasks

1. Engine Developer studies the official NASDAQ ITCH 5.0 specification
   document (Citation Auditor will require the exact PDF version
   citation in the conformance doc).
2. Implement the parser as a single-header library that reads message
   types `A` (add order), `D` (delete), `E` (executed), `C`
   (executed with price), `X` (cancel), `R` (stock directory), and
   `S` (system event). Skip message types not relevant to the demo;
   document the skip list.
3. Reference Implementation Engineer writes the Python ITCH parser
   independently from the C++ implementation. The two must agree on
   every parsed field for every message in the sample tape.
4. Build `meridian-replay` with `--tape`, `--speed`, `--symbols`,
   `--audit out.jsonl` flags. The audit log JSON Lines format is
   defined here and used unchanged in Phase 8 for the WebSocket
   protocol.
5. QA writes the integration test: replay the same tape through both
   engines, compare audit logs.
6. Citation Auditor signs off on `docs/audits/itch-conformance.md`,
   verifying the parser covers the message types it claims to and
   handles the byte order correctly.

### Exit criteria

* `meridian-replay` runs the sample tape end to end without errors.
* Engine vs Python reference diff is zero on the sample tape's audit
  logs.
* Risk Reviewer signs off on the matching semantics under real ITCH.
* Citation Auditor signs off on the conformance document.

### Risks

* **ITCH parser eats the schedule.** Mitigation per SPEC.md risk
  register: cap at the phase budget; fall back to LOBSTER CSV if ITCH
  binary parsing stalls. LOBSTER is text and trivial to parse.

---

## Phase 7: Post-only and FOK order types

**Status:** not started.

**Window cost:** ~30 percent alone, or bundle with Phase 8.

**Goal:** Add two more order types: post-only (rejects if it would
trade) and fill-or-kill (FOK; trades the entire requested quantity or
nothing).

### Outputs

* `matching.cpp` extended for post-only and FOK semantics.
* Python reference extended.
* Property tests updated to cover the new invariants.
* `docs/risk/matching-semantics.md` updated.

### Exit criteria

* New property tests pass on 10,000 generated sequences each.
* Engine vs reference diff zero on a corpus that includes the new
  order types.
* Risk Reviewer signs off.

---

## Phase 8: WebSocket server and protocol

**Status:** not started.

**Window cost:** ~60 percent alone, or bundle with Phase 7.

**Goal:** Wire uWebSockets, define the snapshot plus delta protocol,
and ship the `meridian-server` binary.

### Outputs

| File or artifact | Owner |
|---|---|
| CMake plumbing for uWebSockets | DevOps Engineer (09) |
| `apps/server/main.cpp` (`meridian-server`) | Engine Developer (03) |
| `apps/server/protocol.h` (snapshot, delta, control message schemas) | Engine Developer (03) |
| `tests/integration/test_websocket_smoke.cpp` | QA Engineer (10), DevOps Engineer (09) |
| `docs/api/websocket.md` (protocol spec) | Engine Developer (03), Citation Auditor (17) |
| Concurrency review of sampler -> publisher -> WS handoff | Concurrency Reviewer (19) |
| WebSocket handshake and origin checks | Security Engineer (08) |

### High-level tasks

1. DevOps adds uWebSockets via FetchContent.
2. Engine Developer wires the matching loop on core 4, sampler on
   core 6, uWebSockets event loop on core 5 (per design spec section
   4 architecture).
3. Implement the WebSocket protocol: on connect, send `snapshot`
   (full L10 book plus recent trades plus perf metrics for the
   subscribed symbol); thereafter the sampler builds and broadcasts
   `delta` messages at 30 Hz.
4. Implement the in-memory audit log ring buffer (per design spec
   section 13 resolved decision: no disk persistence on the live
   demo).
5. Concurrency Reviewer audits the sampler-to-event-loop handoff for
   data races; in particular, the JSON delta the sampler builds must
   be either copied into the event loop's thread or owned by an
   atomic pointer swap.
6. Security Engineer signs off on the handshake (origin allow-list,
   max payload, max client count, per-IP rate limit).
7. Citation Auditor audits `docs/api/websocket.md` for accuracy
   against the actual protocol code.

### Exit criteria

* `meridian-server --tape <sample.bin>` boots, accepts WebSocket
  connections, sends snapshot then deltas at 30 Hz.
* CI smoke test (build server, start it, connect, verify snapshot
  arrives, verify at least one delta arrives) passes.
* Concurrency Reviewer signs off on the handoff path.
* Security Engineer signs off on the handshake.
* Citation Auditor signs off on the protocol document.

### Risks

* **uWebSockets has bugs in our use case.** Mitigation per SPEC.md
  risk register: fall back to Boost.Beast or hand-rolled WebSocket if
  blocked. Beast adds dependency weight but is well tested.

---

## Phase 9: React frontend with Twilight visual

**Status:** not started.

**Window cost:** ~95 to 99 percent of one window.

**Goal:** Build the React frontend per the canonical Twilight design
at `docs/design/canonical.html`. All seven panels (Header, Hero,
Ladder, DepthChart, Tape, PerfPanel, Footer) plus the cold-start
connection state machine.

### Outputs

| File or artifact | Owner |
|---|---|
| `frontend/src/components/Header.tsx` (with five-symbol switcher) | Frontend Developer (04) |
| `frontend/src/components/Hero.tsx` | Frontend Developer (04) |
| `frontend/src/components/Ladder.tsx` (L8 depth ladder, FIFO tooltips) | Frontend Developer (04) |
| `frontend/src/components/DepthChart.tsx` (hand-rolled SVG, gradient fills, dashed mid line) | Frontend Developer (04) |
| `frontend/src/components/Tape.tsx` (last 12 trades, aggressor coloring) | Frontend Developer (04) |
| `frontend/src/components/PerfPanel.tsx` (throughput, latency histogram, p50/p99/p99.9 grid) | Frontend Developer (04) |
| `frontend/src/components/Footer.tsx` | Frontend Developer (04) |
| `frontend/src/components/EngineWarmingUp.tsx` (cold-start state) | Frontend Developer (04) |
| `frontend/src/ws/client.ts` (connection state machine; auto reconnect with exponential backoff) | Frontend Developer (04) |
| `frontend/src/store/index.ts` (Zustand) | Frontend Developer (04) |
| Tailwind config aligned to design tokens | UI/UX Designer (05) |
| Vitest plus Testing Library tests for each component | Frontend Developer (04), QA Engineer (10) |
| Accessibility audit report | Accessibility Specialist (13) |
| CSP `connect-src` review for the Fly WebSocket origin | Security Engineer (08) |

### High-level tasks

1. UI/UX Designer reconciles `frontend/tailwind.config.ts` with the
   canonical HTML tokens (colors, fonts, spacing, radius).
2. Frontend Developer scaffolds the route structure and the seven
   panels.
3. Implement the WebSocket client connection state machine (see
   design spec section 5.5): `connecting` -> `live` -> `stalled` ->
   `disconnected` with exponential backoff (500 ms initial, doubling,
   cap 8 s).
4. Implement the cold-start `EngineWarmingUp` panel that renders
   while in the `connecting` state for longer than 1 second. Copy:
   "Engine warming up. The Fly machine sleeps when idle and takes a
   moment to start."
5. Implement the five-symbol switcher (AAPL, SPY, NVDA, TSLA, GOOG)
   in the Header. On change, unsubscribe from previous topic,
   subscribe to new, re-enter `connecting`.
6. Each component ships with at least one Vitest test (happy path).
7. Accessibility Specialist runs a full a11y audit. WCAG AA contrast
   on the Twilight palette, keyboard navigation, screen reader
   labels, prefers-reduced-motion respect.
8. Security Engineer reviews the CSP, ensures `connect-src` allows
   only the Fly app WebSocket origin plus self.

### Exit criteria

* All seven panels render against a real `meridian-server` running
  locally, with the sample ITCH tape replaying.
* Cold-start state correctly transitions to `live` after the first
  snapshot arrives.
* Symbol switcher transitions correctly between all five symbols.
* Vitest suite passes; bundle size under 200 KB gzipped (Performance
  Engineer measures).
* Accessibility Specialist signs off on WCAG AA compliance.
* Security Engineer signs off on the CSP.

### Risks

* **Cold start UI looks broken on first impression.** Mitigation:
  the explicit `EngineWarmingUp` component plus copy is a hard
  Phase 9 requirement. UI/UX Designer reviews the wording; the user
  reviews the wording.
* **Hand-rolled SVG depth chart is slower than expected at 30 Hz.**
  Mitigation: Performance Engineer profiles in CI; if 30 Hz is too
  much for the chart, render at 10 Hz (sampler still ships at 30 Hz
  for the ladder and tape, which need the higher rate more).

---

## Phase 10: Hosting and CI/CD

**Status:** not started.

**Window cost:** ~75 percent of one window.

**Goal:** Deploy `meridian-server` to Fly.io as a Dockerfile-built
machine; deploy the frontend to Cloudflare Pages at
`meridian-demo.pages.dev`. Final security hardening pass.

### Outputs

| File or artifact | Owner |
|---|---|
| `Dockerfile` (multi-stage) for `meridian-server` | DevOps Engineer (09) plus Engine Developer (03) |
| `fly.toml` (auto-stop, auto-start, healthcheck wired to `/healthz`) | DevOps Engineer (09) |
| Cloudflare Pages project at `meridian-demo.pages.dev` | DevOps Engineer (09) |
| `.github/workflows/deploy.yml` (deploy frontend on `main` push, deploy engine on `main` push) | DevOps Engineer (09) |
| `docs/setup-guide.md` final form | DevOps Engineer (09), Documentation Engineer (15), Citation Auditor (17) |
| Final hardening checklist (CSP, HSTS, secrets rotation) | Security Engineer (08) |
| `docs/observability/runbook.md` final form | Observability Engineer (14) |

### High-level tasks

1. DevOps picks the Fly.io app name (try `meridian` first, then
   `meridian-engine`, then `meridian-lob` if both taken) and the
   region (default `iad`, fall back to `ord`, then `sjc`).
2. Write the multi-stage Dockerfile: builder stage with the C++
   toolchain, runtime stage with a minimal base (Debian slim) and
   the `meridian-server` binary plus the sample ITCH tape. Non-root
   user inside the runtime image.
3. Write `fly.toml`: chosen region, smallest free-tier machine size,
   `auto_stop_machines = true`, `auto_start_machines = true`,
   `min_machines_running = 0`, port mapping, healthcheck pointing at
   `/healthz`.
4. Engine Developer adds `/healthz` to `meridian-server` (returns
   200 with a JSON status line; cheap so Fly's load balancer can hit
   it frequently).
5. `fly launch` (or `fly deploy` against an existing app), confirm
   the machine boots, capture the resulting `wss://<app>.fly.dev/ws`
   URL.
6. Set the frontend's `VITE_WS_URL` to the captured URL; deploy to
   Cloudflare Pages.
7. Configure GitHub Actions to deploy both halves on `main` push.
8. Security Engineer runs the final hardening checklist: HTTPS via
   Fly's edge for the engine; HSTS with `max-age` at least 6 months
   on the frontend; CSP with `connect-src` allowing the Fly origin;
   X-Frame-Options DENY; X-Content-Type-Options nosniff; rotate the
   `FLY_API_TOKEN` and Cloudflare Pages API token.
9. Observability Engineer writes the runbook for `fly logs`,
   `fly ssh console`, rollback (`fly releases`), and the cold-start
   diagnostic flow.
10. Documentation Engineer writes `docs/setup-guide.md` end to end:
    every command from a clean machine to a working deploy.
11. Citation Auditor signs off on the setup guide and the runbook.

### Exit criteria

* `meridian-demo.pages.dev` loads, connects to `wss://<app>.fly.dev/ws`,
  shows the live demo.
* CI deploy on `main` push works for both halves.
* Final hardening checklist fully checked.
* Setup guide reproduces the deploy from scratch on a fresh machine
  in under 30 minutes.
* Citation Auditor signs off on the setup guide and runbook.

### Risks

* **Fly free-tier machine size insufficient for the engine.**
  Mitigation: measure resident set during Phase 8 testing; if the
  smallest free machine is too small, bump to a $5-per-month paid
  machine. Documented as the "Fly.io free tier exceeded" risk in
  the design spec section 11.
* **Cold start hurts demo first impression.** Mitigation: handled
  by the Phase 9 `EngineWarmingUp` component.

---

## Phase 11: Polish, README, and benchmark report

**Status:** not started.

**Window cost:** ~80 percent of one window.

**Goal:** Final shippable polish. Final README with the headline
benchmark number. Benchmark report PDF. Final test pass. Project
ready to put on a resume.

### Outputs

| File or artifact | Owner |
|---|---|
| Final `README.md` with verified benchmark numbers and demo URL | Documentation Engineer (15), Citation Auditor (17) |
| `docs/perf/benchmark-report.pdf` | Performance Engineer (12), Citation Auditor (17) |
| Final architecture summary at `docs/architecture.md` | Documentation Engineer (15) |
| All ADRs reviewed for accuracy against shipped code | Documentation Engineer (15), Citation Auditor (17) |
| Final regression suite run | QA Engineer (10) |
| Final concurrency review | Concurrency Reviewer (19) |
| Final risk and correctness review | Risk Reviewer (16) |

### High-level tasks

1. Performance Engineer re-runs the benchmark on the user's desktop
   with the final binary; captures the headline number with full
   methodology disclosure.
2. Documentation Engineer writes the final README. The "what is
   this" hook fits in 30 seconds. Benchmark headline is a real
   measurement, not a target. Demo URL works.
3. Performance Engineer plus Documentation Engineer assemble the
   benchmark report PDF (machine description, kernel, CPU governor,
   methodology, results, comparison to a cited reference if
   appropriate).
4. Citation Auditor runs a full final pass on every shipping
   document: README, design spec, architecture doc, ADRs, setup
   guide, runbook, threat model, benchmark report. Every concrete
   number, every cited document, every named function, every URL
   must verify.
5. Risk Reviewer plus Concurrency Reviewer give final sign-offs.
6. QA Engineer runs the full regression suite (unit, property,
   integration, frontend) one last time.

### Exit criteria

* README on `main` reflects the final shippable state of the
  project.
* Benchmark report PDF committed at
  `docs/perf/benchmark-report.pdf`.
* Citation Auditor closes the final audit with no open items.
* All sign-offs from Risk, Concurrency, Performance, Security, QA,
  Documentation.
* `STATUS.md` flipped to all phases completed.
* User declares v0.1 shipped.

### Risks

* **Demo URL flaps because Fly free tier limit hit.** Mitigation:
  monitor Fly usage during Phase 11 polish; if usage trends suggest
  the free tier will be exceeded, upgrade to the paid tier before
  Phase 11 closes (per the design spec section 11 risk register).
* **Project drags past the planned schedule.** Mitigation per
  SPEC.md risk register: defer Phase 11 polish first if needed; the
  demo can ship without a benchmark PDF if the README and live URL
  work.

---

## Cross-cutting concerns

These apply across multiple phases and are worth tracking centrally.

### Testing discipline

* Unit tests with GoogleTest in `tests/unit/`.
* Property tests with rapidcheck in `tests/property/`.
* Integration tests (engine vs Python reference) in `tests/integration/`.
* Concurrency tests in `tests/concurrency/`, run under TSAN in CI.
* Frontend tests with Vitest plus Testing Library in `frontend/src/__tests__/`.
* Test discipline: every new feature lands with the test that fails
  first, the implementation that makes it pass, and a commit message
  that says so. Use `superpowers:test-driven-development` on each
  task.
* Coverage target: 85 percent line coverage on `libmeridian.a`. CI
  enforces this from Phase 1 onward via `lcov` plus a hard threshold
  in the workflow.

### Performance discipline

* No heap allocation on the hot path. The debug allocator override
  aborts on `new` or `malloc` inside the matching loop.
* No exceptions on the hot path. Matching returns result codes; uses
  `[[likely]]` and `[[unlikely]]` annotations.
* Logging at `SPDLOG_TRACE` and `SPDLOG_DEBUG` is compile-time
  stripped from release builds.
* Latency budget owned by the Performance Engineer and tracked in
  `docs/perf/budget.md`.

### Security discipline

* Threat model in `docs/security/threat-model.md`, refreshed at
  every phase boundary that touches the threat surface.
* No INTERNET permission beyond what the WebSocket needs.
* No analytics, no error tracking SaaS, no telemetry. The demo is
  read-only and public.
* Secrets in `fly secrets` and GitHub Actions secrets, never in
  the repo.

### Documentation discipline

* Every shipping document (README, ADRs, design spec, architecture
  doc, setup guide, runbook, threat model, perf reports, audit
  reports) goes through the Citation and Fact Auditor before phase
  close.
* ADRs live in `docs/adr/` numbered sequentially.

### Concurrency discipline

* The Concurrency Reviewer signs off on every PR that touches
  threads, atomics, memory ordering, or the seqlock protocol.
* TSAN is non-negotiable. CI runs all concurrency tests under TSAN
  on every PR from Phase 4 onward.

---

## Working notes and decisions log

A short list of decisions that affect more than one phase, captured
here so they do not get lost in commit messages.

* **2026-05-09**: project bootstrapped from the `~/.claude/templates/`
  template. Twilight visual chosen over Bourse, Terminal, Eikon,
  Onyx during brainstorming.
* **2026-05-09**: section-13 open questions resolved during Phase 0
  brainstorm. Hosting domain `meridian-demo.pages.dev`. Backend host
  Fly.io free tier with auto-stop and auto-start. Demo symbol set
  AAPL, SPY, NVDA, TSLA, GOOG. Audit log on the live demo is
  WebSocket-only with an in-memory ring buffer; the offline CLI
  flag `meridian-replay --audit out.jsonl` is unaffected.
* **2026-05-09**: Phase 9 frontend has a hard requirement for an
  explicit cold-start "engine warming up" state, since Fly's free
  machines sleep when idle.
* **2026-05-09**: Phase 10 deploy stack changed from VPS plus systemd
  to Fly.io Dockerfile plus `fly.toml`. Phase 10 window-cost
  estimate dropped from 85 percent to 75 percent.
* **2026-05-09 (Phase 1 open)**: `Book` ships cancel-by-id at the
  per-symbol level in Phase 1. The cross-symbol `BookRegistry` plus
  `OrderIndex` layer is still Phase 2. Reference:
  `docs/superpowers/plans/2026-05-09-phase-1-single-symbol-matching.md`
  key decision 4. Same commit also amends the design spec section 5.1
  (`Book` is the full L3 book per symbol with both sides, not "one
  side") and `docs/plan.md` Phase 1 task 6 wording (two `std::map`
  per side, not "two heaps") to remove cross-document contradictions
  surfaced by the PM at Phase 1 open.

(New decisions append here in chronological order with date and a
one-paragraph rationale.)
