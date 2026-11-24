# Contract: `closuremode`, the surface step, the interim rules and the lighting gate

- **Domains**:
  - Graphics State: storage and declaration;
  - Scene Interface and the renderer core: parsing;
  - Shading: resolution, the step, the gate and the diagnostics.
- **Requirements**: FR-017 to FR-022, FR-024, FR-026 and FR-027; research R8 to R12, R17 and R19.

## Statements

```text
Option    "shade" "string closuremode" ["color" | "both" | "closures"]
Attribute "shade" "string closuremode" ["color" | "both" | "closures"]
```

`"string"` may be omitted, because `closuremode` is pre-declared.

| Input | Result |
|---|---|
| a value outside the three spellings | `CODE_RANGE`: `Unknown closuremode: "<v>" (expected "color", "both" or "closures")`. The previous value is kept, on both the attribute side and the option side (R11, D-2) |
| an unknown token under `Option "shade"` | `CODE_BADTOKEN`, as `Option "shaderformat"` does |
| an unknown token under `Attribute "shade"` | the existing inline-declaration retry, then `CODE_BADTOKEN` (unchanged) |
| `AttributeBegin`/`AttributeEnd`, attribute copies | they carry the attribute value, as they carry `shaderFormat`. RISpec §4: every begin-end block except `TransformBegin` and `MotionBegin` saves the attributes |
| `ObjectBegin`/`ObjectInstance` | An instance takes the attributes current at `ObjectInstance`, mode and Bxdf binding included (RISpec §5.10; Q-R2). openRender's `CDelayedInstance` already does this, so no code changes; `ClosureMode_Scope` tests it |
| `ribOut` | writes both statements at their positions. The declared-type fallback is confirmed, and an explicit case is added only if needed (SC-008) |

## The four layers

| Layer | Site | Change |
|---|---|---|
| 1. Tokens | `riTokens.cpp`, `riVocabulary.h`, `ri.h` | `RI_CLOSUREMODE = "closuremode"`; `RI_SHADE` reused; `Common_VocabularyConsistency` covers it |
| 2. RIB parsing | `rendererContext.cpp`, `RiAttributeV` (`RI_SHADE` block) and `RiOptionV` (a new `RI_SHADE` branch) | parse, validate, store |
| 3. Storage and query | `CAttributes::closureMode`, `COptions::closureMode` (`EClosureMode`, `stateVocabulary.h`); `CAttributes::find`, the option query | default `Unset`; a query answers `"color"` for `Unset` |
| 4. Pre-declaration | `declarations.cpp` | `declare(RI_CLOSUREMODE, "string")` |

## Resolution (FR-017)

```text
effective(object) = attributes.closureMode != Unset ? attributes.closureMode
                  : frame.sceneMode       != Unset ? frame.sceneMode
                  : Color
```

- `frame` is the `ClosureFrame` that the core sets on every context in `beginFrame()`, next to `setSamplingConfig` (`renderer.cpp:854`). It is deleted in `endFrame()`.
- A context with no frame (a contract double, the imager) has `sceneMode == Unset`.
- The resolution runs once per batch, in Shading, in `planSurfaceStep()`. It is the same for every hider and both backends.
- `kRendererServicesVersion` stays 2.6.1.

## The surface step (FR-018, FR-021, FR-022)

`CShadingContext::shade()` keeps its code from `:1074` to `:1548` (sampling, derivatives, interpolation, keys, displacement) unchanged, with **one exception**: the plan call inserted between the shader selection (`:1194`) and the `usedParameters` line (`:1198`). The code from `:1594` (atmosphere, camera hit mode, unwinding) is also unchanged.

The **fast test is inline**: `attributes->closureMode == Unset && sceneMode == Unset && governingMaterial() != Bxdf`. When it holds, `shade()` runs the original code in place, with no out-of-line call. That is the plan's reading of FR-018's "no extra call is made".

Otherwise, the 035 edits are three:

1. `planSurfaceStep(attributes, surface, globals)`, inserted at `:1196`, after the shader selection and before the `usedParameters` line (`:1198`) and `groupPrepare` (`:1290`). It may replace `surface` with the IR-026 substitute. It ORs the geometry record's `PARAMETER_*` bits into `usedParameters` **only when `plan.buildTrees`**.
2. `runSurfaceStep(plan, ...)`, in place of the `if (surface != NULL) {...} else {...}` block (`:1553-1592`). With `plan.mode == Color` and no Bxdf governing, it runs exactly that block. Under IR-017, IR-026 and `both` with no consumer, it runs the same color-path code.
3. `deliverClosures(plan, ...)`, just before `memEnd(threadMemory)` (`:1617`), when `plan.buildTrees`.

The plan, row by row:

| Governing | Mode | Capable | Consumer | Shader runs as | Trees | Ci, Oi | Diagnostic |
|---|---|---|---|---|---|---|---|
| none (no surface, no Bxdf) | any | n/a | any | today's no-shader path | none | today's | none |
| Surface | `color` | any | any | `execute` (today) | none | today's | none |
| Surface | `both`/`closures` | no | any | `execute` (today) | none | today's | **IR-017**: per (shader name, mode) per frame |
| Surface | `both` | yes | none | `execute` (today) | none | today's | none (Q5b, approved) |
| Surface | `both` | yes | yes | `executeClosures`, gate open | one per real point | equal to `color`'s, byte for byte | none |
| Surface | `closures` | yes | none | `executeClosures`, gate **closed** | built, not delivered | `Ci = 0`, `Oi` = the shader's own (Q-R5, RISpec §12.1) | **IR-018**: once per frame |
| Surface | `closures` | yes | yes | `executeClosures`, gate **closed** | one per real point | `Ci = 0`, `Oi` = the shader's own (Q-R5) | none |
| Bxdf | ignored | n/a | none | the substitute surface (A′, R16: `defaultsurface` or the no-shader path), color path | none | the substitute's | **IR-026**: per Bxdf name per frame |
| Bxdf | ignored | n/a | yes | no shader; `table->build` per point | one per real point | `Ci = 0`, `Oi = Os` | none |

