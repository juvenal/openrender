# Shader DSO (shadeop) fixtures, the probe shader and the FR-031 / FR-033 checks (spec 037).
#
#   good, unaware   the full operand (unaware declares no interface version: TEST_NO_VERSION)
#   missing_exec    the table names probe_add, the exec symbol is absent   (directory A)
#   has_exec        the full operand                                       (directory B)
#
# probe.sl calls the operand. It is compiled with oshader against A and B into the build tree;
# the scan scene (scan.rib.in) searches A then B. Search-path order is by directory, never by the
# order osEnumerate returns files in.

or_plugin_test_module(plugin_shadeop_good shadeop good shadeop_fixture.cpp)
or_plugin_test_module(plugin_shadeop_unaware shadeop unaware shadeop_fixture.cpp DEFS TEST_NO_VERSION)
or_plugin_test_module(plugin_shadeop_missing_exec shadeop missing_exec shadeop_fixture.cpp DEFS TEST_OMIT_EXEC)
or_plugin_test_module(plugin_shadeop_has_exec shadeop has_exec shadeop_fixture.cpp)

set(SHADEOP_DIR_A ${OR_PLUGIN_BIN}/shadeop/missing_exec)
set(SHADEOP_DIR_B ${OR_PLUGIN_BIN}/shadeop/has_exec)
set(SHADEOP_SHADER_DIR ${OR_PLUGIN_BIN}/shadeop/shaders)
set(SHADEOP_SHADER_DEFAULT_DIR ${CMAKE_BINARY_DIR}/shaders) # defaultsurface, defaultlight

add_custom_command(
    OUTPUT ${SHADEOP_SHADER_DIR}/probe.rslo
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADEOP_SHADER_DIR}
    COMMAND $<TARGET_FILE:oshader> -I ${SHADEOP_DIR_A} -I ${SHADEOP_DIR_B}
            -o ${SHADEOP_SHADER_DIR}/probe.rslo ${CMAKE_CURRENT_SOURCE_DIR}/probe.sl
    DEPENDS oshader plugin_shadeop_missing_exec plugin_shadeop_has_exec ${CMAKE_CURRENT_SOURCE_DIR}/probe.sl
    COMMENT "Compiling the shadeop probe shader"
    VERBATIM)
add_custom_target(plugin_shadeop_probe ALL DEPENDS ${SHADEOP_SHADER_DIR}/probe.rslo)

or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/scan.rib.in ${OR_PLUGIN_BIN}/shadeop/scan.rib)

# FR-031 -- the SHADEOP macro and dsoExecFunction must describe one signature, from C++20 and from
# strict C17 (the C half is Phase 2, T036). The compile flags are passed on so the check uses the
# project's toolchain (macOS: the SDK).
set(_signature_flags "${CMAKE_CXX_FLAGS}")
if(CMAKE_OSX_SYSROOT)
    set(_signature_flags "${_signature_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()
set(_signature_c_flags "${CMAKE_C_FLAGS}")
if(CMAKE_OSX_SYSROOT)
    set(_signature_c_flags "${_signature_c_flags} -isysroot ${CMAKE_OSX_SYSROOT}")
endif()
or_plugin_add_test(ShadeopAbi_SignatureMatches 120
    COMMAND ${CMAKE_COMMAND}
        -DCXX=${CMAKE_CXX_COMPILER}
        -DCC=${CMAKE_C_COMPILER}
        "-DFLAGS=${_signature_flags}"
        "-DC_FLAGS=${_signature_c_flags}"
        -DINCLUDE_DIR=${OR_SHADEOP_INCLUDE_DIR}
        "-DEXTRA_INCLUDE_DIRS=${CMAKE_SOURCE_DIR}/src/include"
        -DWORKDIR=${OR_PLUGIN_BIN}/shadeop/work/signature
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_signature.cmake)

# FR-033 -- an unresolved exec symbol is skipped and the scan goes on.
or_plugin_add_test(ShadeopAbi_MissingExec 120
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIB=${OR_PLUGIN_BIN}/shadeop/scan.rib
        -DWORKDIR=${OR_PLUGIN_BIN}/shadeop/work/scan
        -DLOG=${OR_PLUGIN_BIN}/shadeop/work/scan.log
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_missing_exec.cmake)
set_tests_properties(ShadeopAbi_MissingExec PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# T028 -- characterization of the old declaration parser (green on the starting tree by design).
# An in-process program that calls dsoParse over declarations.txt. Phase 4 re-points it at
# parseShadeopDeclaration with the same expectations.
add_executable(test_shadeop_declaration test_declaration.cpp)
target_link_libraries(test_shadeop_declaration PRIVATE openrender_common_flags ribRender libshader_shading openrendercommon)
target_include_directories(test_shadeop_declaration PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
target_compile_features(test_shadeop_declaration PRIVATE cxx_std_20)
or_plugin_add_test(ShadeopAbi_DeclarationCharacterization 60
    COMMAND test_shadeop_declaration ${CMAKE_CURRENT_SOURCE_DIR}/declarations.txt)
set_tests_properties(ShadeopAbi_DeclarationCharacterization PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")
