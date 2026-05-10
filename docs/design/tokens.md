# Meridian design tokens (Twilight)

This file is the canonical extraction of every visual token from `/home/mustafa/src/Meridian/docs/design/canonical.html`. The Twilight theme was chosen on 2026-05-09 from a field of five candidate directions. The Frontend Developer must apply these tokens through Tailwind utilities sourced from the configuration block at the end of this document; no hardcoded color, font size, spacing, radius, shadow, or motion duration is permitted in component code.

A future agent should be able to read this file and produce a faithful Tailwind configuration without ever opening the canonical HTML.

## 1. Colors

### 1.1 Surfaces and rules

| Token name | Hex / value | Role | Origin in canonical HTML |
|---|---|---|---|
| `bg` | `#0E1126` | Primary page background (deep indigo). | `:root --bg` |
| `bg-2` | `#0A0D1F` | Footer bar background and bottom of the radial body gradient. | `:root --bg-2` |
| `bg-radial-top` | `#1A1E40` | Top stop of the body radial gradient. | Inline in `body` background. |
| `surface` | `#161A33` | Floating element fill (e.g. the `nav-back` chip background). | `:root --surface` |
| `surface-2` | `#1B1F3D` | Reserved for elevated panels (declared in canonical, not yet applied). | `:root --surface-2` |
| `rule` | `#232847` | Default 1 px divider color (header underline, hero borders, section heads, ladder dividers, main grid gap fill). | `:root --rule` |
| `rule-strong` | `#2F365A` | Reserved stronger divider (declared in canonical, not yet applied). | `:root --rule-strong` |
| `surface-overlay` | `rgba(22, 26, 51, 0.85)` | Translucent fill used by the floating `nav-back` chip. | Inline in `.nav-back` background. |
| `tape-row-rule` | `rgba(35, 40, 71, 0.5)` | Trade tape per-row bottom border (50 percent of `rule`). | Inline in `.trades td` border-bottom. |

### 1.2 Ink (text)

| Token name | Hex | Role |
|---|---|---|
| `ink` | `#EFE9DC` | Primary off-white body text and headline numerals. |
| `ink-2` | `#BFB7A8` | Secondary text (table cells, ladder size and orders columns, imbalance label). |
| `ink-soft` | `#8C879A` | Tertiary muted prose (header meta, hero lede, stat labels, axis legend, replay copy). |
| `ink-faint` | `#56536A` | Quaternary faint label text (table column headers, chart axis tick labels, footer dim notes). |

### 1.3 Accent and semantic

| Token name | Hex / value | Role |
|---|---|---|
| `gold` | `#D4A24C` | Primary brand accent. Used for the brand mark dot, live status pulse, section aux labels, mid-line marker on the depth chart, spread strong text, replay progress bar top, perf labels. |
| `gold-deep` | `#B97D2C` | Darker shade for the bottom of histogram bar gradient and the start of the replay progress bar gradient. |
| `bid` | `#4FA89E` | Muted teal. Bid prices, buy aggressor side label, bid depth chart line, legend dot. |
| `bid-soft` | `rgba(79, 168, 158, 0.16)` | Bid depth bar fill behind ladder rows. |
| `ask` | `#C5765B` | Faded coral. Ask prices, sell aggressor side label, ask depth chart line, legend dot, imbalance "selling pressure" emphasis. |
| `ask-soft` | `rgba(197, 118, 91, 0.16)` | Ask depth bar fill behind ladder rows. |

### 1.4 Compositional fills (gradients)

These are not single tokens; they are gradient recipes derived from the tokens above. Compose them in CSS or inline rather than introducing new color tokens.

