# Spec 023 (common infrastructure floor) tests.
#
# `ctest --test-dir build -L common` runs every test registered here.

# FR-005: golden table for variable declarations. It passes on the
# pre-feature code; after the grammar moves to src/common only the include
# path below changes, and the table must still match byte for byte.
add_executable(test_common_variables test_common_variables.cpp)
target_compile_features(test_common_variables PRIVATE cxx_std_20)
target_include_directories(test_common_variables PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/parse
    ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/core)
target_link_libraries(test_common_variables PRIVATE ribRender openrendercommon)
add_test(NAME Common_VariableDeclarations
    COMMAND test_common_variables ${CMAKE_CURRENT_SOURCE_DIR}/variable_golden.txt)
set_tests_properties(Common_VariableDeclarations PROPERTIES TIMEOUT 60 LABELS "common;unit")

# FR-008 / research R2: every process-wide symbol has exactly one definer, the
# shared libRIBCommon, across every image the tree builds. owned-symbols.txt
# grows phase by phase as symbols move into common.
# tests/ is configured before src/, so the targets are resolved at generate
# time; a target that is not built (openexr without OpenEXR, the platform's
# other framebuffer flavour) expands to an empty entry the script skips.
set(_common_images "")
foreach(_t ribRender ribVector ribOut libshader_shading libshader_compiler libshader_runtime
           file framebuffer rgbe openexr
           orender oshader otexmake precomp sloinfo test_common_variables)
    list(APPEND _common_images "$<$<TARGET_EXISTS:${_t}>:$<TARGET_FILE:${_t}>>")
endforeach()
add_test(NAME Common_StateOwnership
    COMMAND ${CMAKE_COMMAND}
        "-DIMAGES=${_common_images}"
        "-DCOMMON=$<TARGET_FILE:openrendercommon>"
        "-DOWNED=${CMAKE_CURRENT_SOURCE_DIR}/owned-symbols.txt"
        -P ${CMAKE_CURRENT_SOURCE_DIR}/state_ownership.cmake)
set_tests_properties(Common_StateOwnership PROPERTIES TIMEOUT 120 LABELS "common;unit")
