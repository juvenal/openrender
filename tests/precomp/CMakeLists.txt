# Spec 038 (Scene Interface untangle), US7: the precomputation tool's tests.
#
#   Precomp_OutputUnchanged   characterization: the bytes precomp writes, against the raw output of the
#                             unmodified tool (reference/stochasticPrimitives.raw.h), run from a scratch tree.
#                             Green on the starting tree by design.
#   Precomp_NoHiderCore       precomp.cpp includes neither reyes.h nor a core header. Red until Phase 7, so
#                             it carries the temporary label or038_red (tests/architecture/CMakeLists.txt).
add_test(NAME Precomp_OutputUnchanged
    COMMAND ${CMAKE_COMMAND} -DMODE=compare -DPRECOMP=$<TARGET_FILE:precomp>
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/work
            -DREFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/reference/stochasticPrimitives.raw.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_precomp_output.cmake)
set_tests_properties(Precomp_OutputUnchanged PROPERTIES TIMEOUT 300 LABELS "regression;precomp")

add_test(NAME Precomp_NoHiderCore
    COMMAND ${CMAKE_COMMAND}
            -DPRECOMP_SOURCE=${CMAKE_SOURCE_DIR}/src/precomp/precomp.cpp
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DMAP=${CMAKE_SOURCE_DIR}/tests/architecture/domain-map.txt
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_precomp_no_hider_core.cmake)
set_tests_properties(Precomp_NoHiderCore PROPERTIES TIMEOUT 60 LABELS "or038_red;precomp")

# Capture of the reference (scene-interface-capture-baselines, tests/CMakeLists.txt): written once, from the
# unmodified tool, and never replaced unless configured with -DOR038_RECAPTURE=ON.
add_custom_target(si_capture_precomp
    COMMAND ${CMAKE_COMMAND} -DMODE=${OR038_CAPTURE_MODE} -DRECAPTURE=${OR038_RECAPTURE} -DPRECOMP=$<TARGET_FILE:precomp>
            -DWORK=${CMAKE_CURRENT_BINARY_DIR}/capture_work
            -DREFERENCE=${CMAKE_CURRENT_SOURCE_DIR}/reference/stochasticPrimitives.raw.h
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_precomp_output.cmake
    DEPENDS precomp
    COMMENT "Capturing the precomp output reference (an existing reference is never overwritten)"
    VERBATIM)
add_dependencies(scene-interface-capture-baselines si_capture_precomp)
