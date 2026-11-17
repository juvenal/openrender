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
