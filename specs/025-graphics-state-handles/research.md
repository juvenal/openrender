# Research: Graphics State Handles

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Date**: 2026-10-25 | **Tree**: master `dfffba0`

Each item gives the decision, why, and what was rejected. Line numbers are on `dfffba0`. Every
statement about today's behaviour comes from reading the code. The measurements that need a build
are listed in [quickstart.md](quickstart.md) §0 and are still pending.

---

## R1. Today's ownership semantics, slot by slot

The handle form must reproduce these exactly (FR-001, US1.3). There are three kinds of slot, and they behave
differently:

| Holder | Who puts the object there | Reference behaviour today |
|---|---|---|
| `surface`, `displacement`, `atmosphere`, `interior`, `exterior` | `RiSurfaceV`, `RiDisplacementV`, `RiAtmosphereV`, `RiInteriorV`, `RiExteriorV` (`rendererContext.cpp:2344-2409,3293-3306`). The `WorldBegin` default surface (`:990-997`) | **Adopt.** `CShaderInstance`'s constructor attaches itself (`shader.cpp:173`). The handler detaches the old value and stores the new pointer without attaching it. `getShader()` returns `NULL` for `"null"` or a load failure, and the slot then becomes empty. |
| `COptions::imager` | `RiImagerV` (`rendererContext.cpp:1292-1295`) | **Adopt**, the same as the attribute slots. |
| Copy constructor and `restore(shading)` | `CAttributes(const CAttributes*)` (`attributes.cpp:217-279`) and `restore` (`:460-540`) | **Retain.** The copy constructor does `this[0] = a[0]` and then `attach()`es each non-null slot by hand. `restore` detaches its own slots and attaches the other block's. The destructor detaches. |
| `globalMap`, `causticMap` | `CRenderer::render()` (`renderer.cpp:953-965`), lazily, the first time an object with a map name is rendered | **Retain.** `getPhotonMap(name)` returns the cached map, and `render()` calls `attach()`. The copy constructor attaches and the destructor detaches. `restore` copies only the **names**, never the map pointers. |
| `lightSources` entries (`CActiveLight`) | `addLight()` from `RiLightSourceV`, `RiAreaLightSourceV`, `RiIlluminate` and the default light | **Non-owning.** No attach or detach anywhere. The owner is `CRenderer::allLights`: `getShader()` pushes each light there, and `endRenderer()` detaches them (`renderer.cpp:138-144`). |

**Decision**: one value type per handle kind (R2), with operations named after these semantics:
`adopt` for the shader and imager slots, `retain` for copies and maps. Light-list entries stay
non-owning: they hold a raw pointer to the State-owned target type (R2) and are never attached.
[data-model.md](data-model.md) §"Ownership mapping" lists each of today's `attach`/`detach` calls
next to the operation that replaces it, and the State-only test checks the counts at every step
(US1.3).

**Alternatives rejected**:
- **Retain on bind everywhere.** It would give every shader instance one reference too many, so
  none would ever be freed.
- **Owning light entries.** They would make lights outlive `allLights`, which changes their
  lifetime. FR-001 requires lifetime "as today".
- **Explicit retain/release calls placed one-for-one where the `attach`/`detach` calls are today**
  (the thinnest wrapper). This was considered seriously, but rejected for the RAII value type,
  which gives the same final counts with fewer places to get wrong:
  - the destructor and assignment are automatic, so a forgotten release cannot happen;
  - the copy constructor's hand-written `attach` lines are deleted outright. If they were kept
    alongside RAII members, every count would double, so a check in the State-only test fails if
    a copy changes the count by anything other than +1.

## R2. The handle types and how each domain gets its object back

**Decision**: Graphics State owns two empty marker base classes and one handle template.

- **`CShaderGroupRefTarget : public CRefCounter`**: the base of anything a shader slot can hold.
  `CShaderInstance` changes its base from `CRefCounter` to `CShaderGroupRefTarget`. That is a
  Shading → State dependency, which the table allows. The new base has no data members, so
  `CShaderInstance`'s layout and field offsets are unchanged. `CSphereLight`, `CQuadLight` and
  `CProgrammableShaderInstance` inherit it through `CShaderInstance`.
