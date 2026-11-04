# Shader DSO (shadeop) fixtures, the probe shaders and the shadeop checks (spec 037).
#
#   good            the current interface with the 1.1 per-thread table (TEST_THREADS)
#   hidden          good, hidden symbol visibility
#   unaware         declares 1.0.0 explicitly, no per-thread table
#   legacy          no version symbol; a second table, probe_mul
#   newer, v2       1.9.0 and 2.0.0
#   threads_null    good, but threadInit returns NULL for thread 3
#   missing_exec    the table names probe_add, the exec symbol is absent   (directory A)
#   has_exec        the full operand                                       (directory B)
#   order/first     alpha.dsply (id 1), zeta.dsply (id 2) \  probe_order, the scan order
#   order/second    beta.dsply (id 3)                      /
#   example         examples/dsotest/dsotest.cpp built as a module
#
# probe.sl and probe_two.sl call the operands. They are compiled with oshader against the fixture
# directories into the build tree. Search-path order is by directory, never by the order osEnumerate
# returns files in.
#
# Phase 4 tests (tasks.md T064): ShadeopAbi_Versions_Render, ShadeopAbi_LegacyShared, ShadeopAbi_ExampleBuilds,
# ShadeopAbi_MissingExec (Phase 1) and the tests of the shadeop module API (test_registry.cpp,
# test_parser_reentrant.cpp), which were EXCLUDE_FROM_ALL + DISABLED until the module existed (T076 removed
# the markers).

or_plugin_test_module(plugin_shadeop_good shadeop good shadeop_fixture.cpp DEFS TEST_THREADS)
or_plugin_test_module(plugin_shadeop_hidden shadeop hidden shadeop_fixture.cpp HIDDEN DEFS TEST_THREADS)
or_plugin_test_module(plugin_shadeop_unaware shadeop unaware shadeop_fixture.cpp DEFS TEST_ABI_MAJOR=1 TEST_ABI_MINOR=0)
or_plugin_test_module(plugin_shadeop_legacy shadeop legacy shadeop_fixture.cpp DEFS TEST_NO_VERSION TEST_TWO_OPERANDS)
or_plugin_test_module(plugin_shadeop_newer shadeop newer shadeop_fixture.cpp DEFS TEST_ABI_MAJOR=1 TEST_ABI_MINOR=9 TEST_THREADS)
or_plugin_test_module(plugin_shadeop_v2 shadeop v2 shadeop_fixture.cpp DEFS TEST_ABI_MAJOR=2 TEST_ABI_MINOR=0 TEST_THREADS)
or_plugin_test_module(plugin_shadeop_threads_null shadeop threads_null shadeop_fixture.cpp DEFS TEST_THREADS TEST_THREAD_INIT_NULL_AT=3)
or_plugin_test_module(plugin_shadeop_missing_exec shadeop missing_exec shadeop_fixture.cpp DEFS TEST_OMIT_EXEC)
or_plugin_test_module(plugin_shadeop_has_exec shadeop has_exec shadeop_fixture.cpp)

# FR-013 -- the scan order. Two modules in one directory (zeta is built first, alpha sorts first) and one in
# another directory, all defining probe_order(float) (order_fixture.cpp).
or_plugin_test_module(plugin_shadeop_order_zeta shadeop/order first order_fixture.cpp OUTPUT_NAME zeta DEFS TEST_ID=2)
or_plugin_test_module(plugin_shadeop_order_alpha shadeop/order first order_fixture.cpp OUTPUT_NAME alpha DEFS TEST_ID=1)
or_plugin_test_module(plugin_shadeop_order_beta shadeop/order second order_fixture.cpp OUTPUT_NAME beta DEFS TEST_ID=3)

# FR-008 -- the example DSO, built as a module. NO_FLAGS: it is sample code for plug-in authors, not built
# with the project's -Werror (it has unused parameters and short initializers). It includes only shadeop.h,
# which the fixture include directories find, as an installed header would be.
or_plugin_test_module(plugin_shadeop_example shadeop example ${CMAKE_SOURCE_DIR}/examples/dsotest/dsotest.cpp NO_FLAGS)

