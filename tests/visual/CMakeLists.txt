# Visual regression tests for openRender.
#
# Each test:
#   1. Runs orender on a RIB scene from examples/rib/tests/
#   2. Compares the output TIF against a reference in examples/rib/tests/references/
#   3. Fails if any RGB pixel channel differs by more than VISUAL_THRESHOLD (default 3/255)
#
# Tests are labelled "visual;regression" so they can be run separately:
#   ctest -L visual --output-on-failure
#
# Reference images must already exist in examples/rib/tests/references/.
# To regenerate references, render each scene manually and copy the output there.

cmake_minimum_required(VERSION 3.16)

# Block-average threshold (0–255).
# Each 8×8 block's average color is compared; a block fails if any RGB channel
# average differs by more than this value. Stochastic edge noise of ±100 per pixel
# averages down to ±100/64 ≈ ±1.6 per block.
# Complex scenes (spotlight, camera-dof, motion blur) exhibit higher inter-run
# variance (up to ~19 per block) due to stochastic sampling of cone lights and DoF.
# Threshold of 20 catches real shading regressions (50+ block diff) while
# tolerating observed stochastic noise across all test scenes.
set(VISUAL_THRESHOLD 20)

# ---------------------------------------------------------------------------
# Comparison tool
# ---------------------------------------------------------------------------
find_package(TIFF REQUIRED)

add_executable(test_visual_render test_visual_render.cpp)
target_link_libraries(test_visual_render PRIVATE TIFF::TIFF)
target_compile_features(test_visual_render PRIVATE cxx_std_20)

# Radial-energy-histogram CLI tool (FR-009): validates depth-of-field lens
# sampling uniformity. Not wired into ctest — invoked manually against
# rendered DoF scenes (see quickstart.md).
add_executable(test_radial_histogram test_radial_histogram.cpp)
target_link_libraries(test_radial_histogram PRIVATE TIFF::TIFF)
target_compile_features(test_radial_histogram PRIVATE cxx_std_20)

# Cross-hider (reyes vs. raytrace) parity test driver (spec 008). Renders
# both hider variants of a scene and diffs their fresh outputs against each
# other (no static reference — see test_hider_parity.cpp header comment).
add_executable(test_hider_parity test_hider_parity.cpp)
target_link_libraries(test_hider_parity PRIVATE TIFF::TIFF)
target_compile_features(test_hider_parity PRIVATE cxx_std_20)

# ---------------------------------------------------------------------------
# Environment common to all visual tests
# ---------------------------------------------------------------------------
set(VISUAL_ENV
    "SHADERS=${CMAKE_SOURCE_DIR}/shaders"
    "ORENDERHOME=${CMAKE_SOURCE_DIR}/openrender"
    "DISPLAYS=${CMAKE_SOURCE_DIR}/openrender/displays"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/openrender/geometry"
)

# Same as VISUAL_ENV, plus Option B's unified per-bucket sample table (spec
# 008 Phase 11/US9). Only worth enabling on a parity test where correlating
# the two hiders' random draws demonstrably tightens the diff -- T084
# measured this per-scene rather than assuming it universally helps (see
# motion-*-translate-correlated below): dof's diff got *worse* under
# correlation (D9 is a different-model-same-input structural residual, not
# RNG noise, so forcing an identical draw through two different DOF formulas
# just makes the error spatially coherent instead of letting independent
# noise partially cancel), and motion-polygons-translate was flat within
# run-to-run noise. motion-quadrics-translate and motion-patches-translate,
# by contrast, are same-model/different-RNG-stream residuals that the
# correlated table genuinely closes.
set(VISUAL_ENV_CORRELATED
    ${VISUAL_ENV}
    "OPENRENDER_CORRELATED_SAMPLE_TABLE=1"
)

# ---------------------------------------------------------------------------
# Helper macro: register one visual regression test
#
# Arguments:
#   SCENE_NAME   — test identifier (used as CTest name suffix and for scratch dir)
#   RIB_FILE     — path to the .rib scene (relative to CMAKE_SOURCE_DIR)
#   OUTPUT_TIF   — filename the RIB's Display statement writes (relative to CWD)
#   REF_TIF      — reference filename in examples/rib/tests/references/
#   THRESHOLD    — (optional) per-scene threshold override; defaults to VISUAL_THRESHOLD
# ---------------------------------------------------------------------------
macro(add_visual_test SCENE_NAME RIB_FILE OUTPUT_TIF REF_TIF)
    if(${ARGC} GREATER 4)
        set(_thresh ${ARGV4})
    else()
        set(_thresh ${VISUAL_THRESHOLD})
    endif()

    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/${SCENE_NAME})
    file(MAKE_DIRECTORY ${_scratch})

    add_test(NAME Visual_${SCENE_NAME}
        COMMAND test_visual_render
            $<TARGET_FILE:orender>
            ${CMAKE_SOURCE_DIR}/${RIB_FILE}
            ${OUTPUT_TIF}
            ${CMAKE_SOURCE_DIR}/examples/rib/tests/references/${REF_TIF}
            ${_thresh}
        WORKING_DIRECTORY ${_scratch}
    )

    set_tests_properties(Visual_${SCENE_NAME} PROPERTIES
        TIMEOUT        360
        LABELS         "visual;regression"
        ENVIRONMENT    "${VISUAL_ENV}"
        DEPENDS        orender
    )
endmacro()

