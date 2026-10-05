# Implementation Plan: Domain-Dependency Guardrails

**Branch**: `022-domain-dependency-guardrails` | **Date**: 2026-10-05 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/022-domain-dependency-guardrails/spec.md`

## Summary

Add `archcheck`, a C++20 test-only tool run by ctest (`Architecture_DomainBoundaries`, label `architecture`, so it also runs in the release workflow's existing `ctest` steps). It classifies every source file into the Principle VIII domains, renderer core, infrastructure or consumers, through a reviewable domain map. It extracts two kinds of edges:
- **source-level:** each file's direct `#include`s, resolved through the compiler depfiles;
- **link-level:** per-object `nm` undefined→defined symbol references.

It then checks every cross-class edge against a data copy of the constitution's allowed-dependency table. Any edge that is neither allowed nor on the per-edge allowlist fails. The allowlist is platform-taggable, and every entry must reference an Open registry entry. Stale and invalid entries fail too.

The tool also:
- reports domain cycles;
- reports consumer reach into internals;
- reports a deterministic per-domain renderer-core usage metric (reference count + distinct members) against a baseline.

The rest of the spec:
- populate the registry (DB-005 onward) from the tool's own re-measurement of the audit findings, cross-referencing DB-004;
- seed the allowlist;
- remove the vestigial cross-domain includes;
- document the workflow;
- file one tracking issue per remediation spec (023–038), only after maintainer approval.

Every design decision is recorded in [research.md](research.md).

## Technical Context

**Language/Version**: C++20 (project standard, `-Werror`), plus CMake ≥ 3.19 for wiring.

**Primary Dependencies**:
- the C++ standard library (`<filesystem>`, `<cxxabi.h>` demangler);
- the platform `nm` (Apple/LLVM or GNU binutils), already present wherever the project builds;
- `ninja` only for Ninja-generated trees (`ninja -t deps`).

No new third-party dependency (research R1).

**Storage**: four version-controlled text files under `tests/architecture/` (domain map, rules, allowlist, metrics baseline), plus the existing registry `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`.

**Testing**:
- ctest fixture scenarios (tiny compiled source trees with real depfiles and objects; exit code and message asserted);
- a C++ unit-test executable for the parsers;
- the full existing suite stays green.

**Target Platform**: macOS (arm64 and x86_64) and Linux (Ubuntu 24.04, GCC 13), the platforms of the release workflow.

**Project Type**: a test/CI tool inside an existing C++ CMake project (the renderer).

**Performance Goals**: the whole check adds at most 60 s to a full `ctest` run (SC-005). The measured `nm` pass is 2.2 s for 248 objects; parsing is linear in the depfile size.

**Constraints**:
- deterministic output (SC-007);
- never fails silently: missing tooling or an incomplete build tree is exit 2 (FR-001, edge cases);
- no rendering or test-result changes (FR-015);
- no commits by the agent (maintainer workflow).

**Scale/Scope**:
- about 600 project sources and 248 objects;
- edges in the low thousands;
- an allowlist in the hundreds of lines;
- the registry grows by roughly 15–20 entries;
- 16 tracking issues;
- about a dozen include removals.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Status |
|---|---|---|
| I. Clean Code | Small focused units (map, rules, edge extraction, allowlist, report, metric), no magic numbers (thresholds and paths come from the config), "why" comments only | PASS |
| II. Language Standards | C++20 under the project's `-Werror` flags; no compiler extensions; POSIX `popen` for `nm` (Unix-only per Principle VI) | PASS |
| III. TDD (non-negotiable) | Fixture scenarios and parser unit tests are written first and fail before the tool exists (research R10); the user-approval gate is the task review pause | PASS |
| IV. CLI | `archcheck` takes POSIX options and `--help`, writes text or `--format json` to stdout and diagnostics to stderr, and uses exit codes 0/1/2 (contracts/archcheck-cli.md) | PASS |
| V. Minimal Dependencies | No new dependency: C++ stdlib plus the toolchain's own `nm`; Python deliberately avoided (research R1) | PASS |
| VI. Platform Targeting | macOS + Linux; per-platform allowlist tags; the macOS leading-underscore difference is normalized | PASS |
| VII. Documentation and Site Management | Contributor documentation goes to `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` plus a pointer in `CLAUDE.md`, following current repo practice. **Note:** the `site/` folder that VII names doesn't exist. That's a pre-existing gap tracked by the pending constitution 1.1.1 draft, not introduced or worsened here | PASS (pre-existing gap noted) |
| VIII. Domain Separation | See the gate below | PASS |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] Domains touched are named.
  - The tool and its data live under `tests/` and are classified as consumer/test, never as a domain.
  - Production changes are limited to **removing** includes in Graphics State (`state/options.cpp`, `state/attributes.cpp`), Shading (`libshader/shading/shading.cpp`), Texturing (`texture/pointCloud.cpp`) and Hiders (`hiders/reyes.cpp`), plus any others the tool proves unused.
