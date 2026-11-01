# Spec 026 (shading contract v2): the contract tests and the trace replay.
#
# Phase 1: the trace replay. It pins the rays Shading hands to the tracer, as recorded on the starting
# tree through CShadingContext::traceRequestObserver (a null-in-production test hook,
# src/libshader/shading/shading.h).
# Phase 2: the contract programs and their tests, written before the headers they use exist; they are
# behind OPENRENDER_SHADING_CONTRACT_TESTS (the second half of this file).
#
# The replay tests are characterization tests (spec 028's motion replay is the precedent): green on
# the starting tree by construction, shown red by a mutation per builtin (validation.md section 2).
# tests/shading_contract/replay/{macos,linux}.txt are recorded once, on the starting tree, and are
# never re-recorded from a changed tree.
#
# Red-first convention (tasks.md): an executable is EXCLUDE_FROM_ALL (the replay ones joined the build at T021, once the hook existed) and its test DISABLED until the
# phase that provides what it needs, so the main build and every checkpoint stay green.

# The contract programs (Phase 2) need the headers Phase 3 creates: ON by default from Phase 3.
option(OPENRENDER_SHADING_CONTRACT_TESTS "Spec 026 contract programs" ON)

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
# (source file, base name): the replay shaders live under replay/shaders, the contract-test shaders under shaders
set(_contract_shader_names
    cm_attribute cm_option cm_rendererinfo cm_surface cm_displacement cm_atmosphere cm_incident cm_opposite
    cm_lightsource cm_lightsource_noloop cm_producer_surface cm_producer_displacement cm_producer_volume cm_producer_light
    cm_matrices cm_trace cm_transmission cm_visibility cm_gather cm_occlusion cm_indirect cm_env cm_shadow
    sit_marker sit_mirror sit_transmit sit_displace)
set(_shader_specs "")
foreach(_b trace transmission visibility gather environment shadow occlusion indirectdiffuse)
    list(APPEND _shader_specs "replay_${_b}|${CMAKE_CURRENT_SOURCE_DIR}/replay/shaders/replay_${_b}.sl")
endforeach()
foreach(_n IN LISTS _contract_shader_names)
    list(APPEND _shader_specs "${_n}|${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_n}.sl")
endforeach()
foreach(_spec IN LISTS _shader_specs)
    string(REPLACE "|" ";" _parts "${_spec}")
    list(GET _parts 0 _b)
    list(GET _parts 1 _sl)
    add_custom_command(
        OUTPUT ${SHADING_CONTRACT_SHADER_DIR}/${_b}.rslo
        COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                -DOUT=${SHADING_CONTRACT_SHADER_DIR}/${_b}.rslo
                -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes
                -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        DEPENDS ${_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
        WORKING_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR}
        COMMENT ""
        VERBATIM)
    list(APPEND _replay_shader_outputs ${SHADING_CONTRACT_SHADER_DIR}/${_b}.rslo)
    if(OPENRENDER_HAVE_LLVM)
        add_custom_command(
            OUTPUT ${SHADING_CONTRACT_SHADER_DIR}/${_b}.slo
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                    -DOUT=${SHADING_CONTRACT_SHADER_DIR}/${_b}.slo
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -DJIT=ON
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _replay_shader_outputs ${SHADING_CONTRACT_SHADER_DIR}/${_b}.slo)
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
    "SHADING_CONTRACT_REPLAY_SCENES=${CMAKE_CURRENT_SOURCE_DIR}/replay/scenes"
    "SHADING_CONTRACT_SHADERS=${SHADING_CONTRACT_SHADER_DIR}"
    "SHADING_CONTRACT_SCENES=${CMAKE_CURRENT_SOURCE_DIR}/scenes")

