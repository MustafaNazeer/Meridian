# Getting started: first session walkthrough

This file is the literal step by step guide for the first Claude Code session opened in this directory. **For any session after the first**, do not read this file; instead read `STATUS.md` to find the current phase and proceed from there.

## What you (Claude) should do, in order

### Step 1: orient

1. Read `CLAUDE.md` if you have not already (it should have auto loaded).
2. Read `STATUS.md` to confirm the next phase is Phase 0 (it should be on a fresh project).
3. Read `SPEC.md` end to end.
4. Read `agents/00-project-manager.md`.
5. Read the deep technical design at `docs/superpowers/specs/2026-05-09-meridian-design.md` (the engineering contract).
6. List the contents of `design-mockups/themes/` (the 5 candidate visual directions; the chosen one, Twilight, is also copied to `docs/design/canonical.html`).
7. Read this file (you are here).

You now know: the project is a price-time priority limit order book and matching engine in C++20 with a live web visualization. There are 12 phases (0 through 11) and 18 agents (00 PM plus 17 specialists). Each phase has agent task lists. You, by default, are the Project Manager.

### Step 2: confirm with the user

Before any code is written, post a short orientation message to the user that names:

* What phase you are about to start (Phase 0).
* What the Phase 0 deliverables are.
* Which agents you plan to dispatch in parallel.
* The first concrete artifact you will produce (the implementation plan in `docs/plan.md`).

Wait for the user to ack before proceeding. The user may have new context (a Fly.io app name preference, a region preference, etc.; the four open questions originally in design spec section 13 were resolved on 2026-05-09 during Phase 0 bootstrap and are no longer open).

### Step 3: brainstorm and plan

Invoke the brainstorming skill to confirm Phase 0 scope:

```
Skill(skill="superpowers:brainstorming")
```

Then invoke writing plans to produce `docs/plan.md`:

```
Skill(skill="superpowers:writing-plans")
```

`docs/plan.md` should contain:

* A list of all 12 phases.
* For each phase: the agents involved, the deliverables, and the quality gates (QA, Security, Code Review, Risk for matching changes, Performance for hot path changes).
* The current phase highlighted as "in flight".

### Step 4: dispatch Phase 0 agents

Phase 0 deliverables (per SPEC.md):