- [x] No new cross-domain dependency is added. The only production edits delete edges.
- [x] No cross-domain dependency is added or altered, so no interface change is needed. The tool's own interfaces (CLI, data files) are specified in `contracts/`.
- [x] No hider-specific special case is involved.
- [x] No plug-in interface changes.
- [x] Registered violations are not extended. The removed includes reduce what would otherwise be registered, and DB-002 (#25) moves to **Resolved** when this lands.

**Post-design re-check (after Phase 1)**: unchanged, all PASS. The design adds no production dependency, and the data model keeps the rule table as a verbatim mirror of the constitution, so the check can't drift from it without an amendment.

## Project Structure

### Documentation (this feature)

```text
specs/022-domain-dependency-guardrails/
├── spec.md
├── plan.md                      # this file
├── research.md                  # Phase 0 decisions R1–R12
├── data-model.md                # entities: Class, DomainMap, RuleTable, Edge, AllowlistEntry, RegistryEntry, Metric, Report
├── quickstart.md                # validation guide
├── contracts/
│   ├── archcheck-cli.md         # CLI, exit codes, text/JSON report
│   └── data-files.md            # domain-map / rules / allowlist / baseline / registry formats
├── registry-reconciliation.md   # created in implement: V1–V21 → DB-NNN | see DB-004 | gone (reason)
├── checklists/requirements.md
└── tasks.md                     # /speckit-tasks
```

### Source Code (repository root)

```text
tests/architecture/
├── CMakeLists.txt               # builds archcheck + unit tests; registers Architecture_DomainBoundaries and ArchFixture_* (label: architecture)
├── domain-map.txt               # file/prefix → class (research R4)
├── domain-rules.txt             # mirror of the constitution v1.2.0 allowed-dependency table (R5)
├── allowlist.txt                # one line per concrete violating edge, DB-NNN, optional platform tag (R7)
├── metrics-baseline.txt         # per-class refs/distinct baseline (R8)
├── archcheck/
│   ├── main.cpp                 # CLI, orchestration, exit codes
│   ├── config.{h,cpp}           # domain map, rules (acyclicity check), allowlist, baseline, registry parsing
│   ├── includes.{h,cpp}         # direct-include parsing + depfile resolution (Makefiles .o.d; Ninja via `ninja -t deps`)
│   ├── symbols.{h,cpp}          # batched `nm`, symbol tables, attribution rules, demangling
│   ├── graph.{h,cpp}            # edge classification, SCC/cycle reporting
│   ├── metrics.{h,cpp}          # comment/string-stripped CRenderer:: scan
│   └── report.{h,cpp}           # text + JSON writers (deterministic ordering)
├── unit/test_archcheck_parsers.cpp
└── fixtures/
    ├── clean/                   # passes
    ├── forbidden-include/       # GraphicsState -> Shading include
    ├── forbidden-link/          # Texturing -> Hiders symbol
    ├── new-cycle/
    ├── stale-entry/
    ├── unknown-id/  resolved-id/
    ├── unclassified/
    ├── other-platform-tag/
    ├── metric-increase/
    └── forwarding-shim/
        # each: src/<domains>/…, domain-map.txt, domain-rules.txt, allowlist.txt, metrics-baseline.txt, REGISTRY.md

tests/CMakeLists.txt             # add_subdirectory(architecture)
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md   # + "Architecture check" section; + DB-005… entries; DB-002 → Resolved
CLAUDE.md                        # pointer to the check (dev workflow)
src/ri/state/{options,attributes}.cpp, src/libshader/shading/shading.cpp,
src/ri/texture/pointCloud.cpp, src/ri/hiders/reyes.cpp   # vestigial #include removals only (R12)
```

**Structure Decision:** the check lives entirely under `tests/architecture/`, next to the other test suites and built by the existing `tests/CMakeLists.txt`. Production sources change only where a dead include is deleted. Fixture source trees are compiled as OBJECT libraries of their own, so fixtures get real depfiles and real object symbols without touching the renderer's targets.

## Implementation phases (overview for /speckit-tasks)

Each phase ends with a pause for the maintainer's review and commit.

1. **Setup:** the `tests/architecture/` scaffold, CMake wiring, empty data files.
2. **Foundational (tests first):**
   - parser unit tests and the fixture scenarios, written and failing;
   - config loading, domain map, rule table with the acyclicity check.
3. **US1 (P1), detection:**
   - include-edge extraction (depfile resolution), link-edge extraction (`nm`), classification, violation and cycle reporting;
   - fixture scenarios pass;
   - text and JSON report.
4. **US2 (P1), registry and allowlist:**
   - allowlist parsing with platform tags, registry Open/Resolved parsing, stale and bad-ID detection;
   - `--emit-allowlist` bootstrap;
   - **re-measure V1–V21**, write DB-005…, write `registry-reconciliation.md`, seed `allowlist.txt`;
   - the real-tree test passes;
   - draft the 16 tracking-issue texts, **filed only after maintainer approval**, then link them from the registry.
5. **US3 (P2), metric:** the scanner, baseline file, warnings; deterministic output.
6. **US4 (P3), cleanup:** remove vestigial includes one at a time, with build plus full suite after each; registry/allowlist updated if any prove necessary; Linux validation path.
7. **Polish:**
   - contributor documentation (`DOMAIN_BOUNDARIES.md` section, `CLAUDE.md` pointer);
   - DB-002 → Resolved;
   - runtime check against SC-005;
   - full suite.

## Complexity Tracking

No constitution violations to justify.
