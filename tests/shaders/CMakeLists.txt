# tests/shaders/CMakeLists.txt
#
# Shader compiler immutability test.
#
# Compiles every .sl file in shaders/ with the current oshader binary and
# diffs the output against reference .rslo files in references/.
#
# Failure means: the compiled output changed unexpectedly.
# On intentional compiler changes, update references/ and commit.

add_test(NAME ShaderCompilerImmutability
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_shader_compile.sh
        $<TARGET_FILE:oshader>
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_SOURCE_DIR}/references
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/shaders
)

set_tests_properties(ShaderCompilerImmutability PROPERTIES
    TIMEOUT  120
    LABELS   "shader;compiler;regression"
    DEPENDS  oshader
)

# sloinfo golden-output test.
#
# Runs sloinfo against a reference .rslo fixture (name must be printed, not
# the file path) and, when LLVM is available, against a freshly JIT-compiled
# .slo fixture (no internal "local variables" in the output).

add_test(NAME SloinfoGoldenOutput
    COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/test_sloinfo_output.sh
        $<TARGET_FILE:sloinfo>
        $<TARGET_FILE:oshader>
        ${CMAKE_SOURCE_DIR}/shaders
        ${CMAKE_CURRENT_SOURCE_DIR}/references
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/tests/shaders
)

set_tests_properties(SloinfoGoldenOutput PROPERTIES
    TIMEOUT  60
    LABELS   "shader;sloinfo;regression"
    DEPENDS  "sloinfo;oshader"
)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/shaders)

message(STATUS "  - ShaderCompilerImmutability (compile all shaders, diff vs references)")
message(STATUS "  - SloinfoGoldenOutput (sloinfo name + no internal vars in output)")
