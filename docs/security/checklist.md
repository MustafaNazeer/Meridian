# Meridian security checklist

**Last updated:** 2026-05-09
**Companion documents:** `threat-model.md`, `secrets.md`.

## How to use this file

Each milestone below has its own checklist section. Items are filled in as the milestone opens (review scope and add items the threat model requires) and checked off before the milestone closes. A milestone does not close while any of its security checklist items remain unchecked.

The Hosting and CI/CD section is the project's master hardening list and is filled in up front, since those items are the ones already known to be needed at the end. Earlier sections are stubs that get expanded when the relevant work opens.

Checkbox key: `[ ]` open, `[x]` complete, `[~]` not applicable for this milestone (with a one line justification).

## Foundations

* [x] Threat model authored at `docs/security/threat-model.md`.
* [x] Per-milestone checklist authored at `docs/security/checklist.md`.
* [x] Secrets reference authored at `docs/security/secrets.md`.
* [ ] Fact-check pass on the three security documents.

## Single-symbol matching

This is engine internals only; no networking, no untrusted input. Most security items are not applicable until a later milestone introduces a surface.

* [ ] Confirm the matching loop has no `new` or `malloc` (verified by the debug allocator override).
* [ ] Confirm no exceptions on the hot path; setup paths only.
* [~] No network surface yet. Not applicable.
* [~] No user input yet. Not applicable.

## Multi-instrument and cancel-by-id

* [ ] Confirm symbol lookup uses a fixed allow list at engine init (no dynamic symbol creation from external input until ITCH replay lands).
* [~] No network surface yet. Not applicable.

## Property-based tests for matching invariants

* [x] No new attack surface introduced. Hand rolled deterministic generators in `tests/property/`; no external PBT framework pulled in. The differential test shells out to `python3` via `std::system` to invoke the Python reference (the same call shape already used by `tests/integration/test_engine_vs_reference.cpp`); no other I/O.

## Seqlock-protected top-of-book and sampler

* [ ] Confirm the seqlock writer protocol does not introduce a torn read that a future WebSocket reader could observe and forward to the client unchecked.
* [ ] Confirm the sampler thread does not allocate on the hot path.

## Benchmark hits 6M events per second

* [~] No network surface. The bench binary is offline.
* [ ] Confirm the bench binary writes output only to stdout and to the configured CI artifact path, not to arbitrary user supplied paths.

## NASDAQ ITCH 5.0 replay

* [ ] ITCH parser uses bounds checked reads everywhere; no raw `memcpy` past validated lengths.
* [ ] ITCH parser rejects truncated frames and unknown message types loudly (not silently).
* [ ] Fuzz test the ITCH parser with libFuzzer, or with rapidcheck generators, against at least 60 minutes of CPU time on a representative workstation. Record the fuzz corpus and any findings under `docs/security/fuzz-itch-{date}.md`.
* [ ] Tape file integrity: the SHA-256 of every ITCH tape used in CI and the live demo is recorded in a tape manifest (e.g., `bench/tape-manifest.json`) and verified before use.
* [ ] Confirm the parser produces only POD `EngineEvent` structs; no callbacks, no virtual dispatch into untrusted code.

## Post-only and FOK order types

* [~] No new network surface. Engine internals only.
* [ ] Confirm post-only and FOK rejection paths do not leak distinguishing information (this is theoretical for a public read-only demo, but worth a one line review).

## WebSocket server and protocol

* [ ] Origin header is checked against the allow list (`https://meridian-demo.pages.dev` plus `http://localhost:5173` for dev). Anything else closes the socket before upgrade completes.
* [ ] Max inbound payload size is set to 64 KB. Frames exceeding the cap close the socket.
* [ ] Max client count is set, sized to the Fly machine memory budget. Excess connections rejected at handshake.
* [ ] Per IP connection rate limit on handshake attempts (e.g., 10 per minute) is implemented in code, not only in documentation.
* [ ] Inbound subscribe payload is parsed with simdjson into a strict schema; any deviation closes the socket.
* [ ] The symbol field on a subscribe message is matched against the compile time five symbol allow list before any book lookup.
* [ ] No `eval`, no runtime symbol lookup that creates a new book on demand.
* [ ] Server side audit ring buffer is bounded; once full, the oldest entries are overwritten in place. No file I/O on the hot path.
* [ ] Confirm the server emits no client IP, no headers, no PII to the audit ring buffer or to any log sink.
* [ ] Fact-check pass on `docs/api/websocket.md`.

## React frontend

* [ ] CSP `connect-src` allows only `'self'` plus the resolved Fly origin (`wss://<fly-app>.fly.dev`). Confirmed in the deployed `_headers` file.
* [ ] CSP `script-src` is `'self'`. No inline scripts, no `eval`. Confirmed in the deployed `_headers` file.
* [ ] CSP `style-src` is `'self'` plus `https://fonts.googleapis.com`; `font-src` is `https://fonts.gstatic.com`. No other third party origins.
* [ ] CSP `frame-ancestors 'none'`, plus `X-Frame-Options: DENY`, plus `X-Content-Type-Options: nosniff`, plus `Referrer-Policy: no-referrer`.
* [ ] Production build does not emit source maps (`build.sourcemap` is false in `vite.config.ts`); CI verifies the build artifact contains no `.map` files.
* [ ] No third party analytics, no error tracking SaaS, no telemetry script in the bundle. Verified by inspecting the production bundle output.
* [ ] No use of the React unsafe innerHTML escape hatch and no direct `innerHTML` writes anywhere in `frontend/src/`.
* [ ] Fact-check pass on any frontend documentation produced.

