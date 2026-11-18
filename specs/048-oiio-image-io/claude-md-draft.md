# Draft: proposed edits to CLAUDE.md, NEWS.md and ChangeLog.md (spec 048, T125)

**For the maintainer's approval. Nothing in this file has been applied.** `CLAUDE.md`, `NEWS.md` and `ChangeLog.md` are not edited by the implement agent; the coordinator
applies the approved text at merge preparation (T126), exactly as approved here (lines the maintainer strikes are not applied). Figures marked `(Phase 9)` are filled from the
closing validation. Line anchors refer to the current files.

## 1. CLAUDE.md

### 1a. Header paragraph, "Key CMake options" (the sentence listing `USE_FLEX_BISON`, ...)

Add `OPENRENDER_ENABLE_OIIO` (OpenImageIO image backend and the `oiio` display driver, default OFF, needs OpenImageIO 3.0 or newer) to the list.

### 1b. New section after "Plug-in ABIs (spec 037)" (architecture)

```
### Image I/O utility library (spec 048)
Constitution 1.5.0 adds the class **ImageIO** (`src/imageio/`, `libRIBImageIO`): the first *utility library*, a library that depends on Infra only and
that exactly two domains may use, Texturing (the OIIO decode and bake, `imageInputOiio.*`, `texmake.cpp`'s `COiioBakeSink`) and Display Outputs (the `oiio`
display driver, `src/display/oiio/`). It is built only with `-DOPENRENDER_ENABLE_OIIO=ON` (`HAVE_OIIO`, defined on `ribRender_obj` only, never in `config.h`), is
shared only, and is the one image of the install tree that carries OpenImageIO (`ImageIO_SingleCopy`); only `orimageio::` leaves it (`exports.txt` on macOS,
hidden visibility and `--exclude-libs` on Linux), and its header `orImageIO.h` names no OIIO type. `Option "imageio" "backend" ["legacy"|"oiio"]` and the per-call
`"string imageio"` parameter of the four `Make*` calls select the backend (the call wins over the Option, then `legacy`); `otexmake -imageio`; `Display ... "oiio"`
is the driver. The legacy path is the default and is unchanged: with OIIO off every image, file and hash is identical (the per-platform legacy bake hashes,
`tests/unit/imageio/fixtures/bake/legacy-bake-{macos,linux}.sha256`). See `DEVNOTES_DETAILS/IMAGE_IO.md` and gotcha 40.
```

### 1c. Testing list: add after the `registry` line

```
ctest --test-dir build-oiio -L imageio                  # spec 048 (OIIO-on tree): decode parity and differences, selection, bake parity and exclusions, display parity, single copy
```

and, below the list, one sentence: `-L imageio` registers different tests with OIIO off (`ImageIO_AbsentBuild`, `ImageIO_SelectWithoutOIIO`, `ImageIO_CiHasNoOiio`,
`ImageIO_VcpkgManifest`) and on (the rest); the OIIO-on tree is a separate build directory (`build-oiio`) with its own vcpkg install directory.

### 1d. New numbered gotcha (after 39)

```
40. **A utility library has its own rule, and the OIIO tree has its own vcpkg install directory (spec 048).** `libRIBImageIO` (class ImageIO, constitution 1.5.0)
   may include only the standard library, `common/`, its own header and OpenImageIO's headers (`ImageIO_NoDomainInclude`, archcheck), and the only edges into it are
   Texturing's and Display Outputs'; a renderer-core or Scene Interface include in `src/imageio/` is a failing test. Never match an OIIO symbol by "OpenImageIO_v3":
   a mangled name is `N11OpenImageIO4v3_1...` (`ImageIO_SingleCopy` matches `OpenImageIO`), and read an UNSTRIPPED install (`DESTDIR=...`, because
   `cmake --install --prefix` ignores `OPENRENDER_HOMEDIR`, GitHub #23): a stripped one drops the local symbols that hold the statically linked OIIO.
   `OPENRENDER_ENABLE_OIIO=ON` under vcpkg installs into `<VCPKG_INSTALLED_DIR>-oiio` (or `vcpkg_installed-oiio`) and the configure refuses the shared tree and
   the manifest-relative default (manifest mode would uninstall what the OIIO-off builds need, and write this manifest into a tree other worktrees use): pass no
   `VCPKG_INSTALLED_DIR` in the environment equal to the OIIO directory. The OIIO baked texture is written by OIIO only where its PHOTOMETRIC and EXTRASAMPLES
   equal the legacy writer's (the exclusion table `kOiioBakeExclusions` in `texmake.cpp`: 16-bit with 3 or more channels, and 5 or more channels, stay legacy,
   with a `CODE_SYSTEM` info message, because `RiError` never forwards `CODE_LOG` to the handler); an OIIO bake carries a `SOFTWARE` tag that begins
   `OpenImageIO` and a legacy one does not (the probe the tests use). OIIO stores a 4-sample TIFF's alpha as the display driver declares it (associated) and PNG
   alpha as straight; the legacy `file` driver writes 16-bit PNG byte-swapped (GitHub #181), so the parity tests decode the legacy side with its bytes swapped back
   (`D-PNG-WRITE16`). A truncated TIFF reads as success from OIIO unless `has_error()` is checked (`Reader::read` does), and OIIO names its 32-bit integer types
   `uint`/`int`.
```

