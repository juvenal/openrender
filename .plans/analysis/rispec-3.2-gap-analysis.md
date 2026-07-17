# RenderMan Spec 3.2 Compliance — Gap Analysis

Read-only audit of the full RI surface (`src/ri/ri.h`, 185 functions) against the actual render path (`CRendererContext` in `src/ri/rendererContext.cpp`, 5785 lines), the RSL compiler/runtime (`src/libshader/`), and the display/file I/O layer (`src/ri/rendererDisplay.cpp`, `src/file/`, `src/openexr/`, `src/rgbe/`). Four parallel audits covered: (1) geometric primitives/modeling, (2) graphics state/transformations/options/attributes, (3) shading system (shader types, RSL language, texture tools), (4) RIB structure/archiving/display-driver. Findings below are merged, deduplicated, and cross-checked against each other and against the existing `DEVNOTES_DETAILS/RISPEC_GAPS.md`.

**Methodology note:** "Implemented" below always means a traced consumer was found — parsed data that actually flows into tessellation, shading, or output, not just accepted-and-stored. Every "Missing"/"Partial" verdict is backed by a file:line citation to the actual stub or limitation. Where an audit's seed hypothesis turned out to be wrong (a suspected gap that's actually fine, or vice versa), that correction is called out explicitly rather than silently dropped — several of these overturn or sharpen assumptions the previous `RISPEC_GAPS.md` doc made.

---

## 1. Executive Summary

