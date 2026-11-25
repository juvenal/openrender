# Phase 3: defects found in the approved tests when the implementation first ran them (proposal, nothing applied)

Found 2026-11-25, T075: with the production code of T062 to T074 in place, 16 of the 33 `closuremode` tests failed on Linux. Every one that fails after the
production work is a defect of the test (an assumption about the renderer that the tree does not satisfy), except one production gap that the test found (the
`gather()` gate, below, fixed in `gatherSample`). No test was changed in the worktree; the diff below was run only in the Linux tree (scratch), where the
`closuremode` label then passes 33 of 33 (Gate_jit and Gate_interp 10 runs of 10).

| # | Test(s) | What the test assumed | What the tree does | Proposed change |
|---|---|---|---|---|
| T1 | every test with a light (`mode_support.h`) | a `LightSource` made inside `AttributeBegin/End` stays on | a light is on only inside the block that made it (RISpec): after `AttributeEnd` no light is active, so `lightRuns` was 0 under `color` too | make the light in a `TransformBegin/End` block (`Translate` still positions it) |
| T2 | `ColorIdentity_*` (E7) | no `Surface` statement means no surface | `RiWorldBegin` binds a default surface (`defaultsurface`), which is not closure capable: IR-017 warns under `both`/`closures`, which is the specified behaviour for a shipped shader | `Surface "null"` for the no-surface case: attributes->surface() is then null |
| T3 | `Both_{zbuffer,reyes}`, `HiderParity` | a camera hit carries `RayType::Camera` | a rasterised grid has `RayType::None` (`shaderGlobals.h`), only the raytrace hider's primary hits are `Camera` | expect `None` for reyes and zbuffer |
| T4 | `Closures_zbuffer`, `IR018_zbuffer` | every hider writes the shader's own `Oi` into alpha | the z-buffer hider writes alpha 1 even under `color` (measured with the cut-out shader: 1 in both formats) | check the opacity for raytrace and reyes; object `Opacity` 0.5 so that the shader's 0.25 and the object's 0.5 differ |
| T5 | `ClosureMode_Scope` | `SolidBegin/End` and `ObjectBegin/End` restore attributes (RISpec section 4) | in this tree they do not restore any attribute (measured: the surface colour set inside `SolidBegin` is not restored by `SolidEnd` either, the control) | closuremode is checked to be carried like the control (restored exactly when the colour is); AttributeBegin, TransformBegin, FrameBegin stay as written. The nonconformance is pre-existing and not filed here (search first) |
| T6 | `ClosureMode_Scope`, `RibRoundTrip`, `Precedence` | `RiGetContext()` works inside a frame or a world | it is legal outside a frame or a world only ("Bad scope"), so `options()` was null inside `FrameBegin` (SEGFAULT) | `CRenderer::context->getOptions()` |
| T7 | `RibRoundTrip` | a RIB with `WorldBegin` and no `WorldEnd` can be read in process | `RiEnd` inside an open world is "Bad scope" and the session never ends | no `WorldBegin` in the RIB: the Option then the Attribute (order kept) |
| T8 | `Gate_{interp,jit}` | `shadow("raytrace", P)` from a surface is a stable not-gated value | it reads the light state (Ps, direction) that the lights leave behind; with no light run (a closed gate) it reads uninitialised memory, as it does today for a surface that calls it before any light (measured: 0, 0.0156, 0.0625, 0.97, 1 over runs, JIT) | leave `r_shadow` out of the equality, assert it stays a fraction; the other six not-gated builtins are compared and equal in the lit scene |
| T9 | `HiderParity` | the secondary hits of raytrace use another batch shape | raytrace shades camera and secondary hits as spans (shape 1 only) | the two-shape assertion is for reyes (grid + span) |
| T10 | `AreaLight_*` | an area light made in an attribute block lights what follows the block | like T1; the scenes of spec 057 end the block and call `Illuminate` | `RiIlluminate(light, RI_TRUE)` after the emitter's block |

## One production gap the tests found: `gather()` under the closed gate

