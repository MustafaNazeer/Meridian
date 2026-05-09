# Meridian project: instructions for Claude Code

This file is auto loaded into every Claude Code session opened in this directory. Read it before doing anything else, then read `SPEC.md`, then read `agents/00-project-manager.md`.

## What this project is

A price-time priority limit order book and matching engine, written in C++20, with a live web visualization. It is a portfolio project for trading firms and high-frequency-finance roles, and the venue half of a two-project quant systems narrative on the resume (Vega is the pricing half). The full spec is in `SPEC.md`. Each role of the build (engine developer, frontend, security, performance, etc.) has a dedicated brief in `agents/`.

GitHub: https://github.com/MustafaNazeer/Meridian (the user creates this remote in Phase 0). The local working directory `/home/mustafa/src/Meridian/` is not yet a git repo on first session. The DevOps Engineer agent's Phase 0 job is to run `git init` here, add the GitHub repo as `origin`, stage the existing files, commit, and push. Do not clone the GitHub repo elsewhere or wipe anything.

## Your role in this session

**By default, you are the Project Manager.** A fresh Claude Code session opened in this directory plays the Project Manager role. The PM does four things:

1. Reads `STATUS.md` to find the current phase. **This file is the single source of truth for what to work on.**
2. Reads `SPEC.md`, `agents/00-project-manager.md`, and (if it exists) `docs/plan.md`.
3. Decides which phase to work on next, based on `STATUS.md`'s "Next phase" line.
4. Executes that phase by either:
   * **Dispatching a subagent** via the `Task` tool with `subagent_type: "general-purpose"` for bounded, scoped work. Pass the agent file path so the subagent reads it as its brief. See "How to dispatch a specialist agent" below.
   * **Playing the role yourself** in the current session for work that benefits from accumulated context (Engine Developer building up the matching engine across many edits, Frontend Developer iterating on the UI, DevOps Engineer setting up the pipeline).

### "Work on the next phase" shortcut

If the user says "work on the next phase" or any equivalent ("continue", "keep going", "next one", "let's do the next phase"), do this:

1. Read `/home/mustafa/src/Meridian/STATUS.md`.
2. Identify the phase named in the "Next phase" section.
3. Confirm with the user once: "STATUS.md says the next phase is Phase **{N}: {name}**. Window cost: {budget}. Starting now unless you say otherwise."
4. If the user does not object within their next message, begin the phase.
5. Update `STATUS.md` to set that phase's status to `in progress` and update the "Last updated" line before doing any other work.

### Updating STATUS.md is mandatory

The PM session must update `STATUS.md` at three moments, no exceptions:

* **Starting a phase**: status from `not started` to `in progress`. Update "Last updated" and "Next phase".
* **Pausing mid phase**: status to `paused`. Write a Resume notes entry naming the last clean commit and the next concrete task.
* **Completing a phase**: status to `completed`. Fill in "Completed on" date. Update "Next phase" to the next not-started phase. If you bundled phases, mark the bundled one as `bundled with phase N`.

Never start, pause, or complete a phase without updating `STATUS.md` first.

**You are not the PM only when** the user explicitly says "for this session you are the [Role] agent" or "act as the [Role]". In that case, read `SPEC.md` and `agents/NN-role.md` and do what that file says. Stay in that role for the session.

## How to dispatch a specialist agent

Use the `Task` tool. The prompt template:

```
You are the [Role] agent for the Meridian project at /home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/SPEC.md
- /home/mustafa/src/Meridian/agents/NN-role.md

Execute the [Phase X] tasks listed in your agent file. Use the plugins listed in the "Plugins to use" section by invoking them with the Skill tool. Stay strictly in the role of [Role]; do not take on tasks that belong to other agents (route those back to the Project Manager).

When you are done, report:
1. What you produced (file paths).
2. Any decisions you made that the PM should review.
3. The next agent in the handoff chain per your agent file.
```

Substitute `[Role]`, `NN-role`, and `[Phase X]` for each dispatch. Always pass absolute paths.

### When to dispatch vs play the role yourself

| Use a subagent (Task tool) | Play the role yourself |
|---|---|
| Bounded, scoped work with a clear deliverable. | Long lived development with iterative edits. |
| Independent of other in flight work. | Needs to react to the user, the test runner, or other agents. |
| Produces an artifact (a doc, a test file, a security review report). | Builds up context across many tool calls. |
| Examples: Code Review pass, Security Engineer threat model, QA writing tests, Documentation Engineer drafting a doc, Performance Engineer one-shot benchmark report. | Examples: Engine Developer building the matching engine, Frontend Developer building React components, DevOps Engineer wiring CI/CD. |

