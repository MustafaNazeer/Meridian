# Regression checklist

**Date**: 2026-05-09
**Introduced alongside**: single-symbol matching milestone

This file is the manual smoke-test list run before each milestone closes.
Every item must report green for a milestone to be considered done. The
list grows milestone by milestone: the "Single-symbol matching set"
section below is the floor, and the "Later additions" section names
items that arrive with later work.

The list is short by design. Heavy verification (property tests,
integration replays, benchmark regressions, frontend a11y) lives in CI
and in dedicated workstreams. This checklist is the human-loop sanity
pass.

---

## Single-symbol matching set (extended for the multi-instrument milestone)

Run from a clean checkout of the repo at the commit being closed.

### 1. CMake configures cleanly in Debug and Release

```bash
cd /home/mustafa/src/Meridian
rm -rf build build-release
cmake -B build         -S . -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake -B build-release -S . -G Ninja -DCMAKE_BUILD_TYPE=Release
```

**Pass criterion**: both invocations exit 0, no warnings about unknown
options, and both produce a `build.ninja` plus a `compile_commands.json`
in their respective build directories.

### 2. Build succeeds in both configurations with no warnings

The repo's CMake target compile options include `-Wall -Wextra
-Wpedantic -Werror`, so any warning is already a build failure. Build
in both configurations and confirm both link green.

```bash
cmake --build build
cmake --build build-release
```

**Pass criterion**: both `cmake --build` invocations exit 0. No diagnostic
text is emitted other than ninja's progress lines.

### 3. ctest passes 100 percent in both configurations

```bash
ctest --test-dir build         --output-on-failure
ctest --test-dir build-release --output-on-failure
```

**Pass criterion**: each run reports `100% tests passed, 0 tests failed`.
The single-symbol floor is 101 tests (35 unit plus 60 integration plus
6 PostOnly/FOK and `Book::erase_empty_level` coverage tests). Later
work adds to this count.

### 4. Coverage on libmeridian.a is at least 85 percent

This step uses `lcov` and `genhtml`. If `lcov` is not available on the
host, `gcovr` (a Python wrapper around `gcov`) computes the same
line-coverage percentage. Both are acceptable; the percentage on
`src/` plus `include/meridian/` is the single number to sign off on.

#### lcov pipeline (when lcov is installed)

```bash
cd /home/mustafa/src/Meridian
rm -rf build-coverage
cmake -B build-coverage -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -O0" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
  -DCMAKE_SHARED_LINKER_FLAGS="--coverage"
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
lcov --capture --directory build-coverage --output-file coverage.info \
  --no-external --base-directory $(pwd) \
  --ignore-errors mismatch,gcov,unused
lcov --extract coverage.info '*/include/meridian/*' '*/src/*' \
  --output-file coverage_filtered.info \
  --ignore-errors unused
lcov --summary coverage_filtered.info
```

#### gcovr fallback (when lcov is not installed)

```bash
cd /home/mustafa/src/Meridian
rm -rf build-coverage
cmake -B build-coverage -S . -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -O0" \
  -DCMAKE_EXE_LINKER_FLAGS="--coverage" \
  -DCMAKE_SHARED_LINKER_FLAGS="--coverage"
cmake --build build-coverage
ctest --test-dir build-coverage --output-on-failure
gcovr --root . \
      --filter 'src/.*' \
      --filter 'include/meridian/.*' \
      --object-directory build-coverage \
      --txt
