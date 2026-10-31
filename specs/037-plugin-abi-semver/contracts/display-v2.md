# Contract: display interface 2.0.0 (`src/display/dsply.h`; FR-001..FR-003, FR-014..FR-021, US3)

Display Outputs domain. Installed as `dsply.h` in the layout's include directory (self-contained `<ORENDERHOME>/include`; FHS `<prefix>/include/openRender/`). Includes `orplugin.h` only.

## 1. Version

```c
#define OR_DISPLAY_ABI_VERSION_MAJOR 2
#define OR_DISPLAY_ABI_VERSION_MINOR 0
#define OR_DISPLAY_ABI_VERSION_PATCH 0
#define OR_DISPLAY_ABI_VERSION OR_ABI_VERSION(2, 0, 0)
#define OR_DISPLAY_DECLARE_VERSION() \
    OR_DECLARE_ABI_VERSION_SYMBOL(orDisplayAbiVersion, OR_DISPLAY_ABI_VERSION)
```

Version history (in the header and in `DISPLAY_PLUGIN_GUIDE.md`):

| Version | Change | Host support |
|---|---|---|
| 1.0.0 | The Pixie-era interface: `displayStart`/`displayData`/`displayRawData`/`displayFinish`, `TDisplayParameterFunction`. Modules without `orDisplayAbiVersion` are 1.0.0. | Supported through the v1 adapter. Unversioned modules warn and are rejected from 1.2.0. |
| 2.0.0 | Function table `OrDisplayPlugin`, per-display context, `const` snapshot, channel descriptors, threading contract; `displayRawData` removed; `ParameterType` replaced by `OrDisplayParamType`. | Supported. |

Dropping MAJOR 1 support is itself a documented change of this table (constitution 1.3.0 rule 4).

## 2. Types

```c
#include "orplugin.h"

OR_DECLARE_HANDLE(OrDisplayContext);          /* host-owned, one per display */

typedef enum OrDisplayParamType {             /* values equal EDisplayParameterType (State) */
    OR_DISPLAY_PARAM_FLOAT   = 0,
    OR_DISPLAY_PARAM_VECTOR  = 1,
    OR_DISPLAY_PARAM_MATRIX  = 2,
    OR_DISPLAY_PARAM_STRING  = 3,
    OR_DISPLAY_PARAM_INTEGER = 4
} OrDisplayParamType;

typedef struct OrDisplayParameter {
    const char        *name;
    OrDisplayParamType type;
    int                numItems;
    const void        *data;  /* float: const float[n]; vector: const float[3n]; matrix: const float[16n];
                                 integer: const int[n]; string: one const char * (first element only) */
} OrDisplayParameter;

typedef enum OrDisplayValueType {
    OR_DISPLAY_VALUE_FLOAT  = 0,  /* standard rgba/z channels and float AOVs */
    OR_DISPLAY_VALUE_COLOR  = 1,
    OR_DISPLAY_VALUE_POINT  = 2,
    OR_DISPLAY_VALUE_VECTOR = 3,
    OR_DISPLAY_VALUE_NORMAL = 4,
    OR_DISPLAY_VALUE_MATRIX = 5
} OrDisplayValueType;            /* storage is always 32-bit float */

typedef struct OrDisplayChannel {
    const char        *name;
    int                numSamples;
    OrDisplayValueType valueType;
    int                offset;     /* first float of this channel inside one pixel */
    const float       *quantize;   /* [5] zero, one, min, max, dither — or NULL: use the display's */
} OrDisplayChannel;

typedef struct OrDisplayInfo {
    OrTableHeader             header;     /* structSize/abiVersion of this struct (host's) */
    const char               *name;       /* output name, frame expansions applied */
    const char               *type;       /* display type as written in RIB ("tiff", "png", "file", ...) */
    const char               *mode;       /* RIB mode string, informational only */
    int                       width, height;
    int                       numSamples; /* floats per pixel in data() */
    int                       numChannels;
    const OrDisplayChannel   *channels;
    int                       numParameters;
    const OrDisplayParameter *parameters; /* RIB parameters first, then renderer-provided */
} OrDisplayInfo;

typedef struct OrDisplayPlugin {
    OrTableHeader header;  /* structSize = sizeof(OrDisplayPlugin), abiVersion = OR_DISPLAY_ABI_VERSION */
    OrStatus (*start)(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance);
    OrStatus (*data)(OrDisplayContext *ctx, void *instance,
                     int x, int y, int width, int height, float *pixels);
                     /* pixels: host-owned scratch for this call only; the driver may modify it */
    void     (*finish)(OrDisplayContext *ctx, void *instance);
} OrDisplayPlugin;          /* required size for 2.x = sizeof of exactly these fields */

OR_EXTERN_C_BEGIN
OR_VISIBLE uint32_t               orDisplayAbiVersion(void);              /* via OR_DISPLAY_DECLARE_VERSION() */
OR_VISIBLE const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo *host);
OR_EXTERN_C_END

/* First parameter whose name, type and count all match (the old findParameter precedence). */
static inline const void *orDisplayFindParameter(const OrDisplayInfo *info, const char *name,
                                                 OrDisplayParamType type, int numItems);
```