| Recipe | Definition | Where it appears |
|---|---|---|
| Body radial | `radial-gradient(ellipse at top, #1A1E40 0%, var(--bg) 55%, var(--bg-2) 100%)` with `background-attachment: fixed`. | `body` background. |
| Spread row band | `linear-gradient(to right, transparent, rgba(212, 162, 76, 0.12) 20%, rgba(212, 162, 76, 0.12) 80%, transparent)` with top and bottom borders of `rgba(212, 162, 76, 0.3)`. | `.spread-row td` background. |
| Histogram bar | `linear-gradient(to top, var(--gold-deep), var(--gold))`. | `.histo .bar` fill. |
| Replay progress | `linear-gradient(to right, var(--gold-deep), var(--gold))` plus `box-shadow: 0 0 10px var(--gold)`. | `.progress-bar` fill and glow. |
| Bid SVG fill | `linearGradient` `#4FA89E` at 0 percent opacity 0.5 to 100 percent opacity 0.02, vertical. | Depth chart bid area, SVG `id="bidGrad"`. |
| Ask SVG fill | `linearGradient` `#C5765B` at 0 percent opacity 0.5 to 100 percent opacity 0.02, vertical. | Depth chart ask area, SVG `id="askGrad"`. |

## 2. Typography

### 2.1 Font families

Three families load from Google Fonts in the canonical HTML. These are mandatory; no substitutions.

| Family | Weights / styles | Role | Tailwind key |
|---|---|---|---|
| Newsreader | 400, 500, 600 (regular and italic), opsz 6 to 72 | Editorial display: brand mark, hero symbol, section titles, stat labels, perf labels, hero lede, brand tagline, spread row copy, imbalance copy, mid-line label inside SVG. Italic almost everywhere it appears. | `font-display` |
| Inter | 400, 500, 600, 700 | UI body text and small labels (table column headers, footer bar copy, hero meta, stat sub copy, perf cell labels). | `font-body` |
| JetBrains Mono | 300, 400, 500, 700 | Every numeric value in the UI. Always paired with `font-variant-numeric: tabular-nums`. Used in stat values, ladder cells, trade tape, perf headlines, perf cell numbers, replay timestamps, header time, footer build version. | `font-mono` |

Body default in the canonical HTML is Inter at 14 px with line height 1.5 and `font-feature-settings: 'tnum', 'cv11', 'ss01'`.

### 2.2 Type scale

The canonical HTML uses an editorial scale with deliberate jumps; do not invent intermediate sizes.

| Token name | Size (px) | Tailwind key | Used by |
|---|---|---|---|
| `text-mark` | 42 | `text-mark` | Header brand mark "meridian." |
| `text-display-xl` | 72 | `text-display-xl` | Hero symbol "AAPL". |
| `text-display-sm` | 38 | `text-display-sm` | Performance throughput headline ("6.24M"). |
| `text-display-stat` | 32 | `text-display-stat` | Hero stat values (Last, Spread, Volume). |
| `text-section` | 22 | `text-section` | Section titles ("the book", "cumulative depth", "the tape", "performance"). |
| `text-perf-cell` | 16 | `text-perf-cell` | Perf cell numbers (p50, p99, p99.9, max, orders live, etc). |
| `text-body` | 14 | `text-body` | Body default, brand tagline, hero lede, stat sub copy, spread row copy, ladder cells (13 px override below). |
| `text-ladder` | 13 | `text-ladder` | Ladder table cells. |
| `text-trades` | 12 | `text-trades` | Trade tape rows, hero meta, legend, imbalance, stat sub, perf headline unit suffix. |
| `text-meta` | 11 | `text-meta` | Header meta line, footer bar, replay copy, stat label, perf headline italic caption, nav back chip. |
| `text-micro` | 10 | `text-micro` | Table column headers (ladder and trades), section aux labels, perf cell labels, chart axis ticks. |

### 2.3 Weights

Inter and JetBrains Mono are mixed by weight to create hierarchy without leaning on color alone.

| Token name | Value | Used by |
|---|---|---|
| `font-light` | 300 | Hero stat values, performance headline, perf cell numbers (display numerals look more refined at 300 in JetBrains Mono). |
| `font-normal` | 400 | Body text, Newsreader display by default, replay copy. |
| `font-medium` | 500 | Brand mark italic, hero symbol italic, section titles italic, table headers, side-buy and side-sell labels, footer strong, replay strong, gold spread strong, imbalance strong, section aux. |
| `font-semibold` | 600 | Reserved (Newsreader 600 is loaded but not currently applied). |
| `font-bold` | 700 | Reserved (Inter 700 and JetBrains Mono 700 loaded but not currently applied). |

