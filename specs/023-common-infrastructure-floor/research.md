# Research: Common Infrastructure Floor (spec 023)

All decisions below were made against master `010ce04`, using the worktree's pristine
Release build (`build/`). No `NEEDS CLARIFICATION` remains open.

## R0 — Edge inventory: every DB-016 line has a resolving decision

The allowlist has 215 DB-016 lines (75 include, 140 link). Each target maps to one decision:

| Target (lines) | Kind | Resolved by |
|---|---|---|
| `src/ri/parse/error.h` (45) | include | R3: `common/diagnostics.h` |
| `error` (30), `warning` (8), `info` (1) | link | R3: defined in `libRIBCommon` |
| `src/ri/parse/ri.h` (11) | include | R4: `common/riVocabulary.h` |
| `RI_*` tokens (87) | link | R4: defined in `common/riTokens.cpp` |
| `src/ri/render/rendererc.h` (10) | include | R4: moved to `common/rendererc.h` |
| `src/ri/parse/variable.h` (6) | include | R5: moved to `common/variable.h` |
| `parseVariable` (3) | link | R5: grammar built in `libRIBCommon` |
| `CRenderer::globalMemory` (9) | link | R6: `globalArena` in `libRIBCommon` |
| `src/ri/render/renderer.h` from `core/pl.cpp` (1) | include | R8: `pl` moves to Graphics State; the include is removed |
| `state/attributes.h`, `state/xform.h` from `core/pl.h` (2) | include | R8: become intra-domain |
| `CAttributes::findParameter` from `core/pl.cpp` (1) | link | R8: becomes intra-domain |
| `CRenderer::retrieveVariable` from `core/pl.cpp` (1) | link | R8: Graphics State-owned lookup hook |

Total: 45 + 39 + 11 + 87 + 10 + 6 + 3 + 9 + 1 + 2 + 1 + 1 = **215**.

Of the `ri.h` includers, only `state/attributes.cpp` and `state/options.cpp` use non-token RI
symbols: `RiBezierBasis` and `RiCatmullRomFilter`. Both are already *defined* in common
(`src/common/rslConstants.cpp`), so only their declarations need to move (R4).

The same moves also make **12 DB-001 lines** stale:

| DB-001 link target | Lines |
|---|---|
| `error` | 4 |
| `warning` | 5 |
| `info` | 1 |
| `parseVariable` | 1 |
| `CRenderer::globalMemory` | 1 |

DB-001 drops from 30 to at most 18 lines. The allowlist drops from 686 to at most 459, plus whatever else
the check reports stale. Nothing is added.

The spec-022 brief for 024 planned a renderer service for `parseVariable`. That service is no
longer needed: Shading may depend on common directly.

## R1 — Packaging: a shared `libRIBCommon`, the only definer of process-wide state

**Decision**:
- `src/common` builds one OBJECT library, `openrendercommon_obj` (PIC). Two targets are built from
  it:
  - **`openrendercommon`** becomes **SHARED**: `OUTPUT_NAME RIBCommon`, `VERSION`/`SOVERSION
    ${OPENRENDER_COMPAT_SOVERSION}`, the same scheme as the other `libRIB*` libraries.
  - **`openrendercommon_static`** (`OUTPUT_NAME RIBCommon`) is the archive installed today as
    `libRIBCommon.a`.
- The in-tree target name `openrendercommon` keeps naming the library that every in-tree
  executable, test and shared library links. So the ~50 existing
  `target_link_libraries(... openrendercommon)` calls switch to the shared library without edits.

**Linking rule (FR-008)**: a process gets common from exactly one image.
- No shared image embeds the static archive. The `$<BUILD_INTERFACE:openrendercommon>` embedding
  in `libshader_compiler`, `libshader_runtime` and `libshader_shading` becomes an ordinary link to
  the shared library.
- **`ribRender_static`'s interface links the *shared* `libRIBCommon`.** `libshader_shading` is
  shared-only and binds its infrastructure two-level to `libRIBCommon.dylib`. If a static consumer
  of `libRIBRender.a` also carried the static common archive, the process would hold two sinks, two
  arenas and two `stats` objects, with no error from the linker or loader.
