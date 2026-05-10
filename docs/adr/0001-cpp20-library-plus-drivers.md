# ADR 0001: Ship as a C++20 static library plus three driver binaries

**Date**: 2026-05-09
**Status**: Accepted
**Deciders**: Project Manager, Engine Developer, Performance Engineer
**Source**: design spec section 4 (`docs/superpowers/specs/2026-05-09-meridian-design.md`)

## Context

Meridian is a price-time priority limit order book and matching engine, plus a live web visualization. The headline performance bullet on the resume is "single threaded matching loop, 6M events per second target, sub microsecond p50 latency target". That number is only defensible if the binary that produced it is the same binary that contains the matching logic, with zero ambient cost from networking, JSON serialization, or logging on the measurement path.

There are several plausible ways to package the engine:

1. **One monolithic binary** that is the benchmark, the replayer, and the live server depending on a `--mode` flag. Simple to build; muddies the benchmark story (the benchmark binary contains networking code, even if it is never exercised, which inflates binary size and may affect inlining and code layout decisions the compiler makes).
2. **One library plus one binary**, where the binary is the live server and the benchmark and the replayer are subcommands. Slightly cleaner but still ships networking code with the benchmark.
3. **One static library plus three thin driver binaries**: `meridian-bench` (no networking), `meridian-replay` (no networking), `meridian-server` (uWebSockets). Each driver links the library and adds only what it needs.

The matching engine itself is identical across all three binaries; the only thing that varies is what surrounds it.

## Decision

Ship the engine as a **static library `libmeridian.a`** plus **three thin driver binaries**:

* **`meridian-bench`**: pure throughput and latency benchmark. Zero networking code linked in. The 6M events per second target is reported by this binary, not by the live server.
* **`meridian-replay`**: CLI ITCH replayer. Streams `ExecutionReport`s as JSON Lines on stdout. Zero networking code; the JSON serialization is for the audit log, not for a wire protocol.
* **`meridian-server`**: the live demo. Replayer plus matching loop plus 30 Hz sampler plus uWebSockets, three threads pinned to three different cores.

The library lives in `include/meridian/` (public headers) and `src/` (implementation). The three drivers live in `apps/bench/`, `apps/replay/`, and `apps/server/`. Each driver is a thin `main.cpp` that links the library; the drivers contain no matching logic of their own.

## Consequences

### Positive

* **The headline benchmark number is defensible in isolation.** A reviewer who runs `meridian-bench` is running the same matching code that runs in production, but without any of the I/O, JSON, or networking overhead that would inflate the measurement. The library and the bench binary contain zero networking code by construction.
* **Each driver can be reasoned about in isolation.** The bench binary's failure modes are bench failure modes; the server's failure modes are server failure modes. There is no shared `--mode` switch path that has to handle both.
* **Build artifact size stays honest.** The bench binary does not pay for uWebSockets symbols; the replayer does not pay for them either. The server pays for what it uses.
* **CI gets cheaper.** The bench and the integration tests can build the library plus the bench binary without ever pulling uWebSockets, so PRs that do not touch networking finish faster.
* **The library is reusable.** A future strategy gateway, a future shared memory ring buffer process per symbol architecture (design spec section 12 lists these as out of scope future directions), or a future C++ to Rust port comparison can all link the same library.

### Negative

* **Three CMake targets to maintain instead of one.** Mitigated by the per directory `CMakeLists.txt` pattern under `apps/` so each driver's build description lives next to its source.
* **Slightly more boilerplate in the test setup.** Tests link against the library directly, not against any driver. This is the right shape, but it is one more CMake `target_link_libraries` line per test target.
* **Slightly more build configuration surface to keep clean.** PGO has to be configured per driver; the bench binary's PGO profile is the one that matters for the headline number, and the server's PGO profile is the one that matters for the live demo. The Performance Engineer owns this in Phase 5.

### Neutral

* The "library plus drivers" split mirrors the layout the C++ ecosystem expects (e.g., LLVM, Boost.Asio examples, most internal exchange codebases the user's resume audience would recognize). It is not an unusual choice.

## Alternatives considered

* **Option 1 (one monolithic binary)** rejected because it muddies the benchmark story. A reviewer who runs the benchmark wants to know that the only thing the binary does on its hot path is match orders.
* **Option 2 (one library plus one binary)** rejected because the live server still ships networking code, so a benchmark mode inside that binary still has the same defensibility problem as Option 1.

## References

* Design spec section 4 (high level architecture): `docs/superpowers/specs/2026-05-09-meridian-design.md`.
* SPEC.md "Project summary" paragraph: "The library and the bench binary contain zero networking code, so the headline 6M events per second target will be defensible in isolation once measured in Phase 5."
* Repo layout: design spec section 10, plus `CLAUDE.md` "Directory layout" section.
