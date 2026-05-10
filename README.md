# Meridian

A price-time priority limit order book and matching engine in C++20, with a live web visualization.

> **Status**: under construction. Project scaffolded 2026-05-09; foundations in progress, with the engine's first line of code following in the next milestone. Every concrete metric and URL on this page is labeled "target" or "v1 will" until measured or deployed. This README will be rewritten with real numbers and a real demo link once the live deploy lands.

## What this is

Meridian is the venue half of a two-project quant systems narrative on the resume. [Vega](https://github.com/MustafaNazeer/Vega) prices instruments. Meridian is the place those instruments would trade.

The engine implements strict price-time priority (FIFO within each price level) and supports five order types: limit, market, immediate or cancel (IOC), post-only, and fill or kill (FOK), plus cancel by id. It is multi-instrument: one matching thread handles many symbols via a symbol keyed dispatch. Top of book is published to readers (the sampler thread, the WebSocket server) through a seqlock protected snapshot, so the matching loop never blocks on I/O. The single threaded matching loop is the headline performance number.

## v1 targets

Every number in this section is a **target, not yet measured**. Real numbers replace these once `meridian-bench` runs on a fixed reference machine with the final binary.

* **Throughput target**: 6M events per second, single threaded.
* **Latency target**: p50 under 500 ns, p99 under 2 us, p99.9 under 5 us per event.
* **Correctness target**: 10 matching invariants. Six are verified today by hand rolled property based tests in `tests/property/`, each running 1000 generated cases per CI run; the remaining four (spread non negative, top of book monotonicity, FOK all or nothing, post only never crosses) land alongside the seqlock and post only / FOK milestones. The C++ engine's audit log is diffed byte for byte against a Python reference implementation under `tests/reference/` on every test run, including 100 random sequences in the property suite.
* **Replay target**: NASDAQ ITCH 5.0 binary tapes, parsed and replayed against the engine.
* **Live demo target**: 30 Hz web visualization with the engine running in real time, decoupled from the matching loop via the seqlock protected snapshot.

## How it ships

Three thin driver binaries link against `libmeridian.a`:

| Binary | Purpose | Networking |
|---|---|---|
| `meridian-bench` | Pure throughput and latency benchmark. The headline events per second target comes from this binary. | None. |
| `meridian-replay` | CLI: replays an ITCH tape, streams JSON Lines (fills, cancels, top of book) to stdout. | None. |
| `meridian-server` | Live demo: replayer plus matching loop plus 30 Hz sampler plus uWebSockets. | uWebSockets only. |

The library and the bench binary contain zero networking code, so the headline number is defensible in isolation.

## Tech stack

| Layer | Choice |
|---|---|
| Engine language | C++20 (clang 19+ primary, gcc 14+ secondary) |
| Build | CMake 3.25+ with Ninja |
| Tests | GoogleTest, with hand rolled deterministic generators for property based invariants under `tests/property/` |
| Benchmark | Google Benchmark plus a hand rolled HDR histogram latency harness |
| WebSocket library | uWebSockets v20+ |
| ITCH parser | Hand rolled, single header |
| Logging | spdlog, level filtered out of the release hot path |
| JSON | simdjson (parse), glaze (serialize) |
| Reference implementation | Python 3.11 plus pytest under `tests/reference/` |
| Frontend framework | React 19 plus Vite plus TypeScript |
| Frontend styling | Tailwind CSS |
| State management (frontend) | Zustand |
| Charts | Hand rolled SVG (no chart library) |
| Frontend host | Cloudflare Pages |
| Backend host | Fly.io machine (free tier, Dockerfile plus `fly.toml`, auto stop on idle, auto start on first request) |
| Package manager (frontend) | pnpm |
| CI/CD | GitHub Actions |
| License | MIT |

## Visual design

The live demo's visual identity is the **Twilight** theme: deep indigo `#0E1126` background, gold `#D4A24C` accent, muted teal `#4FA89E` for bid, faded coral `#C5765B` for ask, off-white `#EFE9DC` ink. Display type is Newsreader italic, body type is Inter, all numerics are JetBrains Mono with `font-variant-numeric: tabular-nums`. The canonical design HTML at [`docs/design/canonical.html`](docs/design/canonical.html) is the visual ground truth; the React implementation under `frontend/` mirrors its tokens, anatomy, and personality.

Twilight was chosen on 2026-05-09 from a field of five candidate directions. The four rejected directions (Bourse, Terminal, Eikon, Onyx) live under [`design-mockups/themes/`](design-mockups/themes/) for design archaeology. The contrast between Vega's "Oxblood" theme (warm dark reds) and Meridian's "Twilight" theme (cool indigos with gold) is intentional so the two projects read as a constellation rather than two clones of the same template.

## Live demo

v1 will deploy the frontend to **`meridian-demo.pages.dev`** (Cloudflare Pages default subdomain; no custom domain for v1) and the engine to a Fly.io machine reachable at `wss://<fly-app>.fly.dev/ws`. The Fly app name and region are picked at deploy time. Neither URL is live yet; this section will link to a working demo once the deploy ships.

The Fly machine auto stops when idle and auto starts on the first request, so the first WebSocket connection after a quiet period takes roughly 5 to 15 seconds while Fly wakes the machine. The frontend renders an explicit "engine warming up" state during this window rather than appearing broken.

## Status

The build is sequenced into shippable milestones (foundations, single-symbol matching, multi-instrument and cancel, property tests, concurrency, the bench push, ITCH replay, the remaining order types, the WebSocket server, the frontend, the deploy, the public README rewrite). Single-symbol matching, multi-instrument and cancel, and the property based invariant suite have landed; concurrency (the seqlock protected top of book and TSAN coverage) is next.

## Entry points

* [`docs/architecture.md`](docs/architecture.md): a 15 minute orientation for a developer new to the codebase.
* [`docs/risk/matching-semantics.md`](docs/risk/matching-semantics.md): the matching invariants and the worked-example catalogue the integration tests are built around.
* [`docs/adr/`](docs/adr/): architecture decision records (numbered; start with 0001 for the library + drivers split).
* [`docs/security/threat-model.md`](docs/security/threat-model.md): the threat model and hardening posture for the live deploy.
* [`tests/reference/`](tests/reference/): the Python reference matching engine that the C++ engine is diffed against in CI.

## Repository layout

```
include/meridian/             public library headers
src/                          libmeridian.a implementation
apps/{bench,replay,server}/   three driver binaries
tests/                        unit, property, integration, concurrency, reference
bench/                        CI benchmark baseline JSON
frontend/                     React app for the live demo
docs/                         design, security, perf, risk, ADRs
```

## License

MIT. A `LICENSE` file will land alongside the public deploy.