# ---------------------------------------------------------------------------
# The replay: the recorder and the checker. Both render in process (one Ri session per child process,
# GitHub #2) and need the renderer library.
# ---------------------------------------------------------------------------
foreach(_t record check)
    add_executable(test_shading_contract_trace_replay_${_t} replay/trace_replay_${_t}.cpp)
    target_compile_features(test_shading_contract_trace_replay_${_t} PRIVATE cxx_std_20)
    target_include_directories(test_shading_contract_trace_replay_${_t} PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_trace_replay_${_t} PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_trace_replay_${_t} shading_contract_replay_shaders)
    if(OPENRENDER_HAVE_LLVM)
        target_compile_definitions(test_shading_contract_trace_replay_${_t} PRIVATE SHADING_CONTRACT_HAVE_JIT=1)
    endif()
endforeach()

# ShadingContract_TraceReplayRecord: writes replay-fresh.txt into the build tree only. Enabled at T023.
add_test(NAME ShadingContract_TraceReplayRecord COMMAND test_shading_contract_trace_replay_record
         WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(ShadingContract_TraceReplayRecord PROPERTIES
    TIMEOUT 600 LABELS "shading_contract-record" ENVIRONMENT "${SHADING_CONTRACT_ENV}")

# ShadingContract_TraceReplay_<builtin>: a fresh run against the recorded replay. Enabled at T024.
foreach(_b trace transmission visibility gather environment shadow occlusion indirectdiffuse)
    set(_cwd ${CMAKE_CURRENT_BINARY_DIR}/cwd/TraceReplay_${_b})
    file(MAKE_DIRECTORY ${_cwd})
    add_test(NAME ShadingContract_TraceReplay_${_b} COMMAND test_shading_contract_trace_replay_check ${_b}
             WORKING_DIRECTORY ${_cwd})
    set_tests_properties(ShadingContract_TraceReplay_${_b} PROPERTIES
        TIMEOUT 300 LABELS "shading_contract" ENVIRONMENT "${SHADING_CONTRACT_ENV}")
endforeach()

# ---------------------------------------------------------------------------
# The contract programs (spec 026, Phase 2). Written before the headers they use exist
# (serviceTypes.h, tracer.h, shaderGlobals.h, situation.h, sloLoader.h arrive in Phase 3), so they are
# built only with OPENRENDER_SHADING_CONTRACT_TESTS (OFF until Phase 3, ON by default from then). The
# standalone and the rendered program are part of the default build from Phase 3 (the headers exist); the
# situations program stays EXCLUDE_FROM_ALL until Phase 9, because it reads report fields only Phase 9 adds. The tests are registered either way and every one is DISABLED until the
# phase named beside it enables it (red-first convention: a disabled test is "Not Run", never a failure).
# Red evidence: a scratch build with -DOPENRENDER_SHADING_CONTRACT_TESTS=ON fails to compile them.
#
#   test_shading_contract_standalone   libshader_shading + libRIBCommon + the State-only library + the
#                                      load stub + the services double. NO ribRender (SC-001).
#   test_shading_contract_rendered     adds ribRender: real objects, real bound shaders, the Hiders' bundles
#   test_shading_contract_situations   as the rendered program, its own executable (reads report fields
#                                      only Phase 9 adds)
# ---------------------------------------------------------------------------
set(SHADING_CONTRACT_PROGRAM_DEFS "")
if(OPENRENDER_HAVE_LLVM)
    list(APPEND SHADING_CONTRACT_PROGRAM_DEFS SHADING_CONTRACT_HAVE_JIT=1)
endif()

if(OPENRENDER_SHADING_CONTRACT_TESTS)
    if(NOT TARGET state_only_sources)
        message(FATAL_ERROR "OPENRENDER_SHADING_CONTRACT_TESTS needs the State-only library: enable OPENRENDER_STATE_ONLY_TESTS")
    endif()
    add_executable(test_shading_contract_standalone
        standalone_main.cpp case_link.cpp case_attributes.cpp case_messages.cpp case_matrices.cpp
        case_trace_requests.cpp case_no_services.cpp case_globals_double.cpp load_stub.cpp)
    target_compile_features(test_shading_contract_standalone PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_standalone PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_standalone PRIVATE ${SHADING_CONTRACT_INCLUDES} ${CMAKE_SOURCE_DIR}/src/display)
    # No ribRender, no ribRender_static: the link line is the test (StandaloneLinkLine)
    target_link_libraries(test_shading_contract_standalone PRIVATE openrender_common_flags libshader_shading openrendercommon state_only_sources)
    add_dependencies(test_shading_contract_standalone shading_contract_replay_shaders)

    add_executable(test_shading_contract_rendered
        rendered_main.cpp case_real_answers.cpp case_gather_requests.cpp case_canned_trace.cpp case_no_tracer.cpp case_double_globals.cpp)
    target_compile_features(test_shading_contract_rendered PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_rendered PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_rendered PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_rendered PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_rendered shading_contract_replay_shaders)

    add_executable(test_shading_contract_situations EXCLUDE_FROM_ALL situations_main.cpp case_situations.cpp)
    target_compile_features(test_shading_contract_situations PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_situations PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_situations PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_situations PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_situations shading_contract_replay_shaders)
endif()

# contract_program(<var> <program>): the command that runs a program. With the option ON it is the target;
# with it OFF no target exists, so the (disabled) test gets a placeholder that `ctest -N` can find.
function(contract_program VAR PROGRAM)
    if(OPENRENDER_SHADING_CONTRACT_TESTS)
        set(${VAR} "$<TARGET_FILE:${PROGRAM}>" PARENT_SCOPE)
    else()
        set(${VAR} "${CMAKE_COMMAND};-E;true" PARENT_SCOPE)
    endif()
endfunction()

# add_contract_test(<test name> <program> <case>... [JIT] [ENABLED])
#   One CTest entry per case, DISABLED until the phase that provides what it needs enables it. The entry
#   exists (disabled) whether or not the program is configured.
function(add_contract_test NAME PROGRAM CASE)
    cmake_parse_arguments(ARG "JIT;ENABLED" "" "" ${ARGN})
    if(ARG_JIT AND NOT OPENRENDER_HAVE_LLVM)
        return()
    endif()
    set(_cwd ${CMAKE_CURRENT_BINARY_DIR}/cwd/${NAME})
    file(MAKE_DIRECTORY ${_cwd})
    contract_program(_cmd ${PROGRAM})
    if(OPENRENDER_SHADING_CONTRACT_TESTS)
        add_test(NAME ${NAME} COMMAND ${_cmd} ${CASE} WORKING_DIRECTORY ${_cwd})
    else()
        add_test(NAME ${NAME} COMMAND ${CMAKE_COMMAND} -E true WORKING_DIRECTORY ${_cwd})
    endif()
    set_tests_properties(${NAME} PROPERTIES TIMEOUT 120 LABELS "shading_contract"
        ENVIRONMENT "${SHADING_CONTRACT_ENV}")
    if(NOT ARG_ENABLED)
        set_tests_properties(${NAME} PROPERTIES DISABLED TRUE)
    endif()
endfunction()

# Phase 3 (the foundation: the headers, the version, the stub, the loader move).
# The two test-code edits of tools/phase3-test-fixes.diff (contract_services.h retrieveVariable; the key copy in case_globals_double.cpp)
# were confirmed by the maintainer 2026-10-31; the four tests below are ENABLED.
add_contract_test(ShadingContract_StandaloneLink test_shading_contract_standalone StandaloneLink ENABLED)
add_test(NAME ShadingContract_StandaloneLinkLine
         COMMAND ${CMAKE_COMMAND} -DBUILD_DIR=${CMAKE_CURRENT_BINARY_DIR} -DTARGET=test_shading_contract_standalone
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_link_line.cmake)
set_tests_properties(ShadingContract_StandaloneLinkLine PROPERTIES TIMEOUT 60 LABELS "shading_contract")
add_contract_test(ShadingContract_ShaderGlobalsDouble test_shading_contract_standalone ShaderGlobalsDouble ENABLED)
add_contract_test(ShadingContract_DoubleGlobalsMatchRenderer test_shading_contract_rendered DoubleGlobalsMatchRenderer ENABLED)

# Phase 4 (getMatrix)
add_contract_test(ShadingContract_MatricesInterp test_shading_contract_standalone MatricesInterp)
add_contract_test(ShadingContract_MatricesJit test_shading_contract_standalone MatricesJit JIT)
add_contract_test(ShadingContract_NoServices_Matrix test_shading_contract_standalone NoServices_Matrix)
add_contract_test(ShadingContract_RealAnswersInterp_Matrices test_shading_contract_rendered RealAnswersInterp_Matrices)
add_contract_test(ShadingContract_RealAnswersJit_Matrices test_shading_contract_rendered RealAnswersJit_Matrices JIT)

# Phase 5 (getAttribute)
add_contract_test(ShadingContract_AttributesInterp test_shading_contract_standalone AttributesInterp)
add_contract_test(ShadingContract_AttributesJit test_shading_contract_standalone AttributesJit JIT)
add_contract_test(ShadingContract_NoServices_Attribute test_shading_contract_standalone NoServices_Attribute)
add_contract_test(ShadingContract_RealAnswersInterp_Attributes test_shading_contract_rendered RealAnswersInterp_Attributes)
add_contract_test(ShadingContract_RealAnswersJit_Attributes test_shading_contract_rendered RealAnswersJit_Attributes JIT)

# Phase 6 (getMessage)
add_contract_test(ShadingContract_MessagesInterp test_shading_contract_standalone MessagesInterp)
add_contract_test(ShadingContract_MessagesJit test_shading_contract_standalone MessagesJit JIT)
add_contract_test(ShadingContract_NoServices_Message test_shading_contract_standalone NoServices_Message)
add_contract_test(ShadingContract_RealAnswersInterp_Messages test_shading_contract_rendered RealAnswersInterp_Messages)
add_contract_test(ShadingContract_RealAnswersJit_Messages test_shading_contract_rendered RealAnswersJit_Messages JIT)

# Phase 8 (the tracer)
add_contract_test(ShadingContract_TraceRequestsInterp test_shading_contract_standalone TraceRequestsInterp)
add_contract_test(ShadingContract_TraceRequestsJit test_shading_contract_standalone TraceRequestsJit JIT)
add_contract_test(ShadingContract_NoServices_Tracer test_shading_contract_standalone NoServices_Tracer)
add_contract_test(ShadingContract_GatherRequestsInterp test_shading_contract_rendered GatherRequestsInterp)
add_contract_test(ShadingContract_GatherRequestsJit test_shading_contract_rendered GatherRequestsJit JIT)
add_contract_test(ShadingContract_CannedTraceInterp test_shading_contract_rendered CannedTraceInterp)
add_contract_test(ShadingContract_CannedTraceJit test_shading_contract_rendered CannedTraceJit JIT)
add_contract_test(ShadingContract_NoTracer test_shading_contract_rendered NoTracer)

# Phase 9 (ShaderGlobals, the situations): its own executable
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/ShadingContract_Situations)
if(OPENRENDER_SHADING_CONTRACT_TESTS)
    add_test(NAME ShadingContract_Situations COMMAND $<TARGET_FILE:test_shading_contract_situations>
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/ShadingContract_Situations)
else()
    add_test(NAME ShadingContract_Situations COMMAND ${CMAKE_COMMAND} -E true
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/ShadingContract_Situations)
endif()
set_tests_properties(ShadingContract_Situations PROPERTIES TIMEOUT 300 LABELS "shading_contract"
    ENVIRONMENT "${SHADING_CONTRACT_ENV}" DISABLED TRUE)
