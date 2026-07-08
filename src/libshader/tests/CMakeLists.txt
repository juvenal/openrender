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
