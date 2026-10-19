# Spec 039 (sampling foundation) unit tests.
#
# `ctest --test-dir build -L sampling` runs every test registered here, plus
# `DiskSampling` (tests/CMakeLists.txt), which has carried the `sampling`
# label since before spec 039.
#
# Phase 1 only creates the target. No test is registered yet; the sampler
# tests of Phase 2 register themselves with
#     set_tests_properties(<name> PROPERTIES TIMEOUT 60 LABELS "sampling;unit")
# and link the shared libRIBCommon, never the static archive (CLAUDE.md
# gotcha 22: a second copy would hold a second arena and diagnostic sink).
add_executable(test_sampling test_sampling.cpp)
target_compile_features(test_sampling PRIVATE cxx_std_20)
target_include_directories(test_sampling PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sampling PRIVATE openrendercommon)

# --- Phase 2 (US2): the sampler library tests -----------------------------------
#
# Written before src/common/sampling.h existed (contracts/sampler-api.md); the
# sampler library (src/common/sampling.*) now builds into libRIBCommon, so the
# targets are part of the default build.
#
# Each executable takes the case to run as its argument. They need no fixture
# or reference file: every expected value is computed in the test itself.
find_package(Threads REQUIRED)

# add_sampling_test_exe(<target> <source> [LIBS <libs>...])
function(add_sampling_test_exe target source)
    cmake_parse_arguments(ARG "" "" "LIBS" ${ARGN})
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
    target_link_libraries(${target} PRIVATE openrendercommon ${ARG_LIBS})
endfunction()

# add_sampling_test(<ctest name> <target> <case> [TIMEOUT <s>])
function(add_sampling_test name target case)
    cmake_parse_arguments(ARG "" "TIMEOUT" "" ${ARGN})
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 120)
    endif()
    add_test(NAME ${name} COMMAND ${target} ${case})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${ARG_TIMEOUT} LABELS "sampling;unit")
endfunction()

add_sampling_test_exe(test_sampling_reference test_reference.cpp)
add_sampling_test(Sampling_SobolReference test_sampling_reference sobol_reference)

add_sampling_test_exe(test_sampling_range_purity test_range_purity.cpp LIBS Threads::Threads)
add_sampling_test(Sampling_Range test_sampling_range_purity range)
add_sampling_test(Sampling_Purity test_sampling_range_purity purity)

add_sampling_test_exe(test_sampling_stratification test_stratification.cpp)
add_sampling_test(Sampling_NetProperty test_sampling_stratification net TIMEOUT 300)
add_sampling_test(Sampling_PMJ02Property test_sampling_stratification pmj02 TIMEOUT 300)

add_sampling_test_exe(test_sampling_uniformity test_uniformity.cpp)
add_sampling_test(Sampling_Uniformity test_sampling_uniformity uniformity)
add_sampling_test(Sampling_Discrepancy test_sampling_uniformity discrepancy TIMEOUT 300)

add_sampling_test_exe(test_sampling_independence test_independence.cpp)
add_sampling_test(Sampling_DrawIndependence test_sampling_independence draw_independence)
add_sampling_test(Sampling_Padding test_sampling_independence padding)
add_sampling_test(Sampling_Composite3D4D test_sampling_independence composite)

add_sampling_test_exe(test_sampling_convergence_smooth test_convergence_smooth.cpp)
add_sampling_test(Sampling_ConvergenceSmooth test_sampling_convergence_smooth convergence_smooth)

add_sampling_test_exe(test_sampling_camera_grid test_camera_grid.cpp)
add_sampling_test(Sampling_CameraGridConsistency test_sampling_camera_grid camera_grid)

add_sampling_test_exe(test_sampling_keys test_keys.cpp)
add_sampling_test(Sampling_Keys test_sampling_keys keys)

add_sampling_test_exe(test_sampling_keyed_stream test_keyed_stream.cpp LIBS Threads::Threads)
add_sampling_test(Sampling_KeyedStream test_sampling_keyed_stream keyed_stream)

# --- Phase 2 amendment: true PMJ02 construction and Morton-ordered Sobol camera ------
add_sampling_test_exe(test_sampling_pmj02_table test_pmj02_table.cpp LIBS Threads::Threads)
add_sampling_test(Sampling_PMJ02Construction test_sampling_pmj02_table construction)

add_sampling_test_exe(test_sampling_morton test_morton.cpp)
add_sampling_test(Sampling_MortonIndex test_sampling_morton morton)
add_sampling_test(Sampling_CameraSobolBlocks test_sampling_morton camera_sobol_blocks)

# --- Phase 4 (US6): identity plumbing, tests first ---------------------------------
#
# Written before CObject::stableId and CShadingContext::pointKeyObserver existed
# (see scene_probe.h); both are there now, so the targets are part of the default
# build. They run
# real Ri sessions, so they link the renderer, in a scratch directory (each render
# writes scene_probe.tif) with ORENDERHOME at the source root (no .orenderrc, so a
# Display there cannot make these sessions open a file display), and one case per
# process (GitHub #2).
set(SCENE_PROBE_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src/libshader/shading
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
set(SCENE_PROBE_ENV "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR};DISPLAYS=$<TARGET_FILE_DIR:file>")

# add_scene_probe_test(<target> <source> <ctest prefix> <case>...)
function(add_scene_probe_test target source prefix)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE ${SCENE_PROBE_INCLUDES})
    target_link_libraries(${target} PRIVATE ribRender libshader_shading openrendercommon)
    foreach(_case ${ARGN})
        set(_scratch ${CMAKE_BINARY_DIR}/tests/unit/sampling/${prefix}_${_case})
        file(MAKE_DIRECTORY ${_scratch})
        add_test(NAME ${prefix}_${_case} COMMAND ${target} ${_case} WORKING_DIRECTORY ${_scratch})
        set_tests_properties(${prefix}_${_case} PROPERTIES TIMEOUT 120 LABELS "sampling;unit"
            ENVIRONMENT "${SCENE_PROBE_ENV}")
    endforeach()
endfunction()

# T038: stable object identity
add_scene_probe_test(test_sampling_object_ids test_object_ids.cpp Sampling_ObjectIds
    scene_loads instances delayed tessellation splits)

# T042: no two shading points of one object share a key, per primitive family
add_scene_probe_test(test_sampling_cell_keys test_cell_keys.cpp Sampling_CellKeyUniqueness
    patch mesh quadric nurbs subdivision points curves)

# --- Phase 5 (US3): correlated camera samples, tests first ---------------------------
#
# Written before the shared absolute-subpixel function existed: CSampler's
# reyesAbsSub / raytraceAbsSub / cameraSample are the test seam (src/ri/hiders/
# sampler.h), stubs until T060. No Ri session is needed, but CSampler lives in the
# renderer library.
add_sampling_test_exe(test_sampling_camera_slots test_camera_slots.cpp
    LIBS ribRender libshader_shading)
target_include_directories(test_sampling_camera_slots PRIVATE ${CMAKE_SOURCE_DIR}/src/ri/hiders)
add_sampling_test(Sampling_CameraSlotsEqual test_sampling_camera_slots camera_slots)

# Phase 6 follow-up: the displacement-only passes. Written before the fix (tests first); needs the
# displaced-position hook (CShadingContext::pointDisplacedObserver).
add_scene_probe_test(test_sampling_displace_consistency test_displace_consistency.cpp
    Sampling_DisplacePassConsistency sphere)