When in doubt, dispatch. A subagent that returns "not enough context" is cheap to recover from.

## How to invoke plugins and skills

Use the `Skill` tool. The skills listed in each agent file's "Plugins to use" section are the ones to invoke.

Example:

```
Skill(skill="superpowers:writing-plans")
Skill(skill="superpowers:test-driven-development")
Skill(skill="security-review")
```

Plugin and skill names map to what is listed in your session's available skills. If a skill in an agent file is not available in your session, surface that to the user; do not silently skip it.

## Phase model: do not advance prematurely

`SPEC.md` defines 12 phases (0 through 11). A phase is not done until:

* All agent task lists for that phase are complete.
* QA Engineer reports green.
* Security Engineer reports no open criticals.
* Code Reviewer has approved every PR in the phase.
* Risk and Financial Correctness Reviewer has signed off if the phase touched matching invariants, price-time priority, or any logic where a real exchange would care about determinism.
* Performance Engineer has signed off if the phase changed the hot path or the latency budget.
* Concurrency Reviewer has signed off if the phase changed threads, atomics, memory ordering, or the seqlock protocol.
* Citation and Fact Auditor has signed off if the phase touched any document containing a concrete number, named external reference, function name, file path, or behavior claim. Hallucinated documentation is a phase blocker.
* Reference Implementation Engineer has confirmed the Python reference under `tests/reference/` agrees with the C++ engine on the integration corpus, when the phase touched matching semantics or the ITCH parser.
* Documentation Engineer has updated `docs/`.

The PM session checks these gates before opening the next phase.

## Pacing rule: one phase per usage window, with mandatory check-in

The user is on the Claude Max 5x plan. Each plan window has a usage cap that resets on a fixed schedule. To make this build sustainable across many sessions, every phase is **sized to consume roughly 90 to 99 percent of a single usage window**. This is intentional. Some phases are small; when the next phase is small enough, the PM may bundle it into the current window. The PM never starts a new phase if doing so risks crossing the window boundary mid phase.

### Mandatory check-in at every phase boundary

When a phase closes, **the PM session must stop and ask the user before continuing**. Do not auto advance to the next phase, even in auto mode. The check-in message should be short and specific. Use this template (or close to it):

> Phase **{N}** is complete. Summary of what shipped: {one paragraph}.
>
> Before I open Phase **{N+1}**, please tell me:
>
> 1. What is your Claude Max usage at right now (the percentage shown in your dashboard or in the rate limit indicator)?
> 2. Roughly how long until your next reset window?
> 3. Do you want to continue into Phase {N+1} now, pause until the next window, or stop here for the day?
>
> Phase {N+1} scope: {one or two sentences on what it covers}. Estimated cost: roughly 1 full window.

Wait for the user's answer. Do not proceed without explicit confirmation.

### What to do based on the user's answer

* **"Continue"**: open Phase {N+1}. If the user reports usage above 80 percent and the upcoming phase is large, recommend pausing instead and explain why.
* **"Pause"**: write a short note to the "Resume notes" section of `STATUS.md` so the next session can resume cleanly. Acknowledge and end the session there.
* **"Stop"**: same as pause. Do not start Phase {N+1}.
* **No answer / ambiguous**: ask once more, then default to pause. Never assume "continue".

### Mid phase usage check

If during a phase the rate limit indicator approaches 90 percent and the phase is not yet at a natural commit point, the PM session should:

1. Finish the smallest unit of work that produces a clean commit (e.g., one passing test plus its implementation).
2. Commit and push.
3. Update the "Resume notes" section of `STATUS.md` with the commit ref and the next concrete task.
4. Mark the phase as `paused` in the STATUS.md status table.
5. Stop and tell the user: "I'm at roughly 90 percent for this window. I committed at {ref}. Next session can resume from there."

Never burn the last few percent of a window on a half finished change that cannot be committed.

## Directory layout