- **7 RI calls are genuine stubs** (accept parameters, do nothing or error): `RiBlobby`, `RiTrimCurve`, `RiSolidBegin`/`RiSolidEnd`, `RiGeometricRepresentation`, `RiDeformation`, `RiShadingInterpolation`, `RiArchiveRecord` (in the render path only — it works in the RIB-writer backend).
- **2 correctness bugs were found incidentally**, not just missing features: an OpenEXR writer buffer overrun for >5 output channels, and `RiArchiveBegin`/`RiArchiveEnd` writing a temp file that `RiReadArchive` can never find.
- **The existing `DEVNOTES_DETAILS/RISPEC_GAPS.md` is partly stale**: its "Interior/Exterior unimplemented due to missing CSG" claim is wrong — both shaders attach and execute today, just not as full per-point surface shaders (see §3). Its other 6 claims (Imager done, Blobby/TrimCurve/CSG/trace-subset gaps) were all independently re-confirmed.
- **RiAreaLightSource is a hidden semantic gap**: it's wired as a byte-for-byte synonym for `RiLightSource` — no code associates trailing RIB geometry with a light's emission shape, so the RISpec-standard "declare arealight, then emit geometry" pattern silently produces a point light. Real area sampling exists only via non-standard hardcoded classes reached by magic shader names (`"spherelight"`/`"quadlight"`), bypassing RSL entirely.
- **Two earlier "likely stubbed" seed hypotheses turned out to be false alarms**, worth recording so they aren't re-investigated: `RiGeometryV` (arbitrary/named geometric primitives, e.g. `"teapot"`) is fully implemented — the earlier suspicion conflated it with the unrelated (and genuinely stubbed) `RiGeometricRepresentation` attribute call. Conditional RIB (`RiIfBegin`/`RiElse`/`RiElseIf`/`RiIfEnd`) is a real, working expression-evaluating engine backed by a dedicated Bison grammar (`ifexpr.y`), not a no-op.
- **Format I/O is lopsided**: only TIFF has genuine read+write round-trip support. PNG, OpenEXR, and RGBE can all write render output but none of them can be read back in as texture input (RGBE's read codec exists but is dead code, never called).

---

## 2. Confirmed Gaps — Geometric Primitives & Modeling

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| `RiSolidBegin`/`RiSolidEnd` (CSG) | **Missing** | `rendererContext.cpp:5320-5331` — `type` param (`primitive`/`union`/`intersection`/`difference`) not even inspected; `SolidEnd` tracks no state | Blocks are transparent — geometry inside renders as if the CSG wrapper weren't there; no boolean combination happens at all. |
| `RiBlobby`/`RiBlobbyV` | **Missing** | `rendererContext.cpp:5312-5318`, `CODE_INCAPABLE` | No opcode/blend-function interpreter exists anywhere. A plausible foundation exists though: `CImplicit` (`implicitSurface.h:39-64`, reached via the unrelated `RiGeometry "implicit"` extension) already has ray-marching/SDF evaluation machinery a real Blobby implementation could build on. |
| `RiTrimCurve` | **Missing — fails silently** | `rendererContext.cpp:4080-4096`, `CODE_INCAPABLE` | Higher risk than a typical stub: a trimmed NURBS patch renders as its full untrimmed rectangular domain with no error, not a loud failure. |
| `RiGeometricRepresentation` | **Missing** | `rendererContext.cpp:2180-2188`, `CODE_INCAPABLE` | Distinct from `RiGeometryV` (which works — see below). An earlier investigation seed conflated the two because of adjacent line numbers; recorded here to prevent that mix-up recurring. |
| `RiDeformation`/`RiDeformationV` | **Missing** | `rendererContext.cpp:2911-2919`, `CODE_INCAPABLE`, "Arbitrary deformations are not currently supported" | Confirmed independently by both the geometry audit and the shading audit (it's simultaneously a geometry-primitive call and a shader-type attachment) — same stub, one gap, not two. |
| `RiGeometricApproximation` — `flatness` | **Partial** | `rendererContext.cpp:2160-2179` — only `RI_MOTIONFACTOR` (line 2172) is honored; `flatness`/`normaldeviation`/`pointdeviation` are warned-and-ignored (line 2169) | `flatness` is the RISpec-primary adaptive-tessellation quality/speed knob — commonly tuned in production RIB, currently has zero effect regardless of value. |
| Subdivision scheme: `"loop"` | **Missing** | No Loop-subdivision code path exists anywhere; only `"catmull-clark"` is handled (`rendererContext.cpp:5251-5310`, `subdivisionCreator.cpp`) | Unrecognized scheme strings error via `CODE_INCAPABLE` (`rendererContext.cpp:5269`) — loud failure, not silent. |
| Subdivision tag: `facevaryinginterpolateboundary` | **Missing — fails loudly** | No token defined anywhere (grep-confirmed zero matches); falls through to `error(CODE_BADTOKEN, "Unknown subdivision tag...")` at `subdivisionCreator.cpp:1826` | Lowest-risk gap in this category — the failure is an explicit error, not silently-wrong geometry. `crease`/`corner`/`hole`/`interpolateboundary` tags are all genuinely implemented (real geometric effect on the eigen-basis subdivision math), only this one tag is missing. |

**Confirmed working, no action needed** (recorded to prevent re-audit): all quadrics (Sphere/Cone/Cylinder/Hyperboloid/Paraboloid/Disk/Torus), all polygonal primitives (Polygon/GeneralPolygon/PointsPolygons/PointsGeneralPolygons), Patch/PatchMesh (bilinear+bicubic, all periodic/nonperiodic wrap modes), `RiBasis` (all 5 standard matrices round-trip correctly), `RiNuPatch` itself (knot/order infrastructure is real — only its trim-curve carving is missing), Points/Curves width handling, and — importantly — all three procedural mechanisms (`DelayedReadArchive`/`RunProgram`/`DynamicLoad`) are fully functional. `RiGeometryV` (arbitrary/named primitives like `"teapot"`, plus `"implicit"`/`"dlo"` DSO dispatch) is also fully implemented — do not confuse with `RiGeometricRepresentation` above.

---

## 3. Confirmed Gaps — Shading System (shader types, RSL language, texture tools)

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| `RiInterior`/`RiExterior` | **Partial — corrects `RISPEC_GAPS.md`** | Attach: `rendererContext.cpp:2007-2039`. Execution: `giOpcodes.h:85,96` (`postShader` on `gather()`-dispatched and reflection/refraction rays, `executeMisc.cpp`) | `RISPEC_GAPS.md` claims these are "logically unimplemented due to missing CSG support" — **that's inaccurate**. Both shaders attach and genuinely execute as `postShader`s on traced rays, independent of CSG. The real limitation: `shading.cpp:773`'s own comment states they never run as full per-point surface shaders with real varying/`P`/`N` access, and side/medium determination is done per-ray via a normal-dot-direction sign test rather than true CSG volume containment. This should be re-classified from "blocked on CSG" to "architecturally limited," which changes how it'd be prioritized. |
| `RiAreaLightSource`/`RiAreaLightSourceV` | **Partial — hidden semantic gap** | `rendererContext.cpp:1938-1955` is structurally byte-for-byte identical to `RiLightSourceV` (same `getShader(SL_LIGHTSOURCE)` + `addLight()`) | No code associates subsequently-declared RIB geometry with the light's emission shape — the standard RISpec pattern (declare `AreaLightSource`, then emit geometry as its shape) silently degrades to a point light. Real stochastic area sampling exists, but only through a parallel, non-RISpec mechanism: hardcoded `CSphereLight`/`CQuadLight` C++ classes (`hcshader.h/.cpp`) reachable only via magic shader names `"spherelight"`/`"quadlight"` special-cased inside `getShader()` — radius/sample-count are hardcoded constructor args, not derived from RIB geometry. Any production RIB scene using the standard area-light idiom gets silently wrong (point-light) results. |
| `RiDeformation` (shader-type aspect) | **Missing** | Same stub as §2 — one underlying gap, listed once there. |
| `RiShadingInterpolation` | **Missing** | `rendererContext.cpp:2058-2060` — literal empty body, comment "renderer always uses smooth shading interpolation" | Global no-op regardless of hider; `"constant"` mode has zero effect. |
| `bump()` RSL built-in | **Missing at runtime — grammar accepts it, nothing executes it** | Compiler accepts and rewrites the call (`rslo.cpp:1092-1093`, `rslo.y:2777`, `expression.cpp:1229-1235`), but there is no corresponding execution opcode. Confirmed by an explicit source comment: `shaderFunctions.h:2293-2296`, `// FIXME : missing functions : // bump` | This is exactly the "grammar accepts ≠ runtime executes" failure mode the audits were told to watch for. Any shader calling `bump()` fails at link/bind time, not compile time — a worse developer experience than a clean compile error. |
| `trace()` — `subset` parameter | **Missing — confirms `RISPEC_GAPS.md`** | Zero occurrences of `"subset"` anywhere in `src/ri/trace.cpp` | Ray-trace `subset` filtering doesn't exist at all; this claim in the existing gaps doc was accurate as stated. |
| `RiMakeBump`/`RiMakeBumpV` | **Partial** | `rendererContext.cpp:5395-5413` calls the **exact same** `makeTexture()` function as `RiMakeTexture` | Succeeds and produces a valid texture file, but there's no bump-specific processing (e.g. height-field→normal conversion) — functionally indistinguishable from `RiMakeTexture`. |
| `RiMakeCubeFaceEnvironment`/`V` | **Partial** | `rendererContext.cpp:5425-5433`, explicit source comment: "Partial: cube-face environment assembled; fov is accepted per spec but not applied to the projection" | Self-documented in the code already — `fov` parameter silently unused. |

**Confirmed working, no action needed:** all 5 core RSL shader-type grammar keywords (surface/displacement/light/volume/imager) compile end-to-end; `illuminance()` including the category-filtering variant (`illuminance(category, P, N, angle)`) with real light-category runtime matching and `-category` inversion; `illuminate()`/`solar()`; message-passing query functions (`surface()`/`displacement()`/`atmosphere()`/etc. — this is core RISpec 3.2 machinery, not a project-specific `->` extension as originally suspected); `noise()`/`pnoise()`/`cellnoise()`/`random()` (all arities); `Du()`/`Dv()`/`Deriv()`; `filterwidth()` (an RSL-prelude macro built on real `Du`/`Dv`/`area()` builtins, not itself a compiled opcode, but functionally works); `texture()`/`environment()`/`shadow()` with real filtered lookups; `ctransform()`/`transform()`/`vtransform()`/`ntransform()`; the `matrix` type is first-class. `RiImager` is confirmed fully implemented (validates the existing gaps doc's claim). PRMan/later-spec extensions present beyond core 3.2 and working: `gather()`, `occlusion()`, `indirectdiffuse()` — flagged as bonus, not required by 3.2. `RiMakeTexture`, `RiMakeShadow`, `RiMakeLatLongEnvironment`, `RiMakeBrickMap` (extension) are all genuinely implemented.

---

## 4. Confirmed Gaps — Graphics State, Transformations, Options, Attributes

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| `RiShadingInterpolation` | **Missing** | (Same item as §3 — listed once, cross-referenced.) |
| `RiDisplayChannelV` — `quantize`/`dither` sub-params | **Partial** | `rendererContext.cpp:1253-1314` — `warning(CODE_UNIMPLEMENT, ...)` for both | Core AOV/channel declaration mechanism (`fill`/`matte`/`filter`) works; only these two sub-parameters are inert. |
| `RiCustomDisplayV` — inline parameters | **Partial (PRMan extension scope)** | `rendererContext.cpp:1239-1251` | Display registration works; the `n`/`tokens`/`params` inline parameter list passed to the custom display function is not processed. Not core RISpec 3.2. |
| `RiContext`/`RiGetContext` — true multi-context concurrency | **Partial — architecturally sequential-only** | `ri.cpp:553-665` swaps a single global `CRiInterface*`; `RiBegin()` explicitly refuses a second context while one is live (`error(CODE_NESTING, "Already started")`); `CRenderer`'s ~40+ fields (camera matrices, clip planes, filter/DOF/shutter params, etc., per `renderer.h`) are all `static` — process-wide singleton state | Context-switching (finish-one-then-switch-to-another) works; two contexts cannot hold independent in-flight render state simultaneously. Most single-context production RIB usage is unaffected; this only matters for concurrent multi-scene use of one process. |
| `RiArchiveRecord` | **Missing in the render path** | `rendererContext.cpp:5456-5458` — literal empty body, "calls are silently ignored" | Comment/structure RIB directives are silently dropped when actually rendering. Works fully in the separate RIB-writer/passthrough backend (`CRibOut`, `ribOut.cpp:1432-1447`) — only matters for RIB re-serialization tools, not rendering itself, but worth knowing the two backends diverge here. |

**Confirmed working, no action needed:** every graphics-state block (World/Frame/Attribute/Transform/Object Begin-End, `ObjectInstance` — confirmed instances re-transform rather than share a tessellated grid, which is correct RISpec semantics, just with a re-instantiation cost worth knowing about), all 11 transformation calls including `RiTransformPoints` (a genuine coordinate-system transform, not a stub), all attributes (`RiColor`/`RiOpacity` were traced all the way into shader `Cs`/`Os` seeding; `RiMatte` was traced into both hiders' compositing logic; `RiSides`/`RiOrientation`/`RiReverseOrientation` traced into backface-culling logic across 6 files), and nearly every option (`RiProjection` defaults correctly to orthographic per spec when unset; `RiClippingPlane` — suspected stub in the seed hypothesis — is actually wired into `trace.cpp`'s ray-intersection logic; `RiColorSamples` does real N-channel matrix conversion, not RGB-only; `RiPixelFilter` implements all 5 core kernels plus PRMan extensions; `RiDisplayV`'s `RGBZ`/`RGBAZ` compound modes correctly auto-decompose into two real display entries; multiple simultaneous `Display` statements per frame are genuinely supported, not "last one wins"). `RiResource`/`RiResourceBegin`/`RiResourceEnd` (PRMan extension) work for the one resource type PRMan itself defines (`"attributes"`).

---

## 5. Confirmed Gaps — RIB Structure, Archiving, Display Driver, File I/O

| Feature | Status | Evidence | Notes |
|---|---|---|---|
| `RiArchiveBegin`/`RiArchiveEnd` — named-macro replay | **Partial, previously unflagged, more serious than it first appears** | Writes a real RIB file to `CRenderer::temporaryPath` (`rendererContext.cpp:5482-5508`), but `archivePath` (what `RiReadArchiveV`'s file lookup actually searches, `options.cpp:268`) is never unioned with `temporaryPath` anywhere in the codebase (grep-confirmed) | The PRMan extension's whole purpose — define a named in-memory RIB macro, then `ReadArchive` it by name — does not work out of the box. A user must manually add the OS temp dir to their archive search path, which isn't documented or automatic. |
| Binary RIB encoding | **Missing entirely** | No opcode table, magic-byte (0x80-0x8f) detection, or binary tokenized reader found anywhere in `rib.l`/`rib.y`/`src/ri/` | RISpec 3.2 Appendix C defines a binary encoding; only ASCII (optionally gzip-compressed) RIB is supported. |
| DSPY standard C ABI | **Missing — non-standard, incompatible plugin ABI** | openRender's display-plugin ABI (`dsply.h:38-57`: `displayStart`/`displayData`/`displayRawData`/`displayFinish`) has only 4 entry points and no `DspyImageQuery`-equivalent capability negotiation | Third-party PRMan-conformant `.so`/`.dll` display drivers built against Pixar's `ndspy.h` cannot be loaded. openRender does have its own genuine dynamic-loading plugin architecture (`rendererDisplay.cpp:789-816`, real `dlopen`/`dlsym`-equivalent, exercised in production by the TIFF/PNG/framebuffer modules) — it's just a different, incompatible ABI, not absent capability. |
| **OpenEXR write — correctness bug, not just a gap** | **Bug: buffer overrun for >5 channels** | `src/openexr/openexr.cpp` — channel names built from a hardcoded literal `"R\0G\0B\0A\0Z\0"` walked by a `chName += 2` pointer for `ns` channels, with no bounds check against the literal's 5-name length | Any render with more than 5 output samples (RGBAZ plus any custom AOV) walks past the string literal — garbage channel names / undefined behavior, not merely "extra channels unsupported." This is the one item in the whole audit that isn't "missing feature" but an active correctness defect. |
| OpenEXR / PNG / RGBE — read (texture input) | **Missing** | Zero `Imf::InputFile` usage anywhere (EXR); zero `png_read_*` calls anywhere (PNG); `RGBE_ReadHeader`/`RGBE_ReadPixels` exist in `src/rgbe/rgbe.cpp` but have zero call sites outside that file (RGBE) | Only TIFF has genuine round-trip (read+write) support; the other three formats are write-only render outputs and cannot be used as texture-conversion input. RGBE is the odd one out — the read codec is fully written, just never wired into any texture-loading path (dead code, not absent code). |

**Confirmed working, no action needed:** ASCII RIB parsing (full Flex/Bison grammar) and gzip-compressed `.rib.gz` (both read and write, via zlib); `RiDeclare`; conditional RIB (`RiIfBegin`/`RiElseIf`/`RiElse`/`RiIfEnd`) — **this was a seed hypothesis that turned out false**: it's a real, working expression-evaluating state machine backed by a dedicated grammar (`ifexpr.y`), not a no-op; `RiReadArchive` (including nested archives); all three procedural mechanisms (cross-confirmed by the geometry audit); `RiErrorHandler` and friends; TIFF read+write including float pixel formats and tiled textures.

---

## 6. Extensions vs. Core Spec — Disambiguation

These are implemented (or partially implemented) but are **not core RISpec 3.2 requirements** — their limitations shouldn't be weighted the same as a core-spec gap when prioritizing:

- `RiResource`/`RiResourceBegin`/`RiResourceEnd`, `RiCustomDisplay(V)`, `RiArchiveBegin`/`RiArchiveEnd`, `RiMakeBrickMap` — all RenderMan Pro Server extensions, postdate core 3.2.
- `gather()`, `occlusion()`, `indirectdiffuse()` in RSL — later-spec/PRMan-era shading language additions, implemented anyway (bonus, not a gap that needs closing for 3.2 compliance).
- Mitchell, Blackman-Harris, and the "step" variants of pixel filters, plus a Bessel filter — PRMan/openRender-specific filter kernels beyond the 5 core-spec kernels (box, triangle, catmull-rom, sinc, gaussian), which are themselves all correctly implemented.

---

## 7. Corrections to `DEVNOTES_DETAILS/RISPEC_GAPS.md`

| Existing claim | Verdict | Action needed |
|---|---|---|
| Imager Shaders fully implemented | Confirmed accurate | None. |
| Blobby stubbed at `rendererContext.cpp:4711` | Confirmed (line number drifted to 5312-5318 in current code, same stub) | Update line number. |
| NURBS Trim Curves stubbed at `rendererContext.cpp:3527` | Confirmed (drifted to 4080-4096) | Update line number. |
| CSG stubbed at `rendererContext.cpp:4719` | Confirmed (drifted to 5320-5331) | Update line number. |
| Raytraced motion blur — `CRaytracer` needs moving-surface support | Confirmed, already tracked in the prior hider-parity report (`.plans/analysis/hider-parity-report.md`, Spec Plan D) | No new action — cross-reference only. |
| **Interior/Exterior "logically unimplemented due to missing CSG support"** | **Inaccurate** — both attach and execute today via `gather()`/reflection-ray `postShader` dispatch, independent of CSG | Reclassify as "Partial — architecturally limited to postShader dispatch, no true per-point surface shading or CSG-based volume containment" (§3 above). |
| `trace()` doesn't filter by `subset` | Confirmed accurate | None. |

---

## 8. Full List for Prioritization (unranked — for review)

**Genuine stubs (zero implementation):**
RiSolidBegin/End (CSG) · RiBlobby · RiTrimCurve · RiGeometricRepresentation · RiDeformation · RiShadingInterpolation · RiArchiveRecord (render path) · Binary RIB encoding · Loop subdivision scheme

**Partial implementations (works, but with a named limitation):**
RiGeometricApproximation flatness · facevaryinginterpolateboundary tag · RiInterior/RiExterior (no true per-point shading) · RiAreaLightSource (degrades to point light) · bump() RSL builtin (compiles, doesn't execute) · trace() subset filtering · RiMakeBump (= RiMakeTexture, no bump processing) · RiMakeCubeFaceEnvironment (fov ignored) · RiDisplayChannelV quantize/dither · RiCustomDisplayV inline params · RiContext/RiGetContext (sequential-only) · RiArchiveBegin/End (temp file orphaned from archive search path)

**Correctness bugs (not missing features — something actively wrong):**
OpenEXR channel-name buffer overrun for >5 samples · RiArchiveBegin/End writing to a path RiReadArchive never searches

**Format I/O gaps:**
PNG/EXR/RGBE cannot be read as texture input (RGBE codec exists but is dead code)

**Architecture-level (not a single-call fix):**
DSPY plugin ABI incompatible with third-party PRMan display drivers

No prioritization or next-step recommendation is made here — this is the complete review-ready list per your request.
