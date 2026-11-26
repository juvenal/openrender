# Draft for CLAUDE.md (spec 035)

For the maintainer. `CLAUDE.md` itself is not edited by the spec's agent. Nothing here depends on the merge.

## 1. Architecture: a "Closure model, the BSDF table, `closuremode` and the Bxdf statement (spec 035)" subsection

(after "Hider and integrator registries, the Transport domain (spec 030)", or after the area-light paragraph if that is where the newest
sections go)

```
### Closure model, the BSDF table, closuremode and Bxdf (spec 035)
A **closure tree** (`src/libshader/closures/`, closure ABI 1.0.0, `closureAbi.h`) is arena-allocated plain data laid out like OSL's
(`OrClosureComponent` 16 bytes, `OrClosureMul` and `OrClosureAdd` 24; the block offset 16 is the compatibility figure): components with a
three-channel weight and a parameter block, added and multiplied. The registry (`closureRegistry.h`) holds eight built-ins named as OSL names them
(`diffuse`, `oren_nayar`, `microfacet`, `reflection`, `refraction`, `transparent`, `holdout`, `uniform_edf`); `ClosureTreeBuilder` makes nodes on the
batch's arena and refuses a bad weight or block once per frame; `closureFlatten` and the accessors read a tree. Tree weights are **not**
premultiplied by `Os`: the consumer applies `Oi = Os`. `OrBsdfTable` (`bsdf.hpp`, the math in `bsdfMath.cpp`/`bsdfComponents.cpp`, D19) evaluates,
samples and gives the density of a flattened tree, stateless and bit-identical on any thread.

`Option`/`Attribute "shade" "closuremode"` (`color` default, `both`, `closures`; the attribute wins) is a new token through the four layers.
`CShadingContext::shade()` keeps its code with an inline fast test (no attribute or scene mode and no governing Bxdf), `planSurfaceStep()`, the
surface block through `runSurfaceStep()` when the plan builds trees, and `deliverClosures()` before the batch's arena is released
(`closureStep.{h,cpp}`); `ClosureFrame` (per frame, next to `setSamplingConfig`) holds the scene mode and the once-per-frame keys. `IClosureConsumer`
gets the trees (valid only during `consume`; `closureTreeCopy()` keeps one), `IShaderBackend::emitsClosures`/`executeClosures` have default bodies
(no shipped shader is capable, FR-017). Under `closures` the **lighting gate** (`CShadingState::lightingSuppressed`) is read at four funnels only:
`iterateLights`, the three ambient sites through one check, `irradianceLookup`, and `gather()` (`gatherHeaderBegin` and `gatherSample`).

`Bxdf "name" ["handle"] <pl>` binds a built-in Bxdf (`diffuse`, `plastic`, `conductor`, `dielectric`: Bxdf interface 1.0.0, `bxdfAbi.h`,
`bxdfRegistry`, `builtinBxdfs.cpp`) in the attributes' bxdf slot (`CBxdfInstance`); the last-bound record says whether Surface or Bxdf governs.
It is invalid inside `ObjectBegin` (`VALID_BXDF_BLOCKS`) and takes no instance key. Interim rules (`closureInterim.cpp`, each owned by the spec
that replaces it, precedence IR-026, IR-017, IR-018): IR-017 a surface that cannot emit closures renders as colour; IR-018 `closures` with no
consumer; IR-026 a governing Bxdf with no consumer renders the substitute surface. See `DEVNOTES_DETAILS/CLOSURES.md` and gotcha 49.
```

## 2. Known gotcha 49

```
49. **The Bxdf slot holds a `CBxdfInstance`, never a shader; the Bxdf step is declarative; the substitute is the world's default surface (spec 035).**
   Read the slot with `bxdfInstanceOf()` (`bxdfInstance.h`), a checked cast that answers `nullptr` for any other object (a State double, an RSL
   instance an old test binds): never `shaderInstanceOf(attributes.bxdf())`, which is a `static_cast` and would turn the Bxdf into a shader.
   `RiBxdfV` never calls `takeInstanceKey()` and a refused statement leaves `pendingInstanceKey` and the binding alone
   (`Bxdf_NoKey`, `Bxdf_Refusals`). `RiWorldBegin` keeps `worldDefaultSurface` only when `setDefaultSurface()` bound the `defaultsurface`; a Bxdf
   bound before `WorldBegin` takes it **before** `setDefaultSurface()` (its `checkParameters()` stores `usedParameters` from the substitute), and
   a pre-world `Surface` means the no-shader substitute: never a user Surface (`Closure_Interim_IR026_*`). While a Bxdf governs, `shade()` runs
   the substitute but the message-passing builtins read the **slot** Surface: `SurfaceStepPlan::messageSurface` makes `shade()` prepare the
   slot Surface's locals in `ACCESSOR_SURFACE` (not run) and the substitute's in `substituteLocals` (`Closure_Bxdf_MessagePassing`). The normal
   a Bxdf receives is `faceforward(normalize(N), I)` with `+ 0.0f` so a flipped zero is `+0` (trees are hashed byte for byte). `alpha =
   roughness`, not squared, in all four built-ins; `plastic`'s `ior` 0 means no Fresnel term. Closure tree weights carry no `Os`; the
   consumer applies it. `closuremode`'s ray type of a camera hit is interim (`Camera` on a raytrace primary, `None` on a grid batch) until
   spec 058 (#267). A lookup that matches `"sl:"`/`"osl:"` is reserved, never unknown. The z-buffer hider writes no opacity, so a test that
   reads alpha skips it.
```

