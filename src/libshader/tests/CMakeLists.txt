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