### 2.4 Letter spacing

| Token name | Value | Used by |
|---|---|---|
| `tracking-tight-display` | `-0.02em` | Hero symbol, hero stat values, perf headline. |
| `tracking-tight-mark` | `-0.015em` | Brand mark. |
| `tracking-tight-section` | `-0.01em` | Section titles. |
| `tracking-base` | `0` | Body text default. |
| `tracking-stat` | `0.04em` | Stat labels, header meta, footer bar. |
| `tracking-perf-label` | `0.08em` | Perf cell labels. |
| `tracking-aux` | `0.12em` | Table column headers (ladder, trades). |
| `tracking-section-aux` | `0.14em` | Section aux uppercase labels ("L8 · 16 levels", "single thread", etc). |

### 2.5 Line height

| Token name | Value | Used by |
|---|---|---|
| `leading-1` | `1` | Display numerals (brand mark, hero symbol, hero stat values, perf headline). |
| `leading-snug` | `1.4` | Brand tagline (multi-line italic). |
| `leading-base` | `1.5` | Body default. |
| `leading-replay` | `2` | Replay copy block (deliberately airy to read like a list). |

### 2.6 Italic and case

* The Newsreader family is used in italic for the brand mark, hero symbol, section titles, hero lede, stat labels, perf labels, spread row copy, imbalance copy, perf headline unit, and the depth chart mid-line label. The brand mark dot (`.`) explicitly resets to `font-style: normal` and recolors to gold.
* Uppercase is reserved for tiny labels: nav back chip, table column headers, section aux labels, perf cell labels.
* Numeric strong text inside otherwise italic copy (the `.spread-row strong`, the `.imbalance strong`, the `.replay strong`) resets to `font-style: normal` and uses JetBrains Mono.

### 2.7 Font features

Body sets `font-feature-settings: 'tnum', 'cv11', 'ss01'`. The `'tnum'` feature enables tabular numerals globally; `'cv11'` and `'ss01'` are Inter character variants for a single-story `a` and stylistic alternates. Tailwind exposes this via the `font-feature-settings` arbitrary plugin or the `@layer base` directive.

The `.num` utility additionally sets `font-variant-numeric: tabular-nums` to guarantee aligned numeric columns; mirror this in a `.num` Tailwind component class.

## 3. Spacing scale

Spacing in the canonical HTML uses a modest set of values. Map each to the Tailwind 4 px base scale where it lines up; introduce custom keys for the editorial outliers.

| Token | Pixel value | Tailwind key | Used by |
|---|---|---|---|
| `space-0` | 0 | `0` | Reset margins and paddings. |
| `space-1` | 1 | `px` | Main grid `gap: 1px` (the rule color shows through). |
| `space-2` | 2 | `0.5` | Depth bar inset (`top: 2px; bottom: 2px`), progress bar height. |
| `space-3` | 3 | `0.75` | Perf cell label margin-bottom. |
| `space-4` | 4 | `1` | Header meta line gap. |
| `space-6` | 6 | `1.5` | Live dot size (6 by 6), header meta padding inside nav back chip vertical, ladder row padding, ladder header padding, spread row strong inline margin, trades cell vertical padding, trades cell right margin, trades th right margin. |
| `space-8` | 8 | `2` | Chart axis margin-top, brand-mark dot offset, perf-headline unit margin-left, legend-dot margin-right, stat-label margin-bottom, imbalance margin-left auto, replay progress padding-top. |
| `space-10` | 10 | `2.5` | Perf label margin-bottom. |
| `space-12` | 12 | `3` | Nav back horizontal padding, hero lede margin-bottom, hero meta margin-top, section head padding-bottom, trades cell horizontal padding, trades th right padding gap. |
| `space-14` | 14 | `3.5` | Footer bar vertical padding, perf grid gap row, perf grid margin-top, legend margin-top, replay progress margin-top, histogram margin (top and bottom). |
| `space-18` | 18 | `4.5` | Stat border-left padding-left, brand tagline padding-left, nav back top offset (top 18). |
| `space-22` | 22 | `5.5` | Brand internal gap, section head margin-bottom, nav back right offset (right 22), stat padding-left override, perf grid column gap. |
| `space-24` | 24 | `6` | Legend gap. |
| `space-32` | 32 | `8` | Column horizontal padding inside `.col`. |
| `space-36` | 36 | `9` | Header top padding, column vertical padding, chart-wrap margin-bottom, perf-block margin-bottom (32 px in fact; 36 reserved for header). |
| `space-44` | 44 | `11` | Hero bottom padding. |
| `space-48` | 48 | `12` | Header horizontal padding, hero horizontal padding, hero gap, footer horizontal padding. |
| `space-56` | 56 | `14` | Hero top padding. |