`orDisplayFindParameter` uses only `<string.h>` (`strcmp`), so the header stays self-contained.

## 3. Renderer-provided parameters

Always present, after the RIB parameters, all host-owned copies (data-model §4.3):
`quantize` (float, 4), `dither` (float, 1), `near`, `far` (float, 1), `Nl`, `NP`, `screen`
(float, 16), `gamma`, `gain` (float, 1), `Software` (string), `type` (string). `quantize`/`dither`
are the display's own `Quantize` values when set, otherwise the depth defaults for a mode of exactly
`z` and the colour defaults otherwise (today's rule, `rendererDisplay.cpp:69-98`). No pointer in
`OrDisplayInfo` aliases renderer-core state (FR-015).

## 4. Threading and lifetime contract (FR-017, FR-018)

1. `start` is called once per display, on one thread, with no other call for that display in
   flight. `OR_STATUS_OK` plus a `*instance` (may be `NULL` if the driver needs none) starts it; any
   other status means "not started": no `data`, no `finish`.
2. `data` may be called concurrently from several threads for disjoint tiles of the same display.
   `pixels` holds `width * height * info->numSamples` floats, row-major, channel layout as the
   descriptors say. It is the host's per-call copy for this display (not renderer state): the
   driver may modify it in place (the in-tree drivers clamp and quantize it there), and it is valid
   only during the call.
3. A non-OK `data` disables the display. The host starts no new `data` call once it has seen the
   failure; calls already in flight on other threads may still arrive, and must return quickly
   (the driver should return non-OK again).
4. `finish` is called exactly once for every started display, enabled or disabled, after every
   `data` call has returned and on one thread. Nothing is called after it. The driver frees its
   instance in `finish`, never in `data`.
5. `info`, its strings, parameters and descriptors are valid from `start` until `finish` returns.
6. The module is unloaded only after `finish` has returned for every display using it
   (FR-018). Displays that never started release their reference at the same point.
7. Different displays (even of the same module) may be in `data` at the same time; a driver with
   module-wide state protects it itself.

## 5. Host behaviour (renderer core, `CDisplayHost`)

- Load: `locateFileEx(..., displayPath)`; `osLoadModule(located)`; `checkPluginVersion(display)`;
  MAJOR 2 → `orDisplayPlugin(&hostInfo)` then `checkPluginTable(..., requiredSize = sizeof the 2.0
  fields)`; MAJOR 1 (unversioned or explicit) → the v1 adapter (`display-v1-adapter.md`); other →
  rejected, the display is disabled and the render continues or stops exactly as for a missing
  driver today (`numActiveDisplays == 0` → `HIDER_BREAK`).
- A missing `start`, `data` or `finish` in a v2 table → "has missing implementation", as today.
- The `"custom"` device (`RiCustomDisplay`) always goes through the v1 adapter (its callbacks are
  RenderMan Interface v1-shaped).
- `"tsm"` and the network client path are unchanged (no plug-in).
- No file-static or global "current display" (FR-014); the free function `findParameter` and
  `currentDisplay` are deleted.

### Host interface (what the tests drive; `src/ri/render/displayHost.h`)

Written with the Phase 5 tests (T078 to T082); approved by the maintainer with them (2026-11-01):

```cpp
struct SDisplayParameterSpec {      // one RIB display parameter, values owned by the spec
    std::string name;
    int type;                       // an OrDisplayParamType value; any other value is skipped (research M4)
    int numItems;
    std::vector<float> floats;      // float: n; vector: 3n; matrix: 16n
    std::vector<int> integers;      // integer: n
    std::vector<std::string> strings; // string: only the first is exposed
};
struct SDisplayChannelSpec { std::string name; int numSamples; OrDisplayValueType valueType;
                             bool hasQuantize; float quantize[5]; };   // hasQuantize: this channel's own five values
struct SDisplaySpec {
    std::string name, type, mode;   // as written in the RIB (name with frame expansions applied)
    std::string module;             // the located module file; "" for a RiCustomDisplay display
    RtDisplayStartFunction customStart; RtDisplayDataFunction customData; RtDisplayFinishFunction customFinish;
    int width, height;
    std::vector<SDisplayChannelSpec> channels;     // in mode order; the host computes the offsets
    std::vector<SDisplayParameterSpec> parameters; // the RIB parameters, in order
    float quantizer[5];             // the display's own Quantize; quantizer[0] == -1: not set
    float colorQuantizer[5], depthQuantizer[5];    // the renderer's defaults
    float clipMin, clipMax, fromWorld[16], worldToNDC[16], toScreen[16], gamma, gain;
    std::string software;           // OPENRENDER_PROJECT_NAME
};
class CDisplayHost {
  public:
    int add(const SDisplaySpec &);            // returns the display index; nothing is loaded yet
    bool startDisplay(int index);             // load, version-check, bind, build the snapshot, start; false: disabled
    int startAll();                           // startDisplay for every display in order; returns how many started
    bool dispatch(int index, int x, int y, int w, int h, float *pixels);
                                              // false once the display is disabled (a failed start or data); never
                                              // calls the plug-in for a display with numSamples == 0
    void finishAll();                         // finish exactly once for every started display, then release the modules
    const OrDisplayInfo *info(int index) const; // the snapshot, valid from startDisplay until finishAll
    bool enabled(int index) const;
    ~CDisplayHost();                          // finishAll() if it has not run
};
```

The core (`rendererDisplay.cpp`) resolves the module file with `locateFileEx` against the display path and hands the
located path over in `SDisplaySpec::module`; the host loads exactly that file (a test passes an absolute path). A
string parameter's `OrDisplayParameter::data` is the NUL-terminated string itself (the `const char *` a v1 driver reads
as `(const char *)findParameter("type", STRING_PARAMETER, 1)`), not a pointer to a pointer.

