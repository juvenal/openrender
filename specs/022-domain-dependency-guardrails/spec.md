# Feature Specification: Domain-Dependency Guardrails

**Feature Branch**: `022-domain-dependency-guardrails`

**Created**: 2026-10-05

**Status**: Draft

**Input**: User description: "Domain-dependency guardrails for constitution Principle VIII (resolves registry DB-002 / GitHub #25). Add a ctest-runnable architecture check (label `architecture`), also runnable in CI, that maps source paths and build targets to the nine Principle VIII domains (renderer core = composition root; common/infrastructure layer allowed for all). It extracts dependency edges at source level and at link level, and fails on any dependency cycle or any cross-domain edge outside the constitution's allowed-dependency table, with a clear message naming the files/symbols. Known violations are listed in an allowlist that may only shrink; each allowlist entry references a registry entry in DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md. This spec adds the audit's violations V1–V21 to that registry as new entries starting at DB-005 (DB-004 / #27 already covers Geometry→renderer-core and Geometry→Scene-Interface coupling; overlapping findings cross-reference it), re-measured against current master. It also removes the vestigial cross-domain includes the audit flagged (V19) and reports per-domain counts of renderer-core uses as a tracked metric. Out of scope: fixing any registered violation (later specs 023–038)."

## Background

Constitution v1.2.0, Principle VIII, divides the renderer into nine domains and forbids any cross-domain dependency outside a fixed allowed-dependency table. **Rule 7** requires an automated check in CI that fails on dependency cycles and on disallowed edges. No such check exists yet (registry entry **DB-002**, GitHub **#25**).

A Principle VIII audit (2026-10-03, on master `079feda`) found 21 violation groups (V1–V21). Since then, spec 021 merged and changed some of the code involved:
- added registry entry **DB-004** (GitHub #27) for Geometry's dependence on the renderer core and on Scene Interface;
- added a fifth link-time-substitution pair (`surfaceCrossingDispatch`).

The registry today holds DB-001…DB-004. Most of the audit's findings are not registered yet, as rule 8 requires. Without an automated check, every later remediation spec (023–038) relies on reviewers spotting regressions by hand.

This feature is the safety net all later Principle VIII work depends on. It:
- makes every current violation visible and recorded;
- blocks new violations automatically;
- gives later specs a measurable way to show progress.

It deliberately fixes nothing except trivially dead includes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - New boundary violations are caught automatically (Priority: P1)

A contributor changes code and accidentally adds a dependency the constitution forbids. Examples: a Graphics State file including a Shading header, Texturing calling into Hiders, or a new cycle between two domains. When the test suite runs locally or in CI, the architecture check fails. The message names the offending file (or symbol), the two domains involved, and the rule broken, so the contributor can fix it before review.

**Why this priority**: Rule 7 requires this, and every later remediation spec depends on it. Without it, violations can come back unnoticed.

**Independent Test**: On an otherwise clean tree, introduce one forbidden include and, separately, one forbidden link-level reference. Each must make the check fail with an actionable message, and reverting the change makes it pass again.

**Acceptance Scenarios**:

1. **Given** the current tree with all known violations allowlisted, **When** the architecture check runs, **Then** it passes and reports the known violations as allowlisted.
2. **Given** a Graphics State source file that newly includes a Shading header, **When** the check runs, **Then** it fails, naming the file, the included header, both domains, and the broken rule.
3. **Given** a newly added reference from a Texturing file to a Hiders symbol that only shows up at link level, **When** the check runs, **Then** it fails, naming the referencing library, the symbol, and both domains.
4. **Given** a change that creates a dependency cycle between two domains not already recorded as a violation, **When** the check runs, **Then** it fails and reports the cycle path.
5. **Given** a dependency the allowed-dependency table permits (e.g. Hiders → Geometry through Geometry's headers), **When** the check runs, **Then** it does not report it.

---

### User Story 2 - Every known violation is registered and the allowlist can only shrink (Priority: P1)

A maintainer can open the violation registry and see every current Principle VIII violation, with evidence and a tracking reference. The check's allowlist matches it one to one. Later specs can remove allowlist entries as they fix violations, but nobody can add a violation to the allowlist without a matching registry entry. A violation that has been fixed but whose allowlist entry was left behind is reported, so the allowlist doesn't go stale.

**Why this priority**: Rule 8 requires every known violation to be registered with a tracking issue. The allowlist is what lets the check land without failing on day one; it must never become a way to sneak violations in.

**Independent Test**: Compare the allowlist against the registry: every entry maps to a registry ID. Remove a registry ID that an entry still points to, and the check fails. Add an allowlist entry that points to no registry ID, and the check fails. Leave behind an entry for a violation that no longer occurs, and the check reports it as stale.

**Acceptance Scenarios**:

1. **Given** the audit findings V1–V21 re-measured on current master, **When** this feature is complete, **Then** the registry contains an entry for each finding still present, numbered from DB-005, each with evidence, the rule broken, and a direction for the fix. Findings already covered by DB-004 cross-reference DB-004 instead of duplicating it.
2. **Given** an allowlist entry that references no existing registry entry, **When** the check runs, **Then** it fails and names the entry.
3. **Given** an allowlist entry whose violation no longer occurs in the code, **When** the check runs, **Then** it fails and names the entry as stale, so it gets removed (and its registry entry moved to Resolved).
4. **Given** an audit finding that turns out to be gone on current master, **When** the registry is populated, **Then** no entry is created for it, and the reason is recorded in the spec's evidence notes.

---

### User Story 3 - Progress toward a smaller renderer core is measurable (Priority: P2)

A maintainer reviewing a remediation spec can see, per domain, how many times the code uses renderer-core global state and functions, and compare that with the previous baseline. Later specs can show those numbers shrinking.

**Why this priority**: The renderer-core "god object" (V8) is cleaned up incrementally across many specs. A tracked metric makes that progress visible and stops it silently reversing. It's secondary to blocking new violations.

**Independent Test**: Run the check and read the reported per-domain counts. Add one renderer-core use in a Geometry file, and the Geometry count goes up by one.

**Acceptance Scenarios**:

1. **Given** the current tree, **When** the check runs, **Then** it reports one count of renderer-core uses per domain, plus a total.
2. **Given** a recorded baseline, **When** a domain's count rises above it, **Then** the check reports the increase, naming the domain and the difference. It may only warn here, not fail (see FR-012).

---

### User Story 4 - Dead cross-domain includes are removed (Priority: P3)

Includes that cross a domain boundary but are not actually used (audit finding V19) are removed. They no longer show up as violations or hide real dependencies.

**Why this priority**: It's cheap cleanup that removes noise from the check's output, but nothing depends on it.

**Independent Test**: Each removed include is gone, the tree builds cleanly, the full suite passes, and the check no longer reports those edges.

**Acceptance Scenarios**:

1. **Given** each include the audit flagged as vestigial, **When** it is removed and the tree rebuilt, **Then** the build succeeds with no new warnings and the full test suite stays green. If an include turns out to be needed, it stays and is registered as a real violation.

---

### Edge Cases

- **Headers with the same name in several domains** (e.g. a forwarding header in infrastructure that re-exports a Shading header): the check classifies a header by its actual location, and treats a forwarding header as a dependency on what it forwards to, so forwarding can't hide a cross-domain edge.
- **Files outside the nine domains:**
  - Renderer core (composition root): may depend on every domain.
  - Infrastructure/common: every domain may depend on it, but it may not depend on any domain.
  - Executables, tools, language bindings and the preview subsystem: act as consumers. Their reach into domain internals is reported against the registry (V21) instead of being silently allowed.
  - Link-time stub/substitution sources: classified explicitly.
- **Generated parser/lexer sources:** classified by the domain of the grammar they come from.
- **Platform differences:** the link-level analysis works on both supported platforms (macOS and Linux). It uses whatever symbol-inspection tooling each platform provides, and reports clearly (rather than passing silently) if that tooling is missing.
- **Build options that remove code** (e.g. building without the JIT, or without optional image formats): the check still runs, and judges only the code actually built.
- **Symbols defined twice** (infrastructure code compiled into more than one library): not reported as a cross-domain edge.
- **A violation that appears at both source and link level:** reported once, attributed to one registry entry.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The project MUST provide an architecture check that runs as part of the standard test suite under a dedicated `architecture` label, can be run on its own, and runs in every CI configuration that runs the test suite.
- **FR-002**: The check MUST classify every source file and every built library into exactly one of:
  - the nine Principle VIII domains;
  - the renderer core (composition root);
  - the common/infrastructure layer;
  - consumers (executables, tools, bindings, preview).
  The classification MUST live in one reviewable, version-controlled mapping, and an unclassified file MUST make the check fail and name the file.
- **FR-003**: The check MUST derive source-level dependency edges from the includes actually used when compiling each file (not from a hand-maintained list), mapping both ends to their domains.
- **FR-004**: The check MUST derive link-level dependency edges from each built library's undefined and defined symbols, attributing each cross-library symbol reference to the domains of the referencing code and of the defining code.
- **FR-005**: The check MUST encode the constitution's allowed-dependency table exactly, including:
  - the renderer core may depend on everything, and no domain may depend on it;
  - every domain may depend on infrastructure, and infrastructure may depend on no domain;
  - each plug-in domain may depend only on its own published interface.
  It MUST report any edge outside the table.
- **FR-006**: The check MUST detect dependency cycles between domains and report each cycle with its full path.
- **FR-007**: Every reported violation MUST name: the source file or library, the included header or referenced symbol, the source and target domains, and the constitutional rule broken.
- **FR-008**: Known violations MUST be listed in a version-controlled allowlist. Allowlisted violations are reported as known and do not fail the check; anything not allowlisted fails it.
- **FR-009**: Every allowlist entry MUST reference an existing registry entry ID in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. An entry without a valid registry reference MUST fail the check.
- **FR-010**: An allowlist entry whose violation no longer occurs MUST fail the check as stale, so the allowlist only shrinks as violations are fixed.
- **FR-011**: The registry MUST gain entries, numbered from DB-005, for every audit finding (V1–V21) still present when re-measured on current master. Each entry gives:
  - the rule broken;
  - concrete evidence (file and line, or symbol);
  - the size of the problem;
  - a direction for the fix;
  - a tracking reference.

  Findings already covered by DB-004 MUST cross-reference DB-004 instead of duplicating it. Findings no longer present MUST be left out, and the reason recorded.
- **FR-012**: The check MUST report, per domain, the number of uses of renderer-core global state and functions, and compare them with a recorded baseline. An increase is reported as a warning; it does not fail the check.
- **FR-013**: The includes the audit flagged as vestigial (V19) MUST be removed where removal leaves the build and test suite unchanged. Any that prove necessary MUST be kept and registered as real violations.
- **FR-014**: The check's purpose, how to read its output, how to classify a new file, and how to update the allowlist and registry when fixing a violation MUST be documented for contributors, and referenced from the existing developer documentation.
- **FR-015**: Adding the check MUST NOT change rendering output or any existing test result; the full test suite MUST stay green.

### Key Entities

- **Domain map:** the mapping from source locations and built libraries to the nine domains, the renderer core, the infrastructure layer and consumers. Version-controlled, reviewable, and exhaustive: every file maps to exactly one entry.
- **Allowed-dependency table:** the constitution's table of which domain may depend on which. The check's only source of truth for allowed edges.
- **Dependency edge:** a source → target relationship between two domains, of kind *source-level* (an include) or *link-level* (a symbol reference), carrying the concrete file/header or library/symbol evidence.
- **Allowlist entry:** a known violating edge or edge pattern, tied to exactly one registry ID.
- **Registry entry (DB-NNN):** a recorded violation in `DOMAIN_BOUNDARIES.md`, with rule, evidence, size, direction and tracking reference; moves to Resolved when fixed.
- **Renderer-core usage metric:** per-domain counts of renderer-core uses, with a recorded baseline for comparison.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A single injected forbidden dependency of each kind is detected in 100% of trials, and its message lets a contributor find the offending line without other tools: (a) a disallowed include, (b) a disallowed link-level symbol reference, (c) a new cycle.
- **SC-002**: On the current tree the check passes, and every violation it reports maps to a registry entry. There are no unregistered violations and no allowlist entries without a registry reference.
- **SC-003**: Every audit finding (V1–V21) ends in exactly one of three states: registered (DB-005 onward), cross-referenced to DB-004, or documented as no longer present. None is left unaccounted for.
- **SC-004**: The full test suite passes with the check included, with no change to rendering output or to any existing test result.
- **SC-005**: The check adds no more than 60 seconds to a full test-suite run on a typical development machine.
- **SC-006**: A contributor following the documentation can classify a new file, or retire a fixed violation (allowlist entry removed, registry entry moved to Resolved), in under 15 minutes.
- **SC-007**: The per-domain renderer-core usage counts are reported on every run and are reproducible: two runs on the same tree give identical numbers.

## Assumptions

- Edges are judged against the allowed-dependency table in constitution v1.2.0, Principle VIII. The infrastructure layer (common utilities and shared headers) is usable by every domain but may depend on no domain, and the renderer core is the only place allowed to depend on everything.
- "CI" means every automated configuration that already runs the test suite (currently the release workflow). Adding a new per-change CI workflow is out of scope, but the check must be included wherever the suite runs.
- Link-level analysis covers the libraries and executables the project builds, on macOS and Linux, using the symbol tools standard on each platform. A missing tool is reported clearly, never silently skipped.
- When the audit's evidence and current master disagree, current master wins. Every registry entry is re-measured, not copied from the audit.
- New registry entries get tracking references, i.e. GitHub issues, filed as part of this work so that rule 8 is met. Filing them is outward-facing and done only with the maintainer's approval at the relevant pause.
- Fixing any registered violation, other than removing dead includes, is out of scope. Specs 023–038 do that work.
- Per the maintainer's workflow, all work happens in this spec's worktree, changes are staged but never committed by the agent, and work pauses for review at each phase.