```

**Pass criterion**: line coverage on `libmeridian.a` (the union of
`src/*.cpp` and `include/meridian/*.hpp`) is at least 85 percent.
Single-symbol baseline is 97 percent.

#### Acceptable gaps

The following lines may legitimately appear as uncovered without
blocking the milestone:

* `src/order_pool.cpp` lines for the debug allocator's `fprintf` plus
  `abort()` and the `throw std::bad_alloc{}` in the `operator new`
  override. These are reached only when a heap allocation occurs while
  the matching hot path is active or when the system is out of memory.
  Triggering them would abort the test process or simulate OOM. They
  are defensive and unreachable from test code.
* `src/matching.cpp` lines that report `#####` only on the open-brace
  line of a multi-line `out.push_back(ExecutionReport{...})` while the
  inner field lines are hit. This is a known gcov 15.x reporting
  artifact; the branch is exercised. Confirm the branch is hit by
  running `gcov` directly on the file and inspecting the inner field
  line counts.

Any other uncovered line is a gap to fill with a unit test, or to mark
in the test file (not in `src/`) with a comment naming the future
milestone that will fill it.

### 5. Integration corpus passes 60 scenarios

```bash
ctest --test-dir build --output-on-failure -R EngineVsReferenceTest
```

**Pass criterion**: 60 parameterized cases pass. The breakdown is:

* `HandCrafted` (15): the original bootstrap scenarios.
* `LimitWorkedExamples` (7): LIM-1..LIM-7 from
  `docs/risk/matching-semantics.md` section 3.
* `MarketWorkedExamples` (6): MKT-1..MKT-6 from section 4.
* `IocWorkedExamples` (7): IOC-1..IOC-7 from section 5.
* `CancelWorkedExamples` (6): CXL-1..CXL-6 from section 6.
* `CompoundScenarios` (9): chained sequences exercising more than one
  worked-example transition per scenario.
* `CornerCases` (9) plus `CornerCasesRandom` (1): the corner-case list.

If any scenario fails, the C++ engine and the Python reference
disagree on at least one execution report. The reference is presumed
correct unless a known reference bug is identified; either way the
divergence has to be resolved before the milestone closes.

### 6. Python reference test suite passes

```bash
cd /home/mustafa/src/Meridian
python3 -m unittest discover tests/reference -v
```

**Pass criterion**: 29 tests run, all OK. The reference is the
canonical answer for "what should the C++ engine have produced?" so a
reference regression invalidates the integration corpus's diff.

---

## Later additions

Items below are not run for the single-symbol close. They land with
their respective milestone. When you add an item here, also extend the
relevant section above with the concrete commands.

* **Multi-symbol diff** (folded into the set above once landed): the
  multi-instrument integration test
  `tests/integration/test_multi_instrument.cpp` passes 6 scenarios
  driving 5 symbols (AAPL=1, SPY=2, NVDA=3, TSLA=4, GOOG=5) against
  the Python reference: independent activity per symbol, cross-symbol
  cancel isolation, cross-symbol cancel routing by id, unknown-symbol
  reject, id reuse after cancel on a different symbol, and a 50-event
  mixed-symbol corpus with a deterministic seed. The Python reference
  suite must also pass with the multi-symbol extension landed
  (`python3 -m unittest discover tests/reference -v` reports at least
  40 tests passing).
* **Property-based tests**: hand rolled deterministic generators run
  the five in-scope single-symbol invariants under `tests/property/`
  at 1000 generated cases per invariant per CI run, plus a
  differential test that diffs 100 random sequences byte-for-byte
  against the Python reference. The seqlock concurrency milestone
  added the spread non-negative and top-of-book correctness invariants
  (`SpreadNonNegative`, `SnapshotMatchesBook`); the two remaining
  deferred invariants (FOK all-or-nothing, post-only never crosses)
  land alongside the post-only / FOK milestone. Acceptance for the
  in-scope set is a one-time pass at 10000 generated sequences per
  invariant with zero failures.
* **TSAN clean for the seqlock**: `tests/concurrency/test_seqlock_concurrent`
  is built with ThreadSanitizer and runs as part of every CI run for
  three seconds (one writer, two readers, zero torn reads). Acceptance
  is a one-time pass at `MERIDIAN_TSAN_DURATION_SEC=60` with zero
  TSAN warnings and zero torn-read mismatches.
* **Benchmark regression**: `meridian-bench` runs in CI on every PR
  and compares against `bench/baseline.json`; the build fails if
  throughput drops more than 5 percent or if any of p50, p99, p99.9
  latency percentiles increase more than 10 percent.
* **ITCH integration**: a 5-minute NASDAQ ITCH 5.0 sample is replayed
  through both the C++ engine and the Python reference; the final
  book state and total fill count must match exactly.
* **WebSocket smoke**: `meridian-server` runs against a 60-second
  replay; a client receives a `snapshot` followed by at least one
  `delta`; the JSON shape matches `docs/api/websocket.md`.
* **Frontend audit**: `pnpm --filter frontend test` is green; Vitest
  covers every component on at least the happy path; a WCAG AA pass
  signs off; the frontend build size and Lighthouse scores meet the
  targets in `docs/perf/budget.md`.

---

## How to record a checklist run

When a milestone closes, append one line to the closing PR description
(or merge commit body) recording:

* Date.
* Milestone name.
* Items 1..6 (or expanded list) each marked PASS or FAIL.
* The coverage percentage and the integration test count for that
  milestone.

Example for the single-symbol close:

```
2026-05-09 single-symbol checklist:
  1. cmake configure: PASS (Debug, Release)
  2. cmake build:     PASS (Debug, Release; no warnings under -Werror)
  3. ctest:           PASS (101 of 101 in both)
  4. coverage:        PASS (97 percent on libmeridian.a)
  5. integration:     PASS (60 of 60 scenarios)
  6. python ref:      PASS (29 of 29)
```