# ---------------------------------------------------------------------------
# Helper macro: register one cross-hider parity test (spec 008)
#
# Renders both RIB_A and RIB_B with orender, then diffs the two fresh
# outputs against each other (no static reference — both hiders' outputs
# are compared to each other, not to a "ground truth" image).
#
# Arguments:
#   SCENE_NAME   — test identifier (used as CTest name suffix and for scratch dir)
#   RIB_A        — path to the first hider's .rib scene (relative to CMAKE_SOURCE_DIR)
#   OUTPUT_A     — filename RIB_A's Display statement writes (relative to CWD)
#   RIB_B        — path to the second hider's .rib scene (relative to CMAKE_SOURCE_DIR)
#   OUTPUT_B     — filename RIB_B's Display statement writes (relative to CWD)
#   THRESHOLD    — (optional) per-scene threshold override; defaults to VISUAL_THRESHOLD
#   ENV_LIST     — (optional) environment override; defaults to VISUAL_ENV. Pass
#                  VISUAL_ENV_CORRELATED to exercise Option B's unified sample
#                  table (spec 008 Phase 11/US9) instead of each hider drawing
#                  its own independent stream.
# ---------------------------------------------------------------------------
macro(add_parity_test SCENE_NAME RIB_A OUTPUT_A RIB_B OUTPUT_B)
    if(${ARGC} GREATER 5)
        set(_thresh ${ARGV5})
    else()
        set(_thresh ${VISUAL_THRESHOLD})
    endif()

    if(${ARGC} GREATER 6)
        set(_env ${ARGV6})
    else()
        set(_env ${VISUAL_ENV})
    endif()

    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/parity-${SCENE_NAME})
    file(MAKE_DIRECTORY ${_scratch})

    add_test(NAME Parity_${SCENE_NAME}
        COMMAND test_hider_parity
            $<TARGET_FILE:orender>
            ${CMAKE_SOURCE_DIR}/${RIB_A}
            ${OUTPUT_A}
            ${CMAKE_SOURCE_DIR}/${RIB_B}
            ${OUTPUT_B}
            ${_thresh}
        WORKING_DIRECTORY ${_scratch}
    )

    set_tests_properties(Parity_${SCENE_NAME} PROPERTIES
        TIMEOUT        360
        LABELS         "parity;regression"
        ENVIRONMENT    "${_env}"
        DEPENDS        orender
    )
endmacro()

# ---------------------------------------------------------------------------
# Test registrations
# (SCENE_NAME, RIB, output TIF from Display stmt, reference TIF filename)
# ---------------------------------------------------------------------------

# --- Teapot material variants ---
add_visual_test(teapot-matte-reyes
    examples/rib/tests/teapot-matte-reyes.rib
    teapot-matte-reyes.tif  teapot-matte-reyes.tif)

add_visual_test(teapot-matte-raytrace
    examples/rib/tests/teapot-matte-raytrace.rib
    teapot-matte-raytrace.tif  teapot-matte-raytrace.tif)

add_visual_test(teapot-blue_marble-reyes
    examples/rib/tests/teapot-blue_marble-reyes.rib
    teapot-blue_marble-reyes.tif  teapot-blue_marble-reyes.tif)

add_visual_test(teapot-blue_marble-raytrace
    examples/rib/tests/teapot-blue_marble-raytrace.rib
    teapot-blue_marble-raytrace.tif  teapot-blue_marble-raytrace.tif)

add_visual_test(teapot-brushedmetal-reyes
    examples/rib/tests/teapot-brushedmetal-reyes.rib
    teapot-brushedmetal-reyes.tif  teapot-brushedmetal-reyes.tif)

add_visual_test(teapot-brushedmetal-raytrace
    examples/rib/tests/teapot-brushedmetal-raytrace.rib
    teapot-brushedmetal-raytrace.tif  teapot-brushedmetal-raytrace.tif)

add_visual_test(teapot-wood-reyes
    examples/rib/tests/teapot-wood-reyes.rib
    teapot-wood-reyes.tif  teapot-wood-reyes.tif)

add_visual_test(teapot-wood-raytrace
    examples/rib/tests/teapot-wood-raytrace.rib
    teapot-wood-raytrace.tif  teapot-wood-raytrace.tif)

# --- .slo (LLVM JIT) variants — compare against same references as .rslo counterparts ---
add_visual_test(teapot-wood-reyes-slo
    examples/rib/tests/teapot-wood-reyes-slo.rib
    teapot-wood-reyes-slo.tif  teapot-wood-reyes.tif)

add_visual_test(teapot-wood-raytrace-slo
    examples/rib/tests/teapot-wood-raytrace-slo.rib
    teapot-wood-raytrace-slo.tif  teapot-wood-raytrace.tif)

add_visual_test(teapot-blue_marble-reyes-slo
    examples/rib/tests/teapot-blue_marble-reyes-slo.rib
    teapot-blue_marble-reyes-slo.tif  teapot-blue_marble-reyes.tif)

add_visual_test(teapot-blue_marble-raytrace-slo
    examples/rib/tests/teapot-blue_marble-raytrace-slo.rib
    teapot-blue_marble-raytrace-slo.tif  teapot-blue_marble-raytrace.tif)

add_visual_test(teapot-brushedmetal-reyes-slo
    examples/rib/tests/teapot-brushedmetal-reyes-slo.rib
    teapot-brushedmetal-reyes-slo.tif  teapot-brushedmetal-reyes.tif)

