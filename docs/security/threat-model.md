# Meridian threat model

**Version:** 0.1 (Phase 0 baseline)
**Last updated:** 2026-05-09
**Owner:** Security Engineer (agent 08)
**Methodology:** STRIDE per attack surface, supplemented by deployment surface notes.

## Purpose and scope

Meridian is a public, read-only portfolio demo of a price-time priority limit order book and matching engine. The live demo at `meridian-demo.pages.dev` lets any visitor watch a deterministic NASDAQ ITCH 5.0 replay against the C++ matching engine in real time over WSS. There is no authentication, no user submitted orders, no persistence, and no monetary value at stake. The threat model is sized accordingly: the goal is a demo that is hard to embarrass the author with, not a regulated trading venue.

This document covers four attack surfaces that exist in the deployed system as resolved on 2026-05-09 (design spec section 13):

1. The WebSocket endpoint exposed by `meridian-server` running on a Fly.io machine (default region `iad`, app name resolved at Phase 10).
2. Static asset serving from Cloudflare Pages at `meridian-demo.pages.dev`, plus any static files served directly by the Fly machine.
3. The replay tape file consumed by `meridian-replay` and `meridian-server` (trusted input, but the assumption is made explicit here).
4. The build and deploy pipeline (GitHub Actions, Cloudflare Pages deploy hook, `flyctl` deploy), including dependency provenance for the C++ libraries pulled via CMake FetchContent.

Out of scope for this threat model:

* The local developer workstation. The user is responsible for laptop hygiene; nothing in this project assumes the workstation is compromised, but nothing in this model defends against that case either.
* The `meridian-bench` binary in isolation, since it has no networking and runs only on developer or CI hardware.
* Anything Vega specific. Vega is a separate project; cross project assumptions are not made.

## Component overview

The components here track the design spec section 5 and section 13 resolved decisions. Numbering matches the design spec where possible.

| Component | Lives at | Trust boundary | Notes |
|---|---|---|---|
| C5.1 `libmeridian.a` | Linked into all three driver binaries | Internal | Deterministic; no I/O of its own. |
| C5.2 `meridian-bench` | Developer or CI hardware | Internal | No network. Out of scope for this threat model. |
| C5.3 `meridian-replay` | Developer hardware | Internal | Reads a tape file, writes JSONL to stdout. |
| C5.4 `meridian-server` | Fly.io machine, Dockerfile plus `fly.toml`, region default `iad` | Public network boundary | Hosts the matching loop, the seqlock-protected snapshot, the sampler, and uWebSockets. Auto-stops when idle, auto-starts on first request. |
| C5.5 React web client | Cloudflare Pages, `meridian-demo.pages.dev` | Public network boundary | Static SPA. No backend code on Pages. |
| Build pipeline | GitHub Actions in `MustafaNazeer/Meridian` | Mixed (public repo, private secrets) | Builds, tests, deploys. Holds `FLY_API_TOKEN` and the Cloudflare Pages API token as repo secrets. |

References: design spec section 5 (components), section 13 (resolved decisions on hosting domain, backend host, demo symbol set, audit log), `SPEC.md` Phase 8 (WebSocket protocol), Phase 10 (hosting and CI/CD).

## Trust boundaries

1. **Public internet to Cloudflare Pages edge.** Cloudflare Pages serves the static React SPA. TLS is terminated at Cloudflare's edge. Cloudflare Pages does not run any first party code on our behalf; the SPA is plain HTML, JS, and CSS produced by `pnpm --filter frontend build`.
2. **Public internet to Fly.io edge.** The Fly.io edge proxies WSS to the `meridian-server` container running on a Fly machine. Fly issues and renews the TLS certificate for `<app>.fly.dev` automatically. The WSS upgrade terminates at Fly's edge and is forwarded to `meridian-server` over the internal private network.
3. **Fly.io machine to libmeridian.** Inside the Fly machine, `meridian-server` runs as a non-root user inside a minimal Docker image. The matching engine and the uWebSockets event loop share an address space; this is by design (single-process is the architecture) and is not a meaningful trust boundary on its own.
4. **GitHub repository to GitHub Actions.** The repository is public. Actions workflows execute with secrets available only to workflows triggered from `main` and from PRs in the same repository (the GitHub default for the `secrets:` context on `pull_request_target` is intentionally not used). Forked PRs do not get access to secrets, which means CI for forked PRs cannot deploy.
5. **GitHub Actions to Fly.io and Cloudflare Pages.** The `FLY_API_TOKEN` and the Cloudflare Pages API token are stored as encrypted repository secrets. They are exposed to workflow steps only on workflows that run against `main` or against PRs from the same repository.

## Assets

