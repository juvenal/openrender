# DRAFT — Constitution Amendment for Review

**Status**: PROPOSED, NOT RATIFIED. This file is a review artifact.
`.specify/memory/constitution.md` is unchanged and remains the authority
until the maintainer approves this amendment.

**Proposed version change**: 1.1.0 → **1.1.1** (PATCH)
**Scope**: Principle VII only. No other principle is touched.
**Raised by**: `/speckit-analyze` finding **D1** on `specs/012-jit-parity-followups`
(MEDIUM) — Principle VII mandates a `site` folder that does not exist, so every
feature plan's Constitution Check either fails against it or quietly
reinterprets it.

---

## 1. Why this is a PATCH, not a MINOR

The two corrections below change **no one's obligations**. In both cases the
principle already meant the thing that actually exists; the text names it
incorrectly. Documentation must live in a Hugo site, be written in Markdown
with front matter, and be deployed by GitHub Actions — before and after,
identically. Only the identifiers are fixed.

Requirements that are *not* currently satisfied by the repository are
deliberately **left in force and unamended** — see §5. Weakening a MUST to
match a gap would be dilution, which is exactly what the constitution's own
Governance section forbids.

---

## 2. The correction, with evidence

| # | Constitution says | Repository actually has | Evidence |
|---|---|---|---|
| 1 | a dedicated `site` folder | `docs/site/` — there is no `site/` at any level | `specs/001-hugo-docs-migration/spec.md:6` records the maintainer's own original instruction: *"Place this new, reviewed content into 'docs/site/content' folder in the project root."* `spec.md:77` (FR-001) makes `docs/site/content` the mandated target. Both workflows build from `docs/site`. |
| 2 | `config.toml/yaml/json` | `docs/site/hugo.yaml` | `hugo.yaml` is the modern config filename for Hugo ≥ v0.110, which supersedes `config.*`. `docs-deploy.yml` pins `HUGO_VERSION: 0.152.2`. |
| 3 | deployment on "pushes to main branch" | the default branch is `master`; **no `main` branch exists** locally or on `origin` | `refs/remotes/origin/HEAD` → `refs/remotes/origin/master`. `docs-deploy.yml` triggers on `master` and is the live deployer. |

**Point-in-time note on the evidence above**: rows 2 and 3 cite
`docs-deploy.yml` as the live Pages deployer. That is accurate as of this
draft, but T003i retires that file once `deploy-site.yml` absorbs its build
(see §5). The evidence stands as written — it is a citation of the state that
justified the correction, not a description of the current topology.

**Item 3 is the one judgment call in this draft**, flagged so it can be vetoed
independently of items 1–2. Since no `main` has ever existed in this
repository, the phrase cannot be read as a branch mandate that the repo is
violating — it is generic phrasing carried in from a template. Naming the
default branch instead is therefore the same class of factual correction as
items 1–2. If you disagree and consider "main" a genuine unmet requirement
(i.e. the repository should be renaming `master`), pull this line out of the
amendment and it becomes a §5 follow-up instead; items 1–2 stand on their own.

---

## 3. Principle VII — current text (verbatim, `constitution.md:46`)

> Project documentation and development details MUST be maintained in a dedicated `site` folder using Hugo as the static site generator. The `site` folder MUST contain all source files for the project documentation website. Hugo configuration files (config.toml/yaml/json) MUST define the site structure, themes, and deployment settings. The site MUST be regularly updated to reflect changes and new features in the project. A `.github/workflows` folder MUST contain GitHub Actions workflows that automate site deployment. CI/CD pipelines MUST handle both the site content deployment and the project source code build/test processes. Site content MUST be written in Markdown format with appropriate front matter for Hugo processing. Site deployment MUST occur automatically on pushes to main branch and release tags.

## 4. Principle VII — proposed replacement

> Project documentation and development details MUST be maintained in a dedicated `docs/site` folder using Hugo as the static site generator. The `docs/site` folder MUST contain all source files for the project documentation website. The Hugo configuration file (`docs/site/hugo.yaml`) MUST define the site structure, themes, and deployment settings. The site MUST be regularly updated to reflect changes and new features in the project. A `.github/workflows` folder MUST contain GitHub Actions workflows that automate site deployment. CI/CD pipelines MUST handle both the site content deployment and the project source code build/test processes. Site content MUST be written in Markdown format with appropriate front matter for Hugo processing. Site deployment MUST occur automatically on pushes to the repository's default branch (`master`) and release tags.

