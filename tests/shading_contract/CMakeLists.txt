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
    cm_mm_attribute cm_mm_option cm_mm_rendererinfo cm_mm_real cm_nullobject
    cm_producer_light_big cm_lightsource_big
    cm_mm_msg_surface cm_mm_msg_displacement cm_mm_msg_atmosphere cm_mm_msg_incident cm_mm_msg_opposite
    cm_mm_real_msg_surface cm_mm_real_msg_displacement cm_mm_real_msg_atmosphere cm_mm_real_msg_incident cm_mm_real_msg_opposite cm_nullobject_msg
    cm_matrices cm_trace cm_transmission cm_visibility cm_gather cm_occlusion cm_indirect cm_env cm_shadow
    sit_marker sit_mirror sit_transmit sit_displace sit_photon_light)
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
    "SHADING_CONTRACT_SCENES=${CMAKE_CURRENT_SOURCE_DIR}/scenes"
    # spec 029: the directory of the implicit-surface plug-in fixture (tests/plugins/implicit, good.dsply) that the
    # implicit-shadow scene loads through the procedural search path
    "SHADING_CONTRACT_IMPLICIT_DIR=${CMAKE_BINARY_DIR}/tests/plugins/implicit/good")

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
#                                      services double. NO ribRender, no load stub (SC-001).
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
        case_trace_requests.cpp case_no_services.cpp case_globals_double.cpp case_type_mismatch_attribute.cpp case_type_mismatch_message.cpp case_lightsource_no_loop.cpp)
    target_compile_features(test_shading_contract_standalone PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_standalone PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_standalone PRIVATE ${SHADING_CONTRACT_INCLUDES} ${CMAKE_SOURCE_DIR}/src/display)
    # No ribRender, no ribRender_static: the link line is the test (StandaloneLinkLine)
    target_link_libraries(test_shading_contract_standalone PRIVATE openrender_common_flags libshader_shading openrendercommon state_only_sources)
    add_dependencies(test_shading_contract_standalone shading_contract_replay_shaders)

    add_executable(test_shading_contract_rendered
        rendered_main.cpp case_real_answers.cpp case_gather_requests.cpp case_canned_trace.cpp case_no_tracer.cpp case_double_globals.cpp case_null_object.cpp case_type_mismatch_real_message.cpp case_lightsource_no_loop_real.cpp case_null_object_messages.cpp case_lightsource_oversized.cpp)
    target_compile_features(test_shading_contract_rendered PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_rendered PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_rendered PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_rendered PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_rendered shading_contract_replay_shaders)

    # The attribute family's real-services mismatch test calls the query wrappers with the QueryType argument that
    # Phase 5 adds, so it could not compile before then (B10): an executable of its own, EXCLUDE_FROM_ALL until the
    # implementation made it compile (T105); its red evidence was the failed build of this target.
    add_executable(test_shading_contract_typemismatch_attr typemismatch_attr_main.cpp case_type_mismatch_real.cpp)
    target_compile_features(test_shading_contract_typemismatch_attr PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_typemismatch_attr PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_typemismatch_attr PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_typemismatch_attr PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_typemismatch_attr shading_contract_replay_shaders)
    # The offending-object test asserts setOffendingObject(ObjectHandle), which exists from Phase 7 on (C6, B10): an
    # executable of its own, EXCLUDE_FROM_ALL until the implementation makes it compile (T125/T131). Its red evidence
    # is the failed build of this target.
    add_executable(test_shading_contract_offending case_offending_object.cpp)
    target_compile_features(test_shading_contract_offending PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_contract_offending PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
    target_include_directories(test_shading_contract_offending PRIVATE ${SHADING_CONTRACT_INCLUDES})
    target_link_libraries(test_shading_contract_offending PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
    add_dependencies(test_shading_contract_offending shading_contract_replay_shaders)
    add_executable(test_shading_contract_situations situations_main.cpp case_situations.cpp)
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
add_contract_test(ShadingContract_MatricesInterp test_shading_contract_standalone MatricesInterp ENABLED)
add_contract_test(ShadingContract_MatricesJit test_shading_contract_standalone MatricesJit JIT ENABLED)
add_contract_test(ShadingContract_NoServices_Matrix test_shading_contract_standalone NoServices_Matrix ENABLED)
add_contract_test(ShadingContract_RealAnswersInterp_Matrices test_shading_contract_rendered RealAnswersInterp_Matrices ENABLED)
add_contract_test(ShadingContract_RealAnswersJit_Matrices test_shading_contract_rendered RealAnswersJit_Matrices JIT ENABLED)

# Phase 5 (getAttribute)
add_contract_test(ShadingContract_AttributesInterp test_shading_contract_standalone AttributesInterp ENABLED)
add_contract_test(ShadingContract_AttributesJit test_shading_contract_standalone AttributesJit JIT ENABLED)
add_contract_test(ShadingContract_NoServices_Attribute test_shading_contract_standalone NoServices_Attribute ENABLED)
add_contract_test(ShadingContract_RealAnswersInterp_Attributes test_shading_contract_rendered RealAnswersInterp_Attributes ENABLED)
add_contract_test(ShadingContract_RealAnswersJit_Attributes test_shading_contract_rendered RealAnswersJit_Attributes JIT ENABLED)
add_contract_test(ShadingContract_TypeMismatch_AttributeInterp test_shading_contract_standalone TypeMismatch_AttributeInterp ENABLED)
add_contract_test(ShadingContract_TypeMismatch_AttributeJit test_shading_contract_standalone TypeMismatch_AttributeJit JIT ENABLED)
add_contract_test(ShadingContract_TypeMismatchReal_AttributeInterp test_shading_contract_typemismatch_attr TypeMismatchReal_AttributeInterp ENABLED)
add_contract_test(ShadingContract_TypeMismatchReal_AttributeJit test_shading_contract_typemismatch_attr TypeMismatchReal_AttributeJit JIT ENABLED)
add_contract_test(ShadingContract_NullObject_AttributesInterp test_shading_contract_rendered NullObject_AttributesInterp ENABLED)
add_contract_test(ShadingContract_NullObject_AttributesJit test_shading_contract_rendered NullObject_AttributesJit JIT ENABLED)
# F4 (FR-029): the accessor constants of incident() and opposite() agree with the message source they ask for
add_test(NAME ShadingContract_MessageAccessors
         COMMAND ${CMAKE_COMMAND} -DSHADER_FUNCTIONS=${CMAKE_SOURCE_DIR}/src/libshader/shading/shaderFunctions.h
                 -DSHADING_CPP=${CMAKE_SOURCE_DIR}/src/libshader/shading/shading.cpp
                 -P ${CMAKE_CURRENT_SOURCE_DIR}/check_message_accessors.cmake)
set_tests_properties(ShadingContract_MessageAccessors PROPERTIES TIMEOUT 60 LABELS "shading_contract")

# Phase 6 (getMessage)
add_contract_test(ShadingContract_MessagesInterp test_shading_contract_standalone MessagesInterp ENABLED)
add_contract_test(ShadingContract_MessagesJit test_shading_contract_standalone MessagesJit JIT ENABLED)
add_contract_test(ShadingContract_NoServices_Message test_shading_contract_standalone NoServices_Message ENABLED)
add_contract_test(ShadingContract_RealAnswersInterp_Messages test_shading_contract_rendered RealAnswersInterp_Messages ENABLED)
add_contract_test(ShadingContract_RealAnswersJit_Messages test_shading_contract_rendered RealAnswersJit_Messages JIT ENABLED)
add_contract_test(ShadingContract_TypeMismatch_MessageInterp test_shading_contract_standalone TypeMismatch_MessageInterp ENABLED)
add_contract_test(ShadingContract_TypeMismatch_MessageJit test_shading_contract_standalone TypeMismatch_MessageJit JIT ENABLED)
add_contract_test(ShadingContract_LightsourceNoLoopInterp test_shading_contract_standalone LightsourceNoLoopInterp ENABLED)
add_contract_test(ShadingContract_LightsourceNoLoopJit test_shading_contract_standalone LightsourceNoLoopJit JIT ENABLED)
add_contract_test(ShadingContract_TypeMismatchReal_MessageInterp test_shading_contract_rendered TypeMismatchReal_MessageInterp ENABLED)
add_contract_test(ShadingContract_TypeMismatchReal_MessageJit test_shading_contract_rendered TypeMismatchReal_MessageJit JIT ENABLED)
add_contract_test(ShadingContract_LightsourceNoLoopRealInterp test_shading_contract_rendered LightsourceNoLoopRealInterp ENABLED)
add_contract_test(ShadingContract_LightsourceNoLoopRealJit test_shading_contract_rendered LightsourceNoLoopRealJit JIT ENABLED)
add_contract_test(ShadingContract_NullObject_MessagesInterp test_shading_contract_rendered NullObject_MessagesInterp ENABLED)
add_contract_test(ShadingContract_NullObject_MessagesJit test_shading_contract_rendered NullObject_MessagesJit JIT ENABLED)
add_contract_test(ShadingContract_LightsourceOversizedJit test_shading_contract_rendered LightsourceOversizedJit JIT ENABLED)

# Phase 7 (the offending object): its own executable, no case argument
file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/ShadingContract_OffendingObject)
if(OPENRENDER_SHADING_CONTRACT_TESTS)
    add_test(NAME ShadingContract_OffendingObject COMMAND $<TARGET_FILE:test_shading_contract_offending>
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/ShadingContract_OffendingObject)
else()
    add_test(NAME ShadingContract_OffendingObject COMMAND ${CMAKE_COMMAND} -E true
             WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/cwd/ShadingContract_OffendingObject)
endif()
set_tests_properties(ShadingContract_OffendingObject PROPERTIES TIMEOUT 120 LABELS "shading_contract"
    ENVIRONMENT "${SHADING_CONTRACT_ENV}")

# Phase 8 (the tracer)
add_contract_test(ShadingContract_TraceRequestsInterp test_shading_contract_standalone TraceRequestsInterp ENABLED)
add_contract_test(ShadingContract_TraceRequestsJit test_shading_contract_standalone TraceRequestsJit JIT ENABLED)
add_contract_test(ShadingContract_NoServices_Tracer test_shading_contract_standalone NoServices_Tracer ENABLED)
add_contract_test(ShadingContract_GatherRequestsInterp test_shading_contract_rendered GatherRequestsInterp ENABLED)
add_contract_test(ShadingContract_GatherRequestsJit test_shading_contract_rendered GatherRequestsJit JIT ENABLED)
add_contract_test(ShadingContract_CannedTraceInterp test_shading_contract_rendered CannedTraceInterp ENABLED)
add_contract_test(ShadingContract_CannedTraceJit test_shading_contract_rendered CannedTraceJit JIT ENABLED)
add_contract_test(ShadingContract_NoTracer test_shading_contract_rendered NoTracer ENABLED)

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
    ENVIRONMENT "${SHADING_CONTRACT_ENV}")

