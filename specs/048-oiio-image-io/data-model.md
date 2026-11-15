# Data Model: OIIO Image I/O Backend (spec 048)

Entities, the values they hold, and their rules. The interfaces are in [contracts/](contracts/). Names are proposals
the tasks stage keeps unless review changes them.

## 1. Backend selection

| Entity | Owner (class) | Values | Where it lives |
|---|---|---|---|
| `EImageBackend` | Texturing | `Legacy` (0, default), `OIIO` | `src/ri/texture/imageInput.h` |
| `EImageIOBackend` | Graphics State vocabulary | `Legacy`, `OIIO` | `src/ri/state/stateVocabulary.h` |
| `COptions::imageBackend` | Graphics State | `EImageIOBackend`, default `Legacy` | `options.h`, initialised in `options.cpp` beside `textureCacheEnabled` |
| `CTextureSystemSettings::imageBackend` | Texturing | `EImageBackend`, default `Legacy` (zero-initialised `= {}` already) | `textureSystem.h` |
| `Make*` parameter `"imageio"` | Scene Interface token, read by Texturing | `"legacy"`, `"oiio"` | `texmake.cpp` parameter loop (beside `"resize"`) |
| `otexmake -imageio` | consumer | `legacy`, `oiio` | appended as the `"imageio"` parameter |

**Rules**
- Precedence for a `Make*` call: its own `"imageio"` parameter, then `COptions::imageBackend` (the core appends
  `"imageio"` when the call has none and the Option is `oiio`), then `legacy`.
- Render-time decode of unbaked sources and the disk cache read `CTextureSystemSettings::imageBackend`. The core maps
  `COptions` to it in `CRenderer::beginFrame` (`renderer.cpp:775`), next to the disk-cache fields.
- Graphics State never includes a Texturing header; the core does the mapping (a `switch`), with a `static_assert`
  that `EImageIOBackend` and `EImageBackend` have the same enumerators and values, so one cannot drift from the other.
- Unknown value: `error(CODE_BADTOKEN, …)`, and the previous value stays.
- Value `oiio` in a build without `HAVE_OIIO`: for the Option, one error and the value stays `legacy`; for a
  per-call parameter or `otexmake`, the call fails with the error (P-Q6).
- The disk-cache entry name (spec 020) gains a `.oiio` component only when the backend is `OIIO`. Legacy names are
  unchanged (FR-014).

## 2. Texturing interface changes (additive)

```text
enum class EImageBackend { Legacy, OIIO };

CImageInput *createImageInput(const char *filename);                          // unchanged, == Legacy
CImageInput *createImageInput(const char *filename, EImageBackend backend);   // new overload

class CImageInput {
    …existing open/readImage/close…
    // New, default returns false. Reads a 4x4 float matrix stored with the source:
    // "worldtocamera" or "worldtoscreen" (TIFF Pixar tags). Valid after a successful open().
    virtual bool readMatrix(const char *name, float m[16]);
};
```

- `CTiffImageInput::readMatrix` reads `TIFFTAG_PIXAR_MATRIX_WORLDTO{CAMERA,SCREEN}` with libtiff. It is the code
  `makeSideEnvironment` runs today through `dynamic_cast<CTiffImageInput *>(…)->getHandle()`
  (`texmake.cpp:483-500`), moved, not changed. `texmake.cpp` then calls `imageInput->readMatrix(…)`.
  `ImageInput_TiffBakeByteIdentical` pins the shadow bake bytes.
- `COiioImageInput::readMatrix` returns OIIO's `worldtocamera`/`worldtoscreen` attributes, which
  `tiffinput.cpp:1414-1419` fills from the same tags.
- PNG, RGBE and EXR keep the default `false`, so the shadow bake takes the same "tag missing" path as today.
- Suffix table: one table for both backends (`.tif`, `.tiff` -> tiff; `.png` -> png; `.hdr`, `.pic` -> hdr; `.exr` ->
  openexr when `HAVE_OPENEXR`, else the existing error). Any other suffix returns `nullptr` on both paths (FR-023).
- `OIIO` in a translation unit compiled without `HAVE_OIIO` (`libRIBVector`, an OIIO-off build):
  `error(CODE_BADTOKEN, "OpenImageIO support not built into this binary: \"%s\"\n", filename)` and `nullptr`, the
  same shape as the EXR message (`imageInput.cpp:71-76`).

