# Meridian setup guide

> **Status**: skeleton only. Each section below is a placeholder for the DevOps Engineer to fill in across phases. The bulk of this document lands in Phase 10 (Hosting and CI/CD), but earlier phases drop in their relevant sections as they ship. The Citation and Fact Auditor signs off on the final form at Phase 10 close. For the rolling per phase plan that says when each section gets written, see [`docs/plan.md`](plan.md).
>
> Audience: a developer with a clean machine and zero prior context who wants to clone the repo, build it, run it locally, and (eventually) deploy it. Every command in this guide should be reproducible end to end on a fresh Linux desktop in under 30 minutes.

## 1. Prerequisites

To be filled in by DevOps in Phase 0. Will list the toolchain versions (clang 19+ or gcc 14+, CMake 3.25+, Ninja, Python 3.11, pnpm via corepack, Node 20+) and the OS expectations.

## 2. Cloning the repo

To be filled in by DevOps in Phase 0. Will list the `git clone` command, the `git submodule init` command if vendored deps land that way, and the first build sanity check.

## 3. Building libmeridian and the bench binary (Phase 1+)

To be filled in by DevOps in Phase 1, once the CMake project has real sources. Will cover the configure step (`cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release`), the build step (`cmake --build build`), and a smoke test (`./build/apps/bench/meridian-bench --mode synthetic --duration 1`).

## 4. Running the unit and property test suite (Phase 1, expanded in Phase 3)

To be filled in by DevOps in Phase 1 (unit tests) and Phase 3 (property tests). Will cover `ctest --output-on-failure`, the rapidcheck case count knob, and how to run a single property test in isolation.

## 5. Running the Python reference implementation (Phase 1+)

To be filled in by DevOps and the Reference Implementation Engineer in Phase 1. Will cover the venv setup (`python3.11 -m venv .venv`), pip install of the `tests/reference/` requirements, and how to run the reference engine standalone (`python -m tests.reference.run_reference --tape sample.bin`).

## 6. Building and running meridian-replay (Phase 6)

To be filled in by DevOps and the Engine Developer in Phase 6. Will cover the replay binary's flags (`--tape`, `--speed`, `--symbols`, `--audit out.jsonl`) and how to diff its audit log against the Python reference's.

## 7. Building and running meridian-server locally (Phase 8)

To be filled in by DevOps and the Engine Developer in Phase 8. Will cover the `meridian.toml` config schema, port binding, allowed origin policy, and how to connect a manual WebSocket client (`websocat`) to verify the snapshot plus delta protocol.

## 8. Running the React frontend locally (Phase 9)

To be filled in by DevOps and the Frontend Developer in Phase 9. Will cover `pnpm install`, `pnpm dev`, the `VITE_WS_URL` environment variable, and how to point the dev server at a locally running `meridian-server`.

## 9. The benchmark workflow (Phase 5+)

To be filled in by DevOps and the Performance Engineer in Phase 5. Will cover the PGO build target, how to run `meridian-bench` with the same configuration CI uses, how to compare a local run against `bench/baseline.json`, and how to regenerate the baseline (the `regenerate baseline` workflow on `main`).

## 10. Deploying the frontend to Cloudflare Pages (Phase 10)

To be filled in by DevOps in Phase 10. Will cover creating the Cloudflare Pages project, wiring the GitHub repo, setting the build command and output directory, configuring the `VITE_WS_URL` environment variable, and verifying the deploy at `meridian-demo.pages.dev`.

## 11. Deploying meridian-server to Fly.io (Phase 10)

To be filled in by DevOps in Phase 10. Will cover the multi stage Dockerfile, the `fly.toml` machine config (auto stop on idle, auto start on first request, healthcheck), the `fly launch` (or `fly deploy`) command, picking the app name and region (default `iad`), and verifying the WebSocket endpoint at `wss://<fly-app>.fly.dev/ws`.

## 12. Wiring up GitHub Actions CI/CD (Phase 0 for `ci.yml`, Phase 5 for `bench.yml`, Phase 10 for `deploy.yml`)

To be filled in by DevOps as each workflow lands. Will cover the secrets configured in GitHub (`FLY_API_TOKEN`, `CLOUDFLARE_API_TOKEN`, `CLOUDFLARE_ACCOUNT_ID`), the protection rules on `main`, and how to manually trigger the bench workflow.

### 12.1 Branch protection on `main`

Applied 2026-05-09 via `gh api -X PUT repos/MustafaNazeer/Meridian/branches/main/protection` after PRs #3 and #4 ran `ci.yml` and registered the check names with GitHub. The rule is active and enforces:

1. **Require a pull request before merging.** Approvals: `1`. Dismiss stale pull request approvals when new commits are pushed: enabled. Require review from Code Owners: disabled (no `CODEOWNERS` file in v1).
2. **Require status checks to pass before merging.** Require branches to be up to date before merging: enabled. Required checks:
   * `engine (clang)`
   * `engine (gcc)`
   * `frontend`
3. **Require conversation resolution before merging.** Enabled.
4. **Require linear history.** Enabled (matches the squash merge convention in `CLAUDE.md`).
5. **Restrict who can push to matching branches.** No direct pushes to `main`; all changes flow through pull requests. Force pushes and branch deletion both blocked.

**Admin bypass posture.** `enforce_admins` is set to `false` because the legacy GitHub branch-protection API cannot express the per-rule admin carveout the project actually wants (admins should be able to bypass the "1 review required" rule on solo PRs since the rule is structurally unsatisfiable, but should never bypass required status checks or linear history). The discipline is enforced in `CLAUDE.md` instead: the standing `--admin` authorization is explicitly limited to the review-bypass case only and never extends to bypassing required CI checks or any other rule. If a future iteration needs the carveout enforced by the platform rather than by convention, migrate to GitHub Repository Rulesets, which support per-rule bypass actors.

When Phase 5 lands `bench.yml` (manual workflow), the bench job is added to the required-checks list only if the user explicitly requests it; bench runs are typically gated to manual triggers, so making it a required check would block merges on a workflow that does not auto-run. Update via `gh api -X PATCH repos/MustafaNazeer/Meridian/branches/main/protection/required_status_checks` with the augmented `contexts` array.

When Phase 8 lands the WebSocket smoke job inside `ci.yml`, that job's name is added to the required-checks list with the same `gh api` PATCH.

When Phase 10 lands `deploy.yml`, the deploy jobs are not required checks; they run after merge to `main`, not on PRs.

## 13. Rotating secrets and tokens (Phase 10)

To be filled in by Security Engineer and DevOps in Phase 10. Will cover the rotation schedule for the Fly API token, the Cloudflare Pages API token, and the link to `docs/security/secrets.md`.

## 14. Troubleshooting

To be filled in across phases as known failure modes are discovered. Candidate sections: TSAN false positives on the seqlock under unusual scheduler conditions, Fly cold start exceeding the 15 second budget, Cloudflare Pages preview deploys not picking up `VITE_WS_URL`, pnpm version mismatch via corepack.

## 15. Reproducing the benchmark report

To be filled in by Performance Engineer in Phase 11. Will cover the exact machine specs (CPU model, kernel version, governor setting, frequency pinning), the exact build command (PGO with both passes), the exact bench invocation, and how to regenerate `docs/perf/benchmark-report.pdf` from the JSON output.
