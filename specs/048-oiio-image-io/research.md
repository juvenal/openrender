# Research: OIIO Image I/O Backend (spec 048)

Phase 0 of the plan. Every decision below is backed by a source read or a measurement taken on 2026-11-14. Nothing
was built, configured or installed. Line numbers refer to master `278b0394` (openRender) and to OpenImageIO
**3.1.14.0**, the vcpkg port's version at baseline `319504a5`. The OIIO source was read from the tarball already in
the vcpkg download cache (`~/.vcpkg/downloads/AcademySoftwareFoundation-OpenImageIO-v3.1.14.0.tar.gz`), extracted to
a scratch directory and only read.

## R1. Which shared library holds OIIO

**Decision**: a new shared library, `libRIBImageIO` (target `openrenderimageio`, `src/imageio/`), built only when
OIIO is enabled. It contains OIIO's code, or with a shared system OIIO it is the only image that links it. It
publishes an OIIO-free C++ interface and depends only on `libRIBCommon`. `libRIBRender` links it, and through it
`otexmake` (`src/otexmake/CMakeLists.txt` links `ribRender`); so does the `oiio` display module. `libRIBVector` does
not.

**The shared-source constraint**: `texture/imageInput.cpp` and `texture/texmake.cpp` are compiled into both
`ribRender_obj` and `ribVector_obj` (`src/ri/CMakeLists.txt:385-396`, `ribVector_sources = ri_sources` minus a list
that keeps both files). The OIIO adapter is selected inside `createImageInput()`, so the question is how
`libRIBVector` avoids an OIIO dependency. The answer already exists for OpenEXR: `HAVE_OPENEXR` is defined only on
`ribRender_obj` (`:230-233`), so `imageInput.cpp` compiles its EXR branch into `libRIBRender` alone, and
`imageInputExr.cpp` is an empty translation unit in `libRIBVector`. `HAVE_OIIO` does the same:
`imageInputOiio.cpp` is empty and `createImageInput(…, OIIO)` reports "not built" in `libRIBVector`, which never
bakes or decodes at runtime (the comment at `:385-396` records that `ribpreview_load()` never reaches `makeTexture`).

**Static packages**: `libRIBImageIO` has no static archive. `ribRender_static` (INTERFACE) and, if ever needed,
`ribVector_static` link the shared library, the way `libshader_shading` is shared-only (gotcha 22). A static
consumer then carries no OIIO code (FR-004).

**One image, enforced**: with vcpkg's static triplet (`VCPKG_LIBRARY_LINKAGE static`, `CMake/vcpkg-triplets/*.cmake`)
OIIO, OCIO and their codecs are static archives folded into `libRIBImageIO.dylib`. Their symbols would be exported by
default, so the library links with an exported-symbols list (`LINKER:-exported_symbols_list`, `__ZN9orimageio*` and
its typeinfo) on macOS and `-Wl,--exclude-libs,ALL` plus `-fvisibility=hidden` on Linux. Its export table then holds
only `orimageio::`.

The codec libraries (libtiff, libpng, OpenEXR) are then present twice in a vcpkg process: once inside
`libRIBImageIO`, hidden, and once in `libRIBRender`, as today. Each copy keeps its own global error handler and
thread pool, and no codec object crosses between the copies (the interface passes plain buffers). This is the state
the display modules are already in: `file.dsply` is 1.46 MB because it carries its own static libtiff and libpng,
measured in the main build. FR-004 concerns OIIO's code, which exists once. With Homebrew or a Linux prefix OIIO,
everything is shared and single.

**Alternatives rejected**:
- **`libRIBCommon`**: every program links it (`oshader`, `rsloinfo`, `libshader_shading`, the display modules). OIIO
  plus OCIO would load into all of them, and Infra must stay a dependency-free floor.
- **`libRIBRender`**: the display module cannot link it (Display may not depend on Core), and `ribVector`,
  `otexmake` and the static package would face the same duplication question.