## 3. The OIIO decode path

### `orimageio::SourceTraits` (facts, filled by the library)

| Field | Meaning | Source |
|---|---|---|
| `format` | `Tiff`, `Png`, `Rgbe`, `OpenExr` | the format the reader was created for |
| `formatName` | OIIO's `format_name()` after open | must equal the expected name |
| `width`, `height`, `channels` | data window | `ImageSpec` |
| `storedBits` | bits per sample as stored | `oiio:BitsPerSample` / `tiff:BitsPerSample`; PNG IHDR byte 24 |
| `storedKind` | `UInt`, `Int`, `Float` | `spec.format.basetype` before conversion |
| `tiled` | tiled source | `spec.tile_width > 0` |
| `planarSeparate` | TIFF PLANARCONFIG_SEPARATE | `tiff:PlanarConfiguration` |
| `photometric` | TIFF photometric code, 0 if not TIFF | `tiff:PhotometricInterpretation` |
| `subimages` | count, stopping at 2 | `seek_subimage(1, 0)` |
| `deep` | deep data | `spec.deep` |
| `channelNames` | stored order | `spec.channelnames` |
| `pngColorType`, `pngInterlace` | PNG IHDR bytes 25 and 28 | a 33-byte read of the file header |

### Delivered sample type (matches `CImageInfo`)

| OIIO stored type | Delivered | `bitsPerSample` / `isFloatFormat` |
|---|---|---|
| UINT8 | uint8 | 8 / false |
| UINT16 | uint16, native byte order | 16 / false |
| HALF | float (exact widening) | 32 / true |
| FLOAT | float | 32 / true |
| anything else (UINT32, INT8, INT16, INT32, DOUBLE) | refused | n/a |

Layout: row-major, tightly packed, channels in stored order (EXR: OIIO's order is R,G,B,A first, equal to the legacy
reorder for the accepted RGB/RGBA/1-channel layouts).

### Acceptance (applied by `COiioImageInput`, legacy's rules; P-Q5)

| Format | Refused when | Message names |
|---|---|---|
| any | `formatName` != expected (content is another format) | file, expected format |
| PNG | `pngInterlace != 0`; `pngColorType == 3`; `storedBits` not 8/16 | file, reason (as `imageInputPng.cpp`) |
| TIFF | `storedBits` not 8/16/32; `tiled`; delivered type refused | file, reason |
| EXR | `subimages > 1`; `deep`; layout not {R,G,B}, {R,G,B,A} or 1 channel | file, reason (as `imageInputExr.cpp`) |
| RGBE | `channels != 3` (OIIO always gives 3) | file |

Everything accepted here that legacy also accepts must decode identically, unless it has an entry on the difference
list.

### State transitions (one-shot contract, unchanged)

`created -> open() ok -> readImage() once -> close()`; `open()` false -> closed, error reported; `readImage()` before
`open()` or twice -> false. `close()` is idempotent; the destructor closes.

## 4. The bake sink (`texmake.cpp`)

```text
struct CBakeSink {                 // file-local to texmake.cpp
    virtual void setTextureFormat(const char *);       // dir-scoped, like TIFFSetField
    virtual void setWrapModes(const char *);
    virtual void setMatrices(const float w2c[16], const float w2s[16]);
    virtual void setFullSize(int w, int h);
    virtual void appendLayer(int numSamples, int bits, int tileSize, int w, int h, const void *data) = 0;
    virtual bool close() = 0;
};
```

- **Pending-tag semantics**: tags set before `appendLayer` apply to that directory only, then clear. That is
  libtiff's `TIFFWriteDirectory` behaviour, which the legacy writer relies on (format/wrap on directory 0, full size
  on the first directory of each pyramid).
- `CLegacyBakeSink` calls `TIFFSetField`/the current `appendLayer` body. The bytes are identical, pinned by
  `ImageInput_TiffBakeByteIdentical`.
- `COiioBakeSink` stores the pending tags and calls `orimageio::TiledTiffWriter::appendDirectory`
  ([contracts/bake-writer.md](contracts/bake-writer.md)).
