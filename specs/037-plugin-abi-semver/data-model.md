# Data Model: Versioned Plug-in ABIs (spec 037)

Entities in the order a load meets them. Field names are the planned ones; the exact C
declarations are in [contracts/](contracts/). "Host" is the renderer process (or `oshader` for the
compiler's shadeop scan).

---

## 1. Interface version (`uint32_t`)

| Field | Bits | Meaning |
|---|---|---|
| MAJOR | 31..16 | incompatible change |
| MINOR | 15..8 | backward-compatible addition |
| PATCH | 7..0 | fix, no interface change |

- Encode `OR_ABI_VERSION(M, m, p) = (M << 16) | (m << 8) | p`; decode with `OR_ABI_MAJOR/MINOR/PATCH`.
- MINOR and PATCH are capped at 255 by the encoding (spec Assumptions). A header that would need
  MINOR 256 must bump MAJOR.
- Per family the host holds: its own version, the list of MAJORs it supports, and whether an
  absent version symbol warns.

| Family | Host version | Supported MAJORs | Absent symbol |
|---|---|---|---|
| display | 2.0.0 | 1 (v1 adapter), 2 | 1.0.0 + legacy warning |
| shadeop | 1.1.0 | 1 | 1.0.0 + legacy warning |
| dlo | 1.0.0 | 1 | 1.0.0 + legacy warning |
| implicit | 1.0.0 | 1 | 1.0.0 + legacy warning |
| dynamicload | 1.0.0 | 1 | 1.0.0, **no** warning (FR-009) |

## 2. Version verdict (host, `libRIBCommon`)

`SPluginVersionVerdict { EPluginVerdict verdict; uint32_t pluginVersion; bool versioned; }`

```
verdict ∈ { Accept, AcceptNewerMinor, AcceptUnversioned, Reject }
```

Rules, evaluated once per (family, `realpath(module)`) per process and cached:

| Condition | Verdict | Diagnostic |
|---|---|---|
| symbol absent, family warns | AcceptUnversioned | one warning: unversioned, treated as 1.0.0, rejected from 1.2.0 |
| symbol absent, dynamicload | Accept (as 1.0.0) | none |
| MAJOR(plugin) ∉ supported | Reject | one error: file, interface, plug-in version (raw hex too), host version, "rebuild against the installed header" |
| MAJOR supported, MINOR(plugin) > MINOR(host for that MAJOR) | AcceptNewerMinor | one warning naming both versions |
| otherwise | Accept | none |

The "host for that MAJOR" version of display MAJOR 1 is 1.0.0 (the adapter's level). A cached
verdict is replayed silently on later loads; a `Reject` is never retried in the same process.

## 3. Function table header (convention)

`OrTableHeader { uint32_t structSize; uint32_t abiVersion; }` — first two members of every table.

- **Required size** = `offsetof(last field of MAJOR.0) + sizeof(it)`, a constant per table type
  per MAJOR, held by the host.
- `structSize < sizeof(OrTableHeader)` or `< required` → reject without reading further.
- `MAJOR(abiVersion)` must equal the module's version-symbol MAJOR → otherwise reject.
- Fields added in MINOR *k* are read only if `structSize` covers them **and** `MINOR(abiVersion) ≥ k`.
- Bytes beyond the host's known size are ignored.

## 4. Display entities (v2)

### 4.1 Display module (`CDisplayModule`, renderer core)

| Field | Notes |
|---|---|
| path | resolved file, or `"custom"` for `RiCustomDisplay` |
| osHandle | `osLoadModule` result (null for custom) |
| verdict | §2 |
| plugin | `const OrDisplayPlugin *` (v2) or the v1 adapter's table |
| refCount | one per display using it; unload at 0, only from `CDisplayHost::finishAll` |

### 4.2 Display context (`OrDisplayContext`, opaque; host `CDisplayContext`)

| Field | Notes |
|---|---|
| module | §4.1 reference |
| instance | `void *` returned by `start` |
| info | §4.3, host-owned, const to the plug-in |
| state | `NotStarted → Started → Disabled? → Finished` |
| enabled | `std::atomic<bool>`, read before each `data` |
| storage | the copies behind §4.3 (strings, float arrays, channel names) |

State transitions:

```
NotStarted --start OK--> Started --data non-OK--> Disabled
NotStarted --start fail--> Finished (no finish call; module ref released at finishAll)
Started  --finishAll--> Finished (finish called once)
Disabled --finishAll--> Finished (finish called once; v1 adapter skips the v1 displayFinish)
```

No `data` call starts after `Disabled` is observed; no call of any kind after `Finished`.

### 4.3 Parameter snapshot (`OrDisplayInfo`, const)

| Field | Source |
|---|---|
| structSize, abiVersion | host (2.0.0) |
| name | `getDisplayName()` result (frame/sequence expansions applied) |
| type | `COptions::CDisplay::outDevice` as written (e.g. `tiff`, `png`, `shadow`) |
| mode | `outSamples` as written (informational) |
| width, height | `xres`, `yres` |
| numSamples | floats per pixel in `data` = Σ channel `numSamples` |
| numChannels, channels | §4.4 |
| numParameters, parameters | RIB display parameters (order kept), then renderer-provided |

`OrDisplayParameter { const char *name; OrDisplayParamType type; int numItems; const void *data; }`

Renderer-provided entries (copies taken at `start`):

| name | type | numItems | value |
|---|---|---|---|
| `quantize` | float | 4 | display quantizer if set, else depth (mode `z`) or colour default |
| `dither` | float | 1 | same source, element 4 |
| `near`, `far` | float | 1 | `clipMin`, `clipMax` |
| `Nl` | float | 16 | `fromWorld` |
| `NP` | float | 16 | `worldToNDC` |
| `screen` | float | 16 | `toScreen` |
| `gamma`, `gain` | float | 1 | exposure |
| `Software` | string | 1 | `OPENRENDER_PROJECT_NAME` |
| `type` | string | 1 | `outDevice` |

Data layouts: float → `const float[numItems]`; vector → `const float[3*numItems]`; matrix →
`const float[16*numItems]`; integer → `const int[numItems]`; string → one `const char *` (the host
keeps only the first element, as today). Lookup = first entry with equal name, type and count
(RIB entries come first), which reproduces `findParameter`'s precedence.

Validation: an RIB parameter whose stored type is not one of the five (quad, mpoint, double; see
research M4) is not copied into the snapshot; FR-040 also stops `RiDisplayV` from storing it (research R18).

### 4.4 Channel descriptor (`OrDisplayChannel`)

| Field | Notes |
|---|---|
| name | channel name (`rgba`, `z`, an AOV name), host-owned copy |
| numSamples | floats for this channel |
| valueType | `OR_DISPLAY_VALUE_FLOAT` for standard rgba/z channels; the variable's type (`COLOR`, `POINT`, `VECTOR`, `NORMAL`, `FLOAT`, `MATRIX`) for AOVs |
| offset | first float of this channel within one pixel of `data` |
| quantize | `const float[5]` (zero, one, min, max, dither) when the channel has its own `DisplayChannel` quantization, else `NULL` |

Invariants: channels are in `mode` order; `offset[0] = 0`; `offset[i+1] = offset[i] + numSamples[i]`;
`Σ numSamples = info.numSamples` (US3-3). A `z`-only display has one descriptor; the list is never
read past `numChannels` (empty-safe).

### 4.5 v1 adapter state (`CDisplayV1State`, renderer core, adapter-private)

| Field | Notes |
|---|---|
| v1 entry points | module symbols, or the `RiCustomDisplay` triple |
| copies | mutable per-display copies of every value a v1 lookup can return |
| handle | v1 `displayStart` result |
| dataMutex | serializes v1 `displayData` for this display |
| failed | set when v1 `displayData` returned FALSE; later calls not forwarded, v1 finish not called |
| warnedUnbound | one-time diagnostic for lookups with no bound display |

Adapter-private `thread_local CDisplayV1State *boundDisplay` is non-null only on the thread inside
a v1 `displayStart`, only for its duration.

## 5. Shadeop entities

### 5.1 Shadeop declaration (parser output)

`SShadeopDeclaration { std::string name; std::string prototype; }` — `prototype` is
`<ret>=<params>` with letters `o f v n p c m s` (void, float, vector, normal, point, color, matrix,
string), identical to today's `dsoParse` output. Parse failure → no declaration.

### 5.2 Shadeop module (`CShadeopModule`, Shader DSO Operands)

| Field | Notes |
|---|---|
| path, osHandle, verdict | §2 |
| tables | resolved `<name>_shadeops` (and `<name>_shadeop_threads` when MINOR ≥ 1) per requested name |
| refCount | operands using it; unload at 0 on release |

### 5.3 Shadeop operand (`CShadeopOperand`; Shading sees it as today's `CDSO` handle)

| Field | Notes |
|---|---|
| name, prototype | key; the cache matches both |
| module | §5.2 reference |
| init, exec, cleanup | resolved; `exec` non-null by construction (FR-013) |
| threadInit, threadCleanup | optional (1.1) |
| initData | `init(0, NULL)` result, process-wide |
| slots | one entry per thread index `{ std::atomic<bool> initialized; void *data; }`, only when threadInit exists; grown (never shrunk) by the registry's `beginFrame(numThreads)` before shading threads start, and sized at acquisition from the current frame's count |

`exec` receives `slots[thread].data` when per-thread entry points exist, else `initData`.
Release order: every initialized slot's `threadCleanup(initData, slot)`, then `cleanup(initData)`,
then the module reference (FR-011).

### 5.4 Operand registry (`CShadeopRegistry`, owned by the caller)

The renderer core owns one (replacing `CRenderer::dsos`) for the `RiBegin`..`RiEnd` session;
`oshader` uses the enumerate call only and owns nothing. No process-wide state in the shadeop
library.

## 6. Geometric Procedures entities

### 6.1 Procedural module (`CProceduralModule : CRefCounter`)

| Field | Notes |
|---|---|
| kind | dlo, implicit, dynamicload |
| path | the located path (dlo, implicit) or the generator string (dynamicload) |
| osHandle, verdict | §2 |
| entry points | `SDloEntryPoints {init, intersect, tini}`, `SImplicitEntryPoints {init, eval, evalNormal, tini}`, `SDynamicLoadEntryPoints {convertParameters, subdivide, free}` |

Lifetime:
- dlo / implicit: one reference per `CDLObject`/`CImplicit`, released in its destructor; unload when
  the count reaches 0.
- dynamicload: a frame reference taken at first expansion in a frame (one per module per frame),
  released by `releaseFrameProceduralModules()` right after `root->destroy()` in `endFrame`.

Required symbols: dlo `dloInit`, `dloIntersect`, `dloTini`; implicit `implicitInit`,
`implicitEval`, `implicitTini` (`implicitEvalNormal` optional, as today); dynamicload none required
(each of the three is called only if resolved, as today).

## 7. Texturing channel information (`STexture3dChannelInfo`)

`{ const char *name; int numSamples; int type; const float *fill; }` — filled by the installed
`CTexture3dChannelSource` from the Graphics State registry. `fill` aliases the registry's array
(today's behaviour). With no source installed, `find()` is not called and the name is reported as
unknown.

## 8. Plug-in test fixtures (build-tree only)

| Family | Variants (one source, compile definitions) |
|---|---|
| display | `v2` (2.0.0), `v2_hidden` (hidden visibility), `v2_newer` (2.9.0), `v3` (3.0.0), `garbage` (`0xFFFFFFFF`), `v1_legacy` (no symbol), `v1_explicit` (1.0.0), `v1_newer` (1.4.0), `v1_hostile`, `v2_hostile`, `v2_record` (logs snapshot and descriptors), `v2_failfirst`, `v1_late_lookup` (keeps the callback, calls it in `data`) |
| shadeop | `good` (1.1, per-thread), `unaware` (1.0, no per-thread), `legacy` (no symbol), `v2` (2.0.0), `newer` (1.9.0), `missing_exec` (dir A) + `has_exec` (dir B), `hidden` |
| dlo | `good`, `legacy`, `v2`, `newer`, `missing_symbol`, `hidden`, `bare_name` (only reachable through the procedural path) |
| implicit | same set as dlo |
| dynamicload | `plain` (no symbol), `versioned` (1.0.0), `v2`, `nested` (Subdivide emits an `RiProcedural` whose functions live in the module; logs unload from a destructor) |
| convention | `conv_current`, `conv_larger`, `conv_smaller` (structSize below required, table placed at the end of a page followed by a `PROT_NONE` guard page), `conv_major2` |
