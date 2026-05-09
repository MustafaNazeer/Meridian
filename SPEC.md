# Meridian: Limit Order Book and Matching Engine

## Project summary

A price-time priority limit order book and matching engine, written in C++20, with a live web visualization. Meridian is built as a portfolio piece for trading firms and high-frequency-finance roles, and the venue half of a two-project quant systems narrative on the resume (Vega is the pricing half).

The engine is delivered as a static library (`libmeridian.a`) plus three thin driver binaries: `meridian-bench` (zero-I/O throughput and latency benchmark), `meridian-replay` (CLI ITCH replayer that streams JSON Lines), and `meridian-server` (live demo with uWebSockets serving a React frontend). The library and the bench binary contain zero networking code, so the headline 6.2M events per second number is defensible in isolation.

GitHub repository: https://github.com/MustafaNazeer/Meridian

Visual direction: the canonical visual ground truth for the project is `/home/mustafa/src/Meridian/docs/design/canonical.html` (the **Twilight** theme: deep indigo `#0E1126` background, gold `#D4A24C` accent, muted teal `#4FA89E` for bid, faded coral `#C5765B` for ask, off-white `#EFE9DC` ink, Newsreader italic for editorial display, Inter for UI body, JetBrains Mono with tabular-nums for all numerics). Open the file in a browser to see the full dashboard rendered with realistic AAPL data. Twilight was chosen on 2026-05-09 from a field of five candidate directions; the rejected four (Bourse, Terminal, Eikon, Onyx) are kept under `design-mockups/themes/` for design archaeology.

The contrast between Vega's "Oxblood" theme (warm dark reds) and Meridian's "Twilight" theme (cool indigos with gold) is intentional: each project gets its own visual identity so the resume reads as a constellation of pet projects rather than two clones of the same template.

## Source material

This spec was written from the user's brainstorming session on 2026-05-09 (preserved in conversation history) and the deep technical design at `/home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md`. The technical design doc is the engineering contract; this file is the higher-level project plan. There is no external transcript or paper underlying the project; the design space (limit order books, price-time priority, seqlock-protected publication, ITCH 5.0 replay) is well-known textbook material plus practitioner blog posts.

Reference reading the agents may consult during implementation:

* NASDAQ TotalView ITCH 5.0 specification (publicly available PDF).
* LOBSTER sample data (public, free).
* "The Microstructure Approach to Exchange Rates" (Lyons) for context on order book theory.
* Jane Street Tech Talks on low-latency systems for hot-path discipline patterns.

## How this spec is meant to be used

Each section of work is owned by a specialized agent so a single agent can focus deeply on one domain. Quality improves when each role stays scoped. Every agent has a dedicated file in `agents/` that describes:

1. **Mission**: the one sentence purpose of the role.
2. **Inputs**: the deliverables this agent receives from upstream agents.
3. **Outputs**: the deliverables this agent produces for downstream agents.
4. **Tasks**: the ordered task checklist, grouped by phase.
5. **Plugin(s) to use**: which Claude Code plugins or skills the agent should invoke.
6. **Definition of done**: the acceptance criteria for the agent's work.
7. **Handoffs**: which agents this role hands work to next.

Read `agents/00-project-manager.md` first. The Project Manager owns sequencing, runs the brainstorming and planning phases, and dispatches every other agent.

## How to deploy these agents

The files in `agents/` are not just documentation. Each one is a self contained brief for a specialized agent that should be deployed and run on its own. The intent is that each agent runs in its own scoped context (its own Claude Code session or subagent invocation) so it stays focused and produces higher quality work than a single generalist trying to do everything.

> **Quick start for Claude Code**: read `CLAUDE.md` (auto loaded) and `GETTING-STARTED.md` for the literal first session walkthrough. The rest of this section is the explanation behind those instructions.

### Recommended deployment pattern

1. **Start a fresh Claude Code session in `/home/mustafa/src/Meridian`.** `CLAUDE.md` auto loads and tells the session it is the Project Manager. The PM reads `SPEC.md` and `agents/00-project-manager.md`, then invokes `superpowers:brainstorming` and `superpowers:writing-plans` to produce the per phase implementation plan in `docs/plan.md`.