# ---------------------------------------------------------------------------
# Spec 029 (hider / shading separation): the new contract programs, written before the interfaces they use exist.
# Each is an executable of its own (adding a case that cannot compile to an executable above would break the approved cases
# already in it), defined only while OPENRENDER_029_RED_TESTS is ON, and its tests are registered DISABLED otherwise. A
# program joins the normal build, and its test loses the DISABLED mark, in the phase that provides what it needs
# (tasks.md, "Test gate"). Red evidence: the failed build of each target, one at a time (validation.md section 2).
#
#   test_shading_contract_standalone_context   T-S2  ShadingContext_Standalone{Interp,Jit}               Phase 5 (enabled)
#   test_shading_contract_photon_lookup        T-P1  ShadingContract_PhotonLookup{Interp,Jit}            Phase 3 (enabled)
#   test_shading_contract_canned_bundles       T-B1  ShadingContract_CannedBundles{Interp,Jit}           Phase 3 (enabled)
#   test_shading_contract_hook_interface       T-H1  ShadingContract_IlluminationHookInterface{Interp,Jit} Phase 5 (enabled)
#   test_shading_contract_hider_tracer         T-T1  HiderTracer_PerContext                              Phase 6 (links ribRender; enabled)
#   test_shading_contract_ray_level            T-T2  ShadingContract_RayLevel                            Phase 6 (enabled)
#   test_shading_contract_producer_situations  T-G1  ShadingContract_ProducerSituations                  Phase 7 (links ribRender; enabled)
#   test_shading_contract_point_source         T-G2  ShadingContract_PointSource{Interp,Jit}             Phase 7 (enabled)
# The shaders they run (cm_standalone, cm_photon, cm_hook_*) are compiled as build steps only here.
# ---------------------------------------------------------------------------
# Phase 3 enables T-P1 and T-B1 (their programs and cm_photon are part of the normal build from here), Phase 5 T-S2 and T-H1
# (cm_standalone, cm_hook_*); the others stay in the block of OPENRENDER_029_RED_TESTS until their phase.
set(SHADING_CONTRACT_029_SHADERS cm_photon cm_standalone cm_hook_illuminate1 cm_hook_illuminate3 cm_hook_solar2 cm_hook_solar0)

