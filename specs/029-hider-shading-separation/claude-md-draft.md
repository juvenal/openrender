# Proposed CLAUDE.md changes for spec 029 (applied 2026-11-05)

> **Applied with the maintainer's authorization, as gotcha 35.** Spec 038 is on master with gotchas 29 to 34, so this
> spec's new gotcha 29 (section 5) became **gotcha 35**. Gotcha 28 got two changes this draft does not list, because the
> draft left them stale: its title (no more per-frame slot) and the sentence saying `ShaderGlobals` and
> `classifyShading()` are consumed by tests only.

The spec-kit plan stage would normally update `CLAUDE.md` through `.specify/scripts/bash/update-agent-context.sh`.
That script was **not** run: `CLAUDE.md` is changed only by the maintainer (spec FR-032). This file holds the
proposed text. It is written against the maintainer's answers of 2026-11-02 (the recommended option of P-Q1 to P-Q16, and the
versioning policy that makes this spec's version 2.1.0) and must be revised to the measured end state before it is
applied. The "Spec-kit agent rules" section is
not touched. The numbers below are the measured end state (2026-11-04).

## 1. Architecture, "Hider system" (CLAUDE.md:183-188)

Replace:

> `CShadingContext` (`src/ri/shading.h`) is the abstract base. Implementations:

with:

> `CHider` (`src/ri/hiders/hider.h`) is the base of every hider (spec 029). It owns its thread's
> `CShadingContext` (Shading, `final`, no virtuals), declares `renderingLoop()`, and is the context's tracer and
> shading producer. Implementations:

and keep the list (`CReyes` -> `CStochastic`/`CZbuffer`, `CRaytracer`, `CPhotonHider`). In "Hider selection",
`renderer.cpp:beginFrame()` (~line 908) -> (~line 815); "a class inheriting `CShadingContext`" -> "a class
inheriting `CHider`".

## 2. Testing (CLAUDE.md:~270-285)

Add:

```bash
ctest --test-dir build -L link                          # spec 029: libshader_shading self-contained, the static consumer, no workaround text
ctest --test-dir build -L '^link-package$'              # opt-in (configure with -DOPENRENDER_LINK_PACKAGE_TEST=ON): an installed openRender package consumed statically (scratch configure, builds a second tree)
```

## 3. Gotcha 23 (the illumination hook sentence)

Replace "with it on, they only call the hider's hooks" with "with it on, they only call the context's
`IIlluminationHook` (`illuminationHook.h`), which the photon hider installs on the context it owns; with none
installed the calls are no-ops".

## 4. Gotcha 28 (rewrite of its last third)

Replace from "The trace calls go through `ITracer` (`tracer.h`): the core sets `CRenderer::frameTracer` ..." through
"... and to the DB-001 allowlist." with:

