# Meridian wireframes (Twilight)

This file describes each top-level component in the Meridian dashboard in plain prose, one section per component. The Frontend Developer reads this as the layout contract for Phase 9. The visual ground truth is `/home/mustafa/src/Meridian/docs/design/canonical.html`; the token contract is `/home/mustafa/src/Meridian/docs/design/tokens.md`. Wireframes here describe layout, anatomy, responsive behavior, and micro-interactions; they do not duplicate token values.

Every visual region in the canonical HTML is annotated with a `data-component` attribute (or will be once the React port is built) and each meaningful sub element is annotated with `data-element`. These attributes are the search keys the Frontend Developer, the QA Engineer, the Accessibility Specialist, and any future Flow A design change rely on. Always preserve them in the React port; they double as test selectors and as Storybook labels.

## Page shell

* **`data-component="App"`** is the outermost container. Root layout is a vertical flex column with no horizontal scroll. Background is the body radial gradient (token: `bg-body-radial`) anchored at the top, fixed during scroll.
* The shell renders, top to bottom: `Header`, `Hero`, `Main` (the three-column grid), `Footer`. The shell never sets a fixed height; the viewport scrolls naturally if content exceeds 100vh.
* **`data-component="ConnectionBanner"`** is a conditional sibling that mounts directly above `Main` only in the `disconnected` connection state (per design spec section 5.5). It is 32 px tall, full-width, with `bg-ask/16` background and `text-ask` copy. When the state returns to `connecting` the banner unmounts; in `stalled` no banner shows (the dot tells that story). When mounted, `Main` does not relayout; the banner is layered above the natural document flow with no margin shift.
* The shell hosts no scrollbars of its own; individual panels (Tape, Ladder) rely on container queries set in their own sections rather than overflow on the page.
* Minimum supported viewport is 1280 px wide. Below that, the layout is allowed to clip; mobile is a future-idea (`/home/mustafa/src/Meridian/future-ideas.md`).
* The shell mounts a Zustand-backed connection state machine and a single WebSocket client (`frontend/src/ws/client.ts`); per design spec section 5.5 the four states are `connecting`, `live`, `stalled`, `disconnected`.

## 1. Header

* **`data-component="Header"`**.
* Layout intent: a single horizontal flex row, baseline-aligned at the bottom edge, full-bleed across the page width with 48 px horizontal padding and 36 px top, 24 px bottom padding. A 1 px bottom border in `rule` color separates it from the Hero.
* Children, left to right:
  * **`data-element="Brand"`** is a flex-baseline pair: a brand mark and a brand tagline.
    * **`data-element="BrandMark"`** renders the literal text `meridian.` where the period is a separately wrapped span that recolors to gold and resets to non-italic. Newsreader italic medium 42 px, `tracking-tight-mark`. The dot is the only place the gold accent appears in the entire header beyond the live status pulse.
    * **`data-element="BrandTag"`** is a two-line italic tagline ("a price-time priority limit order book, / built for the line where bid meets ask"). Newsreader italic 14 px in `ink-soft` with a 1 px left border in `rule` and 18 px left padding. The line break inside the tag is intentional and should be controlled with a `<br>` (or a CSS `display: block` line) rather than relying on natural wrapping; the second line lives at 1.4 line height.
  * **`data-element="HeaderMeta"`** is a right-aligned two-line block. First line is the live timestamp ("HH:MM:SS.mmm ET · matching live") with the live dot prefix. Second line is "YYYY · MM · DD · NASDAQ ITCH 5.0 · core 4 pinned" in `ink-soft` with the date span in `ink` numerals.
    * **`data-element="LiveDot"`** is a 6 px gold pulse (token: `animate-pulse-live` plus `glow-gold`). Per the connection state machine, the dot's color and animation change between states (see tokens.md section 11). The dot reads time and exchange status; the surrounding copy reads stable.
    * **`data-element="HeaderTime"`** updates from the WebSocket clock, not `Date.now()`, so the displayed time matches the engine's last-event timestamp. In `stalled` and `disconnected` the time freezes at the last value and the meta line gains a "stalled · 4s ago" or "disconnected · reconnecting" tail in `ink-soft` or `ask` respectively.
  * The Header also hosts the **`data-element="SymbolSelector"`** (introduced by design spec section 5.5). It is a small inline group of five chips (AAPL, SPY, NVDA, TSLA, GOOG) rendered between `Brand` and `HeaderMeta` on a wide enough viewport, or below `BrandTag` on the 1280 px minimum. Each chip is Inter medium 11 px uppercase tracked 0.12em, 6 px by 12 px padding, 1 px border in `rule`, no fill. The active chip's border swaps to `gold` and its text recolors to `gold`. Hover (non-active) swaps the border to `rule-strong`. Keyboard focus draws a 2 px `gold` outline offset 2 px outside the chip.
