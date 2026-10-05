# Validation: Deferred CSG Evaluation (T083)

**Feature**: 021-deferred-csg-evaluation | **Run**: 2026-10-04 | **Machine**: Apple M2 (arm64), macOS 27.0.1, Release build with vcpkg-vendored dependencies

Every [quickstart.md](quickstart.md) check, run end to end on the Phase 7 code.
Sections §1–§5 come from one full sequential `ctest` run (`nice -n 15 taskpolicy -b ctest --test-dir build -j1`):
**537/537 passed** in 1679 s. §6 is the T081 timing run, and §7 was run directly.

| § | Check | Result |
|---|---|---|
| 1 | Hero and sphere-cube seams, all hiders, both resolutions: `Csg_Seam_(hero\|spherecube)` | **13/13 pass** |
| 2 | Cross-hider agreement: every parity-labelled CSG test (`Parity_csg-*`, `Parity_blobby-csg-*`, the enclosed-holes, secondary-ray and untouched-region checks) | **36/36 pass**; worst parity difference 14.86 against 20 ([parity-thresholds.md](../../tests/visual/parity-thresholds.md)) |
| 3 | Nothing regressed: `ctest -R "CSG\|Csg\|csg"` | **122/122 pass** |
| 3 | Visual suite (`Visual_*`) | **265/265 pass**. No known-flaky or pre-existing failure appeared in this run, so nothing needed classifying |
| 4 | Work only where seen: `Csg_(OffscreenStats\|PartialVisibilityStats\|ResolutionScaling\|TinySolidStats)` | **5/5 pass**. Offscreen: all four counters 0. Partial ÷ full seam splits = 0.63 (≤ 0.75). 1600: 6542 > 800: 2750. Tiny solid: 3 (≤ 16) |
| 5 | Settings and diagnostics: `Csg_ToleranceInert`, the three `Csg_malformed-*` guards, `Csg_procedural-missing-archive`, `Csg_Procedural_{sphere,open_cylinder}` | **7/7 pass** |
| 6 | Performance (SC-007): 59 scenes against the master baselines, throttled, same machine | **pass**: every ratio < 10×; worst 8.35× (`csg-coincident-difference-zbuffer`), median 0.92× ([baseline-timings.md](baseline-timings.md), "Spec 021 render times") |
| 7 | Separation-of-concerns gate: the grep for CSG identifiers in `src/ri/hiders` and `src/libshader/shading` | **pass**: no output |

Also in this run: `RibParse_SessionsReadArchives`, the regression test for the lexer bug fixed in Phase 5, passes (1/1).

## Corrections made while validating

- The quickstart's `ctest -R` patterns named tests `CsgSeam_…`, `CsgOffscreenStats…` and so on, but `add_csg_metric_test` registers them as `Csg_<NAME>`. As written, those commands matched no tests at all. quickstart.md now uses the registered names.
- §6 first failed for two coincident-face scenes (17.1× and 14.4×). That was fixed by the flat-quad fast path in `countBilinearCrossings()`; see baseline-timings.md.

## Linux (T084, constitution VI), 2026-10-05

Run by the maintainer's Ubuntu 24.04 agent ("openRender Linux", reached through Maestri).
- **Toolchain**: GCC 13.3.0, cmake 3.28.3, flex 2.6.4, bison 3.8.2, system LLVM 18.1.3 with the JIT on (50 `.slo` scenes).
- **Configuration**: Development build, `-DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF`, in a fresh `build-021`.
- **Source tree**: the branch's tracked files were rsync'ed from macOS. A SHA-256 over every tracked file matched before the build.

| Check | Result |
|---|---|
| Build, `-j4`, `-Werror` | **clean**: no first-party warnings or errors, no linker or third-party warnings |
| `ctest -L csg` (sequential) | **70/70 pass** |
| `ctest -R 'Csg_\|Visual_(csg\|blobby-csg)\|Parity_(csg\|blobby-csg)\|RibParse\|CSG_'` (sequential) | **123/123 pass** (19 unit, 68 visual, 1 parser), 332 s |

The first Linux build found two GCC 13 `-Werror` failures that Clang on macOS does not report:
1. **`CToroid::intersect` (`quadrics.cpp`)**: `-Werror=array-bounds`, a false positive.
   - GCC analyses `std::sort`'s 16-element insertion threshold against the 4-entry root array `s`, although `ns <= 4`.
   - The code comes from the torus fix already on master (`44369fb`), so master itself did not build on Ubuntu 24.04.
   - Fixed on the separate branch `fix/torus-gcc13-array-bounds`: an insertion sort replaces `std::sort`. On macOS that branch builds clean and passes the torus visual and parity tests and `Polynomial_Solvers`.
   - The Linux run used that exact change. 021 picks it up once it is rebased onto the fixed master.
2. **`tests/unit/csg/test_procedural_operand.cpp`**: `-Werror=sign-compare` (`int` against unsigned attribute flags). Fixed on this branch: `templateInside` is now `unsigned int`. The assertion is unchanged.
