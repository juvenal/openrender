# Contract: the display v1 adapter (FR-006, FR-019, FR-020, FR-039, SC-013)

Renderer core. Files: `src/ri/render/displayV1Adapter.{h,cpp}` and its private
`src/ri/render/displayV1Abi.h`. These are the **only** files that name a v1 display type. The
display host selects the adapter in one function (`CDisplayHost::bindPlugin`); nothing else in the
host branches on the display MAJOR.

## 1. What it wraps

| Source | v1 entry points | Callback type it is given |
|---|---|---|
| A loaded module whose version is 1.x or absent | `displayStart`, `displayData`, `displayFinish` (`displayRawData` ignored) | `TDisplayParameterFunctionV1 = void *(*)(const char *, ParameterTypeV1, int)` |
| `RiCustomDisplay` callbacks (`RtDisplayStartFunction` etc., `riVocabulary.h`) | the three RI callbacks | `RtDisplayParameterFunction = void *(*)(const char *, int, int)` |

`displayV1Abi.h` declares the v1 module types (moved verbatim from today's `dsply.h`, renamed with
a `V1` suffix: `ParameterTypeV1`, `TDisplayStartFunctionV1`, ...). Two adapter functions with
exactly those callback types forward to one lookup; no function pointer is cast between the RI and
module shapes (FR-019). `stateConversions.h` static-asserts `ParameterTypeV1`, `OrDisplayParamType`
and `EDisplayParameterType` agree.

## 2. Interface to the host

```cpp
// Wraps a v1 module or a custom-display triple as a v2 plug-in table.
const OrDisplayPlugin *displayV1PluginForModule(void *osHandle, const char *path); // NULL: missing symbols
const OrDisplayPlugin *displayV1PluginForCustom(RtDisplayStartFunction, RtDisplayDataFunction,
                                                RtDisplayFinishFunction);
```

Both return tables whose `start`/`data`/`finish` are adapter functions; the per-display v1 state is
the `instance` the adapter's `start` returns (data-model §4.5).

## 3. Behaviour

1. **start.** Builds mutable per-display copies of every value a v1 lookup can return (from the
   v2 `OrDisplayInfo`), sets the adapter's `thread_local` `boundDisplay` to this state, calls the v1
   `displayStart(info->name, width, height, numSamples, info->mode, callback)`, clears
   `boundDisplay`, and fails if the v1 handle is `NULL`.
2. **Lookups.** With `boundDisplay` set: the first entry matching name, type and count among the
   copies (RIB first, then renderer-provided, same precedence as v2). Returns a pointer into the
   copy (non-`const`, as the v1 type requires). With `boundDisplay` unset (a call after `start`
   returned, or from another thread): returns `NULL` and logs once per display (FR-020, FR-039).
   A v1 driver that writes through a returned pointer changes only its own copy (FR-006).
3. **data.** Under the per-display `dataMutex`: if `failed`, return `OR_STATUS_ERROR` without
   calling the driver; otherwise call v1 `displayData` with the same host-owned per-call scratch
   buffer v2 receives (`float *` in both versions; the host does not read it back); a `FALSE`
   sets `failed` and returns `OR_STATUS_ERROR`.
4. **finish.** If not `failed`, call v1 `displayFinish(handle)`; free the copies. (A failing v1
   `displayData` had the old contract "no finish afterwards"; legacy drivers such as the old
   framebuffer free themselves there.)
5. **Warnings.** The unversioned warning comes from `checkPluginVersion` (once per module per
   process). An explicit 1.x prints nothing; 1.x with MINOR > 0 prints the newer-MINOR warning.
   Custom displays print nothing.

## 4. Why a thread-local is acceptable here (FR-006 vs FR-014)

The v1 callback carries no context, so a lookup must learn its display from somewhere. FR-006
allows the adapter to "bind the display for the duration of that driver's `displayStart` call
only". The binding is:
- in the adapter only (no host or v2 code can read it),
- per thread (another display starting concurrently on another thread has its own),
- set and cleared around one call (nothing is bound between calls),

so it is not a host-wide or file-static "current display" in the sense of FR-014. Q6 resolved:
the maintainer accepted this reading.

## 5. Tests

| Test | Covers |
|---|---|
| `DisplayV1_LegacyLoads` | base-tree `file.dsply` (unversioned) renders pixels identical to the capture; one legacy warning (SC-013, US1-6) |
| `DisplayV1_Explicit1x` | v1_explicit: no warning; v1_newer: one warning (edge cases) |
| `DisplayV1_HostileCopiesOnly` | v1_hostile writes through every pointer it gets; renderer statics unchanged (SC-013) |
| `DisplayV1_LateLookup` | v1_late_lookup calls the stored callback from `data` for `quantize`, `dither`, `near`, `type`: all `NULL`, one log line, no crash (FR-039, US3-6a) |
| `DisplayV1_CustomDisplay` | `RiCustomDisplay` from a test program receives the same values as before (the callback type is the RI one) |
| `DisplayV1_SerializedData` | v1 driver recording entry/exit of `displayData`, 8 threads, never two calls inside for one display; no `finish` after a failed `data`; module not unloaded while a call is in flight (FR-043, SC-017) |
| `DisplayV1_NoCastCheck` | mechanical: `grep` finds no cast to a display function type, no v1 display type name and no `displayV1Abi.h` include outside `displayV1Adapter.{h,cpp}`/`displayV1Abi.h`, and `bindPlugin` is the only adapter selector (FR-019, FR-046) |