Notes on irregularities preserved from the canonical:

* Header padding is `36px 48px 24px` (top different from bottom). Map to bespoke values rather than rounding.
* Hero padding is `56px 48px 44px`.
* Footer padding is `14px 48px`.
* Nav back chip padding is `6px 12px`.
* Column padding is `36px 32px`.
* Perf block bottom margin is 32 px; do not collapse with `space-36`.

## 4. Layout (grids, widths, breakpoints)

| Token | Definition | Used by |
|---|---|---|
| `grid-hero` | `grid-template-columns: 2fr 1fr 1fr 1fr; gap: 48px;` | Hero strip (lede plus three stats). |
| `grid-main` | `grid-template-columns: 380px 1fr 320px; gap: 1px; background: var(--rule);` | Main three-column dashboard. The 1 px gap with the rule background is what makes the rules between columns appear. |
| `grid-perf` | `grid-template-columns: 1fr 1fr; gap: 14px 18px;` | Performance column 2 by 2 stat grids. |
| `chart-aspect` | `viewBox 0 0 600 240`, intrinsic aspect 5:2, height fixed at 240 px in canonical with `width: 100%`. | Depth chart SVG. |
| `histogram-height` | 48 px tall, 1 px gap between bars. | Latency distribution histogram. |
| `progress-height` | 2 px tall. | Replay progress bar. |

Breakpoints: the canonical HTML is desktop-only. There are no `@media` queries in the file. Mobile is a future-idea per `SPEC.md`. The Tailwind config retains the default breakpoints so the Frontend Developer can opt into them later without re-extracting tokens, but the v1 frontend does not need to honor any below `lg` (1024 px). The minimum supported viewport in v1 is 1280 px.

## 5. Border radius

The canonical HTML is intentionally hard-edged. Only two radii appear.

| Token | Value | Used by |
|---|---|---|
| `radius-pill` | `50%` | Live status dot (perfect circle). |
| `radius-bar` | `1px` | Depth bar squared-off feel; histogram bar `1px 1px 0 0` (top corners only). |

There is no `rounded-md`, no `rounded-lg`, no `rounded-2xl` in this UI. Components default to square corners.

## 6. Shadows and glows

There are three glow effects in the canonical HTML, all driven by the gold or semantic colors. No diffuse drop shadows are used.

| Token | Definition | Used by |
|---|---|---|
| `glow-gold` | `box-shadow: 0 0 10px var(--gold)` | Header live dot, replay progress bar. |
| `glow-bid` | `box-shadow: 0 0 8px var(--bid)` | Legend dot for bid in the depth chart caption. |
| `glow-ask` | `box-shadow: 0 0 8px var(--ask)` | Legend dot for ask in the depth chart caption. |

Tailwind's default `shadow` utilities are not used by any token in the canonical. The Frontend Developer should disable Tailwind's default shadow scale or override it with these three tokens; introducing a generic `shadow-md` would drift from the design.

## 7. Backdrop filter

| Token | Value | Used by |
|---|---|---|
| `blur-chip` | `backdrop-filter: blur(8px)` | Nav back chip overlay. |

## 8. Motion

The canonical HTML defines exactly one keyframe animation and no transition declarations. The Twilight aesthetic prefers stillness with one calm pulse; do not invent additional motion in v1.

