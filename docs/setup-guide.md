# Meridian setup guide

> **Status**: skeleton only. Each section below is a placeholder, filled in as the relevant component lands. The bulk of this document lands alongside hosting setup, but earlier milestones drop in their relevant sections as they ship.
>
> Audience: a developer with a clean machine and zero prior context who wants to clone the repo, build it, run it locally, and (eventually) deploy it. Every command in this guide should be reproducible end to end on a fresh Linux desktop in under 30 minutes.

## 1. Prerequisites

To be filled in. Will list the toolchain versions (clang 19+ or gcc 14+, CMake 3.25+, Ninja, Python 3.11, pnpm via corepack, Node 20+) and the OS expectations.

## 2. Cloning the repo

To be filled in. Will list the `git clone` command, the `git submodule init` command if vendored deps land that way, and the first build sanity check.

## 3. Building libmeridian and the bench binary

Will cover the configure step (`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`), the build step (`cmake --build build`), and a smoke test (`./build/apps/bench/meridian-bench --mode synthetic --duration 1`).

## 4. Running the unit and property test suite

Will cover `ctest --output-on-failure`, the per-test `kCases` knob in `tests/property/`, and how to run a single property test in isolation.

## 5. Running the Python reference implementation

Will cover the venv setup (`python3.11 -m venv .venv`), pip install of the `tests/reference/` requirements, and how to run the reference engine standalone (`python -m tests.reference.run_reference --tape sample.bin`).

## 6. Building and running meridian-replay

Will cover the replay binary's flags (`--tape`, `--speed`, `--symbols`, `--audit out.jsonl`) and how to diff its audit log against the Python reference's.

## 7. Building and running meridian-server locally

Will cover the `meridian.toml` config schema, port binding, allowed origin policy, and how to connect a manual WebSocket client (`websocat`) to verify the snapshot plus delta protocol.

## 8. Running the React frontend locally

Will cover `pnpm install`, `pnpm dev`, the `VITE_WS_URL` environment variable, and how to point the dev server at a locally running `meridian-server`.

## 9. The benchmark workflow

Will cover the PGO build target, how to run `meridian-bench` with the same configuration CI uses, how to compare a local run against `bench/baseline.json`, and how to regenerate the baseline (the `regenerate baseline` workflow on `main`).

## 10. Deploying the frontend to Cloudflare Pages

The frontend deploys as a static Vite bundle to Cloudflare Pages. The Pages project name is `meridian-orderbook`; the default subdomain is `meridian-orderbook.pages.dev`. No custom domain in v1.

There are two equivalent paths for the first deploy: the GitHub Actions workflow at `.github/workflows/deploy.yml` (recommended, reproducible) and the Cloudflare dashboard "Connect to Git" flow (fastest first-time setup, but less reproducible).

### 10.1 Prerequisites

1. A Cloudflare account. Free tier is sufficient.
2. A Cloudflare API token with `Account · Cloudflare Pages · Edit` scope. Create one at `https://dash.cloudflare.com/profile/api-tokens` and save it; it is shown only once.
3. The Cloudflare account ID. Visible at the right of any zone in `https://dash.cloudflare.com`.

### 10.2 Create the Pages project

Use the dashboard: `https://dash.cloudflare.com -> Workers and Pages -> Create -> Pages -> Direct Upload`. Name it `meridian-orderbook`. Do not configure a build command in the Pages UI; the GitHub Actions workflow builds the bundle and uploads `frontend/dist/` via `cloudflare/pages-action@v1`. The Pages project is just the publication target.

### 10.3 Configure GitHub Actions secrets

In `https://github.com/MustafaNazeer/Meridian/settings/secrets/actions` add:

* `CLOUDFLARE_API_TOKEN`: the token from step 10.1.
* `CLOUDFLARE_ACCOUNT_ID`: the account ID from step 10.1.

### 10.4 Run the first deploy

Go to `https://github.com/MustafaNazeer/Meridian/actions/workflows/deploy.yml`, click `Run workflow`, pick `frontend`, run. The job builds the Vite bundle with `VITE_MERIDIAN_WS=wss://meridian-engine.fly.dev/ws` (set in the workflow env) and uploads the dist directory to Cloudflare Pages.

### 10.5 Verify

`curl -I https://meridian-orderbook.pages.dev` should return `200 OK`. Open the URL in a browser; the dashboard renders the `connecting` state and the connection state machine drives `connecting -> live` once the engine is reachable.

## 11. Deploying meridian-server to Fly.io

The engine runs on a single Fly machine that auto-stops when idle and auto-starts on the first WebSocket connection. The app name is `meridian-engine`; the primary region is `iad`. Machine class is `shared-cpu-1x` with 256 MB RAM, which is plenty for the synthetic event stream at 50,000 events per second (the headline 6M events per second is a benchmark target on a desktop reference machine, not a live-demo number).

### 11.1 Prerequisites

1. A Fly.io account. Free tier covers one machine of this size.
2. `flyctl` installed locally: `curl -L https://fly.io/install.sh | sh`. (See `https://fly.io/docs/flyctl/install/` for alternates.)
3. `flyctl auth login` once to authenticate the CLI.

### 11.2 Claim the app name

```bash
cd /path/to/Meridian
flyctl apps create meridian-engine
```

If the name is taken, edit `fly.toml`, `frontend/.env.production`, and `.github/workflows/deploy.yml` to a free alternative (for example `meridian-orderbook`), then rerun the create command.

