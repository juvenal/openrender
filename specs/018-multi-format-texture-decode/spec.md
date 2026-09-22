# Feature Specification: Multi-Format Texture Source Decoding for otexmake

**Feature Branch**: `018-multi-format-texture-decode`

**Created**: 2026-09-22

**Status**: Draft

**Input**: User description: "Spec 018 of a 3-spec layered effort to add multi-format texture support to openRender: introduce an image-decode abstraction and wire it into otexmake's bake pipeline (texmake.cpp) so PNG, OpenEXR, and RGBE/Radiance HDR files can be used as texture bake sources alongside TIFF, with byte-identical output preserved for existing TIFF sources and no change to the baked-texture output format or the runtime render-time read path."

## Clarifications

### Session 2026-09-22

- Q: Should a multi-part OpenEXR file ever be accepted if it happens to contain exactly one part with a supported RGB/RGBA/luminance layout? → A: No — any multi-part OpenEXR file is rejected outright, regardless of what it contains.
- Q: Which PNG color types must `otexmake` support as bake sources? → A: RGB, RGBA, Grayscale, and Grayscale+Alpha only — indexed/palette PNGs are rejected with a clear error, mirroring the EXR channel-layout scope decision above.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Bake a texture from a PNG source (Priority: P1)

A texture artist has a source texture delivered as a PNG file (the common case for
color/albedo maps from 2D paint tools) and wants to bake it into openRender's
native tiled/mipmapped texture format using `otexmake`, without first
round-tripping it through an external tool to convert it to TIFF.

**Why this priority**: PNG is the most common non-TIFF source format artists
already have on hand; this is the smallest, lowest-risk slice that proves the
new decode abstraction end-to-end and delivers immediate value on its own.

**Independent Test**: Run `otexmake` against a sample PNG file with the same
invocation used for a TIFF source today; confirm a valid baked texture is
produced and that its pixel content matches the source.

**Acceptance Scenarios**:

1. **Given** a valid 8-bit RGB PNG source file, **When** an artist bakes it
   with `otexmake` using no new flags, **Then** `otexmake` produces a tiled,
   mipmapped texture file that renders and behaves exactly like one baked
   from an equivalent TIFF source.
2. **Given** a PNG source file with an alpha channel, **When** it is baked,
   **Then** the alpha channel is preserved in the resulting texture.
3. **Given** a 16-bit-per-channel PNG source file, **When** it is baked,
   **Then** the full 16-bit precision is preserved rather than being
   silently downsampled to 8-bit.
4. **Given** an indexed/palette PNG source file, **When** baking is
   attempted, **Then** `otexmake` fails with a clear, specific error rather
   than de-palettizing it or producing incorrect color output.

---

### User Story 2 - Bake a texture from an OpenEXR source (Priority: P2)

A lighting or look-development artist has HDR source data (e.g. a scanned or
rendered reflectance/lighting map) stored as OpenEXR and wants to bake it
into a texture while preserving its full floating-point dynamic range.

**Why this priority**: OpenEXR is the standard HDR interchange format in
production pipelines; this validates that the decode abstraction correctly
carries floating-point precision through the existing bake pipeline, and
establishes the "reject clearly rather than guess" behavior for content the
format supports but this feature does not.

**Independent Test**: Bake an RGB float OpenEXR source and confirm the
resulting texture reproduces values outside the standard 0-1 range; bake an
OpenEXR source with an unsupported channel layout and confirm a clear error
is reported instead of a wrong or silently-truncated result.

**Acceptance Scenarios**:

1. **Given** an RGB or RGBA floating-point OpenEXR source file, **When** it
   is baked, **Then** the resulting texture retains full floating-point
   precision, including values outside the 0-1 range.
2. **Given** a single-channel (luminance) OpenEXR source file, **When** it
   is baked, **Then** it produces a valid single-channel texture.
3. **Given** an OpenEXR source file with a channel set beyond
   RGB/RGBA/luminance, **When** baking is attempted, **Then** `otexmake`
   fails with a clear, specific error identifying the file and the reason,
   rather than guessing a channel subset or crashing.
4. **Given** a multi-part OpenEXR source file, **When** baking is
   attempted, **Then** `otexmake` rejects it outright with a clear error —
   unconditionally, even if the file contains exactly one part with an
   otherwise-supported RGB/RGBA/luminance layout.

---

### User Story 3 - Bake a texture from an RGBE / Radiance HDR source (Priority: P3)

An artist has a legacy or externally-sourced Radiance-format HDR file
(`.hdr`/`.pic`) — commonly used for environment/reflection maps — and wants
to bake it into openRender's native texture format.

**Why this priority**: Lowest priority because RGBE sources are less common
than PNG or OpenEXR in most pipelines, but it delivers real value at very low
implementation cost, since a complete RGBE reader already exists in the
codebase and only needs to be connected to the bake pipeline.

**Independent Test**: Bake a valid RGBE source file and confirm the
resulting texture's decoded radiance values match the source.

**Acceptance Scenarios**:

1. **Given** a valid RGBE (`.hdr`/`.pic`) source file, **When** it is baked,
   **Then** `otexmake` succeeds and produces a texture whose values match
   the source file's decoded radiance data.

---

### Edge Cases

- What happens when the source file's extension does not match a supported
  format, or matches no known format at all? `otexmake` MUST fail with a
  clear error naming the file, rather than silently guessing a format or
  crashing.
