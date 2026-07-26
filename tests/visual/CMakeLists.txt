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

# ---------------------------------------------------------------------------
# Environment common to all visual tests
# ---------------------------------------------------------------------------
set(VISUAL_ENV
    "SHADERS=${CMAKE_SOURCE_DIR}/shaders"
    "ORENDERHOME=${CMAKE_SOURCE_DIR}/openrender"
    "DISPLAYS=${CMAKE_SOURCE_DIR}/openrender/displays"
    "GEOMETRIES=${CMAKE_SOURCE_DIR}/openrender/geometry"
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

# ---------------------------------------------------------------------------
message(STATUS "  - Visual regression tests (${VISUAL_THRESHOLD}/255 threshold):")
message(STATUS "      44 scenes registered under label 'visual'")