add_visual_test(teapot-brushedmetal-raytrace-slo
    examples/rib/tests/teapot-brushedmetal-raytrace-slo.rib
    teapot-brushedmetal-raytrace-slo.tif  teapot-brushedmetal-raytrace.tif)

# --- somewood (.rslo reference + .slo JIT variants) ---
add_visual_test(teapot-somewood-reyes
    examples/rib/tests/teapot-somewood-reyes.rib
    teapot-somewood-reyes.tif  teapot-somewood-reyes.tif)

add_visual_test(teapot-somewood-raytrace
    examples/rib/tests/teapot-somewood-raytrace.rib
    teapot-somewood-raytrace.tif  teapot-somewood-raytrace.tif)

add_visual_test(teapot-somewood-reyes-slo
    examples/rib/tests/teapot-somewood-reyes-slo.rib
    teapot-somewood-reyes-slo.tif  teapot-somewood-reyes.tif)

add_visual_test(teapot-somewood-raytrace-slo
    examples/rib/tests/teapot-somewood-raytrace-slo.rib
    teapot-somewood-raytrace-slo.tif  teapot-somewood-raytrace.tif)

# --- somewood aliasing stress test (elevated grain/swirlfreq to push filter
# width fw well past the fade threshold; guards against vein/turbulence
# aliasing regressing back in) ---
add_visual_test(teapot-somewood-aliasing
    examples/rib/tests/teapot-somewood-aliasing.rib
    teapot-somewood-aliasing.tif  teapot-somewood-aliasing.tif)

# teapot-full-metal uses the "teapot-full-*" reference images
add_visual_test(teapot-full-metal-reyes
    examples/rib/tests/teapot-full-metal-reyes.rib
    teapot-full-reyes.tif  teapot-full-reyes.tif)

add_visual_test(teapot-full-metal-raytrace
    examples/rib/tests/teapot-full-metal-raytrace.rib
    teapot-full-raytrace.tif  teapot-full-raytrace.tif)

# --- Killeroo ---
add_visual_test(killeroo-reyes
    examples/rib/tests/killeroo-reyes.rib
    killeroo-reyes.tif  killeroo-reyes.tif)

add_visual_test(killeroo-raytrace
    examples/rib/tests/killeroo-raytrace.rib
    killeroo-raytrace.tif  killeroo-raytrace.tif)

# --- Bunny ---
add_visual_test(bunny-reyes
    examples/rib/tests/bunny-reyes.rib
    bunny-reyes.tif  bunny-reyes.tif)

add_visual_test(bunny-raytrace
    examples/rib/tests/bunny-raytrace.rib
    bunny-raytrace.tif  bunny-raytrace.tif)

# --- Cubes ---
# Note: cubes-raytrace.rib outputs cubes-raytracer.tif (historical name in Display stmt)
add_visual_test(cubes-reyes
    examples/rib/tests/cubes-reyes.rib
    cubes-reyes.tif  cubes-reyes.tif)

add_visual_test(cubes-raytrace
    examples/rib/tests/cubes-raytrace.rib
    cubes-raytracer.tif  cubes-raytracer.tif)

# --- Spotlight ---
# Spotlight: sharp cone boundary causes high inter-run variance (27/block observed).
add_visual_test(spotlight-reyes
    examples/rib/tests/spotlight-reyes.rib
    spotlight-reyes.tif  spotlight-reyes.tif  40)

add_visual_test(spotlight-raytrace
    examples/rib/tests/spotlight-raytrace.rib
    spotlight-raytrace.tif  spotlight-raytrace.tif  40)

# --- Motion blur ---
add_visual_test(motion-1-reyes
    examples/rib/tests/motion-1-reyes.rib
    motion-1-reyes.tif  motion-1-reyes.tif)

add_visual_test(motion-1-raytrace
    examples/rib/tests/motion-1-raytrace.rib
    motion-1-raytrace.tif  motion-1-raytrace.tif)

add_visual_test(motion-2-reyes
    examples/rib/tests/motion-2-reyes.rib
    motion-2-reyes.tif  motion-2-reyes.tif)

add_visual_test(motion-2-raytrace
    examples/rib/tests/motion-2-raytrace.rib
    motion-2-raytrace.tif  motion-2-raytrace.tif)

# motion-3-reyes: 90° camera rotation with PixelSamples 3 3, 800x600.
# The extreme-blur bands are sparse speckle whose realization depends on the
# thread->bucket assignment of the per-thread jitter streams: two runs of the
# same binary measure up to ~79/255 max block diff. Threshold 100 stays above
# that variance while still catching gross regressions in solid regions.
add_visual_test(motion-3-reyes
    examples/rib/tests/motion-3-reyes.rib
    motion-3-reyes.tif  motion-3-reyes.tif  100)
set_tests_properties(Visual_motion-3-reyes PROPERTIES
    TIMEOUT 900
    LABELS  "visual;regression;slow")

add_visual_test(motion-3-raytrace
    examples/rib/tests/motion-3-raytrace.rib
    motion-3-raytrace.tif  motion-3-raytrace.tif)

# --- Teapot motion ---
add_visual_test(teapot-motion-reyes
    examples/rib/tests/teapot-motion-reyes.rib
    teapot-motion-reyes.tif  teapot-motion-reyes.tif)

