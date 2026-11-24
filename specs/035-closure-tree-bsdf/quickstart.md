# Quickstart: validating spec 035

Every build and test goes through `cpujob`, at `-j4` at most. Long runs go in the background with a log (CLAUDE.md, "Builds and tests"). Never run a test binary directly.

**Paths in this guide**: the commands below assume the worktree at `/Volumes/Projects/Development/CLI/openrender-worktrees/035-closure-tree-bsdf`, with its build tree in `build/`. The contracts are cited as `contracts/<file>`, and the entities as data-model.md entries.

## 0. Prerequisites

- The coordinator or maintainer has synced master into the branch (agents do not merge), and the bug-fix session's fixes that had landed by then are included (#199, #200, #197 and #115 are merged, services 2.6.1; #113 and #203 if merged). Re-read `iterateLights`, `jitIlluminanceBegin` and `prepareAmbient` after the sync (research R23).
- The build tree is complete, so `-L architecture` can run:

```bash
~/.claude/bin/cpujob 035-build cmake --build build --config Release -j4
```

## 1. Nothing changed for a scene that uses none of it (US5, SC-001, FR-027)

```bash
~/.claude/bin/cpujob 035-everyday ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4 --output-on-failure
~/.claude/bin/cpujob 035-sweep    ctest --test-dir build -L '^determinism-sweep$' -j2 --output-on-failure   # background, with a log
~/.claude/bin/cpujob 035-arch     ctest --test-dir build -L architecture -j4 --output-on-failure
```

**Expected**:

- The failing set is the starting tree's, on macOS and on Linux (Ubuntu 24.04, GCC 13 `-Werror`, remote `cpujob -j2`).
- The sweep's old entries are unchanged, and its 24 new entries pass (R22). These prove only the interim behaviours IR-017 and IR-026 (shipped shaders are never closure-capable, so IR-018 is covered in process); the closure path is covered by the in-process tests of §2 and §3.
- `-L architecture` passes, with 0 allowlist lines added and the Shading `CRenderer::` metric not growing (SC-009).
- `Shaders_BytesUnchanged` and `ShaderCompilerImmutability` pass, and no reference is re-captured.

## 2. The closure ABI and the BSDF math (US1, US2, SC-004, SC-005, SC-010)

```bash
~/.claude/bin/cpujob 035-closures ctest --test-dir build -L closures -j4 --output-on-failure
```

