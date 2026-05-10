# Meridian — Design Spec

**Date** 2026-05-09
**Author** Mustafa Nazeer
**Status** Draft, awaiting user review

## 1. Overview

Meridian is a price-time priority limit order book and matching engine, written in C++20, with a live web visualization. It is a portfolio project whose primary audience is hiring at trading firms, market makers, and high-frequency-finance shops, and whose secondary audience is general systems-engineering recruiters who care about low-latency C++ work.

It is the venue half of a two-project quant systems narrative on the resume: Vega prices instruments, Meridian is the place those instruments would trade.

## 2. Goals and non-goals

### 2.1 Goals

1. Match incoming orders against a resting book using strict price-time priority (FIFO within each price level).
2. Support five order types: limit, market, IOC (immediate or cancel), post-only, and FOK (fill or kill), plus cancel-by-id.
3. Sustain 6M+ events per second on a single matching thread, with sub-microsecond p50 latency and well-characterized tail latency at p99 and p99.9.
4. Multi-instrument: one engine handles many symbols via a symbol-keyed dispatch, sharing the matching thread.
5. Real-data replay: parse and replay NASDAQ ITCH 5.0 binary tapes against the engine.
6. Live web visualization at 30 Hz, decoupled from the matching loop via sequence-locked snapshots so the matching loop never blocks on I/O.
7. Property-based tests verifying matching invariants (price-time priority, quantity conservation, no double-fill, no lost orders).
8. Public benchmark report with latency histograms, design rationale, and reproducible build instructions.

### 2.2 Non-goals (deliberately out of scope)

* Not a real exchange. No clearing, settlement, margin, regulation, or risk checks.
* No live market connection. Replay only.
* No order routing across venues.
* No persistence or event sourcing in v1. The book lives in memory; restart loses state.
* No multi-user authentication on the web demo. The demo is read-only and public.
* Not Software as a Medical Device, not regulated, not a product. A portfolio piece.

### 2.3 Success criteria

* Throughput target: meridian-bench reports ≥6M events per second single-threaded on a modern x86_64 desktop (the resume bullet number).
* Latency target: p50 ≤ 500 ns, p99 ≤ 2 μs, p99.9 ≤ 5 μs on the same hardware.
* Correctness: 100% of property-based test invariants hold across at least 10,000 generated event sequences.
* Demo: the live web visualization at `meridian-demo.pages.dev` renders top of book at a steady 30 Hz with the engine replaying a real ITCH tape.
* Repo health: README with benchmark report, design doc, build instructions, and a one-line "what is this" hook a recruiter can read in 30 seconds.

## 3. Tech stack and identity

| Layer | Choice | Rationale |
|---|---|---|
| Language | C++20 | Lingua franca of HFT, matches the resume bullet, modern features (concepts, ranges, std::span) without C++23's tooling fragility |
| Build | CMake 3.25+, Ninja | Industry standard for C++ |
| Test framework | GoogleTest | Most common in HFT codebases, IDE integration |
| Property testing | rapidcheck | The C++ analog of QuickCheck/Hypothesis |
| Benchmark | Google Benchmark | Standard for microbenchmarks |
| WebSocket | uWebSockets v20+ | Single-process WS server with low overhead |
| ITCH parsing | Hand-rolled (one header file) | Format is small and stable; a parser library is overkill |
| Front end | React 19 + Vite + TypeScript + Tailwind | Vite default as of 2026-05-09. Vega's frontend is on React 18; matching majors is a non-goal. |
| Front-end host | Cloudflare Pages | Same as Vega; free tier; instant deploys |
| Back-end host | Fly.io machine, free tier, auto-stop on idle and auto-start on first request | Render does not host raw C++ binaries; Fly machines do, with a free allowance, Dockerfile + `fly.toml` deployment, and no servers to keep patched. Region resolved at Phase 10 (default `iad`). Cold-start UX handled in section 5.5. |
| License | MIT | Industry default for portfolio C++ projects |
| Repo | Public on GitHub (github.com/MustafaNazeer/Meridian) | Maximum recruiter visibility |

