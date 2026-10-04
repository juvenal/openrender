# Unit tests for the RIB parser (src/ri/parse).
#
# `ctest --test-dir build -L rib_parse` runs every test registered here.

add_executable(test_rib_parser_sessions test_parser_sessions.cpp)
target_compile_features(test_rib_parser_sessions PRIVATE cxx_std_20)
target_include_directories(test_rib_parser_sessions PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_rib_parser_sessions PRIVATE ribRender openrendercommon)
add_test(NAME RibParse_SessionsReadArchives COMMAND test_rib_parser_sessions)
set_tests_properties(RibParse_SessionsReadArchives PROPERTIES TIMEOUT 60 LABELS "rib_parse;unit"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