add_visual_test(teapot-motion-raytrace
    examples/rib/tests/teapot-motion-raytrace.rib
    teapot-motion-raytrace.tif  teapot-motion-raytrace.tif)

# --- Camera DoF ---
add_visual_test(camera-dof-reyes
    examples/rib/tests/camera-dof-reyes.rib
    camera-dof-reyes.tif  camera-dof-reyes.tif)

add_visual_test(camera-dof-raytrace
    examples/rib/tests/camera-dof-raytrace.rib
    camera-dof-raytrace.tif  camera-dof-raytrace.tif  30)

# --- Camera motion (small shift) ---
add_visual_test(camera-motion-small-reyes
    examples/rib/tests/camera-motion-small-reyes.rib
    camera-motion-small-reyes.tif  camera-motion-small-reyes.tif)

add_visual_test(camera-motion-small-raytrace
    examples/rib/tests/camera-motion-small-raytrace.rib
    camera-motion-small-raytrace.tif  camera-motion-small-raytrace.tif)

# DoF+motion sampling is realization-sensitive: two runs of the same binary
# measure up to ~64/255 (reyes) and ~97/255 (raytrace) max block diff, so the
# thresholds must sit above that run-to-run variance.
add_visual_test(camera-motion-small-dof-reyes
    "examples/rib/tests/camera-motion-small+dof-reyes.rib"
    camera-motion-small+dof-reyes.tif  camera-motion-small+dof-reyes.tif  90)

add_visual_test(camera-motion-small-dof-raytrace
    "examples/rib/tests/camera-motion-small+dof-raytrace.rib"
    camera-motion-small+dof-raytrace.tif  camera-motion-small+dof-raytrace.tif  110)

# camera-motion-huge: only the reyes variant is tested.
# Like motion-3-reyes, the huge-blur bands are thread-schedule-sensitive
# speckle: two runs of the same binary measure up to ~70/255 max block diff,
# so the threshold must sit above that run-to-run variance.
add_visual_test(camera-motion-huge-reyes
    examples/rib/tests/camera-motion-huge-reyes.rib
    camera-motion-huge-reyes.tif  camera-motion-huge-reyes.tif  100)

# --- NURBS (NuPatch) untrimmed baseline — spec 009, locks in current
# (pre-trim-curve) rendering behavior before any RiTrimCurve code lands ---
add_visual_test(nupatch-vase-untrimmed
    examples/rib/tests/nupatch-vase-untrimmed.rib
    nupatch-vase-untrimmed.tif  nupatch-vase-untrimmed.tif)

# --- NURBS (NuPatch) trimmed hole — spec 009 US1, RiTrimCurve cutting a
# rectangular hole through the vase body, straddling a non-origin u-knot
# span boundary to exercise multi-span trim classification (reyes hider) ---
add_visual_test(nupatch-vase-trimmed-hole
    examples/rib/tests/nupatch-vase-trimmed-hole.rib
    nupatch-vase-trimmed-hole.tif  nupatch-vase-trimmed-hole.tif)

# --- Same scene, raytrace hider — spec 009 US1 SC-008 cross-hider trim
# parity: exercises CTesselationPatch's trim integration (T017) rather than
# CPatch::dice()'s (T016) ---
add_visual_test(nupatch-vase-trimmed-hole-raytrace
    examples/rib/tests/nupatch-vase-trimmed-hole-raytrace.rib
    nupatch-vase-trimmed-hole-raytrace.tif  nupatch-vase-trimmed-hole-raytrace.tif)

# --- NURBS (NuPatch) trim attribute scoping — spec 009 US2, four vase copies
# demonstrating RiTrimCurve as attribute state: scoped inside
# AttributeBegin/End (popped on End), a sibling untrimmed copy outside that
# scope, and one TrimCurve call whose trim persists across two subsequent
# NuPatch calls with no intervening TrimCurve ---
add_visual_test(nupatch-vase-trimmed-scoping
    examples/rib/tests/nupatch-vase-trimmed-scoping.rib
    nupatch-vase-trimmed-scoping.tif  nupatch-vase-trimmed-scoping.tif)

# --- NURBS (NuPatch) trimcurve/sense attribute — spec 009 US3, left vase
# uses the default "inside" sense (same hole as nupatch-vase-trimmed-hole),
# right vase sets Attribute "trimcurve" "sense" ["outside"] to invert which
# side of the loop is kept ---
add_visual_test(nupatch-vase-trimmed-sense
    examples/rib/tests/nupatch-vase-trimmed-sense.rib
    nupatch-vase-trimmed-sense.tif  nupatch-vase-trimmed-sense.tif)

# --- NURBS (NuPatch) multi-loop composition — spec 009 US5, one TrimCurve
# call defining three disjoint rectangular holes (reusing the
# nupatch-vase-trimmed-hole loop plus two more near the neck) to exercise
# the even-odd/XOR composition across independently-toggling loops ---
add_visual_test(nupatch-vase-trimmed-multiloop
    examples/rib/tests/nupatch-vase-trimmed-multiloop.rib
    nupatch-vase-trimmed-multiloop.tif  nupatch-vase-trimmed-multiloop.tif)

