# Unit tests for the optional OpenImageIO backend (spec 048-oiio-image-io).
#
# `ctest --test-dir build -L imageio` runs every test registered here. The label `imageio` is on every test.
#
# Two kinds of build (FR-003): with OIIO (HAVE_OIIO, set by the root CMakeLists.txt after the version check) the
# OIIO tests are registered; without it the "absent" tests are registered instead, and the default build is what
# every other platform and the CI jobs build.

set(IMAGEIO_FIXTURES_DIR ${CMAKE_SOURCE_DIR}/tests/unit/imageio/fixtures)

# --- registered in every build -----------------------------------------------------------------------------------

# P-Q11: CI never enables OIIO in spec 048, so every CI job is a "without OIIO" job.
add_test(NAME ImageIO_CiHasNoOiio
    COMMAND ${CMAKE_COMMAND}
        -DWORKFLOW=${CMAKE_SOURCE_DIR}/.github/workflows/release.yml
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_ci_no_oiio.cmake)
set_tests_properties(ImageIO_CiHasNoOiio PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

# FR-002: the manifest gains only the `oiio` feature; the shared dependency list and the baseline do not move.
add_test(NAME ImageIO_VcpkgManifest
    COMMAND ${CMAKE_COMMAND}
        -DMANIFEST=${CMAKE_SOURCE_DIR}/vcpkg.json
        -P ${CMAKE_CURRENT_SOURCE_DIR}/check_vcpkg_manifest.cmake)
set_tests_properties(ImageIO_VcpkgManifest PROPERTIES TIMEOUT 30 LABELS "imageio;unit")

# --- build with OIIO ----------------------------------------------------------------------------------------------
if(HAVE_OIIO)
    # US5: src/imageio/ includes only the standard library, its own published header, common/ and OIIO's headers.
    add_test(NAME ImageIO_NoDomainInclude
        COMMAND ${CMAKE_COMMAND}
            -DIMAGEIO_DIR=${CMAKE_SOURCE_DIR}/src/imageio
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_no_domain_include.cmake)
    set_tests_properties(ImageIO_NoDomainInclude PROPERTIES TIMEOUT 30 LABELS "imageio;unit")
else()
    # FR-003: a build without OIIO carries no OIIO code and no OIIO driver.
    add_test(NAME ImageIO_AbsentBuild
        COMMAND ${CMAKE_COMMAND}
            -DBUILD_DIR=${CMAKE_BINARY_DIR}
            -DNM=${CMAKE_NM}
            -P ${CMAKE_CURRENT_SOURCE_DIR}/check_absent_build.cmake)
    set_tests_properties(ImageIO_AbsentBuild PROPERTIES TIMEOUT 600 LABELS "imageio;unit")
endif()
