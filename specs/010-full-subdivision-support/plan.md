# Implementation Plan: Full Subdivision Surface Support

**Branch**: `010-full-subdivision-support` | **Date**: 2026-08-11 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/010-full-subdivision-support/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

Harden openRender's existing Catmull-Clark `RiSubdivisionMesh` implementation into full RISpec conformance across
six tiers, entirely at the geometry layer (`src/ri/subdivisionCreator.{h,cpp}`, `subdivision.{h,cpp}`, and new
sibling files), preserving the pre-existing invariant that every hider reaches subdivision geometry purely through
`CObject`/`CSurface` virtual dispatch with zero subdivision-specific hider code (`contracts/
hider-invariant-contract.md`). User Story 1 is pure verification — the cross-hider motion-blur mechanism
(`CTesselationPatch::sampleTesselation()`/`intersect()`) already works generically for any `moving()` primitive
including subdivision surfaces (research.md R1/R2); no renderer code changes, only new test coverage and a
`HIDER_PARITY.md` section. User Story 2 fixes a real data-loss bug: `CSVertex`'s single `facevarying` pointer
collapses to the last-processed incident face's value on any shared vertex, destroying UV-seam discontinuities;
the fix extends the existing `CVertexFace` per-incident-face node with its own `facevarying` slot and threads a
requesting-face parameter through `computeVarying()`'s recursive chain from `CSFace::computeVarying()`/the
`sort()` call sites, not through `gatherData()`'s 9 call sites (rejected once read in full — those operate on
already-synthesized sub-primitives, the wrong layer; research.md R3). User Story 3 adds the three currently-
rejected subdivision tags (`facevaryinginterpolateboundary`, `facevaryingpropagatecorners`, `creasemethod`) as new
dispatch arms in the existing tag chain, following the existing four tags' precedent rather than the `CAttributes`
four-layer pattern (research.md R4). User Story 4 gates any crease-quality fix on first reproducing the two
open, currently-unreproduced `DEVNOTES.md` reports with a concrete test scene — no fix is committed sight-unseen
(research.md R5). User Story 5 introduces `RiHierarchicalSubdivisionMesh[V]` as a new, parallel RI entry point
(not a variant of `RiSubdivisionMesh`) touching seven layers from RIB grammar to Lua bindings, with override
resolution confined entirely to the geometry layer (research.md R6, `contracts/hierarchical-subdivision-
contract.md`). User Story 6 adds Loop subdivision as a second scheme sharing the identical `CObject`/`CSurface`
integration seam Catmull-Clark already uses, deliberately not building a Loop-specific eigenbasis generator since
FR-010 bounds Loop's scope to Catmull-Clark's existing integration depth, not additional capability (research.md
R7). No new third-party dependency is introduced at any tier.

## Technical Context

**Language/Version**: C++20 (project-wide; see `CMAKE_CXX_STANDARD 20` and existing `target_compile_features(...
cxx_std_20)` usage in `tests/visual/CMakeLists.txt`)

**Primary Dependencies**: None new. Existing: CMake build, `Threads::Threads` (POSIX pthreads), LLVM (shading JIT,
unrelated to this feature), libtiff (visual-regression comparison tool). Research.md R7 explicitly rejects
building a new build-time eigenbasis-generation dependency for Loop subdivision (no `precomp.cpp`-style
`CEigenBasis`/valence-indexed basis-data table analog) — Loop uses iterative/uniform subdivision with only
facilities already present in the toolchain.

**Storage**: N/A (in-memory renderer state; new fields live on `CSVertex`/`CVertexFace`, `CSubdivData`, and new
hierarchical-edit/Loop-scheme types, for the lifetime of a render, like every other geometry/attribute data in
this codebase)

**Testing**: `ctest -L visual` (existing `tests/visual/CMakeLists.txt` `add_visual_test`/`add_parity_test` macros
— new scenes registered the same way as existing entries, per research.md R9's explicit decision to introduce no
new test infrastructure); new scenes live under `examples/rib/tests/` (single-hider regression) and
`examples/rib/tests/parity/` (cross-hider comparison), following quickstart.md's Step 0 baseline-first ordering
for the facevarying fix (Constitution Principle III) and User Story 4's reproduce-before-fix gate for creases.

**Target Platform**: Linux / macOS only, per Constitution Principle VI. No Windows-specific code; no new
platform-conditional logic at any tier.