# --- NURBS (NuPatch) malformed trim-loop diagnostics — spec 009 Polish phase,
# T031/T032. Same rectangular loop as nupatch-vase-trimmed-hole but perturbed
# to exercise the two malformed-loop paths: an unclosed loop (implicit-closure
# warning, loop still used) and a non-positive weight (loop rejected, warning,
# fallback to untrimmed rendering). Each must emit exactly one diagnostic and
# must not crash ---
add_visual_test(nupatch-vase-trimmed-malformed-unclosed
    examples/rib/tests/nupatch-vase-trimmed-malformed-unclosed.rib
    nupatch-vase-trimmed-malformed-unclosed.tif  nupatch-vase-trimmed-malformed-unclosed.tif)

add_visual_test(nupatch-vase-trimmed-malformed-badweight
    examples/rib/tests/nupatch-vase-trimmed-malformed-badweight.rib
    nupatch-vase-trimmed-malformed-badweight.tif  nupatch-vase-trimmed-malformed-badweight.tif)

# T033 (FR-020): both malformed loops above, each captured once inside
# ObjectBegin/ObjectEnd and referenced by three ObjectInstance calls apiece —
# verifies the per-loop diagnostic dedup applies to the loop *definition*,
# not to each instantiated copy (six rendered vases, two diagnostics total).
add_visual_test(nupatch-vase-trimmed-malformed-instanced
    examples/rib/tests/nupatch-vase-trimmed-malformed-instanced.rib
    nupatch-vase-trimmed-malformed-instanced.tif  nupatch-vase-trimmed-malformed-instanced.tif)

# ---------------------------------------------------------------------------
# Cross-hider parity tests (spec 008, US1/Phase 3)
#
# Same-scene reyes-vs-raytrace comparisons — see test_hider_parity.cpp
# header and specs/008-hider-parity-convergence/tasks.md T007-T013.
# Thresholds and rationale are documented in tests/visual/parity-thresholds.md.
# ---------------------------------------------------------------------------

# --- Flat-shading (no DOF, no transparency): matte teapot under 3 lights ---
add_parity_test(flatshade
    examples/rib/tests/parity/flatshade-reyes.rib
    flatshade-reyes.tif
    examples/rib/tests/parity/flatshade-raytrace.rib
    flatshade-raytrace.tif  20)

# --- Depth of field: three cones at staggered depths through a wide lens ---
# D2 (pixel-jitter-constant drift, 0.5001011 vs 0.5) is not yet fixed —
# threshold stays loose until Phase 4/T021 unifies jitter via CSampler.
add_parity_test(dof
    examples/rib/tests/parity/dof-reyes.rib
    dof-reyes.tif
    examples/rib/tests/parity/dof-raytrace.rib
    dof-raytrace.tif  60)

# --- AOV capture without transparency: single-file "N" (surface normal) ---
add_parity_test(aov
    examples/rib/tests/parity/aov-reyes.rib
    aov-reyes.tif
    examples/rib/tests/parity/aov-raytrace.rib
    aov-raytrace.tif  25)

# --- Depth-filter default mode: raw float32 "z" AOV, teapot scene ---
# Threshold is dominated by a known raytrace gap (D7): raytrace has no
# depth-filter/zvisibilityThreshold logic yet, so it blends the background
# sentinel into silhouette-edge pixels' averaged z (observed worst-case
# ~437496 block-avg diff in world-space depth units, vs. sub-unit agreement
# everywhere else). Phase 6/S3 implements shared depth-filter modes and
# T045 re-measures/tightens this threshold once that lands.
add_parity_test(depthdefault
    examples/rib/tests/parity/depthdefault-reyes.rib
    depthdefault-reyes.tif
    examples/rib/tests/parity/depthdefault-raytrace.rib
    depthdefault-raytrace.tif  500000)

# --- Transparency: three overlapping semi-transparent spheres (front-to-back
# "over" compositing) — spec 008 Phase 5/US3/R3. Measured ~1.19 pre-refactor;
# plain color/opacity compositing is already shared behavior between hiders,
# so this threshold is not expected to move much after CCompositor lands.
add_parity_test(transparency
    examples/rib/tests/parity/transparency-reyes.rib
    transparency-reyes.tif
    examples/rib/tests/parity/transparency-raytrace.rib
    transparency-raytrace.tif  20)

# --- Matte: a Matte 1 (holdout) sphere partially covering a background
# sphere, plus a normal (non-matte) sphere as a control — spec 008 Phase
# 5/US3/R3. Measured ~3.94 pre-refactor in the rgba channel; matte holdout
# semantics already agree between hiders here (D8 only manifests in the AOV
# channel — see transparency-matte-aov below).
add_parity_test(matte
    examples/rib/tests/parity/matte-reyes.rib
    matte-reyes.tif
    examples/rib/tests/parity/matte-raytrace.rib
    matte-raytrace.tif  20)

# --- Combined effect: an "N" AOV surface behind a semi-transparent sphere,
# plus a separate Matte 1 sphere — spec 008 Phase 5/US3/R3/S4. This is the
# TDD Red gate for T028: measured ~255.00 (maxed out) pre-refactor because
# raytrace's CPrimaryBundle::postShade only copies extra AOV channels on the
# first hit (D6) and doesn't apply the matte holdout to the AOV channel (D8)
# — both spheres show up as separate uncomposited "ghost" layers instead of
# being properly over-composited/held-out the way reyes's rasterEnd already
# does. Threshold set tight (matches the plain "aov" scene) so it fails now
# and is expected to go green once CCompositor (T029-T032) lands; re-measure
# and tighten further at T035.
add_parity_test(transparency-matte-aov
    examples/rib/tests/parity/transparency-matte-aov-reyes.rib
    transparency-matte-aov-reyes.tif
    examples/rib/tests/parity/transparency-matte-aov-raytrace.rib
    transparency-matte-aov-raytrace.tif  25)