- **The display module itself**: a dlopen'ed module that holds OIIO would be a second copy beside the renderer's
  (the spec's edge case).
- **Classifying `src/imageio/` as Infra** instead of a new class: no archcheck code change, but every domain,
  Shading included, could then include the wrapper. D30 part 4 places it beside `libRIBCommon` *because* Texturing
  and Display Outputs may not depend on each other: the point is a narrow, named permission. A new class keeps it
  narrow, and 049 can widen it (Shading, for OSL) by amendment.

## R2. The constitution amendment (D30 part 4)

**Decision**: constitution 1.4.0 -> **1.5.0** (MINOR: a new section, nothing removed or redefined). The full text is
in [contracts/constitution-1.5.0-draft.md](contracts/constitution-1.5.0-draft.md). archcheck gains `Class::ImageIO`
as a layered class. Precedent: spec 030's `Transport` (commit `e0a7e4e3`) changed `config.h` (enum,
`kClassCount` 13 -> 14 here), `config.cpp` (`kClassNames`), the `main.cpp` help text, and added two fixtures.

`tests/architecture/unit/test_archcheck_parsers.cpp` loops over `kClassCount` (lines 196, 227, 239) and lists no
class set, so **no approved test changes**. New fixtures: `imageio-allowed` (Texturing -> ImageIO -> Infra passes)
and `imageio-reverse` (ImageIO -> Texturing is a VIOLATION, exit 1).

**Alternatives**: amend only the archcheck data and leave the constitution silent. Rejected, because FR-006 requires
the text and `domain-rules.txt` says "change only by constitution amendment".

## R3. Selection spellings

**Decision** (full contract: [contracts/selection.md](contracts/selection.md)):

| Surface | Spelling | Default |
|---|---|---|
| Scene (decode of unbaked sources, the spec 020 disk cache, the `Make*` calls) | `Option "imageio" "backend" ["oiio"]` (declared `uniform string`) | `"legacy"` |
| One bake call | `MakeTexture … "string imageio" ["oiio"]` (likewise `MakeShadow`, `MakeLatLongEnvironment`, `MakeCubeFaceEnvironment`) | the Option |
| `otexmake` | `-imageio legacy\|oiio` | `legacy` |
| Display | driver name `"oiio"` (loads `oiio.dsply`), format from `"string type"` or the extension | n/a |
| Build | `-DOPENRENDER_ENABLE_OIIO=ON` | OFF |
| Tests (C++) | `createImageInput(name, EImageBackend::OIIO)`, the writer chosen by the `imageio` parameter | `Legacy` |

**Rationale**:
- `Option "<class>" "<token>"` mirrors `Option "texturecache" "enable"`
  (`specs/020-runtime-tile-paging/contracts/texturecache-option.md`), whose tokens are declared so RIB writes no
  inline type.
- `otexmake`'s flags are single-dash words followed by a value (`-resize up`, `-filter box`;
  `src/otexmake/otexmake.cpp:41-62`).
- `texmake.cpp`'s `make*` functions already take the `Make*` parameter list and parse `"resize"` from it (`:366`), so
  `"imageio"` reaches them with no signature change. `otexmake`, the disk cache (`texture.cpp:2854`) and the core all
  use that channel.
- The driver name follows the convention that the name is the module (`file`, `openexr`, `rgbe`); no alias table
  entry is needed (`rendererDisplay.cpp:742` maps only `tiff`/`png`/`shadow`/`zfile` to `file`).
- No environment variable selects anything (FR-013).

The one backend value governs decode **and** writing. Tests that must force one side and fix the other (FR-013,
"force either path") call the C++ entry points: a writer-parity test decodes both outputs with one fixed reader, and
a decode-parity test never writes.

## R4. OIIO version detection

**Decision**: `find_package(OpenImageIO CONFIG QUIET)` with **no version**, then
`OpenImageIO_VERSION VERSION_GREATER_EQUAL 3.0` by hand.

OIIO's package version file uses `COMPATIBILITY SameMajorVersion` (`OpenImageIO-3.1.14.0/CMakeLists.txt:354-357`;
also in Homebrew's installed `OpenImageIOConfigVersion.cmake`). `find_package(OpenImageIO 3.0)` would therefore reject
a future 4.x, the same trap CLAUDE.md records for LLVM. There is no ceiling.

## R5. Float parity and the tolerance question

