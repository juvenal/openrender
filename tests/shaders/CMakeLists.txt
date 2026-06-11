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

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/tests/shaders)

message(STATUS "  - ShaderCompilerImmutability (compile all shaders, diff vs references)")
