# Feature Specification: Graphics State Handles

**Feature Branch**: `025-graphics-state-handles`
**Created**: 2026-10-25
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L2, after spec 023. Graphics State (`src/ri/state`) stops holding other domains' objects: attributes and options keep opaque shader and map handles (with a new `bxdf` slot) and a scoped pattern-node table, shader-parameter logic moves to Shading, the few foreign enumerations State stores move into a state-owned vocabulary header, and the AOV/channel registry becomes Graphics State's. Resolves registry entry DB-008 (GitHub #31) and moves the registry part of DB-021 (DB-021 itself stays Open). Reference decisions: D5 and D9 in `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md`.

## Clarifications

### Session 2026-10-25

- Q: How does Texturing reach the channel registry once it lives in Graphics State? -> A: Keep a thin `CRenderer` forwarder: `CRenderer::retrieveDisplayChannel` forwards to the State-owned registry; `texture3d.cpp` is untouched; no allowlist line is added or swapped. DB-021 stays Open (only partly resolved by 025) and is left to spec 037's channel descriptors; DB-008 is the entry 025 fully resolves.
- Q: When a Bxdf is bound, what does the `WorldBegin` default-surface binding do? -> A: `RiWorldBegin` binds `defaultsurface` to the surface slot exactly as today and leaves the last-bound (Surface/Bxdf) record untouched. `classic` stays byte-identical; spec 035 reads the record and ignores the surface slot.
- Q: Does this spec add a ribOut test for statement order? -> A: Yes, a small ribOut test of the existing shader statements' order; the `Bxdf` round-trip is left to spec 035.
- Assumptions accepted as decided: a pattern handle redefined in the same scope replaces the earlier one, and a nested definition shadows an outer one until the nested scope ends; the declared-variable table stays in the renderer core; the unchecked `CShaderInstance*` to `CProgrammableShaderInstance*` cast in `addLight()` is preserved as-is, not fixed.

### Session 2026-10-26 (plan questions)

- Q: Today the attribute copy constructor and the shading `restore()` prepend each light, so every `AttributeBegin`, every copy-on-write clone and every `ResourceRestore` reverses the active-light list. Preserve or fix? -> A: Fix it in 025. Lights run in RIB order in every block, and copies and restores keep the order (FR-025, SC-010). It lands in its own phase, after the behaviour-preserving phases, so their byte-identical checks still hold. The plan measures and lists which scenes and references it affects. Any reference re-render needs the maintainer's explicit approval at that time. Thresholds are never widened.
- Q: Does FR-003's "MUST NOT call back into them" forbid an installed hook? -> A: No. It means no direct reference, that is, no include or link edge. A State-owned hook that another domain implements and the renderer core installs, as `variableLookup.h` does, is allowed.
- Q: FR-001 lists the light entries among the retained handles, but today they are never attached. Which applies? -> A: Light-list entries hold a non-owning handle, as today. The renderer core owns the lights.
- Q: Is the `bxdf` slot part of the used-parameter set and of `findParameter`? -> A: No. It stays out of both answers until spec 035.
- Q: Should 025 add a production Graphics State build target (`ribState_obj`)? -> A: No. The State-only test target is enough.

### Session 2026-10-26 (tasks stage)

- Q: May the plan's single `State_Isolation` test be split into `State_Isolation`, `State_LastBound`, `State_PatternTable`, `State_ChannelRegistry` and `State_LightOrderRestore`? -> A: Yes. Each State-only test turns green in the phase that provides its API, and the direct `restore()` order check (`State_LightOrderRestore`) stays disabled until the FR-025 phase, so `State_LightOrder` remains the only red test before phase 7. `ctest -L state` selects all of them.
- Q: May FR-007's part B be a separate test? -> A: Yes. It is `ShadingParity_ShaderParameterEquivalenceDirect`, with the fixtures shared with part A through `tests/shading_parity/shader_parameter_fixtures.h`, because the missing `libshader::` API would otherwise break part A and the main build.
- Q: The State-only library compiles `src/ri/state/*.cpp` a second time, and archcheck exits 2 ("incomplete build tree") on any missing in-scope object, including those of `EXCLUDE_FROM_ALL` targets. How are the red phases kept compatible? -> A: The State-only library and the five `State_*` State-only executables are defined only when the CMake option `OPENRENDER_STATE_ONLY_TESTS` is on, off by default. The red evidence is taken in a scratch build directory with the option on. The handles phase switches it on (default ON from then, and explicitly in the build directory), together with the enabling edits.
- Q: Where is FR-005's "a bound `bxdf` changes neither answer" checked? -> A: By its own test, `ShadingParity_ShaderParameterBxdfIgnored`, written in phase 2 (`EXCLUDE_FROM_ALL` and `DISABLED`, red by build failure until `bindBxdf` exists) and enabled with the `bxdf` slot in phase 5.
- Q: Where does the explicit `ParameterType` to `EDisplayParameterType` conversion helper live? -> A: In a new core-private header, `src/ri/render/stateConversions.h`, with one `static_assert` per enumerator pair beside it.

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who build, maintain and extend openRender: contributors working in Graphics State, maintainers reviewing remediation specs, authors of the later specs that need these seams (026 shading contract, 034 shader networks, 035 material interface, 037 plug-in ABI), and anyone rendering scenes, whose output must not change except for the light-order fix (FR-025).

### Measured starting point (2026-10-25, master `dfffba0`)

- Graphics State is `src/ri/state/` (12 files). The architecture check allowlists **7 include edges** from it under DB-008: `attributes.h` and `options.h` each include Shading's `shader.h` (directly or through the `src/ri/core/shader.h` shim, which the check classifies as Shading; 4 lines), `options.h` includes `src/display/dsply.h` and `src/ri/geometry/blobbyField.h`, and `attributes.cpp` includes `src/ri/hiders/photonMap.h`. There are no link edges from State sources.
- `CAttributes` holds five fixed `CShaderInstance *` slots (`surface`, `displacement`, `atmosphere`, `interior`, `exterior`), an active-light list (`CActiveLight`, holding `CProgrammableShaderInstance *`), and two `CPhotonMap *` (`globalMap`, `causticMap`) that the renderer core fills lazily from the map names at render time. `COptions` holds the `imager` `CShaderInstance *`. All are reference counted through `attach()`/`detach()`; the attribute copy constructor and `restore()` re-attach them.
- Outside State, the five slots are read or written at about 60 sites: Shading (`shading.cpp`, `executeMisc.cpp`), the renderer core (`rendererContext.cpp`: the `Surface`/`Atmosphere`/`Interior`/`Exterior`/`Displacement` handlers and the default-surface binding at `WorldBegin`), Geometry (`surface.cpp:605`) and the renderer core (`geometryDispatch.cpp:281`), which only test the displacement slot for presence. Four Shading sites walk `lightSources`; five Hider sites read the photon-map pointers; the renderer core reads `options->imager`.
- Shader-parameter logic lives in `CAttributes`: `checkParameters()` unions the bound shaders' `requiredParameters()` into `usedParameters` (read once outside State, `shading.cpp:904`), and `findParameter()` walks the `parameters` lists of displacement, surface and atmosphere. `findParameter()` is called by State's own parameter-list parser (`pl.cpp`, 9 lines) and by Geometry (`blobby.cpp:304`); the call in Scene Interface's `rib.y:235` sits in an `if (FALSE)` branch, and `renderer.cpp:635` only mentions the display plug-in's own `findParameter` in a comment (research R4). So a plain move to Shading would give State and Geometry a forbidden dependency on Shading.
- The vocabulary State still takes from other domains is small. `RtDisplay*Function` typedefs and `rendererc.h` are already infrastructure (spec 023). What remains is `ParameterType` from the display plug-in ABI header (`dsply.h`, used by `COptions::CDisplay::TDisplayParameter`) and `BLOBBY_ORDER_RISPEC` from `blobbyField.h` (default of `blobbyOpcodeOrder`).
- The AOV/channel registry is two renderer-core statics (`CRenderer::declaredChannels`, `displayChannels`) with `declareDisplayChannel` (two forms), `retrieveDisplayChannel` and `resetDisplayChannelUsage`, five default channels (`rgb`, `rgba`, `a`, `z`, `rgbaz`) registered by `initDeclarations()` and freed by `shutdownDeclarations()`. The channel *class* (`CDisplayChannel`, `displayChannel.h`) is already in `src/ri/state/`. Callers: `RiDisplayChannel` and the display setup in the renderer core, and `CTexture3d::defineChannels` (Texturing, `texture3d.cpp:119`). The allowlist holds one DB-021 include edge (`texture3d.cpp -> displayChannel.h`) and one DB-010 link edge for `CRenderer::retrieveDisplayChannel` from the same file.
- Files in Geometry, Texturing and Scene Interface that name `CShaderInstance`, `CProgrammableShaderInstance`, `CPhotonMap`, `ParameterType` or `BLOBBY_ORDER_*`: **none** (grep, 2026-10-25). Files that do use them are in Shading, Hiders, the renderer core and consumers (preview, data viewer), all of which may include those headers. So removing State's includes should not force a new forbidden include elsewhere; this is verified by the architecture check, not assumed.
- `addLight()` appends lights in RIB order. But the attribute copy constructor and `restore(shading)` rebuild the list by prepending (`attributes.cpp:259-264,500-505`), so every attribute push, copy-on-write clone and `ResourceRestore` reverses it. The lights of such a block run in reverse order. That is fixed here (FR-025).
- No `ShaderGroupRef`, `MapRef`, `bxdf` slot or pattern-node table exists in the tree. There is no single "RIB round-trip suite": the round-trip tests are `SamplingOption_RibRoundTrip`, `Blobby_RibOutRoundTrip` and the preview debug-dump test, and none of them covers shader statements.

### User Story 1 - Graphics State depends on no other domain (Priority: P1)

A developer building or testing Graphics State on its own, or a reviewer checking constitution Principle VIII, finds that `src/ri/state` includes no header owned by Shading, Hiders, Geometry or Display Outputs and references none of their symbols. State stores handles; it does not know what is behind them.

**Why this priority**: It is the remediation this spec exists for. DB-008 is the registry entry for exactly this, and specs 026 and 034 are blocked on it.

**Independent Test**: Run the architecture check on macOS and Linux and count edges whose source is Graphics State; build and run a State-only unit test that links no Shading, Hiders, Geometry or Display library and drives the handles through a test double.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** it reports zero include and zero link edges from Graphics State to any other domain, on macOS and Linux.
2. **Given** the 7 DB-008 allowlist lines, **When** the change is complete, **Then** all 7 are deleted, no line is added or swapped anywhere, and DB-008 is listed under Resolved with this spec's number and the date.
3. **Given** a State-only test that builds its own concrete object behind a handle, **When** it binds the object to each slot, copies, restores and destroys the attribute block, **Then** the object's reference count rises and falls exactly as it does for today's raw pointers.
4. **Given** the finished tree, **When** a developer greps `src/ri/state` for `shader.h`, `photonMap.h`, `dsply.h` and `blobbyField.h`, **Then** none is included, directly or through a shim.

---

### User Story 2 - Every scene renders exactly as before (Priority: P1)

A user renders any existing scene: surface, displacement, atmosphere and volume shaders, lights (including the default light and the built-in sphere and quad lights), an imager shader, photon maps, `ResourceBegin`/`ResourceRestore`, and shader parameters supplied as primitive variables. Nothing about the result changes, with one exception: the light-order fix (FR-025). Binding a primitive variable to a shader parameter and the used-parameter set a grid shades with come out as today. Lights run in RIB order in every block.

**Why this priority**: The change touches the path every shaded point takes. Behaviour preservation is the contract of a remediation spec.

**Independent Test**: Run the visual, determinism, shading-parity, photon-map and imager suites on the starting tree and the finished tree, and compare. Add a State-level equivalence test for the parameter queries (see FR-007).

**Acceptance Scenarios**:

1. **Given** the full test suite, **When** it runs on the finished tree on macOS and on Linux (GCC 13, warnings as errors), **Then** every test that passed on the starting tree passes, and the bit-identity checks (`DepthDeterminism_*`, `ColorDeterminism_*`) stay green.
2. **Given** a primitive whose parameter list names a shader parameter (inline-declared or pre-declared), **When** the list is parsed, **Then** the variable binds to the same shader parameter, or is skipped on a type or size mismatch, exactly as on the starting tree.
3. **Given** a scene with several `LightSource` statements, light switching and a nested `AttributeBegin`, **When** it renders, **Then** lights run in RIB order and the used-parameter set equals the starting tree's at every grid. On the starting tree the lights of a cloned block run reversed; that difference is the FR-025 fix.
4. **Given** the existing unit tests that reach these fields (`selectVolumeShader` interior/exterior selection, photon-map frame release and sessions, the imager tests), **When** they run, **Then** their assertions are unchanged; only how they reach the object may change.
5. **Given** the slowest visual scenes, **When** they render on both trees, **Then** render time is within the starting tree's run-to-run variance.

---

### User Story 3 - The state records which of Surface and Bxdf was bound last (Priority: P2)

The author of spec 035 (the `Bxdf` statement) and of the integrators needs one answer to "which material statement governs this object": the later of `Surface` and `Bxdf` in the current attribute scope (D5). Graphics State provides the `bxdf` slot and the record; neither statement's RIB syntax is added here. The record is pushed and popped with `AttributeBegin`/`AttributeEnd` and carried by the copy-on-write clone and by the shading restore.

**Why this priority**: 035 and the integrators are blocked on it, but nothing a user can see changes in this spec, so it ranks below the boundary work.

**Independent Test**: State-level unit tests drive the state API directly (no `Bxdf` RIB statement exists yet): bind a surface, then a bxdf, push and pop a scope, clone, restore, and read the answer.

**Acceptance Scenarios**:

1. **Given** a fresh attribute block, **When** the answer is read, **Then** it is "none".
2. **Given** a block where a surface was bound and then a bxdf, **When** the answer is read, **Then** it is "bxdf"; given the reverse order, it is "surface". Both slots keep their handles in either case.
3. **Given** a bxdf bound inside an `AttributeBegin`/`AttributeEnd` pair over an outer surface, **When** the scope ends, **Then** the answer is "surface" again, and the inner bxdf is gone with the scope.
4. **Given** an attribute clone made for a copy-on-write modification, and a shading restore from a saved block, **When** the answer is read, **Then** the clone and the restore carry the saved block's answer.
5. **Given** a RIB read and written back by ribOut, **When** it contains the existing shader statements (`Surface`, `Displacement`, `Atmosphere`, `Interior`, `Exterior`, `LightSource`, `Imager`) in several orders relative to other attribute statements, **Then** the written statements keep the input order (ribOut writes statements as they arrive; checked by a new automated test, not changed). The `Bxdf` round-trip is left to spec 035.

---

### User Story 4 - A scoped pattern-node table is ready for shader networks (Priority: P2)

The author of spec 034 (shader networks) needs somewhere to declare RIS-style `Pattern` nodes by handle in the current attribute scope, and to look them up when a shader slot binds a `reference` parameter. Graphics State provides that storage only: no `Pattern` statement, no `reference` parsing and no group assembly exist until 034.

**Why this priority**: It is named in the brief and 034 depends on it, but it has no user-visible effect in this spec.

**Independent Test**: State-level unit tests define nodes, nest scopes, clone, restore and look nodes up.

**Acceptance Scenarios**:

1. **Given** a node defined under a handle in the current scope, **When** the handle is looked up in that scope or in a nested one, **Then** the node is found.
2. **Given** a node defined inside an `AttributeBegin`/`AttributeEnd` pair, **When** the scope has ended, **Then** looking up the handle finds nothing, and a node of the same handle defined outside is again the one found.
3. **Given** a handle that was never defined, **When** it is looked up, **Then** the lookup reports "not found" and State writes no diagnostic of its own.
4. **Given** a slot that has bound a node, **When** the scope that defined the node ends, **Then** the node stays alive for as long as the slot holds it.

---

### User Story 5 - Graphics State owns the AOV/channel registry (Priority: P2)

A developer working on display channels, AOVs or 3D-texture channels finds the registry in Graphics State, not in renderer-core statics. `DisplayChannel` declarations, the five built-in channels, name lookup and the per-frame usage reset behave as before.

**Why this priority**: It moves the registry part of DB-021 (the entry stays Open for spec 037). Texturing keeps reaching the registry through the `CRenderer` forwarder, which is why this ranks below the boundary work.

**Independent Test**: State-level tests exercise the registry; the AOV visual tests and `Parity_transparency-matte-aov` stay unchanged.

**Acceptance Scenarios**:

1. **Given** a new session, **When** channels are retrieved by name, **Then** `rgb`, `rgba`, `a`, `z` and `rgbaz` exist with their sample counts and offsets, and any other name is not found.
2. **Given** a channel declaration string, **When** it is declared twice with the same shape, **Then** the same channel comes back; with a different shape it is refused with the error the starting tree gives.
3. **Given** channels declared out of order across frames, **When** the usage reset runs, **Then** every variable-backed channel's sample start is unassigned and the declaration order is unchanged.
4. **Given** the finished tree, **When** the renderer-core statics are searched, **Then** no channel table lives in `CRenderer`; `CRenderer::retrieveDisplayChannel` remains only as a forwarder to the State-owned registry.
5. **Given** `CTexture3d::defineChannels`, **When** it runs on the finished tree, **Then** it calls the unchanged `CRenderer::retrieveDisplayChannel`, `texture3d.cpp` has no diff, and the channel list is identical.

---

### Edge Cases

- **Default surface at `WorldBegin`**: the renderer core binds `defaultsurface` when no `Surface` was given. `RiWorldBegin` binds `defaultsurface` to the surface slot exactly as today and leaves the last-bound record untouched, so the binding never turns the answer into "surface" over a bound bxdf, and `classic` output is byte-identical.
- **Shading restore**: `restore(shading=true)` replaces all five slots, the lights, the map names and the user attributes. It must also replace the bxdf slot, the last-bound record and the pattern table, and re-attach every handle.
- **Copy-on-write clones**: `getAttributes(modify)` clones a shared block. The clone must carry the bxdf handle, the record, the pattern table and the map handles.
- **Lazy photon-map binding**: `CRenderer::render()` fills `globalMap`/`causticMap` from the names the first time an object is rendered. The handle form must keep that laziness and must not let two objects sharing a name end up with different maps.
- **Photon-map lifetime**: `test_photon_map_frame_release` and `test_photon_map_sessions` pin frame-end release and session reuse; their results must not change.
- **Light order in cloned blocks**: an attribute push, a copy-on-write clone and a `ResourceRestore` all copy the light list. Each copy must keep the source's order (FR-025). A light switched off and on again with `Illuminate` moves to the end of the list, as `addLight` already does today.
- **Unchecked light cast**: `addLight()` stores a `CShaderInstance *` as `CProgrammableShaderInstance *` without checking, and the built-in sphere and quad lights (DB-013) are not programmable instances. The handle form must preserve today's behaviour for them, and must not "fix" the cast here.
- **Hot paths**: Shading's per-grid and per-light loops (`execute.cpp`, four sites in `shading.cpp`) resolve handles on every shade. This must not add allocation, locking or a measurable cost.
- **No services / no resolver installed**: a State-only program (the test double, or a consumer that links only State) must get a defined result, not a crash, when a handle has no resolver.
- **Motion-blur attribute chains**: `CAttributes::next` chains blocks for motion; each block keeps its own handles.
- **Session reuse**: a second `RiBegin`/`RiEnd` in the same process starts with a fresh registry and empty tables; nothing leaks across sessions (cf. gotcha 20).
- **Build options**: a build without the JIT, and the preview/Python/Lua consumers that include `attributes.h`, still build.

## Requirements *(mandatory)*

### Functional Requirements

**Handles and slots**

- **FR-001**: Graphics State MUST hold, for each of the slots `surface`, `displacement`, `atmosphere`, `interior`, `exterior`, the new `bxdf`, and the `imager` (in `COptions`), only a handle type that Graphics State owns and that exposes no Shading type; each entry of the active-light list holds a non-owning pointer to the State-owned target type (`CShaderGroupRefTarget *`). Reference-counting and lifetime MUST match today's. A slot handle is retained when an attribute block is copied or restored, and released when the slot is replaced or the block is destroyed. An empty slot is still distinguishable from a bound one. Light-list entries hold that non-owning pointer, as today: the renderer core owns the lights, and copying or restoring a block does not retain them (clarification 2026-10-26).
- **FR-002**: `globalMap` and `causticMap` MUST be held the same way (a map handle). Graphics State MUST NOT include the photon-map header. The lazy, name-driven binding at render time and the frame-end release MUST behave as today.
- **FR-003**: Shading, Hiders and the renderer core MUST obtain the concrete object from a handle through an interface owned by the resolving side, in a direction the allowed-dependency table permits. Graphics State MUST NOT call back into them. "Call back" means a direct reference, that is, an include or link edge. A State-owned hook that another domain implements and the renderer core installs, as `variableLookup.h` does, is allowed (clarification 2026-10-26). The two presence checks on the displacement slot (Geometry's `surface.cpp:605` and the renderer core's `geometryDispatch.cpp:281`) MUST work without Geometry depending on Shading.
- **FR-004**: The light list MUST keep duplicate suppression and removal as today, with handles in place of `CProgrammableShaderInstance *`. It MUST keep RIB order in every block (FR-025).

**Shader-parameter logic**

- **FR-005**: Computing the used-parameter set from the bound shaders and finding a shader parameter by name MUST be provided by Shading. The set MUST be recomputed at every point where it is recomputed today (any slot bind, light add or remove, shading restore, the default-surface and default-light bindings). `findParameter` MUST keep its precedence: displacement, then surface, then atmosphere. The `bxdf` slot MUST NOT be part of either answer until spec 035 (clarification 2026-10-26).
- **FR-006**: Callers of those two operations in Graphics State (the parameter-list parser, `pl.cpp`) and Geometry (`blobby.cpp`) MUST still obtain the same answers, and none of them MAY gain a dependency on Shading. How the answer reaches them is the plan's decision; the requirement is the absence of a new edge.
- **FR-007**: A test MUST show that, for a set of bound shaders, the used-parameter set and the parameter lookups (found, not found, shadowed by an earlier slot) equal the values the starting tree computes. The test MUST fail on a build where the answer differs.

**Vocabulary**

- **FR-008**: Graphics State MUST own a vocabulary header supplying the enumerations it stores that other domains define: the display parameter type and the blobby opcode order. Their values MUST be identical to the originals, and a consistency test MUST fail if the two lists drift (as `Common_VocabularyConsistency` does for the RI tokens). The display plug-in ABI header (`dsply.h`) and `blobbyField.h` MUST NOT change.
- **FR-009**: After the change, no file in `src/ri/state` MAY include a header owned by another domain; only infrastructure (`src/common`, `src/ri/core` infrastructure headers) and State's own headers remain.

**Surface / Bxdf record (D5)**

- **FR-010**: Each attribute block MUST record which of `Surface` and `Bxdf` was bound most recently in its scope: none, surface, or bxdf. Binding either one updates the record and leaves the other slot's handle in place. The record MUST be saved and restored with the attribute stack, carried by clones, and replaced by the shading restore.
- **FR-011**: There MUST be one query that answers which of the two governs. No consumer may infer the answer from which slot is empty.
- **FR-012**: This spec MUST NOT add the `Pattern` or `Bxdf` RIB statements, the `reference` parameter syntax or any grammar. The slot, the record and the table are exercised through the state API (specs 034 and 035 add the statements). Name resolution for `Bxdf` (D9) is out of scope.

**Pattern-node table**

- **FR-013**: Each attribute block MUST carry a scoped table of pattern nodes keyed by handle string. It MUST support define, lookup (found or not found) and iteration. A definition made in a scope is visible in that scope and in scopes nested in it, and not after the scope ends. A new definition of an existing handle in the same scope replaces it.
- **FR-014**: The table MUST be carried by clones and replaced by the shading restore, like the slots. Nodes MUST be reference counted so a slot that bound a node keeps it alive after its defining scope ends.
- **FR-015**: Graphics State MUST NOT interpret a node's content. A lookup of an undefined handle returns "not found"; the diagnostic for an unresolved reference belongs to spec 034.

**AOV/channel registry**

- **FR-016**: The registry MUST be owned by Graphics State: the five built-in channels, declaration from a declaration string and from an already-declared variable, retrieval by name, the per-frame usage reset, and iteration in declaration order. Behaviour, error messages and the refusal of an inconsistent redeclaration MUST match today's. Its lifetime MUST stay the declarations' lifetime (created at `RiBegin`, destroyed at `RiEnd`). No channel table MAY remain among `CRenderer`'s statics.
- **FR-017**: The registry MUST NOT gain a dependency on any other domain. Declaring a channel from a string needs the declared-variable table, which stays in the renderer core (see Assumptions).
- **FR-018**: `CRenderer::retrieveDisplayChannel` MUST remain as a thin forwarder to the State-owned registry. `texture3d.cpp` MUST NOT change, and no allowlist line MAY be added or swapped for it (the existing DB-010 link line and DB-021 include line stay as they are). `CTexture3d::defineChannels` MUST produce the same channel list for the same definitions.

**Registry and guardrails**

- **FR-019**: The architecture check MUST pass on macOS and Linux. The 7 DB-008 allowlist lines MUST be deleted. Any other line the change makes stale MUST be deleted. No line MAY be added or swapped. `metrics-baseline.txt` MUST be lowered to what the check reports and MUST NOT rise for any class.
- **FR-020**: The registry MUST be updated: DB-008 moves to Resolved with this spec's number and the date. DB-021 stays Open: its evidence loses "the registry itself is renderer-core state" and keeps what remains (Texturing's include of `displayChannel.h` and its call through the `CRenderer` forwarder), with the tracking line left to spec 037; no text MAY call DB-021 Resolved. DB-010's evidence and the allowlist header counts are updated to match, and DB-010's link line for `retrieveDisplayChannel` stays since the forwarder remains. No registry text MAY refer to anything that exists only after the merge. GitHub #31 closes through the maintainer's merge message.
- **FR-021**: New tests MUST be written first and shown failing on the starting tree, per constitution Principle III: a State-only test that links no other domain's library (rule 5), the vocabulary consistency test, the handle-lifetime test, the last-bound and pattern-table scoping tests, the registry tests, the parameter-equivalence test (FR-007) and the light-order test (FR-025).
- **FR-022**: Rendering output MUST NOT change, except through the light-order fix of FR-025 in its own phase. Every other phase MUST be byte-identical to the starting tree. The full suite MUST stay green on macOS and on Linux (GCC 13, warnings as errors), including the determinism bit-identity checks. Existing unit tests keep their assertions.
- **FR-023**: Record the shader-statement ordering check for ribOut (User Story 3, scenario 5) as an automated test of the existing statements. No `Bxdf` round-trip is added here.
- **FR-024**: `RiWorldBegin` MUST bind `defaultsurface` to the surface slot exactly as today and MUST NOT change the last-bound record.