* Responsive: not responsive in v1. On the 1280 px minimum the Header lays out cleanly because `Brand` is allowed to shrink its tagline; below that the Header may overflow.
* Micro-interactions:
  * The live dot pulses on `pulse 1.6s infinite` keyframes (token: `animate-pulse-live`). In `stalled` the pulse stops and the dot recolors per tokens.md section 11. In `disconnected` the dot recolors to `ask` and freezes.
  * The brand mark dot is decorative; do not animate it. (Resist the temptation; it would compete with the live dot.)
  * Symbol chips have no hover transition by default; on hover the border color swaps instantly. Click triggers the Zustand `selectedSymbol` setter, which closes the current WebSocket subscription and opens a new one; the connection state immediately re-enters `connecting` until the new `snapshot` arrives.

## 2. Hero

* **`data-component="Hero"`**.
* Layout intent: a four-column CSS grid (`grid-hero`: `2fr 1fr 1fr 1fr`) with 48 px gap, 56 px top padding, 48 px horizontal padding, 44 px bottom padding, items aligned to the bottom edge. A 1 px bottom border in `rule` separates Hero from Main.
* Children, left to right:
  * **`data-element="HeroIdentity"`** is the lede block. Three stacked text rows:
    * **`data-element="HeroLede"`**: Newsreader italic 13 px in `ink-soft` tracked 0.02em, with a 12 px bottom margin. Reads "tracking · NMS Tier 1 · {symbol full description} · {market}". The string is symbol-driven and read from a static map keyed by symbol.
    * **`data-element="HeroSymbol"`**: Newsreader italic medium 72 px in `ink`, `tracking-tight-display`, line height 1. This is the single largest piece of type on the page. In the `connecting` state the symbol is replaced with the literal "warming" word at the same size.
    * **`data-element="HeroMeta"`**: Inter 12 px in `ink-soft`, 12 px top margin. Reads "{liveOrderCount} live orders · {activePriceLevels} active price levels · {tradesSinceOpen} prints since open". All three numerals are JetBrains Mono via the `.num` utility, inline within the otherwise sans-serif copy.
  * **`data-element="StatLast"`** (Last price stat block):
    * 1 px left border in `rule`, 22 px left padding (matches all stat blocks).
    * **`data-element="StatLabel"`**: Newsreader italic 11 px in `ink-soft` tracked 0.04em, 8 px bottom margin, reads "Last".
    * **`data-element="StatValue"`**: JetBrains Mono 32 px light in `ink`, line height 1, `tracking-tight-display`. Recolors to `bid` when the latest tick was up and `ask` when down. The colored class is applied only when the previous tick is known; on first paint after `connecting` the color is `ink`.
    * **`data-element="StatSub"`**: Inter 12 px in `ink-soft`, 6 px above. Two spans: signed dollar change and signed percentage change. Up renders in `bid`, down in `ask`. Spans use the `.num` utility.
  * **`data-element="StatSpread"`** (Spread stat block): same anatomy as `StatLast`, but the value is always rendered in `gold` (token: `stat-value gold`). Sub line reads "{bps} bp · {tight|normal|wide}".
  * **`data-element="StatVolume"`** (Volume stat block): same anatomy. Value is the day's volume in millions, with the literal "M" suffix as a small `ink-soft` span at 60 percent of the value font size. Sub line reads "vs 30d avg {avg}M".
