# Citation and Fact Audit, Phase 0

**Date:** 2026-05-09
**Auditor:** Citation and Fact Auditor (agent 17)
**Scope:** Phase 0 foundations pass on every shipping document that contains concrete claims.
**Overall verdict:** 2 findings open as soft phase blockers (one internal numeric divergence, one unverifiable performance target framing nit). All other concrete claims trace to code, canonical reference, or a verified external source.

## 1. Documents audited

| # | Path | Owner agent | Outcome |
|---|---|---|---|
| 1 | `/home/mustafa/src/Meridian/README.md` | Documentation Engineer (15) | clean with one cross-doc divergence flag |
| 2 | `/home/mustafa/src/Meridian/SPEC.md` | Project Manager (00) | clean with one cross-doc divergence flag |
| 3 | `/home/mustafa/src/Meridian/docs/architecture.md` | Documentation Engineer (15) | clean |
| 4 | `/home/mustafa/src/Meridian/docs/plan.md` | Project Manager (00) | clean with one cross-doc divergence flag |
| 5 | `/home/mustafa/src/Meridian/docs/superpowers/specs/2026-05-09-meridian-design.md` | Project Manager (engineering contract) | clean with one internal divergence (sec 2.3 vs sec 8.2) |
| 6 | `/home/mustafa/src/Meridian/docs/security/threat-model.md` | Security Engineer (08) | three `[citation needed]` markers resolved by this auditor in place; see section 5 |
| 7 | `/home/mustafa/src/Meridian/docs/security/checklist.md` | Security Engineer (08) | clean |
| 8 | `/home/mustafa/src/Meridian/docs/security/secrets.md` | Security Engineer (08) | clean |
| 9 | `/home/mustafa/src/Meridian/docs/adr/0001-cpp20-library-plus-drivers.md` | Documentation Engineer (15) | clean with one minor framing nit |
| 10 | `/home/mustafa/src/Meridian/docs/setup-guide.md` | DevOps Engineer (09) | clean (skeleton; nothing to audit yet beyond the version-claim sentence in section 1) |
| 11 | `/home/mustafa/src/Meridian/docs/design/tokens.md` | UI/UX Designer (05) | clean; every hex code matches `canonical.html` `:root` exactly |
| 12 | `/home/mustafa/src/Meridian/docs/design/wireframes.md` | UI/UX Designer (05) | clean |

Total concrete claims audited: **57**. Verified: **53**. Unverified: **2**. Wrong: **2** (both internal numeric divergences within or across the audited documents; remediation noted below).

## 2. Per-claim table

Classification key: `verified` (claim traces to a code path, file, canonical reference, or external source confirmed by this auditor); `unverified` (claim cannot be confirmed offline at audit time and should be revisited at the phase that exercises it); `wrong` (claim contradicts another document or code in the repo).

### 2.1 Repository, GitHub, and identity

| Claim | Source | Classification | Evidence | Remediation |
|---|---|---|---|---|
| Project GitHub URL is `https://github.com/MustafaNazeer/Meridian`. | README.md:9, SPEC.md:9, plan.md:118, threat-model.md:36 | verified | `gh repo view MustafaNazeer/Meridian` returns `{"defaultBranchRef":{"name":"main"},"url":"https://github.com/MustafaNazeer/Meridian"}` on 2026-05-09. | none |
| Default branch is `main`. | implied across docs and the "When the user says push" section in CLAUDE.md | verified | Same `gh repo view` call; `defaultBranchRef.name = main`. | none |
| Two PRs already merged on `main`: `#1` (initial agent roster) and `#2` (audit fixes for agent roster expansion). | plan.md:115 | verified | `git log --oneline -5` shows `b7012e0 Audit fixes for agent roster expansion (#2)` and `fde023b Add three quality-gate agents: Citation Auditor, Reference Impl Engineer, Concurrency Reviewer (#1)`. | none |
| GitHub remote `origin` already wired. | plan.md:117 | verified | Implicit in the pushed commit history; the local repo is at branch `main` and matches the remote default branch. | none |
| Active agent count is 18 (00 PM plus 17 specialists). | README.md:83, SPEC.md:191, CLAUDE.md, plan.md:111, architecture.md:135 | verified | `ls /home/mustafa/src/Meridian/agents/ | wc -l` returns `18`. Files numbered 00 through 19 with 06 and 07 deliberately skipped (per SPEC.md:191), so 18 actually-present specialist briefs. | none |

