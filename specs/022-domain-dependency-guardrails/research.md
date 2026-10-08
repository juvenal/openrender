# Research: Domain-Dependency Guardrails (spec 022)

Every decision below was checked against the tree at master `1f456ea` / branch head `f589eb3` (the main checkout's Unix Makefiles build, macOS arm64), unless marked otherwise.

## R1. Implementation language for the check

- **Decision:** a small C++20 command-line tool, `archcheck`, built under `tests/architecture/`. It shells out only to the platform `nm`.
- **Rationale:**
  - Every existing test in the repo is C++ or a CMake script; Python is not a build or test dependency today (it only appears as the installed `prman.py` binding).
  - Principle V (minimal dependencies) and Principle II (C++20) both favour C++.
  - The tool's work is file parsing plus one external process per object batch, which is easy in C++20 with `std::filesystem`.
  - Demangling uses `abi::__cxa_demangle` (in both libc++ and libstdc++), so no `c++filt` dependency.
- **Alternatives considered:**
  - **Python 3:** faster to write, but adds an interpreter requirement to every test run (CI included) for one test.
  - **A CMake `-P` script:** no dependency, but parsing ~250 depfiles of ~300 lines each and 21k `nm` lines in CMake's string language would be slow and hard to unit-test.

## R2. Source-level edges: direct includes, resolved through compiler depfiles

- **Decision:** parse each file's **direct** `#include` directives textually, then resolve each one against the depfile of a translation unit that actually compiled it. An include is counted only if the resolved file appears in that TU's depfile.
- **Rationale:**
  - The Makefiles generator writes one `<obj>.o.d` per object (248 found). Each lists every header the TU reached, **transitively**.
  - Using that list directly as edges would misattribute: `blobby.cpp` would "depend on" `renderer.h` only because `object.h` includes it. The real violation is the `object.h → renderer.h` edge, and the allowlist (one entry per concrete edge, clarification Q1) must name it exactly once.
  - Resolving direct includes against the depfile set does three things:
    - it handles the flat include path, where many directories are on `-I`, so a bare `"renderer.h"` needs the TU's actual resolution;
    - it handles ambiguous basenames (`shader.h`, `shading.h`, `noise.h`, `shaderPl.h` exist in both `ri/core` and `libshader/shading`);
    - it drops includes in inactive `#if` branches (Windows, `HAVE_OPENEXR`, …), because those never reach the depfile.
  - Headers get edges too: a header's direct includes are resolved against the depfile of any TU that includes the header.
- **Ninja:** Ninja folds depfiles into `.ninja_deps`. The tool reads them through `ninja -t deps`, which prints the same "object: deps…" shape. Makefiles stays the primary path, since it is what CI and local builds use.
- **Unresolvable headers:** a header that no compiled translation unit includes can't be resolved, so it yields no edges. It's listed under `UNRESOLVED` in the report for information, and never fails the run.
- **Unsupported generators** (anything but Unix Makefiles / Ninja) are an environment error (exit 2).
- **Path normalization:** depfiles contain the real path (`/Users/juvenal/Projects/...`) while the checkout may be reached through `/Volumes/...`. All paths are canonicalized (`std::filesystem::weakly_canonical`) before comparison.
- **Alternatives considered:**
  - **Pure textual include resolution** (emulating `-I` order): fragile with the flat include path and conditional includes.
  - **Transitive depfile edges:** rejected for misattribution (above).
  - **`clang-scan-deps`:** an extra dependency, and not available with every GCC toolchain.

## R3. Link-level edges: per-object `nm`, attributed through the domain map

- **Decision:** run `nm -g -A` (batched) over every object file of the project's non-test targets. Build two maps: symbol → defining source files, and source file → undefined symbols. A link edge is `referencing source file → symbol`, attributed to the domain(s) of the defining file(s).
  - Leading `_` is stripped on macOS, so the Itanium-mangled key is the same on both platforms.
  - Demangled names are used for display only; the mangled name is the key.
- **Rationale:**
  - Library-level analysis (`dyld_info` on the dylib) can't attribute edges, because one object library (`ribRender_obj`) mixes six domains.
  - Object level gives the source file, hence the domain, on both platforms, with one portable tool (`nm` from Apple/LLVM or GNU binutils).
  - This refines the clarification's "library → symbol" wording into "source file (inside a library) → symbol". It is strictly more precise, and the edge stays concrete.
  - Measured cost: 2.2 s for all 248 objects / 21,495 `nm` lines.
- **Attribution rules:**
  - Ignore a reference when any defining file is in the referencing file's own domain or in Infra (local or shared-infrastructure definitions are benign).
  - Ignore a reference when nothing in the project defines the symbol (system or third-party libraries).
  - Attribute to every distinct domain among the remaining definers. This surfaces link-time substitution pairs (`*Dispatch.cpp` vs `*DispatchStub.cpp`) as the violations they are.
  - Sources compiled into several object libraries (`ribRender_obj`, `ribVector_obj`, `ribOut_obj`) are de-duplicated by source path.
- **Alternatives considered:**
  - **`dyld_info -fixups`:** macOS only, library granularity.
  - **`readelf`/`objdump`:** platform-specific output formats.
  - **Reading the linker map:** generator- and linker-specific.

## R4. Domain map: path prefixes, classified by location, with few file overrides

- **Decision:** a line-based, comment-friendly map (`tests/architecture/domain-map.txt`): longest-prefix rules plus per-file overrides. Files are classified **by location**, so misplaced code shows up as a violation instead of being hidden by reclassification. Overrides exist only where the spec requires them:
  - forwarding shims in `src/ri/core` (`shader.h`, `shading.h`, `shaderPl.h`, `noise.h`) belong to **Shading**, so forwarding cannot hide a Shading edge;
  - plug-in ABI headers: `src/ri/render/shadeop.h` → Shader DSO Operands; `src/ri/geometry/dlo.h` and `src/ri/geometry/implicit.h` → Geometric Procedures;
  - generated parser sources in the build tree → their grammar's domain: `rib_generated.cpp`, `dso_generated.cpp`, `variable_generated.cpp` and `ifexpr_generated.cpp` → Scene Interface; the `rslo` grammars → Shading.
- **Initial classification:**

  | Location | Class |
  |---|---|
  | `src/common`, `src/includes`, `src/ri/core` (minus shims) | Infra |
  | `src/ri/parse`, `src/ri/output` | Scene Interface |
  | `src/ri/state` | Graphics State |
  | `src/ri/geometry` | Geometry |
  | `src/ri/hiders` | Hiders |
  | `src/ri/texture` | Texturing |
  | `src/libshader/**` (compiler, runtime, shading, include) | Shading |
  | `src/display/**` | Display Outputs |
  | `src/ri/render`, `src/ri/vector`, `src/ri/hcshader.*` | Core (composition root) |
  | `src/ri/dataviewer`, `src/orender`, `src/oshader`, `src/otexmake`, `src/precomp`, `src/preview`, `src/sloinfo`, `src/rslo`, `src/python`, `src/lua` | Consumer |
  | `tests/**` | not scanned |

- **Rationale:** a reviewable single source of truth (FR-002). An unmatched file under `src/` fails the check and is named.
- **Alternatives considered:** classification through CMake target properties. Rejected because object libraries mix domains (`ribRender_obj`), so targets can't be the unit.

## R5. Allowed-dependency rules as data, verified acyclic

- **Decision:** `tests/architecture/domain-rules.txt` encodes the constitution v1.2.0 table:
  - Core → any;
  - any → Infra;
  - Infra → none;
  - Consumer → any (consumer reach into internals is still recorded, see R6);
  - the plug-in domains → their own ABI header + Infra.
  The tool checks at start-up that the rule graph (Core and Consumer excluded) is acyclic, and refuses to run otherwise.
- **Plug-ins and Infra:** in-tree plug-in implementations (display drivers) use `src/common`, which is linked statically into them and is not a host dependency. The constitution's "only their own published ABI header" restricts what a plug-in may need *from the host*. So plug-ins may also depend on Infra, and on nothing else. The audit found the display drivers compliant under that reading.
- **Cycles:** because the allowed graph is a DAG, any cycle needs at least one disallowed edge, and an un-allowlisted disallowed edge already fails. The tool still computes the strongly connected components over all edges (allowlisted ones included) and **reports every domain cycle with its path** (FR-006). It fails only when a cycle contains a non-allowlisted edge, so the cycles caused by today's registered violations are reported but don't fail.
- **Which cycles are listed (decided in convergence, T070):** for every class-to-class step that is not purely allowed, the report lists the shortest cycle through it, rotated to start at its alphabetically first class and de-duplicated. A second, longer cycle through the same steps is not listed separately. Every failing step therefore appears in at least one listed cycle marked `[FAILING]`, so failure detection is complete, while the report stays bounded (at most one cycle per pair of classes) instead of enumerating every elementary cycle of a dense graph.
- **Alternative considered:** hard-coding the table in C++. Rejected so the table is reviewable next to the constitution text it mirrors.

## R6. Consumers and the "tools reach into internals" finding (V21)

- **Decision:** consumers may depend on anything, so they never fail the check. For V21's handful of tool → internal-header edges, the tool emits a separate "consumer reach" report section (no allowlist needed). V21 is registered with that evidence.
- **Rationale:** the constitution doesn't make tools a domain. Failing on them would be inventing a rule. Recording them keeps V21 visible.

## R7. Allowlist format, platform tags, and the registry link

- **Decision:** one line per concrete edge (clarification Q1), optional platform tag (Q4):
  ```
  DB-NNN  include  <file>  ->  <resolved header>            [macos|linux]
  DB-NNN  link     <file>  ->  <mangled symbol>  # <demangled>  [macos|linux]
  ```
  Paths are repo-relative. The `# demangled` comment is informational and ignored by the parser.
- **Validation:**
  - A referenced `DB-NNN` must exist as an **Open** entry (`### DB-NNN` heading above `## Resolved`) in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. Unknown or resolved IDs fail (FR-009).
  - An entry applies to this run when its platform tag matches, or it has none. An applicable entry is **stale** (fail, FR-010) when its `from` file was compiled in this build and its edge is absent, or present but now allowed.
  - **Not built:** an applicable entry whose `from` file wasn't compiled in this build is skipped and counted as not built, never stale. This covers configuration-dependent code (JIT off, OpenEXR off).
    - For `include` entries, "compiled" means the file appears in at least one translation unit's depfile.
    - For `link` entries, it means the file was compiled into an object.
  - Entries tagged for the other platform are ignored (FR-010a).
  - The same edge listed both tagged and untagged is a configuration error.
- **Bootstrap:** `archcheck --emit-allowlist` prints today's violating edges in this format with `DB-???` placeholders. It is used once in this spec, with IDs assigned while writing the registry entries; later specs only ever delete lines.
- **Alternatives considered:**
  - **JSON/TOML:** JSON can't carry comments, and TOML needs a parser (Python 3.11 `tomllib` or a library).
  - **Glob or pattern entries:** rejected by clarification Q1.

## R8. Renderer-core usage metric

- **Decision:** scan source text (`.cpp`, `.h`, `.y`, `.l`) of every non-Core, non-Consumer file, with comments and string literals stripped, for `CRenderer::<member>` and `CRendererContext::<member>`.
  - Per domain, report the **reference count** and the **distinct-member count** (clarification Q3).
  - Compare against `tests/architecture/metrics-baseline.txt`.
  - An increase prints a warning naming the domain, metric and delta, and does not fail (FR-012).
  - A decrease prints a reminder to lower the baseline.
  - Output is deterministic: sorted, no timestamps (SC-007).
- **Rationale:** this matches the audit's counting (Hiders ~611, Texturing ~115, …), so later specs can quote comparable numbers. Generated sources are skipped in favour of their `.y`/`.l` grammars, to avoid double counting.

## R9. Where the check runs (CI) and how it is wired into ctest

- **Decision:**
  - `add_test(NAME Architecture_DomainBoundaries …)` with `LABELS architecture`, running `archcheck --source-dir … --build-dir … --config tests/architecture`.
  - It runs in every default `ctest` invocation, including the three release-workflow jobs (Ubuntu, macOS x86_64, macOS arm64), which already run `ctest` after a full build.
  - **No workflow edits are needed**: the check rides on the existing `ctest` steps.
  - Missing tooling (`nm`, or `ninja` under a Ninja build) makes the test fail with an explicit message, never skip silently.
- **Ordering:** the test reads object files and depfiles, so it needs a completed build. That is the normal state when `ctest` runs after `cmake --build`. If objects are missing, the tool fails with "build tree incomplete: <target>" rather than under-reporting.

## R10. Testing the tool itself (Principle III: TDD)

- **Decision:**
  - **Fixture trees** under `tests/architecture/fixtures/<scenario>/`: tiny `src/` layouts with their own domain map, rules, allowlist and mini registry. They are compiled by CMake into OBJECT libraries, so they have real depfiles and real `nm` symbols.
  - **One ctest per scenario** (label `architecture`), each asserting exit code and message via `PASS_REGULAR_EXPRESSION` / `WILL_FAIL`. Scenarios:
    - clean pass;
    - forbidden include;
    - forbidden link reference;
    - new cycle, and a cycle made only of allowlisted edges (reported, passes);
    - incomplete build tree (environment error);
    - stale entry, and an entry whose edge is now allowed (stale);
    - unknown registry ID;
    - resolved registry ID;
    - unclassified file;
    - tagged entry on the other platform;
    - entry whose file isn't compiled (not built);
    - same edge tagged and untagged (configuration error);
    - metric-increase warning, and metric-decrease note;
    - forwarding-shim attribution.
  - **Parser unit tests** in a C++ test executable: depfile, include directive, allowlist line, registry heading, domain-map rule.
  - Tests are written first and fail before the tool exists.
- **Scan scope:** archcheck only considers objects and depfiles whose source file lies under `<source-dir>/src/` (or matches an `@build/` rule of the domain map). Fixtures pass `--source-dir tests/architecture/fixtures/<name>` and so see only their own objects, sharing the one build tree. No extra CLI option.
- **Rationale:** these are exactly SC-001's injection trials, automated and repeatable, without ever modifying production sources.

## R11. Registry population and tracking issues

- **Decision:**
  - Re-measure V1–V21 with the tool's own report (edges grouped by source domain → target domain and by file) plus targeted greps for the non-edge findings (V6 duplicated effects, V9 behaviour mismatch, V11 ABI, V15, V16, V17).
  - Write DB-005… entries in `DOMAIN_BOUNDARIES.md`, in the same shape as DB-001…DB-004.
  - Overlaps with DB-004 cross-reference it: V8's Geometry part, V3/V6's Geometry → core parts, and V12's `CRiInterface` in `instantiate()`.
  - Findings no longer present are recorded in `specs/022-…/registry-reconciliation.md`, with the reason.
  - Tracking: one GitHub issue per remediation spec (023–038; 16 issues, clarification Q2). These are filed **only after the maintainer approves at the implement pause**, and each registry entry links its spec issue(s).
- **Known deltas to re-measure:**
  - spec 021 added the `surfaceCrossingDispatch` pair (5 Dispatch/Stub pairs now) and `CGeometryHost`, which partly inverts the Geometry → core edge DB-004 covers;
  - the hcshader/friend state after 021.

## R12. Vestigial includes (V19)

- **Decision:** remove each include the audit flagged, one at a time:
  - `options.cpp` → `texture.h`;
  - `attributes.cpp` → `irradiance.h`;
  - `shading.cpp` → `raytracer.h`, `remoteChannel.h`, `brickmap.h`, `pointCloud.h`, `points.h`;
  - `pointCloud.cpp` → `object.h`, `raytracer.h`;
  - `reyes.cpp` → `points.h`;
  - any others the tool shows to be unused cross-domain includes.

  After each removal: a clean build with `-Werror` on both compilers (Clang on macOS, GCC 13 on Linux; Linux is a merge prerequisite, see caveat), then the full suite.
  - If the build breaks: the include is genuinely needed, so restore it and register it as a real violation.
  - If the build passes: removal is behaviour-neutral, since an unused include changes no code generation. That is confirmed by the visual suite staying identical.
- **Caveat:** a header may be needed only for a macro or inline. Build success on both compilers is the arbiter. Linux/GCC 13 validation follows the existing rsync + Maestri path (project memory) and is a merge prerequisite (spec US4 #1, FR-010a). If the Linux path is unavailable at that pause, the phase stays open (not ready to merge) until it is.
