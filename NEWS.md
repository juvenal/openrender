# News

## Latest (1.0.0)

openRender 1.0.0 is the first release under the openRender name, rebranded from Pixie. It includes:

- CMake-based build and install
- Documentation converted to Markdown (README, INSTALL, LICENSE, AUTHORS, etc.)
- Semantic versioning (MAJOR.MINOR.PATCH)

For a detailed list of changes, see [ChangeLog.md](ChangeLog.md).

## Since 5399a1b (2025-08-27 – 2026-02-10)

- (2025-12-07–2025-12-08) Modernized the codebase for C++20/C17 with improved 64-bit portability and portable I/O for photon maps, point clouds, and deep shadow maps; standardized formatting and documentation across core components.
- (2025-12-12–2025-12-16) Migrated documentation to a Hugo-based site with a dedicated theme, CI-based deployment, and updated configuration.
- (2025-12-27–2025-12-31) Simplified math macros, enhanced versioning and path handling, and refreshed shader assets and `.gitignore` entries.
- (2026-01-02–2026-01-04) Added new shaders, RIB examples (including the Utah Teapot and wood materials), custom geometry, and NSI documentation references.
- (2026-02-08–2026-02-10) Completed the openRender rebrand, added speckit tooling, rewrote the README, and introduced Geometry RIB statement support with updated geometry definitions.
- (2026-03-10) Updated the compiled shader extension from `.sdr` to `.rslo`. The `oshader` compiler now outputs `.rslo` by default, while maintaining full backward compatibility for existing `.sdr` shaders. Added `--legacy-sdr` flag to `oshader`.