- **`CMapRefTarget : public CRefCounter`**: the base of anything a map slot can hold.
  `CPhotonMap`'s base list (`photonMap.h:74`) replaces `public CRefCounter` with
  `public CMapRefTarget`. That is Hiders → State, which is allowed.
- **`TStateRef<T>`**: a header-only RAII value type, with `ShaderGroupRef = TStateRef<CShaderGroupRefTarget>`
  and `MapRef = TStateRef<CMapRefTarget>`. Its operations:
  - `adopt(T*)` and `retain(T*)`;
  - copy, which retains;
  - move;
  - the destructor, which releases;
  - assignment, which retains the new object before releasing the old one;
  - `get()`, an `explicit operator bool`, and `==`.

Getting the concrete object back is each domain's own inline accessor, a `static_cast` from the
State base. These are the interfaces "owned by the resolving side" that FR-003 asks for:
- Shading: `shaderInstanceOf(const ShaderGroupRef&)` and `lightInstanceOf(const CActiveLight&)`,
  in the new `src/libshader/shading/shaderRefs.h`;
- Hiders: `photonMapOf(const MapRef&)`, in `photonMap.h`;
- the renderer core uses those same Shading and Hiders headers, which it is allowed to include.

Graphics State never calls them.

**Rationale**:
- With a typed base, a map cannot be bound into a shader slot.
- A State-only test double is a plain subclass of the base.
- `static_cast` is resolved at compile time and handles `CPhotonMap`'s multiple-inheritance
  offset correctly.
