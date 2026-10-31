# Data Model: Shading Contract v2

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Spec**: [spec.md](spec.md) | **Research**: [research.md](research.md)

All types below are Shading-owned. They are internal C++ in namespace `openrender::shading`, except
for the existing `C`-prefixed classes. They name no Geometry, Hiders or renderer-core type (FR-003).
The exact declarations are in [contracts/](contracts/).

## Handles (`serviceTypes.h`)

| Type | Field | Meaning |
|---|---|---|
| `ObjectHandle` | `const void *ptr` | The shaded object, opaque. Made only by `CShadingContext::objectHandle()` (`shading.cpp`). Null when no object exists (the imager, a bare execution). Only an implementer casts it back. |
| `ShaderHandle` | `const void *ptr` | A shader instance (the executing shader, or a light), opaque. Made from a `CShaderInstance *`. |

Validity: a handle is valid for the duration of the shading call that made it. It is never stored by
an implementer.

## Query vocabulary (`serviceTypes.h`)

| Type | Values | Notes |
|---|---|---|
| `EQueryStatus` | `Found`, `NotFound`, `TypeMismatch`, `NoObject`, `Unavailable` | `NoObject` only for `getMatrix`. `Unavailable` means the implementer does not provide the service (the default body). |
| `QueryType` | `Float`, `Triple`, `Matrix`, `String` | What the shader expects. It comes from the interpreter's `DEFFUNC` overload (F, V, S, M) or the JIT's `resultKind` (1, 3, 16, -1). |
| `AttributeScope` | `Attribute`, `Option`, `RendererInfo` | Which name table `getAttribute` searches. |
| `MessageSource` | `Surface`, `Displacement`, `Atmosphere`, `Interior`, `Exterior`, `Light` | `incident()` maps to `Interior` and `opposite()` to `Exterior`, as master does (research R7). |

## Requests and answers

### `AttributeRequest` -> `AttributeValue` (FR-005)

| Request field | Type | |
|---|---|---|
| `scope` | `AttributeScope` | |
| `object` | `ObjectHandle` | ignored for `Option` and `RendererInfo` |
| `name` | `const char *` | the query name, `name[0]` of the call (one request per call, not per lane) |
| `expected` | `QueryType` | |

| Answer field | Type | |
|---|---|---|
| `status` | `EQueryStatus` | `Found`, `NotFound`, `TypeMismatch` or `Unavailable` |
| `type` | `EVariableType` (`common/variable.h`) | the stored type |
| `count` | `int` | floats to copy, or `char *` slots for a string |
| `data` | `const void *` | the value. It points at the renderer's own storage or at `inlineValue`. It stays valid until the end of the shading call. |
| `inlineValue` | `float[16]` | storage for computed answers (`Sides`, `Format`, `CropWindow`, ...) |

Ownership: the caller (Shading) owns the `AttributeValue` and passes it to
`getAttribute(request, answer)`. The type is not copyable, because `data` may point into its own
`inlineValue`.

Validation: when `status` is `Found`, `data` is non-null and `count >= 1`. `TypeMismatch` (FR-026) means the copy cannot be made safely:
`Triple` against more than 3 floats, `Matrix` against more than 16, `String` against a non-string, any other type against a string. Shading
refuses it like `NotFound` and warns once (contract, R4); `type` and `count` then describe the stored value for the diagnostic.

### `MessageRequest` -> `MessageAnswer` (FR-006)

| Request field | Type | |
|---|---|---|
| `source` | `MessageSource` | |
| `object` | `ObjectHandle` | for the five object sources |
| `light` | `ShaderHandle` | for `Light` |
| `name` | `const char *` | parameter name |
| `expected` | `QueryType` | |
| `dest` | `void *` | where a default value is written, in the parameter's layout, as `getParameter()` does today |
| `wantVariable` | `bool` | true where today's caller passes non-null `var`/`globalIndex` (`surface`, `displacement`, `atmosphere`, the interpreter's `lightsource`) |

| Answer field | Type | |
|---|---|---|
| `status` | `EQueryStatus` | |
| `variable` | `CVariable *` | set when a mutable or varying parameter must be read from the producer's storage; else null |
| `globalIndex` | `int` | the light's saved-state index (`savedState[2 + globalIndex]`), as today; -1 otherwise |

### `MatrixRequest` -> `MatrixAnswer` (FR-008)

| Request field | Type | |
|---|---|---|
| `name` | `const char *` | system or colour-space name |
| `object` | `ObjectHandle` | for `"object"` |
| `shader` | `ShaderHandle` | the executing instance, for `"shader"` |
| `light` | `ShaderHandle` | the executing light, for `"light"` |
| `time` | `float` | normalised shutter time. Shading passes 0 (no call site carries one); the renderer ignores it. |

| Answer field | Type | |
|---|---|---|
| `status` | `EQueryStatus` | `Found`, `NotFound`, `NoObject` or `Unavailable` |
| `kind` | `ECoordinateSystem` (`common/rendererc.h`) | needed by the colour-space callers |
| `from`, `to` | `const float *` | column-major 4x4, valid until the end of the shading call; never null when `Found` |

## `ITracer` (`tracer.h`, FR-010)

| Member | Request | Effect |
|---|---|---|
| `traceRay(CShadingContext &, CRay *)` | a `CRay` with `from`, `dir` (unit), `t`, `tmin`, `time`, `flags`, `da`, `db`, `key`, `depth` | Fills `object` (null on a miss), `u`, `v`, `N`, `t`. No shading. |
| `traceBundle(CShadingContext &, CRayBundle *)` | a bundle: `numRays`, `rays`, `label`, `depth`, `postShader`, with the virtual `postTraceAction`/`postShade`/`post` protocol | traces one level deeper and shades the hits, as `traceEx` does; honours the maximum ray depth |