- `ribVector_static`, `ribOut_static`, `libshader_compiler_static` and `libshader_runtime_static`
  stay fully static (`openrendercommon_static`). No shared openRender image is involved on those
  paths, so one copy is guaranteed.
- The `-force_load` / whole-archive workaround for `libRIBRender.a` stays. DB-001's remaining
  symbols (trace, bundles, `getDSO`…) still bind flat; spec 029 removes the workaround.

**Binary compatibility (FR-014, SC-006)**:
- **macOS**: `libRIBRender`, `libRIBVector` and `libRIBOut` each **re-export** `libRIBCommon`
  (`LINKER:-reexport_library,$<TARGET_FILE:openrendercommon>`; CMake 3.19 has no
  `LINK_LIBRARY:REEXPORT_LIBRARY` feature).
  - An existing binary records two-level bindings such as "`_RI_P` from `libRIBRender.1.dylib`".
    dyld then searches `libRIBRender` and the libraries it re-exports, so the binding still
    resolves.
  - Verified per library with `otool -l` (`LC_REEXPORT_DYLIB`) and `dyld_info -exports`.
  - Spike risk: ld64 may warn when the same dylib is both linked and re-exported. The fallback is to
    pass it only through the re-export option, plus `add_dependencies`. The first foundational
    task settles this.
  - **Spike result (T009, 2026-10-09)**: linking `openrendercommon` normally *and* adding
    `LINKER:-reexport_library,$<TARGET_FILE:openrendercommon>` works with no ld64 warning. All three
    RI libraries carry `LC_REEXPORT_DYLIB @rpath/libRIBCommon.1.dylib`, so no fallback is needed.
    Spelling out `-undefined,error` on `libRIBCommon` is deprecated in current ld64 (and is its
    default for dylibs), so only Linux passes `--no-undefined`.
- **Linux**: the RI libraries carry `DT_NEEDED libRIBCommon.so.1`. ELF resolves symbols through the
  global scope, so existing binaries find moved functions in `libRIBCommon`.
  - Copy relocations are by name, so an executable's copy of a moved data symbol (`RI_*`, `stats`)
    is filled from `libRIBCommon`.
  - Every library then binds to that single copy through the GOT. This needs no `-Bsymbolic` and no
    protected visibility, which is the current default.
- **Runtime search paths**:
  - `libRIBCommon` installs into the same lib directory as the RI libraries, so their existing
    `@rpath`/`$ORIGIN` settings find it.
  - The `orender-wire.app` bundle (`Contents/Frameworks`), the self-contained install and the
    release CI bundling step must also copy it.
- **CMake package**:
  - `openRenderTargets` gains the shared `openRender::RIBCommon`.
  - `openRenderStaticTargets` keeps the archive as `openRender::RIBCommon_static`.
  - The `Static` component requires the `Shared` component's `RIBCommon` (see the linking rule).

**No upward references (link-order rule)**: `libRIBCommon` is a shared library, so it must
resolve every symbol it uses from itself or the system.
- Enforced by linking it with `-Wl,-undefined,error` (the macOS default) and `-Wl,--no-undefined`
  on Linux.
- Measured on master: today's `libRIBCommon.a` has no reference outside common or the system
  libraries, so the packaging step links as is.
