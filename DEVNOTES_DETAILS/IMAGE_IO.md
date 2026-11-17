# Image I/O through OpenImageIO (spec 048)

The optional OpenImageIO (OIIO) backend of openRender's image input and output. It is off by default
(`OPENRENDER_ENABLE_OIIO`, constitution 1.5.0's Image I/O utility library, `libRIBImageIO`), and nothing changes
unless it is asked for. This guide grows by phase: this version covers decoding; baking and the display driver are added
by their phases.

## The decode path

`createImageInput(path, EImageBackend::OIIO)` (`src/ri/texture/imageInput.h`) returns a `COiioImageInput`
(`imageInputOiio.{h,cpp}`) behind the same `CImageInput` interface the legacy decoders implement. It owns an
`orimageio::Reader` (`src/imageio/reader.cpp`, the only code that includes OIIO headers) which

- creates the OIIO reader **by format name** (`tiff`, `png`, `hdr`, `openexr`), never by probing, so the file's content and
  its suffix must agree, and checks `format_name()` after opening;
- opens with `oiio:UnassociatedAlpha=1` and `oiio:RawColor=1`, so samples arrive as stored (no alpha association, no
  colour-model conversion);
- reports facts about the source (`SourceTraits`: the stored type and bit depth, tiled, planar, photometric, the second
  subimage, deep, the channel names, the PNG header's colour type and interlace method) and reads the pixels once.

`COiioImageInput` applies the legacy decoders' acceptance rules to those facts, delivers the sample types of `CImageInfo`
(uint8, uint16, and float32 for half and float; there is no half tier) in row-major, tightly packed, stored channel order,
and enforces the one-shot contract (open once, read at most once, then close). One suffix table serves both backends:
`.tif` and `.tiff`; `.png`; `.hdr` and `.pic`; `.exr` (with OpenEXR); any other suffix, JPEG, TGA, BMP and WebP included,
is refused on both.

## Acceptance policy (P-Q5): the OIIO path refuses what the legacy decoders refuse

| Format | Refused when |
|---|---|
| any | the content is another format than the suffix says |
| PNG | interlaced; indexed (palette); a bit depth other than 8 or 16 |
| TIFF | bits per sample other than 8, 16 or 32; tiled; a stored type with no `CImageInfo` tier (any type other than uint8, uint16, half and float) |
| EXR | multi-part; deep; a layout other than R,G,B, R,G,B,A or one channel |
| RGBE | not 3 channels |
| missing, truncated, corrupted | always; no partial image |

Every refusal reports an error that names the file and the reason. Everything accepted by both paths decodes bit for bit
the same, except the entries below.

## The difference list

Files the legacy decoders **accept** and the two paths decode differently. The data file is
`tests/unit/imageio/differences.txt`; `ImageIO_DecodeDifferences` runs one check per entry and
`ImageIO_DifferenceListDoc` requires this table to name the same ids. A difference the parity test meets that is not
listed is a failure. The entries are predictions from the OIIO 3.1.14 source, confirmed or corrected by their tests.

| Id | Fixture | Legacy | OIIO | Note |
|---|---|---|---|---|
| D-RGBE-EXP | `rgbe_exp3.hdr`, `rgbe_exp256.hdr` | exact `ldexp` values | exponent bytes 40, 223 and 226 one float ulp lower (OIIO's decimal literal table); the other 253 identical | far outside any texture's range (about 1e-29, 1e26, 1e27) |
| D-RGBE-RLE | `rle_16x4_rle.hdr`, `rle_16x4_flat.hdr` | cannot read an RLE file (`RGBE_ReadPixels` reads flat pixels) | decodes it correctly | legacy defect, GitHub #171, not fixed here |
| D-PNG-TRNS | `trns_rgb8_4x4.png` | 3 channels | 4 channels (tRNS becomes alpha) | alpha 0 where the pixel equals the colour key, else 255 |
| D-TIFF-SEPARATE | `tiff_separate_rgb8_4x4.tif` | reads plane 0 into an interleaved buffer | interleaved correctly | legacy defect, not fixed here |
| D-TIFF-PALETTE | `tiff_palette8_4x4.tif` | 1 channel, the raw indices | 3 channels, the palette expanded | |
| D-TIFF-MINISWHITE | `tiff_miniswhite8_4x4.tif` | the raw values | inverted | to be confirmed with `oiio:RawColor=1` |
| D-TIFF-UINT32 | `tiff_uint32_4x4.tif` | 32 bits per sample read as float bits | refused: no `CImageInfo` tier | legacy defect, not fixed here |
| D-TIFF-INT16 | `tiff_int16_4x4.tif` | the raw signed bits delivered as uint16 | refused | legacy defect, not fixed here |
| D-TIFF-HALF | `tiff_half_4x4.tif` | the raw half bits delivered as uint16 | widened to float32 | |

## Two OpenImageIO behaviours the reader neutralises (not differences)

Found while implementing the decode path; neither is a difference-list entry, because both are handled inside `orimageio::Reader`
and `COiioImageInput`, so the two backends agree.

- **A truncated TIFF reads as a success.** OIIO's TIFF reader returns `true` from `read_image` for a file whose strips run past the
  end of the file, recording `TIFFReadEncodedStrip failed ... Read error` and leaving the rest of the buffer unwritten (a truncated
  512x512 RGB8 file: the legacy decoder refuses at scanline 255; OIIO returned `true` with about half of the 786,432 bytes never
  written). `Reader::read` treats an error OIIO recorded (`has_error()`) as a failure, reports it through `error()` naming the file,
  and returns `false`. The `seek_subimage(1)` probe that `open` uses to count subimages records its own "could not seek to subimage 1"
  error on every single-subimage file, so `open` clears it before any read.
- **OIIO names its 32-bit integer types `uint` and `int`.** `SourceTraits::storedType` carries OIIO's name; the refusal message of
  `COiioImageInput` says `uint32` and `int32`, the width the TIFF itself names, so the error for a 32-bit unsigned TIFF
  (`D-TIFF-UINT32`) reads "Unsupported sample type uint32 in <file>".

## Colour (P-Q10)

Decoding never converts a colour: no `ColorConfig` conversion call exists anywhere in the library, and the samples are the
bytes (or half and float values) the file holds. OpenColorIO may still be loaded by OIIO itself, for metadata, for instance
on a PNG with a `cICP` chunk. `ImageIO_NoColourConversion` decodes a `cICP` PNG, a PNG, a TIFF and an EXR with `OCIO`
unset and with `OCIO` pointing at a different configuration, and requires identical samples, equal to the legacy decode.

## Baking through OpenImageIO (FR-015 to FR-017, Phase 6)

`Option "imageio" "backend" "oiio"` (or a per-call `"imageio" "oiio"`) also selects the **writer** of the five `make*` entry points
(`makeTexture`, `makeSideEnvironment`, `makeCubicEnvironment`, `makeCylindericalEnvironment`, `makeSphericalEnvironment`), by the same
precedence as the decoder. What is written is the pyramid our own code computes (`adjustSize`, `filterScaleImage`, `appendPyramid`);
only the storage step differs. `texmake.cpp` hands each layer to a file-local `CBakeSink`: `CLegacyBakeSink` holds the libtiff code
that was always there (moved, not changed: `ImageInput_TiffBakeByteIdentical` and `legacy_bake_hashes --check` pin it byte for byte),
`COiioBakeSink` calls `orimageio::TiledTiffWriter` (`src/imageio/tiledTiffWriter.cpp`). Reading baked textures is unchanged.

- **The per-directory tag rule.** The tags set before `appendLayer()` belong to that directory only and are then cleared, which is
  libtiff's `TIFFWriteDirectory` behaviour. So `PIXAR_TEXTUREFORMAT` (and `PIXAR_WRAPMODES`, and the two shadow matrices) sit on
  directory 0, and the full size sits on the first directory of each pyramid (the six cube faces each have one). The OIIO sink keeps
  the same model: pending values, written with the next directory, then cleared. The first directory is opened with `Create`, each
  later one with `AppendSubimage` (never a MIP level, so no `SUBFILETYPE`).
- **The full-size rule.** OIIO's TIFF writer writes `PIXAR_IMAGEFULLWIDTH/LENGTH` as `full + offset`, and only when the display window
  differs from the data window or has an offset. A bake carries them even when they equal the data size (they are the valid size), so
  the writer states the window as `full_x = 1, full_width = F - 1` on x, and the same on y when the valid length is at least 2; a valid
  length of 1 stays plain on y (a 1x1 image writes `full_x = 1, full_width = 0, full_height = 1`: written 1, 1). The parity test
  compares these tags directory by directory, so a later OIIO that changed the rule fails loudly.
- **What else the writer states.** LZW with `tiff:Predictor` 1, orientation top-left, resolution unit none with X and Y resolution 1,
  tiles of the bake's tile size, unassociated alpha for a 2 or 4 channel 8-bit or float directory, no extra-samples tag for a 16-bit
  one (`tiff:write_extrasamples` 0), and a `Software` tag (`OpenImageIO <version>`), because OIIO writes that tag only when given one.
  `SOFTWARE` and `DATETIME` are the only tags an OIIO bake carries that the legacy one does not; no Pixar tag is ever extra.
- **The exclusion table (FR-017).** OIIO derives `PHOTOMETRIC` and `EXTRASAMPLES` from the channel count; the legacy writer sets them
  by its own rule, so OIIO writes a (precision, channels) combination only where the two agree. One rule, every kind, one table
  (`kOiioBakeExclusions` in `texmake.cpp`, checked once per `make*` call before any output file exists):

  | Precision | Channels | Writer | Why |
  |---|---|---|---|
  | 16-bit | 3 or more | legacy | OIIO writes `PHOTOMETRIC_RGB`; the texture reader reads that on a 16-bit texture as the half-range pixar encoding and scales it (GitHub #18) |
  | 8-bit, float | 5 or more | legacy | OIIO writes `PHOTOMETRIC_RGB`; the legacy writer leaves it unset |
  | 16-bit | 1 or 2 | OIIO | equal to legacy (`MINISBLACK`, no extra samples) |
  | 8-bit, float | 1 to 4 | OIIO | equal to legacy |

  An excluded bake goes to the legacy writer and reports once, as an information message (`CODE_SYSTEM` severity info: the RI error
  handler never sees `CODE_LOG`): `"<output>: 16-bit <n>-channel textures are baked by the legacy writer (...)"` or
  `"<output>: <n>-channel textures are baked by the legacy writer (...)"`. No OIIO-written file of an excluded kind is produced. A cube
  environment chooses the writer once for its six faces: the faces' headers are read first, and any excluded face sends the whole
  bake to the legacy writer.
- **The disk cache of spec 020.** `Option "texturecache"` bakes through `makeTexture` with `"imageio" "oiio"` when the backend is
  OIIO, so the cache entry is written by the OIIO writer (`<hash>-<mtime>.oiio.tex`) and by the legacy writer otherwise.
- **Tests.** `ImageIO_BakeParity` (28 allowed cases: every tag of every directory and every in-image sample equal to the legacy bake of
  the same platform), `ImageIO_BakeExclusions` (the 10 excluded cases: message once, before the output exists, output equal to the
  legacy bake; and the 28 allowed ones: written by OIIO), `ImageIO_BakeRender` (legacy-baked against OIIO-baked texture renders, one
  scene per kind, byte-identical at threshold 20). The legacy bake hashes are per platform (`legacy-bake-macos.sha256`,
  `legacy-bake-linux.sha256`), each checked against its own file.