## 3. Testing section, new lines

```
ctest --test-dir build -L closures                      # spec 035: the tree, the registry, the BSDF numerics, the modes, the Bxdf statement and Bxdfs, Closure_DocPaths (125 tests)
ctest --test-dir build -L closuremode                   # spec 035: closuremode as a statement and as state, the plan and the interim rules, the lighting gate, the area-light and photon cases, ClosureDet_* (59 tests)
ctest --test-dir build -L bxdf                          # spec 035: Bxdf_Layout/Registry/Tree_*, the statement tests, Closure_Interim_IR026_*, Closure_Bxdf_* (37 tests)
```

and in the sweep line "opt-in full sweep (761 entries ..." becomes "(785 entries ...", with the 24 new entries named `DeterminismSweep_closures-*`.
The counts are the lists of `ctest -N -L` on the macOS tree of the branch; the maintainer confirms them at merge.

## 4. Active Technologies (one entry)

```
- C++20 and C17 (`closureAbi.h` and `bxdfAbi.h` are strict C17), CMake ≥3.19, `-Werror`; no new dependencies: the closure tree and registry, the BSDF table and its
  math, `closuremode` (the plan, the step, the lighting gate, `IClosureConsumer`) and the `Bxdf` statement with four built-ins, all in `libshader/closures`
  (`libRSLRuntime`); closure ABI 1.0.0, Bxdf interface 1.0.0 (035-closure-tree-bsdf)
- Storage: N/A; memory only; the sweep scenes are generated files (`specs/035-closure-tree-bsdf/tools/gen_scenes.py`) (035-closure-tree-bsdf)
```

## 5. Recent Changes (one entry, newest first)

```
- 035-closure-tree-bsdf: the closure model and `closuremode`, the `Bxdf` statement (issue #41): an OSL-layout closure tree with a registry of eight
  built-ins, the BSDF table and its math (reciprocity and chi-square tested against the density, tolerances from the standard deviation, none widened),
  `Option`/`Attribute "shade" "closuremode"` (`color`, `both`, `closures`) through the four layers with the surface step, the lighting gate and
  `IClosureConsumer`, and `Bxdf "name" ["handle"]` with `diffuse`, `plastic`, `conductor` and `dielectric` (a registered table, typed parameters,
  `bxdfInstanceOf`, no instance key). Interim rules IR-017, IR-018 and IR-026 until specs 036, 041 and 042. Closure ABI 1.0.0 and Bxdf interface
  1.0.0; `kRendererServicesVersion` unchanged. With neither statement every image, file and hash is byte-identical to the starting tree; no threshold
  widened and no reference re-captured. The determinism sweep is 785 entries (24 new: 4 scenes x 3 hiders x 2 formats). Found and filed on the way:
  #235 (SolidBegin/ObjectBegin did not save attributes), #236 (`shadow("raytrace")` read uninitialised light state), #267, #269. See `specs/035-closure-tree-bsdf/`.
```

## 6. Merge notes

- (a) Master's `CLAUDE.md` already has gotcha 48 (the bug-fix session's L/Cl/Ol scope and traced-shadow guard), so 035 takes **49**; spec 054 renumbers its gotcha after it.
- (b) At the second sync, bring the bug list of the Recent Changes entry (section 5) up to the state of master then. #235 and #269 are already
  fixed on master, and so is #236 (merge 36988670: a traced `shadow()` from a surface outside `illuminance` answers a defined 0, and `shade()` zeroes `L`, `Cl` and `Ol` before the shader runs). The entry names issues by number only, with no closing keyword next to one.
- (c) Re-count the `-L closures`, `-L closuremode` and `-L bxdf` lists (the numbers in section 3) after T111 (the red option OFF) and again at the
  sync, and put the final numbers into the Testing lines.

At merge time the maintainer writes the `Closes` line for issue #41 and the spec issue in the merge message; nothing here depends on it.
