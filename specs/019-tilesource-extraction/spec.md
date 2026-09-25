# Feature Specification: Runtime Texture Tile-Fetch Abstraction (CTileSource)

**Feature Branch**: `019-tilesource-extraction`

**Created**: 2026-09-25

**Status**: Draft

**Input**: User description: "Spec 019 of a 3-spec layered effort to add multi-format texture support to openRender: extract a CTileSource tile-fetch abstraction from the runtime texture-read path (textureLoadBlock() in src/ri/texture/texture.cpp), migrating the existing TIFF-backed baked-texture read path onto it with zero intended behavior change, as groundwork for a later effort (spec 020) to let the renderer natively read non-TIFF-sourced textures at render time without rewriting this hot shading-time path per format."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Existing renders are completely unaffected (Priority: P1)

An artist or technical director renders a scene that references an already
`otexmake`-baked texture (a plain texture, or an environment/shadow map,
all of which read baked textures the same way at render time). The
rendered image must come out exactly as it did before this internal
change — this is a pure refactor of how texture pixel data is fetched
internally; nothing about what gets rendered should differ.

**Why this priority**: This is the entire promise of a "zero intended
behavior change" refactor. If this doesn't hold, the refactor has failed
regardless of any other benefit it might deliver.

**Independent Test**: Render an existing scene that uses a baked TIFF
texture (and separately, one using an environment or shadow map) before
and after the refactor; compare the rendered output bytes directly.

**Acceptance Scenarios**:

1. **Given** a scene with a `Surface` shader parameter referencing an
   already-baked TIFF texture, **When** it is rendered before and after
   this refactor, **Then** the rendered output is byte-for-byte identical.
2. **Given** a scene using a baked cubic, spherical, or cylindrical
   environment map, **When** it is rendered before and after this
   refactor, **Then** the rendered output is byte-for-byte identical.
3. **Given** a scene using a baked shadow map, **When** it is rendered
   before and after this refactor, **Then** the rendered output is
   byte-for-byte identical, including any embedded projection metadata
   the shadow map carries.

---

### User Story 2 - Concurrent texture access is verified safe (Priority: P2)

Today, nothing in the automated test suite actually exercises what
happens when multiple rendering threads fault in different texture tiles
at the same time — the renderer's own threading model assumes this is
safe, but that assumption has never been checked by a test. This refactor
is a natural point to close that gap: after extracting the tile-fetch
seam, a genuine multi-threaded test should prove the extraction didn't
introduce (or fail to catch a pre-existing) concurrency problem.

**Why this priority**: Lower than "nothing breaks" (User Story 1), but
independently valuable and specifically enabled by touching this code —
deferring it further would mean rewriting the same hot path again later
just to add the test.

**Independent Test**: Run a test that starts multiple threads, each
concurrently faulting in different tiles/textures, and confirms all
fetches complete correctly with no corruption or crash — both before this
refactor begins (to establish a baseline) and after it completes.

**Acceptance Scenarios**:

1. **Given** multiple rendering threads simultaneously requesting
   different, previously-uncached tiles, **When** they fault in
   concurrently, **Then** every thread receives correct pixel data with
   no data races or crashes.
2. **Given** multiple rendering threads simultaneously requesting the
   *same* previously-uncached tile, **When** they fault in concurrently,
   **Then** the tile is loaded exactly once and every thread receives the
   correct, complete data (the existing per-tile-cache-slot locking
   behavior, unchanged by this refactor).

---

### User Story 3 - The fetch mechanism is isolated behind one swappable seam (Priority: P3)

A future maintainer (the very next piece of work in this multi-part
effort) needs to add support for reading non-TIFF-baked textures at
render time. Today that would mean touching the hot shading-time fetch
function directly, once per new format. After this refactor, that
maintainer should be able to add a new implementation of a single,
well-defined interface without modifying anything else in the render-time
lookup path.

**Why this priority**: This is the actual motivation for doing the work,
but it's realized by the other two stories' success criteria (identical
output, verified concurrency) plus a structural property (no
format-specific code left outside the new interface) rather than being
independently testable as runtime behavior in its own right.

**Independent Test**: Inspect the code that performs a tile lookup for a
texture, an environment map, and a shadow map — confirm none of it
references TIFF-specific APIs or types directly; all format-specific
logic lives behind the one new interface.

**Acceptance Scenarios**:

1. **Given** the completed refactor, **When** a developer looks at the
   code path a texture, environment map, or shadow map lookup goes
   through to fetch pixel data, **Then** they find exactly one
   format-specific implementation, reached through one abstract interface,
   not format-specific calls scattered through the lookup path itself.

---

### Edge Cases

- What happens if the underlying baked-texture file is deleted or its
  volume unmounted between when a texture is first referenced and a later
  tile fault-in? This is a pre-existing, already-tolerated situation (the
  current code has a known gap here) — this refactor MUST NOT make it any
  worse, but is not expected to fix it either.