## 6. In-tree drivers (FR-008, FR-021)

`file.dsply` (TIFF, PNG), `openexr.dsply`, `rgbe.dsply`, `framebuffer.dsply` each add
`OR_DISPLAY_DECLARE_VERSION()` and an `orDisplayPlugin` table. `CFileOutputBase` takes
`const OrDisplayInfo *` instead of `TDisplayParameterFunction` and reads `quantize`, `dither`,
`gamma` through `orDisplayFindParameter`. `file.cpp` reads `type` the same way (still the RIB
type, so `.png` vs `"tiff"` selection is unchanged). The framebuffer driver moves its
`delete cWindow` (its own connection object; `finish` sends DONE and closes the socket, and the `orender-fb` helper and its
window stay open) from the failing `data` into `finish`, and reads the channel string from
`info->mode`.

## 7. Tests

| Test | Covers |
|---|---|
| `DisplayAbi_Versions` | v2, v2_newer (1 warning), v3 and garbage (rejected, never entered), v1_legacy (1 legacy warning across 2 displays), v1_explicit (no warning), v1_newer (1 warning) — SC-001, SC-002, SC-013, US1 |
| `DisplayAbi_HiddenVisibility` | v2_hidden loads (US2-3) |
| `DisplayAbi_TwoDisplayParams` | two displays with different `quantize`/`dither`, 100 randomized start orders via `CDisplayHost` (SC-005, US3-1) |
| `DisplayAbi_Hostile` | v2_hostile and v1_hostile overwrite everything reachable; renderer statics (clip planes, matrices, gamma, gain, quantizers) and the other display's output unchanged (SC-005, US3-2, SC-013) |
| `DisplayAbi_ZeroAndDepthOnly` | zero-channel display (`start`/`finish` only) and a `z`-only display (one descriptor, depth defaults) (FR-049) |
| `DisplayAbi_Descriptors` | AOV display: names, counts, types, offsets; Σ = numSamples on every tile (US3-3) |
| `DisplayAbi_LateDisable` | v2_failfirst on ≥ 8 threads, 1,000 start-dispatch-finish cycles, finish once, unload after finish (SC-006, US3-4) |
| `DisplayAbi_ReleaseAfterFinish` | every module unloaded after its finish in `endDisplays` (FR-035, US3-6b) |
| `DisplayAbi_PixelsIdentical` | decoded pixels of every in-tree driver on the baseline scenes equal the pre-change capture (FR-021, SC-004) |
| `DisplayAbi_UnsupportedParamType` | `RiDisplayV` with `double`/`quad`/`mpoint` parameters: dropped with one warning each, `numParameters` counts only initialized entries, a `COptions` clone and the snapshot see only valid entries (FR-040, research R18) |
| `DisplayAbi_LegacyPixelsIdentical` | the unversioned base-tree `file.dsply` through the adapter equals the same capture (SC-013) |