### 1e. "Active Technologies" list: add

```
- C++20, CMake ≥3.19, `-Werror`; optional OpenImageIO ≥ 3.0 (`OPENRENDER_ENABLE_OIIO`, off by default): the Image I/O utility library `libRIBImageIO`
  (`orimageio::Reader`, `TiledTiffWriter`, `ScanlineWriter`), the `COiioImageInput` decode adapter, the `COiioBakeSink` bake writer and the `oiio` display driver
  (048-oiio-image-io)
- Storage: N/A; the legacy bake hashes and the `tests/unit/imageio/fixtures` references are recorded files (048-oiio-image-io)
```

### 1f. "Recent Changes": new first entry

```
- 048-oiio-image-io: an optional OpenImageIO backend for image input and output (`OPENRENDER_ENABLE_OIIO`, off by default; constitution 1.5.0 adds the Image I/O
  utility library `libRIBImageIO`). `Option "imageio" "backend"`, the per-call `"imageio"` parameter and `otexmake -imageio` select OpenImageIO to decode source images and to
  bake textures (tags and samples equal to the legacy bake, except 16-bit 3+ channel and 5+ channel textures, which the legacy writer keeps with a message); the `oiio` display
  driver writes TIFF, PNG, OpenEXR and RGBE with the legacy drivers' samples (display interface unchanged at 2.0.0); exactly one image of the install tree carries OIIO
  (`ImageIO_SingleCopy`); no colour conversion on read or write. The decode difference list (`D-*`, `IMAGE_IO.md`) is tested; with OIIO off no image, file or hash changed on either
  platform. vcpkg gets an `oiio` feature in its own install directory. Filed, not fixed: #171, #172, #174, #181 (legacy RGBE cannot read RLE; the RGBE `rgba` output is garbled;
  otexmake exits 0 for undecodable sources; the legacy `file` driver writes 16-bit PNG byte-swapped). (Phase 9) closing figures: <fill>.
```

## 2. NEWS.md (new first line under "## Unreleased")

```
- (2026-11-17) An optional OpenImageIO backend for images (spec 048). Configure with `-DOPENRENDER_ENABLE_OIIO=ON` (OpenImageIO 3.0 or newer; off by default, and a build without it is unchanged). `Option "imageio" "backend" ["oiio"]` (or `otexmake -imageio oiio`, or `"string imageio" ["oiio"]` on `MakeTexture`, `MakeShadow`, `MakeLatLongEnvironment` and `MakeCubeFaceEnvironment`) decodes source images and bakes textures through OpenImageIO; the baked texture has the same tags and samples as the built-in one, except 16-bit textures of 3 or more channels and textures of 5 or more channels, which the built-in writer still writes (with a message). A new display driver, `Display "name" "oiio" "rgba"`, writes TIFF, PNG, OpenEXR and RGBE through OpenImageIO. No colour conversion is applied on read or write. Known: the built-in `file` display driver writes 16-bit PNG with its bytes swapped (GitHub #181); the `oiio` driver does not.
```

## 3. ChangeLog.md (under `## [Unreleased]` / `### Added`)

```
- Optional OpenImageIO image backend (`OPENRENDER_ENABLE_OIIO`, off by default): `libRIBImageIO`, `Option "imageio" "backend"`, the per-call `"imageio"` parameter, `otexmake -imageio`, and the `oiio` display driver (spec 048).
- Constitution 1.5.0: utility libraries (the Image I/O library is the first).
```