| Token | Definition | Used by |
|---|---|---|
| `animate-pulse-live` | `pulse 1.6s infinite` with keyframe `50% { opacity: 0.4 }` | Header live status dot. |
| `transition-hover-default` | None declared. Hover effect on the nav back chip (`border-color` and `color` swap to gold) instantly applies. The Frontend Developer may add a `150ms ease` transition to hover states for polish; document any addition here when applied. | Nav back chip on hover (canonical), Frontend Developer hover and focus polish (Phase 9). |
| `duration-tape-flash` | Reserved. Not in the canonical, but Phase 9 will need a brief highlight for newly arrived trade rows. Recommended: 600 ms ease-out fade from a translucent gold to transparent. Confirm with this agent at Phase 9. | Trade tape new-row highlight (Phase 9 only). |

## 9. Z-index

| Token | Value | Used by |
|---|---|---|
| `z-chip` | `100` | Nav back chip floats above content. |
| `z-bar` | `1` | Inline `position: relative; z-index: 1` on `.ladder td > span` so the depth bar background sits behind the numeric text. |

## 10. Component-specific token notes

These are derived tokens (compositions of the base tokens above) called out so the Frontend Developer never has to re-derive them.

* **Live status dot**: 6 px square, `radius-pill`, `bg-gold`, `glow-gold`, `animate-pulse-live`.
* **Spread row band**: full-width row inside the ladder. Vertical padding 14 px. Background uses the spread-row gradient recipe (section 1.4). Top and bottom borders `rgba(212, 162, 76, 0.3)`. Italic Newsreader text at 13 px in `ink-soft`. Numerals inside the row reset to JetBrains Mono medium in `gold` with 8 px horizontal margin.
* **Depth bar inside ladder rows**: absolutely positioned, 2 px inset from top and bottom, anchored to the right edge, `radius-bar`. Width is set inline via `style="width: NN%"` where NN is the level's depth as a percentage of the heaviest level on that side. Fill is `bid-soft` for bid rows and `ask-soft` for ask rows.
* **Section head**: flex row, baseline aligned, 22 px bottom margin, 12 px bottom padding, 1 px bottom border in `rule`. Title is Newsreader italic medium 22 px in `ink`, tracked tight. Aux is Inter medium uppercase 10 px in `gold`, tracking 0.14em.
* **Stat block (hero)**: 1 px left border in `rule`, 22 px left padding. Label is Newsreader italic 11 px in `ink-soft` tracked 0.04em with 8 px bottom margin. Value is JetBrains Mono 32 px light in `ink` (or `bid` for up, `ask` for down, `gold` for spread accent). Sub line is Inter 12 px in `ink-soft` 6 px below the value, with up/down spans in `bid` or `ask`.
* **Perf cell**: 2-by-2 grid item. Label is Inter 10 px uppercase in `ink-soft` tracked 0.08em with 3 px bottom margin. Number is JetBrains Mono 16 px light in `ink`.
* **Histogram bar**: vertical bar, height set inline via `style="height: NN%"`, fills with histogram bar gradient (section 1.4), `border-radius: 1px 1px 0 0`. The container is a flex row, items aligned to the bottom, 1 px gap between bars, total height 48 px.
* **Progress bar (replay)**: 2 px tall. Track is `rule`. Fill is replay progress gradient with 10 px gold glow.
* **Footer bar**: 1 px top border in `rule`, 14 px vertical padding, 48 px horizontal padding. Background is `bg-2` (slightly darker than the page). Inter 11 px in `ink-soft` tracked 0.04em. `strong` resets weight to medium and color to `ink`. `dim` recolors to `ink-faint`.

## 11. Connection state surfaces (Phase 9 scope, derived from design spec section 5.5)

The canonical HTML renders only the `live` state. Phase 9 also needs to render `connecting`, `stalled`, and `disconnected` states. Tokens for each, building on the palette above:

