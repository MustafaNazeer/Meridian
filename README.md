# Meridian

A price-time priority limit order book and matching engine in C++20, with a live web visualization.

> **Status**: under construction. Project scaffolded 2026-05-09; build begins at Phase 1. The headline benchmark numbers and live demo URL below are **target values for v1**, not measured yet. This README will be rewritten by the Documentation Engineer in Phase 11 with real numbers and a real demo link.

## What this is

The venue half of a two-project quant systems narrative on the resume. [Vega](https://github.com/MustafaNazeer/Vega) prices instruments. Meridian is the place those instruments would trade.

The engine implements strict price-time priority (FIFO within each price level) and supports five order types: limit, market, immediate-or-cancel (IOC), post-only, and fill-or-kill (FOK), plus cancel-by-id. It is multi-instrument: one matching thread handles many symbols via a symbol-keyed dispatch. The single-thread matching loop is the headline performance number.

## Targets (v1)

* **Throughput**: 6M+ events per second, single-threaded.
* **Latency**: p50 ≤ 500 ns, p99 ≤ 2 μs, p99.9 ≤ 5 μs per event.
* **Correctness**: 10 matching invariants verified by `rapidcheck` property-based tests, ≥1000 generated cases per CI run.
* **Replay**: NASDAQ ITCH 5.0 binary tapes, parsed and replayed against the engine.
* **Live demo**: 30 Hz web visualization with the engine running in real time, decoupled from the matching loop via sequence-locked snapshots.

## How it ships

Three thin driver binaries link against `libmeridian.a`:

| Binary | Purpose | Networking |
|---|---|---|
| `meridian-bench` | Pure throughput and latency benchmark. The 6M events per second number comes from this binary. | None. |
| `meridian-replay` | CLI: replays an ITCH tape, streams JSON Lines (fills, cancels, top-of-book) to stdout. | None. |
| `meridian-server` | Live demo: replayer + matching loop + uWebSockets at the same time. | uWebSockets. |

The library and the bench binary contain zero networking code, so the headline number is defensible.

## Tech stack

| Layer | Choice |
|---|---|
| Engine language | C++20 (clang 19+ primary, gcc 14+ secondary) |
| Build | CMake 3.25+ with Ninja |
| Tests | GoogleTest plus rapidcheck (property-based) |
| Benchmark | Google Benchmark plus a hand-rolled latency harness |
| WebSocket | uWebSockets v20+ |
| ITCH parser | Hand-rolled (single-header) |
| Frontend | React 18 plus Vite plus TypeScript plus Tailwind |
| Frontend host | Cloudflare Pages |
| Backend host | Fly.io machine (free tier, Dockerfile plus `fly.toml`) |
| CI/CD | GitHub Actions |
| License | MIT |

## Visual design

The live demo's visual identity is the **Twilight** theme: deep indigo `#0E1126` background, gold `#D4A24C` accent, muted teal `#4FA89E` for bid, faded coral `#C5765B` for ask, off-white `#EFE9DC` ink, Newsreader italic for editorial display, Inter for UI body, JetBrains Mono with `tabular-nums` for all numerics. The canonical design HTML at [`docs/design/canonical.html`](docs/design/canonical.html) is the visual ground truth.

Twilight was chosen on 2026-05-09 from a field of five candidate directions. The four rejected directions (Bourse, Terminal, Eikon, Onyx) live under [`design-mockups/themes/`](design-mockups/themes/) for design archaeology.

## Entry points

* [`SPEC.md`](SPEC.md): the full project spec, agent roster, and 12 phase build plan.
* [`CLAUDE.md`](CLAUDE.md): the auto loaded brief for any Claude Code session opened in this directory.
* [`STATUS.md`](STATUS.md): single source of truth for which phase is in flight.
* [`GETTING-STARTED.md`](GETTING-STARTED.md): the first-session walkthrough.
* [`docs/superpowers/specs/2026-05-09-meridian-design.md`](docs/superpowers/specs/2026-05-09-meridian-design.md): the deep technical design (the engineering contract).
* [`docs/plan.md`](docs/plan.md): the per phase implementation plan, written by the Project Manager in Phase 0.

## Live demo

(Will be updated to the live URL after Phase 10 deployment ships.)

## Repository layout

See `CLAUDE.md` for the canonical directory tree. Headline:

```
include/meridian/   public library headers
src/                libmeridian.a implementation
apps/{bench,replay,server}/  three driver binaries
tests/              unit, property, integration tests
bench/              CI benchmark baseline JSON
frontend/           React app for the live demo
docs/               specs, design, security, perf, risk, ADRs
agents/             18 specialist briefs
```

## License

MIT. See [`LICENSE`](LICENSE) once it lands in Phase 0.
