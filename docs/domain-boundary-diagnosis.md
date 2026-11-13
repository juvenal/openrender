## Diagnosis

**Yes—openRender can have enforceable domain boundaries, but namespaces alone will not provide them.** Your remediation plan establishes the necessary interfaces; it does **not yet establish the hard wall you want**.

Given your clarified requirements—resisting deliberate source/build bypasses, coordinated rebuilds acceptable, and protected enforcement policy allowed—I recommend:

> **Separate domain libraries, API-only build inputs, hidden implementation symbols, and protected dependency enforcement.** Use namespaces for organization; consider C++20 modules later.

This can prevent unauthorized dependencies from entering accepted builds. It cannot make native code sharing one process a security boundary against arbitrary casts, memory access, or malicious execution.

## What the current code shows

### 1. Most domains still share one compilation boundary

`src/ri/CMakeLists.txt:196` builds `ribRender_obj` from State, Geometry, Hiders, Texturing, Scene Interface, and renderer-core sources.

The same file:

- Adds implementation directories through directory-wide `include_directories()` calls, lines 16–56.
- Publishes those directories to consumers through `ribRender`’s `PUBLIC` include paths, lines 231–244.
- Repeats broad visibility for `ribVector` and `ribOut`.

**Consequence:** physical directories exist, but most domain boundaries are not compiler or linker boundaries.

Splitting this into per-domain OBJECT targets would improve dependency accounting, but **would not create independent link barriers** if all objects ultimately enter the same library.

### 2. The architecture checker enforces domain direction—not interface exclusivity

In `tests/architecture/archcheck/graph.cpp:140–145`, an edge is accepted when the source and destination **domains** satisfy the rule table.

It does not distinguish:

- A published Geometry interface.
- A private Geometry implementation header.
- An internal Geometry function that happens to be externally linkable.

For example, Hiders → Geometry is permitted. The checker therefore does not, by itself, enforce “Hiders may use only Geometry’s declared API.”

**This is the biggest enforcement gap relative to your request.** Even an empty violation allowlist would not establish interface-only access.

### 3. Several existing interfaces are good foundations

The current source already contains useful building blocks:

| Area | Evidence | Assessment |
|---|---|---|
| Texturing | `src/ri/texture/textureSystem.h:101–127` | Abstract service, opaque handles, explicit factory: a strong starting boundary. |
| Geometry host services | `src/ri/geometry/geometryHost.h:57–83` | Correct dependency inversion: Geometry owns what it asks its host to implement. |
| Graphics State | `src/ri/state/stateHandles.h` | Handles remove concrete shader/map ownership dependencies. |
| Shading callbacks | `tracer.h`, `pointSource.h`, `rendererServices.h` | Established contracts for upward capabilities without linking back to implementations. |
| Shading library | `src/libshader/shading/CMakeLists.txt:66–109` | Separate shared library, strict undefined-symbol linking, no renderer library dependency. |
| External plug-ins | `orplugin.h`, published family headers | Existing C ABI convention can remain the external extension mechanism. |

These make the proposal practical. This is **not** an architecture that must be replaced wholesale.

However, some handles expose underlying pointers, and State still exposes substantial mutable representation through `CAttributes`. Those are design choices that need explicit classification as either supported API or implementation detail—not automatically proof of isolation.

### 4. Implementation symbols are not systematically hidden

I found no domain-wide hidden-visibility/export-list configuration in the inspected CMake configuration.

**Consequence:** putting code in separate shared libraries is insufficient by itself. A consumer can potentially redeclare an implementation function and link to it if that symbol remains exported.

You need an explicit distinction between **linkable API symbols** and **non-exported implementation symbols**.

### 5. JIT lookup is another touchpoint that must be covered

`src/libshader/shading/llvmJit.cpp:96–106` installs a current-process symbol search generator.

That is separate from the old unresolved-library-symbol issue that spec 029 addressed.

**Consequence:** an investigation limited to C++ includes and ordinary library links misses a runtime symbol-resolution route. A complete boundary design must restrict JIT resolution to its declared runtime services rather than broadly searching process symbols.

### 6. Enforcement is currently optional during development

The domain checker is registered with `add_test()` in `tests/architecture/CMakeLists.txt:35–39`; it is not a mandatory normal-build gate.

The repository’s build/test workflow, `.github/workflows/release.yml:3–6`, runs on version tags. There is no checked-in pull-request build workflow among the two workflow files present.

Remote branch protection was not inspected, so I cannot claim anything about its configuration.

## Which mechanisms would actually help?