### 11.3 First deploy

```bash
flyctl deploy --remote-only
```

`--remote-only` ships the build context to Fly's remote builder so the local machine does not need Docker. The Dockerfile at the repo root builds a multi-stage image (debian:trixie-slim, gcc, ninja for the builder; minimal runtime with `libstdc++6` and a non-root user for the runtime). First build runs roughly 60 to 120 seconds end to end.

### 11.4 Configure GitHub Actions secrets for subsequent deploys

In `https://github.com/MustafaNazeer/Meridian/settings/secrets/actions` add:

* `FLY_API_TOKEN`: generate via `flyctl auth token` and copy the output.

### 11.5 Verify

```bash
curl https://meridian-engine.fly.dev/healthz
# {"status":"ok"}

curl https://meridian-engine.fly.dev/metrics
# {"connections_total":0,...}
```

Open the deployed frontend in a browser. The connection state machine should walk `connecting -> live` within roughly 5 to 15 seconds (the cold-start window the hero copy already names). The engine's Origin allowlist (set via `MERIDIAN_ORIGINS` in `fly.toml`) restricts `/ws` upgrades to `https://meridian-orderbook.pages.dev` and `http://localhost:5173`; any other origin gets `HTTP 403`. Confirm the lockdown is in force with:

```bash
# Upgrade attempt from an unlisted origin should return 403:
curl -s -i \
  -H "Connection: Upgrade" \
  -H "Upgrade: websocket" \
  -H "Sec-WebSocket-Version: 13" \
  -H "Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==" \
  -H "Origin: https://attacker.example.com" \
  https://meridian-engine.fly.dev/ws | head -1
# HTTP/2 403
```

### 11.6 Subsequent deploys

After the first deploy and once `FLY_API_TOKEN` is configured, either:

* Run the GitHub Actions workflow at `https://github.com/MustafaNazeer/Meridian/actions/workflows/deploy.yml` with target `engine` or `both`.
* Run `flyctl deploy --remote-only` from a local checkout on `main`.

When the deploy cadence stabilizes, flip the `on:` block in `.github/workflows/deploy.yml` from `workflow_dispatch` to `push: branches: [main]` so subsequent merges deploy automatically.

## 12. Wiring up GitHub Actions CI/CD

Will cover the secrets configured in GitHub (`FLY_API_TOKEN`, `CLOUDFLARE_API_TOKEN`, `CLOUDFLARE_ACCOUNT_ID`), the protection rules on `main`, and how to manually trigger the bench workflow.

### 12.1 Branch protection on `main`

Applied 2026-05-09 via `gh api -X PUT repos/MustafaNazeer/Meridian/branches/main/protection` after the first PRs ran `ci.yml` and registered the check names with GitHub. The rule is active and enforces:

1. **Require a pull request before merging.** Approvals: `1`. Dismiss stale pull request approvals when new commits are pushed: enabled. Require review from Code Owners: disabled (no `CODEOWNERS` file in v1).
2. **Require status checks to pass before merging.** Require branches to be up to date before merging: enabled. Required checks:
   * `engine (clang)`
   * `engine (gcc)`
   * `frontend`
3. **Require conversation resolution before merging.** Enabled.
4. **Require linear history.** Enabled (matches the squash merge convention used on this repo).
5. **Restrict who can push to matching branches.** No direct pushes to `main`; all changes flow through pull requests. Force pushes and branch deletion both blocked.

**Admin bypass posture.** `enforce_admins` is set to `false` because the legacy GitHub branch-protection API cannot express the per-rule admin carveout the project actually wants (admins should be able to bypass the "1 review required" rule on solo PRs since the rule is structurally unsatisfiable, but should never bypass required status checks or linear history). The discipline is enforced by convention instead: the standing `--admin` authorization is limited to the review-bypass case only and never extends to bypassing required CI checks or any other rule. If a future iteration needs the carveout enforced by the platform rather than by convention, migrate to GitHub Repository Rulesets, which support per-rule bypass actors.

When `bench.yml` (manual workflow) lands, the bench job is added to the required-checks list only on explicit request; bench runs are typically gated to manual triggers, so making it a required check would block merges on a workflow that does not auto-run. Update via `gh api -X PATCH repos/MustafaNazeer/Meridian/branches/main/protection/required_status_checks` with the augmented `contexts` array.

When the WebSocket smoke job lands inside `ci.yml`, that job's name is added to the required-checks list with the same `gh api` PATCH.

When `deploy.yml` lands, the deploy jobs are not required checks; they run after merge to `main`, not on PRs.

## 13. Rotating secrets and tokens

Will cover the rotation schedule for the Fly API token, the Cloudflare Pages API token, and the link to `docs/security/secrets.md`.

## 14. Troubleshooting

To be filled in as known failure modes are discovered. Candidate sections: TSAN false positives on the seqlock under unusual scheduler conditions, Fly cold start exceeding the 15 second budget, Cloudflare Pages preview deploys not picking up `VITE_WS_URL`, pnpm version mismatch via corepack.

## 15. Reproducing the benchmark report

Will cover the exact machine specs (CPU model, kernel version, governor setting, frequency pinning), the exact build command (PGO with both passes), the exact bench invocation, and how to regenerate `docs/perf/benchmark-report.pdf` from the JSON output.
