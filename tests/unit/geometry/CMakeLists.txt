# Unit tests for surface geometry: what a surface's sample() writes.
#
# `ctest --test-dir build -L geometry` runs every test registered here.
#
# The suites run Ri in-process with RiBegin(RI_NULL) and capture primitives
# through the CSG suites' CCsgSession (tests/unit/csg/csgTestUtils.h), and
# never render. ORENDERHOME points at the source root, which has no
# .orenderrc, for the reason given in tests/unit/csg/CMakeLists.txt (GitHub #2).

set(GEOMETRY_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libshader/shading ${CMAKE_SOURCE_DIR}/tests/unit/csg)

# normalFix()/tangentFix() honour sample()'s start offset (raytraced derivative points)
add_executable(test_geometry_degenerate_fix_offset test_degenerate_fix_offset.cpp)
target_compile_features(test_geometry_degenerate_fix_offset PRIVATE cxx_std_20)
target_include_directories(test_geometry_degenerate_fix_offset PRIVATE ${GEOMETRY_TEST_INCLUDES})
target_link_libraries(test_geometry_degenerate_fix_offset PRIVATE ribRender openrendercommon)
add_test(NAME Geometry_DegenerateFixOffset COMMAND test_geometry_degenerate_fix_offset)
set_tests_properties(Geometry_DegenerateFixOffset PROPERTIES TIMEOUT 120 LABELS "geometry;unit"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