* Responsive: not responsive in v1. At 1280 px the Hero remains a four-column grid; the lede column does most of the shrinking.
* Micro-interactions:
  * The stat values flash a 600 ms gold tint when their underlying datum changes (Phase 9 polish, see tokens.md `duration-tape-flash`). The flash is a single-shot `animation`, not a `transition`, so rapid updates do not compound. Confirm the flash with the UI/UX Designer before enabling in Phase 9.
  * The numerics use `font-variant-numeric: tabular-nums` so digits never jitter horizontally as values change.
  * In `connecting`, all three stat blocks render skeleton placeholders: a 32 px tall, 80 px wide block in `surface` with the same `radius-bar`. No shimmer animation; the skeleton is static.

## 3. Ladder (order book)

* **`data-component="Ladder"`** lives in the left column of `Main` (`grid-main` first track, 380 px wide).
* Layout intent: a single `<table>` with `border-collapse: collapse`, full-width, font is JetBrains Mono 13 px tabular-nums. The table is preceded by a `data-component="SectionHead"` strip (section title "the book", section aux "L8 · 16 levels"). Column padding is 36 px vertical, 32 px horizontal (per `.col` rule).
* Sub elements, top to bottom:
  * **`data-element="LadderHead"`** is a `<thead>` row with four cells: `price` (left), `size` (right), `orders` (right), `cum` (right). All four use Inter 10 px medium uppercase in `ink-faint` tracked 0.12em, with 6 px vertical padding.
  * **`data-element="LadderAsks"`** is a `<tbody>` block of 8 rows representing levels above the spread. Highest ask renders at the top; rows are listed from highest to lowest price. Each row has `data-element="LadderRow"` and `data-side="ask"`.
  * **`data-element="SpreadRow"`** is a single full-width row that visually separates asks from bids. Vertical padding 14 px, italic Newsreader copy in `ink-soft`. Numerics inside the row reset to JetBrains Mono medium in `gold` with 8 px horizontal margin. Background is the spread-band gradient. Top and bottom borders in semi-transparent gold (token recipe in tokens.md section 1.4).
  * **`data-element="LadderBids"`** is a `<tbody>` of 8 rows for levels below the spread, highest bid first. Each row has `data-side="bid"`.
* Each `LadderRow` cell anatomy (left to right):
  * **`data-element="RowPrice"`**: numeral colored `bid` or `ask` depending on side. Sits on top of the depth bar via `position: relative; z-index: 1`.
  * **`data-element="RowSize"`**: numeral in `ink-2`, right-aligned. Behind it sits the `data-element="DepthBar"`, an absolutely positioned 2 px-inset bar anchored to the right edge whose width is set inline (`style="width: NN%"`) and whose fill is `bid-soft` or `ask-soft` per row side.
  * **`data-element="RowOrders"`**: numeral in `ink-2`, right-aligned, reports the count of resting orders at this level (FIFO queue length).
  * **`data-element="RowCum"`**: numeral in `ink-2`, right-aligned, reports the cumulative size from this level outward to the top of book.
* Responsive: 380 px column is fixed in v1. Ladder cells truncate gracefully via tabular-nums plus right-alignment; numerals never wrap.
* Micro-interactions:
  * On row hover, the row gains a faint `surface` underlay; this is a Phase 9 addition and the trigger should be CSS-only (`tr:hover td`). No tooltip on first cut; the FIFO tooltip listed in the design spec is deferred to Phase 9 polish and should land only if the Frontend Developer can ensure it does not conflict with screen reader announcements.
  * On a level update from a delta, the affected row's `RowSize` numeral flashes the gold tint (token `duration-tape-flash`) for 600 ms. New levels appearing or disappearing are not animated in v1; the row simply mounts or unmounts. Animating book churn would be visual noise.
  * The depth bar widths recompute on every delta. Their inline-styled widths are bound to the level's percentage of the heaviest level on its side; transitions on width are deliberately not used (movement would mislead a reader).
  * Keyboard focus inside the table is rare in v1 (no row selection), so the table does not need `tabindex` on rows; the column headings are read by screen readers in normal table flow.

## 4. DepthChart (cumulative depth)

