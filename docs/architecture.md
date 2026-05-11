# Meridian architecture

> Audience: a developer who has never seen this project before and wants to understand it well enough to navigate the codebase in roughly 15 minutes. For the higher level project framing (what Meridian is, who it is for, what the resume bullet says) read [`README.md`](../README.md). For the per-decision rationale, read the ADRs under [`docs/adr/`](adr/). For the matching invariants and worked examples, read [`docs/risk/matching-semantics.md`](risk/matching-semantics.md).

## 1. One paragraph orientation

Meridian is a price-time priority limit order book and matching engine, written in C++20, plus a live web visualization. The engine is delivered as a static library (`libmeridian.a`) plus three thin driver binaries: `meridian-bench` (zero I/O throughput and latency benchmark), `meridian-replay` (CLI ITCH replayer that streams JSON Lines), and `meridian-server` (the live demo, with a hand rolled `poll()`-based WebSocket server feeding a React frontend hosted on Cloudflare Pages). The library and the bench binary contain zero networking code, so the headline events per second number is defensible in isolation. The matching loop is single threaded; all I/O happens on other threads via a seqlock protected top of book snapshot, so the matching loop never blocks on the network or the file system.

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
|                                              |  WsServer (poll)   |              |
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
* **TopOfBookSnapshot**: the seqlock protected struct that the publish path uses to hand top of book updates to readers without blocking the matching loop. Fields: `best_bid_px`, `best_bid_qty`, `best_ask_px`, `best_ask_qty`, `ts` (the event timestamp at publish). The sequence counter is managed inside `SeqlockSnapshot` (see `include/meridian/seqlock_snapshot.hpp`); the writer (the matching thread) bumps `seq` to odd, stores each data field, then bumps `seq` to even. Readers retry on a torn read. See ADR 0003 for the per-field `std::atomic<T>` layout, the memory ordering, and the alternatives (raw fields plus fences, RWLock, RCU, hazard pointers) that were rejected.

### 3.2 meridian-bench (apps/bench)

The headline benchmark binary. Zero networking, zero JSON, zero logging on the measurement path. Two modes: `--mode synthetic` generates Poisson distributed events with configurable arrival rate and cancel ratio; `--mode itch --tape <file>` replays an ITCH 5.0 file as fast as the engine can consume it. Reports events per second (overall and steady state), an HDR histogram of per event latency, percentile cuts (p50, p90, p99, p99.9, max), and the count of heap allocations during the hot path (which must be zero). Output is a human readable Markdown table on stdout plus a JSON sidecar at `bench/baseline.json` that CI uses for regression checks (5 percent throughput tolerance, 10 percent per percentile latency tolerance).

### 3.3 meridian-replay (apps/replay)

The CLI replayer. Streams `ExecutionReport`s as JSON Lines on stdout for piping into other tools or for diffing against the Python reference under `tests/reference/`. Flags: `--tape itch-2024-06-13.bin --speed 1.0 --symbols AAPL,SPY,NVDA,TSLA,GOOG --audit out.jsonl`. The optional `--audit out.jsonl` flag writes a deterministic audit log used in the integration tests; that flag is for offline use only and is not exposed by `meridian-server`.

### 3.4 meridian-server (apps/server)