**Decision**: no tolerance anywhere. One listed, pinned difference (D-RGBE-EXP).

**Evidence**, by path:
- **RGBE decode.** Legacy computes `(float)ldexp(1.0, e - 136)` and multiplies by the mantissa byte
  (`src/common/rgbe.cpp:115-126`). OIIO uses the same formula through a 256-entry table of decimal literals
  (`hdr.imageio/hdrinput.cpp:119-188`, used at `:192-201`). Each literal was parsed as float32 and compared with the
  exact `ldexp(1.0, i-136)`: **3 of 256 entries differ by one ulp**.

  | Exponent byte | OIIO literal | as float32 | exact power of two |
  |---|---|---|---|
  | 40 | `1.2621774e-29f` | 1.2621773731e-29 | 1.2621774484e-29 |
  | 223 | `1.547425e+26f` | 1.5474249569e+26 | 1.5474250491e+26 |
  | 226 | `1.23794e+27f` | 1.2379399655e+27 | 1.2379400393e+27 |

  Those exponents encode about 1e-29, 1e26 and 1e27, far outside any texture's range. Every other exponent gives
  bit-identical floats (same multiply, `uchar * float`). The difference is listed with a test that builds a 3-pixel
  flat RGBE file with those exponents and asserts OIIO's exact values. The parity fixtures use ordinary exponents and
  require bit identity.
- **RGBE encode (display).** OIIO: `frexpf(v,&e) * 256.0f / v` in float (`hdroutput.cpp:62-80`). Legacy:
  `(float)(frexp(v,&e) * 256.0 / v)` in double, then rounded (`rgbe.cpp:90-109`). `frexp` is exact, the multiply by
  256 is exact, and one division of two floats computed in double (53-bit) and rounded to float is the correctly
  rounded float quotient (53 >= 2·24 + 2). So the scale is identical, and so are the bytes `(uchar)(c * v)`. The
  display-parity test confirms it.
- **EXR half (display).** Legacy converts each sample with Imath `half(float)`, round-to-nearest-even
  (`openexr.cpp:94-98`). OIIO converts with `vcvt_f16_f32` on ARM or `_mm_cvtps_ph(…, _MM_FROUND_TO_NEAREST_INT)`
  with F16C (`simd.h:6942-6946`), or Imath otherwise: IEEE round-to-nearest-even in every case. NaN payloads may
  differ. The test compares NaN by class only, and the scene produces none. The fixture includes exact ties,
  half denormals, 65504/65520 and overflow to infinity.
- **EXR decode.** half to float widening is exact. FLOAT is copied.
- **Integer paths.** Quantization is shared code (`openrenderfilebase`), and the codecs are lossless. The bake writes
  samples our code computed (`adjustSize`/`appendPyramid`), so the writers only store them.
- **Out of parity scope**: `compression "jpeg"` (lossy, codec-dependent), and `dither` (random noise, so not
  repeatable between two renders).

## R6. OIIO behaviours the adapter must neutralise

Read from the 3.1.14 source:
- **Alpha association.** The PNG reader premultiplies unless `oiio:UnassociatedAlpha=1` (`pnginput.cpp:211`,
  `:349-360`). The PNG writer de-associates unless the spec says `oiio:UnassociatedAlpha=1` (`pngoutput.cpp:240-241`,
  `:348`). TIFF handles it likewise (`tiffinput.cpp:903`, `:1342-1385`). The reader always passes the config, and
  the writers always set the attribute: the legacy paths store and return samples as given.
- **Colour.** TIFF CMYK and YCbCr are converted unless `oiio:RawColor=1` (`tiffinput.cpp:905`), which is always
  passed. No `ColorConfig` conversion API is ever called. OCIO is still *loaded* for metadata by the PNG reader on a
  cICP chunk (`png_pvt.h:332`) and by the PNG writer always (`png_pvt.h:629`). See P-Q10.
- **Format probing.** The static `ImageInput::open(filename)` falls back to probing other formats when the
  extension's reader fails. The reader uses `ImageInput::create(<format name>)` with the suffix's format and opens
  that instance, so content and suffix must agree, as they must on the legacy path. After opening it checks
  `format_name()` against the expected `tiff`/`png`/`hdr`/`openexr` (FR-023).
