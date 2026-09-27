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
`adjustSize<T>()`/`filterScaleImage<T>()` — the exact same ratio-preserving
"round" resize mode and `RiCatmullRomFilter` default `otexmake`'s own CLI
uses (`otexmake.cpp:79`) — before building any mip level; (2) build the
full mip pyramid from that (now power-of-two) base level using the same
2x2-block-average reduction `appendPyramid<T>()` already performs
(`texmake.cpp:200-273`), producing `tiffNumLevels(width, height)` levels
total (`tiff.h:43`).

**Rationale**: FR-012 requires this spec to handle non-power-of-two
sources "using the same resizing behavior the project's existing bake
step already applies" — reusing the identical functions, not
re-deriving equivalent logic, is both the literal requirement and the
only way to guarantee the *same* visual result a bake-then-reference
workflow would have produced (User Story 1 Acceptance Scenario 2's
"visually equivalent" bar).

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
convention"). Confirmed by direct inspection of `RiOptionV()`: existing
option classes are recognized by a plain `strcmp` chain against `RtToken`
constants, not subject to the 4-layer `Attribute`-style pre-declaration
system (`CLAUDE.md`'s "Adding attributes" note is specific to
`RiAttributeV`) — so adding a new class needs only a new token constant
(`ri.h`/`ri.cpp`) plus a new dispatch branch, exactly like every existing
class already works. Grouping "enable" and "directory" under one class
(rather than two separately-named `Option` statements) mirrors how
`"limits"` already groups several related settings (`bucketsize`,
`gridsize`, `eyesplits`, etc.) under one class.

**Alternatives considered**: An environment variable (rejected by the
spec's own clarification — an `Option` was explicitly chosen for
per-scene portability). Two separate new `Option` classes instead of one
class with two tokens (rejected — no existing precedent in this
codebase splits closely-related settings this way; grouping matches
`"limits"`'s own convention).

## 8. Disk-cache writer: reusing `makeTexture()`

**Decision**: The cache-write path calls `makeTexture(sourcePath,
tempCachePath, texturePath /* existing TSearchpath* */, "periodic",
"periodic", RiCatmullRomFilter, 1.0f, 1.0f, 0, nullptr, nullptr)` —
`"periodic"` wrap modes matching spec.md's resolved wrap-mode default
(FR-013), `RiCatmullRomFilter` at width/height 1.0 (matching
`otexmake`'s own CLI default filter and its default "round" resize
behavior, since `numParams=0` means no explicit resize-mode override is
given, and `makeTexture()`'s own `getResizeMode()` call already applies
its documented default in that case).

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