## Hosting and CI/CD

This is the master hardening list, captured up front so it does not have to be reconstructed at deploy time.

### TLS and headers

* [ ] HTTPS via Fly's edge: Fly issues and renews the TLS certificate for `<app>.fly.dev` automatically. Confirm the WebSocket upgrade is over `wss://`, not `ws://`.
* [ ] HSTS with `max-age` of at least 6 months on the Cloudflare Pages frontend. Confirmed via `curl -I https://meridian-demo.pages.dev` showing `strict-transport-security: max-age=15552000` or higher.
* [ ] CSP on the frontend: `connect-src` allows the Fly app origin (`wss://<app>.fly.dev`) plus `'self'`; `script-src` is `'self'`; `style-src` is `'self'` plus `https://fonts.googleapis.com`; `default-src` is `'none'`. Plus X-Frame-Options DENY and X-Content-Type-Options nosniff.
* [ ] Confirm the Cloudflare Pages `_headers` file actually applies the CSP and security headers to all routes, not only `/`. Verified via `curl -I` on `/` and on a deep route.

### Container hardening

* [ ] The Fly machine runs `meridian-server` as a non-root user inside the Docker image. Confirmed via `fly ssh console` reporting the user account and `id`.
* [ ] The non-root user has `/sbin/nologin` or `/usr/sbin/nologin` as its login shell. No interactive shell available.
* [ ] The Docker base image is pinned by digest, not by tag.

### Audit log posture

* [ ] The audit log is in-memory only on the live demo; confirm no client IPs are persisted to disk.
* [ ] Confirm `meridian-server` does not write any file under `/var/log` or any other persistent path on the Fly volume; only stdout / stderr and the in-memory ring buffer are used.

### Dependency audit

* [ ] Run `pnpm audit` against the frontend lockfile; resolve any high or critical advisories before deploy.
* [ ] Manual C++ dependency audit: compare the locked commit SHAs in CMake FetchContent declarations against upstream advisories for `uWebSockets`, `simdjson`, `glaze`, `GoogleTest`, `HDRHistogram-c`, and `Google Benchmark`. Record the audit at `docs/security/dependency-audit-{date}.md`. [citation needed for any advisory found]
* [ ] If any Python is present in `tests/` or in CI helpers, run `pip-audit` against its environment.

### Secret rotation

* [ ] Rotate `FLY_API_TOKEN` to a fresh value before going live publicly. Old token is revoked in the Fly dashboard.
* [ ] Rotate the Cloudflare Pages API token to a fresh value before going live publicly. Old token is revoked in the Cloudflare dashboard.
* [ ] Confirm both fresh tokens are present as encrypted GitHub Actions repository secrets, named `FLY_API_TOKEN` and `CLOUDFLARE_API_TOKEN` respectively.
* [ ] Confirm no secret value appears in the Cloudflare Pages build log or in any GitHub Actions log; check three recent runs.

### Build pipeline integrity

* [ ] Branch protection on `main` requires PRs and required CI checks. Direct pushes to `main` are blocked at the server side.
* [ ] CI workflow does not run `set -x`; deploy step does not echo secrets.
* [ ] CI runs `gitleaks` (or equivalent) as a required check; confirmed by inspecting the workflow file.
* [ ] CMake FetchContent declarations are pinned by commit SHA or by an immutable upstream release tag (release tags only; branch tags such as `main`, `master`, or `HEAD` are forbidden because they retarget). `grep -E 'GIT_TAG\s+(main|master|HEAD)' third_party/` returns no hits in production manifests.
* [ ] Verify the deploy workflow records the commit SHA it built from in its log output.

### Final live demo smoke test

* [ ] Visit `https://meridian-demo.pages.dev` from a clean browser profile. Confirm the page loads, the WebSocket connects to the Fly origin, and no third party origins appear in DevTools' Network tab.
* [ ] Run an SSL Labs (or equivalent) check on the Fly origin and on the Pages origin; both report a clean TLS configuration. [citation needed for the specific tool used]
* [ ] Confirm the cold start UX renders the "engine warming up" hero when the Fly machine has been idle.

## Polish, README, and benchmark report

* [ ] Confirm no new third party scripts, fonts, or analytics were introduced in the polish pass.
* [ ] Re-run the dependency audit (`pnpm audit` plus the manual C++ pass) one final time before publishing the README headline.
* [ ] Re-run the live demo smoke test from the hosting section once more on the final hosted environment.
* [ ] Fact-check pass on the final README and benchmark report.
* [ ] Confirm `docs/security/threat-model.md` and this checklist are accurate as of the shipping commit; bump the version line in the threat model if anything changed.