if(TARGET state_only_sources)
    set(_029_shader_outputs "")
    foreach(_n IN LISTS SHADING_CONTRACT_029_SHADERS)
        set(_sl ${CMAKE_CURRENT_SOURCE_DIR}/shaders/${_n}.sl)
        add_custom_command(
            OUTPUT ${SHADING_CONTRACT_SHADER_DIR}/${_n}.rslo
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                    -DOUT=${SHADING_CONTRACT_SHADER_DIR}/${_n}.rslo
                    -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _029_shader_outputs ${SHADING_CONTRACT_SHADER_DIR}/${_n}.rslo)
        if(OPENRENDER_HAVE_LLVM)
            add_custom_command(
                OUTPUT ${SHADING_CONTRACT_SHADER_DIR}/${_n}.slo
                COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_sl}
                        -DOUT=${SHADING_CONTRACT_SHADER_DIR}/${_n}.slo
                        -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes -DJIT=ON
                        -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
                DEPENDS ${_sl} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
                WORKING_DIRECTORY ${SHADING_CONTRACT_SHADER_DIR}
                COMMENT ""
                VERBATIM)
            list(APPEND _029_shader_outputs ${SHADING_CONTRACT_SHADER_DIR}/${_n}.slo)
        endif()
    endforeach()
    add_custom_target(shading_contract_029_shaders DEPENDS ${_029_shader_outputs})

    # or029_contract_program(<target> SOURCES <main source> <case source> [RENDERED])
    #   STANDALONE: libshader_shading + libRIBCommon + the State-only library, no ribRender, no load stub (SC-001)
    #   RENDERED:   adds ribRender
    function(or029_contract_program TARGET)
        cmake_parse_arguments(ARG "RENDERED" "" "SOURCES" ${ARGN})
        add_executable(${TARGET} ${ARG_SOURCES})
        target_compile_features(${TARGET} PRIVATE cxx_std_20)
        target_compile_definitions(${TARGET} PRIVATE ${SHADING_CONTRACT_PROGRAM_DEFS})
        target_include_directories(${TARGET} PRIVATE ${SHADING_CONTRACT_INCLUDES} ${CMAKE_SOURCE_DIR}/src/display)
        if(ARG_RENDERED)
            target_link_libraries(${TARGET} PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon state_only_sources)
        else()
            target_link_libraries(${TARGET} PRIVATE openrender_common_flags libshader_shading openrendercommon state_only_sources)
        endif()
        add_dependencies(${TARGET} shading_contract_replay_shaders shading_contract_029_shaders)
    endfunction()

    # Enabled in Phase 3 (T-P1, T-B1), Phase 5 (T-S2, T-H1), Phase 6 (T-T2, T-T1) and Phase 7 (T-G2, T-G1)
    or029_contract_program(test_shading_contract_photon_lookup SOURCES photon_lookup_main.cpp case_photon_lookup.cpp)
    or029_contract_program(test_shading_contract_canned_bundles SOURCES canned_bundles_main.cpp case_canned_bundles.cpp)
    or029_contract_program(test_shading_contract_standalone_context SOURCES standalone_context_main.cpp case_standalone_context.cpp)
    or029_contract_program(test_shading_contract_hook_interface SOURCES hook_interface_main.cpp case_illumination_hook_interface.cpp)
    or029_contract_program(test_shading_contract_ray_level SOURCES ray_level_main.cpp case_ray_level.cpp)
    or029_contract_program(test_shading_contract_hider_tracer RENDERED SOURCES hider_tracer_main.cpp case_hider_tracer.cpp)
    or029_contract_program(test_shading_contract_point_source SOURCES point_source_main.cpp case_point_source.cpp)
    or029_contract_program(test_shading_contract_producer_situations RENDERED SOURCES producer_situations_main.cpp case_producer_situations.cpp)