The live demo. CLI: `--port N` (0 = OS-assigned; the assigned port is announced on stdout), `--rate N` (synthetic engine event rate), `--seed N`. Three threads cooperate: the engine thread runs a synthetic limit/market/cancel mix through `MatchingEngine::apply`; the sampler thread wakes at 30 Hz, reads the seqlock snapshot, and hands a JSON delta to the WS server; the main thread runs `WsServer::serve_forever()` over a `poll()` loop on the listen socket plus connected client fds. Serves three routes: `WSS /ws` (the live data stream), `GET /healthz` (a 200 OK with a one-line JSON status, used by Fly's load balancer for readiness checks), and `GET /metrics` (JSON counters: connections total, active, broadcasts total, bytes sent, handshake failures). The React SPA is not served by this binary; Cloudflare Pages owns the frontend at `meridian-demo.pages.dev`. On WebSocket connect, the client receives a `snapshot` message (today: best bid/ask only; L10 depth and recent trades are documented as follow-ups in `docs/qa/regression-checklist.md`); thereafter the sampler builds and broadcasts `delta` messages at 30 Hz. The server keeps audit events in an in-memory ring buffer only; nothing is written to disk on the live machine: the WebSocket stream is the audit log on the demo.

### 3.5 frontend (React 19 + Vite + TypeScript + Tailwind)

A single page React app under `frontend/`. Seven panels follow the Twilight wireframes in `docs/design/wireframes.md`: Header (brand mark, live status dot, five symbol switcher for AAPL, SPY, NVDA, TSLA, GOOG, wall clock plus engine tick), Hero (symbol, mid price with up or down color, spread in basis points, top size and resting depth), Ladder (price by size by orders by cumulative, eight rows per side, real top of book on the row adjacent to the spread row and placeholder rows above and below pending the extended snapshot payload), DepthChart (hand rolled SVG with gradient filled bid and ask blocks and a dashed gold mid line), Tape (twelve placeholder rows pending the extended snapshot payload), PerfPanel (wire throughput headline measured client side, latency histogram skeleton pending engine instrumentation, book state cells, replay copy), and Footer (build info, runtime line, ws engine).

The WebSocket client (`frontend/src/ws/client.ts`) implements a four state connection state machine: `connecting` (initial; the hero renders an explicit "warming" state with a row of pulsing gold dots), `live` (snapshot or delta received, store is being updated), `stalled` (no message in 1 second; header dot recolors to gold-deep and the meta line gains a "stalled · {n}s ago" tail), `disconnected` (socket closed; exponential backoff with 500 ms initial, doubling, cap 8 s, then back to `connecting`). State is owned by a Zustand store under `frontend/src/store/dashboard.ts`. Charting is hand rolled SVG. The store also records the most recent up to 64 mid prices for the per tick "Last" color, the running wire counter totals, and a rolling one second deltas-per-second and bytes-per-second window populated by the client's perf tick.

The wire contract today is the same single message shape the server emits: a JSON object with `kind: "snapshot" | "delta"`, `bid_px`, `bid_qty`, `ask_px`, `ask_qty`, and `ts`. An empty side is encoded as price `-1` and the frontend decodes that to `null`. The Ladder, DepthChart, Tape, and the PerfPanel latency histogram are wired against this top of book payload today and explicitly flag in their section aux labels that the extended snapshot (L8 depth, recent trades, engine latency) lands in a follow-up milestone.

The Vitest suite under `frontend/src/ws/client.test.ts` and `frontend/src/store/dashboard.test.ts` covers the connection state machine (connecting -> live -> stalled -> live, exponential backoff under repeated closes, perf window flush), the store reducers (snapshot resets, delta colors up or down, equal mid keeps the prior color, symbol change resets the stream), and the wire byte / delta accounting. A short Node smoke harness at `frontend/scripts/smoke.mjs` drives the real `MeridianWsClient` against a running `meridian-server` for end to end verification outside of CI.

### 3.6 deploy topology

* **Frontend**: built with `pnpm run build`. Deploys to Cloudflare Pages project `meridian-demo` at `meridian-demo.pages.dev` (default subdomain; no custom domain for v1). The deploy workflow at `.github/workflows/deploy.yml` triggers on `workflow_dispatch` for v1 (manual button in the Actions UI) so the first merge does not run before secrets are provisioned; once the first deploy lands the trigger flips to `push: branches: [main]` in a one-line follow-up. The workflow builds with `VITE_MERIDIAN_WS=wss://meridian-engine.fly.dev/ws` baked into the bundle and publishes `frontend/dist/` via `cloudflare/pages-action@v1`.
* **Engine**: packaged into a multi-stage `Dockerfile` at the repo root. Builder stage on `debian:trixie-slim` installs `build-essential`, `cmake`, `ninja-build`, and `g++`; runtime stage on `debian:trixie-slim` keeps only `libstdc++6`, `ca-certificates`, the `meridian-server` binary, and a non-root user `meridian` (uid 10000). Deployed to a Fly.io app named **`meridian-engine`** described by `fly.toml`: primary region `iad` (Ashburn, Virginia), `shared-cpu-1x` with 256 MB RAM, `auto_stop_machines = "stop"`, `auto_start_machines = true`, `min_machines_running = 0`, healthcheck against `/healthz` every 30 s with a 10 s grace and 5 s timeout. Listen port is 8080 inside the container; Fly's edge terminates TLS and proxies HTTPS / WSS to the machine. The resulting WebSocket origin `wss://meridian-engine.fly.dev/ws` is the value baked into `frontend/.env.production` and into the deploy workflow's `VITE_MERIDIAN_WS` env.
* **Origin allowlist** (security hardening that lands with the deploy milestone): `WsServer::set_allowed_origins()` accepts a list of allowed Origin headers, and the `meridian-server` CLI exposes `--origins LIST`. An empty allowlist (the dev default and the test default) accepts any Origin; a non-empty allowlist rejects upgrades whose Origin header is missing or not in the list with HTTP 403, bumps `handshake_failures` plus a new `origin_rejects` counter, and never reaches the 101 Switching Protocols path. In production the allowlist is set by `fly.toml`'s `[env]` block (`MERIDIAN_ORIGINS = "https://meridian-demo.pages.dev,http://localhost:5173"`). Tests under `tests/unit/test_ws_origin.cpp` cover both the permissive and locked-down behaviours.
* **Cold-start UX**: the first WebSocket connection after a quiet period takes roughly 5 to 15 seconds while Fly wakes the machine. The frontend's connection state machine (see section 3.5) renders the `connecting` state during this window, with a Newsreader italic "warming" headline and a row of pulsing gold dots, so the cold start reads as expected behaviour rather than as a broken demo.

## 4. Data flow

End to end, an ITCH message becomes a rendered tick on the browser through six stages:

1. **Ingest**: the replayer decodes the next ITCH message into an `EngineEvent` struct and pushes it onto a single producer queue.
2. **Match**: the matching loop on core 4 pops the event, calls `Book::apply(event)` against the relevant symbol's book, and generates zero or more `ExecutionReport`s.
3. **Publish**: after each event, the matching loop atomically updates the symbol's `TopOfBookSnapshot` using the seqlock writer protocol (section 5).
4. **Sample**: the sampler thread on core 6 wakes at 30 Hz, reads all symbols' snapshots using the seqlock reader protocol with retry, and builds a JSON delta describing the changes since the last sample.
5. **Push**: the sampler hands the delta to `WsServer::broadcast()`, which enqueues the payload behind a mutex and lets the main thread's `poll()` loop drain it on the next wakeup (bounded by the 33 ms poll timeout).
6. **Render**: the frontend's WebSocket client receives the delta, applies it to the Zustand store, and React re-renders the affected panels.

The matching loop never touches a socket, never serializes JSON, never holds a mutex. Its only interaction with the outside world is updating the seqlock protected snapshot and pushing execution reports onto a separate ring buffer that the audit consumer drains.

## 5. Threading model

The server pins three threads to three different cores so the matching loop is never preempted by I/O work:

| Thread | Core | Responsibility |
|---|---|---|
| Matching loop | 4 (pinned) | Drains the event queue, applies events to books, publishes top of book via the seqlock writer protocol, pushes execution reports onto the audit ring buffer. No I/O, no mutex, no logging on the hot path. |
| Sampler | 6 (pinned, future) | Wakes at 30 Hz, reads each symbol's seqlock snapshot with retry, builds a JSON delta, hands it to the WS server via `broadcast()`. Owns no engine state. Today the sampler runs unpinned in `meridian-server`; the explicit core pinning lands when `meridian.toml` does. |
| WsServer event loop | 5 (pinned, future) | Accepts WebSocket connections via `poll()`, sends snapshots on connect, broadcasts deltas at 30 Hz, culls dropped clients. Never touches engine state directly. Today runs unpinned on the binary's main thread. |

The seqlock protocol on `TopOfBookSnapshot` is what makes the publish step lock free. The writer increments `seq` (odd value: write in progress), writes the data fields (with `release` ordering), increments `seq` again (even value: write complete). The reader loads `seq`, reads the data, loads `seq` again, and retries if the two `seq` reads differ or if `seq` is odd. ADR 0003 documents this choice over the alternatives (raw fields plus `std::atomic_thread_fence`, RWLock, RCU, hazard pointers). Memory ordering specifics live in that ADR.

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
