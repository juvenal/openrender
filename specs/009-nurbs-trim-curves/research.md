# Phase 0 Research: NURBS Trim Curves (RiTrimCurve)

## R1: Thread-level parallelism for polyline flattening / crossing tests

**Decision**: Do not use `std::execution::par`/`par_unseq` parallel STL algorithms, and do not introduce new
thread-level parallelism (`std::jthread`, manual work partitioning) inside `CPatch::dice()` or
`CTesselationPatch::tesselate()`. Keep the per-vertex trim crossing test and the per-loop polyline flattening
sequential, written as a plain contiguous-array loop that the compiler can auto-vectorize (see R2).

**Rationale**: Two independent facts close this off:

1. **Toolchain fact, verified on this machine** (`c++ --version` → Apple clang 21.0.0, arm64-apple-darwin25.5.0,
   libc++): `__cpp_lib_parallel_algorithm` is **not defined**, and compiling
   `std::for_each(std::execution::par, v.begin(), v.end(), ...)` fails with `error: no member named 'par' in
   namespace 'std::execution'`. Apple Clang's libc++ does not ship the C++17/20 Parallel STL execution policies at
   all (no partial "accepted but sequential" fallback — it's a hard compile error). Since macOS is one of this
   project's two target platforms (Constitution Principle VI) and openRender has zero existing dependency on a
   PSTL backend (no oneTBB/oneDPL in the build), `std::execution::par` is not portably usable without adding
   exactly the kind of new third-party dependency Constitution Principle V disfavors. This makes the "standard
   library only" parallel-STL approach originally scoped for this feature non-viable on macOS as written.
2. **Architectural fact**: `CPatch::dice()` and `CTesselationPatch::tesselate()` do not run on the main thread —
   they execute inside one of the renderer's `numThreads` worker threads, dispatched via
   `osCreateThread(rendererDispatchThread, ...)` (`src/ri/renderer.cpp:1210`; the equivalent server path is
   `renderer.cpp:1180`). The renderer is therefore already thread-parallel at the bucket/ray-batch level for every
   surface these two functions touch. Adding a second layer of thread-level parallelism *inside* a per-surface trim
   test would nest parallelism inside an already-saturated thread pool — pure oversubscription, not a speedup. This
   is the same class of hazard already documented as Known Gotcha #6 in this project's `CLAUDE.md`
   ("Multi-threaded raster early-outs are dangerous... under multithreading, another thread could inject into the
   active queue"): the bucket-parallel dispatch is a shared invariant that new code inside `dice()`/`tesselate()`
   must respect, not fight.

Given both facts, thread-level parallelism is the wrong tool for this feature's actual hot path. The place where
"modern C++20 parallelism" pays off here is data-parallel, not thread-parallel: keeping the flattened-polyline and
per-vertex classification data contiguous and branch-light so the compiler auto-vectorizes it (R2), which composes
correctly with the existing bucket-thread model instead of contending with it.

**Alternatives considered**:
- *`std::execution::par` parallel STL, as originally scoped*: rejected — does not compile on this project's macOS
  toolchain (libc++ gap, verified above); would require introducing oneTBB/oneDPL as a new dependency to fix,
  which Constitution Principle V disfavors for a feature whose actual per-surface workload doesn't need it.
- *Manual `std::jthread` partitioning of the per-vertex crossing test inside `dice()`*: rejected — `dice()` already
  runs inside a bucket worker thread; spawning child threads there oversubscribes `numThreads` and risks the same
  class of concurrency hazard flagged in Known Gotcha #6. Per-`NuPatch` polyline flattening (a handful to a few
  hundred edges, once per surface) is also too small to amortize thread creation cost even if nesting were safe.
- *GCC's libstdc++ parallel algorithms (TBB-backed) on Linux only, sequential fallback on macOS*: rejected as a
  platform-conditional code path — Constitution Principle VI requires Linux/macOS to behave the same way absent a
  documented reason, and a silently-sequential-on-macOS-only "parallel" algorithm is a maintenance trap, not a
  real capability.

## R2: SIMD / auto-vectorization approach

**Decision**: No SIMD intrinsics library (no xsimd, no hand-written SSE/AVX/NEON). New trim-curve data — the
per-loop flattened polyline vertex array and the per-span control-point arrays consumed to build it — are stored as
contiguous, POD `float`/`double` arrays, laid out the same way `src/common/algebra.h`'s existing `vector`/`matrix`
typedefs are (plain C arrays, no padding), so the per-vertex crossing-test inner loop is a simple, branch-light,
compiler-auto-vectorizable loop over contiguous memory. `src/common/algebra.h` itself is **not modified** by this
feature — its typedefs (`vector` at `algebra.h:31`, `matrix` at `algebra.h:34`, `dvector`/`dmatrix` at
`algebra.h:36,39`) back every primitive, grid, and shader buffer in the renderer; adding `alignas` or otherwise
changing their layout would be a global, unbounded blast-radius change and would directly risk FR-004's
byte-for-byte untrimmed-rendering guarantee. Any alignment/layout work this feature does is confined to the new,
feature-local polyline/control-point buffers only.

**Rationale**: The project already builds with `-O3 -DNDEBUG` and no explicit vectorization flags; auto-vectorization
of simple loops over contiguous data is the compiler's default behavior at `-O3` and requires no new build-system
work or dependency. The existing architecture-detection macros in `src/common/align.h`
(`OPENRENDER_ARCH_X86_64`/`OPENRENDER_ARCH_ARM64`/`OPENRENDER_CACHE_LINE_SIZE`, `align.h:55-68`) are available if a
specific new buffer benefits from cache-line-aware sizing, but this is an optional micro-optimization, not a
functional requirement — no trim behavior may depend on which architecture branch is active.

**Alternatives considered**:
- *xsimd or a similar portable SIMD wrapper library*: rejected per Constitution Principle V — the workload (a
  per-`NuPatch`, once-per-surface polyline flattening, and an O(edges) crossing test) does not need guaranteed
  explicit vectorization to meet SC-007's "no measurable regression" bar; auto-vectorization of a simple loop is
  sufficient, and adding a dependency for it would be unjustified.
- *Hand-written SSE2/AVX2/NEON intrinsics behind `align.h`'s arch macros*: rejected for v1 — real added complexity
  (three code paths to maintain and test) for a workload that hasn't been shown to need it. Left as a named
  follow-up optimization opportunity, not built now.

## R3: GPU trim classification

**Decision**: No GPU compute implementation in this feature. Document, in prose only, where a future GPU-backed
batch trim classifier could plug in.

**Rationale**: openRender has no existing GPU compute infrastructure — the only GPU-adjacent code in the repository
is an unrelated Metal *rasterization* shader for the macOS-only wireframe preview tool
(`src/preview/orender-wire-macos/Sources/Shaders.metal`), a different tool with a different purpose (interactive
viewport display, not offline shading/geometry). Building GPU compute infrastructure from scratch to accelerate a
per-surface classification test whose CPU cost is O(polyline edges) per vertex, amortized once per `NuPatch` mesh,
is disproportionate to this feature's scope and would violate the additive/minimal-footprint intent of the spec.
Per FR-010, the Shared Trim Test is already the single, hider-agnostic classification step consulted identically by
`CPatch::dice()` and `CTesselationPatch` — that single seam is exactly where a future GPU-backed batch classifier
(operating on many surfaces' polylines/vertex grids at once) would need to intercept, since it's the one place every
hider's tessellated vertices already funnel through. This plan records that seam as a forward-looking note (see
`data-model.md`'s Shared Trim Test entity and `contracts/shared-trim-test-contract.md`) rather than building an
abstraction for it now.

**Alternatives considered**:
- *Build a minimal GPU compute abstraction now (e.g. a compute-shader classification kernel) so a future backend has
  something to extend*: rejected — no second concrete use case exists yet to validate the abstraction's shape, and
  this project's own convention (CLAUDE.md: "Don't design for hypothetical future requirements") argues directly
  against it. A documentation note at the correct seam is lower-risk and equally useful to a future implementer.

## R4: Forward-looking note for Constructive Solid Geometry (CSG)

**Decision**: Do not generalize the Shared Trim Test into an abstract/reusable classification interface now.
Document, in prose only, that FR-010's shared classification seam is the same architectural seam a future CSG
boolean-classification test would need.

**Rationale**: A second concrete consumer (CSG boolean classification) is expected but does not exist yet. Building
an abstraction (e.g. an `IClassificationTest` interface) ahead of that second use case is premature — this project's
established convention (both the constitution's minimal-dependencies spirit and the standing "don't design for
hypothetical future requirements" project rule) treats a single-consumer abstraction as complexity added on
speculation, not evidence. Instead, `data-model.md`'s Shared Trim Test entity and `contracts/shared-trim-test-
contract.md` explicitly call out that this seam — a single hider-agnostic `(u,v)` classification test, consulted
identically by `CPatch::dice()` and `CTesselationPatch` before either produces tessellated vertices — is the
precedent a future CSG spec/plan should find and reuse, so that work doesn't have to rediscover it from scratch.

**Alternatives considered**:
- *Introduce an abstract classification interface now, with `TrimClassifier` as its sole implementation*: rejected
  for the same premature-abstraction reason as R3 — no second implementation exists to validate the interface
  shape, and CSG's actual classification needs (solid-vs-solid boolean tests in 3D, vs. this feature's 2D
  parameter-space test) are different enough that guessing the shared interface now risks getting it wrong and
  having to break it later anyway.

## R5: Trim curve storage representation (`CAttributes` pending state → `CNURBSPatchMesh` owned state)

**Decision**: `RiTrimCurve` stores its arrays (`ncurves`, `order`, `knot`, `min`, `max`, `n`, `u`, `v`, `w`) as a new
heap-owned "pending trim loops" field on `CAttributes`, mirroring the shape `CRibOut::RiTrimCurve`
(`ribOut.cpp:1115-1154`) already expects to serialize. `CNURBSPatchMesh::create()` (`patches.cpp:1823-1891`) reads
this pending state once when a mesh is constructed, builds the Shared Trim Test (flattened polylines + validated
loop set) from it, and stores the built test on the mesh — not a copy of the raw arrays — so every per-Bezier-span
child (`patches.cpp:1874`) references one mesh-owned test rather than re-flattening or re-validating per span.

**Rationale**: FR-002/FR-003 require `TrimCurve` to behave exactly like every other attribute (push/pop via
`AttributeBegin`/`AttributeEnd`, replaced or cleared by a subsequent `TrimCurve` call) — `CAttributes` is the only
existing home for that semantics, confirmed by the existing (currently inert) `VALID_ATTRIBUTE_BLOCKS` dispatch for
`RiTrimCurve` at `ri.cpp:1629-1636`. FR-009 requires the mesh, not each span, to own the global knot range and trim
data, since spans map local `(u,v)` into the mesh's global range before classification. Building the Shared Trim
Test at mesh-construction time (rather than lazily on first dice/intersect) keeps FR-018's "amortized at
surface-setup time" cost paid exactly once, in one place, regardless of how many hiders or motion samples later
consult it.

**Alternatives considered**:
- *Store raw trim arrays on the mesh and flatten lazily on first `dice()`/`intersect()` call*: rejected — makes the
  "amortized once per surface" guarantee (FR-018) depend on which hider touches the surface first, and risks
  double-flattening if both the REYES and ray-tracing paths race to build it (the renderer is multi-threaded per
  R1). Building eagerly at `create()` time, which already runs once per mesh, avoids the race entirely.
- *Store trim data per-span instead of per-mesh*: rejected outright by FR-009 — would duplicate the loop data across
  every Bezier span and require re-deriving the global-to-local parameter mapping per span instead of once.

## R6: Malformed-loop warning deduplication key (FR-020)

**Decision**: Key the "already warned" set by the `CNURBSPatchMesh`'s Shared Trim Test identity (i.e., the mesh
object that owns the built/validated trim data), not by `NuPatch` call site or render-time instance. Because
`ObjectInstance` shares one underlying geometry definition (and therefore one `CNURBSPatchMesh`) across every
instance that references it, deduplicating at the mesh level naturally satisfies FR-020 ("once per distinct
malformed loop definition, not once per instance") without needing a separate hash table keyed on loop contents —
identity of the already-validated mesh is a strictly cheaper and equally correct key, since validation happens
exactly once at `CNURBSPatchMesh::create()` time (R5) regardless of how many instances later reference that mesh.

**Rationale**: Validation (closure check, weight check) happens once, at mesh-construction time, per R5 — so the
warning is naturally emitted at most once per mesh already, with no extra bookkeeping needed beyond emitting it
inline during that one-time construction rather than at dice/intersect time (which *would* run once per instance
per hider and require explicit dedup). This is recorded explicitly in `data-model.md` because it is the detail that
makes FR-020 implementable, not just describable.

**Alternatives considered**:
- *Hash the loop's control-point/knot data and dedup against a process-wide seen-set*: rejected as unnecessary
  complexity — two distinct `NuPatch` calls with identical trim data are, per FR-020's own wording ("distinct
  malformed loop definition"), different definitions and should each warn once; mesh-identity-based dedup already
  gives exactly that without a hashing scheme.

## R7: Untrimmed baseline scene source

**Decision**: Promote `geometry/vase.rib` (11-line `NuPatch` surface-of-revolution definition, currently an
`ObjectBegin`-wrapped snippet with no `Display`/`WorldBegin`, not registered in `tests/visual/CMakeLists.txt`) into
a standalone renderable scene under `examples/rib/tests/nupatch-vase-untrimmed.rib` by wrapping it with the
`Display`/`WorldBegin`/camera boilerplate already used by other `add_visual_test` entries, with no changes to the
`NuPatch` call itself. The trimmed/sense/multi-loop scenes (User Stories 1, 3, 5) reuse the same wrapped vase body
plus a `TrimCurve` call, so all four scenes share one known-good untrimmed geometry definition and differ only in
trim state.

**Rationale**: Directly satisfies User Story 4 / SC-002's requirement for an untrimmed-`NuPatch` regression baseline
captured before any trim code lands, using the one existing NURBS example already in the repository rather than
authoring a new surface from scratch. Reusing the same body across all four new scenes keeps the trimmed-vs-
untrimmed comparisons (Acceptance Scenario 2 of User Story 1) meaningful — differences in the rendered image are
attributable only to trim state, not to a different underlying surface.

**Alternatives considered**:
- *Author a new, simpler NURBS surface (e.g. a flat trimmed plane) instead of the vase*: rejected — the vase is
  already validated repository content with non-trivial knot multiplicity (order 3, repeated internal knots),
  which better exercises FR-008's full-knot-range domain assumption than a trivial flat patch would, and reusing it
  avoids introducing an entirely new geometry definition to maintain.
