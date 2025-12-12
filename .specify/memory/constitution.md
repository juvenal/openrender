<!--
Sync Impact Report:
Version change: 1.0.0 → 1.1.0
Modified principles: None
Added sections: VII. Documentation and Site Management
Removed sections: None
Templates requiring updates:
  ✅ plan-template.md - Constitution Check section updated to include VII
  ✅ agent-file-template.md - Documentation and Site Management section added
  ✅ spec-template.md - Documentation requirements added to success criteria
  ✅ tasks-template.md - Documentation tasks added to Polish phase
  ✅ checklist-template.md - Documentation checklist items added
Follow-up TODOs: None
-->

# Pixie Constitution

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

Project documentation and development details MUST be maintained in a dedicated `site` folder using Hugo as the static site generator. The `site` folder MUST contain all source files for the project documentation website. Hugo configuration files (config.toml/yaml/json) MUST define the site structure, themes, and deployment settings. The site MUST be regularly updated to reflect changes and new features in the project. A `.github/workflows` folder MUST contain GitHub Actions workflows that automate site deployment. CI/CD pipelines MUST handle both the site content deployment and the project source code build/test processes. Site content MUST be written in Markdown format with appropriate front matter for Hugo processing. Site deployment MUST occur automatically on pushes to main branch and release tags.

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

All changes MUST be reviewed for constitution compliance. Reviewers MUST verify TDD process was followed. Reviewers MUST check for clean code principles adherence. Reviewers MUST validate platform compatibility. Complexity additions MUST be justified.

### Integration Testing

Integration tests MUST validate CLI tool behavior end-to-end. Integration tests MUST cover error handling and edge cases. Integration tests MUST verify cross-platform compatibility (Linux and macOS). Integration tests MUST be runnable in CI/CD pipelines.

## Governance

This constitution supersedes all other coding practices and guidelines. Amendments to this constitution require:

- Documentation of the rationale for change
- Approval through project maintainer review
- Migration plan for any breaking changes
- Version increment following semantic versioning

All pull requests and code reviews MUST verify compliance with these principles. Complexity MUST be justified when it conflicts with simplicity principles. Use this constitution as the primary reference for all development decisions.

**Version**: 1.1.0 | **Ratified**: 2025-12-07 | **Last Amended**: 2025-12-08
