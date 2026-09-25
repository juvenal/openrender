# Phase 0 Research: Runtime Texture Tile-Fetch Abstraction

## 1. `CTileSource` instance granularity: one per texture, or one per mip level?

**Decision**: **One `CTileSource` instance per mip level**, matching the
existing `CTextureLayer` ownership model exactly (each `CTiledTexture`/
`CBasicTexture` layer is already fully self-contained, independently
owning its own `strdup`'d filename and TIFF directory index — see
`plan.md`'s grounded architecture notes). Each `CTileSource` instance
represents exactly one already-known level; the interface sketched during
specify/plan (`fetchTile(level, tileX, tileY, dest)`, `numLevels()`,
`levelInfo(level)`) is revised accordingly — see §2.

**Rationale**: The alternative (one shared `CTileSource` per *texture*,
indexed by a `level` parameter) was seriously considered, since it more
directly generalizes `readMadeTexture()`'s per-level metadata-query loop
too. But that loop is explicitly **out of this spec's scope** — spec.md
frames the seam as `textureLoadBlock()` (the per-tile-fault *pixel* fetch,
called repeatedly during shading), not `readMadeTexture()`/`readTexture()`
(the one-time construction-time *metadata* query, called once per texture
reference, analogous in spirit to spec 018's `CImageInput::open()`).
Making one `CTileSource` serve a whole texture would require either (a)
rewriting `readMadeTexture()`'s existing, working direct-TIFF-call
geometry loop to route through the new interface too (real scope
expansion, not requested, and risky for a "zero intended behavior change"
refactor), or (b) constructing a texture-wide `CTileSource` solely for
pixel-fetch purposes while `readMadeTexture()` keeps querying geometry
directly — duplicating the same TIFF-directory-geometry query in two
places with no shared source of truth, a code smell and a real risk of
the two disagreeing. The per-level model avoids both: it's a strict 1:1
replacement of each layer's `(filename, directory)` pair with a
`CTileSource*`, no new shared-ownership machinery, no duplicated queries.

**Alternatives considered**: One-per-texture (rejected, above). A
hybrid where `CTileSource` is constructed once per texture but only used
for pixel fetch, geometry left duplicated (rejected — the duplication
risk outweighs the marginal simplicity gain).

## 2. Revised `CTileSource` interface (corrects the specify/plan sketch)

**Decision**:

```cpp
// src/ri/texture/tileSource.h

struct CTileLevelInfo {
    int width, height, tileWidth, tileHeight, numChannels, bitsPerSample;
    bool isFloatFormat;
};

class CTileSource {
public:
    virtual ~CTileSource() {}

    // This instance's single level's geometry.
    virtual void info(CTileLevelInfo &info) = 0;

    // Full, tile-aligned tile (CTiledTexture) or the whole image as the
    // implicit tile (0,0) (CBasicTexture, the flat/un-made path with no
    // mip pyramid) -- both existing callers always request the complete
    // region, confirmed by exhaustive grep, never a sub-region. Must be
    // safe to call concurrently from multiple threads for DIFFERENT
    // CTileSource instances/tiles (matches the existing per-CTextureBlock
    // lock model above this seam, unchanged by this spec) -- not
    // necessarily safe for repeated concurrent calls on the SAME
    // instance for the SAME tile, since the per-block lock already
    // serializes that case before this interface is ever reached.
    virtual bool fetchTile(int tileX, int tileY, void *dest) = 0;
};
```

**Rationale**: Dropped `numLevels()` (a whole-texture concept `CMadeTexture`
already tracks via its `layers` array length, computed once via the
existing `tiffNumLevels()` call in `readMadeTexture()` — unrelated to any
single `CTileSource` instance) and the `level` parameter on
`fetchTile()`/`levelInfo()` (meaningless once each instance represents
exactly one level) per the granularity decision in §1. `info()` replaces
`levelInfo(level)` — same purpose, no level parameter needed. This
interface is not exercised for geometry queries by this spec's own
`CTiffTileSource` usage (see §1) but is kept complete and self-consistent
so a future `CTileSource` backend (spec 020) can implement it meaningfully
without this spec needing to guess ahead of time exactly how.

## 3. `CTiffTileSource` placement: new file, or file-local in `texture.cpp`?

**Decision**: `CTiffTileSource` (the concrete implementation) stays
**file-local within `texture.cpp`**, declared and defined alongside
`CTextureBlock`/`CTextureLayer`/`CTiledTexture`/`CBasicTexture`/
`CMadeTexture` — all of which are already file-local there today (none
appear in `texture.h`, confirmed directly). Only the abstract
`CTileSource`/`CTileLevelInfo` (§2) go into the new shared
`src/ri/texture/tileSource.h`.

**Rationale**: `CTiffTileSource` is tightly coupled to `textureLoadBlock`'s
existing logic and to the layer classes that construct it — nothing
outside `texture.cpp` needs to reference it directly in this spec, so
moving it to its own file would be pure churn with no benefit yet. The
shared header exists specifically because spec 020 (a separate, later
effort) is expected to add further backends, most naturally each in their
own `.cpp` file mirroring spec 018's `imageInputPng.cpp`/
`imageInputExr.cpp`/`imageInputRgbe.cpp` pattern — those future files need
`tileSource.h` to include; they don't need anything from
`CTiffTileSource`'s implementation.

**Alternatives considered**: A new `tileSourceTiff.h`/`.cpp` pair,
mirroring `imageInputTiff.h`/`.cpp` from spec 018 (rejected for this spec
specifically — spec 018's `CImageInput` implementations needed to be
separate files because `texmake.cpp` calls into all of them uniformly
through the factory; `CTiffTileSource` has no equivalent multi-caller
factory in this spec, so the extra file split isn't earning its keep yet).

## 4. `readMadeTexture()`/`readTexture()` construction-site changes

**Decision**: Each per-level (or, for `CBasicTexture`, the single) layer
constructor call in `readMadeTexture()`/`readTexture()` additionally
constructs a `CTiffTileSource` (from the same filename and TIFF directory
index it already computes) and passes it to the `CTiledTexture`/
`CBasicTexture` constructor **in place of** the raw filename+directory
pair those constructors currently take. `CTextureLayer`'s
`strdup`(filename)/directory-index members are removed in favor of owning
the `CTileSource*` (construct/destroy paired with the layer's own
lifetime, mirroring the exact ownership discipline `CTextureLayer`
already uses for its `strdup`'d filename — one owner, freed in the
destructor). `readMadeTexture()`'s/`readTexture()`'s own direct TIFF
metadata queries (for `fileWidth`/`fileHeight`/`tileWidth`/`tileHeight`/
`numSamples`/`bitsPerSample`, used to size the constructor call itself)
are **unchanged** — see §1.

**Rationale**: Follows directly from §1; stated explicitly here because
it's the actual code-site change, not just the class-design decision.

## 5. Byte-identical rendered-output regression test

**Decision**: Pick 2-3 existing visual-regression scenes already in
`examples/rib/tests/` that reference an already-baked texture: one plain
`Texture`-parameter scene, and at least one environment-or-shadow-map
scene (per spec.md User Story 1's acceptance scenarios 2-3). Render each
with the pre-refactor `orender` binary (captured once, before any
`CTileSource` code exists — mirroring spec 018's Foundational-phase
baseline-binary approach) and with the post-refactor binary; `cmp` the
rendered output bytes directly. Exact scene selection is a task-level
detail (`tasks.md`), not fixed here, but should reuse existing scenes
rather than author new ones — this is a regression check, not new
coverage, so no new baked-texture fixtures are needed for it.

**Rationale**: Mirrors spec 018's `T006`-style byte-identical regression
methodology (compare against a genuine pre-refactor baseline binary), but
one level up the pipeline: comparing *rendered images*, not baked
`.tex` files, since this spec changes the *read* side, not the *write*
side. A `cmp`-exact bar (not the project's usual 8×8 block-average visual
threshold) is necessary because that threshold (20-40/255) is coarse
enough to hide a subtle tile-indexing or mip-level-selection bug — exactly
the class of bug this refactor could introduce.

## 6. Multi-threaded concurrency test (new coverage, spec.md User Story 2)

**Decision**: A new unit test that starts several threads and drives them
through texture lookups against a texture with multiple tiles/levels,
covering both cases spec.md's acceptance scenarios call for: (a) different
threads requesting different, previously-uncached tiles concurrently, and
(b) different threads requesting the *same* previously-uncached tile
concurrently (exercising the per-`CTextureBlock` lock's fault-in-once
guarantee). Correctness is checked by comparing fetched pixel data against
known-expected values from a small synthetic fixture (reusing spec 018's
fixture-generation approach — a closed-form per-pixel formula, so exact
values are cheap to assert without a stored reference image). Run
repeatedly within the test (a loop of several iterations, exact count a
task-level detail) to surface intermittent races rather than relying on a
single lucky/unlucky interleaving. Written and run successfully against
the **pre-refactor** code first, to establish that it's a meaningful,
passing baseline before `CTileSource` exists — not a test that only
happens to pass once the refactor changes something.

**Rationale**: Directly implements spec.md's FR-006/SC-002/User Story 2.
Using a synthetic fixture with known values (rather than a real production
texture) keeps the correctness check exact and fast, consistent with
spec 018's fixture-design philosophy. Running it against the pre-refactor
code first is what makes this a genuine regression-safety net for the
refactor, not just new coverage added incidentally alongside it.

**Alternatives considered**: A thread-sanitizer-only approach (build
under `-fsanitize=thread`, rely on TSAN to catch races without an
explicit correctness assertion) — not rejected, but treated as a
worthwhile *addition* for whoever implements this (if convenient in this
project's build setup) rather than a *replacement* for an explicit,
deterministic correctness test, since TSAN availability/CI integration
here is unverified and out of scope to newly wire up as part of this
already-scoped refactor.
