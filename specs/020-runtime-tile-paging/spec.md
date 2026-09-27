# Feature Specification: Runtime Bake-on-Load for Non-TIFF Texture Sources

**Feature Branch**: `020-runtime-tile-paging`

**Created**: 2026-09-26

**Status**: Draft

**Input**: User description: "Spec 020 of a 3-spec layered effort to add multi-format texture support to openRender: runtime bake-on-load for non-tileable (non-TIFF) texture sources. When a texture reference isn't a baked TIFF, fall back to decoding it directly and serving it through a new, synthesized tile-fetch backend that builds an in-memory mip pyramid once and serves tiles from it, letting artists reference PNG/EXR/RGBE files directly with texture(), with no otexmake bake step, while keeping the existing baked-TIFF fast path completely untouched. Includes an opt-in disk-cache sidecar so repeated renders don't repeat the decode+synthesis cost."

## Clarifications

### Session 2026-09-26

- Q: What should be the default texture-wrap (tiling) behavior for a
  texture referenced directly from an unbaked PNG/EXR/RGBE source, given
  there's no bake-time metadata (and no per-call `texture()` parameter
  today) to carry a wrap-mode choice the way a baked texture does? → A:
  Periodic — matches `otexmake`'s own CLI default (confirmed: `smode`/
  `tmode` both default to `"periodic"` when unspecified), so an unbaked
  texture behaves exactly like a freshly-baked one with no wrap arguments
  given.
- Q: How should an artist/pipeline enable the opt-in disk cache for
  prepared (synthesized) unbaked textures? → A: A scene-level RIB
  `Option` — this project's existing idiom for texture-related settings
  (e.g. `Option "searchpath" "texture"`); travels with the RIB file, so
  the choice is visible and portable per-scene.
- Q: Where should the opt-in disk cache's persisted files be written? →
  A: A configurable cache root, defaulting to a system temp/cache
  directory — resolves this spec's own "read-only asset directory" edge
  case directly (never requires write access next to the original
  source); a pipeline can still redirect it, matching this project's
  `ORENDERHOME`-style configurable-location convention.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Reference an unbaked image directly as a texture (Priority: P1)

An artist or technical director references a plain image file (a PNG
screenshot, an OpenEXR render pass, a Radiance HDR photo) directly as a
texture in a shader parameter, with no separate "bake this into a texture
format" step ever run on it beforehand. The render succeeds and the image
appears correctly mapped onto the surface, the same way it would if the
artist had baked it first.

**Why this priority**: This is the entire point of the feature — removing
a workflow step (baking) for the common case of "I just want to use this
picture as a texture." Without this, the feature delivers nothing.

**Independent Test**: Render a scene whose shader references a plain PNG
(and separately, an OpenEXR and a Radiance HDR image) with no bake step
ever run against that file; confirm the texture appears correctly mapped,
matching what baking the same source first and referencing the baked
result would have produced.

**Acceptance Scenarios**:

1. **Given** a shader parameter referencing a plain PNG image file that has
   never been baked, **When** the scene is rendered, **Then** the image is
   correctly sampled and mapped onto the surface.
2. **Given** the same source image, once baked via the existing bake step
   and once referenced directly unbaked, **When** both scenes are
   rendered, **Then** the two renders are visually equivalent (same
   mapping, same filtering behavior at a given mip level).
3. **Given** a shader parameter referencing a file that is neither a
   baked texture nor any supported plain image format, **When** the scene
   is rendered, **Then** the system reports the texture as not found,
   exactly as it does today for any unreadable texture reference — it
   does not crash or silently substitute the wrong image.

---

### User Story 2 - Repeated renders skip re-decoding the same unbaked source (Priority: P2)

A studio pipeline re-renders the same scene (or a batch of related scenes
sharing textures) many times — for lighting iteration, farm re-renders, or
incremental changes elsewhere in the scene. Decoding and preparing the
same unbaked source image from scratch on every single render is wasted
work once the source hasn't changed. An artist or pipeline can opt in to
a persisted, on-disk cache of the prepared texture so subsequent renders
reuse it instead of repeating that work — and if the source image is
later edited, the next render detects this and rebuilds the cache rather
than using outdated data.

**Why this priority**: A real, valuable optimization for production
workflows, but strictly secondary to User Story 1 delivering correct
output at all — a studio gets no benefit from a faster cache of a texture
that doesn't render correctly in the first place.

**Independent Test**: Render the same scene twice with the cache option
enabled; confirm the second render reuses the cached preparation (and
produces output indistinguishable from the first render). Then modify the
source image and render a third time; confirm the stale cache is detected
and rebuilt rather than silently reused.

