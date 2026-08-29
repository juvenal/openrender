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