**Light order**

- **FR-025**: Every attribute block's active-light list MUST hold its lights in the order they became active in the RIB stream. An attribute push (`AttributeBegin`), a copy-on-write clone and a shading restore (`ResourceRestore`) MUST copy the source block's list in order, not reversed as the starting tree does.
  - Light switching (`Illuminate` off then on), duplicate suppression and removal behave as today.
  - The fix MUST land in its own phase, after the behaviour-preserving phases.
  - The scenes and references whose output it changes MUST be measured on the starting tree and listed in the plan.
  - A reference image MAY be re-rendered only with the maintainer's explicit approval, given at that time, for a listed scene and reason. No threshold is widened.
  - A test MUST show the order inside a nested `AttributeBegin`, after a copy-on-write clone and after a `ResourceRestore`. It MUST fail on the starting tree.

### Key Entities

- **Shader handle** (`ShaderGroupRef`): an opaque, reference-counted reference to a bound shader (or, later, a shader group or network) that Graphics State stores in a slot and never inspects.
- **Map handle** (`MapRef`): the same for a photon map held by an attribute block.
- **Slots**: `surface`, `displacement`, `atmosphere`, `interior`, `exterior`, `bxdf`, the ordered light list, and (in options) `imager`.
- **Last-bound record**: none, surface or bxdf; which statement governs the object's material.
- **Pattern-node table**: handle string to opaque node, scoped like the attribute stack.
- **State vocabulary header**: the enumerations State stores that other domains define.
- **Channel registry**: the named set of display channels and AOVs, with the five built-ins, owned by Graphics State.
- **Used-parameter set**: the bitmask of globals the bound shaders need; computed by Shading, read when grids shade.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The architecture check reports 0 include edges and 0 link edges whose source is Graphics State, on macOS and Linux (7 before).
- **SC-002**: All 7 DB-008 allowlist lines are deleted, no allowlist line is added or swapped, and DB-008 is under Resolved with this spec's number and date.
- **SC-003**: `src/ri/state` includes none of `shader.h`, `photonMap.h`, `dsply.h`, `blobbyField.h`, directly or through a shim.
- **SC-004**: The full suite passes on macOS and Linux with the same results as before, apart from the new tests and the FR-025 changes the plan lists. `DepthDeterminism_*` and `ColorDeterminism_*` stay bit-identical. Existing unit tests keep their assertions. Every phase before FR-025's renders byte-identically to the starting tree.
- **SC-005**: The slowest three visual scenes render within the starting tree's measured run-to-run variance.
- **SC-006**: The State-only tests, linking no Shading, Hiders, Geometry or Display library, build and pass and cover handles, last-bound, pattern scoping and the registry. The vocabulary is covered by the consumer test `State_VocabularyConsistency` (SC-007), which includes `dsply.h` and `blobbyField.h` and so cannot be State-only.
- **SC-007**: The vocabulary consistency test passes and fails when a value in either list is changed.
- **SC-008**: No channel table is left among `CRenderer`'s statics (only the forwarder remains), `texture3d.cpp` is unchanged, DB-021 is still Open, and the AOV visual tests give the same results.
- **SC-009**: The registry and allowlist header counts agree with the check's report, and `metrics-baseline.txt` is lowered only to values the check reports.
- **SC-010**: The light-order test fails on the starting tree and passes on the finished tree. Every scene the plan lists as affected by FR-025 passes its visual or parity test at its unchanged threshold, or has a reference re-render the maintainer approved. No unlisted scene changes output.

