# Contract: `libRIBImageIO`, the Image I/O utility library

Class `ImageIO` (constitution 1.5.0, utility library). Target `openrenderimageio`, output name `RIBImageIO`, sources
`src/imageio/`. It is built only when `HAVE_OIIO`.

## Dependencies

- PRIVATE: `OpenImageIO::OpenImageIO` (and through it OpenColorIO and the codecs).
- PUBLIC: `openrendercommon` (diagnostics). Nothing else.
- It includes no header outside `src/imageio/`, `src/common/` and OIIO's. `ImageIO_NoDomainInclude` is a
  `cmake -P` check over the target's sources, and archcheck enforces the same rule as `ImageIO ->`.

## Linkage and export rules

- Shared only, with no static variant.
- Exports `orimageio::` symbols only:
  - macOS: `LINKER:-exported_symbols_list,src/imageio/exports.txt`, plus `-dead_strip`;
  - Linux: `CXX_VISIBILITY_PRESET hidden`, `VISIBILITY_INLINES_HIDDEN`, `ORIMAGEIO_API` on the public classes,
    `LINKER:--exclude-libs,ALL`.
- `orImageIO.h` includes only standard headers. It names no OIIO, OCIO, Imath or libtiff type and uses pimpl. A
  consumer needs no OIIO include path and instantiates no OIIO template.
- Installed into the library directory of both layouts, in the `openRenderTargets` export. `orImageIO.h` is **not**
  a public header (not in `OPENRENDER_PUBLIC_HEADERS`): the interface stays internal until 049.

## Published interface (`src/imageio/orImageIO.h`)

```cpp
namespace orimageio {

enum class Format { Tiff, Png, Rgbe, OpenExr };
enum class SampleType { UInt8, UInt16, Half, Float };   // Half: storage type for writing only
enum class StoredKind { UInt, Int, Float };

// One-time OIIO setup is done lazily inside every entry point (std::call_once).
const char *oiioVersion();                 // e.g. "3.1.14"

struct SourceTraits { /* data-model.md §3 */ };

class ORIMAGEIO_API Reader {               // one-shot: open, read once, close
public:
    Reader(); ~Reader();
    Reader(const Reader &) = delete; Reader &operator=(const Reader &) = delete;
    bool open(const char *path, Format expected, SourceTraits &traits);  // false + error() on failure
    bool read(void *dest, SampleType delivered);   // UInt8/UInt16/Float; tightly packed, stored channel order
    bool matrix(const char *name, float m[16]) const;   // "worldtocamera" / "worldtoscreen"
    void close();                                  // idempotent
private:
    struct Impl; Impl *impl;
};

struct DirectoryDesc {                     // one TIFF directory of a baked texture
    int width, height, channels;
    SampleType type;                       // UInt8, UInt16, Float
    int tileSize;                          // > 0, multiple of 16
    const char *textureFormat;             // nullptr = not written
    const char *wrapModes;                 // nullptr = not written
    const float *worldToCamera;            // nullptr = not written
    const float *worldToScreen;            // nullptr = not written
    int fullWidth, fullHeight;             // 0 = not written
    bool unassociatedAlpha;                // true for every bake (matches EXTRASAMPLE_UNASSALPHA)
};

class ORIMAGEIO_API TiledTiffWriter {
public:
    bool open(const char *path);
    bool appendDirectory(const DirectoryDesc &d, const void *pixels);   // whole layer, row-major
    bool close();                          // false if any write failed; the file is then removed
};

struct ScanlineDesc {
    Format format; int width, height, channels; SampleType type;
    const char *channelNames[5];           // "R","G","B","A","Z" by position
    const char *compression;               // already mapped to OIIO's name, nullptr = format default
    const char *software;                  // nullptr = none
    const float *worldToCamera, *worldToScreen;   // TIFF only, nullptr = none
};

class ORIMAGEIO_API ScanlineWriter {
public:
    bool open(const char *path, const ScanlineDesc &d);
    bool writeRow(int y, const void *row);       // rows in order, `type`-typed, tightly packed
    bool close();
};

} // namespace orimageio
```

## Behaviour

- **Reader**: `ImageInput::create(<format name>)`, then `open(path, spec, config)` with
  `oiio:UnassociatedAlpha=1` and `oiio:RawColor=1`. It never uses the probing static open. After opening it checks
  `format_name()` and fills the traits (PNG IHDR bytes from a 33-byte header read). `read` converts only HALF to
  FLOAT; any other conversion request is a bug (`CODE_BUG`).
- **TiledTiffWriter**: the first directory opens with `Create`, every later one with `AppendSubimage`, never as a
  MIP level. For each directory it sets:
  - `compression "lzw"`, `tiff:Predictor` 1, `Orientation` 1, `ResolutionUnit "none"`, `XResolution` and
    `YResolution` 1, `oiio:UnassociatedAlpha` from the desc;
  - the Pixar attributes `textureformat`, `wrapmodes`, `worldtocamera`, `worldtoscreen` when given;
  - for the full size, `full_x = 1, full_width = W - 1` and `full_y = 1, full_height = H - 1`, with the offset moved
    to the other axis for a 1-pixel axis (research R8, P-Q3a). When `fullWidth == 0` it leaves full == data, and
    OIIO writes no full-size tag.
- **ScanlineWriter**: sets `oiio:UnassociatedAlpha=1` for PNG and TIFF and the channel names. For RGBE it requires
  3 channels: the driver passes R,G,B (data-model §5, P-Q8).
- **Errors**: every failure calls `error(CODE_NOFILE | CODE_BADTOKEN | CODE_SYSTEM, …)` with the file and OIIO's
  message, and returns false. Every OIIO call is inside `try`/`catch (...)`, and nothing throws out of the library.
- **Threads**: a `Reader` or writer is used by one thread at a time. Several may be open at once on different
  threads. The library holds no lock of its own across calls, takes no lock owned by the texture system, and never
  calls back into a caller (gotcha 25).
- **Init**: on first use, OIIO attributes `threads` = 1, the EXR pool set to keep work in the calling thread, and
  `plugin_searchpath` = "".
