# Research: Hider and Integrator Registry (spec 030)

**Date**: 2026-11-07 | **Base**: master `e83d005d` | **Spec**: [spec.md](spec.md)

Method: static reading of the worktree at `e83d005d` (no build or test was run for the plan stage), the
spec 022 registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`), the reference document
(`DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §0 D1/D2/D7/D8/D21/D28/D30, §1.2, §5.1-5.5, §9), the
constitution v1.3.0 and the spec texts of the sibling specs 031 and 032. Every decision below is
recorded as Decision / Rationale / Alternatives. Line numbers are those of `e83d005d`.

The clarify answers in spec.md are decisions, not inputs to re-open; where this file makes a choice the
spec left to the plan, it says so.

---

## R1. Where the hider registry lives and how built-ins get into it

**Decision.** The hider registry is a Hiders class, `CHiderRegistry` (`src/ri/hiders/hiderRegistry.{h,cpp}`),
with one process instance returned by `hiderRegistry()` (a function-local static, so there is no static
initialization order problem). It is filled once, under `std::call_once`, on first access:

1. **Built-ins first, in a fixed order** (`raytrace`, `reyes`, `zbuffer`, `photon`): the registry calls
   `registerBuiltinHiders(CHiderRegistry &)` in `src/ri/hiders/builtinHiders.cpp`, a Hiders file that
   calls each hider's own table getter (`orHiderTableRaytrace()`, ...). Each table, with its name,
   aliases, capabilities and factory, is defined **in that hider's own source file** (FR-002).
2. **Then the static registrars** of hiders that are not built in (a test double, a program's own
   hider): `OR_REGISTER_HIDER(tableGetter)` (in `hiderAbi.hpp`) defines a file-local object whose
   constructor calls **`orHiderAddPending(tableGetter)`, an out-of-line function defined once in
   `hiderRegistry.cpp`**, which appends the getter to a pending list (a function-local static inside that
   translation unit; no diagnostic, no lookup, no allocation beyond the vector). Pending entries are
   processed in the order they were appended. The pending list must never be a static reached from the
   header itself: a test or program linked against the *shared* `libRIBRender` would then get its own
   copy of it, register into that copy, and the library's registry would never see the entry (the same
   class of defect as gotcha 22, process-wide state duplicated across copies).
3. Every entry is validated as it is added (R2); refusals are reported then.

The core touches the registry first in `CRenderer::beginRenderer()` (one call, `hiderRegistry().size()`),
so registration diagnostics appear once per process at the first `RiBegin`, through the sink that is
installed by then.

**Rationale.**
- Static-link survival (FR-006, gotcha 31). Once `renderer.cpp` stops writing `new CStochastic(i)` and
  the others, nothing in a plain static link references `stochastic.cpp.o`, `zbuffer.cpp.o`,
  `raytracer.cpp.o` or `photon.cpp.o`, so per-file static registrars alone would be dropped silently from
  `ribRender_static`. With the built-in list reached by the call graph (core → `hiderRegistry()` →
  `registerBuiltinHiders()` → each table getter), the linker keeps every built-in object by construction:
  no `-u` option, no force-load. The core already references the registry, so the registry object is
  always linked.
- Determinism: built-ins register in a fixed order, so "the first registration stays" (FR-004) is
  well defined between them; static initialization order across translation units is unspecified and is
  used only for the non-built-in entries, whose order is their append order.
- The core names no concrete hider (FR-007): `builtinHiders.cpp` is in Hiders, naming its own members.
  Adding a built-in hider edits Hiders files and the CMake source list, never a renderer-core source
  (FR-003). Adding an outside hider (test, program) needs only `OR_REGISTER_HIDER` in its own file,
  because an object file given directly to the linker is always linked.