**Acceptance Scenarios**:

1. **Given** the disk-cache option is enabled and a scene references an
   unbaked source for the first time, **When** it renders, **Then** a
   persisted cache of the prepared texture is written to disk.
2. **Given** that same scene is rendered again with the cache present and
   the source file unchanged, **When** it renders, **Then** the render
   reuses the existing cache rather than re-decoding the source, and
   produces rendered output indistinguishable from the first render.
3. **Given** the source image file is modified after the cache was
   written, **When** the scene is rendered again, **Then** the system
   detects the cache is out of date and rebuilds it from the modified
   source, rather than rendering with stale texture data.

---

### User Story 3 - Concurrent renders don't corrupt or crash on the new path (Priority: P3)

A scene shades many points simultaneously across multiple rendering
threads, and several of those points reference the same unbaked texture
at the same time — some faulting in different regions of it for the
first time, some re-reading regions another thread already prepared. This
is new code introduced by this feature (the existing baked-texture path's
own concurrency handling is untouched), so it needs its own proof that
concurrent access is safe, the same discipline this project already
applied when the equivalent seam was last touched.

**Why this priority**: Correctness under concurrency is a hard requirement
for a multi-threaded renderer, but it is a property of code enabled by
User Story 1 — there is nothing to test concurrently until the core
capability exists.

**Independent Test**: Render a scene that heavily reuses a single unbaked
texture reference under the renderer's normal multi-threaded operation,
repeated several times; confirm no crash, no data corruption, and
consistent (not merely "didn't crash") rendered output across repeated
runs.

**Acceptance Scenarios**:

1. **Given** multiple rendering threads simultaneously sampling
   previously-unrequested regions of the same unbaked texture, **When**
   they fault in concurrently, **Then** every thread receives correct
   pixel data with no data races or crashes.
2. **Given** multiple rendering threads simultaneously sampling a region
   of the same unbaked texture that another thread has already prepared,
   **When** they read concurrently, **Then** every thread receives the
   correct, complete data.

---

### Edge Cases

- What happens to a scene that already references a properly baked TIFF
  texture? Nothing about its behavior may change — the existing baked-
  texture path MUST be tried first and MUST take priority unconditionally
  whenever it succeeds; this feature is reached only when that path
  fails.
- What happens to environment maps and shadow maps referencing an unbaked
  source? These are explicitly unsupported by this feature (they require
  projection/orientation metadata a plain image file has no way to carry)
  and MUST continue behaving exactly as they do today — reported as not
  found if not already a valid baked file, the same as before this
  feature existed.
- What happens if the disk cache is enabled but the location it would
  write to isn't writable (e.g. a read-only, shared asset directory)?
  The render MUST still succeed via the in-memory-only path — a disk
  cache write failure degrades to "no caching happened," never to a
  failed or incorrect render.
- What happens to an unbaked source image whose dimensions aren't a
  power of two (the overwhelmingly common case for a photo or a
  screenshot, as opposed to a purpose-authored texture)? It MUST still
  work, handled the same way this project's existing bake step already
  handles non-power-of-two sources today.
- What happens for a texture reference that has already been fully
  prepared (in memory or via a valid disk cache) and is simply being read
  again? This MUST remain as fast and correct as any other already-cached
  texture read — no new per-read overhead this feature doesn't already
  require.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The system MUST render a `texture()` reference to a
  supported plain image file (PNG; OpenEXR when the running build
  includes OpenEXR support; Radiance HDR/RGBE) that has never been through
  a separate bake step, producing correctly mapped, correctly filtered
  output.
- **FR-002**: The system MUST try the existing baked-texture path first
  for every texture reference, and MUST NOT alter its behavior in any way
  — this feature's new path is reached only when a reference fails to
  open as a valid baked texture.
- **FR-003**: When a texture reference fails to open as a baked texture,
  the system MUST attempt to decode it as one of the supported plain
  image formats before reporting it as not found.
- **FR-004**: The in-memory representation the system builds for an
  unbaked source MUST provide the same mip-level (level-of-detail)
  structure and level-reduction behavior that the project's existing bake
  step produces for a baked source, so filtering/mip-selection behaves
  consistently regardless of whether a texture was baked or referenced
  directly.
- **FR-005**: The system MUST support an opt-in mechanism to persist the
  prepared (decoded + mip-leveled) representation of an unbaked source to
  disk, so that a subsequent render referencing the same, unchanged source
  does not repeat the decode-and-prepare work.
- **FR-006**: When the opt-in disk cache is in use, the system MUST detect
  when a source file has been modified more recently than its persisted
  cache and rebuild the cache from the current source, rather than
  serving stale data.