* **`data-component="DepthChart"`** lives in the center column of `Main` (`grid-main` middle track, fluid 1fr), above the Tape. It is preceded by a `data-component="SectionHead"` strip (section title "cumulative depth", section aux "10 levels each side"). The whole component sits inside the same `.col` as the Tape; the section head separates the two visually.
* Layout intent: a vertical stack of three children: SVG chart, axis row, legend row. Bottom margin 36 px so the Tape's section head sits clear.
* Sub elements:
  * **`data-element="DepthSVG"`** is an inline SVG with `viewBox="0 0 600 240"`, `preserveAspectRatio="none"`, `width="100%"`, `height="240"`. Two filled paths: bid (left of mid, climbing left, gradient fill `bidGrad`, stroke `bid` 1.5 px) and ask (right of mid, climbing right, gradient fill `askGrad`, stroke `ask` 1.5 px). Four horizontal grid lines at y=48, 96, 144, 192 in `rule` 0.5 px stroke. One vertical mid line at x=300 in `gold` 0.75 px dashed (4 4) at 0.7 opacity. A small Newsreader italic 12 px gold label "mid {price}" sits at x=306, y=16. Path coordinates derive from the live ladder's cumulative bid and ask sizes; the projection is linear with mid pinned at x=300.
  * **`data-element="ChartAxis"`** is a flex row, justify-between, JetBrains Mono 10 px in `ink-faint`, 8 px top margin. Five labels: best-bid edge, mid-3, "mid", mid+3, best-ask edge. Tick values are derived from the symbol's tick size and the current mid.
  * **`data-element="ChartLegend"`** is a flex row, 24 px gap, items center-aligned, 14 px top margin, 12 px copy in `ink-soft`. Two legend entries (`data-element="LegendBid"`, `data-element="LegendAsk"`) each with a 12 px by 2 px colored dot (with a 0 0 8 px glow in the matching color) and a label "Bid · cum {qty}" / "Ask · cum {qty}". A third entry `data-element="Imbalance"` is auto-margin-left aligned; copy reads "imbalance · {signed%} {selling pressure | buying pressure}". The strong span recolors to `bid` for buying pressure and `ask` for selling pressure.
* Responsive: SVG `preserveAspectRatio="none"` means the chart stretches to fill its container while keeping the mid line centered. The 240 px height is fixed in v1; do not let the chart squash below that.
* Micro-interactions:
  * On every delta, the bid and ask paths re-render with new `d` attribute values; do not animate the path interpolation in v1 (proper geometric interpolation is non-trivial and stutters cheaply look worse than an instant snap).
  * The mid line label recomputes when mid changes by at least 0.5 ticks; stable updates avoid label jitter.
  * Hover behavior: deferred to Phase 9 polish. If implemented, a vertical hover guide draws a 1 px `ink-soft` line at the cursor x and renders a small floating tooltip in `surface-overlay` with cumulative depth at that price. Confirm with the UI/UX Designer and Accessibility Specialist before adding; the hover affordance must have a keyboard-accessible alternative.

## 5. Tape (recent trades)

* **`data-component="Tape"`** lives in the same `.col` as the DepthChart, below it. Preceded by a `data-component="SectionHead"` strip (section title "the tape", section aux "last 12 prints").
* Layout intent: a single `<table>` with `border-collapse: collapse`, full-width, JetBrains Mono 12 px tabular-nums.
* Sub elements:
  * **`data-element="TapeHead"`** is a `<thead>` row with six columns: time, side, price (right), size (right), notional (right), aggressor (right). Inter 10 px medium uppercase in `ink-faint` tracked 0.12em, 6 px top padding, 8 px bottom padding, 1 px bottom border in `rule`.
  * **`data-element="TapeBody"`** is a `<tbody>` of 12 rows, newest at the top. Each row has `data-element="TapeRow"` and `data-side="buy"|"sell"`. Cells:
    * **`data-element="TapeTime"`**: JetBrains Mono in `ink-2`. Renders HH:MM:SS.mmm.
    * **`data-element="TapeSide"`**: Inter medium 12 px in `bid` for BUY and `ask` for SELL. Literally "BUY" or "SELL".
    * **`data-element="TapePrice"`**: JetBrains Mono. Recolors to `bid` if the trade printed at-or-above the prior trade's price (`price-up`) and to `ask` if below (`price-dn`). Right-aligned.
    * **`data-element="TapeSize"`**: JetBrains Mono in `ink-2`, right-aligned.
    * **`data-element="TapeNotional"`**: JetBrains Mono in `ink-2`, right-aligned. Computed as price times size, rounded to whole units.
    * **`data-element="TapeAggressor"`**: Inter 12 px in `ink-soft`, right-aligned. In v1 every print reads "taker"; the column exists so a future "maker" annotation (when the engine surfaces resting-order origin) can land without a layout change.
  * Each row has a 1 px bottom border in `rule` at 50 percent opacity (token `rule-tape`). The last row drops the border to avoid double-stacking with the column container's bottom edge.
