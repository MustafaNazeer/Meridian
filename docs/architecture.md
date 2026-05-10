# Meridian architecture

> Audience: a developer who has never seen this project before and wants to understand it well enough to navigate the codebase in roughly 15 minutes. For the higher level project framing (what Meridian is, who it is for, what the resume bullet says) read [`README.md`](../README.md). For the per-decision rationale, read the ADRs under [`docs/adr/`](adr/). For the matching invariants and worked examples, read [`docs/risk/matching-semantics.md`](risk/matching-semantics.md).

## 1. One paragraph orientation

Meridian is a price-time priority limit order book and matching engine, written in C++20, plus a live web visualization. The engine is delivered as a static library (`libmeridian.a`) plus three thin driver binaries: `meridian-bench` (zero I/O throughput and latency benchmark), `meridian-replay` (CLI ITCH replayer that streams JSON Lines), and `meridian-server` (the live demo with uWebSockets serving a React frontend hosted on Cloudflare Pages). The library and the bench binary contain zero networking code, so the headline events per second number is defensible in isolation. The matching loop is single threaded and pinned to its own core; all I/O happens on other cores via a seqlock protected top of book snapshot, so the matching loop never blocks on the network or the file system.

## 2. High level diagram

This diagram is the canonical picture of the runtime topology.

```
+--------------------------- meridian-server (one binary) -------------------------+
|                                                                                  |
|  +--------------+    +------------------+    +--------------------+              |
|  |  Replayer    |--->|  Match Loop      |--->|  TopOfBook         |              |
|  |  ITCH 5.0    |    |  core 4, pinned  |    |  Sampler 30 Hz     |              |
|  |              |    |                  |    |  core 6, pinned    |              |
|  +--------------+    +------------------+    +--------------------+              |
|                              |                         |                         |
|                              v                         v                         |
|         +---------------------------------------------------------+              |
|         |              libmeridian.a (static)                     |              |
|         |  Order, Level, Book, BookRegistry, MatchingEngine       |              |
|         |  Seqlock-protected TopOfBookSnapshot                    |              |
|         |  OrderPool (free-list, zero alloc on hot path)          |              |
|         +---------------------------------------------------------+              |
|                                                        |                         |
|                                                        v                         |
|                                              +--------------------+              |
|                                              |  uWebSockets       |              |
|                                              |  core 5, pinned    |              |
|                                              |  serves /  + /ws   |              |
|                                              +--------------------+              |
+------------------------------------------------------+---------------------------+
                                                       | WSS
                                                       v
                              +------------------------------------+
                              |  React 19 + Vite + TS + Tailwind   |
                              |  Cloudflare Pages                  |
                              |  Twilight visual identity          |
                              +------------------------------------+
```

The library `libmeridian.a` is the engine. Three thin binaries link against it (see section 3).

## 3. Per module overview

### 3.1 libmeridian.a (the engine)

The static library that contains every line of matching logic. It is compiled once and linked into all three driver binaries. The library has no networking code, no logging on the hot path (level filtered out at compile time), and no heap allocation on the hot path. Its public headers live in `include/meridian/` and its implementation in `src/`.

Core types:

* **Order**: 64 byte struct, fits one cache line. Holds id, symbol, side, type, price (32 bit fixed point ticks), remaining quantity, arrival timestamp, and the intrusive linked list pointers (`prev`, `next`) used to splice into a `Level`.
* **Level**: doubly linked list of `Order`s at a single price, owning total quantity and order count. Insertion is at the tail (FIFO).
* **Book**: one side (bid or ask) of one symbol's order book. A sorted map of `Level`s by price plus an `unordered_map<OrderId, Order*>` for O(1) cancel by id.
* **OrderPool**: free list allocator over a fixed preallocated arena. The matching loop never calls operator new. A debug build allocator override aborts on any heap allocation during the hot path so this invariant is checked, not assumed.
* **MatchingEngine**: single writer state machine that consumes `EngineEvent` (NewOrder, Cancel) and produces `ExecutionReport` (Fill, Acknowledge, Reject, Cancel). Implements price-time priority across all five order types (limit, market, IOC, post-only, FOK).
* **BookRegistry**: `unordered_map<Symbol, Book>` for multi-instrument dispatch. Symbols are pre-allocated at startup from the ITCH directory message; no symbols are added later.
* **TopOfBookSnapshot**: the seqlock protected struct that the publish path uses to hand top of book updates to readers without blocking the matching loop. Fields: `best_bid_px`, `best_bid_qty`, `best_ask_px`, `best_ask_qty`, `last_seq`. Writer (the matching thread) bumps `seq` before and after each update; readers retry on torn read.

### 3.2 meridian-bench (apps/bench)