- The accessors are inline and the handle is header-only. So libshader_shading gains no new
  imported symbol from libRIBRender (spec 024's flat-namespace import count stays at 7), and the
  hot loops gain no call, allocation or lock (the "Hot paths" edge case). Slot accessors return
  `const ShaderGroupRef&`, so reading a slot never touches a reference count.

**Alternatives rejected**:
- **A type-erased `CRefCounter*` in a generic handle.** It needs no hierarchy change, but loses
  type safety, and a test double could not be told apart from a map.
- **`void*` or `reinterpret_cast`.** Wrong for `CPhotonMap`'s offset, and banned by the advice
  that came with the brief.
- **Virtual accessors on the target base**, such as `requiredParameters()` or `parameters()`.
  These would pull shader semantics into State's interface; see R4.
- **`dynamic_cast`.** It costs time in the hot loops and would "check" the light cast that R3
  must leave unchecked.

## R3. The unchecked light cast (DB-013), preserved as-is

Today `addLight(CShaderInstance*)` stores `(CProgrammableShaderInstance *)cLight`
(`attributes.cpp:361`) without a check. The built-in `CSphereLight` and `CQuadLight`
(`hcshader.h:39,68`, renderer core) derive from `CShaderInstance`, not from
`CProgrammableShaderInstance`.

The four Shading consumers read only members `CShaderInstance` declares (`flags`, `categories`,
`prepare`, `illuminate`; `shader.h:168-200`), so the formally undefined cast works in practice:
- `execute.cpp:409`;
- `shading.cpp:1907,1970,2200`.

**Decision**:
- State stores the light as `CShaderGroupRefTarget*` and casts nothing.
- The cast moves, unchanged and still unchecked, into the one Shading inline `lightInstanceOf()`:
  `static_cast<CProgrammableShaderInstance *>(static_cast<CShaderInstance *>(entry.light))`.
  Every class in the chain uses single inheritance with the reference-counted base first, so the
  pointer value is identical to today's C-style cast for programmable and built-in lights alike.
- The four sites call it.
- A comment at the helper names DB-013, and DB-013's History records where the cast now lives.
- No `dynamic_cast`, no assert, no fix (as decided in clarification).

**Test**: the light-order characterization test (R10, T-L1) also compares the pointers it reads
back against the handles `RiLightSource` returned. That comparison runs on the starting tree and
on the finished tree.

## R4. Shader-parameter logic moves to Shading behind a State-owned hook

The callers are:
- `findParameter`:
  - State's `pl.cpp` (9 sites);
  - Geometry's `blobby.cpp:304`;
  - Scene Interface's `rib.y:235`, which sits in an `if (FALSE)` branch.
  - **Correction to the spec's measured starting point:** the renderer core does **not** call it.
    `renderer.cpp:635` is a comment about the display plug-in's own `findParameter`.
- `usedParameters`: read once, at `shading.cpp:904`.

`checkParameters()` runs in these places:
- inside State: the constructor, `addLight`, `removeLight` and `restore(shading)`;
- in the core, after each slot assignment: the five handlers and the `WorldBegin` default surface.

**Decision**: mirror `variableLookup.h` (spec 023).

- **The hook (Graphics State)**: a new `shaderParameterHooks.{h,cpp}` holds a struct of two
  function pointers:
  - `usedParameters(const CAttributes&) -> unsigned`;
  - `findParameter(const CAttributes&, const char*) -> CVariable*`.

  It has `installShaderParameterHooks(const SShaderParameterHooks*)`, where `nullptr`
  uninstalls.
- **What `CAttributes` does with it**:
  - `checkParameters()` stays a member. It stores `hooks ? hooks->usedParameters(*this) : 0`, so
    `usedParameters` stays a stored field and the per-grid read stays a field read.
  - `findParameter()` stays a member that forwards to the hook, or returns `nullptr` when none is
    installed. So `pl.cpp`, `blobby.cpp` and `rib.y` are not edited.
- **The logic (Shading)**: a new `src/libshader/shading/shaderParameters.{h,cpp}` holds
  `libshader::usedShaderParameters`, `libshader::findShaderParameter` and
  `libshader::shaderParameterHooks()`. Moved verbatim:
  - the union over displacement, surface, atmosphere, interior, exterior and the lights, plus
    `PARAMETER_N` when no surface is bound;
  - the precedence walk: displacement, then surface, then atmosphere.
- **Installation (renderer core)**: inside `rendererShadingServicesInit()`, which runs in
  `beginRenderer()` right after `initDeclarations()` and before `CRendererContext` constructs its
  first `CAttributes`. It is uninstalled in `rendererShadingServicesShutdown()`, which runs last in
  `endRenderer()`. That file is ribRender-only.
  - **Not** `rendererDeclarations.cpp`: ribVector compiles that file and does not link
    `libshader_shading`, so ribVector would fail to link.
- **The bxdf slot** is excluded from both answers in 025. Nothing binds it yet, so including it
  would change no answer today. Whether and how a Bxdf contributes is spec 035's call.
- **With no hook installed** the results are `usedParameters = 0` and `findParameter = nullptr`.
  These are defined results for State-only programs, ribVector (the preview) and ribOut.
  - The preview never binds shaders, so today's `findParameter` already returns `NULL` there.
  - `usedParameters` is read only by Shading, which runs only in ribRender, where the hook is
    installed before the first attribute block exists.
  - Unit tests that build a `CAttributes` without `RiBegin` (the CSG tests and
    `test_interior_exterior_selection`) never read `usedParameters`.
- **Recompute points**: the `bind*` setters (R6) call `checkParameters()` themselves, replacing the
  core's separate calls one-for-one. The recompute points are therefore exactly today's (FR-005).

**Alternatives rejected**:
- **Virtual `requiredParameters()` and `parameters()` on `CShaderGroupRefTarget`.** The
  union-and-precedence logic would stay in State, against FR-005's "provided by Shading".
- **Shading computes `usedParameters` lazily at `shading.cpp:904`.** Simpler, but it walks the
  slots and lights on every grid, a hot-path cost, and it moves the recompute points (FR-005).
- **Installing from a libshader static initializer.** Static initialization order is not
  guaranteed, and the hook would be installed in processes that never call `RiBegin`.
- **Installing in `initDeclarations()`.** It breaks the ribVector link, as noted above.

**FR-003 wording**: the hook is State calling through a State-owned function-pointer interface,
like `lookupVariable()`. It adds no include or link edge. The plan reads FR-003's "MUST NOT call
back into them" as "no direct reference" (see the plan's Open questions, Q2).

## R5. Light order today is not RIB order inside a cloned block

`addLight` appends at the tail (`attributes.cpp:353-359`). But the copy constructor (`:259-264`)
and `restore(shading)` (`:500-505`) **prepend**, so both reverse the list. Two kinds of clone
trigger this:
- every `attributeBegin()`, which is `new CAttributes(currentAttributes)`
  (`rendererContext.cpp:874-877`);
- every copy-on-write clone in `getAttributes(TRUE)` (`:258-265`).

So the lights of a block nested at odd depth run in reverse RIB order. Light accumulation order
reaches the bits of `Ci`, which the `ColorDeterminism_*` checks compare.

**Decision**:
- Preserve the reversal exactly: the copy constructor and `restore` keep prepending.
- Pin it with a characterization test that runs green on the starting tree and on the finished
  tree (T-L1).
- Record it as a finding in research and in the report, like the cast.
- Spec US2.3's "lights run in RIB order" is a wording question for the maintainer (plan Q1).
- No fix in 025: a fix would change rendered output, against FR-022.

## R6. Slot API, the last-bound record and FR-024

**Decision**:
- **Slots are private** with inline const accessors: `surface()`, `displacement()`,
  `atmosphere()`, `interior()`, `exterior()`, `bxdf()`, each returning `const ShaderGroupRef&`.
- **Writes go through setters**:
  - `bindSurface(ShaderGroupRef)` and `bindBxdf(ShaderGroupRef)` store the handle, set
    `lastMaterialBinding` to Surface or Bxdf, and recompute the parameters;
  - `bindDisplacement`, `bindAtmosphere`, `bindInterior` and `bindExterior` store and recompute;
  - `setDefaultSurface(ShaderGroupRef)` stores and recomputes **without** touching the record.
    It is used only by `RiWorldBegin` (FR-024, clarification Q2).
- **Query**: `EMaterialBinding governingMaterial() const` returns `None`, `Surface` or `Bxdf`
  (FR-011).
- **An empty handle updates the record** (clarify point): `bindSurface()` of an empty handle, from
  `Surface "null"` or a failed load, sets the record to Surface exactly as a non-empty bind does.
  The State-only test covers it.
- **The record is a plain enum field**, so the copy constructor's memberwise assignment carries it
  into clones and attribute pushes, and `restore(shading)` copies it explicitly (FR-010).
- **Maps stay public** `MapRef globalMap, causticMap`. No invariant needs guarding, and the
  renderer core binds them lazily.
- **`COptions::imager` stays public** as a `ShaderGroupRef`.

**Rationale**: with private slots, no write can bypass the record (FR-011) and no State-external
code can retain or adopt the wrong way. All of the roughly 60 sites change anyway, because each now
resolves through `shaderInstanceOf()`.

**Alternative rejected**: public `ShaderGroupRef` slot fields. One stray `attrs->surface = …`
would silently leave the record stale.

## R7. The scoped pattern-node table

**Decision**:
- **The table**: `CPatternTable`, a value member of `CAttributes`, mapping a handle string to a
  `ShaderGroupRef`.
  - Its storage is a reference-counted, copy-on-write vector of `(std::string, ShaderGroupRef)`
    entries in first-definition order.
  - Copying the table (the copy constructor and `restore(shading)`) shares the storage, which
    costs O(1). `define()` copies the storage first if it is shared.
- **Operations**:
  - `define(handle, ref)` replaces an existing entry in place, keeping its position, or appends;
  - `lookup(handle)` returns `const ShaderGroupRef*`, or `nullptr` for not found, and writes no
    diagnostic (FR-015);
  - `forEach(fn)` visits the visible entries in definition order.
- **Scoping follows from value semantics.** `AttributeBegin` clones the block, the inner block
  defines into its own copy, and `AttributeEnd` drops the inner block. So the semantics in FR-013
  and the clarification hold:
  - visible in nested scopes;
  - gone after the scope ends;
  - replace in the same scope;
  - a nested definition shadows the outer one until its scope ends.
- **Nodes are `ShaderGroupRef`s.** A slot bound from the table shares the reference, so the node
  outlives its scope (FR-014).
- **Nothing consumes the table in 025** (FR-012).

**Alternatives rejected**:
- **Copying the table eagerly on every clone.** That costs O(n) per `AttributeBegin` once spec 034
  fills it.
- **A parent-chained scope list.** State cannot tell a scope push from a copy-on-write clone (both
  are `new CAttributes(a)`), so the chain would mis-scope.
- **A distinct `PatternNodeRef` type.** A slot could then not hold a node, so FR-014 would need a
  conversion. Spec 034 may still introduce one.

## R8. The channel registry moves to Graphics State

Today (`rendererDeclarations.cpp:73-74,286-304,352-355,534-633`; `rendererStatics.cpp:118-119`):
- **Storage**: `CTrie<CDisplayChannel*> declaredChannels` plus `CArray<CDisplayChannel*>
  displayChannels`, holding the five built-ins in this order: `rgb` (3,0), `rgba` (4,0), `a` (1,3),
  `z` (1,4), `rgbaz` (5,0).
- **Declare from a string**:
  - `parseVariable`, then a lookup;
  - an existing channel comes back when its shape matches; otherwise
    `error(CODE_SYSTEM, "Channel \"%s\" was previously defined differently\n")`;
  - a new name goes through `CRenderer::declareVariable(NULL, type)`, which fails with
    `error(CODE_SYSTEM, "Channel definition \"%s\" is ill formed\n")`.
- **Declare from a variable**: an existing channel comes back when its variable is the same one;
  otherwise `NULL`, with no message.
- **Retrieve** by name.
- **Reset usage**: `sampleStart = -1` for variable-backed channels, in declaration order.
- **Shutdown**: `declaredChannels->destroy()` deletes the channels; `delete displayChannels` frees
  only the array.

**Decision**:
- **`CChannelRegistry`**: a new `src/ri/state/channelRegistry.{h,cpp}` class with the same
  storage and the same five built-ins, registered by its constructor. Methods:
  - `declare(const char*)`, `declare(CVariable*)`, `retrieve()`, `resetUsage()`, `forEach()`;
  - the destructor reproduces today's ownership: it deletes each channel once and frees the array.
  - It can be instantiated, so tests build their own.
- **The session instance**: a process-wide instance owned by State, behind
  `beginChannelRegistry()`, `endChannelRegistry()` and `channelRegistry()`. It is created in
  `initDeclarations()` where the two `new`s are today, and destroyed in `shutdownDeclarations()` at
  today's position, before the variables are destroyed. Both functions run on the renderer, ribOut
  and preview paths alike, so a second `RiBegin` starts fresh (the "Session reuse" edge case).
- **Declared-variable access**:
  - State's `variableLookup.h` gains a second hook beside the lookup:
    `installVariableDeclaration(TVariableDeclaration)`, with
    `TVariableDeclaration = CVariable*(*)(const char *name, const char *decl)`, and
    `declareVariableThroughHook()`;
  - `initDeclarations()` installs `&CRenderer::declareVariable`, and `shutdownDeclarations()`
    uninstalls it, next to the existing `installVariableLookup` calls;
  - with no hook, `declare(const char*)` produces today's "ill formed" error and returns
    `nullptr`, a defined result.
- **Error strings and codes** stay byte-identical.
- **The core**:
  - `RiDisplayChannelV` (`rendererContext.cpp:1483`) and the display setup
    (`rendererDisplay.cpp:563,618,671`) call `channelRegistry()` directly; the core may depend on
    State;
  - `CRenderer::declaredChannels`, `CRenderer::displayChannels`, both `declareDisplayChannel`
    overloads and `resetDisplayChannelUsage` are deleted;
  - `CRenderer::retrieveDisplayChannel(const char*)` stays, declared in `renderer.h` and
    **defined out of line** in `rendererDeclarations.cpp`, as a one-line forwarder (FR-018).

**Why the forwarder must stay out of line**: if it were inline, `texture3d.o` would reference State's
`channelRegistry()`/`CChannelRegistry::retrieve` instead. The DB-010 link line would go STALE, and a
Texturing → State link VIOLATION would appear, because Texturing may depend on nothing. With the
out-of-line forwarder, `texture3d.cpp` has no diff and both of its lines (DB-010 link, DB-021
include) stay as they are.

**Alternatives rejected**:
- **The core declares the variable and then registers the channel.** FR-016 requires that the
  registry own declaration from a string.
- **A `static CChannelRegistry*` in `CRenderer`.** That still leaves a channel table among
  `CRenderer`'s statics, which FR-016 and SC-008 forbid.

## R9. The State vocabulary header

Today State takes two names from other domains:
- `ParameterType` from `dsply.h:38-44`, used by `COptions::CDisplay::TDisplayParameter::type` and
  by the copy constructor's `switch` (`options.cpp:136-148`);
- `BLOBBY_ORDER_RISPEC` from `blobbyField.h:156-159`, used as the default of the `int`
  `blobbyOpcodeOrder` (`options.cpp:266`).

`RtDisplay*Function` is already infrastructure.

**Decision**: a new `src/ri/state/stateVocabulary.h` defines:
- `enum class EDisplayParameterType : int { Float = 0, Vector = 1, Matrix = 2, String = 3, Integer = 4 }`;
- `enum class EBlobbyOrder : int { RiSpec = 0, AppNote = 1 }`.

These are scoped enums, so their names cannot collide in a TU that also includes `dsply.h` or
`blobbyField.h`.
- `TDisplayParameter::type` becomes `EDisplayParameterType`.
- `blobbyOpcodeOrder` stays `int` and defaults to `static_cast<int>(EBlobbyOrder::RiSpec)`.
- The core converts explicitly where the two vocabularies meet:
  - the five writes at `rendererContext.cpp:1417-1448`;
  - the comparison in `rendererDisplay.cpp:48-57`, where `findParameter`'s `ParameterType` is
    compared against the stored type.

  Mixed-enum comparison is ill-formed for scoped enums and deprecated for unscoped ones in C++20,
  an error under `-Werror`. One `static_assert` beside the conversion helper guards the values at
  compile time too.
- `State_VocabularyConsistency`, a Consumer test that includes all three headers, compares every
  pair at run time and fails naming the drifted pair (FR-008, SC-007).
- `dsply.h` and `blobbyField.h` are unchanged.

**Limitation**: neither C enum has a count sentinel. The test pins every existing value, but it
cannot detect an enumerator *added* to `dsply.h`. Since `dsply.h` must not change in 025
(DB-003/DB-014), this is recorded, not engineered around.

## R10. Test strategy and what can honestly be red first

| Test | Red on `dfffba0` because | Notes |
|---|---|---|
| `State_Isolation` | **build failure**: `attributes.h`/`options.h` include `shader.h`, `dsply.h` and `blobbyField.h`, which do not resolve on the narrow include path (R11), and the new headers do not exist | Covers handles, counts, the record, the pattern table and the registry (SC-006) |
| `State_VocabularyConsistency` | build failure: `stateVocabulary.h` does not exist | A mutation of one value is shown red (SC-007) |
| `ShadingParity_ShaderParameterEquivalence` part B | build failure: the `libshader::` API does not exist | Part A runs **green on the starting tree**: it compares an API-neutral oracle (bound shaders loaded with `CRenderer::getShader`, distinct defaults per slot for the shared parameter name) against `CAttributes::findParameter`/`usedParameters`. That proves the oracle captures today's answers. A precedence-swap mutation is shown red (FR-007) |
| `State_LightOrderCharacterization` | **not red: a characterization test**, green on both trees | Pins R5's reversal and R3's pointer identity through pointer-value comparisons that compile on both trees |
| `RibOut_ShaderStatementOrder` | **not red: a characterization test**, green on both trees | Clarification Q3; FR-023 says "record … as an automated test", and FR-021's red list does not include it |
| `ctest -L architecture` after the 7 DB-008 lines are deleted | 7 `VIOLATION` lines until the code lands | Any other violation means the design is wrong |

The red evidence goes into `validation.md` during implement, as in specs 023 and 024. The two
characterization tests are named as such in the plan, so nobody mistakes a green first run for a
broken test.

## R11. Building the State-only test

**Decision**: `tests/unit/state/` compiles the State sources (`src/ri/state/*.cpp`) directly into
each State-only test executable and links only `openrendercommon` (the shared `libRIBCommon`).
`tests/framebuffer/CMakeLists.txt` is the precedent.

The include path is deliberately narrow:
- `src/`, for `common/…`;
- `src/ri/state`;
- `${CMAKE_BINARY_DIR}`, for `config.h`;
- a test-only staging directory holding a copy of `src/ri/core/ri_config.h`
  (`configure_file(... COPYONLY)`). `src/ri/core` itself is **off** the path because it also holds
  the Shading shims (`shader.h`, `shading.h`).

So a bare `#include "shader.h"`, `"photonMap.h"`, `"dsply.h"` or `"blobbyField.h"` in State fails
to compile, and an undefined reference into any other domain fails to link. A domain-qualified
spelling such as `"display/dsply.h"` would still resolve through `src/`, so it is caught by the
architecture check and the SC-003 grep, not by this test.

**archcheck**: a State source compiled by a second target becomes a second translation unit with
the same source path, includes and symbols, so it can only repeat edges the production unit
already has. `addUnit` (`includes.cpp:167-178`) does not deduplicate units. Two things are
expected but **not yet verified**:
- whether a repeated edge is reported once or twice;
- that the header copy in the build tree, which maps to no source class, is ignored.

Quickstart §2 checks both the first time the check runs on the finished tree. If either fails,
the fallback is Q3's production OBJECT target, which compiles each State source once.

**Alternative**: a production `ribState_obj` OBJECT library with the narrow include path, linked
into ribRender, ribVector and the test. It would advance DB-023 ("per-domain include visibility
first, then per-domain OBJECT targets"). It is not required by the spec, so it is plan question Q3
with a recommendation.

## R12. Transitive includes

`attributes.h` and `options.h` stop including `shader.h`, `dsply.h` and `blobbyField.h`. Units
that relied on what those headers pulled in will fail to compile.

**Decision**: fix them only with:
- infrastructure headers (`common/rendererc.h`, `common/variable.h`, `ri_config.h`, …);
- their own domain's headers;
- in the renderer core and in consumers, any header.

If a Geometry, Texturing or Scene Interface unit turns out to need a Shading, Hiders or Display
header, implementation **stops and reports it**. The spec's "none" is to be verified, not assumed.

The known core additions are:
- `rendererContext.cpp` gets `blobbyField.h` and `dsply.h`;
- `rendererDisplay.cpp` gets `dsply.h`;
- `renderer.cpp` gets `photonMap.h` and `shaderRefs.h`.

## R13. Registry, allowlist and metric

- **Allowlist**:
  - delete the 7 DB-008 lines and their section header;
  - add, swap and keep no other line, apart from any line the check reports `STALE`;
  - DB-010's link line for `retrieveDisplayChannel` and DB-021's include line stay.
- **Metric**: no `CRenderer::` reference is added or removed in a measured class. Texturing's call
  to the forwarder is unchanged, and State has no row. So `metrics-baseline.txt` is expected not to
  change; it is lowered only if `NOTE metric … < baseline` appears (FR-019).
- **`DOMAIN_BOUNDARIES.md`**:
  - **DB-008** moves to Resolved with "spec 025, <implementation date>" and what was done.
  - **DB-021** stays Open. Its evidence drops "the registry itself is renderer-core state" and
    keeps the include and the call through the `CRenderer` forwarder. The tracking line keeps 037.
  - **DB-010**'s evidence notes that `CRenderer::retrieveDisplayChannel` now forwards to the
    State-owned registry.
  - **DB-013** History records where the unchecked light cast now lives.
  - Nothing refers to the merge.
