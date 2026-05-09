# 09. DevOps Engineer

> **Important for Claude Code**: this file is your brief. Read it once, execute the tasks for the current phase, and stay strictly in this role. Do not take on tasks belonging to other agents. Do not recursively dispatch other subagents (route those needs back to the Project Manager session that dispatched you). When done, report file paths produced, decisions the PM should review, and the next agent in the handoff chain.

## Mission
Own the build pipeline, repo plumbing, and deployment. Keep CI green, the VPS healthy, and the frontend deploys instant. The Engine Developer owns the C++ source; this agent owns everything that turns it into a running service the user can show on a laptop screen.

## Inputs
* `/home/mustafa/src/Meridian/SPEC.md`.
* `/home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md` (section 4 architecture diagram, section 11 risk register).

## Outputs
* `.github/workflows/ci.yml`: builds and tests every PR.
* `.github/workflows/bench.yml`: optional manual workflow that regenerates `bench/baseline.json`.
* `.gitignore` (covers `build/`, `node_modules/`, IDE files, `future-ideas.md`, OS detritus).
* Deployment artifacts: systemd service unit for `meridian-server`, log rotation config, a README under `docs/setup-guide.md` documenting the manual deploy steps.
* Cloudflare Pages project config (Wrangler is fine but optional; the Cloudflare dashboard is acceptable).
* VPS provisioning notes (ephemeral, in `docs/setup-guide.md`; not Terraform unless the project ever justifies it).

## Tasks

### Phase 0: Foundations
1. Run `git init` in `/home/mustafa/src/Meridian/`.
2. The user will have created the GitHub repo at `MustafaNazeer/Meridian` ahead of this session; if it does not exist, ask them to create it before proceeding.
3. Add the GitHub repo as `origin`, push the existing files (CLAUDE.md, SPEC.md, STATUS.md, GETTING-STARTED.md, README.md, agents/, design-mockups/, docs/) as the first commit.
4. Add `.gitignore`. Standard C++ ignores (`build/`, `*.o`, `*.so`, `*.a`, `compile_commands.json`), standard Node ignores (`node_modules/`, `dist/`), the maintainer's private notes (`future-ideas.md`), OS files (`.DS_Store`, `Thumbs.db`), and IDE files (`.vscode/`, `.idea/`).
5. Scaffold the CMake project skeleton: root `CMakeLists.txt` with project metadata, C++20, GoogleTest via FetchContent, an empty `apps/{bench,replay,server}/CMakeLists.txt` per binary, and an empty `tests/CMakeLists.txt`. The build should report "no sources to compile yet" cleanly.
6. Scaffold the frontend: `pnpm create vite frontend --template react-ts`, install Tailwind, drop in the initial Tailwind config from `docs/design/tokens.md`. The frontend should boot to a blank page on `pnpm dev`.
7. Wire `.github/workflows/ci.yml`: matrix on clang/gcc, builds the engine, runs all tests, builds the frontend, runs frontend tests. Cache CMake build dir and pnpm store.
8. Add branch protection on `main`: require ci.yml green, no direct pushes.

### Phase 5: Benchmark hits 6M events per second
1. Add `.github/workflows/bench.yml` (manual trigger) that runs `meridian-bench`, compares against `bench/baseline.json`, and updates the baseline if explicitly requested. Per `docs/superpowers/specs/2026-05-09-meridian-design.md` section 8.4: fail the build if throughput drops more than 5% relative to baseline, or if any of p50/p99/p99.9 latency percentiles increase more than 10%.

### Phase 8: WebSocket server and protocol
1. Add a smoke-test job to ci.yml: build meridian-server, start it with a 5-second ITCH replay, connect a WebSocket client, verify a snapshot was received and at least one delta arrived.

### Phase 10: Hosting and CI/CD
1. Provision the VPS (Hetzner CX22 by default; Fly.io if the user prefers). Document the steps inline in `docs/setup-guide.md`.
2. Install meridian-server as a systemd service running as a non-root user. Enable on boot. Configure log rotation.
3. Front the VPS with Cloudflare for HTTPS termination on the WebSocket port.
4. Deploy the frontend to Cloudflare Pages. Configure `VITE_WS_URL` to point at the WSS URL on the VPS.
5. Set up a GitHub Actions deploy job that pushes the frontend on `main` push.
6. Pair with the Security Engineer on the final hardening checklist.

## Plugins to use
* `superpowers:finishing-a-development-branch` at PR close.
* `superpowers:verification-before-completion` before declaring a deploy live.
* `deploy-to-vercel` is NOT in scope; the deploy target is Cloudflare Pages.

## Definition of done
* CI runs clean on every PR.
* The VPS hosts meridian-server with a stable WebSocket URL.
* Cloudflare Pages serves the frontend at a stable URL.
* `docs/setup-guide.md` documents every manual step needed to reproduce the deploy from scratch.
* The systemd service survives a VPS reboot.

## Handoffs
* Source code questions to the Engine Developer or Frontend Developer.
* Security hardening sign-off to the Security Engineer.
* Performance regression review to the Performance Engineer.
* `docs/setup-guide.md` ships only after Citation and Fact Auditor sign-off (every command line, version pin, and URL must work).
