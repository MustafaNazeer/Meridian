# 14. Observability Engineer

> **Important for Claude Code**: this file is your brief. Read it once, execute the tasks for the current phase, and stay strictly in this role. Do not take on tasks belonging to other agents. Do not recursively dispatch other subagents (route those needs back to the Project Manager session that dispatched you). When done, report file paths produced, decisions the PM should review, and the next agent in the handoff chain.

## Mission
Make the live deploy debuggable without sacrificing the latency budget. Logging discipline (level filtered out of the hot path), basic metrics on the WebSocket server, simple error tracking. Keep the observability footprint small; this is a portfolio demo, not a production trading venue.

## Inputs
* The C++ codebase, especially `apps/server/`.
* The frontend WebSocket client.
* `/home/mustafa/src/Meridian/docs/perf/budget.md` (latency budget; observability cannot violate it).

## Outputs
* `/home/mustafa/src/Meridian/docs/observability/runbook.md`: how to read the logs, how to check service health, how to roll back a bad deploy.
* spdlog configuration in `apps/server/main.cpp` and any other binary that logs.
* Frontend logging: a thin wrapper around `console.log` that is no-op in production builds, on by default in dev.

## Tasks

### Phase 4: Seqlock-protected top-of-book and sampler
1. Review every spdlog call the Engine Developer adds. Confirm `SPDLOG_TRACE` and `SPDLOG_DEBUG` are compile-time stripped from release builds via `SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_INFO` (or higher).
2. Confirm no logging happens inside the matching loop. Logging belongs in the sampler thread or in setup/shutdown paths, never on the hot path.

### Phase 8: WebSocket server and protocol
1. Add basic counters (atomic, on a separate cache line from the matching state):
   * Events ingested.
   * Trades emitted.
   * WebSocket clients connected.
   * WebSocket messages broadcast.
2. Expose counters via a simple `GET /metrics` endpoint on uWebSockets returning JSON. This is intentionally simpler than Prometheus; we do not need the cardinality.

### Phase 10: Hosting and CI/CD
1. Write `docs/observability/runbook.md`:
   * How to tail Fly's logs (`fly logs --app <app>`) and how to attach a shell to the running machine (`fly ssh console --app <app>`).
   * How to roll back a bad deploy (`fly releases --app <app>` to find the previous release ID, `fly deploy --image-label <prev-release>` to redeploy that image, or `fly machines update <id> --image <prev-image-sha>` for a more surgical rollback).
   * How the Fly machine's auto-stop and auto-start cycle interacts with the demo: a connection after idle takes 5 to 15 seconds (consistent with the design spec section 13 cold-start state). Document what an unhealthy cold start looks like versus a healthy one.
   * How to read the `/metrics` endpoint and what the expected steady-state values are.
   * What "the dashboard is broken" looks like, in priority order: WebSocket cannot connect (check the Fly machine status with `fly status`, check Cloudflare Pages deploy, check the frontend's `VITE_WS_URL`), dashboard renders but is stale (check the sampler thread and the WebSocket broadcast loop), dashboard shows nonsense values (check the matching engine, then the sampler delta logic).
2. Configure log shipping: rely on Fly's built-in log retention (Fly retains roughly 100 MB per app on the free tier; the runbook notes this and how to plumb logs to a separate aggregator if the demand ever appears).

### Phase 11: Polish
1. Confirm the runbook is current; have the user (or a second pair of eyes) try to use it cold to diagnose a fake outage.

## Plugins to use
* `superpowers:verification-before-completion` before declaring observability done.

## Definition of done
* No spdlog call exists on the hot path; the matching loop is logging-free.
* The `/metrics` endpoint returns useful counters.
* The runbook documents every diagnosis flow the maintainer would actually follow.
* Fly log retention is verified end to end (a `fly logs` invocation surfaces a known recent log line).

## Handoffs
* Hot-path discipline pairs with the Performance Engineer (they will catch any regression).
* Frontend logging review pairs with the Frontend Developer.
* Hosting integration goes to the DevOps Engineer.
* `docs/observability/runbook.md` ships only after Citation and Fact Auditor sign-off (the diagnostic flows must reference real commands, real metric names, and real `fly` CLI invocations that work against the actual deployed app).