2. **For each phase**, the Project Manager session dispatches the agents whose tasks are listed for that phase. Two ways to dispatch:

   * **Subagent (recommended for parallel, scoped work).** Use the `Task` tool with `subagent_type: "general-purpose"` and a prompt that says "You are the [Role] agent for the Meridian project. Read `/home/mustafa/src/Meridian/SPEC.md` and `/home/mustafa/src/Meridian/agents/NN-role.md` and execute the Phase X tasks listed there. Use the plugins listed in the agent file." Subagents are best for agents whose work does not depend on a long running conversation (e.g., Code Reviewer, Security Engineer review pass, QA test writing, Performance Engineer profiling).

   * **Separate Claude Code session (recommended for agents that own a long lived stream of work).** Open a new terminal in `/home/mustafa/src/Meridian`, start `claude`, and have that session play one role for the duration of the phase. Best for the Engine Developer, Frontend Developer, and DevOps Engineer, where the agent benefits from accumulating context across many edits.

3. **Each agent uses the plugins listed in its file.** For example, the Frontend Developer invokes `frontend-design`, `vercel-react-best-practices`, and `superpowers:test-driven-development`. The Project Manager invokes `superpowers:brainstorming`, `superpowers:writing-plans`, and `superpowers:dispatching-parallel-agents`. The Security Engineer invokes `security-review`. Do not skip these; they are how the agent stays in role.

4. **Handoffs are explicit.** When an agent finishes its phase tasks, it commits its output, opens a PR, and writes a short handoff note in the PR description naming the next agent (per the "Handoffs" section of its agent file). The Project Manager session reviews the handoff and dispatches the next agent.

5. **Quality gates between phases.** Before a phase closes, the Project Manager confirms QA, Security, Code Review, Citation and Fact Auditor (for any phase that touched documentation), and (where applicable) Risk and Financial Correctness Reviewer, Performance Engineer, Concurrency Reviewer, and Reference Implementation Engineer have all signed off. A phase that has not been signed off does not advance. The full gate list is enumerated in `CLAUDE.md` under "Phase model: do not advance prematurely".

### What to do if an agent goes off script

* If an agent's output does not match its agent file, point at the file and re prompt. Do not let scope creep slide.
* If an agent needs information another agent owns, route the question through the Project Manager rather than letting agents talk laterally; this keeps coordination centralized.
* If a phase reveals a missing decision, send it back to brainstorming with the Project Manager. Do not have any one agent make a project wide decision unilaterally.

## Tech stack decisions

| Layer | Choice | Rationale |
|---|---|---|
| Engine language | C++20 | Lingua franca of HFT, matches the resume bullet, modern features (concepts, ranges, std::span) without C++23 tooling fragility. |
| Build system | CMake 3.25+ with Ninja | Industry standard for C++, plays well with FetchContent for vendored deps. |
| Compiler | clang 19+ (primary), gcc 14+ (secondary) | Best diagnostics, sanitizer support, and PGO maturity. |
| Test framework | GoogleTest | Most common in HFT codebases. |
| Property testing | rapidcheck | C++ analog of QuickCheck/Hypothesis for the matching invariants. |
| Benchmark framework | Google Benchmark | Standard for microbenchmarks. |
| WebSocket library | uWebSockets v20+ | Single-process WebSocket server with low overhead. |
| ITCH parser | Hand-rolled (one header file) | The 5.0 format is small and stable; a parser library is overkill. |
| Logging | spdlog | De facto standard, level filtered out of release hot path. |
| JSON | simdjson (parse), glaze (serialize) | Both are fast and modern. |
| Frontend framework | React 18 + Vite + TypeScript | Mirrors Vega's tooling so the constellation has one frontend story. |
| Frontend styling | Tailwind CSS | Locked in for v1. Tokens come from the canonical Twilight HTML at `docs/design/canonical.html`. |
| Visual theme | Twilight (dark) | Background `#0E1126`, accent `#D4A24C`, fonts Inter (UI), Newsreader italic (display), JetBrains Mono (numerics). Source: `docs/design/canonical.html`. |
| Frontend hosting | Cloudflare Pages | Same as Vega; free tier; instant deploys. |
| Backend hosting | Hetzner CX22 VPS (~€4/mo) or Fly.io | Render does not host raw C++ binaries comfortably; need a real Linux box. Fly.io is alternative. |
| State management (frontend) | Zustand | Small, ergonomic, no Redux ceremony. |
| Charts | Hand-rolled SVG | Data is small (≤20 levels per side, ≤50 trades); a chart library is overkill. |
| WebSocket protocol | JSON over WSS | Simple, debuggable in browser DevTools. Binary protocol can come later if measured to matter. |
| Auth | None for v1 | Public read-only demo. |
| License | MIT | Industry default for portfolio C++ projects. |
| Package manager (frontend) | pnpm | Fast and disk efficient. |
| CI/CD | GitHub Actions | Tests on every PR, deploy on tag. |