| Mechanism | What it provides | Enough for your requirement? |
|---|---|---|
| Namespaces | Ownership and naming clarity | **No.** Namespaces can be reopened and names qualified. |
| Private headers and CMake include paths | Cleaner compilation dependencies | **No, alone.** Relative/absolute includes bypass search-path restrictions. |
| Per-domain OBJECT/static libraries | Build organization and dependency accounting | **No, alone.** Final linking can still resolve implementation references. |
| C++20 named modules | Language-level control over exported declarations | **Useful, not sufficient.** They do not protect editable build rules or establish runtime isolation. |
| Shared domain libraries with controlled exports | Linker rejection of references to hidden implementation symbols | **Yes, as one layer.** |
| Isolated builds plus protected policy | Makes unauthorized sources/headers unavailable and rejects policy bypasses | **Essential for the resistance you requested.** |

### Why I would not start with C++20 modules

They are feasible, but would add toolchain migration before resolving the architectural gaps.

Your project currently declares CMake 3.19, and the existing build uses Unix Makefiles and AppleClang. CMake’s documented named-module workflow uses newer module support, supported compiler combinations, and supported generators such as Ninja. The documented GCC scanning support starts at GCC 14, above the repository’s documented GCC 13 baseline.

**Modules should be an optional implementation technique after the boundaries exist—not the prerequisite for getting them.** See [CMake’s module support documentation](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html).

## Recommended hard-wall design

### 1. Publish a deliberately small API for each domain

Separate:

- Boundary interfaces and value types.
- Factories and registration hooks.
- Private implementation headers.
- Concrete implementation classes.

Dependent domains receive **only the approved API**, not another domain’s implementation directory.

An API header must not transitively include private implementation headers. Keep conversions such as `shaderInstanceOf()` inside the owning implementation or a specifically approved adapter.

Because coordinated rebuilding is acceptable, internal boundaries can remain C++. **There is no need to convert every internal interface into a stable C ABI.** Keep the versioned C ABIs for external plug-ins.

### 2. Build domains independently

Each domain should compile using:

- Its own implementation sources.
- Its own API.
- The APIs of permitted dependencies.
- Approved infrastructure and generated headers.

For deliberate-include resistance, removing `-I` entries is insufficient: **other domains’ implementation files must be unavailable in the enforced build environment**. That means isolated source staging or sandboxed filesystem access—not simply separate output directories.

The build driver and staging policy must be protected, rather than allowing arbitrary domain build code to access the full checkout.

### 3. Establish real link boundaries

For the enforced configuration, use separate shared libraries with:

- Hidden visibility by default.
- Explicit API exports.
- Strict undefined-symbol rejection.
- Controlled dependency lists.
- Export/import inspection against approved manifests.

Apply visibility to the targets that actually compile the objects, not only their final shared-library wrappers.

Public abstract interfaces and opaque implementations work well here. Preserve bulk/grid operations so boundary crossings do not unnecessarily become per-point virtual calls.

Static builds can remain supported, but **must not be presented as providing the same independent linker wall**.

### 4. Enforce both permissions

A dependency must satisfy **both**:

1. “This domain may depend on that domain.”
2. “This use crosses through an approved interface.”

Extend the existing checker rather than discarding it. Cover implementation-header access, inappropriate concrete types in APIs, exported mutable globals, and dynamic lookup routes.

Protect the rule table, API manifests, build driver, and legacy exceptions. Comparing exceptions against an approved baseline is important: the current checker detects stale exceptions, but does not alone prove that a change has not added new ones.

### 5. Make enforcement mandatory before acceptance

Run protected enforcement on every pull request and accepted build—not only release tags.

Keep negative tests proving the walls work, including attempted:

- Relative inclusion of a private header.
- Redeclaration and linking of a hidden function.
- Forbidden target dependency.
- Unauthorized JIT symbol lookup.
- Expansion of the violation allowlist.

The tests verify the wall; **they are not the wall itself**.

## How this changes your existing plan

**Keep the remediation chain. Add an explicit enforcement deliverable rather than assuming V20 is solved by narrower include paths and OBJECT targets.**

Priority order:

1. **Define API/private classifications and protected policy now.**
2. **Pilot the wall around Shading**, which already has an independent shared-library boundary.
3. Extract State and Texturing as independent targets with API-only consumption.
4. Complete the Geometry/Hider untangling in spec 031 and GI separation in spec 032 before claiming those domains are sealed.
5. Audit shared infrastructure state and the separately tracked arena/stats injection work; moving mutable globals into Common does not eliminate shared-state coupling.
6. Integrate future Transport under the same enforced rules when its constitution amendment lands.

## Validation caveat

I ran the existing architecture checker against `build/`; it reported 18 violations. **That is not a reliable current-source result:** the generated parsers and sampled object files predate their corresponding sources, some by weeks.

I therefore did not treat those findings as new regressions or rebuild the active tree. This diagnosis is based on current source and configuration; fresh-build validation remains outstanding. No source changes were made.

**Bottom line:** your interface-remediation direction is sound. To reach the stronger requirement, add **API-only isolated compilation, restricted shared-library exports, controlled runtime resolution, and protected mandatory policy**. Namespace migration alone—or per-domain OBJECT targets alone—would leave the central loopholes open.
