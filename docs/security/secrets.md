# Meridian secrets reference

**Last updated:** 2026-05-09
**Companion documents:** `threat-model.md`, `checklist.md`.

## Purpose

This file is the canonical inventory of every secret the Meridian project depends on, where it is stored, who can rotate it, what its blast radius is, and the procedure to rotate it. It is intentionally conservative: a secret that is not listed here is a secret that should not exist.

The repository is public on GitHub at `MustafaNazeer/Meridian`. **No secret value is ever committed to the repo, in any branch, ever.** All secrets live either in GitHub Actions repository secrets, in Fly.io app secrets, or in the maintainer's password manager. `gitleaks` runs in CI as a required check and will fail any PR that introduces a credential pattern. A local pre-commit hook provides a second line of defense.

The Fly.io account and the Cloudflare account themselves are protected by a password and a 2FA device; those credentials are not stored in this repo and are out of scope for this document.

## Secrets inventory

### `FLY_API_TOKEN`

* **Where it lives:** GitHub Actions repository secret on `MustafaNazeer/Meridian`.
* **Where it is consumed:** the deploy workflow under `.github/workflows/` invokes `flyctl deploy` with `FLY_API_TOKEN` exported into the step's environment. It is not exposed to PR runs from forks.
* **What it grants:** deploy permission to the Meridian Fly app. The token is scoped to the single Fly app, not to the whole Fly account.
* **Who can rotate:** the maintainer. The `flyctl auth tokens` CLI manages tokens.
* **Rotation procedure:**
  1. In the Fly dashboard or via `flyctl auth tokens create deploy --app <app>`, mint a fresh token scoped to the Meridian app only.
  2. Update the `FLY_API_TOKEN` GitHub Actions repository secret to the new value.
  3. Trigger a no-op deploy (e.g., re-run the latest workflow on `main`) and confirm it succeeds.
  4. Revoke the previous token via `flyctl auth tokens revoke <token-id>` or via the Fly dashboard.
  5. Update the rotation log below.
* **Rotation cadence:** rotate once before the demo goes live publicly. After that, rotate on demand whenever the token may have leaked, and otherwise at least once every 12 months.

### Cloudflare Pages API token

* **Where it lives:** GitHub Actions repository secret on `MustafaNazeer/Meridian`, named `CLOUDFLARE_API_TOKEN`. Plus `CLOUDFLARE_ACCOUNT_ID` (not a secret, but recorded next to the token for convenience) as a separate repository variable.
* **Where it is consumed:** if the deploy uses a `wrangler` step (rather than relying on the Cloudflare Pages GitHub integration), the workflow exports `CLOUDFLARE_API_TOKEN` into the step's environment. It is not exposed to PR runs from forks. If the deploy instead uses the Pages GitHub integration, this token is unused at the workflow level and the integration handles auth via Cloudflare's GitHub App; the token is still kept in the repository secret store as a fallback.
* **What it grants:** deploy permission to the Cloudflare Pages project. Scope the token to "Edit Cloudflare Pages" on the specific project, not to the whole account.
* **Who can rotate:** the maintainer. Cloudflare's dashboard at "My Profile -> API Tokens" is the management surface.
* **Rotation procedure:**
  1. In the Cloudflare dashboard, mint a fresh API token using the "Edit Cloudflare Pages" template, scoped to the Meridian project.
  2. Update the `CLOUDFLARE_API_TOKEN` GitHub Actions repository secret to the new value.
  3. Trigger a deploy (push a no-op commit to `main` or re-run the latest deploy workflow) and confirm it succeeds.
  4. Revoke the previous token in the Cloudflare dashboard.
  5. Update the rotation log below.
* **Rotation cadence:** same as `FLY_API_TOKEN`. Rotate before the demo goes live publicly, then on demand or every 12 months.

### Fly.io app secrets (set via `fly secrets set`)

* **Where they live:** Fly's encrypted secret storage for the Meridian app. Set via `fly secrets set KEY=value --app <app>` and listed via `fly secrets list --app <app>`. Never set via the dashboard's plaintext UI; CLI only.
* **Where they are consumed:** the `meridian-server` process reads them from `process.env` (or the C++ equivalent: `std::getenv`) at startup and never logs the value. The hosting hardening pass will revisit whether the live demo actually needs any. The current minimum viable list is empty: the demo is read only, and runtime configuration (tape path, replay speed, listen port, allowed origins) lives in `meridian.toml` (non secret, baked into the Docker image).
* **What they grant:** depends on the secret. None are required currently.
* **Who can rotate:** the maintainer, via `fly secrets set` and `fly secrets unset`.
* **Rotation procedure:**
  1. `fly secrets set NEW_KEY=value --app <app>` adds or updates the secret. Fly automatically restarts the machine to pick up the change.
  2. `fly secrets unset OLD_KEY --app <app>` removes one no longer needed.
  3. Confirm the machine started cleanly via `fly status`.
  4. Update the rotation log below.
* **Rotation cadence:** any app secret introduced later must be rotated before the demo goes live publicly, and then on demand or at least every 12 months.

### `GITHUB_TOKEN` (the workflow-default token)

* **Where it lives:** automatically minted by GitHub Actions for every workflow run. Not stored anywhere persistent.
* **Where it is consumed:** standard `actions/checkout` and any workflow step that needs to push to the repo.
* **What it grants:** scoped per workflow run; we set `permissions:` blocks in each workflow to the minimum required. The default at the repo level is `contents: read` and `pull-requests: read` unless a specific workflow needs more.
* **Who can rotate:** not applicable; ephemeral.
* **Rotation cadence:** not applicable.

## What is explicitly not stored as a secret

* **The Fly.io account password and 2FA seed.** Lives in the maintainer's password manager. Not in the repo, not in GitHub Actions, not in Fly app secrets. Out of scope for this document.
* **The Cloudflare account password and 2FA seed.** Same.
* **The GitHub credentials.** Same.
* **NASDAQ ITCH 5.0 sample tape URLs and SHA-256 hashes.** These are public reference data, not secrets. They live in `bench/tape-manifest.json` (or equivalent) so CI can verify integrity before consuming a tape. Not a secret.
* **Domain configuration.** `meridian-orderbook.pages.dev` is a public URL. The Fly app name is public once the demo is live. Not secrets.

## Pre-flight check: nothing committed

The following commands should run in CI before any deploy and any milestone close:

```
gitleaks detect --source . --redact
git ls-files | xargs grep -lE '(FLY_API_TOKEN|CLOUDFLARE_API_TOKEN|api[_-]?key|secret|password)' || true
```

Either should return no matches in production code paths. Test fixtures are allowed to contain dummy values clearly labeled as such; the threat model treats them as data, not credentials.

## Rotation log

Append a one line entry every time a secret is rotated. Format: `YYYY-MM-DD | secret name | reason | actor`.

(empty)
