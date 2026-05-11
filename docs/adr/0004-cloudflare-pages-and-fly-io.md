# ADR 0004: Cloudflare Pages for the frontend, Fly.io for the engine

**Date**: 2026-05-10
**Status**: Accepted

## Context

The live demo of Meridian has two pieces that need to run somewhere publicly reachable: the React SPA (`frontend/`, the Twilight dashboard) and the `meridian-server` binary (the matching loop plus the seqlock sampler plus the hand rolled WebSocket server). The demo serves a one-way data feed to anonymous visitors; there is no auth, no persistence, and no monetary value at stake. The threat model in `docs/security/threat-model.md` sized the deploy posture accordingly.

The constraints that shaped this decision:

* **Cost**: the project is a portfolio demo, not a revenue-bearing service. Free-tier or near-free-tier hosting is a hard requirement; paying for an always-on VPS would be wasted spend.
* **Resume legibility**: the deploy targets should read as recognizable to a quant-systems hiring manager. Cloudflare and Fly.io are both well-known names in the relevant audience.
* **Cold-start tolerance**: the demo is fine with a 5 to 15 second cold start on the first visit after a quiet period. The frontend already renders an explicit "engine warming up" state for this window.
* **WebSocket support on the engine host**: required.
* **No platform-specific code in the engine**: the matching loop and the WebSocket server are pure C++ on Linux; the deploy target must accept a Dockerfile and not require a vendor SDK.
* **No platform-specific code in the frontend**: the SPA is a static build output; the deploy target must accept a `dist/` directory or hook into Vite's build.

There are several plausible combinations of frontend host plus engine host:

1. **Cloudflare Pages (frontend) + Fly.io machine (engine)**. The frontend deploys as static assets to Cloudflare's edge; the engine runs in a Fly machine that auto-stops when idle and auto-starts on the first request.
2. **Vercel (frontend) + Fly.io machine (engine)**. Vercel and Cloudflare Pages are roughly equivalent for a static SPA on the free tier. Vercel's free tier has a bandwidth cap that is lower than Cloudflare's, and Vercel pushes a Next.js framework lock-in that does not apply to a Vite SPA but adds friction to the deploy story.
3. **GitHub Pages (frontend) + Fly.io machine (engine)**. Cheapest, but GitHub Pages does not support custom HTTP headers (notably Content-Security-Policy) on a per-deploy basis, and the deploy URL `mustafanazeer.github.io/Meridian` is less legible on a resume than `meridian-orderbook.pages.dev`.
4. **One Fly.io machine that serves both the engine and the static SPA**. Simplest topology, but conflates two failure domains (a frontend deploy bug can knock the engine offline; an engine restart loses the static cache) and forfeits Cloudflare's edge for the frontend.
5. **AWS or GCP managed services**. More legitimate-feeling for some audiences, but the free tier is meaner, the deploy story is heavier (IAM, VPC, load balancers), and the cost-per-hour of an always-on micro-VM is non-zero.

The matching loop is single threaded with a target throughput of 6M events per second on a desktop reference machine. A shared-CPU 1x Fly machine (one vCPU thread, 256 MB RAM) does not hit that target; it does not need to. The throughput target is the benchmark binary's number, not the live demo's. The live demo runs the matching engine at a synthetic 50,000 events per second (configurable via `--rate`), which fits comfortably inside the shared CPU envelope and stays well inside Fly's free tier idle threshold.

## Decision

