# Motion and camera replay (spec 028, camera-and-motion-layer).
#
# motion_replay is a characterization tool: it drives the primitives' CObject::intersect()
# and CObject::sample() through in-process Ri sessions and writes (--record) or compares
# (--check) every result as C99 hex floats. The replay file in replay/<platform>.txt is
# recorded ONCE per platform on the starting tree (master 4eb683b), before any src/ change,
# and is never re-recorded from a changed tree (contracts/replay-format.md rule 2).
#
# The tool uses starting-tree interfaces only (the Ri API, CObject::intersect(), sample(),
# materialize()), so it builds and records on the unchanged tree.
#
# `ctest --test-dir build -L motion` selects the replay tests; the record test carries the
# separate label "motion-record" (so it is not part of `-L '^motion$'`).

# <platform> names the replay file: macos or linux, nothing else.
if(CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    set(MOTION_REPLAY_PLATFORM macos)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(MOTION_REPLAY_PLATFORM linux)
else()
    message(FATAL_ERROR "tests/unit/motion: no replay platform name for CMAKE_SYSTEM_NAME=${CMAKE_SYSTEM_NAME} (expected Darwin or Linux)")
endif()

set(MOTION_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libshader/shading)

# Test-only plug-ins for the unchanged implicit.h and dlo.h ABIs. They compile no src/ source:
# they are the first coverage of CImplicit and CDLObject (research R14). Both must be named by
# an absolute path: the dlo branch of RiGeometry hands the name to osLoadModule() unchanged.
foreach(_plugin implicit_moving_sphere dlo_sphere)
    add_library(${_plugin} MODULE fixtures/${_plugin}.cpp)
    target_compile_features(${_plugin} PRIVATE cxx_std_20)
    target_link_libraries(${_plugin} PRIVATE openrender_common_flags)
    target_include_directories(${_plugin} PRIVATE ${CMAKE_SOURCE_DIR}/src/ri/geometry)
    set_target_properties(${_plugin} PROPERTIES PREFIX ""
        LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/fixtures)
endforeach()

add_executable(motion_replay motion_replay.cpp)
target_compile_features(motion_replay PRIVATE cxx_std_20)
target_link_libraries(motion_replay PRIVATE openrender_common_flags ribRender openrendercommon)
target_include_directories(motion_replay PRIVATE ${MOTION_TEST_INCLUDES})
target_compile_definitions(motion_replay PRIVATE
    MOTION_REPLAY_PLATFORM="${MOTION_REPLAY_PLATFORM}"
    MOTION_REPLAY_IMPLICIT_PLUGIN="$<TARGET_FILE:implicit_moving_sphere>"
    MOTION_REPLAY_DLO_PLUGIN="$<TARGET_FILE:dlo_sphere>")
add_dependencies(motion_replay implicit_moving_sphere dlo_sphere)

# The in-process Ri sessions follow tests/unit/csg: ORENDERHOME points at the source root, which
# has no .orenderrc. tests/visual's .orenderrc sets a Display and Format, so RiWorldEnd would try
# to open the file display, and a failed display load followed by another RiBegin/RiEnd cycle in
# the same process crashes (GitHub #2); this tool runs one session per scene in one process.
set(MOTION_REPLAY_ENV "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")

# Recording writes ONLY into the build tree, so running it on a later tree cannot overwrite the
# tracked replay file (contracts/replay-format.md rule 2). Copying the result to
# replay/<platform>.txt is a deliberate, starting-tree-only step (task T015).
add_test(NAME Motion_ReplayRecord
    COMMAND motion_replay --record ${CMAKE_CURRENT_BINARY_DIR}/replay-recorded-${MOTION_REPLAY_PLATFORM}.txt)
set_tests_properties(Motion_ReplayRecord PROPERTIES
    TIMEOUT 300
    LABELS "motion-record"
    ENVIRONMENT "${MOTION_REPLAY_ENV}")

# One characterization test per family: motion_replay --check compares the scenes of that family
# with the replay file recorded on the starting tree. A missing file for the platform fails with
# "no replay file for <platform>"; it is never skipped. Green on the starting tree by construction;
# the red evidence is one mutation per family in a scratch copy (task T016).
set(MOTION_REPLAY_FILE ${CMAKE_CURRENT_SOURCE_DIR}/replay/${MOTION_REPLAY_PLATFORM}.txt)
foreach(_family quadrics raytransform bilinear polygons implicit tessellation gridonly emptyshutter)
    add_test(NAME Motion_Replay_${_family}
        COMMAND motion_replay --check ${MOTION_REPLAY_FILE} --family ${_family})
    set_tests_properties(Motion_Replay_${_family} PROPERTIES
        TIMEOUT 120
        LABELS "motion;unit"
        ENVIRONMENT "${MOTION_REPLAY_ENV}")
endforeach()

# ---------------------------------------------------------------------------
# The evaluator and frame-parameter tests (spec 028, tasks T027 to T031)
#
# The four State-only tests link the State-only library of tests/unit/state (see tests/unit/camera) and
# add src/ri/geometry for motion.h; the narrow path excludes src/ri/render, src/libshader, src/ri/hiders
# and src/ri/core, so motion.h cannot include a hider, the renderer core or a shading header
# (Motion_Isolation, FR-012). They exist only when OPENRENDER_CAMERA_MOTION_ONLY_TESTS is ON.
# ---------------------------------------------------------------------------
if(OPENRENDER_CAMERA_MOTION_ONLY_TESTS)
    add_camera_motion_test(Motion_TwoKeyModel test_motion_two_key motion test_motion_two_key.cpp ENABLED
        INCLUDES ${CMAKE_SOURCE_DIR}/src/ri/geometry)
    add_camera_motion_test(Motion_Isolation test_motion_isolation motion test_motion_isolation.cpp ENABLED
        INCLUDES ${CMAKE_SOURCE_DIR}/src/ri/geometry)
    add_camera_motion_test(Motion_CameraSlerpEquivalence test_motion_camera_slerp motion test_motion_camera_slerp.cpp ENABLED
        INCLUDES ${CMAKE_SOURCE_DIR}/src/ri/geometry)
    add_camera_motion_test(Motion_TransformRayEquivalence test_motion_transform_ray motion test_motion_transform_ray.cpp ENABLED
        INCLUDES ${CMAKE_SOURCE_DIR}/src/ri/geometry)
endif()

# Frame_ParamsEqualCore is a Consumer (it drives in-process Ri sessions, like motion_replay) and needs no
# option. Enabled since task T045 (the foundation phase provides frameParams.h and the core installs it).
add_executable(test_frame_params_equal_core test_frame_params_equal_core.cpp)
target_compile_features(test_frame_params_equal_core PRIVATE cxx_std_20)
target_link_libraries(test_frame_params_equal_core PRIVATE openrender_common_flags ribRender openrendercommon)
target_include_directories(test_frame_params_equal_core PRIVATE ${MOTION_TEST_INCLUDES})
add_test(NAME Frame_ParamsEqualCore COMMAND test_frame_params_equal_core)
set_tests_properties(Frame_ParamsEqualCore PROPERTIES
    TIMEOUT 120
    LABELS "motion;unit"
    ENVIRONMENT "${MOTION_REPLAY_ENV}")