| Test group | Checks | Contract |
|---|---|---|
| `Closure_Layout` | sizes, alignments and offsets of the three nodes (static and run time); the expected table is printed for spec 049 | contracts/closure-abi.md "Nodes" |
| `Closure_Registry_*` | every built-in name: one id, category and parameter list; `emission` → `uniform_edf` with one deprecation; an unregistered name (`layer`, `dielectric_bsdf`) is refused, naming it; duplicate refusal; open registration | "Registry" |
| `Closure_Flatten_*` | 1000 seeded random additive trees plus hand-built nested ones: the lobe list equals the generic evaluator bit for bit; a zero weight contributes exactly nothing; a negative or NaN weight drops the node with one diagnostic | "Building, flattening, lifetime" |
| `Closure_Arena_*` | the tree is unreachable after the batch's `memEnd`; `closureTreeCopy` survives; the per-batch budget gives an empty tree and one `CODE_LIMIT` per frame | same |
| `Closure_Accessors` | presence, emission and holdout | same |
| `Bsdf_<component>_{Reciprocity,Energy,PdfNorm,SamplePdf,ChiSquare,ReversePdf}` | for each of `diffuse`, `oren_nayar`, `microfacet` (ggx and beckmann, `refract` 0, 1 and 2), `reflection`, `refraction`, reciprocity in both transport modes where the lobe is not symmetric, energy and the white furnace in `IMPORTANCE` mode (radiance mode's `(eta_i/eta_t)^2` factor exceeds 1 by design); the tolerances come from each estimator's standard error and are written in the test | "Numerical acceptance" |
| `Bsdf_Delta_*`, `Bsdf_Horizon_*`, `Bsdf_RoughnessFloor`, `Bsdf_NotScatterer`, `Bsdf_ImportanceFactor` | the conventions 3 to 8 of the contract | "BSDF table" |
| `ClosureDet_BsdfRepeat` | bit-identical answers on 1 and 4 threads (US2 AS6) | convention 9 |
| `Closure_DocPaths` | the versions quoted in `DEVNOTES_DETAILS/CLOSURES.md` equal the headers' history tables | SC-010 |

## 3. `closuremode` through the three camera hiders (US3, US5, SC-002, SC-003, SC-007)

These tests are in-process programs in `tests/closures/`. Each registers the `cldbl` third backend (a wrapper that delegates `execute` to the real `rslo` or `slo` backend for a compiled test shader) and a consumer or light double, then runs a `RiBegin`/`RiWorldBegin` session.

| Test | Expected |
|---|---|
| `ClosureMode_Precedence` | attribute over option over `color`; removing the attribute falls back to the option, then to `color` (US3 AS2) |
| `ClosureMode_BadValue` | `CODE_RANGE` names the value and the three spellings; the previous value is kept, on both sides (AS5) |
| `ClosureMode_Scope` | `AttributeBegin`/`End` carry the mode. An object instance takes the mode and the Bxdf binding current at its `ObjectInstance`, not those of its definition scope (AS6; RISpec §5.10, Q-R2) |
| `ClosureMode_RibRoundTrip` | `ribOut` then parse again: the statements and their positions are kept (AS7) |
| `ClosureMode_Both_{reyes,zbuffer,raytrace}` | the image is byte-identical to `color`'s; the count of delivered trees equals the real shaded-point count (SC-002) |
| `ClosureMode_Closures_{reyes,zbuffer,raytrace}` | the counting light runs 0 times for the object (≥ 1 per point under `color`/`both`), and so does the counting `illuminance`; the trees equal `both`'s; the double's pattern counter equals `color`'s (SC-003) |
| `ClosureMode_Gate_{interp,jit}` | through `cldbl` delegating to the real `rslo` (interp) or `slo` (jit) backend for `cl_gate_probe.sl`: `ambient`, `diffuse`, `specular`, `phong`, `illuminance`, `occlusion`, `indirectdiffuse` and `gather` behave as gated; `trace()` still runs (FR-019) |
| `ClosureDet_ThreadIdentity_{reyes,zbuffer,raytrace}` | tree hashes at `-t:1` equal those at `-t:4` (SC-006) |
| `ClosureDet_HiderParity` | a flat constant-attribute object under a capable surface whose tree depends only on `Cs` and the wrapper's parameters: the set of distinct tree hashes is identical across reyes, zbuffer and raytrace (FR-022) |
| `ClosureDet_BatchShape` | the same point inputs through a grid, a span and a 0-D batch give the same trees (FR-022) |
| `ClosureMode_AreaLight` | a bound area light: `color` and `both` equal the spec 057 reference byte for byte, interpreter and JIT; the `closures` receiver draws no emitter sample and runs no light shader; the `color` receiver's pixels equal the scene without the `closures` receiver. The scene keeps that receiver out of the other receiver's shadow and reflection rays (R19) (US5 AS2 and AS3, SC-007) |
| `Closure_Interim_IR017_*`, `_IR018_*`, `_IR026_*` | each rule's output and its exact dedup: IR-017 once per (shader, mode) per frame; IR-018 once per frame; IR-026 once per Bxdf name per frame; never two rules for one object (R17). IR-018 writes `Ci = 0` and keeps the shader's own `Oi`: a texture cut-out survives (Q-R5). IR-026's substitute is A′ (Q-R1): `defaultsurface` in a scene with no `Surface`, and the no-shader path when a pre-world `Surface` exists, never that `Surface` |
| `Closure_Network_Loopback` | local and `-P:2` images (`test_depth_determinism --configs "||-P:2"`) are byte-identical in `color`, `both` and `closures` (Q4, R12) |
| `Closure_Network_LoopbackBxdf` | the same for a Bxdf scene, with one IR-026 diagnostic from the server |
| `ClosureMode_Photon` | a photon pass gives the same `.gpm` in every mode (US5 AS4) |

```bash
~/.claude/bin/cpujob 035-modes ctest --test-dir build -L closuremode -j4 --output-on-failure
```

## 4. The `Bxdf` statement (US4, SC-008)

| Test | Expected |
|---|---|
| `Bxdf_LastBound` | `Bxdf` then `Surface` in one block: `Surface` governs; swapped, `Bxdf` governs; after `AttributeEnd`, the enclosing choice returns (AS1) |
| `Bxdf_Tree_{diffuse,plastic,conductor,dielectric}` | the tree at a point equals the hand-built tree, with the base colour from the point's `Cs` and RISpec's defaults (`Kd`, `Ks`, `roughness` .1, `specularcolor`) for the three modelled Bxdfs; `alpha = roughness` in all four, `dielectric` included; `plastic` at its default `ior` 0 builds a `microfacet` with `eta = 0` (no Fresnel term), so its highlight weight is `Ks * specularcolor` at normal incidence, while with `ior` 1.5 the lobe carries the dielectric Fresnel term; a negative `plastic` `ior` and a `dielectric` `ior` at or below 0 refuse the statement; `Ka` gives the unknown-parameter diagnostic; other out-of-range parameters are clamped or refused per the table, naming the parameter (AS2, Q-R3) |
| `Bxdf_ColorSamples` | a `color` parameter under `ColorSamples` with n ≠ 3 is converted to RGB as `RiColor`'s is (RISpec §4.1.3) |
| `Bxdf_ObjectBlock` | `Bxdf` inside `ObjectBegin`/`ObjectEnd` is refused by `check()`; `Surface` and `Attribute` there behave as today (RISpec §5.10, Q-R4) |
| `Bxdf_Refusals` | an unknown name, `sl:`/`osl:` and a refused MAJOR each give a diagnostic naming the reason; the binding is unchanged; no instance key is pending and `shaderOrdinal` has not advanced (AS3, FR-016) |
| `Bxdf_RibRoundTrip`, `Bxdf_Bindings_{python,lua}` | the statement, handle, parameters and position are kept; the helpers emit the same text (AS4) |
| `Bxdf_Instancing` | Bxdf A bound before `ObjectBegin` (the statement is refused inside the block) and Bxdf B bound before `ObjectInstance`: B governs the instance, A does not (Q-R2, RISpec §5.10) |
| `Bxdf_Tree_NoOsPremultiply` | the tree built with `Os = 0.5` and with `Os = 1` is bit-identical: weights are not premultiplied by `Os` (B1) |
| `Closure_Interim_IR026_PreWorld` | a Bxdf bound before `WorldBegin` renders the `defaultsurface` like the no-material object (B2) |
| `Closure_Bxdf_MessagePassing` | with a Bxdf governing and a Surface in the slot, `surface("x", v)` from a displacement or atmosphere shader reads the slot's Surface (E12) |
| `Bxdf_Network` | the `RiBxdfV` of a `-P:N` client returns early (R12) |

```bash
~/.claude/bin/cpujob 035-bxdf ctest --test-dir build -L bxdf -j4 --output-on-failure
```

## 5. Render by hand

```bash
SHADERS="$(pwd)/build/shaders" ORENDERHOME="$(pwd)/openrender" DISPLAYS="$(pwd)/build/src/display" \
  ~/.claude/bin/cpujob 035-render build/src/orender/orender examples/rib/tests/closures-bxdf-interim-raytrace.rib
```

**Expected**:

- one IR-026 diagnostic per Bxdf name;
- the Bxdf-governed spheres render with the default surface (A′): as the same spheres with no material statement, in this scene with no `Surface` statement;
- the `closuremode` spheres render as `color`, with one IR-017 warning per (shader, mode).
