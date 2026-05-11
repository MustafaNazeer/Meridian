# Meridian frontend

The Twilight dashboard for `meridian-server`. React 19 + Vite + TypeScript + Tailwind, with Zustand for the connection / book state and a hand rolled `poll()` style WebSocket client. The visual ground truth is [`/docs/design/canonical.html`](../docs/design/canonical.html); the layout contract is [`/docs/design/wireframes.md`](../docs/design/wireframes.md); the token contract is [`/docs/design/tokens.md`](../docs/design/tokens.md).

## Quick start

```bash
pnpm install
pnpm dev
```

The dev server defaults to `http://localhost:5173`. The WebSocket URL is read from `import.meta.env.VITE_MERIDIAN_WS` and falls back to `ws://localhost:8080/ws`; run `meridian-server --port 8080` next to `pnpm dev` to feed it the live engine stream.

## Scripts

| Script | Purpose |
|---|---|
| `pnpm dev` | Vite dev server with HMR |
| `pnpm test` | Vitest unit suite (state machine + store reducers) |
| `pnpm test:watch` | Vitest in watch mode |
| `pnpm run build` | `tsc -b` plus `vite build` into `dist/` |
| `pnpm run preview` | Serve the production bundle locally |
| `pnpm run lint` | ESLint |

## Layout

```
src/
  App.tsx                  page shell + three column grid
  main.tsx                 React 19 root
  types.ts                 wire message + connection state types
  index.css                Tailwind directives + page globals
  lib/
    format.ts              numeric and time formatters
    symbols.ts              static per-symbol metadata
    build.ts               build-time env vars (VITE_*)
  store/
    dashboard.ts           Zustand store
    dashboard.test.ts
  ws/
    client.ts              connection state machine + reconnect
    client.test.ts
  hooks/
    useMeridianStream.ts   mounts the client and bridges to the store
  components/
    SectionHead.tsx
    ConnectionBanner.tsx
    Header.tsx
    Hero.tsx
    Ladder.tsx
    DepthChart.tsx
    Tape.tsx
    PerfPanel.tsx
    Footer.tsx
scripts/
  smoke.mjs                end-to-end smoke against a live server
```

## Build-time env vars

| Var | Purpose | Default |
|---|---|---|
| `VITE_MERIDIAN_WS` | WebSocket URL the client connects to. | `ws://localhost:8080/ws` |
| `VITE_MERIDIAN_VERSION` | Footer build version. | `0.1.0-dev` |
| `VITE_MERIDIAN_COMPILER` | Footer compiler line. | `clang 19.1.7` |
| `VITE_MERIDIAN_FLAGS` | Footer compile flags line. | `-O3 -march=native -DNDEBUG` |

Set these via `.env.production`, the Cloudflare Pages dashboard, or the deploy workflow.

## Connection state machine

Four states with explicit visual surfaces:

* `connecting` (initial): the hero replaces the symbol with a "warming" headline and a row of pulsing gold dots. Header dot pulses gold.
* `live`: the canonical layout with real top of book values flowing through the store. Header dot pulses gold.
* `stalled`: no message in 1 second. Header dot recolors to gold-deep, header meta gains a "stalled · {n}s ago" tail, numerics faded slightly. The next message flips back to `live`.
* `disconnected`: socket closed or errored. `ConnectionBanner` mounts above the main grid in muted coral. Client retries with exponential backoff (500 ms initial, doubling, cap 8 s).

## Current data coverage

The wire payload today is top of book only (`bid_px`, `bid_qty`, `ask_px`, `ask_qty`, `ts`). The Header, Hero, the row of the Ladder adjacent to the spread, the bid and ask blocks of the DepthChart, the throughput block of the PerfPanel, and the Footer all read from real data. The other ladder rows, the recent trades tape, and the engine latency histogram render the canonical anatomy and explicitly flag in their section aux labels that they fill in alongside the extended snapshot follow-up milestone.