* The Fly.io account credentials (account password and 2FA, which are not in the repo).
* The `FLY_API_TOKEN` GitHub Actions secret (deploy permission to Fly).
* The Cloudflare Pages API token (deploy permission to the Pages project).
* The repository contents and CI configuration (public, but write access is restricted to the user).
* The reputation of the demo. Defacement, redirection, or visible compromise would be embarrassing in front of the recruiter audience the demo targets.

There is no PII, no user data, no monetary value, and no proprietary algorithm at stake. The matching engine is open source MIT.

## STRIDE analysis by surface

### Surface 1: WebSocket endpoint at `wss://<fly-app>.fly.dev/ws`

The endpoint is publicly reachable from any client. On connect, the client receives a `snapshot` message and thereafter `delta` messages at 30 Hz. There is no authentication. The client cannot submit orders; the protocol is read-only. References: design spec section 5.4 and 5.5, `SPEC.md` Phase 8.

| STRIDE | Threat | Mitigation | Status |
|---|---|---|---|
| Spoofing | An attacker hosts a clone at `wss://attacker.example/ws` and persuades a victim to connect. | Out of scope: the demo is read only and unauthenticated, so the only impact is the attacker fabricating fake market data for their own visitors. The Cloudflare Pages frontend is hardcoded to the resolved Fly origin via `VITE_WS_URL`. CSP `connect-src` on the frontend is locked to that origin. | Phase 10 hardening enforces this. |
| Tampering | A man in the middle modifies WebSocket frames. | TLS is mandatory: the server only accepts WSS, the frontend's `VITE_WS_URL` is `wss://...`, and Fly's edge enforces HTTPS via automatic certificate provisioning. HSTS at the Cloudflare frontend prevents protocol downgrade for return visitors. | Phase 10 hardening. |
| Repudiation | Not applicable. The demo is read only and unauthenticated. There is nothing for a user to repudiate. | None required. | Accepted. |
| Information disclosure | Server inadvertently leaks data on the wire. | The WebSocket protocol streams only public synthetic and ITCH replay derived order book state. There is no PII, no client IP, no internal config. The audit log per section 13 is in memory only and contains only `EngineEvent` and `ExecutionReport` records, all of which are derived from the deterministic public ITCH replay. | Phase 8 review. |
| Denial of service | Out of scope per the threat model exclusion list (demo is best effort, free tier). The Phase 10 hardening checklist still enforces a max client count and per IP connection rate limit because they are cheap, but DoS is not modeled as a primary threat. | Implemented as defense in depth, not as a primary mitigation. | Phase 8 hardening. |
| Elevation of privilege | A WebSocket message triggers a code path that escalates inside `meridian-server`. | The server protocol is one way: it sends `snapshot` and `delta` to clients, and ignores all client to server payloads except a subscribe message naming a symbol from the fixed five symbol set (AAPL, SPY, NVDA, TSLA, GOOG). The subscribe handler validates the symbol against an allow list before dispatching; an unknown symbol is dropped. There is no command interface, no eval, no dynamic code path. | Phase 8 implementation must enforce the allow list in code, not only in documentation. |

Specific Phase 8 hardening requirements (these become checklist items, see `checklist.md`):

* Origin header check against an allow list containing the Cloudflare Pages production origin (`https://meridian-demo.pages.dev`) and `http://localhost:5173` for dev. Anything else closes the socket before the upgrade completes.
* Max payload size of 64 KB on inbound frames. The protocol's largest expected inbound message is a subscribe naming one of five symbols, well under 100 bytes.
* Max client count sized to the Fly machine memory budget. Hundreds, not thousands. Excess connections are rejected at handshake.
* Per IP connection rate limit on handshake attempts (e.g., 10 per minute), implemented in uWebSockets accept callback or via a small token bucket in `apps/server/`.
* Subscribe payload is parsed with simdjson into a strict schema; any deviation closes the socket.
* The symbol field is matched against the compile time symbol allow list before any book lookup.
* Server side audit ring buffer is bounded; once full, the oldest entries are overwritten in place. No file I/O on the hot path.

### Surface 2: Static asset serving

Two static asset surfaces exist:

1. The React SPA on Cloudflare Pages at `meridian-demo.pages.dev`. This is the primary surface visitors hit.
2. The Fly machine's HTTP listener may also serve a small set of static files (e.g., a placeholder `/` page) so that operators visiting the Fly URL directly do not get a blank response. The design intent is that the React app lives on Cloudflare; the Fly origin is reached only over WSS. If we keep a static `/` on Fly, it must be a single page with no JavaScript and no inline event handlers.

