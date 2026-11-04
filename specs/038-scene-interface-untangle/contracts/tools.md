# Contract: tools (FR-023, FR-024, US7) and the Procedural record (FR-008)

Research M9, M13, R5, R15.

## `precomp`

- New Hiders-owned constants header `src/ri/hiders/rasterFlags.h` (Q4 option (a)): the `RASTER_*`
  constants of `reyes.h:42-55` (`RASTER_MOVING` ... `RASTER_DEPTHFILT_MASK`), moved verbatim, no
  includes. `reyes.h` includes it in their place, so every hider source sees the same names.
  (`RASTER_DRAW_FRONT` and the rest of `reyes.h:57-73` stay in `reyes.h`; precomp does not use them.)
- `src/precomp/precomp.cpp:35-36`: `#include "ri/hiders/reyes.h"` and `"ri/render/renderer.h"`
  replaced by `#include "ri/hiders/rasterFlags.h"`. The printed text (`CRenderer::depthFilter ==
  DEPTH_MID`, `:399`) is a string and stays.
- `Precomp_OutputUnchanged`: runs the built `precomp` with its working directory in a scratch tree
  (`<work>/run`, writing `<work>/ri/hiders/stochasticPrimitives.h`, never the source tree) and compares
  the output byte for byte with `tests/precomp/reference/stochasticPrimitives.raw.h`, captured once
  from the unmodified tool (criterion (i) of Q4). Green on the starting tree by design.
- `Precomp_NoHiderCore` (`cmake -P`): `precomp.cpp` includes neither `reyes.h` nor any Core header.
  Red on the starting tree.
- The CONSUMER line `precomp.cpp -> src/ri/hiders/reyes.h` becomes `-> src/ri/hiders/rasterFlags.h`
  (a consumer may depend on anything; DB-024 records it as the constants header the tool needs).

Option (b) (`src/common/rasterFlags.h`) satisfies SC-009's "0 hider headers" literally but puts a
Hiders vocabulary in infrastructure; option (c) (local copies plus a consumer drift test) duplicates
ten constants.

## Data viewer (`src/ri/dataviewer/dataLoad.{h,cpp}`)

| Today | After |
|---|---|
| `CRenderer::initDeclarations()` (`dataLoad.cpp:165`) | `session = new CRiStandardSession()` |
| `renderMan = new CRiInterface()` (`:175`) | `sink = new CRiInterface(); riInstallContext(sink);` |
| destructor: `delete renderMan; renderMan = NULL; globalArena.tini(); CRenderer::shutdownDeclarations();` (`:141-144`) | `delete sink; globalArena.tini(); delete session;` |

`CDataDocument` gains the two members. The `identitym(CRenderer::fromWorld)` seeding (`:181-189`)
and the readers' Hiders/Texturing reach stay (DB-025, spec 032). In `ribRender` the data viewer used
the renderer's full declarations (coordinate systems and global identifiers too); its readers use
neither (`grep` over `texture/{brickmap,pointCloud,texture3d}.cpp`, `hiders/{photonMap,irradiance}.cpp`),
so the standard session is equivalent.

## `CDelayedData` (`src/ri/parse/delayedData.h`, Scene Interface)

```cpp
#include "common/global.h"   // vector, NULL, free (and common/algebra.h if needed)

class CDelayedData {      // moved verbatim from geometry/delayed.h:34-55
    public:
        CDelayedData();   // generator = helper = NULL
        ~CDelayedData();  // free(generator), free(helper)
        char *generator;
        char *helper;
        vector bmin, bmax;
};
```

`geometry/delayed.h` no longer defines it and does not include `delayedData.h`. Includers:
`parse/rib.y`, `parse/ri.cpp` (in place of `delayed.h`), `render/rendererProcedures.cpp`, and the
two tests `tests/unit/csg/test_procedural_operand.cpp`, `tests/unit/sampling/test_object_ids.cpp`
(one `#include` line each, TEST-APPROVAL).