- **Threads.** `threads` defaults to the hardware concurrency, and so does `exr_threads` (`imageio.h:3723-3745`).
  The library sets `threads=1` once (`std::call_once`) so OIIO starts no worker pool inside the renderer, and sets
  the EXR pool to the value that keeps decoding in the calling thread, confirmed against the 3.1 documentation at
  implementation. It also sets an empty `plugin_searchpath`, so only the embedded readers exist
  (`EMBEDPLUGINS` ON, `CMakeLists.txt:121`).
- **Errors.** Every OIIO call is inside `try`/`catch (...)`. `geterror()` of the instance (or `OIIO::geterror()`) is
  turned into `error(CODE_…, "… \"%s\" …")` naming the file and the reason (FR-011). No exception leaves the library.

## R7. Decode acceptance policy

**Decision** (P-Q5): the OIIO path **refuses everything the legacy decoders refuse, with the same checks**. The
difference list then holds only cases where legacy accepts a file and its result differs from OIIO's. The library
reports facts (`orimageio::SourceTraits`, data-model §3); the Texturing adapter applies legacy's rules, so the policy
lives with `CImageInput`'s contract.

| Legacy rule (file:line) | OIIO path check |
|---|---|
| PNG interlaced -> refuse (`imageInputPng.cpp:~90`) | IHDR byte 28 != 0 -> refuse |
| PNG palette -> refuse | IHDR colour type == 3 -> refuse |
| PNG bit depth not 8/16 -> refuse | IHDR bit depth not 8/16 -> refuse |
| TIFF bits per sample not 8/16/32 -> refuse (`imageInputTiff.cpp:~70`) | `tiff:BitsPerSample` / `oiio:BitsPerSample` not 8/16/32 -> refuse |
| TIFF tiled -> fails at `TIFFReadScanline` | `tile_width > 0` -> refuse at open |
| EXR multi-part -> refuse (`imageInputExr.cpp:~50`) | a second subimage exists -> refuse |
| EXR layout not RGB/RGBA/1-channel -> refuse | same names and count check |
| EXR deep | `spec.deep` -> refuse |
| Unknown suffix -> `nullptr` (`imageInput.cpp:59-79`) | same table, same `nullptr` |
| `.exr` without `HAVE_OPENEXR` -> error | same: the OIIO format set is the legacy set of the same build |

PNG's colour type, bit depth and interlace are read from the 13-byte IHDR at file offsets 24 to 28 (a 33-byte read,
no libpng), because OIIO expands palettes and low bit depths (`png_set_expand`, `png_pvt.h:201`) and keeps no
attribute that says it did.

Cases where legacy accepts and the results differ become difference-list entries
([contracts/difference-list.md](contracts/difference-list.md)): RLE RGBE, the RGBE exponent table, PNG with tRNS
(`png_set_expand` adds an alpha channel), TIFF separate planar, palette TIFF, MINISWHITE, 32-bit unsigned TIFF
(legacy reads the bits as float, OIIO refuses: no `CImageInfo` tier), signed-integer TIFF, and 16-bit float TIFF.
Each has a tiny fixture.

**Alternative**: accept everything OIIO decodes correctly (interlaced and palette PNG, tiled TIFF) and list each
widening. Closer to 049, where OIIO's behaviour becomes the behaviour, but 048 is a parity spec.

## R8. Baking: what OIIO 3.1.14 can and cannot write

Legacy writer (`texmake.cpp:85-195`, `:331-360`, `:420-431`, `:514-525`, `:578`, `:683-684`, `:749-750`), per
directory:

