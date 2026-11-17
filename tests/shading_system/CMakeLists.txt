# Spec 033: the shading system's tests (tests/shading_system). Phase 1 holds only the loading benchmark program of
# research R12: it is not a ctest test, tools/timing_loading.sh runs it through cpujob. The red-first tests of Phase 2
# are registered here behind OPENRENDER_033_RED_TESTS.

add_executable(bench_shader_load bench_load.cpp)
target_compile_features(bench_shader_load PRIVATE cxx_std_20)
target_include_directories(bench_shader_load PRIVATE
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/libshader/shading
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_BINARY_DIR})
target_link_libraries(bench_shader_load PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)

# ---------------------------------------------------------------------------------------------------------------------------------
# Phase 2 (spec 033): the new tests, red first (DEP-2). Each is its own executable inside the option's block; the tests are
# registered by or033_red_test (tests/CMakeLists.txt), DISABLED while OPENRENDER_033_RED_TESTS is OFF. Every one is red for the
# reason in tasks.md T026: shadingSystem.h, shaderBackend.h, shaderGroup.h, shaderPayload.h and ShaderMetadata.h do not exist, or
# EMatrixPhase, MatrixRequest::phase and libshader::initialiseInstance() do not exist (a build failure naming the missing piece).
# The phase that makes a test compile moves its definition out of the block (the Test gate).
#
#   ShadingSystem_ThirdBackend       SC-003                      Phase 4 (JIT trees)
#   ShadingSystem_Precedence         the format precedence       Phase 4
#   ShadingSystem_Cache              one load per name, #167     Phase 4
#   ShadingSystem_BindOrder          the bind order              Phase 4
#   ShadingSystem_GroupOneLayer      US4                         Phase 6
#   ShadingSystem_NoJitFallback      JIT-less trees only         Phase 5
#   ShadingSystem_BindUnknownSpace   P-Q2                        Phase 3: ENABLED (moved out of this block)
#   ShaderMetadata_Shipped           US6                         Phase 8
# ---------------------------------------------------------------------------------------------------------------------------------
set(SHADING_SYSTEM_INCLUDES
    ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src/libshader/shading
    ${CMAKE_SOURCE_DIR}/src/display ${CMAKE_SOURCE_DIR}/tests/shading_contract ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_BINARY_DIR})
set(SHADING_SYSTEM_DEFS "")
if(OPENRENDER_HAVE_LLVM)
    list(APPEND SHADING_SYSTEM_DEFS SHADING_CONTRACT_HAVE_JIT=1)
endif()
set(SHADING_SYSTEM_SHADER_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)
set(SHADING_SYSTEM_ENV
    "SHADING_SYSTEM_FIXTURES=${CMAKE_BINARY_DIR}/tests/shaderformat/fixtures"
    "SHADING_CONTRACT_SHADERS=${CMAKE_BINARY_DIR}/tests/shading_contract/shaders")

# standalone executable: libshader_shading + libRIBCommon + the State-only library, no ribRender (SC-003)
if(TARGET state_only_sources)
    function(add_system_test_exe TARGET)
        add_executable(${TARGET} ${ARGN})
        target_compile_features(${TARGET} PRIVATE cxx_std_20)
        target_compile_definitions(${TARGET} PRIVATE ${SHADING_SYSTEM_DEFS})
        target_include_directories(${TARGET} PRIVATE ${SHADING_SYSTEM_INCLUDES})
        target_link_libraries(${TARGET} PRIVATE openrender_common_flags libshader_shading openrendercommon state_only_sources Threads::Threads)
        add_dependencies(${TARGET} shading_contract_replay_shaders shaderformat_fixtures)
    endfunction()

    # Enabled in Phase 5 (the Test gate): the facade tests
    if(OPENRENDER_HAVE_LLVM)
        add_system_test_exe(test_shading_system_third_backend test_third_backend.cpp)
        add_system_test_exe(test_shading_system_precedence test_precedence.cpp)
        add_system_test_exe(test_shading_system_cache test_cache.cpp)
        # Enabled in Phase 6 (the Test gate): the group builder tests
        add_system_test_exe(test_shading_system_bind_order test_bind_order.cpp)
        add_system_test_exe(test_shading_system_group_one_layer test_group_one_layer.cpp)
    else()
        add_system_test_exe(test_shading_system_nojit_fallback test_nojit_fallback.cpp)
    endif()
endif()

