# Contract: allowlist, metrics and registry end state

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Requirements**: FR-019 to FR-021, FR-025; SC-008, SC-009, SC-011.
**Baseline**: the starting tree's `tests/architecture/allowlist.txt`, read at the plan stage. Phase 1
re-takes it with `archcheck` on this worktree's macOS build and on the Linux per-spec tree, and
stores the reports as `tools/archcheck-base-{macos,linux}.txt`.

The allowlist only shrinks. **No line is added, re-keyed or swapped.** Each phase deletes its own
lines first; `ctest -L architecture` must then report exactly those edges as `VIOLATION` until the
phase's code lands.

## Lines deleted

**DB-006, 2 include lines** (the phase that moves the executor's reads behind `objectAttributes()`
and the offending-object upcast behind `objectHandle()`):

```text
DB-006  include  src/libshader/shading/execute.cpp  ->  src/ri/geometry/surface.h
DB-006  include  src/libshader/shading/executeMisc.cpp  ->  src/ri/geometry/surface.h
```

`DB-006  include  src/libshader/shading/shading.cpp  ->  src/ri/geometry/object.h` **stays**:
`shade()` takes the primitive until spec 029, and `objectHandle()`/`objectAttributes()` live in that
unit. DB-006 goes from 33 to 31 lines (24 include, 7 link).

**DB-001, 3 link lines** (the tracer phase; Q1):

```text
DB-001  link     src/libshader/shading/executeMisc.cpp  ->  _ZN15CShadingContext5traceEP4CRay   # CShadingContext::trace(CRay*)
DB-001  link     src/libshader/shading/executeMisc.cpp  ->  _ZN15CShadingContext7traceExEP10CRayBundle   # CShadingContext::traceEx(CRayBundle*)
DB-001  link     src/libshader/shading/shading.cpp  ->  _ZN15CShadingContext7traceExEP10CRayBundle   # CShadingContext::traceEx(CRayBundle*)
```

DB-001 goes from 8 to 5 lines: the two bundle vtables, `CGatherBundle::CGatherBundle()`, and the two
`CPhotonMap::lookup` lines. The flat-namespace imports go from 6 to 4.

**DB-005**: no line changes (8). **DB-018**: it has no lines.

None of these lines carries a platform tag. The Linux baseline report confirms that the same five
lines exist there.

## Registry edits (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)

- **DB-018** (stays Open):
  - Evidence: the eight builtins now ask `getAttribute`/`getMessage`, and none reads
    `currentObject->attributes` (spec 026, <the date the registry task is done>).
  - Still open: the JIT's message-passing entry points (`op_*_param`, `op_lightsource_f`, spec 033);
    `shadername(type)` (Q8, a direct read, null-guarded by FR-027); group-layer messages and `setmessage` (spec 034); the `lightsource()`
    backend divergence (filed, #115); and the unknown destination size of a float-class query (filed, #121).
  - Direction: drop the parts that are done.
- **DB-006** (stays Open):
  - Size 33 -> 31 (26 include -> 24, 7 link). History line: "Spec 026, <date>: execute.cpp and
    executeMisc.cpp no longer include `surface.h` (the executor's attribute reads go through
    `CShadingContext::objectAttributes()`); `shading.cpp -> object.h` stays for spec 029. The
    `ShaderGlobals` contract exists (`shaderGlobals.h`); nothing fills it yet."
  - Evidence: "Shading includes `object.h` (one file)".
- **DB-001** (stays Open; edited per Q1, decided 2026-10-30):
  - Evidence: 4 imports (`CGatherBundle`, the two bundle vtables, `CPhotonMap::lookup`).
  - Size 8 -> 5.
  - History line: "Spec 026, <date>: Shading's trace calls go through the Shading-owned `ITracer`
    (`tracer.h`), reached through `CRendererServices::tracer()` and answered by the hiders' adapter
    `CHiderTracer`; `trace`/`traceEx` are no longer imported. The record is
    `specs/026-shading-contract-v2/tools/libshader-imports-{base,after}-macos.txt`."
  - Direction: name `ITracer` as the interface the bundles and the photon map follow (spec 029).
- **DB-005**: not edited.
- **No entry moves to Resolved.** Each of DB-001, DB-005, DB-006 and DB-018 keeps lines or findings.
- **No text refers to anything that exists only after the merge** (FR-020, FR-025).

## Metrics (`tests/architecture/metrics-baseline.txt`)

- **Expected**: no change. Shading reads 0 `CRenderer::` references today and gains none.
  `CHiderTracer` references no `CRenderer::` member. The new core references
  (`CRendererServicesImpl`, `frameTracer`) belong to the core, which the metric's per-domain rows do
  not count.
- **If the final report differs**, the baseline is set to the measured values with a comment in the
  file's house style. It is never raised to silence a warning.

## Allowlist header counts

The section comments are updated to the new counts:

- `# --- DB-001: ... (8) ---` becomes `(5)`;
- `# --- DB-006: ... (33) ---` becomes `(31)`.