```
/home/mustafa/src/Meridian/
├── CLAUDE.md                  # This file. Auto loaded.
├── STATUS.md                  # Single source of truth: which phase is next, which are done.
├── SPEC.md                    # The full project spec.
├── GETTING-STARTED.md         # First session walkthrough.
├── README.md                  # Created in Phase 0 by Documentation Engineer.
├── future-ideas.md            # Maintainer's private notes; gitignored.
├── agents/                    # 18 agent briefs (00 PM plus 17 specialists). Read on dispatch.
├── design-mockups/            # 5 visual direction mockups built during brainstorming (kept for design archaeology).
│                              # The chosen direction is Twilight; canonical design lives at docs/design/canonical.html.
├── docs/                      # All project docs. Created by agents during phases.
│   ├── plan.md                # Detailed implementation plan; created by PM in Phase 0, updated as the build progresses. (For high level "which phase is next" status, read STATUS.md instead.)
│   ├── setup-guide.md         # User facing deployment guide.
│   ├── architecture.md        # Created by Documentation Engineer in Phase 0.
│   ├── design/                # Canonical visual reference (`canonical.html`), wireframes, tokens.
│   ├── security/              # Threat model, checklists, secrets reference.
│   ├── qa/                    # Regression checklist.
│   ├── perf/                  # Latency budgets and findings. Central to this project.
│   ├── observability/         # Logging and metrics runbooks.
│   ├── risk/                  # Matching engine correctness conventions and audit cases.
│   ├── adr/                   # Architecture Decision Records.
│   └── superpowers/specs/     # Per feature design specs (e.g., the v0.1 design at 2026-05-09-meridian-design.md).
├── include/meridian/          # Public library headers. Created in Phase 1 by Engine Developer.
├── src/                       # libmeridian.a implementation. Created in Phase 1 by Engine Developer.
├── apps/                      # Driver binaries (bench, replay, server). Created across phases by Engine Developer and DevOps.
│   ├── bench/                 # meridian-bench: zero-I/O throughput and latency benchmark.
│   ├── replay/                # meridian-replay: CLI ITCH replayer that streams JSON Lines to stdout.
│   └── server/                # meridian-server: live demo binary with uWebSockets.
├── tests/                     # Unit, property-based, and integration tests.
├── bench/                     # Benchmark baseline JSON files for CI regression checks.
├── third_party/               # Vendored dependencies pulled via CMake FetchContent.
└── frontend/                  # React app. Created in Phase 9 by Frontend Developer.
```

Always use absolute paths when referring to files in this project. `/home/mustafa/src/Meridian/...` is the canonical prefix.

## Visual design source of truth

The canonical visual ground truth for the project is `/home/mustafa/src/Meridian/docs/design/canonical.html`. The theme is **Twilight** (dark surface, deep indigo `#0E1126` background, gold `#D4A24C` accent, muted teal `#4FA89E` for bid, faded coral `#C5765B` for ask, off-white `#EFE9DC` ink, Newsreader italic for editorial display, Inter for UI body, JetBrains Mono with tabular-nums for all numerics). This file is the visual ground truth for the project. It is openable in any browser, edited by the user (and by Claude Design when the user asks for revisions), and tracked in git. The implemented React frontend is expected to match its visual personality, design tokens, and component anatomy.

The implementation is React plus Vite plus Tailwind, NOT this HTML directly. Treat the HTML as a design reference, not a runtime artifact.

The Twilight theme was chosen during brainstorming on 2026-05-09 from a field of 5 candidate directions (Bourse, Twilight, Terminal, Eikon, Onyx). The other 4 are kept under `design-mockups/themes/` for design archaeology.

### Useful structure inside the HTML for any agent reading it

* **`:root` CSS variables**: every color, font size, spacing value, radius, shadow, and motion duration. Edit these to reskin the whole app.
* **`data-component` attributes**: every visual region is labeled with its eventual React component name (`Header`, `Hero`, `Ladder`, `DepthChart`, `Tape`, `PerfPanel`, etc.). Sub elements use `data-element`. Use these as test selectors and as the search keys for "find the X".

There are two ways the design changes. The user picks. Both are first class flows; do not push the user toward Claude Design unless they explicitly want a fresh visual exploration.

### Flow A: direct design change from the terminal (preferred for incremental tweaks)

Use this when the user says something like "make the gold accent deeper", "tighten the ladder row spacing", "the header should be sticky", "the hero numerics should be larger", or any plain English design ask. The user does not need to open Claude Design or edit the HTML by hand.

Steps:

1. Read the current state of the affected files:
   * `frontend/tailwind.config.ts` (tokens).
   * The relevant React component(s) under `frontend/src/`.
   * `docs/design/canonical.html` (the canonical visual reference).
