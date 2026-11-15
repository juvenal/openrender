# Feature Specification: Area Light Geometry

**Feature Branch**: `057-area-light-geometry`
**Created**: 2026-11-13
**Status**: Plan approved 2026-11-14 and tasks approved 2026-11-14 (analyze fixes applied 2026-11-15); implementation not started
**Input**: GitHub issue #152 (new capability, layer L5, "Spec 057: AreaLightSource binds its geometry as the emitter; area sampling and light-surface globals (RISpec §4.2.3)"); the maintainer's plan of 2026-11-05 (section "Spec 057 - area-light-geometry", placed before 041, parallel to 033 and 035); `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §2.7, §4, §5.6, §6.2 and decision D10 (D1 and D28 as context). Base: `master` at `278b0394`, with specs 029 (hider/shading separation), 030 (hider and integrator registry, `hcshader` retired), 031 (geometry hider-agnostic), 039 (sampling foundation) and 056 (volume shader segments) merged.

## Clarifications

### Session 2026-11-13

The maintainer answered the specify stage's open questions and decisions. Encoded below; the affected requirements carry the same wording.

- Q: Which primitive kinds are emitters, and in what order? -> A: Polygons, polygon meshes and subdivision surfaces are P1; the quadrics are P2; bilinear and bicubic patches, patch meshes and NURBS are P3 and the plan may cut P3. Points, curves, blobbies and implicit surfaces are out of scope and report once (FR-009, A12).
- Q: Decision A, who owns the emitter-side cosine and the `1/r^2` falloff? -> A: The renderer applies only the `1/p_A` factor. The light shader keeps its own emitter cosine and distance falloff, as the shipped `arealight.sl` does. Existing light shaders therefore stay correct unchanged and no shader hash or `.rslo`/`.slo` reference is re-captured (FR-012, FR-021).
- Q: What does `Attribute "arealight" "mode" "fast"` mean? -> A: Fewer samples, with the occlusion test still performed (FR-018).
- Q: How do the scene, light and object levels combine? -> A: Most specific wins: object over light over scene; `off` at any level turns the light off for that receiver (FR-018).
- Q: FR-023, default occluder visibility and emitter self-exclusion? -> A: Option B (the maintainer, after the probe): an occluder casts an area-light shadow only when it carries `Attribute "visibility" "int transmission" [1]`; documented; no change to existing images or to the shadow test; `transmission 0` stays the opt-out. The emitter is excluded from its own shadow rays by the identity of the emitter set, not by a ray bias (FR-017, FR-023).
- Q: A light bound to several emitters? -> A: An emitter is chosen in proportion to its area, so the density is `1/(total area)` (FR-006, FR-012).
- Q: Does a bound light illuminate its own emitter primitives? -> A: No, they are skipped, consistent with FR-017's self-exclusion (FR-011).
- Q: FR-025, what does the camera see of a camera-visible emitter? -> A: Option A: only its own `Surface` shader's result, as today, byte-identical for existing scenes. No automatic radiance, no attribute (FR-025).
- Q: Decision B, `spherelight.sl` versus a sphere emitter? -> A: The old-versus-new agreement claim is for the quad only. The sphere is documented as a model difference (FR-021, FR-022, US9).
- Coordination, unchanged and restated: spec 033 merges first; 057 keeps its changes to shader loading and light-instance creation minimal; 057 owns the `pendingInstanceKey` leak fix and its test, and 033 moves that code unchanged (FR-004, FR-032).
- Note: GitHub #158, #159, #160, #161, #164, #165 and #166 were closed by mistake on 2026-11-13 by a keyword in a merge message and have been reopened. Whatever this document says about open issues counts them as open (Overlaps).

## User Scenarios & Testing *(mandatory)*

### Measured starting point (2026-11-13, master `278b0394`, by reading the source; nothing was built or rendered for this stage)

Every line below was read in the tree, not recalled from the plan.

- `CRendererContext::RiAreaLightSourceV` (`src/ri/render/rendererContext.cpp:2430-2454`) is `RiLightSourceV` (`:2403-2428`) without the debug log line: it loads the shader, calls `attributes->addLight()` and returns the instance. Geometry inside the block is ordinary geometry and is not tied to the light. The RIB writer (`ribOut.cpp:613`), the parser entry points (`ri.cpp`, `rib.y`, `rib.l`) and the Python and Lua bindings already carry `AreaLightSource`, so no new statement is needed.
- `CAttributes` (`src/ri/state/attributes.h`) has the active-light list (`lightSources`, copied in order on clone) and no notion of a current area light. `AttributeEnd` therefore has nothing to clear.
- `CShadingContext::iterateLights` (`src/libshader/shading/shading.cpp:2006-2074`) copies the shading point into `Ps`, the surface normal into the state's `Ns`, and runs each active light once with the surface's own `varying` arrays. A light shader sees the surface's `P`, `N` and `u/v`, not a light surface's. The 056 comment at `:2036` already notes that "an area light writes its own normal into `N`", which is the workaround CLAUDE.md gotcha 10 describes: `quadlight.sl` and `arealight.sl` write a normal into the global `N` themselves because nothing supplies one.
- `CSurface::samplePoint(u, v, P, Ng)` (`src/ri/geometry/object.cpp:452`) evaluates a surface at a parameter pair. It has no area measure and no pdf: uniform `(u, v)` is not uniform by area on a mesh, a subdivision surface, a sphere or a trimmed patch. Its two callers (`csgCaps.cpp:50`, `surface.cpp:917`) want one point, not a distribution.
- Nothing in the renderer samples a surface by area. `shaders/quadlight.sl` and `shaders/spherelight.sl` are standalone light shaders that draw their own positions with `random()` and test them with `visibility()`; the geometry that the scene draws for the same light is a separate, unrelated object. Since spec 030 the C++ versions are gone (`hcshader` deleted), and a missing shader reports `Failed to find shader`.
- `shaders/arealight.sl` (the RISpec-style area light) computes `Cl = intensity * lightcolor * (N.normalize(L)) / (L.L)`, writing `N = normalize(N)` first. It is a light shader that needs a light surface's `N` and `P`, which no code supplies for it. No RIB in `examples/` or `tests/` calls `AreaLightSource`; `arealight` is not referenced by any visual test.
- A shadow or `visibility()` probe is a ray with the transmission visibility flag, and an object intersects it only if `cRay->flags & attributes->flags` is non-zero (`surface.cpp:930`, `:2460`; `executeMisc.cpp:806`). The default attribute flags set `PRIMARY_VISIBLE`, `DOUBLE_SIDED` and `DISPLACEMENTS` and **not** `TRANSMISSION_VISIBLE` (`attributes.cpp:104-110`); only `Attribute "visibility" "transmission" 1` sets it (`rendererContext.cpp:3780`). Read literally, an unmarked occluder does not block a shadow ray, yet `examples/rib/quadlight.rib` sets no such attribute. Whether its committed image shows a shadow is **not established here**; the plan stage must measure it with a probe scene before FR-023 is answered (see the marker).
- A pre-existing leak in the light-handle path, confirmed by reading: `RiLightSourceV` and `RiAreaLightSourceV` call `extractLightHandle(..., pendingInstanceKey)` (`rendererContext.cpp:2414`, `:2440`), which stores the handle's key in `pendingInstanceKey`. `takeInstanceKey()` (the only reader that clears it, `:2369-2377`) is reached only from inside `getShader()` after a shader is found and of the right type (`:320`). `getShader()` returns early for the name `"null"` (`:301`), and returns before `takeInstanceKey()` for a missing shader or one of the wrong type. The key then survives into the next shader created, which takes the wrong identity, so its random draws change. This matters here because `AreaLightSource "null"` is the statement that closes an area-light scope.
- `RISPEC_GAPS.md` ("Area Light Sources", `[~]`) cites `RiAreaLightSourceV` at `rendererContext.cpp:2045`; it is at `:2430` now. Spec 056 left that entry to this spec (`specs/056-volume-shader-segments/spec.md` §Discrepancies, item 7). `DEVNOTES.md` repeats the "partial" status.
- The occlusion path is the ray tracer's on every camera hider: `visibility()` is a trace request answered by the hider that owns the shading context (CLAUDE.md gotcha 28). The photon pass reaches lights through the illumination hook (gotcha 23); photon emission never calls `shade()`.