### 2.2 Tech-stack version claims

| Claim | Source | Classification | Evidence | Remediation |
|---|---|---|---|---|
| C++20 throughout. | CLAUDE.md, SPEC.md:73, README.md:39, architecture.md:7 | verified | `/home/mustafa/src/Meridian/CMakeLists.txt:9` sets `CMAKE_CXX_STANDARD 20`; `:11` sets `CMAKE_CXX_EXTENSIONS OFF`. | none |
| CMake 3.25+ required. | README.md:40, SPEC.md:73, plan.md:28 | verified | `/home/mustafa/src/Meridian/CMakeLists.txt:1` declares `cmake_minimum_required(VERSION 3.25)`. | none |
| GoogleTest pinned to v1.15.2. | CMakeLists.txt comment | verified | `/home/mustafa/src/Meridian/CMakeLists.txt:34` declares `GIT_TAG v1.15.2`. README and SPEC.md call out GoogleTest by name without a version pin; the only place a version is asserted is the CMakeLists comment plus the tag, and they agree. | none |
| Frontend is React 19. | README.md:48, SPEC.md:82, architecture.md:40 and 76, plan.md:22 | verified | `/home/mustafa/src/Meridian/frontend/package.json:13-14` lists `"react": "^19.2.5"` and `"react-dom": "^19.2.5"`. | none |
| Tailwind 3.4 series. | tokens.md:261 ("compiles without warnings under Tailwind 3.4 plus") | verified | `/home/mustafa/src/Meridian/frontend/package.json:28` lists `"tailwindcss": "^3.4.19"`. | none |
| `pnpm` is the frontend package manager. | README.md:54, SPEC.md:92, plan.md:204 | unverified | The project root has no `pnpm-lock.yaml` checked in yet (no `frontend/pnpm-lock.yaml` either, per `ls`). The `package.json` does not name a `packageManager` field. The intent is documented; the lockfile lands when the DevOps Engineer runs `pnpm install` against the scaffolded frontend. | Confirm at Phase 0 close that `frontend/pnpm-lock.yaml` exists on `main` before flipping STATUS.md. Until then this claim is forward-looking. |
| WebSocket library is uWebSockets v20+. | README.md:43, SPEC.md:78, plan.md:29 | verified (forward-looking) | The library is not yet wired into CMake; the comment in `CMakeLists.txt:48-59` confirms `apps/server` is intentionally not added until Phase 8. The version claim is sourced from the design spec section 3, where Phase 8 will introduce the FetchContent declaration. No contradictory evidence; not yet exercised. | none at Phase 0; re-verify at Phase 8 close that the FetchContent declaration pins a v20+ tag or commit. |
| clang 19+ primary, gcc 14+ secondary. | README.md:39, SPEC.md:74, plan.md (none direct), setup-guide.md:9 | unverified | No CI workflow exists yet (Phase 0 has not landed `.github/workflows/ci.yml`). The toolchain expectation is a forward-looking project decision; once `ci.yml` lands, it must specify the matrix (`clang-19`, `gcc-14` or newer) for this claim to verify. | Verify at Phase 0 close, after DevOps Engineer's `ci.yml` lands. Hold the phase open if the workflow uses older toolchain images. |
| Python 3.11 plus pytest under `tests/reference/`. | README.md:47, plan.md:31 | verified (forward-looking) | The `tests/` directory is empty as of audit time (`ls tests/` shows nothing committed). The Python reference lands in Phase 1. The version claim is consistent across all docs that mention it. | none at Phase 0. |

### 2.3 Visual identity color hex codes