Lifetime: one installed per frame (`tracer()` is non-null from `beginFrame()` to `endFrame()`).
Stateless; all per-thread state lives in the context argument.

## `ShaderGlobals` (`shaderGlobals.h`, FR-015 to FR-018)

### Vocabularies

| Enum | Values (appendable) | |
|---|---|---|
| `BatchShape` | `Grid`, `Span`, `Point` | `Grid` is `gridU` x `gridV` points, u fastest (`numUvertices`/`numVvertices`); `Span` is independent points (ray hits, `SHADING_0D`); `Point` is a batch of one |
| `RayType` | `None`, `Camera`, `Secondary`, `Shadow`, `Photon` | `None` covers a rasterised grid or point, a displacement pass, the imager and a bare execution |
| `HitMode` | `Shade`, `Primitive` | from `camerahitmode`, `specularhitmode`, `transmissionhitmode` (`'s'`/`'p'`) |
| `TransportMode` | `Radiance`, `Importance` | |
| `ShadingPass` | `Full`, `DisplaceOnly`, `DisplaceEstimate` | `shade()`'s `displaceOnly` values `FALSE`, `TRUE`, `3` |

### `Strided<T>`

`{ const T *data; int stride; }`. `stride` counts elements of `T` between consecutive points; 0 means
one value shared by every point. `data == nullptr` means the field is **absent**, and a consumer must
test `present()` before reading.

### Per-batch fields

| Field | Type | Today's source (for 029's producer) |
|---|---|---|
| `numPoints` | `int` | `numRealVertices` |
| `numDerivativeTail` | `int` | 0, or `2 * numPoints` for derivative-shaded ray hits (`shading.cpp:1065-1071`) |
| `shape`, `gridU`, `gridV` | `BatchShape`, `int`, `int` | `shadingDim`, `numUvertices`, `numVvertices` |
| `activeTags` | `const int *` | `tags` (0 = active); null = all active |
| `object` | `ObjectHandle` | `currentObject` |
| `shader` | `ShaderHandle` | the instance about to run |
| `rayType`, `hitMode`, `transport`, `pass` | enums | `classifyShading()` (situation table) |
| `pathDepth` | `int` | `currentRayDepth` |
| `shadingCellFootprint` | `float` | `CAttributes::shadingRate` (already √area, pixels; never square-rooted again) |
| `shutterOpen`, `dtime` | `float` | `complete()`'s `t0`, `shutterClose - shutterOpen` |

### Per-point fields and presence rules

| Field | `T`, stride | Grid (REYES) | Ray hit | Photon emission | Displacement pass | Imager |
|---|---|---|---|---|---|---|
| `P` | float, 3 | present | present | present (`Ps`) | present | present (pixel coords) |
| `N`, `Ng` | float, 3 | present | present | absent | present | absent |
| `I` | float, 3 | present | present | absent | absent | absent |
| `u`, `v` | float, 1 | present | present | absent | present | absent |
| `du`, `dv` (parametric step) | float, 1 | present | absent (master overloads `du` with the ray width; see `rayWidth`) | absent | present | absent |
| `rayWidth` (`da * t + db`) | float, 1 | absent | present | absent | absent | absent |
| `dPdu`, `dPdv` | float, 3 | present | present when derivatives are used | absent | present | absent |
| `dPdx`, `dPdy` | float, 3 | absent (reserved) | absent (reserved) | absent | absent | absent |
| `time` | float, 1 | present | present | present | present | present |
| `dPdtime` | float, 3 | present when moving | present when moving | absent | present when moving | absent |
| `sampleKey` | `SampleKey`, 1 | present | present | present | present | absent |

Rules:

- **Derivative tail** (FR-016): with `numDerivativeTail > 0`, points `[numPoints, numPoints +
  numDerivativeTail)` are the du-offset then dv-offset copies of the real points, as `shade()` lays
  them out (`shading.cpp:1065-1071`). They are not counted in `numPoints` and are never active lanes.
  Their `sampleKey` copies the real point's key (spec 039).
- **Inactive lanes**: a lane with `activeTags[i] != 0` holds unspecified values.
- **Absence**: a consumer may assume only `P`, `time` and the per-batch fields. Everything else must
  be tested with `present()`.
- **Additions**: a field appended at the end is MINOR (research R2). Removing a field or changing a
  presence rule is MAJOR.

### State transitions

None: `ShaderGlobals` is a value description with no lifecycle. In 026 only test doubles produce and
consume it (FR-018).

## `CShadingContext` members added or changed

| Member | Kind | |
|---|---|---|
| `ObjectHandle objectHandle() const` | new, out of line in `shading.cpp` | the one upcast of `currentObject` |
| `const CAttributes *objectAttributes() const` | new, out of line in `shading.cpp` | for the executor reads that stay (research R5) |
| `int lightParameter(void *dest, const char *name, CVariable **, int *)` | new (Q2) | the interpreter's `lightsource()` through `getMessage` |
| `surfaceParameter` ... `oppositeParameter`, `attributes`, `options`, `rendererInfo` | bodies changed, gain a `QueryType` argument | go through `getMessage`/`getAttribute` |
| `findCoordinateSystem(name, from, to, type)` | body changed | goes through `getMatrix` |
| `rendererSetOffendingObject(ObjectHandle)` | signature changed | |
| `static traceRequestObserver` | new test hook, null in production (Q7) | |
| `CPointKeyReport`, `CPointDisplacedReport` | trailing fields added (`dim`, `displaceOnly`, `inShadow`, `rayDepth`, `illuminationHook`) | for `ShadingContract_Situations` |