# --- Depth-filter modes: same three overlapping semi-transparent spheres as
# "transparency" above, but rendered with PixelSamples 4 4 (so min/max/avg
# diverge across a pixel's subsample grid, not just within one fragment
# list) and a "z" AOV output. A zthreshold of 0.4 excludes the back sphere
# (opacity 0.35) from z-visibility, exercising FR-013 alongside the mode
# dispatch. This is the TDD Red gate for T039/T040-T043: raytrace has
# neither depth-filter-mode dispatch nor zvisibilityThreshold logic yet
# (raytracer.cpp always emits the first hit's z, D7), so threshold is left
# loose until CCompositor::evaluateDepth lands; T045 re-measures/tightens.
add_parity_test(depthfilter-min
    examples/rib/tests/parity/depthfilter-min-reyes.rib
    depthfilter-min-reyes.tif
    examples/rib/tests/parity/depthfilter-min-raytrace.rib
    depthfilter-min-raytrace.tif  500000)

add_parity_test(depthfilter-max
    examples/rib/tests/parity/depthfilter-max-reyes.rib
    depthfilter-max-reyes.tif
    examples/rib/tests/parity/depthfilter-max-raytrace.rib
    depthfilter-max-raytrace.tif  500000)

add_parity_test(depthfilter-avg
    examples/rib/tests/parity/depthfilter-avg-reyes.rib
    depthfilter-avg-reyes.tif
    examples/rib/tests/parity/depthfilter-avg-raytrace.rib
    depthfilter-avg-raytrace.tif  500000)

add_parity_test(depthfilter-mid
    examples/rib/tests/parity/depthfilter-mid-reyes.rib
    depthfilter-mid-reyes.tif
    examples/rib/tests/parity/depthfilter-mid-raytrace.rib
    depthfilter-mid-raytrace.tif  500000)

# --- Displacement parity: a single sphere with the "bump_lowfreq"
# displacement shader bound (no "trace"/"displacements" attribute set on
# the raytrace side). This is the TDD Red gate for T047/T048-T051 (spec
# 008-hider-parity-convergence, Phase 7/S2): today raytrace requires an
# explicit `Attribute "trace" "displacements" [1]` opt-in to displace at all
# (ATTRIBUTES_FLAGS_DISPLACEMENTS defaults off), so its shading stays on the
# undisplaced sphere while reyes always displaces when a displacement shader
# is bound.
#
# Originally used the "dented" shader (6-octave 1/f turbulence), but that
# shader's high-frequency content aliased into each hider's independently
# adaptive dicing/tessellation grid -- reyes dices by ShadingRate, raytrace
# by ray-differential screen footprint, so the two grids sample the same
# noise field at different parametric points and produce a hairline
# ridge-vs-ridge diff that swamped the real "is this side displaced at all"
# signal (measured ~2.7/255 for both-displaced vs ~2.6/255 for the pre-fix
# one-side-disabled bug -- the metric could not tell the two apart).
# Switched to "bump_lowfreq" (single-octave, large-period noise, see
# shaders/bump_lowfreq.sl) so the two grids converge to nearly the same
# displaced surface; residual diff is now dominated by ordinary silhouette
# antialiasing (measured MaxBlockAvgDiff ~2.3-2.7/255, 48-53/1200 blocks
# fail across repeated runs -- some run-to-run jitter is expected from
# PixelSamples). A simulated regression (raytrace displacement forced off
# again) measures MaxBlockAvgDiff ~204/255, 240/1200 blocks fail -- an ~80x
# gap that a threshold of 6 cleanly separates: passes real parity, still
# fails the pre-fix bug.
add_parity_test(displacement
    examples/rib/tests/parity/displacement-reyes.rib
    displacement-reyes.tif
    examples/rib/tests/parity/displacement-raytrace.rib
    displacement-raytrace.tif  6)

# --- Object motion blur (spec 008 Phase 8/US6/S5, closes D10): verifies the
# raytracer's tessellation-path ray-time interpolation (cRay->time for
# patches/polygons, rv->time for quadrics) against reyes on a per-primitive
# basis, for both transform-motion (MotionBegin wraps a Translate) and
# deform-motion (MotionBegin wraps two different geometry snapshots — a
# repeated Patch/Polygon/Sphere with different data). No pre-existing example
# scene exercised object (non-camera) motion through the raytracer before
# this; all six pairs measured genuinely converged (both hiders agree on
# silhouette, streak/interpolated shape, and shading) with no correctness
# fix required — residual diff is ordinary silhouette antialiasing on a
# curved/moving edge. Thresholds set at the standard 20 used for
# flatshade/transparency/matte, except motion-patches-deform (measured
# ~16.8-17.1 across repeated runs, a curved bicubic patch flattening over
# the shutter interval produces more silhouette-edge shading gradient than
# the other five) which gets a slightly looser 25 for run-to-run margin.
add_parity_test(motion-patches-translate
    examples/rib/tests/parity/motion-patches-translate-reyes.rib
    motion-patches-translate-reyes.tif
    examples/rib/tests/parity/motion-patches-translate-raytrace.rib
    motion-patches-translate-raytrace.tif  20)