**Alternatives considered.**
- *Per-file static registrars plus one `-u` anchor per built-in in `ribRender_static`'s INTERFACE link
  options* (gotcha 31's idiom, `ribrender_context_anchor`): works, but each new built-in needs a CMake
  anchor line that a contributor can forget, with a silent failure only a static link shows, and the
  built-in order would be link order. Rejected.
- *A table of built-ins in the core*: forbidden by FR-007 and DB-013's point.
- *Registration at first lookup inside `beginFrame`*: registration diagnostics would then depend on the
  first frame and would be reported inside a world block. Rejected for the explicit touch in
  `beginRenderer()`.

The integrator registry (`CIntegratorRegistry`, `src/ri/transport/integratorRegistry.{h,cpp}`,
`registerBuiltinIntegrators()` in `builtinIntegrators.cpp`, `OR_REGISTER_INTEGRATOR`) is built the same
way (FR-010). The shared name/alias bookkeeping is one header-only template in Infra,
`src/common/kindRegistry.h` (`TKindNameTable<Entry>`: names, aliases, collision checks, enumeration), so
the two registries share the rules without Hiders and Transport sharing code across a boundary.

---

## R2. Names, aliases and their validation

**Decision.**
- An alias record is `{ name, deprecationWarning, integrator }`: `deprecationWarning` is `NULL` for a
  plain alias and otherwise the exact warning text; `integrator` is the reserved pair field of FR-014
  (R7).
- Validation at registration, each refusal one `error(CODE_BADTOKEN, ...)` that names both entries, the
  first registration staying (FR-004):
  - a name equal to a registered name or alias: `Hider "<n>" is already registered (by "<first>"); the second registration is ignored`;
  - an alias equal to any registered name or alias: same text with "alias";
  - an alias whose target is missing cannot happen in this table shape (an alias belongs to the table
    that declares it, so its target is the table's own name). The spec's edge case "an alias that points
    at a name registered later or never" is therefore refused at the table level: a table with a `NULL`
    or empty name is refused (`Hider registration without a name ignored`), and that is the only way
    to have an alias without a target. Recorded so the tasks write that test instead of an impossible
    one.
- Matching is exact and case-sensitive (edge case, unchanged from today).
- **Aliases resolve at statement time**, in `CRendererContext::RiHiderV`, exactly where today's
  `"hidden"`/`"stochastic"` mapping is (`rendererContext.cpp:1544-1564`): a registered name or alias stores
  the canonical name in `COptions::hider`; a deprecated alias reports its own warning text once per
  statement (`warning(CODE_BADTOKEN, ...)`, today's text for `stochastic`:
  `Deprecated Hider "stochastic"; use "reyes" instead`); an unknown name is stored unchanged and
  diagnosed at frame begin (FR-005). `RI_HIDDEN` stays the RISpec token; the `hidden` alias lives in the
  `reyes` table with no warning.

**Rationale.** Statement-time resolution keeps every observable of today: the warning appears at the
statement, once (not once per thread or per frame, FR-023); `CRenderer::hider` holds `reyes` for both
aliases, so messages that print the hider name and the TSM check behave as today; an unknown name keeps
its text for the `Hider "bogus" unavailable` error and for the TSM check (R4).

**Alternatives considered.** Resolving aliases only at frame begin would move the deprecation warning from
the statement to `RiWorldBegin` and repeat it per frame. Rejected (behaviour change).

---

## R3. The capability vocabulary

**Decision.** A capability is `OrCapability { const char *name; uint32_t version; }` with the version in
the `OR_ABI_VERSION` encoding (`src/ri/transport/capability.h`, Transport, C17). A requirement is met when
the hider declares the same name with `provided >= required` (a plain comparison of the encoded value).
The vocabulary of this spec is exactly what the four built-in hiders and `classic` declare or what the
core reads, all at 1.0.0:

| Name | Meaning (absence = the hider is limited in that respect) | reyes | zbuffer | raytrace | photon | Replaces |
|---|---|---|---|---|---|---|
| `rasterization` | the hider rasterizes the objects the core submits to it | yes | yes | - | - | `dynamic_cast<CReyes *>(contexts[0])` (`renderer.cpp:1022`) |
| `display-output` | the hider produces display output | yes | yes | yes | - | `HIDER_NODISPLAY` (`photon.cpp:118`, read at `rendererDisplay.cpp:201`) |
| `display-channels` | arbitrary display channels and AOVs, not only RGBAZ | yes | - | yes | - | `HIDER_RGBAZ_ONLY` (`zbuffer.cpp:90`, read at `rendererDisplay.cpp:627, 643`) |
| `deep-shadow-output` | deep shadow (TSM) output | yes | - | - | - | `strcmp(hider, "reyes") != 0` (`rendererDisplay.cpp:770`) |
| `illumination-hook` | the hider drives lights through the illumination hook | - | - | - | yes | `HIDER_ILLUMINATIONHOOK` (`photon.cpp:118`, read at `rendererServicesImpl.h:61`) |

`classic` requires nothing and uses nothing optionally (FR-013: it pairs with all four). The names of
the reference document's §5.3 that no built-in declares today (`ray-intersection`, `ray-occlusion`,
`light-sampling`, `surface-closures`, `derivatives`, ...) are **reserved** and listed in the capability
header's comment; the spec that needs one adds it as a MINOR step of the capability vocabulary (it is part
of the integrator interface's version history, R10).

Two flags are not carried over:
- `HIDER_GEOMETRYHOOK` (`renderer.h:52`): defined, never used (spec R14, clarify answer).
- `HIDER_PHOTONMAP_OVERWRITE` (`renderer.h:51`): set by `CPhotonHider::preDisplaySetup()` and **never
  read anywhere in the tree** (`grep -rn HIDER_PHOTONMAP_OVERWRITE src` finds only its definition and
  the write). FR-008 lists it among the writes to replace; replacing a write nobody reads means deleting
  it. The plan deletes it and does not invent a capability no code reads (FR-009: the vocabulary is
  limited to what is declared *or needed*). **Maintainer answer Q3 (2026-11-07)**: deletion accepted, no
  replacement capability (set-only at `photon.cpp:118`, defined at `renderer.h:51`).

**Rationale.** String plus version, not bits (FR-009, §5.3 "capabilities carry versions"); every row is a
property the code reads today, so the table is checkable against the starting tree (US1 AS4).

**Alternatives considered.** Declaring `ray-intersection` for all four hiders now (every `CHider` is an
`ITracer`, so it is true): rejected for this spec, because nothing requires or reads it until spec 042,
and FR-009 asks for the minimal set. Bit masks (§5.3's enum): rejected by FR-009.

---

## R4. Replacing `hiderFlags` with per-frame traits; the TSM check

**Decision.**
- A core-owned per-frame slot, `SFrameTransport` (`src/ri/render/rendererTransport.{h,cpp}`), built in
  `beginFrame()` where the first `strcmp` chain is today (before `beginDisplays()`), torn down at the end
  of `endFrame()` (after `endDisplays()`, so the imager and anything run by the display finish still read
  it). It holds:
  - `named`: the registration of `CRenderer::hider` *as written* (`nullptr` for an unknown name);
  - `hider`: the resolved registration (`named`, or `reyes` after the unknown-name error);
  - `integratorNamed` / `integrator`: the same for the integrator (R8);
  - `traits`: booleans computed once from `hider`'s capabilities: `rasterization`, `displayOutput`,
    `displayChannels`, `illuminationHook`; and `deepShadowOutput` computed from **`named`** (false for an
    unknown name);
  - `refused` (R8), the integrator instance, and the peer group (R5).
- The four read sites read the traits: `rendererDisplay.cpp:201` (`!traits.displayOutput || refused`),
  `:627` and `:643` (`!traits.displayChannels`), `:770` (`!traits.deepShadowOutput`, same message text),
  `rendererServicesImpl.h:61` (`traits.illuminationHook`, a plain `bool` load: it is queried at every
  `illuminate`/`solar` begin and end, so it must not be a string lookup, SC-010).
- `CRenderer::hiderFlags` stays for the frame-state bits `HIDER_BREAK` and `HIDER_DONE` (FR-008), with
  their values (2 and 8) unchanged. `HIDER_RGBAZ_ONLY`, `HIDER_NODISPLAY`, `HIDER_ILLUMINATIONHOOK`,
  `HIDER_PHOTONMAP_OVERWRITE` and `HIDER_GEOMETRYHOOK` are deleted from `renderer.h`.
- The hiders' static `preDisplaySetup()` functions are deleted: after FR-008 none has a body left
  (`CRaytracer` and `CStochastic` are empty inline functions, `CZbuffer` and `CPhotonHider` only set the
  flags). The table keeps the `preDisplaySetup` slot (FR-011), `NULL` for all four built-ins, called by
  the core before `beginDisplays()` when non-`NULL`.

**Rationale.** The TSM edge case: an unknown hider name falls back to the reyes hider, but today the TSM
check still errors for it because `CRenderer::hider` keeps the bad name. Reading `deepShadowOutput` from
the *named* entry (none) preserves that exactly; reading it from the resolved entry would let the
fallback pass the check. The other traits follow today's behaviour for an unknown name, which runs
`CStochastic::preDisplaySetup()` (no flags), i.e. the resolved reyes kind's capabilities.

`hasIlluminationHook()` changes its body only; `CRendererServices` is not changed, so
`kRendererServicesVersion` does **not** step (and spec 032, which may step it for its own signature
changes, gets no double step from this spec).

**Alternatives considered.** Translating the capabilities into the old bits inside the core at frame begin
(`hiderFlags |= HIDER_NODISPLAY` when `display-output` is absent): fewer lines changed, but keeps a
mutable bitfield as the carrier of a static property, which DB-011's direction asks to remove. Rejected.
Putting the traits in Graphics State's `FrameParams`: Graphics State may not name Transport's
capabilities, and spec 031 adds fields to `FrameParams` (collision). Rejected (R15).

---

## R5. Reyes reaching its sibling threads: the peer group

**Decision.** A Hiders-owned, header-only `CHiderPeers { CHider *const *members; int count; }`
(`src/ri/hiders/hiderPeers.h`). The core owns one per frame inside `SFrameTransport`, pointing at the
`contexts` array (allocated before the construction loop, so the pointer is stable) with
`count = numThreads`, and hands its address to every per-thread hider through the create info (R6).
`CReyes` stores it (`const CHiderPeers *peers`), and the sibling loop at `reyes.cpp:1992-1993` reads
`static_cast<CReyes *>(peers->members[i])` instead of `CRenderer::contexts[i]`. The loop bound and the
five other `CRenderer::numThreads` reads in `reyes.cpp` (`:1255, :1281, :1989, :2078, :2080`) are **not**
changed.

Effect: the allowlist line `DB-011 link src/ri/hiders/reyes.cpp -> _ZN9CRenderer8contextsE`
(`allowlist.txt:211`) is deleted (FR-008 MUST). The optional `numThreads` line (`:176`) stays.

**Rationale.** FR-008 makes `contexts` a MUST and `numThreads` optional. Spec 031's FR-028 introduces a
frame-limits value that carries the thread count and thread index for the hiders; removing `numThreads`
here through the peer group would build a second carrier of the same value and edit the same `reyes.cpp`
lines 031 rewrites. Leaving it to 031 avoids the collision; the peer group's `count` field exists, so a
later spec can move the reads to it if 031 does not. Value identity: `members[i]` is `contexts[i]`, so
the insertion order and every image are unchanged.

**Alternatives considered.** (a) A `CHider` base member set by a setter after construction: edits the
`CHider` base, which spec 032 also edits (FR-027); rejected in favour of a `CReyes` constructor argument.
(b) Deleting the `numThreads` line too: rejected as above (overlap with 031); recorded as a deliberate
non-goal, not an oversight.

---

## R6. The hider registration table and the object-submission call

**Decision.** The registration table (contract: [contracts/hider-registration.md](contracts/hider-registration.md))
carries name, aliases, provided capabilities, dispatch kind (`OR_HIDER_DISPATCH_BUCKETS` /
`OR_HIDER_DISPATCH_PHOTONS`), optional `preDisplaySetup`, and `create`/`destroy` over an opaque
`OrHiderInstance *`. The create info carries the thread index, the thread count and an opaque
`const OrHiderFrame *`; for the built-in (internal C++) hiders the C++ convenience header turns the frame
into `SHiderFrame { const CHiderPeers *peers; const openrender::ICamera *camera; CAttributes *(*acquireWorldAttributes)(void *host); void *host; }`
and the opaque instance into the `CHider *` it is (`hiderOf(OrHiderInstance *)`), documented as internal
until spec 051 (FR-011, clarify answer).

Byte-identity details of the factories:
- `raytrace`: `new CRaytracer(thread, *frame.camera)`, where the core passes `frameCamera()`
  (`rendererFrame.h:38`, Core; the raytracer cannot call it itself).
- `reyes`: `new CStochastic(thread, *frame.peers)`; `zbuffer`: `new CZbuffer(thread, *frame.peers)`.
- `photon`: `new CPhotonHider(thread, frame.acquireWorldAttributes(frame.host))`. The core's callback
  calls `context->getAttributes(TRUE)`, **once per thread and only for the photon hider**, exactly as
  today; calling it for every hider (passing a `CAttributes *` in the frame) could copy the world
  attribute block for hiders that never asked for it today.
- The dispatcher: `dispatchJob = dispatch == BUCKETS ? dispatchReyes : dispatchPhoton`, set as today
  inside the per-thread loop. The unknown-name fallback is the resolved `reyes` entry, so it gets
  `dispatchReyes` and a `CStochastic`, as today.

**Object submission.** `CHider` gains `virtual void drawObject(CObject *) {}` (default no-op);
`CReyes::drawObject` becomes its override (signature unchanged, `reyes.h:247`). `CRenderer::render()`
replaces `dynamic_cast<CReyes *>(contexts[0])` with
`if (frameTransport().traits.rasterization) contexts[0]->drawObject(cObject);`.

**Rationale.** The cast is the only concrete-type use left in `renderer.cpp`; a virtual with a no-op
default plus the declared `rasterization` capability expresses the same rule (reyes-family hiders take
submitted objects, the others reach objects through the intersection hierarchy). One line in `CHider`
and one call site: spec 031, which moves `drawObject` onto its Geometry-owned dicing interface, has one
line to rewrite whichever spec merges second.

**Alternatives considered.** A `submitObject` slot in the C table: puts a Geometry type into a
registration-level C table that is meant to stay runtime-free until 051. Rejected.

---

## R7. The integrator interface and `classic`'s lifecycle

**Decision.** Contract: [contracts/integrator-interface.md](contracts/integrator-interface.md). The table
carries name, aliases, required and optional capabilities, `create`/`destroy` over an opaque
`OrIntegratorInstance *`, and three lifecycle slots whose signatures are pinned at 2.0.0 so spec 042
extends them by MINOR steps (FR-016, §5.1):

```c
OrStatus (*configure)(OrIntegratorInstance *, const OrIntegratorSettings *settings, int *acceptedParameters);
OrStatus (*prepare)(OrIntegratorInstance *, const OrTransportHost *host, const OrIntegratorFrame *frame);
void     (*finish)(OrIntegratorInstance *, const OrTransportHost *host);
```

- `OrIntegratorSettings` (structSize-headed): the resolved hider's name and provided capabilities (so an
  integrator sees an absent optional capability, US4 AS4), the parameter list (R9).
- `OrTransportHost` and `OrIntegratorFrame` are structSize-headed and carry no members beyond the size in
  2.0.0: they are the places where 042 adds the services facade (§5.4, §5.7) and the frame view, as
  appended members, without changing a slot's signature.
- The `render` slot of §5.1 is **not** in 2.0.0; 042 appends it to the table in a MINOR step (the
  table's `structSize` tells the host whether a module has it, `checkPluginTable`).
- `classic`: requires none, optional none; `create` returns a static instance; `configure` accepts no
  parameter (every `acceptedParameters[i] = 0`); `prepare` and `finish` return at once. It counts its
  three calls in three atomics exposed to tests only through
  `transportClassicLifecycleCounts()` (`src/ri/transport/classicIntegrator.h`) so SC-006's "observed
  once per frame" is testable on `classic` itself.

Call sites (core, `rendererTransport.cpp`, called from `renderer.cpp`): `create` + `configure` in
`beginFrame()` right after the pair check passes; `prepare` after the per-thread hiders exist (end of
`beginFrame()`); `finish` + `destroy` at the start of `endFrame()`, before the hiders are deleted. On a
refused frame none of them is called.

Today's transport call sites (`illuminance`, `trace`, `gather`, `indirectdiffuse`, the photon hook) are
untouched (clarify answer): `classic` does nothing, so every image is unchanged by construction.

**Alternatives considered.** Putting `render` in 2.0.0 as an optional `NULL` slot: it would freeze a
signature (§5.2's `RenderWork`) that 042 has not designed. Rejected; the slot is added when it is
designed. Counting lifecycle calls in the core instead of in `classic`: would not prove `classic`'s own
slots are the ones called. Rejected.

**FR-014 pair alias.** The alias record carries `const char *integrator`. In 2.0.0 a non-`NULL` value is
not applied: the registry reports `warning(CODE_BADTOKEN, "Hider alias \"%s\" names integrator \"%s\"; pair aliases are not supported yet, the alias selects the hider only")`
once at registration and keeps the alias. Spec 050 gives the field its meaning (a MINOR step) and
decides precedence against an explicit `Integrator`.

---

## R8. The pair check and the refused frame through `RiWorldEnd`

**Decision: refuse before anything frame-specific is built, then let the world block run against an
empty frame.** This is the clarify-stage open item; it proved **not too invasive**: five core sites,
no hider, Geometry or Shading file. **Maintainer answer Q1 (2026-11-07)**: approved as written.

Order in `beginFrame()` at the place of the first `strcmp` chain (`renderer.cpp:777`):
1. Resolve the hider: `named = registry.find(hider)`; if `nullptr`,
   `error(CODE_BADTOKEN, "Hider \"%s\" unavailable\n", hider)` (today's text) and `hider = reyes entry`.
2. Resolve the integrator from `COptions::integrator`: if unknown,
   `error(CODE_BADTOKEN, "Integrator \"%s\" unavailable; registered: %s. Using \"classic\"\n", ...)`
   (lists the registered names, FR-015/FR-024) and `integrator = classic`.
3. Check the pair (`transportCheckPair`, Transport, pure function over two capability arrays). On a
   shortfall: one `error(CODE_LIMIT, "Hider \"%s\" cannot run integrator \"%s\": %s; the frame is not rendered\n", ...)`
   listing every missing capability (`missing "C" 1.0.0`) and every version shortfall
   (`"C" 1.0.0 provided, 2.0.0 required`), and `refused = true`. `CODE_LIMIT` is the code of the TSM
   check, the closest existing "this combination cannot render" diagnostic.

On a refused frame:

| # | Site (all Core) | What happens |
|---|---|---|
| 1 | `CRenderer::beginFrame()` | no `preDisplaySetup`, `beginDisplays()` runs in its no-display branch (the path `Hider "photon"` takes every frame, so `numSamples = 0` and no driver is opened), **no per-thread hider is constructed** (`contexts` stays `NULL`), no integrator is created. Everything before (texture system, shadeops, `FrameParams`) is set up as usual so the world block's statements work and `endFrame()` tears down what it expects. |
| 2 | `CRenderer::render()` | `drawObject` is reached only through `traits.rasterization`, which a refused frame clears; raytraced objects still join `root`, unraytraced ones are released by `addObject`'s own `o->detach()` (`rendererContext.cpp:771`), as a culled object is. Nothing leaks. |
| 3 | `CRenderer::renderFrame()` | returns at once when the frame is refused (before `root->setChildren(&contexts[0]->...)` at `renderer.cpp:1062`). No thread is spawned, so `dispatchPhoton` (which never reads `HIDER_BREAK`) is never reached. |
| 4 | `CRenderer::endFrame()` | the delete loop runs only when `contexts != NULL`; no `finish`. `root->destroy()`, the procedural modules, `endDisplays()` (no display started), remote channels and photon maps are released as usual. |
| 5 | `RiWorldEnd` | unchanged: it calls `renderFrame()` (now a no-op for the frame) and `endFrame()`. |

Already null-safe and unchanged: `CRenderer::notifyGlobalVariable` (`rendererDeclarationsDispatch.cpp:76`,
guards `contexts != NULL`, so a `Declare` inside a refused world block is safe; spec 031 deletes this
file, which is why this spec does not touch it), the imager's context lookup (`imager.cpp:133`).
`CRenderer::activeContext` is set only by `rendererDispatchThread`, which a refused frame never runs.

Process and output: the refusal is an `error()`, not `severe()`, so the process continues (FR-024);
`RiLastError` is set, so `orender` exits -1 at the end, as for any error. No image file is written
(no driver is started) and no photon map is written (no `CPhotonHider`, whose destructor is the only map
writer, exists). The next frame (`RiFrameBegin`/`RiFrameEnd`) checks its own pair.

Network rendering: servers run the same `beginFrame()` with the same options and registry, so a refused
frame is refused on every node and neither the client nor a server enters the bucket protocol. Not
testable without a network harness; recorded as a reasoned claim, not a measured one.

**Late failures are aborts, not refusals.** The "no output file" guarantee holds because the pair check
and `configure` run before `beginDisplays()`. A `create` returning `NULL` or a `prepare` returning non-OK
is detected after the displays have started: the core then destroys the hiders built so far at once,
sets `contexts = NULL`, clears the traits and sets `HIDER_BREAK`, which puts the frame in the refused
state above (no thread, null-safe paths), except that `endDisplays()` finishes the started drivers as in
today's "no active display" abort. Neither path is reachable with the built-ins; the contracts state it.

**Rejected alternatives.**
- *Construct the resolved hider and set `HIDER_BREAK`* (the "no active display" abort path): the photon
  constructor ORs `ATTRIBUTES_FLAGS_PRIMARY_VISIBLE` into `raytracingFlags`, `dispatchPhoton` never reads
  `HIDER_BREAK` (photons would be traced), `~CPhotonHider` writes the maps it balanced, and the raytrace
  and reyes constructors size buffers from `numSamples`/`sampleOrder`/`numExtraSamples`, which nothing
  exercises with no displays. Rejected.
- *Have `CRendererContext` ignore every world-block call while a frame is refused*: touches every `Ri*`
  entry that can occur in a world block. Too invasive; rejected.
- *A hidden "null hider" constructed for the refused frame*: the core would name a concrete hider class
  (FR-007) or the registry would carry an unselectable entry. Rejected.

Verification belongs to the tasks: one refusal test per built-in hider (a test-double integrator
requiring a capability none provides), each with geometry and a `Declare` in the world block, run in a
Debug build too (the `stats.numRasterGrids/Objects == 0` asserts of `endFrame()`), asserting the
diagnostic, no output file, no crash, a following frame that renders, and exit -1 for `orender`.

---

## R9. `Integrator` parameters: storage, validation, the reserved `handle`

**Decision.**
- Storage (Graphics State, which knows no integrator): `COptions` gains `char *integrator` (default
  `"classic"`) and `int numIntegratorParameters; COptions::CTypedParameter *integratorParameters`, each
  `{ char *name; int type /* TYPE_* of common/variable grammar */; int numItems; void *data; }`, deep-copied
  by the `COptions` copy constructor and freed by its destructor, exactly as `CDisplay::parameters` are.
  The statement's last occurrence before the frame replaces name and list together (FR-018).
- Parsing: `CRendererContext::RiIntegratorV` resolves each token as `RiDisplayV` does
  (`rendererContext.cpp:1397-1460`): a pre-declared variable or an inline declaration
  (`"int maxdepth" [5]`); an undeclared plain name is rejected before it reaches the call by the existing
  parser rule (`Parameter not declared`). No core pre-declaration list for integrator parameters (FR-020).
- `handle` (FR-020): a parameter named `handle` of type string is consumed by the core (stored as
  `COptions::integratorHandle`, not passed to the integrator, no diagnostic); a `handle` of any other type
  is `error(CODE_BADTOKEN, "Integrator parameter \"handle\" must be a string")` at the statement. So a
  later RIS-style handle needs no grammar change, and no integrator can define `handle` differently.
- Validation at frame begin (FR-021): the core passes the list (minus `handle`) to `configure`, which fills
  `acceptedParameters[i]`; the core reports every unaccepted one:
  `error(CODE_BADTOKEN, "Integrator \"%s\" does not define parameter \"%s\"; ignored\n", ...)`. The frame
  renders. `error` (not `warning`) matches the existing severity of an unknown option
  (`Unknown %s option: "%s"`, `rendererContext.cpp:1765`), so `orender` exits -1 for it as it does for
  any unknown option. Diagnostics are reported by the host, not the integrator, so every integrator gets
  the same text.

**Alternatives considered.** Reusing `CDisplay::TDisplayParameter`: its type enum is Display Outputs'
vocabulary mirrored in Graphics State (`stateVocabulary.h`), kept equal to the display ABI by a test;
integrator parameters would couple to it. Rejected for a `TYPE_*`-typed record. Reporting unknown
parameters as warnings: would make `orender` exit 0 where an unknown option exits -1. **Maintainer answer Q4
(2026-11-07)**: errors, as decided (`orender` exits -1, like an unknown `Option`).

---

## R10. Versions, families and the version check

**Decision.**
- New interfaces, each with a version-history table in its family header:
  - hider registration: `OR_HIDER_ABI_VERSION = OR_ABI_VERSION(2, 0, 0)`, `src/ri/hiders/hiderAbi.h`;
  - integrator interface, including the capability vocabulary: `OR_INTEGRATOR_ABI_VERSION = OR_ABI_VERSION(2, 0, 0)`,
    `src/ri/transport/integratorAbi.h`.
  Both start at **2.0.0**, with history row "1: none; the hider was chosen by name inside the renderer
  core (unversioned)" / "1: none; no integrator existed". Rule recorded in both headers: until spec 055
  MAJOR stays at 2 and 2.x is unstable; a breaking change steps MINOR and says so in its row; a
  comment-only change gets no step (project policy 2026-11-02). Starting at 1.0.0 would force a MAJOR
  step before the milestone ends at MAJOR 2; starting at 2.0.0 follows the `CRendererServices` precedent
  (2.0.0, version 1 being the unversioned interface). **Maintainer answer Q2 (2026-11-07)**: both start at 2.0.0, version 1
  meaning "chosen by name in the core" / "no integrator".
- One `SPluginFamily` per interface (FR-011): `{"hider", "orHiderAbiVersion", OR_HIDER_ABI_VERSION, {2.0.0}, 1, true}`
  and `{"integrator", "orIntegratorAbiVersion", ...}`, defined beside each registry.
- The check. `checkPluginVersion(family, module, path)` resolves the version symbol in a loaded module;
  a built-in has no module, and calling it with `module == nullptr` reports "declares no interface
  version" (`pluginAbi.cpp:98-105`). Decision: `pluginAbi.{h,cpp}` gains one overload,
  `checkPluginVersion(const SPluginFamily &, uint32_t declaredVersion, const char *origin)`, and the module
  overload is refactored to resolve the symbol and call the same verdict code (same rules, same cache keyed
  by interface and origin, same diagnostics). The registry calls it with the table header's `abiVersion`
  and origin `"built-in hider \"reyes\""` (or the registrar's `__FILE__` for a static registrar), then
  `checkPluginTable` for the size rule. A rejected table is not registered. Gotcha 27 holds: the verdict
  rules stay in one routine; the families add entries.
- Neither interface is dlopened in this spec (run-time loading is spec 051), so the policy question
  "ask before a breaking MINOR on a dlopened family" does not arise; no existing dlopened family
  (display, shadeop, dlo, implicit, dynamicload) changes. `orplugin.h`/`.hpp` are unchanged.
- The new headers are **not** added to `OPENRENDER_PUBLIC_HEADERS` (not installed): they become external
  extension points with spec 051 (D30 (3)). The `PluginAbi_*` install-layout tests are therefore unchanged.

**Alternatives considered.** (a) Built-ins checked by `checkPluginTable` only, `checkPluginVersion` unused
until 051: does not meet FR-011's "checked by `checkPluginVersion`"; rejected. **Maintainer answer Q5
(2026-11-07)**: the `checkPluginVersion` overload, as decided (one version check, gotcha 27). (b) A second, registry-local version check:
forbidden by gotcha 27.

---

## R11. The Transport domain: directory, build, check, ordering

**Decision.**
- Directory `src/ri/transport/`, compiled into `ribRender` (shared and static) like `src/ri/hiders/`.
  Files: `capability.h`, `capabilityCheck.{h,cpp}` (`transportCheckPair`), `integratorAbi.h`,
  `integratorAbi.hpp`, `integratorRegistry.{h,cpp}`, `builtinIntegrators.cpp`,
  `classicIntegrator.{h,cpp}`. `src/ri/CMakeLists.txt`: `transport/*.h` joins the `ri_headers` glob, the
  `.cpp` files join `ri_sources`; the new Hiders, Transport and Core sources join `ribVector_sources`'
  `REMOVE_ITEM` list (`libRIBVector` renders nothing; `builtinHiders.cpp` references hider objects it
  already removes).
- `tests/architecture/domain-map.txt`: `prefix src/ri/transport/ Transport`; the line
  `prefix src/ri/hcshader. Core` is deleted (FR-025).
- `tests/architecture/domain-rules.txt`: `Transport -> GraphicsState Geometry Shading`; `Hiders -> GraphicsState Geometry Shading Display Transport`;
  header and source line brought to the amended constitution version (v1.4.0 if the amendment is the
  MINOR step FR-029 asks; R6 of spec.md: the header said v1.2.0 while the constitution was v1.3.0, so the
  edit corrects both at once).
- `archcheck` knows its classes by a compiled enum (`tests/architecture/archcheck/config.h`, `kClassCount = 12`,
  `config.cpp` `kClassNames`, the help text in `main.cpp:76`): `Transport` is added there (13 classes,
  and it takes part in the acyclicity check like the other domains). `metrics-baseline.txt` gains a
  `Transport 0 0` row if the metric report lists every domain class.
- New fixtures (`tests/architecture/fixtures/`): `transport-allowed` (Transport → Shading and Hiders →
  Transport pass) and `transport-cycle` (Transport → Hiders fails as a `VIOLATION` and a `CYCLE`).
- The constitution amendment (FR-029) is written at implement through `speckit-constitution`, with the
  maintainer's approval, as a MINOR step 1.3.0 → 1.4.0: the domain table gains the Transport row, the
  allowed-dependency table gains `Transport | Graphics State, Geometry, Shading` and `Hiders | ..., Transport`,
  the plan template's Principle VIII checklist names ten domains, with a Sync Impact Report. This plan only
  lists it.

**Ordering gate (for tasks).** A source file under `src/ri/transport/` makes `archcheck` exit 2
(unclassified) until `domain-map.txt` names the prefix, and a Hiders → Transport include fails the check
until `domain-rules.txt` allows it; both data-file edits follow the amendment. So the first Transport file
lands in the same phase as the approved amendment and the three data-file edits, and no task before that
phase may add a Transport file or a Hiders → Transport include. If the maintainer does not approve the
amendment, the design does not hold (the capability vocabulary and the integrator registry have no legal
home) and implement stops.

**Alternatives considered.** A top-level `src/transport/` with its own library: Transport depends on
Graphics State, Geometry and Shading, which live in `ribRender` (State and Geometry), so a separate
library would either link `ribRender` (and `ribRender` link it back for the core) or duplicate them.
Rejected; DB-023 ("domains are not build targets") stays as it is. Putting the capability vocabulary in
Infra to avoid the Hiders → Transport edge: contradicts D30 and the spec's Key Entities (Transport owns the
vocabulary). Rejected.

---

## R12. Retiring `hcshader`

**Decision.**
- Delete `src/ri/hcshader.{h,cpp}`; the `getShader` fallback (`rendererContext.cpp:345-360`); the
  `hcshader.h` includes in `renderer.cpp:38`, `rendererContext.cpp:49`, `rendererStatics.cpp:43`;
  `hcshader.cpp` in `ribRender`'s source list (`src/ri/CMakeLists.txt:93`) and in the `ribVector`
  removal list (`:394`); the comments at `:313, :326, :510`; the `domain-map.txt` line (`:69`).
- `lightInstanceOf()` (`src/libshader/shading/shaderRefs.h:41-46`): after the deletion,
  `CProgrammableShaderInstance` is the only class derived from `CShaderInstance`
  (`grep -rn "public CShaderInstance" src`), so the cast is always valid. Its comment, which cites DB-013
  and the built-in lights, is rewritten to say so; the code is unchanged. (One comment in a Shading header;
  no build or behaviour effect.)
- `doc/Documentation/Hardcoded_shaders.html` (FR-028): replaced by a short page saying the C++ lights were
  retired in spec 030 and pointing at `shaders/spherelight.sl` and `shaders/quadlight.sl`; the page's
  links from the legacy index stay valid. **Maintainer answer Q6 (2026-11-07)**: replace with a retirement
  note; do not delete the page.
- `NEWS`: the behaviour change (a named `spherelight`/`quadlight` with no compiled shader on the path
  now reports `Failed to find shader`).
- No test relies on the C++ substitute: every visual test runs with
  `SHADERS=${CMAKE_BINARY_DIR}/shaders` (`tests/visual/CMakeLists.txt:133`), where the build compiles
  `spherelight.sl`/`quadlight.sl`; the four `Visual_*light*` tests, their determinism entries
  (`determinism-scenes.cmake:28, 29, 297, 390`) and `tests/convergence/scenes/arealight-raytrace.rib.in`
  name the RSL shaders. The byte-identity run on the finished tree (FR-035) is the proof; no test is
  repointed.

**Spec reconciliation found by this plan (for the record, not a spec edit):**
- **R16**: spec.md's starting point (and its R13 row) says `rendererStatics.cpp` includes no concrete
  hider header. It includes five: `photon.h:51`, `raytracer.h:59`, `reyes.h:63`, `stochastic.h:69`,
  `zbuffer.h:74` (plus `hcshader.h:43`). FR-007 lists `rendererStatics.cpp`, so all five go; any type
  they supplied comes from a non-hider header.
- **R17**: `HIDER_PHOTONMAP_OVERWRITE` is never read (R3).
- **R18**: `CReyes::drawObject` is already declared in `reyes.h:247`; the spec's "an object-submission
  capability or interface on the hider base" is met by both (R6).
- **R19**: Hider has no `RiGetOption`-style query (`COptions::find` answers only the `hider` *parameters*:
  `jitter`, `emit`, `samplespectrum`, `samplemotion`, `options.cpp:838`); FR-017's "query where `Hider` has
  one" therefore adds no query for `Integrator`.

---

## R13. Static-link proof (FR-006, SC-008)

**Decision.** A new test, `Link_StaticConsumerRegistries` (label `link`), built like `Link_StaticConsumer`
(`tests/link/run_static_consumer.cmake`): a small program linked plainly against `ribRender_static` that
prints the registered hider names and aliases and the integrator names (through the registries'
enumeration, FR-015) and renders one frame with each built-in hider under `Integrator "classic"`. It
expects exactly `raytrace reyes(hidden, stochastic*) zbuffer photon` and `classic`.

**Rationale.** FR-006 says "`Link_StaticConsumer` extended"; extending an approved test edits it, so the
plan adds a sibling test that reuses its harness and leaves the approved one byte-for-byte as it is.
Listed for approval with the other new tests (plan.md). **Maintainer answer Q7 (2026-11-07)**: the new test;
`Link_StaticConsumer` stays untouched; spec.md's US2 AS3 and FR-006 now say so.

---

## R14. Per-frame immutability (FR-022)

**Decision.** The integrator choice and instance live in the core's `SFrameTransport` (R4), written only
by `beginFrame()` before any render thread exists and cleared at the end of `endFrame()` after the last
thread has been joined, the same window spec 028 uses for `FrameParams`. Render threads read it only
(`frameTransport()` returns a `const` reference). It is not placed in `FrameParams`: Graphics State cannot
name Transport, and spec 031 adds fields to `FrameParams` (R15).

---

## R15. Overlap with the specs planned in the same round (031, 032, 056)

Measured from the spec texts at the time of this plan (their plans were still unwritten). None of the
three depends on 030 and 030 depends on none of them.

| Shared item | 031 geometry hider-agnostic | 032 GI cache split | How 030 avoids colliding |
|---|---|---|---|
| `renderer.cpp:beginFrame()` | adds `cocFactorSamples` to `FrameParams` (computed at `:456`, installed at `:805`); "nothing in this spec edits the hider selection" | - | 030 replaces only `:777-792` and `:808-840`; `beginFrameParams()` at `:805` stays where it is. Textual neighbours only. |
| `CRenderer::render()` `drawObject` (`:1017-1024`) | moves `drawObject`/`drawGrid`/`drawPoints` onto a Geometry-owned dicing interface | - | 030 changes the call to one virtual on `CHider` guarded by a trait (R6); the second spec to merge rewrites one line. |
| `CHider` base (`hider.h`) | every hider implements the new dicing and ray-side interfaces | the base installs the irradiance sampler (FR-027) | 030 adds one member function (`virtual void drawObject(CObject *) {}`) and nothing else; the peer group goes to `CReyes`, not to the base (R5). |
| `reyes.cpp` | camera/lens statics to `FrameParams` (DB-011 lines `:189, :192, :193, :199, :207, :210, :213`) and a frame-limits value with thread count/index (FR-028) | - | 030 edits `:1992-1993` (peers) and the constructor; it leaves every `CRenderer::numThreads` read to 031 (R5). Allowlist line `:211` (030) sits between 031's `:210` and `:213`: a textual merge conflict, no semantic one. |
| `stochastic.cpp`, `zbuffer.cpp` | camera statics (`stochastic.cpp` lines `:240, :243, :250, :252`) | - | 030 changes constructors (peer group), deletes `CZbuffer::preDisplaySetup`, adds each file's table. |
| `photon.cpp` | its 6 DB-017 lines deleted (dicing interface) | the store/balance/write path (`:97-102`, FR-005, FR-028); "030 must not touch `photon.cpp`'s store path" | 030 touches only `preDisplaySetup` (`:113-119`, deleted) and appends the registration table; constructor, destructor and store path are untouched. |
| `raytracer.cpp` | ray-side interface | - | 030 appends the table; the constructor is unchanged. |
| `rendererDeclarationsDispatch.cpp` | deleted (DB-017) | - | 030 does not touch it (already null-safe for a refused frame, R8). |
| `rendererStatics.cpp` | its `ribvector-core-residual.txt` row | its residual row | 030 edits only its `#include` lines (R12); no residual row changes. |
| `allowlist.txt` DB-011 | up to 11 lines | 5 lines (`irradiance.cpp`, `photonMap.cpp`); "`photon.cpp`'s DB-011 lines stay" | 030 deletes 3 lines (`:127`, `:211`, `:263`), disjoint from both. |
| `metrics-baseline.txt` Hiders row | lowered | lowered | Expected deltas only (R16); each spec re-measures on the tree it merges into. |
| `DOMAIN_BOUNDARIES.md` | DB-004/006/009/011/017/025 | DB-010/011/017/020/024/025 | 030 edits DB-013 (Resolved), DB-011 (a history entry), and nothing in DB-020 unless its text needs the stale "030" tracking fixed (032's FR-022 rewrites that line; 030 leaves it to 032). |
| `kRendererServicesVersion` | not mentioned | steps if `getCache`/`getTexture3d`/`getPhotonMap` change | 030 does not step it (R4). |
| Constitution, `domain-rules.txt`, `archcheck` class list | not amended | "not amended"; treats `domain-rules.txt` as fixed | Only 030 amends. 030 adds a Transport row and one Hiders target; it changes no other row (032's constraint holds). |
| `COptions`, RI tokens, RIB grammar, `pluginAbi` | - | - | 030 only. |
| CLAUDE.md, DEVNOTES.md | gotchas 13/14/17/19/26/35 at the end | gotchas 24/25/28 at the end | 030 edits the Hider-system paragraph, gotcha 35's `contexts` sentence (the name no longer has an allowlist key once `reyes.cpp:211` goes; it keeps its name anyway, renaming it would be churn) and adds one new gotcha, at the end. Textual merge only. |

**056 volume shader segments**: no spec text exists yet (no `specs/056*` in any worktree). What the
siblings say: 031 "touches `csgSolid.cpp`"; 032 "no overlap found; it must not add a Hiders → Texturing
edge"; 030's spec "takes none of this scope". 030's overlap with 056 cannot be verified; nothing in 030's
file list is a volume or `Interior`/`Exterior`/`Atmosphere` file. If 056 later proposes a volume
*integrator* or a hider capability, it would extend 030's vocabulary (a MINOR step), which is the intended
direction, and would then depend on 030: the coordinator should check that when 056 is specified.

**Allowlist totals**: every spec computes its own total from 344. 030's effect is stated as a delta:
**-3 lines** (all DB-011), **+0**. The merged total is 344 minus the sum of the merged specs' deletions.

---

## R16. Expected records after the work (deltas)

- `allowlist.txt`: -3 (`photon.cpp -> CRenderer::hiderFlags`, `reyes.cpp -> CRenderer::contexts`,
  `zbuffer.cpp -> CRenderer::hiderFlags`); DB-011 164 → 161 lines. No line added or re-keyed.
- `metrics-baseline.txt` (Hiders): references -3 (`photon.cpp` and `zbuffer.cpp` `hiderFlags`, `reyes.cpp`
  `contexts`), members -2 (`hiderFlags`, `contexts`, if no other Hiders file references them; measured at
  implement). No class rises: Transport starts at 0 0 (it references no `CRenderer::` symbol).
- `DOMAIN_BOUNDARIES.md`: DB-013 → Resolved (spec 030, date, measured end state; corrected line
  references); DB-011 history entry (3 lines, the metric); DB-020 unchanged (spec R2).
- `ribvector-core-residual.txt`: unchanged (no Core file leaves or joins `ribVector_obj`; `hcshader.cpp`
  was never compiled there).

---

## R17. Byte identity and performance

- Selection, construction and dispatch produce the same hider objects with the same arguments in the same
  order (R6), the same dispatcher, and the same flags' effects (R4). `classic` executes nothing (R7).
  `hcshader` was reachable only with no compiled `spherelight`/`quadlight` on the path, which no test has
  (R12). So every image and every `MaxBlockAvgDiff` is expected unchanged; the baseline recorded on
  `e83d005d` (macOS and Linux) is the proof (FR-035).
- Cost: two registry lookups and one pair check per frame, one virtual call per submitted object instead
  of a `dynamic_cast` (cheaper), and `hasIlluminationHook()` stays a single load. SC-010 is checked by an
  interleaved timing run of one reyes and one raytrace scene (base vs finished tree, same machine, same
  thread count), as spec 028 did.