| State | Header dot | Header meta copy | Hero behavior |
|---|---|---|---|
| `connecting` | `gold` with `animate-pulse-live` (same as live; the cold start is expected, not an error). | "engine warming up · Fly cold start" in `ink-soft`. | Replace symbol and stats with an "Engine warming up" hero block: Newsreader italic 32 px headline in `ink`, body copy in `ink-soft` at 13 px naming the cold start as expected, and a determinate-looking dot row that re-uses the `glow-gold` token. |
| `live` | `gold` with `animate-pulse-live`. | Live timestamp as in canonical. | Default canonical layout. |
| `stalled` | `ink-faint` static (no pulse), then briefly amber. Recommend introducing `--amber: #D4A24C` at 60 percent saturation. To avoid token drift, reuse `gold-deep` (`#B97D2C`). | "stalled · last update Ns ago" in `ink-soft`. | Layout unchanged; ladder, depth chart, tape continue to show the last good state with a `0.5` opacity overlay on numerics. |
| `disconnected` | `ask` static (no pulse). | "disconnected · reconnecting in Ns" in `ask`. | Layout unchanged; an inline banner above the main grid in `ask-soft` background with `ask`-colored copy explains the reconnect attempt. Banner height: 32 px. Banner padding: 6 px 48 px. |

The Frontend Developer reviews these state surfaces with this agent before Phase 9 closes; if any feels jarring against the canonical, adjust here first, then in code.

## 12. Definition of done

Token extraction is complete when:

* Every `:root` CSS variable in `canonical.html` is reproduced in section 1 (colors), section 2 (typography), or section 6 (shadows).
* Every distinct font-size, padding, margin, gap, border-radius, and box-shadow used in the canonical's class definitions is named here.
* Every gradient (linear or radial) used in the canonical is reproduced as a recipe in section 1.4 or section 6.
* The Tailwind config block at the end of this document compiles without warnings under Tailwind 3.4 plus and uses only the tokens listed here.
* The Frontend Developer can build a faithful Header, Hero, Ladder, DepthChart, Tape, PerfPanel, and Footer using only these tokens plus the wireframes in `/home/mustafa/src/Meridian/docs/design/wireframes.md`, with no hardcoded literals.

A future Flow A or Flow B design change updates this file before any React or Tailwind code changes; this file and `canonical.html` must agree at every commit.

## 13. Tailwind config (drop into frontend/tailwind.config.ts)

This is the initial Tailwind 3 plus configuration. The DevOps Engineer scaffolds the frontend in parallel with this dispatch, so this block is provided here for the PM to reconcile into `frontend/tailwind.config.ts` once the scaffold lands. The Frontend Developer extends this in Phase 9; the Engine Developer and other agents do not touch it.

