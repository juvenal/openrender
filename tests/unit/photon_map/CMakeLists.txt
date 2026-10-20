# Photon-map lifetime tests (GitHub #64).
#
# `ctest --test-dir build -L photon` runs these with the other photon tests.

# photonmap_FrameCacheRelease: an in-process photon frame whose world-level
# attributes hold both a global and a caustic map. The test replaces operator
# delete to catch a map that is freed while a counted reference remains, or
# written after it is freed; no sanitizer needed.
add_executable(test_photon_map_frame_release test_photon_map_frame_release.cpp)
target_compile_features(test_photon_map_frame_release PRIVATE cxx_std_20)
target_include_directories(test_photon_map_frame_release PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${CMAKE_SOURCE_DIR}/src/ri/parse)
target_link_libraries(test_photon_map_frame_release PRIVATE ribRender openrendercommon)
add_test(NAME photonmap_FrameCacheRelease COMMAND test_photon_map_frame_release)
set_tests_properties(photonmap_FrameCacheRelease PROPERTIES TIMEOUT 60 LABELS "photon;unit;regression"
    ENVIRONMENT "SHADERS=${CMAKE_BINARY_DIR}/shaders;ORENDERHOME=${CMAKE_SOURCE_DIR}")