Notes on the table:

- **`Os`** (written only on the Bxdf-with-consumer row, where no shader runs) is the primitive's `Os` when it gives one (the `varying[VARIABLE_OS]` that `complete()` filled), otherwise the attribute's `surfaceOpacity`.
- **The shader's own `Oi`** on the `closures` rows: `runSurfaceStep` leaves `varying[VARIABLE_OI]` as `executeClosures` wrote it and zeroes only `Ci`.
- **Camera hit mode**: `camerahitmode "primitive"` still overrides the opacity afterwards (`:1606`).
- **"Real point"**: `numRealVertices`. The `SHADING_2D` derivative helpers of the raytrace path are not points (`:1319`).
- **Ray types**: every full `shade()` (`displaceOnly == FALSE`) applies the plan, whatever the ray type. The batch record carries `rayType` and `pathDepth`, so a consumer can filter (Q11, approved).
- **Out of scope**: the photon hider never calls `shade()`, so it is unaffected (US5 AS4). The volume step (`shadeVolumeSegments`), the imager, displacement and atmosphere are not `Surface` bindings, and the plan does not touch them (A9).

## Interim rules (owner: spec 042)

| ID | Function (`closureInterim.cpp`) | Replaces | Test |
|---|---|---|---|
| IR-017 | `interimSurfaceNotClosureCapable()` | spec 036, for capable shaders; spec 042 (D6) for a `Surface` under a non-classic integrator | `Closure_Interim_IR017_*` |
| IR-018 | `interimClosuresWithoutConsumer()` | spec 042, when its consumer is installed | `Closure_Interim_IR018_*` |
| IR-026 | `interimBxdfWithoutConsumer()` and `interimSurfaceFor()` (R16, A′) | spec 042's "Bxdf under `Integrator classic`" | `Closure_Interim_IR026_*` |

- **Precedence per object** (R17): IR-026, then IR-017, then IR-018. At most one rule fires per object. A frame may report all three, for different objects.
- **Dedup**: the keys live in the `ClosureFrame`'s shared set (mutex), so "once per frame" holds across the render threads. With no frame set, the dedup is once per process.

## The lighting gate (FR-019)

`CShadingState::lightingSuppressed` is set by `runSurfaceStep` only around `executeClosures` under `closures`, and cleared after it. It is read at four funnels:

| Funnel | Builtins | Behaviour when set |
|---|---|---|
| `CShadingContext::iterateLights` (the `saveCat` overload; the other one forwards) | `illuminance` (both backends, after #200 too), `diffuse`, `specular`, `phong`, `lightsource()` in a loop, every classic and bound area light (`iterateBoundAreaLight` is reached only from here) | returns before touching `lights`, `lightsExecuted` or the lighting cache; no light shader runs |
| `CShadingContext::prepareAmbient` | `ambient()` | returns; `alights` stays empty; `ambient()` gives 0 |
| `libshader::irradianceLookup`, and the point-based branches of `IDEXPR` and `jitOcclusion`/`jitIndirectDiffuse` | `occlusion()`, `indirectdiffuse()` | writes 0; no ray, no cache read or insert, no warning (Q10, approved; RISpec 3.2 defines neither builtin) |
| `CShadingContext::gatherHeaderBegin` (shared by `GATHERHEADEREXPR_PRE` and `jitGatherHeaderBegin`) | `gather()` | the sample count becomes 0: the body and the `else` run zero times, which is exactly `gather(..., 0)` |

- **Message passing while a Bxdf governs** (analyze E12): `surface("x", v)` called from a displacement or atmosphere shader reads the Surface shader in the slot, which is the shader attached to the primitive under RISpec §15.8, even though that Surface does not run (the Bxdf governs, IR-026 or a consumer). `Closure_Bxdf_MessagePassing` pins it.
- **Untouched**: `trace()`, `transmission()`, `visibility()`, `environment()`, `texture()` and `shadow()` called from the surface. A ray traced from a `closures` surface shades its hit in a new state, with the hit object's own mode.
- **Area lights**: the light's emitter registry, its sampler and its keyed draws are unchanged. A gated receiver draws no emitter sample (FR-024, A12).
- **Discrepancy D-1** (resolved in spec.md FR-019, 2026-11-23): `indirectspecular` has no builtin in this tree and FR-019 no longer names it, so there is nothing to gate.

## `-P:N` (R12)

- Servers parse the RIB and render exactly as a local run does. No consumer is installed in 035, so the same rules apply, and `-P:N` images equal local images in every mode.
- `RiBxdfV` returns early on the client, as `RiSurfaceV` does.
- Trees never cross the wire.
- This replaces A13's default (Q4, approved 2026-11-23). RISpec 3.2 does not define network rendering.