| STRIDE | Threat | Mitigation | Status |
|---|---|---|---|
| Spoofing | Attacker stands up `meridian-deemo.pages.dev` and pretends to be us. | Out of scope (squatting on a Cloudflare Pages subdomain). The README will name the canonical URL. | Accepted. |
| Tampering | Attacker injects JavaScript via reflected user input. | The SPA does not read URL parameters, form input, or any user controlled content into the DOM via the React unsafe innerHTML escape hatch or via direct `innerHTML` writes. All rendered text is data from the deterministic WebSocket stream, which is itself synthetic or ITCH derived. CSP `script-src 'self'` denies inline and external scripts. | Phase 9 review. |
| Repudiation | Not applicable. | None required. | Accepted. |
| Information disclosure | Source maps or debug builds leak unintended code. | Production builds (`pnpm build`) do not emit source maps. The Vite config explicitly sets `build.sourcemap` to `false` for production. CI verifies this in the build artifact step. | Phase 9 implementation must enforce. |
| Denial of service | Out of scope per exclusion list. Cloudflare Pages absorbs trivial flooding via its CDN. | None at the application level. | Accepted. |
| Elevation of privilege | A static asset triggers code execution in the browser that bypasses CSP. | CSP locked to `default-src 'none'; script-src 'self'; style-src 'self' https://fonts.googleapis.com; font-src https://fonts.gstatic.com; connect-src 'self' wss://<fly-app>.fly.dev; img-src 'self' data:; frame-ancestors 'none'`. X-Frame-Options DENY. X-Content-Type-Options nosniff. Referrer-Policy `no-referrer`. | Phase 10 hardening. |

The Cloudflare Pages frontend cannot set HTTP response headers via plain Pages output; the `_headers` file (Cloudflare Pages convention) is the mechanism. The Phase 10 hardening checklist requires checking that the deployed `_headers` file actually applies the CSP and security headers to all routes.

### Surface 3: Replay tape file

The matching engine consumes ITCH 5.0 binary tapes via the parser at `include/meridian/itch.hpp`. The tapes used in development and on the live demo are public NASDAQ samples committed to the repository or downloaded at build time from a known URL.

| STRIDE | Threat | Mitigation | Status |
|---|---|---|---|
| Spoofing | An attacker substitutes a malicious tape file. | The tape source is trusted: either checked into the repo at a known commit or downloaded from a NASDAQ owned URL. The tape's SHA-256 is recorded in `bench/baseline.json` (or an analogous tape manifest) so a CI step verifies the hash before use. | Phase 6 implementation must record the hash; Phase 10 hardening verifies CI does the check. |
| Tampering | A bug in the ITCH parser is exploited via a crafted input. | The parser is hand rolled and intentionally minimal, decoding only the subset of ITCH 5.0 messages we use. The Phase 6 plan calls for fuzz testing the parser with libFuzzer (or `rapidcheck` generators if libFuzzer is awkward to integrate) before the parser is exposed to anything other than known good NASDAQ samples. The parser must use bounds checked reads (no raw memcpy past the buffer end); abort on truncated frames; reject unknown message types loudly. | Phase 6 implementation. Until then, the assumption "tape input is trusted" is documented and enforced by code review. |
| Repudiation | Not applicable. | None required. | Accepted. |
| Information disclosure | The parser logs message contents containing sensitive data. | ITCH content is public market data. There is nothing sensitive to disclose. spdlog level is filtered out of the release hot path. | Accepted. |
| Denial of service | Out of scope per exclusion list. | None at the application level. | Accepted. |
| Elevation of privilege | A crafted ITCH message yields code execution inside `meridian-server`. | Same mitigation as Tampering: bounds checks, fuzz testing in Phase 6, no raw pointer arithmetic past validated lengths, no calls into a deserialization library that supports type instantiation. The parser produces POD `EngineEvent` structs only. | Phase 6 implementation must demonstrate fuzz coverage before sign off. |

### Surface 4: Build and deploy pipeline

The pipeline includes:

* The public GitHub repository `MustafaNazeer/Meridian`.
* GitHub Actions workflows under `.github/workflows/`.
* CMake `FetchContent` declarations that pull C++ dependencies (uWebSockets, simdjson, glaze, rapidcheck, GoogleTest, Google Benchmark) at configure time.
* `pnpm` lockfile pinning frontend dependencies.
* `flyctl` invoked from a workflow step to deploy `meridian-server`.
* The Cloudflare Pages deploy, triggered either by the Cloudflare Pages GitHub integration or by a `wrangler` step in a workflow.