set(SHADEOP_FIXTURE_TARGETS
    plugin_shadeop_good plugin_shadeop_hidden plugin_shadeop_unaware plugin_shadeop_legacy plugin_shadeop_newer
    plugin_shadeop_v2 plugin_shadeop_threads_null plugin_shadeop_missing_exec plugin_shadeop_has_exec
    plugin_shadeop_order_zeta plugin_shadeop_order_alpha plugin_shadeop_order_beta)

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
add_custom_command(
    OUTPUT ${SHADEOP_SHADER_DIR}/probe_two.rslo
    COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADEOP_SHADER_DIR}
    COMMAND $<TARGET_FILE:oshader> -I ${OR_PLUGIN_BIN}/shadeop/legacy
            -o ${SHADEOP_SHADER_DIR}/probe_two.rslo ${CMAKE_CURRENT_SOURCE_DIR}/probe_two.sl
    DEPENDS oshader plugin_shadeop_legacy ${CMAKE_CURRENT_SOURCE_DIR}/probe_two.sl
    COMMENT "Compiling the shadeop probe_two shader"
    VERBATIM)
add_custom_target(plugin_shadeop_probe ALL DEPENDS ${SHADEOP_SHADER_DIR}/probe.rslo ${SHADEOP_SHADER_DIR}/probe_two.rslo)

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
# An in-process program that calls the declaration parser over declarations.txt: dsoParse until Phase 4,
# parseShadeopDeclaration since T068, with the same expectations.
add_executable(test_shadeop_declaration test_declaration.cpp)
target_link_libraries(test_shadeop_declaration PRIVATE openrender_common_flags openrendershadeop openrendercommon)
target_include_directories(test_shadeop_declaration PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_BINARY_DIR})
target_compile_features(test_shadeop_declaration PRIVATE cxx_std_20)
or_plugin_add_test(ShadeopAbi_DeclarationCharacterization 60
    COMMAND test_shadeop_declaration ${CMAKE_CURRENT_SOURCE_DIR}/declarations.txt)
set_tests_properties(ShadeopAbi_DeclarationCharacterization PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# ---------------------------------------------------------------------------------------------------------
# Phase 4 (T063, T064)

# --- ShadeopAbi_Versions_Render and ShadeopAbi_LegacyShared: .rslo renders, no new API needed (enabled) ---
# render.rib.in: two frames, four shading threads. One RIB per case (the procedural search path and the
# surface differ); RIB_OUTPUT is the display file.
function(shadeop_case_rib name surface proc_path threads)
    set(RIB_SURFACE ${surface})
    set(RIB_PROC_PATH "${proc_path}")
    set(RIB_THREADS ${threads})
    set(RIB_OUTPUT ${name}.tif)
    if(ARGN STREQUAL "ONE_FRAME")
        or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/render_1f.rib.in ${OR_PLUGIN_BIN}/shadeop/versions/${name}.rib)
    else()
        or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/render.rib.in ${OR_PLUGIN_BIN}/shadeop/versions/${name}.rib)
    endif()
endfunction()

set(_B ${OR_PLUGIN_BIN}/shadeop)
shadeop_case_rib(good probe ${_B}/good 4)
shadeop_case_rib(hidden probe ${_B}/hidden 4)
shadeop_case_rib(good_f1 probe ${_B}/good 4 ONE_FRAME)
shadeop_case_rib(hidden_f1 probe ${_B}/hidden 4 ONE_FRAME)
shadeop_case_rib(newer probe ${_B}/newer 4)
shadeop_case_rib(v2_only probe ${_B}/v2 4)
shadeop_case_rib(v2_then_good probe "${_B}/v2:${_B}/good" 4)
shadeop_case_rib(legacy probe_two ${_B}/legacy 4)
shadeop_case_rib(unaware_mt probe ${_B}/unaware 4)
shadeop_case_rib(unaware_st probe ${_B}/unaware 1)

