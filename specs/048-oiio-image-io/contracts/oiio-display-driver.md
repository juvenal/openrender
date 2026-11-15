# Contract: the `oiio` display driver (FR-019, FR-020, FR-008)

Module `oiio.dsply`, sources `src/display/oiio/oiio.cpp`, class Display. Built only when `HAVE_OIIO`. Installed
into `OPENRENDER_DISPLAYSDIR` beside the legacy drivers.

## ABI

- Display interface 2.0.0: `OR_DISPLAY_DECLARE_VERSION()`, `orDisplayPlugin()` returning an `OrDisplayPlugin`
  (`start`/`data`/`finish`), C linkage, loaded and version-checked by `CDisplayHost` and `checkPluginVersion`
  (spec 037).
- No ABI change, so no version step and no `PLUGIN_ABI.md` version-table change.
- Includes `dsply.h`, `src/display/file/file_base.h` (in-tree helper) and `src/imageio/orImageIO.h` only. No
  renderer-core header (FR-008), no OIIO header.
- Links `openrenderfilebase` and `openrenderimageio`. It carries no OIIO code (`ImageIO_SingleCopy`).
- `start`, `data` and `finish` catch everything: an error returns `OR_STATUS_ERROR` after `error()`.

## Formats and parameters

| Format | Selected by | Sample type written | Parameters honoured (as the legacy driver) |
|---|---|---|---|
| TIFF | `type "tiff"`, `.tif`, `.tiff` | UINT8/UINT16 from `quantize`, else FLOAT | `quantize`, `dither`, `gamma` (base), `compression` lzw/jpeg/deflate/none -> `lzw`/`jpeg`/`zip`/`none`, `NP`/`Nl` -> `worldtoscreen`/`worldtocamera`, `Software` |
| PNG | `type "png"`, `.png` | UINT8, or UINT16 when the quantize maximum exceeds 255 (`file_png.cpp:86`) | `quantize`, `dither`, `gamma`, `Software`; `oiio:UnassociatedAlpha=1` |
| OpenEXR | `type "openexr"`, `.exr` | HALF | `compression` RLE/ZIPS/ZIP/PIZ/PXR24 -> `rle`/`zips`/`zip`/`piz`/`pxr24`, `Software`; channel names R,G,B,A,Z by position |
| RGBE | `type "rgbe"`, `.hdr`, `.pic` | FLOAT -> RGBE (RLE, by OIIO) | R,G,B from a 3- or 4-sample display (alpha dropped, documented); 1, 2 or 5 samples -> error (P-Q8) |

- An unknown or missing format is `error(CODE_BADTOKEN, "oiio display: cannot tell the format of \"%s\"; give a
  known extension or \"string type\"")` and `OR_STATUS_ERROR`. Nothing is written (P-Q7).
- OpenEXR is built only where `HAVE_OPENEXR`, so the format set equals the legacy drivers' set.
- `type "openexr"` in a build without OpenEXR returns an error naming the format.

## Parity (`ImageIO_DisplayParity`, SC-003)

One 16x16 deterministic scene, no dither, no `jpeg` compression. For each case (TIFF 8/16/float, PNG 8/16, EXR
half, RGBE `rgb`), the scene is rendered once with the legacy driver (`file`/`openexr`/`rgbe`) and once with `oiio`.
Both files of a pair are decoded by one fixed decoder that is neither writer: libtiff, libpng or OpenEXR in test
code for TIFF, PNG and EXR. For RGBE the decoder is the OIIO `Reader`: the OIIO driver writes RLE, and the legacy
decoder cannot read RLE (GitHub #171, P-Q9). Every sample is compared exactly, NaN by class. 0 differing samples.

**Display parameters** (FR-019; same fixed decoders, exact comparison): `quantize` 8 and 16, `gamma`, TIFF `compression`
`lzw`/`zip`(`deflate`)/`none`, EXR `compression` `zip`/`piz`, the `NP`/`Nl` Pixar matrices on TIFF (tags read back
equal), and `dither` with the same seed through the shared `file_base` code. If `file_base`'s dither cannot be
seeded, the seedless case is not tested, FR-019 is narrowed to say so, and the maintainer is told (T102).

## Not covered

`framebuffer` (no format), deep or multi-part output, `dither` equality without a fixed seed, `compression "jpeg"` (lossy), and RGBE `rgba` output, which the
legacy `rgbe` driver garbles today (GitHub #172, P-Q9; noted in `DEVNOTES_DETAILS/IMAGE_IO.md`).
