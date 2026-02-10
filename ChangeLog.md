# Changelog

All notable changes to openRender are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- (2025-10-23) Added speckit templates and agent context management for developer tooling (8ef2931).
- (2025-12-12–2025-12-16) Added Hugo documentation migration workflow, theme subrepo, site content, and GitHub Actions deployment for the documentation site (b7a69ec, 67ba426, a809bcb, 6913619, dbb592b).
- (2025-12-18) Added Homebrew setup guide and quickstart script for openRender (133fe82).
- (2026-01-02–2026-01-04) Added new shaders, debugging scripts, wood rendering examples, custom geometry directory, Utah Teapot RIB, and NSI PDF references (3186153, 9483a60, 12f9aef, c95d2df, 8f6148d).
- (2026-02-08–2026-02-10) Added comprehensive README for openRender and Geometry RIB statement support with updated geometry definitions (7cd1eed, 43b23a0, a992b91).

### Changed

- (2025-08-27) Standardized source formatting and cleaned up the build process (17b40c9, 6f91888).
- (2025-12-07) Updated framebuffer classes, algebra/align headers, and header comments to Doxygen style; added a `.clang-format` configuration; established the project constitution (3be02d7, 0e1b9a5, d121092, 8e6ba32, 855b25a).
- (2025-12-08) Updated CMake installation paths and modernized the codebase for C++20/C17 with 64-bit portable I/O for photon maps, point clouds, and deep shadow maps (fdcdc94, 3952bf1, a9348a9, f23095a, 602559f, 15cb212).
- (2025-12-16–2025-12-18) Updated CMake configuration and project structure, merged upstream changes, refreshed project configuration, and standardized shader formatting (a26eca0, 09029dd, ad42a3d, a8da1a3).
- (2025-12-27–2025-12-31) Replaced max/min macros with clearer conditionals, improved versioning and path handling in options, and refreshed shader sets and `.gitignore` entries (b6dacb9, 24ea6e7, bb70eb6, ddf438e, 6756d8b).
- (2026-02-08) Refactored `clampData`, renamed Pixie to openRender and updated related components, added speckit commands, and adjusted configuration formatting (ce208bf, c4e15a5, c3ab17f, 43737b7).

### Fixed

- (2025-12-13) Fixed Hugo build workflow issues and parameter placement in the documentation pipeline (8b2f277, 99bf323, 740f97a).
- (2026-02-08) Replaced `sprintf` with `snprintf` to prevent buffer overflows and resolve related warnings (fca5271).

### Docs

- (2025-12-08) Updated migration guide for Phase 2 completion (15cb212).
- (2025-12-16) Updated Hugo configuration for the openRender documentation site (dbb592b).

## [1.0.0] - 2025-02-10

### Added

- openRender 1.0.0: rebrand from Pixie; CMake-based build and install; documentation converted to Markdown; semantic versioning (MAJOR.MINOR.PATCH).
