# Research: Closure Tree and BSDF Interface (spec 035)

All findings were made by reading the source on 2026-11-23, in the worktree at `b9dec4e5` and, where noted, in the bug-fix worktrees. Nothing was built or rendered.
Each entry gives the decision, the reason and the alternatives. An entry marked **Approved (Qn)** was an open item the maintainer decided on 2026-11-23; plan.md, "Decisions approved by the maintainer", lists them all.

Decision IDs refer to `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §0.

**Status (2026-11-23)**: every **Approved (Qn)** item below was approved by the maintainer as recommended. The answers are in spec.md, "Plan-stage answers".

A RISpec 3.2 review followed (plan.md, "RISpec 3.2 conformance"). It found five conflicts. The maintainer approved the RISpec-conforming answer for each on 2026-11-23, and the entries below are written with them:

| Answer | Entry | RISpec | Decision |
|---|---|---|---|
| Q-R1 | R16 | §4.2.4 | the default surface is A′ |
| Q-R2 | R11 | §5.10 | instancing takes the attributes at `ObjectInstance` |
| Q-R3 | R21 | Table 4.8, Appendix A | the standard shaders' parameters, `Cs`/`Os` as point inputs, `alpha = roughness` |
| Q-R4 | R20 | §5.10 | `RiBxdf` is refused in an Object block |
| Q-R5 | R17, R19 | §12.1 | the shader's own `Oi` under `closures` |

One decision was **added**, with no conflict: Bxdf `color` parameters are converted from `ColorSamples` with `COptions::convertColor`, as `RiColor` is (§4.1.3).

---

## R1. Where the closure model lives, and in which library

- **Decision**: a new directory `src/libshader/closures/`, compiled into `libRSLRuntime`. It holds:
  - the tree, the registry, the BRDF math library, the BSDF service;
  - the Bxdf table, the Bxdf registry, the built-in Bxdfs;
  - the consumer interface, the mode vocabulary and the interim rules.
  - `Spectrum` is header-only in `src/common/spectrum.h` (Infra).
- **Rationale**:
  - D30 (2) puts the closure registry and the BRDF math library in Shading, with no amendment.
  - The domain map already classifies `src/libshader/` as Shading (`tests/architecture/domain-map.txt:63`), and `src/common/` as Infra (`:18`). So no data-file edit is needed.
  - `libRSLRuntime` is the shared, self-contained shading engine (gotcha 28). Putting the closure code there gives the renderer core, a later Transport integrator (spec 042) and the unit tests one copy of the registry. Gotcha 22 is the reason one copy matters for any process-wide state.
  - A header-only `Spectrum` adds no link edge, and Shading, Transport and Hiders may all include Infra (FR-025).
- **Alternatives**:
  - Putting the files in `src/libshader/shading/`. Rejected: that directory is already 60 files, and the closure model is a separable unit that 036 and 049 will read on its own.
  - A new static library linked into `libRSLRuntime`. Rejected: a static archive linked into a shared library and also into a test would give a second copy of the registry (gotcha 22).
  - `Spectrum` in Shading. Rejected: spec 041's light service and Transport will carry colours too, and FR-025 asks for Infra.

## R2. Closure node layout (FR-001, D17)

- **Decision**: three POD structs in a strict C17/C++20 header, `closureAbi.h`:
  - `OrClosureNode { int32_t id; }`, where `id >= 0` is a component, `-1` a multiply and `-2` an add.
  - `OrClosureComponent { int32_t id; float w[3]; }`, 16-byte aligned. The parameter block starts at `(const char *)(c + 1)`.
  - `OrClosureMul { int32_t id; float weight[3]; const OrClosureNode *closure; }`.
  - `OrClosureAdd { int32_t id; const OrClosureNode *closureA; const OrClosureNode *closureB; }`.
  - The expected LP64 values are pinned by `static_assert`s and by a run-time test that prints them:

    | Struct | sizeof | Field offsets |
    |---|---|---|
    | `OrClosureComponent` | 16 | `id` 0, `w` 4, alignment 16 |
    | `OrClosureMul` | 24 | `weight` 4, `closure` 16 |
    | `OrClosureAdd` | 24 | `closureA` 8, `closureB` 16 |

    `contracts/closure-abi.md` derives them from reference doc §2.4 and the C layout rules.
  - The parameter blocks are renderer-defined, one struct per registered closure. In OSL too the renderer registers a closure's parameters with their offsets; `oslclosure.h` fixes only the node layout. Each block's offsets and size are part of the registry entry.
- **Rationale**:
  - D17 says to mirror `oslclosure.h` without including it. Spec 049's layout test needs fixed numbers to compare, and FR-001 asks for compile-time and run-time checks.
  - A C17 header follows the `hiderAbi.h` precedent: the registration headers of spec 030 are strict C17.
- **Alternatives**:
  - C++ structs with inheritance, as in reference §2.4's sketch. Rejected for the ABI header: a C boundary is D7's rule. A C++ view (`closureTree.h`) wraps the C structs without changing their layout.
  - A string type of our own for the `microfacet` distribution. Rejected: a `const char *` pointing at an interned string matches what OSL's `ustring` holds (one pointer).
- **Risk**: R-2 in plan.md. The numbers are derived, not measured against `oslclosure.h`. Spec 049 does that comparison; 035 only pins its own.

## R3. The registry (FR-004, D14, reference §2.3 and §2.5)

- **Decision**: a process-wide registry in `libRSLRuntime`. One entry holds:
  - an id, a name, a category (BSDF, EDF, VDF or signal);
  - a typed parameter list (name, type, offset);
  - the block size and alignment;
  - for a BSDF, the component operations (`evaluate`, `sample`, `pdf`, and `albedo` for lobe selection).
- **Built-ins**: the registry fills itself with them on first use, through the call graph. That is the `hiderRegistry()` → `registerBuiltinHiders()` pattern, so a static link needs no `-u` anchor (gotcha 38).
- **Fixed ids, in this order**:

  | Id | Name | Category |
  |---|---|---|
  | 0 | `diffuse` | BSDF |
  | 1 | `oren_nayar` | BSDF |
  | 2 | `microfacet` | BSDF |
  | 3 | `reflection` | BSDF |
  | 4 | `refraction` | BSDF |
  | 5 | `transparent` | signal |
  | 6 | `holdout` | signal |
  | 7 | `uniform_edf` | EDF |

- **Later registrations**: they take the next ids; `registerClosure()` refuses a duplicate name.
- **`emission`**: an alias that resolves to `uniform_edf`'s id and reports a deprecation once per process.
- **Unregistered names**: a lookup of `layer`, `dielectric_bsdf` and similar is refused with a diagnostic naming it.
- **Parameter lists**, taken from `stdosl.h` as reference §2.3 quotes it:

  | Closure | Parameters |
  |---|---|
  | `diffuse` | `N` |
  | `oren_nayar` | `N`, `sigma` |
  | `microfacet` | `distribution` (string `"ggx"` or `"beckmann"`), `N`, `alpha`, `eta`, `refract` (int) |
  | `reflection` | `N`, `eta` |
  | `refraction` | `N`, `eta` |
  | `transparent` | none |
  | `holdout` | none |
  | `uniform_edf` | `emittance` (color) |

- **`refract` values**: 0 means reflection only, 1 refraction only, 2 both. The value 2 is recorded as **to verify against `stdosl.h` at implement**. Spec 049 compares the two.
- **Rationale**: D14 says to use OSL's names. Reference §2.5 says the OSL backend registers into the same registry. Fixed ids for the built-ins make trees hashable and comparable across runs (FR-028).
- **Alternatives**:
  - Ids assigned per run in registration order. Rejected: they would be unstable as soon as a test registers a closure.
  - A factory function per entry, as in reference §2.5's sketch. Not needed: a component is plain data, and its operations are stateless functions of the block.

## R4. Arena, lifetime and budget (FR-002, edge case "tree larger than the arena budget")

- **Decision**: trees live in the batch's `threadMemory` page, the arena that `shade()` checkpoints with `memBegin(threadMemory)` (`shading.cpp:1522`) and releases with `memEnd(threadMemory)` (`:1617`).
  - The consumer is called before that `memEnd`. A tree is therefore valid for the duration of the call, and a consumer that keeps one copies it (`closureTreeCopy`).
  - The builder counts bytes against a per-batch cap of `numRealVertices * kClosureTreeBytesPerPoint`, with the constant at 1024 bytes. A point whose tree would pass the cap gets an empty tree, and the frame reports one `CODE_LIMIT` error.
- **Rationale**:
  - `CMemPage` pages grow on demand, so without a cap the edge case could never happen and could never be tested.
  - A per-point budget makes the cap independent of the grid size.
  - 1024 bytes holds about 16 components with the largest built-in block (`microfacet`: 48 bytes plus the 16-byte header).
- **Approved (Q12)**: 1024 bytes per point.
- **Alternatives**:
  - A separate per-thread arena. Rejected: it would need its own checkpointing, and `threadMemory` already has exactly the batch's lifetime.
  - No cap. Rejected: the spec lists the edge case.

## R5. Flattening and the generic evaluator (FR-003, SC-005)

- **Decision**:
  - `closureFlatten()` walks the tree depth first, A before B, with an accumulated weight. A multiply node multiplies the weight. An add node visits both children. A component becomes a lobe `{id, weight, params}`.
  - A component whose accumulated weight is exactly zero in all three channels is dropped. Both paths skip it, so a NaN or Inf from its evaluation cannot leak through `0 * x` ("contributes exactly nothing").
  - The generic evaluator recurses over the same nodes and sums in the same order. For every tree of the first registry, the two paths are therefore bit-identical. The 1e-6 relative bound of SC-005 is kept for future non-additive closures.
  - The node order a builder writes is the order the shader or Bxdf emitted, and flattening preserves it. That is the "stable node order" A6 promises to spec 042.
- **Alternatives**:
  - A lobe list only. Rejected: D17 asks for the generic path for non-additive trees (`layer`, later).

## R6. The BSDF interface (FR-007 to FR-011, reference §2.6 and §7.4)

- **Decision**: a C function table `OrBsdfTable` in `closureAbi.h`, versioned with the closure ABI (see R14), with a C++ wrapper `bsdf.hpp`.
  - `prepare(tree, shadingFrame, allocator) → OrBsdf *` flattens the tree once and computes the lobe-selection probabilities. The result is an opaque handle allocated in the caller's arena.
  - `evaluate(bsdf, query, OrBsdfEval *)`, `sample(bsdf, query, u1, u2, uLobe, OrBsdfSample *)` and `pdf(bsdf, query, OrBsdfPdf *)` follow §2.6:
    - `wo` and `wi` point away from the surface;
    - `f` is returned without the cosine;
    - densities are per steradian;
    - every answer carries both the forward and the reverse density, and the flags.
  - A query carries a transport mode (`RADIANCE` or `IMPORTANCE`), a mask of requested lobe kinds and the geometric normal's side. The interface reports the side but does not reject light leaks.
  - **Lobe selection**: the probability of a lobe is proportional to its weight's channel mean times the component's `albedo(wo)` estimate. The probabilities are renormalised over the requested lobes. A non-delta `pdf` is the mixture `sum_i p_i pdf_i(wi)`.
  - **Delta lobes**: when `sample` chooses a delta lobe, it returns `f` as the Fresnel-weighted delta value divided by nothing. `pdfForward` then holds the discrete choice probability `p_i`, with `OR_BSDF_DELTA` set and documented as a probability, not a density. `evaluate` and `pdf` give a delta lobe nothing and set `OR_BSDF_HAS_DELTA` in the answer. This is the edge case "delta lobes in a mixed tree".
  - **Failed samples**: a direction below the horizon of the shading frame, or a sample that fails, returns `f = 0`, densities 0 and `OR_BSDF_FAILED`. Every output is finite.
  - **Transport mode**: refraction in `RADIANCE` mode scales `f` by `(eta_i/eta_t)^2`; `IMPORTANCE` does not (Veach 1997 §5.2). A reflection-only lobe gives the same answer in both modes.
  - **EDF and signal components**: `evaluate` and `sample` answer zero with `OR_BSDF_NOT_SCATTERER`. The tree exposes presence, emission and holdout through accessors (`closureTreePresence`, `closureTreeEmission`, `closureTreeHoldout`) (FR-006, D16).
  - **Determinism**: the table keeps no state and draws nothing. Its three numbers come from the caller (FR-008).
- **Rationale**:
  - Reference §2.6 is ADOPT. A `prepare` step is "construct once, evaluate many" (§2.4): an integrator evaluates one point's BSDF against many light samples.
  - §7.4 asks for both transport directions and both densities from day one.
- **Alternatives**:
  - Evaluating the tree directly on every call, with no `prepare`. Rejected: it flattens again for every light sample.
  - pbrt's "uniform lobe choice". Rejected: the variance is worse, and the albedo-weighted choice is as simple.
  - A single density per answer. Rejected by §2.6, FR-007 and the BDPT checklist.

## R7. The BRDF math library (FR-012, D19)

- **Decision**: `bsdfMath.{h,cpp}` holds the shared math:
  - orthonormal frames (Duff et al. 2017, "Building an Orthonormal Basis, Revisited");
  - GGX and Beckmann `D`, Smith `G1` and the height-correlated `G2` (Heitz 2014, "Understanding the Masking-Shadowing Function");
  - visible-normal sampling: GGX by Heitz 2018 ("Sampling the GGX Distribution of Visible Normals"), Beckmann by Heitz and d'Eon 2014;
  - the exact dielectric Fresnel term, and Schlick;
  - microfacet refraction (Walter et al. 2007, "Microfacet Models for Refraction through Rough Surfaces");
  - cosine-hemisphere sampling (Malley's method);
  - Oren-Nayar, the qualitative model of Oren and Nayar 1994, as OSL's `oren_nayar` uses it;
  - the balance and power MIS heuristics (Veach 1997).
- **Licence**: all of it is written from the published papers. No outside code is adapted, so there is no licence to check. If an implementer adapts code, the task records the source and its LGPL-2.1 check.
- **No rerouting**: classic `diffuse()`, `specular()` and `phong()` are not touched (`specularTerm.h` and the `*EXPR` macros stay as they are, FR-012).
- **Alpha floor**: below `kMicrofacetDeltaAlpha` (1e-4), a `microfacet` lobe behaves as the delta `reflection`/`refraction` it converges to. This is the edge case "roughness at or near zero".
- **Alternatives**:
  - pbrt-v4's code (Apache-2.0). Rejected: it would need a licence analysis, and the papers are enough.
  - Beckmann sampling of `D * cos` only. Acceptable fallback if visible-normal sampling for Beckmann proves fragile at implement: it changes no interface, only variance.

## R8. How the surface step changes, and where (FR-017 to FR-019, FR-021 to FR-023)

- **Decision**: `CShadingContext::shade()` keeps its sampling, derivative, interpolation, key and displacement code untouched (`shading.cpp:1074-1548`).
  - The 035 edit is confined to two places:
    1. The `if (surface != NULL) ... else ...` block at `:1553-1592`. It becomes one call, `libshader::closures::runSurfaceStep(*this, plan, ...)`.
    2. One call, `deliverClosures(...)`, just before `memEnd(threadMemory)` at `:1617`.
  - The plan is computed once per batch, after the shader selection at `:1159-1194`, by `planSurfaceStep()`. It returns:
    - the governing material (Surface or Bxdf);
    - the effective mode;
    - whether the surface instance can emit closures;
    - whether a consumer is installed;
    - which interim rule applies, if any;
    - the substitute surface instance for FR-026.
  - When the plan replaces the surface (FR-026), it does so before `groupPrepare` (`:1290`), so the locals belong to the substitute.
  - **The `color` path is literally today's code.** The fast test is inline in `shade()` and reads three bytes:

    ```cpp
    attributes->closureMode == Unset && sceneMode == Unset && governingMaterial() != Bxdf
    ```

    When it holds, `shade()` runs the original two branches in place, and calls neither `planSurfaceStep` nor `runSurfaceStep`. No tree is built, the arena is not touched and no consumer is called. This is how the plan reads FR-018's "no extra call is made": no out-of-line call, no tree, no consumer. FR-027 holds by construction, and `SC-001` checks it.
    - An explicit `Attribute "shade" "closuremode" "color"` takes the out-of-line plan once per batch, and the plan then runs the same two branches. The output is identical.
- **Rationale**:
  - Spec 054 changes the sampling side of `shade()` and `shadingProducer.cpp`. Keeping 035's edit to the post-displacement surface block and the hand-off keeps the two diffs apart. plan.md lists the boundary.
  - FR-022 asks for one implementation in Shading. The plan is computed in Shading from the batch's attributes, so every hider inherits it.
- **Alternatives**:
  - Handling the mode in the hiders' producer. Rejected: that is Principle VIII rule 3, an effect implemented per hider.
  - Rewriting `shade()` around a material object. Rejected for 035: it causes the largest possible conflict with 054.

## R9. The lighting gate (FR-019)

- **Decision**: a flag on the shading state, `CShadingState::lightingSuppressed`. `runSurfaceStep` sets it only around the execution of a closure-capable Surface under `closures`, and clears it afterwards. The flag is per state, so a `trace()` from that shader shades its hit in a new state, with the hit object's own mode. The flag is read at four funnels that both backends already share:

  | Funnel | Covers | Behaviour when suppressed |
  |---|---|---|
  | `CShadingContext::iterateLights` (the `saveCat` overload, `shading.cpp:2053`; the other overload forwards to it) | `illuminance` (interpreter `runLights`/`runCategoryLights`, `execute.cpp:446-468`; JIT `jitIlluminanceBegin`, `:3006`; the #200 fix keeps this call), `diffuse`/`specular`/`phong` (`callDiffuse`/`callSpecular`/`callPhong`, `:2335-2451`), `lightsource()` inside a loop, and the bound area light (`iterateBoundAreaLight` is reached only from here, `:2116`) | returns before any state change: `lights`, `lightsExecuted` and the lighting cache stay as `shade()` reset them (`:1527-1536`), so no later batch can trust a "lit" state that never ran |
  | `CShadingContext::prepareAmbient` (`:2508`; its light loop calls `iterateLights`, `:2553`) | `ambient()` | returns at once: `alights` stays empty, so `ambient()` gives 0 |
  | `libshader::irradianceLookup` (`irradianceCalls.cpp:42`), plus the point-based branch of `IDEXPR` (`giFunctions.h:236`) and of `jitOcclusion`/`jitIndirectDiffuse` | `occlusion()`, `indirectdiffuse()`, interpreter and JIT, cache on and off | writes 0, traces nothing, inserts nothing, warns nothing |
  | `CShadingContext::gatherHeaderBegin` (`:4907`, shared by `GATHERHEADEREXPR_PRE` and `jitGatherHeaderBegin`) | `gather()` | sets the bundle's sample count to 0, so the body and the `else` branch run zero times. This is exactly `gather(..., 0)` today, in both backends, by construction |

  - **Not gated**, because FR-019 keeps them:
    - `trace()`, `transmission()`, `visibility()`, `environment()`, `texture()`, `shadow()` called from the surface;
    - the volume step, displacement and atmosphere.
  - The light shaders themselves never run, so their `illuminate`/`solar` and their `shadow()` calls never happen either.
- **Rationale**:
  - All light work converges on these funnels. I read every caller of `iterateLights` and `prepareAmbient` (`grep` over `src/libshader/shading/`).
  - The #200 diff (`fix-200-slo-illuminance-category`) changes `jitIlluminanceBegin` to pass a category and still calls `iterateLights`.
  - #197 (`jitIlluminateEnd`) is inside a light shader, which never runs under the gate.
  - The design does not depend on the bug-fix session's code, only on the funnels.
- **Discrepancy D-1**: FR-019 lists `indirectspecular`. No such builtin exists in this tree: `grep -rn indirectspecular src/libshader` finds nothing. It is reserved in D15 for RSL 2.0. Nothing to gate. **Resolved** (maintainer, 2026-11-23): spec.md FR-019 no longer names it and says why.
- **Approved (Q10)**: under the gate, `occlusion()` and `indirectdiffuse()` write 0. RISpec 3.2 does not define either builtin.
- **Alternatives**:
  - Gating in the compiler or the emitter. Rejected: FR-019 says the context skips them, so a backend cannot run them by accident.
  - Gating each builtin separately, which would mean 9 interpreter macros and 9 JIT entries. Rejected: four funnels cover them all.

## R10. The scene mode reaches Shading without a services change (FR-017, FR-025)

- **Decision**: a Shading-owned per-frame object, `openrender::shading::ClosureFrame` (`closureFrame.h`). It holds:
  - the scene's mode, from `Option "shade"`;
  - the frame's shared, mutex-protected "once per frame" diagnostic set.
  - **Lifecycle**: the core creates one in `beginFrame()` and sets it on every context, in the loop that already calls `setSamplingConfig` (`renderer.cpp:854`), through `CShadingContext::setClosureFrame(const ClosureFrame *)`. It deletes the object in `endFrame()`, after the render threads are joined.
  - **No frame installed** (contract doubles, the imager's context): a context answers the scene mode as unset (`color`), and dedups diagnostics once per process.
- **Rationale**:
  - FR-025 aims for no services change.
  - `frameParams()` cannot be read from `libRSLRuntime`: it is defined in `src/ri/state/frameParams.cpp` (`libRIBRender`), and the shading library links with undefined symbols as an error (gotcha 28).
  - The core's per-context setter is an existing pattern: `setSamplingConfig` is called by the core and read by Shading. It needs no hider edit and no services step.
  - The contexts are per thread. "One warning per frame" therefore needs a set shared across threads, and the per-frame object is that set.
- **Alternatives**:
  - A services member `closureMode()` with a default body, which would step `kRendererServicesVersion` to 2.7.0 as a non-breaking MINOR. It is simple, but it is a services change the spec wants to avoid, and it does not give the per-frame dedup set.
  - A field in `FrameParams`, installed on the context by the `CHider` constructor. It touches Graphics State, the core and Hiders for the same effect.
- **Approved (Q3)**: the per-frame object, not the services member.

## R11. `closuremode` storage and the four layers (FR-020, the attributes-system section of CLAUDE.md)

- **Decision**:
  1. **Tokens**: a new `RI_CLOSUREMODE = "closuremode"` in `riTokens.cpp`, `riVocabulary.h` and `ri.h`. Gotcha 22 says all three, and `Common_VocabularyConsistency` checks them. `RI_SHADE` already exists (`riTokens.cpp:196`) and serves both `Attribute "shade"` and the new `Option "shade"`.
  2. **RIB parsing**:
     - A new token branch in `RiAttributeV`'s `RI_SHADE` block (`rendererContext.cpp:3923-3950`).
     - A new `RI_SHADE` branch in `RiOptionV`, modelled on the `RI_SHADERFORMAT` branch (`:2106-2123`). An unknown token under `Option "shade"` gives `CODE_BADTOKEN`, as `shaderformat`'s does.
     - The values `"color"`, `"both"` and `"closures"` are compared as literals, as `"slo"`/`"rslo"` are.
  3. **Storage and query**:
     - `CAttributes::closureMode` and `COptions::closureMode`, of type `EClosureMode : uint8_t { Unset, Color, Both, Closures }`, defined in `src/ri/state/stateVocabulary.h` next to `EAreaLightMode`.
     - `Unset` is the default of both. The attribute value is copied by the copy constructor and by `restore(...)` in its shading group, at the same sites as `shaderFormat` (`attributes.cpp:265`, `:503`).
     - It is answered by `CAttributes::find()` (`:721`, the `shaderformat` query), so `attribute("shade:closuremode")` returns the string, with `"color"` when unset, and by the option query.
     - Shading mirrors the enum as `openrender::shading::ClosureMode` (`closureMode.h`), with a mirror test, `State_ClosureModeVocabularyMirror`. That is the `EAreaLightMode`/`AreaLightMode` precedent (`State_AreaLightVocabularyMirror`): the Shading-side headers a Transport integrator includes then need no Graphics State header.
  4. **Pre-declaration**: `declare(RI_CLOSUREMODE, "string")` in `declarations.cpp`, next to `RI_SHADERFORMAT` (`:189`). Declarations are by token name only, so one line serves the attribute and the option.
  - **`ribOut`**: `CRibOut::RiOptionV` and `RiAttributeV` (`ribOut.cpp:342`, `:778`) special-case some names and fall back to the declared-type writer for the rest. The implement task confirms that `Option "shade"` and `Attribute "shade" "closuremode"` round-trip, and adds an explicit case only if the fallback does not cover them (SC-008).
  - **Instancing** (Q-R2, approved 2026-11-23): an instance takes the `closuremode` and the Bxdf binding current at `ObjectInstance`, as it takes the `Surface` binding (RISpec §5.10). openRender's `CDelayedInstance::instantiate` already builds the children with the instance's attributes, so no instancing code changes. `ClosureMode_Scope` tests instance-time inheritance.
- **Discrepancy D-2**: US3 AS5 and FR-020 say "the previous setting is kept (the existing `shaderformat` pattern)". The two sides of `shaderformat` disagree:
  - The **option** side does that: `CODE_RANGE`, keep the old value (`rendererContext.cpp:2109-2119`).
  - The **attribute** side does not: `CAttributes::setShaderFormat` (`attributes.cpp:911-919`) warns `CODE_BADTOKEN` and falls back to `"slo"`, leaking the old string.
  - `closuremode` follows the option side on **both** sides, which is what the spec's words describe. `shaderformat` is not changed (FR-027).
- **Approved (Q8)**. This also conforms to RISpec §7.2: the renderer ignores the erroneous information and continues.

## R12. The `-P:N` network path (assumption A13)

- **Finding**: the client never shades.
  - `netSetup` sends each server the RIB file name (`rendererNetwork.cpp`, `send(control, ribFile, ...)` around `:612` and `:678`). Each server parses the RIB itself and renders the buckets it is given.
  - The client returns early from every shader statement (`RiSurfaceV`, `RiLightSourceV`, `RiAtmosphereV` and the rest: `if (CRenderer::netNumServers > 0) return`, `rendererContext.cpp:2445`, `:2545`, ...).
  - A server therefore reads the same `Option "shade"`, `Attribute "shade"` and `Bxdf` statements, and runs the same Shading code.
- **Decision**:
  - `RiBxdfV` takes the same early return on a client.
  - A server applies exactly the rules of a local render: no consumer is ever installed in 035, so `both` behaves as `color`, `closures` follows FR-018, a Bxdf follows FR-026 and FR-017 applies to every shipped shader.
  - Trees never cross the wire.
  - **Result**: `-P:N` images equal local images in every mode. The diagnostics are printed by the servers.
- **Rationale**: A13's default ("`color` on the server with one warning") would make a `closures` render differ between `-P:N` and a local run: lit colour on the server, black locally. "Same as local" needs no code at all on the network path.
- **Approved (Q4)**: "same as local" replaces A13's default.

## R13. Closure-capable backends (FR-023)

- **Decision**: `IShaderBackend` gains two virtuals, both with default bodies, so the `rslo` and `slo` backends do not change:

  ```cpp
  virtual bool emitsClosures(const CShaderInstance &) const { return false; }
  virtual void executeClosures(CShadingContext &, CShaderInstance &, float **locals,
                               openrender::shading::ClosureTreeBuilder &) const {}
  ```

  - `executeClosures` must write the same `Ci` and `Oi` as `execute` whenever the lighting gate is open. That makes `both` byte-identical to `color` (FR-018), and the contract states it.
  - The header comment's sentence "No member names ... a closure" becomes "No member names a language, a closure component, an LLVM type or a bytecode type". The builder is an opaque forward-declared type.
  - The test vehicle is a third backend, `cldbl`, in `tests/closures/`, built on `tests/shading_system/test_backend.h`'s `CTstBackend` pattern. It wraps a real compiled test shader and **delegates `execute` to the real `rslo` or `slo` backend** (maintainer, 2026-11-23: it reimplements nothing), so `occlusion`, `indirectdiffuse`, `gather`, `ambient`, `illuminance` and the rest run through the real interpreter macros and JIT entry points. It answers `emitsClosures` true, and its `executeClosures` runs the delegated `execute` (the gate acts inside it) and then emits a tree that is a pure function of the point's inputs and the wrapper's parameters. "Pattern" and texture work is counted by a test-local shadeop operand that the wrapped shader calls, so the counter is the real call count of the real backend.
- **Alternatives**: a capability flag on `CShader`. Rejected: FR-017 says "an instance whose backend does not declare the capability", so it is the backend's answer.

## R14. Versions (FR-009, D22, the maintainer's rule of 2026-11-23)

- **Decision**: two new version numbers. Both start at 1.0.0 and stay 1.x.

  | Version | Macro | Covers |
  |---|---|---|
  | Closure ABI | `OR_CLOSURE_ABI_VERSION` 1.0.0, in `closureAbi.h` | the node layout, the registry entry shape and the `OrBsdfTable` evaluation conventions |
  | Bxdf table | `OR_BXDF_ABI_VERSION` 1.0.0, in `bxdfAbi.h` | the table a Bxdf registers |

  - `kRendererServicesVersion` stays 2.6.1, because no services member changes (R10).
  - `OR_HIDER_ABI_VERSION` and `OR_INTEGRATOR_ABI_VERSION` stay 2.0.0.
  - The history-table wording is chosen so it cannot be read against `hiderAbi.h`'s "until spec 055 MAJOR stays 2". Suggested text: "New in the path-tracing milestone; starts at 1.0.0 (maintainer, 2026-11-23). The MAJOR-2 rule of `hiderAbi.h` and `kRendererServicesVersion` covers interfaces that predate the milestone, not this one. 1.x is unstable until the milestone closes: until then a breaking change steps MINOR and says so here; after it, only a real breaking change steps MAJOR, to 2.0.0."
- **Rationale**:
  - FR-009 and SC-010 name exactly two versions to quote: "the closure-ABI and Bxdf-interface versions".
  - The BSDF evaluation conventions are part of what reference §9 standardises "with closure evaluation".
- **Approved (Q6)**: two versions.

## R15. The Bxdf binding: a separate registry, no instance key (FR-013 to FR-016, A10)

- **Decision**:
  - `RiBxdfV` resolves the name in a Shading-owned Bxdf registry (`bxdfRegistry.{h,cpp}`) and makes a `CBxdfInstance` (`bxdfInstance.h`).
  - `CBxdfInstance` derives from `CShaderGroupRefTarget` and holds:
    - the table;
    - the table's per-binding data (`create` from the parameter list);
    - the handle string;
    - the FR-026 substitute surface handle (R16).
  - It binds through `CAttributes::bindBxdf(ShaderGroupRef::adopt(instance))`, which already updates the last-bound record (`attributes.cpp:334`).
  - Shading turns the handle back with a new **checked** accessor, `bxdfInstanceOf()`. It is a `dynamic_cast`, which compiles because `CRefCounter` has a virtual destructor (`common/refCounter.h:40`), and it runs once per bind and once per batch. It returns `nullptr` for anything that is not a `CBxdfInstance`. The callers treat that slot as "no Bxdf instance", and `governingMaterial()` alone never selects the Bxdf path.
    - This keeps every existing test that puts a double into the slot well-defined:
      - `tests/unit/state/test_state_last_bound.cpp` binds State doubles, with no shader-parameter hook installed;
      - `tests/shading_parity/test_shader_parameter_bxdf_ignored.cpp` binds an RSL `CShaderInstance`.
    - `grep -rn 'bindBxdf(' src tests` finds only those two, besides `attributes.cpp`. Both keep passing unchanged.
  - A Bxdf bind **takes no instance key**: it draws nothing, and it never calls `takeInstanceKey()`. That call advances `shaderOrdinal` (`rendererContext.cpp:2414-2422`) and would change every later instance's key and draws (FR-016).
  - A refused statement leaves the binding unchanged. It never touches `pendingInstanceKey`, so a pending key cannot leak (US4 AS3).
  - **Refusals**:
    - a `"sl:"` or `"osl:"` prefix: "reserved for spec 036/049";
    - an unknown name;
    - a table whose MAJOR `checkPluginVersion` refused at registration, through the in-process overload of `pluginAbi.h:72`. The registry remembers the refusal reason, so the statement reports it.
  - **Later RSL and OSL Bxdfs** (specs 036 and 049) register a table whose `build` runs a shader. The bxdf slot therefore always holds a `CBxdfInstance`, and `bxdfInstanceOf` never needs to tell two types apart.
  - Built-ins register through the call graph, on the first registry use. They use the same `registerBxdf(const OrBxdfTable *)` a plug-in would (D8).
- **Rationale**: going through `ShadingSystem::bind()` or `ShaderGroupBuilder::shader()` takes an instance key on every successful bind. A10's "a Bxdf binds a group of one layer" is met in substance, because the slot holds one layer. The group builder stays the RSL route.
- **Approved (Q2)**: this reading of A10, and a Bxdf takes no key.
- **Alternatives**: a `"bxdf"` `IShaderBackend` inside `ShadingSystem`. Rejected: it takes a key, it forces a fake `CShaderInstance` with interpreter-shaped virtuals (`illuminate`, `prepare`), and D9's file search does not apply to built-ins.

## R16. What "the default surface" of FR-026 renders (clarify item 2)

- **Finding**:
  - `RiWorldBegin` binds `defaultsurface` only when no `Surface` was given before it (`rendererContext.cpp:995-1001`, `setDefaultSurface`, which leaves the last-bound record alone).
  - `bindBxdf` leaves `surfaceSlot` alone. After `Surface "plastic"; Bxdf "diffuse"`, the slot still holds `plastic`.
  - With the slot empty, `shade()` takes the no-shader path, `Ci = Cs * |I.N|` and `Oi = Os` (`shading.cpp:1558-1592`).
- **Options**:

  | Option | Renders | State and cost | Byte-identical to |
  |---|---|---|---|
  | **A** | the surface that `RiWorldBegin` left in the world block's slot: `defaultsurface`, a pre-world `Surface`, or nothing, in which case the no-shader path | the core captures that handle at WorldBegin and passes it to `CBxdfInstance` at bind. A Bxdf bound before WorldBegin gets the no-shader path (superseded for this case by B2). `usedShaderParameters` unions the substitute's `requiredParameters()` in place of the surface slot's when Bxdf governs | the same object with no material statement anywhere in its scope chain |
  | **B** | always the no-shader path | no handle. `usedShaderParameters` treats the surface slot as empty when Bxdf governs | the same object under `Surface "null"` |
  | **C** | whatever is in the surface slot (the Surface the Bxdf shadows) | none. `usedShaderParameters` unchanged | the same scene with the `Bxdf` statement deleted |

- **Decision (Q1, refined by Q-R1, approved 2026-11-23): A′**, RISpec's implementation default surface (§4.2.4: "an implementation-dependent default surface shader (but not "null")"), never a user-bound `Surface`:
  - When `RiWorldBegin` bound `defaultsurface` through `setDefaultSurface()`, the core captures that handle at `RiWorldBegin`, and `RiBxdfV` passes it to the `CBxdfInstance`.
  - Otherwise the substitute is the built-in no-shader path. That covers a pre-world `Surface` (`defaultsurface` was not loaded) and a `defaultsurface` that could not be loaded. RISpec §4.2.4 allows this path too, because it is not null.
  - **A Bxdf bound before `WorldBegin` resolves lazily** (analyze B2, maintainer 2026-11-23): its substitute starts empty. In `RiWorldBegin`, when `defaultsurface` is about to be bound, the core **first** hands the loaded shader to the `CBxdfInstance` already in the current attributes' bxdf slot (`bxdfInstanceOf`) and **then** calls `setDefaultSurface()`: its `checkParameters()` stores `usedParameters`, and `usedShaderParameters` reads the substitute, so the order matters (analyze R1). A pre-world Bxdf therefore renders the initial default surface of §4.2.4 exactly as the same object with no material statement does, and the acceptance below holds for it too.
  - **Cost**: no extra shader load and no instance key, so Q2 holds.
  - `usedShaderParameters` uses the substitute's `requiredParameters()` (or `PARAMETER_N` for the no-shader path) in place of the surface slot's, when a Bxdf governs.
  - **Acceptance**: in a scene with no `Surface` statement anywhere, a Bxdf-governed object equals the same object with no material statement. In a scene with a pre-world `Surface`, it equals the same object rendered by the no-shader path.
  - Option A's fallback to a user's pre-world `Surface`, and options B and C, are not taken.
- **One function**: the choice lives in `interimSurfaceFor(attributes)`, which both `planSurfaceStep` and `usedShaderParameters` call.
- **Test consequence**: `tests/shading_parity/test_shader_parameter_bxdf_ignored.cpp` (spec 025, "until spec 035") binds an RSL `CShaderInstance` into the bxdf slot. The checked accessor of R15 answers `nullptr` for it, so the used-parameter union ignores the slot, as the test asserts. The test passes unchanged under A, B and C.
  - A new test, `ShadingParity_ShaderParameterBxdfGoverned`, pins the new rule with a real `CBxdfInstance`.
  - The old test's header comment ("until spec 035") is updated to say what it now pins: a foreign object in the slot is ignored. That is a comment edit, not a change of assertion.
- **Approved**: Q1 (as A′, by Q-R1) and Q13 (the comment-only update of that test).

## R17. Interim rules and their deduplication (clarify items 1 and 5)

- **Decision, per-object precedence** (evaluated once per batch by `planSurfaceStep`):
  1. **IR-026**: the governing material is a Bxdf and no consumer is installed. The object renders with the R16 substitute. One diagnostic per frame **per Bxdf name** names it. The mode is not consulted at all, because `closuremode` governs only a `Surface` binding (FR-017), so IR-017 and IR-018 can never fire for this object. The substitute itself is never checked for closure capability.
  2. **IR-017**: the governing material is a Surface, the mode is `both` or `closures`, and the instance's backend does not emit closures. The object renders as `color`. One warning per frame per **(shader name, mode)**. The object is then a `color` object, so IR-018 cannot apply to it.
  3. **IR-018**: a capable Surface under `closures` with no consumer installed. `Ci = 0`, and `Oi` is the value the shader computed (Q-R5, RISpec §12.1). The gate leaves the non-lighting work that computes `Oi` running, a texture cut-out for instance. One warning per frame.
  - A capable Surface under `both` with no consumer takes the `color` path silently. FR-021 says it "behaves as `color` and drops its trees". Silent (Q5b, approved).
  - A frame can carry all three kinds of warning, for different objects. No object carries two.
- **How spec 042 names what it replaces**:
  - Each rule has an ID, `IR-017`, `IR-018` or `IR-026`, named after its FR.
  - Each ID is:
    - an enumerator of `openrender::shading::ClosureInterimRule`;
    - **one** function in `closureInterim.{h,cpp}`;
    - one test named `Closure_Interim_IR0xx_*`;
    - one row of the "Interim rules (owner: spec 042)" table in `DEVNOTES_DETAILS/CLOSURES.md`.
  - 042's spec names the ID, replaces that function and retires that test by name.
  - Applying the answer to "per Bxdf name or per frame" changes one dedup key.
- **Approved**:
  - Q5a: FR-026's key is per Bxdf name.
  - Q14: the ID is not in the message text; it lives in the code, the tests and the guide.

## R18. Bxdf-governed object with a consumer installed

- **Decision**: a Bxdf binding is closure-only by nature (US3). With a consumer installed:
  - the Bxdf builds a tree per point and the consumer receives it;
  - the Surface slot is not run;
  - `Ci = 0` and `Oi = Os`, with no warning, because a consumer exists. No shader runs, so `Os` is the opacity, as every RISpec Appendix A surface writes `Oi = Os`;
  - displacement and atmosphere run as usual (A9).
- **Rationale**: this is derived from US3 and FR-021, and it needs no new rule. Spec 042 decides what its integrator writes. It is not listed as a question, but the report flags it.

## R19. What reaches the consumer, and for which batches (FR-021, SC-002)

- **Decision**:
  - The consumer is called once per batch with a `ClosureBatch`:
    - `numPoints` = `numRealVertices`. The raytrace derivative helper vertices of `shade()`'s `SHADING_2D` path are not shading points (`:1319-1402`).
    - the trees;
    - strided views of `P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v` and `I` in `varying`;
    - the `ObjectHandle` (`currentObject`) and the points' `SampleKey`s (`pointKeys`);
    - the batch's `rayType`, `pathDepth` and `shape`, from `ShaderGlobals`.
  - Every full `shade()` of a capable object delivers, whatever the ray type. A consumer filters by `rayType`.
  - The geometry record's parameters are added to `usedParameters` **only** when the plan builds trees, meaning a consumer is installed and the mode is not `color` (or a Bxdf governs). The `color` path and the no-consumer paths therefore sample exactly what they sample today.
- **Consequence**: under IR-018, a `closures` object's shadows and its appearance in other objects' `trace()` use `Ci = 0` and the shader's own `Oi` (Q-R5), consistently. SC-007's "0 pixels differ" holds only for a scene that keeps the `closures` object out of the `color` objects' shadow and reflection rays. The quickstart scene does that.
- **Approved (Q11)**: the per-real-point count, delivery for all ray types, and the SC-007 scene condition.

## R20. The `Bxdf` statement in the Scene Interface (FR-013)

- **Decision**: `RiBxdf(RtToken name, RtToken handle, ...)` and `RiBxdfV(name, handle, n, tokens, params)` (`ri.h`, `ri.cpp`).
  - **Validity** (Q-R4, approved 2026-11-23): its own mask, `VALID_BXDF_BLOCKS`, is `VALID_ATTRIBUTE_BLOCKS` without `RENDERMAN_OBJECT_BLOCK`. `check()` therefore refuses the statement inside an `ObjectBegin` block, as RISpec §5.10 says ("shader assignments or other attributes are not allowed within an Object block").
  - `Surface` and `Attribute` keep today's mask. That older deviation is filed by the coordinator as its own issue and is not 035 work.
  - **Colour parameters** are read with the current `ColorSamples` count and converted with `COptions::convertColor`, as `RiColor` is (RISpec §4.1.3).
  - `CRiInterface::RiBxdfV` has a default body that ignores the statement, as `RiIntegratorV`'s does (`riInterface.cpp:172`), so the preview ignores it with no edit.
  - **Grammar**: `RIB_BXDF RIB_TEXT RIB_TEXT ribPL` (with a handle) and `RIB_BXDF RIB_TEXT ribPL` (without). That is the `LightSource` precedent (`rib.y:1326-1370`): a second string is read as the handle, so a first parameter whose value is a bare string needs brackets when the handle is omitted. The guide documents this.
  - `ribOut` writes `Bxdf "name" "handle" <pl>`, or `Bxdf "name" <pl>` without a handle, at the statement's position.
  - Python and Lua get `Bxdf(name, handle=None, parameterlist=None)`, beside `Integrator` (`prman.py:282`, `prman.lua:334`).
  - The handle and parameter-list grammar is the one spec 034 extends with `"reference ..."` parameters.
- **Approved (Q9)**: the handle is optional (the `LightSource` rule).

## R21. Built-in Bxdfs and their parameters (A4, FR-015)

- **Decision** (Q7 and Q-R3, approved 2026-11-23): four tables. Every parameter is typed, range-checked and documented:
  - out-of-range values are clamped, with one diagnostic naming the parameter;
  - a refusing parameter refuses the statement: a negative `plastic` `ior`, or a `dielectric` `ior` at or below 0.
- **Where a Bxdf models a RISpec 3.2 standard shader** (Table 4.8, Appendix A), it takes that shader's parameter names and defaults.
- **Common rules**:
  - The base colour is the point's `Cs` and the opacity is `Oi = Os` (§4.2.1: every primitive inherits the current colour and opacity). `Cs` and `Os` are point inputs, like `N`, so the Bxdf stays declarative (FR-015).
  - `Ka` is not a parameter: ambient is lighting work with no closure lobe, so `Ka` gets the unknown-parameter diagnostic.
  - The GGX `alpha = roughness`, not squared, for **all four** built-ins, `dielectric` included (maintainer, 2026-11-23). RISpec leaves `specularbrdf` implementation-dependent (§15.6). openRender's highlight is `pow(N.H, 10/roughness)` (gotcha 25): exponent 100 at the default 0.1, which corresponds to a microfacet `alpha` of about 0.14.

| Bxdf (models) | Parameters (default; range) | Tree |
|---|---|---|
| `diffuse` (`matte`) | `float Kd` (1; ≥ 0); extension `float roughness` (0; [0,1]) | `Kd*Cs * diffuse(N)`; with `roughness > 0`, `Kd*Cs * oren_nayar(N, roughness)` |
| `plastic` (`plastic`) | `float Kd` (.5; ≥ 0), `float Ks` (.5; ≥ 0), `float roughness` (.1; [0,1]), `color specularcolor` (1; ≥ 0); extension `float ior` (0; ≥ 0; negative refused) | `Kd*Cs * diffuse(N) + Ks*specularcolor * microfacet("ggx", N, roughness, ior, 0)`, additive as in Appendix A's `plastic`. With the default `ior` 0 the `microfacet` lobe has no Fresnel term (`F = 1`, the same convention as `conductor`), so the highlight weight matches the standard `plastic`'s `Ks * specularcolor * specular()`. `ior > 0` enables the dielectric Fresnel term |
| `conductor` (`metal`) | `float Ks` (1; ≥ 0), `float roughness` (.1; [0,1]) | `Ks*Cs * microfacet("ggx", N, roughness, 0, 0)`, with `eta = 0` read as "no Fresnel term" (`F = 1`) |
| `dielectric` (no RISpec counterpart) | `float ior` (1.5; > 0), `float roughness` (0; [0,1]), `color reflectColor` (1), `color transmitColor` (1) | `reflectColor * microfacet("ggx", N, roughness, ior, 0) + transmitColor * microfacet("ggx", N, roughness, ior, 1)`; below the alpha floor these become `reflection`/`refraction` (R7) |

- `N` is the shading normal of the point, `faceforward`ed against `I` (as every Appendix A surface does), in camera space, the shading space of every vector in the tree.
- **Decided by the maintainer** (2026-11-23):
  - **`plastic`'s `ior`** defaults to 0, meaning no Fresnel term, so the highlight matches the standard `plastic` shader. `ior > 0` enables Fresnel.
  - **`dielectric`** uses `alpha = roughness` like the other three, so there is one roughness mapping.

- **Opacity is the consumer's** (analyze B1, maintainer 2026-11-23): the trees carry no `Os`. RISpec Appendix A writes `Ci = Os * (...)`; the consumer applies `Oi = Os` and the premultiplication, and `DEVNOTES_DETAILS/CLOSURES.md` says so. `Bxdf_Tree_NoOsPremultiply` pins that the weights are independent of `Os`.

## R22. Tests and determinism entries (FR-028, SC-006)

- **Finding**: the determinism sweep runs `orender` images (`tests/visual/determinism-scenes.cmake`, `add_determinism_sweep`). `orender` has no closure-emitting shader, because FR-017 turns every shipped shader into `color`, and it has no consumer. Sweep entries can therefore not observe trees.
- **Decision**: the count is split in two.
  - **24 image entries** in the sweep, generated by `specs/035-closure-tree-bsdf/tools/gen_scenes.py`:

    | Scene | What it exercises |
    |---|---|
    | `closures-modes` | option `both`, a nested attribute `closures` and a nested attribute `color`: IR-017 on shipped shaders |
    | `closures-bxdf-interim` | the four Bxdfs and the Surface/Bxdf order, in nested blocks: IR-026 |
    | `closures-arealight` | a bound area light, with a `color` receiver and an isolated `closures` receiver |
    | `closures-instancing` | `ObjectBegin`/`ObjectInstance`, carrying the mode and a Bxdf |

    Each of the 4 scenes runs under each of the 3 camera hiders (`reyes`, `zbuffer`, `raytrace`) in each of the 2 shader formats (`rslo`, `slo`): 4 x 3 x 2 = 24.
  - **In-process tree checks** in `tests/closures/`, with the `cldbl` backend and a hashing consumer double. The unit-test programs drive a real `RiBegin`/`RiWorldBegin` session in-process, as `tests/shading_parity/test_used_parameters_oracle.cpp` does:
    - `ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}`: tree hashes at 1 and at 4 threads;
    - `ClosureDet_BatchShape`: the same point inputs through a grid, a span and a 0-D batch give the same trees, which is FR-022's "same shading input";
    - `ClosureDet_HiderParity`: a flat object whose tree depends only on `Cs` and the wrapper's parameters gives the same set of distinct tree hashes from reyes, zbuffer and raytrace;
    - `ClosureDet_BsdfRepeat`: the BSDF answers on 1 thread and on 4 threads.
- **Sweep coverage, stated plainly** (accepted by the maintainer, 2026-11-23): the 24 sweep scenes use shipped shaders, which are never closure-capable (FR-017), so they prove only the interim behaviours IR-017 and IR-026 across threads, hiders and formats (IR-018 needs a capable Surface, so it cannot fire there and is covered by `Closure_Interim_IR018_*`). The closure path is observed only by the in-process tests.
- **Spec deviation**: SC-006 asks for bit-identical trees, evaluations and samples "over at least 24 new determinism-sweep entries". The sweep cannot observe a tree, so the plan reads SC-006 as **24 sweep image entries plus 6 in-process tree and BSDF checks**: the three `ClosureDet_ThreadIdentity_*` tests, `ClosureDet_BatchShape`, `ClosureDet_HiderParity` and `ClosureDet_BsdfRepeat`.
- **Approved (Q15)**: SC-006 is amended to that wording in spec.md.
- **Risk R-5**: a full in-process render through a registered third backend has not been done before. `ShadingSystem_ThirdBackend` binds and executes it at system level only. The first task of the mode phase proves the harness.

## R23. Master movement and the bug-fix baseline

- **Finding** (as of 2026-11-23, master `4ee81900`; the worktree base is `b9dec4e5`):
  - **Merged**: #199 (light categories rebuilt after the RIB parameters: `shader.cpp`, `shaderGroup.cpp`, files 035 does not edit); #200 (`.slo` `illuminance()` honours its category: `jitIlluminanceBegin` passes it and still calls `iterateLights`); #197 (a `.slo` light's `__nondiffuse`/`__nonspecular` are saved by `jitIlluminateEnd`/`jitSolarEnd`, inside a light shader, which never runs under the gate); #115 (`lightsource()` follows RISpec §15.8 in both backends through `op_lightsource_param`; `kRendererServicesVersion` **2.6.1**, a PATCH step); #204 (`ShadingInterpolation "constant"` in the reyes and zbuffer rasterizers: `reyes.cpp`, `stochasticQuad.h`, `zbufferQuad.h`, `rendererContext.cpp`, `attributes.h`; outside the lighting work, but it touches two files 035 edits, so expect neighbouring hunks, not an overlap).
  - **Still to land**: #113 (`phong()` reads only the first vertex's size), #203 (`jitParameterFinish`) and spec 054 (`shade()`'s sampling side).
- **Decision**: 035 designs on the funnels (R9), not on the JIT wrappers, so none of those fixes changes the design.
  - A master sync into the branch is a **coordinator or maintainer prerequisite**, not an agent step: agents do not merge. tasks.md has two, the first at the start of Phase 0 and the second before the closing validation; the first task after each re-reads `iterateLights`, `jitIlluminanceBegin`, `prepareAmbient`, `gatherHeaderBegin` and `shade()` on the synced tree.
  - Overlap is limited to `shading.cpp` and `shading.h` (different functions from the funnels' own edits, as declarations) and to `rendererContext.cpp` and `attributes.{h,cpp}` (neighbouring hunks).
  - No requirement or task depends on the merge.
