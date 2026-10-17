# Unit tests for libshader subsystems.
# Tests follow the project pattern: plain C++ executables registered with CTest.

# ---------------------------------------------------------------------------
# test_compiler — CScriptContext RSL compiler unit tests
# ---------------------------------------------------------------------------
add_executable(test_libshader_compiler test_compiler.cpp)

target_compile_features(test_libshader_compiler PRIVATE cxx_std_20)

target_include_directories(test_libshader_compiler PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # rslo.h (CScriptContext)
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
)

target_link_libraries(test_libshader_compiler PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_Compiler
    COMMAND test_libshader_compiler)

set_tests_properties(LibShader_Compiler PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_Compiler (unit tests for RSL compiler)")

# ---------------------------------------------------------------------------
# test_oshader_cli_args — GitHub #12: oshader's -I/-D argv matcher only ever
# read the value from the same argv token, never consuming a following
# token for the customary space-separated form. Shells out to the real
# oshader CLI (subprocess) since the bug lives in main()'s argv loop, not
# any separately-callable function.
# ---------------------------------------------------------------------------
add_executable(test_libshader_oshader_cli_args test_oshader_cli_args.cpp)

target_compile_features(test_libshader_oshader_cli_args PRIVATE cxx_std_20)

add_test(NAME LibShader_OshaderCliArgs
    COMMAND test_libshader_oshader_cli_args)

set_tests_properties(LibShader_OshaderCliArgs PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
    ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>"
)

message(STATUS "  - LibShader_OshaderCliArgs (GitHub #12 -I/-D space-separated argument parsing)")

# ---------------------------------------------------------------------------
# test_atmosphere_debug_builtins — GitHub #5: atmosphere() and debug() were
# missing from rslo.cpp's compiler symbol table (present in shaderFunctions.h's
# runtime dispatch tables already). One compile() call per process (see the
# file's header comment for why), so two ctest entries run the same binary
# with an argv switch rather than sharing one executable/process.
# ---------------------------------------------------------------------------
add_executable(test_libshader_atmosphere_debug_builtins test_atmosphere_debug_builtins.cpp)

target_compile_features(test_libshader_atmosphere_debug_builtins PRIVATE cxx_std_20)

target_include_directories(test_libshader_atmosphere_debug_builtins PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # rslo.h (CScriptContext)
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
)

target_link_libraries(test_libshader_atmosphere_debug_builtins PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_AtmosphereBuiltin
    COMMAND test_libshader_atmosphere_debug_builtins)
add_test(NAME LibShader_DebugBuiltin
    COMMAND test_libshader_atmosphere_debug_builtins debug)

set_tests_properties(LibShader_AtmosphereBuiltin LibShader_DebugBuiltin PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_AtmosphereBuiltin / LibShader_DebugBuiltin (GitHub #5 compiler symbol-table regression)")

# ---------------------------------------------------------------------------
# test_mcomp_consistency — GitHub #9: op_mcomp/op_setmcomp (rslOps.cpp,
# shared by the JIT and, via scriptFunctions.h's mirrored macros, the
# interpreter) used different index formulas for the matrix form, so
# comp() read the transpose of what setcomp() wrote. The two functions
# under test are pure free functions (no CShadingContext), but
# libshader_shading.dylib itself is not self-contained -- it has
# undefined references to CRenderer:: statics (normally provided by
# ribRender) resolved at dyld load time regardless of which functions a
# given caller actually calls, so ribRender must be linked too.
# ---------------------------------------------------------------------------
add_executable(test_libshader_mcomp_consistency test_mcomp_consistency.cpp)

target_compile_features(test_libshader_mcomp_consistency PRIVATE cxx_std_20)

target_include_directories(test_libshader_mcomp_consistency PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/shading   # rslOps.h
    ${CMAKE_SOURCE_DIR}/src                     # common/, logging.h
)

target_link_libraries(test_libshader_mcomp_consistency PRIVATE
    ribRender
    libshader_shading
    openrendercommon
)

add_test(NAME LibShader_McompConsistency
    COMMAND test_libshader_mcomp_consistency)

set_tests_properties(LibShader_McompConsistency PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;shading;unit"
)

message(STATUS "  - LibShader_McompConsistency (GitHub #9 comp()/setcomp() matrix-index regression)")

# ---------------------------------------------------------------------------
# The three tests below all reach into llvmEmitter.cpp -- kHandledOpcodes,
# kOpcodeParamTable and computeUsedParameters() are defined there, and that
# translation unit is only compiled when OPENRENDER_HAVE_LLVM is set (see
# src/libshader/compiler/CMakeLists.txt). Registered unconditionally, they fail
# to *link* in a JIT-less build:
#
#     Undefined symbols for architecture arm64:
#       "_kHandledOpcodes", referenced from: _main in test_opcode_coverage.cpp.o
#
# which broke the whole build, not just the tests. Pre-existing on master and
# only reachable by configuring without LLVM, which nothing did until the
# version floor above started routing too-old toolchains down the same path.
# ---------------------------------------------------------------------------
if(OPENRENDER_HAVE_LLVM)

# ---------------------------------------------------------------------------
# test_opcode_coverage — JIT (.slo) / .rslo opcode-parity coverage guard
# (spec 011-jit-opcode-parity, FR-006). Compares kHandledOpcodes
# (llvmEmitter.cpp) against a reachable-opcode set computed at test-run time
# from opcodes.cpp's kAllOpcodeMnemonics minus kDeadOpcodes. Runs only under
# ctest -L libshader, never at render runtime.
# ---------------------------------------------------------------------------
add_executable(test_libshader_opcode_coverage test_opcode_coverage.cpp)

target_compile_features(test_libshader_opcode_coverage PRIVATE cxx_std_20)

target_include_directories(test_libshader_opcode_coverage PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # llvmEmitter.h (kHandledOpcodes)
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
)

target_link_libraries(test_libshader_opcode_coverage PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_OpcodeCoverage
    COMMAND test_libshader_opcode_coverage)

set_tests_properties(LibShader_OpcodeCoverage PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_OpcodeCoverage (JIT/.rslo opcode-parity coverage guard)")

# ---------------------------------------------------------------------------
# test_gate_hardening — negative test for the hardened coverage gate
# (spec 017-jit-builtin-function-coverage, US2, T018,
# contracts/gate-hardening-contract.md's Verification Obligation #2).
#
# Redesigned 2026-09-20: originally shelled out to `oshader --jit` on a
# fixture .sl calling a real, confirmed-unhandled builtin. That approach
# became structurally unsatisfiable once this spec's own US4 finished --
# every mnemonic in kAllFunctionMnemonics gained a dispatch case, leaving
# no real RSL builtin left to serve as a negative fixture. Now links
# libshader_compiler directly and drives emitLLVMBitcode() with a
# hand-built IRModule containing a synthetic, guaranteed-never-real
# opcode -- tests isHandledOpcode()'s actual contract, not a proxy for
# it, and can never go stale as coverage grows. See test_gate_hardening.cpp's
# header comment for the full rationale, including the positive-control
# assertion that precedes the negative one.
# ---------------------------------------------------------------------------
add_executable(test_libshader_gate_hardening test_gate_hardening.cpp)

target_compile_features(test_libshader_gate_hardening PRIVATE cxx_std_20)

target_include_directories(test_libshader_gate_hardening PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # llvmEmitter.h, ir.h, rslo.h
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
)

target_link_libraries(test_libshader_gate_hardening PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_GateHardening
    COMMAND test_libshader_gate_hardening
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(LibShader_GateHardening PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_GateHardening (hardened coverage-gate negative test)")

# ---------------------------------------------------------------------------
# test_illumination_emitter -- illuminate/solar operand forms in the JIT
# emitter (spec 024-shading-services-routing, US1, T013).
#
# Same approach as test_gate_hardening: emitLLVMBitcode() is driven directly
# with hand-built IRModules. A positive control (illuminate with a resolvable
# P) comes first; then the no-argument solar() form must emit
# op_solar0_begin, and operands that resolve to none of the four known forms
# must fail the emit with a diagnostic naming the mnemonic instead of being
# skipped silently.
# ---------------------------------------------------------------------------
add_executable(test_libshader_illumination_emitter test_illumination_emitter.cpp)

target_compile_features(test_libshader_illumination_emitter PRIVATE cxx_std_20)

target_include_directories(test_libshader_illumination_emitter PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # llvmEmitter.h, ir.h, rslo.h
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
)

target_link_libraries(test_libshader_illumination_emitter PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_IlluminationOperands
    COMMAND test_libshader_illumination_emitter
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

set_tests_properties(LibShader_IlluminationOperands PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_IlluminationOperands (illuminate/solar operand forms in the JIT emitter)")

# ---------------------------------------------------------------------------
# test_used_parameters_table — tier-1 table-parity test (spec 014, T007).
# Asserts the llvmEmitter.cpp X-macro re-expansion table matches the
# interpreter's own shaderFunctions.h/shaderOpcodes.h/giFunctions.h/
# giOpcodes.h (text, params) rows exactly. Compiler-only, no shading engine.
# ---------------------------------------------------------------------------
add_executable(test_libshader_used_parameters_table test_used_parameters_table.cpp)

target_compile_features(test_libshader_used_parameters_table PRIVATE cxx_std_20)

target_include_directories(test_libshader_used_parameters_table PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # llvmEmitter.h
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
    ${CMAKE_SOURCE_DIR}/src/ri                   # rendererc.h (PARAMETER_* constants)
    ${CMAKE_SOURCE_DIR}/src/ri/hiders
    ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture
    ${CMAKE_SOURCE_DIR}/src/ri/output
    ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer
    ${CMAKE_SOURCE_DIR}/src/libshader/shading    # scriptFunctions.h/scriptOpcodes.h
)

target_link_libraries(test_libshader_used_parameters_table PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_UsedParametersTable
    COMMAND test_libshader_used_parameters_table)

set_tests_properties(LibShader_UsedParametersTable PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_UsedParametersTable (opcode/function bit table parity)")

# ---------------------------------------------------------------------------
# test_used_parameters_gating — tier-2 gating-condition test (spec 014, T008/T009).
# Compiles small RSL fixtures and asserts the computed usedParameters bitmask
# (not just the table) is correct for the Ci/Oi default-fill gate.
# Compiler-only, no shading engine.
# ---------------------------------------------------------------------------
add_executable(test_libshader_used_parameters_gating test_used_parameters_gating.cpp)

target_compile_features(test_libshader_used_parameters_gating PRIVATE cxx_std_20)

target_include_directories(test_libshader_used_parameters_gating PRIVATE
    ${CMAKE_SOURCE_DIR}/src/libshader/compiler   # llvmEmitter.h
    ${CMAKE_BINARY_DIR}/src/libshader/compiler   # Flex/Bison generated headers
    ${CMAKE_SOURCE_DIR}/src                      # common/, logging.h
)

target_link_libraries(test_libshader_used_parameters_gating PRIVATE
    libshader_compiler
    openrendercommon
)

add_test(NAME LibShader_UsedParametersGating
    COMMAND test_libshader_used_parameters_gating)

set_tests_properties(LibShader_UsedParametersGating PROPERTIES
    TIMEOUT 30
    LABELS  "libshader;compiler;unit"
)

message(STATUS "  - LibShader_UsedParametersGating (usedParameters bitmask gating conditions)")

else()
    message(STATUS "  - LibShader_OpcodeCoverage / UsedParametersTable / UsedParametersGating "
                   "SKIPPED (no LLVM JIT in this build -- they test the JIT emitter)")
endif()

# ---------------------------------------------------------------------------
# Spec 039, Phase 4 (US6): call-site ordinals and stale-shader rejection.
# Written before the implementation (tests first). They read LLVM bitcode, so
# they need the JIT.
#   test_call_sites       T039 LibShader_CallSiteOrdinals (C1, C3)
#                         T040 LibShader_RandomNotPure (C2)
#   test_stale_shaders    T041 LibShader_StaleShaderRejected (research R3.6): the
#                         .rslo half and the .slo half, as two ctest entries
# The fixtures are generated by the tests in a temporary directory.
# ---------------------------------------------------------------------------
if(OPENRENDER_HAVE_LLVM)
    llvm_map_components_to_libnames(_call_sites_llvm_libs core bitreader bitwriter support)

    add_executable(test_libshader_call_sites test_call_sites.cpp)
    target_compile_features(test_libshader_call_sites PRIVATE cxx_std_20)
    target_include_directories(test_libshader_call_sites PRIVATE
        ${CMAKE_SOURCE_DIR}/src/libshader/compiler
        ${CMAKE_BINARY_DIR}/src/libshader/compiler
        ${CMAKE_SOURCE_DIR}/src)
    target_include_directories(test_libshader_call_sites SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(test_libshader_call_sites PRIVATE ${LLVM_DEFINITIONS})
    target_link_libraries(test_libshader_call_sites PRIVATE libshader_compiler openrendercommon ${_call_sites_llvm_libs})

    foreach(_case ordinals random_not_pure)
        if(_case STREQUAL "ordinals")
            set(_name LibShader_CallSiteOrdinals)
        else()
            set(_name LibShader_RandomNotPure)
        endif()
        add_test(NAME ${_name} COMMAND test_libshader_call_sites ${_case})
        set_tests_properties(${_name} PROPERTIES TIMEOUT 60 LABELS "libshader;compiler;unit"
            ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;SHADER_INCLUDES=${CMAKE_SOURCE_DIR}/shaders/includes")
    endforeach()

    add_executable(test_libshader_stale_shaders test_stale_shaders.cpp)
    target_compile_features(test_libshader_stale_shaders PRIVATE cxx_std_20)
    target_include_directories(test_libshader_stale_shaders PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/libshader/shading
        ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri
        ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/texture
        ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/dataviewer)
    target_include_directories(test_libshader_stale_shaders SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(test_libshader_stale_shaders PRIVATE ${LLVM_DEFINITIONS})
    target_link_libraries(test_libshader_stale_shaders PRIVATE ribRender libshader_shading openrendercommon ${_call_sites_llvm_libs})

    add_test(NAME LibShader_StaleShaderRejected COMMAND test_libshader_stale_shaders stale_rslo)
    add_test(NAME LibShader_StaleShaderRejectedJit COMMAND test_libshader_stale_shaders stale_slo)
    set_tests_properties(LibShader_StaleShaderRejected LibShader_StaleShaderRejectedJit PROPERTIES TIMEOUT 60
        LABELS "libshader;compiler;unit"
        ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;SHADER_INCLUDES=${CMAKE_SOURCE_DIR}/shaders/includes;SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")

    # Spec 039, Phase 4 follow-up: the .rslo and .slo format versions follow semver
    # (same major, file minor <= supported minor; patch never checked). Written
    # before the implementation. See the test's header comment for each case.
    add_executable(test_libshader_format_versions test_format_versions.cpp)
    target_compile_features(test_libshader_format_versions PRIVATE cxx_std_20)
    target_include_directories(test_libshader_format_versions PRIVATE
        ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/libshader/shading
        ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/render ${CMAKE_SOURCE_DIR}/src/ri
        ${CMAKE_SOURCE_DIR}/src/ri/geometry ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/texture
        ${CMAKE_SOURCE_DIR}/src/ri/core ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/dataviewer)
    target_include_directories(test_libshader_format_versions SYSTEM PRIVATE ${LLVM_INCLUDE_DIRS})
    target_compile_definitions(test_libshader_format_versions PRIVATE ${LLVM_DEFINITIONS})
    target_link_libraries(test_libshader_format_versions PRIVATE ribRender libshader_shading openrendercommon ${_call_sites_llvm_libs})

    set(_format_cases rslo_old_refused rslo_newer_refused rslo_lower_minor rslo_current_loads
        slo_emitted slo_old_refused slo_missing_refused slo_newer_refused slo_current_loads)
    foreach(_case IN LISTS _format_cases)
        add_test(NAME LibShader_Format_${_case} COMMAND test_libshader_format_versions ${_case})
        set_tests_properties(LibShader_Format_${_case} PROPERTIES TIMEOUT 60
            LABELS "libshader;compiler;unit"
            ENVIRONMENT "OSHADER_BIN=$<TARGET_FILE:oshader>;SHADER_INCLUDES=${CMAKE_SOURCE_DIR}/shaders/includes;SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
    endforeach()
    # Nothing lower than minor 0 exists: report a skip, not a pass, while that holds.
    set_tests_properties(LibShader_Format_rslo_lower_minor PROPERTIES SKIP_RETURN_CODE 77)
endif()
