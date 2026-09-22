# Contract: `CImageInput` decode interface

This is the internal C++ interface contract this feature introduces. It is
the seam every current and future source-format decoder implements, and
the seam `texmake.cpp`'s bake paths consume through — this contract is the
deliverable that specs 019/020 (a separate, later effort) and any future
OpenImageIO-backed decoder are expected to build against without changes
to this shape.

## Interface

```cpp
// src/ri/texture/imageInput.h

struct CImageInfo {
    int width = 0;
    int height = 0;
    int numChannels = 0;
    int bitsPerSample = 0;   // 8, 16, or 32 — no half-precision tier
    bool isFloatFormat = false;
};

class CImageInput {
public:
    virtual ~CImageInput() {}

    // Opens `filename` and populates `info` on success. Returns false
    // (and reports a clear error via the project's existing error()
    // mechanism, naming the file and the reason) on any failure: file not
    // found, wrong/corrupted content for the format, or an unsupported
    // layout for that format (e.g. indexed PNG, multi-part EXR).
    virtual bool open(const char *filename, CImageInfo &info) = 0;

    // Reads the full image into `dest`, a caller-allocated, row-major,
    // tightly-packed buffer sized as
    // width * height * numChannels * (bitsPerSample / 8) bytes, using the
    // native sample type implied by `info` (uint8_t, uint16_t, or float).
    // Must only be called once, after a successful open(). Returns false
    // on any I/O or decode failure.
    virtual bool readImage(void *dest) = 0;

    // Releases any open file handles / decoder state. Safe to call
    // multiple times; also called implicitly by the destructor if not
    // called explicitly.
    virtual void close() = 0;
};

// Returns a new CImageInput instance appropriate for `filename`'s
// extension (see data-model.md's Format Registration Table), or nullptr
// if the extension maps to a format this build does not support (e.g.
// `.exr` when HAVE_OPENEXR is off). Ownership transfers to the caller.
CImageInput *createImageInput(const char *filename);
```

## Contract rules

1. **One-shot usage.** Callers must call `open()` exactly once, then
   `readImage()` at most once, then `close()` (or destroy the object).
   Implementations are not required to support reuse, reopening, or
   partial/streamed reads — this matches how every current
   `texmake.cpp` call site already uses `readLayer()`.
2. **Native precision preserved.** Implementations must report the
   source's own native sample precision in `CImageInfo` and deliver
   samples in that precision from `readImage()` — never silently upcast
   or downcast. The one documented exception is OpenEXR `HALF` channels,
   which have no native tier in this system and are promoted to 32-bit
   float (see `research.md` §3).
3. **No colorspace/gamma conversion.** `readImage()` delivers raw decoded
   samples exactly as stored in the source file. This applies uniformly
   across all four decoders.
4. **Fail closed, not partial.** Any layout a decoder does not support
   (e.g. an indexed PNG, a multi-part or non-RGB/RGBA/luminance EXR) must
   cause `open()` to return `false` with a clear error — never a partial,
   guessed, or silently-wrong decode.
5. **`createImageInput()` never throws or crashes on an unrecognized or
   unsupported-in-this-build extension** — it returns `nullptr`, and the
   caller (`texmake.cpp`) is responsible for reporting the resulting "no
   decoder available" error before aborting the bake operation.

## Conformance

Each concrete decoder (`CTiffImageInput`, `CPngImageInput`,
`COpenExrImageInput`, `CRgbeImageInput`) is validated against this contract
by the unit tests under `tests/unit/image_input/` (see `quickstart.md`):
open/readImage/close round-trips against small format-specific fixtures,
plus one negative test per documented rejection rule (indexed PNG,
multi-part EXR, unsupported EXR channel set, corrupted/malformed file per
format).