- **Gate** `kOiioBakeExclusions` (one table, checked once per `make*` call before any output is created):

  The rule: OIIO writes a combination only if its `PHOTOMETRIC` and `EXTRASAMPLES` equal legacy's
  (`texmake.cpp:125-170` against `tiffoutput.cpp:657`, `:815-833`); otherwise the legacy writer, with a message.

  | Bits | Channels | Legacy writes | OIIO can write | Writer |
  |---|---|---|---|---|
  | 8, float | 1 | MINISBLACK | MINISBLACK (`alpha_channel` -1) | OIIO |
  | 8, float | 2 | MINISBLACK + UNASSALPHA | MINISBLACK + UNASSALPHA (`alpha_channel` 1, `oiio:UnassociatedAlpha` 1) | OIIO |
  | 8, float | 3 | RGB | RGB | OIIO |
  | 8, float | 4 | RGB + UNASSALPHA | RGB + UNASSALPHA (`alpha_channel` 3) | OIIO |
  | 8, float | >= 5 | PHOTOMETRIC unset, no EXTRASAMPLES | RGB forced | **legacy** |
  | 16 | 1 | MINISBLACK | MINISBLACK | OIIO |
  | 16 | 2 | MINISBLACK, no EXTRASAMPLES | MINISBLACK, EXTRASAMPLES off (`alpha_channel` -1, `tiff:write_extrasamples` 0) | OIIO |
  | 16 | >= 3 | MINISBLACK | RGB forced (no override); the reader doubles 16-bit RGB (`texture.cpp:2476`, issue #18) | **legacy** |

  If P-Q3 is answered (c), a row "valid size == data size -> legacy" is added.

## 5. The `oiio` display driver instance

| Field | From |
|---|---|
| format | `"string type"` (`tiff`, `png`, `openexr`, `rgbe`), else the extension, else error (P-Q7) |
| width, height, numSamples | `OrDisplayInfo` |
| sample type | quantize: 8-bit -> UINT8, 16-bit -> UINT16, none -> FLOAT (TIFF), UINT16 if the quantize maximum > 255 else UINT8 (PNG, `file_png.cpp:86`), HALF (EXR), FLOAT (RGBE) |
| channel names | `R,G,B,A,Z` by position |
| compression | the `compression` parameter, mapped per format (contract) |
| matrices | `NP`, `Nl` (TIFF only) |
| software | `Software` |

Lifecycle: `start` opens a `ScanlineWriter`; `data` gives rows to `CFileOutputBase`, which calls the driver's
`flushRow`; `finish` closes and reports errors. No exception crosses `start`/`data`/`finish`.

## 6. The difference list

One data file, `tests/unit/imageio/differences.txt`. It is rendered into `DEVNOTES_DETAILS/IMAGE_IO.md` and
checked by `ImageIO_DifferenceListDoc`, which requires that both name the same IDs.

| Field | Meaning |
|---|---|
| `id` | `D-<AREA>-<NAME>` |
| `fixture` | file under `tests/unit/imageio/fixtures/` |
| `legacy` | `accept:<summary>` or `refuse` |
| `oiio` | `accept:<summary>` or `refuse` |
| `check` | what the test asserts (exact values, channel counts, error text) |
| `issue` | GitHub issue if it records a legacy defect |

A difference the parity test finds that is not on the list is a failure (FR-012, SC-009). Initial entries:
[contracts/difference-list.md](contracts/difference-list.md).

## 7. Parity fixtures

- Reuse the 018 fixtures (`tests/unit/image_input/fixtures/`, formulas in its `FIXTURES.md`). The approved
  references there are never edited.
- Add, under `tests/unit/imageio/fixtures/`, generated from the same closed-form formulas by a checked-in generator
  (`tools/gen_fixtures.cpp`, test-only, libtiff/libpng/OpenEXR):
  - TIFF 8/16/float with 1, 2, 3 and 4 channels;
  - EXR half with ties, denormals and overflow;
  - the difference fixtures;
  - a JPEG, a TGA, a BMP and a WebP for the FR-023 refusals (a few hundred bytes each).
- The display-parity scene is a small deterministic RIB (16x16, no dither, no `jpeg` compression), rendered once per
  driver and format.