or_plugin_add_test(ShadeopAbi_Versions_Render 600
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DRIBDIR=${OR_PLUGIN_BIN}/shadeop/versions
        -DWORKROOT=${OR_PLUGIN_BIN}/shadeop/work/versions
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_versions_render.cmake)
set_tests_properties(ShadeopAbi_Versions_Render PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

or_plugin_add_test(ShadeopAbi_LegacyShared 600
    COMMAND ${CMAKE_COMMAND}
        -DORENDER=$<TARGET_FILE:orender>
        -DTOOL=$<TARGET_FILE:plugin_abi_pixel_compare>
        -DRIBDIR=${OR_PLUGIN_BIN}/shadeop/versions
        -DWORKROOT=${OR_PLUGIN_BIN}/shadeop/work/legacy_shared
        -DCOMMON=${OR_PLUGIN_COMMON_DIR}
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_legacy_shared.cmake)
set_tests_properties(ShadeopAbi_LegacyShared PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")

# --- ShadeopAbi_ExampleBuilds (FR-008): enabled, red until T074 ---
or_plugin_add_test(ShadeopAbi_ExampleBuilds 60
    COMMAND ${CMAKE_COMMAND}
        -DMODULE=$<TARGET_FILE:plugin_shadeop_example>
        -DREADME=${CMAKE_SOURCE_DIR}/examples/dsotest/README.md
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_exports.cmake)

# --- the tests that call the shadeop module API ---
# Written before src/shadeop/ existed (EXCLUDE_FROM_ALL + DISABLED, red state: the compile error of their
# targets); T076 removed the markers. openrendershadeop is the library target T069 creates (tests/ is
# configured before src/, so the name is resolved at generate time).
add_executable(test_shadeop_registry test_registry.cpp)
target_link_libraries(test_shadeop_registry PRIVATE openrender_common_flags openrendershadeop openrendercommon)
target_include_directories(test_shadeop_registry PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_compile_features(test_shadeop_registry PRIVATE cxx_std_20)
add_dependencies(test_shadeop_registry ${SHADEOP_FIXTURE_TARGETS} oshader)

add_executable(test_shadeop_parser_reentrant test_parser_reentrant.cpp)
target_link_libraries(test_shadeop_parser_reentrant PRIVATE openrender_common_flags openrendershadeop openrendercommon)
target_include_directories(test_shadeop_parser_reentrant PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_features(test_shadeop_parser_reentrant PRIVATE cxx_std_20)
find_package(Threads REQUIRED)
target_link_libraries(test_shadeop_parser_reentrant PRIVATE Threads::Threads)
target_link_libraries(test_shadeop_registry PRIVATE Threads::Threads)

foreach(_mode versions perthread lifetime scanorder)
    set(_test_versions ShadeopAbi_Versions_Registry)
    set(_test_perthread ShadeopAbi_PerThread)
    set(_test_lifetime ShadeopAbi_ThreadLifetime)
    set(_test_scanorder ShadeopAbi_ScanOrder)
    set(_test ${_test_${_mode}})
    file(MAKE_DIRECTORY ${OR_PLUGIN_BIN}/shadeop/work/registry_${_mode})
    or_plugin_add_test(${_test} 120 COMMAND test_shadeop_registry ${_mode} ${OR_PLUGIN_BIN})
    set_tests_properties(${_test} PROPERTIES WORKING_DIRECTORY ${OR_PLUGIN_BIN}/shadeop/work/registry_${_mode})
endforeach()

file(MAKE_DIRECTORY ${OR_PLUGIN_BIN}/shadeop/work/compiler)
or_plugin_add_test(ShadeopAbi_CompilerAndRenderer 120
    COMMAND test_shadeop_registry compiler ${OR_PLUGIN_BIN} $<TARGET_FILE:oshader> ${CMAKE_CURRENT_SOURCE_DIR}/probe.sl ${OR_PLUGIN_BIN}/shadeop/work/compiler)
set_tests_properties(ShadeopAbi_CompilerAndRenderer PROPERTIES WORKING_DIRECTORY ${OR_PLUGIN_BIN}/shadeop/work/compiler)

or_plugin_add_test(ShadeopAbi_ParserReentrant 120
    COMMAND test_shadeop_parser_reentrant ${CMAKE_CURRENT_SOURCE_DIR}/declarations.txt)

# ---------------------------------------------------------------------------------------------------------
# Spec 038 (Scene Interface untangle), US6: the JIT's DSO call path. Tests first (Phase 1): every case below is
# red on the starting tree, because `oshader --jit` exits 3 with "JIT coverage gap for '<shader>': builtin
# 'DSO' has no emitFunction() case" (contracts/jit-dso-call.md; tasks.md T042-T044). A JIT-off build has no
# .slo, so the cases exist only under OPENRENDER_ENABLE_JIT.
#
# Each case renders one scene twice in one test run, as .rslo (the interpreter) and as .slo (the JIT), and
# compares (check_jit_parity.cmake). The shaders are compiled by the check, at ctest time, so the build stays
# green. The scene is render_format.rib.in, render.rib.in with the shader format as a parameter (the approved
# render.rib.in hard-codes "rslo" and is not edited here).
#
#   ShadeopAbi_Jit_Probe          probe.sl, a uniform value-returning call (the 1.1 fixture)
#   ShadeopAbi_Jit_ProbeTwo       probe_two.sl, two operands of one (unversioned) module
#   ShadeopAbi_Jit_Args           probe_args.sl: varying, uniform-promoted, string and if-masked arguments
#   ShadeopAbi_Jit_Void           probe_void.sl: a void operand; its records are compared
#   ShadeopAbi_Jit_Init           probe_init.sl: a parameter default that calls the operand (init section, Q9)
#   ShadeopAbi_Jit_PerThread8     probe.sl on 8 shading threads: 8 data pointers and 8 cleanups, as .rslo reports
#   ShadeopAbi_Jit_MissingOperand probe.slo with the module removed from the search path (decision Q2)
# ---------------------------------------------------------------------------------------------------------
if(OPENRENDER_ENABLE_JIT)
    or_plugin_test_module(plugin_shadeop_jit shadeop jit jit_fixture.cpp)

    set(_JB ${OR_PLUGIN_BIN}/shadeop)

    # shadeop_jit_case(<name> <kind> <shader> <source> <include dirs> <procedural path> <threads> <size> <fixture tag>)
    function(shadeop_jit_case name kind shader source includes proc_path threads size tag)
        set(_work ${_JB}/jit_work/${name})
        set(RIB_SURFACE ${shader})
        set(RIB_PROC_PATH "${proc_path}")
        set(RIB_THREADS ${threads})
        set(RIB_SIZE ${size})
        set(RIB_OUTPUT ${shader}.tif)
        foreach(_format rslo slo)
            set(RIB_SHADER_FORMAT ${_format})
            if(kind STREQUAL "missing")
                set(RIB_SHADER_DIR ${_work}/both)
            else()
                set(RIB_SHADER_DIR ${_work}/${_format})
            endif()
            or_plugin_rib(${CMAKE_CURRENT_SOURCE_DIR}/render_format.rib.in ${_work}/${_format}.rib)
        endforeach()
        or_plugin_add_test(ShadeopAbi_Jit_${name} 600
            COMMAND ${CMAKE_COMMAND}
                -DCASE=${name} -DKIND=${kind} -DSHADER=${shader} -DSOURCE=${source}
                "-DINCLUDE_DIRS=${includes}" -DTAG=${tag} -DOUTPUT=${shader}.tif
                -DOSHADER=$<TARGET_FILE:oshader> -DORENDER=$<TARGET_FILE:orender>
                -DTOOL=$<TARGET_FILE:plugin_abi_pixel_compare>
                -DRIB_RSLO=${_work}/rslo.rib -DRIB_SLO=${_work}/slo.rib
                -DWORKROOT=${_work}
                -DCOMMON=${OR_PLUGIN_COMMON_DIR}
                -P ${CMAKE_CURRENT_SOURCE_DIR}/check_jit_parity.cmake)
        set_tests_properties(ShadeopAbi_Jit_${name} PROPERTIES ENVIRONMENT "${OR_PLUGIN_RENDER_ENV}")
    endfunction()

    shadeop_jit_case(Probe pixels probe ${CMAKE_CURRENT_SOURCE_DIR}/probe.sl ${_JB}/good ${_JB}/good 4 64 good)
    shadeop_jit_case(ProbeTwo pixels probe_two ${CMAKE_CURRENT_SOURCE_DIR}/probe_two.sl ${_JB}/legacy ${_JB}/legacy 4 64 legacy)
    shadeop_jit_case(Args pixels probe_args ${CMAKE_CURRENT_SOURCE_DIR}/probe_args.sl ${_JB}/jit ${_JB}/jit 4 64 jit)
    shadeop_jit_case(Void void probe_void ${CMAKE_CURRENT_SOURCE_DIR}/probe_void.sl ${_JB}/jit ${_JB}/jit 4 64 jit)
    shadeop_jit_case(Init pixels probe_init ${CMAKE_CURRENT_SOURCE_DIR}/probe_init.sl ${_JB}/jit ${_JB}/jit 4 64 jit)
    shadeop_jit_case(PerThread8 perthread probe ${CMAKE_CURRENT_SOURCE_DIR}/probe.sl ${_JB}/good ${_JB}/good 8 256 good)
    shadeop_jit_case(MissingOperand missing probe ${CMAKE_CURRENT_SOURCE_DIR}/probe.sl ${_JB}/good ${_JB}/no_such_module_dir 4 64 good)
endif()