# Correlated-table variant (spec 008 Phase 11/US9, T084): same scene, same
# RIB pair, run with Option B's unified per-bucket sample table instead of
# each hider drawing independently. Measured 2.95/3.12/3.00 across 3 runs
# (vs. ~5.81 uncorrelated) -- a genuine, stable improvement, so this variant
# gets its own tightened threshold rather than loosening the uncorrelated
# test above (which stays as-is; Option B is opt-in, not the default path).
add_parity_test(motion-patches-translate-correlated
    examples/rib/tests/parity/motion-patches-translate-reyes.rib
    motion-patches-translate-reyes.tif
    examples/rib/tests/parity/motion-patches-translate-raytrace.rib
    motion-patches-translate-raytrace.tif  8  "${VISUAL_ENV_CORRELATED}")

add_parity_test(motion-patches-deform
    examples/rib/tests/parity/motion-patches-deform-reyes.rib
    motion-patches-deform-reyes.tif
    examples/rib/tests/parity/motion-patches-deform-raytrace.rib
    motion-patches-deform-raytrace.tif  25)

add_parity_test(motion-polygons-translate
    examples/rib/tests/parity/motion-polygons-translate-reyes.rib
    motion-polygons-translate-reyes.tif
    examples/rib/tests/parity/motion-polygons-translate-raytrace.rib
    motion-polygons-translate-raytrace.tif  20)

add_parity_test(motion-polygons-deform
    examples/rib/tests/parity/motion-polygons-deform-reyes.rib
    motion-polygons-deform-reyes.tif
    examples/rib/tests/parity/motion-polygons-deform-raytrace.rib
    motion-polygons-deform-raytrace.tif  20)

add_parity_test(motion-quadrics-translate
    examples/rib/tests/parity/motion-quadrics-translate-reyes.rib
    motion-quadrics-translate-reyes.tif
    examples/rib/tests/parity/motion-quadrics-translate-raytrace.rib
    motion-quadrics-translate-raytrace.tif  20)

# Correlated-table variant (spec 008 Phase 11/US9, T084): measured
# 2.16/2.19/2.20/2.31 across 4 runs (vs. ~4.05 uncorrelated) -- the largest,
# most stable improvement of any candidate scene re-measured for T084. See
# motion-patches-translate-correlated above for why this is a new variant
# rather than a change to the uncorrelated test's threshold.
add_parity_test(motion-quadrics-translate-correlated
    examples/rib/tests/parity/motion-quadrics-translate-reyes.rib
    motion-quadrics-translate-reyes.tif
    examples/rib/tests/parity/motion-quadrics-translate-raytrace.rib
    motion-quadrics-translate-raytrace.tif  6  "${VISUAL_ENV_CORRELATED}")

add_parity_test(motion-quadrics-deform
    examples/rib/tests/parity/motion-quadrics-deform-reyes.rib
    motion-quadrics-deform-reyes.tif
    examples/rib/tests/parity/motion-quadrics-deform-raytrace.rib
    motion-quadrics-deform-raytrace.tif  20)

# --- Combined effect: FR-002's DOF+motion scene for this story (spec 008
# Phase 8/US6/S5) — three spheres at staggered depths through the same lens
# as the "dof" scene above (DepthOfField 2 1 5), with the middle sphere also
# translate-motion-blurred. Measured ~26.50, stable across repeated runs,
# well under the standalone "dof" scene's own residual (~45.94, threshold
# 60) — the combined scene's divergence is dominated by DOF's own D9
# residual (screen-space scatter vs true lens rays, explicitly documented
# as a bounded, not-closable-by-refactor residual, not this story's motion
# blur — motion alone converges to ~5-17 per the six scenes above).
# Threshold set at 40 (~1.5x measured, matching the margin convention used
# elsewhere in this file).
add_parity_test(dof-motion
    examples/rib/tests/parity/dof-motion-reyes.rib
    dof-motion-reyes.tif
    examples/rib/tests/parity/dof-motion-raytrace.rib
    dof-motion-raytrace.tif  40)

# --- Subdivision surfaces (spec 010-full-subdivision-support, US1) — same
# generic tessellation-path mechanism as the six patches/polygons/quadrics
# scenes above, exercised on CSubdivMesh/CSubdivision instead; confirms the
# mechanism needs zero subdivision-specific code (research.md R1/R2). The
# translate scene (transform-motion via MotionBegin-wrapped Translate)
# measured ~4.48, in line with the standard-threshold scenes above, so it
# keeps the default 20. The rotate scene (transform-motion via
# MotionBegin-wrapped Rotate, 75 degrees) measured ~37.8-38.3 across runs
# (confirmed stable, not an undersampling artifact -- doubling PixelSamples
# to 8x8 moved it by <0.5) -- a genuinely rotating Catmull-Clark limit
# surface's silhouette sweeps more screen area per shutter than the
# translate/deform cases above, producing more REYES-dicing-vs-raytrace-
# tessellation antialiasing residual on the edge. Threshold set at 45
# (~1.2x measured), the same margin-based convention used for dof-motion.
add_parity_test(motion-subdiv-translate
    examples/rib/tests/parity/motion-subdiv-translate-reyes.rib
    motion-subdiv-translate-reyes.tif
    examples/rib/tests/parity/motion-subdiv-translate-raytrace.rib
    motion-subdiv-translate-raytrace.tif  20)