> The trace calls go through `ITracer` (`tracer.h`), answered by the hider that owns the context: each `CHider`
> installs itself with `setTracer()`, and `libshader::traceRay`/`traceBundle` (`traceCalls.h`) ask the context's
> tracer first and the services' `tracer()` only for a context with no owner (the contract doubles); the renderer's
> services answer `nullptr`. With neither, they warn once and treat the rays as misses. There is no per-frame slot
> and no adapter any more (spec 029). The depth rule lives in the context's `enterRayLevel()`/`leaveRayLevel()`;
> the hider passes `CRenderer::maxRayDepth`. `libshader_shading` is self-contained: it links with undefined
> symbols as an error (`--no-undefined` on Linux, ld64's default on macOS) and has no flat-namespace import, so
> there is no load stub and no `-force_load` in the package; a new undefined reference fails its link.

and "`CRendererServices` is version 2.0.0" -> "2.1.0" (with "`getPhotonMap` returns `IPhotonLookup *`"). Replace the
gotcha's last sentence ("A change to the interface bumps `kRendererServicesVersion` (MAJOR: ...; MINOR: ...); every
implementer `static_assert`s its MAJOR.") with: "A change to the interface steps `kRendererServicesVersion` and adds a
history entry (MAJOR: breaking; MINOR: a non-breaking addition; PATCH: a bug fix only), but MAJOR stays at 2 until spec
055: nothing versioned has been distributed, so 2.x is unstable and a MINOR step may break, saying so in its entry.
Every implementer `static_assert`s MAJOR 2." 

## 5. New gotcha 29

> 29. **Hiders own a shading context; several names are frozen by the allowlist (spec 029).** `CRenderer::contexts`
>    is a `CHider **` that kept its name, because `reyes.cpp`'s DB-011 line is keyed on the symbol; reach the context
>    with `contexts[i]->shadingContext()`. The trace bodies are `CHider` members that stay in `trace.cpp` (its DB-011
>    and DB-017 lines are keyed on the file). Geometry keeps calling `CShadingContext::shade(CSurface *, ...)`,
>    `displace()` and `displaceEstimate()`: an opaque forwarding entry that calls the owning hider's
>    `IShadingProducer`, which builds the point source (`CSurfacePointSource`) and the `ShaderGlobals` batch and calls
>    `shade(const ShaderGlobals &, IPointSource &)`; spec 031 retires the forwarding entry. `shade()` branches on the
>    batch's ray type and hit mode only, which the producer takes from `classifyShading()`, corrected in spec 029 to
>    the code in three places: a shadow hit reads `transmissionhitmode` at every dimension; a ray hit traced from a
>    REYES grid (any depth, any frame) is `Secondary`; and the `Photon` rule is narrowed to a 0-D batch at depth 0 not
>    in a shadow. Photon emission itself never calls `shade()` (`photon.cpp` sets its 0-D state and runs the light with
>    `illuminate()`), so only the test doubles reach the `Photon` rule; a ray a light traces during emission is a
>    `Secondary` hit with the hook on (`sit_photon_emission_trace` pins it). Never C-cast between a hider and a context: a cast from `CHider **`'s elements to `CReyes *` is a
>    `static_cast` (use it explicitly), a C-style cast from the context is a `reinterpret_cast`, and a `dynamic_cast` from
>    the non-polymorphic `CShadingContext` does not compile. The object hash stays the context's (`objectHash()`), and its empty-entry sentinel is the
>    context's address, not the hider's. The core's dispatcher sets
>    `CRenderer::activeContext` for every render thread. The shaded object is three fields of the state set
>    together with `setShadedObject()` (opaque surface, `ObjectHandle`, attributes); Shading includes no Geometry or
>    Hiders header.

## 6. Repository layout

Under `src/libshader/shading/`: mention `bundles.{h,cpp}` (moved from `src/ri/hiders/` by spec 029).

## 7. Active Technologies (append)

> - C++20, CMake >=3.19, `-Werror`; no new dependencies: hiders own a `final` `CShadingContext` (`CHider`), implement
>   `ITracer` and a Shading-owned `IShadingProducer`; Shading reaches the photon map through `IPhotonLookup` and the
>   photon pass through `IIlluminationHook`; `shade()` consumes `ShaderGlobals` and an `IPointSource`;
>   `libshader_shading` is self-contained (029-hider-shading-separation)
> - Storage: N/A; memory only (029-hider-shading-separation)

## 8. Recent Changes (prepend)

> - 029-hider-shading-separation: the hiders own their shading context instead of deriving from it (`CHider`); the
>   context is `final` and has no virtuals and no foreign friends; the hiders trace for the context they own
>   (`CHiderTracer` and `frameTracer` gone); the bundles moved into Shading and the photon map is reached through
>   `IPhotonLookup`, so `libshader_shading` has no flat-namespace import, links with undefined symbols as an error,
>   and a static consumer launches with no `-force_load`/`--whole-archive` (GitHub #24, #44); `shade()` consumes
>   `ShaderGlobals` (ray type and hit mode explicit) and an `IPointSource`; `kRendererServicesVersion` 2.1.0. DB-001
>   and DB-005 resolved, DB-006 31 -> 30, DB-011 168 -> 164, DB-020 narrowed; the allowlist 372 -> 354 entries, the
>   `CRenderer::` metric 612 -> 610 references (Hiders 558 -> 556). No image changed on either platform; the spec 026
>   trace replay unchanged (see spec 029).