## Assumptions

- Spec 023 is merged (`libRIBCommon` supplies diagnostics, vocabulary, `CVariable`/`parseVariable`, reference counting). Spec 022's architecture check, allowlist and registry are the yardstick. Spec 024 is merged.
- The slots are those of the plan's V5 row: `surface`, `displacement`, `atmosphere`, `interior`, `exterior`, `bxdf`, the light list and the imager. `MapRef` covers the global and caustic photon maps; the irradiance cache is held by handle name only today and is not touched.
- Handles are thin reference-counted wrappers around today's objects; no copy of a shader or map is made and no behaviour changes (the plan's step 1).
- The `Pattern` and `Bxdf` RIB statements, `reference` parameters, group assembly and unresolved-handle diagnostics are specs 034 and 035. This spec supplies storage and the query only; the last-bound record and the table are tested through the state API.
- A pattern handle redefined in the same scope replaces the earlier node, and a nested definition shadows an outer one until the nested scope ends. Decided in clarification (2026-10-25); 034 may still revisit.
- The declared-variable table (`CRenderer::declareVariable`, `retrieveVariable`) stays in the renderer core. Declaring a channel from a string reaches it through a hook State already owns the way `variableLookup.h` does, or the core declares the variable and registers the channel; the plan chooses.
- Channel descriptors passed to displays and Texturing (V18's second half) are spec 037. DB-021's tracking line keeps 037.
- The display plug-in ABI (`dsply.h`) and the published blobby header do not change; their versioning is DB-003/DB-014.
- The photon hider's, built-in light's (DB-013) and hider-sharing (DB-005) couplings are out of scope.
- The unchecked light-pointer cast is preserved as-is, as a finding, not fixed here (decided in clarification).
- The light-order reversal on copy and restore is fixed here (FR-025, decided 2026-10-26). It is the only intended output change.
- There is no production Graphics State build target in 025 (decided 2026-10-26).
- Verification that this spec adds to the stage report: the architecture check run before and after, `ctest -L visual`, `-L shading_parity`, `-L '^determinism$'`, the photon-map and imager unit tests, and the new State-only tests. The determinism sweep (`-L '^determinism-sweep$'`) runs once before the merge, not per phase.
