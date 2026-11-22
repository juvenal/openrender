# CLAUDE.md draft for spec 057 (area-light geometry)

A **proposal**: no task edits `CLAUDE.md`; the maintainer applies it (Constitution VII, FR-033). The patch below is also the file
`specs/057-area-light-geometry/tools/proposed/claude-md.diff`, made against the worktree's `CLAUDE.md` at commit b0de56be
(`git apply specs/057-area-light-geometry/tools/proposed/claude-md.diff`; if master's `CLAUDE.md` has moved, re-apply by hand with
the same text). Every figure in it comes from a run or from the tree, not from memory:

| Figure | Value | Where it comes from |
|---|---|---|
| determinism sweep | 761 entries (618 plus the 143 of spec 057) | `ctest -N -L '^determinism-sweep$'` |
| default determinism set | 169 | `ctest -N -L '^determinism$'` (167 plus the two `_ThreadIdentity` tests of Phase 12) |
| `-L arealight` | 260 tests | `ctest -N -L arealight` (includes `AreaLight_DocPaths`) |
| `-L architecture` | 45 tests | `ctest -N -L architecture` |
| compiled shaders | 137 `.rslo` and 137 `.slo`, unchanged | `ls build/shaders` |
| `kRendererServicesVersion` | 2.6.0 | `rendererServices.h` |
| the quad agreement | 4.45 on the 8 x 8 block metric, threshold 8 | `validation.md` sections 25 and 26 |

