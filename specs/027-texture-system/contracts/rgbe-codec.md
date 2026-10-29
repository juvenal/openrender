# Contract: the RGBE codec in libRIBCommon (FR-017, DB-026)

## Files

| Before | After |
|---|---|
| `src/display/rgbe/rgbe.h` | `src/common/rgbe.h` (moved; `git mv`) |
| `src/display/rgbe/rgbe.cpp` | `src/common/rgbe.cpp` (moved; body unchanged) |
| `src/ri/CMakeLists.txt:67` `../display/rgbe/rgbe.cpp` in `ri_sources` | removed (so `ribRender` and `ribVector` stop compiling it) |
| `src/common/CMakeLists.txt` `openrendercommon_obj` | gains `rgbe.cpp` |
| `src/display/rgbe/CMakeLists.txt` | unchanged: its `file(GLOB *.cpp)` now finds only `file.cpp`; it links the shared `openrendercommon` through `openrenderfilebase` (`PUBLIC`) |
| `src/display/rgbe/file.cpp`, `src/ri/texture/imageInputRgbe.cpp` | include `common/rgbe.h` |

## Interface

`src/common/rgbe.h` keeps every declaration (`rgbe_header_info`, the `RGBE_VALID_*` and `RGBE_RETURN_*`
constants, the six functions) and wraps the functions in `extern "C"`:

```cpp
#ifdef __cplusplus
extern "C" {
#endif
int RGBE_WriteHeader(FILE *fp, int width, int height, rgbe_header_info *info);
int RGBE_ReadHeader(FILE *fp, int *width, int *height, rgbe_header_info *info);
int RGBE_WritePixels(FILE *fp, float *data, int numpixels);
int RGBE_ReadPixels(FILE *fp, float *data, int numpixels);
int RGBE_WritePixels_RLE(FILE *fp, float *data, int scanline_width, int num_scanlines);
int RGBE_ReadPixels_RLE(FILE *fp, float *data, int scanline_width, int num_scanlines);
#ifdef __cplusplus
}
#endif
```

The codec has no state (only `static` helpers and its `stderr`/`perror` error printer), so the same behaviour and
bytes follow from moving the file unchanged. C linkage makes the six symbols spell the same on macOS and glibc
(today `FILE *` mangles as `__sFILE` and `_IO_FILE`), which `Common_StateOwnership` needs (below). The symbols
were never part of a published ABI: the display driver compiled its own copy.

## One definer (gotcha 22)

- `tests/unit/common/owned-symbols.txt` gains a section `# --- Spec 027: the RGBE codec ---` with
  `RGBE_WriteHeader`, `RGBE_ReadHeader`, `RGBE_WritePixels`, `RGBE_ReadPixels`, `RGBE_WritePixels_RLE`,
  `RGBE_ReadPixels_RLE`.
- The image list (`tests/unit/common/CMakeLists.txt:23-29`) does not change: it already scans `ribRender`,
  `ribVector`, `ribOut`, the three `libshader_*` libraries, the `file`, `framebuffer`, `rgbe` and `openexr` display
  modules, the programs and the spec 023 test executables. Before the move this check would fail (three definers:
  `ribRender`, `ribVector`, `rgbe`); after it, exactly one (`libRIBCommon`).

## Tests

- `ImageInput_Rgbe`, `Parity_texture-rgbe`, `Parity_unbaked-rgbe` and the two `DeterminismSweep_parity-*-rgbe-*`
  pairs keep their results.
- `Display_RgbeBytes` (new, `tests/unit/image_input/`, labels `image_input;unit`, a bash script like
  `test_tiff_bake_byte_identical.sh`): renders `rgbe-display.rib`, a small deterministic scene (`-t:1`, box filter,
  no jitter), through `Display "rgbe-display.hdr" "rgbe" "rgb"` and compares the file byte for byte with
  `tests/unit/image_input/fixtures/references/rgbe-display.hdr`, which the **starting tree's** driver writes in
  phase 2. Green on both trees by design (characterization); it is the test US4 AS2 refers to, which does not
  exist today.
- `Common_StateOwnership` with the six names.
- archcheck: the 5 DB-026 lines are deleted at the start of phase 3 (3 `VIOLATION`s on macOS until the move
  lands; the 2 `linux` lines are checked on Linux) and DB-026 moves to Resolved.