- **FR-007**: A failure to write the opt-in disk cache (e.g. an
  unwritable location) MUST NOT fail or alter the render — the system
  falls back to preparing the texture in memory only for that render.
- **FR-008**: This feature MUST NOT extend to environment map or shadow
  map lookups — those remain baked-texture-only, unchanged by this work.
- **FR-009**: This feature MUST NOT require any new external decoding
  library or build dependency beyond what the project already supports
  for plain image decoding.
- **FR-010**: This feature MUST NOT change the existing baked-texture
  container format, the existing bake-tool command-line interface, or any
  bake-time workflow already relied upon by existing scenes/pipelines.
- **FR-011**: Concurrent `texture()` lookups from multiple rendering
  threads against the same unbaked-source texture MUST remain correct (no
  data corruption, no crash) — verified by an automated test, not
  asserted by inspection alone.
- **FR-012**: An unbaked source image whose dimensions are not a power of
  two MUST be handled correctly, using the same resizing behavior the
  project's existing bake step already applies to non-power-of-two
  sources.
- **FR-013**: The system MUST default to periodic texture
  wrapping/tiling behavior when a texture is referenced without ever
  having been baked, matching this project's own existing default for a
  texture baked with no wrap-mode arguments given.
- **FR-014**: The opt-in disk cache MUST be enabled via a scene-level
  option, so the choice travels with the scene (RIB file) that made it,
  consistent with this project's existing convention for other texture-
  related settings.
- **FR-015**: The opt-in disk cache's persisted files MUST be written to
  a configurable cache location, defaulting to a system temporary/cache
  directory when not otherwise configured — never requiring write access
  next to the original source image.

### Key Entities

- **Unbaked Texture Source**: A plain image file (PNG, OpenEXR, or
  Radiance HDR) referenced directly by a shader as a texture, with no
  prior bake step. Distinguished from a baked texture only by how the
  system discovers and prepares it — once prepared, it behaves as a
  normal texture to everything downstream.
- **Prepared (Synthesized) Texture**: The in-memory, ready-to-sample
  representation the system builds once from an Unbaked Texture Source —
  conceptually equivalent to what a baked texture already provides, but
  built at texture-load time instead of ahead of time via a separate
  step.
- **Disk Cache Entry**: An opt-in, persisted copy of a Prepared
  (Synthesized) Texture, associated with the source file it was built
  from and the point in time it was built, so a later render can detect
  whether it's still valid for that source or must be rebuilt.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An artist can reference a PNG, OpenEXR (when supported), or
  Radiance HDR image file directly as a texture, with zero separate bake
  step, and see it rendered correctly — demonstrated by new example
  scenes covering all three formats.
- **SC-002**: 100% of existing scenes/regression tests that reference an
  already-baked texture, environment map, or shadow map continue to
  produce byte-for-byte identical rendered output after this feature is
  added — zero regression to any already-supported workflow.
- **SC-003**: With the disk cache enabled, a second render referencing
  the same, unchanged unbaked source completes its texture preparation
  step without repeating the decode-and-build work the first render did,
  while producing rendered output indistinguishable from a render that
  built the texture fresh in memory.
- **SC-004**: A source file modified after its disk cache entry was
  written is never silently rendered with stale texture data — 100%
  detection in a dedicated, repeatable test.
- **SC-005**: A new automated test demonstrating concurrent, multi-
  threaded access to an unbaked-source texture passes reliably and
  repeatably (no intermittent failures across repeated runs), where no
  equivalent test existed before this work.

## Assumptions

- "Users" in the scenarios above are the people who render scenes
  (artists/TDs) and the people who operate rendering pipelines, not end
  users of a separately-shipped product.
- This feature adds no new supported image format beyond what this
  project's existing plain-image decoding already covers (PNG, OpenEXR
  when built with OpenEXR support, Radiance HDR); a further, separate
  future effort could add more formats without needing to revisit this
  feature's own design.
- Environment maps and shadow maps are out of scope, not merely deferred
  — they require projection/orientation metadata that only an authored
  bake step produces, and a plain image file structurally cannot supply
  it.
- No formal render-time performance target is fixed for preparing an
  unbaked source in memory (the disk-cache option exists specifically to
  amortize this cost across repeated renders); the expected benefit is
  qualitative (avoiding repeated work), not bound to a specific numeric
  threshold in this spec.
- The disk cache, once written, is expected to be read back by the
  ordinary, already-existing baked-texture path (i.e. the persisted
  representation is itself a valid baked texture) rather than requiring
  any new read-side logic of its own.