**Frontend**: Cloudflare Pages, project `meridian-orderbook`, default subdomain `meridian-orderbook.pages.dev`. No custom domain for v1. The deploy artifact is the Vite production bundle (`pnpm run build`'s `dist/` directory). The Cloudflare Pages action publishes on every push to `main` once the deploy workflow's trigger is flipped from `workflow_dispatch` to `push`.

**Engine**: Fly.io, app `meridian-engine`, primary region `iad` (Ashburn, Virginia). The deploy artifact is the `Dockerfile` at the repo root, built remotely by Fly's builder via `flyctl deploy --remote-only`. Machine class is `shared-cpu-1x` with 256 MB RAM. `auto_stop_machines = "stop"` and `auto_start_machines = true` mean the machine sleeps when idle and wakes on the first request; `min_machines_running = 0` keeps the always-on cost at zero.

**Wire shape**: the frontend's `VITE_MERIDIAN_WS` env var points at `wss://meridian-engine.fly.dev/ws`. The engine's Origin allowlist (configured via `--origins` in the Dockerfile entrypoint and `MERIDIAN_ORIGINS` in `fly.toml`) restricts `/ws` upgrades to the production Cloudflare Pages origin plus the localhost dev origin.

**CI/CD**: GitHub Actions, two workflows. `ci.yml` (already in place) runs the build matrix and tests on every PR. `deploy.yml` (new in this milestone) ships the frontend to Cloudflare Pages and the engine to Fly.io. For v1, `deploy.yml` is triggered manually via `workflow_dispatch` so the first merge does not run a deploy before secrets are provisioned. Once the first manual deploy succeeds, the trigger flips to `push: branches: [main]` so subsequent merges deploy automatically.

## Consequences

### Positive

* **Two independent failure domains**. A frontend deploy bug does not knock the engine offline, and an engine restart does not affect the static cache. Cloudflare's edge stays serving the SPA even when the Fly machine is asleep.
* **Free tier today, cheap tomorrow**. Cloudflare Pages free tier covers everything the demo needs (custom domain optional, 500 builds per month, unlimited bandwidth on the default subdomain). Fly's free tier covers a single 256 MB shared-CPU machine with auto-stop; the demo's idle cost is zero.
* **Cold-start UX is honest**. The frontend's `connecting` state already renders an explicit warming message during the 5 to 15 second Fly wake window, so the cold start reads as expected behaviour rather than as a broken demo.
* **The deploy story reads cleanly on a resume**. "React on Cloudflare Pages, C++ on Fly.io" is a sentence the audience parses in one beat.
* **Origin allowlist is enforced at the engine boundary**. The only places where the production frontend origin and the dev origin are listed are `fly.toml` (one line under `[env]`) and the threat model. Adding a new origin is a one-line edit to `fly.toml` plus a redeploy.
* **The Dockerfile is the source of truth for the engine runtime**. No environment-specific build steps, no platform-specific shim. The same Dockerfile that Fly builds remotely is the one a local developer can build with `docker build -t meridian-engine .`.

### Negative

* **Two vendor accounts to maintain**. Cloudflare and Fly each have their own dashboard, their own API token, their own quota model. The setup guide documents both, and the GitHub Actions secrets are colocated in one repo settings page, but the operational surface is wider than a one-vendor deploy.
* **Two API tokens are repo secrets**. `CLOUDFLARE_API_TOKEN` and `FLY_API_TOKEN` are in GitHub Actions. They are scoped to the deploy permissions only and revocable; the threat model section on the build pipeline covers their handling.
* **Fly free tier limits change**. Fly has, historically, adjusted free tier mechanics. The 256 MB shared-CPU envelope is comfortable today; a future tightening could force a move to a paid tier or a different host.
* **No HTTP/2 push or service worker on the engine side**. Fly's HTTP service routes WebSocket and plain HTTP fine; the demo does not need anything more sophisticated. Worth naming as a constraint in case a future feature wants it.

### Neutral

* **Region defaults to `iad`**. Ashburn is geographically near the user's audience and is one of the lower-latency Fly regions for US East Coast clients. The choice is reconfigurable in `fly.toml` with a single line.
* **The Cloudflare Pages project name and the Fly app name are public**. Both URLs are visible to anyone who reads the README or watches the demo. This is intentional; the demo is a public portfolio surface.

## Alternatives considered

* **Option 2 (Vercel + Fly)**: rejected because Vercel's free-tier bandwidth ceiling is lower than Cloudflare's and because Vercel's Next.js framing adds friction to a Vite SPA. Cloudflare Pages is a closer fit to "publish a Vite dist directory and forget about it."
* **Option 3 (GitHub Pages + Fly)**: rejected because GitHub Pages does not let the deploy set per-route CSP headers, and the `<account>.github.io/<repo>` URL is less legible than a Pages subdomain on a resume.
* **Option 4 (single Fly machine for everything)**: rejected because it conflates failure domains, forfeits the edge cache for static assets, and complicates the build (the Dockerfile would have to bake the frontend `dist/` into the runtime image, re-pulling pnpm on every engine change).
* **Option 5 (AWS or GCP managed services)**: rejected because the free tier is mean, the deploy story is heavy, and an always-on micro-VM costs more than zero per month even when idle.

A future milestone may introduce a custom domain (something like `meridian.mustafa.eng` or `demo.meridian.dev`) once the v1 deploy lands and stabilizes. Until then, the `pages.dev` and `fly.dev` defaults are the public URLs.