The contract said the gate sets the sample count to 0 "so the body and the else run zero times, as `gather(..., 0)` does". Measured on both backends: `gather(..., 0)` runs
the loop body once (the loop control is `gatherEndAdvance`, after the first iteration), so `r_gather` was 1, not 0. Fixed in production, not in the test: the gate also
reads `lightingSuppressed` at the top of `CShadingContext::gatherSample` (shared by both backends) and every active point takes the miss path with no ray traced (body for
no point, else for all). That is a **fifth read site of the flag** (plan said four funnels): `gatherHeaderBegin` and `gatherSample` are the two halves of the `gather()` funnel.

## The declared-set reference (SceneInterface_Services), a pinned reference

`closuremode` is one new declared variable (string). It is declared first of all (before `samples`, as spec 057 and 048 did) so that no recorded variable changes its
position. `tests/unit/scene_interface/reference/declared_set.txt` changes by exactly two lines (captured with `capture_scene_interface_declared_set` into a scratch directory;
the reference file is untouched until you approve):

```
2c2
< variables 124
---
> variables 125
126a127
> v 124 closuremode type=9 container=1 storage=0 entry=- numFloats=1 numItems=1 marker=0x0
```

Declared set before 124 variables (29 globals), after 125 (29 globals).

## The test diff (applies to the six files of tests/closures; the Linux scratch ran it)

