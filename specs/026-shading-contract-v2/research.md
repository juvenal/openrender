# Research: Shading Contract v2

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Spec**: [spec.md](spec.md) | **Plan**: [plan.md](plan.md) | **Date**: 2026-10-30

Every finding below was read from the worktree at `0475e1e` (master `d01da47` plus the committed
spec). No build was run at the plan stage. Where a number comes from a binary, the binary is named,
and Phase 1 measures it again on this worktree's own build (quickstart §0).

Decision IDs D1-D30 refer to `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §0. The ones this
spec relies on are:

- **D22**: the renderer-services API is versioned on its own (R2);
- **D25**: 026 stays internal C++ (R2);
- **D20**: the shading-cell footprint is √ShadingRate pixels (R12);
- **D30**: the light service, the BRDF library and the public libraries belong to Shading, so this
  boundary is Shading's own (R1).

---

## R1. Where the interfaces live

**Decision**: every new interface is Shading-owned and lives in `src/libshader/shading/`. The
renderer core implements it, and a Hiders adapter answers the tracer.

| Header (new unless noted) | Holds | Includes (all allowed) |
|---|---|---|
| `rendererServices.h` (edited) | `CRendererServices` v2, the version constant and its history | `common/*`, `serviceTypes.h`, forward declarations |
| `serviceTypes.h` | `ObjectHandle`, `ShaderHandle`, `EQueryStatus`, `QueryType`, the request and answer structs of `getAttribute`/`getMessage`/`getMatrix` | `common/global.h`, `common/rendererc.h`, `common/variable.h` (Infra) |
| `tracer.h` | `ITracer` | `serviceTypes.h`; forward-declares `CShadingContext`, `CRay`, `CRayBundle` |
| `traceCalls.{h,cpp}` | `libshader::traceRay`/`traceBundle`, the one place Shading hands a ray to the tracer, with the no-tracer default | `shading.h`, `tracer.h` |
| `shaderGlobals.h` | `ShaderGlobals`, `Strided<T>`, `RayType`, `HitMode`, `TransportMode`, `ShadingPass`, `BatchShape` | `serviceTypes.h`, `common/sampling.h` (Infra) |
| `situation.{h,cpp}` | `libshader::classifyShading()`, the FR-017 derivation as a pure function, called only by tests (FR-018) | `shaderGlobals.h` |
| `sloLoader.{h,cpp}` (moved, Q5) | `parseSloShader()`, moved verbatim from `src/ri/render/rendererFiles.cpp:53-343` | the includes it has today, minus `renderer.h` |

The implementers:

- `CRendererServicesImpl` (`src/ri/render/rendererServicesImpl.h`, renderer core). The new members are
  defined out of line in `src/ri/render/rendererShadingServices.cpp`, which is already the core's
  services unit.
- `CHiderTracer` (`src/ri/hiders/hiderTracer.{h,cpp}`, Hiders, new). Hiders -> Shading is allowed.
- The core owns one `CHiderTracer` and installs it per frame (R6).

**Rationale**:

- Shading may depend only on Graphics State, Texturing and Shader DSO Operands, so the interfaces must
  name nothing else. Every header above includes only Shading or Infra headers.
- `CRay` is in `src/ri/core/ray.h`, which `domain-map.txt` classifies as Infra. `CRayBundle` is
  declared in `shading.h`. FR-010's "today's types" are therefore already reachable from Shading
  without a new edge.
- The core may depend on every domain, so `CRendererServicesImpl` may cast handles back to `CObject`,
  `CShaderInstance` and `CXform`.

**Alternatives considered**:

- *One header for everything.* Rejected: `shaderGlobals.h` has to be includable by Geometry
  (`fillShaderGlobals`, spec 029) without pulling in the services.
- *The adapter in the core* (`src/ri/render/rendererTracer.cpp`). Allowed by the table, but the spec
  says "the hiders' existing trace code answers it", and 029 makes the hiders implement `ITracer`
  themselves. Keeping the adapter in Hiders already puts it where 029 wants it.

## R2. Version constant and its rules

**Decision**: one constant covers the whole Shading boundary (services, `ITracer`, `ShaderGlobals`
and their vocabularies), following D22.

```cpp
namespace openrender::shading {
struct InterfaceVersion { int major, minor, patch; };
inline constexpr InterfaceVersion kRendererServicesVersion{2, 0, 0};
}
```

The bump rules are written in the header:

- **MAJOR**: a method, field or enumerator is removed or renamed; a signature changes; a documented
  result or not-found behaviour changes.
- **MINOR**: a member is added with a documented no-implementation default; a field is added at the
  end of a struct; an enumerator is appended.
- **PATCH**: the documentation changes, or an implementation is fixed with no change to the contract.

The version history is a table in the same header. Its first row is `2.0.0 (spec 026, <the date the header task is done>)`,
and it states that version 1 was the unversioned interface of specs 024, 025 and 027.

There is no run-time check (FR-001). Each implementer instead carries a `static_assert(kRendererServicesVersion.major == 2)`:
`CRendererServicesImpl`, `CHiderTracer` and the doubles. A MAJOR bump therefore fails to compile in
every implementer until someone has looked at it. This is the "visible act" the spec asks for.

**Alternative**: separate constants for the services, the tracer and `ShaderGlobals` (maintainer
question Q9). Rejected (Q9, decided 2026-10-30): D22 names one renderer-services API version, and the
three parts are adapted together by spec 049.

## R3. The 57 existing methods (FR-014)

**Finding**: `CRendererServices` has **57** pure virtual methods plus the virtual destructor. Two of
the declarations span two lines, which is why a one-line `= 0;` count finds 55.
`rendererServicesImpl.h` has 57 `override`s. The spec's "56" is off by one.

The classification is in [contracts/renderer-services-v2.md](contracts/renderer-services-v2.md#classification-of-the-57-existing-methods):

| Class | Count | Members |
|---|---|---|
| Unchanged | 28 | grid/globals (3), timing (3), raster (3), `additionalParameters`, clip (2), `hasIlluminationHook`, world bounds (2), GI/3D getters (3), the 3D forwarders (3), `textureSystem`, `getGlobalID`, `shootStep`, the filter/variable/DSO lookups (4) |
| Replaced (signature) | 1 | `setOffendingObject(CObject *)` becomes `setOffendingObject(ObjectHandle)` (R5, Q11) |
| Removed | 28 | `findCoordinateSystem`, `findCoordinateSystemWithType` (`getMatrix` replaces them); the 25 option getters `xres` ... `samplingSeed` and `lookupUserOption` (`getAttribute` replaces them, Q3) |

The removal rule ("only when nothing in the tree calls it afterward") is met as follows. Every caller
was listed with a grep of `src/` and `tests/` that left out overrides and forwarders:

- **The 26 option members**: each has exactly one caller, `CShadingContext::options()`
  (`shading.cpp:1665-1796`). Once `options()` asks `getAttribute`, none is left.
- **`findCoordinateSystem` (3 arguments)**: it already has no caller today.
- **`findCoordinateSystemWithType`**: its only caller is `shading.cpp:2857`.
- **`shutterOpen`, `shutterClose`, `clipMin` and `clipMax` are not removed**: they have other callers,
  `complete()` (`shading.cpp:268-272, 407-411`) and `rendererClip*` (`:569-570`).

Removing members breaks the overrides in the four test classes that implement the whole interface.
Those classes are `CForwardingServices` (`table_services.h:32`), `CRecordingServices` and
`CStubServices` (`recording_services.h:31, 143`), and `CTextureServices`
(`recording_texture_system.h:156`). `CCountingTableServices` and `CTableServices` derive from
`CForwardingServices`. Deleting those overrides is an edit to the doubles, which FR-023 only permits
for "the doubles' base class". Q3 (decided 2026-10-30) allows exactly those deletions, and FR-023 is amended.

## R4. `getAttribute`: attribute(), option(), rendererinfo() (FR-005)

**Finding: master never refuses on type.**

- `attributes()` (`shading.cpp:1802-1883`), `options()` (`:1665-1800`) and `rendererInfo()`
  (`:1885-1903`) match on the name and write their own fixed layout into `dest`. For example,
  `ShadingRate` writes 1 float, `name` writes one `char *`, and `user:x` writes `var->numFloats`
  floats or pointers.
- None of them is told which overload the shader used.
- The interpreter's `PARAMETEREXPR_PRE(accessor)` (`shaderFunctions.h:1161`) calls the member the
  same way for the F/V/S/M overloads, and the JIT's `jit*Parameter` (`shading.cpp:3808-3887`) passes
  `resultKind` only to `jitParameterFinish`.
- A mismatch (`attribute("user:m", f)` where `user:m` is a matrix) therefore writes 16 floats into a
  1-float destination. That is undefined behaviour. **Resolved in part by F1 (2026-10-31)**: the unsafe mismatches are refused, the residual is
  filed as O7 (R16).

**Decision**:

- **The request** is `AttributeRequest{scope, object, name, expected}`. `scope` is
  `Attribute | Option | RendererInfo`. `expected` is a `QueryType` (`Float | Triple | Matrix | String`)
  that the caller derives from the overload or from `resultKind`.
- **The interpreter** gets its expected type from a new second argument of `PARAMETEREXPR_PRE`. That
  touches 32 `DEFFUNC` lines in `shaderFunctions.h:1291-1376`.
- **The JIT** maps `resultKind`: 1 -> Float, 3 -> Triple, 16 -> Matrix, -1 -> String. No emitter or
  `op_*` ABI change.
- **The answer** is `AttributeValue{status, type, count, data, inline[16]}`. The caller owns it and
  passes it in (`getAttribute(request, answer)`); the type is non-copyable. Returning it by value
  would leave `data` dangling whenever it points into its own inline buffer, because C++20 does not
  guarantee copy elision for a named local.
  - `data` points either at the renderer's own storage (`var->defaultValue`, `&attributes->name`) or
    at the answer's inline buffer, for computed values (`Sides` from the flags, `Format`).
  - `status` is `Found`, `NotFound`, `TypeMismatch` or `Unavailable` (R9). `TypeMismatch` (F1, FR-026) means the copy cannot be made safely:
    `expected` is `Triple` and the stored value has more than 3 floats, `Matrix` and more than 16, `String` and the stored value is not a string,
    or any other type and the stored value is a string. A stored value smaller than the destination is not a mismatch. `Float` has no size
    limit, because the builtin does not know whether the destination is a scalar or an array (`option("Format", fmt[3])`); that residual is O7.
- **The name table** moves into `CRendererServicesImpl::getAttribute()`, which reads `CAttributes` and
  the `CRenderer` statics with exactly the expressions and casts `attributes()`/`options()` use today
  (`(float)currentAttributes->shadingRate`, `(float)svc->xres()` becomes `(float)CRenderer::xres`).
  `rendererInfo()` constants move with it. The JIT-wide default answers come along unchanged.
- **The copy stays in Shading** (FR-005's "conversion stays in Shading"):
  - `Found` copies the raw value into `dest` in the stored layout, exactly as master writes it, and the builtin returns found = 1;
  - `NotFound`, `Unavailable` and `TypeMismatch` leave `dest` untouched and return 0; `TypeMismatch` also warns once per builtin kind per process.
- **The contract pins this**: `TypeMismatch` is refused in v2.0.0 (FR-026). This is the first version of the status, so it is not a MAJOR change;
  Q4's plan-stage "treated as `Found`" is superseded.

**Rationale**:

- Byte-identity (FR-022) forbids a refusal that changes a correct output, so the rule refuses only the mismatches that overflow a destination of
  known size or mix a string with a number. Both are undefined behaviour on master, and CP-d proves no scene of the suite reaches one.
- Reporting `TypeMismatch` gives spec 049's OSL adapter the status OSL's `get_attribute` needs, with
  no behaviour change now.
- Moving the name table into the renderer is what lets a double answer `option("Format")` from its
  own table (US3).

**Alternatives**:

- *Keep the name tables in Shading and ask the services only for raw fields.* Rejected: the double
  would have to look like `CAttributes`, and `get_attribute` would carry no names.
- *Pass `dest` to the service.* Rejected: the copy (the "conversion") must stay in Shading.
- *Refuse every type-class difference, a float stored in a vector destination included.* Rejected: `option("Format", fmt[3])` copies 3 floats
  into an array on master and is used by `scene_state_probe.sl`; the builtin cannot tell an array from a scalar.

## R5. Object, shader and light handles; the reads that stay

**Decision**: the handles are opaque, typed wrappers.

```cpp
struct ObjectHandle { const void *ptr = nullptr; explicit operator bool() const { return ptr != nullptr; } };
struct ShaderHandle { const void *ptr = nullptr; explicit operator bool() const { return ptr != nullptr; } };
```

- **Object handles**: Shading makes them in one place, `CShadingContext::objectHandle()`, defined out
  of line in `shading.cpp`. That unit keeps `object.h` (the DB-006 line that stays), so it can upcast
  `currentObject` to `const CObject *`.
- **Shader and light handles**: made from a `CShaderInstance *` (a Shading type).
- **Casting back**: only an implementer turns a handle back into a type (`CRendererServicesImpl`:
  `static_cast<const CObject *>(h.ptr)`). A double treats handles as keys.

**The executor reads that stay** (SC-002's list). The eight that are compiled into `execute.cpp` or
`executeMisc.cpp` go through a new out-of-line accessor,
`const CAttributes *CShadingContext::objectAttributes() const` (in `shading.cpp`), so that those two
units stop needing `CSurface`'s complete type. The eight are `execute.cpp:183, 308, 385`,
`executeMisc.cpp:720, 846`, `shaderFunctions.h:364` and `giFunctions.h:291, 338`; the macro headers
count because they are expanded in `execute.cpp`. The reads in `shading.cpp` itself stay direct: that
unit keeps `object.h`, and leaving them avoids a needless diff.

| Site | Reads | Why it stays in 026 |
|---|---|---|
| `execute.cpp:183` (`scripterror`) | the object pointer, for `setOffendingObject` | Becomes `rendererSetOffendingObject(objectHandle())`; the upcast is what needed `surface.h` |
| `execute.cpp:308` (`plBegin`), `shading.cpp:3066` (`jitParameterList`) | `lookup->init(scratch, attributes)`: the parameter-list defaults (bias, max distance, samples, ...) | An executor default, not an RSL query; 029 passes it in `ShaderGlobals`' object |
| `execute.cpp:385` (`runAmbientLights`), `shading.cpp:1935, 1994, 2223` | `lightSources` | Light iteration; the light service is spec 041 |
| `executeMisc.cpp:720`, `shading.cpp:4428` | `flags & ATTRIBUTES_FLAGS_SAMPLEMOTION` | Ray setup for `transmission`/`gather` |
| `executeMisc.cpp:846-848` | `interior()`, `exterior()`, `flags` | `trace()`'s choice of interior and exterior bundle; 029 |
| `shaderFunctions.h:364`, `shading.cpp:2974` | `flags & ATTRIBUTES_FLAGS_INSIDE` | `calculatenormal()`'s orientation; 029 (`ShaderGlobals` orientation) |
| `giFunctions.h:291, 338`, `shading.cpp:4154` | `photonEstimator` | `photonmap()`; spec 032 |
| `shading.cpp:856-1395` (`shade()`) | every shade input | `shade()` takes the primitive until 029 (FR-018) |
| `shading.cpp:2808` (`shaderName(type)`) | the bound shaders | `shadername(type)` is outside FR-006's fixed six (Q8) |

- **No transitive Geometry path is left.** `bundles.h` -> `ray.h` (Infra; it forward-declares
  `CSurface`) and `shading.h`. `irradiance.h` -> `depository.h`, `options.h`, `shader.h`,
  `texture3d.h`. `photonMap.h` -> `dataView.h`, `map.h`, `ray.h`, `stateHandles.h`, `xform.h`. None
  reaches `object.h` or `surface.h`, so deleting the two direct includes removes the coupling, not
  just the edge.
- **Includes those files gain**: `execute.cpp` gains a direct `#include "attributes.h"` (Shading ->
  State, allowed). `executeMisc.cpp` already has it.

**Rejected: caching `const CAttributes *` in `CShadingState` and filling it in `shade()`.** Several
places write `currentObject` without going through `shade()`, and none of them would fill the cache:

- `photon.cpp:178` (the phony object);
- `imager.cpp:82` (`nullptr`);
- three existing tests (`texture_substitute_harness.h:89`, `test_false_if_constructs.cpp:99`,
  `test_ambient_accumulation.cpp:175,255`), which FR-023 does not let us edit.

## R6. `ITracer`, its installation and FR-010's concurrency question

**Decision**:

```cpp
class ITracer {
    public:
        virtual ~ITracer() = default;
        virtual void traceRay(CShadingContext &context, CRay *ray) = 0;          // trace(CRay *): no shading
        virtual void traceBundle(CShadingContext &context, CRayBundle *bundle) = 0; // traceEx(CRayBundle *): one level deeper, shaded
};
```

- **Reaching it**: Shading calls `CRendererServices::tracer()`, which returns `ITracer *` and is
  `nullptr` between frames. Like `textureSystem()`, it is MINOR-style, with a default of `nullptr`.
- **The adapter**: `CHiderTracer` forwards `traceRay` to `context.trace(ray)` and `traceBundle` to
  `context.traceEx(bundle)`. Those are the unchanged Hiders bodies in `trace.cpp:253, 303`.
- **Installation**: the core installs one static `CHiderTracer` per frame. `CRenderer::frameTracer` is
  set in `beginFrame()` right after `textureSystem` is created (`renderer.cpp:764`), so before any
  context exists (`:799`). It is cleared in `endFrame()` after the contexts are deleted (`:843-847`).
  `textureSystem()` (spec 027) and `frameParams()` (spec 028) follow the same discipline.

**Answer to FR-010: concurrent hiders cannot overwrite each other.**

1. **One hider class per frame.** `beginFrame()` makes every thread's context with the same
   `strcmp(hider, ...)` chain (`renderer.cpp:800-823`), so all of a frame's contexts are one hider
   class. A photon map is produced by its own frame (`Hider "photon"`, `dispatchPhoton`); a photon pass
   and a camera pass never share a frame.
2. **The adapter is stateless.** Every piece of per-thread trace state is a member of the context
   passed as the argument: `currentRayDepth`, `currentRayLabel`, `freeStates`, `traceObjectHash`
   (`shading.h` private section), and `trace.cpp` reads them through `this`. So even a future frame
   with mixed hiders could not overwrite another thread's state through the tracer.
3. **The slot is written only while no render thread exists.** The imager runs during the frame's
   dispatch (`rendererDisplay.cpp:315`, before `endFrame()`), so it sees the tracer installed.
4. **Re-entrancy works as today.** A tracer that shades re-enters `shade()` through
   `traceEx` -> `trace(bundle)` -> `CSurface::shade` -> `shade()`, and a shader there may call a traced
   builtin again. The adapter holds nothing across the call.

**Call sites that go through it** (both backends reach the same members):

- `executeMisc.cpp:793, 939` (`trace(CRay *)`, the probe forms);
- `executeMisc.cpp:805, 823, 954, 971, 990, 999` (`traceEx`) in `traceTransmission`/`traceReflection`.
  Those two are reached by `transmission`, `visibility`, `trace` and the ray-traced
  `environment`/`shadow` (`giFunctions.h:98-154`; `shading.cpp:3296-3298`; `jitTraceBatch`
  `shading.cpp:3432-3434`);
- `shading.cpp:4487, 4498` (`gather`, through `gatherSample`, shared with the JIT);
- by Q6 (decided): `irradianceDispatch.cpp:371, 471` (Hiders), the rays of `occlusion`/`indirectdiffuse`.

**No tracer installed** (FR-012). The `traceCalls.cpp` helpers warn once per kind per process
(`CODE_NOTSTARTED`, as `textureCalls` does). Then:

- `traceRay` sets `ray->object = NULL` (a miss);
- `traceBundle` calls `bundle->postShade(bundle->numRays, bundle->rays)`. That is exactly what
  `traceEx` does at the maximum ray depth (`trace.cpp:283-287`), the bundle's own miss path.

**Finding: three DB-001 link lines go stale (Q1).** Once Shading's calls go through a virtual, no
Shading object references `CShadingContext::trace(CRay *)` or `traceEx` any more. Archcheck then
reports `allowlist.txt:15, 16, 21` as `STALE`, and they must be deleted (the allowlist only shrinks).
This contradicts FR-010, FR-020 and the Assumptions ("claims no DB-001 line", "DB-001 is not
edited"):

- after the change: 5 DB-001 lines and 4 imports (`CGatherBundle::CGatherBundle()`, the two bundle
  vtables, `CPhotonMap::lookup`);
- DB-005's 8 include lines do not change: the bundles are still constructed in Shading (029).

**Alternative**: keep the direct `traceEx` calls and add the tracer beside them. Rejected: it
contradicts FR-010 ("Shading's call sites go through it") and FR-011.

## R7. `getMessage`: the six shader-to-shader queries (FR-006)

**Decision**:

- **Request** `MessageRequest{source, object, light, name, expected, dest, wantVariable}`:
  - `source` is one of `Surface | Displacement | Atmosphere | Interior | Exterior | Light`;
  - `dest` is where a default value is written, exactly as `getParameter()` writes it today;
  - `wantVariable` mirrors today's non-null `var`/`globalIndex` arguments.
- **Answer** `MessageAnswer{status, variable, globalIndex}`.
- **Source per builtin** (today's mapping, kept): `surface` -> Surface, `displacement` ->
  Displacement, `atmosphere` -> Atmosphere, `incident` -> **Interior**, `opposite` -> **Exterior**
  (`shading.cpp:1637-1663`). The macros pass `ACCESSOR_EXTERIOR`/`ACCESSOR_INTERIOR` the other way
  round (`shaderFunctions.h:1331, 1341`; the JIT's `jitIncidentParameter`/`jitOppositeParameter` do the same at `shading.cpp:3846, 3854`), but `incident`/`opposite` never get a variable back
  (`wantVariable = false`), so the accessor is never read. **Fixed by F4 (FR-029, 2026-10-31)**: the constants agree with the source, interpreter and JIT, and no output changes.
- **The renderer's implementation** resolves the bound instance with `shaderInstanceOf(attributes->surface())`
  and the like (or the light handle) and calls `getParameter(name, dest, var, globalIndex)`. That is
  the call `surfaceParameter()` and its siblings make today. Shading keeps the
  `locals[accessor][var->entry]` / `varying[...]` selection and the varying-to-uniform guard
  (`PARAMETEREXPR_PRE`, `jitParameterFinish`).

**Finding: `lightsource()` differs between the backends and reaches no `CShadingContext` member (Q2).**

| | Interpreter (`LIGHTPARAMETEREXPR_PRE`, `shaderFunctions.h:1199-1230`) | JIT (`op_lightsource_f`, `rslOps.cpp:918-937`; emitted at `llvmEmitter.cpp:3113-3131`) |
|---|---|---|
| Light | `currentShadingState->currentLight->instance`, the light being iterated | `currentShadingState->currentLightInstance`, the last light shader that ran (`execute.cpp:415`, `shading.cpp:1971, 2016, 2243`) |
| Mutable values | yes: `savedState[2 + globalIndex]` | no (`var = nullptr`) |
| Types | F, V, S, M | float only |
| `result` | the found flag | the parameter's **value** (`IDX(result, sr, 0)[0] = value`) |

- **Decided (Q2, 2026-10-30)**: both backends go through `getMessage` with source `Light`, each keeping its own
  light, its `wantVariable` and its result handling. Each backend's output then stays as it is
  (FR-022).
- **Where the edits land**: a new member `lightParameter()`, used by the macro, and the body of
  `op_lightsource_f`. No emitter change and no `op_*` signature change.
- **What this does to the acceptance criteria**: SC-003's "identical requests from both backends"
  holds for the other seven builtins and is recorded as a divergence for `lightsource`, to be filed as
  an issue.
- **Outside an illuminance loop**: today's interpreter dereferences a null `currentLight`. `lightParameter()` returns 0 for a null light
  (F3, FR-028); the JIT's `op_lightsource_f` already guards it.

## R8. `getMatrix` (FR-008, FR-009)

**Finding**: every one of the 25 coordinate-system call sites (textual calls to `findCoordinateSystem(` or `jitFindCoordinateSystem(`, outside their declarations, definitions and comments; the spec's "about 30" counts mentions) goes through
`CShadingContext::findCoordinateSystem(name, from, to[, type])` (`shading.cpp:2843-2925`):

- 17 in `shaderFunctions.h`, 1 in `giFunctions.h`, 1 in `shaderOpcodes.h`, 3 in `shading.cpp` (`:3532, :3644, :3684`; the 3-argument overload `:2846` and `jitFindCoordinateSystem` `:3003` are two forwarders to the same member);
- 3 in `rslOps.cpp` through `jitFindCoordinateSystem` (`shading.cpp:3000`).

Changing that one body therefore covers both backends.

**Decision**:

- **Request** `MatrixRequest{name, object, shader, light, time}`.
- **Answer** `MatrixAnswer{status, kind, from, to}`:
  - `status` is `Found | NotFound | NoObject | Unavailable`;
  - `kind` is the `ECoordinateSystem`. The colour-space callers branch on it (`shaderFunctions.h:511`
    and the rest), so it is required;
  - `from`/`to` are pointers that stay valid until the end of the shading call.
- **The renderer's implementation** calls `CRenderer::findCoordinateSystem()` (unchanged), then
  applies today's switch:
  - object: `object->xform` (`NoObject` when there is no object);
  - shader and light: `instance->xform`, behind today's `assert`s;
  - camera and current: `identityMatrix`;
  - world, NDC, raster, screen, custom and the colour names: as returned.
- **Diagnostics stay in Shading**, with today's text and code:
  - `NoObject`: `error(CODE_SYSTEM, "Object system reference without an object\n")` and the
    identity matrix;
  - `NotFound`, `Unavailable` and an unknown kind: `warning(CODE_BUG, "Unknown coordinate system: %s\n")`
    and the identity matrix.
- **Time**: none of the call sites carries one today. Shading passes `0.0f`, the normalised
  shutter-open time `CRay::time` uses (`0 <= time <= 1`). The renderer ignores it, as FR-008 says.
  A double may honour it (Q14).

## R9. No services and "not implemented" (US1 AS3, edge cases)

**Decision**:

- **Defaults for the new members**: `getAttribute`, `getMessage`, `getMatrix` and `tracer()` are
  virtual with a documented default (`status = Unavailable`, `tracer() == nullptr`), not pure. A double
  that leaves one out therefore compiles, and the call degrades as it would with no services.
- **Warnings**: Shading warns once per kind per process for `Unavailable` and for a missing state
  services pointer, with the `serviceLookup.cpp` pattern (a `std::atomic<bool>` per kind,
  `CODE_NOTSTARTED`).
- **What the default yields**:
  - `getAttribute`/`getMessage`: not found, destination untouched;
  - `getMatrix`: the "Unknown coordinate system" path (identity);
  - the tracer: R6's miss path.
- **Existing members**: the 28 that stay keep their pure-virtual form, so the spec 024/027 rules are
  unchanged.

## R10. The standalone double and the load-time stub (FR-004, FR-024)

**Measurement (macOS, provisional)**: `nm -m` of `libshader_shading.1.0.0.dylib`. The binary was a
**read-only** look at the 037 worktree's build (`f239ebce`). That tree is behind master in
`src/libshader`: it lacks fix/111 and spec 028, neither of which touches these imports. Phase 1
measures this worktree's own build, and `dyld_info -fixups` as well.

```text
CPhotonMap::lookup(float*, float const*, int)        (dynamically looked up)
CGatherBundle::CGatherBundle()                       (dynamically looked up)
CShadingContext::trace(CRay*)                        (dynamically looked up)
CShadingContext::traceEx(CRayBundle*)                (dynamically looked up)
vtable for CTraceBundle                              (dynamically looked up)
vtable for CTransmissionBundle                       (dynamically looked up)
```

This matches DB-001's six. After R6 (Q1), four are left.

**How binding behaves**:

- **macOS**: the deployment target is 13.3, so the dylib uses chained fixups. dyld then binds every
  import at load, functions included. A program without `ribRender` therefore needs a definer for
  every remaining import just to start. Phase 1 confirms this with `dyld_info -fixups`.
- **Linux**:
  - data relocations (the vtables) bind at load;
  - functions bind lazily unless the library or the program is `BIND_NOW`; Phase 1 records
    `readelf -d` and `nm -D --undefined-only`;
  - linking the executable needs `--allow-shlib-undefined`, which `libshader_shading` already
    exports as an INTERFACE option (`src/libshader/shading/CMakeLists.txt:97`).

**Decision: the stub**, `tests/shading_contract/load_stub.cpp` (test directory, not a domain).
Symbol by symbol:

| Symbol | How the stub provides it | Body |
|---|---|---|
| `_ZTV12CTraceBundle` | defines `CTraceBundle::postTraceAction()`, `postShade(int, CRay **, float **)`, `postShade(int, CRay **)`, `post()`: `postTraceAction` is the key function, so the vtable is emitted there | each calls `stubTrap("CTraceBundle::...")`: message to stderr, then `std::abort()` |
| `_ZTV19CTransmissionBundle` | the same four members of `CTransmissionBundle` | the same |
| `_ZN13CGatherBundleC1Ev` | `CGatherBundle::CGatherBundle()`, `~CGatherBundle()` and its four virtuals (the ctor needs its vtable) | the same |
| `_ZN10CPhotonMap6lookupEPfPKfi` | `CPhotonMap::lookup(float *, const float *, int)` | the same |

Each trap aborts, so a test that reaches a stub fails and does not hang (FR-024). **Two more traps, `CShadingContext::trace(CRay *)` and
`traceEx(CRayBundle *)`, are needed until Phase 8** (QC, 2026-10-31): the starting tree still imports them, so the table is 6 symbols then 4. The stub includes
`bundles.h` and `photonMap.h` (Hiders headers, from a test directory, which is allowed) and compiles
into the test executable only. The stub is deleted by spec 029, which removes the imports.

Constructing `CTraceBundle`/`CTransmissionBundle` on the stack (`executeMisc.cpp:703, 843`) only
writes the vtable pointer, so `trace()` and `transmission()` can still issue their bundles to a
**recording** tracer double in the standalone program. Running their `postShade` would trap.

**What each test program covers** (Q6):

| Program | Links | Covers |
|---|---|---|
| `test_shading_contract_standalone` (SC-001) | `libshader_shading`, `openrendercommon`, `state_only_sources` (spec 025's State-only library), the stub, the doubles. **No `ribRender`** | `getAttribute` (attribute/option/rendererinfo), `getMessage` (5 sources + light), `getMatrix` (every kind), the no-services defaults of the three queries and of the probe forms, the requests of `trace()`'s and `transmission()`'s bundles and of every probe ray (both backends, recording tracer), canned answers for the single-ray probe forms (`visibility`, probe `transmission`) |
| `test_shading_contract_rendered` | adds `ribRender` (real bundles, irradiance cache, `CSurface`) | `gather()`'s requests (`gatherHeaderBegin` runs `new CGatherBundle`, `shading.cpp:4566`, which would trap on the stub); canned answers consumed by `trace`/`transmission`/`gather` (the double drives the bundle protocol); `occlusion`/`indirectdiffuse` rays reaching the tracer double (with Q6's routing); the equivalence of the real renderer's answers with master's; the no-tracer miss values of the bundle forms (their miss path runs `bundle->postShade()`, which traps on the stub) |

Compiling `bundles.cpp` into the standalone program was rejected: it would put a Hiders object on its
link line, against US1 AS1.

**Loading `.slo` without the renderer (Q5)**. `.rslo` loads through `parseShader()`, which is in
`libshader_shading` (`rslo.y:2109`). `.slo` loads through `parseSloShader()`, which is static in the
renderer core (`rendererFiles.cpp:170`). That function and its helpers (`rendererFiles.cpp:53-343`)
reference no `CRenderer::` member. Moving them verbatim into `libshader/shading/sloLoader.cpp` lets
the standalone program's JIT half load shaders. The core keeps calling the function from `getShader`,
so the move changes no behaviour and adds no edge (Core -> Shading). The alternative is for the JIT
half to link `ribRender` only to load, which contradicts SC-001 as written.

**The globals table of the double**: `newState()` needs the 29 built-in globals (`VARIABLE_P` ..
`VARIABLE_CONSTANTWIDTH`, `common/rendererc.h:147-175`).

- The double builds them with `libRIBCommon`'s `parseVariable` from the declarations
  `initDeclarations()` registers.
- `ShadingContract_DoubleGlobalsMatchRenderer` (rendered program) checks that the double's table
  equals the renderer's, entry by entry, so the copy cannot drift.

## R11. `ShaderGlobals` layout (FR-015, FR-016)

**Decision**:

- **Layout**: structure-of-arrays. Each per-point field is a `Strided<T>{const T *data; int stride}`:
  - `stride` is in elements of `T` between consecutive points (3 for a float triple, 1 for a float,
    0 for a value shared by every point);
  - `data == nullptr` means the field is **absent**.
- **Batch shape**: `numPoints` (real points) plus `numDerivativeTail` (0, or `2 * numPoints` for ray
  hits shaded with derivatives, `shading.cpp:1065-1071`) and `BatchShape{Grid, Span, Point}` with
  `gridU`/`gridV`.
- **Active lanes**: an optional `const int *activeTags` uses today's convention (0 = active,
  `tags[]`).
- **Ownership**: the structure owns nothing. Field lists, presence rules and the producer table are in
  [data-model.md](data-model.md).
- **Allocation is the producer's**: the structure only points, so the REYES grid's `varying[]` arrays
  map onto it with no copy. That is what 029 wants for `shade()`.

**Ray differentials**: today no producer has `dPdx`/`dPdy` vectors. A ray hit carries a scalar
footprint `da * t + db`, which `CSurface::shade` stores into `du` (`geometryDispatch.cpp:400`).

- The contract therefore separates three fields: `du`/`dv` (parametric step, grids), `rayWidth`
  (the scalar ray footprint, ray hits) and `dPdx`/`dPdy` (vector differentials, absent in every
  producer of 026, reserved for 049/054 as a MINOR fill).
- **Rationale**: OSL needs `dPdx`/`dPdy`, and REYES needs `du`/`dv` (D20, §5.5, §8.3). Overloading
  `du` as master does would make 029's consumer guess.

## R12. Footprint, path depth, transport mode

- **Shading-cell footprint**: `CAttributes::shadingRate` already stores √area
  (`rendererContext.cpp:2411`, `attributes->shadingRate = sqrtf(size)`). The footprint field is that
  value, **not** its square root again. Its unit is pixels, D20's "√ShadingRate pixels".
- **Path depth**: `CShadingContext::currentRayDepth` (0 for camera grids and camera rays, incremented
  by `traceEx`). It is distinct from `CRay::depth` (spec 039's key depth), which the per-point
  `sampleKey` carries.
- **Transport mode**: `Importance` for the photon pass (light shaders on the phony object with the
  illumination hook, `photon.cpp:166-230`); `Radiance` everywhere else.

## R13. The FR-017 situation table and its test

**Finding**: the table in [contracts/shader-globals.md](contracts/shader-globals.md#situation-table)
was built from every `shade()`/`displace()`/`displaceEstimate()` call site in `src/ri`. It found these
oddities, recorded, not fixed here, and filed as O2 to O6 (`validation.md` §4):

1. **Raytrace camera hits** (`raytracer.cpp:652` -> `trace(&primaryBundle)` -> `CSurface::shade`,
   `SHADING_2D`) obey `specularhitmode` (`shading.cpp:915`), not `camerahitmode`.
2. **`camerahitmode "p"`** applies only to `SHADING_2D_GRID`, and **after** the shaders have run: it
   overwrites `Oi` with the attribute opacity and keeps `Ci` (`shading.cpp:1374-1382`).
3. **Implicit-surface ray hits** are shaded as `SHADING_0D` (`implicitSurface.cpp:258`): no hit mode
   applies, and no derivatives.
4. **DSO-object ray hits** call `shade(this, numRays, -1, SHADING_2D, 0)` (`dlobject.cpp:176`), so
   `vVertices = -1`. Under `assert` that fails, and in Release `numVertices` is negative. Recorded as a
   probable defect, to be filed.
5. **`diffusehitmode`** is parsed, and a value other than `"p"` warns `CODE_UNIMPLEMENT`
   (`rendererContext.cpp:3701-3704`). It is never read, so gather hits use `specularhitmode`.
6. **The atmosphere** runs only at ray depth 0 (`shading.cpp:1361`). A matte object runs no
   atmosphere (`:933-936`).
7. **A shaded transmission ray** skips the displacement shader (`shading.cpp:907`).
8. **REYES points and curves** shaded as `SHADING_0D` (`reyes.cpp:907, 968`) get no `camerahitmode`.

**Decision**:

- `libshader::classifyShading(const ShadingSituation &)` encodes the table as a pure function. Its
  inputs are `dim`, `displaceOnly`, `inShadow`, `rayDepth`, the illumination hook, and the object's
  three hit modes.
- **Test** `ShadingContract_Situations` (rendered program, label `shading_contract`) renders one small
  scene per row and reads the situation inputs from the two existing spec-039 test observers
  (`pointKeyObserver`, `pointDisplacedObserver`). Their report structs gain the situation inputs as
  trailing fields: no new hook, and existing readers are unaffected.
- **What it checks**: `classifyShading()` returns the table's row, and the branch `shade()` took
  (primitive-only versus shaded) matches the hit mode.
- **The rows without `shade()`** (photon emission, imager, probe-only rays: rows 10-14) **and rows 6 and 7** (implicit-surface hits and DSO-object hits, whose present behaviour is an
  oddity, O5 #119 and O2 #116) are checked from the table by a unit assertion on `classifyShading()`, not rendered (B8).
- **SC-006's "every situation in the visual suite"**: every visual scene shades through these call
  sites with the hit-mode values Phase 1 greps from `tests/visual/**/*.rib`. The test's scene set
  covers every (call site, hit mode) pair that the grep finds, rows 6 and 7 excepted.

## R14. SC-005: the same rays as master

**Decision (Q7)**: a recorded replay, in the manner of spec 028's motion replay.

- **The observer**: Phase 1 adds a null-in-production observer, `CShadingContext::traceRequestObserver`,
  to the starting tree. It is called just before each of Shading's trace calls (the 10 sites of R6,
  plus the 2 in `irradianceDispatch.cpp`, Q6), with the ray, or the bundle's rays, its
  label and depth.
- **Moving it**: when the call sites move to `traceCalls.cpp`, the observer call moves into the two
  helpers. That is one place, and it observes the same thing: what is handed to the tracer.
- **Recording**: `ShadingContract_TraceReplayRecord` renders a fixed scene set in process, with one
  thread so the order is defined. It writes, per request: kind, label, bundle depth, path depth, then
  per ray `from`, `dir`, `t`, `tmin`, `time`, `flags`, `da`, `db`, `key`, `depth`, as hex floats.
  - files: `tests/shading_contract/replay/{macos,linux}.txt`;
  - `.rslo` and `.slo` variants of each scene.
- **Checking**: `ShadingContract_TraceReplay_<builtin>` compare a fresh run with the file, exactly.
- **Characterization**: these tests are green on the starting tree by construction, and Phase 1 shows
  each one red under a mutation (for example, the bias dropped in `traceReflection`). This is 028's
  Q3 wording.
- **A red replay** means a call site's rays changed. It is never re-recorded to make it pass.

**Alternative**: compare only the images. Rejected as insufficient: a ray whose change does not
reach an 8-bit pixel would pass.

## R15. Allowlist, registry and metrics

Lines that go (verified against `tests/architecture/allowlist.txt`):

- **DB-006 (2 include lines)**: `:88` `execute.cpp -> surface.h` and `:89` `executeMisc.cpp -> surface.h`.
  `:90` `shading.cpp -> object.h` stays (spec 029). The count goes from 33 to 31 (26 include -> 24, 7
  link).
- **DB-001 (3 link lines, Q1)**: `:15` `executeMisc.cpp -> trace(CRay*)`, `:16` `executeMisc.cpp ->
  traceEx`, `:21` `shading.cpp -> traceEx`. The count goes from 8 to 5.
- **DB-005**: unchanged (8).
- **DB-018**: it has no line; its evidence is edited (FR-020).
- **Platforms**: none of these lines carries a platform tag. Linux's mangling is the same Itanium ABI,
  and Phase 1's Linux archcheck baseline confirms it.

Metrics:

- **Shading**: it already reads 0 `CRenderer::` references (`metrics-baseline.txt`), and nothing in
  026 adds one in Shading.
- **Hiders**: the adapter references no `CRenderer::` member.
- **The core**: its new references are outside the measured classes. FR-021 is expected to need no
  baseline change; Phase 1 and the final phase re-measure.

## R16. Findings: triage (2026-10-31)

Rule (maintainer): a finding whose context matches or impacts this spec is resolved in spec 026; anything else is filed on GitHub as P1.
The classification, reasons and issue text are in `validation.md` §4; O1 to O7 are filed as #115 to #121; the decisions are in spec.md, Clarifications, "Session 2026-10-31".

**Resolved in this spec (tests first)**

1. **F1**: `attribute()`, `option()`, `rendererinfo()` and message passing never check the type (R4). Unsafe mismatches are refused (FR-026).
2. **F2**: a query with no current object (the imager, `imager.cpp:82`) dereferences a null pointer on master (R5). The renderer answers
   `NotFound` for a null `ObjectHandle` (Q12), and `shadername(type)` is null-guarded (FR-027).
3. **F3**: the interpreter's `lightsource()` outside a loop dereferences a null `currentLight` (R7). Guarded (FR-028).
4. **F4**: the accessor constants of `incident()`/`opposite()` are crossed (R7). Made consistent; no output changes (FR-029).

**Filed by the maintainer as P1 (out of scope)**

5. **O1**: JIT `lightsource()` reads the last light that ran, handles floats only, and writes the value where the found flag belongs (R7; Q2).
6. **O2**: `CDLObject::shade` passes `vVertices = -1` (R13.4).
7. **O3**: `camerahitmode` is ignored by raytrace camera hits and by 0-D points (R13.1, .2, .8).
8. **O4**: `diffusehitmode` is never read (R13.5).
9. **O5**: implicit-surface ray hits are shaded 0-D (R13.3).
10. **O6**: three behaviours to verify against the RISpec (R13.6, R13.7, and `incident`/`opposite` always reading interior/exterior).
11. **O7**: the destination size of a float-class query is unknown, the residual of F1 (R4).
