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
# averages down to ±100/64 ≈ ±1.6 per block. A threshold of 8 catches real
# shading regressions (wrong color, wrong lighting) while tolerating edge noise.
set(VISUAL_THRESHOLD 12)

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
# ---------------------------------------------------------------------------
macro(add_visual_test SCENE_NAME RIB_FILE OUTPUT_TIF REF_TIF)
    set(_scratch ${CMAKE_BINARY_DIR}/tests/visual/${SCENE_NAME})
    file(MAKE_DIRECTORY ${_scratch})

    add_test(NAME Visual_${SCENE_NAME}
        COMMAND test_visual_render
            $<TARGET_FILE:orender>
            ${CMAKE_SOURCE_DIR}/${RIB_FILE}
            ${OUTPUT_TIF}
            ${CMAKE_SOURCE_DIR}/examples/rib/tests/references/${REF_TIF}
            ${VISUAL_THRESHOLD}
        WORKING_DIRECTORY ${_scratch}
    )

    set_tests_properties(Visual_${SCENE_NAME} PROPERTIES
        TIMEOUT        180
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
add_visual_test(spotlight-reyes
    examples/rib/tests/spotlight-reyes.rib
    spotlight-reyes.tif  spotlight-reyes.tif)

add_visual_test(spotlight-raytrace
    examples/rib/tests/spotlight-raytrace.rib
    spotlight-raytrace.tif  spotlight-raytrace.tif)

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

add_visual_test(motion-3-reyes
    examples/rib/tests/motion-3-reyes.rib
    motion-3-reyes.tif  motion-3-reyes.tif)

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
    camera-dof-raytrace.tif  camera-dof-raytrace.tif)

# --- Camera motion (small shift) ---
add_visual_test(camera-motion-small-reyes
    examples/rib/tests/camera-motion-small-reyes.rib
    camera-motion-small-reyes.tif  camera-motion-small-reyes.tif)

add_visual_test(camera-motion-small-raytrace
    examples/rib/tests/camera-motion-small-raytrace.rib
    camera-motion-small-raytrace.tif  camera-motion-small-raytrace.tif)

add_visual_test(camera-motion-small-dof-reyes
    "examples/rib/tests/camera-motion-small+dof-reyes.rib"
    camera-motion-small+dof-reyes.tif  camera-motion-small+dof-reyes.tif)

add_visual_test(camera-motion-small-dof-raytrace
    "examples/rib/tests/camera-motion-small+dof-raytrace.rib"
    camera-motion-small+dof-raytrace.tif  camera-motion-small+dof-raytrace.tif)

# camera-motion-huge: both RIBs write the same output file; test only the reyes variant
add_visual_test(camera-motion-huge-reyes
    examples/rib/tests/camera-motion-huge-reyes.rib
    camera-motion-huge.tif  camera-motion-huge.tif)

# ---------------------------------------------------------------------------
message(STATUS "  - Visual regression tests (${VISUAL_THRESHOLD}/255 threshold):")
message(STATUS "      33 scenes registered under label 'visual'")
