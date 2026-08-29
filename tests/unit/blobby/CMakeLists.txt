# Unit tests for RiBlobby implicit surfaces (spec
# 015-blobby-implicit-surfaces), following the tests/unit/csg/ pattern
# established by spec 013.
#
# `ctest --test-dir build -L blobby` runs every test registered here. Each
# test MUST carry the label explicitly -- an unlabelled suite makes that
# command match nothing and report success against zero tests, which is
# indistinguishable from passing (T003, T100).

macro(add_blobby_test target source testname)
    add_executable(${target} ${source})
    target_compile_features(${target} PRIVATE cxx_std_20)
    target_include_directories(${target} PRIVATE
        ${CMAKE_SOURCE_DIR}/src/ri
        ${CMAKE_SOURCE_DIR}/src)
    target_link_libraries(${target} PRIVATE ri openrendercommon)
    add_test(NAME ${testname} COMMAND ${target})
    set_tests_properties(${testname} PROPERTIES TIMEOUT 120 LABELS "blobby;unit")
endmacro()

add_blobby_test(test_blobby_code_validation      test_code_validation.cpp      Blobby_CodeValidation)
add_blobby_test(test_blobby_extent               test_extent.cpp               Blobby_Extent)
add_blobby_test(test_blobby_field_primitives      test_field_primitives.cpp     Blobby_FieldPrimitives)
add_blobby_test(test_blobby_field_combining      test_field_combining.cpp      Blobby_FieldCombining)
add_blobby_test(test_blobby_opcode_order         test_opcode_order.cpp         Blobby_OpcodeOrder)
add_blobby_test(test_blobby_threshold            test_threshold_calibration.cpp Blobby_ThresholdCalibration)
add_blobby_test(test_blobby_polygonize_analytic  test_polygonize_analytic.cpp  Blobby_PolygonizeAnalytic)
add_blobby_test(test_blobby_polygonize_watertight test_polygonize_watertight.cpp Blobby_PolygonizeWatertight)
add_blobby_test(test_blobby_determinism          test_determinism.cpp          Blobby_Determinism)
add_blobby_test(test_blobby_value_blending       test_value_blending.cpp       Blobby_ValueBlending)
add_blobby_test(test_blobby_mpoint               test_mpoint.cpp               Blobby_MPoint)
add_blobby_test(test_blobby_tolerance            test_tolerance.cpp            Blobby_Tolerance)

# The repeller reads a depth file through CRenderer::locateFile, so it needs
# the same search paths the renderer resolves at startup.
add_blobby_test(test_blobby_repeller             test_repeller.cpp             Blobby_Repeller)
set_tests_properties(Blobby_Repeller PROPERTIES
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/shaders)

# Emission tests drive the real RI entry points, so they need the shader and
# home paths the renderer resolves at RiBegin, exactly as tests/unit/csg's
# context-driven tests do.
add_blobby_test(test_blobby_surface_params       test_surface_params.cpp       Blobby_SurfaceParams)
set_tests_properties(Blobby_SurfaceParams PROPERTIES
    ENVIRONMENT "SHADERS=${CMAKE_SOURCE_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