add_parity_test(motion-subdiv-rotate
    examples/rib/tests/parity/motion-subdiv-rotate-reyes.rib
    motion-subdiv-rotate-reyes.tif
    examples/rib/tests/parity/motion-subdiv-rotate-raytrace.rib
    motion-subdiv-rotate-raytrace.tif  45)

# --- Subdivision surfaces facevarying seam (spec 010-full-subdivision-support,
# US2/T017-T020) — a 2x2-quad Catmull-Clark mesh whose center vertex is shared
# by all 4 faces, each carrying a distinct "facevarying float[2] st" corner
# value ((1.0,0.0)/(0.0,1.0)/(1.0,1.0)/(0.3,0.3)); show_st.sl colors each
# fragment by its (s,t), so a correctly-preserved seam renders 4 visibly
# distinct colors meeting at the vertex instead of collapsing to one (the
# T011-T016 bug this locks in a fix for). Both scenes pin
# Option "shaderformat" "default" ["rslo"] — the JIT (.slo) backend has an
# unrelated, pre-existing gap that drops show_st's explicit-colorspace
# "color \"rgb\" (s,t,0)" constructor entirely (see DEVNOTES_DETAILS/BUGS.md);
# pinning rslo sidesteps it and keeps this scene's result independent of the
# local machine's default shader-format configuration. ---
add_visual_test(subdiv-facevarying-seam-raytrace
    examples/rib/tests/subdiv-facevarying-seam-raytrace.rib
    subdiv-facevarying-seam-raytrace.tif  subdiv-facevarying-seam-raytrace.tif)

add_visual_test(subdiv-facevarying-seam-reyes
    examples/rib/tests/subdiv-facevarying-seam-reyes.rib
    subdiv-facevarying-seam-reyes.tif  subdiv-facevarying-seam-reyes.tif)

add_parity_test(subdiv-facevarying-seam
    examples/rib/tests/subdiv-facevarying-seam-reyes.rib
    subdiv-facevarying-seam-reyes.tif
    examples/rib/tests/subdiv-facevarying-seam-raytrace.rib
    subdiv-facevarying-seam-raytrace.tif  20)

# --- Subdivision surfaces: new tags (spec 010-full-subdivision-support,
# US3/T021-T028) — facevaryinginterpolateboundary, facevaryingpropagatecorners,
# and creasemethod, previously unsupported (any unrecognized tag hit
# CODE_BADTOKEN). Raytrace-only: these are geometry/tag-parsing correctness
# checks, not cross-hider parity checks, and the raytrace hider is this
# project's shading-correctness ground truth for facevarying/crease scenes
# (only per-ray shading, not REYES micropolygon dicing, genuinely
# supersamples shading detail). ---
add_visual_test(subdiv-new-tags-raytrace
    examples/rib/tests/subdiv-new-tags-raytrace.rib
    subdiv-new-tags-raytrace.tif  subdiv-new-tags-raytrace.tif)

# Single-tag isolation scenes (T025a) — each isolates one new tag's visible
# effect against an otherwise-identical mesh with the tag absent/defaulted.
# The facevaryingpropagatecorners scene requires "interpolateboundary" to be
# present: on this coarse 2x2-face mesh, omitting it lets the Catmull-Clark
# limit surface collapse to geometry no raytraced ray intersects at all
# (byte-identical blank renders regardless of the tag under test) -- a
# test-scene authoring pitfall, not a renderer defect.
add_visual_test(subdiv-tag-facevaryinginterpolateboundary-raytrace
    examples/rib/tests/subdiv-tag-facevaryinginterpolateboundary-raytrace.rib
    subdiv-tag-facevaryinginterpolateboundary-raytrace.tif
    subdiv-tag-facevaryinginterpolateboundary-raytrace.tif)

add_visual_test(subdiv-tag-facevaryingpropagatecorners-raytrace
    examples/rib/tests/subdiv-tag-facevaryingpropagatecorners-raytrace.rib
    subdiv-tag-facevaryingpropagatecorners-raytrace.tif
    subdiv-tag-facevaryingpropagatecorners-raytrace.tif)

add_visual_test(subdiv-tag-creasemethod-raytrace
    examples/rib/tests/subdiv-tag-creasemethod-raytrace.rib
    subdiv-tag-creasemethod-raytrace.tif
    subdiv-tag-creasemethod-raytrace.tif)

# Out-of-range tag value (T026) — facevaryinginterpolateboundary=99 must warn
# and fall back to the documented default (2), not error out.
add_visual_test(subdiv-new-tags-badvalue-raytrace
    examples/rib/tests/subdiv-new-tags-badvalue-raytrace.rib
    subdiv-new-tags-badvalue-raytrace.tif  subdiv-new-tags-badvalue-raytrace.tif)

# New tag combined with the existing hole/interpolateboundary tags (T027).
add_visual_test(subdiv-new-tags-with-hole-raytrace
    examples/rib/tests/subdiv-new-tags-with-hole-raytrace.rib
    subdiv-new-tags-with-hole-raytrace.tif  subdiv-new-tags-with-hole-raytrace.tif)

# ---------------------------------------------------------------------------
message(STATUS "  - Visual regression tests (${VISUAL_THRESHOLD}/255 threshold):")
message(STATUS "      53 scenes registered under label 'visual'")
message(STATUS "  - Cross-hider parity tests: 22 scene pairs registered under label 'parity'")