**Diff summary** — four substitutions, nothing added or removed:

- `dedicated \`site\` folder` → `dedicated \`docs/site\` folder`
- `The \`site\` folder MUST contain` → `The \`docs/site\` folder MUST contain`
- `Hugo configuration files (config.toml/yaml/json) MUST define` → ``The Hugo configuration file (`docs/site/hugo.yaml`) MUST define``
- `on pushes to main branch and release tags` → `` on pushes to the repository's default branch (`master`) and on release tags``

Every other clause — Hugo as generator, regular updates, `.github/workflows`,
CI/CD covering both site and source, Markdown with front matter, automatic
deployment — is carried through **unchanged and still MUST**.

---

## 5. NOT amended — two real gaps, now scheduled for remediation

These are cases where the repository fails the constitution, not cases where
the constitution is wrong. The amended text therefore **still does not relax
anything** — but the reason has changed since this draft was first written.
The gaps are no longer being left open for separate disposition: the
repository is being brought into compliance instead, under
`specs/012-jit-parity-followups/tasks.md` **T003e-T003i** (Phase 1b). Once
those land, Principle VII's text is fully satisfied by the CI, which
*strengthens* this amendment rather than qualifying it.

**Gap A — no site deployment on release tags.** → **T003h**
`.github/workflows/release.yml` triggers on `v[0-9]+.[0-9]+.[0-9]+*` tags but
contains no Hugo, `docs/site`, or Pages step (verified by grep: zero matches).
Principle VII's "and release tags" is therefore unimplemented. **The
constitution is correct and the CI is incomplete** — the fix belongs in
`release.yml`, and T003h adds a job there that `uses:
./.github/workflows/deploy-site.yml`. Note that `release.yml`'s workflow-level
grant is `contents: write` only; since a called workflow cannot exceed its
caller's grant, the calling job must declare `pages: write` and `id-token:
write` itself.

**Gap B — `deploy-site.yml` targets a branch that does not exist, and duplicates the Pages deployer.** → **T003e-T003g, T003i**
`.github/workflows/deploy-site.yml` triggers on `push` to `main` — a branch
that has never existed — plus `workflow_dispatch`. It is therefore
**manual-only, not dead**: it can still be run by hand, and if it is, it
deploys to GitHub Pages *without* the `concurrency: group: "pages"` guard that
`docs-deploy.yml` carries, and with **no `if:` gate on its deploy job at all**,
so a dispatch from any branch publishes to production.

Two workflows currently target Pages, which constrains the fix in a way worth
recording: naively repointing `deploy-site.yml` at `master` while keeping its
`docs/site/**` path filter would make it an *exact duplicate trigger* of
`docs-deploy.yml`. The concurrency guard serializes but does not dedupe, and
`deploy-site.yml`'s build is inferior on four counts (unpinned Hugo, no Dart
Sass, no `configure-pages`/baseURL, no `fetch-depth: 0`), so the worse build
could land last. (An earlier revision of this section listed "no link
validation" as a fifth count; that was incorrect — `docs-deploy.yml` invokes a
`link-validator.sh` that has never existed in any commit, under `|| true`, so
neither workflow validates links today. Task T003e1 now authors a real one at
`docs/tools/link-validator.sh`.) The tasks therefore consolidate: `deploy-site.yml` absorbs
`docs-deploy.yml`'s build steps and becomes the single implementation
(T003e), gains the `master` trigger plus `workflow_call` (T003f) and both
guards (T003g), and `docs-deploy.yml` is retired last (T003i).

**These remain workflow changes, not constitution changes.** No wording in §4
depends on them.

---

## 6. Governance compliance

The constitution's Governance section (`constitution.md:78-83`) requires four
things of an amendment. Each is discharged as follows:

| Requirement | Status |
|---|---|
| **Documented rationale** | §2 above. Primary evidence is `specs/001-hugo-docs-migration/spec.md:6,77`, where `docs/site/content` was specified by the maintainer directly. |
| **Maintainer approval** | **PENDING** — that is why this is a draft file and not an edit to `constitution.md`. |
| **Migration plan for breaking changes** | **None required.** No compliance obligation changes; no file moves, no CI changes, no spec rework. Documentation already lives where the amended text says it must. |
| **Version increment** | 1.1.0 → 1.1.1 (PATCH: wording/typo/clarification, no semantic change). |

---

## 7. Proposed replacement for the Sync Impact Report header

The existing header block (`constitution.md:1-14`) is the v1.0.0 → v1.1.0
report. It claims five templates were updated for Principle VII, all marked
✅. **That claim does not hold**: `grep -rn "Principle VII|Documentation and
Site|Hugo" .specify/templates/` returns **zero matches** across all six
template files. The replacement below states the verified status rather than
carrying those ✅ marks forward.

```
<!--
Sync Impact Report:
Version change: 1.1.0 → 1.1.1
Modified principles: VII. Documentation and Site Management (factual
  corrections only — no change to any obligation)
  - `site` → `docs/site` (the folder that exists; established by
    specs/001-hugo-docs-migration)
  - `config.toml/yaml/json` → `docs/site/hugo.yaml` (actual Hugo config;
    `hugo.yaml` supersedes `config.*` for Hugo >= v0.110)
  - "pushes to main branch" → "pushes to the repository's default branch
    (`master`)" (no `main` branch exists in this repository)
Added sections: None
Removed sections: None
Templates requiring updates:
  - Verified: no file in .specify/templates/ references Principle VII,
    "Documentation and Site Management", `site`, or Hugo. No template
    update is required by this amendment.
  - NOTE: the previous (1.0.0 -> 1.1.0) report asserted five templates were
    updated for Principle VII. That assertion is not supported by the
    templates as they stand and is not carried forward.
Follow-up TODOs:
  - Two CI gaps were found against Principle VII. Neither is a reason to
    relax the principle; both are being fixed so the repository complies.
    Scheduled as specs/012-jit-parity-followups/tasks.md T003e-T003i.
  - (a) release.yml performs no site deployment, so the release-tag clause
    is unimplemented -> T003h adds a job calling deploy-site.yml.
  - (b) deploy-site.yml triggers on push to `main` (never existed), has no
    `concurrency: group: "pages"` guard, and has no `if:` gate on its
    deploy job -> T003e-T003g repoint it at `master`, port in
    docs-deploy.yml's superior build, and add both guards; T003i then
    retires docs-deploy.yml so exactly one Pages deployer remains.
-->
```

## 8. Proposed footer change (`constitution.md:87`)

```diff
-**Version**: 1.1.0 | **Ratified**: 2025-12-07 | **Last Amended**: 2025-12-08
+**Version**: 1.1.1 | **Ratified**: 2025-12-07 | **Last Amended**: 2026-08-26
```

`Ratified` is unchanged — it records original adoption, not amendment.

---

## 9. Out of scope for this amendment

Four historical spec artifacts also refer to the `site` folder, inheriting the
wording from the constitution:

- `specs/001-hugo-docs-migration/plan.md:40`
- `specs/001-hugo-docs-migration/spec.md:117` (DOC-001)
- `specs/002-wayland-display-driver/spec.md:104` (DOC-001)
- `specs/003-update-shader-extension/plan.md:32`

These are **completed-feature records**, not live governance. Rewriting them
would falsify the historical record. They are listed only so the inconsistency
is known and not rediscovered later. (Note that `001/spec.md` is internally
inconsistent already: line 6 and FR-001 say `docs/site/content` while
DOC-001 at line 117 says `site` — further evidence the `site` wording was
inherited boilerplate rather than an intended location.)

---

## 10. To apply

On your approval I will make exactly three edits to
`.specify/memory/constitution.md` — header block (§7), Principle VII line 46
(§4), footer line 87 (§8).

**This file's disposition is your call.** The Sync Impact Report inserted by
§7 is a *summary*; it does not carry the `specs/001-hugo-docs-migration/spec.md:6,77`
citation, the gap analysis, or the reasoning. Since Governance requires a
*documented rationale* for the amendment, my default is to **keep** this file
as the durable record, renamed to `.specify/memory/constitution-1.1.1-rationale.md`
and with its status header changed from PROPOSED to RATIFIED. Say so if you
would rather it be deleted once applied.

No commit will be made unless you ask for one.