- What happens to the per-file metadata some texture types embed (e.g. a
  shadow map's world-to-camera/world-to-screen projection matrices)? This
  metadata is read once, when a texture is first loaded, through a
  separate mechanism from the per-tile pixel fetch this spec changes —
  it MUST continue working exactly as it does today, untouched.
- What happens to the (currently unreachable in practice) request-a-
  sub-region-instead-of-a-whole-tile and separate-color-plane-layout code
  paths? They MUST be preserved, not deleted, even though no current
  caller reaches them — this is a behavior-preserving extraction, not a
  cleanup pass, and removing "dead" code is a separate, deliberate
  decision outside this work's scope.
- What happens for a texture referenced by more than one shading thread at
  once, when it's already fully cached (no fault-in needed)? Must remain
  as fast and correct as today — reading already-cached data must not
  gain new locking or overhead this refactor doesn't already require.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST fetch tile pixel data for baked TIFF
  textures (both the tiled mip-pyramid form and the flat/un-made form)
  through a single, well-defined interface, such that no code outside
  that interface's implementation performs format-specific (TIFF) API
  calls to obtain pixel data.
- **FR-002**: Rendering any scene that uses an already-baked TIFF texture,
  environment map, or shadow map MUST produce byte-for-byte identical
  output after this refactor compared to before it.
- **FR-003**: The tile-cache/eviction behavior surrounding the fetch
  (caching, memory limits, per-slot reference counting) MUST NOT change —
  this refactor changes only what happens inside a cache-miss fault-in,
  not the caching policy around it.
- **FR-004**: Environment and shadow map lookups MUST continue to work
  exactly as before, including any per-file metadata (such as a shadow
  map's embedded projection matrices) read separately from the per-tile
  pixel fetch.
- **FR-005**: Code paths not reachable by any current caller today (a
  request for a sub-region smaller than a full tile; a source file using
  separate, non-interleaved color planes) MUST be preserved in the new
  implementation, not removed.
- **FR-006**: Concurrent fetches for different tiles or textures, issued
  simultaneously from multiple rendering threads, MUST remain correctly
  synchronized (no data corruption, no crash) — verified by an
  automated test, not asserted by inspection alone.
- **FR-007**: This work MUST NOT modify the bake-time write path
  (`otexmake` and the baked-texture container format it produces) or the
  deep-shadow-map file mechanism, which is unrelated and already separate
  from the code path this spec changes.
- **FR-008**: This work MUST NOT introduce support for any new texture
  source format at render time — that is explicitly a separate, later
  effort building on top of this one.

### Key Entities

- **Tile Source**: The new abstraction this spec introduces — represents
  "a place tile pixel data for one texture can be fetched from," aware of
  how many detail (mip) levels it offers and the pixel-format/dimensions
  of each. This spec provides exactly one concrete implementation, backed
  by the existing baked-TIFF format; a later effort will add others.
- **Texture Level**: One entry in a baked texture's mip pyramid (or, for a
  flat/un-made texture, the sole level). Already an existing concept in
  the system; this spec changes how a level's pixel data is fetched, not
  what a level represents.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of existing visual-regression scenes that reference a
  baked texture, environment map, or shadow map produce byte-for-byte
  identical rendered output before and after this refactor — not merely
  within the project's existing visual-similarity tolerance, but exactly
  identical.
- **SC-002**: A newly-added automated test demonstrating concurrent
  multi-threaded tile fetches passes reliably and repeatably (no
  intermittent failures across repeated runs), where no equivalent test
  existed before this work.
- **SC-003**: A developer can identify, by inspecting the render-time
  texture lookup code alone, that exactly one format-specific
  implementation exists behind the new interface — with zero
  format-specific (TIFF) references remaining in the lookup code that
  calls into it.

## Assumptions

- This is a pure internal refactor with no user-facing capability change;
  "users" in the scenarios above are the people who render scenes
  (artists/TDs) and the people who maintain this codebase, not end users
  of a product in the conventional sense.
- No formal render-time performance target is set for this work — the
  refactor replaces direct calls with one layer of indirection through a
  virtual interface, which is not expected to be measurably slower; this
  is treated as expected engineering practice, not a criterion requiring
  a specific measured threshold.
- The renderer's existing per-cache-slot locking strategy (one mutex per
  cached tile, allowing different tiles to load concurrently) is assumed
  correct as a design and is preserved exactly — this spec's concurrency
  testing (User Story 2) verifies the *extraction* didn't break it, not
  that the underlying design itself is re-evaluated or changed.
- `CDeepShadow` (a texture type using an entirely different, non-baked-
  TIFF file mechanism) and the `otexmake` bake-time write path are both
  out of scope and unaffected — confirmed to be genuinely separate code,
  not merely deferred.
- The three formats spec 020 (a separate, later effort) is expected to
  add are out of scope here; this spec's only concrete implementation of
  the new interface is the existing TIFF-backed one.