* Responsive: the column is fluid (1fr in `grid-main`), so the Tape grows with the viewport. Cells use right-alignment for numerics and never wrap.
* Micro-interactions:
  * On a new trade arriving, the row mounts at the top with a one-shot 600 ms gold-tint flash (token `duration-tape-flash`). The 13th row at the bottom unmounts. The mount is intentionally not slid in via translate; a fade plus tint is enough.
  * The price cell recolors comparing to the previous trade's price; this happens at render time, not on the wire. Equal-price ticks inherit the prior color (cold-start defaults to `ink-2`).
  * The table never scrolls. If more than 12 trades arrive between renders, only the last 12 are kept; older ones drop silently.

## 6. PerfPanel (performance column)

* **`data-component="PerfPanel"`** lives in the right column of `Main` (`grid-main` third track, 320 px wide).
* Layout intent: a vertical stack of four `data-element="PerfBlock"` regions, each separated by 32 px bottom margin. Preceded by a `data-component="SectionHead"` strip (section title "performance", section aux "single thread").
* The four blocks, top to bottom:
  * **`data-element="PerfThroughput"`**:
    * **`data-element="PerfLabel"`**: Newsreader italic 12 px in `gold`, 10 px bottom margin, reads "throughput".
    * **`data-element="PerfHeadline"`**: JetBrains Mono 38 px light in `ink`, line height 1. Renders the throughput number with an inline "M" subscript in `ink-soft` at 60 percent of font size, then a `data-element="PerfUnit"` in Newsreader italic 13 px in `ink-soft` 8 px to the right reading "events / sec".
    * **`data-element="PerfHistogram"`**: a 27-bar histogram, 48 px tall, 1 px gaps between bars, bar fills with the histogram gradient. Each bar's height is set inline (`style="height: NN%"`). Below the histogram, an italic Newsreader caption in `ink-soft` 11 px reads "latency distribution, log scale".
  * **`data-element="PerfLatency"`**: a 2 by 2 grid (`grid-perf`) of cells. Each cell is a `data-element="PerfCell"` with a 10 px uppercase Inter label in `ink-soft` tracked 0.08em and a 16 px JetBrains Mono light value in `ink`. The four cells are p50, p99, p99.9, max.
  * **`data-element="PerfBookState"`**: same 2 by 2 grid anatomy. Cells: orders live, trades, levels, cancel ratio.
  * **`data-element="PerfReplay"`**: PerfLabel "replay", then a `data-element="ReplayCopy"` block in JetBrains Mono 11 px in `ink-soft` line height 2 with four lines (tape name, cursor, elapsed, progress). Each line has a `<strong>` reset to `ink` medium. Below the copy, a `data-element="ProgressBar"`: 2 px tall, `rule` track, fill is the replay-progress gradient with `glow-gold`. Width set inline as a percentage.
* Responsive: 320 px column is fixed in v1.
* Micro-interactions:
  * The throughput headline updates at 30 Hz from delta messages. To avoid distracting jitter, the rendered numeral is rounded to two decimal places; the smoothed display is the WebSocket value averaged over the last 1 s window (a 30-sample moving mean computed in the WebSocket client, not on the matching loop).
  * The histogram bars update at 30 Hz; bar heights re-render with no transition. The histogram intentionally looks like a still photograph rather than a live waveform; it conveys distribution shape, not real-time flicker.
  * The progress bar fills monotonically from 0 to 100 percent over the replay's wall-clock duration. On replay loop or restart, the bar resets to 0 with no animation.
  * In `connecting`, all four blocks render skeletons (same anatomy as Hero stat skeletons).

