# Unit tests for solid (CSG) geometry.
#
# Spec 013 introduced these suites for the BSP boolean kernel; spec 021
# (deferred CSG evaluation) rewrote the kernel suites as classification and
# boundary-rule tests and added the suites below. The tessellation suites
# (test_quadric_tesselation, test_patchmesh_tesselation,
# test_tesselation_flatness) cover spec 013 helpers that spec 021 retires
# with them (T077).
#
# `ctest --test-dir build -L csg` runs every test registered here.

set(CSG_TEST_INCLUDES
    ${CMAKE_SOURCE_DIR}/src/ri ${CMAKE_SOURCE_DIR}/src/ri/hiders ${CMAKE_SOURCE_DIR}/src/ri/render
    ${CMAKE_SOURCE_DIR}/src/ri/state ${CMAKE_SOURCE_DIR}/src/ri/parse ${CMAKE_SOURCE_DIR}/src/ri/geometry
    ${CMAKE_SOURCE_DIR}/src/ri/texture ${CMAKE_SOURCE_DIR}/src/ri/output ${CMAKE_SOURCE_DIR}/src/ri/core
    ${CMAKE_SOURCE_DIR}/src/ri/dataviewer ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/libshader/shading)

# add_csg_unit_test(<CTest name> <source>)
#
# The suites run Ri in-process with RiBegin(RI_NULL) and never render, so
# they must not pick up tests/visual's .orenderrc: its Display and Format
# make RiWorldEnd try to open the file display, and a failed display load
# followed by another RiBegin/RiEnd cycle in the same process crashes
# (GitHub #2). ORENDERHOME therefore points at the source root, which has no
# .orenderrc, as these suites always have.
function(add_csg_unit_test NAME SOURCE)
    get_filename_component(_target ${SOURCE} NAME_WE)
    set(_target test_csg_${_target})
    string(REPLACE "test_csg_test_" "test_csg_" _target ${_target})
    add_executable(${_target} ${SOURCE})
    target_compile_features(${_target} PRIVATE cxx_std_20)
    target_include_directories(${_target} PRIVATE ${CSG_TEST_INCLUDES})
    target_link_libraries(${_target} PRIVATE ribRender openrendercommon)
    add_test(NAME ${NAME} COMMAND ${_target})
    set_tests_properties(${NAME} PROPERTIES TIMEOUT 120 LABELS "csg;unit"
        ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
endfunction()

# Classification and boundary-rule suites (spec 021; the first six keep the
# names of the spec 013 kernel suites they replace)
add_csg_unit_test(CSG_BooleanBoxes        test_boolean_boxes.cpp)
add_csg_unit_test(CSG_BooleanSphereBox    test_boolean_sphere_box.cpp)
add_csg_unit_test(CSG_BooleanSphereSphere test_boolean_sphere_sphere.cpp)
add_csg_unit_test(CSG_BooleanCoplanar     test_boolean_coplanar.cpp)
add_csg_unit_test(CSG_NestedTree          test_nested_tree.cpp)
add_csg_unit_test(CSG_MultiPrimitiveLeaf  test_multi_primitive_leaf.cpp)
add_csg_unit_test(CSG_CountCrossings      test_count_crossings.cpp)
add_csg_unit_test(CSG_ClassifyQuadrics    test_classify_quadrics.cpp)
add_csg_unit_test(CSG_ParityRobustness    test_parity_robustness.cpp)
add_csg_unit_test(CSG_BoundaryRule        test_boundary_rule.cpp)
add_csg_unit_test(CSG_ClassifyDeterminism test_classify_determinism.cpp)
add_csg_unit_test(CSG_Stats               test_csg_stats.cpp)
add_csg_unit_test(CSG_RegionInheritance   test_region_inheritance.cpp)

# Solid capture, lazy instance resolution and implied caps (spec 021,
# Phase 3: T042, T043, T045)
add_csg_unit_test(CSG_ImpliedCaps         test_implied_caps.cpp)
add_csg_unit_test(CSG_CapturePath         test_capture_path.cpp)
add_csg_unit_test(CSG_LazyResolution      test_lazy_resolution.cpp)

# Interior/Exterior shader selection (spec 013; updated by spec 021 T044)
add_csg_unit_test(CSG_InteriorExteriorSelection test_interior_exterior_selection.cpp)

# Spec 013 tessellation helpers, retired by spec 021 T077
add_csg_unit_test(CSG_TesselationFlatness   test_tesselation_flatness.cpp)
add_csg_unit_test(CSG_QuadricTesselation    test_quadric_tesselation.cpp)
add_csg_unit_test(CSG_PatchMeshTesselation  test_patchmesh_tesselation.cpp)