# Enabled in Phase 8 (the Test gate)
if(OPENRENDER_HAVE_LLVM)
    # libRSLInfo only: never libshader_shading / libRSLRuntime (the two share the flex/bison globals)
    add_executable(test_shading_system_metadata_shipped test_metadata_shipped.cpp)
    target_compile_features(test_shading_system_metadata_shipped PRIVATE cxx_std_20)
    target_include_directories(test_shading_system_metadata_shipped PRIVATE ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(test_shading_system_metadata_shipped PRIVATE openrender_common_flags libRSLInfo)
    add_dependencies(test_shading_system_metadata_shipped openrender_shaders)
endif()

if(OPENRENDER_HAVE_LLVM)
    # Enabled in Phase 5
    add_test(NAME ShadingSystem_ThirdBackend COMMAND $<TARGET_FILE:test_shading_system_third_backend>)
    add_test(NAME ShadingSystem_Precedence COMMAND $<TARGET_FILE:test_shading_system_precedence>)
    add_test(NAME ShadingSystem_Cache COMMAND $<TARGET_FILE:test_shading_system_cache>)
    set_tests_properties(ShadingSystem_ThirdBackend ShadingSystem_Precedence ShadingSystem_Cache PROPERTIES
        TIMEOUT 120 ENVIRONMENT "${SHADING_SYSTEM_ENV}")
    set_tests_properties(ShadingSystem_ThirdBackend ShadingSystem_Cache PROPERTIES LABELS "shading_system")
    set_tests_properties(ShadingSystem_Precedence PROPERTIES LABELS "shading_system;shaderformat")
    # Enabled in Phase 6
    add_test(NAME ShadingSystem_BindOrder COMMAND $<TARGET_FILE:test_shading_system_bind_order>)
    add_test(NAME ShadingSystem_GroupOneLayer COMMAND $<TARGET_FILE:test_shading_system_group_one_layer>)
    set_tests_properties(ShadingSystem_BindOrder ShadingSystem_GroupOneLayer PROPERTIES
        TIMEOUT 120 LABELS "shading_system" ENVIRONMENT "${SHADING_SYSTEM_ENV}")
    # Enabled in Phase 8
    or033_test(ShaderMetadata_Shipped
        COMMAND $<TARGET_FILE:test_shading_system_metadata_shipped> ${CMAKE_BINARY_DIR}/shaders ${CMAKE_SOURCE_DIR}/tests/shaders/references/sloinfo137 rslo,slo
        LABELS "libshader;shading_system")
else()
    # Enabled in Phase 5 (JIT-less trees)
    add_test(NAME ShadingSystem_NoJitFallback COMMAND $<TARGET_FILE:test_shading_system_nojit_fallback>)
    set_tests_properties(ShadingSystem_NoJitFallback PROPERTIES TIMEOUT 120 LABELS "shading_system"
        ENVIRONMENT "SHADING_SYSTEM_FIXTURES=${CMAKE_BINARY_DIR}/tests/shaderformat/fixtures;SHADING_CONTRACT_SHADERS=${CMAKE_BINARY_DIR}/tests/shading_contract/shaders")
endif()

# ---------------------------------------------------------------------------------------------------------------------------------
# Enabled in Phase 3 (the Test gate): ShadingSystem_BindUnknownSpace (P-Q2). Red by build failure until libshader::initialiseInstance()
# and EMatrixPhase existed; now an ordinary test, one process per backend (the .slo run only with the JIT).
# ---------------------------------------------------------------------------------------------------------------------------------
if(TARGET state_only_sources AND OPENRENDER_HAVE_LLVM)
    # the shader of ShadingSystem_BindUnknownSpace, compiled as a build step in both formats
    set(_nosuch ${CMAKE_CURRENT_SOURCE_DIR}/shaders/cm_nosuch.sl)
    file(MAKE_DIRECTORY ${SHADING_SYSTEM_SHADER_DIR})
    set(_nosuch_outputs "")
    foreach(_fmt rslo slo)
        set(_jit_arg "")
        if(_fmt STREQUAL "slo")
            set(_jit_arg -DJIT=ON)
        endif()
        add_custom_command(
            OUTPUT ${SHADING_SYSTEM_SHADER_DIR}/cm_nosuch.${_fmt}
            COMMAND ${CMAKE_COMMAND} -DOSHADER=$<TARGET_FILE:oshader> -DSRC=${_nosuch}
                    -DOUT=${SHADING_SYSTEM_SHADER_DIR}/cm_nosuch.${_fmt} -DINCDIR=${CMAKE_SOURCE_DIR}/shaders/includes ${_jit_arg}
                    -P ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            DEPENDS ${_nosuch} oshader ${CMAKE_SOURCE_DIR}/CMake/CompileShader.cmake
            WORKING_DIRECTORY ${SHADING_SYSTEM_SHADER_DIR}
            COMMENT ""
            VERBATIM)
        list(APPEND _nosuch_outputs ${SHADING_SYSTEM_SHADER_DIR}/cm_nosuch.${_fmt})
    endforeach()
    add_custom_target(shading_system_nosuch_shaders DEPENDS ${_nosuch_outputs})
    add_executable(test_shading_system_bind_unknown_space test_bind_unknown_space.cpp)
    target_compile_features(test_shading_system_bind_unknown_space PRIVATE cxx_std_20)
    target_compile_definitions(test_shading_system_bind_unknown_space PRIVATE ${SHADING_SYSTEM_DEFS})
    target_include_directories(test_shading_system_bind_unknown_space PRIVATE ${SHADING_SYSTEM_INCLUDES})
    target_link_libraries(test_shading_system_bind_unknown_space PRIVATE openrender_common_flags libshader_shading openrendercommon state_only_sources)
    add_dependencies(test_shading_system_bind_unknown_space shading_contract_replay_shaders)
    add_dependencies(test_shading_system_bind_unknown_space shading_system_nosuch_shaders)
    add_test(NAME ShadingSystem_BindUnknownSpace
        COMMAND ${CMAKE_COMMAND} -DEXE=$<TARGET_FILE:test_shading_system_bind_unknown_space> -DMODES=rslo,slo
                -P ${CMAKE_SOURCE_DIR}/tests/shading_contract/check_all_modes.cmake)
    set_tests_properties(ShadingSystem_BindUnknownSpace PROPERTIES TIMEOUT 300 LABELS "shading_system"
        ENVIRONMENT "SHADING_CONTRACT_SHADERS=${SHADING_SYSTEM_SHADER_DIR}")
endif()
