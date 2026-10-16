# Spec 039 convergence harness (FR-033, SC-003b).
#
# Opt-in: tests registered here carry the `convergence` label and no other
# default-run label, and a plain `ctest` run must not include them. Run them
# with `ctest --test-dir build -L convergence`.
#
# Phase 1 only creates the target. Registration, the scene templates and the
# `convergence-reference` target arrive in Phase 8.
find_package(TIFF REQUIRED)

add_executable(test_convergence test_convergence.cpp)
target_compile_features(test_convergence PRIVATE cxx_std_20)
target_link_libraries(test_convergence PRIVATE TIFF::TIFF)