| Tag | Legacy | OIIO 3.1.14 | Verdict |
|---|---|---|---|
| `PIXAR_TEXTUREFORMAT` incl. `openRender Environment (cubic/spherical/cylinder)` | dir 0 | attribute `textureformat`, any string (`tiffoutput.cpp:1056`) | **preserved** |
| `PIXAR_WRAPMODES` | dir 0 | attribute `wrapmodes` (`:1061`) | **preserved** |
| `PIXAR_MATRIX_WORLDTOCAMERA/SCREEN` (shadow) | dir 0 | attributes `worldtocamera`/`worldtoscreen`, `TypeMatrix` (`:1066-1072`) | **preserved** |
| `PIXAR_IMAGEFULLWIDTH/LENGTH` | first dir of each pyramid, always | written only if full size != data size, as `full_width + full_x` (`:565-572`) | **preserved via `full_x`/`full_y`** (P-Q3a) |
| `TILEWIDTH/LENGTH` 32 | every dir | `tile_width`, must be a multiple of 16 (`:480-484`) | preserved |
| `SAMPLEFORMAT`, `BITSPERSAMPLE`, `SAMPLESPERPIXEL` | every dir | from the spec format | preserved |
| `COMPRESSION` LZW, no predictor | every dir | `compression "lzw"`; OIIO adds a predictor (`:678-692`) unless `tiff:Predictor` is set (`:1018`) | set `tiff:Predictor` = 1 (none) |
| `PHOTOMETRIC` | MINISBLACK for 1/2 ch and for **all 16-bit**; RGB for 8-bit/float 3/4 ch | MINISBLACK for < 3 ch, **RGB for >= 3, no override** (`:657`) | **16-bit 3/4 ch cannot match**, and the reader doubles them (`texture.cpp:2476-2479`) -> excluded (P-Q4) |
| `EXTRASAMPLES` | UNASSALPHA for 2/4 ch at 8-bit/float; none for 16-bit; none for >= 5 ch (PHOTOMETRIC unset there) | from `alpha_channel`, `oiio:UnassociatedAlpha`, and off with `tiff:write_extrasamples` 0 (`:815-833`) | preserved for 1-4 ch at 8-bit/float and 1-2 ch at 16-bit; **>= 5 ch excluded** (OIIO forces RGB) |
| `ORIENTATION` TOPLEFT, `PLANARCONFIG` CONTIG | every dir | `Orientation` 1 (`:585`), contiguous | preserved |
| `RESOLUTIONUNIT` NONE, `X/YRESOLUTION` 1.0 | every dir | `ResolutionUnit "none"`, `XResolution`/`YResolution` 1 (`:895-910`) | preserved |
| `SUBFILETYPE` | absent | written only from an attribute (`:1053`) | absent: every directory is appended with `AppendSubimage`, never as a MIP level |

**Measured placement** (a read-only IFD walk of the approved references
`tests/unit/image_input/fixtures/references/{texture,shadow,envcube-cubic,envlatl-cylindrical}.tex`, 64x64 RGB8
source, 2026-11-14):
- `TEXTUREFORMAT` sits on directory 0 only.
- `WRAPMODES` sits on directory 0 of `texture` (`periodic,periodic`) and `envlatl` (`periodic,clamp`) only; it is
  absent on `shadow` and `envcube`.
- Both matrices sit on directory 0 of `shadow` only.
- `FULLWIDTH`/`FULLLENGTH` = 64/64, equal to the data size, on the first directory of every pyramid: 0 for the
  single pyramids, and 0, 6, 12, 18, 24 and 30 for the six cube faces.
- In every directory: `COMPRESSION` 5 (LZW), no `PREDICTOR`, `PHOTOMETRIC` 2 (RGB), no `EXTRASAMPLES`,
  `RESOLUTIONUNIT` 1, `ORIENTATION` 1, tile 32, no `SUBFILETYPE`.
- Directory counts: 6, 6, 36 and 6.

This confirms the per-directory pending-tag model and that the full-size tags are written when equal (P-Q3).

`full_x` is safe for this use: TIFF cannot represent a full-size offset, so OIIO folds it into the full-size tags
(`:562-564`, a documented rule). `XPOSITION`/`YPOSITION` come from the data origin `spec.x/y`, not from `full_x`
(`:912-924`). For a 1-pixel axis, the offset goes on the other axis (a 1x1 image writes `full_x=1, full_width=0,
full_height=1`, which meets the condition and writes 1, 1). The bake parity test compares every tag of every
directory, so a later OIIO that changes this rule fails loudly.

`makeSphericalEnvironment` has no caller (`test_tiff_bake_byte_identical.sh` header), so the parity test calls it
through the C++ entry point.

## R9. Display driver

