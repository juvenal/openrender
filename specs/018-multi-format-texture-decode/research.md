# Phase 0 Research: Multi-Format Texture Source Decoding

## 1. Format auto-detection mechanism

**Decision**: Dispatch by filename suffix, via a small static
`{suffix → format}` table checked in `createImageInput()`, defaulting to
TIFF when nothing else matches (mirrors current TIFF-is-the-assumed-format
behavior exactly, so an unrecognized-but-TIFF-shaped file keeps working).
Accepted suffixes: `.tif`/`.tiff` → TIFF, `.png` → PNG, `.exr` → OpenEXR
(only when `HAVE_OPENEXR`), `.hdr`/`.pic` → RGBE.

**Rationale**: The project's own existing dispatcher,
`src/display/file/file.cpp:displayStart()`, already does exactly this for
render-*output* format selection — a suffix comparison
(`strcmp(&name[len-4], ".png") == 0`), TIFF as the fallback. Reusing the
same mechanism keeps the new source-side dispatch consistent with an
established, working project convention instead of inventing a second one
(e.g. magic-byte sniffing), which the spec's Assumptions section already
rules out.

**Alternatives considered**: Magic-byte/content sniffing (rejected — spec
explicitly assumes extension-based detection, and adds complexity for no
stated benefit); a build-time-generated table shared with the *output*
dispatcher (rejected — the two dispatchers select different concrete class
families for different purposes; sharing a table would create a coupling
between read and write format support that doesn't otherwise exist and
isn't needed for this or any of the two dependent later specs).

## 2. Threading `HAVE_OPENEXR` into `src/ri` build targets

**Decision**: `imageInputExr.cpp` is compiled and linked into the
`src/ri` targets (the same targets that already build `texmake.cpp`,
confirmed at `src/ri/CMakeLists.txt` lines ~127-129/~293/~387) only when
`HAVE_OPENEXR` is `ON`, using the already-computed root-level
`OPENRENDER_OPENEXR_LIBS` variable (`CMakeLists.txt:383-412`) for linking.
When `HAVE_OPENEXR` is `OFF`, `imageInputExr.cpp` is excluded from the
build entirely and `createImageInput()` returns a clear "OpenEXR support
not built into this binary" error for `.exr` sources instead of a link or
compile failure.

**Rationale**: `HAVE_OPENEXR`/`OPENRENDER_OPENEXR_LIBS` today only feed the
`src/display/openexr/` *display*-driver module target. This is the first
time OpenEXR becomes a conditional dependency of an `src/ri` target;
mirroring the existing variable rather than re-probing OpenEXR a second
time keeps a single source of truth for "is OpenEXR usable on this build."
Graceful degradation (compiled out, clear runtime error) matches
constitution V (Minimal Dependencies: "Build system MUST gracefully handle
missing optional dependencies").

**Alternatives considered**: Making OpenEXR a hard `REQUIRED` dependency of
`otexmake`/`orender` (rejected — violates constitution V and the project's
own documented rationale for keeping OpenEXR optional, see
`CMakeLists.txt:370-382`); duplicating the `find_package(OpenEXR)` probe
inside `src/ri/CMakeLists.txt` (rejected — redundant, risks the two probes
disagreeing).

## 3. OpenEXR read API and multi-part detection

**Decision**: Open every `.exr` source via `Imf::MultiPartInputFile`
first. If `parts() > 1`, reject immediately with a clear error (per the
spec's clarified, unconditional multi-part rejection). Otherwise, read the
single part's header and pixel data through the classic `Imf`/`Imath` API
(`Imf::InputFile`-equivalent single-part access), the same API family
already used for the *write* side in `src/display/openexr/openexr.cpp`,
which is documented (`CMakeLists.txt:307-320`) to compile unchanged against
both the OpenEXR 3.x (`OpenEXR::OpenEXR`+`Imath::Imath`) and 2.5+
(`OpenEXR::IlmImf`+`IlmBase::Imath`+`IlmBase::Half`) packagings this
project already supports. Channel layout is read from the part's
`ChannelList`: exactly `{R,G,B}`, `{R,G,B,A}`, or a single channel (treated
as luminance) are accepted; anything else is rejected with a clear error
naming the unsupported channel set. Any channel stored as `HALF` is
promoted to 32-bit float on decode (there is no half-precision tier in the
existing bake pipeline's `T ∈ {uint8, uint16, float}` template set).

**Rationale**: Multi-part support was introduced in OpenEXR 2.0, so
`Imf::MultiPartInputFile` is available across every OpenEXR version this
project already targets; checking `parts()` is the standard, cheap way to
detect multi-part files without a bespoke format-sniffing pass. Using the
same classic API family as the existing write-side driver avoids
introducing a second OpenEXR API surface (e.g. the 3.x-only Core C API)
into the codebase.

**Alternatives considered**: The OpenEXR 3.x Core C API (rejected — not
available on the 2.x packaging this project still supports per
`CLAUDE.md`'s documented Ubuntu-baseline constraints); inspecting only
`Imf::InputFile`'s own part-count query if one exists (rejected in favor of
`MultiPartInputFile`, which is the documented, version-stable way to learn
part count up front before committing to a read path).

## 4. libpng read path and indexed/grayscale handling

**Decision**: Use libpng's high-level row-based read API
(`png_read_info`/`png_get_IHDR`/`png_read_row` loop, matching the
project's existing use of libpng on the *display*-output side in
`src/display/file/file_png.cpp` for library-usage consistency). Read
`color_type` from `png_get_IHDR()` immediately after `png_read_info()`:
`PNG_COLOR_TYPE_RGB`, `PNG_COLOR_TYPE_RGB_ALPHA`, `PNG_COLOR_TYPE_GRAY`,
and `PNG_COLOR_TYPE_GRAY_ALPHA` are accepted as-is (no palette expansion,
no gray-to-RGB expansion — each maps directly to a channel count);
`PNG_COLOR_TYPE_PALETTE` is rejected immediately with a clear error before
any pixel data is read. Bit depth (8 or 16) is read as-is; libpng returns
16-bit samples in network (big-endian) byte order per channel, so
`png_set_swap()` is called when the host is little-endian to normalize to
the native-endian `unsigned short` representation the rest of the bake
pipeline (and `CImageInfo`/`readImage()`) already assumes for its 16-bit
tier.

**Rationale**: Reusing the same libpng usage pattern already proven in
`file_png.cpp` (rather than the high-level whole-image
`png_read_png()`/`png_set_expand()` convenience API, which would silently
normalize palette/low-bit-depth images to 8-bit RGB(A) and defeat the
spec's explicit "reject indexed PNGs" requirement) keeps behavior
predictable and matches the spec's clarified scope exactly.

**Alternatives considered**: `png_set_palette_to_rgb()` +
`png_set_expand()` to accept indexed/low-bit-depth PNGs transparently
(rejected — explicitly out of scope per the spec's PNG color-type
clarification; would also reintroduce a "guess and convert" behavior the
spec's OpenEXR channel-scope decision deliberately avoids for symmetry).

## 5. RGBE integration

**Decision**: `CRgbeImageInput::open()`/`readImage()` call the existing
`RGBE_ReadHeader()` and `RGBE_ReadPixels()` functions
(`src/display/rgbe/rgbe.h`/`rgbe.cpp`, currently dead code with zero call
sites anywhere) directly via a standard `fopen()`-obtained `FILE*`. RGBE
data is inherently 3-channel float (`numChannels = 3`, `isFloatFormat =
true`, `bitsPerSample = 32`) — there is no channel-layout ambiguity to
resolve for this format, unlike PNG/EXR.

**Rationale**: The functions already exist, are already correct (used
nowhere yet, but implement the standard Radiance RGBE format per their
existing implementation), and the spec (FR-010) explicitly requires reuse
rather than reimplementation.

**Alternatives considered**: None — this is pure wiring, not a design
decision.

## 6. Error reporting convention

**Decision**: All decode failures (`open()` or `readImage()` returning
`false`) are reported through the existing `error(CODE_..., ...)`
mechanism already used throughout `texmake.cpp` (e.g. the existing
`tiffErrorHandler()` calling `error(CODE_SYSTEM, ...)`), with a message
naming the source file and the specific reason (unsupported format,
unsupported channel/color-type layout, multi-part rejection, corrupted
file). `otexmake` then aborts the current bake operation rather than
proceeding with partial or garbage data.

**Rationale**: Reuses the project's one existing error-reporting channel
rather than introducing a second one; satisfies FR-008 (clear, actionable
errors) and SC-004 with no new infrastructure.

**Alternatives considered**: A dedicated exception type for decode
failures (rejected — the surrounding codebase does not use C++ exceptions
for this class of error; `error()` plus a boolean return is the existing
idiom and keeps this feature consistent with it).
