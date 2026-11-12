# CLAUDE.md draft for spec 030 (T139; NOT applied; FR-034)

Eight hunks, in file order. Old text is quoted exactly; "replace" means the old text becomes the new text, "insert" means the new text
is added at the stated place. Nothing else in `CLAUDE.md` changes. Numbering note: master's `CLAUDE.md` (spec 056, merged as
`11fa3424`) already carries gotcha 36; the new gotcha is therefore numbered **37**, so the merge adds no renumbering (this branch's
`CLAUDE.md` has no 36 until master is merged; the maintainer may prefer to renumber at merge preparation).

## 1. Architecture / Hider system (replace the last two lines of the section)

Old:

```
Hider selection is a `strcmp` chain in `renderer.cpp:beginFrame()` (~line 815).
Adding a hider = new `#include` + `if/else if` branch + a class inheriting `CHider`.
```

New:

```
Hiders are chosen through a registry (spec 030): `hiderRegistry()` (`src/ri/hiders/hiderRegistry.{h,cpp}`) holds one table per hider
kind (`OrHiderTable`, `hiderAbi.h`, version 2.0.0, internal until spec 051): its name and aliases, the capabilities it provides, the
dispatcher that drives it and `create`/`destroy`. `renderer.cpp:beginFrame()` resolves the `Hider` name, builds one hider per thread
through the table and never names a concrete hider (`Registry_CoreNamesNoHider`). Adding a hider = a class inheriting `CHider` plus a
table defined in the hider's own source file and its getter added to `registerBuiltinHiders()` (`builtinHiders.cpp`); a hider outside
the core registers itself with `OR_REGISTER_HIDER(getter)` (`hiderAbi.hpp`) and the core is not edited at all.
```

## 2. Architecture (insert a new section after "Scene Interface services and the active context (spec 038)")

```
### Hider and integrator registries, the Transport domain (spec 030)
There are ten domains since constitution 1.4.0: **Transport** (`src/ri/transport/`) holds the integrators (the transport strategies a
frame is rendered with) and the capability vocabulary hiders and integrators negotiate with (`capability.h`: `rasterization`,
`display-output`, `display-channels`, `deep-shadow-output`, `illumination-hook`, plus reserved names). It includes Infra only today;
Hiders may include it (`hiderAbi.h` does) and Transport includes no Hiders, Scene Interface or Core file
(`ArchFixture_transport_*`). `integratorRegistry()` holds the built-in `classic` (the default: it executes nothing, every image is
unchanged) and the integrators a program registers with `OR_REGISTER_INTEGRATOR`; both registries share the name and alias rules of
`common/kindRegistry.h` (exact, case-sensitive, a collision refuses the whole table). The Scene Interface carries an `Integrator
"<name>" [parameters]` statement (`RiIntegrator`/`RiIntegratorV`, `rib.y`, `ribOut`; ignored silently by the preview) stored in
`COptions` (`integrator`, `integratorHandle`, `integratorParameters`); the name is resolved at frame begin, like the hider's. The
renderer core keeps the frame's choice in one slot, `frameTransport()` (`render/rendererTransport.{h,cpp}`: the resolved hider and
integrator, the integrator instance and the hider's traits), written only by `CRenderer` before the render threads start and after
they are joined. See gotcha 37.
```

## 3. Testing (insert one line after the `-L link` line)

```
ctest --test-dir build -L registry                      # spec 030: the hider and integrator registries (lookup, collisions, pair check and refusal, parameters, the frame abort paths, test-double hider)
```

## 4. Dev workflow (replace the end of the feature-branch sentence)

Old: ``, ..., `021-deferred-csg-evaluation` (deferred, on-demand CSG evaluation,
superseding spec 013's resolution).``

New: ``, ..., `021-deferred-csg-evaluation` (deferred, on-demand CSG evaluation,
superseding spec 013's resolution), `030-hider-and-integrator-registry` (hider and integrator registries, the Transport domain).``

## 5. Known gotchas: gotcha 35, first sentence (replace)

Old: ```CRenderer::contexts` is a `CHider **` that kept its name, because `reyes.cpp`'s DB-011 line is keyed on the symbol; reach the context with `contexts[i]->shadingContext()`.``

New: ```CRenderer::contexts` is a `CHider **` that kept its name (spec 030 deleted `reyes.cpp`'s DB-011 line, so no allowlist key depends on it now; `reyes.cpp` reaches its peers through the frame's `CHiderPeers`); reach the context with `contexts[i]->shadingContext()`.``

## 6. Known gotchas: a new gotcha 37 (insert after gotcha 35, or after 36 once master is merged)

```
37. **Hiders and integrators are registered tables; the core reads declared capabilities, never a name (spec 030).** The four built-in
   hiders are reached by the *call graph* (`beginRenderer()` -> `hiderRegistry()` -> `registerBuiltinHiders()` -> each hider's getter),
   not by a static registrar, so a plain static link finds them with no `-u` anchor (`Link_StaticConsumerRegistries`); only a hider or
   integrator outside the core uses `OR_REGISTER_HIDER`/`OR_REGISTER_INTEGRATOR`, whose pending list is defined once, out of line,
   in the registry's `.cpp` (a header-reached static would give a shared-library consumer its own copy: gotcha 22). `RiHiderV`
   resolves a hider name at statement time (an alias is stored as its target's name, a deprecated alias warns once per statement, an
   unknown name is stored as written); `beginFrameTransport()` resolves again at frame begin. There are two hiders in the slot:
   `named` (the registration of the name as written, `nullptr` if unknown) and `hider` (the resolved one, `reyes` for an unknown
   name): the traits come from `hider`, except `deepShadowOutput`, which comes from `named`, so a TSM display with an unknown hider
   name still fails (it asks for reyes, not for the fallback). The `hiderFlags` description bits are gone (`HIDER_BREAK` and
   `HIDER_DONE` stay); `hasIlluminationHook()` reads `frameTransport().traits.illuminationHook`, a plain `bool`, and tests that need
   the hook outside a frame use `setFrameIlluminationHookForTest()`. An (hider, integrator) pair whose requirements are not met
   **refuses** the frame in `beginFrame()` before the displays start: one `CODE_LIMIT` error, no hider built (`contexts` stays
   `NULL`), no output file, `renderFrame()` returns at once, and every reader of `contexts` is null-safe (`imager.cpp`,
   `notifyGlobalVariable`, `addObject` through `traits.rasterization`); a failure after the displays began (a hider `create` that
   returns `NULL`, an integrator `prepare` that fails) **aborts** instead (`abortFrameTransport()`: hiders destroyed, traits cleared,
   the integrator destroyed without `finish`). `CReyes` keeps a pointer to the slot's `CHiderPeers` (its sibling loop), so the peers
   live in the slot until `endFrameTransport()`, never in a local. A capability is met when the same name is provided at a version
   `>=` the required one; absent optional capabilities never refuse a pair. Integrator parameters live in inline declarations
   (`"int maxdepth" [5]`): the RIB rule `integratorParameterCheck()` (not `parameterListCheck()`) converts integers and rewrites a
   declaration to the count of values it was given, and a parameter the integrator does not define is an *error* at frame begin
   (`orender` exits -1). A named `spherelight`/`quadlight` with no compiled shader on the path now reports `Failed to find shader`:
   the C++ substitutes (`hcshader`) are gone.
```

## 7. Active Technologies (insert after the spec 029 entry)

```
- C++20 and C17 (the registration headers `hiderAbi.h`, `integratorAbi.h` and `capability.h` are strict C17), CMake ≥3.19,
  `-Werror`; no new dependencies: the hider and integrator registries over `common/kindRegistry.h`, the Transport domain
  (`src/ri/transport/`), the `Integrator` statement, the renderer core's `frameTransport()` slot (030-hider-and-integrator-registry)
- Storage: N/A; memory only (030-hider-and-integrator-registry)
```

## 8. Recent Changes (insert as the first entry)

```
- 030-hider-and-integrator-registry: hiders are registered tables (`hiderRegistry()`, name, aliases, declared capabilities, dispatch,
  `create`/`destroy`) and the renderer core selects one per frame without naming a concrete hider (the two `strcmp` chains, the
  `dynamic_cast`, the TSM name test and the `hiderFlags` description bits are gone; three DB-011 allowlist lines and one DB-006
  line deleted, 164 -> 161 and 30 -> 29); integrators are the new Transport domain (constitution 1.4.0, `src/ri/transport/`) with a
  registry, the capability vocabulary, a pair check, the built-in `classic` and an `Integrator` statement (RIB, `RiIntegrator`,
  `ribOut`, Python and Lua helpers; preview ignores it); an unsupported pair refuses the frame with one error, a parameter the
  integrator does not define is an error; the hard-coded `spherelight`/`quadlight` C++ lights (`hcshader`) are retired (a missing
  shader is now an ordinary `Failed to find shader`); `checkPluginVersion` gained an overload for in-process tables. DB-013 resolved.
  No image changed on either platform; both determinism sweeps match the starting tree's (see spec 030).
```