### 3.1 Visual identity (Twilight)

The web demo's visual identity is locked to the Twilight mockup at `design-mockups/themes/twilight.html`. Tokens:

* Background: deep indigo `#0E1126`, with a radial gradient towards `#1A1E40` at the top
* Surface: `#161A33`
* Borders: `#232847` for subtle, `#2F365A` for strong
* Ink: `#EFE9DC` (warm off-white), secondary `#BFB7A8`, soft `#8C879A`
* Accent: gold `#D4A24C` (with deeper `#B97D2C` for gradients)
* Bid: muted teal `#4FA89E`
* Ask: faded coral `#C5765B`
* Display: Newsreader (Google Fonts), italic, 400/500 weights
* Body: Inter, 400/500/600
* Numerics: JetBrains Mono with `font-variant-numeric: tabular-nums`

This visual identity must be distinct from Vega's "Oxblood" theme. No oxblood reds, no warm reds at all. Twilight's palette is cool indigo with one warm accent (gold).

## 4. High-level architecture

```
┌─────────────────────────── meridian-server (one binary) ─────────────────────────┐
│                                                                                   │
│  ┌──────────────┐    ┌──────────────────┐    ┌────────────────────┐              │
│  │  Replayer    │───▶│  Match Loop      │───▶│  TopOfBook         │              │
│  │  ITCH 5.0    │    │  core 4, pinned  │    │  Sampler 30 Hz     │              │
│  │              │    │                  │    │  core 6, pinned    │              │
│  └──────────────┘    └──────────────────┘    └────────────────────┘              │
│                              │                         │                          │
│                              ▼                         ▼                          │
│         ┌───────────────────────────────────────────────────────┐                │
│         │              libmeridian.a (static)                   │                │
│         │  Order · Level · Book · BookRegistry · MatchingEngine │                │
│         │  Seqlock-protected TopOfBookSnapshot                  │                │
│         │  OrderPool (free-list, zero alloc on hot path)        │                │
│         └───────────────────────────────────────────────────────┘                │
│                                                        │                          │
│                                                        ▼                          │
│                                              ┌────────────────────┐               │
│                                              │  uWebSockets       │               │
│                                              │  core 5, pinned    │               │
│                                              │  serves /  + /ws   │               │
│                                              └────────────────────┘               │
└──────────────────────────────────────────────────────┬───────────────────────────┘
                                                       │ WSS
                                                       ▼
                              ┌────────────────────────────────────┐
                              │  React 19 + Vite + TS + Tailwind   │
                              │  Cloudflare Pages                  │
                              │  Twilight visual identity          │
                              └────────────────────────────────────┘
```

The library `libmeridian.a` is the engine. Three thin binaries link against it:

* **meridian-bench**: pure throughput and latency benchmark. Zero networking. The 6M+ events/sec number comes from this binary so it is defensible.
* **meridian-replay**: CLI that replays an ITCH tape and writes JSON Lines to stdout. Useful for piping, reproducible audit trails, and integration tests.
* **meridian-server**: the live demo. Runs a replayer plus the matching loop plus the sampler plus uWebSockets. Serves the WebSocket endpoint and the Cloudflare-hosted React app.

## 5. Components

### 5.1 libmeridian.a

**Order**
```
struct Order {
    OrderId id;
    Symbol symbol;
    Side side;            // Buy | Sell
    OrderType type;       // Limit | Market | IOC | PostOnly | FOK
    Price price;          // 32-bit fixed point, ticks
    Qty qty_remaining;
    Timestamp ts_arrival;
    Order* prev;          // intrusive list at level
    Order* next;
};
```
64 bytes, fits one cache line.

