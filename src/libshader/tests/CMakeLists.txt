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
