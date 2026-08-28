# Unit tests for the solid CSG boolean kernel (spec 013-solid-csg-operations).
# Phase 3 (US1, T013-T016) adds the actual test executables here, following
# the pattern already used by src/libshader/tests/CMakeLists.txt.
#
# `ctest --test-dir build -L csg` runs every test registered here.

add_executable(test_csg_boolean_boxes test_boolean_boxes.cpp)
target_compile_features(test_csg_boolean_boxes PRIVATE cxx_std_20)
target_include_directories(test_csg_boolean_boxes PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_boolean_boxes PRIVATE ri openrendercommon)
add_test(NAME CSG_BooleanBoxes COMMAND test_csg_boolean_boxes)
set_tests_properties(CSG_BooleanBoxes PROPERTIES TIMEOUT 30 LABELS "csg;unit")

add_executable(test_csg_multi_primitive_leaf test_multi_primitive_leaf.cpp)
target_compile_features(test_csg_multi_primitive_leaf PRIVATE cxx_std_20)
target_include_directories(test_csg_multi_primitive_leaf PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_multi_primitive_leaf PRIVATE ri openrendercommon)
add_test(NAME CSG_MultiPrimitiveLeaf COMMAND test_csg_multi_primitive_leaf)
set_tests_properties(CSG_MultiPrimitiveLeaf PROPERTIES TIMEOUT 30 LABELS "csg;unit"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")

add_executable(test_csg_boolean_sphere_box test_boolean_sphere_box.cpp)
target_compile_features(test_csg_boolean_sphere_box PRIVATE cxx_std_20)
target_include_directories(test_csg_boolean_sphere_box PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_boolean_sphere_box PRIVATE ri openrendercommon)
add_test(NAME CSG_BooleanSphereBox COMMAND test_csg_boolean_sphere_box)
set_tests_properties(CSG_BooleanSphereBox PROPERTIES TIMEOUT 30 LABELS "csg;unit")

add_executable(test_csg_boolean_coplanar test_boolean_coplanar.cpp)
target_compile_features(test_csg_boolean_coplanar PRIVATE cxx_std_20)
target_include_directories(test_csg_boolean_coplanar PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_boolean_coplanar PRIVATE ri openrendercommon)
add_test(NAME CSG_BooleanCoplanar COMMAND test_csg_boolean_coplanar)
set_tests_properties(CSG_BooleanCoplanar PROPERTIES TIMEOUT 30 LABELS "csg;unit")

add_executable(test_csg_tesselation_flatness test_tesselation_flatness.cpp)
target_compile_features(test_csg_tesselation_flatness PRIVATE cxx_std_20)
target_include_directories(test_csg_tesselation_flatness PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_tesselation_flatness PRIVATE ri openrendercommon)
add_test(NAME CSG_TesselationFlatness COMMAND test_csg_tesselation_flatness)
set_tests_properties(CSG_TesselationFlatness PROPERTIES TIMEOUT 30 LABELS "csg;unit")

add_executable(test_csg_quadric_tesselation test_quadric_tesselation.cpp)
target_compile_features(test_csg_quadric_tesselation PRIVATE cxx_std_20)
target_include_directories(test_csg_quadric_tesselation PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_quadric_tesselation PRIVATE ri openrendercommon)
add_test(NAME CSG_QuadricTesselation COMMAND test_csg_quadric_tesselation)
set_tests_properties(CSG_QuadricTesselation PROPERTIES TIMEOUT 30 LABELS "csg;unit"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")

add_executable(test_csg_patchmesh_tesselation test_patchmesh_tesselation.cpp)
target_compile_features(test_csg_patchmesh_tesselation PRIVATE cxx_std_20)
target_include_directories(test_csg_patchmesh_tesselation PRIVATE ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_csg_patchmesh_tesselation PRIVATE ri openrendercommon)
add_test(NAME CSG_PatchMeshTesselation COMMAND test_csg_patchmesh_tesselation)
set_tests_properties(CSG_PatchMeshTesselation PROPERTIES TIMEOUT 30 LABELS "csg;unit"
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