**Level**
Doubly linked list of Orders at a single price. Owns total qty and order count. Insertion at tail (FIFO).

**Book**
One side (bid or ask) of one symbol's order book. Sorted map of Levels by price (red-black tree initially via std::map; consider a flat skip list later). Plus an `unordered_map<OrderId, Order*>` for O(1) cancel.

**OrderPool**
Free-list allocator with a fixed pre-allocated arena. The matching loop never calls operator new. Verified by an allocator override in debug builds that aborts on any heap allocation during the hot path.

**MatchingEngine**
Single-writer state machine. Consumes `EngineEvent` (NewOrder, Cancel), produces `ExecutionReport` stream (Fill, Acknowledge, Reject, Cancel). Implements price-time priority matching. Modify (cancel-and-replace) is out of scope for v1 and is listed in Section 12 as a future direction.

**BookRegistry**
`unordered_map<Symbol, Book>` for multi-instrument. Symbols pre-allocated at startup from the ITCH directory message.

**TopOfBookSnapshot**
Seqlock-protected struct: `{best_bid_px, best_bid_qty, best_ask_px, best_ask_qty, last_seq}`. Writer (matching thread) bumps seq before and after each update. Readers retry on torn read.

### 5.2 meridian-bench

* `--mode synthetic`: generates Poisson-distributed events with configurable order arrival rate and cancel ratio
* `--mode itch --tape <file>`: replays an ITCH 5.0 file as fast as the engine can consume it
* Reports: events/sec (overall and steady-state), latency histogram with HDRHistogram, p50/p90/p99/p99.9/max, max heap allocations during hot path (must be 0)
* Output: human-readable Markdown table to stdout, JSON sidecar for CI regression checks

### 5.3 meridian-replay

* `meridian-replay --tape itch-2024-06-13.bin --speed 1.0 --symbols AAPL,SPY,NVDA,TSLA,GOOG`
* Streams `ExecutionReport`s as JSON Lines on stdout
* Optional `--audit out.jsonl` writes a deterministic audit log for diffing against a reference implementation

### 5.4 meridian-server

