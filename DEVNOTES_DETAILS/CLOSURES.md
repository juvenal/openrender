# The closure model, the BSDF table, `closuremode` and the `Bxdf` statement (spec 035)

Spec 035 (GitHub #41) adds the data model the material specs build on: a **closure tree** (what a shading point scatters, emits or lets
through), the BSDF table that evaluates and samples a tree, the `closuremode` option and attribute that decides whether a surface step
builds trees, and the `Bxdf` statement that binds a built-in material without a shader. Nothing in a scene that does not mention
`closuremode` or `Bxdf` changes: every image, file and hash is byte-identical to the tree before the spec.

User-facing text: `docs/site/content/manual/reference/closures.md`. Statements: `DEVNOTES_DETAILS/RIB_GUIDE.md`. Helpers:
`DEVNOTES_DETAILS/BINDINGS_GUIDE.md`. The contracts and the reasons behind each decision are in `specs/035-closure-tree-bsdf/`
(`contracts/closure-abi.md`, `contracts/closuremode.md`, `contracts/bxdf.md`, `contracts/consumer-and-backend.md`, `research.md`).

## Versions

Two interface versions, each defined by the history table of its header (`src/libshader/closures/closureAbi.h`,
`src/libshader/closures/bxdfAbi.h`); this guide only quotes them (`Closure_DocPaths` compares them with the headers' macros):

| Interface | Version | Header macro |
|---|---|---|
| closure ABI version 1.0.0 | the node layout, the registry entry, the eight built-in parameter blocks, the BSDF table's conventions | `OR_CLOSURE_ABI_VERSION` |
| Bxdf interface version 1.0.0 | the Bxdf table, its typed parameters, the point record, the allocator | `OR_BXDF_ABI_VERSION` |

Both start at 1.0.0 (the path-tracing milestone is new; the "MAJOR 2" rule of `hiderAbi.h` and `kRendererServicesVersion` covers
interfaces that predate it) and are unstable (1.x) until the milestone closes. They move independently of the RSL language version and of
`kRendererServicesVersion`, which stays 2.6.1: spec 035 adds no services member. Neither header is installed (they are not in
`OPENRENDER_PUBLIC_HEADERS`); they stay internal until specs 051 and 052.

## The closure tree

A tree is built of three node kinds, laid out like OSL's (`ClosureComponent`, `ClosureMul`, `ClosureAdd`) and built in-tree, with no OSL
dependency (D17; spec 049 adds a layout test against `oslclosure.h`):

| Struct | sizeof (LP64) | alignof | Offsets |
|---|---|---|---|
| `OrClosureComponent` | 16 | 16 | `id` 0, `w` 4; the parameter block follows at 16 |
| `OrClosureMul` | 24 | 8 | `id` 0, `weight` 4, `closure` 16 |
| `OrClosureAdd` | 24 | 8 | `id` 0, `closureA` 8, `closureB` 16 |

`id >= 0` is a registered component; `OR_CLOSURE_MUL` (-1) and `OR_CLOSURE_ADD` (-2) are the two operators. A weight is three channels
(RGB until the spectral spec; `src/common/spectrum.h`). The compatibility figure with OSL is the parameter-block offset (16); the other
numbers are printed by `Closure_Layout` for spec 049. The header is strict C17 and C++20.

The tree is **arena-allocated plain data**. `ClosureTreeBuilder` (`src/libshader/closures/closureTree.h`) makes the nodes on the batch's
arena, refuses what is wrong (a negative or non-finite weight, an unknown closure, a block of the wrong size, a `microfacet` with `eta` 0
and `refract` 1 or 2, a batch over its budget of 1 KiB per point) and reports each kind once per frame; a refused node is the empty tree.
`closureFlatten` turns a tree into its lobes (depth first, A before B, the accumulated weight times the component's own, zero-weight lobes
dropped); `closureTreePresence`, `closureTreeEmission` and `closureTreeHoldout` are the accessors, so nobody walks a tree by hand.

**Tree weights are not premultiplied by `Os`.** RISpec 3.2 Appendix A's `matte`, `metal` and `plastic` write `Ci = Os * (...)` and
`Oi = Os`; a tree carries `Kd * Cs`, `Ks * specularcolor` and so on, and the **consumer** applies `Oi = Os` and the premultiplication
(`Bxdf_Tree_NoOsPremultiply`).

### The registry and the OSL names

`registerClosure`, `findClosure`, `closureEntry` and `internClosureString` (`src/libshader/closures/closureRegistry.h`) hold eight built-ins
with fixed ids, named as OSL names them, so specs 036 and 049 register theirs with no translation layer:

| Id | Name | Category | Block |
|---|---|---|---|
| 0 | `diffuse` | BSDF | `N` |
| 1 | `oren_nayar` | BSDF | `N`, `sigma` |
| 2 | `microfacet` | BSDF | distribution (`"ggx"` or `"beckmann"`, interned), `N`, `alpha`, `eta`, `refract` (0, 1 or 2) |
| 3 | `reflection` | BSDF | `N`, `eta` |
| 4 | `refraction` | BSDF | `N`, `eta` |
| 5 | `transparent` | signal | none |
| 6 | `holdout` | signal | none |
| 7 | `uniform_edf` | EDF | `emittance` |

`emission` is a deprecated alias of `uniform_edf` (one warning per process). A `microfacet` with `eta` 0 and `refract` 0 has **no Fresnel
term** (`F = 1`); `conductor` and `plastic` (at its default `ior` 0) rely on it. A lookup of a published entry takes no lock
(`Closure_Registry_NoLock`); registration is thread safe.

## The BSDF table

`OrBsdfTable` (`prepare`, `evaluate`, `sample`, `pdf`; C++ view `src/libshader/closures/bsdf.hpp`) works on a flattened tree. The
conventions the `Bsdf_*` tests assert:

1. `wo` and `wi` point away from the surface; `f` excludes `|cos theta_i|`; densities are per steradian.
2. Every answer fills `pdfForward = p(wi | wo)` and `pdfReverse = p(wo | wi)` under the same transport mode.
3. A delta lobe is never evaluated or counted by `pdf`; it sets `OR_BSDF_HAS_DELTA`, and a sample that picks it sets `OR_BSDF_DELTA` with
   discrete lobe probabilities.
4. The lobe is chosen with probability proportional to `mean(weight) * albedo(wo)`, renormalised over the requested lobes; `pdf` is the
   mixture's, `f` the weighted sum.
5. In `RADIANCE` mode a transmissive lobe multiplies `f` by `(eta_i / eta_t)^2`; reflection is the same in both modes.
6. Below the horizon, or on failure, `f` and the densities are 0 and a sample is `OR_BSDF_FAILED`; nothing is negative or non-finite.
7. A tree with no BSDF lobe answers zero with `OR_BSDF_NOT_SCATTERER`; EDF and signal lobes never enter the mixture.
8. A `microfacet` with `alpha` below `kMicrofacetDeltaAlpha` (1e-4) behaves as `reflection`/`refraction`, a delta.
9. The table is stateless and draws nothing: `u1`, `u2` and `uLobe` come from the caller, so the same inputs give bit-identical answers on
   any thread (`ClosureDet_BsdfRepeat`).
10. A status is `OR_STATUS_OK` or `OR_STATUS_BAD_ARGUMENT`; a failed answer is still `OR_STATUS_OK`.

The microfacet math (GGX and Beckmann `D`, Smith `G`, visible-normal sampling, Fresnel) and the Oren-Nayar and diffuse lobes live in
`src/libshader/closures/bsdfMath.{h,cpp}` and `bsdfComponents.{h,cpp}`; D19 makes them the shared library later specs use. The numerical
acceptance (reciprocity, energy, chi-square against the density) is in `tests/closures/` and is stated per test from the standard
deviation, never widened. Classic `diffuse()`, `specular()` and `phong()` builtins are unchanged.

## `closuremode`

```
Option    "shade" "string closuremode" ["color" | "both" | "closures"]
Attribute "shade" "string closuremode" ["color" | "both" | "closures"]
```

`color` is today's behaviour and the default. `both` builds a tree per real point and also computes the colour; `closures` builds the
tree, closes the lighting gate and writes `Ci = 0` (the shader's own `Oi` stays). The attribute wins over the option, which wins over
`color`; a bad value is `CODE_RANGE` and keeps the previous one. The four layers of a new token are all done (gotcha 22 and the
attributes system): the token `RI_CLOSUREMODE` (`riTokens.cpp`, `riVocabulary.h`, `ri.h`); the parse in `rendererContext.cpp`; storage in
`CAttributes::closureMode` and `COptions::closureMode` (`EClosureMode`, `stateVocabulary.h`); the pre-declaration in `declarations.cpp`.
Blocks save and restore it like any attribute, including `SolidBegin` and `ObjectBegin` (RISpec 3.2 section 4; fixed by GitHub #235), and
an object instance takes the value current at `ObjectInstance`.

### The surface step

`CShadingContext::shade()` keeps its code with a small set of edits: an inline fast test (no attribute or scene mode and no governing Bxdf:
the original code runs in place, no call), `planSurfaceStep()` (the plan), the surface block run through `runSurfaceStep()` when the plan
builds trees, and `deliverClosures()` before the batch's arena is released (`src/libshader/closures/closureStep.{h,cpp}`). The plan, per
object:

| Governing | Mode | Capable | Consumer | Runs as | Trees | Ci, Oi | Diagnostic |
|---|---|---|---|---|---|---|---|
| none | any | | any | the no-shader path | none | today's | none |
| Surface | `color` | any | any | `execute` | none | today's | none |
| Surface | `both`, `closures` | no | any | `execute` | none | today's | IR-017, per (shader, mode) per frame |
| Surface | `both` | yes | none | `execute` | none | today's | none |
| Surface | `both` | yes | yes | `executeClosures`, gate open | one per real point | equal to `color`'s, byte for byte | none |
| Surface | `closures` | yes | none | `executeClosures`, gate closed | built, not delivered | `Ci = 0`, own `Oi` | IR-018, once per frame |
| Surface | `closures` | yes | yes | `executeClosures`, gate closed | one per real point | `Ci = 0`, own `Oi` | none |
| Bxdf | ignored | | none | the substitute surface, colour path | none | the substitute's | IR-026, per Bxdf name per frame |
| Bxdf | ignored | | yes | no shader; `table->build` per point | one per real point | `Ci = 0`, `Oi = Os` | none |

A "real point" is `numRealVertices`; the derivative helper vertices of a ray-traced batch are not points. Every full `shade()` applies the
plan whatever the ray type; the batch record carries `rayType` and `pathDepth`. A shipped shader is never closure capable (FR-017: no
shader changed), so with the shipped library `both` and `closures` render as `color`, with the IR-017 warning; capable surfaces arrive with
spec 036.

### The interim rules

Each is one function in `src/libshader/closures/closureInterim.{h,cpp}`, reports once per frame (the keys live in the `ClosureFrame`'s
shared set, so "once" holds across threads), is a `CODE_INCAPABLE` warning (it reaches an error handler as a warning), and is owned by the
spec that replaces it. Precedence per object: IR-026, then IR-017, then IR-018.

| Rule | Function | Replaced by |
|---|---|---|
| IR-017 | `interimSurfaceNotClosureCapable()`: a Surface that cannot emit closures renders as colour | spec 036; spec 042 |
| IR-018 | `interimClosuresWithoutConsumer()`: `closures` with no consumer: gate closed, `Ci = 0`, own `Oi` | spec 042 |
| IR-026 | `interimBxdfWithoutConsumer()` and `interimSurfaceFor()`: a governing Bxdf with no consumer renders the substitute surface | spec 042 ("Bxdf under `Integrator classic`") |

The ids appear in code, tests and this guide, never in the message text.

### The lighting gate

`CShadingState::lightingSuppressed` is set around `executeClosures` under `closures` and read at four funnels, and nowhere else:
`CShadingContext::iterateLights` (illuminance, `diffuse`, `specular`, `phong`, `lightsource()` loops, classic and bound area lights);
the three ambient sites, through one shared check; `libshader::irradianceLookup` (`occlusion()`, `indirectdiffuse()`, no ray, no cache
access); and `gather()` through `gatherHeaderBegin` and `gatherSample`. `trace()`, `transmission()`, `visibility()`, `environment()`,
`texture()` and `shadow()` are not gated: a ray traced from a `closures` surface shades its hit with the hit object's own mode. A gated
receiver draws no emitter sample, and the area-light sampler is unchanged.

### The consumer

`IClosureConsumer::consume(context, batch)` (`src/libshader/closures/closureConsumer.h`) is called once per batch that built trees, on the
thread that shaded it, before the batch's arena is released. **Lifetime rule:** everything in the `ClosureBatch` (the trees, the geometry
record, the keys) is valid only during the call; copy a tree you keep with `closureTreeCopy()`. `consume` may run concurrently on several
contexts, so an aggregating consumer synchronises itself, and it must not call back into the context's shading entry points. A consumer is
installed per context (`setClosureConsumer`) or, in a test, as the process default (`setDefaultClosureConsumer`). The interface names no
hider, integrator, light or renderer-core type. `IShaderBackend` gains `emitsClosures()` and `executeClosures()` with default bodies; the
`rslo` and `slo` backends answer "not capable", so they compile unchanged.

The ray type of a camera hit is an interim rule too: `Camera` for a ray-traced primary and `None` for a grid batch (reyes, zbuffer), until
spec 058 (GitHub #267).

## The `Bxdf` statement and the Bxdf table

```
Bxdf "name" ["handle"] "type parameter" [value] ...
```

It binds a Bxdf in the current attribute block and makes it govern: the Surface/Bxdf last-bound record (`CAttributes::governingMaterial()`)
says which of `Surface` and `Bxdf` was bound last, and that one governs; `AttributeEnd` returns the enclosing choice. It is valid wherever
an attribute is, **except inside `ObjectBegin`/`ObjectEnd`** (RISpec 3.2 section 5.10 allows no shader or attribute assignment there:
`VALID_BXDF_BLOCKS`); an instance takes the Bxdf current at `ObjectInstance`. A Bxdf takes **no instance key and no shader ordinal**: it
draws nothing, so no later shader's random stream moves because of it. A name starting with `sl:` or `osl:` is reserved for RSL 2.0 and OSL
Bxdfs (specs 036, 049); an unknown name, a refused interface MAJOR and a parameter out of range with a refusing table are errors that name
the reason and leave the binding unchanged. A `-P:N` client ignores the statement (the servers parse the RIB).

The Bxdf is a CShaderGroupRefTarget in the attributes' bxdf slot (`CBxdfInstance`, `src/libshader/closures/bxdfInstance.h`): read it with
`bxdfInstanceOf()`, a checked cast that answers `nullptr` for anything else; never with `shaderInstanceOf(attributes.bxdf())`, which is a
`static_cast` and would turn the object into a shader. A foreign object in the slot never selects the Bxdf path. The table
(`OrBxdfTable`, `src/libshader/closures/bxdfAbi.h`) names the Bxdf, types its parameters with defaults and ranges (`OR_BXDF_CLAMP` or
`OR_BXDF_REFUSE`), and `build` writes the tree of one point through the allocator it is given: declarative (no ray, no light, no gather) and
a pure function of its data and the point, so equal inputs give bit-identical trees. `registerBxdf` checks a table with
`checkPluginVersion`'s in-process overload (gotcha 27: one version check), records a refused table with its reason and refuses a duplicate
name; the four built-ins register on first use through the call graph, with no static registrar.

### The built-ins

Where a Bxdf models a RISpec 3.2 standard shader it takes that shader's parameter names and defaults. `Ka` is not a parameter (ambient is
lighting work with no lobe). **`alpha = roughness`, not its square**, in all four. The base colour is the point's `Cs` and the opacity is
`Oi = Os`; the normal in the point is `faceforward(normalize(N), I)` in camera space.

| Bxdf (models) | Parameters (default; range) | Tree |
|---|---|---|
| `diffuse` (`matte`) | `Kd` 1 (>= 0); extension `roughness` 0 ([0, 1]) | `Kd*Cs * diffuse(N)`; with `roughness > 0`, `oren_nayar(N, roughness)` |
| `plastic` (`plastic`) | `Kd` .5, `Ks` .5 (>= 0), `roughness` .1 ([0, 1]), `specularcolor` 1 (>= 0); extension `ior` **0** (>= 0, a negative one is refused) | `Kd*Cs * diffuse(N) + Ks*specularcolor * microfacet("ggx", N, roughness, ior, 0)`; `ior` 0 is no Fresnel term, `ior` above 0 the dielectric one |
| `conductor` (`metal`) | `Ks` 1 (>= 0), `roughness` .1 ([0, 1]) | `Ks*Cs * microfacet("ggx", N, roughness, 0, 0)` |
| `dielectric` | `ior` 1.5 (> 0, refused otherwise), `roughness` 0 ([0, 1]), `reflectColor` 1, `transmitColor` 1 | `reflectColor * microfacet("ggx", ..., ior, 0) + transmitColor * microfacet("ggx", ..., ior, 1)` |

An out-of-range value that clamps gives one `CODE_RANGE` warning naming the parameter; a `color` parameter is read with the current
`ColorSamples` count and converted to RGB as `RiColor`'s are (RISpec 3.2 section 4.1.3). `roughness` 0 on `dielectric` is below the alpha
floor, so the BSDF treats the pair as `reflection`/`refraction`.

### While a Bxdf governs and nothing consumes trees (IR-026)

The object renders with a **substitute surface**: the `defaultsurface` that `RiWorldBegin` bound, or the built-in no-shader path
(`Cs * |I.N|`, `Os`) when it bound none (a `Surface` was given before `WorldBegin`, or `defaultsurface` could not load). A Bxdf bound
before `WorldBegin` takes the `defaultsurface` when the world begins. **Never a user-bound Surface**: a Surface in the same block, bound
before the Bxdf, does not run. The stored used-parameter set of the attributes is the substitute's. With a consumer, no shader runs: the
trees come from `table->build`, `Ci` is 0 and `Oi` is `Os`.

**While a Bxdf governs, `surface("x", v)` from a displacement or atmosphere shader reads the Surface shader in the slot** (RISpec 3.2
section 15.8: the shader attached to the primitive), although that Surface does not run. In the full pass `shade()` prepares the slot
Surface's locals for the message and runs the substitute with its own (`Closure_Bxdf_MessagePassing`).

## Decisions implemented, and what is left

Implemented: D13 to D19 (the closure data model, its registry, the shared BSDF math), D22 (the version moves independently) and part of
D28 (the surface step and the interim rules). Left to later specs: **036** (RSL 2.0 layers: capable `Surface`s, the `sl:` Bxdfs, the
closure-emitting shaders), **041** (the direct integrator and its consumer), **042** (the integrator selection: IR-017, IR-018 and IR-026
retire there), **049** (OSL closures through the same registry), **051** and **052** (the plug-in interfaces: these headers become public),
and spec 058 (the camera-hit ray type). GitHub #126 (a JIT surface using `illuminance(P)` alone gathers no light) and #236 are open and
not touched.

## Tests

`ctest -L closures` (everything), `-L closuremode` (the statement, the plan, the gate, the area-light cases, the determinism checks),
`-L bxdf` (the table, the registry, the built-ins, the statement, IR-026, the consumer path). The test vehicle is `cldbl`
(`tests/closures/cldbl_backend.{h,cpp}`): a third shader backend that wraps a real compiled shader of `rslo` or `slo` and adds the tree a
test asks for, so the real interpreter and the real JIT run every builtin and every gate. Determinism: 24 sweep scenes
(`examples/rib/tests/closures-*`, generated by `specs/035-closure-tree-bsdf/tools/gen_scenes.py`) prove the interim behaviours across
threads, hiders, formats and `-P:2`; the closure path itself is observed by the in-process checks `ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}`,
`ClosureDet_HiderParity` (which also compares the same input through a grid and a span batch, FR-022: a section of that test, there is no `ClosureDet_BatchShape`) and `ClosureDet_BsdfRepeat` (SC-006 is stated that way, research R22).
`Closure_DocPaths` checks this guide against the headers and the other documents.
