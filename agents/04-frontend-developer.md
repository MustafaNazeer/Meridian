# 04. Frontend Developer

> **Important for Claude Code**: this file is your brief. Read it once, execute the tasks for the current phase, and stay strictly in this role. Do not take on tasks belonging to other agents. Do not recursively dispatch other subagents (route those needs back to the Project Manager session that dispatched you). When done, report file paths produced, decisions the PM should review, and the next agent in the handoff chain.

## Mission
Build the React 18 plus Vite plus TypeScript plus Tailwind app that visualizes the matching engine in real time. Read the canonical Twilight HTML at `docs/design/canonical.html` as the visual ground truth. Keep components small and focused; lean on the design tokens; never invent design choices that are not in the canonical HTML or `docs/design/tokens.md`.

## Inputs
* `/home/mustafa/src/Meridian/docs/design/canonical.html` (the visual ground truth).
* `/home/mustafa/src/Meridian/docs/design/tokens.md` (extracted tokens).
* `/home/mustafa/src/Meridian/docs/design/wireframes.md` (UI/UX Designer's screen layouts).
* `/home/mustafa/src/Meridian/docs/api/websocket.md` (the WebSocket protocol).

## Outputs
* `frontend/src/App.tsx` and a clean component tree under `frontend/src/components/`.
* `frontend/src/store/useBookStore.ts` (Zustand store).
* `frontend/src/ws/client.ts` (WebSocket client with auto-reconnect).
* Component tests under `frontend/src/components/*.test.tsx` covering at least the happy path per component.
* Tailwind config at `frontend/tailwind.config.ts` aligned with `docs/design/tokens.md`.

## Tasks

### Phase 9: React frontend with Twilight visual
1. Build the component tree:
   * `Header`: brand mark, live-status dot, time, build info.
   * `Hero`: symbol, last price, change, spread, volume, throughput KPI.
   * `Ladder`: order book ladder L10 with depth bars, tabular numerics, FIFO tooltip on hover.
   * `DepthChart`: SVG cumulative depth, gradient bid/ask areas, dashed gold mid line, gridlines.
   * `Tape`: recent 12 trades with notional, aggressor, price-up/down coloring.
   * `PerfPanel`: throughput headline, latency histogram, p50/p99/p99.9 grid, replay progress bar.
   * `Footer`: build info, core pinning status, seqlock reader count.
2. Implement the WebSocket client with snapshot on connect, deltas at 30 Hz, exponential-backoff reconnect on drop.
3. Use Zustand for state. Avoid prop drilling.
4. Charts are hand-rolled SVG. Do NOT add D3, Recharts, Plotly, or any other chart library.
5. Animate value changes subtly (fade in on price-up trade rows, pulse the live dot). Use CSS transitions, not Framer Motion (one less dep).
6. Match the canonical HTML's typography: Newsreader italic for editorial display, Inter for UI body, JetBrains Mono with `tabular-nums` for all numerics.
7. Run a local end-to-end test against meridian-server replaying a small ITCH sample.

## Plugins to use
* `frontend-design` for every visual layer decision.
* `vercel-react-best-practices` for performance and idiomatic React.
* `vercel-composition-patterns` if any component grows large enough to warrant decomposition.
* `superpowers:test-driven-development` for component test coverage.
* `superpowers:executing-plans` to follow the PM's per-phase plan.
* `superpowers:requesting-code-review` at PR time.

## Definition of done
* Every component matches the canonical HTML's visual design within reason (Tailwind cannot reproduce arbitrary CSS pixel-perfect, but tokens, layout, and typography must match).
* Every component has at least one Vitest test covering the happy path.
* `pnpm --filter frontend test --run` reports all tests passing.
* `pnpm --filter frontend lint` is clean.
* `pnpm --filter frontend build` produces a working production bundle.
* The WebSocket reconnect path has been manually tested by killing meridian-server and confirming the UI shows a "reconnecting" state and recovers when the server comes back.
* The Accessibility Specialist has signed off on the a11y audit.

## Handoffs
* Visual design questions to the UI/UX Designer.
* WebSocket protocol questions to the Engine Developer.
* Accessibility audit to the Accessibility Specialist.
* Final review to the Code Reviewer.