* Project Manager runs brainstorming and writes `docs/plan.md`. (Already done in Step 3.)
* UI/UX Designer reads the canonical Twilight HTML at `docs/design/canonical.html`, extracts tokens to `docs/design/tokens.md` and a draft `frontend/tailwind.config.ts` extension, and writes screen layout descriptions to `docs/design/wireframes.md`.
* Security Engineer publishes the threat model and baseline.
* DevOps Engineer wires the local working directory `/home/mustafa/src/Meridian/` into the existing GitHub repo `MustafaNazeer/Meridian` (note: by the time you read this in a fresh session, this work may already be partially done, since a prior session ran `git init` and pushed the initial scaffold to https://github.com/MustafaNazeer/Meridian. Confirm with `git remote -v`. If the remote is set, the DevOps Engineer's remaining Phase 0 work is the CMake skeleton plus the `pnpm` plus Vite plus React plus TypeScript plus Tailwind project under `frontend/`, dropping the Tailwind config sketch from the Twilight HTML's tokens into `frontend/tailwind.config.ts`.).
* Documentation Engineer writes the initial README and `docs/architecture.md`.
* Citation and Fact Auditor runs the first pass on `README.md`, `SPEC.md`, and `docs/architecture.md` once the Documentation Engineer has produced them; files the initial audit at `docs/audits/citation-audit-{date}.md`. This dispatch can be sequential after the Documentation Engineer rather than parallel.

The first four bullets can run in parallel. Use `superpowers:dispatching-parallel-agents` and dispatch each via the `Task` tool. The Citation and Fact Auditor goes after the Documentation Engineer's pass completes. Use the prompt template in `CLAUDE.md` under "How to dispatch a specialist agent".

Concrete example for the Security Engineer dispatch:

```
Task(
  description="Security: Phase 0 threat model",
  subagent_type="general-purpose",
  prompt="""You are the Security Engineer agent for the Meridian project at /home/mustafa/src/Meridian.

Read these files first:
- /home/mustafa/src/Meridian/SPEC.md
- /home/mustafa/src/Meridian/CLAUDE.md
- /home/mustafa/src/Meridian/agents/08-security-engineer.md

Execute the Phase 0 tasks listed in your agent file. Specifically: write the threat model to /home/mustafa/src/Meridian/docs/security/threat-model.md, write the per phase hardening checklist to /home/mustafa/src/Meridian/docs/security/checklist.md, and stub out /home/mustafa/src/Meridian/docs/security/secrets.md.

Use the security-review skill via the Skill tool when forming the threat model. Stay strictly in the Security Engineer role.

When done, report:
1. Files written.
2. Any decisions you made that the PM should review (e.g., suggested CSP policy, WebSocket origin policy, rate limit thresholds, branch protection rules).
3. The next agent in the handoff chain per your agent file.
"""
)
```

Dispatch the UI/UX Designer, DevOps Engineer, and Documentation Engineer in the same session in parallel using the same template.

### Step 5: review handoffs

When each subagent reports back:

1. Read the files it wrote.
2. Verify the deliverables match the agent file's "Definition of done".
3. If yes, mark that part of Phase 0 done in `docs/plan.md`.
4. If no, re prompt the subagent with a specific correction (cite the agent file).

### Step 6: gate, check-in, and only then proceed

Before opening Phase 1:

* Confirm `docs/plan.md` reflects all Phase 0 deliverables as complete.
* Confirm the GitHub repo is initialized (DevOps Engineer task).
* Confirm the user has signed off on the wireframes and tokens (UI/UX Designer task).
* Confirm the threat model is published (Security Engineer task).
* Confirm `README.md` and `docs/architecture.md` exist (Documentation Engineer task).

When all five are true, **update `STATUS.md`** to flip Phase 0's status to `completed`, fill in "Completed on" with today's date, and set the "Next phase" line to "Phase 1 (core data structures and single-symbol matching)". Then **run the mandatory check-in protocol from `CLAUDE.md` ("Pacing rule")**. Concretely, send the user a message like:

> Phase 0 is complete. Summary: implementation plan written, threat model published, design tokens accepted, repo wired into GitHub, CMake scaffold and frontend scaffold up, README and architecture doc shipped.
>
> Before I open Phase 1, please tell me:
>
> 1. What is your Claude Max usage at right now?
> 2. Roughly how long until your next reset window?
> 3. Continue to Phase 1 now, pause until next window, or stop here?
>
> Phase 1 is the core data structures and single-symbol matching loop (~95% of a window). Estimated cost: roughly one full window.

Wait for the user's explicit answer. Do not auto advance, even in auto mode.

## What success looks like at the end of Phase 0

```
/home/mustafa/src/Meridian/
├── .git/                                      # Created by DevOps Engineer (git init)
├── .gitignore                                 # Created by DevOps Engineer
├── README.md                                  # Written by Documentation Engineer
├── CLAUDE.md                                  # Already exists
├── STATUS.md                                  # Already exists; PM flipped Phase 0 to "completed"
├── SPEC.md                                    # Already exists
├── GETTING-STARTED.md                         # Already exists
├── future-ideas.md                            # Already exists
├── agents/                                    # Already exists, 18 files
├── design-mockups/                            # Already exists, 5 candidate visuals
├── CMakeLists.txt                             # Created by DevOps Engineer
├── include/meridian/                          # Created by DevOps Engineer (empty placeholder)
├── src/                                       # Created by DevOps Engineer (empty placeholder)
├── apps/
│   ├── bench/                                 # Created by DevOps Engineer (empty)
│   ├── replay/                                # Created by DevOps Engineer (empty)
│   └── server/                                # Created by DevOps Engineer (empty)
├── tests/                                     # Created by DevOps Engineer (empty)
├── bench/                                     # Created by DevOps Engineer (empty; Phase 5 fills in baseline JSON)
├── third_party/                               # Created by DevOps Engineer (FetchContent populates later)
├── docs/
│   ├── plan.md                                # Written by you (PM) in Step 3
│   ├── setup-guide.md                         # Already a placeholder; expanded in Phase 10
│   ├── architecture.md                        # Written by Documentation Engineer
│   ├── design/canonical.html                  # Already exists (Twilight visual)
│   ├── design/tokens.md                       # Written by UI/UX Designer
│   ├── design/wireframes.md                   # Written by UI/UX Designer
│   ├── security/threat-model.md               # Written by Security Engineer
│   ├── security/checklist.md                  # Written by Security Engineer
│   ├── security/secrets.md                    # Written by Security Engineer (stub)
│   ├── audits/citation-audit-{date}.md        # Written by Citation and Fact Auditor
│   └── superpowers/specs/2026-05-09-meridian-design.md  # Already exists
└── frontend/                                  # Initialized by DevOps Engineer with pnpm + Vite + React + TS + Tailwind (empty)
```

When this tree exists and every file's "Definition of done" is met, Phase 0 is complete and Phase 1 can begin.

## What to do on the second session and beyond

Ignore this file. Read `STATUS.md`. The "Next phase" line tells you exactly which phase to start. The user typically opens Claude Code and says "work on the next phase"; that maps directly to whatever `STATUS.md` says.

For mid phase resume (e.g., a paused phase from a prior window), read the "Resume notes" section of `STATUS.md` to find the last commit and the next concrete task.