## 7. Footer

* **`data-component="Footer"`**.
* Layout intent: a single horizontal flex row, justify-between, full-bleed across the page width with 48 px horizontal padding and 14 px vertical padding. 1 px top border in `rule`. Background is `bg-deep` (slightly darker than the body to anchor the bottom of the page).
* Children, left to right:
  * **`data-element="FooterRuntime"`**: Inter 11 px in `ink-soft` tracked 0.04em. Reads "matching on core 4 · publisher on core 5 · 30 Hz top-of-book sampler · {N} seqlock readers · zero blocking on hot path". Each strong span ("matching", "publisher", "{N}") resets to `ink` medium.
  * **`data-element="FooterBuild"`**: Inter 11 px in `ink-faint` tracked 0.04em (the `dim` modifier). Reads "meridian v{version} · {compiler} · {flags}". Values come from a build-time constants module (`frontend/src/lib/build.ts`) populated at Vite build time from `VITE_*` env vars. The Frontend Developer wires the env vars; the Documentation Engineer documents them in `docs/setup-guide.md` during Phase 10.
* Responsive: not responsive in v1. The footer is allowed to overflow horizontally below 1280 px; mobile is a future-idea.
* Micro-interactions: none in v1. The footer is static.

## Connection state surface integration

Two of the four states from design spec section 5.5 (`connecting`, `disconnected`) require visible affordances beyond the header dot. Their layout integrations are:

* **`connecting`**: Hero's `HeroIdentity` swaps to `data-element="HeroWarmingUp"`, a Newsreader italic 32 px headline in `ink` reading "Engine warming up", a 13 px italic body in `ink-soft` reading "Fly cold start, this takes 5 to 15 seconds the first time", and a row of three 6 px gold dots with staggered `animate-pulse-live` delays (0 ms, 200 ms, 400 ms). Stat blocks, Ladder, DepthChart, Tape, PerfPanel render their skeletons. Header dot pulses gold; symbol selector remains interactive (a click during warmup re-points the in-flight subscription).
* **`stalled`**: no banner. Header dot recolors per tokens.md section 11; header meta gains a tail "stalled · {n}s ago"; numerics across the dashboard render at 50 percent opacity to signal staleness without hiding the last good values. Skeletons are not used here; users want to see the last good state.
* **`disconnected`**: `ConnectionBanner` mounts above `Main`. Header dot recolors to `ask`; header meta tail reads "disconnected · reconnecting in {n}s". The rest of the layout renders the last good state at 50 percent opacity (same treatment as `stalled`). The Reconnect attempt count comes from the WebSocket client's exponential backoff.

These behaviors are wireframed here so the Frontend Developer does not have to invent them in Phase 9. If a state's surface looks wrong against the canonical when implemented, raise it with this agent before changing the canonical or the tokens.

## Future-only sections

These are intentionally not in v1 and are not wireframed beyond a one-line stub. Adding any of them in v1 requires a brainstorm with the PM first.

* **OrderEntryDrawer**: a slide-in side panel for submitting orders to the engine. v2 only.
* **MultiSymbolGrid**: an alternate view that shows three symbols side by side. Possible v1.5.
* **AuditLogPanel**: a Tape-like panel showing the full execution report stream rather than just trades. v2.

## Definition of done

Wireframes are complete when:

* Every `data-component` and `data-element` listed in this file has a clear layout intent, a concrete sub element list, an explicit responsive note, and either a documented micro-interaction or a documented "no micro-interaction in v1".
* The Frontend Developer can build each component without needing to invent layout decisions; ambiguities route back to this agent rather than being resolved silently.
* The four connection states from design spec section 5.5 each have a documented surface here.
* This file and `/home/mustafa/src/Meridian/docs/design/tokens.md` together cover everything needed to render a faithful Twilight dashboard from the spec without opening `canonical.html`. (Opening `canonical.html` is still encouraged for ground-truth verification, just not strictly required.)