```ts
import type { Config } from "tailwindcss";

const config: Config = {
  content: ["./index.html", "./src/**/*.{ts,tsx}"],
  darkMode: "class",
  theme: {
    screens: {
      sm: "640px",
      md: "768px",
      lg: "1024px",
      xl: "1280px",
      "2xl": "1536px",
    },
    extend: {
      colors: {
        bg: {
          DEFAULT: "#0E1126",
          deep: "#0A0D1F",
          radial: "#1A1E40",
        },
        surface: {
          DEFAULT: "#161A33",
          raised: "#1B1F3D",
          overlay: "rgba(22, 26, 51, 0.85)",
        },
        rule: {
          DEFAULT: "#232847",
          strong: "#2F365A",
          tape: "rgba(35, 40, 71, 0.5)",
        },
        ink: {
          DEFAULT: "#EFE9DC",
          2: "#BFB7A8",
          soft: "#8C879A",
          faint: "#56536A",
        },
        gold: {
          DEFAULT: "#D4A24C",
          deep: "#B97D2C",
        },
        bid: {
          DEFAULT: "#4FA89E",
          soft: "rgba(79, 168, 158, 0.16)",
        },
        ask: {
          DEFAULT: "#C5765B",
          soft: "rgba(197, 118, 91, 0.16)",
        },
      },
      fontFamily: {
        display: ["Newsreader", "ui-serif", "Georgia", "serif"],
        body: ["Inter", "ui-sans-serif", "system-ui", "sans-serif"],
        mono: [
          "JetBrains Mono",
          "ui-monospace",
          "SFMono-Regular",
          "Menlo",
          "monospace",
        ],
      },
      fontSize: {
        micro: ["10px", { lineHeight: "1.4" }],
        meta: ["11px", { lineHeight: "1.4" }],
        trades: ["12px", { lineHeight: "1.5" }],
        ladder: ["13px", { lineHeight: "1.5" }],
        body: ["14px", { lineHeight: "1.5" }],
        "perf-cell": ["16px", { lineHeight: "1.2" }],
        section: ["22px", { lineHeight: "1.2", letterSpacing: "-0.01em" }],
        "display-stat": ["32px", { lineHeight: "1", letterSpacing: "-0.02em" }],
        "display-sm": ["38px", { lineHeight: "1", letterSpacing: "-0.02em" }],
        mark: ["42px", { lineHeight: "1", letterSpacing: "-0.015em" }],
        "display-xl": ["72px", { lineHeight: "1", letterSpacing: "-0.02em" }],
      },
      letterSpacing: {
        "tight-display": "-0.02em",
        "tight-mark": "-0.015em",
        "tight-section": "-0.01em",
        stat: "0.04em",
        "perf-label": "0.08em",
        aux: "0.12em",
        "section-aux": "0.14em",
      },
      lineHeight: {
        replay: "2",
      },
      spacing: {
        "0.5": "2px",
        "0.75": "3px",
        "1.5": "6px",
        "2.5": "10px",
        "3.5": "14px",
        "4.5": "18px",
        "5.5": "22px",
        "11": "44px",
        "14": "56px",
      },
      borderRadius: {
        none: "0",
        bar: "1px",
        full: "9999px",
      },
      boxShadow: {
        none: "none",
        "glow-gold": "0 0 10px #D4A24C",
        "glow-bid": "0 0 8px #4FA89E",
        "glow-ask": "0 0 8px #C5765B",
      },
      backdropBlur: {
        chip: "8px",
      },
      backgroundImage: {
        "body-radial":
          "radial-gradient(ellipse at top, #1A1E40 0%, #0E1126 55%, #0A0D1F 100%)",
        "spread-band":
          "linear-gradient(to right, transparent, rgba(212, 162, 76, 0.12) 20%, rgba(212, 162, 76, 0.12) 80%, transparent)",
        histogram: "linear-gradient(to top, #B97D2C, #D4A24C)",
        "replay-progress": "linear-gradient(to right, #B97D2C, #D4A24C)",
      },
      keyframes: {
        "pulse-live": {
          "50%": { opacity: "0.4" },
        },
      },
      animation: {
        "pulse-live": "pulse-live 1.6s infinite",
      },
      gridTemplateColumns: {
        hero: "2fr 1fr 1fr 1fr",
        main: "380px 1fr 320px",
        perf: "1fr 1fr",
      },
    },
  },
  corePlugins: {
    boxShadow: true,
  },
  plugins: [
    function ({ addBase, addUtilities }: any) {
      addBase({
        "html, body": { height: "100%" },
        body: {
          fontFamily: "Inter, system-ui, sans-serif",
          backgroundColor: "#0E1126",
          backgroundImage:
            "radial-gradient(ellipse at top, #1A1E40 0%, #0E1126 55%, #0A0D1F 100%)",
          backgroundAttachment: "fixed",
          color: "#EFE9DC",
          fontSize: "14px",
          lineHeight: "1.5",
          fontFeatureSettings: "'tnum', 'cv11', 'ss01'",
          WebkitFontSmoothing: "antialiased",
        },
      });
      addUtilities({
        ".num": {
          fontFamily: "'JetBrains Mono', ui-monospace, monospace",
          fontVariantNumeric: "tabular-nums",
        },
        ".editorial": {
          fontFamily: "Newsreader, ui-serif, serif",
          fontStyle: "italic",
          fontWeight: "400",
        },
      });
    },
  ],
};

export default config;
```

Reconciliation note for the PM: the DevOps Engineer's `pnpm create vite` scaffold produces a default `tailwind.config.ts`. Replace its contents wholesale with the block above. The block assumes Tailwind 3.4 plus; if DevOps installs Tailwind 4, the Frontend Developer adapts the config to the v4 token model in Phase 9 and updates this section here to match.
