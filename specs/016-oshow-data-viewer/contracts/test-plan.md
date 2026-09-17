# Contract: Test Plan (Constitution Principle III)

Every test below is written and confirmed **failing** before its corresponding implementation
lands (Red), matching the constitution's Test-First Process. All live under `tests/preview/`
using the existing `add_preview_test` macro (`LABELS "preview"`, `TIMEOUT 30`, hand-rolled
`CHECK`/pass-fail counters — no external test framework, matching spec 006's precedent). None
require a display, a GPU context, `ORENDERHOME`, or a full render.

| Test | Pins | Precedes (implementation) |
|---|---|---|
| `test_dataview_chunking` | Tail-flush correctness at exactly `chunkSize`, `chunkSize±1`, `0`, and `1` emitted primitives — the `if (j != chunkSize) draw(...)` idiom repeated across six `src/ri/` files. Also pins the corrected `CPhotonMap`/`CPointCloud` loop-bound parity (research.md §2). | `dataView.{h,cpp}` (I1) |
| `test_debugdump_roundtrip` | Write via `CDebugView`'s own point/line/triangle/quad writers, reopen, count via a test sink. Asserts: quad → exactly 2 triangles; the **last written record is emitted exactly once** (pins the `feof` fix, research.md §1). | `dataView.cpp`'s debug-dump parser (I1) |
| `test_data_disk_expand` | Exactly 60 vertices per disc (20 segments); every rim vertex within `dP + ε` of `P`; every triangle's plane normal parallel to `N`; **no NaN when `P == (0,0,0)` or `P ∥ N`** (pins the axis-picking basis fix, research.md §1). | `diskExpand.{h,cpp}` (I4) |
| `test_data_pointcloud` | Construct a `CPointCloud` via its write constructor, `store()` a known set of points, destroy (flush), reopen through `CDataDocument::open()`, assert type, point count, bounds, and channel names match what was written. Also settles empirically whether `retrieveDisplayChannel` (`texture3d.cpp:119`) is reachable on this read path (research.md §2). **Also covers FR-005**: repeats the round-trip with zero points stored, asserting the reopened document is valid (not an error) with all counts `== 0`. | `dataLoad.{h,cpp}` (I3) |
| `test_data_world_init` | After `CDataDocument::open()` on a constructed photon map fixture, the view's transform is non-degenerate (not the zero matrix) and `bound()` returns finite values — pins the identity-matrix seeding step (research.md §2). | `dataLoad.{h,cpp}` (I3) |
| `test_data_keys` | Drive `ribdata_key()` with each legacy key (`m l b d p q w`) against a brick-map and a point-cloud fixture; assert the corresponding `DataSceneC` field changes (`detailLevel`, `drawMode`, `currentChannel`) and the sink's primitive mix changes accordingly. Also asserts **zero bytes written to stdout** during the whole sequence (captures `fileno(stdout)`), making FR-016 mechanically checkable. | `dataSink.{h,cpp}`, `CDataView` accessors (I3/I4) |
| `test_wire_cli` | `wireCli`'s argument grammar (contracts/cli-interface.md): every exit code 1–5 reachable with the right input; `--json` output is well-formed JSON containing `schemaVersion` and every required key for each `documentType`. **Also covers FR-008 and SC-004**: asserts the synthesized `camera` in a data document's JSON output actually frames the reported `bounds` (non-degenerate, bounds fall within the implied view volume), and wraps the `--json` invocation against a small fixture with a wall-clock assertion under the SC-004 threshold (2 s). | `wireCli.{h,cpp}` (I3) |
| `test_preview_subdiv` | Each of the six retargeted RIB scenes (`examples/rib/tests/**/*-wire.rib`) tessellates through `libribpreview` with a non-zero vertex count and finite bounds — `libribpreview` has no subdivision-surface coverage today. | Retargeted fixtures (I2), exercised once `wireCli`/headless loading exists (I3) |

## Fixture strategy (no render required for any of the six data types)

| Data type | Fixture source |
|---|---|
| Debug-geometry dump | `CDebugView`'s own writer, called directly in the test |
| Point cloud | `CPointCloud`'s write constructor + `store()`, per research.md §2 |
| Photon map | `CPhotonMap::store()` + `write()` (both public, `photonMap.h:96-101`) |
| Irradiance/gather cache | `CIrradianceCache` opened with `CACHE_WRITE`, then reopened `CACHE_READ | CACHE_RDONLY` |
| Brick map | `CBrickMap`'s write constructor + `store()`, flushed on destruction (research.md §3 — resolved during planning; no committed binary fixture needed) |

## What stays manual (`quickstart.md`)

GUI-only behavior that has no automated test in this codebase's existing infrastructure (no
screenshot-diff or GPU-readback harness exists, and building one is out of scope — research.md
§11). See `quickstart.md` steps 3–7 for the authoritative, runnable list — window sizing and
framing, orbit/pan/zoom feel, menu items firing, first-responder keyboard behavior, visible disc
orientation, absence of z-fighting, the Save Camera dialog, launching from Finder and the `bin/`
symlink, and Wayland vs. X11 — rather than restating it here, so the two documents cannot drift
independently. The one manual timing check not covered by `test_wire_cli`'s automated SC-004
assertion above is **SC-006** (the ~1M-primitive, <5s large-file case): quickstart.md step 6
times this by hand, since generating a million-primitive fixture in a 30 s-timeout unit test is
disproportionate to what it would prove.