The headline benchmark binary. Zero networking, zero JSON, zero logging on the measurement path. Two modes: `--mode synthetic` generates Poisson distributed events with configurable arrival rate and cancel ratio; `--mode itch --tape <file>` replays an ITCH 5.0 file as fast as the engine can consume it. Reports events per second (overall and steady state), an HDR histogram of per event latency, percentile cuts (p50, p90, p99, p99.9, max), and the count of heap allocations during the hot path (which must be zero). Output is a human readable Markdown table on stdout plus a JSON sidecar at `bench/baseline.json` that CI uses for regression checks (5 percent throughput tolerance, 10 percent per percentile latency tolerance).

### 3.3 meridian-replay (apps/replay)

The CLI replayer. Streams `ExecutionReport`s as JSON Lines on stdout for piping into other tools or for diffing against the Python reference under `tests/reference/`. Flags: `--tape itch-2024-06-13.bin --speed 1.0 --symbols AAPL,SPY,NVDA,TSLA,GOOG --audit out.jsonl`. The optional `--audit out.jsonl` flag writes a deterministic audit log used in the integration tests; that flag is for offline use only and is not exposed by `meridian-server`.

### 3.4 meridian-server (apps/server)

The live demo. Reads `meridian.toml` for tape path, replay speed, listen port, allowed origins, and max client count. Starts the replayer plus the matching loop on core 4, the sampler on core 6 at 30 Hz, and the uWebSockets event loop on core 5 (per the threading model in section 5). Serves three routes: `WSS /ws` (the live data stream), `GET /healthz` (a 200 OK with a one-line JSON status, used by Fly's load balancer for readiness checks), and `GET /metrics` (JSON counters). The React SPA is not served by this binary; Cloudflare Pages owns the frontend at `meridian-demo.pages.dev`. On WebSocket connect, the client receives a `snapshot` message (full L10 book plus recent trades plus performance metrics for the subscribed symbol); thereafter the sampler builds and broadcasts `delta` messages at 30 Hz. The server keeps audit events in an in-memory ring buffer only; nothing is written to disk on the live machine: the WebSocket stream is the audit log on the demo.

### 3.5 frontend (React 19 + Vite + TypeScript + Tailwind)

A single page React app under `frontend/`. Seven panels: Header (with brand mark, live-status dot, and a five-symbol switcher for AAPL, SPY, NVDA, TSLA, GOOG), Hero (symbol, last price, change, spread, volume), Ladder (L8 depth ladder with FIFO tooltips on hover), DepthChart (hand rolled SVG, gradient filled bid and ask areas, dashed gold mid line), Tape (last 12 trades with notional, aggressor, price up or down coloring), PerfPanel (throughput headline, latency histogram, p50/p99/p99.9 grid, replay progress), and Footer (build info, core pinning, seqlock reader count).

The WebSocket client (`frontend/src/ws/client.ts`) implements a four state connection state machine: `connecting` (initial; renders an explicit "engine warming up" hero), `live` (snapshot received, deltas streaming), `stalled` (no delta in 1 second; header dot turns amber, last good state still shown), `disconnected` (socket closed; exponential backoff with 500 ms initial, doubling, cap 8 s, then back to `connecting`). On symbol change, the client unsubscribes from the previous topic, subscribes to the new one, and re-enters `connecting` until the new `snapshot` arrives. State is managed by Zustand. Charting is hand rolled SVG (data is small: at most 20 levels per side and at most 50 recent trades, so a chart library is overkill).

### 3.6 deploy topology

* **Frontend**: built with `pnpm build`. v1 will deploy to Cloudflare Pages at `meridian-demo.pages.dev` (default subdomain; no custom domain for v1). The deploy on `main` push will run from `.github/workflows/deploy.yml` once Phase 10 wires it up.
* **Engine**: packaged into a multi stage Dockerfile (builder stage with the C++ toolchain, runtime stage with Debian slim and the `meridian-server` binary plus the sample ITCH tape, non root user). Deployed to a Fly.io machine described by `fly.toml` with `auto_stop_machines = true`, `auto_start_machines = true`, `min_machines_running = 0`, healthcheck against `/healthz`. Region default `iad` (Ashburn, Virginia). The Fly app name is picked at deploy time (likely `meridian` or `meridian-engine`); the resulting WebSocket origin `wss://<app>.fly.dev` flows into the frontend via `VITE_WS_URL` and into the frontend's CSP `connect-src`. Cold start UX (5 to 15 seconds while Fly wakes the machine) is handled by the `EngineWarmingUp` component on the frontend.

## 4. Data flow

End to end, an ITCH message becomes a rendered tick on the browser through six stages:

1. **Ingest**: the replayer decodes the next ITCH message into an `EngineEvent` struct and pushes it onto a single producer queue.
2. **Match**: the matching loop on core 4 pops the event, calls `Book::apply(event)` against the relevant symbol's book, and generates zero or more `ExecutionReport`s.
3. **Publish**: after each event, the matching loop atomically updates the symbol's `TopOfBookSnapshot` using the seqlock writer protocol (section 5).
4. **Sample**: the sampler thread on core 6 wakes at 30 Hz, reads all symbols' snapshots using the seqlock reader protocol with retry, and builds a JSON delta describing the changes since the last sample.
5. **Push**: the sampler hands the delta to the uWebSockets event loop on core 5, which broadcasts it to all connected clients on the WebSocket topic for that symbol.
6. **Render**: the frontend's WebSocket client receives the delta, applies it to the Zustand store, and React re-renders the affected panels.

The matching loop never touches a socket, never serializes JSON, never holds a mutex. Its only interaction with the outside world is updating the seqlock protected snapshot and pushing execution reports onto a separate ring buffer that the audit consumer drains.

## 5. Threading model

The server pins three threads to three different cores so the matching loop is never preempted by I/O work:

| Thread | Core | Responsibility |
|---|---|---|
| Matching loop | 4 (pinned) | Drains the event queue, applies events to books, publishes top of book via the seqlock writer protocol, pushes execution reports onto the audit ring buffer. No I/O, no mutex, no logging on the hot path. |
| Sampler | 6 (pinned) | Wakes at 30 Hz, reads each symbol's seqlock snapshot with retry, builds a JSON delta, hands it to the uWebSockets event loop. Owns no engine state. |
| uWebSockets event loop | 5 (pinned) | Accepts WebSocket connections, sends snapshots on connect, broadcasts deltas at 30 Hz, culls dropped clients. Never touches engine state directly. |

The seqlock protocol on `TopOfBookSnapshot` is what makes the publish step lock free. The writer increments `seq` (odd value: write in progress), writes the data fields (with `release` ordering), increments `seq` again (even value: write complete). The reader loads `seq`, reads the data, loads `seq` again, and retries if the two `seq` reads differ or if `seq` is odd. The ADR for this choice over the alternatives (RWLock, RCU, hazard pointers) will be filed alongside the seqlock implementation. Memory ordering specifics live in that ADR.

## 6. Hosting topology

| Component | Host | URL |
|---|---|---|
| React frontend | Cloudflare Pages (free tier, instant deploys, default subdomain) | `meridian-demo.pages.dev` (v1 will) |
| `meridian-server` engine | Fly.io machine (free tier, Dockerfile plus `fly.toml`, auto stop on idle, auto start on first request, default region `iad`) | `wss://<fly-app>.fly.dev/ws` (v1 will) |

Fly.io was picked over a VPS plus systemd because Render does not host raw C++ binaries, Fly machines do, the free tier is generous for a single small machine running an ITCH replay, and there are no servers to keep patched. The cold start (5 to 15 seconds while Fly wakes the machine after idle) is acceptable for a portfolio demo; the frontend renders an explicit "engine warming up" state during this window rather than appearing broken.

CI/CD is GitHub Actions. Three workflows: `ci.yml` (build matrix on clang and gcc plus frontend build and tests, on every PR), `bench.yml` (manual trigger; runs `meridian-bench` and diffs against `bench/baseline.json`), and `deploy.yml` (deploys frontend to Cloudflare Pages and engine to Fly.io on `main` push, once the deploy is wired up).

## 7. File layout

```
/home/mustafa/src/Meridian/
|-- README.md                  Public facing pitch
|-- design-mockups/            5 visual direction mockups (Twilight is the chosen one)
|-- docs/
|   |-- setup-guide.md         Deployment walkthrough
|   |-- architecture.md        This document
|   |-- design/                Canonical visual reference (canonical.html), wireframes, tokens
|   |-- security/              Threat model, checklists, secrets reference
|   |-- qa/                    Regression checklist
|   |-- perf/                  Latency budgets and findings
|   |-- observability/         Logging and metrics runbooks
|   |-- risk/                  Matching engine correctness conventions and audit cases
|   |-- adr/                   Architecture Decision Records (numbered)
|   `-- api/                   WebSocket protocol spec
|-- include/meridian/          Public library headers
|-- src/                       libmeridian.a implementation
|-- apps/
|   |-- bench/                 meridian-bench
|   |-- replay/                meridian-replay
|   `-- server/                meridian-server
|-- tests/
|   |-- unit/                  GoogleTest unit tests
|   |-- property/              property based tests, hand rolled generators
|   |-- integration/           Integration tests (engine vs Python reference)
|   |-- concurrency/           TSAN tests for the seqlock and sampler
|   `-- reference/             Python reference implementation (matching, ITCH)
|-- bench/                     Benchmark baseline JSON for CI regression checks
|-- third_party/               Vendored dependencies (CMake FetchContent)
`-- frontend/                  React app for the live demo
```

## 8. Where to read next

* **For the per-decision rationale**: `docs/adr/`. Numbered, smallest delta per file.
* **For the visual ground truth** (every color, font size, spacing value, component anatomy): `docs/design/canonical.html`. Open it in a browser.
* **For the matching invariants and risk conventions**: `docs/risk/matching-semantics.md`.
* **For the latency budget and benchmark methodology**: `docs/perf/budget.md`.
* **For the deploy walkthrough**: `docs/setup-guide.md`.
