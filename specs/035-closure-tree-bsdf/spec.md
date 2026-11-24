# Feature Specification: Closure Tree and BSDF Interface

**Feature Branch**: `035-closure-tree-bsdf`
**Created**: 2026-11-23
**Status**: Draft (clarify run 2026-11-23)
**Input**: GitHub issue #41 ("Spec 035: backend-neutral closure tree and BSDF interface; closuremode attribute/option", new capability, label `spec`); the maintainer's plan, section "035 material-interface (closure model)" (`~/.claude/plans/there-is-an-issue-glowing-perlis.md`, layer L6a, on the critical path to the direct integrator); `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §2 (materials and closures), §7.4, §9 and decisions D1, D5, D7 to D9, D13 to D19, D22, D28, D30; `DEVNOTES_DETAILS/PATH-TRACING_HIDER.md` (background only, see Discrepancies 3). Base: `master` at `b9dec4e5`, with specs 029, 030, 031, 032, 033, 037, 038, 039, 040, 048, 056 and 057 merged. Depends on spec 033 (#39, merged).

## Clarifications

### Session 2026-11-23

- Q: FR-017, a `Surface` shader that cannot emit closures under `both` or `closures`? -> A: it renders as `color`, with one warning per (shader, mode) per frame (no drop-off, D28).
- Q: FR-018, what `closures` writes with no consumer installed (before spec 042)? -> A: black colour and the object's own `Os` as opacity, with one warning per frame. *(The opacity half was amended at plan by Q-R5: it is the shader's own `Oi`, per RISpec §12.1.)*
- Q: FR-026, a `Bxdf`-governed object before spec 042? -> A: one diagnostic per frame naming the Bxdf, and the object renders with the default surface. Spec 042's clarify settles this for good. *("The default surface" was fixed at plan by Q-R1: RISpec's implementation default, never a user-bound `Surface`.)*
- Q: A4, the first built-in Bxdfs and the component registry? -> A: the Bxdfs are `diffuse`, `plastic`, a GGX conductor and a GGX dielectric. The component registry is `diffuse`, `oren_nayar`, `microfacet` (`ggx` and `beckmann`), `reflection`, `refraction`, `transparent`, `holdout`, `uniform_edf`, plus the deprecated `emission` alias.
- Q: A8, the version at which the closure ABI and the Bxdf table start? -> A (changed by the maintainer): **1.0.0**, MAJOR 1 until the milestone completes. These are the first, baseline implementations of path-tracing features. The "MAJOR 2 at milestone end" rule applies to the existing core interfaces, not to these new ones. They stay separate from `kRendererServicesVersion`, which is unchanged unless the plan finds a services change, and that would be a MINOR step.
- Q: Overlap with 054 (A6), who owns interpolating corner trees? -> A: spec 042 owns interpolating corner closure trees under Bxdf integrators (the same-shape test, weight and parameter blending). 035 guarantees only a stable node order; 054 provides only the hit-to-cell mapping. This is a dependency recorded in A6, not 035 work.
- Q: Scope (registry, `Spectrum`, BRDF library, built-in Bxdfs, `Bxdf` statement)? -> A: accepted by the maintainer; issue #41's body was updated to match.
- Q: A5 (`Option "shade"` a new option family), A13 (network rendering left to plan), the FR-019 definition of "lighting work"? -> A: the specify-stage defaults stand.

### Plan-stage answers (maintainer, 2026-11-23)

The plan's open items Q1 to Q15 were approved as `plan.md` recommended. The reasons are in `research.md`.

Every decision was then reviewed against RISpec 3.2 (`plan.md`, "RISpec 3.2 conformance"). The review found five conflicts, Q-R1 to Q-R5. The maintainer approved the RISpec-conforming answer for each, and they are folded in below and in the requirements. Where RISpec 3.2 defines a behaviour, this spec follows it.

**Must-decide items:**

| Item | Approved answer |
|---|---|
| Q1 (FR-026), as refined by Q-R1 | The "default surface" is RISpec's implementation default surface (§4.2.4), never a user-bound `Surface`. It is the `defaultsurface` instance when `RiWorldBegin` bound it (`setDefaultSurface()`), captured on the Bxdf instance. Otherwise (a pre-world `Surface` was given, or `defaultsurface` could not be loaded) it is the built-in no-shader path (`Cs*|I.N|`, `Os`). There is no extra shader load and no instance key. A Bxdf bound before `WorldBegin` receives the `defaultsurface` handle lazily when `RiWorldBegin` binds it (analyze B2) |
| Q2 (FR-014, FR-016, A10) | A Bxdf binds through its own Shading registry into a `CBxdfInstance`, not through `ShadingSystem`, and takes no instance key |
| Q3 (FR-017, FR-025) | The scene mode reaches Shading through a per-frame `ClosureFrame` that the core sets on every context. There is no services change |
| Q4 (A13) | `-P:N` behaves as a local render: servers parse the RIB and apply the same rules, and no consumer is installed in 035. **This replaces A13's default.** |
| Q5 | IR-026 is reported once per frame per Bxdf name. `both` with a capable shader and no consumer is silent |
| Q6 (FR-009) | Two versions: the closure ABI (the layout, the registry and the BSDF table) and the Bxdf table, both 1.0.0 |
| Q7 (A4), with Q-R3 | The names are `diffuse`, `plastic`, `conductor` and `dielectric`, and the conductor is `microfacet` with `eta = 0` read as `F = 1`. Where a Bxdf models a RISpec standard shader (Table 4.8, Appendix A), it takes that shader's parameters, listed in the table below. Common rules: the base colour is the point's `Cs`; `Oi = Os`; `Cs` and `Os` are point inputs, like `N`; `Ka` is dropped (ambient has no closure lobe); the GGX `alpha = roughness`, not squared. `dielectric` has no RISpec counterpart and keeps its own parameters |
| Q15 (SC-006) | SC-006 is amended to "24 sweep image entries plus 6 in-process checks" |

The three Bxdfs that model a RISpec standard shader (Q-R3):

| Bxdf | Models | Parameters (default) |
|---|---|---|
| `diffuse` | `matte` | `Kd` 1 |
| `plastic` | `plastic` | `Kd` .5, `Ks` .5, `roughness` .1, `specularcolor` 1; and `ior` 0 as an extension (0: no Fresnel term, as the standard shader; > 0 enables Fresnel) |
| `conductor` | `metal` | `Ks` 1, `roughness` .1 |

The GGX `alpha = roughness` holds for all four built-ins, `dielectric` included (maintainer, 2026-11-23).

**Confirmation items:**

| Item | Approved answer |
|---|---|
| Q8 (FR-020) | A bad value keeps the previous value on both sides |
| Q9 (FR-013), with Q-R4 | The handle is optional (the `LightSource` grammar rule). `RiBxdf` is refused inside an `ObjectBegin` block (RISpec §5.10). `Surface` and `Attribute` keep today's behaviour there; the coordinator files that older deviation as a separate issue, outside 035 |
| Q10 (FR-019) | The gated `occlusion()`/`indirectdiffuse()` return 0, and `gather()` runs with 0 samples |
| Q11 (SC-002) | One tree per real point, for every ray type |
| Q12 | 1024 bytes per point |
| Q13 | `test_shader_parameter_bxdf_ignored.cpp` changes in its comment only |
| Q14 | Interim-rule IDs are not put in the message text |

**RISpec 3.2 answers** (maintainer, 2026-11-23):