The closing counts are in `validation.md` section 37 (checkpoint 11), section 38 (after master's merge) and the Phase 12 checkpoint (section 40):
macOS everyday 1683 of 1683 (1 skip), architecture 45, `-L arealight` 260, default determinism 169, full sweep 760 of 761 (the one failure, a GI photon
scene with no area light, passed alone 18 of 18); Linux everyday 1684 total with the one known failure `TextureTile_RenderByteIdentical` (#45), architecture 45,
sweep 761 of 761; the JIT-less configuration `-L arealight` 204 of 204 with the one `.slo` test disabled by design (run before Phase 12).

## What it changes

1. **A new gotcha, 46** (the numbering continues after 45): the binding and the estimator of a bound area light, which is the part
   a later spec on lights, integrators or the path tracer must not break: the identity-only slot, the registry in Geometry, the
   composite varying table swap, `Du`/`Dv` zero, the renderer applying only `V / (n p_A)` while the shader owns the cosine and the
   falloff, the exclusion by identity (and why unsupported kinds are not excluded), the FR-023 flag rule, the trim design, the
   reserved sampling sites `0xFF20` to `0xFF2F`, the failed-statement key leak, the photon path, the `.rslo` receivers of the
   tests.
2. Pointers from gotchas **10** and **11** ("not needed for a bound light").
3. **Gotcha 28**: the services version is 2.6.0 since spec 057 (the placeholder sentence "Spec 057 takes 2.6.0" becomes the entry).
4. The **Testing** list: the new `-L arealight` row, and the two stale figures (the default determinism set and the sweep).
5. **Active Technologies** and **Recent Changes**: one entry each.

## The patch

```diff
--- a/CLAUDE.md
+++ b/CLAUDE.md
@@ -377,7 +377,8 @@
 ctest --test-dir build -L plugin_abi -j4                # plug-in ABIs (spec 037): headers, versions, display host and v1 adapter, shadeop module, procedures, install layouts, doc checks
 ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4   # the everyday full run: everything except the opt-in determinism sweep and the 30-run forms
-ctest --test-dir build -L '^determinism$' -j2           # default determinism set (164): harness, tie and derivative tests, one demanding scene per family
-ctest --test-dir build -L '^determinism-sweep$' -j2     # opt-in full sweep (618 entries, ~10-12 min at -j2): before each merge, and for the Linux validation
+ctest --test-dir build -L '^determinism$' -j2           # default determinism set (169): harness, tie and derivative tests, one demanding scene per family
+ctest --test-dir build -L '^determinism-sweep$' -j2     # opt-in full sweep (761 entries, ~13 min on macOS and ~26 min on Linux at -j2): before each merge, and for the Linux validation
 ctest --test-dir build -L volume                        # spec 056: volume shader segments (the VolumeProbe_ hand-computed pixel checks, VolumeDiffer_, Parity_volume-, Determinism_volume-)
+ctest --test-dir build -L arealight -j4              # spec 057: area-light geometry (AreaSampling_*, the closed-form AreaLightProbe_*, AreaLight_ThreadIdentity*, soft-shadow Parity_/Visual_ scenes, AreaLight_QuadlightAgreement{,-slo}, AreaLight_PhotonSmoke, AreaLight_DocPaths; 200 tests, 80 of the sweep's entries carry a Determinism name too)
 ctest --test-dir build -L link                          # spec 029: libRSLRuntime self-contained, the static consumer, no workaround text; `Link_RenameAliases`, `Link_InfoRuntimeNotCoLinked` (spec 033)
 ctest --test-dir build -L registry                      # spec 030: the hider and integrator registries (lookup, collisions, pair check and refusal, parameters, the frame abort paths, test-double hider)
@@ -552,5 +553,6 @@
    rather than keeping it in a separate local. Found and worked around
    while porting `spherelight`/`quadlight` off hardcoded C++ (`hcshader.cpp`)
-   to real RSL (2026-09-19).
+   to real RSL (2026-09-19). Not needed for a light bound by `AreaLightSource`
+   (spec 057, gotcha 46): there `N` is the emitter's own normal.
 11. **`illuminate()`'s own `L` sign convention is `N.normalize(L)` for a
    Lambertian-emitting light surface, not `-N.normalize(L)`.** A
@@ -561,5 +563,6 @@
    produces a negative (clamped-to-black) `Cl` for every point. Confirmed
    by direct render testing; `shadowarea.sl`/`rayarea.sl` already use the
-   correct `N.normalize(L)` form.
+   correct `N.normalize(L)` form. A bound area light (gotcha 46) uses the same
+   form in the shipped `arealight.sl`.
 12. **`random()`/`urandom()` under the LLVM JIT were broken, now fixed —
    generalized into a full builtin-function coverage sweep (spec
@@ -813,5 +816,5 @@
 
 28. **Shading's boundary is one versioned interface; the tracer is the hider that owns the context (specs 026, 029).**
-   `CRendererServices` is version 2.5.0 (`kRendererServicesVersion`, `rendererServices.h`, with
+   `CRendererServices` is version 2.6.0 (`kRendererServicesVersion`, `rendererServices.h`, with
    `serviceTypes.h`, `tracer.h` and `shaderGlobals.h`; `getPhotonMap` returns `IPhotonLookup *`; `getCache` answers only irradiance caches
    (Core's `getCache` answers `nullptr` for a name registered as another 3D kind, #104), and Shading converts the answer with `irradianceStoreOf()`; the headers document
@@ -855,5 +858,5 @@
    `resetPhotons`, `balancePhotons`, `isModifying`, `setModifying` and `storePhoton` are replaced by `storePhotonForFrame` and `writePhotons`;
    `CIrradianceStore::lookupCached` takes an accept function and a batch view in place of a draw callback, `insertRecord` is replaced by
-   `addPending` and `clampAgainstFixed`). Spec 057 takes 2.6.0.
+   `addPending` and `clampAgainstFixed`). It is 2.6.0 since spec 057 (a MINOR step, non-breaking for implementers and doubles: `ITracer::occluded(context, ray, excludeLight)` with a default body, and `areaLightSampler.h`: `AreaLightMode`, `AreaLightInfo`, `IAreaLightSampler`, installed on a context with `CShadingContext::setAreaLightSampler()`; a context with no sampler runs every light as a classic light).
 29. **The active RI context is installed explicitly (spec 038).** `CRiInterface`'s constructor assigns nothing:
    not `renderMan`, not the diagnostic sink. `riInstallContext(ctx)` makes it the active one (and attaches the
@@ -1057,4 +1060,33 @@
    `uint`/`int`.
 
+46. **A bound area light: the binding is an identity, the estimator is the shader's, and the renderer weights by `V / (n p_A)` (spec 057).**
+   `AreaLightSource` sets `CAttributes::areaLight`, a `const CShaderGroupRefTarget *` that is only an identity (Geometry never sees a light, a shader or a
+   core type: gotcha 24); `AttributeBegin` inherits it, `AttributeEnd` and `AreaLightSource "null"` clear it, and a light statement that fails clears
+   `pendingInstanceKey` (`LightKey_NoLeakAcrossFailedLight`: a later light's draws must not depend on it). The core makes the one `CEmitterRegistry::record(o)` call
+   in `addObject`; the registry (`src/ri/geometry/emitterRegistry.{h,cpp}`, Geometry's, installed per frame by `installEmitterRegistry`) keeps the roots per
+   light, resolves them once (`std::call_once`: `collectEmitters` over the roots, a leaf cdf by area) and answers `CEmitterSet::sample(u1, u2)`, a pure function
+   of two numbers (`pdfA = 1 / totalArea`). Every primitive kind reports its leaf through `CObject::collectEmitters` (`areaSampling.{h,cpp}`): the exact triangle,
+   planar quad and quadric tables (sphere, disk, cylinder, cone under a rotation, translation and one uniform scale; `similarityScale`), else the tessellated
+   table of the true surface (`kAreaTessellationK` 8; `kQuadricTessellationK` 20 for the other quadrics, bilinear fallbacks and NURBS patches; a bicubic patch,
+   which is also every regular subdivision face, keeps 8 because it is one table per face); a **trimmed** surface's table weighs each face by its accepted
+   fraction (probes on an 8 x 8 lattice of the cell, faces below 1/8 dropped) and **every** candidate is tested with the surface's own `trimAccepts` (up to 64 hash
+   candidates per face, then the face's anchor: a draw never emits outside a trim and stays a pure function of its two numbers); a fully trimmed surface adds no
+   leaf (`numFullyTrimmed`); points, curves, implicit surfaces and DSO objects are `unsupported` (counted, reported once per light, ordinary geometry; a blobby emits, through the polygon mesh `RiBlobby` derives: `AreaLightProbe_BlobbyIrradiance_*`).
+   `CShadingContext::iterateBoundAreaLight` runs the light shader n times, 0-D, with the receiver's varying table **swapped** for a composite in which only `P`,
+   `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t` are the sampled point's (`Du`/`Dv` of them are 0) and scales each pushed `Cl` by exactly `V / (n p_A)`: **the
+   renderer adds no emitter cosine and no `1/r^2`, the shader owns both** (`arealight.sl`: `N . normalize(L) / (L . L)` inside `illuminate(P, N, PI/2)`, so the
+   light emits into the side of its normal only and its `intensity` is per unit area: divide by the area to match a light of a given area). `V` is
+   `ITracer::occluded` (ray flags `ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE`): only objects with `Attribute "visibility" "int transmission" [1]` shadow a bound light
+   (FR-023; `visibility()` itself never occludes, GitHub #169), the light's own emitter set is excluded **by identity** (`IRayContext::excludedEmitter()`, the three
+   gates in `surface.cpp`, reached by the polygon, quadric, patch and subdivision intersectors: an unsupported kind does not pass through them; the raytrace hider does not intersect points and curves at all (GitHub #195), so they cast no shadow on any light's probe, inside or outside the block, and only implicit surfaces and DSO objects could be seen blocking their own light: not rendered), and the emitters stay ordinary geometry (their
+   pixels are their own `Surface`'s). The draws are keyed (`kAreaLightSampleSite` 0xFF20, `kAreaLightShadowSite` 0xFF21, `kAreaLightPhotonSite` 0xFF22; the range
+   0xFF20-0xFF2F is reserved in `sampling.h`), so a render is identical at any thread count. The controls (`Option`/`Attribute "arealight"` `"int samples"` and
+   `"string mode"`: object over light over scene, `off` at any level wins, `fast` = max(1, ceil(n / 4)), default 16 and `full`) resolve in `areaLightControls.h`.
+   The photon hider (`photon.cpp`) draws one emitter point per photon through `CHider::sampleEmitters`, writes them into the light's globals, ignores the hook
+   operands for a bound light (the hook hands only the first element, GitHub #68) and multiplies the power by the total area; a classic light keeps the hook path.
+   The `al_surface_*` receivers of `tests/arealight/shaders` run as `.rslo` because a `.slo` surface that uses `illuminance(P)` alone gathers no light (GitHub #126).
+   `quadlight.sl` and `spherelight.sl` are deprecated by their header comments (compiled bytes unchanged: `Shaders_BytesUnchanged`). Gotchas 10 and 11 are not
+   needed for a bound light.
+   **Motion:** an emitter in `MotionBegin` is sampled at the shading point's shutter time: the raytrace hider gives every pixel sample its own time (blurred), reyes and zbuffer shade a static receiver's grid once at time 0 (`reyes.cpp`), so there the emitter acts at its shutter-open pose (GitHub #196; `AreaLight_EmitterMotion`, `_GridTime`).
+
 ## Active Technologies
 - C++20, CMake ≥3.19; `libtiff`/`libpng` (mandatory), OpenEXR (optional,
@@ -1153,5 +1185,18 @@
 - Storage: N/A; the legacy bake hashes and the `tests/unit/imageio/fixtures` references are recorded files (048-oiio-image-io)
 
+- C++20, CMake ≥3.19, `-Werror`; no new dependencies: `AreaLightSource` binds its block's geometry as the emitter through an identity on the attributes and a Geometry-owned
+  per-frame emitter registry (`CEmitterRegistry`, `CEmitterSet::sample`), the light shader runs once per sample over a composite varying table (`iterateBoundAreaLight`) and
+  the renderer scales by `V / (n p_A)`; `kRendererServicesVersion` 2.6.0 (`ITracer::occluded`, `IAreaLightSampler`) (057-area-light-geometry)
+- Storage: N/A; memory only; the probe and scene references are generated files (`specs/057-area-light-geometry/tools/gen_scenes.py`) (057-area-light-geometry)
+
 ## Recent Changes
+- 057-area-light-geometry: `AreaLightSource` binds the geometry created in its attribute block as the light's emitter (issue #152): a per-frame emitter registry in Geometry, an
+  identity-only slot on the attributes, area sampling per primitive kind (polygons, meshes, subdivision surfaces, blobbies through their derived mesh; the quadrics; bilinear and bicubic patches, patch meshes and NURBS
+  with trims, every drawn point tested against the trim; photon emission from bound lights), and a bound-light branch of `iterateLights` that runs the light shader once per sample
+  with the sampled point's light-surface globals and scales `Cl` by `V / (n p_A)` on all three camera hiders, interpreter and JIT, identical at any thread count. `Option`/`Attribute
+  "arealight"` (`samples`, `mode`); only objects with `Attribute "visibility" "int transmission" [1]` shadow a bound light. `quadlight.sl` and `spherelight.sl` are deprecated by
+  header comments (bytes unchanged); the old and the new quad form differ by 4.45 on the 8 x 8 block metric (threshold 8). A failed light statement no longer leaves its instance key
+  pending (`LightKey_NoLeakAcrossFailedLight`). `kRendererServicesVersion` 2.6.0. With no `AreaLightSource` every image is byte-identical to the starting tree; no threshold was widened and
+  no reference re-captured. The determinism sweep is 761 entries (143 new). Open and untouched: GitHub #126 (a JIT surface using `illuminance(P)` gathers no light). See `specs/057-area-light-geometry/`.
 - 048-oiio-image-io: an optional OpenImageIO backend for image input and output (`OPENRENDER_ENABLE_OIIO`, off by default; constitution 1.5.0 adds the Image I/O
   utility library `libRIBImageIO`). `Option "imageio" "backend"`, the per-call `"imageio"` parameter and `otexmake -imageio` select OpenImageIO to decode source images and to
```