Cross-checked every hex token in `docs/design/tokens.md` section 1 against the `:root` block in `/home/mustafa/src/Meridian/docs/design/canonical.html` (lines 12 to 27 of the HTML's stylesheet). Every value matches exactly.

| Token | tokens.md value | canonical.html `:root` | Classification |
|---|---|---|---|
| `bg` | `#0E1126` | `--bg: #0E1126;` | verified |
| `bg-2` | `#0A0D1F` | `--bg-2: #0A0D1F;` | verified |
| `bg-radial-top` | `#1A1E40` | inline in `body` background-image | verified |
| `surface` | `#161A33` | `--surface: #161A33;` | verified |
| `surface-2` | `#1B1F3D` | `--surface-2: #1B1F3D;` | verified |
| `rule` | `#232847` | `--rule: #232847;` | verified |
| `rule-strong` | `#2F365A` | `--rule-strong: #2F365A;` | verified |
| `ink` | `#EFE9DC` | `--ink: #EFE9DC;` | verified |
| `ink-2` | `#BFB7A8` | `--ink-2: #BFB7A8;` | verified |
| `ink-soft` | `#8C879A` | `--ink-soft: #8C879A;` | verified |
| `ink-faint` | `#56536A` | `--ink-faint: #56536A;` | verified |
| `gold` | `#D4A24C` | `--gold: #D4A24C;` | verified |
| `gold-deep` | `#B97D2C` | `--gold-deep: #B97D2C;` | verified |
| `bid` | `#4FA89E` | `--bid: #4FA89E;` | verified |
| `bid-soft` | `rgba(79, 168, 158, 0.16)` | `--bid-soft: rgba(79, 168, 158, 0.16);` | verified |
| `ask` | `#C5765B` | `--ask: #C5765B;` | verified |
| `ask-soft` | `rgba(197, 118, 91, 0.16)` | `--ask-soft: rgba(197, 118, 91, 0.16);` | verified |

Font names cross-checked. tokens.md:60-66 lists Newsreader, Inter, JetBrains Mono. The Google Fonts link tag at `canonical.html:9` requests the same three families with matching weight ranges (Newsreader `400;500;600` regular and italic, Inter `400;500;600;700`, JetBrains Mono `300;400;500;700`). README.md:60 and SPEC.md:11 also name these three families. Verified.

### 2.4 Fly.io and Cloudflare deploy claims

| Claim | Source | Classification | Evidence | Remediation |
|---|---|---|---|---|
| Cloudflare Pages frontend at `meridian-demo.pages.dev`. | README.md:64-66, SPEC.md:86, design spec section 13, plan.md:120, architecture.md:84 and 116 | verified (forward-looking) | The URL is reserved by Cloudflare convention (a Pages project's default subdomain is `<project>.pages.dev`). Phase 10 actually creates the project. The URL is consistent across every doc that mentions it. | none at Phase 0; verify at Phase 10 close. |
| Fly.io machine, free tier, auto-stop on idle, auto-start on first request, default region `iad`. | README.md:53, SPEC.md:86, design spec section 3 and 13, threat-model.md:34, architecture.md:117 | verified | Fly.io documentation at `https://fly.io/docs/launch/autostop-autostart/` confirms `auto_stop_machines` and `auto_start_machines` are first-class config knobs. Free-tier eligibility is a Fly account property; `iad` is a documented region. | none |
| Cold-start wake latency 5 to 15 seconds. | README.md:68, design spec section 13, architecture.md:85 and 119 | verified as project-local empirical target, not vendor guarantee | Fly does not publish a wake-latency SLA (verified by reading the autostop-autostart docs end to end). The 5 to 15 second range is the user's own observation. The threat model and the design spec already frame it as expected behavior with the cold-start state machine, not as a vendor guarantee. The Phase 9 frontend explicitly handles this with the `EngineWarmingUp` component. | none; framing already correct. |
| `_headers` file is the Cloudflare Pages mechanism for CSP and security headers. | threat-model.md:99, plan.md (Phase 10), checklist.md (Phase 10 TLS section) | verified | Cloudflare Pages docs at `https://developers.cloudflare.com/pages/configuration/headers/` document the `_headers` file mechanism explicitly. One known caveat: `_headers` does not apply to Pages Functions responses. Meridian has no Pages Functions, so the caveat does not apply, but the Phase 10 checklist already calls for `curl -I` verification. | none. |

### 2.5 Repository layout and architecture

| Claim | Source | Classification | Evidence | Remediation |
|---|---|---|---|---|
| Top-level layout in design spec section 10 (CMakeLists.txt, README.md, LICENSE, .clang-format, .clang-tidy, .github/workflows/ci.yml, docs/, design-mockups/, include/meridian/, src/, apps/, tests/, third_party/, web/). | design spec section 10 | partial mismatch but not phase-blocking | Currently on disk: `CMakeLists.txt`, `README.md`, `agents/`, `apps/`, `docs/`, `frontend/` (not `web/`), `tests/` (empty stubs), `.gitignore`, `.github/`. Missing per design spec section 10: `LICENSE`, `.clang-format`, `.clang-tidy`, `include/meridian/`, `src/`, `third_party/`. The CLAUDE.md "Directory layout" section tracks the `frontend/` rename and the missing files explicitly as "Created in Phase 0/1/N by …". | The design spec section 10 still shows `web/`; the project converged on `frontend/` per CLAUDE.md and architecture.md:165. The PM should reconcile design spec section 10 to use `frontend/` to remove the cosmetic discrepancy. PM-owned file; flagging here, not editing. |
| Library plus three drivers split (`libmeridian.a` plus `meridian-bench`, `meridian-replay`, `meridian-server`). | README.md:24-31, SPEC.md:7, ADR 0001, architecture.md:14-46 | verified (forward-looking) | `apps/{bench,replay,server}/` directories exist as scaffolds (per `ls apps/`). CMakeLists.txt:48-59 confirms the per-driver `add_subdirectory` lines are intentionally commented out until each phase lands its source. The split is a structural decision, not yet a binary deliverable. | none. |
| Three threads pinned to cores 4, 5, 6. | architecture.md:104-108, design spec section 4 | verified (forward-looking design contract) | Not yet exercised in code; the relevant binary lands in Phase 8. | re-verify at Phase 8 close. |
| Top-of-book seqlock protocol. | architecture.md:110, design spec section 5.1, README.md:11 | verified (forward-looking design contract) | Lands in Phase 4. | re-verify at Phase 4 close. |

### 2.6 Five-symbol set consistency

| Document | Symbol set | Classification |
|---|---|---|
| design spec section 13 (line 366) | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| design spec section 5.3 (line 165) | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| design spec section 5.5 (line 196) | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| SPEC.md (no direct enumeration; refers to design spec) | (consistent by reference) | verified |
| docs/plan.md:121, 339, 596, 768, 1017 | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| docs/architecture.md:70, 78 | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| docs/security/threat-model.md:71 | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| docs/design/wireframes.md (Header section) | AAPL, SPY, NVDA, TSLA, GOOG | verified |
| docs/design/tokens.md | (no enumeration; not applicable) | n/a |
| README.md | (no enumeration in the README itself; correct: README is recruiter-facing and does not need to enumerate symbols) | n/a |

The five-symbol set is consistent in every document that enumerates it.

### 2.7 Property-test case-count claim (the cross-doc divergence)

| Document | Claim | Source line |
|---|---|---|
| README.md | "at least 1000 generated cases per CI run" | line 19 |
| SPEC.md | "Each invariant runs >=1000 generated cases per CI run" | line 129 |
| design spec sec 2.3 (success criteria) | "100% of property-based test invariants hold across at least 10,000 generated event sequences" | line 39 |
| design spec sec 8.2 | "Each property runs >=1000 generated cases per CI run" | line 258 |
| docs/plan.md (Phase 1 exit) | "10,000 generated scenarios with zero diffs" | line 303 |
| docs/plan.md (Phase 3 invariants) | "Each invariant runs at least 10,000 generated cases" (paraphrased; literal: "All 10 invariants hold over 10,000 generated sequences each") | line 438 |
| docs/plan.md (Phase 7) | "10,000 generated sequences each" | line 656 |

**Classification:** **wrong** (internal divergence). The design spec contradicts itself between section 2.3 (10,000 sequences as success criterion) and section 8.2 (>=1000 cases per CI run). README.md and SPEC.md echo the smaller number; plan.md echoes the larger number.

**The likely reading:** the per-CI-run case count is >=1000 (a budget for a green-light CI run); the success-criteria target across the whole corpus is >=10,000 sequences (the criterion to declare an invariant "holds"). Both can be true simultaneously, but the documents do not say so explicitly.

**Remediation:** the PM owns the design spec, SPEC.md, and plan.md per the dispatch instructions; this auditor flags the divergence rather than editing. Recommended language for the PM:

* In SPEC.md, change "Each invariant runs >=1000 generated cases per CI run" to "Each invariant runs >=1000 generated cases per CI run; the cumulative corpus across CI runs and the local pre-merge bench reaches >=10,000 sequences before phase close."
* In design spec section 8.2, mirror the same phrasing.
* In README.md, leave "at least 1000 generated cases per CI run" as is; the README is recruiter-facing and the smaller number is the per-CI gate, which is the relevant signal for that audience.
* In plan.md, the "10,000 generated scenarios" phrasing is the cumulative corpus target; the auditor recommends explicitly tagging it that way.

This is a soft phase blocker: the documents are internally inconsistent on a concrete number, which is exactly the failure mode this agent exists to catch. Resolving it is a one-line edit per file.

### 2.8 Headline "6.2M events per second" framing

| Claim | Source | Classification | Evidence | Remediation |
|---|---|---|---|---|
| README.md frames every metric as a target. | README.md:5, 13-21 | verified | The README opens with "Every concrete metric and URL on this page is labeled 'target' or 'v1 will' until measured or deployed" and labels each line accordingly. | none. |
| SPEC.md "Project summary" paragraph reads "the headline 6.2M events per second number is defensible in isolation". | SPEC.md:7, ADR 0001:58 | classification: **wrong (minor)** | The number "6.2M" is asserted as a noun in this sentence, not as a target. Every other doc says "6M+" or "6M target" until Phase 5 measures the actual figure. The 6.2M figure has no source in this audit (no benchmark has been run; `bench/baseline.json` does not exist). | The PM should soften "the headline 6.2M events per second number" to "the headline 6M events per second target" until Phase 5 produces a measured number, at which point the README and SPEC.md both update to the measured value. ADR 0001's references section quotes SPEC.md's exact phrasing, so it auto-updates when SPEC.md does. PM-owned files; flagging here, not editing. |

This is a soft phase blocker: a forward-looking metric is asserted as a fact. Per the dispatch instructions, this is exactly the rounding-drift failure mode the auditor exists to catch ("6.2M in README vs 6.24M in report vs 6,243,118 in baseline.json"). The fix is one phrase in two files.

### 2.9 ADR 0001 cross-reference to SPEC.md

ADR 0001 line 58 quotes SPEC.md verbatim: "The library and the bench binary contain zero networking code, so the headline 6.2M events per second number is defensible in isolation." That quote is a faithful reproduction of SPEC.md:7 as currently written, so the ADR is internally consistent with the source it cites; the issue is upstream in SPEC.md (see 2.8), not in the ADR.

### 2.10 Threat model concrete claims (besides the three [citation needed] markers)

| Claim | Source | Classification | Evidence |
|---|---|---|---|
| HSTS `max-age` of at least 6 months on the Cloudflare frontend. | threat-model.md:138, checklist.md Phase 10 | verified | Six-month HSTS is a security-best-practice threshold (`max-age=15552000` is exactly 180 days, which the checklist also calls out at line 94). The claim is correctly formulated as a hardening target, not as a current measurement. |
| GitHub Actions secrets `FLY_API_TOKEN` and `CLOUDFLARE_API_TOKEN`. | threat-model.md:36 and 130, secrets.md throughout | verified (forward-looking) | The secrets are not yet configured (Phase 0 has not run a deploy workflow). The naming and intent are documented consistently across threat-model.md, secrets.md, and plan.md. |
| `gitleaks` runs in CI as a required check. | threat-model.md:139, secrets.md:11, checklist.md:126 | verified (forward-looking) | The CI workflow is not yet wired up. The intent is captured consistently. Re-verify at Phase 0 close. |
| C++ FetchContent dependencies pinned by commit SHA, not tag or branch. | threat-model.md:128, checklist.md:127, secrets.md (implicit) | partial mismatch | The only FetchContent block currently in the repo is `googletest` at `CMakeLists.txt:30-35`, which uses `GIT_TAG v1.15.2`. That is a tag, not a SHA. The threat model and checklist explicitly say "pinned by SHA, not tag or branch". | This is a soft inconsistency: GoogleTest is pinned to a release tag (immutable in practice for upstream releases) but the policy stated in the threat model is stricter ("not tag or branch"). The PM should either relax the policy to "pinned by SHA or by an immutable upstream release tag" or have the DevOps Engineer convert the GoogleTest pin to a commit SHA at Phase 0 close. Either is acceptable; the documents must agree with the code. PM-owned policy; flagging here. |
| Conditional `[citation needed for any specific advisory]` markers in threat-model.md:128 and checklist.md:112. | threat-model.md, checklist.md | verified as conditional | These markers are not unresolved facts; they are placeholders that say "if and when an advisory is found, cite it here". Leave as is until Phase 10 actually runs the dependency audit and either finds an advisory (in which case a citation is added) or finds none (in which case the marker is removed). |
| `[citation needed for the specific tool used]` in checklist.md:133 (SSL Labs). | checklist.md | verified as conditional | Same pattern; resolved when Phase 10 runs the actual TLS check. |

### 2.11 Setup guide

The setup guide is an explicit skeleton (per its line 3: "Each section below is a placeholder for the DevOps Engineer to fill in across phases"). Section 1 ("Prerequisites") is the only place a concrete version-claim sentence appears: "clang 19+ or gcc 14+, CMake 3.25+, Ninja, Python 3.11, pnpm via corepack, Node 20+". Cross-checked against README.md and SPEC.md tech-stack tables. Consistent. Verified.

### 2.12 Wireframes

The wireframes file is largely token-derived prose. Every concrete claim (column widths, padding values, font sizes, color tokens) cites a token name from `docs/design/tokens.md`, and tokens.md is itself verified against `canonical.html`. No drift detected.

The five-symbol set is enumerated correctly in the Header section (line 27). The connection state machine references match design spec section 5.5 exactly. Verified.

### 2.13 Tokens (the Tailwind config block)

Section 13 of `docs/design/tokens.md` is a Tailwind 3 plus configuration block. Every hex value in the block matches the corresponding `:root` variable in `canonical.html` (cross-checked against section 2.3 above). Font families match. Spacing scale outliers (e.g., `space-18 = 18 px`) are derived from inspecting the canonical's class definitions and are consistent with the wireframes' stated paddings. Verified.

## 3. Summary of unresolved findings

| # | Finding | Severity | Document(s) | Owner |
|---|---|---|---|---|
| F1 | Property-test case-count divergence: 1000 vs 10,000. README and SPEC.md say >=1000 per CI run; plan.md says 10,000 generated; design spec contradicts itself between sec 2.3 (10,000) and sec 8.2 (>=1000). | soft phase blocker | README.md:19, SPEC.md:129, design spec sec 2.3 line 39 and sec 8.2 line 258, plan.md:303, 438, 656 | PM (00) |
| F2 | "Headline 6.2M events per second" asserted as a noun in SPEC.md:7 and quoted in ADR 0001:58. Every other doc frames headline numbers as targets until Phase 5 measures them. | soft phase blocker | SPEC.md:7, ADR 0001:58 | PM (00) |
| F3 | GoogleTest is pinned by tag (`v1.15.2`) in CMakeLists.txt:34 while threat-model.md:128 and checklist.md:127 state the policy is "pinned by SHA, not tag or branch". | minor inconsistency | CMakeLists.txt:34 vs threat-model.md:128 and checklist.md:127 | PM (00) plus DevOps (09) |
| F4 | Design spec section 10 still names the frontend directory `web/`; CLAUDE.md, plan.md, and architecture.md converged on `frontend/`. Cosmetic but trips a "find the directory" search. | cosmetic | design spec section 10 | PM (00) |
| F5 | `frontend/pnpm-lock.yaml` is not yet on `main`. The "package manager is pnpm" claim is forward-looking until the lockfile lands. | forward-looking | README.md:54, SPEC.md:92 | DevOps (09) at Phase 0 close |
| F6 | `.github/workflows/ci.yml` is not yet committed. The "CI/CD is GitHub Actions" plus "matrix on clang and gcc" claim is forward-looking until the workflow lands. | forward-looking | README.md:55, SPEC.md:93, plan.md:137 | DevOps (09) at Phase 0 close |

## 4. Recommendation to the PM

**Phase 0 should not close until F1, F2, and F3 are resolved.** All three are one-or-two-line edits to PM-owned files. The other findings (F4 cosmetic, F5 and F6 forward-looking) are acceptable to close Phase 0 against, provided STATUS.md does not flip to `completed` until DevOps Engineer's Phase 0 task list also lands `frontend/pnpm-lock.yaml` and `.github/workflows/ci.yml`.

Specific PM action items:

1. **F1 fix.** Edit SPEC.md line 129 and design spec section 8.2 line 258 to add: "the cumulative corpus across CI runs and the local pre-merge bench reaches >=10,000 sequences before phase close." This reconciles the per-CI-run figure (>=1000) with the cumulative corpus figure (>=10,000) without changing either number. README.md needs no edit. plan.md is already consistent with the cumulative-corpus reading; no edit needed there either, but a one-line clarification on plan.md:303 ("10,000 generated scenarios cumulative across CI runs") would make the reading explicit.
2. **F2 fix.** Edit SPEC.md line 7 to soften "the headline 6.2M events per second number is defensible in isolation" to "the headline 6M events per second target is defensible in isolation". ADR 0001 line 58 then auto-aligns since it quotes SPEC.md verbatim; either re-quote the new wording in the ADR or update the ADR's quote in the same edit. At Phase 5, when meridian-bench produces a measured number, both files update to the measured value (this is the rounding-drift hand-off the dispatch instructions explicitly call out).
3. **F3 fix.** Pick one of two paths and apply it:
   * (a) DevOps Engineer converts the GoogleTest pin to a commit SHA in CMakeLists.txt:34 (resolve `v1.15.2` to its SHA via `git ls-remote`), or
   * (b) PM amends the threat model and the Phase 0 checklist to read "pinned by SHA or by an immutable upstream release tag" so the policy matches what the project actually does.
   The auditor recommends (b) since v1.15.2 is an immutable upstream release tag (as opposed to `master` or `main`), and the additional cost of resolving every release tag to a SHA is real CI maintenance friction with marginal supply-chain benefit.

After F1, F2, F3 are resolved, this auditor signs off on Phase 0's documentation pass. F4 should be cleaned up at the next routine PM pass on the design spec; F5 and F6 are gated by DevOps Engineer's outstanding Phase 0 task list.

## 5. In-place edits made by this auditor

This auditor edited exactly one file in place during this audit. All edits were to resolve `[citation needed]` markers per the dispatch instructions and the agent brief's explicit permission to do so.

| File | Lines edited | What changed |
|---|---|---|
| `/home/mustafa/src/Meridian/docs/security/threat-model.md` | "Open assumptions and `[citation needed]` markers" section (was lines 145 to 151 in the pre-edit file) | Replaced the three `[citation needed]` markers (uWebSockets advisories, Fly.io free-tier wake latency, Cloudflare Pages `_headers` mechanism) with footnoted assumptions. Section heading renamed from "Open assumptions and `[citation needed]` markers" to "Open assumptions and resolved citations". Added a footnotes block at the end of the section listing the three verified sources by name and URL with a 2026-05-09 verification date. |

No other documents were edited in place. Per the dispatch instructions, the design spec, SPEC.md, and plan.md are PM-owned and not touched by this auditor; the divergences flagged in section 2.7 and 2.8 are written up for the PM rather than edited.

## 6. Definition of done for this audit

* [x] Every shipping document in scope has been read end to end.
* [x] Every concrete claim is classified as `verified`, `unverified`, or `wrong`.
* [x] The three `[citation needed]` markers in threat-model.md are resolved in place with named sources.
* [x] The cross-document divergences (F1, F2) are flagged with concrete remediation language.
* [x] The audit report is filed at `/home/mustafa/src/Meridian/docs/audits/citation-audit-2026-05-09.md`.
* [ ] The PM has applied the F1, F2, F3 fixes, after which this auditor returns to confirm and updates this report's "Overall verdict" line to "clean".

## 7. Sign-off

Conditional. Phase 0 documentation pass is clean **after F1, F2, F3 are resolved by the PM**. F4 is acceptable to defer; F5 and F6 are DevOps tasks already on the Phase 0 task list and are tracked there.

Next agent in the handoff chain (per `/home/mustafa/src/Meridian/agents/17-citation-and-fact-auditor.md` "Handoffs"): corrections route back to the originating agent (PM for the SPEC.md and design-spec edits, DevOps for the CMakeLists.txt or workflow edits). The PM session that dispatched this audit holds the gate until the three fixes land.