| Item | Approved answer |
|---|---|
| Q-R1 (FR-026) | The default surface is A′ (above, under Q1) |
| Q-R2 (instancing) | An instance takes the `closuremode` and the Bxdf binding current at `ObjectInstance`, not those of the definition scope (RISpec §5.10; openRender's instancing already does this) |
| Q-R3 (A4, FR-015) | The parameter rules above, under Q7 |
| Q-R4 (FR-013) | `RiBxdf` is refused inside an `ObjectBegin` block |
| Q-R5 (FR-018) | Under `closures`, a capable Surface keeps the `Oi` its shader computed (RISpec §12.1), with `Ci = 0`. `Os` is written only where no shader runs (a Bxdf-governed object with a consumer) |

**Versions**: the closure ABI and the Bxdf table start at 1.0.0 and stay 1.x. Existing interfaces stay at MAJOR 2.

## User Scenarios & Testing *(mandatory)*

### Measured starting point (2026-11-23, master `b9dec4e5`, by reading the source; nothing was built or rendered for this stage)

- **No closure exists anywhere in the tree.** `grep -i closure` over `src/` finds only unrelated hits: `src/ri/transport/capability.h` (which reserves the capability names `surface-closures` and `volume-closures` for later), CSG "caps" in Geometry, and one sentence in the `shaderBackend.h` header comment. There is no closure type, no registry, no BSDF interface and no `Spectrum` type. `closuremode` appears only in the planning documents.
- **A `Surface` shader's result is `Ci` and `Oi` and nothing else.** `CShadingContext::shade()` (`src/libshader/shading/shading.cpp`, around `:1556`) runs the surface group with `libshader::groupExecute(*this, surface, locals[ACCESSOR_SURFACE])`; the lights are iterated from inside the shader (the `illuminance`, `ambient`, `diffuse`, `specular`, `phong`, `lightsource` machinery, `iterateLights` and, since spec 057, `iterateBoundAreaLight`). With no surface shader the code takes `Cs` and `Os` from the attributes. Nothing records "what the surface scatters", so no later stage can re-light a shaded point.
- **The `bxdf` slot exists and nothing uses it.** `CAttributes` (`src/ri/state/attributes.h:74-79,144-154,254`) has a private `bxdfSlot`, `bindBxdf()` and the Surface/Bxdf last-bound record `governingMaterial()` (spec 025, FR-010 and FR-011; D5: the later of the two statements wins, both are saved and restored by `AttributeBegin`/`AttributeEnd`). `shaderParameters.cpp` ignores the slot. **No `Bxdf` RIB statement exists:** there is no entry in `rib.y`, `ri.cpp`, `ribOut`, `src/python/prman.py` or `src/lua/prman.lua`. `tests/unit/state/test_state_last_bound.cpp` and `tests/unit/rib_parse/test_ribout_shader_order.cpp` exercise the slot and the order record directly.
- **`ShaderGroupBuilder::connect()` is refused** (`CODE_INCAPABLE`, `src/libshader/shading/shaderGroup.h:44`) and a second layer is refused until spec 034 (CLAUDE.md gotcha 41). A group of one layer is its layer's instance.
- **A backend is a seam with no language in it.** `IShaderBackend` (`shaderBackend.h`) has `key/extension/load/instantiate/initialise/execute`; its header says "No member names a language, a closure, an LLVM type or a bytecode type". A third backend registers with `registerShaderBackend()` and nothing in `src/ri`, `src/display`, `src/shadeop`, `src/procedures` or `src/common` changes (`Shading_CoreNoBackendNames`, `ShadingSystem_ThirdBackend`, gotcha 40). That is the route a closure-emitting test double takes.
- **`closuremode` has a place to live.** The attribute side is `RiAttributeV(... "shade" ...)` (`rendererContext.cpp:3923`, which today sets `shaderformat`); the option side is `RiOptionV` (`:1720` onwards; `Option "shaderformat" "default"` is at `:2106`, and there is no `Option "shade"` today). The four-layer rule of the Attributes-system section of CLAUDE.md applies: token constants (`ri.h`/`ri.cpp`/`riTokens.cpp`/`riVocabulary.h`, kept equal by `Common_VocabularyConsistency`), RIB parsing, storage and query in `CAttributes`/`COptions`, and pre-declaration in `CDeclarations` (`declarations.cpp:189` declares `RI_SHADERFORMAT` as a string).
- **The plug-in convention exists.** Spec 037 merged `src/include/orplugin.h`/`orplugin.hpp` (`OR_ABI_VERSION`, a function table with `structSize` and a semver version, `checkPluginVersion` with an in-process-table overload added by spec 030). `hiderAbi.h` is the precedent for an internal table: version 2.0.0 ("until spec 055 MAJOR stays 2 and 2.x is unstable"), internal until spec 051. The plan's "035 copies the convention forward before 037 lands" is obsolete: 037 is merged, so 035 uses it (Discrepancies 1).
- **The sample stream exists.** Spec 039's keyed sampler (`src/common/sampling.h`: `SampleId`, `SampleKind`, per-call sub-streams) supplies decorrelated numbers by `(id, dimension)`. Call-site constants above `0xFF00` are reserved there; `0xFF20-0xFF2F` belong to spec 057. 035 draws nothing itself: a BSDF `sample()` takes its random numbers from its caller (the integrator, spec 042), and the unit tests draw them from this stream.
- **A bound area light is already in the tree.** `iterateBoundAreaLight` (spec 057, CLAUDE.md gotcha 46) runs a bound area light's shader once per emitter sample, scaled by `V / (n p_A)`; the emitter registry and `IAreaLightSampler` are installed per frame. The renderer services were at 2.6.0 at the base commit and are 2.6.1 on master since GitHub #115 (`kRendererServicesVersion`, a PATCH step), with a history table; 2.6.1 is the starting point of this spec.
- **The bug-fix baseline is moving.** A separate session is fixing #199 (a light's `__category` set in the RIB is ignored), #200 (`.slo` `illuminance()` ignores its category argument), #197 (JIT `illuminate()` lights ignore `__nondiffuse`/`__nonspecular`), #115 (JIT `lightsource()` differs from the interpreter), #113 (JIT `phong()` reads only the first vertex's size) and #169 (`visibility()` always returns 1). All six sit in the lighting work that `closures` mode switches off. (As of 2026-11-23 #199, #200, #197 and #115 are merged into master; #204, `ShadingInterpolation "constant"` in the reyes and zbuffer rasterizers, merged too and is outside the lighting work; #113 and #169 are still open.) Their fixed behaviour is the baseline of `color` and `both`; this spec absorbs none of them.

### What the plan requires (the target)

A shaded point can hand its integrator a **description of how it scatters light** (a tree of weighted, typed components in OSL's layout and with OSL's standard names) instead of only a finished colour. A single interface evaluates, samples and gives the density of any scattering component, so a later direct integrator (spec 042) and path tracer (spec 050) never walk a tree themselves. A per-scope switch, `closuremode`, chooses what a `Surface` binding hands back: the old colour (`color`, the default and byte-identical), colour and tree (`both`), or the tree without the lighting work (`closures`). Reference doc §2.1: *the shader describes scattering; the integrator decides how scattering becomes pixels.*

### User Story 1 - A backend-neutral closure tree with an OSL-shaped layout and a registry of OSL's standard names (Priority: P1)

A developer building a shading backend (the interpreter today, the RSL 2.0 compiler of spec 036 and the OSL backend of spec 049 later) needs one place to put what a shader computed about scattering. A shaded point gets a tree: component nodes (a registered closure id, a weight, a block of typed parameters), multiply nodes (a weight times a child) and add nodes (two children). The tree is a plain-data record set allocated from the shading batch's arena. A registry maps each closure name to an id, a category (BSDF, EDF, VDF or signal) and a typed parameter list; the first registry holds `diffuse`, `oren_nayar`, `microfacet` (`ggx`, `beckmann`), `reflection`, `refraction`, `transparent`, `holdout` and `uniform_edf` (with `emission` as a deprecated alias of the latter). An additive tree flattens to a lobe list that the fast path uses; anything else goes through a generic evaluator.

**Why this priority**: every other story consumes the tree; nothing else changes visibly, so it is safe to land and test alone.

**Independent Test**: build trees by hand (no shader, no hider) and check the layout, the registry lookups, the flattened lobe list against the generic evaluator, the arena lifetime and the category of every registered name.

**Acceptance Scenarios**:

1. **Given** the registry, **When** every standard name is looked up, **Then** it resolves to one id, one category and one typed parameter list; a name outside the set is refused with a diagnostic naming it; and `emission` resolves to `uniform_edf` with a one-time deprecation message.
2. **Given** a tree `w1 * diffuse(N) + w2 * microfacet("ggx", N, alpha, eta, 0)`, **When** it is flattened, **Then** it yields two lobes whose types, weights and parameter blocks equal the nodes', and evaluating the lobe list equals evaluating the tree.
3. **Given** a tree with a multiply of a multiply, a zero weight and a nested add, **When** it is evaluated, **Then** the flattened result equals the generic evaluator's and the zero-weight component contributes exactly nothing.
4. **Given** the node records, **When** their sizes, alignments and field offsets are read, **Then** they match OSL's `ClosureComponent`/`ClosureMul`/`ClosureAdd` as documented in reference doc §2.4 (component id at offset 0; `id >= 0` a component, `-1` a multiply, `-2` an add; 16-byte aligned component; three-float weight; the parameter block directly after the struct), pinned by compile-time and run-time checks that spec 049's layout test can compare against `oslclosure.h`.
5. **Given** a batch that ends, **When** its arena is released, **Then** no tree built in it is reachable; a consumer that needs a tree for longer copies it, and the documented lifetime says so.

---

### User Story 2 - One BSDF interface evaluates, samples and gives the density of any component, ready for bidirectional transport (Priority: P1)

An integrator author (spec 042's direct integrator now, spec 050's path tracer and a later bidirectional tracer after it) asks "what is the scattering function for this outgoing and this incoming direction", "give me an incoming direction for these random numbers and the density you used" and "what is the density of this incoming direction". The answer follows the conventions of reference doc §2.6: `wo` and `wi` point away from the surface; `f` is returned without the cosine factor; densities are per steradian; a delta lobe is flagged explicitly and never faked as a density; a query carries a transport mode (radiance from the camera, importance from the lights) and the answer carries both the forward density `p(wi | wo)` and the reverse density `p(wo | wi)`. The interface is a C function table with a C++ convenience wrapper (D7); integrators never walk closure trees.

**Why this priority**: it is the reason the tree exists, and the unit tests that prove it correct (energy, reciprocity, density) are what spec 042 validates its images against.

**Independent Test**: for every BSDF-category component in the registry, run the numerical checks of SC-004 on the component alone: no shader, hider, light or image.

**Acceptance Scenarios**:

1. **Given** a diffuse component and two directions in the upper hemisphere, **When** it is evaluated, **Then** `f` equals `albedo / pi` with no cosine, each density equals `cos(theta) / pi` of the direction it describes, and the flags say "diffuse reflection, not delta".
2. **Given** a smooth `reflection` or `refraction` component, **When** it is evaluated at any pair of directions, **Then** the answer is zero with the delta flag set; when it is sampled, the returned direction is the mirror or Snell direction, `f` carries the Fresnel-weighted delta value, and no density is invented.
3. **Given** a query in `Radiance` mode and the same query in `Importance` mode on a refracting component, **When** both are evaluated, **Then** `f` differs by the squared index-ratio factor of non-symmetric scattering, while a reflection-only component gives identical answers in both modes.
4. **Given** a sample drawn from random numbers, **When** the returned direction is evaluated and its density queried independently, **Then** the sampled `f` and densities equal the independent evaluation (sample/density consistency), forward and reverse.
5. **Given** a direction in or below the horizon of the shading frame, **When** any component is evaluated, **Then** the answer is zero with zero density, finite everywhere, never negative or non-finite.
6. **Given** the same query and the same random numbers, **When** repeated on one thread or four, **Then** every output is bit-identical (the interface keeps no state and draws nothing of its own).

---

### User Story 3 - `closuremode` chooses what a `Surface` binding hands back, with attribute over option over default (Priority: P1)

A scene author or an integrator test writes `Option "shade" "string closuremode" ["both"]` for the scene and `Attribute "shade" "string closuremode" ["closures"]` for part of it. The effective mode of an object is the attribute if set in its scope, else the option, else `color`. `color` is today's behaviour in every byte; `both` builds the tree in addition and leaves `Ci` and `Oi` as they are; `closures` builds the tree and does none of the lighting work that produces `Ci` and `Oi`. The mode is resolved once per shaded batch from the object's attributes and the frame's options, identically for reyes, zbuffer and raytrace and identically under the interpreter and the JIT. `closuremode` governs only what a `Surface` binding consumes (D13); a `Bxdf` binding is closure-only by nature, and displacement, volume (`Interior`, `Exterior`, `Atmosphere`), imager and light shaders are untouched.

**Why this priority**: it is the issue's acceptance. It is exercised with test doubles (a backend that emits trees) until spec 036 gives RSL closure-emitting `Surface` shaders.

**Independent Test**: render the same small scene under each mode through each camera hider with a closure-emitting double and a counting light double; compare images, trees and light-execution counts.

**Acceptance Scenarios**:

1. **Given** no mention of `closuremode` anywhere, **When** a scene renders, **Then** every image, hash and recorded reference is byte-identical to the starting tree.
2. **Given** `Option "shade" "closuremode" "both"` and `Attribute "shade" "closuremode" "color"` in an object's scope, **When** the object is shaded, **Then** the attribute wins (`color`); remove the attribute and the option applies; remove the option and `color` applies.
3. **Given** `both` with a closure-emitting shader, **When** the object is shaded, **Then** its `Ci` and `Oi` equal the `color` render's byte for byte and one tree per shaded point reaches the closure consumer.
4. **Given** `closures` with a closure-emitting shader and a light that counts its executions, **When** the object is shaded, **Then** the light shader runs zero times for that object (it runs in `color` and `both`), the trees equal `both`'s trees, and the shader's non-lighting work that the tree's parameters depend on (patterns, textures, the shading normal) still runs.
5. **Given** a bad value (`Attribute "shade" "closuremode" "all"`), **When** the statement is read, **Then** a `CODE_RANGE` error names the value and the accepted spellings, and the previous setting is kept (the existing `shaderformat` pattern).
6. **Given** an `AttributeBegin` that sets the mode and an `AttributeEnd` after it, **When** later geometry is created, **Then** it sees the mode of the enclosing scope again; an object instance takes the mode current at its `ObjectInstance`, not the mode of its definition scope (RISpec §5.10, Q-R2).
7. **Given** a scene read through `orender` and written back by `ribOut`, **When** it is parsed again, **Then** the statement and its position are preserved.

---

### User Story 4 - A `Bxdf` statement binds a built-in, versioned C++ Bxdf that builds a closure tree (Priority: P2)

A scene author writes `Color [0.8 0.2 0.1]` then `Bxdf "plastic" "mat1" "float Kd" [0.6] "float roughness" [0.3]`. The Bxdf takes RISpec `plastic`'s parameters, and its base colour comes from the point's `Cs`. The renderer resolves the name in a registry of built-in Bxdfs (diffuse, plastic, a GGX conductor and a GGX dielectric; A4), binds it into the attribute's `bxdf` slot, and records that `Bxdf` is the last-bound of the pair. At shading time the Bxdf, given the shading point, produces a closure tree from its parameters. The Bxdf interface is a C function table with a semver version (the 037 convention), internal like the hider table until spec 051 opens run-time loading, and every built-in Bxdf is registered through the same table a later plug-in would use (D7, D8). Name resolution starts with this registry only; `.slo`/`.rslo` and `.oso` join as RSL 2.0 and OSL can be `Bxdf`s, and the prefixes `"sl:"` and `"osl:"` are reserved (D9).

**Why this priority**: it gives the tree a producer that needs no new language, so the BSDF interface is exercised end to end and spec 042 has materials to light; it is below the three P1 stories because the tree and the interface stand without it.

**Independent Test**: parse a scene with `Bxdf`/`Surface` statements in nested attribute blocks and read back which one governs each primitive; ask each built-in Bxdf for its tree at a point and compare with the hand-built tree of the same description.

**Acceptance Scenarios**:

1. **Given** `Bxdf "diffuse"` followed by `Surface "plastic"` in one attribute block, **When** a primitive follows, **Then** `Surface` governs (the later wins, D5); with the statements swapped, `Bxdf` governs; after `AttributeEnd` the enclosing scope's choice returns.
2. **Given** a built-in Bxdf and a shading point, **When** its tree is requested, **Then** the tree equals the hand-built tree of the same components and weights; a parameter out of range (roughness below zero; a negative `plastic` `ior`; a `dielectric` `ior` at or below zero) reports a diagnostic naming the parameter and is clamped or refused as the guide documents.
3. **Given** an unknown Bxdf name, a `"sl:"` or `"osl:"` prefixed name, or a table of an unsupported MAJOR, **When** the statement is read, **Then** a diagnostic names the reason and the name; the statement does not leave an instance key pending (the lesson of spec 057's `LightKey_NoLeakAcrossFailedLight`) and does not change the binding.
4. **Given** the statement written by `ribOut`, **When** parsed again, **Then** the statement, its handle, its parameters and its place relative to `Surface` are preserved; the Python and Lua bindings expose it as they expose `Surface`.

---

### User Story 5 - Nothing else changes, and the bound area light works in every mode (Priority: P1)

A maintainer who never writes `closuremode` or `Bxdf` must see no difference. A scene with a bound area light (spec 057) renders identically in `color` and `both`; in `closures` the Surface step skips the light, and the emitter registry, the sampler and the light shader remain intact for the integrator of specs 041/042 to use.

**Why this priority**: the "no drop-offs" rule (D28) and the project's byte-identity habit; the 057 light is the one place where the lighting work is not in the shader.

**Independent Test**: the existing suites unchanged; plus, in one scene, a `closures`-mode receiver next to a `color`-mode receiver lit by an area light: the `color` receiver's pixels are byte-identical to the same scene without the `closures` object, the `closures` object being isolated from the `color` receiver's camera, shadow, reflection and transmission rays (SC-007: it still casts shadows and appears in reflections).

**Acceptance Scenarios**:

1. **Given** the full everyday suite and the determinism sweep, **When** run on the branch, **Then** the results equal the starting tree's, with no threshold widened and no reference re-captured.
2. **Given** a bound area light and a `color` or `both` receiver, **When** it renders, **Then** it equals the 057 reference byte for byte, in the interpreter and the JIT, at any thread count.
3. **Given** a bound area light and a `closures` receiver, **When** it renders, **Then** the receiver's Surface step draws no emitter sample and runs no light shader, the light's emitter set and registry are unchanged, and, when the receiver is isolated from the other objects' camera, shadow, reflection and transmission rays (SC-007, R19), no other object's pixels differ from the same scene without the receiver.
4. **Given** a scene that uses the photon hider, **When** a photon pass runs, **Then** it is unchanged in every mode (photon emission never calls `shade()`; `closuremode` is a camera-hider surface-step setting).

---

### User Story 6 - The documents tell the truth and the next specs can read them (Priority: P3)

The reference guide, the RIB guide, DEVNOTES and a drafted CLAUDE.md addition describe the closure model, the three modes, the `Bxdf` statement and the version numbers, so specs 034, 041, 042, 036 and 049 start from written conventions.

**Independent Test**: doc checks of the `PluginAbi_DocPaths` kind (a version quoted in a guide equals the one in the header) and a check that the statement names appear in the RIB guide.

**Acceptance Scenarios**:

1. **Given** the new headers and the guides, **When** the doc test runs, **Then** the closure-ABI and Bxdf-interface versions in the guides equal the headers' history tables.
2. **Given** `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §13, **When** read, **Then** the 035 row matches what was built (§2, §9, D13 to D19) and says which items moved to 036, 041 and 042.

### Edge Cases

- **Empty tree.** A shader that emits no component under `both` produces an empty tree (not an error).
- **Zero, negative and non-finite weights.** A zero weight contributes exactly nothing; a negative or NaN weight is refused when the node is built (one diagnostic per frame, the node dropped), because a BSDF must not return negative energy.
- **Tree larger than the arena budget.** A batch whose trees do not fit reports one `CODE_LIMIT` error per frame, gives the affected points an empty tree and keeps rendering; it never writes past the arena.
- **Below-horizon and grazing directions.** Evaluation returns zero with zero density; a sampled direction below the horizon is reported as a failed sample (zero `f`, zero density, no direction), never as a valid one. The shading frame is the shading normal's; the geometric normal's side is part of the query so a consumer can reject light leaks, and the interface documents that it does not reject them itself.
- **Roughness at or near zero.** A `microfacet` component whose alpha falls below a documented floor behaves as the delta `reflection`/`refraction` it converges to (flag set, no density); there is no density that grows without bound.
- **Delta lobes in a mixed tree.** `evaluate` of a tree that has a delta lobe omits it (it has no value at a direction) and reports that the omitted lobe exists; `sample` may choose it; the probability of choosing a lobe is part of the reported density.
- **`transparent()` weight.** Presence is the weight of the `transparent` component (D16): the tree reports it as a scalar; `Oi` stays compositing opacity, and the legacy colour-valued premultiplied `Oi` of a `Surface` shader is unchanged.
- **`holdout` and `uniform_edf`.** Their categories are signal and EDF: `evaluate`/`sample` of an EDF or a signal component answer "not a scatterer" (zero, flagged); the tree reports emission and holdout through their own accessors.
- **OSL names beyond the first set** (`layer`, `subsurface_bssrdf`, `anisotropic_vdf`, `medium_vdf`, `conductor_bsdf`, `dielectric_bsdf`, `sheen_bsdf` and the rest). Not registered; looking one up is a diagnostic naming it. The registry is open to later registration without an edit of this spec's files.
- **`closuremode` set on an object whose Surface shader cannot emit closures** (every shipped shader, until 036): FR-017.
- **`closuremode` with `Surface "null"` or no surface shader.** The "no surface shader" path (`Cs`/`Os` from the attributes) is not a closure producer; the mode has nothing to act on and the object renders as in `color`, with no diagnostic (the user mis-set nothing).
- **Mode under motion, depth of field and instancing.** The tree carries no ray, time or camera state, so a tree built at one shading point may be evaluated at a different point and time (D20: spec 054's per-cell sharing): the component's frame, parameters and weight are in the tree, and `wo`/`wi` arrive at evaluation. An instance takes the mode and the Bxdf binding current at `ObjectInstance`, exactly as it takes the `Surface` binding.
  - RISpec §5.10: "All of an object's attributes are inherited at the time it is instanced, not at the time at which it is created".
  - openRender already instances this way (`CDelayedInstance::instantiate` builds the children with the instance's attributes).
  - Approved as Q-R2.
- **Thread count and hider.** The same inputs give bit-identical trees and evaluations at `-t:1` and `-t:4`, and the tree for the same shading input is the same from reyes, zbuffer and raytrace.
- **Network rendering (`-P:N`).** Trees do not cross the wire, and no consumer is installed on a server in this spec. A server parses the RIB itself and applies exactly the rules of a local render, so `-P:N` images equal local images in every mode (Q4, approved 2026-11-23; this replaces the earlier default of `color` with a warning).

## Requirements *(mandatory)*

### Functional Requirements

**Closure tree and registry**

- **FR-001**: Shading MUST define a closure tree of plain-data records (no virtual functions, no heap C++ objects) with three node kinds: component (a registered closure id, a three-channel weight, a typed parameter block that follows the record), multiply (a three-channel weight and one child) and add (two children). The layout, ids (`id >= 0` component, `-1` multiply, `-2` add), alignment and field offsets MUST mirror OSL's `ClosureComponent`/`ClosureMul`/`ClosureAdd` as documented in reference doc §2.4 (D17), without linking OSL or including an OSL header, and compile-time and run-time checks MUST pin them for spec 049's layout comparison.
- **FR-002**: Trees MUST be allocated from the shading batch's arena, with a documented lifetime (valid until the batch's arena scope ends; a consumer that needs a tree longer copies it) and no per-node free.
- **FR-003**: Shading MUST provide a flattened lobe list (type, weight, parameter offset per lobe) as the fast path for additive trees and a generic evaluator for every other tree; for every tree the two paths MUST agree within the tolerance of SC-005.
- **FR-004**: Shading MUST provide a closure registry whose entry is an id, a name, a category (BSDF, EDF, VDF or signal) and a typed parameter list, and MUST register at least `diffuse`, `oren_nayar`, `microfacet` (distributions `"ggx"` and `"beckmann"`), `reflection`, `refraction`, `transparent`, `holdout` and `uniform_edf`, with `emission` a deprecated alias of `uniform_edf` that reports once. Names and parameter lists MUST follow OSL's `stdosl.h` (reference doc §2.3, D14); registration MUST be open (specs 036 and 049 register into the same registry, with no translation layer).
- **FR-005**: A node weight MUST be a three-channel value so that the layout stays OSL-compatible (D17, D18). Every other new interface that carries a colour MUST use the `Spectrum` value type (a compile-time channel count, three now) and MUST NOT name `.r/.g/.b` outside conversions; at the C boundary a spectrum is an array and a channel count (D18). **Exception, stated plainly** (analyze E5, maintainer 2026-11-23): `OrBxdfPoint.Cs`/`Os` and `OrBxdfParam.defaultValue` are fixed `float[3]` records, RGB-3 until the spectral spec; no channel count is added to them, as node weights and OSL parameter blocks are exempt under D17.
- **FR-006**: The tree MUST report, from its components, the presence (the weight of `transparent`), the emission (EDF components) and the holdout through accessors, so a consumer never walks the tree (D16, reference doc §2.7).

**BSDF interface**

- **FR-007**: Shading MUST define a BSDF interface with `evaluate`, `sample` and `pdf` over a closure handle, following reference doc §2.6: `wo` and `wi` point away from the surface; `f` is returned without the `|cos(theta_i)|` factor; densities are per steradian; delta lobes are flagged and never given a density; the result carries forward and reverse densities and a flag set (reflection or transmission; diffuse, glossy or specular; delta); the query carries a transport mode (`Radiance` or `Importance`) and a mask of requested lobe kinds.
- **FR-008**: `sample` MUST take its random numbers from its caller (two for the direction and one for the lobe choice), draw nothing of its own and keep no state, so that results are a function of the inputs alone. 035 MUST NOT define a sampler call site; spec 042's integrator owns the dimensions. The unit tests MUST draw their numbers from spec 039's stream.
- **FR-009**: The interface MUST be a C function table (a `structSize`, a semver version, opaque handles, defined ownership and error codes) with a C++ convenience header, following spec 037's plug-in convention (`orplugin.h`), internal until spec 051 (D7, D8). It MUST start at version 1.0.0 with a version-history table in its header and stay MAJOR 1 until the path-tracing milestone completes (these are new, baseline interfaces: the "MAJOR 2 until spec 055" rule of the existing core interfaces, `hiderAbi.h` and `kRendererServicesVersion` among them, does not apply to them), and the closure layout and registry MUST carry a closure-ABI version of their own, separate from the Bxdf interface's and from `kRendererServicesVersion` (D22), which this spec leaves unchanged unless the plan finds a services change (that would be a MINOR step with a history entry).
- **FR-010**: A BSDF-category component MUST be evaluable and samplable; an EDF or signal component MUST answer "not a scatterer" (zero, flagged) rather than a value; a VDF component is registered when specs 049/050 need it, not here.
- **FR-011**: Every built-in BSDF component (`diffuse`, `oren_nayar`, `microfacet` ggx and beckmann, `reflection`, `refraction`) MUST be built on the math library of FR-012, MUST satisfy reciprocity, energy, density-normalisation and sample/density consistency (SC-004), and MUST answer the reverse density consistently with the swapped query.
- **FR-012**: Shading MUST provide a shared BRDF math library (microfacet distribution and masking terms, Fresnel terms, lobe sampling with densities, orthonormal frames, MIS weights; D19) written from the published papers, with any outside code adapted from a source whose licence has been checked against LGPL-2.1 and recorded. Classic `diffuse()`, `specular()` and `phong()` MUST NOT be rerouted through it (their bytes and images stay as they are).

**Bxdf statement and built-in Bxdfs**

- **FR-013**: The Scene Interface MUST carry a `Bxdf "<name>" ["<handle>"] [parameters]` statement (the RIS convention, D5, D9) through `RiBxdf`/`RiBxdfV`, the RIB grammar, `ribOut`, and the Python and Lua `prman` helpers (as spec 030 did for `Integrator`), binding into the `bxdf` slot of Graphics State through `bindBxdf()` and updating the Surface/Bxdf last-bound record. The preview ignores it. The handle and parameter-list grammar MUST be the one spec 034 extends with `reference` parameters.
  - The statement is refused inside an `ObjectBegin` block, where RISpec §5.10 allows no shader or attribute assignment (Q-R4). `Surface` and `Attribute` keep today's behaviour there.
  - Its `color` parameters are read with the current `ColorSamples` count and converted to RGB as `RiColor`'s are (RISpec §4.1.3).
- **FR-014**: The statement MUST resolve its name through a registry of Bxdfs that holds the built-ins (A4), registered through the same table a later plug-in would use. A name carrying `"sl:"` or `"osl:"`, an unknown name or a table of an unsupported MAJOR MUST be refused with a diagnostic that names the reason and the name. A refused statement MUST NOT leave an instance key pending and MUST NOT change the binding.
- **FR-015**: A Bxdf MUST build its closure tree for a shading point from its parameters and the point's inputs alone (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `I`, `u`, `v`, `Cs`, `Os`). It is declarative: it spawns no ray and calls no light, `illuminance`, `gather` or `occlusion` (reference doc §2.9, D15). Every parameter MUST be typed, range-checked and documented.
  - **Built-ins that model a RISpec standard shader** (Table 4.8, Appendix A; Q-R3) take that shader's parameter names and defaults:
    - `diffuse` (matte): `Kd` 1;
    - `plastic`: `Kd` .5, `Ks` .5, `roughness` .1, `specularcolor` 1, plus `ior` as an extension. `ior` defaults to 0, meaning no Fresnel term, as in the standard `plastic`; `ior > 0` enables Fresnel; a negative value is refused;
    - `conductor` (metal): `Ks` 1, `roughness` .1.
  - **Common rules**: the base colour is the point's `Cs`; `Oi = Os`; `Ka` is not a parameter (ambient has no closure lobe).
  - **All four built-ins**, `dielectric` included, use the GGX `alpha = roughness`, never squared.
  - `dielectric` has no RISpec counterpart.
- **FR-016**: Binding a `Bxdf` MUST NOT change any shader, instance key or random stream of an existing scene: a scene with no `Bxdf` statement is byte-identical in its instance keys and draws.

**`closuremode`**

- **FR-017**: `closuremode` takes `"color"`, `"both"` or `"closures"` and governs only the `Surface` binding of the shaded object. The effective mode of an object is `Attribute "shade" "string closuremode"` in its scope if set, else `Option "shade" "string closuremode"`, else `color` (issue #41; D13). The effective mode is resolved once per shaded batch, in Shading, and every hider and both execution backends see the same answer. A `Surface` shader that cannot emit closures (an instance whose backend does not declare the capability: every shipped shader until spec 036) under `both` or `closures` MUST render as `color` (the shader runs as in `color`, no tree is built or handed over) and MUST report one warning per (shader, mode) per frame naming the shader and the mode; it is never refused and never rendered black (no drop-off, D28). Spec 036 replaces this for shaders that can emit closures.
- **FR-018**: With `color` the shading step MUST be exactly the starting tree's: no tree is built, no extra call is made, no arena is touched. With `both` the step MUST build one tree per shaded point and hand it to the closure consumer (FR-021), and `Ci` and `Oi` MUST equal `color`'s byte for byte. With `closures` the step MUST build the same trees, MUST NOT run the lighting work (FR-019) and MUST NOT write the lit colour; what a camera hider writes for such an object is black colour (`Ci` zero) and the `Oi` the shader computed as opacity. It writes the same whether or not a consumer is installed; with none installed (before spec 042) there is also one warning per frame. The frame is not refused, and the shader's own `Ci` is never used.
  - The opacity is the shader's own `Oi` because RISpec §12.1 makes `Oi` the surface shader's output, and the gate leaves the non-lighting work that computes it running (Q-R5, which amends the first clarify answer's "the object's own `Os`").
  - `Os` is written only where no shader runs: a Bxdf-governed object with a consumer.
  - Spec 042 replaces this when its consumer is installed.
- **FR-019**: "The lighting work" is: running any light shader or the illumination hook for the shaded point (`illuminance`, `ambient`, `diffuse`, `specular`, `phong`, `lightsource` and the light loops behind them: `iterateLights` and `iterateBoundAreaLight`), and the GI lookups (`occlusion`, `indirectdiffuse`, `gather`) that produce `Ci`. (`indirectspecular` is not a builtin of this tree and is not named here: it is reserved for RSL 2.0 (D15), so there is nothing to gate; if spec 036 adds it, it joins this list and the gate then.) Under `closures` the shading context itself MUST skip these entry points (they behave as if no light were active), so a backend cannot run them by accident, and MUST run everything else the tree's parameters depend on (patterns, textures, displacement-derived normals, `trace()` used as a query, the shader's own arithmetic). The behaviour of the lighting builtins in `color` and `both` is the baseline of the bug-fix session's fixes (#199, #200, #197, #115, #113, #169) and MUST NOT be changed by this spec.
- **FR-020**: A bad `closuremode` value MUST give a `CODE_RANGE` error naming the value and the three spellings and keep the previous setting; an unknown token under `Option "shade"` or `Attribute "shade"` follows the existing `shaderformat` behaviour (`CODE_BADTOKEN`). The four layers (token constants and their vocabulary check, RIB parsing in `RiOptionV` and `RiAttributeV`, storage and query in `COptions`/`CAttributes`, pre-declaration in `CDeclarations`), `ribOut` and the bindings MUST agree, and attribute copies, `AttributeBegin`/`AttributeEnd` and instancing MUST carry the mode.
- **FR-021**: Shading MUST own the interface through which trees leave the shading step (a closure consumer installed on the shading context; installed per frame by spec 042's integrator and by tests). With none installed, `both` behaves as `color` and drops its trees, and `closures` follows FR-018's answer. The interface MUST hand the consumer, per shaded point, the tree, the point's geometry record (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `I`) and the point's identity (object and the sample key of spec 039), and MUST document that the tree is valid only for the call (FR-002). It MUST NOT name a hider, an integrator, a light or a renderer-core type.
- **FR-022**: The mode MUST be implemented once in Shading and MUST NOT depend on the hider that drives evaluation (constitution Principle VIII rule 3); a hider that shades through the common entry points inherits it, and a parity test MUST show the same trees from reyes, zbuffer and raytrace for the same shading input (proved with an object whose shading inputs are hider-independent, `ClosureDet_HiderParity`, and with identical point inputs through every batch shape, `ClosureDet_BatchShape`). The photon hider is not a parity subject (photon emission never calls `shade()`).
- **FR-023**: A closure-emitting backend MUST be able to plug into the `IShaderBackend` seam without a language, closure-component, LLVM or bytecode name entering the seam's members (`emitsClosures` and `executeClosures` name the capability and the entry, not a closure component, and keep their names): the seam gains at most one capability (the instance can emit closures) and one entry (execute with a tree builder), and a third-backend test double (as `ShadingSystem_ThirdBackend` registers one) is the test vehicle until spec 036. The double wraps a real compiled test shader and **delegates `execute` to the real `rslo` or `slo` backend** (it reimplements no shading code), so the gate is exercised through the real interpreter and JIT.

**Boundary and compatibility**

- **FR-024**: A bound area light (spec 057) MUST work in every mode as US5 states; its emitter registry, sampler and keyed draws MUST be unchanged by the mode of any object.
- **FR-025**: The change MUST add no line to `tests/architecture/allowlist.txt`, MUST add no entry to the registry of `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, MUST add no reference to `CRenderer` or `renderer.h` under `src/libshader`, and MUST keep `ctest -L architecture` green. The closure model, registry, BRDF library and Bxdf interface live in Shading (D30 (2), no constitution amendment); `Spectrum` lives where Shading, Transport and Hiders may all include it without a forbidden edge (Infra). If the services interface changes, `kRendererServicesVersion` MUST step with a history entry (CLAUDE.md gotcha 28); the aim is no change.
- **FR-026**: What a camera hider does for an object whose governing material is a `Bxdf` while no integrator consumes trees (before spec 042) MUST be one diagnostic per frame naming the object's Bxdf, and the object renders with the default surface; the frame is not refused. "The default surface" is RISpec's implementation default surface shader (§4.2.4), never a user-bound one (Q-R1):
  - the `defaultsurface` instance when `RiWorldBegin` bound it;
  - otherwise the built-in no-shader path (`Cs*|I.N|`, `Os`).

  Choosing it loads no shader and takes no instance key. This is an interim rule: spec 042's own clarify item "Bxdf under `Integrator classic`" settles it for good.
- **FR-027**: With no `closuremode`, no `Bxdf` statement and no consumer installed, every image, file, hash, `.rslo`/`.slo` byte, instance key and random draw MUST equal the starting tree's, on macOS and on Linux; no `.sl` file, shader reference (`ShaderCompilerImmutability`, `Shaders_BytesUnchanged`) or compiler output changes; no threshold is widened and no reference re-captured.
- **FR-028**: The closure model MUST be deterministic: trees, evaluations and samples are bit-identical across runs, at any thread count and across hiders for equal inputs; the new scenes join the determinism sweep (the entry count is stated at plan).

**Documentation**

- **FR-029**: The change MUST add a guide `DEVNOTES_DETAILS/CLOSURES.md` (the tree, the registry, the conventions of FR-007, the modes, the lifetime, the version tables), a RIB guide entry for `Bxdf` and `closuremode`, a DEVNOTES status row, a drafted CLAUDE.md addition for the maintainer, the 035 row of §13 of the reference doc, and a Hugo site page `docs/site/content/manual/reference/closures.md` (with the doc-path check `Closure_DocPaths`); and MUST state which decisions it implemented (D13 to D19, D22, and D28 in part).
- **FR-030**: No task and no requirement of this spec depends on the merge; issues close through a `Closes #NN` line the maintainer writes at merge time.

### Key Entities

- **Closure tree**: the per-shaded-point description of scattering, emission, presence and holdout, made of component, multiply and add nodes in an arena, with a flattened lobe-list form.
- **Component**: a registered closure id, a three-channel weight and a typed parameter block (the shading frame and the closure's own parameters).
- **Closure registry**: id, name, category (BSDF, EDF, VDF, signal), typed parameters; open to registration by later backends.
- **Lobe**: one entry of the flattened list (type, weight, parameter offset).
- **BSDF query and result**: the two directions, the transport mode and requested lobe kinds; `f` without cosine, forward and reverse densities per steradian, flags, and for a sample the direction and the relative index of refraction.
- **Spectrum**: the colour value type of every new interface (three channels now); not a node weight.
- **Bxdf and Bxdf table**: a built-in or later plug-in producer of a tree from parameters and a shading point; a versioned C function table.
- **closuremode**: `color`, `both` or `closures`; attribute over option over default.
- **Closure consumer**: the Shading-owned interface that receives each point's tree and geometry record; implemented by tests now and by the direct integrator in spec 042.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: With no new statement or option, every existing suite and the determinism sweep give the starting tree's results on macOS and Linux (the everyday command `-LE 'determinism-sweep|gi_determinism_long'`, the sweep, `-L architecture`): 0 changed images, 0 changed hashes, 0 new tolerances.
- **SC-002**: For a fixed set of scenes rendered through reyes, zbuffer and raytrace with a closure-emitting test double, the `both` image is byte-identical to the `color` image, and the number of trees that reach the consumer equals the shaded-point count of the `color` render, defined as the sum of `numRealVertices` over the full `shade()` calls of that render (derivative-helper vertices are not points), observed by the `cldbl` wrapper's `execute`.
- **SC-003**: Under `closures`, a counting light double executes 0 times for the object (it executes at least once per shaded point under `color` and `both`), a counting `illuminance` double likewise, the trees equal `both`'s trees, and the non-lighting work counters (pattern and texture calls) equal `color`'s.
- **SC-004**: Every BSDF component of the registry passes: reciprocity, in both transport modes where the component is non-symmetric (`f(wo, wi) = f(wi, wo)` for symmetric lobes; the documented factor for non-symmetric ones); energy conservation (directional albedo not above 1, and a white-furnace estimate within the error bound, both measured in `Importance` transport mode: `Radiance` mode multiplies a transmissive lobe by `(eta_i/eta_t)^2`, which exceeds 1 by design and is pinned by `Bsdf_ImportanceFactor`; reciprocity is checked in both modes); density normalisation (the integral of the density over the sphere equals 1 for non-delta lobes within the error bound); sample/density consistency (the sampled `f` and densities equal the independent evaluation, and a chi-square test of the sample histogram against the density passes at a fixed significance); and reverse-density consistency (the reverse density of a query equals the forward density of the swapped query). Tolerances are fixed from the estimator's own standard error, recorded in the test, and never widened to obtain a pass.
- **SC-005**: For 1000 random additive trees and the hand-built non-additive ones, the flattened evaluation equals the generic evaluator's to a relative error of 1e-6 (bit-identical where the sums are in the same order).
- **SC-006** (amended at plan, Q15, 2026-11-23): Trees, evaluations and samples are bit-identical at 1 and 4 threads and across the three camera hiders for the same inputs. This is checked in two parts:
  - **24 new determinism-sweep image entries**: 4 scenes x 3 camera hiders x 2 shader formats.
  - **6 in-process tree and BSDF checks**: `ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}`, `ClosureDet_BatchShape`, `ClosureDet_HiderParity` and `ClosureDet_BsdfRepeat`.
  - **What the sweep proves and does not** (accepted by the maintainer, 2026-11-23): the 24 sweep scenes use shipped shaders, which are never closure-capable (FR-017), so their `both` and `closures` images are `color` images (IR-017) and a `Bxdf` object renders its substitute surface (IR-026). The sweep therefore proves only the **interim behaviours**: thread, hider and shader-format identity of IR-017 and IR-026 (IR-018 needs a capable Surface under `closures` with no consumer, which no shipped shader is, so it is covered only by the in-process `Closure_Interim_IR018_*`). It cannot observe a tree, a consumer call or the lighting gate. Those are proved by the in-process checks above, the `ClosureMode_*` and `Closure_Interim_*` tests and the `cldbl` double.
  - The sweep runs `orender` images and cannot observe a tree, which is why the second part exists.
- **SC-007**: A bound-area-light scene gives pixel-identical results in `color` and `both`. In a scene whose `closures` object is **isolated** (placed outside every camera, shadow, reflection and transmission ray that reaches a `color` object: a `closures` object still casts shadows and appears in reflections, Q11, R19), the `color` objects differ from the same scene without the `closures` object in 0 pixels. The test scene (`ClosureMode_AreaLight`, and the `closures-arealight` sweep scenes) is built to that isolation; without it "0 pixels" would fail by construction (plan.md R-9).
- **SC-008**: `Bxdf` and `closuremode` statements round-trip through `ribOut` and the parser with 0 differences in statement order, handle, parameters and values over the new test scenes, and the Python and Lua helpers emit the same RIB text as the hand-written statement.
- **SC-009**: `ctest -L architecture` passes with 0 added allowlist lines and the Shading `CRenderer::` reference count does not grow.
- **SC-010**: The version numbers quoted in the guide equal those in the headers' history tables (a doc test), and the layout checks of FR-001 pass on both platforms.

## Assumptions

- **A1**: Specs 029, 030, 031, 032, 033, 037, 038, 039, 040, 048, 056 and 057 are merged into master `b9dec4e5` (the worktree was created from it). 033's `ShadingSystem`, `IShaderBackend` and `ShaderGroupBuilder` are the attachment points; 037's plug-in convention and 039's sample stream exist and are used as they are.
- **A2**: The closure model is Shading's (D30 (2)); Transport gets nothing from this spec but a consumer interface it may later implement. No integrator exists in this spec: `classic` is the only one registered, and the closure consumer is installed by tests (spec 042 installs it for `direct`).
- **A3**: Until spec 036, no shipped `Surface` shader emits closures. `closuremode` is therefore exercised with a closure-emitting test-double backend and with the built-in Bxdfs; the compiler, the interpreter and the JIT are not touched, and no `.sl`, `.rslo` or `.slo` byte changes.
- **A4**: The first built-in Bxdf set is `diffuse`, `plastic`, a GGX conductor and a GGX dielectric (confirmed at clarify, 2026-11-23). The component registry is `diffuse`, `oren_nayar`, `microfacet` (`ggx`, `beckmann`), `reflection`, `refraction`, `transparent`, `holdout`, `uniform_edf` and the deprecated `emission` alias (FR-004); Bxdfs are built on top of it.
- **A5**: `Option "shade"` is a new option family that carries `closuremode`; the existing `Option "shaderformat" "default"` is untouched. The plan names `Attribute "shade"` and `Option "shade"`; the spelling of the option token is the only open naming point.
- **A6**: A tree is built per shaded point (per lane of a batch). Sharing one tree per shading-rate cell is spec 054's decision (D20); this spec only guarantees that a tree stores no ray state, may be evaluated at a different point than it was built at, and has a stable node order. Interpolating corner closure trees under Bxdf integrators (a same-shape test, weight and parameter blending) belongs to spec 042, a downstream dependency and not 035 work; spec 054 provides only the hit-to-cell mapping.
- **A7**: New interfaces that carry a colour use `Spectrum`; node weights stay three floats (D18). Going spectral would change the layout and is out of scope.
- **A8**: The Bxdf table and the closure ABI start at **1.0.0** and stay MAJOR 1 until the path-tracing milestone completes (maintainer decision, 2026-11-23). They are the first, baseline implementations of these features; the "MAJOR 2 at milestone end" rule applies to the existing core interfaces, not to them. They are separate from `kRendererServicesVersion` (2.6.1, unchanged unless the plan finds a services change, which would be a MINOR step).
- **A9**: `closuremode` does not affect displacement, volume (`Interior`, `Exterior`, `Atmosphere`), imager or light shaders, or the photon hider; VDF closures belong to specs 049/050.
- **A10**: `ShaderGroupBuilder::connect()` stays refused and a second layer stays refused until spec 034; the `bxdf` slot holds one layer, a `CBxdfInstance` bound outside `ShaderGroupBuilder` and `ShadingSystem` (Q2, R15).
- **A11**: Run-time loading of Bxdf modules, the `.slo`/`.rslo`/`.oso` resolution of a `Bxdf` name and the `"sl:"`/`"osl:"` prefixes are later specs (051, 036, 049); the prefixes are reserved and refused here.
- **A12**: The bound-area-light draws are keyed by call site and light identity (gotcha 46), so a `closures` object that does not draw them cannot disturb the draws of other objects.
- **A13** (amended at plan, Q4, 2026-11-23): Network rendering of tree modes is not extended. A `-P:N` client sends only the RIB file name, and each server parses it and renders exactly as a local run does. `RiBxdfV` returns early on the client, as `RiSurfaceV` does. Images equal the local ones in every mode.

## Out of scope

- Any integrator, any light sampling or light service, MIS between lights and BSDFs, direct lighting from trees (specs 041, 042); path tracing and bidirectional transport (spec 050 and later), though the BSDF interface is shaped for them.
- Closure-emitting RSL (`closure color`, class-shader methods, `Ci` as a closure): spec 036 and RSL 2.0. The compiler, interpreter and JIT are not changed.
- Pattern nodes, `reference` connections, shader networks and a second layer in a group (spec 034).
- The OSL backend, `.oso`, the layout test against `oslclosure.h` (spec 049), run-time loadable Bxdf plug-ins (spec 051), the public shading runtime library (spec 052).
- Volume closures and `Interior`/`Exterior`/`Atmosphere` under closure modes (specs 049/050; spec 042's matrix decides the interim).
- GI with Bxdf materials (photon scattering, irradiance-cache `indirectdiffuse`): spec 053 (D26).
- Per-cell sharing, interpolation of corner trees and `ShadingRate` semantics (spec 054, D20).
- `layer()`, subsurface, anisotropic and medium closures and OSL's other `*_bsdf` names beyond the first set (later registrations).
- Rerouting `diffuse()`, `specular()` or `phong()` through the new math library (D19: byte-identity).
- The JIT and lighting defects #199, #200, #197, #115, #113 and #169, and the area-light follow-ups (#126, #195, #196): fixed by the bug-fix session, referenced here only as the baseline.

## Overlaps and merge-order risks (checked 2026-11-23)

- **Spec 034 (#40, Pattern/Bxdf shader networks), downstream.** 034 adds `Pattern` nodes, `reference` connections, group assembly and the lifted refusal of `connect()` and of a second layer, and builds the `Bxdf` networks on this spec's `bxdf` binding and registry. 035 binds only one registry Bxdf (the slot holds one layer, bound outside `ShaderGroupBuilder`: Q2, R15). Overlap to watch: the `Bxdf` statement grammar (name, handle, parameter list) must be the one 034 extends with `"reference ..."` parameters (FR-013). 034's brief says the statement itself lands in 035; this spec honours that.
- **Spec 041 (#80, light service), downstream.** 041 needs BSDF evaluation for MIS and a clean boundary of "the lighting work": the light-iteration entry points of FR-019 are what 041's classic adapter wraps. 035 defines the skip rule, not the light interface.
- **Spec 042 (#81, direct integrator, the milestone), downstream.** 042 implements the closure consumer and decides the "Bxdf under classic" and "Surface under `closures`" rules for good. FR-017, FR-018 and FR-026 set the interim behaviour that 042 may replace; their clarify answers should be chosen with 042's matrix cells in mind (every hider x integrator x {`Surface`, `Bxdf`} x mode cell renders or reports, D28).
- **Spec 054 (#94, ShadingRate), a parallel round-3 spec.** 054 shades per cell and, under Bxdf integrators, interpolates corner trees of equal shape and otherwise picks a corner by weight (D20). that needs a "same shape" test and a weight and parameter interpolation primitive that neither this spec nor 054 provides: spec 042 owns them (A6), and 054 provides only the hit-to-cell mapping; the tree layout keeps a stable node order so 042 can add them. Both specs touch `shading.cpp`'s surface step (054 the cell sampling, 035 the mode switch), so whichever merges second rebases.
- **Specs 036 (RSL 2.0 layer 5) and 049 (OSL), downstream.** They emit into this tree and registry; the layout checks of FR-001 are their compatibility contract.
- **The bug-fix session.** #199, #200, #197 and #115 have merged (2026-11-23); #113 and #169 are still queued (with #203, `jitParameterFinish`). They change lighting-builtin behaviour in both backends; `closures` mode only skips those entry points, so the interaction is limited to the `color`/`both` baselines. The shading area (`shading.{h,cpp}`) is re-read at each master sync (tasks.md).
- **Spec 057 (merged).** The bound light is one of the lighting-work entry points (FR-019, FR-024); `iterateBoundAreaLight` must not be edited beyond the skip.
- **Spec 043 (#82, compiler library), parallel.** No shared files expected (the compiler is not touched here).

## Discrepancies with the plan and the issue (found at the specify stage)

1. The plan's "035 copies the convention forward before 037 lands" is obsolete: spec 037 is merged, so FR-009 uses `orplugin.h` directly.
2. Issue #41's text names only the tree, the interface and `closuremode`; the plan's 035 brief also puts in the closure registry, `Spectrum`, the BRDF library, the built-in C++ Bxdfs behind a versioned interface and the minimal `Bxdf` RIB statement (034's brief says the statement lands in 035). This spec follows the plan; the maintainer may want to extend the issue body.
3. `DEVNOTES_DETAILS/PATH-TRACING_HIDER.md` (2026-06-04) proposes a path-tracing *hider* with an OSL bridge, `strcmp` hider selection and a `CBsdf` shape. It predates specs 029 and 030 and decisions D1 (hider is visibility, integrator is transport) and D17 to D19, and the plan marks it for correction in spec 049. It is used as background only (the pbrt-v3 BSDF inventory as a checklist of what a reference implementation contains), not as a design source.
4. The plan's wording "Ci/Oi lighting work" is not defined anywhere; FR-019 defines it by the entry points the shading context owns.
5. The plan says `closuremode` precedence is `Attribute "shade"` > `Option "shade"`; no `Option "shade"` exists today (the scene-wide shader format is `Option "shaderformat" "default"`), so the option family is new (A5).

## References

`DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §2.1 to §2.9 (closure idea, classic versus closure semantics, OSL names, storage, registry, BSDF conventions, material outputs, declarative closures), §7.4 (the BDPT checklist the interface is validated against), §9 (plug-in ABI and versioning), D1, D5, D7, D8, D9, D13, D14, D15, D16, D17, D18, D19, D20 (consequences only), D22, D28, D30; `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (constitution Principle VIII; Shading may depend on Graphics State, Texturing and Shader DSO Operands); `DEVNOTES_DETAILS/PATH-TRACING_HIDER.md` (background); specs 025 (the `bxdf` slot and last-bound record), 030 (registries; the `Integrator` statement as the model for a new statement), 033 (`ShadingSystem`, `IShaderBackend`, `ShaderGroupBuilder`), 037 (plug-in convention), 039 (sample stream), 057 (bound area light); CLAUDE.md gotchas 22 to 28, 33, 35, 38, 40, 41, 46 and the Attributes-system section; GitHub #41 and, as neighbours, #39, #40, #80, #81, #94, #113, #115, #169, #197, #199 and #200.
