# Spec 039 (sampling foundation) unit tests.
#
# `ctest --test-dir build -L sampling` runs every test registered here, plus
# `DiskSampling` (tests/CMakeLists.txt), which has carried the `sampling`
# label since before spec 039.
#
# Phase 1 only creates the target. No test is registered yet; the sampler
# tests of Phase 2 register themselves with
#     set_tests_properties(<name> PROPERTIES TIMEOUT 60 LABELS "sampling;unit")
# and link the shared libRIBCommon, never the static archive (CLAUDE.md
# gotcha 22: a second copy would hold a second arena and diagnostic sink).
add_executable(test_sampling test_sampling.cpp)
target_compile_features(test_sampling PRIVATE cxx_std_20)
target_include_directories(test_sampling PRIVATE ${CMAKE_SOURCE_DIR}/src)
target_link_libraries(test_sampling PRIVATE openrendercommon)