- Consequence for sequencing: the **diagnostics core** (`error`/`warning`/`fatal`/`info`, the sink
  slot, the default rules and `current_log_level`'s definition) must move **before**
  `stats.cpp` (`printStats` → `info`) and `fileResource.cpp` (→ `error`).
- `variable.y` doesn't constrain the order: its `varerror()` reporting is commented out, and it
  calls nothing else outside common.

**Display plug-ins**: the file, framebuffer, OpenEXR and RGBE display modules
(`src/display/{file,framebuffer,openexr,rgbe}`) keep linking the `openrendercommon` target, which
is now the shared library.
- They already call into common (`logging.hpp`, `os*`). With the static archive they would also
  pull `logging.o` and hold a private `current_log_level` that `orender -x` never sets.
- Their install-time rpath must find `libRIBCommon` in the lib directory. This is verified by an
  installed-tree render through a TIFF `Display` (quickstart §5).

**Rationale**: process-wide state (sink, arena, `stats`, tokens, log level) needs a single owner
on both platforms, whatever the combination of libraries loaded. Only a shared library guarantees
this on macOS: the two-level namespace gives every image that embeds an archive its own copy. Clarify
Q1 also requires the shading library to resolve the infrastructure symbols *from the common
library*, which only an explicit link to a shared `libRIBCommon` can show.

**Alternatives considered**:

| Alternative | Rejected because |
|---|---|
| Keep common static and rely on flat lookup | Binding becomes load-order dependent, the two-level namespace still duplicates state per image, and SC-002 becomes unmeasurable |
| Put the state in `libRIBRender` and keep common stateless | Contradicts US2: the shading library would keep resolving infrastructure from the renderer library |
| Forwarding stubs in the RI libraries | FR-009 forbids forwarding shims; re-export is a loader feature, not source code |

## R2 — One-definer check (FR-008 test)

**Decision**: add a new ctest, `Common_StateOwnership` (label `common`), run as a `cmake -P` script
over **every built image**:
- every shared library: `libRIB*`, `libshader_*`, display plug-ins;
- every executable: `orender`, `oshader`, `otexmake`, the tests.

For each process-wide symbol it requires **exactly one defining image, `libRIBCommon`**:
- `globalArena`, `stats`, `current_log_level`;
- the sink slot;
- every `RI_*` token;
- the moved functions (`error`, `parseVariable`, `memoryNewPage`, …).

Exception: for the static-consumer test executable (R10), the expected definer is still
`libRIBCommon`, because `ribRender_static` links the shared library.

Tools per platform:
- macOS: `nm -gU` on every image. Defined-only; re-exported symbols are not listed by the
  re-exporter. Mach-O has no copy relocations.
- Linux, shared objects: `nm -D --defined-only`.
- Linux, executables: GCC on Ubuntu builds PIE executables that use **copy relocations** for
  extern data. An executable that reads `RI_*`, `stats` or `current_log_level` therefore defines
  that symbol in its own `.bss`, and `nm` reports it as a definition, although the library binds
  to that copy (R1).
  - A definition in an executable counts as a reference when `readelf -rW` shows an `R_*_COPY`
    relocation for that symbol.
  - Any other definition in an executable is a violation.

**Rationale**: a structural, deterministic proof of FR-008 that doesn't depend on runtime timing,
and it catches any future target that embeds the archive. The runtime test (R10) complements it.

## R3 — Diagnostics: an installable sink in common

**Decision**: `common/diagnostics.{h,cpp}` (see contracts/common-api.md).

What moves to common:
- `EErrorCode` (verbatim from `parse/error.h`) and a new `EDiagnosticSeverity`
  (`INFO`, `WARNING`, `ERROR`, `SEVERE`).
- The `CDiagnosticSink` interface.
- `installDiagnosticSink()`, which returns the previous sink. The slot is a
  `std::atomic<CDiagnosticSink *>`, so worker threads read it without a lock.
- `error()`, `warning()`, `fatal()` and `info()` keep their exact signatures and mangled names. They
  format with `vsnprintf` into an `OS_MAX_PATH_LENGTH` buffer, as today, and dispatch to the
  installed sink. `nullptr` means the default sink.

**Default sink (FR-002, clarify Q3)**:
- `diagnosticsPrintDefault(severity, message)` implements `RiErrorPrint`'s stream rules once:
  - SEVERE: stderr, flush, `exit(-1)`;
  - ERROR: stderr;
  - WARNING: stdout;
  - INFO: stdout only when `current_log_level <= LogLevel::INFO`.
- `RiErrorPrint` (Scene Interface, public API) keeps setting `RiLastError` and then calls the same
  function, so the two can't drift.
- The default sink can't set `RiLastError`, because that is RI state. With no context there is no
  RI caller to read it.

**RI sink (FR-002/FR-003)**: Scene Interface keeps `translate()` (`EErrorCode` to `RIE_*`) and
gains `CRiDiagnosticSink`.
- `report()` reads `renderMan` on each call. If it is non-NULL, it calls
  `renderMan->RiError(translate(code), severityToRIE(severity), message)`, which is exactly today's
  call. If it is NULL, it delegates to `diagnosticsPrintDefault`.
- Installation is **idempotent**. There is one static RI sink and an `attached` flag:
  - `riDiagnosticsAttach()`, called from the `CRiInterface` constructor, does nothing when already
    attached. Otherwise it saves the pointer `installDiagnosticSink` returns, exactly once. Every
    context-creation path goes through the constructor: `RiBegin` factories, `RiContext`, the
    preview, and the nested-archive path at `ri.cpp:2489`.
  - `riDiagnosticsDetach()` runs where `renderMan` stops pointing at a live context, and restores
    the saved pointer.
    - The main point is `~CRiInterface` when `renderMan == this`, covering `RiEnd`, the data
      viewer and `RiArchiveEnd`'s nested delete. The other is the preview's explicit
      `renderMan = nullptr`.
    - `RiArchiveEnd` re-attaches after restoring the outer context; `RiContext` attaches.
    - The plan first said "only in `RiEnd`". T021 refined it, because `renderMan` also becomes
      NULL in those other three places.
  - Several contexts, `RiContext` switching and the `delete renderMan; renderMan =
    savedRenderMan;` path never save the RI sink as its own predecessor.
  - A sink installed before `RiBegin` is back after `RiEnd`. `DiagnosticsCrossLibrary` tests this
    case.
  - Diagnostics emitted from inside the context's destructor therefore still reach the context's
    handler, byte-identical to today (end-of-frame statistics are printed through `info()`).
  - A report arriving after teardown goes to the default sink (spec US3 scenario 4).

**Log level (advisor point 4)**: `current_log_level` is today an `inline` variable in
`src/includes/logging.hpp`.
- Merging it across images relies on weak-definition coalescing.
- Decision: declare it `extern` in the header and give it one definition in `libRIBCommon`
  (`common/logging.cpp`), so the default sink and `orender -x` read the same object
  deterministically.
- Every includer of `logging.hpp` already links common: `display/framebuffer`,
  `libshader/shading`, `orender`, `otexmake`, `precomp`, `ri/parse`, `ri/render`, and `sloinfo`
  (through `libshader_runtime`'s public link). So making it `extern` breaks no link.
- The R2 check covers it.

**Alternatives considered**:
- A function-pointer hook (`setErrorHandler`). Rejected: a sink object can carry state for the
  capture sinks the tests need.
- Keeping `renderMan` in common. Rejected: the RI context stays in Scene Interface (spec
  assumption).

## R4 — Vocabulary

**Decision**:
- `src/ri/render/rendererc.h` moves to **`src/common/rendererc.h`** via `git mv`, contents
  unchanged. This also resolves the back-edge from `common/colorSpace.h` and the
  `ifexpr_generated.cpp` edge.
- New **`src/common/riVocabulary.h`**, `extern "C"`, declares:
  - the `Rt*` typedefs and `RI_NULL`/`RI_TRUE`/`RI_FALSE`-style macros, character-identical to
    `ri.h`, so a translation unit that includes both compiles (C++ and C11 allow identical
    typedef/macro redefinition);
  - every `RI_*` token;
  - the standard basis matrices and filter functions that `common/rslConstants.cpp` already
    defines.
- New **`src/common/riTokens.cpp`** holds the token **definitions**, moved verbatim from
  `ri.cpp:51-372`. One definition per process (R1): tokens are compared by pointer in places, so
  identity matters.
- `src/ri/parse/ri.h`, the installed public header, **stays byte-identical** (FR-004). It keeps
  its own declarations, as FR-009's exception allows.
- New ctest `Common_VocabularyConsistency` keeps the two lists from drifting. It checks two things:
  - The token-name sets match across the `EXTERN(RtToken)` lines of `ri.h`, the declarations in
    `riVocabulary.h` and the definitions in `riTokens.cpp`. This check is a script.
  - A translation unit that includes both headers compiles.
- `rslConstants.cpp` switches from `ri.h` to `riVocabulary.h`.

**Alternatives considered**:
- Have `ri.h` include `riVocabulary.h` and install it flat. Rejected: it changes the public
  header's bytes and installed file set for no consumer benefit, and couples the public API layout
  to an internal header.

## R5 — Variable declarations and their grammar

**Decision**:
- `git mv` `src/ri/parse/variable.{h,y,l}` to **`src/common/`**.
- `src/common/CMakeLists.txt` runs `flex_file(variable.l lex.var.cpp var)` and
  `bison_file(variable.y variable_generated.cpp var)`. The macros are already included at the root,
  `CMakeLists.txt:157-158`.
- The generated sources land in `@build/src/common/`.
- `src/ri/CMakeLists.txt` drops both generator calls and `variable_generated.cpp` from the `ribOut`
  whitelist.
- The domain map:
  - removes the `@build/src/ri/variable_generated.cpp` and `@build/src/ri/lex.var.cpp` file lines;
  - adds `prefix @build/src/common/ Infra`.
- Unused includes go: `renderer.h` and `error.h` in `variable.y`. `variable.h` now includes
  `common/rendererc.h`.
- `VARIABLE_NAME_LENGTH` moves from `ri_config.h` into `variable.h` (R7).

Symbol hygiene:
- The `.l` file's hand-written globals (`boolwrap`, `init_lexical_parser`, `close_lexical_parser`,
  `found_mot`, `buf_state`) become `static` where they stay inside the lexer translation unit.
- Those called across the `.y`/`.l` boundary get a `var` prefix (`varInitLexer`,
  `varCloseLexer`).
- This matches the `var`-prefixed generated symbols and keeps them from clashing with the RIB, DSO,
  `if` and RSL parsers linked into the same process.

**Golden-table test (FR-005)**: new `Common_VariableDeclarations`.
- It parses every accepted declaration form (each class × type × array form) and a set of invalid
  ones, and compares every `CVariable` field and each error outcome with a table captured on the
  baseline before the move.
- It is written and passing *before* the move, against the old location. After the move only the
  include path changes, so its continued pass is the proof.

## R6 — The scratch arena

**Decision**: in `common/memory.h`:
- `class CArena { public: CMemPage *top; void init(); void tini(); }`, with a `constexpr`
  constructor so it is constant-initialized: no static-initialization-order hazard.
- Inline overloads `ralloc(int, CArena &)` and `rstrdup(const char *, CArena &)`.
- Macros `arenaBegin(a)`/`arenaEnd(a)`/`arenaSave(d, a)`/`arenaRestore(d, a)`, defined on top of
  the existing `memBegin`/`memEnd`/`memSave`/`memRestore` applied to `a.top`.
- One process-wide instance, **`extern CArena globalArena;`**, a plain exported global defined in
  `common/memory.cpp`.

Rationale for a plain global:
- A plain global keeps the hot `memBegin`/`ralloc` sequences in the hiders compiled exactly as
  today: one GOT load, then inline bump allocation. An accessor function would add a call per use.
- Injection beyond replacing the static is out of scope (spec assumption).

Every `CRenderer::globalMemory` use (≈135 in 23 files) becomes `globalArena` through these
helpers:
- 27 `memBegin`/`memEnd` pairs;
- 23 `ralloc`, 7 `rstrdup`, 3 `memSave`/`memRestore`;
- 4 `memoryInit`/`memoryTini` pairs, which become `globalArena.init()`/`tini()` at the same
  lifecycle sites: `renderer.cpp`, `ri.cpp`, `previewContext.cpp`, `dataLoad.cpp`.

Removed:
- `CRenderer::globalMemory` (`renderer.h:140`, `rendererStatics.cpp:106`).
- `CRendererServices::globalMemory()` and its implementation (`rendererServices.h:58`,
  `rendererServicesImpl.h:38`). Shading now uses `globalArena` directly, since Shading → Infra is
  allowed. This is a change to a Shading-owned interface, recorded in
  contracts/variable-lookup.md §2.
- The alias `#define threadMemory CRenderer::globalMemory` (`render/init.cpp:59`). FR-009 forbids
  forwarding aliases; its uses are written out against `globalArena`.

Allocation semantics are unchanged: same page sizes, 8-byte alignment and stack discipline.

**Alternatives considered**:
- RAII scope guards. Rejected: they would restore on early `return`, where today's macros don't,
  which is a behavior change.
- Renaming the static in place. Rejected: that leaves a `CRenderer` member, which is not an arena
  object.

## R7 — Generic `src/ri/core` code moves into common

**Decision**: `git mv` these files into `src/common/`, and remove them from `ri_sources` and from
the `ribOut` whitelist:
- `refCounter.{h,cpp}`
- `fileResource.{h,cpp}`
- `memory.{h,cpp}`
- `stats.{h,cpp}`
- `random.{h,cpp}`, which holds `sampleHemisphere`/`sampleCosineHemisphere`
- `atomic.h`, a dependency of `refCounter.h` and `stats.h`

**Includes**: every includer switches to `common/<name>.h`. Some files reach a header today by
bare name through the include path (e.g. `"memory.h"`); they switch too.

**`ri_config.h` split**: `ZONE_INIT_SIZE`/`ZONE_BASE_SIZE`/`ZONE_INCREMENT_SIZE` move into
`common/memory.cpp`, their only user. `VARIABLE_NAME_LENGTH` moves into `common/variable.h`;
`rendererFiles.cpp`'s use gets it through `variable.h`. Everything else stays in
`src/ri/core/ri_config.h`.

**Stay in `src/ri/core`** (not DB-016, not generic infrastructure for this spec):
- `map.h`, `ray.h`, `noise.h`, `ri_config.h` (remainder);
- `shader.h`, `shading.h`, `shaderPl.h` (Shading-mapped).

## R8 — `pl` belongs to Graphics State, with the variable lookup inverted

**Finding**: `src/ri/core/pl.{h,cpp}` (primitive parameter lists) is mapped as Infra, but it:
- includes `state/attributes.h` and `state/xform.h`;
- calls `CAttributes::findParameter`;
- reaches into the renderer core for `CRenderer::retrieveVariable` and `CRenderer::globalMemory`.

These are 10 of DB-016's lines (Infra 20).

**Decision**: `git mv` `pl.{h,cpp}` to **`src/ri/state/`**, i.e. Graphics State.
- The attributes, xform and `findParameter` edges become intra-domain.
- `globalMemory`, `RI_S`/`RI_T`, `error`/`warning` and `parseVariable` go to common (R3–R6).
- Includers are fine: Geometry (11 headers, Geometry → Graphics State allowed), the renderer core
  (allowed) and the preview (consumer). Texturing doesn't include it.

**Lookup inversion**: Graphics State → renderer core is forbidden, so `retrieveVariable` is
inverted through a Graphics State-owned hook (contracts/variable-lookup.md):
- `state/variableLookup.h` declares `using TVariableLookup = CVariable *(*)(const char *);`,
  `void installVariableLookup(TVariableLookup)` and `CVariable *lookupVariable(const char *)`.
- `CRenderer::initDeclarations()` installs `&CRenderer::retrieveVariable`, and
  `shutdownDeclarations()` uninstalls it. Both are core code built into `ribRender` and
  `ribVector`, so the preview path is covered.
- `pl.cpp`'s two calls use `lookupVariable()`. With nothing installed it returns `nullptr`, which
  `pl.cpp` already treats as "not declared": the inline-declaration path still works.

The declaration *registry* stays in the renderer core: moving `retrieveVariable` for its other
callers is spec 024 (spec assumption).

**Alternatives considered**:

| Alternative | Rejected because |
|---|---|
| Move `pl` into the renderer core | Its 11 Geometry includers would need new Geometry → Core allowlist lines (FR-010) |
| Add a lookup parameter to `parseParameterList` | 30 call sites to change for the same effect |
| Move the declaration registry into common | Out of scope; it is renderer state with frame lifetime |

## R9 — Domain map and metric

- `domain-map.txt`:
  - add `prefix @build/src/common/ Infra`;
  - drop the two `@build/src/ri/` variable lines.
  - `src/common/` is already Infra. `pl` moves under `src/ri/state/` (Graphics State) by prefix.
- Metric (`metrics-baseline.txt`, `CRenderer::` refs/distinct) is lowered to the measured values
  (FR-011):
  - Infra falls to 0/0, since `pl` takes its `CRenderer::` references with it and then drops them.
  - Every domain falls by its `globalMemory` references.
  - No domain rises. Graphics State stays at 0 because `pl`'s two `CRenderer::` uses are gone.

## R10 — Test strategy (constitution III: tests first)

Each test below is written first and fails (or, for the golden and consistency tables, passes on
the old code) before the change it guards.

| Test (ctest name, label) | Covers | First state |
|---|---|---|
| `Common_VariableDeclarations` (`common`) | FR-005 golden table | passes on baseline; must keep passing |
| `Common_VocabularyConsistency` (`common`) | FR-004 | fails until `riVocabulary.h`/`riTokens.cpp` exist |
| `Common_StateOwnership` (`common`) | FR-008, R2 | fails until the moves land |
| `Common_DiagnosticsNoContext_{error,warning,info,info_quiet,fatal}` (`common`) | FR-002, FR-015, SC-003 | fails (crash or symbol missing) until the sink lands; a `cmake -P` runner captures stdout, stderr and the exit status per severity, the fatal case expecting status `255` and its message on stderr |
| `Common_DiagnosticsCrossLibrary` (`common`) | FR-008, FR-015, spec edge cases | fails until the sink lands. Case 0: a capture sink installed before two consecutive `RiBegin`/`RiEnd` sessions receives reports before, between and after them, and is back in place after the last `RiEnd` (idempotent attach/detach). Case 1: inside `RiBegin`, a handler installed with `RiErrorHandler` receives `RIE_BADTOKEN` with `RIE_ERROR`/`RIE_WARNING`/`RIE_INFO` and the text, as today. Case 2: the `error` that `libshader_shading` binds to (`dlsym` on its handle) is `libRIBCommon`'s (`dladdr`), is the same address the test sees, and reaches the installed capture sink. Case 3: 8 threads report while the main thread swaps sinks; no report is lost and nothing crashes |
| `Common_DiagnosticsCrossLibraryStatic` (`common`) | advisor point 1 | cases 0 and 2, linked the way the package's `Static` component links (`ribRender_static` + force-load) |
| `Common_VariableLookupHook` (`common`) | contracts/variable-lookup.md §1 | fails (doesn't compile) until `state/variableLookup.h` exists; checks nullptr when nothing is installed, the fake lookup, and install/clear across `RiBegin`/`RiEnd` |
| `Common_ArenaSemantics` (`common`) | FR-006 | nested begin/end, save/restore and page growth on a local `CArena`; fails until `CArena` exists |
| `Common_ShadingInfraBinding` (`common`) | SC-002 | macOS: `nm -m libshader_shading` shows each of the 17 symbols as "(from libRIBCommon)" or defined locally, never "(dynamically looked up)". Linux: `readelf -d` lists `libRIBCommon.so.1`, and `nm -D --defined-only libRIBRender.so` defines none of the 17. Fails on baseline |
| `ctest -L architecture` | FR-010/011 | stale lines reported as each move lands; the allowlist is then pruned |
| full suite | FR-013, SC-004 | unchanged results; deterministic renders (byte-identical across two baseline runs, R11 item 2a) byte-identical |

The **binary-compatibility proof (SC-006)** and the **performance proof (SC-007)** need artifacts
from before the feature, so they are quickstart procedures with baselines captured in the first
setup tasks (R11, R12). They are not ctests.

## R11 — Baselines captured before any source change

The first implementation tasks run before any source edit, while `build/` still matches
master `010ce04`:

1. **Frozen baseline install**:
   - `DESTDIR=$S/023-base cmake --install build`, with `/usr/local` snapshotted before and after,
     as memory advises. `cmake --install --prefix` is still broken in self-contained mode (#23).
   - This install is both the "before" library set for SC-006 and the A side for interleaved
     performance runs.
2. **SC-006 consumer**: `specs/023-common-infrastructure-floor/tools/compat-consumer/` is a tiny C++
   program that:
   - against `libRIBVector`/`libRIBOut`, uses `RiBegin("out-<lib>-<tag>.rib")`, because
     `RiBegin(RI_NULL)` is unsafe against those two;
   - against `libRIBRender`, renders a 64×64 sphere to a TIFF through the installed file display
     driver, with one thread and one pixel sample, so the output is reproducible;
   - uses tokens, `RiErrorHandler(RiErrorPrint)`, `RiDeclare` and a sphere.

   It is built and linked against the baseline install **three times**, once against each of
   `libRIBRender`, `libRIBVector` and `libRIBOut`. Two-level bindings are recorded per library, so
   each re-export needs its own proof; the `otool` check shows the re-export exists, not that a
   binding resolves through it. The three resulting binaries are kept **unchanged** for the
   after-run.
   - The binaries are built in Phase 1 against `$S/023-inplace`, a copy of the frozen install, and
     each carries an rpath into it. `$S/023-base` stays untouched as the performance A side.
   - The after-run installs the new libraries **over** `$S/023-inplace` ("installed in their
     place", spec US4) and runs those same Phase 1 binaries with no `DYLD_*`/`LD_*` variables, so
     the loader's own resolution of the new `@rpath/libRIBCommon` dependency is what gets tested.
2a. **Determinism probe**: the baseline consumers run twice. Only outputs that are byte-identical
    between those two runs are compared byte for byte after the feature; any other output is
    compared with the visual-test block metric.
3. **Shading binding "before" lists**:
   - macOS: already captured at `$S/023-libshader-undef-before.txt`.
   - Linux: the commands are prepared for the user, run from master's `build-022` (see
     quickstart §1).
4. **Variable golden table**: captured by `Common_VariableDeclarations` against the old code.

**Loader isolation**: a DESTDIR install keeps the install-time rpaths, which point at the real
prefix.
- The compatibility consumers avoid this by carrying their own rpath into the install root, and
  run without loader variables (item 2); `run.sh --pinned` is a diagnostic aid only.
- Every other script that runs an installed binary (`render-installed.sh`, `perf_ab.sh`) pins the
  library directory: it exports `DYLD_LIBRARY_PATH` (macOS) or `LD_LIBRARY_PATH` (Linux; CMake
  emits `RUNPATH`, which `LD_LIBRARY_PATH` overrides) to `<root>/<prefix>/lib`.
- All scripts print the resolved libraries once (`DYLD_PRINT_LIBRARIES=1` / `ldd`), so a stray
  `/usr/local` install can't masquerade as either side.
- Each side uses its own shaders: `SHADERS` points at the install's shader dir (A) or
  `build/shaders` (B).

`$S` is the session scratchpad.

## R12 — Performance measurement (FR-016, SC-007)

`ctest -L perf-manual` reports slo/rslo ratios with a 1000× cap, so it gates nothing. SC-007
needs absolute medians.

**Script**: `specs/023-common-infrastructure-floor/tools/perf_ab.sh` covers the six perf-manual
scenes in both variants: 12 RIBs from `examples/rib/tests/sphere-*-reyes{,-slo}.rib`.
- Each RIB runs **interleaved A/B**: baseline `orender` from the frozen install, then the new
  `orender`, 7 rounds each.
- Runs are serial, under `nice -n 19`, with the machine otherwise idle. Interleaving cancels the
  fanless M2's thermal drift.
- It reports the median wall time per RIB and side, plus the after/before ratio.
- **Pass**: every scene's median ratio ≤ 1.02.

**Interpretation of SC-007**: the gate applies to each of the 12 RIBs (every scene, in both its
`.rslo` and `.slo` variants), which is stricter than one median per scene. A RIB above 1.02 is
re-measured once with 7 more rounds before it counts as a failure (T056).

The Release build configuration is the same on both sides.

**Expected effect**: one extra GOT indirection for cross-image data such as `globalArena`, `stats`
and the tokens, and stub calls for `ralloc`'s page refill and the diagnostics. Neither is in a
per-sample loop.

## R13 — Linux validation

The user runs it manually. A new build directory `build-023` goes on the Linux host
(`/home/juvenal/Projects/Development/CLI/openrender`), with GCC 13 and `-Werror`. The procedure
mirrors spec 022:
- an rsync block, a checksum manifest and a command block;
- `ctest -L architecture`, `ctest -L common`, then the full suite, compared against the known 9
  pre-existing non-architecture failures;
- the Linux SC-002/SC-006 checks from quickstart.

## R14 — Registry and documentation

- `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`:
  - DB-016 moves to **Resolved** with commit and date.
  - Its Direction text is corrected: it said "forwarding headers for one spec", which contradicts
    FR-009.
  - DB-001's Evidence and Size list only the lines that remain (≤ 18).
  - Other entries whose sizes change get updated.
- `CLAUDE.md`, *Known gotchas*, gets a new item covering:
  - the one-definer rule for `libRIBCommon`;
  - the `ribRender_static` → shared-common rule;
  - the re-export;
  - the reason `ri.h` keeps duplicate token declarations, guarded by
    `Common_VocabularyConsistency`.
- `DEVNOTES.md`'s status table also gets an entry.
- `INSTALL.md`/`COMPILING.txt` list the new `libRIBCommon` shared library.
