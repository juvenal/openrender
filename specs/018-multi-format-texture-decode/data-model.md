# Phase 1 Data Model: Multi-Format Texture Source Decoding

This feature has no persistent data store — its "data model" is the shape
of the in-memory image data as it flows from a source file, through the
new decode abstraction, into the unchanged bake pipeline.

## Source Image

The artist-supplied input to a bake operation. Represented in-memory by a
`CImageInfo` descriptor plus a flat, row-major pixel buffer.

| Field | Type | Description | Validation |
|---|---|---|---|
| `width` | int | Pixel width | > 0 |
| `height` | int | Pixel height | > 0 |
| `numChannels` | int | Samples per pixel | 1, 2, 3, or 4 depending on format (see per-format rules below) |
| `bitsPerSample` | int | Native sample bit depth | 8, 16, or 32 only — no half-precision tier |
| `isFloatFormat` | bool | Whether samples are IEEE float | `bitsPerSample == 32` implies this is `true` for every format this feature supports |

**Per-format validation rules** (rejection happens at `open()`, before any
pixel data is read):

- **TIFF**: unchanged from current `readLayer()` behavior — whatever
  channel count/bit depth the file declares is accepted as today.
- **PNG**: `numChannels` derives from color type — RGB → 3, RGBA → 4,
  Grayscale → 1, Grayscale+Alpha → 2. `bitsPerSample` is 8 or 16 (16-bit
  samples normalized to native byte order). Indexed/palette color type is
  rejected.
- **OpenEXR**: single-part files only — any file with more than one part
  is rejected unconditionally, regardless of its content. `numChannels`
  derives from the accepted channel sets: `{R,G,B}` → 3, `{R,G,B,A}` → 4,
  a single channel (luminance) → 1. Any other channel set is rejected.
  `HALF`-stored channels are promoted to `bitsPerSample = 32`,
  `isFloatFormat = true` on decode.
- **RGBE**: always `numChannels = 3`, `bitsPerSample = 32`,
  `isFloatFormat = true` — the format has no other valid shape.

**Lifecycle**: A `CImageInput` instance is opened once (`open()` populates
`CImageInfo`), read exactly once (`readImage()` fills a caller-allocated
buffer sized from `CImageInfo`), then closed. This is a one-shot,
non-streaming contract — it matches how the existing `readLayer()` is
already used at every one of its five call sites (open → read whole image
→ proceed to bake), and is not reused across multiple bake invocations.

## Baked Texture (unchanged, referenced for scope clarity only)

The tiled, mipmapped, Pixar-tag-annotated TIFF file `otexmake` writes.
This feature does not add, remove, or alter any field of this format — it
only changes what kinds of Source Images can produce one. No new fields or
validation rules apply here.

## Format Registration Table

A static, compile-time table mapping recognized filename suffixes to the
concrete `CImageInput` subclass responsible for that format, consulted by
`createImageInput()`:

| Suffix | Concrete Type | Always Available? |
|---|---|---|
| `.tif`, `.tiff`, *(no match / fallback)* | `CTiffImageInput` | Yes (TIFF is the existing mandatory dependency and the default when no other suffix matches, preserving current behavior for unrecognized filenames) |
| `.png` | `CPngImageInput` | Yes (PNG is an existing mandatory dependency) |
| `.exr` | `COpenExrImageInput` | Only when `HAVE_OPENEXR` — otherwise a `.exr` source produces a clear "OpenEXR support not built into this binary" error |
| `.hdr`, `.pic` | `CRgbeImageInput` | Yes (in-tree codec, no external dependency) |