2. Decide the change scope. Confirm with the user first **only if** the change touches more than one component, restructures markup, or changes a token used in many places. For trivial changes (a single color, a single spacing value, a single component prop), just make the change and report what you did.
3. Edit the React components and the Tailwind config to apply the change.
4. **Update `docs/design/canonical.html` to match.** This keeps the design source in sync with the implementation so it is still useful as a reference and so a future Claude Design round trip starts from current state, not from a stale snapshot.
5. Run tests (`pnpm --filter frontend test` plus the C++ test suite if a dispatched panel changes the WebSocket protocol).
6. Optional: if the user has the dev server running (`pnpm dev` in `frontend/`) and `meridian-server` running locally, Vite hot reloads the change automatically.
7. Update `docs/design/tokens.md` if a token changed.
8. Commit with `design change: <one line summary>`.
9. Append a one line entry to the "Design sync log" section of `STATUS.md` with today's date and the summary.

### Flow B: Claude Design round trip (use when the user wants a fresh visual exploration)

Use this when the user says "I went back to Claude Design and got a new HTML", "I want to redesign this from scratch", "give me variations", or otherwise indicates they took the round trip externally. The user updates `docs/design/canonical.html` themselves (or tells you to write it from contents they paste). Then:

1. Read the current HTML at `docs/design/canonical.html`.
2. Run `git log -1 --format=%H -- docs/design/canonical.html` to find the last commit that touched the file. Then `git diff <that_commit>~1 -- docs/design/canonical.html` to see what actually changed.
3. Categorize the changes into: (a) design tokens (colors, typography, spacing), (b) component anatomy (markup structure of a component changed), (c) layout (screen level grid changed), (d) motion or interactions, (e) new components or screens.
4. Propose a concrete change list to the user: which Tailwind config entries to update, which React components to modify, which CSS values to retune. Do not start editing until the user accepts the list.
5. After acceptance, dispatch the **UI/UX Designer** agent to refresh `docs/design/wireframes.md` and the Tailwind tokens, then the **Frontend Developer** agent to apply the component and layout changes. Both agents read `docs/design/canonical.html` as their authoritative source.
6. Run the full test suite (`pnpm test` plus `ctest` for the C++ side) plus the a11y audit before committing.
7. Commit with `design sync: <one line summary>`.
8. Append a one line entry to the "Design sync log" section of `STATUS.md`.

### Both flows work during the build AND after Phase 11 is shipped

Neither requires a new "phase". Both are in place edit cycles. Pick the flow that matches what the user actually did (described a change vs replaced the HTML).

## Coding conventions

User's global instructions (`/home/mustafa/.claude/CLAUDE.md`) apply to every file written here. Key reminders for this project:

* **No dashes as prose punctuation.** No em dashes, no en dashes, no hyphens used as punctuation. Hyphens are allowed only inside identifiers, file names, command line flags, package names, URLs, and code. In prose, use commas, colons, semicolons, or parentheses instead.
* **No `Co-Authored-By` trailers** on commits. Authored solely by Mustafa Nazeer.
* **No "Generated with Claude Code"** lines on commit messages or PR descriptions.
* **Ask before guessing.** If a personal preference, deployment choice, or design decision is not in the spec, ask the user. Do not invent.
* **Default to no comments.** Only add comments where the WHY is non obvious.
* **Don't add features beyond the phase scope.** Scope creep is rejected by the Code Reviewer.
* **C++ is C++20 throughout.** No GNU extensions, no `using namespace std`, no `auto` for return types unless deduction is genuinely the right call.
* **No exceptions on the hot path.** The matching loop returns result codes and uses `[[likely]]` / `[[unlikely]]` annotations. Exceptions are allowed only in setup paths (config parse, file open, port bind).
* **No heap allocation on the hot path.** All memory used by the matching loop comes from preallocated pools. An allocator override in debug builds aborts on any new/malloc during matching.
* **Frontend styling is Tailwind CSS.** Locked in for v1. Do not introduce alternative styling solutions without explicit user approval.

## Test discipline

* The matching engine invariants (price-time priority, quantity conservation, no double-fill, no lost orders) must be developed test first. Property-based tests via `rapidcheck` are required.
* C++ unit tests use GoogleTest. Target line coverage on `libmeridian.a` is 85 percent or higher.
* The frontend uses Vitest plus Testing Library; tests cover at least the happy path per component.
* No code is merged with failing tests, no exceptions.
* Use `superpowers:test-driven-development` for any new feature; use `superpowers:verification-before-completion` before declaring work done.

## When the user says "push"

Standing protocol for every session in this repo. When the user says "push" or any equivalent ("ship it", "merge it", "land it"), do not stop mid flow to ask permission for the admin merge step.

