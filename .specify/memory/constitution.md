<!--
Sync Impact Report:
Version change: 1.1.0 → 1.2.0 (MINOR: new principle + new review duty; no obligation removed)
Modified principles: None renamed or redefined
Added sections: VIII. Domain Separation and Modular Boundaries
Modified sections: Development Workflow → Code Review Requirements (domain-boundary review duty)
Removed sections: None
Templates requiring updates:
  ✅ .specify/templates/plan-template.md - Constitution Check gains the Principle VIII gate
  ✅ .specify/templates/spec-template.md - reviewed, no change needed (domain naming is a plan concern)
  ✅ .specify/templates/tasks-template.md - reviewed, no change needed
  ✅ .specify/templates/checklist-template.md - reviewed, no change needed
  ✅ .specify/templates/agent-file-template.md - reviewed, no change needed
  ✅ .claude/skills/speckit-*/SKILL.md - reviewed, no principle-number references to update
New artifact: DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md - violation registry required by VIII rule 8
Follow-up TODOs:
  - Implement the CI domain-dependency check required by VIII rule 7 (registry DB-002, issue #25)
  - Add Semantic Versioning to the plug-in ABI headers per VIII rule 4 (registry DB-003, issue #26)
Pending, intentionally not included: constitution-v1.1.1-draft.md (Principle VII PATCH) remains
  unratified; if ratified later it lands on top of this version.
-->

# openRender Constitution

## Core Principles

### I. Clean Code Standards

Code MUST be readable, maintainable, and self-documenting. Functions MUST be small, focused, and follow the Single Responsibility Principle. Variable and function names MUST clearly express intent. Code MUST avoid magic numbers, deep nesting, and unnecessary complexity. Comments MUST explain "why" not "what". Code structure MUST follow consistent formatting and organization patterns. Refactoring is mandatory when code quality degrades.

### II. Language Standards

Code MUST conform to C++20 standard for C++ code and C17 standard for C code. Compiler flags MUST enforce strict standard compliance with no extensions. Modern C++ features (e.g., concepts, ranges, modules where applicable) SHOULD be preferred over legacy patterns. C++ code MUST use standard library facilities over platform-specific APIs when available. C interop MUST follow proper C linkage conventions.

### III. Test-Driven Development (NON-NEGOTIABLE)

TDD is mandatory: Tests written → User approved → Tests fail → Then implement. The Red-Green-Refactor cycle MUST be strictly enforced. All new features MUST start with failing tests. Tests MUST be written before implementation code. Test coverage MUST be maintained for all critical paths. Integration tests MUST validate CLI interfaces and end-to-end workflows.

### IV. Command Line Interface

All functionality MUST be accessible via command-line interface. Text I/O protocol: stdin/args → stdout, errors → stderr. CLI tools MUST support both human-readable and machine-parseable (e.g., JSON) output formats. Command-line arguments MUST follow POSIX conventions. Help text MUST be comprehensive and accessible via --help flag. Exit codes MUST follow standard Unix conventions (0 for success, non-zero for errors).

### V. Minimal Dependencies

External dependencies MUST be minimized and justified. System libraries (e.g., libc, pthreads) are preferred over third-party libraries. When dependencies are required, they MUST be widely available on target platforms. Dependencies MUST be clearly documented with version requirements. Build system MUST gracefully handle missing optional dependencies. No dependency SHOULD be added without explicit justification in design documentation.

### VI. Platform Targeting

Code MUST target Unix-like systems, specifically Linux and macOS. Platform-specific code MUST be isolated and clearly documented. No direct Windows support is provided; Windows users MUST use WSL (Windows Subsystem for Linux). Build system MUST detect and configure for target platform automatically. Platform-specific features MUST degrade gracefully or provide clear error messages when unavailable.

### VII. Documentation and Site Management

Project documentation and development details MUST be maintained in a dedicated `site` folder using Hugo as the static site generator. The `site` folder MUST contain all source files for the project documentation website. Hugo configuration files (config.toml/yaml/json) MUST define the site structure, themes, and deployment settings. The site MUST be regularly updated to reflect changes and new features in the project. A `.github/workflows` folder MUST contain GitHub Actions workflows that automate site deployment. CI/CD pipelines MUST handle both the site content deployment and the project source code build/test processes. Site content MUST be written in Markdown format with appropriate front matter for Hugo processing. Site deployment MUST occur automatically on pushes to master branch and release tags.

### VIII. Domain Separation and Modular Boundaries

The renderer is composed of independent **domains**, each owning exactly one concern. The long-term
goal is exchangeable modules: any domain's implementation can be replaced behind its boundary
interface without changes to the others.

**Domains**

| Domain | Owns | Today (indicative) |
|---|---|---|
| Scene Interface | RI API entry points, RIB parsing, and RIB output | `src/ri/parse/`, `src/ri/output/`, `CRiInterface` |
| Graphics State | Attributes, options, transforms, the hierarchical state stack | `src/ri/state/` |
| Geometry | Primitives, tessellation/dicing, bounds, intersection | `src/ri/geometry/` |
| Hiders | Visibility determination and sampling strategy | `src/ri/hiders/` |
| Shading | Shader loading, the RSL runtime/JIT, built-ins, shading-state evaluation | `src/libshader/` |
| Texturing | Texture, 3D-texture, brick-map and point-cloud lookup, and tile paging | `src/ri/texture/` |
| Shader DSO Operands | External shadeop extensions callable from RSL | `shadeop.h` |
| Geometric Procedures | Procedural primitives (DSO, RunProgram, DelayedReadArchive) | `dlo.h`, `implicit.h` |
| Display Outputs | Display drivers and output formats | `src/display/`, `dsply.h` |

The **renderer core** (frame orchestration, `CRenderer`/`CRendererContext`) is the composition root,
not a domain: it MAY depend on every domain to wire them together, and **no domain MAY depend on the
renderer core**.

**Allowed dependencies.** A domain MAY depend only on the domains listed for it, and only through
their interfaces:

| Domain | May depend on |
|---|---|
| Graphics State | none |
| Texturing | none |
| Scene Interface | Graphics State, Geometry |
| Geometry | Graphics State, Geometric Procedures |
| Shading | Graphics State, Texturing, Shader DSO Operands |
| Hiders | Graphics State, Geometry, Shading, Display Outputs |
| Shader DSO Operands, Geometric Procedures, Display Outputs | only their own published ABI header |

**Rules**

1. **Explicit boundary interfaces.** Domains MUST interact only through a declared interface: an
   abstract class, a registration/factory hook, or a C-linkage ABI. Reaching into another domain's
   internals, sharing mutable globals across a boundary, and `friend` access across domains are
   prohibited.
2. **Acyclic, declared dependencies.** Cross-domain dependencies MUST follow the table above, at
   the source, CMake-target and link level. A capability that must flow "upward" (e.g. shading asking
   for a ray to be traced) MUST be inverted through an interface owned by the lower domain and
   implemented by the higher one. It MUST NOT be done with unresolved symbols bound at load time.
3. **Domain-agnostic behavior.** Geometry primitives MUST be hider-agnostic. Rendering effects
   (motion blur, depth of field, and future equivalents) MUST be implemented once, at the shared
   layer that owns them, so every capable hider inherits them uniformly. Shading results MUST NOT
   depend on which hider drives evaluation. No domain MAY special-case behavior that belongs to
   another.
4. **Stable, semantically versioned plug-in boundaries.** Shader DSO Operands, Geometric Procedures
   and Display Outputs are external extension points. Their interfaces MUST be C-linkage, MUST NOT
   require a plug-in to know renderer internals, and MUST carry an interface version following
   [Semantic Versioning 2.0.0](https://semver.org/):
   - **MAJOR**: incompatible change (removed or changed function, struct layout, calling
     convention, or semantics a conforming plug-in relies on).
   - **MINOR**: backward-compatible addition (new optional entry point, field, or capability).
   - **PATCH**: backward-compatible fix with no interface change.
   Every version change MUST be documented. The host MUST reject a plug-in whose MAJOR version
   differs from its own, with a clear diagnostic. Scene Interface back-ends (renderer, RIB writer,
   preview) MUST plug in through self-registration, never by the dispatch layer naming a concrete
   implementation.
5. **Independent buildability and testability.** Each domain SHOULD build as its own CMake target,
   and MUST be testable through its boundary interface, using test doubles for neighbouring domains
   where needed.
6. **Change discipline.** Every feature plan MUST name the domain(s) it touches. A change that adds
   or alters a cross-domain dependency MUST specify the interface change in its design artifacts
   before implementation begins.
7. **Automated enforcement.** CI MUST run a check that fails on any dependency cycle and on any
   cross-domain dependency the table above does not allow.
8. **Existing violations.** Known violations MUST be recorded in the violation registry
   (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`), each linked to a tracking issue. A change MUST NOT
   extend an existing violation, and SHOULD reduce one when it touches the affected area. New
   violations are prohibited.

**Rationale**: Clear boundaries keep each concern understandable and testable on its own, let
effects and features apply uniformly across hiders, and move the renderer toward independently
exchangeable modules: a new hider, shading backend or texture source plugs in without touching
the rest. Semantic Versioning of plug-in interfaces lets third-party extensions know, from the
version alone, whether they remain compatible.

## Additional Constraints

### Build System

CMake MUST be used as the primary build system. Build configuration MUST support both Debug and Release modes. Installation MUST follow standard Unix directory conventions (FHS) or support self-contained installation. Cross-compilation SHOULD be supported where feasible.

### Code Quality

Static analysis tools SHOULD be integrated into the build process. Code formatting MUST be enforced consistently (e.g., via .clang-format). Warnings MUST be treated as errors in CI/CD pipelines. Code reviews MUST verify constitution compliance before merge.

## Development Workflow

### Test-First Process

1. Write test cases based on requirements (Red phase)
2. Verify tests fail for correct reasons
3. Implement minimal code to pass tests (Green phase)
4. Refactor while maintaining passing tests (Refactor phase)
5. Repeat for next feature increment

### Code Review Requirements

All changes MUST be reviewed for constitution compliance. Reviewers MUST verify TDD process was followed. Reviewers MUST check for clean code principles adherence. Reviewers MUST validate platform compatibility. Reviewers MUST verify domain-boundary compliance (Principle VIII): no cross-domain dependency outside a declared interface or the allowed-dependency table, no dependency cycles, no hider-specific special cases, Semantic Versioning of any changed plug-in interface, and no growth of a registered violation. Complexity additions MUST be justified.

### Integration Testing

Integration tests MUST validate CLI tool behavior end-to-end. Integration tests MUST cover error handling and edge cases. Integration tests MUST verify cross-platform compatibility (Linux and macOS). Integration tests MUST be runnable in CI/CD pipelines.

## Governance

This constitution supersedes all other coding practices and guidelines. Amendments to this constitution require:

- Documentation of the rationale for change
- Approval through project maintainer review
- Migration plan for any breaking changes
- Version increment following semantic versioning

All pull requests and code reviews MUST verify compliance with these principles. Complexity MUST be justified when it conflicts with simplicity principles. Use this constitution as the primary reference for all development decisions.

**Version**: 1.2.0 | **Ratified**: 2025-12-07 | **Last Amended**: 2026-10-02