elseif(OPENRENDER_029_RED_TESTS)
    message(FATAL_ERROR "OPENRENDER_029_RED_TESTS needs the State-only library: enable OPENRENDER_STATE_ONLY_TESTS")
endif()

# add_contract_test_029(<test name> <program> <case argument> [JIT] [TIMEOUT <s>]): one CTest entry, DISABLED while the
# program is not part of the build (OPENRENDER_029_RED_TESTS OFF), registered with its real target while ON.
function(add_contract_test_029 NAME PROGRAM)
    cmake_parse_arguments(ARG "JIT;ENABLED" "TIMEOUT" "CASE" ${ARGN})
    if(ARG_JIT AND NOT OPENRENDER_HAVE_LLVM)
        return()
    endif()
    if(NOT ARG_TIMEOUT)
        set(ARG_TIMEOUT 120)
    endif()
    set(_cwd ${CMAKE_CURRENT_BINARY_DIR}/cwd/${NAME})
    file(MAKE_DIRECTORY ${_cwd})
    if(OPENRENDER_029_RED_TESTS OR (ARG_ENABLED AND TARGET ${PROGRAM}))
        add_test(NAME ${NAME} COMMAND $<TARGET_FILE:${PROGRAM}> ${ARG_CASE} WORKING_DIRECTORY ${_cwd})
    else()
        add_test(NAME ${NAME} COMMAND ${CMAKE_COMMAND} -E true WORKING_DIRECTORY ${_cwd})
        set_tests_properties(${NAME} PROPERTIES DISABLED TRUE)
    endif()
    set_tests_properties(${NAME} PROPERTIES TIMEOUT ${ARG_TIMEOUT} LABELS "shading_contract" ENVIRONMENT "${SHADING_CONTRACT_ENV}")