```diff
--- a/tests/closures/mode_support.h	2026-11-25 14:43:36
+++ b/tests/closures/mode_support.h	2026-11-25 15:48:16
@@ -257,10 +257,12 @@
         RiProjection("orthographic", RI_NULL);
         RiWorldBegin();
         if (scene.light) {
-            RiAttributeBegin();
+            // a transform block, not an attribute block: AttributeEnd would switch the light off again (RISpec: a light is on for the rest of the
+            // attribute block it was made in)
+            RiTransformBegin();
             RiTranslate(0, 0, 0.5f);
             RiLightSource((std::string("cl_countlight_") + scene.format).c_str(), RI_NULL);
-            RiAttributeEnd();
+            RiTransformEnd();
         }
         if (scene.ambient)
             RiLightSource("ambientlight", RI_NULL);
@@ -289,8 +291,7 @@
         }
         RiColor(colour);
         RiOpacity(opacity);
-        if (!scene.surface.empty())
-            RiSurface((scene.surface + "_" + scene.format).c_str(), RI_NULL);
+        RiSurface(scene.surface.empty() ? "null" : (scene.surface + "_" + scene.format).c_str(), RI_NULL); // "" is no surface at all (E7)
         RiPolygon(4, RI_P, (RtPointer)front, RI_NULL);
         RiAttributeEnd();
         }
--- a/tests/closures/test_mode_step.cpp	2026-11-25 14:39:20
+++ b/tests/closures/test_mode_step.cpp	2026-11-25 15:48:16
@@ -77,7 +77,8 @@
             CL_CHECK(both.trees == color.realPoints);           // one tree per real shaded point, the derivative helpers excluded
             CL_CHECK(both.realPoints == color.realPoints);
             CL_CHECK(both.warnings == 0 && color.warnings == 0);
-            CL_CHECK(both.rayTypes.count((int)RayType::Camera) == 1); // the camera hit carries its ray type (Q11)
+            // the batch carries its ray type (Q11): a camera ray's hit for the raytrace hider, None for a rasterised grid (shaderGlobals.h)
+            CL_CHECK(both.rayTypes.count((int)(std::string(hider) == "raytrace" ? RayType::Camera : RayType::None)) == 1);
             CL_CHECK(both.modes.count((int)ClosureMode::Both) == 1 && both.modes.count((int)ClosureMode::Color) == 0); // Color never reaches a consumer
             // the scene-wide spelling reaches the same plan
             SScene o = base;
@@ -114,19 +115,22 @@
                     for (int k = 0; k < 3; ++k)
                         CL_CHECK(closures.image.at(x, y, k) == 0.0f); // Ci is 0
 
-            // Oi is the shader's own, not the object's Os: cl_cutout writes 0.25 whatever Os is
-            SScene cut = sceneFor(hider, format);
-            cut.surface = "cl_cutcap";
-            cut.opacity = 1.0f;
-            SScene cutClosures = cut;
-            cutClosures.attributeMode = "closures";
-            const SRun cutRun = render(cutClosures);
-            CL_CHECK_NEAR(cutRun.image.at(3, 3, 3), 0.25, 1e-6); // alpha: the cut-out survives
-            // camerahitmode "primitive" still overrides the opacity afterwards: the primitive's Os (1)
-            SScene prim = cutClosures;
-            prim.cameraHitMode = "primitive";
-            const SRun primRun = render(prim);
-            CL_CHECK_NEAR(primRun.image.at(3, 3, 3), 1.0, 1e-6);
+            // Oi is the shader's own, not the object's Os: cl_cutout writes 0.25 whatever Os is. The z-buffer hider writes no opacity (alpha is 1
+            // for it under color too), so the opacity is checked for the hiders that have one.
+            if (hider != "zbuffer") {
+                SScene cut = sceneFor(hider, format);
+                cut.surface = "cl_cutcap";
+                cut.opacity = 0.5f; // the object's Os differs from the shader's Oi
+                SScene cutClosures = cut;
+                cutClosures.attributeMode = "closures";
+                const SRun cutRun = render(cutClosures);
+                CL_CHECK_NEAR(cutRun.image.at(3, 3, 3), 0.25, 1e-6); // alpha: the cut-out survives
+                // camerahitmode "primitive" still overrides the opacity afterwards: the primitive's Os (0.5)
+                SScene prim = cutClosures;
+                prim.cameraHitMode = "primitive";
+                const SRun primRun = render(prim);
+                CL_CHECK_NEAR(primRun.image.at(3, 3, 3), 0.5, 1e-6);
+            }
         }
     }
 
@@ -155,6 +159,7 @@
         SScene base = sceneFor(hider, "rslo");
         base.surface = "cl_cutcap";
         base.attributeMode = "closures";
+        base.opacity = 0.5f; // the shader's own Oi (0.25) differs from the object's Os
         SScene noConsumer = base;
         noConsumer.consumer = false;
         noConsumer.threads = 4;
@@ -165,7 +170,8 @@
         for (int y = 0; y < r.image.height; ++y)
             for (int x = 0; x < r.image.width; ++x) {
                 CL_CHECK(r.image.at(x, y, 0) == 0.0f && r.image.at(x, y, 1) == 0.0f && r.image.at(x, y, 2) == 0.0f); // gate closed, Ci = 0
-                CL_CHECK_NEAR(r.image.at(x, y, 3), 0.25, 1e-6);                                                   // own Oi
+                if (hider != "zbuffer") // the z-buffer hider writes no opacity
+                    CL_CHECK_NEAR(r.image.at(x, y, 3), 0.25, 1e-6);                                               // own Oi
             }
         SScene withConsumer = base;
         withConsumer.threads = 4;
--- a/tests/closures/test_mode_state.cpp	2026-11-25 14:42:13
+++ b/tests/closures/test_mode_state.cpp	2026-11-25 15:48:16
@@ -48,7 +48,7 @@
     std::vector<Diagnostic> g_diagnostics;
     void capture(RtInt code, RtInt severity, const char *message) { g_diagnostics.push_back({code, severity, message}); }
 
-    COptions *options() { return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions(); }
+    COptions *options() { return CRenderer::context->getOptions(); } // RiGetContext is legal outside a frame or a world only
     CAttributes *attributes() { return CRenderer::context->getAttributes(FALSE); }
 
     // The query a shader's attribute("shade:closuremode") reaches: find() under the category "shade"
@@ -200,15 +200,25 @@
         // the other begin-end blocks that save attributes: WorldBegin/End is a frame's; SolidBegin/End and ObjectBegin/End restore them
         RiWorldBegin();
         attributeMode("both");
+        // SolidBegin/End and ObjectBegin/End: closuremode is carried like every other attribute, whatever this tree's blocks do with attributes
+        // (they do not restore them today: the control, the surface colour, is not restored either)
+        RtColor outer = {0.1f, 0.2f, 0.3f}, inner = {0.9f, 0.8f, 0.7f};
+        RiColor(outer);
         RiSolidBegin(RI_PRIMITIVE);
+        RiColor(inner);
         attributeMode("closures");
         CL_CHECK(attributeValue() == EClosureMode::Closures);
         RiSolidEnd();
-        CL_CHECK(attributeValue() == EClosureMode::Both);
+        const bool solidRestoresColour = attributes()->surfaceColor[0] == outer[0];
+        CL_CHECK((attributeValue() == EClosureMode::Both) == solidRestoresColour);
+        attributeMode("both");
+        RiColor(outer);
         RtObjectHandle object = RiObjectBegin();
+        RiColor(inner);
         attributeMode("closures");
         RiObjectEnd();
-        CL_CHECK(attributeValue() == EClosureMode::Both);
+        const bool objectRestoresColour = attributes()->surfaceColor[0] == outer[0];
+        CL_CHECK((attributeValue() == EClosureMode::Both) == objectRestoresColour);
         (void)object;
         RiWorldEnd();
         RiEnd();
@@ -286,8 +296,8 @@
         char tmpl[] = "/tmp/closuremode_roundtrip_XXXXXX";
         const std::string dir = mkdtemp(tmpl);
         const std::string declared = dir + "/declared.rib", inlined = dir + "/inline.rib", written = dir + "/written.rib", writtenInline = dir + "/written-inline.rib", again = dir + "/again.rib";
-        writeFile(declared, "Option \"shade\" \"closuremode\" [\"both\"]\nWorldBegin\nAttribute \"shade\" \"closuremode\" [\"closures\"]\n");
-        writeFile(inlined, "Option \"shade\" \"string closuremode\" [\"both\"]\nWorldBegin\nAttribute \"shade\" \"string closuremode\" [\"closures\"]\n");
+        writeFile(declared, "Option \"shade\" \"closuremode\" [\"both\"]\nAttribute \"shade\" \"closuremode\" [\"closures\"]\n");
+        writeFile(inlined, "Option \"shade\" \"string closuremode\" [\"both\"]\nAttribute \"shade\" \"string closuremode\" [\"closures\"]\n");
         const Modes expected{EClosureMode::Both, EClosureMode::Closures};
         CL_CHECK(parseWithRenderer(declared) == expected);
         CL_CHECK(parseWithRenderer(inlined) == expected);
@@ -296,9 +306,9 @@
         parseIntoWriter(inlined, writtenInline);
         for (const std::string &path : {written, writtenInline}) {
             const std::string text = slurp(path);
-            const size_t option = text.find("Option \"shade\""), world = text.find("WorldBegin"), attribute = text.find("Attribute \"shade\"");
-            CL_CHECK(option != std::string::npos && world != std::string::npos && attribute != std::string::npos);
-            CL_CHECK(option < world && world < attribute); // the positions are kept
+            const size_t option = text.find("Option \"shade\""), attribute = text.find("Attribute \"shade\"");
+            CL_CHECK(option != std::string::npos && attribute != std::string::npos);
+            CL_CHECK(option < attribute); // the positions are kept
             CL_CHECK(text.find("closuremode") != std::string::npos && text.find("both") != std::string::npos && text.find("closures") != std::string::npos);
         }
         CL_CHECK(parseWithRenderer(written) == expected);
--- a/tests/closures/test_mode_arealight.cpp	2026-11-25 14:41:27
+++ b/tests/closures/test_mode_arealight.cpp	2026-11-25 15:48:16
@@ -27,9 +27,10 @@
         RtPoint quad[4] = {{-1, 3, 0.2f}, {1, 3, 0.2f}, {1, 3, 0.8f}, {-1, 3, 0.8f}};
         RtFloat intensity = 200.0f;
         RiAttributeBegin();
-        RiAreaLightSource((std::string("cl_arealight_") + format).c_str(), "intensity", &intensity, RI_NULL);
+        const RtLightHandle light = RiAreaLightSource((std::string("cl_arealight_") + format).c_str(), "intensity", &intensity, RI_NULL);
         RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
         RiAttributeEnd();
+        RiIlluminate(light, RI_TRUE); // the light is on only inside the block that made it, unless it is switched on outside
     }
 
     SScene receiver(const char *format, const char *mode) {
--- a/tests/closures/test_mode_det.cpp	2026-11-25 14:40:49
+++ b/tests/closures/test_mode_det.cpp	2026-11-25 15:48:16
@@ -72,13 +72,15 @@
             // the occluder must be shaded as `both` too: the scene-wide option
             s.optionMode = "both";
             const SRun r = render(s);
-            CL_CHECK(r.rayTypes.count((int)RayType::Camera) == 1);
+            // the front object's hits: a camera ray's for raytrace, a rasterised grid's (ray type None) for reyes; the occluder's: secondary
+            CL_CHECK(r.rayTypes.count((int)(std::string(hider) == "raytrace" ? RayType::Camera : RayType::None)) == 1);
             CL_CHECK(r.rayTypes.count((int)RayType::Secondary) == 1);
             CL_CHECK(!r.secondaryHashes.empty());
             const std::set<uint64_t> secondary(r.secondaryHashes.begin(), r.secondaryHashes.end());
             const std::set<uint64_t> all(r.hashes.begin(), r.hashes.end());
-            CL_CHECK(secondary.size() == 1 && all.size() == 1); // camera hits and secondary hits: one tree, the same
-            CL_CHECK(r.shapes.size() >= 2);                     // delivered through at least two batch shapes
+            CL_CHECK(secondary.size() == 1 && all.size() == 1); // camera or grid hits and secondary hits: one tree, the same
+            if (std::string(hider) == "reyes")
+                CL_CHECK(r.shapes.size() >= 2);                 // a grid and a span: the same input through two batch shapes
         }
     }
 
--- a/tests/closures/test_mode_gate.cpp	2026-11-25 15:22:30
+++ b/tests/closures/test_mode_gate.cpp	2026-11-25 15:50:21
@@ -48,9 +48,13 @@
             // traces no ray, so it reads 0 under color as well and the zero proves nothing about the gate. The JIT run carries the evidence.
             CL_CHECK(closed.outputMean.at(name) == 0.0); // and 0 when the gate is closed
         }
-        const char *open[] = {"r_trace", "r_transmission", "r_visibility", "r_environment", "r_shadow", "r_specbrdf"};
+        // not gated: equal to color's. r_shadow is left out of the equality: shadow("raytrace", P) called from a surface reads the light state
+        // (Ps, the direction of the last light) that the lights leave behind, which a closed gate never sets, and reads uninitialised memory
+        // for it as it does today for a surface that calls it before any light ran; it is only checked to stay a fraction.
+        const char *open[] = {"r_trace", "r_transmission", "r_visibility", "r_environment", "r_texture", "r_specbrdf"};
         for (const char *name : open)
             CL_CHECK(closed.outputMean.at(name) == color.outputMean.at(name)); // not gated
+        CL_CHECK(closed.outputMean.at("r_shadow") >= 0.0 && closed.outputMean.at("r_shadow") <= 1.0);
         CL_CHECK(color.outputMean.at("r_trace") > 0.0);          // the traced ray hit the occluder: the not-gated builtin did work
         CL_CHECK(closed.lightRuns == 0 && color.lightRuns > 0);
 
```