- What happens when a source file has a supported extension but is
  corrupted or not actually valid content for that format (e.g. a truncated
  PNG, or a file renamed to `.exr` that isn't one)? `otexmake` MUST fail
  cleanly with a diagnostic error, not crash or produce garbage output.
- What happens when baking an environment map (cubic/spherical/cylindrical)
  or a shadow map — not just a plain 2D texture — from a non-TIFF source?
  These bake modes MUST gain the same multi-format source support as the
  plain texture-bake path, since they share the same underlying read
  routine.
- What happens when an existing TIFF source is baked after this change?
  Output MUST be byte-for-byte identical to today's behavior — this is a
  regression requirement, not just a "should still work" expectation.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: `otexmake` MUST accept PNG, OpenEXR, and RGBE (Radiance HDR)
  files as bake sources, in addition to the currently-supported TIFF, with
  no new command-line flag required to select the format.
- **FR-002**: `otexmake` MUST determine a source file's format automatically
  from the file itself (by extension), consistent with how the existing
  render-output side already auto-detects output format from filename.
- **FR-003**: `otexmake` MUST preserve each source's native sample
  precision (8-bit integer, 16-bit integer, or floating point) when baking,
  rather than always converting to one fixed precision regardless of the
  source.
- **FR-004**: For OpenEXR sources, `otexmake` MUST support single-part
  files with RGB, RGBA, or single-channel/luminance layouts. Sources with
  additional/unsupported channels MUST be rejected with a clear, specific
  error rather than an automatic or partial interpretation. Multi-part
  files MUST be rejected outright and unconditionally — even one
  containing exactly one part with an otherwise-supported layout.
- **FR-005**: For PNG sources, `otexmake` MUST support RGB, RGBA,
  Grayscale, and Grayscale+Alpha color types. Indexed/palette PNG sources
  MUST be rejected with a clear, specific error rather than being
  de-palettized or otherwise auto-converted.
- **FR-006**: `otexmake` MUST apply no colorspace or gamma conversion when
  decoding any source format — pixel samples are read and baked as-is, with
  no change from the current raw-sample-copy behavior for TIFF sources.
- **FR-007**: Baking a texture, environment map (cubic/spherical/
  cylindrical), or shadow map from an existing TIFF source MUST produce
  output that is byte-for-byte identical to the tool's pre-change behavior.
- **FR-008**: `otexmake` MUST report a clear, actionable error — not a
  crash, hang, or silently incorrect result — when given a source file that
  is an unsupported format, or that is corrupted/unreadable within a
  supported format.
- **FR-009**: This feature MUST NOT require any change to how the renderer
  loads or reads baked textures at render time (`texture()`/`environment()`
  lookups) — the baked-texture output remains the existing tiled/mipmapped
  format, loadable exactly as it is today.
- **FR-010**: RGBE (Radiance HDR) source support MUST reuse the existing,
  already-implemented RGBE decode routines in the codebase rather than
  reimplementing RGBE decoding from scratch.

### Key Entities

- **Source Image**: The artist-supplied input file to a bake operation —
  one of TIFF, PNG, OpenEXR, or RGBE — characterized by its width, height,
  channel count, and native sample precision. Only its pixel data and
  dimensions are consumed; no metadata beyond that is required for this
  feature.
- **Baked Texture**: The tiled, mipmapped output file `otexmake` produces
  (openRender's existing native runtime texture format). This feature does
  not change its shape, structure, or how the renderer consumes it — only
  what kinds of source files can produce one.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: An artist can bake a usable texture from a PNG, OpenEXR, or
  RGBE source file using the same `otexmake` invocation pattern already
  used for TIFF sources today, with no new required flags or steps.
- **SC-002**: Baking from an existing TIFF source produces output that is
  byte-for-byte identical to the tool's current behavior, across 100% of
  the project's existing bake-related test scenes.
- **SC-003**: A texture baked from a PNG, OpenEXR, or RGBE source renders
  correctly when compared against a reference render of an equivalent
  TIFF-sourced texture, agreeing across both the reyes and raytrace camera
  hiders within the project's established cross-hider parity tolerance.
- **SC-004**: Attempting to bake from an unsupported or malformed source
  file produces a clear, immediate error message rather than a crash, a
  hang, or silently incorrect output, in 100% of tested cases.

## Assumptions

- Source format is determined by file extension, matching the convention
  already used by the project's render-output format dispatch — this
  feature does not perform deep content/magic-byte sniffing to identify a
  format independent of its extension.
- `otexmake`'s existing command-line interface is unchanged; only the set
  of source file formats it can read grows.
- The renderer's runtime texture-read path, the on-disk baked-texture
  container format, and environment/shadow-map projection metadata are all
  unaffected by this feature — they are explicitly out of scope here and
  are addressed (where applicable) by two later, separate efforts in this
  same multi-stage initiative.
- OpenImageIO is not introduced as a dependency by this feature; later
  integration of OpenImageIO as an additional source format is a possible
  future extension, not part of this work.
- OpenEXR sources storing 16-bit ("half") floating-point channels are
  decoded and baked at full 32-bit floating-point precision, matching the
  existing float-precision tier already used for other 32-bit-float
  sources in the bake pipeline.
- The point-cloud-based 3D brick-map bake mode (`-texture3d`) is unaffected
  by this feature — it consumes point-cloud data, not 2D images, and is out
  of scope.
