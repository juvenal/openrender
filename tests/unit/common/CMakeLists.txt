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
