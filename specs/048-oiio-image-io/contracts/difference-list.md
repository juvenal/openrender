# Contract: the decode difference list (FR-012, SC-009)

Under policy P-Q5 (the OIIO path refuses what legacy refuses), the only differences are files **legacy accepts**
whose results differ. The list lives in one data file, `tests/unit/imageio/differences.txt`, rendered into
`DEVNOTES_DETAILS/IMAGE_IO.md`. `ImageIO_DecodeDifferences` runs one check per entry. `ImageIO_DifferenceListDoc`
fails if the data file and the doc name different IDs. A difference the parity tests meet that is not listed is a
failure.

Initial entries, predicted from the 3.1.14 source (research R5 to R7). Each is confirmed or corrected by its test at
implementation; an entry is removed only if its test shows no difference.

| ID | Fixture (tiny, generated) | Legacy | OIIO | Test asserts |
|---|---|---|---|---|
| D-RGBE-EXP | 3-pixel flat RGBE, exponent bytes 40, 223, 226 | exact `ldexp` values | 1 float ulp lower (table literals) | OIIO's exact floats, from research R5's table; all other exponents identical |
| D-RGBE-RLE | the same 16x4 pixels written twice, flat and RLE (no exponent byte 40/223/226) | flat: correct; RLE: `RGBE_ReadPixels` cannot read RLE, so it fails or misdecodes | both correct | OIIO's decode of the RLE file equals legacy's decode of the flat file, bit for bit; the legacy RLE outcome recorded as observed; legacy defect issue GitHub #171 (P-Q9) |
| D-PNG-TRNS | 4x4 RGB8 PNG with a tRNS chunk | 3 channels | 4 channels (`png_set_expand` adds alpha) | channel counts; RGB samples equal; alpha 0 where tRNS matches, else 255 |
| D-TIFF-SEPARATE | 4x4 RGB8 TIFF, PLANARCONFIG_SEPARATE | reads plane 0 into an interleaved buffer (wrong) | interleaved correctly | OIIO equals the formula |
| D-TIFF-PALETTE | 4x4 8-bit palette TIFF | 1 channel, raw indices | 3 channels, palette expanded | both outcomes, exact |
| D-TIFF-MINISWHITE | 4x4 8-bit MINISWHITE TIFF | raw values | inverted (to be confirmed with `oiio:RawColor=1`) | exact values on each path |
| D-TIFF-UINT32 | 4x4 1-channel 32-bit unsigned TIFF | read as float bits (`bp == 32` -> float) | refused: no `CImageInfo` tier | legacy info; OIIO error names file and type |
| D-TIFF-INT16 | 4x4 1-channel signed 16-bit TIFF | raw bits as uint16 | refused (INT16) | as above |
| D-TIFF-HALF | 4x4 1-channel 16-bit IEEE float TIFF | raw bits as uint16 | widened to float32 | OIIO values exact; legacy info `16/false` |

`D-TIFF-SEPARATE`, `D-TIFF-INT16` and `D-TIFF-UINT32` are **legacy defects, not fixed here** (legacy reads plane 0 into an
interleaved buffer, returns raw signed bits as unsigned, reads 32-bit unsigned bits as float). They are listed, pinned and
left alone; the coordinator may file them as issues later (no issue number is cited until then).

Refusal parity (not differences; asserted by `ImageIO_DecodeErrors`):
- **Refused identically on both paths**: indexed, interlaced and sub-8-bit PNG; TIFF bits per sample not 8/16/32;
  tiled TIFF (legacy fails at read, OIIO at open; both name the file); multi-part EXR; EXR with unsupported
  channels; deep EXR.
- **Content and suffix disagree** (a PNG named `.tif`): both fail and name the file.
- **Missing, truncated or corrupted files**: both fail and name the file; no partial image.
- **JPEG, TGA, BMP and WebP** (FR-023): refused by the suffix table on both paths, and by the `format_name()` check
  when renamed to a known suffix.