**Project Type**: Single project — C++ renderer core (`src/ri/`), no separate frontend/backend split.

**Performance Goals**: SC-007 — no measurable regression for existing Catmull-Clark scenes (the new
`scheme="loop"` dispatch adds a single string-compare branch to `RiSubdivisionMeshV`, `rendererContext.cpp:
5364-5366`, on the already-taken code path). User Story 4's crease-quality bar is explicitly qualitative only
(visible artifact or noticeably slower vs. a lightly-creased control mesh, research.md R5) — no numeric
performance threshold exists in this codebase to make one meaningful project-wide.

**Constraints**: Purely additive at every tier (new tags, hierarchical edits, and Loop scheme must not change
behavior for scenes that don't use them); the facevarying fix must leave single-incident-face vertices and
facevarying-absent meshes (e.g. `geometry/killeroo.rib`) bit-for-bit unaffected (data-model.md's Facevarying
Corner Value validation rules); zero hider file (`stochastic.cpp`, `reyes.cpp`, `zbuffer.cpp`, `raytracer.cpp`,
`trace.cpp`, `photon.cpp`, `show.cpp`) may gain a subdivision-specific branch, type check, or downcast at any
tier (FR-012/FR-013, `contracts/hider-invariant-contract.md`) — enforced by a standing grep-based regression
check with zero expected matches; no artifact effect (motion blur, DOF, future PathTrace-era effects) may receive
per-hider special-case treatment (standing architectural rule, already satisfied for motion blur per research.md
R1).

**Scale/Scope**: One new RI entry point (`RiHierarchicalSubdivisionMesh[V]`, currently entirely unimplemented —
zero RIB grammar support exists), three new tag tokens (`RI_FACEVARYINGINTERPOLATEBOUNDARY`,
`RI_FACEVARYINGPROPAGATECORNERS`, `RI_CREASEMETHOD`), one new per-incident-face data field
(`CVertexFace.facevarying`), one new geometry-layer scheme (`CLoopSubdivMesh` or equivalent, sharing the existing
`CObject`/`CSurface` contract), new override-resolution logic for hierarchical edits confined to the geometry
layer, and roughly 15+ new visual-regression/parity scenes across REYES, ray-tracing, photon, and (authored-but-
not-required) `CShow`, per spec.md's testing requirements. No change to any hider file at any tier.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. Clean Code Standards | Facevarying fix extends the existing `CVertexFace` node in place (no new parallel array requiring separate index-sync, research.md R3's rejected alternative); new tags follow the existing four-tag dispatch-chain convention rather than inventing a second mechanism (R4); hierarchical-edit override resolution is confined entirely to the geometry layer, never duplicated into a hider (R6, `contracts/hierarchical-subdivision-contract.md`); Loop subdivision is a sibling file sharing the identical `CObject`/`CSurface` contract, not a hider-side special case (R7). | PASS |
| II. Language Standards (C++20/C17) | All new code is C++20; no new language-standard requirement introduced at any tier. | PASS |
| III. TDD (NON-NEGOTIABLE) | quickstart.md Step 0 captures the facevarying-collapse bug reproducing on unmodified `master` *before* the fix lands, and captures the existing `geometry/killeroo.rib` baseline (466 `hole`/`interpolateboundary` calls, research.md R8) that must stay bit-for-bit unaffected. User Story 4 gates any crease fix on first reproducing the bug with a concrete failing scene (R5) — a fix cannot be written before a reproducer exists. | PASS |
| IV. Command Line Interface | No new CLI tool. `RiHierarchicalSubdivisionMesh` is a RIB/RI-API addition; its round-trip is handled by a new parallel `CRibOut::RiHierarchicalSubdivisionMeshV` serializer (Layer 5 of `contracts/hierarchical-subdivision-contract.md`), verified live via the pre-existing `ArchiveBegin`/`ArchiveEnd` mechanism (per `specs/009-nurbs-trim-curves` T034 precedent), consistent with how every other primitive round-trips today. | PASS |
| V. Minimal Dependencies | No new third-party dependency at any tier. Research.md R7 explicitly rejects a Loop-specific eigenbasis-generation dependency in favor of iterative/uniform subdivision using only existing facilities. | PASS |
| VI. Platform Targeting | Linux/macOS only; no platform-conditional code introduced at any tier. | PASS |
| VII. Documentation and Site Management | `HIDER_PARITY.md` gains a new subdivision-surfaces section (research.md R1/R2/R5's findings — motion-blur verification, camera-SLERP closed note, crease-quality outcome). `DEVNOTES.md:42-43`'s two open, unreproduced crease-quality checkboxes are resolved or explicitly deferred with written rationale, per User Story 4's own gate — never left silently stale. **No `site/` Hugo folder exists anywhere in this repository yet** — the repo-wide migration (`specs/001-hugo-docs-migration`) has not landed. This feature uses the same `DEVNOTES.md`/`HIDER_PARITY.md` interim convention every other landed spec (009 included) uses pending that migration, and does not build `site/` itself — that is 001's scope. | PASS (interim convention, pending 001) |

No violations requiring justification — Complexity Tracking is empty.

**Post-Design Re-check** (after Phase 1): `data-model.md`'s six entities and both `contracts/*.md` files confirm
no tier invented a new abstraction beyond what RISpec itself requires — the facevarying fix extends an existing
struct in place, the new tags extend an existing dispatch chain, `RiHierarchicalSubdivisionMesh` is a new-but-
parallel entry point required because RISpec defines it as a structurally distinct interface call (not an
optional-argument variant, research.md R6's rejected alternative), and Loop shares Catmull-Clark's exact
integration seam rather than duplicating it. Principle I holds. No new dependency was introduced by either
contract. Principle V holds. `contracts/hider-invariant-contract.md`'s regression check gives Principle
VI/FR-012's "no hider-specific code" requirement a concrete, automatable verification step rather than an
unenforced convention. Gate re-confirmed: PASS, no changes to the table above.

## Project Structure

### Documentation (this feature)

```text
specs/010-full-subdivision-support/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
│   ├── hider-invariant-contract.md
│   └── hierarchical-subdivision-contract.md
└── tasks.md              # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/ri/
├── ri.h                          # RI_FACEVARYINGINTERPOLATEBOUNDARY / RI_FACEVARYINGPROPAGATECORNERS / RI_CREASEMETHOD token constants (new, alongside the existing RI_HOLE/RI_CREASE/RI_CORNER/RI_INTERPOLATEBOUNDARY at ri.h:231-234); RiHierarchicalSubdivisionMesh[V] declaration (new, parallel to RiSubdivisionMeshV)
├── ri.cpp                        # New token definitions (alongside ri.cpp:160-163); RiHierarchicalSubdivisionMesh[V] registration
├── rib.y                         # New RIB_HIERARCHICAL_SUBDIVISION_MESH grammar production (near the three existing RIB_SUBDIVISION_MESH alternatives, rib.y:2390-2473) — a new production, not a new alternative on the existing rule (research.md R6)
├── rib.l                         # New RIB_HIERARCHICAL_SUBDIVISION_MESH token (alongside the existing SubdivisionMesh token, rib.l:118)
├── rendererContext.cpp           # RiSubdivisionMeshV (rendererContext.cpp:5348-5407): accept "loop" alongside "catmullclark" at the scheme-rejection site (5364-5366, currently `error(CODE_INCAPABLE, ...)`); new RiHierarchicalSubdivisionMesh[V] implementation, parallel to RiSubdivisionMeshV, parsing base mesh + override list
├── subdivisionCreator.h          # CVertexFace (subdivisionCreator.h/.cpp:104-109): new `float *facevarying` field; CSubdivData: new flag bits for the three new tags (alongside FACE_INTEPOLATEBOUNDARY, subdivisionCreator.cpp:47)
├── subdivisionCreator.cpp        # Facevarying fix: CSVertex::computeVarying() (1237-1252), CSEdge::computeVarying() (1387), CSFace::computeVarying() (1433-1467) gain a requesting-face parameter, threaded from the sort() call sites (e.g. 614) where `this` (the enclosing CSFace) is already passed; assignment loop (1858-1860) populates the new CVertexFace.facevarying slot instead of the collapsed CSVertex.facevarying field (removed, was declared at 157). New-tags dispatch: create()'s tag-recognition chain gains three new arms (near the existing FACE_INTEPOLATEBOUNDARY handling, 1692-1720), replacing three CODE_BADTOKEN fall-throughs. Crease-quality fix (if root-caused, User Story 4): CSVertex's crease/corner accumulation logic, scope TBD pending R5's reproduction outcome.
├── subdivisionHierarchical.h/.cpp  # NEW sibling files: CHierarchicalOverride struct (face/level/tag/value tuple) and override-resolution logic consulted during subdivision evaluation — entirely geometry-layer, never referenced by any hider file (contracts/hierarchical-subdivision-contract.md Layer 4)
├── subdivisionLoop.h/.cpp        # NEW sibling files: Loop-scheme subdivision implementing the same CObject/CSurface contract (intersect()/dice()/instantiate()/sample()/interpolate()) that CSubdivMesh/CSubdivision already implement for Catmull-Clark (research.md R7)
├── ribOut.cpp                    # CRibOut::RiSubdivisionMeshV (1288,1304): new tag serialization for the three new tags; new parallel CRibOut::RiHierarchicalSubdivisionMeshV serializer
├── object.h                      # NOT modified — CObject/CSurface's existing virtual contract (60-144) is the seam every new type above implements identically; documented, not changed, in contracts/hider-invariant-contract.md
└── stochastic.cpp, reyes.cpp, zbuffer.cpp, raytracer.cpp, trace.cpp, photon.cpp, show.cpp
                                   # NOT modified by any tier of this feature — contracts/hider-invariant-contract.md's regression check (FR-013) confirms zero subdivision-specific references land here

src/preview/libribpreview/
├── ribGeometryContext.h          # New RiHierarchicalSubdivisionMeshV declaration (alongside the existing RiSubdivisionMeshV at .h:122)
├── ribGeometryContext.cpp        # New RiHierarchicalSubdivisionMeshV handler (alongside the existing handler at 687,706) — parses/draws base-mesh topology only, no override visualization (contracts/hierarchical-subdivision-contract.md Layer 6)
└── previewContext.cpp            # NOT modified — its existing dynamic_cast<CSubdivMesh *> at line 92 is preview-tool-side, explicitly out of scope for FR-013's hider-only regression check (research.md R6)

src/lua/
└── prman.lua                     # New Ri:HierarchicalSubdivisionMesh binding (alongside the existing Ri:SubdivisionMesh at 568,573)

DEVNOTES_DETAILS/
└── HIDER_PARITY.md               # New subdivision-surfaces section: motion-blur verification (R1/R2), crease-quality outcome (R5), hierarchical/Loop cross-hider parity notes

DEVNOTES.md                       # Status table: resolve or explicitly defer-with-rationale the two open crease-quality checkboxes (lines 42-43)

examples/rib/tests/
├── subdiv-facevarying-seam-{reyes,raytrace}.rib       # User Story 2
├── subdiv-new-tags-raytrace.rib                       # User Story 3
├── subdiv-crease-convergence-raytrace.rib             # User Story 4
├── subdiv-hierarchical-override-raytrace.rib          # User Story 5
├── subdiv-loop-{reyes,raytrace}.rib                   # User Story 6
├── parity/
│   ├── motion-subdiv-{translate,rotate}-{reyes,raytrace}.rib   # User Story 1
│   └── subdiv-hierarchical-override-{reyes,raytrace}.rib       # User Story 5 cross-hider check
└── references/
    └── *.tif                                          # Matching reference images for each scene above

tests/visual/
└── CMakeLists.txt                # New add_visual_test(...)/add_parity_test(...) entries for every scene above, following the existing macro/registration pattern (CMakeLists.txt:86,133) — no new test infrastructure introduced (research.md R9)
```

**Structure Decision**: Single-project structure (this is the existing C++ renderer core, `src/ri/`) — no new
top-level directory, module, or build target. `subdivisionHierarchical.{h,cpp}` and `subdivisionLoop.{h,cpp}` are
new sibling files within `src/ri/`, following the same file-splitting convention the codebase already uses for
`subdivisionCreator.{h,cpp}` (topology construction) vs. `subdivision.{h,cpp}` (limit-surface evaluation) — new
capability gets a new sibling file, not a new directory. Every hider file is explicitly unmodified. The only new
directories are documentation artifacts under `specs/010-full-subdivision-support/` and new RIB test scenes +
reference images under `examples/rib/tests/` (and its existing `parity/`/`references/` subdirectories), both
additive to existing directories.

## Complexity Tracking

| Violation | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Principle VII's `site/` Hugo folder is not created or updated by this feature | The repo-wide Hugo migration (`specs/001-hugo-docs-migration`) has not landed; no `site/` directory exists to update | Building a one-off `site/` page ahead of 001's own structure/tooling decisions risks being thrown away when that migration lands; `HIDER_PARITY.md`/`DEVNOTES.md` is the interim convention 009 and every other current spec already uses |