Reuse `openrenderfilebase` (`src/display/file/CMakeLists.txt`), which owns quantize, dither and gamma
(`file_base.cpp:33-41`). The legacy drivers differ only in the codec step, so parity reduces to the codecs.

Legacy specifics the driver mirrors:
- channel names `R,G,B,A,Z` by position (`openexr.cpp:72-76`);
- EXR channels HALF;
- the EXR `compression` names RLE/ZIPS/ZIP/PIZ/PXR24 (`:54-66`), mapped to OIIO's lowercase names;
- the TIFF `compression` names lzw/jpeg/deflate/none (`file_tiff.cpp:89-106`);
- the TIFF Pixar matrices from `NP`/`Nl` (`:41-44`, `:86-87`), read by `blobbyRepeller.cpp:164`;
- `Software`;
- the PNG writer told `oiio:UnassociatedAlpha=1`.

OIIO's HDR writer takes at most 3 channels and refuses 1 or 2 (`hdroutput.cpp:207`), and always writes RLE when the
width is 8 to 32767 (`:150-153`). Its output therefore cannot be read back by the legacy RGBE decoder (P-Q9). The
display-parity test decodes each pair with one decoder that is neither writer: libtiff, libpng or OpenEXR in test
code for TIFF, PNG and EXR, and the OIIO reader only for RGBE.

## R10. vcpkg feature set and the shared install tree

**Decision**:

```json
"features": {
  "oiio": {
    "description": "OpenImageIO 3 image backend (spec 048); pixel I/O only, no colour conversion",
    "dependencies": [ { "name": "openimageio", "default-features": false } ]
  }
}
```

- **Port contents** (`~/.vcpkg/ports/openimageio/vcpkg.json`, 3.1.14 port-version 1): the base dependencies are
  fmt, libjpeg-turbo, libpng, **opencolorio**, openexr, robin-map, tiff (no defaults) and zlib. OCIO is also
  `REQUIRED` in OIIO's own build (`externalpackages.cmake:141`, `VERSION_MIN 2.3`). The port's `opencolorio`
  *feature* only toggles OCIO use; the package comes regardless. No other feature is requested, so there are no
  `tools`, ffmpeg, gif, webp, jxl, libraw, openjpeg or freetype.
- **Measured closure** (a read of the port manifests): the current manifest resolves 11 ports (imath, libdeflate,
  libjpeg-turbo, liblzma, libpng, libspng, llvm, openexr, openjph, tiff, zlib). `openimageio[core]` adds **9**:
  expat, fmt, minizip-ng, opencolorio, openimageio, pystring, robin-map, sse2neon, yaml-cpp. It changes no feature of
  an existing port (tiff, openexr, libpng and zlib are already requested with their defaults), so no existing
  package's ABI hash changes. This is why LLVM's archive stays valid (R11).
- **The shared-tree hazard is permanent, not just this worktree's.** vcpkg's manifest mode installs exactly the
  closure of the requested features and removes the rest. The vcpkg binary here (2026-07-27) carries the
  `PackagesToRemove` message "The following packages will be removed:". An OIIO-on configure and an OIIO-off
  configure sharing one `VCPKG_INSTALLED_DIR` would install and remove OIIO's 9 ports back and forth. So before
  `project()`, with the feature on, the root `CMakeLists.txt`:
  - appends `oiio` to `VCPKG_MANIFEST_FEATURES`;
  - derives `VCPKG_INSTALLED_DIR` = `$ENV{VCPKG_INSTALLED_DIR}-oiio` (or `<source>/vcpkg_installed-oiio` when the
    environment variable is unset), unless one was given on the command line;
  - `FATAL_ERROR`s if the result equals the OIIO-off directory.

  The binary cache (`~/.cache/vcpkg/archives`, 188 MB now) is shared and additive, so the second tree restores
  LLVM and the codecs from archives instead of building them. Shared tree size now: **176 MB**
  (`~/.cache/vcpkg-installed/openrender`).
- **Earlier measurement for comparison** (§11.3, 2026-10-15, stock static triplet with Debug): 23 ports, 3.8 min,
  57 MB installed. Our triplet is release-only, which roughly halved the build in spec-era measurements. M1
  re-measures on `arm64-osx-openrender`.

