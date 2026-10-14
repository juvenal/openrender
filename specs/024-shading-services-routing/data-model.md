# Data Model: Shading Services Routing (spec 024)

There is no persistent data. These are the in-process entities whose shape or ownership changes.

## Renderer services (`CRendererServices`, Shading-owned interface)

- **Owner**: Shading (`src/libshader/shading/rendererServices.h`).
- **Implementer**: the renderer core (`CRendererServicesImpl`, `src/ri/render/rendererServicesImpl.h`).
- **Changes**:
  - Adds `getDSO(name, prototype) → CDSO*` ([contract](contracts/renderer-services.md)).
  - Removes `hiderFlags()` (research R8; approved 2026-10-14). `hasIlluminationHook()`
    stays and becomes the only hook query.
- **Lifetime** (state transitions):

  | Event | Default services |
  |---|---|
  | process start | none |
  | `RiBegin` → `CRenderer::beginRenderer()`, after `initDeclarations()` | renderer implementation registered |
  | `beginFrame()` | unchanged (the per-frame registration call is removed) |
  | `RiEnd` → end of `CRenderer::endRenderer()` | none |

  Later `RiBegin`/`RiEnd` sessions in the same process repeat this cycle.

## Routed lookup (`serviceLookup.{h,cpp}`, Shading)

| Kind | Input | Found | Not found / no services |
|---|---|---|---|
| DSO | shadeop name, prototype | `CDSO*` (cached by the renderer) | `nullptr` |
| Filter | filter name | `RtFilterFunc` | `RiGaussianFilter` |
| Step filter | filter name | `RtStepFilterFunc` | `RiGaussianStepFilter` |
| Global ID | category name | integer ≥ 1 | `0`; the caller skips the category |
| Variable | variable name | `CVariable*` | `nullptr` |

- **Warn-once state**: one process-wide flag per kind, set by the first lookup made with no
  services. Each flag emits exactly one `CODE_NOTSTARTED` warning. The flags are never reset.
- **Rules**:
  - With services installed, every lookup returns exactly what the renderer core returns today,
    unknown names included (filters fall back to Gaussian).
  - A category list never contains 0 except as its terminator.

## DSO handle (`CDSO`)

- **Moves from** `src/ri/render/renderer.h` (renderer core) **to**
  `src/libshader/shading/dsoHandle.h` (Shading). The fields are unchanged:
  - `handle` (`void*`)
  - `init` (`dsoInitFunction`)
  - `exec` (`dsoExecFunction`)
  - `cleanup` (`dsoCleanupFunction`)
  - `name` (`char*`)
  - `prototype` (`char*`)
  - `next` (`CDSO*`)
- The `dso*Function` typedefs stay in the published plug-in header `shadeop.h` (Shader DSO
  Operands), which `dsoHandle.h` includes and does not change.
- **Ownership**: unchanged. The renderer core allocates, caches (the `dsos` list) and frees DSO
  handles. The `.rslo` loader stores the pointer in each DSO instruction (`TCode::dso`), and the
  interpreter calls `exec(handle, n, args)`.

## Illumination hook

- **Source of truth**: `hasIlluminationHook()` on the services (`CRenderer::hiderFlags &
  HIDER_ILLUMINATIONHOOK` in the renderer implementation, set by the photon hider for its frame).
- **Read by**: both backends, through `CShadingContext::rendererIlluminationHook()`, once per
  execution of a construct's begin and of its end.
- **Hook calls** (virtual on `CShadingContext`; the photon hider overrides them): see
  [contracts/illumination-hook.md](contracts/illumination-hook.md).

## Photon-pass parity scene

- **Two RIBs**: `photon-parity.rib` (interpreter) and `photon-parity-slo.rib` (JIT). They are
  identical except for the light shader format and the file names. Frame 1 is a photon pass that
  writes `photon-parity[-slo].gpm`; Frame 2 is a raytrace render of the map.
- **One reference image**, rendered from the interpreter variant.
- **Map file**: deleted before every test run (ctest fixture), because an unmodified map is never
  written.