endfunction()

add_contract_test_029(ShadingContext_StandaloneInterp test_shading_contract_standalone_context ENABLED CASE Interp)
add_contract_test_029(ShadingContext_StandaloneJit test_shading_contract_standalone_context JIT ENABLED CASE Jit)
add_contract_test_029(ShadingContract_PhotonLookupInterp test_shading_contract_photon_lookup ENABLED CASE Interp)
add_contract_test_029(ShadingContract_PhotonLookupJit test_shading_contract_photon_lookup JIT ENABLED CASE Jit)
add_contract_test_029(ShadingContract_CannedBundlesInterp test_shading_contract_canned_bundles ENABLED CASE Interp)
add_contract_test_029(ShadingContract_CannedBundlesJit test_shading_contract_canned_bundles JIT ENABLED CASE Jit)
add_contract_test_029(ShadingContract_IlluminationHookInterfaceInterp test_shading_contract_hook_interface ENABLED CASE Interp)
add_contract_test_029(ShadingContract_IlluminationHookInterfaceJit test_shading_contract_hook_interface JIT ENABLED CASE Jit)
add_contract_test_029(HiderTracer_PerContext test_shading_contract_hider_tracer ENABLED)
add_contract_test_029(ShadingContract_RayLevel test_shading_contract_ray_level ENABLED)
add_contract_test_029(ShadingContract_ProducerSituations test_shading_contract_producer_situations ENABLED TIMEOUT 600)
add_contract_test_029(ShadingContract_PointSourceInterp test_shading_contract_point_source ENABLED CASE Interp)
add_contract_test_029(ShadingContract_PointSourceJit test_shading_contract_point_source JIT ENABLED CASE Jit)
