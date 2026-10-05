# Data Model: Domain-Dependency Guardrails (spec 022)

The entities are what the `archcheck` tool reads, derives and reports. The file formats are fixed in [contracts/data-files.md](contracts/data-files.md).

## Class

The ownership category of a file.

| Value | Kind | Notes |
|---|---|---|
| `SceneInterface`, `GraphicsState`, `Geometry`, `Hiders`, `Shading`, `Texturing`, `ShaderDSO`, `Procedures`, `Display` | domain | The nine Principle VIII domains |
| `Core` | composition root | May depend on everything; no domain may depend on it |
| `Infra` | shared layer | Every class may depend on it; it may depend on no domain and not on Core |
| `Consumer` | executables, tools, bindings, preview, dataviewer | May depend on anything; reported separately (R6) |

## DomainMap

- **Fields:** an ordered list of `Rule { pattern, class, kind }`.
  - `kind` is `prefix` (directory prefix, repo-relative) or `file` (exact repo-relative path, or a build-relative path for generated sources marked `@build/`).
- **Resolution:** a `file` rule beats a `prefix` rule; among prefixes, the longest match wins.
- **Validation:**
  - Every scanned file (under `src/`, plus generated sources referenced by depfiles) MUST resolve to exactly one class, or the run fails with `unclassified: <path>` (FR-002).
  - Duplicate patterns are an error.
  - `tests/` is never scanned.

## RuleTable (allowed dependencies)

- **Fields:** a set of `Allow { from: Class, to: Class }`, plus wildcard forms (`Core -> *`, `* -> Infra`, `Consumer -> *`).
- **Validation at load:** the graph restricted to domains plus Infra MUST be acyclic, and `Infra -> <domain>` MUST NOT appear. Otherwise exit 2 (configuration error).
- **Source:** mirrors constitution v1.2.0 Principle VIII. A header comment cites the constitution version, and any change requires a constitution amendment.

## Edge

- **Fields:**
  - `kind`: `include` | `link`
  - `from_file` (repo-relative), `from_class`
  - `to`: the resolved header path for `include`; the mangled symbol for `link`
  - `to_files` (`link`: the defining source files)
  - `to_class` (`include`: one class; `link`: one or more classes)
  - `display`: the demangled symbol, for `link`
- **Identity:** `(kind, from_file, to)`. Unique per run; duplicates from multiply-compiled sources are merged.
- **Derivation:**
  - `include` edges come from direct `#include`s resolved through a TU depfile (research R2).
  - `link` edges come from undefined symbols resolved to project definers (research R3, with its attribution rules).
- **Status (computed per run and platform):**
  - `allowed`: `(from_class, to_class)` is in the RuleTable, or from/to are the same class.
  - `allowlisted`: not allowed, but matches an AllowlistEntry that applies to this platform.
  - `violation`: neither. Fails the run.
  - `consumer-reach`: `from_class == Consumer` and `to` is a domain's internal file. Reported, never fails.

## AllowlistEntry

- **Fields:** `registry_id` (`DB-NNN`), `kind`, `from_file`, `to`, optional `platform` (`macos` | `linux`; absent = both), optional trailing comment (ignored).
- **Identity:** `(kind, from_file, to, platform)`. Duplicates are an error.
- **Validation (per run):**
  - `registry_id` MUST be an **Open** RegistryEntry, else `unknown-registry-id` or `resolved-registry-id` (fail, FR-009).
  - If the entry applies to the current platform and no edge with the same `(kind, from_file, to)` exists, it fails as **stale** (FR-010).
  - An entry for the other platform is neither matched nor checked (FR-010a).
  - An entry whose edge exists but is *allowed* (e.g. after a rule change) also fails as stale, since it is unnecessary.
- **Lifecycle:** created only during this spec's bootstrap. Afterwards entries are only deleted, when a violation is fixed. Adding one requires a new or existing Open registry entry and review.

## RegistryEntry

- **Source:** `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. A `### DB-NNN — title` heading in the Open section (before `## Resolved`) is Open; one after it is Resolved.
- **Fields used by the tool:** `id` and `status` (Open/Resolved). Everything else (rule, tracking, evidence, direction, history) is human content, validated only by review.
- **Content requirements for new entries (DB-005…, FR-011):** rule broken; evidence (file:line or symbol) re-measured on the current tree; size; direction; tracking (the GitHub issue(s) of the remediation spec(s) that fix it); a "see DB-004" cross-reference where it overlaps.

## MetricBaseline and MetricSample

- **MetricSample (per run):** for each domain and Infra:
  - `refs`: the number of `CRenderer::`/`CRendererContext::` member references;
  - `distinct`: the number of distinct members referenced;
  - plus totals.
  - Counted over comment- and string-stripped source text (research R8).
- **MetricBaseline:** the same shape, stored in `metrics-baseline.txt`.
- **Comparison:** `sample > baseline` → warning; `sample < baseline` → reminder to lower the baseline; never a failure (FR-012). Deterministic (SC-007).

## Report

- **Sections, in order:**
  1. configuration summary (map, rules, allowlist, platform);
  2. **violations** (fail);
  3. stale or invalid allowlist entries (fail);
  4. cycles (each with path; failing ones marked);
  5. allowlisted edges grouped by registry ID, with counts;
  6. consumer reach;
  7. metric table with baseline deltas.
- **Exit code:**
  - `0` = no violations and no allowlist errors (warnings allowed);
  - `1` = one or more violations, stale or invalid entries, or failing cycles;
  - `2` = configuration or environment error (unclassified file, bad rule table, missing `nm`, incomplete build tree).
- **Formats:** human-readable text (default) or `--format json` (Principle IV). Schema in [contracts/archcheck-cli.md](contracts/archcheck-cli.md).