### What RISpec requires (the target)

RISpec §4.2.3: `AreaLightSource` makes the geometry that follows it, up to the end of the attribute block or until `AreaLightSource "null"`, the light's emitting surface; the light shader is evaluated at points on that surface, and §12.2 gives the light shader at such a point the globals of the light surface (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t`), with `Ps` the point being lit. The effect is a soft shadow from an extended source. The emitting geometry also stays in the scene as ordinary geometry (this renderer's choice for the camera-visible question is FR-025).

### User Story 1 - `AreaLightSource` binds the geometry in its scope as the light's emitter (Priority: P1)

A scene author writes `AttributeBegin`, `AreaLightSource "arealight" ...`, a quad or a mesh, `AttributeEnd`. The renderer records every primitive created in that scope as an emitter of that light, including primitives made by procedurals, instances and CSG leaves inside the scope. `AreaLightSource "null"` and `AttributeEnd` close the scope, and a statement after it makes ordinary geometry again.

**Why this priority**: every other story needs the binding; with it alone nothing changes visibly, which makes it independently testable and safe.

**Independent Test**: parse a scene with two area lights, one closed by `"null"` and one by `AttributeEnd`, and query which primitives each light owns. Needs no sampling or shading.

**Acceptance Scenarios**:

1. **Given** an `AreaLightSource` followed by a quad and a mesh in one attribute block, **When** the frame begins, **Then** the light owns both primitives and nothing created after the block.
2. **Given** `AreaLightSource "null"` after a first area light, **When** a primitive follows, **Then** that primitive is not an emitter and the first light keeps its earlier primitives.
3. **Given** a procedural, an `ObjectInstance` and a CSG solid created in the scope, **When** the frame begins, **Then** their expanded leaves are emitters, each instance with its own transform.
4. **Given** a scene where `AreaLightSource "null"`, a missing shader name or a wrong-type shader sits between two lights that carry handles, **When** the second light is created, **Then** it takes its own handle's identity, not a stale one from the first statement (the `pendingInstanceKey` leak is fixed).
5. **Given** `Illuminate handle 0`, **When** the frame renders, **Then** the area light is off exactly as a classic light is.

---

### User Story 2 - Geometry answers "give me a point on this surface, uniformly by area, and the density you used" (Priority: P1)

For polygons, polygon meshes and subdivision surfaces, Geometry offers two operations: the surface's area, and a draw of a point on it from two numbers in [0,1) that returns the point, the normal, the parametric derivatives, the surface coordinates and the density with respect to area. Faces are chosen in proportion to their area. A draw is a function of its two input numbers only, so the same inputs give the same point at any thread count.

**Why this priority**: the estimator is only as right as this sampler; it is also what spec 041's light service wraps (issue #152, "built for 041 to adopt"), so it must stand alone.

**Independent Test**: a Geometry-level test with no shading: the area of known shapes, the density integrating to 1 over the surface, the mean of many draws equal to the centroid, a chi-square on the face-selection frequencies, and the same draws at 1 and 4 threads.

**Acceptance Scenarios**:

1. **Given** a unit quad, **When** 10^5 draws are made from a stratified sequence, **Then** the points are uniform over the quad and every density value is 1/area.
2. **Given** a mesh with faces of very different areas, **When** draws are made, **Then** faces are hit in proportion to area (a chi-square within the project's usual noise bound) and each draw's density is 1/(total area).
3. **Given** a subdivision surface, **When** it is sampled, **Then** the points lie on the limit or tessellated surface the camera sees (the same tessellation a ray would hit), and the area agrees with the tessellated area.
4. **Given** the same pair of numbers, **When** the draw is made twice, on one thread or four, **Then** the returned point, normal, coordinates and density are bit-identical.
5. **Given** a degenerate face of zero area, **When** the surface is sampled, **Then** it is never chosen and the density stays finite.

---

### User Story 3 - A light shader runs once per sample with the light surface's own globals, and the result is the area integral (Priority: P1)

For a light bound to emitters, the surface's shading point gets several light samples. Each sample places the light shader on a point of the emitting surface (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t` are that point's; `Ps` is the point being lit), runs it, and contributes one light contribution `(L, Cl)` scaled by the estimator so that the mean over samples converges to the integral of the light's radiance over its surface. The surface's own `P`, `N` and varyings are not touched while a light runs (which also removes today's shared-varying clobbering by a light that writes `N`). Light categories, `Illuminate`, `__nonspecular`/`__nondiffuse` and the active-light order behave as for any light.

**Why this priority**: this is the capability. It is Shading's half; the estimator's reference is §6.2 (`L_o ~ f_r L_e V G / p_A`).

**Independent Test**: a hand-computed pixel check in the style of 056's `VolumeProbe_` tests: a diffuse plane, one quad emitter, no occluder; the irradiance at a probe point equals the closed-form polygon irradiance within the measured noise, under the interpreter and the JIT.

**Acceptance Scenarios**:

1. **Given** a bound quad emitter running `arealight`, **When** a plane point is shaded, **Then** the light shader sees the quad's `P`/`N` and the plane's `Ps`, and the plane's own `P` and `N` are unchanged afterwards.
2. **Given** the same scene rendered with 4, 16, 64 and 256 samples, **When** the probe pixel is compared with the closed form, **Then** the error falls as the sample count rises, and 256 samples is within the noise-bounded tolerance the plan sets.
3. **Given** a light shader that reads `u`, `v`, `s` and `t`, **When** it runs on a mesh emitter, **Then** it reads the sampled point's coordinates (a textured emitter pattern is possible).
4. **Given** the same scene through `.rslo` and `.slo`, **When** both render, **Then** they agree (the existing JIT/interpreter parity rules apply, including that `lightsource()` is exempt, GitHub #115).
5. **Given** a classic `LightSource` in the same scene, **When** it and a bound light both light a point, **Then** each behaves as before for the classic one and the bound one adds its samples, in RIB order.

---

### User Story 4 - Shadows from an extended source are soft on reyes, zbuffer and raytrace (Priority: P1)

An occluder between a bound quad (or mesh, or subdivision) emitter and a receiver casts a soft shadow with umbra and penumbra, on all three camera hiders. Occlusion is a `trace()`-family probe from the shading point to the sampled emitter point; the emitter does not shadow itself.

**Why this priority**: it is the acceptance statement of issue #152 and the reason the spec exists.

**Independent Test**: one scene (quad light, sphere occluder, plane receiver) rendered under `Hider "reyes"`, `"zbuffer"` and `"raytrace"`; penumbra width and the analytic unoccluded fraction at probe points; cross-hider agreement within thresholds fixed after the noise measurement.

**Acceptance Scenarios**:

1. **Given** that scene, **When** it renders on each of the three hiders, **Then** a penumbra of the analytically expected width appears on each, and the three images agree within parity thresholds set from a measured run-to-run noise (visual-test noise method), with no threshold widened to pass.
2. **Given** a mesh emitter and a subdivision emitter in place of the quad, **When** they render, **Then** the same holds.
3. **Given** the emitter lies inside the occluder's silhouette from the receiver's side, **When** a point is shaded, **Then** the emitter's own surface does not occlude its own samples (FR-023).
4. **Given** the camera sees the emitter directly, **When** the frame renders, **Then** the emitter appears as FR-025 decides.
5. **Given** any thread count, **When** the same scene renders, **Then** the images are bit-identical (every draw comes from a sub-stream keyed by the sample's identity, spec 039).

---

### User Story 5 - `Option "arealight"` and `Attribute "arealight"` control the sampling at scene, light and object level (Priority: P2)

The author sets the sample count and a mode (`full`, `fast`, `off`) once for the scene with `Option "arealight"` and overrides it with `Attribute "arealight"` on a light's scope and on a receiving object's scope. `off` gives no contribution from bound lights at that level; `fast` is a cheaper mode (FR-018); `full` is the default.

**Why this priority**: soft-shadow cost is the main user knob; without it the only way to trade noise for time is editing a shader.

**Independent Test**: render a scene at several sample counts and each mode; compare the observable differences (noise, shadow presence, time) with the specification; check that a statement at each level reaches the right objects and that an unknown token or value reports the usual `Unknown arealight attribute` style error.

**Acceptance Scenarios**:

1. **Given** `Option "arealight" "samples" [4]` then `[64]`, **When** the scene renders, **Then** the noise drops and the expected value is unchanged.
2. **Given** `Attribute "arealight" "mode" ["off"]` on one receiver, **When** it renders, **Then** that receiver gets no light from bound lights and every other receiver is unchanged.
3. **Given** an attribute on the light's scope and another on the receiver, **When** both apply, **Then** the precedence FR-018 states holds.
4. **Given** the new token, **When** it is queried with `RiAttribute` read-back and a RIB round trip through `ribOut`, **Then** it survives (all four attribute layers carry it).

---

### User Story 6 - Quadric emitters: sphere, disk, cylinder, cone, paraboloid, hyperboloid, torus (Priority: P2)

A sphere or disk in an `AreaLightSource` scope emits by the same estimator, sampled analytically by area, so the geometric sphere light (the shape `spherelight.sl` approximates) is available without a bespoke shader.

**Why this priority**: it lets `spherelight` be deprecated in favour of geometry (US9), and a sphere or a disk is the most common area light after a quad.

**Independent Test**: Geometry-level uniformity and area tests for each quadric (as US2), plus the closed-form irradiance of a Lambertian disk at a probe point on its axis.

**Acceptance Scenarios**:

1. **Given** a partial sphere (`thetamax` less than 360, z-limits) in the scope, **When** it is sampled, **Then** only the swept part is sampled, uniformly by area.
2. **Given** a disk emitter, **When** a point on its axis is lit, **Then** the irradiance matches the closed form within noise.

---

### User Story 7 - Patch and NURBS emitters (Priority: P3)

Bilinear and bicubic patches, patch meshes and NURBS (trimmed or not) in the scope are emitters, sampled through their tessellation with area weighting. A trimmed region is not sampled.

**Why this priority**: completes the surface primitives of the RISpec; rarely used as lights. The plan may cut this story (clarified 2026-11-13) and records the cut.

**Independent Test**: as US2, with a trimmed NURBS whose trim removes half the area: the area and the draws cover only the kept half.

**Acceptance Scenarios**:

1. **Given** a trimmed NURBS patch, **When** it is sampled, **Then** no draw falls in a trimmed region and the area excludes it.
2. **Given** a bicubic patch mesh, **When** it is sampled, **Then** the points lie on the surface the camera sees.

---

### User Story 8 - The photon pass emits from an area light (Priority: P2)

A scene with a bound area light renders with the photon hider and a global-illumination pass; the pass still emits photons, and a bound light is reached through the same illumination hook that classic lights use, with the emitting surface's points as the origin of emission. This is a smoke check only: photon and show produce no comparable image and are never parity subjects, and the photon map still differs run to run for the same scene (GitHub #163, not changed here).

**Why this priority**: the plan names it; it keeps GI scenes with area lights from silently emitting nothing.

**Independent Test**: the photon pass on a scene with a bound light emits a non-zero photon count whose origins lie on the emitter, and the existing photon scenes are unchanged.

**Acceptance Scenarios**:

1. **Given** a bound quad light and a photon pass, **When** the pass runs, **Then** it emits photons from points on the quad (a count and an origin check, not an image comparison).
2. **Given** a classic light in the same scene, **When** the pass runs, **Then** its emission is as before.

---

### User Story 9 - `quadlight.sl` and `spherelight.sl` are documented as deprecated; the old and new forms agree within noise (Priority: P2)

The documentation tells authors to use `AreaLightSource` plus geometry instead of the two bespoke shaders, shows the equivalent scenes, and marks the two shaders deprecated. A visual test compares the old `quadlight` form with the new geometry form (the quad only, see Decision B) and they agree within noise, under the interpreter and under the JIT (the existing `quadlight` and `quadlight-slo` tests compare against one shared reference, so the JIT is already part of the contract; the `quadlight.sl` header comment that says its JIT render is broken predates spec 017 and is checked at plan). Nothing is deleted or changed in the two shaders' bytes.

**Why this priority**: the maintainer's acceptance statement; low risk (documentation and tests).

**Independent Test**: the new visual tests; a doc check that no maintained page describes the two shaders as the way to make an area light.

**Acceptance Scenarios**:

1. **Given** the quad scene in both forms, **When** both render, **Then** they agree within the noise measured per the visual-test noise method.
2. **Given** `spherelight`, **When** the documentation describes its geometry equivalent (a sphere emitter), **Then** it states the model difference (`spherelight.sl` emits isotropically and has no emitter cosine; a geometric Lambertian sphere does) and makes no claim that the two renders agree (Decision B, settled 2026-11-13).

---

### User Story 10 - Nothing else changes (Priority: P1)

With no `AreaLightSource` in a scene, every image on every hider and both shader formats is byte-identical to the tree before this spec, and every light shipped today (including `arealight.sl`, `quadlight.sl`, `spherelight.sl` used as plain `LightSource`) renders as before. No scene in `examples/` or `tests/` uses `AreaLightSource` today (measured above), so existing references cannot move; any that does move is a defect, not a re-capture.

**Why this priority**: it is the safety net that lets the other stories land.

**Independent Test**: the full suite outside the determinism sweep, the default determinism set and the architecture check are green with no reference re-captured and no threshold changed.

**Acceptance Scenarios**:

1. **Given** the whole existing suite, **When** it runs, **Then** every pre-existing reference matches exactly as before.
2. **Given** `ctest -L architecture`, **When** it runs, **Then** it passes with no new allowlist line.

---

### User Story 11 - The documents tell the truth (Priority: P3)

`RISPEC_GAPS.md`'s "Area Light Sources" entry moves to implemented, its stale line citation is gone, the stale source lines it cites are corrected, `DEVNOTES.md` (the RISpec row, the 057 row), `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (no new entry expected; any touched entry gets its history line), the RIB and shading guides, the site manual page for lights and the CLAUDE.md gotchas describe the new behaviour.

**Why this priority**: the project keeps its documents as the source of truth; a drifted gap list is how the false "partial" status survived.

**Independent Test**: grep and review; no maintained document calls `RiAreaLightSourceV` "identical to `RiLightSourceV`".

**Acceptance Scenarios**:

1. **Given** the documentation list in FR-033, **When** it is reviewed, **Then** each item matches the code.

---

### Edge Cases

- `AreaLightSource` with no geometry before `AttributeEnd`: the light has no emitter. It must not crash and must not light anything silently; a one-time warning names the light (assumption A7).
- Geometry in the scope of an `AreaLightSource` whose primitive type is not supported by this spec (points, curves, blobbies, implicit surfaces, procedurals that produce them): the primitive stays ordinary geometry and the light reports once that it has no sampleable emitter for it, so no light is lost without a message (D28's spirit: explicit, not silent).
- Two `AreaLightSource` statements in one scope: the second replaces the first for geometry that follows (the current handle is a single slot).
- A bound light inside a `MotionBegin` or an emitter with transform motion: the sample's time picks the emitter's pose; the shading point's `time` is used (assumption A8).
- An emitter whose area is zero (a collapsed quad, a radius-0 sphere): contributes nothing and divides by nothing.
- A back-facing receiver, a receiver behind the emitter, a receiver inside the emitter's own surface: the estimator contributes zero (the light shader's cone test and the `N.L` factor), never a negative or non-finite value.
- A light shader that calls `illuminate()` with the 3-argument cone form and reads the global `N`: now `N` is the emitter's, so gotcha 10's workaround is unnecessary for a bound light; a classic light that still writes `N` itself keeps working.
- `Illuminate`, light categories (`__category`) and `__nonspecular` on a bound light.
- `Attribute "arealight" "mode" ["off"]` on both the light and the object: off.
- An emitter in a `ReverseOrientation` or `Orientation "rh"` scope: the emitting side is the side of the flipped `N`.
- Network rendering (`CRenderer::netNumServers > 0`): `RiAreaLightSourceV` returns early today for a server count above zero (`rendererContext.cpp:2434`); the same applies to the binding (assumption A9).
- The determinism contract: the same scene at `-t:1` and `-t:4` is bit-identical (the area draws are keyed, not shared).

## Requirements *(mandatory)*

### Functional Requirements

**Binding (Graphics State and the renderer core)**

- **FR-001**: `AreaLightSource` MUST set a non-owning "current area light" on the graphics state's attribute block; `AreaLightSource "null"` and the end of the enclosing attribute block MUST clear it; `AttributeBegin` MUST inherit it and clones MUST copy it.
- **FR-002**: Every primitive created while a current area light is set (top-level geometry, procedural and delayed expansions, instance leaves, CSG leaves) MUST be recorded once in a per-frame emitter registry keyed by the light: the registry's type is Geometry's, and the renderer core creates, fills and releases it (research R2). Emitters stay ordinary, visible geometry.
- **FR-003**: Geometry MUST see only the opaque handle on the attributes (no light, shader or core type), as it sees other handles (CLAUDE.md gotcha 24, constitution Principle VIII); no new allowlist line is added.
- **FR-004**: The `pendingInstanceKey` carried by `extractLightHandle` MUST be cleared on every exit of `RiLightSourceV`/`RiAreaLightSourceV` that does not create an instance (the `"null"` name, a missing shader, a wrong-type shader). A scene's draws MUST NOT depend on such a statement's presence before another light. This spec owns the fix and its test (SC-010). The fix is in this branch from Phase 1; spec 033 keeps the two statements unchanged (coordinator, 2026-11-14), so the fix stays as written and is not re-applied by 033.
- **FR-005**: `Illuminate` on the handle of a bound light MUST switch it as it switches a classic light.

**Area sampling (Geometry)**

- **FR-006**: Surfaces that can emit MUST offer `area()` and `sampleArea(u1, u2)`, returning `{P, N, dPdu, dPdv, (u, v), (s, t), pdfA}` with `pdfA` the density with respect to area (1/area for a uniform draw), for the primitive kinds in FR-009. A draw MUST be a pure function of `(u1, u2)`, the surface and the time. For a light bound to several emitters, the emitter is chosen in proportion to its area (the first number of the draw selects it, the rest place the point), so the density of every draw is `1/(total area of all the light's emitters)`; a zero-area emitter is never chosen.
- **FR-007**: The sampler MUST sample the geometry the camera sees (the same tessellation or analytic form a ray would hit), honouring `Orientation`, `Sides`, trims and displacement-free base geometry (displacement is not applied to an emitter; assumption A6).
- **FR-008**: The two numbers of each draw MUST come from spec 039's sampler sub-streams, with a new, documented call-site constant, so the draws are identical at any thread count and stratify where the sampler stratifies. The constant MUST NOT collide with any constant in `sampling.h` on master or in the sibling branches (see Overlaps).
- **FR-009**: Committed primitive kinds, in this order: polygons, polygon meshes and subdivision surfaces (P1); the quadrics (P2); bilinear and bicubic patches, patch meshes and NURBS with trims (P3). Points, curves, blobbies, implicit surfaces and CSG-produced emitters beyond their leaves are not committed (see Out of scope); an unsupported emitter reports once (Edge Cases). The P3 kinds may be cut at plan (A12).
- **FR-010**: Geometry MUST NOT gain a Hider or Shading dependency. The sampling request is carried on the interface spec 031 left for it (the ray-side `IRayContext`, per its FR-021 and acceptance scenario 2), and no member is added to it that the committed kinds do not need.

**Light-surface execution (Shading)**

- **FR-011**: For a bound light, `iterateLights` MUST run the light shader once per sample, with the light surface sample's globals (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t`, `time`) and `Ps` the point being lit; the lit surface's `P`, `N` and varying arrays MUST be unchanged while any light runs. Classic lights MUST be unchanged. A bound light skips its own emitter primitives as receivers (FR-017).
- **FR-012**: Each sample MUST push one light contribution `(L, Cl)` whose `Cl` is the light shader's own result at the sampled emitter point, scaled by the renderer by exactly `1 / (n * p_A)`, where `n` is the sample count in force for the receiver and `p_A` the area-measure density of that sample's draw (for a light with several emitters the draw first chooses an emitter in proportion to its area, so `p_A = 1/(total area of the light's emitters)`, FR-006). The sum over samples is then an unbiased estimate of the integral over the emitter's area of what the shader computes, reference §6.2 with the factors split as follows (Decision A, settled 2026-11-13): the renderer applies **only** `1/p_A` (and the `1/n` of the average); it applies no emitter cosine `cos(theta_l)`, no `1/r^2` and no other geometry term; those belong to the light shader, as in the shipped `arealight.sl` (`N.normalize(L)` and `1/(L.L)`) and as in the RISpec's own area light, whose cone test (`illuminate(P, N, PI/2)`) is the facing test. The receiver's cosine remains the surface shader's, as for any light. Double counting is prevented by construction (the renderer never multiplies a geometry term the shader could also apply) and by a test: a probe light shader that returns a constant `Cl` must give an irradiance equal to that constant times the emitter's area-weighted receiver term with no `cos(theta_l)` or `1/r^2` from the renderer, and `arealight.sl` must match the closed form of SC-002 once, not twice. No shipped shader changes and no shader reference is re-captured.
- **FR-013**: The interpreter and the JIT MUST both implement FR-011 and FR-012 with matching results; the illumination hook path (photon emission) MUST be honoured by both (gotchas 23 and 28; no `costheta` read on that path).
- **FR-014**: The sample points MUST be drawn from sub-streams keyed by the shading point's identity, the light's identity and the sample index (spec 039), never from a global generator; renders MUST be bit-identical at any thread count.
- **FR-015**: Light categories, `__nonspecular`, `__nondiffuse`, `Illuminate` and active-light order MUST behave for a bound light as for a classic one; the 056 `lightSideN` snapshot MUST still be taken before any light runs (a light's own `shadow()` reads it).

**Visibility and controls**

- **FR-016**: Occlusion of a sample MUST go through the ray tracer's visibility probe on every camera hider (reyes, zbuffer, raytrace), the probe semantics of `visibility()` (spec 056: probes run no volume step); if `ITracer` needs an any-hit query it is added and versioned like spec 026's interfaces, with `kRendererServicesVersion` stepped per CLAUDE.md gotcha 28.
- **FR-017**: A sample's shadow ray MUST NOT be blocked by the emitters of the light it was drawn for. The exclusion is by the identity of the light's emitter set (the registry of FR-002), not by a ray bias, so it holds at grazing angles and on curved emitters. A bound light does not illuminate its own emitter primitives either: they are skipped as receivers of that light (they still receive classic lights and other bound lights). Other geometry is blocked as FR-023 says.
- **FR-018**: `Option "arealight"` and `Attribute "arealight"` MUST exist, through all four attribute layers (token constants, RIB parsing, storage and query, pre-declaration), with `samples` (a positive integer) and `mode` (`full`, `fast`, `off`). `full` uses the sample count in force. `fast` uses fewer samples than `full` and still tests occlusion for every sample it takes (clarified 2026-11-13); the reduction rule (a fixed fraction of the count in force, never below one sample) is fixed at plan, documented, and measured for its noise. `off` gives no contribution from bound lights. The three levels are the scene (`Option`), the light (the `Attribute` in force at its `AreaLightSource` statement) and the receiving object (the `Attribute` in its own scope). Most specific wins, object over light over scene, for `samples` and for `mode` independently; `off` at any level turns the light off for that receiver, whatever the other levels say.
- **FR-019**: The defaults (deviation: plan D-7, confirmed by measurement at task T102) MUST preserve the contract of FR-026: with no `Option`/`Attribute "arealight"` the sample count is chosen in the plan from a measurement against `quadlight`'s default (16 in `examples/rib/quadlight.rib`) and a documented noise target, `mode` is `full`.
- **FR-020**: An unknown token or value MUST report the renderer's usual attribute error (`CODE_BADTOKEN`, never `CODE_LOG`, which swallows warnings). The controls MUST read back through the attribute query and survive a RIB round trip through `ribOut`.

**Deprecation and documents**

- **FR-021**: `quadlight.sl` and `spherelight.sl` MUST be documented as deprecated in favour of `AreaLightSource` plus geometry, in their own header comments only if their compiled bytes do not change (the characterization checks in CLAUDE.md compare compiled shader bytes; a comment-only change is verified not to change them, otherwise the note lives in the documentation only). Nothing is deleted. The `spherelight.sl` note names the model difference of Decision B.
- **FR-022**: Visual tests MUST compare the old and new quad forms on the interpreter and the JIT, within noise. The sphere form carries no agreement test (Decision B).

**Visibility defaults for occluders**

- **FR-023** (decided 2026-11-13, option B): An object occludes a bound area light's samples only if it carries `Attribute "visibility" "int transmission" [1]`, as for `visibility()`, `transmission()` and ray-traced `shadow()` in every other shader; this is documented in the manual and the RIB guide. The area light's shadow test is the ordinary one: no new exception, no change to any existing image, and `Attribute "visibility" "int transmission" [0]` stays the opt-out. The emitter set is excluded from the light's own shadow rays by identity (FR-017), not by a bias.

  Evidence (probe of 2026-11-13). Source reading: the default attribute flags lack `TRANSMISSION_VISIBLE` (`attributes.cpp:104-110`) and a shadow or `visibility()` ray carries only that flag (`executeMisc.cpp:806`); the project's own shadow scenes set it explicitly (`tests/convergence/scenes/arealight-cone-raytrace.rib.in`, `examples/rib/tests/csg-secondary-rays-*.rib`), and `quadlight.rib` does not. Renders: the only Release build on the machine (main checkout, built 2026-11-01) is not current master (about 113 commits older, 130 compiled shaders against 137, before specs 029 to 031 and 056). With it, seven read-only scenes (the committed `quadlight.rib`, and variants with an overhead disk or a sphere above a floor, each default, flagged on the occluder, and flagged at world scope) gave floors byte-identical to the no-occluder render in every case, so it showed no shadow even with the flag set. That tree's result is not evidence about master.

  Plan task (required): re-probe on a fresh build of current master (`quadlight.rib`, one occluder with and without the flag, floor compared with a no-occluder render). If the flagged case still shows no shadow on a fresh master build, that is a defect: it is reported to the coordinator for filing and is not fixed silently in this spec. Result (plan, 2026-11-14): `visibility()` never reports an occluder; filed as GitHub #169 and not fixed by 057 (plan.md D-2, research R1).

- **FR-024**: The emitter MUST remain ordinary geometry for every ray class unless the author changes its visibility attributes; binding a light MUST NOT hide an emitter from, or change its visibility to, the camera or any other ray class (FR-023 concerns only what the light's own shadow test may be blocked by).

- **FR-025**: When the camera sees an emitter, the emitter's pixels MUST be exactly what its own `Surface` shader produces, as today (clarified 2026-11-13, option A): the renderer adds no automatic radiance and there is no attribute for it. An unlit or constant-colour surface is a bright quad only if the author makes it one. Scenes with no `AreaLightSource` are byte-identical (FR-026), and so is an emitter's own pixels with or without its light.

**No change otherwise**

- **FR-026**: With no `AreaLightSource` in the scene, the renderer MUST produce byte-identical images on every hider and both shader formats; no reference is re-captured and no threshold is widened for any pre-existing test.
- **FR-027**: `RiAreaLightSourceV`, the RIB writer (`ribOut`), the Python and Lua bindings and the preview MUST round-trip the statement unchanged (the writer already numbers the handle).
- **FR-028**: With the JIT off (`OPENRENDER_ENABLE_JIT=OFF`) the interpreter path MUST carry everything.

**Boundaries and tests**

- **FR-029**: No new include or link edge outside Principle VIII's allowed-dependency table and no new allowlist line (`ctest -L architecture`); Geometry includes nothing of Shading, Hiders or the core; Shading includes no core header; the binding lives in Graphics State behind the existing handle conventions.
- **FR-030**: Convergence tests (a closed-form irradiance for a quad and a disk, an analytic penumbra) and a thread-count-identity test MUST exist for the sampler and the estimator; every new test is verified red before the change and green after (constitution III).
- **FR-031**: Cross-hider agreement MUST be asserted over reyes, zbuffer and raytrace only, with thresholds fixed after the measured run-to-run noise of the scene; photon and show are never parity subjects (the photon pass is a smoke check, US8).

**Merge order and overlap with spec 033** (coordinator decision, 2026-11-13)

- **FR-032**: Spec 033 (the `ShadingSystem` facade; shader loading moves into Shading) merges into master **first**. This spec is designed and built against master as it is, MUST NOT depend on 033's unmerged code, and MUST keep its changes to shader loading and to how a light shader instance is created to the minimum it needs: the light-surface globals are added on the execution side (`iterateLights` and the light's `illuminate` entry), not by reshaping loading or instantiation. When 033 lands on master, this spec merges master into itself at once (mid-implementation, not only at merge preparation) and adapts to the facade; that sync is planned, not a failure. The `pendingInstanceKey` fix (FR-004) and its test belong to this spec; 033 moves that code unchanged and does not re-fix it.
- **FR-033**: Documents to update: `DEVNOTES_DETAILS/RISPEC_GAPS.md` (entry and its stale line citation), `DEVNOTES.md` (the RISpec row and a 057 row), `DEVNOTES_DETAILS/RIB_GUIDE.md` and the site manual's lighting page, `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (history lines of touched entries only), `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` (add the missing 056 and 057 rows to its spec index, §13), `CLAUDE.md` (a gotcha for the binding and the estimator, and the Recent Changes line), and the legacy light-shader pages that call the two shaders the area-light mechanism.

### Key Entities

- **Current area light**: a non-owning handle on the attribute block naming the light that geometry created now belongs to.
- **Emitter registry**: a per-frame table, a Geometry type that the renderer core creates, fills and releases, mapping a light to the primitives it owns (instances and leaves included).
- **Area sample**: `{P, N, dPdu, dPdv, u, v, s, t, pdfA}` on an emitter, drawn from two numbers.
- **Light contribution**: one `(L, Cl)` per sample, weighted by the area estimator, consumed by `illuminance()` and the lighting builtins.
- **Area-light controls**: `samples`, `mode` at scene (Option), light and object (Attribute) level.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A scene with a bound quad, a bound polygon mesh and a bound subdivision emitter, each over a plane with a sphere occluder, shows a soft shadow with a penumbra on reyes, zbuffer and raytrace; for each scene the three hiders agree within thresholds fixed after the scene's measured run-to-run noise, and no threshold is wider than that measurement justifies.
- **SC-002**: For a Lambertian quad and a disk emitter with no occluder, the irradiance at a probe point equals the closed form within the measured noise at the default sample count, and the error falls as the sample count rises (4, 16, 64, 256), interpreter and JIT.
- **SC-003**: Geometry sampling: for each committed primitive kind the draws are uniform by area (a chi-square within the project's noise bound), the density integrates to 1, and every draw is bit-identical at 1 and 4 threads.
- **SC-004**: A scene renders to identical bytes at `-t:1` and `-t:4`.
- **SC-005**: `AreaLightSource "null"` and `AttributeEnd` each close the scope: geometry after them is never an emitter (a registry test).
- **SC-006**: The old `quadlight` form and the new geometry form agree within noise (interpreter and JIT); there is no sphere-form agreement claim (Decision B).
- **SC-007**: With no `AreaLightSource`, every pre-existing visual, parity, determinism (default set and sweep), JIT-parity and plug-in test is unchanged: zero re-captured references, zero widened thresholds (counts reported against the starting tree on macOS and on Linux, Ubuntu 24.04 with GCC 13 `-Werror`).
- **SC-008**: The photon pass emits a non-zero number of photons from a bound light, from points on the emitter, and classic photon scenes are unchanged.
- **SC-009**: `ctest -L architecture` passes with no new allowlist line; the full suite is green apart from failures that fail the same way on the starting tree.
- **SC-010**: The `pendingInstanceKey` leak is covered by a test that fails on the starting tree (a light after a `"null"` or missing-shader light keeps its own identity).

## Assumptions

- **A1**: Spec 029, 030, 031, 039 and 056 are merged: confirmed with git (`git merge-base --is-ancestor` for each branch, and the merge commits `14d8ce2d`, `c2e3cce3`, `732ba64d` on master). The issue's stated dependencies, 029 and 030, are satisfied; 031's interfaces are the attachment point (FR-010).
- **A2**: Classic RSL light semantics only. How a bound area light looks under `Integrator "direct"` and the closure model belongs to specs 041/042; this spec makes the geometry-bound light a non-delta kind that 041 can wrap (the sampler and the registry are built for that adoption), and adds nothing to the integrator interface.
- **A3**: Occlusion uses the visibility-probe semantics of `visibility()` (no volume step, spec 056). A volume-attenuated area-light shadow is not provided.
- **A4**: Emission is one-sided, into the half space of the emitter's `N` (after `Orientation`/`ReverseOrientation`); the shader's `illuminate(P, N, PI/2)` cone does the facing test. A two-sided emitter is a shader choice.
- **A5**: A second `AreaLightSource` in a scope replaces the first; the single slot is not a stack.
- **A6**: The emitter's base geometry is sampled; displacement shaders are not applied to an emitter's samples. A bound light on a displaced surface reports nothing and uses the base surface (stated in the manual).
- **A7**: A bound light with no sampleable emitter warns once and contributes nothing.
- **A8**: An emitter's pose is taken at the shading sample's time (motion blur correct, no extra draw).
- **A9**: Network rendering is not extended: the binding is skipped where `RiAreaLightSourceV` already returns early.
- **A10**: The default sample count is fixed at plan from the measurement in FR-019, not here. *Deviation (plan D-7, approved 2026-11-14)*: the plan fixed 16 and the `fast` rule `max(1, ceil(n/4))` because the new estimator did not exist at plan time; the measurement is task T102 in Phase 6, behind a maintainer pause.
- **A11**: Merge order is fixed by the coordinator: 033 merges first; 057 is built against master as it is and merges master in at once when 033 lands (FR-032).
- **A12**: Points, curves and blobbies are out of scope (below); the quadric and patch increments are in scope in the stated priority order and the plan may cut P3 (patches and NURBS) if it does not fit (maintainer, 2026-11-13).

### Decisions settled at the clarify stage (2026-11-13)

- **A. Who owns the emitter-side geometry factor: settled.** The renderer applies only `1/p_A`; the light shader keeps its cosine and falloff (FR-012). Rejected alternatives: the renderer applying the full `G` with a corrected `arealight.sl` (would change a shipped shader and its hashes), and a per-light declaration.
- **B. `spherelight.sl`'s model: settled.** The agreement claim is for the quad only; the sphere difference is documented (FR-021, US9).

## Out of scope

- Points, curves, blobbies, implicit surfaces and CSG results as emitters beyond their leaves (reported once, FR-009).
- The light service, light kinds, MIS and the direct integrator (specs 041, 042); a physically based emission model; light filters; textured emission beyond what a light shader computes from `u,v,s,t`.
- Volume-attenuated area-light shadows (spec 056's segment step is not applied to the probe); caustics and GI changes; photon-map determinism (GitHub #163).
- The depth-of-field noise and the pixel filter (GitHub #166): unrelated to area-light noise and not pulled in.
- Removing or changing `quadlight.sl`/`spherelight.sl` or `hcshader` leftovers: documentation and tests only; no deletion without the maintainer's approval.
- Network rendering of area lights (A9), and displacement of emitters (A6).
- Shader loading and instantiation changes beyond the minimum (FR-032).

## Overlaps and merge-order risks (checked 2026-11-13)

- **Spec 033 (shading-system backends), a real merge-order risk.** (Written 2026-11-13; at 2026-11-14 033's branch held one design commit and 057 had still read none of it.) 033 adds a `ShadingSystem` facade and moves shader loading into Shading. This spec edits `CRendererContext::getShader` / `RiAreaLightSourceV` / `RiLightSourceV` (the `pendingInstanceKey` fix and the binding) in `rendererContext.cpp`, and `CShadingContext::iterateLights` and the light's `illuminate` entry in Shading, the code 033 is about to move. Decision (coordinator, FR-032): 033 merges first; this spec is built against master as it is, touches loading and light-instance creation only as far as it must (the light-surface globals sit on the execution side), and merges master in as soon as 033 lands to adapt. 033 has no commits on its branch yet (at master, `278b0394`), so there is nothing to design against and nothing to avoid; this spec does not read or depend on it.
- **Spec 040 (GI determinism), a merge risk.** A new call-site constant for the emitter sub-stream (FR-008) lives with the others in `src/common/sampling.h` / the shading call-site table; 040 adds its own constants and keys photon and cache draws. Reserve a constant range and rebase on whichever merges first; 040 also edits `shading.cpp`-adjacent GI code but not `iterateLights`.
- **Spec 048 (OIIO image I/O): no expected overlap** (texture and display I/O; this spec touches no image code).
- **Spec 041 (light service), downstream.** Built to adopt: the sampler and the registry are the pieces 041 wraps as a non-delta kind; nothing of 041's interface is anticipated here beyond FR-010.
- **Open Bug Tracker issues (checked 2026-11-13):** #163 (photon map differs run to run) bounds what the photon smoke check may assert; #100, #101, #126 (JIT light/illuminance defects) may surface in JIT runs of the new tests and are not fixed here; #115 (`lightsource()` differs interpreter vs JIT) bounds the JIT parity claim; #156 (transmission rays with an exactly-zero direction component miss occluders) and #148 (`traceTransmission` assert for hit mode "shader") touch the probe path used for occlusion and must be re-read at plan; #166 is unrelated. Issues #158, #159, #160, #161, #164, #165 and #166 were wrongly closed on 2026-11-13 by a merge-message keyword and have been reopened; they count as open here.

## Discrepancies with the plan and the issue (found at the specify stage)

1. The issue and plan describe a "default visibility flags problem (occluders never enter `root` without `visibility "transmission"`)". Measured by reading: the problem is the ray/object flag test, not the root hierarchy (default flags lack `TRANSMISSION_VISIBLE`; `raytracingFlags` includes it, `renderer.cpp:728-731`), and `quadlight.rib` sets no attribute, so what the existing shadow image shows must be established before FR-023 is answered (marker).
2. The plan says the `pendingInstanceKey` leak is on the `"null"` path. It is wider: also a missing shader and a wrong-type shader (FR-004).
3. `RISPEC_GAPS.md` cites `rendererContext.cpp:2045`; the function is at `:2430`.
4. The plan places `shader loading` and light creation as untouched; with spec 033 pending the coordinator's rule in FR-032 governs.
5. `RENDER_ARCHITECTURE_REFERENCES.md` §13 (the spec-to-section index) has no row for 056 or 057; the plan names §2.7, §4, §5.6, §6.2 and D10 for 057 (FR-033).
6. The reference §6.2 formula and the shipped `arealight.sl` disagree on who applies the emitter cosine and distance falloff (Decision A).
7. The brief says CLAUDE.md gotchas 26, 28, 35, 36, 38 and 39 bear on this spec: confirmed present in CLAUDE.md (gotchas 37, 38, 39 exist there). The ones that matter: 10 and 11 (`N` and the `L` sign), 23 (the illumination hook), 24 (handles), 26 (frame state), 28 (the tracer and the services version), 35 (hiders own a context), 36 (volume segments), 38 (registered hiders and the removed `hcshader`) and 39 (Geometry's interfaces).

## References

`DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §2.7 (material outputs and emission), §4 (lights; D10: three light kinds, the classic adapter is a delta sample and cannot supply pdfs), §5.6 (exact versus approximate visibility), §6.2 (the area estimator), D1 and D28 (hider and integrator separation; no drop-offs); RISpec §4.2.3 and §12.2; GitHub #152; specs 030 (FR-028 and R10), 031 (FR-021), 039, 056 (discrepancy 7).