## Decisions of 2026-11-25 and what was applied

Approved and applied (worktree, staged): the declared-set reference (two lines); the gather() second half noted in `contracts/closuremode.md` and `plan.md` Q10; T1, T2, T4, T6, T7, T9, T10 as proposed;
T5 as `ClosureMode_Scope_SolidObjectRISpec` (WILL_FAIL until GitHub #235; closuremode restored after `SolidEnd` and `ObjectEnd`, the RISpec 3.2 section 4 rule; `ClosureMode_Scope` itself keeps AttributeBegin, nested blocks, TransformBegin, FrameBegin/End and the object instance);
T8 with the comment citing GitHub #236 (`r_shadow` out of the equality, "stays a fraction" kept; `r_texture`, which the lit scene shows equal, joins the six not-gated equalities).

### T3 stays open: what the contract says about the ray type of a rasterised grid

The record's `rayType` is "from the batch's ShaderGlobals" (`contracts/consumer-and-backend.md`, the `ClosureBatch` block), and `shaderGlobals.h` defines the vocabulary: *"None covers a rasterised grid or point, a displacement pass, the imager and a bare execution."* Q11 (plan.md, spec.md) says
"trees delivered for every ray type, with `rayType` in the record" and does not say that a rasterised primary grid is `Camera`. `classifyShading()` is the table the producers fill `ShaderGlobals` from and gotcha 35 forbids changing it here. So the contract says **None**: reyes and zbuffer primary grids are `RayType::None`, raytrace primary hits are `Camera`
(measured: raytrace {Camera, Secondary}, reyes {None, Secondary}).
Proposed correction of Q11 (plan.md row and spec.md row): *"One tree per real point (the raytrace derivative helpers excluded) for SC-002's count; trees delivered for every ray type, with the batch's `rayType` in the record as the producer set it (`Camera` for a raytrace primary hit, `Secondary` and `Shadow` for traced hits, `None` for a rasterised grid or point: reyes and zbuffer primaries); ..."*
Proposed test change (T3, three hunks, not applied; the three tests stay red until you decide: `ClosureMode_Both_zbuffer`, `ClosureMode_Both_reyes`, `ClosureDet_HiderParity`):
- `test_mode_step.cpp`, `sectionBoth`: `CL_CHECK(both.rayTypes.count((int)RayType::Camera) == 1)` becomes `CL_CHECK(both.rayTypes.count((int)(std::string(hider) == "raytrace" ? RayType::Camera : RayType::None)) == 1)`.
- `test_mode_det.cpp`, `sectionHiderParity`: `CL_CHECK(r.rayTypes.count((int)RayType::Camera) == 1)` becomes the same expression with `hider`.

### Warnings and gather(..., 0)

Linux builds: 0 compiler warnings in the Phase 3 build logs. macOS: 23 linker warnings, none from 035: 8 `ld: warning: ignoring duplicate libraries` (the vcpkg LLVM `.a` listed twice) and 15 `orender-wire-macos` (the Swift package: object files built for macOS 13.3, linked at 13.0).
`gather(..., 0)` running its body once: RISpec 3.2 does not define `gather()` at all (`docs/references/RISpec3_2.md` has no occurrence of it; `trace()` is the only ray-casting call it specifies), so there is no RISpec rule to conform to. The in-tree reference (doc/Documentation/SL_Functions.html, Pixie's) says "The number of rays per shading point is controlled by samples", which for 0 reads as no rays; the body running once is an artefact of the loop control (`gatherEndAdvance` after the first iteration). Not filed.


## T11 (new, found by the T077 mutation run): the derivative-helper count has no test that can fail

Mutation (4) of T077 (trees built and delivered for the derivative-offset vertices of a raytrace batch) **survived** `ClosureMode_Both_raytrace`: its probe surface uses no derivative, so a raytrace batch has no helper vertices (`numVertices == numRealVertices`) and the count cannot differ.
Proposed addition (not applied; verified on the Linux tree only): a probe that uses `area(P)` (`tests/closures/shaders/cl_deriv.sl`, wrapper `cl_dercap`), a wrapper counter `allVertices` (the sum of `numVertices`), and in `sectionBoth` for the raytrace hider: the scene has helpers (`allVertices > realPoints`), `both` delivers one tree per real point of the `color` render and the image equals `color`'s.
With the unmutated production code the test passes; with mutation (4) it fails (`FAIL test_mode_step.cpp:91: check: dBoth.trees == dColor.realPoints`).

```diff
--- a/tests/closures/cldbl_backend.h	2026-11-25 14:38:15
+++ b/tests/closures/cldbl_backend.h	2026-11-25 18:02:26
@@ -27,9 +27,10 @@
     struct SCounters {
             std::atomic<long> executes{0};     // calls of execute() the wrapper received
             std::atomic<long> realPoints{0};   // the sum of numRealVertices over those calls
+            std::atomic<long> allVertices{0};  // the sum of numVertices: more than realPoints when the batch has derivative-offset vertices
             std::atomic<long> loads{0}, instances{0};
             std::atomic<long> viaRslo{0}, viaSlo{0}; // executes delegated to the interpreter's / the JIT's backend
-            void reset() { executes = realPoints = loads = instances = viaRslo = viaSlo = 0; }
+            void reset() { executes = realPoints = allVertices = loads = instances = viaRslo = viaSlo = 0; }
     };
     SCounters &counters();
 
--- a/tests/closures/cldbl_backend.cpp	2026-11-25 18:00:25
+++ b/tests/closures/cldbl_backend.cpp	2026-11-25 18:02:26
@@ -152,6 +152,7 @@
                     CClInstance &w = static_cast<CClInstance &>(instance);
                     ++counters().executes;
                     counters().realPoints += context.currentShadingState->numRealVertices;
+                    counters().allVertices += context.currentShadingState->numVertices;
                     const IShaderBackend *real = w.wrapped->shaderDescription()->backend;
                     if (real->key()[0] == 'r')
                         ++counters().viaRslo;
--- a/tests/closures/test_mode_step.cpp	2026-11-25 16:39:58
+++ b/tests/closures/test_mode_step.cpp	2026-11-25 18:02:26
@@ -79,6 +79,18 @@
             CL_CHECK(both.warnings == 0 && color.warnings == 0);
             CL_CHECK(both.rayTypes.count((int)RayType::Camera) == 1); // the camera hit carries its ray type (Q11)
             CL_CHECK(both.modes.count((int)ClosureMode::Both) == 1 && both.modes.count((int)ClosureMode::Color) == 0); // Color never reaches a consumer
+            // a surface that uses a derivative: the raytrace hider shades derivative-offset vertices next to the real ones, and none is a point
+            // (Q11, SC-002): one tree per real point of the color render, however many vertices the batch has
+            if (std::string(hider) == "raytrace") {
+                SScene d = base;
+                d.surface = "cl_dercap";
+                const SRun dColor = render(d);
+                d.attributeMode = "both";
+                const SRun dBoth = render(d);
+                CL_CHECK(dColor.realPoints > 0 && cldbl::counters().allVertices > cldbl::counters().realPoints); // the scene has derivative helpers
+                CL_CHECK(dBoth.trees == dColor.realPoints);
+                CL_CHECK(dBoth.image == dColor.image);
+            }
             // the scene-wide spelling reaches the same plan
             SScene o = base;
             o.optionMode = "both";
--- a/tests/closures/CMakeLists.txt	2026-11-25 16:39:58
+++ b/tests/closures/CMakeLists.txt	2026-11-25 18:02:26
@@ -160,6 +160,7 @@
 cl_shader(cl_count_light)
 cl_shader(cl_cutout)
 cl_shader(cl_lit)
+cl_shader(cl_deriv)
 # A cldbl shader file is one line "<wrapped shader> <rslo|slo> [kind]" and sits beside the compiled wrapped shader (a shipped shader, such
 # as arealight, is found on the SHADERS path). The kind is what the wrapper emits (cldbl_backend.h): none (not closure capable), diffuse
 # or constdiffuse. cl_wrapper(<name> <wrapped> [kind]) writes <name>_rslo.cldbl and <name>_slo.cldbl.
@@ -176,6 +177,7 @@
 cl_wrapper(cl_gatecapconst cl_gate_probe constdiffuse)
 cl_wrapper(cl_cutcap cl_cutout diffuse)
 cl_wrapper(cl_lit cl_lit)
+cl_wrapper(cl_dercap cl_deriv diffuse)
 cl_wrapper(cl_litcap cl_lit diffuse)
 cl_wrapper(cl_countlight cl_count_light)
 cl_wrapper(cl_arealight arealight)
```
