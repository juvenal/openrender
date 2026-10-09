# Feature Specification: Common Infrastructure Floor

**Feature Branch**: `023-common-infrastructure-floor`
**Created**: 2026-10-08
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L1. Move generic infrastructure (diagnostics, the shared vocabulary, variable declarations, the global scratch-memory arena, and the generic reference-counting / file-resource / memory / statistics / sampling code) out of Scene Interface and the renderer core into the shared common layer, so every domain depends only on infrastructure for these services. Resolves registry entry DB-016 (GitHub #29) and the infrastructure part of DB-001 (#24).

## Clarifications

### Session 2026-10-08

- Q: What should "the shading library no longer depends on the renderer library for infrastructure" mean if the common layer becomes a shared library? → A: The infrastructure symbols may stay undefined in the shading library but must be resolved from the common library, never from the renderer library.
- Q: Must programs already compiled and linked against the current openRender libraries keep running when the new libraries are installed, without being rebuilt? → A: Yes, binary compatible: existing binaries keep running unchanged, and every moved symbol they use stays reachable from the library that provides it today.
- Q: With no RI context active, should the default sink handle each severity the way today's default RI error handler (`RiErrorPrint`) does? → A: Yes: errors to standard error, warnings to standard output, informational messages to standard output only at log level INFO or more verbose, and severe/fatal messages to standard error followed by process exit.
- Q: How much render-time slowdown is acceptable from moving the arena, statistics and diagnostics behind the common library? → A: At most 2% slower median render time on the existing performance scenes, measured before and after on the same machine.

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who build, maintain and extend openRender: contributors working on any domain, maintainers reviewing remediation specs, and developers who embed the renderer's libraries or use its command-line tools.

### User Story 1 - Every domain gets shared services from the common layer (Priority: P1)

A contributor working in any domain (Geometry, Graphics State, Hiders, Shading, Texturing, Scene Interface) reports a diagnostic, uses a renderer constant or RI token name, parses a variable declaration, or allocates scratch memory. Today each of these reaches into Scene Interface or the renderer core, which is why 215 cross-domain edges are registered under DB-016. After this feature, all of them come from the shared common layer, and the architecture check confirms that no domain depends on Scene Interface or the renderer core for these services any more.

**Why this priority**: It is the remediation the spec exists for. It removes the largest single registered violation, and every later remediation spec (024–038) builds on a domain-neutral infrastructure layer.

**Independent Test**: Run the architecture check on the finished tree. It passes with no allowlist line referencing DB-016, the registry lists DB-016 as Resolved, and no new allowlist line exists.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** it passes and reports no edge from any domain to Scene Interface or the renderer core for diagnostics, vocabulary, variable parsing or the scratch-memory arena.
2. **Given** the finished tree, **When** the allowlist is compared with the one before this feature, **Then** every DB-016 line is gone, every other removed line was reported stale by the check, and no line was added.
3. **Given** the shared infrastructure code itself, **When** the check runs, **Then** it depends on no domain (no infrastructure-to-domain edge remains).
4. **Given** a contributor adding a new diagnostic or scratch allocation in any domain, **When** they follow the documented headers, **Then** the new code depends only on the common layer.

---

### User Story 2 - The shading library gets its infrastructure from the common layer (Priority: P1)

A developer links or loads the shading library (`libshader_shading`). Today it resolves the reference-counting, file-resource, memory-page, statistics, sampling and diagnostic symbols at load time from the renderer library (DB-001). After this feature, those come from the common layer, so the shading library's dependency on the renderer library shrinks to the renderer-core service calls that spec 024 removes.

**Why this priority**: It is the infrastructure half of DB-001, the coupling that forces whole-archive static linking (#24). Removing it is a precondition for making the shading library self-contained in later specs.

**Independent Test**: On macOS and Linux, list the shading library's undefined symbols and the library each one binds to, and compare against the list measured before this feature.

**Acceptance Scenarios**:

1. **Given** the built shading library, **When** its undefined symbols and their providing libraries are listed, **Then** none of these binds to the renderer library: `CRefCounter` and `CFileResource` (constructors, destructors, typeinfo), `memoryInit`, `memoryNewPage`, `memoryTini`, `stats`, `sampleHemisphere`, `sampleCosineHemisphere`, `error`, `warning`, `info`, `parseVariable`, and the global scratch arena (today the renderer-core global-memory static). Each is either defined in the shading library or resolved from the common library.
2. **Given** the same list, **When** compared with the list before this feature, **Then** the only remaining renderer-library symbols are the renderer-core service calls and hider symbols registered under DB-001 for later specs.

---

### User Story 3 - Diagnostics work without an active RI context (Priority: P2)

A developer runs code that reports a diagnostic while no RI context is active: a command-line tool, a unit test, library code called outside `RiBegin`/`RiEnd`, or code running after the context has ended. Today that dereferences a null context and crashes. After this feature, the message is handled the way the default RI error handler handles it inside a context: errors go to standard error and the program continues, warnings to standard output, informational messages only at INFO verbosity, and a severe/fatal message ends the process after printing.

**Why this priority**: It removes a crash class and makes the shared diagnostics usable by every component, but it is secondary to the structural moves.

**Independent Test**: A test calls each diagnostic level with no RI context active and checks where each message goes and that only the fatal one ends the process (with its message printed), never by crashing.

**Acceptance Scenarios**:

1. **Given** no RI context is active, **When** code reports an error, **Then** the message is written to standard error and execution continues; a warning is written to standard output; an informational message is written to standard output only at INFO verbosity or above.
2. **Given** no RI context is active, **When** code reports a severe/fatal diagnostic, **Then** the message is written to standard error and the process exits with the same status the default RI handler uses, without crashing.
3. **Given** an active RI context with an error handler installed, **When** code reports a diagnostic, **Then** the handler receives exactly what it receives today: same code, severity and text, and the same rendered output.
4. **Given** a context that has ended, **When** a later diagnostic is reported, **Then** it is handled by the default sink rather than reaching the ended context.

---

### User Story 4 - Existing programs and embedders keep working (Priority: P2)

A user runs `orender`, `oshader`, `otexmake` or another tool, or a developer builds a program against the installed openRender headers and libraries (including the CMake package). Nothing they rely on changes: the RI API header declares the same tokens, the error-handler API behaves the same, and existing programs build and link as before.

**Why this priority**: The move is internal. Breaking the public RI API or the package would turn an internal cleanup into a user-facing regression.

**Independent Test**: Build and run the out-of-tree package consumer, and run the full test suite, before and after.

**Acceptance Scenarios**:

1. **Given** a program written against the installed RI API header, **When** it is rebuilt against the new installation, **Then** it compiles and links without source changes and every RI token has the same name and value.
2. **Given** a program already built and linked against the libraries installed before this feature, **When** the new libraries are installed in their place, **Then** it starts and runs without being rebuilt.
3. **Given** the full test suite, **When** it runs on the finished tree, **Then** results are unchanged and rendered images are byte-identical where they were deterministic before.
4. **Given** the out-of-tree CMake package consumer, **When** it is built and run, **Then** it works as before.

---

### Edge Cases

- **One copy of shared state per process:** the diagnostic sink, the global scratch arena and the statistics counters are process-wide. Several libraries link the common layer (renderer, vector, RIB output, shading, preview, tools). A program that loads more than one of them must still see one sink, one arena and one set of statistics, on macOS and on Linux. Two copies would silently split statistics or lose an installed sink.
- **Diagnostics from worker threads** and during static initialization or shutdown: they must reach the installed sink, or the default sink, without a race on sink installation.
- **Several RI sessions in one process** (`RiBegin`/`RiEnd` repeated, as in-process tests do): each session installs and removes its handler, and diagnostics between sessions go to the default sink.
- **Fatal diagnostics:** inside a context, keep today's behavior (the installed handler decides); with no context, print to standard error and exit with the default handler's status.
- **Variable declarations:** every declaration syntax accepted today is accepted, with identical results; invalid declarations are reported the same way.
- **Generated parser symbols:** the variable-declaration parser moving into the common layer must not clash with the other generated parsers (RIB, DSO prototypes, `if` expressions, shader language) when they are linked into the same program.
- **Build options:** builds without the JIT, or without optional image formats, still build and pass.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The common layer MUST provide the diagnostic service used by every domain: report an error, warning, severe/fatal or informational message with its error code. Where messages go MUST be configurable by installing a sink.
- **FR-002**: When no sink is installed (no active RI context), diagnostics MUST be handled the way the default RI error handler (`RiErrorPrint`) handles them today: errors are written to standard error and execution continues; warnings are written to standard output; informational messages are written to standard output only when the log level is INFO or more verbose; severe/fatal messages are written to standard error and the process exits, as it does today with that handler. No path may dereference a missing context. Scene Interface MUST install a sink that routes to the active RI context's error handler while a context is active, and remove it when the context ends.
- **FR-003**: With an RI context active, every diagnostic MUST reach the context's error handler exactly as today (code, severity, text), so tool output and rendered results are unchanged.
- **FR-004**: The common layer MUST provide the shared vocabulary: the renderer-core constants used across domains and the RI token constants. Every domain MUST take them from the common layer. The public RI API header MUST continue to declare every RI token with the same name and value, so programs written against it compile and link unchanged.
- **FR-005**: The common layer MUST provide the variable-declaration type and its parser, built from its grammar as part of the common layer. Every declaration accepted today MUST be accepted with identical results, and every rejected one rejected the same way.
- **FR-006**: The common layer MUST provide a scratch-memory arena object that replaces every direct use of the renderer core's global-memory static, with the same allocation semantics (stack-like begin/end regions and lifetime). No direct use of the static may remain outside the code that owns the arena.
- **FR-007**: The generic reference-counting, file-resource, memory-page, statistics and random/sampling code MUST be built as part of the common layer rather than the renderer library, so that the shading library resolves those symbols from the common library, never from the renderer library (they may remain undefined in the shading library).
- **FR-008**: Process-wide state of the common layer (the installed diagnostic sink, the global scratch arena, the statistics counters) MUST exist once per process, whichever and however many openRender libraries a program loads, on both supported platforms.
- **FR-009**: No forwarding headers: every include and call site MUST use the new common-layer location. The old Scene Interface and renderer-core declarations of the moved services MUST be removed, except where the public RI API header must keep declaring RI tokens (FR-004).
- **FR-010**: The architecture check MUST pass on the finished tree on macOS and Linux. Every allowlist line referencing DB-016 MUST be deleted. Any other line the move makes stale MUST be deleted. No allowlist line may be added. The domain map MUST classify every moved file by its new location.
- **FR-011**: The renderer-core usage metric baseline MUST be lowered to the new counts where usage drops (the global-memory references in particular), and MUST NOT rise.
- **FR-012**: The registry MUST be updated: DB-016 moves to Resolved with the commit and date; DB-001's evidence lists the symbols that remain after this feature; the sizes of any other entry whose edges changed are updated.
- **FR-013**: Adding this feature MUST NOT change rendering output or any existing test result; the full test suite MUST stay green on macOS and on Linux (GCC 13, warnings as errors).
- **FR-014**: The installed libraries and the CMake package MUST keep working for out-of-tree consumers, at the binary level: a program built and linked against the installed libraries before this feature MUST keep running against the new ones without being rebuilt, and MUST still build and link unchanged from source. Every moved symbol a consumer can use today (the RI tokens, the RI error-handler API, and any other symbol the installed headers expose) MUST stay reachable from the library that provides it today.
- **FR-015**: A test MUST cover diagnostics with no RI context (FR-002), and a test MUST cover that a diagnostic installed through one library is seen when reported from another (FR-008).
- **FR-016**: Rendering performance MUST NOT regress by more than 2% in median render time on the existing performance scenes (the six `perf-manual` scenes), measured before and after this feature on the same machine and build configuration.

### Key Entities

- **Diagnostic**: an error code, a severity (info, warning, error, severe/fatal) and a message.
- **Diagnostic sink**: where diagnostics go. The default sink applies the default RI handler's rules (streams per severity, log-level filtering of info, exit on fatal); Scene Interface installs one that forwards to the active RI context's error handler.
- **Shared vocabulary**: renderer-core constants and RI token constants that every domain may use.
- **Variable declaration**: a parsed `[class] [type] name[[n]]` declaration (storage class, type, array size, float count).
- **Scratch arena**: a stack-like region allocator for short-lived memory, with nested begin/end regions.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The architecture check passes on macOS and Linux with zero allowlist lines referencing DB-016 (down from 215 edges), DB-016 is listed as Resolved, and the allowlist has no line that was not in it before this feature.
- **SC-002**: None of the 17 infrastructure symbols listed in User Story 2 (the `CRefCounter` and `CFileResource` constructors, destructors and typeinfo, the three memory-page functions, `stats`, the two sampling functions, the three diagnostic functions, `parseVariable` and the global scratch arena) is resolved from the renderer library, on either platform; each is resolved from the common library or defined in the shading library itself.
- **SC-003**: With no RI context active, 100% of diagnostic calls in the test reach the stream the default RI handler would use for their severity; the process exits normally after error, warning and info calls, and exits with the default handler's status after a fatal call, never by a crash.
- **SC-004**: The full test suite passes on both platforms with the same results as before; deterministic renders are byte-identical. "Deterministic" means byte-identical across two runs of the baseline build; every other render is covered by the visual suite's existing thresholds.
- **SC-005**: The renderer-core usage metric drops by at least the number of global-memory references removed, and the baseline is lowered to the new values.
- **SC-006**: An out-of-tree program built against the libraries installed before this feature runs unchanged (without relinking) against the new installation; the out-of-tree package consumer also rebuilds and renders without changes, and a program using only the public RI API header compiles and links without changes.
- **SC-007**: On the six existing performance scenes, the median render time after this feature is at most 2% above the median before it, measured on the same machine with the same build configuration (Release).

## Assumptions

- Spec 022 (domain-dependency guardrails) is merged; its architecture check, allowlist, registry and metric are the yardstick for this feature.
- The common layer is the existing shared library target (`openrendercommon`, installed as `libRIBCommon`); how it is packaged to satisfy FR-008 is a planning decision.
- The renderer-core service calls the shading library still makes (`getDSO`, `getFilter`, `getStepFilter`, `getGlobalID`, `retrieveVariable`) and the hider symbols are out of scope: specs 024 and 029.
- The RI context, the global context pointer and the RI error-handler API stay in Scene Interface; only the diagnostic routing moves.
- Passing the arena to code explicitly (dependency injection into hiders and other domains) is not required; replacing direct access to the static is.
- The current diagnostic message formats are kept.