| STRIDE | Threat | Mitigation | Status |
|---|---|---|---|
| Spoofing | An attacker pushes a malicious commit and forces a deploy. | Branch protection on `main` requires PRs and required CI checks. Direct pushes to `main` are blocked; the user uses `gh pr merge --admin` only after green CI. The PR flow is enforced by branch protection plus the local pre-commit hook described in `CLAUDE.md`. | Phase 0 / Phase 10. |
| Tampering | A FetchContent dependency is replaced upstream. | Each FetchContent declaration pins a specific commit SHA, not a branch or tag. CI fetches by SHA, so upstream tag retargeting is not exploitable. SBOM generation is a Phase 10 deliverable; until it lands, manual upstream advisory checks at Phase 10 cover known issues. [citation needed for any specific advisory] | Phase 0 documents the convention; Phase 10 verifies and produces an SBOM. |
| Repudiation | A malicious workflow run modifies the deployed binary. | All deploys are tied to a commit SHA recorded by Fly's release API and Cloudflare Pages's build log. The deploy workflow logs the SHA it built from. | Phase 10 verifies the workflow logs the SHA. |
| Information disclosure | Workflow logs leak `FLY_API_TOKEN` or the Cloudflare token. | Both are stored as encrypted repository secrets. GitHub redacts known secret values from logs automatically. The deploy step does not echo secrets. The workflow does not run `set -x`. | Phase 10 hardening checks the workflow files. |
| Denial of service | Out of scope. | None at the pipeline level. | Accepted. |
| Elevation of privilege | A compromised dependency runs malicious code at build time. | C++ FetchContent dependencies are pinned by commit SHA. `pnpm` uses a locked manifest. CI runs `pnpm audit` at Phase 10 and fails the build on any high or critical advisory; for C++ deps, the Phase 10 hardening checklist requires a manual upstream advisory pass against the pinned SHAs. The Docker base image for the Fly machine is a minimal Debian or Alpine image pinned by digest, not by tag. | Phase 0 documents conventions; Phase 10 verifies. |

## Cross cutting controls

These controls apply across all four surfaces and are tracked in `checklist.md`:

* **TLS everywhere.** No plaintext HTTP and no `ws://` outside of `localhost` development. Fly's edge handles TLS for `<app>.fly.dev`; Cloudflare handles TLS for `<project>.pages.dev`. HSTS with `max-age` of at least six months on the Cloudflare frontend.
* **No secrets in the repo.** Secrets are stored in GitHub Actions repository secrets and in Fly app secrets. The repo is public; nothing in `git ls-files` should ever match a secret pattern. `gitleaks` runs in CI as a required check.
* **Non-root container.** The Dockerfile creates a `meridian` user with `/sbin/nologin` (or `/usr/sbin/nologin`, depending on base image), copies only the binary plus its runtime data, and runs as that user via `USER meridian`. No shell needed.
* **No analytics, no telemetry, no third party scripts.** The frontend has no Google Analytics, no Sentry, no Plausible, no New Relic, no error tracking SaaS. The only third party origin allowed is Google Fonts (for Inter, Newsreader, JetBrains Mono), and only because the Twilight visual identity uses those typefaces. CSP enforces this.
* **No client side IP logging.** Per design spec section 13, the audit log on the live demo is in memory only and contains only deterministic engine events. Client IPs are not collected, not displayed, not persisted.
* **Deterministic builds.** CMake build flags, FetchContent SHAs, the Docker base image digest, and the `pnpm` lockfile are all pinned; rebuilding the same commit on the same toolchain produces equivalent binaries. This is a defense in depth against supply chain compromise.

## Open assumptions and `[citation needed]` markers

* **uWebSockets advisory history.** No known CVEs at the pinned version are cited in this document; the Phase 10 hardening pass will verify upstream advisories against the pinned SHA. [citation needed]
* **Fly.io free tier behavior.** The "auto-stop when idle, auto-start on first request" semantics are documented by Fly; the exact wake latency is observed empirically (5 to 15 seconds is the figure in the design spec section 13 cold start UX, derived from the user's own observation, not from a Fly published guarantee). [citation needed]
* **Cloudflare Pages security headers.** The mechanism for delivering CSP and HSTS via Cloudflare Pages is the `_headers` file convention. The Phase 10 hardening checklist verifies this works for our routes. [citation needed]

The Citation and Fact Auditor will run after this document is merged and will either resolve each `[citation needed]` marker or convert it into a footnote that names the verifying source.

## Severity summary

No open critical or high severity findings exist as of Phase 0 (no code shipped yet). The threat model is forward looking: it lists the controls that Phase 6, Phase 8, Phase 9, and Phase 10 must implement before each respective phase can close.

The phase gating model in `SPEC.md` and `CLAUDE.md` already requires Security sign off before each phase advances; this threat model is the criterion document for those sign offs.

## Revision history

* 2026-05-09: v0.1 baseline. Authored at Phase 0. Reflects the resolved deployment stack from design spec section 13 (Cloudflare Pages plus Fly.io, not VPS plus systemd). Subject to Citation and Fact Auditor review before sign off.
