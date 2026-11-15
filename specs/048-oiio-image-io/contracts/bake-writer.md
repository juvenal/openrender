# Contract: baking through OIIO (FR-015 to FR-017)

## Scope

- `makeTexture`, `makeSideEnvironment` (shadow), `makeCubicEnvironment`, `makeCylindericalEnvironment` (`-envlatl`)
  and `makeSphericalEnvironment` (no caller today; tested through the C++ entry point) all write through the bake
  sink.
- What the sink writes is the pyramid our code computes (`adjustSize`, `filterScaleImage`, `appendPyramid`). Only
  the storage step differs between the backends.
- Reading baked textures is unchanged (FR-018).

## Per-directory tag set (compared tag by tag, directory by directory, by `ImageIO_BakeParity`)

| Tag | Required on the OIIO bake |
|---|---|
| `PIXAR_TEXTUREFORMAT` | present exactly where legacy writes it (directory 0), same string: `openRender Texture`, `openRender shadow`, `openRender Environment (cubic)`, `openRender Environment (spherical)`, `openRender Environment (cylinder)` |
| `PIXAR_WRAPMODES` | same directories, same string (directory 0 of plain textures and cylinder environments; absent on shadow and cubic bakes, as legacy) |
| `PIXAR_MATRIX_WORLDTOCAMERA`, `PIXAR_MATRIX_WORLDTOSCREEN` | shadow: directory 0, same 16 floats (from `readMatrix`, or identity plus the existing error when the source has none) |
| `PIXAR_IMAGEFULLWIDTH`, `PIXAR_IMAGEFULLLENGTH` | the first directory of each pyramid, same values, **including when equal to the data size** (`full_x`/`full_y` rule; P-Q3a) |
| `IMAGEWIDTH`, `IMAGELENGTH`, `TILEWIDTH`, `TILELENGTH` | equal |
| `SAMPLESPERPIXEL`, `BITSPERSAMPLE`, `SAMPLEFORMAT` | equal |
| `PHOTOMETRIC`, `EXTRASAMPLES` | equal (combinations where OIIO cannot match are excluded, below) |
| `PLANARCONFIG`, `ORIENTATION`, `RESOLUTIONUNIT`, `XRESOLUTION`, `YRESOLUTION` | equal |
| `COMPRESSION` | LZW; `PREDICTOR` absent or 1 on both |
| directory count | equal |
| other tags (`SOFTWARE`, `DATETIME`, …) | allowed only on the OIIO bake, listed in the test's allow set; never a Pixar tag |

Samples: for every directory, every in-image sample decoded with libtiff (`TIFFReadTile`, padding excluded) is equal.

## Exclusions (FR-017)

One rule, applied to every kind: OIIO writes a (bits, channels) combination only where its `PHOTOMETRIC` and
`EXTRASAMPLES` equal legacy's. The full table is in [data-model.md](../data-model.md) §4.

| Kind | Precision | Channels | Writer used when `oiio` is selected | Message (`CODE_INFO`) |
|---|---|---|---|---|
| every kind | 16-bit | 3 or more | legacy | `"%s: 16-bit %d-channel textures are baked by the legacy writer (OpenImageIO writes PHOTOMETRIC_RGB, which the texture reader scales; GitHub #18)"` |
| every kind | 8-bit, float | 5 or more | legacy | `"%s: %d-channel textures are baked by the legacy writer (OpenImageIO writes PHOTOMETRIC_RGB; legacy leaves it unset)"` |

- The gate is checked once per `make*` call, before the output file exists. No OIIO-written file of an excluded kind
  is ever produced (US3-4).
- The exclusion is documented in `DEVNOTES_DETAILS/IMAGE_IO.md` and `man/otexmake.1`.
- If P-Q3 is answered (c) instead, a second row "valid size equal to data size" is added here.

## Render equivalence (US3-3)

`ImageIO_BakeRender` renders one scene per kind (plain, cubic, cylinder environments and a shadow map; spherical is
not reachable from RIB) with the legacy-baked texture and with the OIIO-baked texture. It compares them with the
visual metric at the scene's existing threshold, unchanged. Equal samples and reader-equivalent tags make
byte-identical images the expected result. A non-identical pair is reported even when it passes the threshold.