* Reads `meridian.toml` for: tape path, replay speed, listen port, allowed origins, max WebSocket clients
* Starts replayer + matching loop on core 4
* Starts sampler on core 6 (30 Hz)
* Starts uWebSockets event loop on core 5
* Serves:
  * `WSS /ws`: top-of-book delta stream (the live demo's only first-class endpoint)
  * `GET /healthz`: returns 200 with a one-line JSON status; Fly's load balancer hits this for readiness checks
  * `GET /metrics`: returns the JSON counters defined by the Observability Engineer (events ingested, trades emitted, clients connected, messages broadcast)
* Does NOT serve the React SPA. Cloudflare Pages owns the SPA at `meridian-demo.pages.dev`; the resolved decision in section 13 puts the frontend on Cloudflare Pages, not on the Fly machine. The earlier draft of this section that mentioned `GET /` returning a static React app is superseded.
* Graceful shutdown on SIGTERM

### 5.5 Web client

React 19 SPA. WebSocket client subscribes to a symbol topic on connect; receives:

* `snapshot`: full book L10 + recent trades + perf metrics (on connect)
* `delta`: per-tick top-of-book changes, new trades, perf updates (30 Hz)

Connection states (Fly cold start: see section 13):

* `connecting`: initial state. The client renders an explicit "Engine warming up" hero with a determinate progress dot and a copy line that names the cold start as expected (rather than appearing broken). Resolves when the first `snapshot` arrives.
* `live`: snapshot received, deltas streaming. Default rendered state.
* `stalled`: no delta in 1 second. Header dot turns amber; the rest of the UI continues showing the last good state.
* `disconnected`: socket closed. Reconnection uses exponential backoff (500 ms initial, doubling, cap 8 s). Returns to `connecting` while the next attempt is in flight.

Symbol switching: the Header exposes a five-symbol selector (AAPL, SPY, NVDA, TSLA, GOOG) wired to a Zustand `selectedSymbol` slice; on change, the client unsubscribes from the previous topic, subscribes to the new one, and re-enters `connecting` until the new `snapshot` arrives.

Rendered panels (Twilight visual):

1. Header strip with brand mark and live-status dot
2. Hero with symbol, last price, change, spread, volume
3. Order book ladder (L8, depth bars, FIFO tooltips on hover)
4. Cumulative depth chart (SVG, gradient-filled bid/ask areas, dashed gold mid line)
5. Recent trade tape (last 12 prints with notional, aggressor, price-up/down coloring)
6. Performance column (throughput headline, latency histogram, p50/p99/p99.9 grid, replay progress bar)
7. Footer with build info, core pinning, seqlock reader count

State management: Zustand (small, ergonomic). No Redux.

Charting: hand-rolled SVG, no D3 dependency. Data is small (≤20 levels per side, ≤50 recent trades) so a chart library is overkill.

## 6. Data flow

1. **Ingest**: Replayer decodes ITCH tape into `EngineEvent` structs and pushes them onto a single-producer queue
2. **Match**: Matching loop pops events on core 4, calls `Book::apply(event)`, generates `ExecutionReport`s
3. **Publish**: After each event, matching loop atomically updates the symbol's `TopOfBookSnapshot` (seqlock writer protocol)
4. **Sample**: Sampler thread on core 6 wakes at 30 Hz, reads all snapshots (seqlock reader protocol), builds a JSON delta
5. **Push**: Sampler hands the delta to the uWebSockets event loop on core 5; broadcast to all connected clients
6. **Render**: Front end applies the delta, re-renders affected panels via React state

The matching loop never touches a socket, never serializes JSON, never holds a mutex. Its only interaction with the outside world is updating the seqlock-protected snapshot and pushing execution reports onto a separate ring buffer that the audit consumer drains.

## 7. Error handling

| Layer | Strategy |
|---|---|
| Hot path (matching loop) | No exceptions. Return codes only. `[[likely]]`/`[[unlikely]]` annotations. Debug-only assertions. |
| Setup path (config, ITCH file open, port bind) | Exceptions allowed. Fatal errors abort startup with a clear message. |
| ITCH replay | Malformed messages logged via spdlog (level filtered out of release hot path) and skipped. Replay continues. |
| WebSocket | Dropped clients silently culled by uWebSockets. Reconnect handled by the front end. |
| Front end | WebSocket errors trigger an exponential-backoff reconnect with a visible "reconnecting" badge. |

## 8. Testing strategy

### 8.1 Unit tests (GoogleTest)
* `Order` construction, comparison
* `Level` insert, remove, total qty conservation
* `Book` price-time priority correctness on adversarial sequences
* `OrderPool` exhaustion behavior
* `BookRegistry` lookup and dispatch
* `MatchingEngine` for each order type
* Seqlock writer/reader correctness under concurrent load (TSAN)

Target: ≥40 unit tests, ≥85% line coverage on `libmeridian.a`.

### 8.2 Property-based tests (rapidcheck)
1. **Price-time priority**: for any sequence of events, the order in which orders fill at a given price level is the order in which they were inserted
2. **Quantity conservation**: total qty filled + total qty remaining + total qty cancelled = total qty submitted
3. **No double fill**: no order's filled qty exceeds its submitted qty
4. **No lost orders**: every submitted order is in exactly one of {filled, partially-filled-resting, fully-resting, cancelled}
5. **Spread non-negative**: best ask ≥ best bid at all times (between events)
6. **Cancel idempotence**: cancelling an already-cancelled order is a no-op (returns Reject reason=NotFound)
7. **IOC never rests**: an IOC order that doesn't fully fill leaves no resting quantity
8. **FOK is all-or-nothing**: a FOK order either fully fills or cancels with no fills
9. **Post-only never crosses**: a post-only order that would cross is rejected before any matching
10. **Top-of-book monotonicity within a tick**: between event N and event N+1, top-of-book changes are atomic from the reader's perspective (no torn reads)

Each property runs ≥1000 generated cases per CI run.

### 8.3 Integration tests (Catch2 BDD style)
* Replay a 5-minute ITCH 5.0 sample, compare final book state against a reference Python implementation
* Run meridian-server, connect a WebSocket client, verify snapshot + delta protocol over a 60-second replay

### 8.4 Benchmark regression
* meridian-bench runs in CI on every PR
* Compares against the baseline JSON checked into the repo at `bench/baseline.json`
* Fails the build if throughput drops more than 5% relative to baseline, or if any of p50, p99, p99.9 latency percentiles increase more than 10% relative to baseline
* Baseline is updated only by an explicit "regenerate baseline" workflow on main, never silently

## 9. Phase plan (10 to 14 weeks)

| Phase | Weeks | Deliverable |
|---|---|---|
| 0. Bootstrap | 1 | Repo, CMake, CI, OrderPool, Order, Level types, build green |
| 1. Single-symbol matching | 2 | Limit + market + IOC matching, FIFO, cancel-by-id, unit tests |
| 2. Multi-instrument | 1 | BookRegistry, symbol dispatch, integration tests |
| 3. Property tests | 1 | All 10 invariants, rapidcheck wired up |
| 4. Seqlock + sampler | 1 | TopOfBookSnapshot, sampler, TSAN clean |
| 5. Benchmark hits 6M | 1 | meridian-bench, PGO, target throughput, latency report |
| 6. ITCH 5.0 replay | 2 | Parser, replayer, integration test against reference |
| 7. Post-only + FOK | 0.5 | Two more order types, property tests updated |
| 8. WebSocket server | 0.5 | uWebSockets wired, meridian-server binary, smoke test |
| 9. Web front end | 2 | React + Vite + TS + Tailwind app, Twilight visual, all panels |
| 10. Hosting + benchmark report | 1 | Cloudflare Pages, Fly.io machine (Dockerfile + `fly.toml`), CI/CD, benchmark report PDF |
| 11. Polish + README | 1 | Docs, README, demo URL, final test pass |

Total: 14 weeks at the leisurely end, 10 weeks if Phases 6 and 9 go fast.

## 10. Repo layout

```
Meridian/
├── CMakeLists.txt
├── README.md                       # Headline + benchmark + design link
├── LICENSE                         # MIT
├── .clang-format
├── .clang-tidy
├── .github/workflows/ci.yml
├── docs/
│   ├── superpowers/specs/2026-05-09-meridian-design.md  # this file
│   ├── architecture.md             # Detailed engine internals
│   ├── benchmark-report.md         # Generated by meridian-bench, with histograms
│   └── itch-protocol-notes.md      # ITCH 5.0 reference for the parser
├── design-mockups/                 # The 5 visual direction mockups (kept for design archaeology)
│   └── themes/{bourse,twilight,terminal,eikon,onyx}.html
├── include/meridian/               # Public library headers
│   ├── order.hpp
│   ├── level.hpp
│   ├── book.hpp
│   ├── matching_engine.hpp
│   └── snapshot.hpp
├── src/                            # Library implementation
│   ├── book.cpp
│   ├── matching_engine.cpp
│   └── ...
├── apps/
│   ├── bench/main.cpp
│   ├── replay/main.cpp
│   └── server/{main.cpp, sampler.cpp, ws.cpp}
├── tests/
│   ├── unit/...
│   ├── property/...
│   └── integration/...
├── third_party/                    # Vendored dependencies (CMake FetchContent)
└── web/                            # React + Vite app
    ├── package.json
    ├── vite.config.ts
    ├── tailwind.config.ts
    ├── src/
    │   ├── App.tsx
    │   ├── components/{Ladder,DepthChart,Tape,Perf,Header}.tsx
    │   ├── store/useBookStore.ts   # Zustand
    │   └── ws/client.ts
    └── public/
```

## 11. Risk register

| Risk | Likelihood | Mitigation |
|---|---|---|
| 6M evt/s target not hit on user's hardware | Medium | Acceptance criterion is "matches the headline within 20% on equivalent hardware"; document the test machine in the report. PGO + LTO before declaring failure. |
| ITCH parser eats the schedule | Medium | Cap at 2 weeks; fall back to LOBSTER CSV if ITCH binary parsing stalls. LOBSTER is text and trivial to parse. |
| uWebSockets has bugs at our use case | Low | Fallback to Boost.Beast or a hand-rolled WebSocket. Beast adds dependency weight but is well-tested. |
| Fly.io free tier exceeded | Low | Free allowance is generous for a single small machine running an ITCH replay. If exceeded, a paid Fly machine at roughly $5 per month is acceptable. |
| Cold start hurts demo UX | Medium | Frontend renders an explicit "engine warming up" state during the 5 to 15 second wake window (section 5.5). Acceptable for a portfolio demo. |
| Twilight visual feels too editorial for HFT | Low | Mockup already approved by user; Vega contrast is the explicit goal. |
| Project drags past 14 weeks | High | Defer Phase 11 polish first if needed; demo can ship without a benchmark PDF. |

## 12. Out-of-scope future directions

These are explicitly NOT in v1 but are valuable to mention in the README for "where this could go":

* Process-per-symbol with shared-memory ring buffer (the rejected Architecture Option 3)
* Persistence and replay via append-only journal (event sourcing)
* Strategy gateway: a simple pluggable interface so a strategy can submit orders in-process and measure round-trip latency
* Multi-venue: model two venues running simultaneously and a SOR (smart order router) between them
* Snapshot-and-replay disaster recovery
* C++ to Rust port comparison writeup

## 13. Resolved decisions (2026-05-09)

The four open questions originally listed here were resolved on 2026-05-09 during Phase 0 brainstorming. Sections 2.3, 3, 5.3, 5.5, 9, and 11 above have been updated to reflect these decisions.

* **Hosting domain**: `meridian-demo.pages.dev` (Cloudflare Pages default subdomain). No custom domain for v1.
* **Backend host**: Fly.io free tier. Machines auto-stop when idle and auto-start on first request. Region picked at Phase 10 from Fly's currently available free-tier regions, defaulting to `iad` (Ashburn, Virginia) if available. The Phase 10 deploy stack is therefore Dockerfile plus `fly.toml`, not VPS plus systemd. The Fly app name is also picked at Phase 10 (likely `meridian` or `meridian-engine`); the resulting WebSocket origin `wss://<app>.fly.dev` flows into the frontend's CSP `connect-src`.
* **Demo symbol set**: AAPL, SPY, NVDA, TSLA, GOOG. Phase 6 ITCH replay extracts these five symbols from a NASDAQ ITCH 5.0 sample file; the Phase 9 frontend ships a symbol switcher in the Header.
* **Audit log on demo**: WebSocket-only. The live `meridian-server` keeps audit events in an in-memory ring buffer and streams them to connected clients; events are not persisted to disk on the Fly machine. Crash means recent events are lost, which is acceptable because the ITCH replay is deterministic and reproducible. The `meridian-replay --audit out.jsonl` CLI flag in section 5.3 is unaffected; that flag is for offline diffing against the Python reference and runs on the developer's machine, not on the demo.

### Cold-start UX

Fly.io's free machines sleep when idle. The first WebSocket connection after an idle period takes roughly 5 to 15 seconds while Fly wakes the machine. The Phase 9 frontend must render an explicit "engine warming up" state during this window rather than appearing broken. The connection state machine in section 5.5 covers this requirement.
