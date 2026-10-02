# Unit tests for the closed-form polynomial solvers in src/common/polynomial.h
# (solveQuadric / solveCubic / solveQuartic; solveQuartic backs the raytraced
# torus intersector, CToroid::intersect).
#
# `ctest --test-dir build -L polynomial` runs every test registered here.

add_executable(test_polynomial test_polynomial.cpp)
target_compile_features(test_polynomial PRIVATE cxx_std_20)
target_include_directories(test_polynomial PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_polynomial PRIVATE openrendercommon)
add_test(NAME Polynomial_Solvers COMMAND test_polynomial)
set_tests_properties(Polynomial_Solvers PROPERTIES TIMEOUT 60 LABELS "polynomial;unit")