## Phased build

Each phase is a shippable milestone. Earlier phases must be production ready before the next phase begins. A phase is "done" when QA, Security, Code Review, and (where applicable) Risk and Performance all sign off, plus the Project Manager.

### Per phase usage budgets

The user is on the Claude Max 5x plan. Each phase below is **sized to fill roughly 90 to 99 percent of a single Max 5x usage window**. The "Window cost" estimate next to each phase is a rough budget; treat it as a planning aid, not a hard cap. After every phase, the PM session is required to stop and run the check-in protocol described in `CLAUDE.md` ("Pacing rule"). Do not advance to the next phase without explicit user confirmation.

When a phase comes in well under budget, the PM may bundle the next short phase into the same window if the user agrees during the check-in.

### Phase 0: Foundations
**Window cost: ~95 percent of one Max 5x window.** Heavy planning, parallel agent dispatches, repo init, frontend skeleton, threat model, design tokens, and architecture doc all happen here.

Project Manager runs brainstorming and writes the implementation plan in `docs/plan.md`. UI/UX Designer reads the canonical Twilight HTML at `docs/design/canonical.html`, extracts tokens to `docs/design/tokens.md` and a draft `frontend/tailwind.config.ts` extension, and writes screen layout descriptions to `docs/design/wireframes.md`. Security Engineer publishes the threat model and baseline. DevOps Engineer wires the local working directory `/home/mustafa/src/Meridian/` into the existing GitHub repo `MustafaNazeer/Meridian` (run `git init` locally, add the GitHub repo as `origin`, push the existing files as the first commit), then scaffolds the CMake project skeleton (root `CMakeLists.txt`, `include/meridian/`, `src/`, `apps/{bench,replay,server}`, `tests/`) and the `pnpm` plus Vite plus React plus TypeScript plus Tailwind project under `frontend/`, dropping the Tailwind config sketch from the Twilight HTML's tokens into `frontend/tailwind.config.ts`. Documentation Engineer writes the initial README and architecture overview. Citation and Fact Auditor runs the first pass on `README.md`, `SPEC.md`, and `docs/architecture.md`, files the initial audit at `docs/audits/citation-audit-{date}.md`, and signs off only when every concrete claim traces to code or canonical reference.

**End of phase**: PM runs the mandatory check-in protocol from `CLAUDE.md`.

### Phase 1: Core data structures and single-symbol matching
**Window cost: ~95 percent of one window.** This is the engine's first real phase: `Order`, `Level`, `Book`, `OrderPool`, and a working limit + market + IOC matching loop with FIFO at price levels.

Quant Domain Validator confirms the price-time priority semantics and IOC behavior against textbook references and writes `docs/risk/matching-semantics.md`. Reference Implementation Engineer writes the first cut of `tests/reference/matching_reference.py` covering limit, market, and IOC; every worked example in `matching-semantics.md` runs against it as a unit test. Engine Developer implements the data structures and the matching loop in `libmeridian.a`. QA Engineer writes unit tests for every C++ type and integrates the Python reference into the test suite. Performance Engineer adds the initial benchmark scaffold under `bench/` and meridian-bench in `apps/bench/`. Code Reviewer audits the hot path for allocations, exceptions, and unnecessary virtual dispatch. Risk and Financial Correctness Reviewer cross-validates by running the same worked examples against both the C++ engine and the Python reference; both must agree.

**End of phase**: run the check-in protocol.

### Phase 2: Multi-instrument and cancel-by-id
**Window cost: small alone (~50 percent). PM should bundle Phase 2 plus Phase 3 into a single window unless the user objects at check-in.**

Engine Developer adds `BookRegistry` (`unordered_map<Symbol, Book>`) for multi-instrument dispatch and the O(1) cancel-by-id path via the per-book order ID map. Reference Implementation Engineer extends `tests/reference/matching_reference.py` with multi-symbol dispatch and cancel-by-id, confirming cross-symbol cancels do not leak across books. QA Engineer writes integration tests across multiple symbols, running both the C++ engine and the Python reference and diffing their outputs. Risk and Financial Correctness Reviewer signs off that cancel-after-match is consistent in both implementations.

**End of phase**: if not bundled with Phase 3, run the check-in protocol.

### Phase 3: Property-based tests for matching invariants
**Window cost: ~50 percent alone, or ~95 percent when bundled with Phase 2.**

