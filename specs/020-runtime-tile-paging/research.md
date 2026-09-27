# Phase 0 Research: Runtime Bake-on-Load for Non-TIFF Texture Sources

## 1. Sharing one in-memory pyramid across N per-level `CTileSource` instances

**Decision**: The full decoded-and-box-filtered pyramid (all levels) is
built **once**, at texture-load time, into a single `CSynthesizedPyramid`
struct (a `std::vector` of per-level buffers). Each `CTiledTexture<T>`
layer (one per level, matching `CMadeTexture`'s existing `layers[]` array
— unchanged by this spec) gets its own `CSynthesizedTileSource` instance,
each holding a `std::shared_ptr<CSynthesizedPyramid>` into the *same*
shared structure plus its own level index. `CTextureLayer`'s destructor
(`delete tileSource;`, unchanged from spec 019) destroys each
`CSynthesizedTileSource` view independently; the underlying pyramid is
freed automatically once the last view's `shared_ptr` drops, whichever
layer happens to be destroyed last.

**Rationale**: Spec 019's `CTileSource` contract is strictly per-level
(one instance = one already-known level — see that spec's own
`research.md` §1), so this spec's new backend must also present one
instance per level to slot into the unchanged `CTiledTexture<T>`/
`CMadeTexture` machinery. But unlike `CTiffTileSource` (where each level's
data lives in a distinct TIFF directory the OS/filesystem already keeps
alive independently), a synthesized pyramid's levels are computed once
together and most naturally live in one contiguous structure.
`std::shared_ptr` (standard library, zero new dependency, satisfies
constitution V) is the simplest correct answer: its refcounting is
already thread-safe, and since the pyramid is fully built *before* any
concurrent `fetchTile()` call can happen and never mutated afterward
(spec.md's own Assumptions), no additional synchronization is needed for
concurrent reads across different level-instances — this is exactly what
FR-011/SC-005's concurrency test empirically proves, not merely asserts.

**Alternatives considered**: A single "owning" `CTileSource` instance
(e.g. level 0's) that all other levels reference and outlive-checks
against (rejected — invents an ordering dependency `CMadeTexture`'s own
destructor, which frees `layers[]` in array order, doesn't guarantee, and
gains nothing over the simpler refcounted approach). Baking the whole
pyramid into one giant buffer addressed by level-offset instead of N
separate per-level buffers (rejected — `appendPyramid<T>`'s own reduction
algorithm, which this spec replicates in memory per FR-004, already
naturally produces one right-sized buffer per level; forcing them into
one contiguous allocation buys nothing and complicates the box-filter
reduction step for no reason).

## 2. Internal tile size and partial trailing-tile handling

**Decision**: `CSynthesizedTileSource::fetchTile()` uses `DEFAULT_TILE_SIZE`
(`src/ri/core/ri_config.h:31`, already reachable from `texture.cpp`) as
its internal tile dimension, matching `otexmake`'s own default exactly —
per the grounded fact already established, this avoids inventing a
second, differently-sourced "default tile size" concept. For a level
whose width/height isn't an exact multiple of `DEFAULT_TILE_SIZE` (the
common case for the smallest mip levels), a requested tile's bounds may
partially exceed the level's actual dimensions; `fetchTile()` copies only
the valid, in-bounds portion and leaves the remainder of the
caller-provided `dest` buffer untouched. No explicit zero-fill or padding
logic is added.

**Rationale**: This is exactly how `libtiff`'s own tiled-image read/write
API already treats a trailing partial tile (confirmed by
`CTiffTileSource::fetchTile()`'s existing, unmodified `TIFFReadTile`
usage from spec 019 — the baked-TIFF path already relies on this same
libtiff behavior for its own smallest mip levels, which are routinely
smaller than one tile). `CTiledTexture<T>::lookupPixel()`'s existing
`xi`/`yi` wrap-mode clamping (unchanged, `texture.cpp`) never reads
beyond a level's valid `width`/`height` bounds regardless of which
backend produced the tile, so an unfilled remainder is never actually
read — matching the baked-TIFF backend's existing behavior exactly,
not inventing new semantics this spec would need to separately justify.

## 3. Non-power-of-two source handling and pyramid construction sequence

**Decision**: At texture-load time (`CRenderer::textureLoad()`'s new
fallback branch), after a successful `CImageInput::open()`+`readImage()`:
(1) if the decoded image's width/height isn't a power of two, resize it
up to the nearest power of two using the *relocated* (see §7)
`adjustSize<T>()`/`filterScaleImage<T>()`, passing the exact same
ratio-preserving `"up"` resize mode (`resizeUpMode`), `"periodic"`/
`"periodic"` wrap modes, and `RiCatmullRomFilter`/3.0/3.0 filter default
`otexmake`'s own CLI and `makeTexture()`'s own no-params-given default both
use (`otexmake.cpp:71-79`; `texmake.cpp`'s `getResizeMode` macro,
`resizeMode = resizeUpMode` when no `"resize"` RIB parameter is given) —
before building any mip level; (2) build the full mip pyramid from that
(now power-of-two) base level using the same 2x2-block-average reduction
`appendPyramid<T>()` already performs (`texmake.cpp:200-273`), producing
`tiffNumLevels(width, height)` levels total (`tiff.h:43`).

**Correction (found during T007 implementation, re-verified directly
against `otexmake.cpp`/`texmake.cpp` rather than trusting an earlier
grounding pass)**: an earlier version of this section, and of the
technical context handed to `/speckit.plan`, described the default resize
mode as `"round"` (round to the *nearest* power of two, up or down). That
is wrong: `otexmake.cpp:71` sets `resizeMode = "up"` as its own CLI
default, and `texmake.cpp`'s `getResizeMode` macro defaults to
`resizeUpMode = "up"` whenever `makeTexture()` is called with no explicit
`"resize"` RIB parameter (`rendererContext.cpp`'s `RiMakeTextureV`
passes RIB parameters through unmodified, so this same default applies via
RIB too). `"round"` is a real, selectable `adjustSize<T>` mode (rounds to
whichever power of two is numerically closer), but it is not the default
either CLI otexmake or `makeTexture()`'s callers actually use. This
spec's synthesis call MUST pass `"up"` to match the actual default
behavior a bake-then-reference workflow would produce, not `"round"`.

**Rationale**: FR-012 requires this spec to handle non-power-of-two
sources "using the same resizing behavior the project's existing bake
step already applies" — reusing the identical functions, not
re-deriving equivalent logic, is both the literal requirement and the
only way to guarantee the *same* visual result a bake-then-reference
workflow would have produced (User Story 1 Acceptance Scenario 2's
"visually equivalent" bar). Getting the specific default mode string
right matters exactly because of this bar — "up" and "round" produce
different pixel dimensions (and thus different resampled pixels) for the
same non-power-of-two source whenever the nearest power of two happens to
be the lower one.

## 4. `adjustSize<T>`/`filterScaleImage<T>` reachability from `texture.cpp`

**Decision**: Relocate both template function *definitions* from
`texmake.cpp` into `texmake.h`, as ordinary reusable template functions
(no behavior change to either). `texmake.cpp`'s own existing call sites
(`adjustSize<unsigned char/unsigned short/float>`,
`filterScaleImage<...>`) are updated to include `texmake.h` and otherwise
unchanged.

**Rationale**: Confirmed by direct inspection — both are template
function *definitions* physically inside `texmake.cpp` with no
declaration anywhere else, meaning they are not currently reachable from
any other translation unit (a template must be visible in full at every
point it's instantiated). Moving the definitions to a header both
functions already logically belong under (`texmake.h`, which already
declares `makeTexture()`, this spec's other reused function) is a pure
reachability fix — no new file, no new build target, no template
specialization/instantiation-linkage tricks needed.

**Alternatives considered**: Explicit template instantiation for the 3
needed types (`unsigned char`/`unsigned short`/`float`) left in
`texmake.cpp`, declared `extern template` in a header (rejected — more
moving parts than simply relocating the definitions, for no benefit
here: neither function is large enough that compile-time cost from
header-visibility is a real concern, unlike, say, a heavily-instantiated
generic container type).

## 4a. Thread-safety of reusing `adjustSize<T>`/`filterScaleImage<T>`/`filterImage<T>` from `textureLoad()`'s new fallback

**Finding, made during T007 implementation (not anticipated at plan time)**:
these three template functions (relocated to `texmake.h` per §4 above)
internally allocate their working/output buffers via
`ralloc(size, CRenderer::globalMemory)` (`src/ri/core/memory.h`).
`CRenderer::globalMemory` is a single, stack-based bump allocator with
`memBegin`/`memEnd` checkpoint semantics — confirmed by direct inspection of
`memory.h` in full: no lock, mutex, or atomic anywhere in `ralloc()`,
`memBegin`, or `memEnd`. Until this spec, every caller of these functions ran
at bake time (`otexmake`'s own process, or `RiMakeTextureV`'s implementation
in `rendererContext.cpp`), which is single-threaded with respect to this
arena. This spec's new fallback runs inside `CRenderer::textureLoad()`,
called from `CRenderer::getTexture()`, called from the `texture()`/
`environment()` RSL builtin implementation (confirmed at
`src/libshader/shading/rslBuiltins.cpp:181`, `svc->getTexture(name)`) — i.e.
at shading time, which this project runs multi-threaded. Two shading
threads referencing the same not-yet-loaded unbaked source concurrently
would both enter this spec's new fallback and could both call into
`adjustSize<T>` concurrently, corrupting the shared bump-pointer arena. This
is a genuinely new hazard this spec's code would introduce — distinct from
the pre-existing, separate `frameFiles`/`CTrie` concurrent-first-load gap
found in the same investigation (filed as GitHub #20, out of scope for this
spec, same boundary as #19).

**Decision**: Reuse `adjustSize<T>`/`filterScaleImage<T>`/`filterImage<T>`
verbatim (per §3/§4's own reachability work — do not re-derive an
equivalent resize/filter implementation), but serialize the entire unbaked-
source synthesis body inside `textureLoad()`'s new fallback (decode via
`CImageInput`, any `adjustSize<T>` resize call, and the box-filter pyramid
reduction) behind a new mutex. Rather than a raw `std::mutex`, this uses
the project's own existing synchronization idiom: a new `TMutex
CRenderer::synthesizeMutex`, declared in `renderer.h` alongside its
siblings (`textureMutex`, `shaderMutex`, `tesselateMutex`, etc. —
`src/ri/render/renderer.h:172-182`) and created/destroyed in
`CRenderer::initMutexes()`/`shutdownMutexes()`
(`src/ri/render/rendererMutexes.cpp`), exactly like every other
project-wide serialization mutex; `texture.cpp` uses it via the same
`osLock`/`osUnlock` calls already used throughout this file (e.g.
`textureMemFlush()`'s existing `osLock(CRenderer::textureMutex)` at
`texture.cpp:140`). `CRenderer::textureMutex` itself was considered and
rejected for reuse (see Alternatives) in favor of a dedicated mutex. The
arena interaction is additionally bracketed with `memBegin(CRenderer::
globalMemory)`/`memEnd(CRenderer::globalMemory)`, held for the same
duration as the mutex; per `memory.h`'s own comment, the bracketed scope
must not be exited early (no early `return` between begin/end), so results
needed after `memEnd` are copied out of the arena into the pyramid's own
heap-owned (`std::vector`-backed) level buffers *before* calling `memEnd`,
never held as raw arena pointers past that point. The pre-resize decode
buffer is itself arena-allocated too (inside the same bracket), so there is
exactly one ownership story for all transient buffers (arena) versus the
final per-level pyramid data (heap, owned by `CSynthesizedPyramid`).

**Rationale**: This is the narrowest fix that doesn't compromise
correctness. Texture *loading* (as opposed to per-tile *fetching*, which
this spec's `CSynthesizedTileSource::fetchTile()` still serves lock-free
from the already-built, immutable pyramid) is a one-time event per distinct
unbaked source per render, not a hot path, so serializing it has no
meaningful performance cost — matching this spec's own stated position
(spec.md Assumptions) that there is no performance target on the in-memory
synthesis path itself. The alternative of re-deriving a heap-only resize
implementation was rejected because it would silently diverge from FR-012's
actual requirement ("using the same resizing behavior the project's
existing bake step already applies") and would invalidate T029c's premise
(that `texmake.h`'s relocated functions are behaviorally unchanged and
still the single source of truth for this resize behavior).

**Alternatives considered**:
- Re-derive a standalone, heap-only, thread-safe resize/filter
  implementation instead of reusing `adjustSize`/`filterScaleImage`/
  `filterImage` (rejected — see Rationale: FR-012 divergence risk, and a
  second, untested numeric implementation of the same behavior).
- A narrower lock scoped only to the `ralloc`/arena calls themselves, not
  the whole synthesis body (rejected — leaves the door open for the same
  hazard around any other, not-yet-identified shared-arena use inside these
  functions' call graph; locking the whole synthesis body is simpler to
  reason about and costs nothing extra given synthesis is a one-time,
  non-hot-path event).
- Adding locking inside `CRenderer::getTexture()`/`frameFiles` itself to
  prevent the double-synthesis case entirely (rejected — that is the
  pre-existing, separately-filed GitHub #20 concern; this spec does not
  modify `getTexture()` or `CTrie`, matching FR-002/FR-010's existing-path-
  unmodified boundary. A duplicate synthesis under #20's race is wasted
  work, not corruption, once this spec's own mutex is in place).
- A raw `std::mutex` local to `texture.cpp` (rejected — this project has
  its own cross-platform `TMutex`/`osCreateMutex`/`osLock`/`osUnlock`
  abstraction, already used for every other project-wide serialization
  mutex including several declared and used inside this very file; a
  `std::mutex` would be a second, redundant concurrency primitive type for
  no benefit, and would not follow the `CRenderer::initMutexes()`/
  `shutdownMutexes()` lifecycle every sibling mutex already uses).
- Reusing the existing `CRenderer::textureMutex` ("to serialize texture
  fetches") instead of adding a new dedicated mutex (rejected — under this
  project's build (`TEXTURE_PERBLOCK_LOCK` always defined per
  `ri_config.h:55`), `textureMutex`'s actual current role is narrow:
  serializing `textureMemFlush()`'s block-scan/eviction-selection logic
  only (`texture.cpp:140`), not per-tile or per-load contention (that's
  handled by each `CTextureBlock`'s own per-block `TMutex`). Holding it for
  this spec's potentially-slower decode+resize+reduction work would give it
  a second, unrelated meaning and could block unrelated texture-memory
  eviction in another thread for the duration of an unbaked-source
  synthesis. A dedicated mutex keeps each primitive's responsibility
  legible, matching this codebase's existing one-mutex-per-documented-
  purpose convention in `rendererMutexes.cpp`.)

## 4b. Spurious "Not a TIFF" error / nonzero exit code on a successful synthesized-fallback render

**Finding, made during T009 implementation via a manual smoke render (not
anticipated at plan time)**: `CRenderer::textureLoad()`'s existing,
unmodified `TIFFOpen(fn, "r")` probe call, when given a genuine PNG/EXR/
RGBE source (this spec's own primary use case), makes libtiff invoke the
already-registered `tiffErrorHandler()` (`TIFFSetErrorHandler(
tiffErrorHandler)`, set unconditionally on every `textureLoad()` call) ->
`error(CODE_SYSTEM, "Not a TIFF or MDI file, bad magic number ...")`. This
sets the global `RiLastError` (`src/ri/parse/ri.cpp:371`), which
`orender`'s own `main()` reads once at exit: `return (RiLastError !=
RIE_NOERROR) ? -1 : 0;` (`orender.cpp:919`) -- so a render that succeeds
completely via this spec's new fallback still exits nonzero.
`test_hider_parity.cpp:339-343` (`runOrender()`) treats any nonzero
`orender` exit code as an outright failure *before* it ever compares
pixels -- and this is the exact harness T011's own 4 new parity tests run
under (`add_parity_test`, `tests/visual/CMakeLists.txt:227-243`). Left
unfixed, every one of T011's new scenes would fail permanently regardless
of pixel correctness.

**Why this is in-scope, not merely a pre-existing/adjacent concern (per
the boundary GitHub #19/#20 established)**: the *root cause* chain
(process-global libtiff error handler + the unsynchronized `RiLastError`
global) is pre-existing and untouched by this spec. But before this spec,
hitting this code path (`TIFFOpen()` failing) was *always* immediately
followed by a second, intentional failure signal (`CODE_NOFILE` +
`CDummyTexture` substitution) -- so the spurious message was harmless
noise alongside a real, correctly-reported failure. This spec is what
turns that scenario into a fully successful one for the first time,
which is precisely what exposes the spurious message as a standalone,
misleading false negative with a concrete, measured consequence for this
spec's own required test coverage (T011). Fixing the test-blocking
symptom is therefore in scope; fixing the general root cause is not.

**Decision**: Add a small, file-local `looksLikeTiff(const char *fn)`
check in `texture.cpp` that reads fn's first 4 bytes and compares them
against TIFF's own magic number, both byte orders (`49 49 2A 00` /
`4D 4D 00 2A`) -- `TIFFOpen()` is only attempted when the magic matches,
or when the check itself could not be performed (file unreadable --
should-never-happen, since `locateFile()` already found it; defaults to
still attempting `TIFFOpen()` rather than silently diverting a file this
check couldn't read). A valid-magic-but-corrupt TIFF still reaches
`TIFFOpen()` and still reports a real, correct error -- only the
"not a TIFF at all" case (this spec's own normal, successful path) is
silenced.

**Rationale**: This is the narrowest fix that does not touch any shared,
concurrently-reached mutable state -- unlike the two alternatives below,
both of which were seriously considered and rejected specifically because
of the concurrency finding in §4a (the same `textureLoad()` call path is
reachable by multiple shading threads simultaneously). A file's own
leading bytes are read once, locally, with no interaction with libtiff's
global handler registration or the global `RiLastError` at all, so this
introduces no new race. For a genuine TIFF, this check is a cheap,
side-effect-free pass-through -- `TIFFOpen()` still runs identically,
preserving FR-002's "existing baked path unaltered" guarantee in the
sense that matters (observable behavior for an actual TIFF is unchanged).

**Alternatives considered**:
- Temporarily save the current libtiff error handler, set it to a no-op/
  NULL, perform the `TIFFOpen()` probe, then restore the saved handler
  (rejected -- `TIFFSetErrorHandler`/`TIFFSetWarningHandler` are libtiff
  *process-global* function pointers, not per-handle or thread-local.
  `textureLoad()` runs on multiple shading threads concurrently
  (research.md §4a); a different thread's own, genuinely-failing
  `TIFFOpen()` call -- or any other libtiff error -- occurring during
  this thread's suppression window would have its real error silently
  dropped too. Unsafe under this codebase's actual concurrency model,
  not merely theoretically imperfect).
- Reset the global `RiLastError` back to `RIE_NOERROR` immediately after
  a successful synthesized-fallback load, undoing the spurious probe's
  side effect directly (rejected -- confirmed by direct inspection
  (`src/ri/parse/ri.cpp:371`) that `RiLastError` is a bare global,
  written unsynchronized from `error()`/`warning()`/`fatal()` from any
  thread. A concurrently-running thread could have set it for an
  unrelated, genuine problem at nearly the same moment; resetting it
  here risks silently clobbering that thread's real signal).
- Leave it unfixed and accept the nonzero exit code as a pre-existing,
  out-of-scope concern, mirroring GitHub #19/#20's boundary (rejected --
  unlike those two, this one has a concrete, measured, in-scope
  consequence: it would make T011's own required regression coverage
  permanently red regardless of correctness, which this spec cannot
  accept without abandoning its own testing requirements).

GitHub #21 filed for the underlying, broader architectural facts (the
process-global libtiff handler and the unsynchronized `RiLastError`
exit-status global) both rejected alternatives ran into -- fixing those
generally remains out of scope for this spec.

## 4c. Standalone-binary dependency on renderer-lifecycle globals (T012)

**Finding, made while writing T012's direct unit test**: unlike
`CTiffTileSource` (whose `info()`/`fetchTile()` only ever call plain
`TIFFOpen`/`TIFFReadTile`, no renderer-global state at all), exercising
`CSynthesizedTileSource`/`createSynthesizedTileSource()` from a bare
standalone binary (no `RiBegin()`/render in progress) crashes twice, for
two separate reasons: `adjustSize<T>`/`filterScaleImage<T>`/
`filterImage<T>` (§4a) allocate from `CRenderer::globalMemory`, a bare
global initialized to `NULL` (`rendererStatics.cpp:106`) until
`CRenderer::beginRenderer()` sets it up; and `CImageInput::open()`'s own
failure-reporting paths call `error()`, which dereferences the global
`renderMan` singleton, likewise unset outside a renderer lifecycle.
Confirmed via `lldb` backtraces for both (`buildSynthesizedPyramid()` on
the first; `CPngImageInput::open()` -> `error()` on the second, after
fixing the first in isolation first surfaced it).

**Decision**: T012's test brackets its body in `RiBegin(RI_NULL)`/
`RiEnd()` -- confirmed to be the project's own existing, minimal-context
convention for exactly this problem, already used (and documented in
their own header comments) by `test_image_input_png.cpp`/
`test_image_input_tiff.cpp` for the identical reason (their own decode-
failure paths call `error()` too). Not a new pattern; this spec's test
simply needed the same treatment `CTiffTileSource`'s own test never
required, since that backend has no renderer-global dependency to begin
with.

**Rationale**: `RiBegin(RI_NULL)`/`RiEnd()` is a standard RenderMan API
call pair already exercised by this exact test category, far lighter than
hand-initializing individual renderer globals (which was tried first --
`memoryInit(CRenderer::globalMemory)` + `CRenderer::initMutexes()` --
and only fixed the first crash, since it does not touch `renderMan`) and
without inventing a second, narrower initialization convention alongside
an existing, working one for the same problem.

## 5. Disk cache filename/key scheme

**Decision**: A cache entry's filename is
`<hash-of-absolute-source-path>-<source-mtime-epoch-seconds>.tex`, using
`std::hash<std::string>` (standard library, zero new dependency) over the
resolved, absolute source file path. "Does a valid, fresh cache entry
exist" reduces to "does a file at this exact, freshness-encoding path
exist" — no separate mtime-comparison step is needed, because a changed
source produces a different filename entirely; an old entry for a
previous mtime is simply never looked up again (orphaned, not actively
cleaned up — no cache-eviction/cleanup requirement exists in spec.md, so
none is invented here).

**Rationale**: This directly and elegantly satisfies FR-006's "detect a
stale cache and rebuild" requirement as a structural property of the
naming scheme rather than an explicit, separately-fallible check (no
risk of comparing mtimes across a filesystem with clock-skew quirks and
getting the comparison direction or tolerance wrong — the exact class of
subtle bug a from-scratch mtime-comparison check could introduce).
`std::hash` collisions are an accepted, standard-library-idiomatic risk
(the same risk any hash-keyed cache already accepts) rather than a
concern unique to this design.

**Alternatives considered**: Mirroring the source's own directory
structure under the cache root (rejected — reconstructing a safe,
collision-free filesystem path from an arbitrary absolute source path,
across platforms, is more complexity than a flat hash for no added
value here). Comparing the existing cache file's own mtime against the
source's mtime on every load (rejected in favor of the above — same end
result, more moving parts, and reintroduces the clock-skew-sensitive
comparison the filename-encoding approach avoids entirely).

## 6. Atomic write-then-rename mechanism

**Decision**: The disk-cache writer (calling `makeTexture()`, see §8)
targets a temporary, uniquely-named path in the *same directory* as the
final cache filename (e.g. suffixed with the writing process's PID),
then calls POSIX `rename()` to move it into the final, freshness-encoding
path (§5) only once `makeTexture()` has returned successfully. A reader
only ever attempts to open the final path — it can never observe an
in-progress write.

**Rationale**: Directly implements spec.md's clarified answer
("atomic write-then-rename, no locking") for FR-016/SC-006. `rename()` is
atomic within the same filesystem/directory (a well-established POSIX
guarantee on both target platforms, constitution VI) — same-directory
placement of the temporary file guarantees this (a cross-filesystem
rename is not atomic and is explicitly avoided by this placement choice).
Whichever concurrent writer's `rename()` happens last simply wins,
overwriting harmlessly — every writer, having read the same unchanged
source, produces an equally valid cache entry for that exact
source-path+mtime key.

## 7. RIB `Option` for the opt-in disk cache

**Decision**: A new `Option` class, `"texturecache"`, with two tokens:
`"enable"` (int\[1], boolean — 0 disables, non-zero enables; default
disabled, matching the opt-in framing) and `"directory"` (string\[1],
optional — overrides the default system temp/cache directory). Both are
dispatched via a new `else if (strcmp(name, RI_TEXTURECACHE) == 0)`
branch inside `CRendererContext::RiOptionV()`
(`src/ri/render/rendererContext.cpp:1455`), the same pattern already used
for `RI_LIMITS`/`RI_SEARCHPATH`/`RI_HIDER`. The resulting settings are
stored on `COptions` (`src/ri/state/options.h`), alongside existing
fields like `texturePath`.

**Rationale**: Directly implements the resolved clarification ("a
scene-level RIB `Option`... consistent with this project's existing
convention"). Grouping "enable" and "directory" under one class (rather
than two separately-named `Option` statements) mirrors how `"limits"`
already groups several related settings (`bucketsize`, `gridsize`,
`eyesplits`, etc.) under one class.

**Correction (found during T016 implementation via an actual render, not
code review alone)**: this section originally claimed existing `Option`
classes are "not subject to the 4-layer `Attribute`-style pre-declaration
system" and that a new token constant plus dispatch branch alone would be
sufficient. That claim was wrong, and a manual smoke render caught it
immediately: `Option "texturecache" "enable" [1] "directory" [...]`
failed at RIB-parse time with `Parameter "enable" is not declared`,
*before* `RiOptionV()`'s new dispatch branch was ever reached. Checking
`initDeclarations()` (`src/ri/render/rendererDeclarations.cpp:88-122`)
directly shows every existing `Option` sub-token IS pre-declared there
too — `declareVariable(RI_BUCKETSIZE, "int[2]")`,
`declareVariable(RI_JITTER, "float")`, `declareVariable(RI_FILELOG,
"string")`, and so on for every single one, with no exception. The RIB
parser's own pre-declaration gate applies uniformly to `Option` and
`Attribute` parameters alike; `CLAUDE.md`'s "Adding attributes" note
happens to describe the 4-layer system in terms of `RiAttributeV`
specifically, but the pre-declaration layer itself is not
`Attribute`-exclusive. **Fix applied**: added
`declareVariable(RI_TEXTURECACHEENABLE, "int")`/
`declareVariable(RI_TEXTURECACHEDIRECTORY, "string")` to
`initDeclarations()`, alongside the other `Option` sub-token
declarations — a fourth step this section's own decision omitted
entirely. Re-verified via the same manual render: the `Option` statement
now parses and dispatches correctly.

**Alternatives considered**: An environment variable (rejected by the
spec's own clarification — an `Option` was explicitly chosen for
per-scene portability). Two separate new `Option` classes instead of one
class with two tokens (rejected — no existing precedent in this
codebase splits closely-related settings this way; grouping matches
`"limits"`'s own convention).

## 8. Disk-cache writer: reusing `makeTexture()`

**Decision**: The cache-write path calls `makeTexture(sourcePath,
tempCachePath, texturePath /* existing TSearchpath* */, "periodic",
"periodic", RiCatmullRomFilter, 3.0f, 3.0f, 0, nullptr, nullptr)` —
`"periodic"` wrap modes matching spec.md's resolved wrap-mode default
(FR-013), `RiCatmullRomFilter` at width/height 3.0 (matching `otexmake`'s
own actual CLI default filter size, `otexmake.cpp:74-75`, not 1.0 — a 1.0
filter width/height would be a materially narrower, near-point-sampling
filter and would not match what a real `otexmake` bake of the same source
produces), with `numParams=0` so `makeTexture()`'s own `getResizeMode()`
macro applies its documented default resize mode, `"up"` (`resizeUpMode`
— **corrected**: an earlier version of this section said "round"; see §3's
own correction for the verification against `otexmake.cpp`/`texmake.cpp`
that found this).

**Rationale**: Confirmed by direct inspection this function is already
renderer-linked (used by `RiMakeTextureV`'s existing implementation,
`rendererContext.cpp:5876,5886`) and already dispatches to
`createImageInput()` internally at all its own call sites — meaning it
already knows how to bake a PNG/EXR/RGBE source into a standard baked
TIFF with the exact same Pixar tags an `otexmake` bake produces. Calling
it directly avoids inventing and maintaining a second TIFF-pyramid-writing
code path with its own risk of subtly disagreeing with the first. This
spec adds a NEW caller with its own synthesized default arguments — the
existing `RiMakeTextureV` call sites are untouched (FR-010's boundary).

## 9. Disk-cache-vs-live-synthesis byte-identical test (SC-003)

**Decision**: Render the same scene (referencing the same unbaked source)
twice with `-t:1`: once with the disk cache freshly populated (so the
render reads back through `CTiffTileSource`, the ordinary baked-texture
path), once with the disk cache deliberately absent/disabled (so the
render goes through `CSynthesizedTileSource` directly). `cmp` the two
rendered outputs byte-for-byte.

**Rationale**: Directly implements the resolved clarification (byte-for-
byte, single-threaded) and reuses spec 019's own established
`test_render_byte_identical.sh` pattern/rationale (this codebase's
default multi-threaded reyes rendering is not byte-reproducible run to
run even with correct code, so `-t:1` is required for any exact-match
test) rather than inventing a new comparison methodology.

## 10. Multi-process concurrent cache-write test (SC-006/FR-016 — genuinely new test shape)

**Decision**: A new shell-driven test (mirroring `test_shader_compile.sh`/
`test_render_byte_identical.sh`'s existing shell-script-test convention,
not a C++ unit test): delete any existing cache entry for a fixture
source, then launch N (e.g. 8) `orender` processes concurrently
(background jobs in the same shell script, all pointed at the same scene
referencing the same unbaked source, disk cache enabled), wait for all to
complete, then assert (a) every process's own rendered output is correct
(matches a known-good reference, `-t:1` where each individual process is
itself pinned single-threaded — the concurrency under test is
*process*-level cache-writing, not intra-process shading-thread
concurrency, so pinning each process's own render threads removes an
unrelated source of variance from this specific test), and (b) the
resulting single cache file on disk opens successfully as a valid baked
texture (a corrupt/partial write would fail this check directly).

**Rationale**: Confirmed no equivalent test exists in this project today
— spec 019's own concurrency test exercises multiple *threads* within one
process; this property (safety across multiple independent *processes*,
motivated by spec.md's own "render farm" framing) has never needed
testing before. A shell-driven multi-process launcher is the simplest
way to get genuine OS-level process concurrency without inventing new
C++ process-spawning test infrastructure this project doesn't otherwise
need.

## 11. Synthesized-backend multi-threaded concurrency test (SC-005)

**Decision**: Follows spec 019's User Story 2 precedent exactly (that
spec's own `research.md` §6, and its actual implementation via
`concurrency-scene.rib` + `test_tile_source_concurrency.sh`): a real,
multi-threaded `orender` render of a scene referencing an *unbaked*
source (large, finely-diced polygon, periodic wrap so many buckets/
threads fault into overlapping tile regions), compared against a `-t:1`
reference via the project's existing block-average visual-diff tool,
with a threshold measured empirically against this spec's own new
fixture (not reused verbatim from spec 019's own measured value, since
the scene/texture content differs) before being fixed in `tasks.md`.

**Rationale**: `CShadingContext` cannot be hand-constructed in a
standalone unit test (confirmed during spec 019's own planning — it's
abstract, no standalone-construction precedent anywhere in this
codebase); this remains true for the new synthesized backend, so the
same real-render-based approach applies, targeting the new backend
specifically rather than re-running spec 019's own baked-TIFF-focused
test (which doesn't exercise `CSynthesizedTileSource` at all).