1. **Commit on a feature branch and push to origin.** Never `git push` directly to `main`; the local hook blocks it and `main` has branch protection. Branch name should describe the change (`phase-1-matching-loop`, `ci-fix-tsan-flag`, etc.).
2. **Open the PR via `gh pr create`.** Title and body follow the project commit conventions: no `Co-Authored-By` trailer, no "Generated with Claude" or similar attribution line, no em or en dashes, no hyphens used as prose punctuation.
3. **Watch the required CI checks** with `gh pr checks <N> --watch --interval 10`. Wait for every required check (Engine build, Engine test, Bench regression, Frontend build, Frontend test, Secrets scan, SAST) to report `pass`. If anything fails, stop and surface the failure to the user; do not admin merge over a failing required check.
4. **Once all required checks are green, admin merge** with `gh pr merge <N> --squash --delete-branch --admin`. Meridian is a solo repo where the user is also the PR author, so the "1 review required" rule is structurally unsatisfiable, and `--admin` is the standing authorization to bypass *that specific rule only*. Squash merge keeps history linear.
5. **Sync local `main`** with `git checkout main && git pull --ff-only`. Confirm the new commit is on `main`.
6. **Report briefly to the user**: the merged PR number, the squash commit SHA on `main`, and a one or two sentence summary of what shipped.

### What `--admin` is and is not authorized for

The standing authorization is **only** for the self approval bypass on PRs that are fully green. It is **not** authorization to:

* Skip pre commit hooks or required CI checks (`--no-verify`, merging over failures).
* Force push to feature branches without `--force-with-lease`, and never to `main`.
* Push directly to `main`, bypassing the PR flow entirely.
* Merge a PR with unresolved review comments left by the user.

## Plugin and skill quick reference

This is the canonical list of which skills are used where. Each agent file restates the relevant subset.

| Skill | Used by | Purpose |
|---|---|---|
| `superpowers:brainstorming` | Project Manager | Before each phase, confirm intent and assumptions. |
| `superpowers:writing-plans` | Project Manager | Produce per phase implementation plans. |
| `superpowers:dispatching-parallel-agents` | Project Manager | When two or more independent subagents can run in parallel. |
| `superpowers:executing-plans` | Engine Developer, Frontend Developer | Execute the PM's plan in the current session. |
| `superpowers:test-driven-development` | Engine Dev, Frontend Dev, Market Microstructure Engineer, Reference Implementation Engineer, QA, Quant Validator | Test first development. |
| `superpowers:verification-before-completion` | All agents at sign off (especially Citation and Fact Auditor) | Confirm work is actually done before claiming so. |
| `superpowers:requesting-code-review` | Engine Dev, Frontend Dev, others | At PR time. |
| `superpowers:finishing-a-development-branch` | Project Manager, DevOps Engineer | At phase close. |
| `superpowers:systematic-debugging` | Any agent debugging a failure (especially Concurrency Reviewer chasing a TSAN report) | Before proposing a fix. |
| `frontend-design` | Frontend Developer, UI/UX Designer | Visual layer of every component. |
| `vercel-react-best-practices` | Frontend Developer | Performance and idiomatic React. |
| `vercel-composition-patterns` | Frontend Developer | When components grow large. |
| `web-design-guidelines` | UI/UX Designer, Accessibility Specialist | Audit pass on the UI. |
| `security-review` | Security Engineer | Formal security review at phase boundaries. |
| `code-review:code-review` | Code Reviewer, Concurrency Reviewer | Every PR (Code Reviewer for general quality; Concurrency Reviewer for threads, atomics, seqlock). |
| `simplify` | Code Reviewer | When the diff has obvious simplification opportunities. |

## When you get stuck

* **You don't know which phase to work on**: read `STATUS.md`. The "Next phase" line names it explicitly. `docs/plan.md` is the longer implementation plan, useful only after STATUS.md tells you which phase you are in.
* **A subagent returned vague or wrong work**: re prompt with the agent file path and a specific correction, or take over the role yourself in this session.
* **A skill listed in an agent file is not available in this session**: tell the user; do not silently substitute.
* **A decision affects multiple agents**: route it through the PM (you, by default) and capture it in `docs/adr/`.
* **You need the deep technical design**: read `/home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md`. That is the engineering contract. SPEC.md is the higher-level project plan.
* **You are unsure of any user preference**: ask. Per the user's global CLAUDE.md, do not guess personal or subjective answers.

## First session entry point

Read `GETTING-STARTED.md` for the literal walkthrough of what to do on the very first session opened in this directory.