Engine Developer plus QA Engineer wire `rapidcheck` into the test build and implement the ten matching invariants listed in the technical design spec (price-time priority, quantity conservation, no double fill, no lost orders, spread non-negative, cancel idempotence, IOC never rests, FOK is all-or-nothing, post-only never crosses, top-of-book monotonicity within a tick). Each invariant runs ≥1000 generated cases per CI run. Reference Implementation Engineer runs the same generated case corpus against the Python reference; both engines must produce identical execution report sequences for any generated input. Disagreement is signal, and the reference is presumed correct unless this agent identifies the disagreement as a known reference bug. Risk and Financial Correctness Reviewer adjudicates any persistent disagreement and signs off.

**End of phase**: run the check-in protocol.

### Phase 4: Seqlock-protected top-of-book and sampler
**Window cost: ~85 percent of one window.** This phase introduces concurrency: a publisher thread reading the seqlock-protected top-of-book at 30 Hz without blocking the matching loop.

Concurrency Reviewer's primary phase. Pairs with the Market Microstructure Engineer on the seqlock protocol from the start; protocol correctness is a design-time decision, not a code-review afterthought. The agent vetos any seqlock change that does not match textbook seqlock pseudocode and signs off only when the TSAN run with a busy writer and many readers is clean. Engine Developer implements `TopOfBookSnapshot` with the seqlock writer/reader protocol per the Concurrency Reviewer's design. QA Engineer writes ThreadSanitizer tests. Performance Engineer profiles the hot path to confirm the seqlock writer adds no observable cost; coordinates with the Concurrency Reviewer on memory ordering choices (the writer's first sequence increment can be `relaxed`, the data writes need `release` ordering relative to the second sequence increment). Documentation Engineer files `docs/adr/0002-seqlock-for-top-of-book.md` capturing the memory ordering rationale; Concurrency Reviewer contributes the technical content.

**End of phase**: run the check-in protocol.

### Phase 5: Benchmark hits 6M events per second
**Window cost: ~95 percent of one window.** The phase that earns the resume bullet.

Performance Engineer drives this phase. Compiler flags (`-O3 -march=native`), Profile Guided Optimization (PGO), cache-line layout audit, branch-prediction friendly inner loops, false sharing audit. Target: meridian-bench reports ≥6M events per second steady state on a modern x86_64 desktop with p50 ≤ 500 ns, p99 ≤ 2 μs, p99.9 ≤ 5 μs. The benchmark report is written to `docs/perf/benchmark-report.md` with histograms and methodology. Concurrency Reviewer pairs on any cache-layout or false-sharing change that touches shared state. Citation and Fact Auditor performs an end-to-end pass on the benchmark report once written: every number traces to either `bench/baseline.json` or a histogram CSV checked into the repo; methodology claims (warmup, sample count, machine specs, compiler version) trace to documented config. Any rounding drift between the report and the eventual README headline is caught here, not at Phase 11.

**End of phase**: run the check-in protocol.

### Phase 6: NASDAQ ITCH 5.0 replay
**Window cost: ~95 percent of one window.** ITCH parser plus replayer plus integration tests.

Engine Developer implements the ITCH 5.0 message parser as a single header file under `include/meridian/itch.hpp`. Replayer in `apps/replay/` reads a tape and produces `EngineEvent`s for the matching loop. Reference Implementation Engineer writes `tests/reference/itch_reference.py` (the simplest possible Python ITCH parser, decoding only the messages the C++ parser handles, with each decoder docstring naming the spec section it implements) and `tests/reference/run_reference.py` (CLI that drives the reference matching engine from the ITCH parser). QA Engineer writes an integration test against a small public ITCH sample: the C++ engine and the reference must agree exactly on final book state and total fill count. Risk and Financial Correctness Reviewer hand-audits at least 10 messages from the sample against the spec and signs off. Citation and Fact Auditor reviews `docs/risk/itch-conformance.md` against the actual ITCH 5.0 specification, spot-checks 10 message-type claims, and signs off.

**End of phase**: run the check-in protocol.

### Phase 7: Post-only and FOK order types
**Window cost: small alone (~30 percent). PM should bundle Phase 7 plus Phase 8 into a single window unless the user objects at check-in.**

Engine Developer adds post-only (rejected if it would cross) and FOK (fill-or-kill: fully fill or fully cancel) order types. Reference Implementation Engineer extends the Python reference with post-only and FOK semantics; the reference's worked-example tests must pass for both order types before the C++ engine is signed off. QA Engineer extends the property tests so the existing invariants still hold and adds new invariants specific to post-only (never crosses) and FOK (all-or-nothing). Risk and Financial Correctness Reviewer signs off after confirming both implementations agree on edge cases.

**End of phase**: if not bundled with Phase 8, run the check-in protocol.

### Phase 8: WebSocket server and protocol
**Window cost: ~60 percent alone, or ~90 percent when bundled with Phase 7.**

Engine Developer plus DevOps Engineer wire uWebSockets into `apps/server/`. Define the WebSocket protocol: on connect, the client receives a `snapshot` message with the L10 book plus recent trades plus performance metrics; thereafter, the sampler thread pushes a `delta` message at 30 Hz with changes only. The protocol is documented in `docs/api/websocket.md`. Security Engineer reviews the WebSocket handshake, origin checks, max client count, and message size limits. Concurrency Reviewer reviews the sampler-publisher-WebSocket event loop interactions: the handoff between the sampler thread and the uWebSockets event loop must be lock-free or use a single SPSC queue with documented memory ordering, and the matching loop must never be blocked by sampler or publisher work. Citation and Fact Auditor reviews `docs/api/websocket.md`: every message field claim is checkable against the actual `apps/server/sampler.cpp` JSON serialization code, and the documented examples parse cleanly.

**End of phase**: run the check-in protocol.

### Phase 9: React frontend with Twilight visual
**Window cost: ~95 to 99 percent of one window.** This is the heaviest UI phase: every component, the WebSocket client, the depth chart SVG, and the trade tape land here.

Frontend Developer builds the React plus Vite app following the Twilight visual tokens and the wireframes from Phase 0. Components: Header, Hero, Ladder, DepthChart, Tape, PerfPanel, Footer. WebSocket client in `frontend/src/ws/client.ts` with auto-reconnect. State managed by Zustand. UI/UX Designer reviews against the canonical HTML. Accessibility Specialist runs an a11y audit (keyboard nav, screen reader labels, color contrast on the dark theme). Security Engineer reviews CSP and the connect-src for the WebSocket URL.

**End of phase**: run the check-in protocol.

### Phase 10: Hosting and CI/CD
**Window cost: ~85 percent of one window.** Deployment requires the user to log in and click through two dashboards (Cloudflare, Hetzner or Fly.io), so expect back and forth with the user during this phase.

DevOps Engineer deploys the frontend to Cloudflare Pages, sets up the C++ binary on the chosen VPS (systemd service, log rotation, restart policy), and configures the WebSocket URL via `VITE_WS_URL`. Sets up the custom subdomain if the user wants one. Security Engineer runs the final hardening checklist (HTTPS, HSTS, CSP, dependency scan via `pip-audit` for any Python in tests/ and `pnpm audit` for the frontend). Documentation Engineer finalizes `docs/setup-guide.md`.

**End of phase**: run the check-in protocol.

### Phase 11: Polish, README, and benchmark report
**Window cost: ~80 percent of one window.** The phase that turns the working build into a presentable portfolio piece.

Documentation Engineer writes the headline README with the benchmark numbers, design rationale, demo URL, and design doc links. Performance Engineer regenerates the benchmark report PDF with the final hosted-environment numbers. Code Reviewer does one full sweep across `libmeridian.a` for any remaining cleanups. QA Engineer runs the full regression suite one more time. Documentation Engineer writes a one-page architecture summary suitable for a recruiter to read in 60 seconds. Citation and Fact Auditor performs the final pass: every shipping document gets audited end to end against code and measurements, every prior `unverified` claim is resolved, and the final audit report at `docs/audits/citation-audit-{date}.md` is the project's pre-publish checklist. Risk and Financial Correctness Reviewer signs off on the project as a whole; Concurrency Reviewer signs off on the threading model; the project ships only when all gates are clean.

**End of phase**: run the check-in protocol; the project is now shipped.

## Agent roster

Each link points to a file in `agents/`. Numbers 06 (Data Engineer) and 07 (Database Administrator) are skipped because Meridian has no persistence in v1. Total active agents: **18** (00 PM plus 17 specialists).

| # | Agent | One line mission |
|---|---|---|
| 00 | [Project Manager](agents/00-project-manager.md) | Owns the plan, sequences phases, runs brainstorming, dispatches every other agent. |
| 01 | [Quant Domain Validator](agents/01-quant-domain-validator.md) | Verifies the price-time priority semantics, ITCH 5.0 conformance, and matching invariants against textbook references. |
| 02 | [Market Microstructure Engineer](agents/02-market-microstructure-engineer.md) | Implements the matching engine semantics: every order type, the matching loop, the seqlock protocol. |
| 03 | [Engine Developer](agents/03-engine-developer.md) | Builds the C++ library and driver binaries (bench, replay, server). |
| 04 | [Frontend Developer](agents/04-frontend-developer.md) | Builds the React app, depth chart, trade tape, and WebSocket client. |
| 05 | [UI/UX Designer](agents/05-ui-ux-designer.md) | Wireframes, design tokens, interaction design, Twilight visual stewardship. |
| 08 | [Security Engineer](agents/08-security-engineer.md) | Threat model, hardening, WebSocket origin and rate limit policy, dependency scanning. |
| 09 | [DevOps Engineer](agents/09-devops-engineer.md) | CMake, CI/CD, VPS provisioning, Cloudflare Pages deployment, systemd service. |
| 10 | [QA Engineer](agents/10-qa-engineer.md) | Test plan, unit tests, property-based tests, integration tests, regression suite. |
| 11 | [Code Reviewer](agents/11-code-reviewer.md) | Reviews every PR for code quality, hot-path discipline, and idiom. |
| 12 | [Performance Engineer](agents/12-performance-engineer.md) | Profiles the matching loop, drives the 6M events per second benchmark, owns the latency budget. |
| 13 | [Accessibility Specialist](agents/13-accessibility-specialist.md) | a11y audit on the demo, WCAG AA compliance, keyboard and screen reader testing. |
| 14 | [Observability Engineer](agents/14-observability-engineer.md) | Logging discipline (level filtered out of hot path), metrics, error tracking on the live deploy. |
| 15 | [Documentation Engineer](agents/15-documentation-engineer.md) | README, setup guide, architecture docs, ADRs, benchmark report. |
| 16 | [Risk and Financial Correctness Reviewer](agents/16-risk-correctness-reviewer.md) | Validates that matching invariants are preserved and that a real exchange's behavior would be reproduced. |
| 17 | [Citation and Fact Auditor](agents/17-citation-and-fact-auditor.md) | Verifies every concrete claim in shipping documents traces to a code path, a measurement, or an external citation. |
| 18 | [Reference Implementation Engineer](agents/18-reference-implementation-engineer.md) | Owns the Python reference implementations used as ground truth for the C++ engine. |
| 19 | [Concurrency Reviewer](agents/19-concurrency-reviewer.md) | Reviews every diff touching threads, atomics, memory ordering, lock-free patterns; vetos seqlock changes. |

## Coordination rules

1. The Project Manager runs the brainstorming and planning skills before any code is written.
2. No phase begins until the prior phase has Security, QA, and Code Review sign off.
3. Every code change goes through the Code Reviewer before merge.
4. Security Engineer has veto power on any change that touches the WebSocket handshake, origin checks, rate limits, third party calls, or user input.
5. Risk and Financial Correctness Reviewer has veto power on any change that touches matching semantics, order types, cancel behavior, or the seqlock protocol. The matching engine's correctness is the project's primary value; correctness signoff is non-optional.
6. Performance Engineer has veto power on any change that affects the hot path's latency budget. A change that makes the matching loop slower is a regression, not progress.
7. Concurrency Reviewer has veto power on any change that touches threads, atomics, memory ordering, lock-free patterns, or the seqlock protocol. The Code Reviewer routes such PRs to this agent rather than approving them generically.
8. Citation and Fact Auditor must sign off on any document that contains a concrete number, a named external reference, a function name, a file path, or a claim about external behavior, before that document ships in a phase close. Hallucinated numbers and drifted documentation are the project's most embarrassing failure mode; this gate is non-optional.
9. Reference Implementation Engineer owns `tests/reference/`. The Python reference is the canonical "what should the C++ engine have produced?" answer for any disagreement caught by the integration tests or the property tests.
10. Documentation Engineer updates `docs/` whenever a phase completes.
11. Future ideas (anything out of scope for v1) are written to `future-ideas.md` at the repo root (gitignored, see `future-ideas.md` privacy note) and not implemented in v1.
12. **Mandatory check-in after every phase.** The PM session must stop, summarize what shipped, ask the user for current Max plan usage and reset window, and wait for explicit confirmation before starting the next phase. See `CLAUDE.md` "Pacing rule" for the exact protocol. This rule is not optional, even in auto mode.
