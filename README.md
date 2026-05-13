# Meridian

A price-time priority limit order book and matching engine in C++20, with a live web visualization.

**Live demo**: [meridian-orderbook.pages.dev](https://meridian-orderbook.pages.dev). The Fly machine auto-stops when idle, so the first visit after a quiet period takes roughly 5 to 15 seconds to wake; the hero renders a "warming" state during that window.

## What this is

Meridian is the venue half of a two-project quant systems narrative on the resume. [Vega](https://github.com/MustafaNazeer/Vega) prices instruments. Meridian is the place those instruments would trade.

The engine implements strict price-time priority (FIFO within each price level) and supports five order types: limit, market, immediate or cancel (IOC), post-only, and fill or kill (FOK), plus cancel by id. It is multi-instrument: one matching thread handles many symbols via a symbol-keyed dispatch. Top of book is published to readers (the sampler thread, the WebSocket server) through a seqlock-protected snapshot, so the matching loop never blocks on I/O. The single-threaded matching loop is the headline performance number.

## v1 numbers (measured)

Captured from `meridian-bench --events 1000000 --seed 42 --warmup 100000` running the release binary single-threaded against a synthetic limit / market / cancel mix. The same harness runs in CI on every PR; the CI baseline is the regression floor.

| Metric | Desktop reference | CI baseline (GitHub Actions ubuntu-24.04) |
|---|---|---|
| Throughput | 4.34 M evt/s | 4.10 M evt/s |
| Latency p50 | 158 ns | 160 ns |
| Latency p90 | 377 ns | 361 ns |
| Latency p99 | 720 ns | 721 ns |
| Latency p99.9 | 1.42 us | 2.55 us |

CI fails the build if throughput drops more than 5 percent or any of the p50, p99, p99.9 latency percentiles rise more than 10 percent versus `bench/baseline.json`. The library and the bench binary contain zero networking code, so these numbers are defensible in isolation.

**Correctness**. All ten matching invariants are verified by hand rolled property-based tests in [`tests/property/`](tests/property/), each running 1000 generated cases per CI run. The C++ engine's audit log is diffed byte-for-byte against a Python reference implementation under [`tests/reference/`](tests/reference/) on every test run, covering 100 random sequences in the property suite plus 26 worked-example scenarios across Limit, Market, IOC, Post-only, FOK, and Cancel. Total test count: 182 C++ tests plus 18 frontend Vitest cases, all green at every commit on `main`.

**Replay**. NASDAQ ITCH 5.0 binary tapes (Add Order and Order Delete messages today; the remaining message types are deferred) are parsed and replayed against the engine via [`apps/replay/`](apps/replay/), with a byte-for-byte differential test against the Python reference in [`tests/integration/test_itch_replay.cpp`](tests/integration/test_itch_replay.cpp).

## How it ships

Three thin driver binaries link against `libmeridian.a`:

| Binary | Purpose | Networking |
|---|---|---|
| `meridian-bench` | Pure throughput and latency benchmark. The headline events per second comes from this binary. | None. |
| `meridian-replay` | CLI: replays an ITCH tape, streams JSON Lines (fills, cancels, top of book) to stdout. | None. |
| `meridian-server` | Live demo: synthetic event generator plus matching loop plus 30 Hz sampler plus a hand rolled `poll()`-based WebSocket server. | TCP listen socket on the configured port; `WSS /ws` plus `GET /healthz` plus `GET /metrics`. |

The library and the bench binary contain zero networking code, so the headline number is defensible in isolation.

## Tech stack

| Layer | Choice |
|---|---|
| Engine language | C++20 (clang 19+ primary, gcc 14+ secondary) |
| Build | CMake 3.25+ with Ninja |
| Tests | GoogleTest, with hand rolled deterministic generators for property based invariants under `tests/property/` |
| Benchmark | Hand rolled HDR histogram latency harness plus throughput timer |
| WebSocket library | Hand rolled, RFC 6455, single threaded `poll()` loop in `src/ws_server.cpp`. Server-to-client text frames only; no fragmentation, no permessage-deflate. Origin header allowlist enforced on `/ws` upgrade. |
| ITCH parser | Hand rolled, single header |
| Reference implementation | Python 3.11 plus `unittest` under `tests/reference/` |
| Frontend framework | React 19 plus Vite plus TypeScript |
| Frontend styling | Tailwind CSS |
| State management (frontend) | Zustand |
| Charts | Hand rolled SVG (no chart library) |
| Frontend host | Cloudflare Pages (`meridian-orderbook.pages.dev`) |
| Engine host | Fly.io machine, `meridian-engine` app, region `iad`, `shared-cpu-1x` with 256 MB RAM, auto-stop on idle |
| Package manager (frontend) | pnpm |
| CI/CD | GitHub Actions: `ci.yml` builds + tests on every PR; `deploy.yml` ships to Cloudflare Pages and Fly.io on every merge to `main`. |
| License | MIT (see [`LICENSE`](LICENSE)) |

## Visual design

The live demo's visual identity is the **Twilight** theme: deep indigo `#0E1126` background, gold `#D4A24C` accent, muted teal `#4FA89E` for bid, faded coral `#C5765B` for ask, off-white `#EFE9DC` ink. Display type is Newsreader italic, body type is Inter, all numerics are JetBrains Mono with `font-variant-numeric: tabular-nums`. The canonical design HTML at [`docs/design/canonical.html`](docs/design/canonical.html) is the visual ground truth; the React implementation under [`frontend/`](frontend/) mirrors its tokens, anatomy, and personality.

Twilight was chosen on 2026-05-09 from a field of five candidate directions. The four rejected directions (Bourse, Terminal, Eikon, Onyx) live under [`design-mockups/themes/`](design-mockups/themes/) for design archaeology. The contrast between Vega's "Oxblood" theme (warm dark reds) and Meridian's "Twilight" theme (cool indigos with gold) is intentional so the two projects read as a constellation rather than two clones of the same template.

## How to read the demo

Open [`meridian-orderbook.pages.dev`](https://meridian-orderbook.pages.dev) and give the engine 5 to 15 seconds to wake (the cold-start UX is documented; the hero renders an explicit "warming" state during the wait).

* **Synthetic stream, not real market data.** The engine runs a synthetic limit / market / cancel mix at 400 events per second; this is the live demo rate, not the benchmark rate (the bench runs at the engine's full unbounded speed, which is the ~4 M evt/s headline above). The symbol on the hero is a label; the prices are not a real `GOOG` quote.
* **Prices in cents.** One engine tick equals one cent. The hero's "Last (mid)" stat hovers around `$100.00` because the synthetic generator anchors there with a mean-reverting random walk of ±2 cents per event. Spreads land at 1 to 3 cents, which displays as 1 to 3 basis points ("tight").
* **30 Hz wire, 4 Hz editorial cells.** The WebSocket sampler broadcasts at 30 Hz, which the PerfPanel's `wire throughput` shows live. Editorial cells (Hero stats, Ladder L1 row, DepthChart) throttle their visible refresh to 4 Hz so the numerals are legible rather than flickering. Bloomberg, Refinitiv, and Eikon all do similar throttling.
* **Connection state machine.** The header dot pulses gold while `live`, recolors to gold-deep when `stalled` (no message in 1 second), and recolors to coral when `disconnected`. A coral banner above the main grid names the reconnect backoff (500 ms initial, doubling, cap 8 s) when the socket has closed.
* **L8 depth, live.** The Ladder displays the live top eight price levels on each side. The DepthChart shows the L8 cumulative step function around the mid. The Tape carries the most-recent trade prints from the engine's ring, newest first, engine-time stamped. The PerfPanel shows the engine's live latency histogram and percentiles (cumulative since server start).

The engine's HTTP routes are public too: [`/healthz`](https://meridian-engine.fly.dev/healthz) returns a one-line JSON status, [`/metrics`](https://meridian-engine.fly.dev/metrics) returns connection and broadcast counters.

## Status

The build is sequenced into shippable milestones (foundations, single-symbol matching, multi-instrument and cancel, property tests, concurrency, the bench push, ITCH replay, the remaining order types, the WebSocket server, the frontend, the deploy, the public README rewrite, and the extended snapshot payload). All thirteen are landed: the v1.1 milestone wires L8 depth, the recent-trades tape, and a live engine latency histogram into the WebSocket envelope, so the four panels that previously rendered placeholder anatomy now render real data. See [`docs/adr/0005-extended-snapshot-payload.md`](docs/adr/0005-extended-snapshot-payload.md) for the wire-shape decision and the alternatives considered.

## Entry points

* [`docs/architecture.md`](docs/architecture.md): a 15 minute orientation for a developer new to the codebase.
* [`docs/risk/matching-semantics.md`](docs/risk/matching-semantics.md): the matching invariants and the worked-example catalogue the integration tests are built around.
* [`docs/adr/`](docs/adr/): architecture decision records (numbered; start with 0001 for the library + drivers split).
* [`docs/security/threat-model.md`](docs/security/threat-model.md): the threat model and hardening posture for the live deploy.
* [`docs/setup-guide.md`](docs/setup-guide.md): clone-to-deploy walkthrough, including the Cloudflare Pages and Fly.io deploy steps.
* [`tests/reference/`](tests/reference/): the Python reference matching engine that the C++ engine is diffed against in CI.

## Repository layout

```
include/meridian/             public library headers
src/                          libmeridian.a implementation
apps/{bench,replay,server}/   three driver binaries
tests/                        unit, property, integration, concurrency, reference
bench/                        CI benchmark baseline JSON
frontend/                     React app for the live demo
docs/                         design, security, perf, risk, ADRs, setup
Dockerfile, fly.toml          engine deploy artifacts (Fly.io)
.github/workflows/            CI plus deploy
```

## License

MIT. See [`LICENSE`](LICENSE).
