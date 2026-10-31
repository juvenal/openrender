# Spec 026 (shading contract v2): the contract tests and the trace replay.
#
# Phase 1 (this file as first written): the trace replay only. The replay pins the rays Shading
# hands to the tracer, as recorded on the starting tree through CShadingContext::traceRequestObserver
# (a null-in-production test hook, src/libshader/shading/shading.h). Phase 2 fills in the contract
# programs behind OPENRENDER_SHADING_CONTRACT_TESTS.
#
# The replay tests are characterization tests (spec 028's motion replay is the precedent): green on
# the starting tree by construction, shown red by a mutation per builtin (validation.md section 2).
# tests/shading_contract/replay/{macos,linux}.txt are recorded once, on the starting tree, and are
# never re-recorded from a changed tree.
#
# Red-first convention (tasks.md): an executable is EXCLUDE_FROM_ALL and its test DISABLED until the
# phase that provides what it needs, so the main build and every checkpoint stay green.

# The contract programs (Phase 2) need headers that do not exist before Phase 3: OFF until then.
option(OPENRENDER_SHADING_CONTRACT_TESTS "Spec 026 contract programs (ON by default from Phase 3)" OFF)

set(SHADING_CONTRACT_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src/libshader/shading
    ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/replay ${CMAKE_BINARY_DIR})

# ---------------------------------------------------------------------------
# The replay shaders, compiled as build steps (as shaders/CMakeLists.txt does): .rslo always, .slo
# with the JIT. They live in the build tree, never next to the sources.
# ---------------------------------------------------------------------------
set(SHADING_CONTRACT_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
file(MAKE_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR})
set(_replay_shader_outputs "")
foreach(_b trace transmission visibility gather environment shadow occlusion indirectdiffuse)
    set(_sl ${CMAKE_CURRENT_SOURCE_DIR}/replay/shaders/replay_${_b}.sl)
    add_custom_command(
        OUTPUT ${SHADING_CONTRACT_SHADER_DIR}/replay_${_b}.rslo
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                -DOUT=${SHADING_CONTRACT_SHADER_DIR}/replay_${_b}.rslo
                -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes
                -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR}
        COMMENT ""
        VERBATIM)
    list(APPEND _replay_shader_outputs ${SHADING_CONTRACT_SHADER_DIR}/replay_${_b}.rslo)
    if(OPENRENDER_HAVE_LLVM)
        add_custom_command(
            OUTPUT ${SHADING_CONTRACT_SHADER_DIR}/replay_${_b}.slo
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                    -DOUT=${SHADING_CONTRACT_SHADER_DIR}/replay_${_b}.slo
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -DJIT=ON
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _replay_shader_outputs ${SHADING_CONTRACT_SHADER_DIR}/replay_${_b}.slo)
    endif()
endforeach()
add_custom_target(shading_contract_replay_shaders DEPENDS ${_replay_shader_outputs})

# The scenes use "constant" for the geometry the replay shader's rays hit: the shipped shaders, built
# by shaders/CMakeLists.txt, are on the search path after the replay shaders.
add_dependencies(shading_contract_replay_shaders openrender_shaders)

set(SHADING_CONTRACT_ENV
    "SHADERS=${SHADING_CONTRACT_SHADER_DIR}:${CMAKE_BINARY_DIR}/shaders"
    "ORENDERHOME=${CMAKE_SOURCE_DIR}"
    "DISPLAYS=$<TARGET_FILE_DIR:file>"
    "SHADING_CONTRACT_REPLAY_DIR=${CMAKE_CURRENT_SOURCE_DIR}/replay"
    "SHADING_CONTRACT_REPLAY_SCENES=${CMAKE_CURRENT_SOURCE_DIR}/replay/scenes")

# ---------------------------------------------------------------------------
# The replay: the recorder and the checker. Both render in process (one Ri session per child process,
# GitHub #2) and need the renderer library.
# ---------------------------------------------------------------------------
foreach(_t record check)
    add_executable(test_shading_contract_trace_replay_${_t} EXCLUDE_FROM_ALL replay/trace_replay_${_t}.cpp)
    target_compile_features(test_shading_contract_trace_replay_${_t} PRIVATE cxx_std_20)
    target_include_directories(test_shading_contract_trace_replay_${_t} PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_trace_replay_${_t} PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_trace_replay_${_t} shading_contract_replay_shaders)
    if(OPENRENDER_HAVE_LLVM)
        target_compile_definitions(test_shading_contract_trace_replay_${_t} PRIVATE SHADING_CONTRACT_HAVE_JIT=1)
    endif()
endforeach()

# ShadingContract_TraceReplayRecord: writes replay-fresh.txt into the build tree only. T023 enables it.
add_test(NAME ShadingContract_TraceReplayRecord COMMAND test_shading_contract_trace_replay_record
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(ShadingContract_TraceReplayRecord PROPERTIES
    TIMEOUT 600 LABELS "shading_contract-record" ENVIRONMENT "${SHADING_CONTRACT_ENV}" DISABLED TRUE)

# ShadingContract_TraceReplay_<builtin>: a fresh run against the recorded replay. T024 enables them.
foreach(_b trace transmission visibility gather environment shadow occlusion indirectdiffuse)
    set(_cwd ${CMAKE_CURRENT_BINARY_DIR}/cwd/TraceReplay_${_b})
    file(MAKE_DIRECTORY ${_cwd})
    add_test(NAME ShadingContract_TraceReplay_${_b} COMMAND test_shading_contract_trace_replay_check ${_b}
             WORKING_DIRECTORY ${_cwd})
    set_tests_properties(ShadingContract_TraceReplay_${_b} PROPERTIES
        TIMEOUT 300 LABELS "shading_contract" ENVIRONMENT "${SHADING_CONTRACT_ENV}" DISABLED TRUE)
endforeach()