## R11. CI cache key (FR-024's "measured at the plan stage")

**Finding** (analysis; the CI run itself is M2): `release.yml:125-131` and `:224-230` key the archive cache on
`vcpkg-<triplet>-${{ hashFiles('vcpkg.json') }}` with `restore-keys: vcpkg-<triplet>-`.
- Adding the feature changes the key, so the first run misses the primary key.
- `restore-keys` then restores the most recent cache with the prefix, which holds the per-package archives.
- vcpkg reuses an archive when the package's **ABI hash** matches. That hash covers the port, its features, the
  triplet, the compiler and the dependencies' ABI, not the top-level manifest (R10: no existing port's features
  change).
- So LLVM is restored, not rebuilt. With the feature unused in CI (P-Q11), nothing new is built at all.
- At the end the job saves a new cache entry under the new key, about another 188 MB counted against the
  repository's cache quota until the old entry ages out.
- Caveat: `actions/cache` restores across refs only from the default branch's caches. A tag run after the merge
  sees master's cache, so this holds.

## R12. Linux

Measured over ssh on the validation host (`ubuntu-2404-desktop`, 2026-11-14): `libopenimageio-dev`
**2.4.17**, `libopencolorio-dev` **2.1.3**, `libopenexr-dev` 3.1.5 (installed), `libimath-dev` 3.1.9 (installed),
`libfmt-dev` 9.1.0, `robin-map-dev` 1.2.1, cmake 3.28.3, GCC 13.3, 2 cores, 9.3 GB free in `/home`.

OIIO 3.1 needs OCIO >= 2.3, so the system OCIO is too old too, not only OIIO. **Decision** (P-Q12):
- Default: absent. With `OPENRENDER_ENABLE_OIIO=OFF` the Linux build is today's build.
- Opt-in: a prefix build of OIIO **3.1.14.0** (pinned to the vcpkg version, so macOS vcpkg and Linux agree;
  Homebrew mode gets 3.2.2, and the parity tests guard against that skew). Build it with
  `-DOpenImageIO_BUILD_MISSING_DEPS=all` (it builds OCIO and its yaml-cpp/pystring/expat/minizip-ng, fmt and
  robin-map, `src/cmake/build_*.cmake`), every optional format off, shared libraries, into
  `~/oiio-048/prefix`. openRender consumes it with `-DOpenImageIO_ROOT=~/oiio-048/prefix`.
- The FHS and release Linux builds do not change (D24 bundles OIIO 3 only when OSL lands).

## R13. macOS Homebrew mode

Homebrew has OIIO **3.2.2** installed on this machine (`brew list --versions`). `libOpenImageIO.3.2.2.dylib` is
7.9 MB, `libOpenImageIO_Util` 0.87 MB and `libOpenColorIO` 6.1 MB. `otool -L` shows it linking ffmpeg, libheif,
libraw, jpeg-xl, webp, freetype, tbb, giflib and pugixml besides the codecs. Hence the OFF default (P-Q13): an
automatic find would pull that closure into every Homebrew build of openRender. With ON, Homebrew mode links the
shared dylib, and libtiff and OpenEXR are then single copies shared with `libRIBRender`.

## R14. Baseline sizes (for SC-007)

From the existing main build (`/Users/…/openrender/build`, vcpkg static triplet; read, not rebuilt):

| File | Size |
|---|---|
| `libRIBRender.1.0.0.dylib` | 9.28 MB |
| `libRIBVector.1.0.0.dylib` | 4.13 MB |
| `libRIBCommon.1.0.0.dylib` | 0.15 MB |
| `file.dsply` (static libtiff + libpng inside) | 1.46 MB |
| `openexr.dsply` (static OpenEXR inside) | 2.77 MB |
| `rgbe.dsply` | 0.04 MB |
| `otexmake` | 0.05 MB |
| `orender` | 0.04 MB |

The cost of OIIO goes to `libRIBImageIO` alone. The earlier comparison point is +8.3 MB per statically linking
binary (§11.3). M4 records the library's size and confirms these files are unchanged when OIIO is OFF, and grow by
no OIIO code when ON.
